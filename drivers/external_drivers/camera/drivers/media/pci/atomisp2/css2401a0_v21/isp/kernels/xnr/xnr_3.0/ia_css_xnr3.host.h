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

#ifndef __IA_CSS_XNR3_HOST_H
#define __IA_CSS_XNR3_HOST_H

#include "ia_css_xnr3_param.h"
#include "ia_css_xnr3_types.h"

extern const struct ia_css_xnr3_config default_xnr3_config;

void
ia_css_xnr3_encode(
	struct sh_css_isp_xnr3_params *to,
	const struct ia_css_xnr3_config *from,
	unsigned size);

void
ia_css_xnr3_debug_dtrace(
	const struct ia_css_xnr3_config *config,
	unsigned level);

#endif /* __IA_CSS_XNR3_HOST_H */
