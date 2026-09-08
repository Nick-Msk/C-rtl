/**
 * @file ds.h
 * @brief Universal interface for reading/writing data from various sources.
 *
 * This module provides a @ref DS abstraction that allows the same 
 * code to read/write characters from various sources:
 *  - @c DS_FILE: Standard C I/O streams (FILE*).
 *  - @c DS_STR: Mutable memory buffers (char*).
 *  - @c DS_CONSTSTR: Constant memory buffers (const char*).
 *  - @c DS_FS: Automated dynamic strings (via @ref fs).
 *
 * @section ownership Ownership Rules:
 * - @b DS_FILE: The @ref DS object __owns__ the @c FILE* pointer and will 
 *   automatically call @c fclose() during @ref dsFree().
 * - @b DS_FS: The @ref DS object __owns__ the @ref fs structure and will 
 *   automatically call @ref fs_free() during @ref dsFree().
 * - @b DS_STR / @c DS_CONSTSTR: The @ref DS object __DOES NOT__ own the 
 *   memory buffer. The user is responsible for the lifetime of the buffer. 
 *   @ref dsFree() will only reset the DS structure.
 *
 * @note This is low-level code. Errors are reported via @c EOF or return codes.
 */

#ifndef _DS_H
#define _DS_H

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "fileutils.h"

#ifndef NO_FSDS
    #include "fs.h"
#endif /* !NO_FSDS */

// ---------------------------------------------------------------------------------
// --------------------------- Public datasource API -------------------------------
// ---------------------------------------------------------------------------------

/**
 * @enum DSType
 * @brief Defines the type of the data source. Now DS_FILE and DS_BUFFER are suppoted
 */
typedef enum {
    DS_UNK = 0,   /**< just a plug */
    DS_FILE,     /**< Data source is a standard file (FILE*). */
    DS_STR,      /**< Data source is a memory buffer (null-terminated string). */
    DS_CONSTSTR, /**< Data source is a memory const buffer (null-terminated string). */
    DS_FS        /**< FS, suppoerted via ifndef NO_FSDS */
} DSType;

/**
 * @brief Returns the string representation of the @ref DSType.
 * @param typ The type to convert.
 * @return A constant string representing the type.
 */
static inline const char               *dsTypeName(DSType typ) {
    switch (typ) {
        CASE_RETURN(DS_UNK);
        CASE_RETURN(DS_FILE);
        CASE_RETURN(DS_STR);
        CASE_RETURN(DS_CONSTSTR);
        CASE_RETURN(DS_FS);
        default: return "Unknown DS type";
    }
}

/**
 * @struct DS
 * @brief A unified wrapper for multiple data source types.
 *
 * Provides a consistent interface for sequential data access (reading,
 * writing, and seeking) regardless of whether the source is a file, 
 * a string, or a dynamic buffer.
 */
typedef struct DS {
    DSType type;                /**< Determines which union member is active. */
    union {     
        struct {    
            FILE *fp;               /**< Pointer to the file (used in @c DS_FILE mode). */
            off_t   filesavepos;       /**< For save FILE position */
        };
        struct {
            union {
                struct {
                    union {
                        char          *ptr; /**< Pointer to the start of the string (used in @c DS_STR mode). */
                        const char    *constptr; /**< Pointer to the start of the constant string (used in @c DS_STR mode). */
                    }; 
                    size_t  cap;        /**< Capacity of STR and CONSTSTR */
                };
#ifndef NO_FSDS
                fs    s;                /**< fs autoextendable string */
#endif  /* !NO_FSDS */             
            };
            size_t  pos;        /**< Current read position in the buffer. */
            bool    memowner;   /**< Owner flag for FS, STR and CONSTSTR */
        };                  /**< Buffer details. */
    };
} DS;

/** @name Macro Constructors
 * Macros for quick initialization of DS objects.
 * @{ */
#define DS(...) (DS) {.type = DS_STR, .pos = 0L, .ptr = NULL, .memowner = false, __VA_ARGS__}
#define DSFILE(...) (DS) {.type = DS_FILE, .fp = NULL, .memowner = false, __VA_ARGS__}
#define DSSTR(...) (DS) {.type = DS_STR, .pos = 0L, .ptr = NULL, .cap = 0L, .memowner = false, __VA_ARGS__}
#define DSCONST(...) (DS) {.type = DS_CONSTSTR, .pos = 0L, .constptr = NULL, .memowner = false, __VA_ARGS__}

