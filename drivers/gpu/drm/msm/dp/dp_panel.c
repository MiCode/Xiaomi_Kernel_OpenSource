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

#define DP_KHZ_TO_HZ 1000
#define DP_PANEL_DEFAULT_BPP 24
#define DP_MAX_DS_PORT_COUNT 1

#define DPRX_FEATURE_ENUMERATION_LIST 0x2210
#define VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED BIT(3)
#define VSC_EXT_VESA_SDP_SUPPORTED BIT(4)
#define VSC_EXT_VESA_SDP_CHAINING_SUPPORTED BIT(5)

struct dp_vc_tu_mapping_table {
	u32 vic;
	u8 lanes;
	u8 lrate; /* DP_LINK_RATE -> 162(6), 270(10), 540(20), 810 (30) */
	u8 bpp;
	u32 valid_boundary_link;
	u32 delay_start_link;
	bool boundary_moderation_en;
	u32 valid_lower_boundary_link;
	u32 upper_boundary_count;
	u32 lower_boundary_count;
	u32 tu_size_minus1;
};

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

static void dp_panel_get_extra_req_bytes(u64 result_valid,
					int valid_bdary_link,
					u64 value1, u64 value2,
					bool *negative, u64 *result,
					u64 compare)
{
	*negative = false;
	if (result_valid >= compare) {
		if (valid_bdary_link
				>= compare)
			*result = value1 + value2;
		else {
			if (value1 < value2)
				*negative = true;
			*result = (value1 >= value2) ?
				(value1 - value2) : (value2 - value1);
		}
	} else {
		if (valid_bdary_link
				>= compare) {
			if (value1 >= value2)
				*negative = true;
			*result = (value1 >= value2) ?
				(value1 - value2) : (value2 - value1);
		} else {
			*result = value1 + value2;
			*negative = true;
		}
	}
}

static u64 roundup_u64(u64 x, u64 y)
{
	x += (y - 1);
	return (div64_ul(x, y) * y);
}

static u64 rounddown_u64(u64 x, u64 y)
{
	u64 rem;

	div64_u64_rem(x, y, &rem);
	return (x - rem);
}

static void dp_panel_calc_tu_parameters(struct dp_panel *dp_panel,
		struct dp_vc_tu_mapping_table *tu_table)
{
	u32 const multiplier = 1000000;
	u64 pclk, lclk;
	u8 bpp, ln_cnt;
	int run_idx = 0;
	u32 lwidth, h_blank;
	u32 fifo_empty = 0;
	u32 ratio_scale = 1001;
	u64 temp, ratio, original_ratio;
	u64 temp2, reminder;
	u64 temp3, temp4, result = 0;

	u64 err = multiplier;
	u64 n_err = 0, n_n_err = 0;
	bool n_err_neg, nn_err_neg;
	u8 hblank_margin = 16;

	u8 tu_size, tu_size_desired = 0, tu_size_minus1;
	int valid_boundary_link;
	u32 resulting_valid;
	u64 total_valid;
	u64 effective_valid;
	u64 effective_valid_recorded;
	int n_tus;
	int n_tus_per_lane;
	int paired_tus;
	int remainder_tus;
	int remainder_tus_upper, remainder_tus_lower;
	int extra_bytes;
	int filler_size;
	int delay_start_link;
	int boundary_moderation_en = 0;
	int upper_bdry_cnt = 0;
	int lower_bdry_cnt = 0;
	int i_upper_bdry_cnt = 0;
	int i_lower_bdry_cnt = 0;
	int valid_lower_boundary_link = 0;
	int even_distribution_bf = 0;
	int even_distribution_legacy = 0;
	int even_distribution = 0;
	int min_hblank = 0;
	int extra_pclk_cycles;
	u8 extra_pclk_cycle_delay = 4;
	int extra_pclk_cycles_in_link_clk;
	u64 ratio_by_tu;
	u64 average_valid2;
	u64 extra_buffer_margin;
	int new_valid_boundary_link;

	u64 resulting_valid_tmp;
	u64 ratio_by_tu_tmp;
	int n_tus_tmp;
	int extra_pclk_cycles_tmp;
	int extra_pclk_cycles_in_lclk_tmp;
	int extra_req_bytes_new_tmp;
	int filler_size_tmp;
	int lower_filler_size_tmp;
	int delay_start_link_tmp;
	int min_hblank_tmp = 0;
	bool extra_req_bytes_is_neg = false;
	struct dp_panel_info *pinfo = &dp_panel->pinfo;

	u8 dp_brute_force = 1;
	u64 brute_force_threshold = 10;
	u64 diff_abs;

