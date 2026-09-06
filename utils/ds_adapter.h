/**
 * @file ds_fs.h
 * @brief DS - fs adapter read/write.
 *
 * This module provides a @ref stdio/fs API via standartized DS function
 * DS - fs, ds -> FILE *.
 * DS_FILE, DS_FS       allowes to R/W (unlimited)
 * DS_CONSTSTR          alloes only R
 * DS_STR               alloes only R, and W only limited by '\0'. No reallocation.
 */

#ifndef _DS_ADAPTER_H
#define _DS_ADAPTER_H

#include <stdarg.h>

#include "ds.h"
#include "checker.h"
#include "common.h"
#include "value64.h"
#include "fs.h"

// ---------------------------------------------------------------------------------
// --------------------------- Public DS - fs adapter API --------------------------
// ---------------------------------------------------------------------------------

// --------------------------------- CONSTANTS AND GLOBALS --------------------------

// ---------------------------------- TYPES -----------------------------------------

// ------------------------- CONSTRUCTOTS/DESTRUCTORS -------------------------------

// -------------------- ACCESS AND MODIFICATORS -------------------------------------

/**
 * @brief DS printf wrapper
 * @note DS_FILE, DS_FS, DS_STR are allowed
 * @note This is stream function, always used ds->pos as start point
 */
extern int                         dsPrintf(DS *restrict ds, const char *restrict msg, ...) __attribute__ ((format (printf, 2, 3)));

/**
 * @brief Formatted input from a DS source (scanf analog).
 *
 * DS_FILE uses vfscanf() directly.
 * DS_STR, DS_CONSTSTR and DS_FS use an internal helper that wraps
 * fmemopen() + vfscanf() + ftell() in order to automatically advance
 * the read position.
 *
 * @param pds pointer to the data source
 * @param msg scanf‑style format string
 * @param ...  pointers to store the parsed values
 * @return     number of successfully matched items, or -1 on error
 */
extern int                         dsScanf(DS *restrict pds, const char *restrict msg, ...) __attribute__ ((format (scanf, 2, 3)));

// -------------------------------------- fs adapter ------------------------------------------------

/**
 * @brief Writes the content of an @ref fs object into a @ref DS stream.
 *
 * This is an adapter function that bridges the @ref fs data structure 
 * to the universal @ref DS interface. It performs a direct data transfer 
 * from the source to the destination stream.
 *
 * @param[in,out] out The destination @ref DS stream. Must be a writable 
 *                    type (e.g., @c DS_FILE, @c DS_STR, or @c DS_FS).
 * @param[in]     s   The source @ref fs object containing the data to be written.
 *
 * @return The number of bytes successfully written on success, or a negative 
 *         error code if the operation failed (e.g., invalid output type 
 *         or null pointers).
 *
 * @note This function is more efficient than @ref dsputc for bulk data 
 *       transfers and is preferred when working with @ref fs objects.
 * @warning If @p out is a read-only stream (e.g., @c DS_CONSTSTR), 
 *          the function will return an error.
 */
extern long                        fs_dswrite(DS *restrict out, const fs *restrict s);

/**
 * @brief Serializes an @ref fs object into a @ref DS stream.
 *
 * This function transforms a raw data source (@ref fs) into a formatted 
 * string suitable for storage or transmission. It converts the data into 
 * a structured text format with escaping to ensure data integrity.
 *
 * The resulting serialized format is:
 * @code
 * FS(<length>): "<escaped_content>"
 * @endcode
 * 
 * where:
 * - <length> is the number of bytes in the original @ref fs object.
 * - <escaped_content> is the original data, where special characters 
 *   (like \n, \t, ", \\) are escaped with a backslash.
 *
 * @details
 * The function performs the following steps:
 * <ol>
 *   <li>Writes the header: @c FS("<length>"): "</li>
 *   <li>Iterates through the source, escaping characters via @ref dsputcEscape.</li>
 *   <li>Writes the footer: @" and a newline character.</li>
 * </ol>
 *
 * @param[in,out] out The destination @ref DS stream.
 * @param[in]     s   The source @ref fs object to be serialized.
 *
 * @return The total number of bytes written (including header, footer, 
 *         and escape characters) on success, or -1 on error.
 *
 * @note This function is non-idempotent as it modifies the state of the 
 *       @ref DS stream.
 * @warning If an error occurs during the process (e.g., disk full, 
 *          stream error), the function returns -1 and the output 
 *          stream may be left in a partially written state.
 */