#ifndef NO_FSDS
    #define DSFS(...) (DS) {.type = DS_FS, .pos = 0L, .s = FS(), .memowner = false, __VA_ARGS__}
#endif  /* !NO_FSDS */    
/** @} */

/**
 * @brief Initializes a DS object for reading from a file.
 * 
 * @param[out] pds Pointer to the DS structure.
 * @param[in]  fp Pointer to an open FILE pointer.
 * @return true if successful, false otherwise.
 */
extern bool                     dsInitf(DS *restrict pds, FILE *restrict fp);

/**
 * @brief Creates and initializes a DS object for a file.
 * 
 * @param[in]  fp Pointer to an open FILE pointer.
 * @return A initialized DS object (or a zeroed DS on failure).
 */
static inline DS                dsCreatef(FILE *fp) {
    DS      res = DSFILE();
    if (dsInitf(&res, fp) )
        return res;
    res = (DS) {0};
    return res;
}

/**
 * @brief Initializes a DS object for a file given its filename.
 * 
 * @param[out] pds Pointer to the DS structure.
 * @param[in]  fname Filename to open.
 * @param[in]  mode File mode (e.g., "r", "w+", "a").
 * @return true if successful, false otherwise.
 */
extern bool                     dsInitFilename(DS *restrict pds, const char *restrict fname, const char *restrict mode);

/**
 * @brief Creates and initializes a DS object for a file by filename.
 * 
 * @param[in]  fname Filename to open.
 * @param[in]  mode File mode.
 * @return A initialized DS object (or a zeroed DS on failure).
 */
static inline DS                dsCreateFilename(const char *restrict fname, const char *restrict mode) {
    DS      res = DSFILE();
    if (dsInitFilename(&res, fname, mode) )
        return res;
    res = (DS) {0};
    return res;
}

/**
 * @brief Initializes a DS object for a mutable string buffer.
 * 
 * @param[out] ds Pointer to the DS structure.
 * @param[in]  buf Pointer to the buffer.
 * @param[in]  cap Capacity of the buffer (if 0, uses strlen of buf).
 * @return true if successful, false otherwise.
 */
extern bool                     dsInitstrCap(DS *restrict ds, char *restrict buf, size_t cap);

/**
 * @brief Initializes a DS object for a mutable string buffer (auto-detect capacity).
 * 
 * @param[in]  buf Pointer to the null-terminated string.
 * @return true if successful, false otherwise.
 */
static inline bool              dsInitstr(DS *restrict ds, char *restrict buf) {
    return dsInitstrCap(ds, buf, 0L);
}
/**
 * @brief Initializes a Datasource (DS) object for reading from a c-string.
 * 
 * @param[in]  buf Pointer to a null-terminated string (const char*).
 * @param[in]  cap Capacity must be > 0L
 * @return     A initialized DS object (or a zeroed DS on failure).
 */
static inline DS                dsCreatestrCap(char *buf, size_t cap) {
    DS      tmp = DSSTR();
    if (cap > 0L && dsInitstrCap(&tmp, buf, cap) )
        return tmp;
    DS      empty = {0};
    return empty;
}

static inline DS                dsCreatestrAlloc(size_t cap) {
    DS       tmp = DSSTR();
    char    *buf = malloc(cap);
    if (!buf)
        return userraise(tmp, ERR_UNABLE_ALLOCATE, "unable allocation %zu", cap);
    tmp = dsCreatestrCap(buf, cap);
    tmp.memowner = true;

    return logsimpleret(tmp, "DS/str owner is %s created", buf ? "": "NOT");
}

/**
 * @brief Initializes a Datasource (DS) object for reading from a c-string.
 * 
 * @param[in]  buf Pointer to a null-terminated string (const char*).
 * @return      A initialized DS object (or a zeroed DS on failure).
 */
