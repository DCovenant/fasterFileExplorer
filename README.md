# GreedSearcher (FasterFileSearch) - Greedy by nature, precise by design.

## 🧭 Overview
**GreedSearcher** is a lightweight graphical application built to **replace the slow and unreliable Windows File Explorer search**.

The default file explorer search depends heavily on indexing — it *feels* fast only because it searches through cached paths. In reality, it often misses files that clearly exist but aren’t indexed.

GreedSearcher is different: it’s **greedy** — it searches *everything* within your chosen directory recursively and returns **all matching files**, quickly and accurately.

Powered by the `fd-find` library, it’s much faster and more precise than the native Windows search.

The app is written in **C++ and Qt**, so it’s compact and efficient — maybe not the prettiest, but definitely not ugly either.

---

## ⚙️ Installation

1. Download and run the `setup.exe`.
2. **Windows Defender Warning:**  
   You might see a “grayware” or “potentially unwanted software” warning.  
   This happens because the installer includes another installer inside it — specifically, the official Microsoft Visual C++ Redistributable (`vc_redist.x64.exe`).

   > This is a **false positive**.  
   > The dependency comes directly from Microsoft (via [aka.ms/vs/17/release/vc_redist.x64.exe](https://aka.ms/vs/17/release/vc_redist.x64.exe)) and is completely safe.  
   > You can safely ignore or allow the installer to run.

---

## 🪄 Usage

When you launch the application, a small terminal window will also appear.  
**Do not close it** — if you close the terminal, the app will also close.  
(This will be removed once the app leaves beta.)

### Interface Overview

<img width="1152" height="794" alt="image" src="https://github.com/user-attachments/assets/4b3da1eb-12f0-4065-80d1-dd3d9464905b" />

**[1] Directory Selector**  
Choose any folder as your base directory.  
All searches will be performed recursively inside that folder.

---

**[2] Search Field**  
Enter the term you want to search for.  

**Example:**
Search: example1
Results:
.../folders_that_dont_matter/example1.pdf
.../folders_that_dont_matter/example1hello.xls
.../folders_that_dont_matter/wowexample1.any_other_extension


---

**[3] Search Button / Enter Key**  
Starts the search process.

---

**[4] Folder Filter Toggle**  
Enables or disables the folder filtering option.

---

**[5] Folder Filter Field**  
When activated, you can specify a folder name to narrow down results.

**Example:**
Without filter ("example2"):
.../folder1/example2.any_extension
.../folder2/example2.any_extension
.../folder1/wow_again_example2.any_extension

With folder filter "folder1":
.../folder1/example2.any_extension
.../folder1/wow_again_example2.any_extension


---

**[6] Stop Button**  
Stops an ongoing search.

---

**[7] Status Label**  
Displays the current state (`Running` or `Stopped`).  
If it shows something unexpected, please contact **vlc@sisint.pt**.

---

**[8] Results List**  
Displays all search results.  
Double-click any entry to open the file.

---

## 🚀 Features
- Fast and accurate file search using `fd-find`
- Recursive search in any directory
- Optional folder-based filtering
- Lightweight C++/Qt application
- Double-click to open any file path.

---

## 🧰 Tech Stack
- **Language:** C++
- **Framework:** Qt
- **Search Engine:** fd-find (Rust-based)([Advice to give a look at the amazing work of sharkdp](https://github.com/sharkdp/fd))
- **Platform:** Windows

---

## ⚠️ Notes
This project is still in **beta**, so some visual or behavioral quirks may occur (such as the terminal window appearing at startup).

---

## 💬 Feedback
If you encounter issues or have feature requests, feel free to open an issue.

---

**GreedSearcher** — *Greedy by nature, precise by design.*
