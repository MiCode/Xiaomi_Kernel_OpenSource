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

#ifndef __IA_CSS_FORMATS_TYPES_H
#define __IA_CSS_FORMATS_TYPES_H

/** @file
* CSS-API header file for output format parameters.
*/

#include "type_support.h"

/** Formats configuration.
 *
 *  ISP block: FORMATS
 *  ISP1: FORMATS is used.
 *  ISP2: FORMATS is used.
 */
struct ia_css_formats_config {
	uint32_t video_full_range_flag; /**< selects the range of YUV output.
				u8.0, [0,1],
				default 1, ineffective n/a */
};

#endif /* __IA_CSS_FORMATS_TYPES_H */
