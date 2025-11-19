# Ultra-Fast File Indexer

Two implementations: Python (optimized) and Rust (maximum performance)

## Rust Version (rustIndexP2Sql) - **FASTEST**

### Build
```bash
cargo build --release
```

### Run
```bash
# Binary will be in target/release/
./target/release/rustIndexP2Sql /path/to/index

# Or run directly with cargo
cargo run --release -- /path/to/index

# With custom thread count
./target/release/rustIndexP2Sql /path/to/index 16
```

### Performance
- **Expected speed: 5,000-20,000 files/sec** (vs Python's 500-2,000 files/sec)
- Uses parallel directory traversal with rayon
- Zero-cost abstractions
- No GIL limitations
- Memory-mapped I/O for SQLite

### Optimizations
- `par_bridge()` for parallel file discovery
- Crossbeam channels for lock-free communication
- Batch processing (10,000 files per batch)
- SQLite PRAGMA optimizations (256MB mmap, memory journal)
- Link-time optimization (LTO)
- Single codegen unit for maximum inlining
- Stripped binary

---

## Python Version (indexPathToSql.py)

### Run
```bash
# Install dependencies first
pip install tqdm

# Basic usage
python indexPathToSql.py /path/to/index

# With custom thread count
python indexPathToSql.py /path/to/index 64

# With process pool (faster)
python indexPathToSql.py /path/to/index 12 p
```

### Performance
- **Expected speed: 500-2,000 files/sec**
- Highly optimized Python with low-level tricks
- Good for occasional use

---

## Comparison

| Feature | Rust | Python |
|---------|------|--------|
| Speed | 5-20K files/sec | 0.5-2K files/sec |
| Memory | Lower | Higher |
| Startup | Instant | Instant |
| Dependencies | Compile-time | Runtime (pip) |
| Binary Size | ~3MB | N/A |

## Recommendation

- **For production/frequent use**: Use Rust version
- **For occasional use**: Python version is fine
- **Your Ryzen 5 5600H**: Both will work well, Rust will be 5-10x faster
