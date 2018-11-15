/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include "dp_panel.h"
#include <drm/drm_fixed.h>

#define DP_KHZ_TO_HZ 1000
#define DP_PANEL_DEFAULT_BPP 24
#define DP_MAX_DS_PORT_COUNT 1

#define DPRX_FEATURE_ENUMERATION_LIST 0x2210
#define DPRX_EXTENDED_DPCD_FIELD 0x2200
#define VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED BIT(3)
#define VSC_EXT_VESA_SDP_SUPPORTED BIT(4)
#define VSC_EXT_VESA_SDP_CHAINING_SUPPORTED BIT(5)

enum dp_panel_hdr_pixel_encoding {
	RGB,
	YCbCr444,
	YCbCr422,
	YCbCr420,
	YONLY,
	RAW,
};

enum dp_panel_hdr_rgb_colorimetry {
	sRGB,
	RGB_WIDE_GAMUT_FIXED_POINT,
	RGB_WIDE_GAMUT_FLOATING_POINT,
	ADOBERGB,
	DCI_P3,
	CUSTOM_COLOR_PROFILE,
	ITU_R_BT_2020_RGB,
};

enum dp_panel_hdr_dynamic_range {
	VESA,
	CEA,
};

enum dp_panel_hdr_content_type {
	NOT_DEFINED,
	GRAPHICS,
	PHOTO,
	VIDEO,
	GAME,
};

enum dp_panel_hdr_state {
	HDR_DISABLED,
	HDR_ENABLED,
};

struct dp_panel_private {
	struct device *dev;
	struct dp_panel dp_panel;
	struct dp_aux *aux;
	struct dp_link *link;
	struct dp_parser *parser;
	struct dp_catalog_panel *catalog;
	bool custom_edid;
	bool custom_dpcd;
	bool panel_on;
	bool vsc_supported;
	bool vscext_supported;
	bool vscext_chaining_supported;
	enum dp_panel_hdr_state hdr_state;
	u8 spd_vendor_name[8];
	u8 spd_product_description[16];
	u8 major;
	u8 minor;
};

static const struct dp_panel_info fail_safe = {
	.h_active = 640,
	.v_active = 480,
	.h_back_porch = 48,
	.h_front_porch = 16,
	.h_sync_width = 96,
	.h_active_low = 0,
	.v_back_porch = 33,
	.v_front_porch = 10,
	.v_sync_width = 2,
	.v_active_low = 0,
	.h_skew = 0,
	.refresh_rate = 60,
	.pixel_clk_khz = 25200,
	.bpp = 24,
};

/* OEM NAME */
static const u8 vendor_name[8] = {81, 117, 97, 108, 99, 111, 109, 109};

/* MODEL NAME */
static const u8 product_desc[16] = {83, 110, 97, 112, 100, 114, 97, 103,
	111, 110, 0, 0, 0, 0, 0, 0};

struct tu_algo_data {
	s64 lclk_fp;
	s64 pclk_fp;
	s64 lwidth;
	s64 hbp_relative_to_pclk;
	int nlanes;
	int bpp;
	int pixelEnc;
	int dsc_en;
	int async_en;
	int bpc;

	uint delay_start_link_extra_pixclk;
	int extra_buffer_margin;
	s64 ratio_fp;
	s64 original_ratio_fp;

	s64 err_fp;
	s64 n_err_fp;
	s64 n_n_err_fp;
	int tu_size;
	int tu_size_desired;
	int tu_size_minus1;

	int valid_boundary_link;
	s64 resulting_valid_fp;
	s64 total_valid_fp;
	s64 effective_valid_fp;
	s64 effective_valid_recorded_fp;
	int n_tus;
	int n_tus_per_lane;
	int paired_tus;
	int remainder_tus;
	int remainder_tus_upper;
	int remainder_tus_lower;
	int extra_bytes;
	int filler_size;
	int delay_start_link;

	int extra_pclk_cycles;
	int extra_pclk_cycles_in_link_clk;
	s64 ratio_by_tu_fp;
	s64 average_valid2_fp;
	int new_valid_boundary_link;
	int remainder_symbols_exist;
	int n_symbols;
	s64 n_remainder_symbols_per_lane_fp;
	s64 last_partial_tu_fp;
	s64 TU_ratio_err_fp;

	int n_tus_incl_last_incomplete_tu;
	int extra_pclk_cycles_tmp;
	int extra_pclk_cycles_in_link_clk_tmp;
	int extra_required_bytes_new_tmp;
	int filler_size_tmp;
	int lower_filler_size_tmp;
	int delay_start_link_tmp;

	bool boundary_moderation_en;
	int boundary_mod_lower_err;
	int upper_boundary_count;
	int lower_boundary_count;
	int i_upper_boundary_count;
	int i_lower_boundary_count;
	int valid_lower_boundary_link;
	int even_distribution_BF;
	int even_distribution_legacy;
	int even_distribution;
	int min_hblank_violated;
	s64 delay_start_time_fp;
	s64 hbp_time_fp;
	s64 hactive_time_fp;
	s64 diff_abs_fp;

	s64 ratio;
};

static int _tu_param_compare(s64 a, s64 b)
{
	u32 a_int, a_frac;
	u32 b_int, b_frac;

	if (a == b)
		return 0;

	a_int = (a >> 32) & 0x7FFFFFFF;
	a_frac = a & 0xFFFFFFFF;

	b_int = (b >> 32) & 0x7FFFFFFF;
	b_frac = b & 0xFFFFFFFF;

	if (a_int > b_int)
		return 1;
	else if (a_int < b_int)
		return 2;
	else if (a_frac > b_frac)
		return 1;
	else
		return 2;
}

