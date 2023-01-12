/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifdef _FIXP_ARITH_H
#error "This implementation is meant to override fixp-arith.h, don't use both"
#endif

#ifndef _FIXEDPOINT_H_
#define _FIXEDPOINT_H_

#include <linux/types.h>
#include <linux/bits.h>

/*
 * Normally would typedef'ed, but checkpatch doesn't like typedef.
 * Also should be normally typedef'ed to intmax_t but that doesn't seem to be
 * available in the kernel
 */
#define fp_t size_t

/* (Arbitrarily) make the first 25% of the bits to be the fractional bits */
#define FP_FRACTIONAL_BITS ((sizeof(fp_t) * 8) / 4)

#define FP(__i, __f_n, __f_d) \
	((((fp_t)(__i)) << FP_FRACTIONAL_BITS) + \
	(((__f_n) << FP_FRACTIONAL_BITS) / (__f_d)))

#define FP_INT(__i) FP(__i, 0, 1)
#define FP_ONE FP_INT(1)
#define FP_ZERO FP_INT(0)

static inline size_t fp_frac_base(void)
{
	return GENMASK(FP_FRACTIONAL_BITS - 1, 0);
}

static inline size_t fp_frac(fp_t a)
{
	return a & GENMASK(FP_FRACTIONAL_BITS - 1, 0);
}

static inline size_t fp_int(fp_t a)
{
	return a >> FP_FRACTIONAL_BITS;
}

static inline size_t fp_round(fp_t a)
{
	/* is the fractional part >= frac_max / 2? */
	bool round_up = fp_frac(a) >= fp_frac_base() / 2;

	return fp_int(a) + round_up;
}

static inline fp_t fp_mult(fp_t a, fp_t b)
{
	return (a * b) >> FP_FRACTIONAL_BITS;
}


static inline fp_t fp_div(fp_t a, fp_t b)
{
	return (a << FP_FRACTIONAL_BITS) / b;
}

#endif
