/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __MATH_SUPPORT_H
#define __MATH_SUPPORT_H

#include "storage_class.h" /* for STORAGE_CLASS_INLINE */
#if defined(__KERNEL__)
#include <linux/kernel.h> /* Override the definition of max/min from linux kernel*/
#endif /*__KERNEL__*/

#if defined(_MSC_VER)
#include <stdlib.h> /* Override the definition of max/min from stdlib.h*/
#endif /* _MSC_VER */

/* in case we have min/max/MIN/MAX macro's undefine them */
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef MIN /* also defined in include/hrt/numeric.h from SDK */
#undef MIN
#endif
#ifdef MAX
#undef MAX
#endif
#ifdef ABS
#undef ABS
#endif

#define IS_ODD(a)            ((a) & 0x1)
#define IS_EVEN(a)           (!IS_ODD(a))

/* force a value to a lower even value */
#define EVEN_FLOOR(x)        ((x) & ~1)

/* A => B */
#define IMPLIES(a, b)        (!(a) || (b))

#define ABS(a)               ((a) >= 0 ? (a) : -(a))

/* for preprocessor and array sizing use MIN and MAX
   otherwise use min and max */
#define MAX(a, b)            (((a) > (b)) ? (a) : (b))
#define MIN(a, b)            (((a) < (b)) ? (a) : (b))
#define CEIL_DIV(a, b)       ((b) ? ((a) + (b) - 1) / (b) : 0)
#define CEIL_MUL(a, b)       (CEIL_DIV(a, b) * (b))
#define CEIL_MUL2(a, b)      (((a) + (b) - 1) & ~((b) - 1))
#define CEIL_SHIFT(a, b)     (((a) + (1 << (b)) - 1)>>(b))
#define CEIL_SHIFT_MUL(a, b) (CEIL_SHIFT(a, b) << (b))

/* min and max should not be macros as they will evaluate their arguments twice.
   if you really need a macro (e.g. for CPP or for initializing an array)
   use MIN() and MAX(), otherwise use min() and max().


*/

#if !defined(PIPE_GENERATION)

#ifndef INLINE_MATH_SUPPORT_UTILS
/*
This macro versions are added back as we are mixing types in usage of inline.
This causes corner cases of calculations to be incorrect due to conversions
between signed and unsigned variables or overflows.
Before the addition of the inline functions, max, min and ceil_div were macros
and therefore adding them back.

Leaving out the other math utility functions as they are newly added
*/

#define max(a, b)		(MAX(a, b))
#define min(a, b)		(MIN(a, b))
#define ceil_div(a, b)		(CEIL_DIV(a, b))

#else /* !defined(INLINE_MATH_SUPPORT_UTILS) */

STORAGE_CLASS_INLINE int max(int a, int b)
{
	return MAX(a, b);
}

STORAGE_CLASS_INLINE int min(int a, int b)
{
	return MIN(a, b);
}

STORAGE_CLASS_INLINE unsigned int ceil_div(unsigned int a, unsigned int b)
{
	return CEIL_DIV(a, b);
}
#endif /* !defined(INLINE_MATH_SUPPORT_UTILS) */

STORAGE_CLASS_INLINE unsigned int umax(unsigned int a, unsigned int b)
{
	return MAX(a, b);
}

STORAGE_CLASS_INLINE unsigned int umin(unsigned int a, unsigned int b)
{
	return MIN(a, b);
}


STORAGE_CLASS_INLINE unsigned int ceil_mul(unsigned int a, unsigned int b)
{
	return CEIL_MUL(a, b);
}

STORAGE_CLASS_INLINE unsigned int ceil_mul2(unsigned int a, unsigned int b)
{
	return CEIL_MUL2(a, b);
}

STORAGE_CLASS_INLINE unsigned int ceil_shift(unsigned int a, unsigned int b)
{
	return CEIL_SHIFT(a, b);
}

STORAGE_CLASS_INLINE unsigned int ceil_shift_mul(unsigned int a, unsigned int b)
{
	return CEIL_SHIFT_MUL(a, b);
}

#endif /* !defined(PIPE_GENERATION) */

#if !defined(__ISP) && !defined(__SP)
/*
 * For SP and ISP, SDK provides the definition of OP_std_modadd.
 * We need it only for host
 */
#define OP_std_modadd(base, offset, size) ((base+offset)%(size))
#endif /* !defined(__ISP) && !defined(__SP) */

#if !defined(__KERNEL__)
#define clamp(a, min_val, max_val) MIN(MAX((a), (min_val)), (max_val))
#endif /* !defined(__KERNEL__) */

#endif /* __MATH_SUPPORT_H */
