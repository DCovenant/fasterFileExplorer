use crossbeam_channel::bounded;
use indicatif::{ProgressBar, ProgressStyle};
use rayon::prelude::*;
use rusqlite::{params, Connection};
use std::collections::{HashMap, HashSet};
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Instant, UNIX_EPOCH};
use walkdir::WalkDir;

const BATCH_SIZE: usize = 10000;
const CHANNEL_SIZE: usize = 100000;

#[derive(Debug, Clone)]
struct FileRecord {
    path: String,
    name: String,
    size: u64,
    modified: i64,
}

struct Database {
    conn: Mutex<Connection>,
}

impl Database {
    fn new(db_path: &str) -> Result<Self, rusqlite::Error> {
        let conn = Connection::open(db_path)?;
        
        // Ultra-aggressive SQLite optimizations
        conn.execute_batch(
            "PRAGMA synchronous = OFF;
             PRAGMA journal_mode = MEMORY;
             PRAGMA temp_store = MEMORY;
             PRAGMA cache_size = -128000;
             PRAGMA page_size = 65536;
             PRAGMA locking_mode = EXCLUSIVE;
             PRAGMA count_changes = OFF;
             PRAGMA auto_vacuum = NONE;
             PRAGMA mmap_size = 268435456;
             PRAGMA threads = 8;",
        )?;

        // Create table
        conn.execute(
            "CREATE TABLE IF NOT EXISTS files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                path TEXT UNIQUE NOT NULL,
                name TEXT NOT NULL,
                size INTEGER,
                modified INTEGER
            )",
            [],
        )?;

        Ok(Database {
            conn: Mutex::new(conn),
        })
    }

    fn insert_batch(&self, records: &[FileRecord]) -> Result<usize, rusqlite::Error> {
        let mut conn = self.conn.lock().unwrap();
        let tx = conn.transaction()?;

        {
            let mut stmt = tx.prepare_cached(
                "INSERT OR REPLACE INTO files (path, name, size, modified) VALUES (?1, ?2, ?3, ?4)",
            )?;

            for record in records {
                stmt.execute(params![
                    &record.path,
                    &record.name,
                    record.size as i64,
                    record.modified
                ])?;
            }
        }

        tx.commit()?;
        Ok(records.len())
    }

    fn create_indexes(&self) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.execute("CREATE INDEX IF NOT EXISTS idx_name ON files(name)", [])?;
        conn.execute("CREATE INDEX IF NOT EXISTS idx_path ON files(path)", [])?;
        conn.execute("ANALYZE", [])?;
        Ok(())
    }

    fn vacuum(&self) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.execute("VACUUM", [])?;
        Ok(())
    }

    fn get_existing_files(&self, indexed_folder: &str) -> Result<HashMap<String, (i64, i64)>, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare("SELECT path, size, modified FROM files WHERE path LIKE ?1")?;
        let rows = stmt.query_map([format!("{}%", indexed_folder)], |row| {
            Ok((row.get::<_, String>(0)?, (row.get::<_, i64>(1)?, row.get::<_, i64>(2)?)))
        })?;
        
        let mut map = HashMap::new();
        for row in rows {
            if let Ok((path, data)) = row {
                map.insert(path, data);
            }
        }
        Ok(map)
    }

    fn delete_batch(&self, paths: &[String]) -> Result<usize, rusqlite::Error> {
        if paths.is_empty() {
            return Ok(0);
        }

        let mut conn = self.conn.lock().unwrap();
        let tx = conn.transaction()?;
        {
            for chunk in paths.chunks(500) {
                let placeholders = chunk.iter().map(|_| "?").collect::<Vec<_>>().join(",");
                let sql = format!("DELETE FROM files WHERE path IN ({})", placeholders);
                let mut stmt = tx.prepare(&sql)?;
                stmt.execute(rusqlite::params_from_iter(chunk))?;
            }
        }
        tx.commit()?;
        Ok(paths.len())
    }

    fn get_size(&self, db_path: &str) -> u64 {
        fs::metadata(db_path).map(|m| m.len()).unwrap_or(0)
    }
}

