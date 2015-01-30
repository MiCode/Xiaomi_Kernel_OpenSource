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

#ifndef __IA_CSS_PDAF_TYPES_H
#define __IA_CSS_PDAF_TYPES_H

#include "type_support.h"
#include "isp2600_config.h"
/*
* CSS-API header file for PDAF parameters
* These parameters shall be filled by host/driver
* and will be converted to ISP parameters in encode
* function.
*/
struct ia_css_extraction_config
{
	uint8_t num_x_patterns;
	uint8_t num_y_patterns;
	int16_t y_step_size[ISP_NWAY];
	int16_t y_offset[ISP_NWAY];
	int16_t x_step_size[ISP_NWAY];
	int16_t x_offset[ISP_NWAY];
};

struct ia_css_pdaf_config
{
	uint16_t frm_length;
	uint16_t frm_width;
	struct ia_css_extraction_config ext_cfg_data;
};

#endif /* __IA_CSS_PDAF_TYPES_H */
