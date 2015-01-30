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

#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "sh_css_frac.h"
#include "ia_css_pdaf.host.h"

const struct ia_css_pdaf_config default_pdaf_config =
{
	0, 0,
	{ 0, 0,
	  {0}, {0},
	  {0}, {0}
	}
};

void
ia_css_pdaf_dmem_encode(
	struct sh_css_isp_pdaf_dmem_params *to,
	const struct ia_css_pdaf_config *from,
	unsigned size)
{
	(void)size;
	to->frm_length = from->frm_length;
	to->frm_width  = from->frm_width;
	to->ext_cfg_data_dmem.num_x_patterns = from->ext_cfg_data.num_x_patterns;
	to->ext_cfg_data_dmem.num_y_patterns = from->ext_cfg_data.num_y_patterns;
}

void
ia_css_pdaf_vmem_encode(
	struct sh_css_isp_pdaf_vmem_params *to,
	const struct ia_css_pdaf_config *from,
	unsigned size)
{

	unsigned int i;
	(void)size;

	for ( i=0 ; i < ISP_NWAY; i++)
	{
		to->ext_cfg_data_vmem.y_step_size[i] = from->ext_cfg_data.y_step_size[i];
		to->ext_cfg_data_vmem.y_offset[i] = from->ext_cfg_data.y_offset[i];
	}

  for ( i=0 ; i < ISP_NWAY; i++)
	{
		to->ext_cfg_data_vmem.x_step_size[i] = from->ext_cfg_data.x_step_size[i];
		to->ext_cfg_data_vmem.x_offset[i] = from->ext_cfg_data.x_offset[i];
	}
}
