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

#ifndef __IA_CSS_DPC2_PARAM_H
#define __IA_CSS_DPC2_PARAM_H

#include "type_support.h"
#include "vmem.h" /* for VMEM_ARRAY*/


/* 4 planes : GR, R, B, GB */
#define NUM_PLANES		4

/* ToDo: Move this to testsetup */
#define MAX_FRAME_SIMDWIDTH	30

/* 3 lines state per color plane input_line_state */
#define DPC2_STATE_INPUT_BUFFER_HEIGHT	(3 * NUM_PLANES)
/* Each plane has width equal to half frame line */
#define DPC2_STATE_INPUT_BUFFER_WIDTH	CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)

/* 1 line state per color plane for local deviation state*/
#define DPC2_STATE_LOCAL_DEVIATION_BUFFER_HEIGHT	(1 * NUM_PLANES)
/* Each plane has width equal to half frame line */
#define DPC2_STATE_LOCAL_DEVIATION_BUFFER_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)

/* MINMAX state buffer stores 1 full input line (GR-R color line) */
#define DPC2_STATE_SECOND_MINMAX_BUFFER_HEIGHT	1
#define DPC2_STATE_SECOND_MINMAX_BUFFER_WIDTH	MAX_FRAME_SIMDWIDTH


struct ia_css_isp_dpc2_params {
	int32_t one_plus_metric1;
	int32_t one_minus_metric1;
	int32_t one_plus_metric3;
	int32_t wb_gain_gr;
	int32_t wb_gain_r;
	int32_t wb_gain_b;
	int32_t wb_gain_gb;
	int32_t wb_gain_gr_scaled_by_metric2;
	int32_t wb_gain_r_scaled_by_metric2;
	int32_t wb_gain_b_scaled_by_metric2;
	int32_t wb_gain_gb_scaled_by_metric2;
};

#endif /* __IA_CSS_DPC2_PARAM_H */
