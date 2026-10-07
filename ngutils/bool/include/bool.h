/**
 * @file bool.h
 * @brief Tiny helpers to convert between @c bool and its string form.
 */

#ifndef BOOL_H_
#define BOOL_H_

#include <stdbool.h>

#if defined(_MSC_VER)
  #include <string.h>
  static inline int bool_streq_ci(const char *a, const char *b) {
      return _stricmp(a, b);
  }
#else
  #include <strings.h>
  static inline int bool_streq_ci(const char *a, const char *b) {
      return strcasecmp(a, b);
  }
#endif

/**
 * @brief Convert a @c bool to its string representation.
 *
 * @param[in] v  Value to convert.
 *
 * @return Pointer to a string literal: @c "true" or @c "false".
 *
 * @warning The returned pointer refers to a static string literal.
 *          Do not free or modify it.
 *
 * @note No allocation is performed; safe to call from any context.
 *
 * @code
 *   printf("%s\n", bool_str(true));   // prints: true
 * @endcode
 */
static inline const char*       bool_str(bool v){
	return v ? "true" : "false";
}

/**
 * @brief Parse a string into a @c bool.
 *
 * Accepts @c "true" and @c "false" (case-insensitive). Does not trim
 * whitespace and does not accept synonyms such as @c "yes", @c "on",
 * or @c "1".
 *
 * @param[in]  str  Input string. May be @c NULL.
 * @param[out] out  Output pointer. Must not be @c NULL.
 *
 * @retval true   Parsing succeeded; @p *out holds the parsed value.
 * @retval false  Parsing failed. @p *out is left untouched. Failure
 *                occurs when @p str is @c NULL, @p out is @c NULL,
 *                or @p str is not a recognized literal.
 *
 * @code
 *   bool ok;
 *   bool v = bool_tryparse("True", &ok);
 *   if (ok) { ... }   // v == true
 * @endcode
 */
static inline bool bool_tryparse(const char *str, bool *out) {
	if (!str)
		return false;
	enum { BOOL_UNK, BOOL_TRUE, BOOL_FALSE } res = BOOL_UNK;
	if (bool_streq_ci(str, "true") == 0)
		res = BOOL_TRUE;
	else if (bool_streq_ci(str, "false") == 0)
		res = BOOL_FALSE;
	if (res == BOOL_UNK)
		return false;
	if (out)
		*out = res == BOOL_TRUE;
	return true;
}

/**
 * @brief Parse a string into a @c bool, with a fallback value.
 *
 * Convenience wrapper around @ref bool_tryparse that never fails:
 * returns @p def when the input cannot be parsed.
 *
 * @param[in] str  Input string. May be @c NULL.
 * @param[in] def  Value returned when parsing fails.
 *
 * @return Parsed value on success, @p def on failure.
 *
 * @code
 *   bool debug = bool_parsedef(getenv("DEBUG"), false);
 * @endcode
 */
static inline bool bool_parsedef(const char *str, bool def) {
	bool res;
	if (!bool_tryparse(str, &res) )
		return def;
	else
		return res;
}

#endif /* !BOOL_H */
