#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "common.h"

typedef struct grrRegexStackFrame {
    grrRegex *regex;
    size_t idx;
    char reason;
} grrRegexStackFrame;

typedef struct grrRegexStack {
    grrRegexStackFrame *frames;
    size_t length;
    size_t capacity;
} grrRegexStack;

static grrRegex *
regexNew(void)
{
    grrRegex *regex;

    regex = malloc(sizeof(*regex));
    if ( regex ) {
        *regex = (grrRegex){0};
    }
    return regex;
}

static void
printIdxForString(const char *string, size_t idx)
{
    fprintf(stderr, "%s\n", string);
    for (size_t k=0; k<idx; k++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^\n");
}

static int
pushToStack(grrRegexStack *stack, grrRegex *regex, size_t idx, char reason)
{
    if ( stack->length == stack->capacity ) {
        size_t new_capacity;
        grrRegexStackFrame *success;

        new_capacity = stack->capacity + 5;
        success = realloc(stack->frames, sizeof(grrRegexStackFrame)*new_capacity);
        if ( !success ) {
            return GRR_ENGINE_RET_OUT_OF_MEMORY;
        }
        stack->frames = success;
        stack->capacity = new_capacity;
    }

    stack->frames[stack->length].regex = regex;
    stack->frames[stack->length].idx = idx;
    stack->frames[stack->length].reason = reason;
    stack->length++;

    return GRR_ENGINE_RET_OK;
}

static int
resolveDisjunction(grrRegex *regex1, grrRegex *regex2)
{
    size_t length1 = regex1->lenbth, length2 = regex2->length, new_length;
    grrRegexState *success;

    new_length = 1 + length1 + length2;
    if ( new_length == SIZE_MAX || new_length < 1 + length1 ) {
        return GRR_ENGINE_RET_TOO_LONG;
    }

    success = realloc(regex1->states, sizeof(*success)*new_length);
    if ( !success ) {
        return GRR_ENGINE_RET_OUT_OF_MEMORY;
    }
    regex1->states = success;
    regex1->length = new_length;

    memmove(success+1, success, sizeof(*success)*length1);
    memcpy(success+1+regex1->length, regex2->states, sizeof(*success)*length2);
    if ( regex2->max_match_length > regex1->max_match_length ) {
        regex1->max_match_length = regex2->max_match_length;
    }
    grrRegexFree(regex2);

    success[0].num_transitions = 2;
    memcpy(&success[0].transitions, 0, sizeof(success[0].transitions));
    for (int k=0; k<2; k++) {
        SET_FLAG(success[0].transitions[k].symbols, GRR_ENGINE_SYMBOL_EMPTY_TRANSITION);
    }
    success[0].transitions[0].motion = 1;
    success[0].transitions[1].motion = 1+length1;

    for (size_t k=0; k<length1; k++) {
        grrRegexState *state = success + 1 + k;

        for (unsigned int k=0; k<state->num_transitions; k++) {
            ssize_t motion = state->transitions[k].motion;

            if ( motion > 0 && k + motion == length1 ) {
                state->transitions[k].motion += length2;
            }
        }
    }

    return GRR_ENGINE_RET_OK;
}

static int
resolveConjunction(grrRegex *regex1, grrRegex2 *regex2)
{
    size_t length1 = regex1->lenbth, length2 = regex2->length, new_length;
    grrRegexState *success;

    new_length = length1 + length2;
    if ( new_length == SIZE_MAX || new_length < length1 ) {
        return GRR_ENGINE_RET_TOO_LONG;
    }

    success = realloc(regex1->states, sizeof(*(regex1->states))*new_length);
    if ( !success ) {
        return GRR_ENGINE_RET_OUT_OF_MEMORY;
    }
    regex1->states = success;
    regex1->length = new_length;

    memcpy(success+length1, regex2->states, sizeof(*success)*length2);

    if ( regex1->max_match_length + regex2->max_match_length < regex1->max_match_length ) {
        regex1->max_match_length = SIZE_MAX;
    }
    else {
        regex1->max_match_length += regex2->max_match_length;
    }
    grrRegexFree(regex2);

    return GRR_ENGINE_RET_OK;
}

static int
resolveBraces(grrRegex *regex, const char *string, size_t *idx)
{

}

static int
checkForQuantifier(grrRegex *regex, const char *string, size_t *idx)
{
    size_t new_idx = *idx+1;
    bool question = false, plus = false;

    switch ( string[new_idx] ) {
    case '?':
        question = true; break;

    case '*':
        question = plus = true; break;

    case '{':
        (*idx)++;
        return resolveBraces(regex, string, idx);

    default:
        return GRR_ENGINE_RET_OK;
    }

    if ( plus ) {
        unsigned int length = regex->length;
        grrRegexState *success;

        success = realloc(regex->states, sizeof(*success)*(length+1));
        if ( !success ) {
            return GRR_ENGINE_RET_OUT_OF_MEMORY;
        }
        regex->states = success;
        regex->max_match_length = SIZE_MAX;

        memset(success+length, 0, sizeof(*success));
        success[length].num_transitions = 2;
        for (int k=0; k<2; k++) {
            SET_FLAG(success[length].transitions[k].symbols, GRR_ENGINE_SYMBOL_EMPTY_TRANSITION);
        }
        success[length].transitions[0].motion = -1*regex->length;
        success[length].transitions[1].motion = 1;
        regex->length++;
    }

    if ( question ) {
        size_t length = regex->length;
        grrRegexState *states = regex->states;

        if ( states[0].num_transitions == 1 ) {
            SET_FLAG(states[0].transitions[1].symbols, GRR_ENGINE_SYMBOL_EMPTY_TRANSITION);
            states[0].transitions[1].motion = length;
        }
        else {
            grrRegexState *success;

            success = realloc(states, sizeof(*states)*(1+length));
            if ( !success ) {
                return GRR_ENGINE_RET_OUT_OF_MEMORY;
            }
            regex->states = success;
            regex->length++;

            memset(success, 0, sizeof(*success));
            success[0].num_transitions = 1;
            SET_FLAG(success[0].transitions[0].symbols, GRR_ENGINE_SYMBOL_EMPTY_TRANSITION);
            success[0].transitions[0].motion = 1+length;
        }
    }

    return GRR_ENGINE_RET_OK;
}

static bool
findParensInStack(const grrRegexStack *stack, size_t *idx)
{
    for (size_t k=stack->length; k>0; k--) {
        if ( stack->frames[k-1].reason == '(' ) {
            *idx = k-1;
            return true;
        }
    }

    return false;
}

static int
resolveCharacterClass(const char *string, size_t *idx, grrRegex *regex, bool verbose)
{
    if ( string[++(*idx)] == '\0' ) {
    }
}

int
grrRegexCompile(const char *string, grrRegex **regex, bool verbose)
{
    int ret;
    grrRegex *current;
    grrRegexStack stack = {0};

    if ( !string || !regex ) {
        return GRR_ENGINE_RET_USAGE;
    }

    current = regexNew();
    if ( !current ) {
        return GRR_ENGINE_RET_OUT_OF_MEMORY;
    }

    for (size_t idx=0; string[idx]; idx++) {
        char c = string[idx];

        if ( !isprint(c) && c != '\t' ) {
            if ( verbose ) {
                fprintf(stderr, "Unprintable character (0x%02x) at position %zu.\n", c, idx);
            }
            ret = GRR_ENGINE_RET_BAD_DATA;
            goto error;
        }

        switch ( c ) {
            size_t stack_idx;

        case '(':
        case '|':
            ret = pushToStack(&stack, current, idx, c);
            if ( ret != GRR_ENGINE_RET_OK ) {
                goto error;
            }
            current = regexNew();
            if ( !current ) {
                ret = GRR_ENGINE_RET_OUT_OF_MEMORY;
                goto error;
            }
            break;

        case ')':
            if ( !findParensInStack(&stack, &stack_idx) ) {
                if ( verbose ) {
                    fprintf(stderr, "Closing parenthesis not matched by preceding opening parenthesis:\n");
                    printIdxForString(string, idx);
                }
                ret = GRR_ENGINE_RET_BAD_DATA;
                goto error;
            }

            if ( stack_idx + 1 < stack.length ) {
                grrRegex *temp = stack.frames[stack_idx+1].regex;

                for (size_t k=stack_idx+2; k<stack.length; k++) {
                    ret = resolveDisjunction(temp, stack.frames[k].regex);
                    if ( ret != GRR_ENGINE_RET_OK ) {
                        // temp is still on the stack and so will be cleaned up.
                        goto error;
                    }
                    stack.frames[k].regex = NULL;
                }

                ret = resolveDisjunction(temp, current);
                if ( ret != GRR_ENGINE_RET_OK ) {
                    goto error;
                }

                current = temp;
                stack.length = stack_idx + 1;
            }

            ret = checkForQuantifier(current, string, idx, &idx);
            if ( ret != GRR_ENGINE_RET_OK ) {
                goto error;
            }

            ret = resolveConjunction(stack.frames[stack_idx].regex, current);
            if ( ret != GRR_ENGINE_RET_OK ) {
                goto error;
            }
            current = stack.frames[stack_idx].regex;
            stack.length = stack_idx;
        }
    }
}
