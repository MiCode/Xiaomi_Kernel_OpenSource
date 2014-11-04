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
/* ToDo: check how to dump using the debug trace functions */
//#include "ia_css_debug.h"
#include "type_support.h"

#include "ia_css_bnr22.host.h"

#ifndef __HIVECC
#include <stdio.h>
#endif

/* Default kernel parameters. */
const struct ia_css_bnr22_config default_bnr22_config = {
	200,
	200,
	200,
	200,
	200,
	200,
	200,
	200,
	200,
	200,
	200,
	200,
	0,
	4096,
	8191,
	128,
	0,
	0,
	0,
	8191,
	0,
	8191
};

void
ia_css_bnr22_encode(
	struct sh_css_isp_bnr22_params *to,
	const struct ia_css_bnr22_config *from)
{
	/* BNR (Bayer Noise Reduction) */

	to->d_var_gain_r = from->d_var_gain_r1 - from->d_var_gain_r0;
	to->d_var_bias_r = from->d_var_gain_r0;
	to->d_var_gain_g = from->d_var_gain_g1 - from->d_var_gain_g0;
	to->d_var_bias_g = from->d_var_gain_g0;
	to->d_var_gain_b = from->d_var_gain_b1 - from->d_var_gain_b0;
	to->d_var_bias_b = from->d_var_gain_b0;
	to->n_var_gain_r = from->n_var_gain_r1 - from->n_var_gain_r0;
	to->n_var_bias_r = from->n_var_gain_r0;
	to->n_var_gain_g = from->n_var_gain_g1 - from->n_var_gain_g0;
	to->n_var_bias_g = from->n_var_gain_g0;
	to->n_var_gain_b = from->n_var_gain_b1 - from->n_var_gain_b0;
	to->n_var_bias_b = from->n_var_gain_b0;
	to->d_thres = from->d_thres;
	to->d_thres_w = from->d_thres_w;
	to->d_var_th_min = from->d_var_th_min;
	to->d_var_th_max = from->d_var_th_max;
	to->n_var_th_min = from->n_var_th_min;
	to->n_var_th_max = from->n_var_th_max;
	to->coeff_var_th_gain = from->var_offset_coef;
	to->d_gain = from->d_gain;
	to->detail_gain = 1;
	to->detail_gain_asr = 7;
	to->detail_level_bias = 0;
}

void
ia_css_bnr22_debug_trace(
	const struct ia_css_bnr22_config *bnr,
	unsigned level)
{
	if (!bnr)
		return;

#if 1
	/* isp_test_bench can not access ia_css_debug_dtrace because of css
	 * dependencies included in ia_css_debug.h.
	 * This debug is enabled till ia_css_debug_dtrace() function access is
	 * restored for isp_test_bench. */
	(void)level;

	printf("Bayer Noise Reduction 2.2:\n");
	printf("\t%-32s = %d\n", "d_var_gain_r0", bnr->d_var_gain_r0);
	printf("\t%-32s = %d\n", "d_var_gain_g0", bnr->d_var_gain_g0);
	printf("\t%-32s = %d\n", "d_var_gain_b0", bnr->d_var_gain_b0);
	printf("\t%-32s = %d\n", "d_var_gain_r1", bnr->d_var_gain_r1);
	printf("\t%-32s = %d\n", "d_var_gain_g1", bnr->d_var_gain_g1);
	printf("\t%-32s = %d\n", "d_var_gain_b1", bnr->d_var_gain_b1);

	printf("\t%-32s = %d\n", "n_var_gain_r0", bnr->n_var_gain_r0);
	printf("\t%-32s = %d\n", "n_var_gain_g0", bnr->n_var_gain_g0);
	printf("\t%-32s = %d\n", "n_var_gain_b0", bnr->n_var_gain_b0);
	printf("\t%-32s = %d\n", "n_var_gain_r1", bnr->n_var_gain_r1);
	printf("\t%-32s = %d\n", "n_var_gain_g1", bnr->n_var_gain_g1);
	printf("\t%-32s = %d\n", "n_var_gain_b1", bnr->n_var_gain_b1);

	printf("\t%-32s = %d\n", "d_thres", bnr->d_thres);
	printf("\t%-32s = %d\n", "d_thres_w", bnr->d_thres_w);
	printf("\t%-32s = %d\n", "var_offset_coef", bnr->var_offset_coef);
	printf("\t%-32s = %d\n", "d_gain", bnr->d_gain);
	printf("\t%-32s = %d\n", "detail_threshold", bnr->detail_threshold);
	printf("\t%-32s = %d\n", "detail_level_offset", bnr->detail_level_offset);

	printf("\t%-32s = %d\n", "d_var_th_min", bnr->d_var_th_min);
	printf("\t%-32s = %d\n", "d_var_th_max", bnr->d_var_th_max);
	printf("\t%-32s = %d\n", "n_var_th_min", bnr->n_var_th_min);
	printf("\t%-32s = %d\n", "n_var_th_max", bnr->n_var_th_max);
#else
	ia_css_debug_dtrace(level, "Bayer Noise Reduction 2.2:\n");
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_gain_r0", bnr->d_var_gain_r0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_gain_g0", bnr->d_var_gain_g0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_gain_b0", bnr->d_var_gain_b0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_gain_r1", bnr->d_var_gain_r1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_gain_g1", bnr->d_var_gain_g1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_gain_b1", bnr->d_var_gain_b1);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_gain_r0", bnr->n_var_gain_r0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_gain_g0", bnr->n_var_gain_g0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_gain_b0", bnr->n_var_gain_b0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_gain_r1", bnr->n_var_gain_r1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_gain_g1", bnr->n_var_gain_g1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_gain_b1", bnr->n_var_gain_b1);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_thres", bnr->d_thres);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_thresW", bnr->d_thres_w);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "varOffsetCoef", bnr->var_offset_coef);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_gain", bnr->d_gain);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "detail_threshold", bnr->detail_threshold);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "detail_level_offset", bnr->detail_level_offset);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_th_min", bnr->d_var_th_min);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "d_var_th_max", bnr->d_var_th_max);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_th_min", bnr->n_var_th_min);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "n_var_th_max", bnr->n_var_th_max);
#endif
}
