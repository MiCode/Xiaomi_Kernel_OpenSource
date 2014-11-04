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

#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "assert_support.h"

#include "ia_css_ctcv2.host.h"
#include <stdio.h>

/*Default configuration of parameters for CTCv2*/
const struct ia_css_ctcv2_config default_ctcv2_config = {
	INEFFECTIVE_VAL, INEFFECTIVE_VAL, INEFFECTIVE_VAL,
	INEFFECTIVE_VAL, INEFFECTIVE_VAL, INEFFECTIVE_VAL,
	BASIC_VAL * 2, BASIC_VAL * 4, BASIC_VAL * 6,
	BASIC_VAL * 8, INEFFECTIVE_VAL, INEFFECTIVE_VAL,
	BASIC_VAL >> 1, BASIC_VAL};


/*Calculation of the Slope of a Line & the Right Shift value for the Slope*/
static void ctcv2_gradient(int *dydx, int *shift,
			   int y1, int y0, int x1, int x0)
{
	int frc_bits = max(IA_CSS_CTC_COEF_SHIFT, 16);
	int dy = y1 - y0;
	int dx = x1 - x0;
	int dydx_int;
	int dydx_frc;
	/* max_dydx = the maxinum gradient = the maximum y (gain) */
	int max_dydx = (1 << IA_CSS_CTC_COEF_SHIFT) - 1;
	int sft;
	int tmp_dydx;

	/*Protection for paramater values, & avoiding zero divisions*/
	assert(y0 >= 0 && y0 <= max_dydx);
	assert(y1 >= 0 && y1 <= max_dydx);
	assert(x0 < x1);
	assert(dydx != NULL);
	assert(shift != NULL);
	assert(dx > 0);

	dydx_int = dy / dx;
	dydx_frc = ((dy - dydx_int * dx) << frc_bits) / dx;

	/* search "sft" which meets this condition:
	   (1 << (IA_CSS_CTC_COEF_SHIFT - 1))
	   <= (((float)dy / (float)dx) * (1 << sft))
	   <= ((1 << IA_CSS_CTC_COEF_SHIFT) - 1)
	*/
	for (sft = 0; sft <= IA_CSS_CTC_COEF_SHIFT; sft++) {
		tmp_dydx = (dydx_int << sft) + (dydx_frc >> (frc_bits - sft));
		if (tmp_dydx <= max_dydx) {
			*dydx = tmp_dydx;
			*shift = sft;
		}
		if (tmp_dydx >= max_dydx)
			break;
	}
}


/*Encode Function to translate parameters from userspace into ISP space*/
void ia_css_ctcv2_encode(struct sh_css_isp_ctcv2_params *to,
			 struct ia_css_ctcv2_config *from)
{
	to->y_y0 = from->y_y0;
	to->y_y1 = from->y_y1;
	to->y_y2 = from->y_y2;
	to->y_y3 = from->y_y3;
	to->y_y4 = from->y_y4;
	to->y_y5 = from->y_y5;

	to->y_x1 = from->y_x1;
	to->y_x2 = from->y_x2;
	to->y_x3 = from->y_x3;
	to->y_x4 = from->y_x4;

	to->uv_y0 = from->uv_y0;
	to->uv_y1 = from->uv_y1;
	to->uv_x0 = from->uv_x0;
	to->uv_x1 = from->uv_x1;

	/*Gradient Calculations*/
	ctcv2_gradient(&(to->dydx0),
		       &(to->dydx0_shift),
		       from->y_y1, from->y_y0,
		       from->y_x1, 0);

	ctcv2_gradient(&(to->dydx1),
		       &(to->dydx1_shift),
		       from->y_y2, from->y_y1,
		       from->y_x2, from->y_x1);

	ctcv2_gradient(&to->dydx2,
		       &to->dydx2_shift,
		       from->y_y3, from->y_y2,
		       from->y_x3, from->y_x2);

	ctcv2_gradient(&to->dydx3,
		       &to->dydx3_shift,
		       from->y_y4, from->y_y3,
		       from->y_x4, from->y_x3);

	ctcv2_gradient(&(to->dydx4),
		       &(to->dydx4_shift),
		       from->y_y5, from->y_y4,
		       SH_CSS_BAYER_MAXVAL, from->y_x4);

	ctcv2_gradient(&(to->uv_dydx),
		       &(to->uv_dydx_shift),
		       from->uv_y1, from->uv_y0,
		       from->uv_x1, from->uv_x0);
}
