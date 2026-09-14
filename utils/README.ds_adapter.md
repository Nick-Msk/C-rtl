The DS-FS Bridge: Serialization & Parsing

ds_adapter.h
 provides the essential "glue" between the universal Data Source (DS) abstraction and the managed FastString (fs) structure.

While DS allows you to read/write data generically, ds_adapter provides specialized logic to convert structured, escaped, and formatted fs objects into linear byte streams (and vice versa). This is the core module used for Data Persistence and Network Communication.

🛠 Core Capabilities
1. Serialization (Data $\to$ Stream)
Converts an fs object into a formatted, safe-for-transmission string.

Format: FS(<length>): "<escaped_content>"
Escaping: Automatically handles special characters (\n, \t, \", \\) to ensure the resulting stream can be parsed back without ambiguity.
Functions:
fs_dsserialize: The primary serializer.
fs_dstechprint: Serializes with an added header/name for debugging.
2. Deserialization (Stream $\to$ Data)
Reconstructs an fs object from a formatted DS stream.

Functions:
fs_dsload: The primary deserializer.
dsParseQuotedLimfs: Extracts quoted, escaped strings from a stream.
dsParseUnlimfs: Extracts unquoted strings.
3. Advanced Parsing (Structured Extraction)
Provides specialized parsers to extract typed data directly from a DS stream without manual pointer arithmetic:

Primitives: dsParseInt, dsParseLong, dsParseDouble, dsParseChar.
Complex types: dsParseV64 (for polymorphic value64 objects).
Words: dsParseWord for whitespace-delimited tokens.
⚖️ Transactional Safety vs. Performance
One of the most critical features of this adapter is the use_buffer mechanism used in parsing functions (e.g., fs_dsload, dsParseQuotedLimfs).

When parsing data from a stream into an fs object, you must choose a mode:

| Mode | Parameter | Behavior | Complexity | Best For... | 
| :--- | :--- | :--- | :--- | :--- | 
| Transactional | use_buffer = true | Data is first loaded into a temporary buffer. The destination fs is only updated if the entire sequence is valid. | $O(n)$ (extra copy) | Critical data where partial/corrupted reads are unacceptable. | 
| Zero-Copy | use_buffer = false | Data is written directly into the destination fs buffer. | $O(1)$ (no extra copy) | High-performance scenarios where performance is more critical than atomicity. |

[!WARNING] In Zero-Copy mode, if parsing fails halfway through (e.g., due to a malformed escape sequence), the destination fs object will be left in a partially-written/corrupted state.

🚀 Key API Summary
| :--- | :--- | :--- | :--- | 
| dsPrintf | DS*, fmt... | int | Formatted writing to a DS stream. | 
| dsScanf | DS*, fmt... | int | Formatted reading from a DS stream. | 
| fs_dswrite | DS*, fs* | long | Fast bulk transfer from fs to DS. | 
| fs_dsserialize| DS*, fs* | long | Convert fs to a formatted text stream. | 
| fs_dsload | DS*, fs*, bool| long | Convert formatted text stream to fs. |