static void _tu_valid_boundary_calc(struct tu_algo_data *tu)
{
	s64 temp1_fp, temp2_fp, temp, temp1, temp2;
	int compare_result_1, compare_result_2, compare_result_3;

	temp1_fp = drm_fixp_from_fraction(tu->tu_size, 1);
	temp2_fp = drm_fixp_mul(tu->ratio_fp, temp1_fp);

	tu->new_valid_boundary_link = drm_fixp2int_ceil(temp2_fp);

	temp = (tu->i_upper_boundary_count *
				tu->new_valid_boundary_link +
				tu->i_lower_boundary_count *
				(tu->new_valid_boundary_link-1));
	tu->average_valid2_fp = drm_fixp_from_fraction(temp,
					(tu->i_upper_boundary_count +
					tu->i_lower_boundary_count));

	temp1_fp = drm_fixp_from_fraction(tu->bpp, 8);
	temp2_fp = drm_fixp_from_fraction(tu->lwidth, 1);
	temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);
	temp2_fp = drm_fixp_div(temp1_fp, tu->average_valid2_fp);
	tu->n_tus = drm_fixp2int(temp2_fp);

	temp1_fp = drm_fixp_from_fraction(tu->n_tus, 1);
	temp2_fp = drm_fixp_mul(temp1_fp, tu->average_valid2_fp);
	temp1_fp = drm_fixp_from_fraction(tu->n_symbols, 1);
	temp2_fp = temp1_fp - temp2_fp;
	temp1_fp = drm_fixp_from_fraction(tu->nlanes, 1);
	temp2_fp = drm_fixp_div(temp2_fp, temp1_fp);
	tu->n_remainder_symbols_per_lane_fp = temp2_fp;

	temp1_fp = drm_fixp_from_fraction(tu->tu_size, 1);
	tu->last_partial_tu_fp =
			drm_fixp_div(tu->n_remainder_symbols_per_lane_fp,
					temp1_fp);

	if (tu->n_remainder_symbols_per_lane_fp != 0)
		tu->remainder_symbols_exist = 1;
	else
		tu->remainder_symbols_exist = 0;

	temp1_fp = drm_fixp_from_fraction(tu->n_tus, tu->nlanes);
	tu->n_tus_per_lane = drm_fixp2int(temp1_fp);

	tu->paired_tus = (int)((tu->n_tus_per_lane) /
					(tu->i_upper_boundary_count +
					 tu->i_lower_boundary_count));

	tu->remainder_tus = tu->n_tus_per_lane - tu->paired_tus *
						(tu->i_upper_boundary_count +
						tu->i_lower_boundary_count);

	if ((tu->remainder_tus - tu->i_upper_boundary_count) > 0) {
		tu->remainder_tus_upper = tu->i_upper_boundary_count;
		tu->remainder_tus_lower = tu->remainder_tus -
						tu->i_upper_boundary_count;
	} else {
		tu->remainder_tus_upper = tu->remainder_tus;
		tu->remainder_tus_lower = 0;
	}

	temp = tu->paired_tus * (tu->i_upper_boundary_count *
				tu->new_valid_boundary_link +
				tu->i_lower_boundary_count *
				(tu->new_valid_boundary_link - 1)) +
				(tu->remainder_tus_upper *
				 tu->new_valid_boundary_link) +
				(tu->remainder_tus_lower *
				(tu->new_valid_boundary_link - 1));
	tu->total_valid_fp = drm_fixp_from_fraction(temp, 1);

	if (tu->remainder_symbols_exist) {
		temp1_fp = tu->total_valid_fp +
				tu->n_remainder_symbols_per_lane_fp;
		temp2_fp = drm_fixp_from_fraction(tu->n_tus_per_lane, 1);
		temp2_fp = temp2_fp + tu->last_partial_tu_fp;
		temp1_fp = drm_fixp_div(temp1_fp, temp2_fp);
	} else {
		temp2_fp = drm_fixp_from_fraction(tu->n_tus_per_lane, 1);
		temp1_fp = drm_fixp_div(tu->total_valid_fp, temp2_fp);
	}
	tu->effective_valid_fp = temp1_fp;

	temp1_fp = drm_fixp_from_fraction(tu->tu_size, 1);
	temp2_fp = drm_fixp_mul(tu->ratio_fp, temp1_fp);
	tu->n_n_err_fp = tu->effective_valid_fp - temp2_fp;

	temp1_fp = drm_fixp_from_fraction(tu->tu_size, 1);
	temp2_fp = drm_fixp_mul(tu->ratio_fp, temp1_fp);
	tu->n_err_fp = tu->average_valid2_fp - temp2_fp;

	tu->even_distribution = tu->n_tus % tu->nlanes == 0 ? 1 : 0;

	temp1_fp = drm_fixp_from_fraction(tu->bpp, 8);
	temp2_fp = drm_fixp_from_fraction(tu->lwidth, 1);
	temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);
	temp2_fp = drm_fixp_div(temp1_fp, tu->average_valid2_fp);

	if (temp2_fp)
		tu->n_tus_incl_last_incomplete_tu = drm_fixp2int_ceil(temp2_fp);
	else
		tu->n_tus_incl_last_incomplete_tu = 0;

	temp1 = 0;
	temp1_fp = drm_fixp_from_fraction(tu->tu_size, 1);
	temp2_fp = drm_fixp_mul(tu->original_ratio_fp, temp1_fp);
	temp1_fp = tu->average_valid2_fp - temp2_fp;
	temp2_fp = drm_fixp_from_fraction(tu->n_tus_incl_last_incomplete_tu, 1);
	temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);

	if (temp1_fp)
		temp1 = drm_fixp2int_ceil(temp1_fp);

	temp = tu->i_upper_boundary_count * tu->nlanes;
	temp1_fp = drm_fixp_from_fraction(tu->tu_size, 1);
	temp2_fp = drm_fixp_mul(tu->original_ratio_fp, temp1_fp);
	temp1_fp = drm_fixp_from_fraction(tu->new_valid_boundary_link, 1);
	temp2_fp = temp1_fp - temp2_fp;
	temp1_fp = drm_fixp_from_fraction(temp, 1);
	temp2_fp = drm_fixp_mul(temp1_fp, temp2_fp);

	if (temp2_fp)
		temp2 = drm_fixp2int_ceil(temp2_fp);
	else
		temp2 = 0;
	tu->extra_required_bytes_new_tmp = (int)(temp1 + temp2);

	temp1_fp = drm_fixp_from_fraction(8, tu->bpp);
	temp2_fp = drm_fixp_from_fraction(
	tu->extra_required_bytes_new_tmp, 1);
	temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);

	if (temp1_fp)
		tu->extra_pclk_cycles_tmp = drm_fixp2int_ceil(temp1_fp);
	else
		tu->extra_pclk_cycles_tmp = 0;

	temp1_fp = drm_fixp_from_fraction(tu->extra_pclk_cycles_tmp, 1);
	temp2_fp = drm_fixp_div(tu->lclk_fp, tu->pclk_fp);
	temp1_fp = drm_fixp_mul(temp1_fp, temp2_fp);

	if (temp1_fp)
		tu->extra_pclk_cycles_in_link_clk_tmp =
						drm_fixp2int_ceil(temp1_fp);
	else
		tu->extra_pclk_cycles_in_link_clk_tmp = 0;

	tu->filler_size_tmp = tu->tu_size - tu->new_valid_boundary_link;

	tu->lower_filler_size_tmp = tu->filler_size_tmp + 1;

	tu->delay_start_link_tmp = tu->extra_pclk_cycles_in_link_clk_tmp +
					tu->lower_filler_size_tmp +
					tu->extra_buffer_margin;

	temp1_fp = drm_fixp_from_fraction(tu->delay_start_link_tmp, 1);
	tu->delay_start_time_fp = drm_fixp_div(temp1_fp, tu->lclk_fp);

	compare_result_1 = _tu_param_compare(tu->n_n_err_fp, tu->diff_abs_fp);
	if (compare_result_1 == 2)
		compare_result_1 = 1;
	else
		compare_result_1 = 0;

	compare_result_2 = _tu_param_compare(tu->n_n_err_fp, tu->err_fp);
	if (compare_result_2 == 2)
		compare_result_2 = 1;
	else
		compare_result_2 = 0;

	compare_result_3 = _tu_param_compare(tu->hbp_time_fp,
					tu->delay_start_time_fp);
	if (compare_result_3 == 2)
		compare_result_3 = 0;
	else
		compare_result_3 = 1;

	if (((tu->even_distribution == 1) ||
			((tu->even_distribution_BF == 0) &&
			(tu->even_distribution_legacy == 0))) &&
			tu->n_err_fp >= 0 && tu->n_n_err_fp >= 0 &&
			compare_result_2 &&
			(compare_result_1 || (tu->min_hblank_violated == 1)) &&
			(tu->new_valid_boundary_link - 1) > 0 &&
			compare_result_3 &&
			(tu->delay_start_link_tmp <= 1023)) {
		tu->upper_boundary_count = tu->i_upper_boundary_count;
		tu->lower_boundary_count = tu->i_lower_boundary_count;
		tu->err_fp = tu->n_n_err_fp;
		tu->boundary_moderation_en = true;
		tu->tu_size_desired = tu->tu_size;
		tu->valid_boundary_link = tu->new_valid_boundary_link;
		tu->effective_valid_recorded_fp = tu->effective_valid_fp;
		tu->even_distribution_BF = 1;
		tu->delay_start_link = tu->delay_start_link_tmp;
	} else if (tu->boundary_mod_lower_err == 0) {
		compare_result_1 = _tu_param_compare(tu->n_n_err_fp,
							tu->diff_abs_fp);
		if (compare_result_1 == 2)
			tu->boundary_mod_lower_err = 1;
	}
}

static void _dp_panel_calc_tu(struct dp_tu_calc_input *in,
				   struct dp_vc_tu_mapping_table *tu_table)
{
	struct tu_algo_data tu;
	int compare_result_1, compare_result_2;
	u64 temp = 0;
	s64 temp_fp = 0, temp1_fp = 0, temp2_fp = 0;

	s64 LCLK_FAST_SKEW_fp = drm_fixp_from_fraction(6, 10000); /* 0.0006 */
	s64 const_p49_fp = drm_fixp_from_fraction(49, 100); /* 0.49 */
	s64 const_p56_fp = drm_fixp_from_fraction(56, 100); /* 0.56 */
	s64 RATIO_SCALE_fp = drm_fixp_from_fraction(1001, 1000);

	u8 DP_BRUTE_FORCE = 1;
	s64 BRUTE_FORCE_THRESHOLD_fp = drm_fixp_from_fraction(1, 10); /* 0.1 */
	uint EXTRA_PIXCLK_CYCLE_DELAY = 4;
	uint HBLANK_MARGIN = 4;

	memset(&tu, 0, sizeof(tu));

	tu.lclk_fp              = drm_fixp_from_fraction(in->lclk, 1);
	tu.pclk_fp              = drm_fixp_from_fraction(in->pclk_khz, 1000);
	tu.lwidth               = in->hactive;
	tu.hbp_relative_to_pclk = in->hporch;
	tu.nlanes               = in->nlanes;
	tu.bpp                  = in->bpp;
	tu.pixelEnc             = in->pixel_enc;
	tu.dsc_en               = in->dsc_en;
	tu.async_en             = in->async_en;

