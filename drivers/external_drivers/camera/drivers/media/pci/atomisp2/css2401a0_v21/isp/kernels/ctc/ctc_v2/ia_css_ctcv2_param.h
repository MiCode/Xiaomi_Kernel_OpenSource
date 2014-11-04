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

#ifndef __IA_CSS_CTCV2_PARAM_H
#define __IA_CSS_CTCV2_PARAM_H

#define IA_CSS_CTC_COEF_SHIFT          13

/* CTC (Chroma Tone Control)ISP Parameters */
struct sh_css_isp_ctcv2_params {

	/**< Gains by Y(Luma) at Y =0.0,Y_X1, Y_X2, Y_X3, Y_X4 and Y_X5
	*   --default/ineffective value: 4096(0.5f)
	*/
	int32_t y_y0;
	int32_t y_y1;
	int32_t y_y2;
	int32_t y_y3;
	int32_t y_y4;
	int32_t y_y5;
	/** 1st-4th  kneepoints by Y(Luma)  --default/ineffective value:n/a
	*   requirement: 0.0 < y_x1 < y_x2 <y _x3 < y_x4 < 1.0
	*/
	int32_t y_x1;
	int32_t y_x2;
	int32_t y_x3;
	int32_t y_x4;
	/** Gains by UV(Chroma) under kneepoints uv_x0 and uv_x1
	*   --default/ineffective value: 4096(0.5f)
	*/
	int32_t uv_y0;
	int32_t uv_y1;
	/** Kneepoints by UV(Chroma)- uv_x0 and uv_x1
	*   --default/ineffective value: n/a
	*/
	int32_t uv_x0;
	int32_t uv_x1;
	/** Slopes of lines interconnecting
	 *  0.0 -> y_x1 -> y_x2 -> y _x3 -> y_x4 -> 1.0
	*/
	int32_t dydx0;
	int32_t dydx1;
	int32_t dydx2;
	int32_t dydx3;
	int32_t dydx4;
	/** Right Shift values for Slopes of lines interconnecting
	*   0.0 -> y_x1 -> y_x2 -> y _x3 -> y_x4 -> 1.0
	*/
	int32_t dydx0_shift;
	int32_t dydx1_shift;
	int32_t dydx2_shift;
	int32_t dydx3_shift;
	int32_t dydx4_shift;
	/** Slope of line interconnecting uv_x0 -> uv_x1
	*/
	int32_t uv_dydx;
	/** Right Shift value for Slope of line interconnecting
	*   uv_x0 -> uv_x1
	*/
	int32_t uv_dydx_shift;

};
#endif /* __IA_CSS_CTCV2_PARAM_H */
