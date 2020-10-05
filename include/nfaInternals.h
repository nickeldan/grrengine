#ifndef __GRR_NFA_INTERNALS_H__
#define __GRR_NFA_INTERNALS_H__

#include "nfaDef.h"

enum specialCharacterValues {
    GRR_NFA_EMPTY_TRANSITION = 0,
    GRR_NFA_FIRST_CHAR,
    GRR_NFA_LAST_CHAR,
    GRR_NFA_BAR,
    GRR_NFA_TAB,
};

#define GRR_NFA_EMPTY_TRANSITION_FLAG (1 << GRR_NFA_EMPTY_TRANSITION)
#define GRR_NFA_FIRST_CHAR_FLAG (1 << GRR_NFA_FIRST_CHAR)
#define GRR_NFA_LAST_CHAR_FLAG (1 << GRR_NFA_LAST_CHAR)
#define GRR_NFA_BAR_FLAG (1 << GRR_NFA_BAR)
#define GRR_NFA_TAB_FLAG (1 << GRR_NFA_TAB)

#define GRR_NFA_ASCII_OFFSET 5
#define GRR_NFA_NUM_SYMBOLS (GRR_NFA_ASCII_OFFSET + 0x7e + 1 - 0x20) // ASCII printables
#define GRR_NFA_ASCII_ADJUSTMENT (0x20 - GRR_NFA_ASCII_OFFSET)

#define ADJUST_CHARACTER(c) ( ( (c) == '\t' )? GRR_NFA_TAB : (c)-GRR_NFA_ASCII_ADJUSTMENT )

typedef struct nfaTransition {
    int motion;
    unsigned char symbols[(GRR_NFA_NUM_SYMBOLS+7)/8];
} nfaTransition;

typedef struct nfaNode {
    nfaTransition transitions[2];
    unsigned char two_transitions;
} nfaNode;

typedef struct nfaStateRecord {
    size_t start_idx;
    size_t end_idx;
    size_t score;
    unsigned int state;
} nfaStateRecord;

typedef struct nfaStateSet {
    union {
        nfaStateRecord *records;
        unsigned char *flags;
    } set;
    unsigned int length;
} nfaStateSet;
#define s_records set.records
#define s_flags set.flags

struct grrNfaStruct {
    nfaNode *nodes;
    char *string;
    nfaStateSet current;
    nfaStateSet next;
    unsigned int length;
};

#define SET_FLAG(state,flag) (state)[(flag)/8] |= ( 1 << ((flag)%8) )
#define IS_FLAG_SET(state,flag) ( (state)[(flag)/8]&( 1 << ((flag)%8) ) )

#endif // __GRR_NFA_INTERNALS_H__