	tu.err_fp = drm_fixp_from_fraction(1000, 1); /* 1000 */

	if (tu.pixelEnc == 420) {
		temp_fp = drm_fixp_from_fraction(2, 1);
		tu.pclk_fp = drm_fixp_div(tu.pclk_fp, temp_fp);
		tu.lwidth /= 2;
		tu.hbp_relative_to_pclk /= 2;
	}

	if (tu.pixelEnc == 422) {
		switch (tu.bpp) {
		case 24:
			tu.bpp = 16;
			tu.bpc = 8;
			break;
		case 30:
			tu.bpp = 20;
			tu.bpc = 10;
			break;
		default:
			tu.bpp = 16;
			tu.bpc = 8;
			break;
		}
	} else {
		tu.bpc = tu.bpp/3;
	}

	temp1_fp = drm_fixp_from_fraction(4, 1);
	temp2_fp = drm_fixp_mul(temp1_fp, tu.lclk_fp);
	temp_fp = drm_fixp_div(temp2_fp, tu.pclk_fp);
	tu.extra_buffer_margin = drm_fixp2int_ceil(temp_fp);

	temp1_fp = drm_fixp_from_fraction(tu.bpp, 8);
	temp2_fp = drm_fixp_mul(tu.pclk_fp, temp1_fp);
	temp1_fp = drm_fixp_from_fraction(tu.nlanes, 1);
	temp2_fp = drm_fixp_div(temp2_fp, temp1_fp);
	tu.ratio_fp = drm_fixp_div(temp2_fp, tu.lclk_fp);

	tu.original_ratio_fp = tu.ratio_fp;
	tu.boundary_moderation_en = false;
	tu.upper_boundary_count = 0;
	tu.lower_boundary_count = 0;
	tu.i_upper_boundary_count = 0;
	tu.i_lower_boundary_count = 0;
	tu.valid_lower_boundary_link = 0;
	tu.even_distribution_BF = 0;
	tu.even_distribution_legacy = 0;
	tu.even_distribution = 0;
	tu.delay_start_time_fp = 0;

	tu.err_fp = drm_fixp_from_fraction(1000, 1);
	tu.n_err_fp = 0;
	tu.n_n_err_fp = 0;

	tu.ratio = drm_fixp2int(tu.ratio_fp);
	if ((((u32)tu.lwidth % tu.nlanes) != 0) &&
			!tu.ratio && tu.dsc_en == 0) {
		tu.ratio_fp = drm_fixp_mul(tu.ratio_fp, RATIO_SCALE_fp);
		tu.ratio = drm_fixp2int(tu.ratio_fp);
		if (tu.ratio)
			tu.ratio_fp = drm_fixp_from_fraction(1, 1);
	}

	if (tu.ratio > 1)
		tu.ratio = 1;

	if (tu.ratio == 1)
		goto tu_size_calc;

	compare_result_1 = _tu_param_compare(tu.ratio_fp, const_p49_fp);
	if (!compare_result_1 || compare_result_1 == 1)
		compare_result_1 = 1;
	else
		compare_result_1 = 0;

	compare_result_2 = _tu_param_compare(tu.ratio_fp, const_p56_fp);
	if (!compare_result_2 || compare_result_2 == 2)
		compare_result_2 = 1;
	else
		compare_result_2 = 0;

	if (tu.dsc_en && compare_result_1 && compare_result_2) {
		HBLANK_MARGIN += 4;
		pr_info("Info: increase HBLANK_MARGIN to %d\n", HBLANK_MARGIN);
	}

tu_size_calc:
	for (tu.tu_size = 32; tu.tu_size <= 64; tu.tu_size++) {
		temp1_fp = drm_fixp_from_fraction(tu.tu_size, 1);
		temp2_fp = drm_fixp_mul(tu.ratio_fp, temp1_fp);
		temp = drm_fixp2int_ceil(temp2_fp);
		temp1_fp = drm_fixp_from_fraction(temp, 1);
		tu.n_err_fp = temp1_fp - temp2_fp;

		if (tu.n_err_fp < tu.err_fp) {
			tu.err_fp = tu.n_err_fp;
			tu.tu_size_desired = tu.tu_size;
		}
	}

	tu.tu_size_minus1 = tu.tu_size_desired - 1;

	temp1_fp = drm_fixp_from_fraction(tu.tu_size_desired, 1);
	temp2_fp = drm_fixp_mul(tu.ratio_fp, temp1_fp);
	tu.valid_boundary_link = drm_fixp2int_ceil(temp2_fp);

	temp1_fp = drm_fixp_from_fraction(tu.bpp, 8);
	temp2_fp = drm_fixp_from_fraction(tu.lwidth, 1);
	temp2_fp = drm_fixp_mul(temp2_fp, temp1_fp);

	temp1_fp = drm_fixp_from_fraction(tu.valid_boundary_link, 1);
	temp2_fp = drm_fixp_div(temp2_fp, temp1_fp);
	tu.n_tus = drm_fixp2int(temp2_fp);

	tu.even_distribution_legacy = tu.n_tus % tu.nlanes == 0 ? 1 : 0;
	pr_info("Info: n_sym = %d, num_of_tus = %d\n",
		tu.valid_boundary_link, tu.n_tus);

	temp1_fp = drm_fixp_from_fraction(tu.tu_size_desired, 1);
	temp2_fp = drm_fixp_mul(tu.original_ratio_fp, temp1_fp);
	temp1_fp = drm_fixp_from_fraction(tu.valid_boundary_link, 1);
	temp2_fp = temp1_fp - temp2_fp;
	temp1_fp = drm_fixp_from_fraction(tu.n_tus + 1, 1);
	temp2_fp = drm_fixp_mul(temp1_fp, temp2_fp);

	temp = drm_fixp2int(temp2_fp);
	if (temp && temp2_fp)
		tu.extra_bytes = drm_fixp2int_ceil(temp2_fp);
	else
		tu.extra_bytes = 0;

	temp1_fp = drm_fixp_from_fraction(tu.extra_bytes, 1);
	temp2_fp = drm_fixp_from_fraction(8, tu.bpp);
	temp1_fp = drm_fixp_mul(temp1_fp, temp2_fp);

	temp - drm_fixp2int(temp1_fp);
	if (temp && temp1_fp)
		tu.extra_pclk_cycles = drm_fixp2int_ceil(temp1_fp);
	else
		tu.extra_pclk_cycles = 0;

