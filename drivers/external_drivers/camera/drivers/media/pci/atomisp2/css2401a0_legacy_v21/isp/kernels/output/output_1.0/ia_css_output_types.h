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

#ifndef __IA_CSS_OUTPUT_TYPES_H
#define __IA_CSS_OUTPUT_TYPES_H

/** @file
* CSS-API header file for parameters of output frames.
*/

/** Output frame
 *
 *  ISP block: output frame
 */

//#include "ia_css_frame_public.h"
struct ia_css_frame_info;

struct ia_css_output_configuration {
	const struct ia_css_frame_info *info;
};

struct ia_css_output0_configuration {
	const struct ia_css_frame_info *info;
};

struct ia_css_output1_configuration {
	const struct ia_css_frame_info *info;
};

struct ia_css_output_config {
	uint8_t enable_hflip;  /**< enable horizontal output mirroring */
	uint8_t enable_vflip;  /**< enable vertical output mirroring */
};

#endif /* __IA_CSS_OUTPUT_TYPES_H */

