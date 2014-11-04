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

#ifndef __IA_CSS_FPN_TYPES_H
#define __IA_CSS_FPN_TYPES_H

/** @file
* CSS-API header file for Fixed Pattern Noise parameters.
*/

/** Fixed Pattern Noise table.
 *
 *  This contains the fixed patterns noise values
 *  obtained from a black frame capture.
 *
 *  "shift" should be set as the smallest value
 *  which satisfies the requirement the maximum data is less than 64.
 *
 *  ISP block: FPN1
 *  ISP1: FPN1 is used.
 *  ISP2: FPN1 is used.
 */

struct ia_css_fpn_table {
	int16_t *data;		/**< Table content (fixed patterns noise).
					u0.[13-shift], [0,63] */
	uint32_t width;		/**< Table width (in pixels).
					This is the input frame width. */
	uint32_t height;	/**< Table height (in pixels).
					This is the input frame height. */
	uint32_t shift;		/**< Common exponent of table content.
					u8.0, [0,13] */
	uint32_t enabled;	/**< Fpn is enabled.
					bool */
};

struct ia_css_fpn_configuration {
	const struct ia_css_frame_info *info;
};

#endif /* __IA_CSS_FPN_TYPES_H */
