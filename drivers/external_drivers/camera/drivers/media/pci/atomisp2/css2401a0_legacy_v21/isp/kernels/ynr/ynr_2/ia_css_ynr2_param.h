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

#ifndef __IA_CSS_YNR2_PARAM_H
#define __IA_CSS_YNR2_PARAM_H

#include "type_support.h"

/* YNR (Y Noise Reduction), YEE (Y Edge Enhancement) */
struct sh_css_isp_yee2_params {
	int32_t edge_sense_gain_0;
	int32_t edge_sense_gain_1;
	int32_t corner_sense_gain_0;
	int32_t corner_sense_gain_1;
};

/* Fringe Control */
struct sh_css_isp_fc_params {
	int32_t gain_exp;
	uint16_t coring_pos_0;
	uint16_t coring_pos_1;
	uint16_t coring_neg_0;
	uint16_t coring_neg_1;
	int32_t gain_pos_0;
	int32_t gain_pos_1;
	int32_t gain_neg_0;
	int32_t gain_neg_1;
	int32_t crop_pos_0;
	int32_t crop_pos_1;
	int32_t crop_neg_0;
	int32_t crop_neg_1;
};

#endif /* __IA_CSS_YNR2_PARAM_H */
