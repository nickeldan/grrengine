/*
Written by Daniel Walker, 2020.
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#include <assert.h>

#include "nfaRuntime.h"
#include "nfaInternals.h"

typedef struct nfaStateRecord {
    size_t start_idx;
    size_t end_idx;
    size_t score;
    unsigned int state;
} nfaStateRecord;

typedef struct nfaStateSet {
    nfaStateRecord *records;
    unsigned int length;
} nfaStateSet;

static bool
determineNextState(unsigned int depth, grrNfa nfa, unsigned int state, char character,
                   unsigned char *state_set);

static bool
canTransitionToAcceptingState(grrNfa nfa, unsigned int state, unsigned char *visited_states);

static void
determineNextStateRecord(unsigned int depth, grrNfa nfa, unsigned int state, const nfaStateRecord *record,
                         char character, unsigned char flags, nfaStateSet *set);

static void
maybePlaceRecord(const nfaStateRecord *record, unsigned int state, nfaStateSet *set, bool update_score);

int
grrMatch(const grrNfa nfa, const char *string, size_t len)
{
    unsigned int state_set_len=(nfa->length+1+7)/8; // The +1 is for the accepting state.
    unsigned char current_state_set[state_set_len], next_state_set[state_set_len];

    if ( !nfa || !string ) {
        return GRR_RET_BAD_ARGS;
    }

    memset(current_state_set,0,state_set_len);
    SET_FLAG(current_state_set,0);

    for (size_t idx=0; idx<len; idx++) {
        bool still_alive=false;
        char character;

        character=string[idx];
        if ( !isprint(character) && character != '\t' ) {
            return GRR_RET_BAD_DATA;
        }

        character=ADJUST_CHARACTER(character);
        memset(next_state_set,0,state_set_len);

        for (unsigned int state=0; state<nfa->length; state++) {
            if ( !IS_FLAG_SET(current_state_set,state) ) {
                continue;
            }

            if ( determineNextState(0,nfa,state,character,next_state_set) ) {
                still_alive=true;
            }
        }

        if ( !still_alive ) {
            return GRR_RET_NOT_FOUND;
        }

        memcpy(current_state_set,next_state_set,state_set_len);
    }

    for (unsigned int k=0; k<nfa->length; k++) {
        if ( IS_FLAG_SET(current_state_set,k) ) {
            unsigned char visited_states[state_set_len];

            memset(visited_states,0,sizeof(visited_states));

            if ( canTransitionToAcceptingState(nfa,k,visited_states) ) {
                return GRR_RET_OK;
            }
        }
    }

    return GRR_RET_NOT_FOUND;
}

int
grrSearch(grrNfa nfa, const char *string, size_t len, size_t *start, size_t *end, size_t *cursor,
          bool tolerant)
{
    unsigned int length;
    size_t champion_score;
    nfaStateSet current_state_set, next_state_set;

    if ( !nfa || !string ) {
        return GRR_RET_BAD_ARGS;
    }

    if ( cursor ) {
        *cursor=len;
    }

    length=nfa->length;
    current_state_set.records=alloca(sizeof(nfaStateRecord)*(length+1));
    memset(current_state_set.records,0,sizeof(nfaStateRecord)*(length+1));
    current_state_set.length=0;

    next_state_set.records=alloca(sizeof(nfaStateRecord)*(length+1));

    for (size_t idx=0; idx<len; idx++) {
        char character;
        unsigned char flags=0;
        nfaStateRecord first_state;

        character=string[idx];

        if ( character == '\r' || character == '\n' ) {
            if ( cursor ) {
                *cursor=idx;
            }

            break;
        }

        next_state_set.length=0;

        if ( !isprint(character) && character != '\t' ) {
            if ( !tolerant ) {
                if ( cursor ) {
                    *cursor=idx;
                }

                return GRR_RET_BAD_DATA;
            }

            for (unsigned int k=0; k<current_state_set.length; k++) {
                if ( current_state_set.records[k].state == length ) {
                    if ( k > 0 ) {
                        memcpy(current_state_set.records+0,current_state_set.records+k,
                            sizeof(nfaStateRecord));
                    }
                    goto skip_over_clear;
                }
            }

            memset(current_state_set.records,0,
                sizeof(nfaStateRecord)*current_state_set.length);

            skip_over_clear:
            do {
                idx++;
            } while ( idx < len && !isprint(string[idx]) && string[idx] != '\t' );
            if ( idx == len ) {
                break;
            }

            character=string[idx];
            flags |= GRR_NFA_FIRST_CHAR_FLAG;
        }
        else if ( idx == 0 ) {
            flags |= GRR_NFA_FIRST_CHAR_FLAG;
        }

        if ( idx == len-1 || !isprint(string[idx+1]) ) {
            flags |= GRR_NFA_LAST_CHAR_FLAG;
        }

        character=ADJUST_CHARACTER(character);

        for (unsigned int k=0; k<current_state_set.length; k++) {
            determineNextStateRecord(0,nfa,current_state_set.records[k].state,
                current_state_set.records+k,character,flags,&next_state_set);
        }

        first_state.state=0;
        first_state.start_idx=first_state.end_idx=idx;
        first_state.score=0;

        determineNextStateRecord(0,nfa,0,&first_state,character,flags,&next_state_set);

        memcpy(current_state_set.records,next_state_set.records,
            sizeof(nfaStateRecord)*next_state_set.length);
        current_state_set.length=next_state_set.length;
    }

    champion_score = 0;
    for (unsigned int k=0; k<current_state_set.length; k++) {
        if ( current_state_set.records[k].score > champion_score ) {
            unsigned char visited_states[(nfa->length+1+7)/8]; // The +1 is for the accepting state.

            memset(visited_states,0,sizeof(visited_states));

            if ( canTransitionToAcceptingState(nfa,current_state_set.records[k].state,visited_states) ) {
                if ( start ) {
                    *start=current_state_set.records[k].start_idx;
                }
                if ( end ) {
                    *end=current_state_set.records[k].end_idx;
                }

                champion_score = current_state_set.records[k].score;
            }
        }
    }

    return ( champion_score > 0 )? GRR_RET_OK : GRR_RET_NOT_FOUND;
}

ssize_t
grrFirstMatch(grrNfa *nfa_list, size_t num, const char *source, size_t size, size_t *processed,
              size_t *score)
{
    unsigned char flags=GRR_NFA_FIRST_CHAR_FLAG|GRR_NFA_LAST_CHAR_FLAG;
    size_t champion;
    nfaStateSet current_state_sets[num], next_state_sets[num];
    
    if ( !nfa_list || num == 0 || !source || size == 0 || !processed ) {
        return -1;
    }

    for (size_t k=0; k<num; k++) {
        current_state_sets[k].records=alloca(sizeof(nfaStateRecord)*(nfa_list[k]->length+1));
        current_state_sets[k].length=0;

        next_state_sets[k].records=alloca(sizeof(nfaStateRecord)*(nfa_list[k]->length+1));
    }

    for (*processed=0; *processed<size; (*processed)++) {
        char character;
        bool still_alive=false;

        character=source[*processed];
        if ( !isprint(character) && character != '\t' ) {
            break;
        }
        character=ADJUST_CHARACTER(character);

        if ( *processed == 0 ) {
            for (size_t k=0; k<num; k++) {
                nfaStateRecord first_state={0};

                determineNextStateRecord(0,nfa_list[k],0,&first_state,character,flags,next_state_sets+k);
            }
        }
        else {
            for (size_t k=0; k<num; k++) {
                nfaStateSet *current;

                current=current_state_sets+k;
                if ( current->length == 0 ) {
                    continue;
                }
                next_state_sets[k].length=0;

                for (unsigned int j=0; j<current->length; j++) {
                    determineNextStateRecord(0,nfa_list[k],current->records[j].state,current->records+j,
                        character,flags,next_state_sets+k);
                }
            }
        }

        for (size_t k=0; k<num; k++) {
            nfaStateSet *next;

            next=next_state_sets+k;

            if ( next->length > 0 ) {
                memcpy(current_state_sets[k].records,next->records,sizeof(nfaStateRecord)*next->length);
                if ( next->length > 1 || next->records[0].state != nfa_list[k]->length ) {
                    still_alive=true;
                }
            }
        }

        if ( !still_alive ) {
            break;
        }
    }

    *score=0;
    for (size_t k=0; k<num; k++) {
        nfaStateSet *current;

        current=current_state_sets+k;

        for (unsigned int j=0; j<current->length; j++) {
            if ( current->records[j].state == nfa_list[k]->length ) {
                if ( current->records[j].score > *score ) {
                    *score=current->records[j].score;
                    champion=k;
                }
            }
        }
    }

    return ( *score > 0 )? (ssize_t)champion : -1;
}

static bool
determineNextState(unsigned int depth, grrNfa nfa, unsigned int state, char character,
                   unsigned char *state_set)
{
    const nfaNode *nodes;
    bool still_alive=false;

    if ( state == nfa->length ) {
        // We've reached the accepting state but we have another character to process.
        return false;
    }

    assert(depth < nfa->length);

    nodes=nfa->nodes;
    for (unsigned int k=0; k<=nodes[state].two_transitions; k++) {
        unsigned int new_state;

        new_state=state+nodes[state].transitions[k].motion;

        if ( IS_FLAG_SET(nodes[state].transitions[k].symbols,character) ) {
            SET_FLAG(state_set,new_state);
            still_alive=true;
        }
        else if ( IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_EMPTY_TRANSITION) ) {
            if ( determineNextState(depth+1,nfa,new_state,character,state_set) ) {
                still_alive=true;
            }
        }
        else if ( IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_LOOKAHEAD) ) {
            return false;
        }
    }

    return still_alive;
}

static bool
canTransitionToAcceptingState(grrNfa nfa, unsigned int state, unsigned char *visited_states) 
{
    const nfaNode *nodes;

    if ( state == nfa->length ) {
        return true;
    }

    if ( IS_FLAG_SET(visited_states,state) ) {
        return false;
    }
    SET_FLAG(visited_states,state);

    nodes=nfa->nodes;
    for (unsigned int k=0; k<=nodes[state].two_transitions; k++) {
        if ( IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_EMPTY_TRANSITION)
                || IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_LOOKAHEAD) ) {
            unsigned int new_state;

            new_state=state+nodes[state].transitions[k].motion;
            if ( canTransitionToAcceptingState(nfa,new_state,visited_states) ) {
                return true;
            }
        }
    }

    return false;
}

static void
determineNextStateRecord(unsigned int depth, grrNfa nfa, unsigned int state, const nfaStateRecord *record,
                         char character, unsigned char flags, nfaStateSet *set)
{
    const nfaNode *nodes;

    if ( state == nfa->length ) {
        maybePlaceRecord(record,state,set,(depth > 0));
        return;
    }

    assert(depth < nfa->length);

    nodes=nfa->nodes;
    for (unsigned int k=0; k<=nodes[state].two_transitions; k++) {
        unsigned int new_state;
        const unsigned char *symbols;

        new_state=state+nodes[state].transitions[k].motion;

        symbols=nodes[state].transitions[k].symbols;
        if ( IS_FLAG_SET(symbols,GRR_NFA_LOOKAHEAD) ) {
            if ( !IS_FLAG_SET(symbols,character) ) {
                continue;
            }

            maybePlaceRecord(record,new_state,set,false);
        }
        else if ( IS_FLAG_SET(symbols,character) ) {
            maybePlaceRecord(record,new_state,set,true);
        }
        else if ( IS_FLAG_SET(symbols,GRR_NFA_EMPTY_TRANSITION) ) {
            if ( IS_FLAG_SET(symbols,GRR_NFA_FIRST_CHAR) && !(flags&GRR_NFA_FIRST_CHAR_FLAG) ) {
                continue;
            }

            if ( IS_FLAG_SET(symbols,GRR_NFA_LAST_CHAR) && !(flags&GRR_NFA_LAST_CHAR_FLAG) ) {
                continue;
            }

            determineNextStateRecord(depth+1,nfa,new_state,record,character,flags,set);
        }
    }
}

static void
maybePlaceRecord(const nfaStateRecord *record, unsigned int state, nfaStateSet *set, bool update_score)
{
    unsigned int k;
    size_t new_score;

    for (k=0; k<set->length; k++) {
        if ( set->records[k].state == state ) {
            break;
        }
    }

    new_score=record->score+(update_score? 1 : 0);
    if ( k == set->length || new_score > set->records[k].score ) {
        set->records[k].start_idx=record->start_idx;
        set->records[k].end_idx=record->end_idx;
        set->records[k].score=new_score;
        if ( update_score ) {
            set->records[k].end_idx++;
        }

        if ( k == set->length ) {
            set->records[k].state=state;
            set->length++;
        }
    }
}
