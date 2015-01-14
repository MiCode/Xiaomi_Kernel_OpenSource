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

#ifndef __IA_CSS_TDF_HOST_H
#define __IA_CSS_TDF_HOST_H

#include "ia_css_tdf_types.h"
#include "ia_css_tdf_param.h"
#include "ia_css_tdf_default.host.h"

void
ia_css_tdf_vmem_encode(
	struct ia_css_isp_tdf_vmem_params *to,
	const struct ia_css_tdf_config *from,
	size_t size);

void
ia_css_tdf_encode(
	struct ia_css_isp_tdf_dmem_params *to,
	const struct ia_css_tdf_config *from,
	size_t size);

void
ia_css_tdf_debug_dtrace(
	const struct ia_css_tdf_config *config, unsigned level)
;

#endif /* __IA_CSS_TDF_HOST_H */
