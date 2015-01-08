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

#ifndef __IA_CSS_RAW_HOST_H
#define __IA_CSS_RAW_HOST_H

#include "ia_css_binary.h"

#include "ia_css_raw_types.h"
#include "ia_css_raw_param.h"

void
ia_css_raw_config(
	struct sh_css_isp_raw_isp_config      *to,
	const struct ia_css_raw_configuration *from,
	unsigned size);

void
ia_css_raw_configure(
	const struct sh_css_sp_pipeline *pipe,
	const struct ia_css_binary     *binary,
	const struct ia_css_frame_info *in_info,
	const struct ia_css_frame_info *internal_info,
	bool two_ppc,
	bool deinterleaved);

#endif /* __IA_CSS_RAW_HOST_H */
