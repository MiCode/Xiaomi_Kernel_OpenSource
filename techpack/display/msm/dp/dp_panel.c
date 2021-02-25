// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#include "dp_panel.h"
#include <linux/unistd.h>
#include <drm/drm_fixed.h>
#include "dp_debug.h"
#include <drm/drm_dsc.h>
#include "sde_dsc_helper.h"

#define DP_KHZ_TO_HZ 1000
#define DP_PANEL_DEFAULT_BPP 24
#define DP_MAX_DS_PORT_COUNT 1

#define DPRX_FEATURE_ENUMERATION_LIST 0x2210
#define DPRX_EXTENDED_DPCD_FIELD 0x2200
#define VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED BIT(3)
#define VSC_EXT_VESA_SDP_SUPPORTED BIT(4)
#define VSC_EXT_VESA_SDP_CHAINING_SUPPORTED BIT(5)

#define DP_COMPRESSION_RATIO_2_TO_1 2
#define DP_COMPRESSION_RATIO_3_TO_1 3
#define DP_COMPRESSION_RATIO_NONE 1

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

struct dp_dhdr_maxpkt_calc_input {
	u32 mdp_clk;
	u32 lclk;
	u32 pclk;
	u32 h_active;
	u32 nlanes;
	s64 mst_target_sc;
	bool mst_en;
	bool fec_en;
};

struct tu_algo_data {
	s64 lclk_fp;
	s64 pclk_fp;
	s64 lwidth;
	s64 lwidth_fp;
	s64 hbp_relative_to_pclk;
	s64 hbp_relative_to_pclk_fp;
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

/**
 * Mapper function which outputs colorimetry and dynamic range
 * to be used for a given colorspace value when the vsc sdp
 * packets are used to change the colorimetry.
 */
static void get_sdp_colorimetry_range(struct dp_panel_private *panel,
	u32 colorspace, u32 *colorimetry, u32 *dynamic_range)
{

	u32 cc;

	/*
	 * Some rules being used for assignment of dynamic
	 * range for colorimetry using SDP:
	 *
	 * 1) If compliance test is ongoing return sRGB with
	 *    CEA primaries
	 * 2) For BT2020 cases, dynamic range shall be CEA
	 * 3) For DCI-P3 cases, as per HW team dynamic range
	 *    shall be VESA for RGB and CEA for YUV content
	 *    Hence defaulting to RGB and picking VESA
	 * 4) Default shall be sRGB with VESA
	 */

	cc = panel->link->get_colorimetry_config(panel->link);

	if (cc) {
		*colorimetry = sRGB;
		*dynamic_range = CEA;
		return;
	}

	switch (colorspace) {
	case DRM_MODE_COLORIMETRY_BT2020_RGB:
		*colorimetry = ITU_R_BT_2020_RGB;
		*dynamic_range = CEA;
		break;
	case DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65:
	case DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER:
		*colorimetry = DCI_P3;
		*dynamic_range = VESA;
		break;
	default:
		*colorimetry = sRGB;
		*dynamic_range = VESA;
	}
}

/**
 * Mapper function which outputs colorimetry to be used for a
 * given colorspace value when misc field of MSA is used to
 * change the colorimetry. Currently only RGB formats have been
 * added. This API will be extended to YUV once its supported on DP.
 */
static u8 get_misc_colorimetry_val(struct dp_panel_private *panel,
	u32 colorspace)
{
	u8 colorimetry;
	u32 cc;

	cc = panel->link->get_colorimetry_config(panel->link);
	/*
	 * If there is a non-zero value then compliance test-case
	 * is going on, otherwise we can honor the colorspace setting
	 */
	if (cc)
		return cc;

	switch (colorspace) {
	case DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65:
	case DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER:
		colorimetry = 0x7;
		break;
	case DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED:
		colorimetry = 0x3;
		break;
	case DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT:
		colorimetry = 0xb;
		break;
	case DRM_MODE_COLORIMETRY_OPRGB:
		colorimetry = 0xc;
		break;
	default:
		colorimetry = 0;
	}

	return colorimetry;
}

static int _tu_param_compare(s64 a, s64 b)
{
	u32 a_int, a_frac, a_sign;
	u32 b_int, b_frac, b_sign;
	s64 a_temp, b_temp, minus_1;

	if (a == b)
		return 0;

	minus_1 = drm_fixp_from_fraction(-1, 1);

	a_int = (a >> 32) & 0x7FFFFFFF;
	a_frac = a & 0xFFFFFFFF;
	a_sign = (a >> 32) & 0x80000000 ? 1 : 0;

	b_int = (b >> 32) & 0x7FFFFFFF;
	b_frac = b & 0xFFFFFFFF;
	b_sign = (b >> 32) & 0x80000000 ? 1 : 0;

	if (a_sign > b_sign)
		return 2;
	else if (b_sign > a_sign)
		return 1;

	if (!a_sign && !b_sign) { /* positive */
		if (a > b)
			return 1;
		else
			return 2;
	} else { /* negative */
		a_temp = drm_fixp_mul(a, minus_1);
		b_temp = drm_fixp_mul(b, minus_1);

		if (a_temp > b_temp)
			return 2;
		else
			return 1;
	}
}

static void dp_panel_update_tu_timings(struct dp_tu_calc_input *in,
					struct tu_algo_data *tu)
{
	int nlanes = in->nlanes;
	int dsc_num_slices = in->num_of_dsc_slices;
	int dsc_num_bytes  = 0;
	int numerator;
	s64 pclk_dsc_fp;
	s64 dwidth_dsc_fp;
	s64 hbp_dsc_fp;
	s64 overhead_dsc;

	int tot_num_eoc_symbols = 0;
	int tot_num_hor_bytes   = 0;
	int tot_num_dummy_bytes = 0;
	int dwidth_dsc_bytes    = 0;
	int  eoc_bytes           = 0;

	s64 temp1_fp, temp2_fp, temp3_fp;

	tu->lclk_fp              = drm_fixp_from_fraction(in->lclk, 1);
	tu->pclk_fp              = drm_fixp_from_fraction(in->pclk_khz, 1000);
	tu->lwidth               = in->hactive;
	tu->hbp_relative_to_pclk = in->hporch;
	tu->nlanes               = in->nlanes;
	tu->bpp                  = in->bpp;
	tu->pixelEnc             = in->pixel_enc;
	tu->dsc_en               = in->dsc_en;
	tu->async_en             = in->async_en;
	tu->lwidth_fp            = drm_fixp_from_fraction(in->hactive, 1);
	tu->hbp_relative_to_pclk_fp = drm_fixp_from_fraction(in->hporch, 1);

	if (tu->pixelEnc == 420) {
		temp1_fp = drm_fixp_from_fraction(2, 1);
		tu->pclk_fp = drm_fixp_div(tu->pclk_fp, temp1_fp);
		tu->lwidth_fp = drm_fixp_div(tu->lwidth_fp, temp1_fp);
		tu->hbp_relative_to_pclk_fp =
				drm_fixp_div(tu->hbp_relative_to_pclk_fp, 2);
	}

	if (tu->pixelEnc == 422) {
		switch (tu->bpp) {
		case 24:
			tu->bpp = 16;
			tu->bpc = 8;
			break;
		case 30:
			tu->bpp = 20;
			tu->bpc = 10;
			break;
		default:
			tu->bpp = 16;
			tu->bpc = 8;
			break;
		}
	} else
		tu->bpc = tu->bpp/3;

	if (!in->dsc_en)
		goto fec_check;

	temp1_fp = drm_fixp_from_fraction(in->compress_ratio, 100);
	temp2_fp = drm_fixp_from_fraction(in->bpp, 1);
	temp3_fp = drm_fixp_div(temp2_fp, temp1_fp);
	temp2_fp = drm_fixp_mul(tu->lwidth_fp, temp3_fp);

	temp1_fp = drm_fixp_from_fraction(8, 1);
	temp3_fp = drm_fixp_div(temp2_fp, temp1_fp);

	numerator = drm_fixp2int(temp3_fp);

	dsc_num_bytes  = numerator / dsc_num_slices;
	eoc_bytes           = dsc_num_bytes % nlanes;
	tot_num_eoc_symbols = nlanes * dsc_num_slices;
	tot_num_hor_bytes   = dsc_num_bytes * dsc_num_slices;
	tot_num_dummy_bytes = (nlanes - eoc_bytes) * dsc_num_slices;

	if (dsc_num_bytes == 0)
		DP_DEBUG("incorrect no of bytes per slice=%d\n", dsc_num_bytes);

	dwidth_dsc_bytes = (tot_num_hor_bytes +
				tot_num_eoc_symbols +
				(eoc_bytes == 0 ? 0 : tot_num_dummy_bytes));
	overhead_dsc     = dwidth_dsc_bytes / tot_num_hor_bytes;

	dwidth_dsc_fp = drm_fixp_from_fraction(dwidth_dsc_bytes, 3);

	temp2_fp = drm_fixp_mul(tu->pclk_fp, dwidth_dsc_fp);
	temp1_fp = drm_fixp_div(temp2_fp, tu->lwidth_fp);
	pclk_dsc_fp = temp1_fp;

	temp1_fp = drm_fixp_div(pclk_dsc_fp, tu->pclk_fp);
	temp2_fp = drm_fixp_mul(tu->hbp_relative_to_pclk_fp, temp1_fp);
	hbp_dsc_fp = temp2_fp;

