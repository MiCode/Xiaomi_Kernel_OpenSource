/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
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

#ifndef _SH_CSS_PARAMS_SHADING_H_
#define _SH_CSS_PARAMS_SHADING_H_

#include "sh_css.h"
#include "sh_css_internal.h"
#include "sh_css_binary.h"

void
sh_css_param_shading_table_init(void);

void
sh_css_param_shading_table_changed_set(bool changed);

bool
sh_css_param_shading_table_changed_get(void);

unsigned int
sh_css_param_shading_table_fraction_bits_get(void);

bool
sh_css_param_shading_table_store(
	hrt_vaddress isp_sc_tbl,
	unsigned int sensor_binning,
	bool raw_binning,
	const struct sh_css_binary *binary);

struct sh_css_shading_table *
sh_css_param_shading_table_get(
	unsigned int sensor_binning,
	bool raw_binning);

bool
sh_css_param_shading_table_set(
	const struct sh_css_shading_table *table);


#endif