fn collect_files(root_path: &Path) -> Vec<PathBuf> {
    println!("\nScanning directory structure...");
    eprintln!("Scanning started");  // Write to stderr to ensure it's seen
    
    let pb = ProgressBar::new_spinner();
    pb.set_style(
        ProgressStyle::default_spinner()
            .template("{spinner:.green} [{elapsed_precise}] Scanning: {human_pos} files ({per_sec})")
            .unwrap(),
    );
    pb.enable_steady_tick(std::time::Duration::from_millis(100));

    let counter = Arc::new(AtomicUsize::new(0));
    let counter_clone = Arc::clone(&counter);
    let last_print = Arc::new(AtomicUsize::new(0));
    let last_print_clone = Arc::clone(&last_print);

    // Ultra-fast parallel directory traversal with batched progress updates
    let files: Vec<PathBuf> = WalkDir::new(root_path)
        .follow_links(false)
        .into_iter()
        .par_bridge() // Parallel processing
        .filter_map(|entry| {
            match entry {
                Ok(e) => {
                    if e.file_type().is_file() {
                        // Update counter every 100 files to reduce contention
                        let count = counter_clone.fetch_add(1, Ordering::Relaxed);
                        if count % 100 == 0 {
                            pb.set_position(count as u64);
                        }
                        // Print progress every 1000 files for non-TTY environments
                        if count % 1000 == 0 && count > last_print_clone.load(Ordering::Relaxed) {
                            last_print_clone.store(count, Ordering::Relaxed);
                            eprintln!("Scanning... {} files found", format_number(count));
                        }
                        Some(e.path().to_path_buf())
                    } else {
                        None
                    }
                }
                Err(_) => None,
            }
        })
        .collect();

    pb.set_position(counter.load(Ordering::Relaxed) as u64);
    pb.finish_and_clear();
    println!("[OK] Found {} files", format_number(files.len()));
    eprintln!("Found {} files", format_number(files.len()));
    
    files
}

