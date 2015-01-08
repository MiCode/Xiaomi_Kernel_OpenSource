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

#ifndef __IA_CSS_XNR3_PARAM_H
#define __IA_CSS_XNR3_PARAM_H

#include "type_support.h"

/* Scaling factor of the alpha values: which fixed-point value represents 1.0?
 * It must be chosen such that 1/min_sigma still fits in an ISP vector
 * element. */
#define XNR_ALPHA_SCALE_LOG2        5
#define XNR_ALPHA_SCALE_FACTOR      (1 << XNR_ALPHA_SCALE_LOG2)

/* Scaling factor of the coring values on the ISP. */
#define XNR_CORING_SCALE_LOG2       (ISP_VEC_ELEMBITS-1)
#define XNR_CORING_SCALE_FACTOR     (1 << XNR_CORING_SCALE_LOG2)

/* XNR3 filter size. Must be 11x11, 9x9 or 5x5. */
#ifdef FLT_KERNEL_9x9
#define XNR_FILTER_SIZE             9
#else
#ifdef FLT_KERNEL_11x11
#define XNR_FILTER_SIZE             11
#else
#define XNR_FILTER_SIZE             5
#endif
#endif

/* XNR3 alpha (1/sigma) parameters on the ISP, expressed as a base (0) value
 * for dark areas, and a scaled diff towards the value for bright areas. */
struct sh_css_xnr3_alpha_params {
	int32_t y0;
	int32_t u0;
	int32_t v0;
	int32_t ydiff;
	int32_t udiff;
	int32_t vdiff;
};

/* XNR3 coring parameters on the ISP, expressed as a base (0) value
 * for dark areas, and a scaled diff towards the value for bright areas. */
struct sh_css_xnr3_coring_params {
	int32_t u0;
	int32_t v0;
	int32_t udiff;
	int32_t vdiff;
};

/* XNR3 ISP parameters */
struct sh_css_isp_xnr3_params {
	struct sh_css_xnr3_alpha_params  alpha;
	struct sh_css_xnr3_coring_params coring;
};

#endif  /*__IA_CSS_XNR3_PARAM_H */
