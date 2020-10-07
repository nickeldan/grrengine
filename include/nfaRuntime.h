/**
 * \file        nfaRuntime.h
 *
 * \brief       Regex-matching functions.
 */

#ifndef __GRR_ENGINE_RUNTIME_H__
#define __GRR_ENGINE_RUNTIME_H__

#include <stdbool.h>
#include <sys/types.h>

#include "nfaDef.h"

/**
 * \brief           Determines if the entire string matches the regex.
 *
 * \param nfa       The Grr regex object.
 * \param string    The string (does not have to be null-terminated).
 * \param len       The length of the string.
 * \return          GRR_RET_OK if the string matched the regex.
 *                  GRR_RET_BAD_ARGS is either nfa or string is NULL.
 *                  GRR_RET_BAD_DATA if the string contained a non-printable character.
 */
int
grrMatch(const grrNfa nfa, const char *string, size_t len);

/**
 * \brief            Determines if a string contains a substring which matches the regex.
 *
 * The function will stop processing characters when either it exhausts the specified length, a newline is
 * encountered, or a non-printable character is encountered with tolerant set to false.  If tolerant is true,
 * then the function will skip over such characters and treat them as beginning and end of lines.  For
 * example, if the regex is "^a+" and the string is "aa\x00aaad", then both sequences of "a"'s will be
 * considered matches.  However, they will be considered separate matches and so *end-*start will be 3.
 *
 * \param nfa       The Grr regex object.
 * \param string    The string (does not have to be null-terminated).
 * \param len       The length of the string.
 * \param start     A pointer which will, if not NULL, point to the index of the beginning of the longest
 *                  match if one was found.
 * \param end       A pointer which will, if not NULL, point to the index of the character after the end of
 *                  the longest match if one was found.
 * \param cursor    A pointer which will, if not NULL, point to the index of the character where the function
 *                  stopped searching.
 * \param tolerant  Indicates whether or not the function should exit upon encountering a non-printable
 *                  character.
 * \return          GRR_RET_OK if a substring match was found.
 *                  GRR_RET_BAD_ARGS if either nfa or string is NULL.
 *                  GRR_RET_NOT_FOUND if no substring match was found.
 *                  GRR_RET_BAD_DATA if the string contained a non-printable character and tolerant was set
 *                  to false.
 */
int
grrSearch(grrNfa nfa, const char *string, size_t len, size_t *start, size_t *end, size_t *cursor,
          bool tolerant);

#endif // __GRR_ENGINE_RUNTIME_H__
