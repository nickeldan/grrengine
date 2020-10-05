/*
Written by Daniel Walker, 2020.
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "nfaRuntime.h"
#include "nfaInternals.h"

static bool
determineNextState(unsigned int depth, const grrNfa nfa, unsigned int state, char character);

static bool
canTransitionToAcceptingState(const grrNfa nfa, unsigned int state);

static void
determineNextStateRecord(unsigned int depth, grrNfa nfa, unsigned int state, const nfaStateRecord *record,
                         char character, unsigned char flags);

static void
maybePlaceRecord(const nfaStateRecord *record, unsigned int state, nfaStateSet *set, bool update_score);

int
grrMatch(const grrNfa nfa, const char *string, size_t len)
{
    unsigned int state_set_len;

    if ( !nfa || !string ) {
        return GRR_RET_BAD_ARGS;
    }

    state_set_len=(nfa->length+1+7)/8; // The +1 is for the accepting state.
    memset(nfa->current.s_flags,0,state_set_len);
    SET_FLAG(nfa->current.s_flags,0);

    for (size_t idx=0; idx<len; idx++) {
        bool still_alive=false;
        char character;

        character=string[idx];
        if ( !isprint(character) && character != '\t' ) {
            return GRR_RET_BAD_DATA;
        }

        character=ADJUST_CHARACTER(character);
        memset(nfa->next.s_flags,0,state_set_len);

        for (unsigned int state=0; state<nfa->length; state++) {
            if ( !IS_FLAG_SET(nfa->current.s_flags,state) ) {
                continue;
            }

            if ( determineNextState(0,nfa,state,character) ) {
                still_alive=true;
            }
        }

        if ( !still_alive ) {
            return GRR_RET_NOT_FOUND;
        }

        memcpy(nfa->current.s_flags,nfa->next.s_flags,state_set_len);
    }

    for (unsigned int k=0; k<nfa->length; k++) {
        if ( IS_FLAG_SET(nfa->current.s_flags,k) && canTransitionToAcceptingState(nfa,k) ) {
            return GRR_RET_OK;
        }
    }

    return GRR_RET_NOT_FOUND;
}

int
grrSearch(grrNfa nfa, const char *string, size_t len, size_t *start, size_t *end, size_t *cursor,
          bool tolerant)
{
    unsigned int length;

    if ( !nfa || !string ) {
        return GRR_RET_BAD_ARGS;
    }

    length=nfa->length;
    nfa->current.length=0;

    if ( cursor ) {
        *cursor=len;
    }

    for (size_t idx=0; idx<len; idx++) {
        char character;
        unsigned char flags=0;
        unsigned int current_length;
        nfaStateRecord first_state;

        character=string[idx];

        if ( character == '\r' || character == '\n' ) {
            if ( cursor ) {
                *cursor=idx;
            }

            break;
        }

        nfa->next.length=0;
        current_length=nfa->current.length;

        if ( !isprint(character) && character != '\t' ) {
            if ( !tolerant ) {
                if ( cursor ) {
                    *cursor=idx;
                }

                return GRR_RET_BAD_DATA;
            }

            for (unsigned int k=0; k<current_length; k++) {
                if ( nfa->current.s_records[k].state == length ) {
                    if ( k > 0 ) {
                        memcpy(nfa->current.s_records+0,nfa->current.s_records+k,
                            sizeof(nfa->current.s_records[0]));
                    }
                    current_length=nfa->current.length=1;
                    goto skip_over_clear;
                }
            }

            memset(&nfa->current,0,sizeof(nfa->current));

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

        for (unsigned int k=0; k<current_length; k++) {
            determineNextStateRecord(0,nfa,nfa->current.s_records[k].state,nfa->current.s_records+k,
                character,flags);
        }

        first_state.state=0;
        first_state.start_idx=first_state.end_idx=idx;
        first_state.score=0;

        determineNextStateRecord(0,nfa,0,&first_state,character,flags);

        memcpy(nfa->current.s_records,nfa->next.s_records,nfa->next.length*sizeof(*nfa->next.s_records));
        nfa->current.length=nfa->next.length;
    }

    for (unsigned int k=0; k<nfa->current.length; k++) {
        if ( nfa->current.s_records[k].state == length ) {
            if ( start ) {
                *start=nfa->current.s_records[k].start_idx;
            }
            if ( end ) {
                *end=nfa->current.s_records[k].end_idx;
            }

            return GRR_RET_OK;
        }
    }

    return GRR_RET_NOT_FOUND;
}

ssize_t
grrFirstMatch(grrNfa *nfa_list, size_t num, FILE *file, char *destination, size_t capacity, size_t *size)
{
    unsigned char flags=GRR_NFA_FIRST_CHAR_FLAG|GRR_NFA_LAST_CHAR_FLAG;
    size_t champion, champion_score;

    if ( !nfa_list || !file || !destination || !size ) {
        return -1;
    }

    for (size_t k=0; k<num; k++) {
        nfa_list[k]->current.length=0;
        nfa_list[k]->next.length=0;
    }

    for (*size=0; *size<capacity; (*size)++) {
        int character;
        bool still_alive=false;

        character=destination[*size]=fgetc(file);
        if ( character == EOF ) {
            break;
        }

        if ( !isprint(character) && character != '\t' ) {
            ungetc(character,file);
            break;
        }

        character=ADJUST_CHARACTER(character);
        
        if ( *size == 0 ) {
            for (size_t k=0; k<num; k++) {
                nfaStateRecord first_state={0};

                determineNextStateRecord(0,nfa_list[k],0,&first_state,character,flags);
            }
        }
        else {
            for (size_t k=0; k<num; k++) {
                grrNfa nfa;

                nfa=nfa_list[k];
                if ( nfa->current.length == 0 ) {
                    continue;
                }
                nfa->next.length=0;

                for (unsigned int k=0; k<nfa->current.length; k++) {
                    determineNextStateRecord(0,nfa,nfa->current.s_records[k].state,nfa->current.s_records+k,
                        character,flags);
                }
            }
        }

        for (size_t k=0; k<num; k++) {
            grrNfa nfa;

            nfa=nfa_list[k];

            if ( nfa->next.length > 0 ) {
                memcpy(nfa->current.s_records,nfa->next.s_records,
                    nfa->next.length*sizeof(*nfa->next.s_records));
                still_alive=true;
            }
        }

        if ( !still_alive ) {
            return -1;
        }
    }

    champion_score=0;
    for (size_t k=0; k<num; k++) {
        grrNfa nfa;

        nfa=nfa_list[k];

        for (unsigned int j=0; j<nfa->current.length; j++) {
            if ( nfa->current.s_records[j].state == nfa->length ) {
                if ( nfa->current.s_records[j].score > champion_score ) {
                    champion_score=nfa->current.s_records[j].score;
                    champion=k;
                }
            }
        }
    }

    return ( champion_score > 0 )? (ssize_t)champion : -1;
}

static bool
determineNextState(unsigned int depth, const grrNfa nfa, unsigned int state, char character)
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
            SET_FLAG(nfa->next.s_flags,new_state);
            still_alive=true;
        }
        else if ( IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_EMPTY_TRANSITION) ) {
            if ( determineNextState(depth+1,nfa,new_state,character) ) {
                still_alive=true;
            }
        }
        else if ( IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_BAR) ) {
            return false;
        }
    }

    return still_alive;
}

static bool
canTransitionToAcceptingState(const grrNfa nfa, unsigned int state) 
{
    const nfaNode *nodes;

    if ( state == nfa->length ) {
        return true;
    }

    nodes=nfa->nodes;
    for (unsigned int k=0; k<=nodes[state].two_transitions; k++) {
        if ( IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_EMPTY_TRANSITION)
                || IS_FLAG_SET(nodes[state].transitions[k].symbols,GRR_NFA_BAR) ) {
            unsigned int new_state;

            new_state=state+nodes[state].transitions[k].motion;
            if ( canTransitionToAcceptingState(nfa,new_state) ) {
                return true;
            }
        }
    }

    return false;
}

static void
determineNextStateRecord(unsigned int depth, grrNfa nfa, unsigned int state, const nfaStateRecord *record,
                         char character, unsigned char flags)
{
    const nfaNode *nodes;

    if ( state == nfa->length ) {
        maybePlaceRecord(record,state,&nfa->next,(depth > 0));
        return;
    }

    assert(depth < nfa->length);

    nodes=nfa->nodes;
    for (unsigned int k=0; k<=nodes[state].two_transitions; k++) {
        unsigned int new_state;
        const unsigned char *symbols;

        new_state=state+nodes[state].transitions[k].motion;

        symbols=nodes[state].transitions[k].symbols;
        if ( IS_FLAG_SET(symbols,GRR_NFA_BAR) ) {
            if ( IS_FLAG_SET(symbols,character) ) {
                continue;
            }

            maybePlaceRecord(record,new_state,&nfa->next,false);
        }
        else if ( IS_FLAG_SET(symbols,character) ) {
            maybePlaceRecord(record,new_state,&nfa->next,true);
        }
        else if ( IS_FLAG_SET(symbols,GRR_NFA_EMPTY_TRANSITION) ) {
            if ( IS_FLAG_SET(symbols,GRR_NFA_FIRST_CHAR) && !(flags&GRR_NFA_FIRST_CHAR_FLAG) ) {
                continue;
            }

            if ( IS_FLAG_SET(symbols,GRR_NFA_LAST_CHAR) && !(flags&GRR_NFA_LAST_CHAR_FLAG) ) {
                continue;
            }

            determineNextStateRecord(depth+1,nfa,new_state,record,character,flags);
        }
    }
}

static void
maybePlaceRecord(const nfaStateRecord *record, unsigned int state, nfaStateSet *set, bool update_score)
{
    unsigned int k;
    size_t new_score;

    for (k=0; k<set->length; k++) {
        if ( set->s_records[k].state == state ) {
            break;
        }
    }

    new_score=record->score+(update_score? 1 : 0);
    if ( k == set->length || new_score > set->s_records[k].score ) {
        set->s_records[k].start_idx=record->start_idx;
        set->s_records[k].end_idx=record->end_idx;
        set->s_records[k].score=new_score;
        if ( update_score ) {
            set->s_records[k].end_idx++;
        }

        if ( k == set->length ) {
            set->s_records[k].state=state;
            set->length++;
        }
    }
}
