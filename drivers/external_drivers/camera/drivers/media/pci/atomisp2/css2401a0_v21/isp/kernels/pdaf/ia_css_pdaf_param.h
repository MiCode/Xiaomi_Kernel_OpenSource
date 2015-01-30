/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __IA_CSS_PDAF_PARAM_H
#define __IA_CSS_PDAF_PARAM_H

#include "type_support.h"

/*
 * Extraction configuration parameters
 */

struct sh_css_isp_extraction_dmem_params
{
	uint8_t num_x_patterns;
	uint8_t num_y_patterns;
};

struct sh_css_isp_extraction_vmem_params
{
	int16_t y_step_size[ISP_NWAY];
	int16_t y_offset[ISP_NWAY];
	int16_t x_step_size[ISP_NWAY];
	int16_t x_offset[ISP_NWAY];
};

/*
 * PDAF configuration parameters
 */
struct sh_css_isp_pdaf_vmem_params
{
	struct sh_css_isp_extraction_vmem_params ext_cfg_data_vmem;
};

struct sh_css_isp_pdaf_dmem_params
{
	uint16_t frm_length;
	uint16_t frm_width;
	struct sh_css_isp_extraction_dmem_params ext_cfg_data_dmem;
};

#endif /* __IA_CSS_PDAF_PARAM_H */