fn process_files_incremental(
    files: Vec<PathBuf>,
    db: Arc<Database>,
    num_threads: usize,
    indexed_folder: &str,
) -> (usize, usize, usize) {
    println!("\nChecking for changes...");
    eprintln!("Loading existing database entries...");
    
    // Load existing files from database
    let existing_files = match db.get_existing_files(indexed_folder) {
        Ok(files) => files,
        Err(e) => {
            eprintln!("[WARNING] Could not load existing files: {}, doing full index", e);
            HashMap::new()
        }
    };
    
    let existing_count = existing_files.len();
    eprintln!("Found {} existing entries in database", format_number(existing_count));
    
    // Build set of current file paths for fast lookup
    let current_paths: HashSet<String> = files.iter()
        .map(|p| p.to_string_lossy().into_owned())
        .collect();
    
    // Find deleted files (in DB but not on disk)
    let deleted_paths: Vec<String> = existing_files.keys()
        .filter(|path| !current_paths.contains(*path))
        .cloned()
        .collect();
    
    let deleted_count = deleted_paths.len();
    if deleted_count > 0 {
        eprintln!("Found {} deleted files, removing from database...", format_number(deleted_count));
        if let Err(e) = db.delete_batch(&deleted_paths) {
            eprintln!("[WARNING] Failed to delete some files: {}", e);
        }
    } else {
        eprintln!("No deleted files found");
    }
    
    println!("\nIndexing new and modified files...");
    println!("Using {} threads", num_threads);
    eprintln!("Indexing started with {} threads", num_threads);

    let total_files = files.len();
    let pb = ProgressBar::new(total_files as u64);
    pb.set_style(
        ProgressStyle::default_bar()
            .template("{spinner:.green} [{elapsed_precise}] [{bar:40.cyan/blue}] {pos}/{len} ({per_sec}, {eta})")
            .unwrap()
            .progress_chars("#>-"),
    );

    let indexed_count = Arc::new(AtomicUsize::new(0));
    let skipped_count = Arc::new(AtomicUsize::new(0));
    let last_print = Arc::new(AtomicUsize::new(0));
    let (sender, receiver) = bounded::<Vec<FileRecord>>(CHANNEL_SIZE);

    // Spawn consumer thread for database writes
    let db_clone = Arc::clone(&db);
    let indexed_clone = Arc::clone(&indexed_count);
    let last_print_clone = Arc::clone(&last_print);
    let pb_clone = pb.clone();
    
    let consumer_handle = std::thread::spawn(move || {
        while let Ok(batch) = receiver.recv() {
            if let Ok(count) = db_clone.insert_batch(&batch) {
                let total = indexed_clone.fetch_add(count, Ordering::Relaxed) + count;
                pb_clone.inc(count as u64);
                pb_clone.tick();
                
                if total / 5000 > last_print_clone.load(Ordering::Relaxed) {
                    last_print_clone.store(total / 5000, Ordering::Relaxed);
                    eprintln!("Indexed {} new/modified files so far...", format_number(total));
                }
            }
        }
        pb_clone.finish_and_clear();
    });

    // Process files in parallel batches - only new or modified files
    let existing_files_arc = Arc::new(existing_files);
    let skipped_clone = Arc::clone(&skipped_count);
    
    rayon::ThreadPoolBuilder::new()
        .num_threads(num_threads)
        .build()
        .unwrap()
        .install(|| {
            files
                .par_chunks(BATCH_SIZE)
                .for_each_with((sender.clone(), existing_files_arc.clone(), skipped_clone.clone()), |(tx, existing, skipped), chunk| {
                    let mut batch = Vec::with_capacity(chunk.len());

                    for path in chunk {
                        if let Some(record) = process_file(path) {
                            // Check if file needs updating
                            let needs_update = if let Some((db_size, db_modified)) = existing.get(&record.path) {
                                // File exists in DB - only update if size or modified time changed
                                *db_size != record.size as i64 || *db_modified != record.modified
                            } else {
                                // New file - always add
                                true
                            };
                            
                            if needs_update {
                                batch.push(record);
                            } else {
                                skipped.fetch_add(1, Ordering::Relaxed);
                            }
                        }
                    }

                    if !batch.is_empty() {
                        let _ = tx.send(batch);
                    }
                });
        });

    // Close sender and wait for consumer
    drop(sender);
    consumer_handle.join().unwrap();

    pb.finish_and_clear();
    
    let indexed = indexed_count.load(Ordering::Relaxed);
    let skipped = skipped_count.load(Ordering::Relaxed);
    
    (indexed, skipped, deleted_count)
}

#[inline(always)]
fn process_file(path: &Path) -> Option<FileRecord> {
    // Ultra-fast metadata extraction
    let metadata = fs::metadata(path).ok()?;
    
    // Extract filename (fastest method)
    let name = path.file_name()?.to_string_lossy().into_owned();
    
    // Get modification time
    let modified = metadata
        .modified()
        .ok()?
        .duration_since(UNIX_EPOCH)
        .ok()?
        .as_secs() as i64;

    Some(FileRecord {
        path: path.to_string_lossy().into_owned(),
        name,
        size: metadata.len(),
        modified,
    })
}

fn format_number(n: usize) -> String {
    let s = n.to_string();
    let mut result = String::new();
    let len = s.len();
    
    for (i, c) in s.chars().enumerate() {
        if i > 0 && (len - i) % 3 == 0 {
            result.push(',');
        }
        result.push(c);
    }
    
    result
}

