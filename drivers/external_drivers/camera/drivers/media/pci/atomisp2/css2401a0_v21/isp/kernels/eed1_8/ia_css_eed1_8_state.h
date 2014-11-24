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

#ifndef __IA_CSS_EED1_8_STATE_H
#define __IA_CSS_EED1_8_STATE_H

#include "type_support.h"
#include "vmem.h" /* for VMEM_ARRAY*/

#include "ia_css_eed1_8_param.h"

struct ia_css_isp_eed1_8_vmem_state {
	VMEM_ARRAY(eed1_8_input_lines[EED1_8_STATE_INPUT_BUFFER_HEIGHT], EED1_8_STATE_INPUT_BUFFER_WIDTH*ISP_NWAY);
	VMEM_ARRAY(eed1_8_rb_zipped[EED1_8_STATE_RB_ZIPPED_HEIGHT], EED1_8_STATE_RB_ZIPPED_WIDTH*ISP_NWAY);
	VMEM_ARRAY(eed1_8_Yc[EED1_8_STATE_YC_HEIGHT], EED1_8_STATE_YC_WIDTH*ISP_NWAY);
	VMEM_ARRAY(eed1_8_Cg[EED1_8_STATE_CG_HEIGHT], EED1_8_STATE_CG_WIDTH*ISP_NWAY);
	VMEM_ARRAY(eed1_8_Co[EED1_8_STATE_CO_HEIGHT], EED1_8_STATE_CO_WIDTH*ISP_NWAY);
	VMEM_ARRAY(eed1_8_AbsK[EED1_8_STATE_ABSK_HEIGHT], EED1_8_STATE_ABSK_WIDTH*ISP_NWAY);
};

#endif /* __IA_CSS_EED1_8_STATE_H */