extern long                        fs_dsserialize(DS *restrict out, const fs *restrict s);
/**
 * @brief Loads a serialized data string from a source into an @ref fs object.
 *
 * This function parses a data stream following the custom serialization format:
 * @code
 * FS(<length>): "<content>"
 * @endcode
 * It automatically handles de-escaping of special characters (e.g., \n, \t, \", \\).
 *
 * @details The function provides two modes of operation via the @p use_buffer parameter:
 * <ul>
 *   <li><b>Transactional Mode (@p use_buffer = @c true):</b>
 *       The data is first loaded into a temporary intermediate buffer. The @ref dst 
 *       object is only updated (via @ref fs_cat) if the entire sequence is read 
 *       successfully and the actual number of decoded characters matches the 
 *       length specified in the header. This ensures that @ref dst remains 
 *       unmodified if an error occurs.</li>
 *   <li><b>Zero-Copy Mode (@p use_buffer = @c false):</b>
 *       Data is written directly into the @ref dst object's memory. This is 
 *       significantly faster as it avoids an extra memory copy, but it is 
 *       not atomic.</li>
 * </ul>
 *
 * @param[in]  in         Pointer to the input @ref DS source.
 * @param[in,out] dst     Pointer to the destination @ref fs object to be populated.
 * @param[in]  use_buffer Toggle for transactional safety. 
 *                         If @c true, ensures atomicity. 
 *                         If @c false, optimizes for performance.
 *
 * @return The number of bytes successfully loaded on success, or -1 on error.
 *
 * @warning If @p use_buffer is set to @c false, an error during parsing (e.g., 
 *          mismatched length, unexpected EOF, or malformed escape sequence) 
 *          will leave the @ref dst object in a corrupted or partially-written 
 *          state.
 *
 * @note The input must strictly follow the format: 
 *       @code FS(<unsigned_long>): "<escaped_string>" @endcode
 */
extern long                        fs_dsload(DS *restrict in, fs *restrict s, bool use_buffer);
/**
 * @brief Serializes an @ref fs object into a @ref DS stream.
 *
 * This function transforms a raw data source (@ref fs) into a formatted 
 * string suitable for storage or transmission. It converts the data into 
 * a structured text format with escaping to ensure data integrity.
 *
 * The resulting serialized format is:
 * @code
 * FS(<length>): "<escaped_content>"
 * @endcode
 * 
 * where:
 * - <length> is the number of bytes in the original @ref fs object.
 * - <escaped_content> is the original data, where special characters 
 *   (like \n, \t, ", \\) are escaped with a backslash.
 *
 * @details
 * The function performs the following steps:
 * <ol>
 *   <li>Writes the header: @c FS("<length>"): "</li>
 *   <li>Iterates through the source, escaping characters via @ref dsputcEscape.</li>
 *   <li>Writes the footer: @" and a newline character.</li>
 * </ol>
 *
 * @param[in,out] out The destination @ref DS stream.
 * @param[in]     s   The source @ref fs object to be serialized.
 *
 * @return The total number of bytes written (including header, footer, 
 *         and escape characters) on success, or -1 on error.
 *
 * @note This function is non-idempotent as it modifies the state of the 
 *       @ref DS stream.
 * @warning If an error occurs during the process (e.g., disk full, 
 *          stream error), the function returns -1 and the output 
 *          stream may be left in a partially written state.
 */
extern long                        fs_dstechprint(DS *restrict out, const fs *restrict s, const char *restrict name);

// ------------------------------- SCANNERS ----------------------------------------

/**
 * @brief Parses an integer from the provided data source.
 *
 * This function acts as an adapter that handles different data source types 
 * defined in the @c DS structure. It supports:
 * - @c DS_FILE: Reads an integer from a file stream using @c fscanf.
 * - @c DS_STR, @c DS_FS, @c DS_CONSTSTR: Parses an integer from a memory buffer.
 *
 * @param[in,out] pds  Pointer to the @c DS structure containing the data source.
 * @param[out] pval    Pointer to the integer where the parsed value will be stored.
 *
 * @return true if the integer was successfully parsed and stored in @p pval, 
 *         false otherwise.
 * 
 * @note For @c DS_FILE, the file position is advanced after a successful read.
 * @retval false if input pointers are NULL, the source type is unsupported, or parsing fails.
 */
extern bool                        dsParseInt(DS *restrict pds, int *restrict val);

/**
 * @brief Parses an long from the provided data source.
 *
 * This function acts as an adapter that handles different data source types 
 * defined in the @c DS structure. It supports:
 * - @c DS_FILE: Reads an integer from a file stream using @c fscanf.
 * - @c DS_STR, @c DS_FS, @c DS_CONSTSTR: Parses an integer from a memory buffer.
 *
 * @param[in,out] pds  Pointer to the @c DS structure containing the data source.
 * @param[out] pval    Pointer to the long where the parsed value will be stored.
 *
 * @return true if the integer was successfully parsed and stored in @p pval, 
 *         false otherwise.
 * 
 * @note the position is advanced after a successful read.
 * @retval false if input pointers are NULL, the source type is unsupported, or parsing fails.
 */
extern bool                        dsParseLong(DS *restrict pds, long *restrict pval);

/**
 * @brief Parses a unsigned int from the provided data source.
 *
 * This function acts as an adapter that handles different data source types 
 * defined in the @c DS structure. It supports:
 * - @c DS_FILE: Reads an integer from a file stream using @c fscanf.
 * - @c DS_STR, @c DS_FS, @c DS_CONSTSTR: Parses an integer from a memory buffer.
 *
 * @param[in,out] pds  Pointer to the @c DS structure containing the data source.
 * @param[out] pval    Pointer to the unsigned int where the parsed value will be stored.
 *
 * @return true if the integer was successfully parsed and stored in @p pval, 
 *         false otherwise.
 * 
 * @note the position is advanced after a successful read.
 * @retval false if input pointers are NULL, the source type is unsupported, or parsing fails.
 */
