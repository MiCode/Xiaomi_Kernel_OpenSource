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

#ifndef __IA_CSS_CTCV2_TYPES_H
#define __IA_CSS_CTCV2_TYPES_H

/** Chroma Tone Control configuration.
*
*  ISP block: CTC2 (CTC by polygonal approximation)
* (ISP1: CTC1 (CTC by look-up table) is used.)
*  ISP2: CTC2 is used.
*  ISP261: CTCV2 (CTC by Fast Approximate Distance)
*/
struct ia_css_ctcv2_config {

	/**< Gains by Y(Luma) at Y =0.0,Y_X1, Y_X2, Y_X3, Y_X4 and Y_X5
	*   --default/ineffective value: 4096(0.5f)
	*/
	uint16_t y_y0;
	uint16_t y_y1;
	uint16_t y_y2;
	uint16_t y_y3;
	uint16_t y_y4;
	uint16_t y_y5;
	/** 1st-4th  kneepoints by Y(Luma) --default/ineffective value:n/a
	*   requirement: 0.0 < y_x1 < y_x2 <y _x3 < y_x4 < 1.0
	*/
	uint16_t y_x1;
	uint16_t y_x2;
	uint16_t y_x3;
	uint16_t y_x4;
	/** Gains by UV(Chroma) under threholds uv_x0 and uv_x1
	*   --default/ineffective value: 4096(0.5f)
	*/
	uint16_t uv_y0;
	uint16_t uv_y1;
	/** Minimum and Maximum Thresholds by UV(Chroma)- uv_x0 and uv_x1
	*   --default/ineffective value: n/a
	*/
	uint16_t uv_x0;
	uint16_t uv_x1;
	};

#endif /* __IA_CSS_CTCV2_TYPES_H */
