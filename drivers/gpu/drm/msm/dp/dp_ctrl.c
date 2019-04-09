/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/delay.h>

#include "dp_ctrl.h"

#define DP_KHZ_TO_HZ 1000

#define DP_CTRL_INTR_READY_FOR_VIDEO     BIT(0)
#define DP_CTRL_INTR_IDLE_PATTERN_SENT  BIT(3)

/* dp state ctrl */
#define ST_TRAIN_PATTERN_1		BIT(0)
#define ST_TRAIN_PATTERN_2		BIT(1)
#define ST_TRAIN_PATTERN_3		BIT(2)
#define ST_TRAIN_PATTERN_4		BIT(3)
#define ST_SYMBOL_ERR_RATE_MEASUREMENT	BIT(4)
#define ST_PRBS7			BIT(5)
#define ST_CUSTOM_80_BIT_PATTERN	BIT(6)
#define ST_SEND_VIDEO			BIT(7)
#define ST_PUSH_IDLE			BIT(8)

#define MR_LINK_TRAINING1  0x8
#define MR_LINK_SYMBOL_ERM 0x80
#define MR_LINK_PRBS7 0x100
#define MR_LINK_CUSTOM80 0x200
#define MR_LINK_TRAINING4  0x40

struct dp_vc_tu_mapping_table {
	u32 vic;
	u8 lanes;
	u8 lrate; /* DP_LINK_RATE -> 162(6), 270(10), 540(20), 810 (30) */
	u8 bpp;
	u8 valid_boundary_link;
	u16 delay_start_link;
	bool boundary_moderation_en;
	u8 valid_lower_boundary_link;
	u8 upper_boundary_count;
	u8 lower_boundary_count;
	u8 tu_size_minus1;
};

struct dp_ctrl_private {
	struct dp_ctrl dp_ctrl;

	struct device *dev;
	struct dp_aux *aux;
	struct dp_panel *panel;
	struct dp_link *link;
	struct dp_power *power;
	struct dp_parser *parser;
	struct dp_catalog_ctrl *catalog;

	struct completion idle_comp;
	struct completion video_comp;

	bool orientation;
	bool power_on;

	atomic_t aborted;

	u32 pixel_rate;
	u32 vic;
};

enum notification_status {
	NOTIFY_UNKNOWN,
	NOTIFY_CONNECT,
	NOTIFY_DISCONNECT,
	NOTIFY_CONNECT_IRQ_HPD,
	NOTIFY_DISCONNECT_IRQ_HPD,
};

static void dp_ctrl_idle_patterns_sent(struct dp_ctrl_private *ctrl)
{
	pr_debug("idle_patterns_sent\n");
	complete(&ctrl->idle_comp);
}

static void dp_ctrl_video_ready(struct dp_ctrl_private *ctrl)
{
	pr_debug("dp_video_ready\n");
	complete(&ctrl->video_comp);
}

static void dp_ctrl_abort(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	atomic_set(&ctrl->aborted, 1);
}

static void dp_ctrl_state_ctrl(struct dp_ctrl_private *ctrl, u32 state)
{
	ctrl->catalog->state_ctrl(ctrl->catalog, state);
}

static void dp_ctrl_push_idle(struct dp_ctrl *dp_ctrl)
{
	int const idle_pattern_completion_timeout_ms = 3 * HZ / 100;
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (!ctrl->power_on) {
		pr_err("CTRL off, return\n");
		return;
	}

	reinit_completion(&ctrl->idle_comp);
	dp_ctrl_state_ctrl(ctrl, ST_PUSH_IDLE);

	if (!wait_for_completion_timeout(&ctrl->idle_comp,
			idle_pattern_completion_timeout_ms))
		pr_warn("PUSH_IDLE pattern timedout\n");

	pr_debug("mainlink off done\n");
}