	struct dp_panel_private *panel;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	ln_cnt =  panel->link->link_params.lane_count;

	bpp = pinfo->bpp;
	lwidth = pinfo->h_active;
	h_blank = pinfo->h_back_porch + pinfo->h_front_porch +
				pinfo->h_sync_width;
	pclk = pinfo->pixel_clk_khz * 1000;

	boundary_moderation_en = 0;
	upper_bdry_cnt = 0;
	lower_bdry_cnt = 0;
	i_upper_bdry_cnt = 0;
	i_lower_bdry_cnt = 0;
	valid_lower_boundary_link = 0;
	even_distribution_bf = 0;
	even_distribution_legacy = 0;
	even_distribution = 0;
	min_hblank = 0;

	lclk = drm_dp_bw_code_to_link_rate(
		panel->link->link_params.bw_code) * DP_KHZ_TO_HZ;

	pr_debug("pclk=%lld, active_width=%d, h_blank=%d\n",
						pclk, lwidth, h_blank);
	pr_debug("lclk = %lld, ln_cnt = %d\n", lclk, ln_cnt);
	ratio = div64_u64_rem(pclk * bpp * multiplier,
				8 * ln_cnt * lclk, &reminder);
	ratio = div64_u64((pclk * bpp * multiplier), (8 * ln_cnt * lclk));
	original_ratio = ratio;

	extra_buffer_margin = roundup_u64(div64_u64(extra_pclk_cycle_delay
				* lclk * multiplier, pclk), multiplier);
	extra_buffer_margin = div64_u64(extra_buffer_margin, multiplier);

	/* To deal with cases where lines are not distributable */
	if (((lwidth % ln_cnt) != 0) && ratio < multiplier) {
		ratio = ratio * ratio_scale;
		ratio = ratio < (1000 * multiplier)
				? ratio : (1000 * multiplier);
	}
	pr_debug("ratio = %lld\n", ratio);

	for (tu_size = 32; tu_size <= 64; tu_size++) {
		temp = ratio * tu_size;
		temp2 = ((temp / multiplier) + 1) * multiplier;
		n_err = roundup_u64(temp, multiplier) - temp;

		if (n_err < err) {
			err = n_err;
			tu_size_desired = tu_size;
		}
	}
	pr_debug("Info: tu_size_desired = %d\n", tu_size_desired);

	tu_size_minus1 = tu_size_desired - 1;

	valid_boundary_link = roundup_u64(ratio * tu_size_desired, multiplier);
	valid_boundary_link /= multiplier;
	n_tus = rounddown((lwidth * bpp * multiplier)
			/ (8 * valid_boundary_link), multiplier) / multiplier;
	even_distribution_legacy = n_tus % ln_cnt == 0 ? 1 : 0;
	pr_debug("Info: n_symbol_per_tu=%d, number_of_tus=%d\n",
					valid_boundary_link, n_tus);

	extra_bytes = roundup_u64((n_tus + 1)
			* ((valid_boundary_link * multiplier)
			- (original_ratio * tu_size_desired)), multiplier);
	extra_bytes /= multiplier;
	extra_pclk_cycles = roundup(extra_bytes * 8 * multiplier / bpp,
			multiplier);
	extra_pclk_cycles /= multiplier;
	extra_pclk_cycles_in_link_clk = roundup_u64(div64_u64(extra_pclk_cycles
				* lclk * multiplier, pclk), multiplier);
	extra_pclk_cycles_in_link_clk /= multiplier;
	filler_size = roundup_u64((tu_size_desired - valid_boundary_link)
						* multiplier, multiplier);
	filler_size /= multiplier;
	ratio_by_tu = div64_u64(ratio * tu_size_desired, multiplier);

	pr_debug("extra_pclk_cycles_in_link_clk=%d, extra_bytes=%d\n",
				extra_pclk_cycles_in_link_clk, extra_bytes);
	pr_debug("extra_pclk_cycles_in_link_clk=%d\n",
				extra_pclk_cycles_in_link_clk);
	pr_debug("filler_size=%d, extra_buffer_margin=%lld\n",
				filler_size, extra_buffer_margin);

	delay_start_link = ((extra_bytes > extra_pclk_cycles_in_link_clk)
			? extra_bytes
			: extra_pclk_cycles_in_link_clk)
				+ filler_size + extra_buffer_margin;
	resulting_valid = valid_boundary_link;
	pr_debug("Info: delay_start_link=%d, filler_size=%d\n",
				delay_start_link, filler_size);
	pr_debug("valid_boundary_link=%d ratio_by_tu=%lld\n",
				valid_boundary_link, ratio_by_tu);

