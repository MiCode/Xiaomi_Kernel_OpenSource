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

#ifndef __IA_CSS_BH_TYPES_H
#define __IA_CSS_BH_TYPES_H

/** Number of elements in the BH table.
  * Should be consistent with hmem.h
  */
#define IA_CSS_HMEM_BH_TABLE_SIZE	ISP_HIST_DEPTH
#define IA_CSS_HMEM_BH_UNIT_SIZE	(ISP_HIST_DEPTH/ISP_HIST_COMPONENTS)

#define BH_COLOR_R	(0)
#define BH_COLOR_G	(1)
#define BH_COLOR_B	(2)
#define BH_COLOR_Y	(3)
#define BH_COLOR_NUM	(4)

/** BH table */
struct ia_css_bh_table {
	uint32_t hmem[ISP_HIST_COMPONENTS][IA_CSS_HMEM_BH_UNIT_SIZE];
};

#endif /* __IA_CSS_BH_TYPES_H */


