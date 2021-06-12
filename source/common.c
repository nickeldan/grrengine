#include <stdlib.h>

#include "common.h"

void
grrRegexFree(grrRegex *regex)
{
    if ( regex ) {
        free(regex->states);
        free(regex);
    }
}
