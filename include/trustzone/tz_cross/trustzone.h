/*
 * Basic Data Types & function for common TZ.
 */

#ifndef __REE_TRUSTZONE_H__
#define __REE_TRUSTZONE_H__

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Temp memory reference paramter define
 * The parameter pass by copying data. The size limit for temp memory
 * parameter is 4KB.
 *
 * @param buffer    A pointer to the buffer.
 * @param size    Buffer size in bytes.
 */
typedef struct {
    void *buffer;
    uint32_t size;
} MTEEC_MEM;

/**
 * Registed shared memory paramter define
 *
 * @param handle    memory handle.
 * @param offset    Offset size in bytes. The shared memory is used based
 *                  on this offset.
 * @param size    Buffer size in bytes. 
 */
typedef struct {
    uint32_t handle;
    uint32_t offset;
    uint32_t size;
} MTEEC_MEMREF;

/**
 * Registed shared memory paramter define
 *
 * @param a    Implementation defined value.
 * @param b    Implementation defined value.
 */
typedef struct {
    uint32_t a;
    uint32_t b;
} MTEEC_VALUE;

/**
 * Paramter define
 *
 * @param mem    Parameter for temp memory reference. Parameter types are TZPT_MEM_XXX.
 * @param memref    Parameter for registed shared memory or allocated secure memory. 
 * Parameter types are TZPT_MEMREF_XXX.
 * @param value    Parameter for value. Parameter types are TZPT_VALUE_XXX.
 */
typedef union {
    MTEEC_MEM mem;
    MTEEC_MEMREF memref;
    MTEEC_VALUE value;
} MTEEC_PARAM;

/**
 * Parameter type define
 *
 * @see TZ_PARAM_TYPES
 */
typedef enum 
{
    TZPT_NONE                      = 0,
    TZPT_VALUE_INPUT               = 1,
    TZPT_VALUE_OUTPUT              = 2,
    TZPT_VALUE_INOUT               = 3,
    TZPT_MEM_INPUT                 = 4,
    TZPT_MEM_OUTPUT                = 5,
    TZPT_MEM_INOUT                 = 6,
    TZPT_MEMREF_INPUT              = 7,
    TZPT_MEMREF_OUTPUT             = 8,
    TZPT_MEMREF_INOUT              = 9,
} TZ_PARAM_TYPES;


/// Macros to build parameter types for ?REE_TeeServiceCall
/// @see TZ_ParamTypes
#define TZ_ParamTypes1(t1)          TZ_ParamTypes(t1, TZPT_NONE, TZPT_NONE, TZPT_NONE)
#define TZ_ParamTypes2(t1,t2)       TZ_ParamTypes(t1, t2, TZPT_NONE, TZPT_NONE)
#define TZ_ParamTypes3(t1,t2,t3)    TZ_ParamTypes(t1, t2, t3, TZPT_NONE)
#define TZ_ParamTypes4(t1,t2,t3,t4)    TZ_ParamTypes(t1, t2, t3, t4)

/**
 * Macros to build parameter types for ?REE_TeeServiceCall
 *
 * @see KREE_TeeServiceCall
 * @see UREE_TeeServiceCall
 * @param t1 types for param[0]
 * @param t2 types for param[1]
 * @param t3 types for param[2]
 * @param t4 types for param[3]
 * @return value for paramTypes.
 */
static inline uint32_t TZ_ParamTypes(TZ_PARAM_TYPES t1, TZ_PARAM_TYPES t2,
                              TZ_PARAM_TYPES t3, TZ_PARAM_TYPES t4)
{
    return (TZ_PARAM_TYPES)(t1 | (t2 << 8) | (t3 << 16) | (t4 << 24));
}

/* 
 * Get TZ_PARAM_TYPES for a parameter.
 *
 * @param paramTypes paramTypes packed by TZ_ParamTypes.
 * @param num Which parameter types to get.
 */
static inline TZ_PARAM_TYPES TZ_GetParamTypes(uint32_t paramTypes, int num)
{
    return (TZ_PARAM_TYPES)((paramTypes >> (8*num)) & 0xff);
}

/**
 * Return code
 *
 * This global return code is used for both REE and TEE.
 * Implementation-Defined 0x00000001 - 0xFFFEFFFF
 * Reserved for Future Use 0xFFFF0011 V 0xFFFFFFFF
 *
 * @see TZ_RESULT
 */
#define TZ_RESULT_SUCCESS 0x00000000 // The operation was successful.
#define TZ_RESULT_ERROR_GENERIC 0xFFFF0000 // Non-specific cause.
#define TZ_RESULT_ERROR_ACCESS_DENIED 0xFFFF0001 // Access privileges are not sufficient.
#define TZ_RESULT_ERROR_CANCEL 0xFFFF0002 // The operation was cancelled.
#define TZ_RESULT_ERROR_ACCESS_CONFLICT 0xFFFF0003 // Concurrent accesses caused conflict.
#define TZ_RESULT_ERROR_EXCESS_DATA 0xFFFF0004 // Too much data for the requested operation was passed.
#define TZ_RESULT_ERROR_BAD_FORMAT 0xFFFF0005 // Input data was of invalid format.
#define TZ_RESULT_ERROR_BAD_PARAMETERS 0xFFFF0006 // Input parameters were invalid.
#define TZ_RESULT_ERROR_BAD_STATE 0xFFFF0007 // Operation is not valid in the current state.
#define TZ_RESULT_ERROR_ITEM_NOT_FOUND 0xFFFF0008 // The requested data item is not found.
#define TZ_RESULT_ERROR_NOT_IMPLEMENTED 0xFFFF0009 // The requested operation should exist but is not yet implemented.
#define TZ_RESULT_ERROR_NOT_SUPPORTED 0xFFFF000A // The requested operation is valid but is not supported in this Implementation.
#define TZ_RESULT_ERROR_NO_DATA 0xFFFF000B // Expected data was missing.
#define TZ_RESULT_ERROR_OUT_OF_MEMORY 0xFFFF000C // System ran out of resources.
#define TZ_RESULT_ERROR_BUSY 0xFFFF000D // The system is busy working on something else.
#define TZ_RESULT_ERROR_COMMUNICATION 0xFFFF000E // Communication with a remote party failed.
#define TZ_RESULT_ERROR_SECURITY 0xFFFF000F // A security fault was detected.
#define TZ_RESULT_ERROR_SHORT_BUFFER 0xFFFF0010 // The supplied buffer is too short for the generated output.
#define TZ_RESULT_ERROR_INVALID_HANDLE 0xFFFF0011 // The handle is invalid.

typedef int TZ_RESULT;

/**
 * Return a human readable error string.
 */
const char *TZ_GetErrorString(TZ_RESULT res);

#ifdef __cplusplus
}
#endif

#endif /* __REE_TRUSTZONE_H__ */
