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

#ifndef __IA_CSS_CSC_TYPES_H
#define __IA_CSS_CSC_TYPES_H

/** @file
* CSS-API header file for Color Space Conversion parameters.
*/

/** Color Correction configuration.
 *
 *  This structure is used for 3 cases.
 *  ("YCgCo" is the output format of Demosaic.)
 *
 *  1. Color Space Conversion (YCgCo to YUV) for ISP1.
 *     ISP block: CSC1 (Color Space Conversion)
 *     struct ia_css_cc_config   *cc_config
 *
 *  2. Color Correction Matrix (YCgCo to RGB) for ISP2.
 *     ISP block: CCM2 (Color Correction Matrix)
 *     struct ia_css_cc_config   *yuv2rgb_cc_config
 *
 *  3. Color Space Conversion (RGB to YUV) for ISP2.
 *     ISP block: CSC2 (Color Space Conversion)
 *     struct ia_css_cc_config   *rgb2yuv_cc_config
 *
 *  default/ineffective:
 *  1. YCgCo -> YUV
 *  	1	0.174		0.185
 *  	0	-0.66252	-0.66874
 *  	0	-0.83738	0.58131
 *
 *	fraction_bits = 12
 *  	4096	713	758
 *  	0	-2714	-2739
 *  	0	-3430	2381
 *
 *  2. YCgCo -> RGB
 *  	1	-1	1
 *  	1	1	0
 *  	1	-1	-1
 *
 *	fraction_bits = 12
 *  	4096	-4096	4096
 *  	4096	4096	0
 *  	4096	-4096	-4096
 *
 *  3. RGB -> YUV
 *	0.299	   0.587	0.114
 * 	-0.16874   -0.33126	0.5
 * 	0.5	   -0.41869	-0.08131
 *
 *	fraction_bits = 13
 *  	2449	4809	934
 *  	-1382	-2714	4096
 *  	4096	-3430	-666
 */
struct ia_css_cc_config {
	uint32_t fraction_bits;/**< Fractional bits of matrix.
					u8.0, [0,13] */
	int32_t matrix[3 * 3]; /**< Conversion matrix.
					s[13-fraction_bits].[fraction_bits],
					[-8192,8191] */
};

#endif /* __IA_CSS_CSC_TYPES_H */
