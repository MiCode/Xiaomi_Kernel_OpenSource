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

#ifndef __IA_CSS_BNR2_2_PARAM_H
#define __IA_CSS_BNR2_2_PARAM_H

#include "type_support.h"

/* BNR (Bayer Noise Reduction) ISP parameters */
struct sh_css_isp_bnr2_2_params {
	int32_t d_var_gain_r;
	int32_t d_var_gain_g;
	int32_t d_var_gain_b;
	int32_t d_var_gain_slope_r;
	int32_t d_var_gain_slope_g;
	int32_t d_var_gain_slope_b;
	int32_t n_var_gain_r;
	int32_t n_var_gain_g;
	int32_t n_var_gain_b;
	int32_t n_var_gain_slope_r;
	int32_t n_var_gain_slope_g;
	int32_t n_var_gain_slope_b;
	int32_t dir_thres;
	int32_t dir_thres_w;
	int32_t var_offset_coef;
	int32_t dir_gain;
	int32_t detail_gain;
	int32_t detail_gain_divisor;
	int32_t detail_level_offset;
	int32_t d_var_th_min;
	int32_t d_var_th_max;
	int32_t n_var_th_min;
	int32_t n_var_th_max;
};

#endif /* __IA_CSS_BNR2_2_PARAM_H */
