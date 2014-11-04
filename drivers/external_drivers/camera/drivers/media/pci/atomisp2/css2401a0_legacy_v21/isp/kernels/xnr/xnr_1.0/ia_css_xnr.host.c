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


#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "ia_css_debug.h"
#include "sh_css_frac.h"

#include "ia_css_xnr.host.h"

const struct ia_css_xnr_config default_xnr_config = {
	/** default threshold 6400 translates to 25 on ISP. */
	6400
};

void
ia_css_xnr_table_vamem_encode(
	struct sh_css_isp_xnr_vamem_params *to,
	const struct ia_css_xnr_table *from,
	unsigned size)
{
	(void)size;
	memcpy (&to->xnr,  &from->data, sizeof(to->xnr));
}

void
ia_css_xnr_encode(
	struct sh_css_isp_xnr_params *to,
	const struct ia_css_xnr_config *from,
	unsigned size)
{
	(void)size;
	
	to->threshold =
		uDIGIT_FITTING(from->threshold, 16, SH_CSS_ISP_YUV_BITS);
}

void
ia_css_xnr_table_debug_dtrace(
	const struct ia_css_xnr_table *config,
	unsigned level)
{
	(void)config;
	(void)level;
}

void
ia_css_xnr_debug_dtrace(
	const struct ia_css_xnr_config *config,
	unsigned level)
{
	ia_css_debug_dtrace(level,
		"config.threshold=%d\n", config->threshold);
}
