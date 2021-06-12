#ifndef GRR_ENGINE_COMMON_H
#define GRR_ENGINE_COMMON_H

#include "grrengine.h"

enum grrEngineSpecialSymbol {
    GRR_ENGINE_SYMBOL_EMPTY_TRANSITION = 0,
    GRR_ENGINE_SYMBOL_FIRST_CHAR,
    GRR_ENGINE_SYMBOL_LAST_CHAR,
    GRR_ENGINE_SYMBOL_LOOKAHEAD,
    GRR_ENGINE_SYMBOL_TAB,

    GRR_ENGINE_SYMBOL_UNUSED,
};

#define ASCII_OFFSET GRR_ENGINE_SYMBOL_UNUSED
#define NUM_SYMBOLS (ASCII_OFFSET + 0x7e + 1 - 0x20) // Include ASCII printables.
#define ASCII_ADJUSTMENT (0x20 - ASCII_OFFSET)

#define ADJUST_CHARACTER(c) (((c) == '\t')? GRR_ENGINE_SYMBOL_TAB : (c)-ASCII_ADJUSTMENT)

typedef struct grrRegexTransition {
    ssize_t motion;
    unsigned char symbols[(NUM_SYMBOLS+7)/8];
} grrRegexTransition;

#define SET_FLAG(state, flag) (state)[(flag) / 8] |= (1 << ((flag) % 8))
#define IS_FLAG_SET(state, flag) ((state)[(flag) / 8] & (1 << ((flag) % 8)))

typedef struct grrRegexState {
    grrRegexTransition transitions[2];
    unsigned char num_transitions;
} grrRegexState;

typedef struct grrRegex {
    grrRegexState *states;
    size_t length;
    size_t max_match_length;
} grrRegex;

#endif // GRR_ENGINE_COMMON_H