	diff_abs = (resulting_valid >= ratio_by_tu)
				? (resulting_valid - ratio_by_tu)
				: (ratio_by_tu - resulting_valid);

	if (err != 0 && ((diff_abs > brute_force_threshold)
			|| (even_distribution_legacy == 0)
			|| (dp_brute_force == 1))) {
		err = multiplier;
		for (tu_size = 32; tu_size <= 64; tu_size++) {
			for (i_upper_bdry_cnt = 1; i_upper_bdry_cnt <= 15;
						i_upper_bdry_cnt++) {
				for (i_lower_bdry_cnt = 1;
					i_lower_bdry_cnt <= 15;
					i_lower_bdry_cnt++) {
					new_valid_boundary_link =
						roundup_u64(ratio
						* tu_size, multiplier);
					average_valid2 = (i_upper_bdry_cnt
						* new_valid_boundary_link
						+ i_lower_bdry_cnt
						* (new_valid_boundary_link
							- multiplier))
						/ (i_upper_bdry_cnt
							+ i_lower_bdry_cnt);
					n_tus = rounddown_u64(div64_u64(lwidth
						* multiplier * multiplier
						* (bpp / 8), average_valid2),
							multiplier);
					n_tus /= multiplier;
					n_tus_per_lane
						= rounddown(n_tus
							* multiplier
							/ ln_cnt, multiplier);
					n_tus_per_lane /= multiplier;
					paired_tus =
						rounddown((n_tus_per_lane)
							* multiplier
							/ (i_upper_bdry_cnt
							+ i_lower_bdry_cnt),
							multiplier);
					paired_tus /= multiplier;
					remainder_tus = n_tus_per_lane
							- paired_tus
						* (i_upper_bdry_cnt
							+ i_lower_bdry_cnt);
					if ((remainder_tus
						- i_upper_bdry_cnt) > 0) {
						remainder_tus_upper
							= i_upper_bdry_cnt;
						remainder_tus_lower =
							remainder_tus
							- i_upper_bdry_cnt;
					} else {
						remainder_tus_upper
							= remainder_tus;
						remainder_tus_lower = 0;
					}
					total_valid = paired_tus
						* (i_upper_bdry_cnt
						* new_valid_boundary_link
							+ i_lower_bdry_cnt
						* (new_valid_boundary_link
							- multiplier))
						+ (remainder_tus_upper
						* new_valid_boundary_link)
						+ (remainder_tus_lower
						* (new_valid_boundary_link
							- multiplier));
					n_err_neg = nn_err_neg = false;
					effective_valid
						= div_u64(total_valid,
							n_tus_per_lane);
					n_n_err = (effective_valid
							>= (ratio * tu_size))
						? (effective_valid
							- (ratio * tu_size))
						: ((ratio * tu_size)
							- effective_valid);
					if (effective_valid < (ratio * tu_size))
						nn_err_neg = true;
					n_err = (average_valid2
						>= (ratio * tu_size))
						? (average_valid2
							- (ratio * tu_size))
						: ((ratio * tu_size)
							- average_valid2);
					if (average_valid2 < (ratio * tu_size))
						n_err_neg = true;
					even_distribution =
						n_tus % ln_cnt == 0 ? 1 : 0;
					diff_abs =
						resulting_valid >= ratio_by_tu
						? (resulting_valid
							- ratio_by_tu)
						: (ratio_by_tu
							- resulting_valid);

					resulting_valid_tmp = div64_u64(
						(i_upper_bdry_cnt
						* new_valid_boundary_link
						+ i_lower_bdry_cnt
						* (new_valid_boundary_link
							- multiplier)),
						(i_upper_bdry_cnt
							+ i_lower_bdry_cnt));
					ratio_by_tu_tmp =
						original_ratio * tu_size;
					ratio_by_tu_tmp /= multiplier;
					n_tus_tmp = rounddown_u64(
						div64_u64(lwidth
						* multiplier * multiplier
						* bpp / 8,
						resulting_valid_tmp),
						multiplier);
					n_tus_tmp /= multiplier;

					temp3 = (resulting_valid_tmp
						>= (original_ratio * tu_size))
						? (resulting_valid_tmp
						- original_ratio * tu_size)
						: (original_ratio * tu_size)
						- resulting_valid_tmp;
					temp3 = (n_tus_tmp + 1) * temp3;
					temp4 = (new_valid_boundary_link
						>= (original_ratio * tu_size))
						? (new_valid_boundary_link
							- original_ratio
							* tu_size)
						: (original_ratio * tu_size)
						- new_valid_boundary_link;
					temp4 = (i_upper_bdry_cnt
							* ln_cnt * temp4);

					temp3 = roundup_u64(temp3, multiplier);
					temp4 = roundup_u64(temp4, multiplier);
					dp_panel_get_extra_req_bytes
						(resulting_valid_tmp,
						new_valid_boundary_link,
						temp3, temp4,
						&extra_req_bytes_is_neg,
						&result,
						(original_ratio * tu_size));
					extra_req_bytes_new_tmp
						= div64_ul(result, multiplier);
					if ((extra_req_bytes_is_neg)
						&& (extra_req_bytes_new_tmp
							> 1))
						extra_req_bytes_new_tmp
						= extra_req_bytes_new_tmp - 1;
					if (extra_req_bytes_new_tmp == 0)
						extra_req_bytes_new_tmp = 1;
					extra_pclk_cycles_tmp =
						(int)(extra_req_bytes_new_tmp
						      * 8 * multiplier) / bpp;
					extra_pclk_cycles_tmp /= multiplier;

					if (extra_pclk_cycles_tmp <= 0)
						extra_pclk_cycles_tmp = 1;
					extra_pclk_cycles_in_lclk_tmp =
						roundup_u64(div64_u64(
							extra_pclk_cycles_tmp
							* lclk * multiplier,
							pclk), multiplier);
					extra_pclk_cycles_in_lclk_tmp
						/= multiplier;
					filler_size_tmp = roundup_u64(
						(tu_size * multiplier *
						new_valid_boundary_link),
						multiplier);
					filler_size_tmp /= multiplier;
					lower_filler_size_tmp =
						filler_size_tmp + 1;
					if (extra_req_bytes_is_neg)
						temp3 = (extra_req_bytes_new_tmp
						> extra_pclk_cycles_in_lclk_tmp
						? extra_pclk_cycles_in_lclk_tmp
						: extra_req_bytes_new_tmp);
					else
						temp3 = (extra_req_bytes_new_tmp
						> extra_pclk_cycles_in_lclk_tmp
						? extra_req_bytes_new_tmp :
						extra_pclk_cycles_in_lclk_tmp);

					temp4 = lower_filler_size_tmp
						+ extra_buffer_margin;
					if (extra_req_bytes_is_neg)
						delay_start_link_tmp
							= (temp3 >= temp4)
							? (temp3 - temp4)
							: (temp4 - temp3);
					else
						delay_start_link_tmp
							= temp3 + temp4;

					min_hblank_tmp = (int)div64_u64(
						roundup_u64(
						div64_u64(delay_start_link_tmp
						* pclk * multiplier, lclk),
						multiplier), multiplier)
						+ hblank_margin;

					if (((even_distribution == 1)
						|| ((even_distribution_bf == 0)
						&& (even_distribution_legacy
								== 0)))
						&& !n_err_neg && !nn_err_neg
						&& n_n_err < err
						&& (n_n_err < diff_abs
						|| (dp_brute_force == 1))
						&& (new_valid_boundary_link
									- 1) > 0
						&& (h_blank >=
							(u32)min_hblank_tmp)) {
						upper_bdry_cnt =
							i_upper_bdry_cnt;
						lower_bdry_cnt =
							i_lower_bdry_cnt;
						err = n_n_err;
						boundary_moderation_en = 1;
						tu_size_desired = tu_size;
						valid_boundary_link =
							new_valid_boundary_link;
						effective_valid_recorded
							= effective_valid;
						delay_start_link
							= delay_start_link_tmp;
						filler_size = filler_size_tmp;
						min_hblank = min_hblank_tmp;
						n_tus = n_tus_tmp;
						even_distribution_bf = 1;

						pr_debug("upper_bdry_cnt=%d, lower_boundary_cnt=%d, err=%lld, tu_size_desired=%d, valid_boundary_link=%d, effective_valid=%lld\n",
							upper_bdry_cnt,
							lower_bdry_cnt, err,
							tu_size_desired,
							valid_boundary_link,
							effective_valid);
					}
				}
			}
		}

		if (boundary_moderation_en == 1) {
			resulting_valid = (u32)(upper_bdry_cnt
					*valid_boundary_link + lower_bdry_cnt
					* (valid_boundary_link - 1))
					/ (upper_bdry_cnt + lower_bdry_cnt);
			ratio_by_tu = original_ratio * tu_size_desired;
			valid_lower_boundary_link =
				(valid_boundary_link / multiplier) - 1;

			tu_size_minus1 = tu_size_desired - 1;
			even_distribution_bf = 1;
			valid_boundary_link /= multiplier;
			pr_debug("Info: Boundary_moderation enabled\n");
		}
	}

