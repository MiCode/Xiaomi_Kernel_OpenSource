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

#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "ia_css_debug.h"

#include "ia_css_anr2.host.h"

void
ia_css_anr2_vmem_encode(
	struct ia_css_isp_anr2_params *to,
	const struct ia_css_anr_thres *from,
	size_t size)
{
	unsigned i;

	(void)size;
	for (i = 0; i < ANR_PARAM_SIZE; i++) {
		unsigned j;
		for (j = 0; j < ISP_VEC_NELEMS; j++) {
			to->data[i][j] = from->data[i*ISP_VEC_NELEMS+j];
		}
	}
}

void
ia_css_anr2_debug_dtrace(
	const struct ia_css_anr_thres *config,
	unsigned level)
{
	(void)config;
	(void)level;
}

