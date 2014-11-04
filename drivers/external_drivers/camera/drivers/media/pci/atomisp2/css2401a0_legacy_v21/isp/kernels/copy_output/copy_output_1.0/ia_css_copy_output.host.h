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

#ifndef __IA_CSS_COPY_OUTPUT_HOST_H
#define __IA_CSS_COPY_OUTPUT_HOST_H

#include "type_support.h"
#include "ia_css_binary.h"

#include "ia_css_copy_output_param.h"

void
ia_css_copy_output_config(
	struct sh_css_isp_copy_output_isp_config      *to,
	const struct ia_css_copy_output_configuration *from,
	unsigned size);

void
ia_css_copy_output_configure(
	const struct ia_css_binary     *binary,
	bool enable);

#endif /* __IA_CSS_COPY_OUTPUT_HOST_H */