	temp1_fp = drm_fixp_div(tu.lclk_fp, tu.pclk_fp);
	temp2_fp = drm_fixp_from_fraction(tu.extra_pclk_cycles, 1);
	temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);

	temp = drm_fixp2int(temp1_fp);
	if (temp && temp1_fp)
		tu.extra_pclk_cycles_in_link_clk = drm_fixp2int_ceil(temp1_fp);
	else
		tu.extra_pclk_cycles_in_link_clk = 0;

	tu.filler_size = tu.tu_size_desired - tu.valid_boundary_link;

	temp1_fp = drm_fixp_from_fraction(tu.tu_size_desired, 1);
	tu.ratio_by_tu_fp = drm_fixp_mul(tu.ratio_fp, temp1_fp);

	tu.delay_start_link = tu.extra_pclk_cycles_in_link_clk +
				tu.filler_size + tu.extra_buffer_margin;

	tu.resulting_valid_fp =
			drm_fixp_from_fraction(tu.valid_boundary_link, 1);

	temp1_fp = drm_fixp_from_fraction(tu.tu_size_desired, 1);
	temp2_fp = drm_fixp_div(tu.resulting_valid_fp, temp1_fp);
	tu.TU_ratio_err_fp = temp2_fp - tu.original_ratio_fp;

	temp1_fp = drm_fixp_from_fraction(
			(tu.hbp_relative_to_pclk - HBLANK_MARGIN), 1);
	tu.hbp_time_fp = drm_fixp_div(temp1_fp, tu.pclk_fp);

	temp1_fp = drm_fixp_from_fraction(tu.delay_start_link, 1);
	tu.delay_start_time_fp = drm_fixp_div(temp1_fp, tu.lclk_fp);

	compare_result_1 = _tu_param_compare(tu.hbp_time_fp,
					tu.delay_start_time_fp);
	if (compare_result_1 == 2) /* if (hbp_time_fp < delay_start_time_fp) */
		tu.min_hblank_violated = 1;

	temp1_fp = drm_fixp_from_fraction(tu.lwidth, 1);
	tu.hactive_time_fp = drm_fixp_div(temp1_fp, tu.pclk_fp);

	compare_result_2 = _tu_param_compare(tu.hactive_time_fp,
						tu.delay_start_time_fp);
	if (compare_result_2 == 2)
		tu.min_hblank_violated = 1;

	tu.delay_start_time_fp = 0;

	/* brute force */

	tu.delay_start_link_extra_pixclk = EXTRA_PIXCLK_CYCLE_DELAY;
	tu.diff_abs_fp = tu.resulting_valid_fp - tu.ratio_by_tu_fp;

	temp = drm_fixp2int(tu.diff_abs_fp);
	if (!temp && tu.diff_abs_fp <= 0xffff)
		tu.diff_abs_fp = 0;

	/* if(diff_abs < 0) diff_abs *= -1 */
	if (tu.diff_abs_fp < 0)
		tu.diff_abs_fp = drm_fixp_mul(tu.diff_abs_fp, -1);

	tu.boundary_mod_lower_err = 0;
	if ((tu.diff_abs_fp != 0 &&
			((tu.diff_abs_fp > BRUTE_FORCE_THRESHOLD_fp) ||
			 (tu.even_distribution_legacy == 0) ||
			 (DP_BRUTE_FORCE == 1))) ||
			(tu.min_hblank_violated == 1)) {
		do {
			tu.err_fp = drm_fixp_from_fraction(1000, 1);

			temp1_fp = drm_fixp_div(tu.lclk_fp, tu.pclk_fp);
			temp2_fp = drm_fixp_from_fraction(
					tu.delay_start_link_extra_pixclk, 1);
			temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);

			if (temp1_fp)
				tu.extra_buffer_margin =
					drm_fixp2int_ceil(temp1_fp);
			else
				tu.extra_buffer_margin = 0;

			temp1_fp = drm_fixp_from_fraction(tu.bpp, 8);
			temp2_fp = drm_fixp_from_fraction(tu.lwidth, 1);
			temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);

			if (temp1_fp)
				tu.n_symbols = drm_fixp2int_ceil(temp1_fp);
			else
				tu.n_symbols = 0;

			for (tu.tu_size = 32; tu.tu_size <= 64; tu.tu_size++) {
				for (tu.i_upper_boundary_count = 1;
					tu.i_upper_boundary_count <= 15;
					tu.i_upper_boundary_count++) {
					for (tu.i_lower_boundary_count = 1;
						tu.i_lower_boundary_count <= 15;
						tu.i_lower_boundary_count++) {
						_tu_valid_boundary_calc(&tu);
					}
				}
			}
			tu.delay_start_link_extra_pixclk--;
		} while (tu.boundary_moderation_en != true &&
			tu.boundary_mod_lower_err == 1 &&
			tu.delay_start_link_extra_pixclk != 0);

		if (tu.boundary_moderation_en == true) {
			temp1_fp = drm_fixp_from_fraction(
					(tu.upper_boundary_count *
					tu.valid_boundary_link +
					tu.lower_boundary_count *
					(tu.valid_boundary_link - 1)), 1);
			temp2_fp = drm_fixp_from_fraction(
					(tu.upper_boundary_count +
					tu.lower_boundary_count), 1);
			tu.resulting_valid_fp =
					drm_fixp_div(temp1_fp, temp2_fp);

			temp1_fp = drm_fixp_from_fraction(
					tu.tu_size_desired, 1);
			tu.ratio_by_tu_fp =
				drm_fixp_mul(tu.original_ratio_fp, temp1_fp);

			tu.valid_lower_boundary_link =
				tu.valid_boundary_link - 1;

			temp1_fp = drm_fixp_from_fraction(tu.bpp, 8);
			temp2_fp = drm_fixp_from_fraction(tu.lwidth, 1);
			temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);
			temp2_fp = drm_fixp_div(temp1_fp,
						tu.resulting_valid_fp);
			tu.n_tus = drm_fixp2int(temp2_fp);

			tu.tu_size_minus1 = tu.tu_size_desired - 1;
			tu.even_distribution_BF = 1;

			temp1_fp =
				drm_fixp_from_fraction(tu.tu_size_desired, 1);
			temp2_fp =
				drm_fixp_div(tu.resulting_valid_fp, temp1_fp);
			tu.TU_ratio_err_fp = temp2_fp - tu.original_ratio_fp;
		}
	}

	temp1_fp = drm_fixp_from_fraction(tu.lwidth, 1);
	temp2_fp = drm_fixp_mul(LCLK_FAST_SKEW_fp, temp1_fp);

	if (temp2_fp)
		temp = drm_fixp2int_ceil(temp2_fp);
	else
		temp = 0;

	temp1_fp = drm_fixp_from_fraction(tu.nlanes, 1);
	temp2_fp = drm_fixp_mul(tu.original_ratio_fp, temp1_fp);
	temp1_fp = drm_fixp_from_fraction(tu.bpp, 8);
	temp2_fp = drm_fixp_div(temp1_fp, temp2_fp);
	temp1_fp = drm_fixp_from_fraction(temp, 1);
	temp2_fp = drm_fixp_mul(temp1_fp, temp2_fp);
	temp = drm_fixp2int(temp2_fp);

	if (tu.async_en)
		tu.delay_start_link += (int)temp;

	temp1_fp = drm_fixp_from_fraction(tu.delay_start_link, 1);
	tu.delay_start_time_fp = drm_fixp_div(temp1_fp, tu.lclk_fp);

	/* OUTPUTS */
	tu_table->valid_boundary_link       = tu.valid_boundary_link;
	tu_table->delay_start_link          = tu.delay_start_link;
	tu_table->boundary_moderation_en    = tu.boundary_moderation_en;
	tu_table->valid_lower_boundary_link = tu.valid_lower_boundary_link;
	tu_table->upper_boundary_count      = tu.upper_boundary_count;
	tu_table->lower_boundary_count      = tu.lower_boundary_count;
	tu_table->tu_size_minus1            = tu.tu_size_minus1;

	pr_info("TU: valid_boundary_link: %d\n", tu_table->valid_boundary_link);
	pr_info("TU: delay_start_link: %d\n", tu_table->delay_start_link);
	pr_info("TU: boundary_moderation_en: %d\n",
			tu_table->boundary_moderation_en);
	pr_info("TU: valid_lower_boundary_link: %d\n",
			tu_table->valid_lower_boundary_link);
	pr_info("TU: upper_boundary_count: %d\n",
			tu_table->upper_boundary_count);
	pr_info("TU: lower_boundary_count: %d\n",
			tu_table->lower_boundary_count);
	pr_info("TU: tu_size_minus1: %d\n", tu_table->tu_size_minus1);
}

static void dp_panel_calc_tu_parameters(struct dp_panel *dp_panel,
		struct dp_vc_tu_mapping_table *tu_table)
{
	struct dp_tu_calc_input in;
	struct dp_panel_info *pinfo;
	struct dp_panel_private *panel;
	int bw_code;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	pinfo = &dp_panel->pinfo;
	bw_code = panel->link->link_params.bw_code;

	in.lclk = drm_dp_bw_code_to_link_rate(bw_code) / 1000;
	in.pclk_khz = pinfo->pixel_clk_khz;
	in.hactive = pinfo->h_active;
	in.hporch = pinfo->h_back_porch + pinfo->h_front_porch +
				pinfo->h_sync_width;
	in.nlanes = panel->link->link_params.lane_count;
	in.bpp = pinfo->bpp;
	in.pixel_enc = 444;
	in.dsc_en = 0;
	in.async_en = 0;

	_dp_panel_calc_tu(&in, tu_table);
}

void dp_panel_calc_tu_test(struct dp_tu_calc_input *in,
		struct dp_vc_tu_mapping_table *tu_table)
{
	_dp_panel_calc_tu(in, tu_table);
}