	min_hblank = ((int) roundup_u64(div64_u64(delay_start_link * pclk
			* multiplier, lclk), multiplier))
			/ multiplier + hblank_margin;
	if (h_blank < (u32)min_hblank) {
		pr_debug(" WARNING: run_idx=%d Programmed h_blank %d is smaller than the min_hblank %d supported.\n",
					run_idx, h_blank, min_hblank);
	}

	if (fifo_empty)	{
		tu_size_minus1 = 31;
		valid_boundary_link = 32;
		delay_start_link = 0;
		boundary_moderation_en = 0;
	}

	pr_debug("tu_size_minus1=%d valid_boundary_link=%d delay_start_link=%d boundary_moderation_en=%d\n upper_boundary_cnt=%d lower_boundary_cnt=%d valid_lower_boundary_link=%d min_hblank=%d\n",
		tu_size_minus1, valid_boundary_link, delay_start_link,
		boundary_moderation_en, upper_bdry_cnt, lower_bdry_cnt,
		valid_lower_boundary_link, min_hblank);

	tu_table->valid_boundary_link = valid_boundary_link;
	tu_table->delay_start_link = delay_start_link;
	tu_table->boundary_moderation_en = boundary_moderation_en;
	tu_table->valid_lower_boundary_link = valid_lower_boundary_link;
	tu_table->upper_boundary_count = upper_bdry_cnt;
	tu_table->lower_boundary_count = lower_bdry_cnt;
	tu_table->tu_size_minus1 = tu_size_minus1;
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
	u8 *dpcd, rx_feature;
	u32 dfp_count = 0;
	unsigned long caps = DP_LINK_CAP_ENHANCED_FRAMING;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	dpcd = dp_panel->dpcd;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	link_info = &dp_panel->link_info;

