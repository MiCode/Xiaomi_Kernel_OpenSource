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

#ifndef __IA_CSS_TNR_PARAM_H
#define __IA_CSS_TNR_PARAM_H

#include "type_support.h"
#include "sh_css_defs.h"
#include "dma.h"

/* TNR (Temporal Noise Reduction) */
struct sh_css_isp_tnr_params {
	int32_t coef;
	int32_t threshold_Y;
	int32_t threshold_C;
};

struct ia_css_tnr_configuration {
	const struct ia_css_frame *tnr_frames[NUM_VIDEO_TNR_FRAMES];
};

struct sh_css_isp_tnr_isp_config {
	uint32_t width_a_over_b;
	uint32_t frame_height;
	struct dma_port_config port_b;
	hrt_vaddress tnr_frame_addr[NUM_VIDEO_TNR_FRAMES];
};

#endif /* __IA_CSS_TNR_PARAM_H */
