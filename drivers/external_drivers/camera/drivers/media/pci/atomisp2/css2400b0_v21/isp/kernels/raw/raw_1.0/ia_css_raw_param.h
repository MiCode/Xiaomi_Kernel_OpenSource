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

#ifndef __IA_CSS_RAW_PARAM_H
#define __IA_CSS_RAW_PARAM_H

#include "type_support.h"

#include "dma.h"

/* Raw channel */
struct sh_css_isp_raw_isp_config {
	uint32_t width_a_over_b;
	struct dma_port_config port_b;
	uint32_t inout_port_config;
	uint32_t input_needs_raw_binning;
	uint32_t format; /* enum ia_css_frame_format */
	uint32_t required_bds_factor;
	uint32_t two_ppc;
	uint32_t stream_format; /* enum sh_stream_format */
	uint32_t deinterleaved;
	uint8_t start_column; /*left crop offset*/
	uint8_t start_line; /*top crop offset*/
	uint8_t enable_left_padding; /*need this for multiple binary case*/
};

#endif /* __IA_CSS_RAW_PARAM_H */
