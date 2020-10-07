/*
Written by Daniel Walker, 2020.
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include "nfa.h"
#include "nfaInternals.h"

#define GRR_INVALID_CHARACTER 0x00
#define GRR_WHITESPACE_CODE 0x01
#define GRR_WILDCARD_CODE 0x02
#define GRR_EMPTY_TRANSITION_CODE 0x03
#define GRR_FIRST_CHAR_CODE 0x04
#define GRR_LAST_CHAR_CODE 0x05
#define GRR_DIGIT_CODE 0x06

#define GRR_NFA_PADDING 5

typedef struct nfaStackFrame {
    grrNfa nfa;
    size_t idx;
    char reason;
} nfaStackFrame;

typedef struct nfaStack {
    nfaStackFrame *frames;
    size_t length;
    size_t capacity;
} nfaStack;

#define NEW_NFA() calloc(1,sizeof(struct grrNfaStruct))

static void
printIdxForString(const char *string, size_t len, size_t idx);

static int
pushNfaToStack(nfaStack *stack, grrNfa nfa, size_t idx, char reason);

static void
freeNfaStack(nfaStack *stack);

static ssize_t
findParensInStack(const nfaStack *stack);

static char
resolveEscapeCharacter(char c) __attribute__ ((pure));

static grrNfa
createCharacterNfa(char c);

static void
setSymbol(nfaTransition *transition, char c);

static int
concatenateNfas(grrNfa nfa1, grrNfa nfa2);

static int
addDisjunctionToNfa(grrNfa nfa1, grrNfa nfa2);

static int
checkForQuantifier(grrNfa nfa, const char *string, size_t len, size_t idx, size_t *newIdx);

static int
resolveBraces(grrNfa nfa, const char *string, size_t len, size_t idx, size_t *newIdx);

static int
resolveCharacterClass(const char *string, size_t len, size_t *idx, grrNfa *nfa);

int
grrCompile(const char *string, size_t len, grrNfa *nfa)
{
    int ret;
    nfaStack stack={0};
    grrNfa current;

    if ( !string || len == 0 || !nfa ) {
        return GRR_RET_BAD_ARGS;
    }

    for (size_t idx=0; idx<len; idx++) {
        if ( !isprint(string[idx]) ) {
            fprintf(stderr,"Unprintable character at index %zu: 0x%02x\n", idx, (unsigned char)string[idx]);
            return GRR_RET_BAD_DATA;
        }
    }

    current=NEW_NFA();
    if ( !current ) {
        return GRR_RET_OUT_OF_MEMORY;
    }

    for (size_t idx=0; idx<len; idx++) {
        ssize_t stackIdx;
        char character;
        grrNfa temp;

        character=string[idx];
        switch ( character ) {
            case '(':
            case '|':
            ret=pushNfaToStack(&stack,current,idx,character);
            if ( ret != GRR_RET_OK ) {
                goto error;
            }
            current=NEW_NFA();
            if ( !current ) {
                ret=GRR_RET_OUT_OF_MEMORY;
                goto error;
            }
            break;

            case ')':
            stackIdx=findParensInStack(&stack);
            if ( stackIdx < 0 ) {
                fprintf(stderr,"Closing parenthesis not matched by preceding opening parenthesis:\n");
                printIdxForString(string,len,idx);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }

            if ( (size_t)stackIdx == stack.length-1 ) { // The parentheses were empty.
                grrFreeNfa(current);
            }
            else {
                temp=stack.frames[stackIdx+1].nfa;
                stack.frames[stackIdx+1].nfa=NULL;

                for (size_t k=stackIdx+2; k<stack.length; k++) {
                    ret=addDisjunctionToNfa(temp,stack.frames[k].nfa);
                    if ( ret != GRR_RET_OK ) {
                        grrFreeNfa(temp);
                        goto error;
                    }
                    stack.frames[k].nfa=NULL;
                }

                ret=addDisjunctionToNfa(temp,current);
                if ( ret != GRR_RET_OK ) {
                    grrFreeNfa(temp);
                    goto error;
                }
                current=temp;

                ret=checkForQuantifier(current,string,len,idx,&idx);
                if ( ret != GRR_RET_OK ) {
                    grrFreeNfa(temp);
                    goto error;
                }

                ret=concatenateNfas(stack.frames[stackIdx].nfa,current);
                if ( ret != GRR_RET_OK ) {
                    goto error;
                }
            }

            current=stack.frames[stackIdx].nfa;
            stack.length=stackIdx;
            break;

            case '[':
            ret=resolveCharacterClass(string,len,&idx,&temp);
            if ( ret != GRR_RET_OK ) {
                goto error;
            }
            ret=concatenateNfas(current,temp);
            if ( ret != GRR_RET_OK ) {
                grrFreeNfa(temp);
                goto error;
            }
            break;

            case ']':
            fprintf(stderr,"Unmatched bracket:\n:");
            printIdxForString(string,len,idx);
            ret=GRR_RET_BAD_DATA;
            goto error;

            case '*':
            case '+':
            case '?':
            fprintf(stderr,"Invalid use of quantifier:\n");
            printIdxForString(string,len,idx);
            ret=GRR_RET_BAD_DATA;
            goto error;

            case '{':
            fprintf(stderr,"Invalid use of curly brace:\n");
            printIdxForString(string,len,idx);
            ret=GRR_RET_BAD_DATA;
            goto error;

            case '}':
            fprintf(stderr,"Unmatched curly brace:\n");
            printIdxForString(string,len,idx);
            ret=GRR_RET_BAD_DATA;
            goto error;

            case '\\':
            character=resolveEscapeCharacter(string[++idx]);
            if ( character == GRR_INVALID_CHARACTER ) {
                fprintf(stderr,"Invalid character escape:\n");
                printIdxForString(string,len,idx);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }
            goto add_character;

            case '.':
            character=GRR_WILDCARD_CODE;
            goto add_character;

            case '^':
            if ( current->length > 0 ) {
                fprintf(stderr,"'^' impossible to match:\n");
                printIdxForString(string,len,idx);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }
            character=GRR_FIRST_CHAR_CODE;
            goto add_character;

            case '$':
            character=GRR_LAST_CHAR_CODE;
            goto add_character;

            case '/':
            if ( ++idx == len ) {
                fprintf(stderr,"Expecting character class following '/':\n");
                printIdxForString(string,len,idx-1);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }

            if ( string[idx] == '[' ) {
                ret=resolveCharacterClass(string,len,&idx,&temp);
                if ( ret != GRR_RET_OK ) {
                    goto error;
                }
            }
            else if ( string[idx] == '\\' ) {
                character=resolveEscapeCharacter(string[++idx]);
                if ( character == GRR_INVALID_CHARACTER ) {
                    fprintf(stderr,"Invalid character escape:\n");
                    printIdxForString(string,len,idx);
                    ret=GRR_RET_BAD_DATA;
                    goto error;
                }
                temp=createCharacterNfa(character);
                if ( !temp ) {
                    ret=GRR_RET_OUT_OF_MEMORY;
                    goto error;
                }
            }
            else {
                temp=createCharacterNfa(string[idx]);
                if ( !temp ) {
                    ret=GRR_RET_OUT_OF_MEMORY;
                    goto error;
                }
            }
            temp->nodes[0].transitions[0].symbols[0] |= GRR_NFA_LOOKAHEAD_FLAG;
            temp->nodes[0].transitions[0].symbols[0] &= ~GRR_NFA_EMPTY_TRANSITION_FLAG;

            if ( idx != len-1 ) {
                fprintf(stderr,"Unexpected text following ending bar:\n");
                printIdxForString(string,len,idx+1);
                grrFreeNfa(temp);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }

            ret=concatenateNfas(current,temp);
            if ( ret != GRR_RET_OK ) {
                grrFreeNfa(temp);
                goto error;
            }
            break;

            default:
            add_character:
            temp=createCharacterNfa(character);
            if ( !temp ) {
                ret=GRR_RET_OUT_OF_MEMORY;
                goto error;
            }

            ret=checkForQuantifier(temp,string,len,idx,&idx);
            if ( ret != GRR_RET_OK ) {
                grrFreeNfa(temp);
                goto error;
            }

            ret=concatenateNfas(current,temp);
            if ( ret != GRR_RET_OK ) {
                grrFreeNfa(temp);
                goto error;
            }
            break;
        }
    }

    for (ssize_t k=stack.length-1; k>=0; k--) {
        if ( !stack.frames[k].nfa ) {
            continue;
        }

        if ( stack.frames[k].reason == '(' ) {
            fprintf(stderr,"Unclosed open parenthesis:\n");
            printIdxForString(string,len,stack.frames[k].idx);
            ret=GRR_RET_BAD_DATA;
            goto error;
        }
        ret=addDisjunctionToNfa(stack.frames[k].nfa,current);
        if ( ret != GRR_RET_OK ) {
            goto error;
        }
        current=stack.frames[k].nfa;
        stack.frames[k].nfa=NULL;
    }
    free(stack.frames);

    current->string=malloc(len+1);
    if ( !current->string ) {
        ret=GRR_RET_OUT_OF_MEMORY;
        goto error;
    }
    memcpy(current->string,string,len);
    current->string[len]='\0';

    *nfa=current;
    return GRR_RET_OK;

    error:

    grrFreeNfa(current);
    freeNfaStack(&stack);

    return ret;
}

static void
printIdxForString(const char *string, size_t len, size_t idx)
{
    fprintf(stderr,"\t%.*s\n\t", (int)len, string);
    for (size_t k=0; k<idx; k++) {
        fprintf(stderr," ");
    }
    fprintf(stderr,"^\n");
}

static int
pushNfaToStack(nfaStack *stack, grrNfa nfa, size_t idx, char reason)
{
    if ( stack->length == stack->capacity ) {
        nfaStackFrame *success;
        size_t newCapacity;

        newCapacity=stack->capacity+GRR_NFA_PADDING;
        success=realloc(stack->frames,sizeof(nfaStackFrame)*newCapacity);
        if ( !success ) {
            return GRR_RET_OUT_OF_MEMORY;
        }

        stack->frames=success;
        stack->capacity=newCapacity;
    }

    stack->frames[stack->length].nfa=nfa;
    stack->frames[stack->length].idx=idx;
    stack->frames[stack->length].reason=reason;
    stack->length++;

    return GRR_RET_OK;
}

static void
freeNfaStack(nfaStack *stack)
{
    size_t len;

    len=stack->length;
    for (size_t k=0; k<len; k++) {
        grrFreeNfa(stack->frames[k].nfa);
        stack->frames[k].nfa=NULL;
    }
}

static ssize_t
findParensInStack(const nfaStack *stack)
{
    for (ssize_t idx=(ssize_t)stack->length-1; idx>=0; idx--) {
        if ( stack->frames[idx].nfa && stack->frames[idx].reason == '(' ) {
            return idx;
        }
    }

    return -1;
}

static char
resolveEscapeCharacter(char c)
{
    switch ( c ) {
        case 't':
        return '\t';
        break;

        case '\\':
        case '/':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '.':
        case '*':
        case '+':
        case '?':
        case '^':
        case '$':
        case '|':
        return c;
        break;

        case 's':
        return GRR_WHITESPACE_CODE;

        case 'd':
        return GRR_DIGIT_CODE;

        default:
        return GRR_INVALID_CHARACTER;
    }
}

static grrNfa
createCharacterNfa(char c)
{
    grrNfa nfa;
    nfaNode *nodes;

    nodes=calloc(1,sizeof(nfaNode));
    if ( !nodes ) {
        return NULL;
    }

    nodes->transitions[0].motion=1;
    setSymbol(&nodes->transitions[0],c);
    if ( c == GRR_FIRST_CHAR_CODE || c == GRR_LAST_CHAR_CODE ) {
        setSymbol(&nodes->transitions[0],GRR_EMPTY_TRANSITION_CODE);
    }

    nfa=NEW_NFA();
    if ( !nfa ) {
        free(nodes);
        return NULL;
    }

    nfa->nodes=nodes;
    nfa->length=1;

    return nfa;
}

static void
setSymbol(nfaTransition *transition, char c)
{
    char c2;

    switch ( c ) {
        case GRR_WHITESPACE_CODE:
        transition->symbols[0] |= GRR_NFA_TAB_FLAG;
        c2=' '-GRR_NFA_ASCII_ADJUSTMENT;
        goto set_character;

        case GRR_WILDCARD_CODE:
        memset(transition->symbols+1,0xff,sizeof(transition->symbols)-1);
        transition->symbols[0] |= 0xfe;
        break;

        case GRR_EMPTY_TRANSITION_CODE:
        c2=GRR_NFA_EMPTY_TRANSITION;
        goto set_character;

        case GRR_FIRST_CHAR_CODE:
        c2=GRR_NFA_FIRST_CHAR;
        goto set_character;

        case GRR_LAST_CHAR_CODE:
        c2=GRR_NFA_LAST_CHAR;
        goto set_character;

        case GRR_DIGIT_CODE:
        for (int k=0; k<10; k++) {
            c2='0'+k-GRR_NFA_ASCII_ADJUSTMENT;
            SET_FLAG(transition->symbols,c2);
        }
        break;

        case '\t':
        c2=GRR_NFA_TAB;
        goto set_character;

        default:
        c2=c-GRR_NFA_ASCII_ADJUSTMENT;
        set_character:
        SET_FLAG(transition->symbols,c2);
        break;
    }
}

static int
concatenateNfas(grrNfa nfa1, grrNfa nfa2)
{
    size_t newLen;
    nfaNode *success;

    newLen=nfa1->length+nfa2->length;
    success=realloc(nfa1->nodes,sizeof(nfaNode)*newLen);
    if ( !success ) {
        return GRR_RET_OUT_OF_MEMORY;
    }

    nfa1->nodes=success;

    memcpy(nfa1->nodes+nfa1->length,nfa2->nodes,sizeof(nfaNode)*nfa2->length);
    grrFreeNfa(nfa2);
    nfa1->length=newLen;

    return GRR_RET_OK;
}

static int
addDisjunctionToNfa(grrNfa nfa1, grrNfa nfa2)
{
    unsigned int newLen, len1, len2;
    nfaNode *success;

    len1=nfa1->length;
    len2=nfa2->length;
    newLen=1+len1+len2;
    success=realloc(nfa1->nodes,sizeof(nfaNode)*newLen);
    if ( !success ) {
        return GRR_RET_OUT_OF_MEMORY;
    }
    nfa1->nodes=success;

    memmove(success+1,success,sizeof(nfaNode)*len1);
    memcpy(success+1+len1,nfa2->nodes,sizeof(nfaNode)*len2);
    nfa1->length=newLen;

    memset(success,0,sizeof(nfaNode));
    success[0].two_transitions=1;
    for (int k=0; k<2; k++) {
        setSymbol(&success[0].transitions[k],GRR_EMPTY_TRANSITION_CODE);
    }
    success[0].transitions[0].motion=1;
    success[0].transitions[1].motion=len1+1;

    for (unsigned int k=0; k<len1; k++) {
        for (unsigned int j=0; j<=success[k+1].two_transitions; j++) {
            if ( k + (unsigned int)success[k+1].transitions[j].motion == len1 ) {
                success[k+1].transitions[j].motion += len2;
            }
        }
    }

    grrFreeNfa(nfa2);
    return GRR_RET_OK;
}

static int
checkForQuantifier(grrNfa nfa, const char *string, size_t len, size_t idx, size_t *newIdx)
{
    bool question=false, plus=false;

    if ( idx+1 == len ) {
        return GRR_RET_OK;
    }

    switch ( string[idx+1] ) {
        case '?':
        question=true;
        break;

        case '+':
        plus=true;
        break;

        case '*':
        question=plus=true;
        break;

        case '{':
        return resolveBraces(nfa,string,len,idx+1,newIdx);

        default:
        *newIdx=idx;
        return GRR_RET_OK;
    }

    *newIdx=idx+1;

    if ( plus ) {
        unsigned int length;
        nfaNode *success;

        length=nfa->length;
        success=realloc(nfa->nodes,sizeof(nfaNode)*(length+1));
        if ( !success ) {
            return GRR_RET_OUT_OF_MEMORY;
        }
        nfa->nodes=success;

        memset(success+length,0,sizeof(nfaNode));
        success[length].two_transitions=1;
        for (int k=0; k<2; k++) {
            setSymbol(&success[length].transitions[k],GRR_EMPTY_TRANSITION_CODE);
        }
        success[length].transitions[0].motion=-1*(int)length;
        success[length].transitions[1].motion=1;

        nfa->length++;
    }

    if ( question ) {
        if ( nfa->nodes[0].two_transitions ) {
            nfaNode *success;

            success=realloc(nfa->nodes,sizeof(nfaNode)*(nfa->length+1));
            if ( !success ) {
                return GRR_RET_OUT_OF_MEMORY;
            }
            nfa->nodes=success;

            memmove(success+1,success,sizeof(nfaNode)*nfa->length);
            memset(success,0,sizeof(nfaNode));
            success[0].two_transitions=1;

            for (int k=0; k<2; k++) {
                setSymbol(&success[0].transitions[k],GRR_EMPTY_TRANSITION_CODE);
            }
            success[0].transitions[0].motion=1;
            success[0].transitions[1].motion=nfa->length+1;

            nfa->length++;
        }
        else {
            nfaNode *node;

            node=nfa->nodes;
            memset(node->transitions[1].symbols,0,sizeof(nfa->nodes[0].transitions[1].symbols));
            setSymbol(&node->transitions[1],GRR_EMPTY_TRANSITION_CODE);
            node->transitions[1].motion=nfa->length;
            node->two_transitions=1;
        }
    }

    return GRR_RET_OK;
}

static int
resolveBraces(grrNfa nfa, const char *string, size_t len, size_t idx, size_t *newIdx)
{
    size_t end, numNodes;
    long value;
    char *buffer, *temp;
    nfaNode *success;

    for (end=idx+1; end<len && string[end] != '}'; end++) {
        if ( !isdigit(string[end]) ) {
            fprintf(stderr,"Expected digit inside braces:\n");
            printIdxForString(string,len,end);
            return GRR_RET_BAD_DATA;
        }
    }

    if ( end == len ) {
        fprintf(stderr,"Unclosed brace:\n");
        printIdxForString(string,len,idx);
        return GRR_RET_BAD_DATA;
    }
    if ( end == idx+1 ) {
        fprintf(stderr,"Empty braces:\n");
        printIdxForString(string,len,idx);
        return GRR_RET_BAD_DATA;
    }

    *newIdx=end;

    buffer=alloca(end-idx);
    memcpy(buffer,string+idx+1,end-1-idx);
    buffer[end-idx]='\0';

    errno=0;
    value=strtol(buffer,&temp,10);
    if ( errno != 0 || temp == buffer || temp[0] != '\0' ) {
        fprintf(stderr,"Invalid quantifier inside braces:\n");
        printIdxForString(string,len,idx+1);
        return GRR_RET_BAD_DATA;
    }
    if ( value == 1 ) {
        return GRR_RET_OK;
    }

    numNodes=nfa->length;
    success=realloc(nfa->nodes,sizeof(nfaNode)*numNodes*value);
    if ( !success ) {
        return GRR_RET_OUT_OF_MEMORY;
    }
    nfa->nodes=success;

    for (long k=1; k<value; k++) {
        memcpy(nfa->nodes+k*numNodes,nfa->nodes,sizeof(nfaNode)*numNodes);
    }

    nfa->length*=value;

    return GRR_RET_OK;
}

static int
resolveCharacterClass(const char *string, size_t len, size_t *idx, grrNfa *nfa)
{
    int ret;
    size_t idx2;
    bool negation;
    nfaNode *node;

    if ( *idx == len-1 ) {
        fprintf(stderr,"Unclosed character class:\n");
        printIdxForString(string,len,*idx);
        return GRR_RET_BAD_DATA;
    }

    node=calloc(1,sizeof(nfaNode));
    if ( !node ) {
        return GRR_RET_OUT_OF_MEMORY;
    }

    idx2=*idx;
    if ( string[*idx+1] == '^' ) {
        negation=true;
        *idx += 2;
    }
    else {
        negation=false;
        (*idx)++;
    }
    if ( *idx < len && string[*idx] == '-' ) {
        setSymbol(&node->transitions[0],'-');
        (*idx)++;
    }
    while ( *idx < len-1 && string[*idx] != ']' ) {
        char character;

        character=string[*idx];

        if ( string[*idx+1] == '-' ) {
            char possibleRangeEnd, character2;

            if ( *idx == len-2 ) {
                fprintf(stderr,"Unclosed range in character class:\n");
                printIdxForString(string,len,*idx);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }

            if ( character >= 'A' && character < 'Z' ) {
                possibleRangeEnd='Z';
            }
            else if ( character >= 'a' && character < 'z' ) {
                possibleRangeEnd='z';
            }
            else if ( character >= '0' && character < '9' ) {
                possibleRangeEnd='9';
            }
            else {
                fprintf(stderr,"Invalid character class range:\n");
                printIdxForString(string,len,*idx);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }

            character2=string[*idx+2];
            if ( ! ( character2 > character && character2 <= possibleRangeEnd ) ) {
                fprintf(stderr,"Invalid character class range:\n");
                printIdxForString(string,len,*idx);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }

            for (char c=character; c<=character2; c++) {
                setSymbol(&node->transitions[0],c);
            }

            *idx+=3;
            continue;
        }

        if ( character == '\\' ) {
            character=string[*idx+1];

            switch ( character ) {
                case '[':
                case ']':
                break;

                case 't':
                character='\t';
                break;

                default:
                fprintf(stderr,"Invalid character escape:\n");
                printIdxForString(string,len,*idx);
                ret=GRR_RET_BAD_DATA;
                goto error;
            }

            *idx+=2;
        }
        else {
            (*idx)++;
        }

        setSymbol(&node->transitions[0],character);
    }

    if ( *idx >= len || string[*idx] != ']' ) {
        fprintf(stderr,"Unclosed character class:\n");
        printIdxForString(string,len,idx2);
        ret=GRR_RET_BAD_DATA;
        goto error;
    }

    if ( negation ) {
        for (size_t k=0; k<sizeof(node->transitions[0].symbols); k++) {
            node->transitions[0].symbols[k] ^= 0xff;
        }
        node->transitions[0].symbols[0] &= ~GRR_NFA_FIRST_CHAR_FLAG;
        node->transitions[0].symbols[0] &= ~GRR_NFA_LAST_CHAR_FLAG;
        node->transitions[0].symbols[0] &= ~GRR_NFA_LOOKAHEAD_FLAG;
    }

    node->transitions[0].motion=1;

    *nfa=NEW_NFA();
    if ( !*nfa ) {
        ret=GRR_RET_OUT_OF_MEMORY;
        goto error;
    }
    (*nfa)->nodes=node;
    (*nfa)->length=1;

    ret=checkForQuantifier(*nfa,string,len,*idx,idx);
    if ( ret != GRR_RET_OK ) {
        grrFreeNfa(*nfa);
        return ret;
    }

    return GRR_RET_OK;

    error:

    free(node);
    return ret;
}