static void dp_ctrl_config_ctrl(struct dp_ctrl_private *ctrl)
{
	u32 config = 0, tbd;
	u8 *dpcd = ctrl->panel->dpcd;

	config |= (2 << 13); /* Default-> LSCLK DIV: 1/4 LCLK  */
	config |= (0 << 11); /* RGB */

	/* Scrambler reset enable */
	if (dpcd[DP_EDP_CONFIGURATION_CAP] & DP_ALTERNATE_SCRAMBLER_RESET_CAP)
		config |= (1 << 10);

	tbd = ctrl->link->get_test_bits_depth(ctrl->link,
			ctrl->panel->pinfo.bpp);

	if (tbd == DP_TEST_BIT_DEPTH_UNKNOWN)
		tbd = DP_TEST_BIT_DEPTH_8;

	config |= tbd << 8;

	/* Num of Lanes */
	config |= ((ctrl->link->link_params.lane_count - 1) << 4);

	if (drm_dp_enhanced_frame_cap(dpcd))
		config |= 0x40;

	config |= 0x04; /* progressive video */

	config |= 0x03;	/* sycn clock & static Mvid */

	ctrl->catalog->config_ctrl(ctrl->catalog, config);
}

/**
 * dp_ctrl_configure_source_params() - configures DP transmitter source params
 * @ctrl: Display Port Driver data
 *
 * Configures the DP transmitter source params including details such as lane
 * configuration, output format and sink/panel timing information.
 */
static void dp_ctrl_configure_source_params(struct dp_ctrl_private *ctrl)
{
	u32 cc, tb;

	ctrl->catalog->lane_mapping(ctrl->catalog);
	ctrl->catalog->mainlink_ctrl(ctrl->catalog, true);

	dp_ctrl_config_ctrl(ctrl);

	tb = ctrl->link->get_test_bits_depth(ctrl->link,
		ctrl->panel->pinfo.bpp);
	cc = ctrl->link->get_colorimetry_config(ctrl->link);
	ctrl->catalog->config_misc(ctrl->catalog, cc, tb);
	ctrl->panel->timing_cfg(ctrl->panel);
}

