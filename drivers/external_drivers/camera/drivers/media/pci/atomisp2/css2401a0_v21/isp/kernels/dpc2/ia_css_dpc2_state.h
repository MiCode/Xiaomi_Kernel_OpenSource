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

#ifndef __IA_CSS_DPC2_STATE_H
#define __IA_CSS_DPC2_STATE_H

#include "type_support.h"
#include "vmem.h" /* for VMEM_ARRAY*/

#include "ia_css_dpc2_param.h"

struct sh_css_isp_dpc2_vmem_state {
	VMEM_ARRAY(dpc2_input_lines[DPC2_STATE_INPUT_BUFFER_HEIGHT], DPC2_STATE_INPUT_BUFFER_WIDTH*ISP_NWAY);
	VMEM_ARRAY(dpc2_local_deviations[DPC2_STATE_LOCAL_DEVIATION_BUFFER_HEIGHT], DPC2_STATE_LOCAL_DEVIATION_BUFFER_WIDTH*ISP_NWAY);
	VMEM_ARRAY(dpc2_second_min[DPC2_STATE_SECOND_MINMAX_BUFFER_HEIGHT], DPC2_STATE_SECOND_MINMAX_BUFFER_WIDTH*ISP_NWAY);
	VMEM_ARRAY(dpc2_second_max[DPC2_STATE_SECOND_MINMAX_BUFFER_HEIGHT], DPC2_STATE_SECOND_MINMAX_BUFFER_WIDTH*ISP_NWAY);
};

#endif /* __IA_CSS_DPC2_STATE_H */
