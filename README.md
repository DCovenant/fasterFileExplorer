# GreedSearcher

**Greedy by nature, precise by design.**

GreedSearcher is a lightweight graphical application designed to replace the Windows File Explorer search function. File Explorer's search is slow and unreliable, sometimes missing results we know are there. GreedSearcher searches everything—it's greedy and therefore finds all files matching your specifications.
This app was created with the objective of having a way to be faster searching networked disks.

## Installation

To install GreedSearcher, simply run the setup file. Windows Defender will likely flag it as containing a virus. This is just a grayware detection, meaning Windows Defender has detected that this installer contains another installer inside it. The installer in question is a Microsoft dependency (vc_redist.x64.exe). This warning can be safely ignored and/or you can allow the execution of this setup.

## Usage

<img width="800" height="685" alt="mainw" src="https://github.com/user-attachments/assets/e9dd2360-e085-48ed-a9f8-038ea8d3cbeb" />

**[1] – Directory Selection Button**  
Choose any folder to serve as the base directory. All searches will be performed recursively within this folder only.

**[2] – Search Term Field**  
Only activated once a directory is selected. Enter the term you want to search for in file names.

Example: Searching for "example1" returns:
```
.../irrelevant_folders/example1.pdf
.../irrelevant_folders/example1hello.xls
.../irrelevant_folders/wowexample1.any_format
```

**[3] – Search Button**  
Activates the search. You can also press Enter to start searching.

**[4] – Folder Filter Checkbox**  
When this checkbox is enabled, field [5] is activated.

**[5] – Folder Filter Field**  
Enter a term to filter folders.

Example: Without this field activated, searching for "example2" returns:
```
.../folder1/example2.any_format
.../folder2/example2.any_format
.../folder1/wow_again_example2.any_format
```

With "folder1" in this field, results are filtered to:
```
.../folder1/example2.any_format
.../folder1/wow_again_example2.any_format
```

**[6] – Stop Button**  
Stops the current search.

**[7] – Results List**  
Displays search results. Double-click any result to open the file.

**[8] – Results Filter Button**  
Applies a specific search filter (entered in field [9]) to the current results.

**[9] – Results Filter Field**  
Enter specific text to filter within the results.

**[10] – Load More Button**  
Only 5,000 results are displayed at a time. This button loads the remaining results.

**[11] – File Format Filter**  
Filter results by file format/extension.

**[12] – Indexing**  
To ensure instant search speed, folders must be indexed first. This process is not very time-consuming. A folder with 120,000 files took approximately 5 minutes to index. It's recommended to re-index whenever you want to remove deleted files from the local database and/or add files that aren't yet present.

<img width="801" height="631" alt="indexw" src="https://github.com/user-attachments/assets/b2285116-2506-40f5-a3bc-6e541fcfcfe4" />

**[13] – Index Folder Selection**  
Opens a menu to choose which folder to index.

**[14] – Start Indexing**  
Begins the indexing process.

## Features

- **Fast & Reliable**: Unlike File Explorer, GreedSearcher finds everything
- **Recursive Search**: Searches through all subdirectories
- **Advanced Filtering**: Filter by filename, folder path, and file format
- **Indexing System**: Index folders for instant search results
- **Large Result Handling**: Efficiently manages thousands of search results
- **Direct File Access**: Double-click results to open files immediately