	/* output */
	tu->pclk_fp = pclk_dsc_fp;
	tu->lwidth_fp = dwidth_dsc_fp;
	tu->hbp_relative_to_pclk_fp = hbp_dsc_fp;

fec_check:
	if (in->fec_en) {
		temp1_fp = drm_fixp_from_fraction(976, 1000); /* 0.976 */
		tu->lclk_fp = drm_fixp_mul(tu->lclk_fp, temp1_fp);
	}
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
	temp2_fp = tu->lwidth_fp;
	temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);
	temp2_fp = drm_fixp_div(temp1_fp, tu->average_valid2_fp);
	tu->n_tus = drm_fixp2int(temp2_fp);
	if ((temp2_fp & 0xFFFFFFFF) > 0xFFFFF000)
		tu->n_tus += 1;

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
	temp2_fp = tu->lwidth_fp;
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

static void _dp_calc_boundary(struct tu_algo_data *tu)
{

	s64 temp1_fp = 0, temp2_fp = 0;

	do {
		tu->err_fp = drm_fixp_from_fraction(1000, 1);

		temp1_fp = drm_fixp_div(tu->lclk_fp, tu->pclk_fp);
		temp2_fp = drm_fixp_from_fraction(
				tu->delay_start_link_extra_pixclk, 1);
		temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);

		if (temp1_fp)
			tu->extra_buffer_margin =
				drm_fixp2int_ceil(temp1_fp);
		else
			tu->extra_buffer_margin = 0;

		temp1_fp = drm_fixp_from_fraction(tu->bpp, 8);
		temp1_fp = drm_fixp_mul(tu->lwidth_fp, temp1_fp);

		if (temp1_fp)
			tu->n_symbols = drm_fixp2int_ceil(temp1_fp);
		else
			tu->n_symbols = 0;

		for (tu->tu_size = 32; tu->tu_size <= 64; tu->tu_size++) {
			for (tu->i_upper_boundary_count = 1;
				tu->i_upper_boundary_count <= 15;
				tu->i_upper_boundary_count++) {
				for (tu->i_lower_boundary_count = 1;
					tu->i_lower_boundary_count <= 15;
					tu->i_lower_boundary_count++) {
					_tu_valid_boundary_calc(tu);
				}
			}
		}
		tu->delay_start_link_extra_pixclk--;
	} while (!tu->boundary_moderation_en &&
		tu->boundary_mod_lower_err == 1 &&
		tu->delay_start_link_extra_pixclk != 0);
}

