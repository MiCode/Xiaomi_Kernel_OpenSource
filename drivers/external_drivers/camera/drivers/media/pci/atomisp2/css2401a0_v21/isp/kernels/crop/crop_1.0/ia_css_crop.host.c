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

#include <assert_support.h>
#include <ia_css_frame_public.h>
#include <ia_css_frame.h>
#include <ia_css_binary.h>
#define IA_CSS_INCLUDE_CONFIGURATIONS
#include "ia_css_isp_configs.h"
#include "isp.h"
#include "ia_css_crop.host.h"

void
ia_css_crop_encode(
	struct sh_css_isp_crop_isp_params *to,
	const struct ia_css_crop_config *from,
	unsigned size)
{
	(void)size;
	to->crop_pos = from->crop_pos;
}

void
ia_css_crop_config(
	struct sh_css_isp_crop_isp_config *to,
	const struct ia_css_crop_configuration  *from,
	unsigned size)
{
	unsigned elems_a = ISP_VEC_NELEMS;

	(void)size;
	ia_css_dma_configure_from_info(&to->port_b, from->info);
	to->width_a_over_b = elems_a / to->port_b.elems;

	/* Assume divisiblity here, may need to generalize to fixed point. */
	assert (elems_a % to->port_b.elems == 0);
}

void
ia_css_crop_configure(
	const struct ia_css_binary     *binary,
	const struct ia_css_frame_info *info)
{
	const struct ia_css_crop_configuration config =
		{ info };
	ia_css_configure_crop(binary, &config);
}
