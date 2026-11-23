A high-performance, managed autoextendable string library for C.

fs.h
 provides a robust and flexible way to handle strings in C. Unlike standard char*, the fs structure treats strings as managed objects, keeping track of their length, capacity, and memory ownership. This prevents common errors like $O(n)$ strlen() calls and frequent heap reallocations.

🚀 Key Features
Memory Awareness: Built-in management for different memory segments:
FS_FLAG_STATIC: For string literals.
FS_FLAG_ALLOC: For dynamic heap-allocated strings.
FS_FLAG_LOCAL: For stack-allocated fixed-size buffers.
Formatted Manipulation: Powerful sprintf-like capabilities with automatic buffer expansion (fs_sprintf, fs_sprintf_concat).
String Operations:
Concatenation: fs_cat, fs_catstr, fs_rev_catstr.
Substrings: fs_substr, fs_left, fs_newsubstr.
Padding: fs_lpad, fs_rpad.
Advanced Searching:
Substring search: fs_instr, fs_iinstr (insensitive).
Character search: fs_chr, fs_rchr (reverse search).
Serialization: Direct support for saving/loading strings via files or custom fs formats.
Search Algorithms: Case-sensitive and case-insensitive search implementations.
🛠 The fs Structure
The core of the library is the fs struct:

typedef struct fs {
    size_t      len;  // Logical length (excluding null terminator)
    size_t      sz;   // Total capacity (including null terminator)
    int         flags; // Memory ownership and state flags
    char       *v;    // Pointer to the string buffer
} fs;
📖 Usage Examples
1. Initialization
Depending on whether you need a static literal or a dynamic buffer:


// Static literal (No allocation, fast)
fs s_lit = FSLITERAL("Hello World");

// Dynamic buffer (Heap allocated, can grow)
fs s_dyn = FS(); 
fs_sprintf(&s_dyn, "Value: %d", 42);

// Local stack-based buffer
fslocal(my_str, 100);
2. String Manipulation

fs main_str = fscopy("The quick brown fox");
fs target = FS();

// Concatenate
fs_cat(&target, main_str);
fs_catstr(&target, " jumps over the lazy dog");

// Substring
fs new_part = fs_newsubstr(&target, 4, 13); // "quick brown"

// Padding
fs_lpad(&target, 20, "0"); // "00000000000The quick..."

fs_free(&target);
3. Searching

fs text = fscopy("Programming in C is fun");

// Substring search
long pos = fs_instr(&text, "in C");
if (pos != -1) {
    printf("Found at: %ld\n", pos);
}

// Character search (Reverse)
long char_pos = fs_rchr(&text, 'n');
4. Serialization

// Save to file
fs_save("data.txt", &text);

// Load from file
fs buffer = fs_load("data.txt");
⚙️ Complexity Analysis
| Operation | Complexity | Note | 
| :--- | :--- | :--- | 
| fs_len | $O(1)$ | No strlen() overhead | 
| fs_cat | $O(n)$ | Linear concatenation | 
| fs_sprintf | $O(n)$ | Formatted write | 
| fs_instr | $O(n \cdot m)$ | Based on strstr | 
| fs_chr | $O(n)$ | Linear scan |

⚠️ Safety Notes
Memory Ownership: Always use fs_free() for any fs object initialized with FS() or fs_heapcreate(). Using fs_free() on a FSLITERAL is safe (it will do nothing).
Buffer Overlap: Avoid performing operations where the source and destination fs buffers overlap in memory to prevent undefined behavior.
Null Termination: fs structures are designed to always maintain a null terminator \0 at v[len].