	if (!panel->custom_dpcd) {
		rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_DPCD_REV,
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
	}

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux,
		DPRX_FEATURE_ENUMERATION_LIST, &rx_feature, 1);
	if (rlen != 1) {
		pr_debug("failed to read DPRX_FEATURE_ENUMERATION_LIST\n");
		panel->vsc_supported = false;
		panel->vscext_supported = false;
		panel->vscext_chaining_supported = false;
	} else {
		panel->vsc_supported = !!(rx_feature &
			VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED);

		panel->vscext_supported = !!(rx_feature &
			VSC_EXT_VESA_SDP_SUPPORTED);

		panel->vscext_chaining_supported = !!(rx_feature &
			VSC_EXT_VESA_SDP_CHAINING_SUPPORTED);
	}

	pr_debug("vsc=%d, vscext=%d, vscext_chaining=%d\n",
		panel->vsc_supported, panel->vscext_supported,
		panel->vscext_chaining_supported);

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
	}

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

	return 0;
}

static int dp_panel_read_edid(struct dp_panel *dp_panel,
	struct drm_connector *connector)
{
	int ret = 0;
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto end;
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
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	pinfo = &dp_panel->pinfo;

	/*
	 * print resolution info as this is a result
	 * of user initiated action of cable connection
	 */
	pr_info("SET NEW RESOLUTION:\n");
	pr_info("%dx%d@%dfps\n", pinfo->h_active,
		pinfo->v_active, pinfo->refresh_rate);
	pr_info("h_porches(back|front|width) = (%d|%d|%d)\n",
			pinfo->h_back_porch,
			pinfo->h_front_porch,
			pinfo->h_sync_width);
	pr_info("v_porches(back|front|width) = (%d|%d|%d)\n",
			pinfo->v_back_porch,
			pinfo->v_front_porch,
			pinfo->v_sync_width);
	pr_info("pixel clock (KHz)=(%d)\n", pinfo->pixel_clk_khz);
	pr_info("bpp = %d\n", pinfo->bpp);
	pr_info("active low (h|v)=(%d|%d)\n", pinfo->h_active_low,
		pinfo->v_active_low);
end:
	return rc;
}

static int dp_panel_deinit_panel_info(struct dp_panel *dp_panel)
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

	if (dp_panel->stream_id >= DP_STREAM_MAX) {
		pr_err("invalid stream id:%d\n", dp_panel->stream_id);
		return -EINVAL;
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

	dp_panel = &panel->dp_panel;
	dp_panel->max_bw_code = DP_LINK_BW_8_1;
	dp_panel->spd_enabled = true;
	memcpy(panel->spd_vendor_name, vendor_name, (sizeof(u8) * 8));
	memcpy(panel->spd_product_description, product_desc, (sizeof(u8) * 16));
	dp_panel->stream_id = DP_STREAM_MAX;
	dp_panel->connector = in->connector;

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
