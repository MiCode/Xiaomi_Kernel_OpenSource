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

#ifndef __IA_CSS_GC2_TYPES_H
#define __IA_CSS_GC2_TYPES_H

/** @file
* CSS-API header file for Gamma Correction parameters.
*/

/** sRGB Gamma table, used for sRGB Gamma Correction.
 *
 *  ISP block: GC2 (sRGB Gamma Correction)
 * (ISP1: GC1(YUV Gamma Correction) is used.)
 *  ISP2: GC2 is used.
 */

/** Number of elements in the sRGB gamma table. */
#define IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE_LOG2 8
#define IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE      (1U<<IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE_LOG2)

/** Number of elements in the sRGB gamma table. */
#define IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE_LOG2    8
#define IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE     ((1U<<IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE_LOG2) + 1)

/**< IA_CSS_VAMEM_TYPE_1(ISP2300) or
     IA_CSS_VAMEM_TYPE_2(ISP2400) */
union ia_css_rgb_gamma_data {
	uint16_t vamem_1[IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE];
	/**< RGB Gamma table on vamem type1. This table is not used,
		because sRGB Gamma Correction is not implemented for ISP2300. */
	uint16_t vamem_2[IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE];
		/**< RGB Gamma table on vamem type2. u0.12, [0,4095] */
};

struct ia_css_rgb_gamma_table {
	enum ia_css_vamem_type vamem_type;
	union ia_css_rgb_gamma_data data;
};

#endif /* __IA_CSS_GC2_TYPES_H */
