# GreedSearcher — Greedy by nature, precise by design.

## GreedSearcher

GreedSearcher is a lightweight graphical application designed to replace the built-in Windows File Explorer search menu.  
File Explorer’s search can often be slow and unreliable, sometimes missing files you know are there.  
GreedSearcher searches *everything*—it’s greedy, and therefore it finds all files that match your criteria.

---

## Installation

To install GreedSearcher, simply run the setup file.

Windows Defender may warn you about a potential threat. This is a **grayware detection**, meaning the installer contains another installer inside it.  
The additional installer is a required Microsoft dependency (**vc_redist.x64.exe**).

This warning can be safely ignored, and you may allow the setup to run.

---

## Usage

When the program starts, a terminal window will appear.  
If you close this terminal, the application will also close.  
This behavior will be removed once the program leaves the beta stage.

### Interface Guide

<img width="1007" height="791" alt="image" src="https://github.com/user-attachments/assets/be601424-3e56-4df5-8089-e5524e4d228b" />

**[1] — Directory Selection Button**  
Choose any folder you want. All searches will be performed **recursively** inside this selected folder.

---

**[2] — Search Term Input**  
Enabled only after selecting a directory.  
Enter the keyword you want to search for within filenames.

**Example:**  
If you search for `"example1"`  
Possible results:  

.../some_folder/example1.pdf
.../another_folder/example1ola.xls
.../nested_folder/wowexample1.someextension


---

**[3] — Search Button**  
Starts the search. You can also press **Enter**.

---

**[4] — Folder Filter Checkbox**  
When enabled, it activates section [5].

---

**[5] — Folder Filter Input**  
Allows filtering results to include only files inside folders matching your filter term.

**Example:**  
Searching for `"example2"` normally may return:  

.../pasta1/example2.any
.../pasta2/example2.any
.../pasta1/wow_again_example2.any

If you add `"pasta1"` as a folder filter:  

.../pasta1/example2.any
.../pasta1/wow_again_example2.any


---

**[6] — Stop Button**  
Stops the active search.

---

**[7] — Results List**  
Displays all found files.  
Double-clicking a result opens the file.

---

**[8] — Refine Search Button**  
Allows you to search *within the results* using the field in [9].

---

**[9] — Results Filter Input**  
Enter a specific keyword to filter the existing results.

---

**[10] — Show More Results Button**  
Only the first 100 results are shown at once.  
This button reveals the remaining results.

---

# GreedSearcher — Greedy by nature, precise by design.