static void dp_panel_config_tr_unit(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;
	struct dp_catalog_panel *catalog;
	u32 dp_tu = 0x0;
	u32 valid_boundary = 0x0;
	u32 valid_boundary2 = 0x0;
	struct dp_vc_tu_mapping_table tu_calc_table;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return;
	}

	if (dp_panel->stream_id != DP_STREAM_0)
		return;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;

	dp_panel_calc_tu_parameters(dp_panel, &tu_calc_table);

	dp_tu |= tu_calc_table.tu_size_minus1;
	valid_boundary |= tu_calc_table.valid_boundary_link;
	valid_boundary |= (tu_calc_table.delay_start_link << 16);

	valid_boundary2 |= (tu_calc_table.valid_lower_boundary_link << 1);
	valid_boundary2 |= (tu_calc_table.upper_boundary_count << 16);
	valid_boundary2 |= (tu_calc_table.lower_boundary_count << 20);

	if (tu_calc_table.boundary_moderation_en)
		valid_boundary2 |= BIT(0);

	pr_debug("dp_tu=0x%x, valid_boundary=0x%x, valid_boundary2=0x%x\n",
			dp_tu, valid_boundary, valid_boundary2);

	catalog->dp_tu = dp_tu;
	catalog->valid_boundary = valid_boundary;
	catalog->valid_boundary2 = valid_boundary2;

	catalog->update_transfer_unit(catalog);
}

static int dp_panel_read_dpcd(struct dp_panel *dp_panel, bool multi_func)
{
	int rlen, rc = 0;
	struct dp_panel_private *panel;
	struct drm_dp_link *link_info;
	struct drm_dp_aux *drm_aux;
	u8 *dpcd, rx_feature, temp;
	u32 dfp_count = 0, offset = DP_DPCD_REV;
	unsigned long caps = DP_LINK_CAP_ENHANCED_FRAMING;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	dpcd = dp_panel->dpcd;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	drm_aux = panel->aux->drm_aux;
	link_info = &dp_panel->link_info;

	/* reset vsc data */
	panel->vsc_supported = false;
	panel->vscext_supported = false;
	panel->vscext_chaining_supported = false;

	if (panel->custom_dpcd) {
		pr_debug("skip dpcd read in debug mode\n");
		goto skip_dpcd_read;
	}

	rlen = drm_dp_dpcd_read(drm_aux, DP_TRAINING_AUX_RD_INTERVAL, &temp, 1);
	if (rlen != 1) {
		pr_err("error reading DP_TRAINING_AUX_RD_INTERVAL\n");
		rc = -EINVAL;
		goto end;
	}

	/* check for EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT */
	if (temp & BIT(7)) {
		pr_debug("using EXTENDED_RECEIVER_CAPABILITY_FIELD\n");
		offset = DPRX_EXTENDED_DPCD_FIELD;
	}

	rlen = drm_dp_dpcd_read(drm_aux, offset,
		dp_panel->dpcd, (DP_RECEIVER_CAP_SIZE + 1));
	if (rlen < (DP_RECEIVER_CAP_SIZE + 1)) {
		pr_err("dpcd read failed, rlen=%d\n", rlen);
		if (rlen == -ETIMEDOUT)
			rc = rlen;
		else
			rc = -EINVAL;

		goto end;
	}

	print_hex_dump(KERN_DEBUG, "[drm-dp] SINK DPCD: ",
		DUMP_PREFIX_NONE, 8, 1, dp_panel->dpcd, rlen, false);

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux,
		DPRX_FEATURE_ENUMERATION_LIST, &rx_feature, 1);
	if (rlen != 1) {
		pr_debug("failed to read DPRX_FEATURE_ENUMERATION_LIST\n");
		goto skip_dpcd_read;
	}
	panel->vsc_supported = !!(rx_feature &
		VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED);
	panel->vscext_supported = !!(rx_feature & VSC_EXT_VESA_SDP_SUPPORTED);
	panel->vscext_chaining_supported = !!(rx_feature &
			VSC_EXT_VESA_SDP_CHAINING_SUPPORTED);

	pr_debug("vsc=%d, vscext=%d, vscext_chaining=%d\n",
		panel->vsc_supported, panel->vscext_supported,
		panel->vscext_chaining_supported);

skip_dpcd_read:
	link_info->revision = dp_panel->dpcd[DP_DPCD_REV];

	panel->major = (link_info->revision >> 4) & 0x0f;
	panel->minor = link_info->revision & 0x0f;
	pr_debug("version: %d.%d\n", panel->major, panel->minor);

	link_info->rate =
		drm_dp_bw_code_to_link_rate(dp_panel->dpcd[DP_MAX_LINK_RATE]);
	pr_debug("link_rate=%d\n", link_info->rate);

	link_info->num_lanes = dp_panel->dpcd[DP_MAX_LANE_COUNT] &
				DP_MAX_LANE_COUNT_MASK;

	if (multi_func)
		link_info->num_lanes = min_t(unsigned int,
			link_info->num_lanes, 2);

	pr_debug("lane_count=%d\n", link_info->num_lanes);

	if (drm_dp_enhanced_frame_cap(dpcd))
		link_info->capabilities |= caps;

	dfp_count = dpcd[DP_DOWN_STREAM_PORT_COUNT] &
						DP_DOWN_STREAM_PORT_COUNT;

	if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT)
		&& (dpcd[DP_DPCD_REV] > 0x10)) {
		rlen = drm_dp_dpcd_read(panel->aux->drm_aux,
			DP_DOWNSTREAM_PORT_0, dp_panel->ds_ports,
			DP_MAX_DOWNSTREAM_PORTS);
		if (rlen < DP_MAX_DOWNSTREAM_PORTS) {
			pr_err("ds port status failed, rlen=%d\n", rlen);
			rc = -EINVAL;
			goto end;
		}
	}

	if (dfp_count > DP_MAX_DS_PORT_COUNT)
		pr_debug("DS port count %d greater that max (%d) supported\n",
			dfp_count, DP_MAX_DS_PORT_COUNT);

end:
	return rc;
}

static int dp_panel_set_default_link_params(struct dp_panel *dp_panel)
{
	struct drm_dp_link *link_info;
	const int default_bw_code = 162000;
	const int default_num_lanes = 1;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}
	link_info = &dp_panel->link_info;
	link_info->rate = default_bw_code;
	link_info->num_lanes = default_num_lanes;
	pr_debug("link_rate=%d num_lanes=%d\n",
		link_info->rate, link_info->num_lanes);

	return 0;
}

static int dp_panel_set_edid(struct dp_panel *dp_panel, u8 *edid)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (edid) {
		dp_panel->edid_ctrl->edid = (struct edid *)edid;
		panel->custom_edid = true;
	} else {
		panel->custom_edid = false;
		dp_panel->edid_ctrl->edid = NULL;
	}

	pr_debug("%d\n", panel->custom_edid);
	return 0;
}

static int dp_panel_set_dpcd(struct dp_panel *dp_panel, u8 *dpcd)
{
	struct dp_panel_private *panel;
	u8 *dp_dpcd;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_dpcd = dp_panel->dpcd;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dpcd) {
		memcpy(dp_dpcd, dpcd, DP_RECEIVER_CAP_SIZE + 1);
		panel->custom_dpcd = true;
	} else {
		panel->custom_dpcd = false;
	}

	pr_debug("%d\n", panel->custom_dpcd);

	return 0;
}

static int dp_panel_read_edid(struct dp_panel *dp_panel,
	struct drm_connector *connector)
{
	int ret = 0;
	struct dp_panel_private *panel;
	struct edid *edid;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (panel->custom_edid) {
		pr_debug("skip edid read in debug mode\n");
		goto end;
	}

	sde_get_edid(connector, &panel->aux->drm_aux->ddc,
		(void **)&dp_panel->edid_ctrl);
	if (!dp_panel->edid_ctrl->edid) {
		pr_err("EDID read failed\n");
		ret = -EINVAL;
		goto end;
	}
end:
	edid = dp_panel->edid_ctrl->edid;
	dp_panel->audio_supported = drm_detect_monitor_audio(edid);

	return ret;
}

static int dp_panel_read_sink_caps(struct dp_panel *dp_panel,
	struct drm_connector *connector, bool multi_func)
{
	int rc = 0, rlen, count, downstream_ports;
	const int count_len = 1;
	struct dp_panel_private *panel;

	if (!dp_panel || !connector) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	rc = dp_panel_read_dpcd(dp_panel, multi_func);
	if (rc || !is_link_rate_valid(drm_dp_link_rate_to_bw_code(
		dp_panel->link_info.rate)) || !is_lane_count_valid(
		dp_panel->link_info.num_lanes) ||
		((drm_dp_link_rate_to_bw_code(dp_panel->link_info.rate)) >
		dp_panel->max_bw_code)) {
		if ((rc == -ETIMEDOUT) || (rc == -ENODEV)) {
			pr_err("DPCD read failed, return early\n");
			goto end;
		}
		pr_err("panel dpcd read failed/incorrect, set default params\n");
		dp_panel_set_default_link_params(dp_panel);
	}

