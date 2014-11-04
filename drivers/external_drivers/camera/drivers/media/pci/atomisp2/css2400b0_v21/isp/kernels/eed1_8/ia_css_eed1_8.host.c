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


#ifndef IA_CSS_NO_DEBUG
#include "ia_css_debug.h"
#endif

#include "type_support.h"

#include "ia_css_eed1_8.host.h"

void
ia_css_eed1_8_encode(
	struct ia_css_isp_eed1_8_params *to,
	const struct ia_css_eed1_8_config *from)
{
	int i;

	to->rbzp_strength = from->rbzp_strength;

	to->fcstrength = from->fcstrength;
	to->fcthres_0 = from->fcthres_0;
	to->fcthres_1 = from->fcthres_1;
	to->fc_sat_coef = from->fc_sat_coef;
	to->fc_coring_prm = from->fc_coring_prm;

	to->aerel_thres0 = from->aerel_thres0;
	to->aerel_gain0 = from->aerel_gain0;
	to->aerel_thres1 = from->aerel_thres1;
	to->aerel_gain1 = from->aerel_gain1;

	to->derel_thres0 = from->derel_thres0;
	to->derel_gain0 = from->derel_gain0;
	to->derel_thres1 = from->derel_thres1;
	to->derel_gain1 = from->derel_gain1;

	to->coring_pos0 = from->coring_pos0;
	to->coring_pos1 = from->coring_pos1;
	to->coring_neg0 = from->coring_neg0;
	to->coring_neg1 = from->coring_neg1;

	to->gain_exp = from->gain_exp;
	to->gain_pos0 = from->gain_pos0;
	to->gain_pos1 = from->gain_pos1;
	to->gain_neg0 = from->gain_neg0;
	to->gain_neg1 = from->gain_neg1;

	to->pos_margin0 = from->pos_margin0;
	to->pos_margin1 = from->pos_margin1;
	to->neg_margin0 = from->neg_margin0;
	to->neg_margin1 = from->neg_margin1;

	for (i = 0; i < IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS; i++) {
		to->dew_enhance_seg_x[i] = from->dew_enhance_seg_x[i];
		to->dew_enhance_seg_y[i] = from->dew_enhance_seg_y[i];
	}

	to->dedgew_max = from->dedgew_max;
}

#ifndef IA_CSS_NO_DEBUG
void
ia_css_eed1_8_debug_trace(
	const struct ia_css_eed1_8_config *eed,
	unsigned level)
{
	if (!eed)
		return;

	ia_css_debug_dtrace(level, "Edge Enhancing Demosaic 1.8:\n");
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "rbzp_strength", eed->rbzp_strength);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "fcstrength", eed->fcstrength);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "fcthres_0", eed->fcthres_0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "fcthres_1", eed->fcthres_1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "fc_sat_coef", eed->fc_sat_coef);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "fc_coring_prm", eed->fc_coring_prm);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "aerel_thres0", eed->aerel_thres0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "aerel_gain0", eed->aerel_gain0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "aerel_thres1", eed->aerel_thres1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "aerel_gain1", eed->aerel_gain1);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "derel_thres0", eed->derel_thres0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "derel_gain0", eed->derel_gain0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "derel_thres1", eed->derel_thres1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "derel_gain1", eed->derel_gain1);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "coring_pos0", eed->coring_pos0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "coring_pos1", eed->coring_pos1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "coring_neg0", eed->coring_neg0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "coring_neg1", eed->coring_neg1);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "gain_exp", eed->gain_exp);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "gain_pos0", eed->gain_pos0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "gain_pos1", eed->gain_pos1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "gain_neg0", eed->gain_neg0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "gain_neg1", eed->gain_neg1);

	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "pos_margin0", eed->pos_margin0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "pos_margin1", eed->pos_margin1);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "neg_margin0", eed->neg_margin0);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "neg_margin1", eed->neg_margin1);
}
#endif