static inline DS                dsCreatestr(char *buf) {
    DS      tmp = DSSTR();
    if (dsInitstr(&tmp, buf) )
        return tmp;
    DS      empty = {0};
    return empty;
}
/**
 * @brief Initializes a Datasource (DS) object for reading from a const c-string.
 * 
 * @param[out] DS Pointer to the Datasource (DS) structure to be initialized.
 * @param[in]  buf Pointer to a null-terminated string (const char *).
 * @return true if successful, false otherwise.
 */
extern bool                     dsInitconst(DS *restrict pds, const char *restrict buf);
/**
 * @brief Initializes a Datasource (DS) object for reading from a const c-string.
 * 
 * @param[in]  buf Pointer to a null-terminated string (const char*).
 * @return     A initialized DS object (or a zeroed DS on failure).
 */
static inline DS                dsCreateconst(const char *buf) {
    DS      tmp = DSCONST();
    if (dsInitconst(&tmp, buf) )
        return tmp;
    DS      empty = {0};
    return empty;
}
// ----------------------------------------------------------------------
#ifndef NO_FSDS

/**
 * @brief Initializes a DS object for a dynamic @ref fs buffer.
 * 
 * @param[out] pds Pointer to the DS structure.
 * @param[in]  s Pointer to the fs object.
 * @return true if successful, false otherwise.
 * @note The @ref fs structure is moved into the DS object.
 */
extern bool                     dsInitfs(DS *restrict pds, fs *restrict s);
/**
 * @brief Creates and initializes a DS object for a dynamic @ref fs buffer.
 * 
 * @param[in]  s Pointer to the fs.
 * @return     A initialized DS object (or a zeroed DS on failure).
 * @note       fs s is MOVED into DS structure!
 */
static inline DS                dsCreatefs(fs *s) {
    DS      tmp = DSFS();
    if (dsInitfs(&tmp, s) )
        return tmp;
    DS      empty = {0};
    return empty;
}

static inline DS                dsCreatefsAlloc(void) {
    fs     *buf = fs_create();
    DS      tmp = dsCreatefs(buf);
    tmp.memowner = true;
    return logsimpleret(tmp, "DS/Fs memory owner is created");
}

/**
 * @brief Creates a new, empty dynamic @ref fs buffer.
 * 
 * @return A new empty @ref fs object.
 */
static inline DS                dsCreatefsempty(void) {
    fs  tmp = FS();
    return dsCreatefs(&tmp);
}

#endif  /* !NO_FSDS */   

/**
 * @brief Releases resources associated with the @ref DS object.
 * 
 * This function performs a deep release of the data source:
 *  - For @c DS_FILE: Closes the file stream using @c fclose().
 *  - For @c DS_FS: Releases the dynamic buffer using @ref fs_free().
 *  - For @c DS_STR / @c DS_CONSTSTR: Resets the structure to zero.
 *
 * @param[in,out] pds Pointer to the DS structure to be cleared.
 * @note After calling this function, the @ref DS object is reset to a 
 *       zero-initialized state.
 */
static inline void              dsFree(DS *pds) {
    if (pds) {
#ifndef NO_FSDS
        if (pds->type == DS_FS)
            fsfree(pds->s);
#endif  /* !NO_FSDS */   
        if (pds->type == DS_FILE)
            fclose(pds->fp);
        if (pds->type == DS_STR && pds->memowner)
            free(pds->ptr);
        *pds = DS();
    }
}

/**
 * @brief Extracts/releases the @ref fs buffer from a @ref DS object.
 * 
 * This function allows transferring ownership of the internal @ref fs 
 * structure to an external object, or simply destroying it.
 * 
 * @param[out] dst Pointer to the destination @ref fs structure. 
 *                 If @c NULL, the @ref DS buffer is destroyed via @ref fs_free.
 * @param[in,out] pds Pointer to the source @ref DS object (must be @c DS_FS).
 * 
 * @return true if the operation was successful, false if the DS is NULL 
 *         or the type is not @c DS_FS.
 * 
 * @warning This function modifies the @c pds object. Even on success, 
 *          the @c pds structure is reset to its default state to prevent 
 *          dangling pointers or double-free errors.
 * 
 * @note This function is only valid for @c DS_FS type.
 */