	downstream_ports = dp_panel->dpcd[DP_DOWNSTREAMPORT_PRESENT] &
				DP_DWN_STRM_PORT_PRESENT;

	if (downstream_ports) {
		rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_SINK_COUNT,
				&count, count_len);
		if (rlen == count_len) {
			count = DP_GET_SINK_COUNT(count);
			if (!count) {
				pr_err("no downstream ports connected\n");
				panel->link->sink_count.count = 0;
				rc = -ENOTCONN;
				goto end;
			}
		}
	}

	rc = dp_panel_read_edid(dp_panel, connector);
	if (rc) {
		pr_err("panel edid read failed, set failsafe mode\n");
		return rc;
	}

	dp_panel->widebus_en = panel->parser->has_widebus;
end:
	return rc;
}

static u32 dp_panel_get_supported_bpp(struct dp_panel *dp_panel,
		u32 mode_edid_bpp, u32 mode_pclk_khz)
{
	struct drm_dp_link *link_info;
	const u32 max_supported_bpp = 30, min_supported_bpp = 18;
	u32 bpp = 0, data_rate_khz = 0;

	bpp = min_t(u32, mode_edid_bpp, max_supported_bpp);

	link_info = &dp_panel->link_info;
	data_rate_khz = link_info->num_lanes * link_info->rate * 8;

	while (bpp > min_supported_bpp) {
		if (mode_pclk_khz * bpp <= data_rate_khz)
			break;
		bpp -= 6;
	}

	return bpp;
}

static u32 dp_panel_get_mode_bpp(struct dp_panel *dp_panel,
		u32 mode_edid_bpp, u32 mode_pclk_khz)
{
	struct dp_panel_private *panel;
	u32 bpp = mode_edid_bpp;

	if (!dp_panel || !mode_edid_bpp || !mode_pclk_khz) {
		pr_err("invalid input\n");
		return 0;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dp_panel->video_test)
		bpp = dp_link_bit_depth_to_bpp(
				panel->link->test_video.test_bit_depth);
	else
		bpp = dp_panel_get_supported_bpp(dp_panel, mode_edid_bpp,
				mode_pclk_khz);

	return bpp;
}

static void dp_panel_set_test_mode(struct dp_panel_private *panel,
		struct dp_display_mode *mode)
{
	struct dp_panel_info *pinfo = NULL;
	struct dp_link_test_video *test_info = NULL;

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	pinfo = &mode->timing;
	test_info = &panel->link->test_video;

	pinfo->h_active = test_info->test_h_width;
	pinfo->h_sync_width = test_info->test_hsync_width;
	pinfo->h_back_porch = test_info->test_h_start -
		test_info->test_hsync_width;
	pinfo->h_front_porch = test_info->test_h_total -
		(test_info->test_h_start + test_info->test_h_width);

	pinfo->v_active = test_info->test_v_height;
	pinfo->v_sync_width = test_info->test_vsync_width;
	pinfo->v_back_porch = test_info->test_v_start -
		test_info->test_vsync_width;
	pinfo->v_front_porch = test_info->test_v_total -
		(test_info->test_v_start + test_info->test_v_height);

	pinfo->bpp = dp_link_bit_depth_to_bpp(test_info->test_bit_depth);
	pinfo->h_active_low = test_info->test_hsync_pol;
	pinfo->v_active_low = test_info->test_vsync_pol;

	pinfo->refresh_rate = test_info->test_rr_n;
	pinfo->pixel_clk_khz = test_info->test_h_total *
		test_info->test_v_total * pinfo->refresh_rate;

	if (test_info->test_rr_d == 0)
		pinfo->pixel_clk_khz /= 1000;
	else
		pinfo->pixel_clk_khz /= 1001;

	if (test_info->test_h_width == 640)
		pinfo->pixel_clk_khz = 25170;
}

static int dp_panel_get_modes(struct dp_panel *dp_panel,
	struct drm_connector *connector, struct dp_display_mode *mode)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dp_panel->video_test) {
		dp_panel_set_test_mode(panel, mode);
		return 1;
	} else if (dp_panel->edid_ctrl->edid) {
		return _sde_edid_update_modes(connector, dp_panel->edid_ctrl);
	}

	/* fail-safe mode */
	memcpy(&mode->timing, &fail_safe,
		sizeof(fail_safe));
	return 1;
}

static void dp_panel_handle_sink_request(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (panel->link->sink_request & DP_TEST_LINK_EDID_READ) {
		u8 checksum = sde_get_edid_checksum(dp_panel->edid_ctrl);

		panel->link->send_edid_checksum(panel->link, checksum);
		panel->link->send_test_response(panel->link);
	}
}

static void dp_panel_tpg_config(struct dp_panel *dp_panel, bool enable)
{
	u32 hsync_start_x, hsync_end_x;
	struct dp_catalog_panel *catalog;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return;
	}

	if (dp_panel->stream_id >= DP_STREAM_MAX) {
		pr_err("invalid stream id:%d\n", dp_panel->stream_id);
		return;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	pinfo = &panel->dp_panel.pinfo;

	if (!panel->panel_on) {
		pr_debug("DP panel not enabled, handle TPG on next panel on\n");
		return;
	}

	if (!enable) {
		panel->catalog->tpg_config(catalog, false);
		return;
	}

	/* TPG config */
	catalog->hsync_period = pinfo->h_sync_width + pinfo->h_back_porch +
			pinfo->h_active + pinfo->h_front_porch;
	catalog->vsync_period = pinfo->v_sync_width + pinfo->v_back_porch +
			pinfo->v_active + pinfo->v_front_porch;

	catalog->display_v_start = ((pinfo->v_sync_width +
			pinfo->v_back_porch) * catalog->hsync_period);
	catalog->display_v_end = ((catalog->vsync_period -
			pinfo->v_front_porch) * catalog->hsync_period) - 1;

	catalog->display_v_start += pinfo->h_sync_width + pinfo->h_back_porch;
	catalog->display_v_end -= pinfo->h_front_porch;

	hsync_start_x = pinfo->h_back_porch + pinfo->h_sync_width;
	hsync_end_x = catalog->hsync_period - pinfo->h_front_porch - 1;

	catalog->v_sync_width = pinfo->v_sync_width;

	catalog->hsync_ctl = (catalog->hsync_period << 16) |
			pinfo->h_sync_width;
	catalog->display_hctl = (hsync_end_x << 16) | hsync_start_x;

	panel->catalog->tpg_config(catalog, true);
}

static int dp_panel_config_timing(struct dp_panel *dp_panel)
{
	int rc = 0;
	u32 data, total_ver, total_hor;
	struct dp_catalog_panel *catalog;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	pinfo = &panel->dp_panel.pinfo;

	pr_debug("width=%d hporch= %d %d %d\n",
		pinfo->h_active, pinfo->h_back_porch,
		pinfo->h_front_porch, pinfo->h_sync_width);

	pr_debug("height=%d vporch= %d %d %d\n",
		pinfo->v_active, pinfo->v_back_porch,
		pinfo->v_front_porch, pinfo->v_sync_width);

	total_hor = pinfo->h_active + pinfo->h_back_porch +
		pinfo->h_front_porch + pinfo->h_sync_width;

	total_ver = pinfo->v_active + pinfo->v_back_porch +
			pinfo->v_front_porch + pinfo->v_sync_width;

	data = total_ver;
	data <<= 16;
	data |= total_hor;

	catalog->total = data;

	data = (pinfo->v_back_porch + pinfo->v_sync_width);
	data <<= 16;
	data |= (pinfo->h_back_porch + pinfo->h_sync_width);

	catalog->sync_start = data;

	data = pinfo->v_sync_width;
	data <<= 16;
	data |= (pinfo->v_active_low << 31);
	data |= pinfo->h_sync_width;
	data |= (pinfo->h_active_low << 15);

	catalog->width_blanking = data;

	data = pinfo->v_active;
	data <<= 16;
	data |= pinfo->h_active;

	catalog->dp_active = data;

	catalog->widebus_en = pinfo->widebus_en;

	panel->catalog->timing_cfg(catalog);
	panel->panel_on = true;
end:
	return rc;
}