static void _dp_calc_extra_bytes(struct tu_algo_data *tu)
{
	u64 temp = 0;
	s64 temp1_fp = 0, temp2_fp = 0;

	temp1_fp = drm_fixp_from_fraction(tu->tu_size_desired, 1);
	temp2_fp = drm_fixp_mul(tu->original_ratio_fp, temp1_fp);
	temp1_fp = drm_fixp_from_fraction(tu->valid_boundary_link, 1);
	temp2_fp = temp1_fp - temp2_fp;
	temp1_fp = drm_fixp_from_fraction(tu->n_tus + 1, 1);
	temp2_fp = drm_fixp_mul(temp1_fp, temp2_fp);

	temp = drm_fixp2int(temp2_fp);
	if (temp && temp2_fp)
		tu->extra_bytes = drm_fixp2int_ceil(temp2_fp);
	else
		tu->extra_bytes = 0;

	temp1_fp = drm_fixp_from_fraction(tu->extra_bytes, 1);
	temp2_fp = drm_fixp_from_fraction(8, tu->bpp);
	temp1_fp = drm_fixp_mul(temp1_fp, temp2_fp);

	if (temp1_fp)
		tu->extra_pclk_cycles = drm_fixp2int_ceil(temp1_fp);
	else
		tu->extra_pclk_cycles = drm_fixp2int(temp1_fp);

	temp1_fp = drm_fixp_div(tu->lclk_fp, tu->pclk_fp);
	temp2_fp = drm_fixp_from_fraction(tu->extra_pclk_cycles, 1);
	temp1_fp = drm_fixp_mul(temp2_fp, temp1_fp);

	if (temp1_fp)
		tu->extra_pclk_cycles_in_link_clk = drm_fixp2int_ceil(temp1_fp);
	else
		tu->extra_pclk_cycles_in_link_clk = drm_fixp2int(temp1_fp);
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

	dp_panel_update_tu_timings(in, &tu);

	tu.err_fp = drm_fixp_from_fraction(1000, 1); /* 1000 */

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
	temp1_fp = drm_fixp_from_fraction(tu.nlanes, 1);
	temp2_fp = tu.lwidth_fp % temp1_fp;
	if (temp2_fp != 0 &&
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
		DP_DEBUG("Info: increase HBLANK_MARGIN to %d\n", HBLANK_MARGIN);
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
	temp2_fp = tu.lwidth_fp;
	temp2_fp = drm_fixp_mul(temp2_fp, temp1_fp);

	temp1_fp = drm_fixp_from_fraction(tu.valid_boundary_link, 1);
	temp2_fp = drm_fixp_div(temp2_fp, temp1_fp);
	tu.n_tus = drm_fixp2int(temp2_fp);
	if ((temp2_fp & 0xFFFFFFFF) > 0xFFFFF000)
		tu.n_tus += 1;

	tu.even_distribution_legacy = tu.n_tus % tu.nlanes == 0 ? 1 : 0;
	DP_DEBUG("Info: n_sym = %d, num_of_tus = %d\n",
		tu.valid_boundary_link, tu.n_tus);

	_dp_calc_extra_bytes(&tu);

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

	temp1_fp = drm_fixp_from_fraction(HBLANK_MARGIN, 1);
	temp1_fp = tu.hbp_relative_to_pclk_fp - temp1_fp;
	tu.hbp_time_fp = drm_fixp_div(temp1_fp, tu.pclk_fp);

	temp1_fp = drm_fixp_from_fraction(tu.delay_start_link, 1);
	tu.delay_start_time_fp = drm_fixp_div(temp1_fp, tu.lclk_fp);

	compare_result_1 = _tu_param_compare(tu.hbp_time_fp,
					tu.delay_start_time_fp);
	if (compare_result_1 == 2) /* hbp_time_fp < delay_start_time_fp */
		tu.min_hblank_violated = 1;

	tu.hactive_time_fp = drm_fixp_div(tu.lwidth_fp, tu.pclk_fp);

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

		_dp_calc_boundary(&tu);

		if (tu.boundary_moderation_en) {
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
			temp1_fp = drm_fixp_mul(tu.lwidth_fp, temp1_fp);
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

	temp2_fp = drm_fixp_mul(LCLK_FAST_SKEW_fp, tu.lwidth_fp);

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

	DP_DEBUG("TU: valid_boundary_link: %d\n", tu_table->valid_boundary_link);
	DP_DEBUG("TU: delay_start_link: %d\n", tu_table->delay_start_link);
	DP_DEBUG("TU: boundary_moderation_en: %d\n",
			tu_table->boundary_moderation_en);
	DP_DEBUG("TU: valid_lower_boundary_link: %d\n",
			tu_table->valid_lower_boundary_link);
	DP_DEBUG("TU: upper_boundary_count: %d\n",
			tu_table->upper_boundary_count);
	DP_DEBUG("TU: lower_boundary_count: %d\n",
			tu_table->lower_boundary_count);
	DP_DEBUG("TU: tu_size_minus1: %d\n", tu_table->tu_size_minus1);
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
	in.dsc_en = dp_panel->dsc_en;
	in.async_en = 0;
	in.fec_en = dp_panel->fec_en;
	in.num_of_dsc_slices = pinfo->comp_info.dsc_info.slice_per_pkt;

	if (pinfo->comp_info.comp_ratio)
		in.compress_ratio = pinfo->comp_info.comp_ratio * 100;

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
		DP_ERR("invalid input\n");
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

	DP_DEBUG("dp_tu=0x%x, valid_boundary=0x%x, valid_boundary2=0x%x\n",
			dp_tu, valid_boundary, valid_boundary2);

	catalog->dp_tu = dp_tu;
	catalog->valid_boundary = valid_boundary;
	catalog->valid_boundary2 = valid_boundary2;

	catalog->update_transfer_unit(catalog);
}

static void dp_panel_get_dto_params(u8 comp_ratio, u32 *num, u32 *denom,
		u32 org_bpp)
{
	if ((comp_ratio == 2) && (org_bpp == 24)) {
		*num = 1;
		*denom = 2;
	} else if ((comp_ratio == 2) && (org_bpp == 30)) {
		*num = 5;
		*denom = 8;
	} else if ((comp_ratio == 3) && (org_bpp == 24)) {
		*num = 1;
		*denom = 3;
	} else if ((comp_ratio == 3) && (org_bpp == 30)) {
		*num = 5;
		*denom = 12;
	} else {
		DP_ERR("dto params not found\n");
		*num = 0;
		*denom = 1;
	}
}

static void dp_panel_dsc_prepare_pps_packet(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;
	struct dp_dsc_cfg_data *dsc;
	u8 *pps, *parity;
	u32 *pps_word, *parity_word;
	int i, index_4;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	dsc = &panel->catalog->dsc;
	pps = dsc->pps;
	pps_word = dsc->pps_word;
	parity = dsc->parity;
	parity_word = dsc->parity_word;

	memset(parity, 0, sizeof(dsc->parity));

	dsc->pps_word_len = dsc->pps_len >> 2;
	dsc->parity_len = dsc->pps_word_len;
	dsc->parity_word_len = (dsc->parity_len >> 2) + 1;

	for (i = 0; i < dsc->pps_word_len; i++) {
		index_4 = i << 2;
		pps_word[i] = pps[index_4 + 0] << 0 |
				pps[index_4 + 1] << 8 |
				pps[index_4 + 2] << 16 |
				pps[index_4 + 3] << 24;

		parity[i] = dp_header_get_parity(pps_word[i]);
	}

	for (i = 0; i < dsc->parity_word_len; i++) {
		index_4 = i << 2;
		parity_word[i] = parity[index_4 + 0] << 0 |
				   parity[index_4 + 1] << 8 |
				   parity[index_4 + 2] << 16 |
				   parity[index_4 + 3] << 24;
	}
}

static void _dp_panel_dsc_get_num_extra_pclk(struct msm_display_dsc_info *dsc,
				u8 ratio)
{
	unsigned int dto_n = 0, dto_d = 0, remainder;
	int ack_required, last_few_ack_required, accum_ack;
	int last_few_pclk, last_few_pclk_required;
	int start, temp, line_width = dsc->config.pic_width/2;
	s64 temp1_fp, temp2_fp;

	dp_panel_get_dto_params(ratio, &dto_n, &dto_d,
			dsc->config.bits_per_component * 3);

	ack_required = dsc->pclk_per_line;

	/* number of pclk cycles left outside of the complete DTO set */
	last_few_pclk = line_width % dto_d;

	/* number of pclk cycles outside of the complete dto */
	temp1_fp = drm_fixp_from_fraction(line_width, dto_d);
	temp2_fp = drm_fixp_from_fraction(dto_n, 1);
	temp1_fp = drm_fixp_mul(temp1_fp, temp2_fp);
	temp = drm_fixp2int(temp1_fp);
	last_few_ack_required = ack_required - temp;

	/*
	 * check how many more pclk is needed to
	 * accommodate the last few ack required
	 */
	remainder = dto_n;
	accum_ack = 0;
	last_few_pclk_required = 0;
	while (accum_ack < last_few_ack_required) {
		last_few_pclk_required++;

		if (remainder >= dto_n)
			start = remainder;
		else
			start = remainder + dto_d;

		remainder = start - dto_n;
		if (remainder < dto_n)
			accum_ack++;
	}

	/* if fewer pclk than required */
	if (last_few_pclk < last_few_pclk_required)
		dsc->extra_width = last_few_pclk_required - last_few_pclk;
	else
		dsc->extra_width = 0;

	DP_DEBUG("extra pclks required: %d\n", dsc->extra_width);
}

static void _dp_panel_dsc_bw_overhead_calc(struct dp_panel *dp_panel,
		struct msm_display_dsc_info *dsc,
		struct dp_display_mode *dp_mode, u32 dsc_byte_cnt)
{
	int num_slices, tot_num_eoc_symbols;
	int tot_num_hor_bytes, tot_num_dummy_bytes;
	int dwidth_dsc_bytes, eoc_bytes;
	u32 num_lanes;
	struct dp_panel_private *panel;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	num_lanes = panel->link->link_params.lane_count;
	num_slices = dsc->slice_per_pkt;

	eoc_bytes = dsc_byte_cnt % num_lanes;
	tot_num_eoc_symbols = num_lanes * num_slices;
	tot_num_hor_bytes = dsc_byte_cnt * num_slices;
	tot_num_dummy_bytes = (num_lanes - eoc_bytes) * num_slices;

	if (!eoc_bytes)
		tot_num_dummy_bytes = 0;

	dwidth_dsc_bytes = tot_num_hor_bytes + tot_num_eoc_symbols +
				tot_num_dummy_bytes;

	DP_DEBUG("dwidth_dsc_bytes:%d, tot_num_hor_bytes:%d\n",
			dwidth_dsc_bytes, tot_num_hor_bytes);

	dp_mode->dsc_overhead_fp = drm_fixp_from_fraction(dwidth_dsc_bytes,
			tot_num_hor_bytes);
	dp_mode->timing.dsc_overhead_fp = dp_mode->dsc_overhead_fp;
}

static void dp_panel_dsc_pclk_param_calc(struct dp_panel *dp_panel,
		struct msm_display_dsc_info *dsc,
		u8 ratio,
		struct dp_display_mode *dp_mode)
{
	int comp_ratio = 100, intf_width;
	int slice_per_pkt, slice_per_intf;
	s64 temp1_fp, temp2_fp;
	s64 numerator_fp, denominator_fp;
	s64 dsc_byte_count_fp;
	u32 dsc_byte_count, temp1, temp2;

	intf_width = dp_mode->timing.h_active;
	if (!dsc || !dsc->config.slice_width || !dsc->slice_per_pkt ||
			 (intf_width < dsc->config.slice_width))
		return;

	slice_per_pkt = dsc->slice_per_pkt;
	slice_per_intf = DIV_ROUND_UP(intf_width,
			dsc->config.slice_width);

	if (ratio)
		comp_ratio = ratio * 100;

	temp1_fp = drm_fixp_from_fraction(comp_ratio, 100);
	temp2_fp = drm_fixp_from_fraction(slice_per_pkt * 8, 1);
	denominator_fp = drm_fixp_mul(temp1_fp, temp2_fp);
	numerator_fp = drm_fixp_from_fraction(
			intf_width * dsc->config.bits_per_component * 3, 1);
	dsc_byte_count_fp = drm_fixp_div(numerator_fp, denominator_fp);
	dsc_byte_count = drm_fixp2int_ceil(dsc_byte_count_fp);

	temp1 = dsc_byte_count * slice_per_intf;
	temp2 = temp1;
	if (temp1 % 3 != 0)
		temp1 += 3 - (temp1 % 3);

	dsc->eol_byte_num = temp1 - temp2;

	temp1_fp = drm_fixp_from_fraction(slice_per_intf, 6);
	temp2_fp = drm_fixp_mul(dsc_byte_count_fp, temp1_fp);
	dsc->pclk_per_line = drm_fixp2int_ceil(temp2_fp);

	_dp_panel_dsc_get_num_extra_pclk(dsc, ratio);
	dsc->pclk_per_line--;

	_dp_panel_dsc_bw_overhead_calc(dp_panel, dsc, dp_mode, dsc_byte_count);
}

struct dp_dsc_slices_per_line {
	u32 min_ppr;
	u32 max_ppr;
	u8 num_slices;
};

struct dp_dsc_peak_throughput {
	u32 index;
	u32 peak_throughput;
};

struct dp_dsc_slice_caps_bit_map {
	u32 num_slices;
	u32 bit_index;
};

const struct dp_dsc_slices_per_line slice_per_line_tbl[] = {
	{0,     340,    1   },
	{340,   680,    2   },
	{680,   1360,   4   },
	{1360,  3200,   8   },
	{3200,  4800,   12  },
	{4800,  6400,   16  },
	{6400,  8000,   20  },
	{8000,  9600,   24  }
};

const struct dp_dsc_peak_throughput peak_throughput_mode_0_tbl[] = {
	{0, 0},
	{1, 340},
	{2, 400},
	{3, 450},
	{4, 500},
	{5, 550},
	{6, 600},
	{7, 650},
	{8, 700},
	{9, 750},
	{10, 800},
	{11, 850},
	{12, 900},
	{13, 950},
	{14, 1000},
};

const struct dp_dsc_slice_caps_bit_map slice_caps_bit_map_tbl[] = {
	{1, 0},
	{2, 1},
	{4, 3},
	{6, 4},
	{8, 5},
	{10, 6},
	{12, 7},
	{16, 0},
	{20, 1},
	{24, 2},
};

static bool dp_panel_check_slice_support(u32 num_slices, u32 raw_data_1,
		u32 raw_data_2)
{
	const struct dp_dsc_slice_caps_bit_map *bcap;
	u32 raw_data;
	int i;

	if (num_slices <= 12)
		raw_data = raw_data_1;
	else
		raw_data = raw_data_2;

	for (i = 0; i < ARRAY_SIZE(slice_caps_bit_map_tbl); i++) {
		bcap = &slice_caps_bit_map_tbl[i];

		if (bcap->num_slices == num_slices) {
			raw_data &= (1 << bcap->bit_index);

			if (raw_data)
				return true;
			else
				return false;
		}
	}

	return false;
}

static int dp_panel_dsc_prepare_basic_params(
		struct msm_compression_info *comp_info,
		const struct dp_display_mode *dp_mode,
		struct dp_panel *dp_panel)
{
	int i;
	const struct dp_dsc_slices_per_line *rec;
	const struct dp_dsc_peak_throughput *tput;
	u32 slice_width;
	u32 ppr = dp_mode->timing.pixel_clk_khz/1000;
	u32 max_slice_width;
	u32 ppr_max_index;
	u32 peak_throughput;
	u32 ppr_per_slice;
	u32 slice_caps_1;
	u32 slice_caps_2;

	comp_info->dsc_info.config.dsc_version_major = 0x1;
	comp_info->dsc_info.config.dsc_version_minor = 0x1;
	comp_info->dsc_info.scr_rev = 0x0;

	comp_info->dsc_info.slice_per_pkt = 0;
	for (i = 0; i < ARRAY_SIZE(slice_per_line_tbl); i++) {
		rec = &slice_per_line_tbl[i];
		if ((ppr > rec->min_ppr) && (ppr <= rec->max_ppr)) {
			comp_info->dsc_info.slice_per_pkt = rec->num_slices;
			i++;
			break;
		}
	}

	if (comp_info->dsc_info.slice_per_pkt == 0)
		return -EINVAL;

	ppr_max_index = dp_panel->dsc_dpcd[11] &= 0xf;
	if (!ppr_max_index || ppr_max_index >= 15) {
		DP_DEBUG("Throughput mode 0 not supported");
		return -EINVAL;
	}

	tput = &peak_throughput_mode_0_tbl[ppr_max_index];
	peak_throughput = tput->peak_throughput;

	max_slice_width = dp_panel->dsc_dpcd[12] * 320;
	slice_width = (dp_mode->timing.h_active /
				comp_info->dsc_info.slice_per_pkt);

	ppr_per_slice = ppr/comp_info->dsc_info.slice_per_pkt;

	slice_caps_1 = dp_panel->dsc_dpcd[4];
	slice_caps_2 = dp_panel->dsc_dpcd[13] & 0x7;

	/*
	 * There are 3 conditions to check for sink support:
	 * 1. The slice width cannot exceed the maximum.
	 * 2. The ppr per slice cannot exceed the maximum.
	 * 3. The number of slices must be explicitly supported.
	 */
	while (slice_width >= max_slice_width ||
			ppr_per_slice > peak_throughput ||
			!dp_panel_check_slice_support(
			comp_info->dsc_info.slice_per_pkt, slice_caps_1,
			slice_caps_2)) {
		if (i == ARRAY_SIZE(slice_per_line_tbl))
			return -EINVAL;

		rec = &slice_per_line_tbl[i];
		comp_info->dsc_info.slice_per_pkt = rec->num_slices;
		slice_width = (dp_mode->timing.h_active /
				comp_info->dsc_info.slice_per_pkt);
		ppr_per_slice = ppr/comp_info->dsc_info.slice_per_pkt;
		i++;
	}

	comp_info->dsc_info.config.block_pred_enable =
			dp_panel->sink_dsc_caps.block_pred_en;

	comp_info->dsc_info.config.pic_width = dp_mode->timing.h_active;
	comp_info->dsc_info.config.pic_height = dp_mode->timing.v_active;
	comp_info->dsc_info.config.slice_width = slice_width;

	if (comp_info->dsc_info.config.pic_height % 108 == 0)
		comp_info->dsc_info.config.slice_height = 108;
	else if (comp_info->dsc_info.config.pic_height % 16 == 0)
		comp_info->dsc_info.config.slice_height = 16;
	else if (comp_info->dsc_info.config.pic_height % 12 == 0)
		comp_info->dsc_info.config.slice_height = 12;
	else
		comp_info->dsc_info.config.slice_height = 15;

	comp_info->dsc_info.config.bits_per_component =
		(dp_mode->timing.bpp / 3);
	comp_info->dsc_info.config.bits_per_pixel =
		comp_info->dsc_info.config.bits_per_component << 4;
	comp_info->dsc_info.config.slice_count =
		DIV_ROUND_UP(dp_mode->timing.h_active, slice_width);

	comp_info->comp_type = MSM_DISPLAY_COMPRESSION_DSC;
	comp_info->comp_ratio = DP_COMPRESSION_RATIO_3_TO_1;
	return 0;
}

static int dp_panel_read_dpcd(struct dp_panel *dp_panel, bool multi_func)
{
	int rlen, rc = 0;
	struct dp_panel_private *panel;
	struct drm_dp_link *link_info;
	struct drm_dp_aux *drm_aux;
	u8 *dpcd, rx_feature, temp;
	u32 dfp_count = 0, offset = DP_DPCD_REV;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
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
		DP_DEBUG("skip dpcd read in debug mode\n");
		goto skip_dpcd_read;
	}

	rlen = drm_dp_dpcd_read(drm_aux, DP_TRAINING_AUX_RD_INTERVAL, &temp, 1);
	if (rlen != 1) {
		DP_ERR("error reading DP_TRAINING_AUX_RD_INTERVAL\n");
		rc = -EINVAL;
		goto end;
	}

	/* check for EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT */
	if (temp & BIT(7)) {
		DP_DEBUG("using EXTENDED_RECEIVER_CAPABILITY_FIELD\n");
		offset = DPRX_EXTENDED_DPCD_FIELD;
	}

	rlen = drm_dp_dpcd_read(drm_aux, offset,
		dp_panel->dpcd, (DP_RECEIVER_CAP_SIZE + 1));
	if (rlen < (DP_RECEIVER_CAP_SIZE + 1)) {
		DP_ERR("dpcd read failed, rlen=%d\n", rlen);
		if (rlen == -ETIMEDOUT)
			rc = rlen;
		else
			rc = -EINVAL;

		goto end;
	}

	print_hex_dump_debug("[drm-dp] SINK DPCD: ",
		DUMP_PREFIX_NONE, 8, 1, dp_panel->dpcd, rlen, false);

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux,
		DPRX_FEATURE_ENUMERATION_LIST, &rx_feature, 1);
	if (rlen != 1) {
		DP_DEBUG("failed to read DPRX_FEATURE_ENUMERATION_LIST\n");
		rx_feature = 0;
	}

skip_dpcd_read:
	if (panel->custom_dpcd)
		rx_feature = dp_panel->dpcd[DP_RECEIVER_CAP_SIZE + 1];

	panel->vsc_supported = !!(rx_feature &
		VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED);
	panel->vscext_supported = !!(rx_feature & VSC_EXT_VESA_SDP_SUPPORTED);
	panel->vscext_chaining_supported = !!(rx_feature &
			VSC_EXT_VESA_SDP_CHAINING_SUPPORTED);

	DP_DEBUG("vsc=%d, vscext=%d, vscext_chaining=%d\n",
		panel->vsc_supported, panel->vscext_supported,
		panel->vscext_chaining_supported);

	link_info->revision = dpcd[DP_DPCD_REV];
	panel->major = (link_info->revision >> 4) & 0x0f;
	panel->minor = link_info->revision & 0x0f;

	/* override link params updated in dp_panel_init_panel_info */
	link_info->rate = min_t(unsigned long, panel->parser->max_lclk_khz,
			drm_dp_bw_code_to_link_rate(dpcd[DP_MAX_LINK_RATE]));

	link_info->num_lanes = dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK;

	if (is_link_rate_valid(panel->dp_panel.link_bw_code)) {
		DP_DEBUG("debug link bandwidth code: 0x%x\n",
				panel->dp_panel.link_bw_code);
		link_info->rate = drm_dp_bw_code_to_link_rate(
				panel->dp_panel.link_bw_code);
	}

	if (is_lane_count_valid(panel->dp_panel.lane_count)) {
		DP_DEBUG("debug lane count: %d\n", panel->dp_panel.lane_count);
		link_info->num_lanes = panel->dp_panel.lane_count;
	}

	if (multi_func)
		link_info->num_lanes = min_t(unsigned int,
			link_info->num_lanes, 2);

	DP_DEBUG("version:%d.%d, rate:%d, lanes:%d\n", panel->major,
		panel->minor, link_info->rate, link_info->num_lanes);

	if (drm_dp_enhanced_frame_cap(dpcd))
		link_info->capabilities |= DP_LINK_CAP_ENHANCED_FRAMING;

	dfp_count = dpcd[DP_DOWN_STREAM_PORT_COUNT] &
						DP_DOWN_STREAM_PORT_COUNT;

	if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT)
		&& (dpcd[DP_DPCD_REV] > 0x10)) {
		rlen = drm_dp_dpcd_read(panel->aux->drm_aux,
			DP_DOWNSTREAM_PORT_0, dp_panel->ds_ports,
			DP_MAX_DOWNSTREAM_PORTS);
		if (rlen < DP_MAX_DOWNSTREAM_PORTS) {
			DP_ERR("ds port status failed, rlen=%d\n", rlen);
			rc = -EINVAL;
			goto end;
		}
	}

	if (dfp_count > DP_MAX_DS_PORT_COUNT)
		DP_DEBUG("DS port count %d greater that max (%d) supported\n",
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
		DP_ERR("invalid input\n");
		return -EINVAL;
	}
	link_info = &dp_panel->link_info;
	link_info->rate = default_bw_code;
	link_info->num_lanes = default_num_lanes;
	DP_DEBUG("link_rate=%d num_lanes=%d\n",
		link_info->rate, link_info->num_lanes);

	return 0;
}