static inline bool              dsReleaseFs(fs *restrict dst, DS *restrict pds) {
    if (pds == NULL)
        return userraise(false, ERR_NULL_INPUT, "Ds is null");

#ifndef NO_FSDS

    if (pds->type != DS_FS)
        return userraise(false, ERR_UNSUPPORTED_TYPE, 
            "%d/%s isn't supported", pds->type, dsTypeName(pds->type));
    if (dst == NULL)        // just free
        fsfree(pds->s);
    else {
        fs_free(dst);   // to avoid leaking
        fs_moveto(dst, &pds->s);
    }

#endif  /* !NO_FSDS */   

    *pds = DS();      // reset
    return true;
}

static inline char               *dsReleaseStr(DS *restrict pds) {
    if (pds == NULL)
        return userraise(NULL, ERR_NULL_INPUT, "Ds is null");
    if (pds->type != DS_STR)
        return userraise(NULL, ERR_UNSUPPORTED_TYPE, 
            "%d/%s isn't supported", pds->type, dsTypeName(pds->type));
    char    *tmp = pds->ptr;
    *pds = DS();
    return tmp;
}

/**
 * @brief Convenience macro to call @ref dsFree on a pointer.
 * @param ds Pointer to the DS object.
 */
#define DSFREE(ds) dsFree(&(ds))

// ----------------------------------------------------------------------

/**
 * @brief Reads the next character from the source.
 * 
 * @param[in,out] DS Pointer to the data source.
 * @return The next character (0-255) or @c EOF if the end of the source is reached.
 * @note For @c DS_BUFFER mode, EOF is returned when the '\0' character is encountered.
 */
extern int                     dsgetc(DS *pds);
/**
 * @brief Pushes a character back into the stream or rolls back the position.
 * 
 * @param[in]  c The character to push back.
 * @param[in,out] ds Pointer to the data source.
 * @return The character 'c' if successful, or @c EOF if the operation failed.
 */
extern int                     dsungetc(int c, DS *pds);

/**
 * @brief Pushes ANY character back into the stream (DS_STR, DS_FS)
 *  
 * @param[in]  c The character to push back.
 * @param[in,out] ds Pointer to the data source.
 * @return The character 'c' if successful, or @c EOF if the operation failed.
 */
extern int                      dsreplacec(int c, DS *pds);

/**
 * @brief Pushes ANY character into the stream (DS_STR, DS_FS, DS_FILE)
 *  
 * @param[in]  c The character to push back.
 * @param[in,out] ds Pointer to the data source.
 * @return The 1 if successful, 0 if no change (FS_STR) or @c EOF if the operation failed.
 */
extern int                      dsputc(int c, DS *pds);
/**
 * @brief Pushes Ecraned sequence for character into the stream
 *  
 * @param[in]  c The ecraning character to push back.
 * @param[in,out] ds Pointer to the data source.
 * @return The character 'c' if successful, or @c EOF if the operation failed.
 */
extern int                      dsputcEscape(int c, DS *pds);

extern bool                     dsgetcEscaped(DS *restrict in, int *c);
// NOT USE! TO BE REMOVED!!!
// extern int                      dsparseEscaped(DS *restrict in, bool *restrict error);

/**
 * @brief Pushes c-string into the stream
 *  
 * @param[in]  ptr The pointrer to c-string.
 * @param[in]  len Real length of c-string
 * @param[in,out] ds Pointer to the data source.
 * @return The character 'c' if successful, or @c EOF if the operation failed.
 */
extern long                     dswrite(DS *restrict out, const char *restrict ptr, size_t len);

/**
 * @brief Tried to Read c-string from the stream
 *  
 * @param[in]  literal c-string
 * @param[in,out] in Pointer to the data source.
 * @return The true if successful read, false - otherwise.
 */
extern bool                     dsExpect(DS *restrict in, const char *restrict literal);

extern size_t                   dsGetcap(const DS* pds);
/**
 * @brief Debugging print implementation.
 * @param out output FILE *
 * @param pds datasource
 * @param Name displayed name
 * Returns the number of characters printed.
 */
extern int                      dsTechFPrint(FILE *restrict out, const DS *restrict pds, const char *restrict name);
static inline int               dsTechPrint(const DS *restrict pds, const char *restrict name) {
    return dsTechFPrint(stdout, pds, name);
}