fn main() {
    println!("{}", "=".repeat(60));
    println!("   ULTRA-FAST RUST FILE INDEXER");
    println!("{}", "=".repeat(60));

    // Parse arguments
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 2 {
        eprintln!("\n[ERROR] No path provided");
        eprintln!("\nUsage:");
        eprintln!("   {} <path_to_index> [num_threads] [database_path]", args[0]);
        eprintln!("\nExample:");
        eprintln!("   {} /path/to/index", args[0]);
        eprintln!("   {} /path/to/index 16", args[0]);
        eprintln!("   {} /path/to/index 16 C:/custom/path/file_index.db", args[0]);
        std::process::exit(1);
    }

    let root_path = Path::new(&args[1]);

    // Validate path
    if !root_path.exists() {
        eprintln!("\n[ERROR] Path does not exist: {}", root_path.display());
        std::process::exit(1);
    }

    if !root_path.is_dir() {
        eprintln!("\n[ERROR] Path is not a directory: {}", root_path.display());
        std::process::exit(1);
    }

    println!("\n[TARGET] Path: {}", root_path.display());

    // Determine number of threads
    let num_threads = if args.len() >= 3 {
        args[2].parse::<usize>().unwrap_or_else(|_| {
            println!("[WARNING] Invalid thread count, using auto-detect");
            num_cpus::get() * 2
        })
    } else {
        // Optimal thread count for I/O-bound work
        std::cmp::min(64, num_cpus::get() * 2)
    };

    // Database location - use provided path or default
    let db_path = if args.len() >= 4 {
        args[3].clone()
    } else {
        "file_index.db".to_string()
    };
    println!("[DATABASE] Location: {}", db_path);

    // Start timing
    let start = Instant::now();

    // Initialize database
    let db = match Database::new(&db_path) {
        Ok(db) => Arc::new(db),
        Err(e) => {
            eprintln!("\n[ERROR] Failed to create database: {}", e);
            std::process::exit(1);
        }
    };

    println!("[OK] Database created: {}", db_path);

    // Collect all files
    let files = collect_files(root_path);

    if files.is_empty() {
        println!("[ERROR] No files found!");
        std::process::exit(0);
    }

    // Process files with smart incremental update
    let indexed_folder = root_path.to_string_lossy().into_owned();
    let (new_or_modified, unchanged, deleted) = process_files_incremental(files, Arc::clone(&db), num_threads, &indexed_folder);
    
    eprintln!("Indexing completed!");
    eprintln!("  New/Modified: {} files", format_number(new_or_modified));
    eprintln!("  Unchanged: {} files", format_number(unchanged));
    eprintln!("  Deleted: {} files", format_number(deleted));

    // Create indexes
    println!("\n[INDEXES] Creating indexes...");
    eprintln!("Creating database indexes...");
    if let Err(e) = db.create_indexes() {
        eprintln!("[WARNING] Failed to create indexes: {}", e);
    } else {
        eprintln!("Database indexes created successfully");
    }

    // Compact database (reclaim deleted space)
    if deleted > 0 {
        println!("\n[OPTIMIZE] Compacting database...");
        eprintln!("Running VACUUM to reclaim space...");
        if let Err(e) = db.vacuum() {
            eprintln!("[WARNING] Failed to compact database: {}", e);
        } else {
            eprintln!("Database compacted successfully");
        }
    }

    // Final statistics
    let elapsed = start.elapsed();
    let elapsed_secs = elapsed.as_secs_f64();
    let total_processed = new_or_modified + unchanged;

    let db_size_before = db.get_size(&db_path) as f64 / (1024.0 * 1024.0);

    println!("\n[COMPLETE] Indexing complete!");
    println!("   Total files scanned: {}", format_number(total_processed));
    println!("   New or modified: {}", format_number(new_or_modified));
    println!("   Unchanged (skipped): {}", format_number(unchanged));
    println!("   Deleted: {}", format_number(deleted));
    println!("   Time taken: {:.1} seconds", elapsed_secs);
    println!(
        "   Scan speed: {:.0} files/sec",
        total_processed as f64 / elapsed_secs
    );
    println!("   Database size: {:.1} MB", db_size_before);
    
    // Show average bytes per file for reference
    if total_processed > 0 {
        let bytes_per_file = (db_size_before * 1024.0 * 1024.0) / total_processed as f64;
        println!("   Average: {:.0} bytes/file", bytes_per_file);
    }
}