static int dp_panel_set_edid(struct dp_panel *dp_panel, u8 *edid)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
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

	DP_DEBUG("%d\n", panel->custom_edid);
	return 0;
}

static int dp_panel_set_dpcd(struct dp_panel *dp_panel, u8 *dpcd)
{
	struct dp_panel_private *panel;
	u8 *dp_dpcd;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp_dpcd = dp_panel->dpcd;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dpcd) {
		memcpy(dp_dpcd, dpcd, DP_RECEIVER_CAP_SIZE +
				DP_RECEIVER_EXT_CAP_SIZE + 1);
		panel->custom_dpcd = true;
	} else {
		panel->custom_dpcd = false;
	}

	DP_DEBUG("%d\n", panel->custom_dpcd);

	return 0;
}

static int dp_panel_read_edid(struct dp_panel *dp_panel,
	struct drm_connector *connector)
{
	int ret = 0;
	struct dp_panel_private *panel;
	struct edid *edid;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (panel->custom_edid) {
		DP_DEBUG("skip edid read in debug mode\n");
		goto end;
	}

	sde_get_edid(connector, &panel->aux->drm_aux->ddc,
		(void **)&dp_panel->edid_ctrl);
	if (!dp_panel->edid_ctrl->edid) {
		DP_ERR("EDID read failed\n");
		ret = -EINVAL;
		goto end;
	}
end:
	edid = dp_panel->edid_ctrl->edid;
	dp_panel->audio_supported = drm_detect_monitor_audio(edid);

