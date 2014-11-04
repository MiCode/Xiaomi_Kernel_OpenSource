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

#ifndef __IA_CSS_VF_PARAM_H
#define __IA_CSS_VF_PARAM_H

#include "type_support.h"
#include "dma.h"
#include "gc/gc_1.0/ia_css_gc_param.h" /* GAMMA_OUTPUT_BITS */
#include "ia_css_frame_comm.h" /* ia_css_frame_sp_info */
#include "ia_css_vf_types.h"

#define VFDEC_BITS_PER_PIXEL	GAMMA_OUTPUT_BITS

/** Viewfinder decimation */
struct sh_css_isp_vf_isp_config {
	uint32_t vf_downscale_bits; /**< Log VF downscale value */
	uint32_t enable;
	struct ia_css_frame_sp_info info;
	struct {
		uint32_t width_a_over_b;
		struct dma_port_config port_b;
	} dma;
};

#endif /* __IA_CSS_VF_PARAM_H */