extern bool                        dsParseUnsigned(DS *restrict pds, unsigned int *restrict pval);

/**
 * @brief Parses a unsigned long from the provided data source.
 *
 * This function acts as an adapter that handles different data source types 
 * defined in the @c DS structure. It supports:
 * - @c DS_FILE: Reads an integer from a file stream using @c fscanf.
 * - @c DS_STR, @c DS_FS, @c DS_CONSTSTR: Parses an integer from a memory buffer.
 *
 * @param[in,out] pds  Pointer to the @c DS structure containing the data source.
 * @param[out] pval    Pointer to the unsigned long where the parsed value will be stored.
 *
 * @return true if the integer was successfully parsed and stored in @p pval, 
 *         false otherwise.
 * 
 * @note the position is advanced after a successful read.
 * @retval false if input pointers are NULL, the source type is unsupported, or parsing fails.
 */
extern bool                        dsParseUnsignedLong(DS *restrict pds, unsigned long *restrict pval);

/**
 * @brief Parses a double from the provided data source.
 *
 * This function acts as an adapter that handles different data source types 
 * defined in the @c DS structure. It supports:
 * - @c DS_FILE: Reads an integer from a file stream using @c fscanf.
 * - @c DS_STR, @c DS_FS, @c DS_CONSTSTR: Parses an integer from a memory buffer.
 *
 * @param[in,out] pds  Pointer to the @c DS structure containing the data source.
 * @param[out] pval    Pointer to the double int where the parsed value will be stored.
 *
 * @return true if the integer was successfully parsed and stored in @p pval, 
 *         false otherwise.
 * 
 * @note the position is advanced after a successful read.
 * @retval false if input pointers are NULL, the source type is unsupported, or parsing fails.
 */
extern bool                         dsParseDouble(DS *restrict pds, double *restrict pdval);

/**
 * @brief Parses a char from the provided data source.
 *
 * This function acts as an adapter that handles different data source types 
 * defined in the @c DS structure. It supports:
 * - @c DS_FILE: Reads an integer from a file stream using @c fscanf.
 * - @c DS_STR, @c DS_FS, @c DS_CONSTSTR: Parses an integer from a memory buffer.
 *
 * @param[in,out] pds  Pointer to the @c DS structure containing the data source.
 * @param[out] pval    Pointer to the char where the parsed value will be stored.
 *
 * @return true if the integer was successfully parsed and stored in @p pval, 
 *         false otherwise.
 * 
 * @note the position is advanced after a successful read.
 * @retval false if input pointers are NULL, the source type is unsupported, or parsing fails.
 */
extern bool                         dsParseChar(DS *restrict pds, char *restrict pval);

// TODO: not sure about that since V64 has own value64_loadds/value64_loadfile/value64_loadstr
extern bool                         dsParseV64(DS *restrict pds, value64 *restrict pval, value64_type v64typ);

/**
 * @brief Parses a quoted, escaped string from a @ref DS source and stores it in an @ref fs object.
 *
 * This function reads a quoted string from the input stream, processes any 
 * escape sequences (e.g., \n, \t), and writes the decoded content into the 
 * provided @ref fs buffer.
 *
 * @param[in]  pds  Pointer to the source @ref DS stream.
 * @param[in,out] dst Pointer to the destination @ref fs object. 
 *                    The @ref dst->sz will be updated to the actual number 
 *                    of parsed characters upon success.
 *
 * @return The number of characters successfully parsed and stored in @p dst.
 *
 * @note This function is a wrapper around @ref dsHelperParseEscapedString 
 *       and automatically updates the @ref fs->len property.
 * @warning If parsing fails, the input @ref DS stream is rolled back to its 
 *          original position, and the function returns 0.
 */
extern bool                       dsParseQuotedLimitedfs(DS *restrict in, fs *restrict dst, size_t maxlen, bool use_buffer);

static inline bool                dsParseQuotedLimitedfsDirect(DS *restrict in, fs *restrict dst, size_t maxlen) {
    return dsParseQuotedLimitedfs(in, dst, maxlen, false);
}
static inline bool                dsParseQuotedLimitedfsBuffered(DS *restrict in, fs *restrict dst, size_t maxlen) {
    return dsParseQuotedLimitedfs(in, dst, maxlen, true);
}

static inline bool                dsParseQuotedUnlimfsDirect(DS *restrict in, fs *restrict dst) {
    return dsParseQuotedLimitedfs(in, dst, 0L, false);
}
static inline bool                dsParseQuotedUnlimfsBufferre(DS *restrict in, fs *restrict dst) {
    return dsParseQuotedLimitedfs(in, dst, 0L, true);
}

// ------------------------ PRINTERS/CHECKERS ---------------------------------------

// ------------------------------------ ETC. ----------------------------------------


#endif /* !_DS_ADAPTER_H */
