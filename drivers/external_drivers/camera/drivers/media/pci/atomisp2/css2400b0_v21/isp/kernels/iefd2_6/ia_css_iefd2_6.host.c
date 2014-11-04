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

#include "ia_css_debug.h"
#include "ia_css_iefd2_6.host.h"


void
ia_css_iefd2_6_encode(
	struct ia_css_isp_iefd2_6_dmem_params *to,
	const struct ia_css_iefd2_6_config *from,
	size_t size)
{
	int i;

	(void)size;
	to->horver_diag_coeff		= from->horver_diag_coeff;
	to->ed_horver_diag_coeff	= from->ed_horver_diag_coeff;
	to->dir_smooth_enable		= from->dir_smooth_enable;
	to->dir_metric_update		= from->dir_metric_update;
	to->unsharp_c00			= from->unsharp_c00;
	to->unsharp_c01			= from->unsharp_c01;
	to->unsharp_c02			= from->unsharp_c02;
	to->unsharp_c11			= from->unsharp_c11;
	to->unsharp_c12			= from->unsharp_c12;
	to->unsharp_c22			= from->unsharp_c22;
	to->unsharp_weight		= from->unsharp_weight;
	to->unsharp_amount		= from->unsharp_amount;
	to->cu_dir_sharp_pow		= from->cu_dir_sharp_pow;
	to->cu_dir_sharp_pow_bright	= from->cu_dir_sharp_pow_bright;
	to->cu_non_dir_sharp_pow	= from->cu_non_dir_sharp_pow;
	to->cu_non_dir_sharp_pow_bright	= from->cu_non_dir_sharp_pow_bright;
	to->dir_far_sharp_weight	= from->dir_far_sharp_weight;
	to->rad_cu_dir_sharp_x1		= from->rad_cu_dir_sharp_x1;
	to->rad_cu_non_dir_sharp_x1	= from->rad_cu_non_dir_sharp_x1;
	to->rad_dir_far_sharp_weight	= from->rad_dir_far_sharp_weight;
	to->sharp_nega_lmt_txt		= from->sharp_nega_lmt_txt;
	to->sharp_posi_lmt_txt		= from->sharp_posi_lmt_txt;
	to->sharp_nega_lmt_dir		= from->sharp_nega_lmt_dir;
	to->sharp_posi_lmt_dir		= from->sharp_posi_lmt_dir;
	to->clamp_stitch		= from->clamp_stitch;
	to->rad_enable			= from->rad_enable;
	to->rad_x_origin		= from->rad_x_origin;
	to->rad_y_origin		= from->rad_y_origin;
	to->rad_nf			= from->rad_nf;
	to->rad_inv_r2			= from->rad_inv_r2;
	to->vssnlm_enable		= from->vssnlm_enable;
	to->vssnlm_x0			= from->vssnlm_x0;
	to->vssnlm_x1			= from->vssnlm_x1;
	to->vssnlm_x2			= from->vssnlm_x2;
	to->vssnlm_y1			= from->vssnlm_y1;
	to->vssnlm_y2			= from->vssnlm_y2;
	to->vssnlm_y3			= from->vssnlm_y3;

	/* Setup for configurable units */
	/* First copy the hardcoded items */

	to->curad_x[0]			= 0;
	to->curad_a[0]			= 0;
	to->curad_b[0]			= from->cu_radial_slope_b[1];
	to->e_cued_x[0]			= 0;
	to->e_cued_a[0]			= 0;
	to->e_cued_b[0]			= from->cu_ed_slopes_b[1];
	to->e_cu_dir_x[0]		= 0;
	to->e_cu_dir_a[0]		= 0;
	to->e_cu_dir_b[0]		= from->cu_dir_sharp_slopes_b[1];
	to->e_cu_non_dir_x[0]		= 0;
	to->e_cu_non_dir_a[0]		= 0;
	to->e_cu_non_dir_b[0]		= from->cu_non_dir_sharp_slopes_b[1];
	to->e_cued2_a			= from->cu_ed2_slopes_a;
	to->e_cu_vssnlm_a		= from->cu_vssnlm_slopes_a;

	/* copying remaining array part */

	for (i = 1; i < 3; i++)
	{
		to->e_cu_dir_a[i]		= from->cu_dir_sharp_slopes_a[i];
		to->e_cu_dir_b[i]		= from->cu_dir_sharp_slopes_b[i];
		to->e_cu_non_dir_a[i]		= from->cu_non_dir_sharp_slopes_a[i];
		to->e_cu_non_dir_b[i]		= from->cu_non_dir_sharp_slopes_b[i];
	}

	for (i = 1; i < 4; i++)
	{
		to->e_cu_dir_x[i]		= from->cu_dir_sharp_points_x[i];
		to->e_cu_non_dir_x[i]		= from->cu_non_dir_sharp_points_x[i];
	}

	for (i = 1; i < 5; i++)
	{
		to->curad_a[i]			= from->cu_radial_slope_a[i];
		to->curad_b[i]			= from->cu_radial_slope_b[i];
		to->e_cued_a[i]			= from->cu_ed_slopes_a[i];
		to->e_cued_b[i]			= from->cu_ed_slopes_b[i];
	}

	for (i = 1; i < 6; i++)
	{
		to->curad_x[i]			= from->cu_radial_points_x[i];
		to->e_cued_x[i]			= from->cu_ed_points_x[i];
	}

	to->e_cued2_b		= ((0-from->cu_ed2_points_x[1]) * from->cu_ed2_slopes_a)>>4;
	to->e_cu_vssnlm_b	= ((0-from->cu_vssnlm_points_x[1]) * from->cu_vssnlm_slopes_a)>>4;
}

void
ia_css_iefd2_6_debug_dtrace(
	const struct ia_css_iefd2_6_config *config,
	unsigned level)
{
	(void)config;
	(void)level;
}
