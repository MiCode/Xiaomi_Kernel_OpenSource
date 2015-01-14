/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
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

#ifndef __IA_CSS_HB_PARAM_H
#define __IA_CSS_HB_PARAM_H

#include "type_support.h"

#ifndef PIPE_GENERATION
#define __INLINE_HMEM__
#include "hmem.h"
#endif

#include "ia_css_bh_types.h"

/* AE (3A Support) */
struct sh_css_isp_bh_params {
	/* coefficients to calculate Y */
	int32_t y_coef_r;
	int32_t y_coef_g;
	int32_t y_coef_b;
};

/* This should be hmem_data_t, but that breaks the pipe generator */
struct sh_css_isp_bh_hmem_params {
	uint32_t bh[ISP_HIST_COMPONENTS][IA_CSS_HMEM_BH_UNIT_SIZE];
};

#endif /* __IA_CSS_HB_PARAM_H */
