/**
 * \file    nfaCompiler.h
 * \brief   Compile regexes.
 */

#ifndef __GRR_ENGINE_COMPILER_H__
#define __GRR_ENGINE_COMPILER_H__

#include <sys/types.h>

#include "nfaDef.h"

/**
 *  \brief          Compiles a string into a regex object.
 *
 *  \param string   The string to be compiled (does not have to be null-terminated).
 *  \param len      The length of the string.
 *  \param nfa      A pointer to the GrrEngine regex object to be populated.
 *  \return         GRR_RET_OK if successful.
 *                  GRR_RET_BAD_ARGS if either string or nfa is NULL.
 *                  GRR_RET_BAD_DATA if the string contained non-printable characters.
 */
int
grrCompile(const char *string, size_t len, grrNfa *nfa);

#endif  // __GRR_ENGINE_COMPILER_H__
