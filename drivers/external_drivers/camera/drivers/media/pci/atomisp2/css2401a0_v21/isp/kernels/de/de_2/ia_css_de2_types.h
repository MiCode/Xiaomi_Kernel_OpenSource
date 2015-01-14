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

#ifndef __IA_CSS_DE2_TYPES_H
#define __IA_CSS_DE2_TYPES_H

/** @file
* CSS-API header file for Demosaicing parameters.
*/

/** Eigen Color Demosaicing configuration.
 *
 *  ISP block: DE2
 * (ISP1: DE1 is used.)
 *  ISP2: DE2 is used.
 */
struct ia_css_ecd_config {
	uint16_t zip_strength;	/**< Strength of zipper reduction.
				u0.13, [0,8191],
				default 5489(0.67), ineffective 0 */
	uint16_t fc_strength;	/**< Strength of false color reduction.
				u0.13, [0,8191],
				default 8191(almost 1.0), ineffective 0 */
	uint16_t fc_debias;	/**< Prevent color change
				     on noise or Gr/Gb imbalance.
				u0.13, [0,8191],
				default 0, ineffective 0 */
};

#endif /* __IA_CSS_DE2_TYPES_H */