	return ret;
}

static void dp_panel_decode_dsc_dpcd(struct dp_panel *dp_panel)
{
	if (dp_panel->dsc_dpcd[0]) {
		dp_panel->sink_dsc_caps.dsc_capable = true;
		dp_panel->sink_dsc_caps.version = dp_panel->dsc_dpcd[1];
		dp_panel->sink_dsc_caps.block_pred_en =
				dp_panel->dsc_dpcd[6] ? true : false;
		dp_panel->sink_dsc_caps.color_depth =
				dp_panel->dsc_dpcd[10];

		if (dp_panel->sink_dsc_caps.version >= 0x11)
			dp_panel->dsc_en = true;
	} else {
		dp_panel->sink_dsc_caps.dsc_capable = false;
		dp_panel->dsc_en = false;
	}
}

static void dp_panel_read_sink_dsc_caps(struct dp_panel *dp_panel)
{
	int rlen;
	struct dp_panel_private *panel;
	int dpcd_rev;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		return;
	}

	dpcd_rev = dp_panel->dpcd[DP_DPCD_REV];

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	if (panel->parser->dsc_feature_enable && dpcd_rev >= 0x14) {
		rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_DSC_SUPPORT,
			dp_panel->dsc_dpcd, (DP_RECEIVER_DSC_CAP_SIZE + 1));
		if (rlen < (DP_RECEIVER_DSC_CAP_SIZE + 1)) {
			DP_DEBUG("dsc dpcd read failed, rlen=%d\n", rlen);
			return;
		}

		print_hex_dump_debug("[drm-dp] SINK DSC DPCD: ",
			DUMP_PREFIX_NONE, 8, 1, dp_panel->dsc_dpcd, rlen,
			false);

		dp_panel_decode_dsc_dpcd(dp_panel);
	}
}

static void dp_panel_read_sink_fec_caps(struct dp_panel *dp_panel)
{
	int rlen;
	struct dp_panel_private *panel;
	s64 fec_overhead_fp = drm_fixp_from_fraction(1, 1);

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		return;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	rlen = drm_dp_dpcd_readb(panel->aux->drm_aux, DP_FEC_CAPABILITY,
			&dp_panel->fec_dpcd);
	if (rlen < 1) {
		DP_ERR("fec capability read failed, rlen=%d\n", rlen);
		return;
	}

	dp_panel->fec_en = dp_panel->fec_dpcd & DP_FEC_CAPABLE;
	if (dp_panel->fec_en)
		fec_overhead_fp = drm_fixp_from_fraction(100000, 97582);

	dp_panel->fec_overhead_fp = fec_overhead_fp;

	return;
}

static int dp_panel_read_sink_caps(struct dp_panel *dp_panel,
	struct drm_connector *connector, bool multi_func)
{
	int rc = 0, rlen, count, downstream_ports;
	const int count_len = 1;
	struct dp_panel_private *panel;

	if (!dp_panel || !connector) {
		DP_ERR("invalid input\n");
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
			DP_ERR("DPCD read failed, return early\n");
			goto end;
		}
		DP_ERR("panel dpcd read failed/incorrect, set default params\n");
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
				DP_ERR("no downstream ports connected\n");
				panel->link->sink_count.count = 0;
				rc = -ENOTCONN;
				goto end;
			}
		}
	}

	/* There is no need to read EDID from MST branch */
	if (panel->parser->has_mst && dp_panel->read_mst_cap(dp_panel))
		goto skip_edid;

	rc = dp_panel_read_edid(dp_panel, connector);
	if (rc) {
		DP_ERR("panel edid read failed, set failsafe mode\n");
		return rc;
	}

skip_edid:
	dp_panel->widebus_en = panel->parser->has_widebus;
	dp_panel->dsc_feature_enable = panel->parser->dsc_feature_enable;
	dp_panel->fec_feature_enable = panel->parser->fec_feature_enable;

	dp_panel->fec_en = false;
	dp_panel->dsc_en = false;

	if (dp_panel->dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14 &&
			dp_panel->fec_feature_enable) {
		dp_panel_read_sink_fec_caps(dp_panel);

		if (dp_panel->dsc_feature_enable && dp_panel->fec_en)
			dp_panel_read_sink_dsc_caps(dp_panel);
	}

	DP_INFO("fec_en=%d, dsc_en=%d, widebus_en=%d\n", dp_panel->fec_en,
			dp_panel->dsc_en, dp_panel->widebus_en);
end:
	return rc;
}

static u32 dp_panel_get_supported_bpp(struct dp_panel *dp_panel,
		u32 mode_edid_bpp, u32 mode_pclk_khz)
{
	struct dp_link_params *link_params;
	struct dp_panel_private *panel;
	const u32 max_supported_bpp = 30;
	u32 min_supported_bpp = 18;
	u32 bpp = 0, data_rate_khz = 0;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dp_panel->dsc_en)
		min_supported_bpp = 24;

	bpp = min_t(u32, mode_edid_bpp, max_supported_bpp);

	link_params = &panel->link->link_params;

	data_rate_khz = link_params->lane_count *
		drm_dp_bw_code_to_link_rate(link_params->bw_code) * 8;

	for (; bpp > min_supported_bpp; bpp -= 6) {
		if (dp_panel->dsc_en) {
			if (bpp == 36 && !(dp_panel->sink_dsc_caps.color_depth
					& DP_DSC_12_BPC))
				continue;
			else if (bpp == 30 &&
					!(dp_panel->sink_dsc_caps.color_depth &
					DP_DSC_10_BPC))
				continue;
			else if (bpp == 24 &&
					!(dp_panel->sink_dsc_caps.color_depth &
					DP_DSC_8_BPC))
				continue;
		}

		if (mode_pclk_khz * bpp <= data_rate_khz)
			break;
	}

	if (bpp < min_supported_bpp)
		DP_ERR("bpp %d is below minimum supported bpp %d\n", bpp,
				min_supported_bpp);
	if (dp_panel->dsc_en && bpp != 24 && bpp != 30 && bpp != 36)
		DP_ERR("bpp %d is not supported when dsc is enabled\n", bpp);

	return bpp;
}

