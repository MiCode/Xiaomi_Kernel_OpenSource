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

#include "ia_css_formats.host.h"
#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "ia_css_debug.h"
//#include "sh_css_frac.h"



const struct ia_css_formats_config default_formats_config = {
	1
};

void
ia_css_formats_encode(
	struct sh_css_isp_formats_params *to,
	const struct ia_css_formats_config *from,
	unsigned size)
{
	(void)size;
	to->video_full_range_flag = from->video_full_range_flag;
}

void
ia_css_formats_dump(
	const struct sh_css_isp_formats_params *formats,
	unsigned level)
{
	if (!formats) return;
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"video_full_range_flag", formats->video_full_range_flag);
}

void
ia_css_formats_debug_dtrace(
	const struct ia_css_formats_config *config,
	unsigned level)
{
	ia_css_debug_dtrace(level,
		"config.video_full_range_flag=%d\n",
		config->video_full_range_flag);
}
