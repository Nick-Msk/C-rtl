Unified Data Source Abstraction (DS)
The 
ds.h
 module provides a polymorphic interface for sequential data access in C. It abstracts the underlying storage mechanism, allowing your application to treat files, memory buffers, and dynamic strings as a single, unified "Data Source" (DS).

By using the DS abstraction, you can write generic algorithms (parsers, compressors, encoders) that don't care whether they are reading from a disk or a network buffer.

💎 Key Features
Polymorphism: A single interface for multiple data types.
Unified I/O: Use the same functions (dsgetc, dsputc, dswrite) regardless of the source.
Smart Ownership: Automatic management of resource lifetimes (closing files, freeing dynamic strings).
Position Management: Seamlessly seek to positions, save/restore positions, and skip characters (whitespace/newlines) across different media.
Extensibility: Easily extensible to support new media types without changing the core logic of your application.
📂 Supported Data Sources
The DS module supports four primary modes:

| Type | Description | Ownership Rule | 
| :--- | :--- | :--- | 
| DS_FILE | Standard C FILE* stream | Owns the file (calls fclose on dsFree). | 
| DS_FS | Dynamic fs buffer | Owns the buffer (calls fs_free on dsFree). | 
| DS_STR | Mutable char* buffer | Does NOT own the memory. | 
| DS_CONSTSTR | Constant const char* buffer | Does NOT own the memory. |

🛠 Usage Examples
1. Reading from a File

Apply
DS reader = dsCreateFilename("config.txt", "r");
if (reader.type != DS_UNK) {
    char c = dsgetc(&reader);
    // ... processing ...
    dsFree(&reader); // Automatically closes the file
}
2. Reading from a String (Memory)

Apply
char buffer[] = "Hello World";
DS reader = dsInitstr(&buffer);

int c = dsgetc(&reader); // returns 'H'
dsFree(&reader);        // Resets structure, does NOT free 'buffer'
3. Writing to a Dynamic Buffer (FS)

Apply
// Initialize a dynamic buffer as a Data Source
DS writer = dsCreatefs(fs_heapcreate());

dsputc('A', &writer);
dsputc('B', &writer);

// Release the buffer back to an 'fs' object for further use
fs my_fs;
dsReleaseFs(&my_fs, &writer); 
// my_fs now contains "AB"
fs_free(&my_fs);
4. Position Management

Apply
DS ds = dsCreatef("data.bin", "rb");
dsSavepos(&ds); // Save current position

// ... some operations ...

dsRestorepos(&ds, saved_pos); // Return to saved position
dsFree(&ds);
⚠️ Important Rules
Ownership Awareness: Always remember that dsFree() will close a file if it was opened via ds_FILE, but it will not free a char* if it was initialized via DS_STR.
Error Handling: Functions return false or error codes if the operation fails (e.g., reaching EOF or attempting an unsupported operation on a specific type).
DS_FS Ownership: When using DS_FS, the DS structure takes ownership of the fs structure. Use dsReleaseFs to transfer ownership back to your own fs object.
🛠 Complexity
| Operation | Complexity | Note | 
| :--- | :--- | :--- | 
| dsgetc / dsputc | $O(1)$ | Single character access | 
| dsRestorepos | $O(1)$ | Direct seek / index update | 
| dsWrite | $O(n)$ | Linear copy | 
| dsFree | $O(1)$ | Constant time (or $O(\text{allocation})$ for FS) |