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

#include <assert_support.h>
#include <ia_css_frame_public.h>
#include <ia_css_frame.h>
#include <ia_css_binary.h>
#include <ia_css_types.h>
#include <sh_css_defs.h>
#include <ia_css_debug.h>

#define IA_CSS_INCLUDE_CONFIGURATIONS
#include "ia_css_isp_configs.h"
#include "isp.h"

#include "ia_css_fpn.host.h"

void
ia_css_fpn_encode(
	struct sh_css_isp_fpn_params *to,
	const struct ia_css_fpn_table *from,
	unsigned size)
{
	(void)size;
	to->shift = from->shift;
	to->enabled = from->data != NULL;
}

void
ia_css_fpn_dump(
	const struct sh_css_isp_fpn_params *fpn,
	unsigned level)
{
	if (!fpn) return;
	ia_css_debug_dtrace(level, "Fixed Pattern Noise Reduction:\n");
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"fpn_shift", fpn->shift);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"fpn_enabled", fpn->enabled);
}

void
ia_css_fpn_config(
	struct sh_css_isp_fpn_isp_config *to,
	const struct ia_css_fpn_configuration *from,
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
ia_css_fpn_configure(
	const struct ia_css_binary     *binary,
	const struct ia_css_frame_info *info)
{
	const struct ia_css_frame_info my_info =
		{ { CEIL_DIV(info->res.width, 2), /* Packed by 2x */
		    info->res.height
		  },
		  CEIL_DIV(info->padded_width, 2), /* Packed by 2x */
		  info->format,
		  FPN_BITS_PER_PIXEL,
		  info->raw_bayer_order,
		  { info->crop_info.start_column,
		    info->crop_info.start_line
		  }
		};
	const struct ia_css_fpn_configuration config =
		{ &my_info };
	ia_css_configure_fpn(binary, &config);
}