static int dp_panel_edid_register(struct dp_panel_private *panel)
{
	int rc = 0;

	panel->dp_panel.edid_ctrl = sde_edid_init();
	if (!panel->dp_panel.edid_ctrl) {
		pr_err("sde edid init for DP failed\n");
		rc = -ENOMEM;
	}

	return rc;
}

static void dp_panel_edid_deregister(struct dp_panel_private *panel)
{
	sde_edid_deinit((void **)&panel->dp_panel.edid_ctrl);
}

static int dp_panel_set_stream_info(struct dp_panel *dp_panel,
		enum dp_stream_id stream_id, u32 ch_start_slot,
			u32 ch_tot_slots, u32 pbn)
{
	if (!dp_panel || stream_id > DP_STREAM_MAX) {
		pr_err("invalid input. stream_id: %d\n", stream_id);
		return -EINVAL;
	}

	dp_panel->stream_id = stream_id;
	dp_panel->channel_start_slot = ch_start_slot;
	dp_panel->channel_total_slots = ch_tot_slots;
	dp_panel->pbn = pbn;

	return 0;
}

static int dp_panel_init_panel_info(struct dp_panel *dp_panel)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	pinfo = &dp_panel->pinfo;

	/*
	 * print resolution info as this is a result
	 * of user initiated action of cable connection
	 */
	pr_info("DP RESOLUTION: active(back|front|width|low)\n");
	pr_info("%d(%d|%d|%d|%d)x%d(%d|%d|%d|%d)@%dfps %dbpp %dKhz %dLR %dLn\n",
		pinfo->h_active, pinfo->h_back_porch, pinfo->h_front_porch,
		pinfo->h_sync_width, pinfo->h_active_low,
		pinfo->v_active, pinfo->v_back_porch, pinfo->v_front_porch,
		pinfo->v_sync_width, pinfo->v_active_low,
		pinfo->refresh_rate, pinfo->bpp, pinfo->pixel_clk_khz,
		panel->link->link_params.bw_code,
		panel->link->link_params.lane_count);
end:
	return rc;
}

static int dp_panel_deinit_panel_info(struct dp_panel *dp_panel, u32 flags)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_catalog_hdr_data *hdr;
	struct drm_connector *connector;
	struct sde_connector_state *c_state;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (flags & DP_PANEL_SRC_INITIATED_POWER_DOWN) {
		pr_debug("retain states in src initiated power down request\n");
		return 0;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	hdr = &panel->catalog->hdr_data;

	if (!panel->custom_edid)
		sde_free_edid((void **)&dp_panel->edid_ctrl);

	dp_panel_set_stream_info(dp_panel, DP_STREAM_MAX, 0, 0, 0);
	memset(&dp_panel->pinfo, 0, sizeof(dp_panel->pinfo));
	memset(&hdr->hdr_meta, 0, sizeof(hdr->hdr_meta));
	panel->panel_on = false;

	connector = dp_panel->connector;
	c_state = to_sde_connector_state(connector->state);

	connector->hdr_eotf = 0;
	connector->hdr_metadata_type_one = 0;
	connector->hdr_max_luminance = 0;
	connector->hdr_avg_luminance = 0;
	connector->hdr_min_luminance = 0;
	connector->hdr_supported = false;

	memset(&c_state->hdr_meta, 0, sizeof(c_state->hdr_meta));

	return rc;
}

static u32 dp_panel_get_min_req_link_rate(struct dp_panel *dp_panel)
{
	const u32 encoding_factx10 = 8;
	u32 min_link_rate_khz = 0, lane_cnt;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		goto end;
	}

	lane_cnt = dp_panel->link_info.num_lanes;
	pinfo = &dp_panel->pinfo;

	/* num_lanes * lane_count * 8 >= pclk * bpp * 10 */
	min_link_rate_khz = pinfo->pixel_clk_khz /
				(lane_cnt * encoding_factx10);
	min_link_rate_khz *= pinfo->bpp;

	pr_debug("min lclk req=%d khz for pclk=%d khz, lanes=%d, bpp=%d\n",
		min_link_rate_khz, pinfo->pixel_clk_khz, lane_cnt,
		pinfo->bpp);
end:
	return min_link_rate_khz;
}

static bool dp_panel_hdr_supported(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return false;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	return panel->major >= 1 && panel->vsc_supported &&
		(panel->minor >= 4 || panel->vscext_supported);
}

static int dp_panel_setup_hdr(struct dp_panel *dp_panel,
		struct drm_msm_ext_hdr_metadata *hdr_meta)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_catalog_hdr_data *hdr;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	hdr = &panel->catalog->hdr_data;

	/* use cached meta data in case meta data not provided */
	if (!hdr_meta) {
		if (hdr->hdr_meta.hdr_state)
			goto cached;
		else
			goto end;
	}

	panel->hdr_state = hdr_meta->hdr_state;

	hdr->ext_header_byte0 = 0x00;
	hdr->ext_header_byte1 = 0x04;
	hdr->ext_header_byte2 = 0x1F;
	hdr->ext_header_byte3 = 0x00;

	hdr->vsc_header_byte0 = 0x00;
	hdr->vsc_header_byte1 = 0x07;
	hdr->vsc_header_byte2 = 0x05;
	hdr->vsc_header_byte3 = 0x13;

	hdr->vscext_header_byte0 = 0x00;
	hdr->vscext_header_byte1 = 0x87;
	hdr->vscext_header_byte2 = 0x1D;
	hdr->vscext_header_byte3 = 0x13 << 2;

	/* VSC SDP Payload for DB16 */
	hdr->pixel_encoding = RGB;
	hdr->colorimetry = ITU_R_BT_2020_RGB;

	/* VSC SDP Payload for DB17 */
	hdr->dynamic_range = CEA;

	/* VSC SDP Payload for DB18 */
	hdr->content_type = GRAPHICS;

	hdr->bpc = dp_panel->pinfo.bpp / 3;

	hdr->version = 0x01;
	hdr->length = 0x1A;

	if (panel->hdr_state)
		memcpy(&hdr->hdr_meta, hdr_meta, sizeof(hdr->hdr_meta));
	else
		memset(&hdr->hdr_meta, 0, sizeof(hdr->hdr_meta));
cached:
	if (panel->panel_on) {
		panel->catalog->stream_id = dp_panel->stream_id;
		panel->catalog->config_hdr(panel->catalog, panel->hdr_state);
	}
end:
	return rc;
}

