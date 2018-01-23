/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef _FIXP_ARITH_H
#error "This implementation is meant to override fixp-arith.h, don't use both"
#endif

#ifndef __FP_H__
#define __FP_H__

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