static u32 dp_panel_get_mode_bpp(struct dp_panel *dp_panel,
		u32 mode_edid_bpp, u32 mode_pclk_khz)
{
	struct dp_panel_private *panel;
	u32 bpp = mode_edid_bpp;

	if (!dp_panel || !mode_edid_bpp || !mode_pclk_khz) {
		DP_ERR("invalid input\n");
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
		DP_ERR("invalid params\n");
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
		DP_ERR("invalid input\n");
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
		DP_ERR("invalid input\n");
		return;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (panel->link->sink_request & DP_TEST_LINK_EDID_READ) {
		u8 checksum;

		if (dp_panel->edid_ctrl->edid)
			checksum = sde_get_edid_checksum(dp_panel->edid_ctrl);
		else
			checksum = dp_panel->connector->real_edid_checksum;

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
		DP_ERR("invalid input\n");
		return;
	}

	if (dp_panel->stream_id >= DP_STREAM_MAX) {
		DP_ERR("invalid stream id:%d\n", dp_panel->stream_id);
		return;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	pinfo = &panel->dp_panel.pinfo;

	if (!panel->panel_on) {
		DP_DEBUG("DP panel not enabled, handle TPG on next panel on\n");
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
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	pinfo = &panel->dp_panel.pinfo;

	DP_DEBUG("width=%d hporch= %d %d %d\n",
		pinfo->h_active, pinfo->h_back_porch,
		pinfo->h_front_porch, pinfo->h_sync_width);

	DP_DEBUG("height=%d vporch= %d %d %d\n",
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

static u32 _dp_panel_calc_be_in_lane(struct dp_panel *dp_panel)
{
	struct dp_panel_info *pinfo;
	struct msm_compression_info *comp_info;
	u32 dsc_htot_byte_cnt, mod_result;
	u32 numerator, denominator;
	s64 temp_fp;
	u32 be_in_lane = 10;

	pinfo = &dp_panel->pinfo;
	comp_info = &pinfo->comp_info;

	if (!dp_panel->mst_state)
		return be_in_lane;

	if (pinfo->comp_info.comp_ratio == DP_COMPRESSION_RATIO_2_TO_1)
		denominator = 16; /* 2 * bits-in-byte */
	else if (pinfo->comp_info.comp_ratio == DP_COMPRESSION_RATIO_3_TO_1)
		denominator = 24; /* 3 * bits-in-byte */
	else
		denominator = 8;

	numerator = (pinfo->h_active + pinfo->h_back_porch +
				pinfo->h_front_porch + pinfo->h_sync_width) *
				pinfo->bpp;
	temp_fp = drm_fixp_from_fraction(numerator, denominator);
	dsc_htot_byte_cnt = drm_fixp2int_ceil(temp_fp);

	mod_result = dsc_htot_byte_cnt % 12;
	if (mod_result == 0)
		be_in_lane = 8;
	else if (mod_result <= 3)
		be_in_lane = 1;
	else if (mod_result <= 6)
		be_in_lane = 2;
	else if (mod_result <= 9)
		be_in_lane = 4;
	else if (mod_result <= 11)
		be_in_lane = 8;
	else
		be_in_lane = 10;

	return be_in_lane;
}

static void dp_panel_config_dsc(struct dp_panel *dp_panel, bool enable)
{
	struct dp_catalog_panel *catalog;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;
	struct msm_compression_info *comp_info;
	struct dp_dsc_cfg_data *dsc;
	int rc;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	catalog = panel->catalog;
	dsc = &catalog->dsc;
	pinfo = &dp_panel->pinfo;
	comp_info = &pinfo->comp_info;

	if (comp_info->comp_type == MSM_DISPLAY_COMPRESSION_DSC && enable) {
		rc = sde_dsc_create_pps_buf_cmd(&comp_info->dsc_info,
				dsc->pps, 0, sizeof(dsc->pps));
		if (rc) {
			DP_ERR("failed to create pps cmd %d\n", rc);
			return;
		}
		dsc->pps_len = DSC_1_1_PPS_PARAMETER_SET_ELEMENTS;
		dp_panel_dsc_prepare_pps_packet(dp_panel);

		dsc->slice_per_pkt = comp_info->dsc_info.slice_per_pkt - 1;
		dsc->bytes_per_pkt = comp_info->dsc_info.bytes_per_pkt;
		dsc->bytes_per_pkt /= comp_info->dsc_info.slice_per_pkt;
		dsc->eol_byte_num = comp_info->dsc_info.eol_byte_num;
		dsc->dto_count = comp_info->dsc_info.pclk_per_line;
		dsc->be_in_lane = _dp_panel_calc_be_in_lane(dp_panel);
		dsc->dsc_en = true;
		dsc->dto_en = true;

		dp_panel_get_dto_params(comp_info->comp_ratio, &dsc->dto_n,
				&dsc->dto_d, pinfo->bpp);
	} else {
		dsc->dsc_en = false;
		dsc->dto_en = false;
		dsc->dto_n = 0;
		dsc->dto_d = 0;
	}

	catalog->stream_id = dp_panel->stream_id;
	catalog->dsc_cfg(catalog);

	if (catalog->dsc.dsc_en && enable)
		catalog->pps_flush(catalog);
}

static int dp_panel_edid_register(struct dp_panel_private *panel)
{
	int rc = 0;

	panel->dp_panel.edid_ctrl = sde_edid_init();
	if (!panel->dp_panel.edid_ctrl) {
		DP_ERR("sde edid init for DP failed\n");
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
			u32 ch_tot_slots, u32 pbn, int vcpi)
{
	if (!dp_panel || stream_id > DP_STREAM_MAX) {
		DP_ERR("invalid input. stream_id: %d\n", stream_id);
		return -EINVAL;
	}

	dp_panel->vcpi = vcpi;
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
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	pinfo = &dp_panel->pinfo;

	drm_dp_dpcd_writeb(panel->aux->drm_aux, DP_SET_POWER, DP_SET_POWER_D3);
	/* 200us propagation time for the power down to take effect */
	usleep_range(200, 205);
	drm_dp_dpcd_writeb(panel->aux->drm_aux, DP_SET_POWER, DP_SET_POWER_D0);

	/*
	* According to the DP 1.1 specification, a "Sink Device must exit the
	* power saving state within 1 ms" (Section 2.5.3.1, Table 5-52, "Sink
	* Control Field" (register 0x600).
	*/
	usleep_range(1000, 2000);
end:
	return rc;
}

static int dp_panel_deinit_panel_info(struct dp_panel *dp_panel, u32 flags)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct drm_msm_ext_hdr_metadata *hdr_meta;
	struct dp_sdp_header *dhdr_vsif_sdp;
	struct sde_connector *sde_conn;
	struct dp_sdp_header *shdr_if_sdp;
	struct dp_catalog_vsc_sdp_colorimetry *vsc_colorimetry;
	struct drm_connector *connector;
	struct sde_connector_state *c_state;

	if (flags & DP_PANEL_SRC_INITIATED_POWER_DOWN) {
		DP_DEBUG("retain states in src initiated power down request\n");
		return 0;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	hdr_meta = &panel->catalog->hdr_meta;
	dhdr_vsif_sdp = &panel->catalog->dhdr_vsif_sdp;
	shdr_if_sdp = &panel->catalog->shdr_if_sdp;
	vsc_colorimetry = &panel->catalog->vsc_colorimetry;

	if (!panel->custom_edid && dp_panel->edid_ctrl->edid)
		sde_free_edid((void **)&dp_panel->edid_ctrl);

	dp_panel_set_stream_info(dp_panel, DP_STREAM_MAX, 0, 0, 0, 0);
	memset(&dp_panel->pinfo, 0, sizeof(dp_panel->pinfo));
	memset(hdr_meta, 0, sizeof(struct drm_msm_ext_hdr_metadata));
	memset(dhdr_vsif_sdp, 0, sizeof(struct dp_sdp_header));
	memset(shdr_if_sdp, 0, sizeof(struct dp_sdp_header));
	memset(vsc_colorimetry, 0,
		sizeof(struct dp_catalog_vsc_sdp_colorimetry));

	panel->panel_on = false;

	connector = dp_panel->connector;
	sde_conn = to_sde_connector(connector);
	c_state = to_sde_connector_state(connector->state);

	sde_conn->hdr_eotf = 0;
	sde_conn->hdr_metadata_type_one = 0;
	sde_conn->hdr_max_luminance = 0;
	sde_conn->hdr_avg_luminance = 0;
	sde_conn->hdr_min_luminance = 0;
	sde_conn->hdr_supported = false;
	sde_conn->hdr_plus_app_ver = 0;

	sde_conn->colorspace_updated = false;

	memset(&c_state->hdr_meta, 0, sizeof(c_state->hdr_meta));
	memset(&c_state->dyn_hdr_meta, 0, sizeof(c_state->dyn_hdr_meta));

	dp_panel->link_bw_code = 0;
	dp_panel->lane_count = 0;

	return rc;
}

static bool dp_panel_hdr_supported(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		return false;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	return panel->major >= 1 && panel->vsc_supported &&
		(panel->minor >= 4 || panel->vscext_supported);
}

static u32 dp_panel_calc_dhdr_pkt_limit(struct dp_panel *dp_panel,
		struct dp_dhdr_maxpkt_calc_input *input)
{
	s64 mdpclk_fp = drm_fixp_from_fraction(input->mdp_clk, 1000000);
	s64 lclk_fp = drm_fixp_from_fraction(input->lclk, 1000);
	s64 pclk_fp = drm_fixp_from_fraction(input->pclk, 1000);
	s64 nlanes_fp = drm_int2fixp(input->nlanes);
	s64 target_sc = input->mst_target_sc;
	s64 hactive_fp = drm_int2fixp(input->h_active);
	const s64 i1_fp = DRM_FIXED_ONE;
	const s64 i2_fp = drm_int2fixp(2);
	const s64 i10_fp = drm_int2fixp(10);
	const s64 i56_fp = drm_int2fixp(56);
	const s64 i64_fp = drm_int2fixp(64);
	s64 mst_bw_fp = i1_fp;
	s64 fec_factor_fp = i1_fp;
	s64 mst_bw64_fp, mst_bw64_ceil_fp, nlanes56_fp;
	u32 f1, f2, f3, f4, f5, deploy_period, target_period;
	s64 f3_f5_slot_fp;
	u32 calc_pkt_limit;
	const u32 max_pkt_limit = 64;

	if (input->fec_en && input->mst_en)
		fec_factor_fp = drm_fixp_from_fraction(64000, 65537);

	if (input->mst_en)
		mst_bw_fp = drm_fixp_div(target_sc, i64_fp);

	f1 = drm_fixp2int_ceil(drm_fixp_div(drm_fixp_mul(i10_fp, lclk_fp),
			mdpclk_fp));
	f2 = drm_fixp2int_ceil(drm_fixp_div(drm_fixp_mul(i2_fp, lclk_fp),
			mdpclk_fp)) + drm_fixp2int_ceil(drm_fixp_div(
			drm_fixp_mul(i1_fp, lclk_fp), mdpclk_fp));

	mst_bw64_fp = drm_fixp_mul(mst_bw_fp, i64_fp);
	if (drm_fixp2int(mst_bw64_fp) == 0)
		f3_f5_slot_fp = drm_fixp_div(i1_fp, drm_int2fixp(
				drm_fixp2int_ceil(drm_fixp_div(
				i1_fp, mst_bw64_fp))));
	else
		f3_f5_slot_fp = drm_int2fixp(drm_fixp2int(mst_bw_fp));

	mst_bw64_ceil_fp = drm_int2fixp(drm_fixp2int_ceil(mst_bw64_fp));
	f3 = drm_fixp2int(drm_fixp_mul(drm_int2fixp(drm_fixp2int(
				drm_fixp_div(i2_fp, f3_f5_slot_fp)) + 1),
				(i64_fp - mst_bw64_ceil_fp))) + 2;

	if (!input->mst_en) {
		f4 = 1 + drm_fixp2int(drm_fixp_div(drm_int2fixp(50),
				nlanes_fp)) + drm_fixp2int(drm_fixp_div(
				nlanes_fp, i2_fp));
		f5 = 0;
	} else {
		f4 = 0;
		nlanes56_fp = drm_fixp_div(i56_fp, nlanes_fp);
		f5 = drm_fixp2int(drm_fixp_mul(drm_int2fixp(drm_fixp2int(
				drm_fixp_div(i1_fp + nlanes56_fp,
				f3_f5_slot_fp)) + 1), (i64_fp -
				mst_bw64_ceil_fp + i1_fp + nlanes56_fp)));
	}

	deploy_period = f1 + f2 + f3 + f4 + f5 + 19;
	target_period = drm_fixp2int(drm_fixp_mul(fec_factor_fp, drm_fixp_mul(
			hactive_fp, drm_fixp_div(lclk_fp, pclk_fp))));

	calc_pkt_limit = target_period / deploy_period;

	DP_DEBUG("input: %d, %d, %d, %d, %d, 0x%llx, %d, %d\n",
		input->mdp_clk, input->lclk, input->pclk, input->h_active,
		input->nlanes, input->mst_target_sc, input->mst_en ? 1 : 0,
		input->fec_en ? 1 : 0);
	DP_DEBUG("factors: %d, %d, %d, %d, %d\n", f1, f2, f3, f4, f5);
	DP_DEBUG("d_p: %d, t_p: %d, maxPkts: %d%s\n", deploy_period,
		target_period, calc_pkt_limit, calc_pkt_limit > max_pkt_limit ?
		" CAPPED" : "");

	if (calc_pkt_limit > max_pkt_limit)
		calc_pkt_limit = max_pkt_limit;

	DP_DEBUG("packet limit per line = %d\n", calc_pkt_limit);
	return calc_pkt_limit;
}

static void dp_panel_setup_colorimetry_sdp(struct dp_panel *dp_panel,
	u32 cspace)
{
	struct dp_panel_private *panel;
	struct dp_catalog_vsc_sdp_colorimetry *hdr_colorimetry;
	u8 bpc;
	u32 colorimetry = 0;
	u32 dynamic_range = 0;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	hdr_colorimetry = &panel->catalog->vsc_colorimetry;

	hdr_colorimetry->header.HB0 = 0x00;
	hdr_colorimetry->header.HB1 = 0x07;
	hdr_colorimetry->header.HB2 = 0x05;
	hdr_colorimetry->header.HB3 = 0x13;

	get_sdp_colorimetry_range(panel, cspace, &colorimetry,
		&dynamic_range);

	/* VSC SDP Payload for DB16 */
	hdr_colorimetry->data[16] = (RGB << 4) | colorimetry;

	/* VSC SDP Payload for DB17 */
	hdr_colorimetry->data[17] = (dynamic_range << 7);
	bpc = (dp_panel->pinfo.bpp / 3);

	switch (bpc) {
	default:
	case 10:
		hdr_colorimetry->data[17] |= BIT(1);
		break;
	case 8:
		hdr_colorimetry->data[17] |= BIT(0);
		break;
	case 6:
		hdr_colorimetry->data[17] |= 0;
		break;
	}

	/* VSC SDP Payload for DB18 */
	hdr_colorimetry->data[18] = GRAPHICS;
}

static void dp_panel_setup_hdr_if(struct dp_panel_private *panel)
{
	struct dp_sdp_header *shdr_if;

	shdr_if = &panel->catalog->shdr_if_sdp;

	shdr_if->HB0 = 0x00;
	shdr_if->HB1 = 0x87;
	shdr_if->HB2 = 0x1D;
	shdr_if->HB3 = 0x13 << 2;
}

static void dp_panel_setup_dhdr_vsif(struct dp_panel_private *panel)
{
	struct dp_sdp_header *dhdr_vsif;

	dhdr_vsif = &panel->catalog->dhdr_vsif_sdp;

	dhdr_vsif->HB0 = 0x00;
	dhdr_vsif->HB1 = 0x81;
	dhdr_vsif->HB2 = 0x1D;
	dhdr_vsif->HB3 = 0x13 << 2;
}

static void dp_panel_setup_misc_colorimetry(struct dp_panel *dp_panel,
	u32 colorspace)
{
	struct dp_panel_private *panel;
	struct dp_catalog_panel *catalog;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;

	catalog->misc_val &= ~0x1e;

	catalog->misc_val |= (get_misc_colorimetry_val(panel,
		colorspace) << 1);
}

static int dp_panel_set_colorspace(struct dp_panel *dp_panel,
	u32 colorspace)
{
	int rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (panel->vsc_supported)
		dp_panel_setup_colorimetry_sdp(dp_panel,
			colorspace);
	else
		dp_panel_setup_misc_colorimetry(dp_panel,
			colorspace);

	/*
	 * During the first frame update panel_on will be false and
	 * the colorspace will be cached in the connector's state which
	 * shall be used in the dp_panel_hw_cfg
	 */
	if (panel->panel_on) {
		DP_DEBUG("panel is ON programming colorspace\n");
		rc =  panel->catalog->set_colorspace(panel->catalog,
			  panel->vsc_supported);
	}

end:
	return rc;
}

static int dp_panel_setup_hdr(struct dp_panel *dp_panel,
		struct drm_msm_ext_hdr_metadata *hdr_meta,
		bool dhdr_update, u64 core_clk_rate, bool flush)
{
	int rc = 0, max_pkts = 0;
	struct dp_panel_private *panel;
	struct dp_dhdr_maxpkt_calc_input input;
	struct drm_msm_ext_hdr_metadata *catalog_hdr_meta;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	catalog_hdr_meta = &panel->catalog->hdr_meta;

	/* use cached meta data in case meta data not provided */
	if (!hdr_meta) {
		if (catalog_hdr_meta->hdr_state)
			goto cached;
		else
			goto end;
	}

	panel->hdr_state = hdr_meta->hdr_state;

	dp_panel_setup_hdr_if(panel);

	if (panel->hdr_state) {
		memcpy(catalog_hdr_meta, hdr_meta,
			   sizeof(struct drm_msm_ext_hdr_metadata));
	} else {
		memset(catalog_hdr_meta, 0,
			sizeof(struct drm_msm_ext_hdr_metadata));
	}
cached:
	if (dhdr_update) {
		dp_panel_setup_dhdr_vsif(panel);

		input.mdp_clk = core_clk_rate;
		input.lclk = drm_dp_bw_code_to_link_rate(
				panel->link->link_params.bw_code);
		input.nlanes = panel->link->link_params.lane_count;
		input.pclk = dp_panel->pinfo.pixel_clk_khz;
		input.h_active = dp_panel->pinfo.h_active;
		input.mst_target_sc = dp_panel->mst_target_sc;
		input.mst_en = dp_panel->mst_state;
		input.fec_en = dp_panel->fec_en;
		max_pkts = dp_panel_calc_dhdr_pkt_limit(dp_panel, &input);
	}

	if (panel->panel_on) {
		panel->catalog->stream_id = dp_panel->stream_id;
		panel->catalog->config_hdr(panel->catalog, panel->hdr_state,
			max_pkts, flush);
		if (dhdr_update)
			panel->catalog->dhdr_flush(panel->catalog);
	}
end:
	return rc;
}

static int dp_panel_spd_config(struct dp_panel *dp_panel)
{
	int rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	if (dp_panel->stream_id >= DP_STREAM_MAX) {
		DP_ERR("invalid stream id:%d\n", dp_panel->stream_id);
		return -EINVAL;
	}

	if (!dp_panel->spd_enabled) {
		DP_DEBUG("SPD Infoframe not enabled\n");
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

	tbd = panel->link->get_test_bits_depth(panel->link,
			dp_panel->pinfo.bpp);

	if (tbd == DP_TEST_BIT_DEPTH_UNKNOWN || dp_panel->dsc_en)
		tbd = (DP_TEST_BIT_DEPTH_8 >> DP_TEST_BIT_DEPTH_SHIFT);

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
	struct drm_connector *connector;
	u32 misc_val;
	u32 tb, cc, colorspace;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	connector = dp_panel->connector;
	cc = 0;

	tb = panel->link->get_test_bits_depth(panel->link, dp_panel->pinfo.bpp);
	colorspace = connector->state->colorspace;


	cc = (get_misc_colorimetry_val(panel, colorspace) << 1);

	misc_val = cc;
	misc_val |= (tb << 5);
	misc_val |= BIT(0); /* Configure clock to synchronous mode */

	/* if VSC is supported then set bit 6 of MISC1 */
	if (panel->vsc_supported)
		misc_val |= BIT(14);

	catalog->misc_val = misc_val;
	catalog->config_misc(catalog);
}

static void dp_panel_config_msa(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;
	struct dp_catalog_panel *catalog;
	u32 rate;
	u32 stream_rate_khz;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;

	catalog->widebus_en = dp_panel->widebus_en;

	rate = drm_dp_bw_code_to_link_rate(panel->link->link_params.bw_code);
	stream_rate_khz = dp_panel->pinfo.pixel_clk_khz;

	catalog->config_msa(catalog, rate, stream_rate_khz);
}

static void dp_panel_resolution_info(struct dp_panel_private *panel)
{
	struct dp_panel_info *pinfo = &panel->dp_panel.pinfo;

	/*
	 * print resolution info as this is a result
	 * of user initiated action of cable connection
	 */
	DP_INFO("DP RESOLUTION: active(back|front|width|low)\n");
	DP_INFO("%d(%d|%d|%d|%d)x%d(%d|%d|%d|%d)@%dfps %dbpp %dKhz %dLR %dLn\n",
		pinfo->h_active, pinfo->h_back_porch, pinfo->h_front_porch,
		pinfo->h_sync_width, pinfo->h_active_low,
		pinfo->v_active, pinfo->v_back_porch, pinfo->v_front_porch,
		pinfo->v_sync_width, pinfo->v_active_low,
		pinfo->refresh_rate, pinfo->bpp, pinfo->pixel_clk_khz,
		panel->link->link_params.bw_code,
		panel->link->link_params.lane_count);
}

static void dp_panel_config_sdp(struct dp_panel *dp_panel,
	bool en)
{
	struct dp_panel_private *panel;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	panel->catalog->stream_id = dp_panel->stream_id;

	panel->catalog->config_sdp(panel->catalog, en);
}

static int dp_panel_hw_cfg(struct dp_panel *dp_panel, bool enable)
{
	struct dp_panel_private *panel;
	struct drm_connector *connector;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	if (dp_panel->stream_id >= DP_STREAM_MAX) {
		DP_ERR("invalid stream_id: %d\n", dp_panel->stream_id);
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	panel->catalog->stream_id = dp_panel->stream_id;
	connector = dp_panel->connector;

	if (enable) {
		dp_panel_config_ctrl(dp_panel);
		dp_panel_config_misc(dp_panel);
		dp_panel_config_msa(dp_panel);
		if (panel->vsc_supported) {
			dp_panel_setup_colorimetry_sdp(dp_panel,
				connector->state->colorspace);
			dp_panel_config_sdp(dp_panel, true);
		}
		dp_panel_config_dsc(dp_panel, enable);
		dp_panel_config_tr_unit(dp_panel);
		dp_panel_config_timing(dp_panel);
		dp_panel_resolution_info(panel);
	} else {
		dp_panel_config_sdp(dp_panel, false);
	}

	panel->catalog->config_dto(panel->catalog, !enable);

	return 0;
}

static int dp_panel_read_sink_sts(struct dp_panel *dp_panel, u8 *sts, u32 size)
{
	int rlen, rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel || !sts || !size) {
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		return rc;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_SINK_COUNT_ESI,
		sts, size);
	if (rlen != size) {
		DP_ERR("dpcd sink sts fail rlen:%d size:%d\n", rlen, size);
		rc = -EINVAL;
		return rc;
	}

	return 0;
}

static int dp_panel_update_edid(struct dp_panel *dp_panel, struct edid *edid)
{
	int rc;

	dp_panel->edid_ctrl->edid = edid;
	sde_parse_edid(dp_panel->edid_ctrl);

	rc = _sde_edid_update_modes(dp_panel->connector, dp_panel->edid_ctrl);
	dp_panel->audio_supported = drm_detect_monitor_audio(edid);

	return rc;
}

static bool dp_panel_read_mst_cap(struct dp_panel *dp_panel)
{
	int rlen;
	struct dp_panel_private *panel;
	u8 dpcd;
	bool mst_cap = false;

	if (!dp_panel) {
		DP_ERR("invalid input\n");
		return 0;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_MSTM_CAP,
		&dpcd, 1);
	if (rlen < 1) {
		DP_ERR("dpcd mstm_cap read failed, rlen=%d\n", rlen);
		goto end;
	}

	mst_cap = (dpcd & DP_MST_CAP) ? true : false;

end:
	DP_DEBUG("dp mst-cap: %d\n", mst_cap);

	return mst_cap;
}

static void dp_panel_convert_to_dp_mode(struct dp_panel *dp_panel,
		const struct drm_display_mode *drm_mode,
		struct dp_display_mode *dp_mode)
{
	const u32 num_components = 3, default_bpp = 24;
	struct msm_compression_info *comp_info;
	bool dsc_cap = (dp_mode->capabilities & DP_PANEL_CAPS_DSC) ?
				true : false;
	int rc;

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
	dp_mode->timing.dsc_overhead_fp = 0;

	comp_info = &dp_mode->timing.comp_info;
	comp_info->comp_ratio = DP_COMPRESSION_RATIO_NONE;
	comp_info->comp_type = MSM_DISPLAY_COMPRESSION_NONE;

	if (dp_panel->dsc_en && dsc_cap) {
		if (dp_panel_dsc_prepare_basic_params(comp_info,
					dp_mode, dp_panel)) {
			DP_DEBUG("prepare DSC basic params failed\n");
			return;
		}

		rc = sde_dsc_populate_dsc_config(&comp_info->dsc_info.config, 0);
		if (rc) {
			DP_DEBUG("failed populating dsc params \n");
			return;
		}

		rc = sde_dsc_populate_dsc_private_params(&comp_info->dsc_info,
				dp_mode->timing.h_active);
		if (rc) {
			DP_DEBUG("failed populating other dsc params\n");
			return;
		}

		dp_panel_dsc_pclk_param_calc(dp_panel,
				&comp_info->dsc_info,
				comp_info->comp_ratio,
				dp_mode);
	}
	dp_mode->fec_overhead_fp = dp_panel->fec_overhead_fp;
}

static void dp_panel_update_pps(struct dp_panel *dp_panel, char *pps_cmd)
{
	struct dp_catalog_panel *catalog;
	struct dp_panel_private *panel;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	catalog = panel->catalog;
	catalog->stream_id = dp_panel->stream_id;
	catalog->pps_flush(catalog);
}

struct dp_panel *dp_panel_get(struct dp_panel_in *in)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_panel *dp_panel;
	struct sde_connector *sde_conn;

	if (!in->dev || !in->catalog || !in->aux ||
			!in->link || !in->connector) {
		DP_ERR("invalid input\n");
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
	dp_panel->link_bw_code = 0;
	dp_panel->lane_count = 0;
	memcpy(panel->spd_vendor_name, vendor_name, (sizeof(u8) * 8));
	memcpy(panel->spd_product_description, product_desc, (sizeof(u8) * 16));
	dp_panel->connector = in->connector;

	dp_panel->dsc_feature_enable = panel->parser->dsc_feature_enable;
	dp_panel->fec_feature_enable = panel->parser->fec_feature_enable;

	if (in->base_panel) {
		memcpy(dp_panel->dpcd, in->base_panel->dpcd,
				DP_RECEIVER_CAP_SIZE + 1);
		memcpy(dp_panel->dsc_dpcd, in->base_panel->dsc_dpcd,
				DP_RECEIVER_DSC_CAP_SIZE + 1);
		memcpy(&dp_panel->link_info, &in->base_panel->link_info,
				sizeof(dp_panel->link_info));
		dp_panel->mst_state = in->base_panel->mst_state;
		dp_panel->widebus_en = in->base_panel->widebus_en;
		dp_panel->fec_en = in->base_panel->fec_en;
		dp_panel->dsc_en = in->base_panel->dsc_en;
		dp_panel->fec_overhead_fp = in->base_panel->fec_overhead_fp;
	}

	dp_panel->init = dp_panel_init_panel_info;
	dp_panel->deinit = dp_panel_deinit_panel_info;
	dp_panel->hw_cfg = dp_panel_hw_cfg;
	dp_panel->read_sink_caps = dp_panel_read_sink_caps;
	dp_panel->get_mode_bpp = dp_panel_get_mode_bpp;
	dp_panel->get_modes = dp_panel_get_modes;
	dp_panel->handle_sink_request = dp_panel_handle_sink_request;
	dp_panel->set_edid = dp_panel_set_edid;
	dp_panel->set_dpcd = dp_panel_set_dpcd;
	dp_panel->tpg_config = dp_panel_tpg_config;
	dp_panel->spd_config = dp_panel_spd_config;
	dp_panel->setup_hdr = dp_panel_setup_hdr;
	dp_panel->set_colorspace = dp_panel_set_colorspace;
	dp_panel->hdr_supported = dp_panel_hdr_supported;
	dp_panel->set_stream_info = dp_panel_set_stream_info;
	dp_panel->read_sink_status = dp_panel_read_sink_sts;
	dp_panel->update_edid = dp_panel_update_edid;
	dp_panel->read_mst_cap = dp_panel_read_mst_cap;
	dp_panel->convert_to_dp_mode = dp_panel_convert_to_dp_mode;
	dp_panel->update_pps = dp_panel_update_pps;

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
	struct sde_connector *sde_conn;

	if (!dp_panel)
		return;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	dp_panel_edid_deregister(panel);
	sde_conn = to_sde_connector(dp_panel->connector);
	if (sde_conn)
		sde_conn->drv_panel = NULL;

	devm_kfree(panel->dev, panel);
}