static int dp_panel_spd_config(struct dp_panel *dp_panel)
{
	int rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	if (dp_panel->stream_id >= DP_STREAM_MAX) {
		pr_err("invalid stream id:%d\n", dp_panel->stream_id);
		return -EINVAL;
	}

	if (!dp_panel->spd_enabled) {
		pr_debug("SPD Infoframe not enabled\n");
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	panel->catalog->spd_vendor_name = panel->spd_vendor_name;
	panel->catalog->spd_product_description =
		panel->spd_product_description;

	panel->catalog->stream_id = dp_panel->stream_id;
	panel->catalog->config_spd(panel->catalog);
end:
	return rc;
}

static void dp_panel_config_ctrl(struct dp_panel *dp_panel)
{
	u32 config = 0, tbd;
	u8 *dpcd = dp_panel->dpcd;
	struct dp_panel_private *panel;
	struct dp_catalog_panel *catalog;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;

	config |= (2 << 13); /* Default-> LSCLK DIV: 1/4 LCLK  */
	config |= (0 << 11); /* RGB */

	/* Scrambler reset enable */
	if (dpcd[DP_EDP_CONFIGURATION_CAP] & DP_ALTERNATE_SCRAMBLER_RESET_CAP)
		config |= (1 << 10);

	tbd = panel->link->get_test_bits_depth(panel->link,
			dp_panel->pinfo.bpp);

	if (tbd == DP_TEST_BIT_DEPTH_UNKNOWN)
		tbd = DP_TEST_BIT_DEPTH_8;

	config |= tbd << 8;

	/* Num of Lanes */
	config |= ((panel->link->link_params.lane_count - 1) << 4);

	if (drm_dp_enhanced_frame_cap(dpcd))
		config |= 0x40;

	config |= 0x04; /* progressive video */

	config |= 0x03;	/* sycn clock & static Mvid */

	catalog->config_ctrl(catalog, config);
}

static void dp_panel_config_misc(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;
	struct dp_catalog_panel *catalog;
	u32 misc_val;
	u32 tb, cc;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;

	tb = panel->link->get_test_bits_depth(panel->link, dp_panel->pinfo.bpp);
	cc = panel->link->get_colorimetry_config(panel->link);

	misc_val = cc;
	misc_val |= (tb << 5);
	misc_val |= BIT(0); /* Configure clock to synchronous mode */

	catalog->misc_val = misc_val;
	catalog->config_misc(catalog);
}

static bool dp_panel_use_fixed_nvid(struct dp_panel *dp_panel)
{
	u8 *dpcd = dp_panel->dpcd;
	struct sde_connector *c_conn = to_sde_connector(dp_panel->connector);

	/* use fixe mvid and nvid for MST streams */
	if (c_conn->mst_port)
		return true;

	/*
	 * For better interop experience, used a fixed NVID=0x8000
	 * whenever connected to a VGA dongle downstream.
	 */
	if (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT) {
		u8 type = dpcd[DP_DOWNSTREAMPORT_PRESENT] &
			DP_DWN_STRM_PORT_TYPE_MASK;
		if (type == DP_DWN_STRM_PORT_TYPE_ANALOG)
			return true;
	}

	return false;
}

static void dp_panel_config_msa(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;
	struct dp_catalog_panel *catalog;
	u32 rate;
	u32 stream_rate_khz;
	bool fixed_nvid;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;

	catalog->widebus_en = dp_panel->widebus_en;

	fixed_nvid = dp_panel_use_fixed_nvid(dp_panel);
	rate = drm_dp_bw_code_to_link_rate(panel->link->link_params.bw_code);
	stream_rate_khz = dp_panel->pinfo.pixel_clk_khz;

	catalog->config_msa(catalog, rate, stream_rate_khz, fixed_nvid);
}

static int dp_panel_hw_cfg(struct dp_panel *dp_panel, bool enable)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (dp_panel->stream_id >= DP_STREAM_MAX) {
		pr_err("invalid stream_id: %d\n", dp_panel->stream_id);
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	panel->catalog->stream_id = dp_panel->stream_id;

	if (enable) {
		dp_panel_config_ctrl(dp_panel);
		dp_panel_config_misc(dp_panel);
		dp_panel_config_msa(dp_panel);
		dp_panel_config_tr_unit(dp_panel);
		dp_panel_config_timing(dp_panel);
	}

	panel->catalog->config_dto(panel->catalog, !enable);

	return 0;
}

static int dp_panel_read_sink_sts(struct dp_panel *dp_panel, u8 *sts, u32 size)
{
	int rlen, rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel || !sts || !size) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		return rc;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_SINK_COUNT_ESI,
		sts, size);
	if (rlen != size) {
		pr_err("dpcd sink sts fail rlen:%d size:%d\n", rlen, size);
		rc = -EINVAL;
		return rc;
	}

	return 0;
}

static int dp_panel_update_edid(struct dp_panel *dp_panel, struct edid *edid)
{
	dp_panel->edid_ctrl->edid = edid;
	sde_parse_edid(dp_panel->edid_ctrl);
	return _sde_edid_update_modes(dp_panel->connector, dp_panel->edid_ctrl);
}

static bool dp_panel_read_mst_cap(struct dp_panel *dp_panel)
{
	int rlen;
	struct dp_panel_private *panel;
	u8 dpcd;
	bool mst_cap = false;

	if (!dp_panel) {
		pr_err("invalid input\n");
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_MSTM_CAP,
		&dpcd, 1);
	if (rlen < 1) {
		pr_err("dpcd mstm_cap read failed, rlen=%d\n", rlen);
		goto end;
	}

	mst_cap = (dpcd & DP_MST_CAP) ? true : false;

end:
	pr_debug("dp mst-cap: %d\n", mst_cap);

	return mst_cap;
}

static void dp_panel_convert_to_dp_mode(struct dp_panel *dp_panel,
		const struct drm_display_mode *drm_mode,
		struct dp_display_mode *dp_mode)
{
	const u32 num_components = 3, default_bpp = 24;

	dp_mode->timing.h_active = drm_mode->hdisplay;
	dp_mode->timing.h_back_porch = drm_mode->htotal - drm_mode->hsync_end;
	dp_mode->timing.h_sync_width = drm_mode->htotal -
			(drm_mode->hsync_start + dp_mode->timing.h_back_porch);
	dp_mode->timing.h_front_porch = drm_mode->hsync_start -
					 drm_mode->hdisplay;
	dp_mode->timing.h_skew = drm_mode->hskew;

	dp_mode->timing.v_active = drm_mode->vdisplay;
	dp_mode->timing.v_back_porch = drm_mode->vtotal - drm_mode->vsync_end;
	dp_mode->timing.v_sync_width = drm_mode->vtotal -
		(drm_mode->vsync_start + dp_mode->timing.v_back_porch);

	dp_mode->timing.v_front_porch = drm_mode->vsync_start -
					 drm_mode->vdisplay;

	dp_mode->timing.refresh_rate = drm_mode->vrefresh;

	dp_mode->timing.pixel_clk_khz = drm_mode->clock;

	dp_mode->timing.v_active_low =
		!!(drm_mode->flags & DRM_MODE_FLAG_NVSYNC);

	dp_mode->timing.h_active_low =
		!!(drm_mode->flags & DRM_MODE_FLAG_NHSYNC);

	dp_mode->timing.bpp =
		dp_panel->connector->display_info.bpc * num_components;
	if (!dp_mode->timing.bpp)
		dp_mode->timing.bpp = default_bpp;

	dp_mode->timing.bpp = dp_panel_get_mode_bpp(dp_panel,
			dp_mode->timing.bpp, dp_mode->timing.pixel_clk_khz);

	dp_mode->timing.widebus_en = dp_panel->widebus_en;
}

struct dp_panel *dp_panel_get(struct dp_panel_in *in)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_panel *dp_panel;
	struct sde_connector *sde_conn;

	if (!in->dev || !in->catalog || !in->aux ||
			!in->link || !in->connector) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	panel = devm_kzalloc(in->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel) {
		rc = -ENOMEM;
		goto error;
	}

	panel->dev = in->dev;
	panel->aux = in->aux;
	panel->catalog = in->catalog;
	panel->link = in->link;
	panel->parser = in->parser;

	dp_panel = &panel->dp_panel;
	dp_panel->max_bw_code = DP_LINK_BW_8_1;
	dp_panel->spd_enabled = true;
	memcpy(panel->spd_vendor_name, vendor_name, (sizeof(u8) * 8));
	memcpy(panel->spd_product_description, product_desc, (sizeof(u8) * 16));
	dp_panel->connector = in->connector;

	dp_panel->widebus_en = panel->parser->has_widebus;

	if (in->base_panel) {
		memcpy(dp_panel->dpcd, in->base_panel->dpcd,
				DP_RECEIVER_CAP_SIZE + 1);
		memcpy(&dp_panel->link_info, &in->base_panel->link_info,
				sizeof(dp_panel->link_info));
	}

	dp_panel->init = dp_panel_init_panel_info;
	dp_panel->deinit = dp_panel_deinit_panel_info;
	dp_panel->hw_cfg = dp_panel_hw_cfg;
	dp_panel->read_sink_caps = dp_panel_read_sink_caps;
	dp_panel->get_min_req_link_rate = dp_panel_get_min_req_link_rate;
	dp_panel->get_mode_bpp = dp_panel_get_mode_bpp;
	dp_panel->get_modes = dp_panel_get_modes;
	dp_panel->handle_sink_request = dp_panel_handle_sink_request;
	dp_panel->set_edid = dp_panel_set_edid;
	dp_panel->set_dpcd = dp_panel_set_dpcd;
	dp_panel->tpg_config = dp_panel_tpg_config;
	dp_panel->spd_config = dp_panel_spd_config;
	dp_panel->setup_hdr = dp_panel_setup_hdr;
	dp_panel->hdr_supported = dp_panel_hdr_supported;
	dp_panel->set_stream_info = dp_panel_set_stream_info;
	dp_panel->read_sink_status = dp_panel_read_sink_sts;
	dp_panel->update_edid = dp_panel_update_edid;
	dp_panel->read_mst_cap = dp_panel_read_mst_cap;
	dp_panel->convert_to_dp_mode = dp_panel_convert_to_dp_mode;

	sde_conn = to_sde_connector(dp_panel->connector);
	sde_conn->drv_panel = dp_panel;

	dp_panel_edid_register(panel);

	return dp_panel;
error:
	return ERR_PTR(rc);
}

void dp_panel_put(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;

	if (!dp_panel)
		return;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	dp_panel_edid_deregister(panel);
	devm_kfree(panel->dev, panel);
}
