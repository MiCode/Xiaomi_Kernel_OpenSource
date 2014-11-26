/* Release Version: irci_master_20141125_0453 */
/* Release Version: irci_master_20141125_0453 */
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

#ifndef __IA_CSS_HDR_PARAMS_H
#define __IA_CSS_HDR_PARAMS_H

#include "type_support.h"

#define HDR_NUM_INPUT_FRAMES         (3)

/* HDR irradiance map parameters on ISP. */
struct sh_css_hdr_irradiance_params {
	int32_t test_irr;
	int32_t match_shift[HDR_NUM_INPUT_FRAMES - 1];  /* Histogram matching shift parameter */
	int32_t match_mul[HDR_NUM_INPUT_FRAMES - 1];    /* Histogram matching multiplication parameter */
	int32_t thr_low[HDR_NUM_INPUT_FRAMES - 1];      /* Weight map soft threshold low bound parameter */
	int32_t thr_high[HDR_NUM_INPUT_FRAMES - 1];     /* Weight map soft threshold high bound parameter */
	int32_t thr_coeff[HDR_NUM_INPUT_FRAMES - 1];    /* Soft threshold linear function coefficient */
	int32_t thr_shift[HDR_NUM_INPUT_FRAMES - 1];    /* Soft threshold precision shift parameter */
	int32_t weight_bpp;                             /* Weight map bits per pixel */
};

/* HDR deghosting parameters on ISP */
struct sh_css_hdr_deghost_params {
	int32_t test_deg;
};

/* HDR exclusion parameters on ISP */
struct sh_css_hdr_exclusion_params {
	int32_t test_excl;
};

/* HDR ISP parameters */
struct sh_css_isp_hdr_params {
	struct sh_css_hdr_irradiance_params irradiance;
	struct sh_css_hdr_deghost_params    deghost;
	struct sh_css_hdr_exclusion_params  exclusion;
};

#endif /* __IA_CSS_HDR_PARAMS_H */
