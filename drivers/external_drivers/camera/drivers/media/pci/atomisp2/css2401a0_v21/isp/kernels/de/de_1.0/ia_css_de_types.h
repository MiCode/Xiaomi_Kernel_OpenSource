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

#ifndef __IA_CSS_DE_TYPES_H
#define __IA_CSS_DE_TYPES_H

/** @file
* CSS-API header file for Demosaic (bayer-to-YCgCo) parameters.
*/

/** Demosaic (bayer-to-YCgCo) configuration.
 *
 *  ISP block: DE1
 *  ISP1: DE1 is used.
 * (ISP2: DE2 is used.)
 */
struct ia_css_de_config {
	ia_css_u0_16 pixelnoise; /**< Pixel noise used in moire elimination.
				u0.16, [0,65535],
				default 0, ineffective 0 */
	ia_css_u0_16 c1_coring_threshold; /**< Coring threshold for C1.
				This is the same as nr_config.threshold_cb.
				u0.16, [0,65535],
				default 128(0.001953125), ineffective 0 */
	ia_css_u0_16 c2_coring_threshold; /**< Coring threshold for C2.
				This is the same as nr_config.threshold_cr.
				u0.16, [0,65535],
				default 128(0.001953125), ineffective 0 */
};

#endif /* __IA_CSS_DE_TYPES_H */