#define DSTECHFPRINT(out, ds)   dsTechFPrint((out), &(ds), #ds)   
#define DSTECHPRINT(ds)         dsTechPrint( &(ds), #ds)

/**
 * @brief Return pointer to buffer for DS_STR, DS_FS and DS_CONSTSTR
 *  
 * @param[in,out] ds Pointer to the data source.
 * @return pointer to c-str buffer
 */
static inline const char       *dsStrbuf(const DS *pds) {
    switch (pds->type) {
        case DS_STR:
            return pds->ptr;
        case DS_CONSTSTR:
            return pds->constptr;
        case DS_FS:
            return pds->s.v;
        default:
            return NULL;
    }
}
/**
 * @brief Return pointer to buffer for DS_STR and DS_CONSTSTR
 *  
 * @param[in,out] ds Pointer to the data source.
 * @return true if DS is c-string source
 */
static inline bool              dsIsstr(const DS *pds) {
    return pds->type == DS_CONSTSTR || pds->type == DS_STR;
}

/**
 * @brief Return the current position of the data source.
 * 
 * @param pds Pointer to the data source.
 */
static inline off_t             dsGetpos(const DS *pds) {
    switch (pds->type) {
        case DS_FILE:
            return ftell(pds->fp);
        case DS_STR: case DS_CONSTSTR: case DS_FS:
            return pds->pos;
        default:
            return logsimpleerr(0L, "Not suppotred type %d/%s", pds->type, dsTypeName(pds->type));
    }
}
/**
 * @brief Saves the current position of the data source for later restoration.
 * 
 * @param pds Pointer to the data source.
 */
static inline off_t            dsSavepos(DS *pds) {
    return dsGetpos(pds);
}

/**
 * @brief Restores the position of the data source to the last saved state.
 * 
 * @param pds Pointer to the data source.
 * @param savepos previously saved position
 * @return true if successful, false otherwise.
 */
static inline bool              dsRestorepos(DS *pds, off_t savepos) {
    switch (pds->type) {
        case DS_FILE:
            return fseek(pds->fp, savepos, SEEK_SET) == 0;
        case DS_STR: case DS_CONSTSTR: case DS_FS:
            pds->pos = savepos;
            break;
        default:
            return logsimpleerr(false, "Not suppotred type %d/%s", pds->type, dsTypeName(pds->type));
    }
    return true;
}

/**
 * @brief Resets the position of the data source to the beginning.
 * 
 * @param pds Pointer to the data source.
 * @return true if successful, false otherwise.
 */
static inline bool              dsReset(DS *pds) {
    switch (pds->type) {
        case DS_FILE:
            return fseek(pds->fp, 0L, SEEK_SET) == 0;
        case DS_STR: case DS_CONSTSTR: case DS_FS:
            pds->pos = 0;
            break;
        default:
            return logsimpleerr(false, "Not suppotred type %d/%s", pds->type, dsTypeName(pds->type));
    }
    return true;
} 

/**
 * @brief Skips the next character if it is a newline.
 * 
 * This function consumes the next character from the source if it is 
 * a '\n' character. If the character is not a newline or if @c EOF 
 * is reached, the character is pushed back into the stream and 
 * @c false is returned.
 * 
 * @param[in,out] ds Pointer to the @ref DS source.
 * @return true if a newline character was consumed, false otherwise.
 */
static inline bool               dsSkipNl(DS *ds) {
    int c = dsgetc(ds);
    if (c == EOF)
        return false;
    if (c == '\n')
        return true;
    dsungetc(c, ds);           // return back
    return false;
}

/**
 * @brief Skips the next character if it is a whitespace character.
 * 
 * This function consumes the next character from the source if it 
 * satisfies the @c isspace() condition. If the character is not 
 * whitespace or if @c EOF is reached, the character is pushed back 
 * into the stream and @c false is returned.
 * 
 * @param[in,out] ds Pointer to the @ref DS source.
 * @return true if a whitespace character was consumed, false otherwise.
 */
static inline bool               dsSkipSpace(DS *ds) {
    int c = dsgetc(ds);
    if (c == EOF)
        return false;
    if (isspace(c))
        return true;
    dsungetc(c, ds);           // return back
    return false;
}

#endif /* !_DS_H */
