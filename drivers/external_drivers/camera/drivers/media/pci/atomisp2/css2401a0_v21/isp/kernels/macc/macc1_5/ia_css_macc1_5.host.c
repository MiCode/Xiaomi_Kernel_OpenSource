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

#ifndef IA_CSS_NO_DEBUG
/* FIXME: See BZ 4427 */
#include "ia_css_debug.h"
#endif

#include "ia_css_macc1_5.host.h"

const struct ia_css_macc1_5_config default_macc1_5_config = {
	1
};

void
ia_css_macc1_5_encode(
	struct sh_css_isp_macc1_5_params *to,
	const struct ia_css_macc1_5_config *from,
	unsigned size)
{
	(void)size;
	to->exp = from->exp;
}

void
ia_css_macc1_5_vmem_encode(
	struct sh_css_isp_macc1_5_vmem_params *params,
	const struct ia_css_macc1_5_table *from,
	unsigned size)
{
	unsigned int i, j, k, idx;
	unsigned int idx_map[] = {
		0, 1, 3, 2, 6, 7, 5, 4, 12, 13, 15, 14, 10, 11, 9, 8};

	(void)size;

	for (k = 0; k < 4; k++)
		for (i = 0; i < IA_CSS_MACC_NUM_AXES; i++) {
			idx = (idx_map[i] + (k*IA_CSS_MACC_NUM_AXES));
			j   = 4*i;

			params->data[0][(idx)] = from->data[j];
			params->data[1][(idx)] = from->data[j+1];
			params->data[2][(idx)] = from->data[j+2];
			params->data[3][(idx)] = from->data[j+3];
		}

}

#ifndef IA_CSS_NO_DEBUG
void
ia_css_macc1_5_debug_dtrace(
	const struct ia_css_macc1_5_config *config,
	unsigned level)
{
	ia_css_debug_dtrace(level,
		"config.exp=%d\n",
		config->exp);
}
#endif
