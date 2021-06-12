#ifndef GRR_ENGINE_H
#define GRR_ENGINE_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct grrRegex grrRegex;

enum grrEngineReturnValue {
    GRR_ENGINE_RET_OK,
    GRR_ENGINE_RET_NOT_FOUND,
    GRR_ENGINE_RET_USAGE,
    GRR_ENGINE_RET_BAD_DATA,
    GRR_ENGINE_RET_OUT_OF_MEMORY,
    GRR_ENGINE_RET_TOO_LONG,
};

int
grrRegexCompile(const char *pattern, grrRegex **regex, bool verbose);

int
grrRegexSearch(const grrRegex *regex, bool tolerant, const char *text, size_t len, size_t *start, size_t *end);

int
grrRegexMatch(const grrRegex *regex, const char *text, size_t len);

void
grrRegexFree(grrRegex *regex);

#endif // GRR_ENGINE_H
