#include <stdlib.h>

#include "nfaInternals.h"

void grrFreeNfa(grrNfa nfa) {
    if ( !nfa ) {
        return;
    }

    free(nfa->nodes);
    free(nfa->string);
    free(nfa);
}

const char *grrDescription(const grrNfa nfa) {
    return nfa->string;
}
