/*
 * This header provides macros for different types and conversions
 */

#ifndef _DT_BINDINGS_TYPES_H_
#define _DT_BINDINGS_TYPES_H_

/*
 * S32_TO_U32: This macro converts the signed number to 2's complement
 * unisgned number. E.g. S32_TO_U32(-3) will be 0xfffffffd and
 * S32_TO_U32(3) will be 0x3;
 * Use of_property_read_s32() for getting back the correct signed value
 * in driver.
 */
#define S32_TO_U32(x) (((x) < 0) ? (((-(x)) ^ 0xFFFFFFFFU) + 1) : (x))

#endif

