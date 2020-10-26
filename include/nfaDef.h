/**
 * \file    nfaDef.h
 * \brief   Basic GrrEngine definitions.
 */

#ifndef __GRR_ENGINE_NFA_DEF_H__
#define __GRR_ENGINE_NFA_DEF_H__

/**
 * \brief Function return values.
 */
enum grrRetValue {
    /// The function succeeded.
    GRR_RET_OK = 0,
    /// Bad arguments were passed to the function,
    GRR_RET_BAD_ARGS,
    /// The requested item was not found.
    GRR_RET_NOT_FOUND,
    /// Memory allocation failure.
    GRR_RET_OUT_OF_MEMORY,
    /// Invalid data was passed to the function.
    GRR_RET_BAD_DATA,
};

/**
 * \brief   An opaque reference to GrrEngine's regex object.
 */
typedef struct grrNfaStruct *grrNfa;

#endif // __GRR_ENGINE_NFA_DEF_H__