static void dp_ctrl_get_extra_req_bytes(u64 result_valid,
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

static void dp_ctrl_calc_tu_parameters(struct dp_ctrl_private *ctrl,
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
	u64 resulting_valid;
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
	struct dp_panel_info *pinfo = &ctrl->panel->pinfo;

	u8 dp_brute_force = 1;
	u64 brute_force_threshold = 10;
	u64 diff_abs;

	ln_cnt =  ctrl->link->link_params.lane_count;

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
		ctrl->link->link_params.bw_code) * DP_KHZ_TO_HZ;

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
					dp_ctrl_get_extra_req_bytes
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
						(u64)(extra_req_bytes_new_tmp
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
			resulting_valid = (u64)(upper_bdry_cnt
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

static void dp_ctrl_setup_tr_unit(struct dp_ctrl_private *ctrl)
{
	u32 dp_tu = 0x0;
	u32 valid_boundary = 0x0;
	u32 valid_boundary2 = 0x0;
	struct dp_vc_tu_mapping_table tu_calc_table;

	dp_ctrl_calc_tu_parameters(ctrl, &tu_calc_table);

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

	ctrl->catalog->dp_tu = dp_tu;
	ctrl->catalog->valid_boundary = valid_boundary;
	ctrl->catalog->valid_boundary2 = valid_boundary2;

	ctrl->catalog->update_transfer_unit(ctrl->catalog);
}

static int dp_ctrl_wait4video_ready(struct dp_ctrl_private *ctrl)
{
	int ret = 0;

	ret = wait_for_completion_timeout(&ctrl->video_comp, HZ / 2);
	if (ret <= 0) {
		pr_err("Link Train timedout\n");
		ret = -EINVAL;
	}

	return ret;
}

static int dp_ctrl_update_sink_vx_px(struct dp_ctrl_private *ctrl,
		u32 voltage_level, u32 pre_emphasis_level)
{
	int i;
	u8 buf[4];
	u32 max_level_reached = 0;

	if (voltage_level == DP_LINK_VOLTAGE_MAX) {
		pr_debug("max. voltage swing level reached %d\n",
				voltage_level);
		max_level_reached |= BIT(2);
	}

	if (pre_emphasis_level == DP_LINK_PRE_EMPHASIS_MAX) {
		pr_debug("max. pre-emphasis level reached %d\n",
				pre_emphasis_level);
		max_level_reached  |= BIT(5);
	}

	pre_emphasis_level <<= 3;

	for (i = 0; i < 4; i++)
		buf[i] = voltage_level | pre_emphasis_level | max_level_reached;

	pr_debug("sink: p|v=0x%x\n", voltage_level | pre_emphasis_level);
	return drm_dp_dpcd_write(ctrl->aux->drm_aux, 0x103, buf, 4);
}

static int dp_ctrl_update_vx_px(struct dp_ctrl_private *ctrl)
{
	struct dp_link *link = ctrl->link;

	ctrl->catalog->update_vx_px(ctrl->catalog,
		link->phy_params.v_level, link->phy_params.p_level);

	return dp_ctrl_update_sink_vx_px(ctrl, link->phy_params.v_level,
		link->phy_params.p_level);
}

static int dp_ctrl_train_pattern_set(struct dp_ctrl_private *ctrl,
		u8 pattern)
{
	u8 buf[4];

	pr_debug("sink: pattern=%x\n", pattern);

	buf[0] = pattern;
	return drm_dp_dpcd_write(ctrl->aux->drm_aux,
		DP_TRAINING_PATTERN_SET, buf, 1);
}

static int dp_ctrl_read_link_status(struct dp_ctrl_private *ctrl,
					u8 *link_status)
{
	int ret = 0, len;
	u32 const offset = DP_LANE_ALIGN_STATUS_UPDATED - DP_LANE0_1_STATUS;
	u32 link_status_read_max_retries = 100;

	while (--link_status_read_max_retries) {
		len = drm_dp_dpcd_read_link_status(ctrl->aux->drm_aux,
			link_status);
		if (len != DP_LINK_STATUS_SIZE) {
			pr_err("DP link status read failed, err: %d\n", len);
			ret = len;
			break;
		}

		if (!(link_status[offset] & DP_LINK_STATUS_UPDATED))
			break;
	}

	return ret;
}

static int dp_ctrl_link_train_1(struct dp_ctrl_private *ctrl)
{
	int tries, old_v_level, ret = 0;
	u8 link_status[DP_LINK_STATUS_SIZE];
	int const maximum_retries = 5;

	ctrl->aux->state &= ~DP_STATE_TRAIN_1_FAILED;
	ctrl->aux->state &= ~DP_STATE_TRAIN_1_SUCCEEDED;
	ctrl->aux->state |= DP_STATE_TRAIN_1_STARTED;

	dp_ctrl_state_ctrl(ctrl, 0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	ctrl->catalog->set_pattern(ctrl->catalog, 0x01);
	ret = dp_ctrl_train_pattern_set(ctrl, DP_TRAINING_PATTERN_1 |
		DP_LINK_SCRAMBLING_DISABLE); /* train_1 */
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	ret = dp_ctrl_update_vx_px(ctrl);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	tries = 0;
	old_v_level = ctrl->link->phy_params.v_level;
	while (!atomic_read(&ctrl->aborted)) {
		drm_dp_link_train_clock_recovery_delay(ctrl->panel->dpcd);

		ret = dp_ctrl_read_link_status(ctrl, link_status);
		if (ret)
			break;

		if (drm_dp_clock_recovery_ok(link_status,
			ctrl->link->link_params.lane_count)) {
			break;
		}

		if (ctrl->link->phy_params.v_level == DP_LINK_VOLTAGE_MAX) {
			pr_err_ratelimited("max v_level reached\n");
			ret = -EAGAIN;
			break;
		}

		if (old_v_level == ctrl->link->phy_params.v_level) {
			tries++;
			if (tries >= maximum_retries) {
				pr_err("max tries reached\n");
				ret = -ETIMEDOUT;
				break;
			}
		} else {
			tries = 0;
			old_v_level = ctrl->link->phy_params.v_level;
		}

		pr_debug("clock recovery not done, adjusting vx px\n");

		ctrl->link->adjust_levels(ctrl->link, link_status);
		ret = dp_ctrl_update_vx_px(ctrl);
		if (ret <= 0) {
			ret = -EINVAL;
			break;
		}
	}
end:
	ctrl->aux->state &= ~DP_STATE_TRAIN_1_STARTED;

	if (ret)
		ctrl->aux->state |= DP_STATE_TRAIN_1_FAILED;
	else
		ctrl->aux->state |= DP_STATE_TRAIN_1_SUCCEEDED;

	return ret;
}

static int dp_ctrl_link_rate_down_shift(struct dp_ctrl_private *ctrl)
{
	int ret = 0;

	if (!ctrl)
		return -EINVAL;

	switch (ctrl->link->link_params.bw_code) {
	case DP_LINK_BW_8_1:
		ctrl->link->link_params.bw_code = DP_LINK_BW_5_4;
		break;
	case DP_LINK_BW_5_4:
		ctrl->link->link_params.bw_code = DP_LINK_BW_2_7;
		break;
	case DP_LINK_BW_2_7:
	case DP_LINK_BW_1_62:
	default:
		ctrl->link->link_params.bw_code = DP_LINK_BW_1_62;
		break;
	};

	pr_debug("new bw code=0x%x\n", ctrl->link->link_params.bw_code);

	return ret;
}

static void dp_ctrl_clear_training_pattern(struct dp_ctrl_private *ctrl)
{
	dp_ctrl_train_pattern_set(ctrl, 0);
	drm_dp_link_train_channel_eq_delay(ctrl->panel->dpcd);
}

static int dp_ctrl_link_training_2(struct dp_ctrl_private *ctrl)
{
	int tries = 0, ret = 0;
	char pattern;
	int const maximum_retries = 5;
	u8 link_status[DP_LINK_STATUS_SIZE];

	ctrl->aux->state &= ~DP_STATE_TRAIN_2_FAILED;
	ctrl->aux->state &= ~DP_STATE_TRAIN_2_SUCCEEDED;
	ctrl->aux->state |= DP_STATE_TRAIN_2_STARTED;

	dp_ctrl_state_ctrl(ctrl, 0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	if (drm_dp_tps3_supported(ctrl->panel->dpcd))
		pattern = DP_TRAINING_PATTERN_3;
	else
		pattern = DP_TRAINING_PATTERN_2;

	ret = dp_ctrl_update_vx_px(ctrl);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}
	ctrl->catalog->set_pattern(ctrl->catalog, pattern);
	ret = dp_ctrl_train_pattern_set(ctrl,
		pattern | DP_RECOVERED_CLOCK_OUT_EN);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	do  {
		drm_dp_link_train_channel_eq_delay(ctrl->panel->dpcd);

		ret = dp_ctrl_read_link_status(ctrl, link_status);
		if (ret)
			break;

		if (drm_dp_channel_eq_ok(link_status,
			ctrl->link->link_params.lane_count))
			break;

		if (tries > maximum_retries) {
			ret = -ETIMEDOUT;
			break;
		}
		tries++;

		ctrl->link->adjust_levels(ctrl->link, link_status);
		ret = dp_ctrl_update_vx_px(ctrl);
		if (ret <= 0) {
			ret = -EINVAL;
			break;
		}
	} while (!atomic_read(&ctrl->aborted));
end:
	ctrl->aux->state &= ~DP_STATE_TRAIN_2_STARTED;

	if (ret)
		ctrl->aux->state |= DP_STATE_TRAIN_2_FAILED;
	else
		ctrl->aux->state |= DP_STATE_TRAIN_2_SUCCEEDED;
	return ret;
}

static int dp_ctrl_link_train(struct dp_ctrl_private *ctrl)
{
	int ret = 0;
	u8 encoding = 0x1;
	struct drm_dp_link link_info = {0};

	ctrl->link->phy_params.p_level = 0;
	ctrl->link->phy_params.v_level = 0;

	dp_ctrl_config_ctrl(ctrl);

	link_info.num_lanes = ctrl->link->link_params.lane_count;
	link_info.rate = drm_dp_bw_code_to_link_rate(
		ctrl->link->link_params.bw_code);
	link_info.capabilities = ctrl->panel->link_info.capabilities;

	ret = drm_dp_link_configure(ctrl->aux->drm_aux, &link_info);
	if (ret)
		goto end;

	ret = drm_dp_dpcd_write(ctrl->aux->drm_aux,
		DP_MAIN_LINK_CHANNEL_CODING_SET, &encoding, 1);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	ret = dp_ctrl_link_train_1(ctrl);
	if (ret) {
		pr_err("link training #1 failed\n");
		goto end;
	}

	/* print success info as this is a result of user initiated action */
	pr_info("link training #1 successful\n");

	ret = dp_ctrl_link_training_2(ctrl);
	if (ret) {
		pr_err("link training #2 failed\n");
		goto end;
	}

	/* print success info as this is a result of user initiated action */
	pr_info("link training #2 successful\n");

end:
	dp_ctrl_state_ctrl(ctrl, 0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	dp_ctrl_clear_training_pattern(ctrl);
	return ret;
}

static int dp_ctrl_setup_main_link(struct dp_ctrl_private *ctrl, bool train)
{
	bool mainlink_ready = false;
	int ret = 0;

	ctrl->catalog->mainlink_ctrl(ctrl->catalog, true);

	if (ctrl->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN)
		goto end;

	if (!train)
		goto send_video;

	/*
	 * As part of previous calls, DP controller state might have
	 * transitioned to PUSH_IDLE. In order to start transmitting a link
	 * training pattern, we have to first to a DP software reset.
	 */
	ctrl->catalog->reset(ctrl->catalog);

	ret = dp_ctrl_link_train(ctrl);
	if (ret)
		goto end;

send_video:
	/*
	 * Set up transfer unit values and set controller state to send
	 * video.
	 */
	dp_ctrl_setup_tr_unit(ctrl);
	ctrl->catalog->state_ctrl(ctrl->catalog, ST_SEND_VIDEO);

	dp_ctrl_wait4video_ready(ctrl);
	mainlink_ready = ctrl->catalog->mainlink_ready(ctrl->catalog);
	pr_debug("mainlink %s\n", mainlink_ready ? "READY" : "NOT READY");
end:
	return ret;
}

static void dp_ctrl_set_clock_rate(struct dp_ctrl_private *ctrl,
		char *name, u32 rate)
{
	u32 num = ctrl->parser->mp[DP_CTRL_PM].num_clk;
	struct dss_clk *cfg = ctrl->parser->mp[DP_CTRL_PM].clk_config;

	while (num && strcmp(cfg->clk_name, name)) {
		num--;
		cfg++;
	}

	pr_debug("setting rate=%d on clk=%s\n", rate, name);

	if (num)
		cfg->rate = rate;
	else
		pr_err("%s clock could not be set with rate %d\n", name, rate);
}

static int dp_ctrl_enable_mainlink_clocks(struct dp_ctrl_private *ctrl)
{
	int ret = 0;

	ctrl->power->set_pixel_clk_parent(ctrl->power);

	dp_ctrl_set_clock_rate(ctrl, "ctrl_link_clk",
		drm_dp_bw_code_to_link_rate(ctrl->link->link_params.bw_code));

	dp_ctrl_set_clock_rate(ctrl, "ctrl_pixel_clk", ctrl->pixel_rate);

	ret = ctrl->power->clk_enable(ctrl->power, DP_CTRL_PM, true);
	if (ret) {
		pr_err("Unabled to start link clocks\n");
		ret = -EINVAL;
	}

	return ret;
}

static int dp_ctrl_disable_mainlink_clocks(struct dp_ctrl_private *ctrl)
{
	return ctrl->power->clk_enable(ctrl->power, DP_CTRL_PM, false);
}

static int dp_ctrl_host_init(struct dp_ctrl *dp_ctrl, bool flip, bool reset)
{
	struct dp_ctrl_private *ctrl;
	struct dp_catalog_ctrl *catalog;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->orientation = flip;
	catalog = ctrl->catalog;

	if (reset) {
		catalog->usb_reset(ctrl->catalog, flip);
		catalog->phy_reset(ctrl->catalog);
	}
	catalog->enable_irq(ctrl->catalog, true);

	return 0;
}

/**
 * dp_ctrl_host_deinit() - Uninitialize DP controller
 * @ctrl: Display Port Driver data
 *
 * Perform required steps to uninitialize DP controller
 * and its resources.
 */
static void dp_ctrl_host_deinit(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->catalog->enable_irq(ctrl->catalog, false);

	pr_debug("Host deinitialized successfully\n");
}

static int dp_ctrl_link_maintenance(struct dp_ctrl *dp_ctrl)
{
	int ret = 0;
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (!ctrl->power_on || atomic_read(&ctrl->aborted)) {
		pr_err("CTRL off, return\n");
		return -EINVAL;
	}

	ctrl->aux->state &= ~DP_STATE_LINK_MAINTENANCE_COMPLETED;
	ctrl->aux->state &= ~DP_STATE_LINK_MAINTENANCE_FAILED;
	ctrl->aux->state |= DP_STATE_LINK_MAINTENANCE_STARTED;

	ctrl->dp_ctrl.push_idle(&ctrl->dp_ctrl);
	ctrl->dp_ctrl.reset(&ctrl->dp_ctrl);

	ctrl->pixel_rate = ctrl->panel->pinfo.pixel_clk_khz;

	do {
		if (ret == -EAGAIN) {
			/* try with lower link rate */
			dp_ctrl_link_rate_down_shift(ctrl);

			ctrl->catalog->mainlink_ctrl(ctrl->catalog, false);
		}

		ctrl->catalog->phy_lane_cfg(ctrl->catalog,
			ctrl->orientation, ctrl->link->link_params.lane_count);

		/*
		 * Disable and re-enable the mainlink clock since the
		 * link clock might have been adjusted as part of the
		 * link maintenance.
		 */
		dp_ctrl_disable_mainlink_clocks(ctrl);

		ret = dp_ctrl_enable_mainlink_clocks(ctrl);
		if (ret)
			continue;

		dp_ctrl_configure_source_params(ctrl);

		ctrl->catalog->config_msa(ctrl->catalog,
			drm_dp_bw_code_to_link_rate(
			ctrl->link->link_params.bw_code), ctrl->pixel_rate);

		reinit_completion(&ctrl->idle_comp);

		ret = dp_ctrl_setup_main_link(ctrl, true);
	} while (ret == -EAGAIN);

	ctrl->aux->state &= ~DP_STATE_LINK_MAINTENANCE_STARTED;

	if (ret)
		ctrl->aux->state |= DP_STATE_LINK_MAINTENANCE_FAILED;
	else
		ctrl->aux->state |= DP_STATE_LINK_MAINTENANCE_COMPLETED;

	return ret;
}

static void dp_ctrl_process_phy_test_request(struct dp_ctrl *dp_ctrl)
{
	int ret = 0;
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (!ctrl->link->phy_params.phy_test_pattern_sel) {
		pr_debug("no test pattern selected by sink\n");
		return;
	}

	pr_debug("start\n");

	ctrl->dp_ctrl.push_idle(&ctrl->dp_ctrl);
	/*
	 * The global reset will need DP link ralated clocks to be
	 * running. Add the global reset just before disabling the
	 * link clocks and core clocks.
	 */
	ctrl->dp_ctrl.reset(&ctrl->dp_ctrl);
	ctrl->dp_ctrl.off(&ctrl->dp_ctrl);

	ret = ctrl->dp_ctrl.on(&ctrl->dp_ctrl);
	if (ret)
		pr_err("failed to enable DP controller\n");

	pr_debug("end\n");
}

static void dp_ctrl_send_phy_test_pattern(struct dp_ctrl_private *ctrl)
{
	bool success = false;
	u32 pattern_sent = 0x0;
	u32 pattern_requested = ctrl->link->phy_params.phy_test_pattern_sel;

	dp_ctrl_update_vx_px(ctrl);
	ctrl->catalog->send_phy_pattern(ctrl->catalog, pattern_requested);
	ctrl->link->send_test_response(ctrl->link);

	pattern_sent = ctrl->catalog->read_phy_pattern(ctrl->catalog);
	pr_debug("pattern_request: %s. pattern_sent: 0x%x\n",
			dp_link_get_phy_test_pattern(pattern_requested),
			pattern_sent);

	switch (pattern_sent) {
	case MR_LINK_TRAINING1:
		if (pattern_requested ==
				DP_TEST_PHY_PATTERN_D10_2_NO_SCRAMBLING)
			success = true;
		break;
	case MR_LINK_SYMBOL_ERM:
		if ((pattern_requested ==
				DP_TEST_PHY_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT)
			|| (pattern_requested ==
				DP_TEST_PHY_PATTERN_CP2520_PATTERN_1))
			success = true;
		break;
	case MR_LINK_PRBS7:
		if (pattern_requested == DP_TEST_PHY_PATTERN_PRBS7)
			success = true;
		break;
	case MR_LINK_CUSTOM80:
		if (pattern_requested ==
				DP_TEST_PHY_PATTERN_80_BIT_CUSTOM_PATTERN)
			success = true;
		break;
	case MR_LINK_TRAINING4:
		if (pattern_requested ==
				DP_TEST_PHY_PATTERN_CP2520_PATTERN_3)
			success = true;
		break;
	default:
		success = false;
		break;
	}

	pr_debug("%s: %s\n", success ? "success" : "failed",
			dp_link_get_phy_test_pattern(pattern_requested));
}

static void dp_ctrl_reset(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("invalid params\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);
	ctrl->catalog->reset(ctrl->catalog);
}

static int dp_ctrl_on(struct dp_ctrl *dp_ctrl)
{
	int rc = 0;
	struct dp_ctrl_private *ctrl;
	u32 rate = 0;
	u32 link_train_max_retries = 100;
	u32 const phy_cts_pixel_clk_khz = 148500;

	if (!dp_ctrl) {
		rc = -EINVAL;
		goto end;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	atomic_set(&ctrl->aborted, 0);
	rate = ctrl->panel->link_info.rate;

	ctrl->catalog->hpd_config(ctrl->catalog, true);

	if (ctrl->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN) {
		pr_debug("using phy test link parameters\n");
		if (!ctrl->panel->pinfo.pixel_clk_khz)
			ctrl->pixel_rate = phy_cts_pixel_clk_khz;
	} else {
		ctrl->link->link_params.bw_code =
			drm_dp_link_rate_to_bw_code(rate);
		ctrl->link->link_params.lane_count =
			ctrl->panel->link_info.num_lanes;
		ctrl->pixel_rate = ctrl->panel->pinfo.pixel_clk_khz;
	}

	pr_debug("bw_code=%d, lane_count=%d, pixel_rate=%d\n",
		ctrl->link->link_params.bw_code,
		ctrl->link->link_params.lane_count, ctrl->pixel_rate);

	ctrl->catalog->phy_lane_cfg(ctrl->catalog,
			ctrl->orientation, ctrl->link->link_params.lane_count);

	rc = dp_ctrl_enable_mainlink_clocks(ctrl);
	if (rc)
		goto end;

	reinit_completion(&ctrl->idle_comp);

	dp_ctrl_configure_source_params(ctrl);

	while (--link_train_max_retries && !atomic_read(&ctrl->aborted)) {
		ctrl->catalog->config_msa(ctrl->catalog,
			drm_dp_bw_code_to_link_rate(
			ctrl->link->link_params.bw_code), ctrl->pixel_rate);

		rc = dp_ctrl_setup_main_link(ctrl, true);
		if (!rc)
			break;

		/* try with lower link rate */
		dp_ctrl_link_rate_down_shift(ctrl);

		ctrl->catalog->mainlink_ctrl(ctrl->catalog, false);

		dp_ctrl_disable_mainlink_clocks(ctrl);
		/* hw recommended delay before re-enabling clocks */
		msleep(20);

		dp_ctrl_enable_mainlink_clocks(ctrl);
	}

	if (ctrl->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN)
		dp_ctrl_send_phy_test_pattern(ctrl);

	ctrl->power_on = true;
	pr_debug("End-\n");

end:
	return rc;
}

static void dp_ctrl_off(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl)
		return;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->catalog->mainlink_ctrl(ctrl->catalog, false);
	ctrl->catalog->reset(ctrl->catalog);

	/* Make sure DP is disabled before clk disable */
	wmb();

	dp_ctrl_disable_mainlink_clocks(ctrl);

	ctrl->power_on = false;
	pr_debug("DP off done\n");
}

static void dp_ctrl_isr(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl)
		return;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->catalog->get_interrupt(ctrl->catalog);

	if (ctrl->catalog->isr & DP_CTRL_INTR_READY_FOR_VIDEO)
		dp_ctrl_video_ready(ctrl);

	if (ctrl->catalog->isr & DP_CTRL_INTR_IDLE_PATTERN_SENT)
		dp_ctrl_idle_patterns_sent(ctrl);
}

struct dp_ctrl *dp_ctrl_get(struct dp_ctrl_in *in)
{
	int rc = 0;
	struct dp_ctrl_private *ctrl;
	struct dp_ctrl *dp_ctrl;

	if (!in->dev || !in->panel || !in->aux ||
	    !in->link || !in->catalog) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	ctrl = devm_kzalloc(in->dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl) {
		rc = -ENOMEM;
		goto error;
	}

	init_completion(&ctrl->idle_comp);
	init_completion(&ctrl->video_comp);

	/* in parameters */
	ctrl->parser   = in->parser;
	ctrl->panel    = in->panel;
	ctrl->power    = in->power;
	ctrl->aux      = in->aux;
	ctrl->link     = in->link;
	ctrl->catalog  = in->catalog;
	ctrl->dev  = in->dev;

	dp_ctrl = &ctrl->dp_ctrl;

	/* out parameters */
	dp_ctrl->init      = dp_ctrl_host_init;
	dp_ctrl->deinit    = dp_ctrl_host_deinit;
	dp_ctrl->on        = dp_ctrl_on;
	dp_ctrl->off       = dp_ctrl_off;
	dp_ctrl->push_idle = dp_ctrl_push_idle;
	dp_ctrl->abort     = dp_ctrl_abort;
	dp_ctrl->isr       = dp_ctrl_isr;
	dp_ctrl->reset	   = dp_ctrl_reset;
	dp_ctrl->link_maintenance = dp_ctrl_link_maintenance;
	dp_ctrl->process_phy_test_request = dp_ctrl_process_phy_test_request;

	return dp_ctrl;
error:
	return ERR_PTR(rc);
}

void dp_ctrl_put(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl)
		return;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	devm_kfree(ctrl->dev, ctrl);
}
