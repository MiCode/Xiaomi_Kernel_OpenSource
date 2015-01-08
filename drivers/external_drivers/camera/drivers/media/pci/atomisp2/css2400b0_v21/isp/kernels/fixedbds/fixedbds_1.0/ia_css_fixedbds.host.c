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

#include "ia_css_fixedbds.host.h"

void
ia_css_bds_encode(
	struct sh_css_isp_bds_params *to,
	const struct ia_css_aa_config *from,
	unsigned size)
{
	(void)size;
	to->baf_strength = from->strength;
}

void
ia_css_bds_dump(
	const struct sh_css_isp_bds_params *bds,
	unsigned level)
{
	(void)bds;
	(void)level;
}

void
ia_css_bds_debug_dtrace(
	const struct ia_css_aa_config *config,
	unsigned level)
{
  (void)config;
  (void)level;
}
