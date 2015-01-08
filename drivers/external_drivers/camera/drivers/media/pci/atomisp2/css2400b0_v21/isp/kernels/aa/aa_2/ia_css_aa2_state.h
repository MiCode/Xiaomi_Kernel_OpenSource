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

#ifndef __IA_CSS_AA2_STATE_H
#define __IA_CSS_AA2_STATE_H

#include "type_support.h"
#include "vmem.h" /* for VMEM_ARRAY*/

/* Denotes the maximum number of pixels per line that can be processed:
* MAX_AA_VECTORS_PER_LINE  = maximum_line_width / ISP_NWAY */
#ifndef MAX_AA_VECTORS_PER_LINE
#error Please define MAX_AA_VECTORS_PER_LINE.
#endif

/* This uses 2 history lines for both y, u and v*/
#define AA_STATE_Y_BUFFER_HEIGHT	2
#define AA_STATE_UV_BUFFER_HEIGHT	2
#define AA_STATE_Y_BUFFER_WIDTH		MAX_AA_VECTORS_PER_LINE
/* The number of u and v elements is half y due to yuv420 downsampling. */
#define AA_STATE_UV_BUFFER_WIDTH	(AA_STATE_Y_BUFFER_WIDTH/2)


struct ia_css_isp_aa_vmem_state {
	VMEM_ARRAY(y[AA_STATE_Y_BUFFER_HEIGHT], AA_STATE_Y_BUFFER_WIDTH*ISP_NWAY);
	VMEM_ARRAY(u[AA_STATE_UV_BUFFER_HEIGHT], AA_STATE_UV_BUFFER_WIDTH*ISP_NWAY);
	VMEM_ARRAY(v[AA_STATE_UV_BUFFER_HEIGHT], AA_STATE_UV_BUFFER_WIDTH*ISP_NWAY);
};

#endif /* __IA_CSS_AA2_STATE_H */
