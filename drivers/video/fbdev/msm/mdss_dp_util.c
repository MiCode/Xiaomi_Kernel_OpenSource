/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/io.h>
#include <linux/delay.h>

#include "mdss_dp_util.h"

#define HEADER_BYTE_2_BIT	 0
#define PARITY_BYTE_2_BIT	 8
#define HEADER_BYTE_1_BIT	16
#define PARITY_BYTE_1_BIT	24
#define HEADER_BYTE_3_BIT	16
#define PARITY_BYTE_3_BIT	24
#define DP_LS_FREQ_162		162000000
#define DP_LS_FREQ_270		270000000
#define DP_LS_FREQ_540		540000000

enum mdss_dp_pin_assignment {
	PIN_ASSIGNMENT_A,
	PIN_ASSIGNMENT_B,
	PIN_ASSIGNMENT_C,
	PIN_ASSIGNMENT_D,
	PIN_ASSIGNMENT_E,
	PIN_ASSIGNMENT_F,
	PIN_ASSIGNMENT_MAX,
};

static const char *mdss_dp_pin_name(u8 pin)
{
	switch (pin) {
	case PIN_ASSIGNMENT_A: return "PIN_ASSIGNMENT_A";
	case PIN_ASSIGNMENT_B: return "PIN_ASSIGNMENT_B";
	case PIN_ASSIGNMENT_C: return "PIN_ASSIGNMENT_C";
	case PIN_ASSIGNMENT_D: return "PIN_ASSIGNMENT_D";
	case PIN_ASSIGNMENT_E: return "PIN_ASSIGNMENT_E";
	case PIN_ASSIGNMENT_F: return "PIN_ASSIGNMENT_F";
	default: return "UNKNOWN";
	}
}

struct mdss_hw mdss_dp_hw = {
	.hw_ndx = MDSS_HW_EDP,
	.ptr = NULL,
	.irq_handler = dp_isr,
};

/* DP retrieve ctrl HW version */
u32 mdss_dp_get_ctrl_hw_version(struct dss_io_data *ctrl_io)
{
	return readl_relaxed(ctrl_io->base + DP_HW_VERSION);
}

/* DP retrieve phy HW version */
u32 mdss_dp_get_phy_hw_version(struct dss_io_data *phy_io)
{
	return readl_relaxed(phy_io->base + DP_PHY_REVISION_ID3);
}
/* DP PHY SW reset */
void mdss_dp_phy_reset(struct dss_io_data *ctrl_io)
{
	writel_relaxed(0x04, ctrl_io->base + DP_PHY_CTRL); /* bit 2 */
	udelay(1000);
	writel_relaxed(0x00, ctrl_io->base + DP_PHY_CTRL);
}

void mdss_dp_switch_usb3_phy_to_dp_mode(struct dss_io_data *tcsr_reg_io)
{
	writel_relaxed(0x01, tcsr_reg_io->base + TCSR_USB3_DP_PHYMODE);
}

/* DP PHY assert reset for PHY and PLL */
void mdss_dp_assert_phy_reset(struct dss_io_data *ctrl_io, bool assert)
{
	if (assert) {
		/* assert reset line for PHY and PLL */
		writel_relaxed(0x5,
			ctrl_io->base + DP_PHY_CTRL); /* bit 0 & 2 */
	} else {
		/* remove assert for PLL and PHY reset line */
		writel_relaxed(0x00, ctrl_io->base + DP_PHY_CTRL);
	}
}

/* reset AUX */
void mdss_dp_aux_reset(struct dss_io_data *ctrl_io)
{
	u32 aux_ctrl = readl_relaxed(ctrl_io->base + DP_AUX_CTRL);

	aux_ctrl |= BIT(1);
	writel_relaxed(aux_ctrl, ctrl_io->base + DP_AUX_CTRL);
	udelay(1000);
	aux_ctrl &= ~BIT(1);
	writel_relaxed(aux_ctrl, ctrl_io->base + DP_AUX_CTRL);
}

/* reset DP controller */
void mdss_dp_ctrl_reset(struct dss_io_data *ctrl_io)
{
	u32 sw_reset = readl_relaxed(ctrl_io->base + DP_SW_RESET);

	sw_reset |= BIT(0);
	writel_relaxed(sw_reset, ctrl_io->base + DP_SW_RESET);
	udelay(1000);
	sw_reset &= ~BIT(0);
	writel_relaxed(sw_reset, ctrl_io->base + DP_SW_RESET);
}

/* reset DP Mainlink */
void mdss_dp_mainlink_reset(struct dss_io_data *ctrl_io)
{
	u32 mainlink_ctrl = readl_relaxed(ctrl_io->base + DP_MAINLINK_CTRL);

	mainlink_ctrl |= BIT(1);
	writel_relaxed(mainlink_ctrl, ctrl_io->base + DP_MAINLINK_CTRL);
	udelay(1000);
	mainlink_ctrl &= ~BIT(1);
	writel_relaxed(mainlink_ctrl, ctrl_io->base + DP_MAINLINK_CTRL);
}

/* Configure HPD */
void mdss_dp_hpd_configure(struct dss_io_data *ctrl_io, bool enable)
{
	if (enable) {
		u32 reftimer =
			readl_relaxed(ctrl_io->base + DP_DP_HPD_REFTIMER);

		writel_relaxed(0xf, ctrl_io->base + DP_DP_HPD_INT_ACK);
		writel_relaxed(0xf, ctrl_io->base + DP_DP_HPD_INT_MASK);

		/* Enabling REFTIMER */
		reftimer |= BIT(16);
		writel_relaxed(0xf, ctrl_io->base + DP_DP_HPD_REFTIMER);
		/* Enable HPD */
		writel_relaxed(0x1, ctrl_io->base + DP_DP_HPD_CTRL);
	} else {
		/*Disable HPD */
		writel_relaxed(0x0, ctrl_io->base + DP_DP_HPD_CTRL);
	}
}

/* Enable/Disable AUX controller */
void mdss_dp_aux_ctrl(struct dss_io_data *ctrl_io, bool enable)
{
	u32 aux_ctrl = readl_relaxed(ctrl_io->base + DP_AUX_CTRL);

	if (enable)
		aux_ctrl |= BIT(0);
	else
		aux_ctrl &= ~BIT(0);

	writel_relaxed(aux_ctrl, ctrl_io->base + DP_AUX_CTRL);
}

/* DP Mainlink controller*/
void mdss_dp_mainlink_ctrl(struct dss_io_data *ctrl_io, bool enable)
{
	u32 mainlink_ctrl = readl_relaxed(ctrl_io->base + DP_MAINLINK_CTRL);

	if (enable)
		mainlink_ctrl |= BIT(0);
	else
		mainlink_ctrl &= ~BIT(0);

	writel_relaxed(mainlink_ctrl, ctrl_io->base + DP_MAINLINK_CTRL);
}

bool mdss_dp_mainlink_ready(struct mdss_dp_drv_pdata *dp)
{
	u32 data;
	int cnt = 10;
	int const mainlink_ready_bit = BIT(0);

	while (--cnt) {
		/* DP_MAINLINK_READY */
		data = readl_relaxed(dp->base + DP_MAINLINK_READY);
		if (data & mainlink_ready_bit)
			return true;
		udelay(1000);
	}
	pr_err("mainlink not ready\n");

	return false;
}

/* DP Configuration controller*/
void mdss_dp_configuration_ctrl(struct dss_io_data *ctrl_io, u32 data)
{
	writel_relaxed(data, ctrl_io->base + DP_CONFIGURATION_CTRL);
}

void mdss_dp_config_ctl_frame_crc(struct mdss_dp_drv_pdata *dp, bool enable)
{
	if (dp->ctl_crc.en == enable) {
		pr_debug("CTL crc already %s\n",
			enable ? "enabled" : "disabled");
		return;
	}

	writel_relaxed(BIT(8), dp->ctrl_io.base + MMSS_DP_TIMING_ENGINE_EN);
	if (!enable)
		mdss_dp_reset_frame_crc_data(&dp->ctl_crc);
	dp->ctl_crc.en = enable;

	pr_debug("CTL crc %s\n", enable ? "enabled" : "disabled");
}

int mdss_dp_read_ctl_frame_crc(struct mdss_dp_drv_pdata *dp)
{
	u32 data;
	u32 crc_rg = 0;
	struct mdss_dp_crc_data *crc = &dp->ctl_crc;

	data = readl_relaxed(dp->ctrl_io.base + MMSS_DP_TIMING_ENGINE_EN);
	if (!(data & BIT(8))) {
		pr_debug("frame CRC calculation not enabled\n");
		return -EPERM;
	}

	crc_rg = readl_relaxed(dp->ctrl_io.base + MMSS_DP_PSR_CRC_RG);
	crc->r_cr = crc_rg & 0xFFFF;
	crc->g_y = crc_rg >> 16;
	crc->b_cb = readl_relaxed(dp->ctrl_io.base + MMSS_DP_PSR_CRC_B);

	pr_debug("r_cr=0x%08x\t g_y=0x%08x\t b_cb=0x%08x\n",
		crc->r_cr, crc->g_y, crc->b_cb);

	return 0;
}

/* DP state controller*/
void mdss_dp_state_ctrl(struct dss_io_data *ctrl_io, u32 data)
{
	writel_relaxed(data, ctrl_io->base + DP_STATE_CTRL);
}

static void mdss_dp_get_extra_req_bytes(u64 result_valid,
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

static void mdss_dp_calc_tu_parameters(u8 link_rate, u8 ln_cnt,
				struct dp_vc_tu_mapping_table *tu_table,
				struct mdss_panel_info *pinfo)
{
	u32 const multiplier = 1000000;
	u64 pclk, lclk;
	u8 bpp;
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

	u8 dp_brute_force = 1;
	u64 brute_force_threshold = 10;
	u64 diff_abs;

	bpp = pinfo->bpp;
	lwidth = pinfo->xres; /* active width */
	h_blank = pinfo->lcdc.h_back_porch + pinfo->lcdc.h_front_porch +
				pinfo->lcdc.h_pulse_width;
	pclk = pinfo->clk_rate;

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

	lclk = link_rate * DP_LINK_RATE_MULTIPLIER;

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
					mdss_dp_get_extra_req_bytes
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
		pr_err(" WARNING: run_idx=%d Programmed h_blank %d is smaller than the min_hblank %d supported.\n",
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

void mdss_dp_timing_cfg(struct dss_io_data *ctrl_io,
					struct mdss_panel_info *pinfo)
{
	u32 total_ver, total_hor;
	u32 data;

	pr_debug("width=%d hporch= %d %d %d\n",
		pinfo->xres, pinfo->lcdc.h_back_porch,
		pinfo->lcdc.h_front_porch, pinfo->lcdc.h_pulse_width);

	pr_debug("height=%d vporch= %d %d %d\n",
		pinfo->yres, pinfo->lcdc.v_back_porch,
		pinfo->lcdc.v_front_porch, pinfo->lcdc.v_pulse_width);

	total_hor = pinfo->xres + pinfo->lcdc.h_back_porch +
		pinfo->lcdc.h_front_porch + pinfo->lcdc.h_pulse_width;

	total_ver = pinfo->yres + pinfo->lcdc.v_back_porch +
			pinfo->lcdc.v_front_porch + pinfo->lcdc.v_pulse_width;

	data = total_ver;
	data <<= 16;
	data |= total_hor;
	/* DP_TOTAL_HOR_VER */
	writel_relaxed(data, ctrl_io->base + DP_TOTAL_HOR_VER);

	data = (pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width);
	data <<= 16;
	data |= (pinfo->lcdc.h_back_porch + pinfo->lcdc.h_pulse_width);
	/* DP_START_HOR_VER_FROM_SYNC */
	writel_relaxed(data, ctrl_io->base + DP_START_HOR_VER_FROM_SYNC);

	data = pinfo->lcdc.v_pulse_width;
	data <<= 16;
	data |= (pinfo->lcdc.v_active_low << 31);
	data |= pinfo->lcdc.h_pulse_width;
	data |= (pinfo->lcdc.h_active_low << 15);
	/* DP_HSYNC_VSYNC_WIDTH_POLARITY */
	writel_relaxed(data, ctrl_io->base + DP_HSYNC_VSYNC_WIDTH_POLARITY);

	data = pinfo->yres;
	data <<= 16;
	data |= pinfo->xres;
	/* DP_ACTIVE_HOR_VER */
	writel_relaxed(data, ctrl_io->base + DP_ACTIVE_HOR_VER);
}

static bool use_fixed_nvid(struct mdss_dp_drv_pdata *dp)
{
	/*
	 * For better interop experience, used a fixed NVID=0x8000
	 * whenever connected to a VGA dongle downstream
	 */
	return mdss_dp_is_dsp_type_vga(dp);
}

void mdss_dp_sw_config_msa(struct mdss_dp_drv_pdata *dp)
{
	u32 pixel_m, pixel_n;
	u32 mvid, nvid;
	u64 mvid_calc;
	u32 const nvid_fixed = 0x8000;
	struct dss_io_data *ctrl_io = &dp->ctrl_io;
	struct dss_io_data *dp_cc_io = &dp->dp_cc_io;
	u32 lrate_kbps;
	u64 stream_rate_khz;

	if (use_fixed_nvid(dp)) {
		pr_debug("use fixed NVID=0x%x\n", nvid_fixed);
		nvid = nvid_fixed;

		lrate_kbps = dp->link_rate * DP_LINK_RATE_MULTIPLIER /
			DP_KHZ_TO_HZ;
		stream_rate_khz = div_u64(dp->panel_data.panel_info.clk_rate,
			DP_KHZ_TO_HZ);
		pr_debug("link rate=%dkbps, stream_rate_khz=%lluKhz",
			lrate_kbps, stream_rate_khz);

		/*
		 * For intermediate results, use 64 bit arithmetic to avoid
		 * loss of precision.
		 */
		mvid_calc = stream_rate_khz * nvid;
		mvid_calc = div_u64(mvid_calc, lrate_kbps);

		/*
		 * truncate back to 32 bits as this final divided value will
		 * always be within the range of a 32 bit unsigned int.
		 */
		mvid = (u32) mvid_calc;
	} else {
		pixel_m = readl_relaxed(dp_cc_io->base + MMSS_DP_PIXEL_M);
		pixel_n = readl_relaxed(dp_cc_io->base + MMSS_DP_PIXEL_N);
		pr_debug("pixel_m=0x%x, pixel_n=0x%x\n", pixel_m, pixel_n);
		mvid = (pixel_m & 0xFFFF) * 5;
		nvid = (0xFFFF & (~pixel_n)) + (pixel_m & 0xFFFF);
		if (dp->link_rate == DP_LINK_RATE_540)
			nvid *= 2;
	}

	pr_debug("mvid=0x%x, nvid=0x%x\n", mvid, nvid);
	writel_relaxed(mvid, ctrl_io->base + DP_SOFTWARE_MVID);
	writel_relaxed(nvid, ctrl_io->base + DP_SOFTWARE_NVID);
}

void mdss_dp_config_misc(struct mdss_dp_drv_pdata *dp, u32 bd, u32 cc)
{
	u32 misc_val = cc;

	misc_val |= (bd << 5);
	misc_val |= BIT(0); /* Configure clock to synchronous mode */

	pr_debug("Misc settings = 0x%x\n", misc_val);
	writel_relaxed(misc_val, dp->ctrl_io.base + DP_MISC1_MISC0);
}

void mdss_dp_setup_tr_unit(struct dss_io_data *ctrl_io, u8 link_rate,
		u8 ln_cnt, u32 res, struct mdss_panel_info *pinfo)
{
	u32 dp_tu = 0x0;
	u32 valid_boundary = 0x0;
	u32 valid_boundary2 = 0x0;
	struct dp_vc_tu_mapping_table const *tu_entry = tu_table;
	struct dp_vc_tu_mapping_table tu_calc_table;

	for (; tu_entry != tu_table + ARRAY_SIZE(tu_table); ++tu_entry) {
		if ((tu_entry->vic == res) &&
			(tu_entry->lanes == ln_cnt) &&
			(tu_entry->lrate == link_rate))
		break;
	}

	if (tu_entry == tu_table + ARRAY_SIZE(tu_table)) {
		pr_err("requested res=%d, ln_cnt=%d, lrate=0x%x not supported\n",
				res, ln_cnt, link_rate);
	}

	mdss_dp_calc_tu_parameters(link_rate, ln_cnt, &tu_calc_table, pinfo);
	dp_tu |= tu_calc_table.tu_size_minus1;
	valid_boundary |= tu_calc_table.valid_boundary_link;
	valid_boundary |= (tu_calc_table.delay_start_link << 16);

	valid_boundary2 |= (tu_calc_table.valid_lower_boundary_link << 1);
	valid_boundary2 |= (tu_calc_table.upper_boundary_count << 16);
	valid_boundary2 |= (tu_calc_table.lower_boundary_count << 20);

	if (tu_calc_table.boundary_moderation_en)
		valid_boundary2 |= BIT(0);

	writel_relaxed(valid_boundary, ctrl_io->base + DP_VALID_BOUNDARY);
	writel_relaxed(dp_tu, ctrl_io->base + DP_TU);
	writel_relaxed(valid_boundary2, ctrl_io->base + DP_VALID_BOUNDARY_2);

	pr_debug("valid_boundary=0x%x, valid_boundary2=0x%x\n",
				valid_boundary, valid_boundary2);
	pr_debug("dp_tu=0x%x\n", dp_tu);
}

void mdss_dp_aux_set_limits(struct dss_io_data *ctrl_io)
{
	u32 const max_aux_timeout_count = 0xFFFFF;
	u32 const max_aux_limits = 0xFFFFFFFF;

	pr_debug("timeout=0x%x, limits=0x%x\n",
			max_aux_timeout_count, max_aux_limits);

	writel_relaxed(max_aux_timeout_count,
			ctrl_io->base + DP_AUX_TIMEOUT_COUNT);
	writel_relaxed(max_aux_limits, ctrl_io->base + DP_AUX_LIMITS);
}

void mdss_dp_phy_aux_update_config(struct mdss_dp_drv_pdata *dp,
		enum dp_phy_aux_config_type config_type)
{
	u32 new_index;
	struct dss_io_data *phy_io = &dp->phy_io;
	struct mdss_dp_phy_cfg *cfg = mdss_dp_phy_aux_get_config(dp,
			config_type);

	if (!cfg) {
		pr_err("invalid config type %s",
			mdss_dp_phy_aux_config_type_to_string(config_type));
		return;
	}

	new_index = (cfg->current_index + 1) % cfg->cfg_cnt;

	pr_debug("Updating %s from 0x%08x to 0x%08x\n",
		mdss_dp_phy_aux_config_type_to_string(config_type),
		cfg->lut[cfg->current_index], cfg->lut[new_index]);
	writel_relaxed(cfg->lut[new_index], phy_io->base + cfg->offset);
	cfg->current_index = new_index;

	/* Make sure the new HW configuration takes effect */
	wmb();

	/* Reset the AUX controller before any subsequent transactions */
	mdss_dp_aux_reset(&dp->ctrl_io);
}

void mdss_dp_ctrl_lane_mapping(struct dss_io_data *ctrl_io, char *l_map)
{
	u8 bits_per_lane = 2;
	u32 lane_map = ((l_map[0] << (bits_per_lane * 0))
			    | (l_map[1] << (bits_per_lane * 1))
			    | (l_map[2] << (bits_per_lane * 2))
			    | (l_map[3] << (bits_per_lane * 3)));
	pr_debug("%s: lane mapping reg = 0x%x\n", __func__, lane_map);
	writel_relaxed(lane_map,
		ctrl_io->base + DP_LOGICAL2PHYSCIAL_LANE_MAPPING);
}

void mdss_dp_phy_aux_setup(struct mdss_dp_drv_pdata *dp)
{
	int i;
	void __iomem *adjusted_phy_io_base = dp->phy_io.base +
		dp->phy_reg_offset;

	writel_relaxed(0x3d, adjusted_phy_io_base + DP_PHY_PD_CTL);

	for (i = 0; i < PHY_AUX_CFG_MAX; i++) {
		struct mdss_dp_phy_cfg *cfg = mdss_dp_phy_aux_get_config(dp, i);

		pr_debug("%s: offset=0x%08x, value=0x%08x\n",
			mdss_dp_phy_aux_config_type_to_string(i), cfg->offset,
			cfg->lut[cfg->current_index]);
		writel_relaxed(cfg->lut[cfg->current_index],
				dp->phy_io.base + cfg->offset);
	};
	writel_relaxed(0x1e, adjusted_phy_io_base + DP_PHY_AUX_INTERRUPT_MASK);
}

int mdss_dp_irq_setup(struct mdss_dp_drv_pdata *dp_drv)
{
	int ret = 0;

	mdss_dp_hw.irq_info = mdss_intr_line();
	if (mdss_dp_hw.irq_info == NULL) {
		pr_err("Failed to get mdss irq information\n");
		return -ENODEV;
	}

	mdss_dp_hw.ptr = (void *)(dp_drv);

	ret = dp_drv->mdss_util->register_irq(&mdss_dp_hw);
	if (ret)
		pr_err("mdss_register_irq failed.\n");

	return ret;
}

void mdss_dp_irq_enable(struct mdss_dp_drv_pdata *dp_drv)
{
	unsigned long flags;

	spin_lock_irqsave(&dp_drv->lock, flags);
	writel_relaxed(dp_drv->mask1, dp_drv->base + DP_INTR_STATUS);
	writel_relaxed(dp_drv->mask2, dp_drv->base + DP_INTR_STATUS2);
	spin_unlock_irqrestore(&dp_drv->lock, flags);

	dp_drv->mdss_util->enable_irq(&mdss_dp_hw);
}

void mdss_dp_irq_disable(struct mdss_dp_drv_pdata *dp_drv)
{
	unsigned long flags;

	spin_lock_irqsave(&dp_drv->lock, flags);
	writel_relaxed(0x00, dp_drv->base + DP_INTR_STATUS);
	writel_relaxed(0x00, dp_drv->base + DP_INTR_STATUS2);
	spin_unlock_irqrestore(&dp_drv->lock, flags);

	dp_drv->mdss_util->disable_irq(&mdss_dp_hw);
}

static void mdss_dp_initialize_s_port(enum dp_port_cap *s_port, int port)
{
	switch (port) {
	case 0:
		*s_port = PORT_NONE;
		break;
	case 1:
		*s_port = PORT_UFP_D;
		break;
	case 2:
		*s_port = PORT_DFP_D;
		break;
	case 3:
		*s_port = PORT_D_UFP_D;
		break;
	default:
		*s_port = PORT_NONE;
	}
}

void mdss_dp_usbpd_ext_capabilities(struct usbpd_dp_capabilities *dp_cap)
{
	u32 buf = dp_cap->response;
	int port = buf & 0x3;

	dp_cap->receptacle_state =
			(buf & BIT(6)) ? true : false;

	dp_cap->dlink_pin_config =
			(buf >> 8) & 0xff;

	dp_cap->ulink_pin_config =
			(buf >> 16) & 0xff;

	mdss_dp_initialize_s_port(&dp_cap->s_port, port);
}

void mdss_dp_usbpd_ext_dp_status(struct usbpd_dp_status *dp_status)
{
	u32 buf = dp_status->response;
	int port = buf & 0x3;

	dp_status->low_pow_st =
			(buf & BIT(2)) ? true : false;

	dp_status->adaptor_dp_en =
			(buf & BIT(3)) ? true : false;

	dp_status->multi_func =
			(buf & BIT(4)) ? true : false;

	dp_status->switch_to_usb_config =
			(buf & BIT(5)) ? true : false;

	dp_status->exit_dp_mode =
			(buf & BIT(6)) ? true : false;

	dp_status->hpd_high =
			(buf & BIT(7)) ? true : false;

	dp_status->hpd_irq =
			(buf & BIT(8)) ? true : false;

	pr_debug("low_pow_st = %d, adaptor_dp_en = %d, multi_func = %d\n",
			dp_status->low_pow_st, dp_status->adaptor_dp_en,
			dp_status->multi_func);
	pr_debug("switch_to_usb_config = %d, exit_dp_mode = %d, hpd_high =%d\n",
			dp_status->switch_to_usb_config,
			dp_status->exit_dp_mode, dp_status->hpd_high);
	pr_debug("hpd_irq = %d\n", dp_status->hpd_irq);

	mdss_dp_initialize_s_port(&dp_status->c_port, port);
}

u32 mdss_dp_usbpd_gen_config_pkt(struct mdss_dp_drv_pdata *dp)
{
	u8 pin_cfg, pin;
	u32 config = 0;

	pin_cfg = dp->alt_mode.dp_cap.dlink_pin_config;

	for (pin = PIN_ASSIGNMENT_A; pin < PIN_ASSIGNMENT_MAX; pin++) {
		if (pin_cfg & BIT(pin)) {
			if (dp->alt_mode.dp_status.multi_func) {
				if (pin == PIN_ASSIGNMENT_D)
					break;
			} else {
				break;
			}
		}
	}

	if (pin == PIN_ASSIGNMENT_MAX)
		pin = PIN_ASSIGNMENT_C;

	pr_debug("pin assignment: %s\n", mdss_dp_pin_name(pin));

	config |= BIT(pin) << 8;

	config |= (0x1 << 2); /* configure for DPv1.3 */
	config |= 0x2; /* Configuring for UFP_D */

	pr_debug("DP config = 0x%x\n", config);
	return config;
}

void mdss_dp_phy_share_lane_config(struct dss_io_data *phy_io,
		u8 orientation, u8 ln_cnt, u32 phy_reg_offset)
{
	u32 info = 0x0;

	info |= (ln_cnt & 0x0F);
	info |= ((orientation & 0x0F) << 4);
	pr_debug("Shared Info = 0x%x\n", info);
	writel_relaxed(info, phy_io->base + phy_reg_offset + DP_PHY_SPARE0);
}

void mdss_dp_config_audio_acr_ctrl(struct dss_io_data *ctrl_io, char link_rate)
{
	u32 acr_ctrl = 0;
	u32 select = 0;

	switch (link_rate) {
	case DP_LINK_RATE_162:
		select = 0;
		break;
	case DP_LINK_RATE_270:
		select = 1;
		break;
	case DP_LINK_RATE_540:
		select = 2;
		break;
	default:
		pr_debug("Unknown link rate\n");
		select = 0;
		break;
	}

	acr_ctrl |= select << 4 | BIT(31) | BIT(8) | BIT(14);

	pr_debug("select = 0x%x, acr_ctrl = 0x%x\n", select, acr_ctrl);

	writel_relaxed(acr_ctrl, ctrl_io->base + MMSS_DP_AUDIO_ACR_CTRL);
}

static u8 mdss_dp_get_g0_value(u8 data)
{
	u8 c[4];
	u8 g[4];
	u8 rData = 0;
	u8 i;

	for (i = 0; i < 4; i++)
		c[i] = (data >> i) & 0x01;

	g[0] = c[3];
	g[1] = c[0] ^ c[3];
	g[2] = c[1];
	g[3] = c[2];

	for (i = 0; i < 4; i++)
		rData = ((g[i] & 0x01) << i) | rData;

	return rData;
}

static u8 mdss_dp_get_g1_value(u8 data)
{
	u8 c[4];
	u8 g[4];
	u8 rData = 0;
	u8 i;

	for (i = 0; i < 4; i++)
		c[i] = (data >> i) & 0x01;

	g[0] = c[0] ^ c[3];
	g[1] = c[0] ^ c[1] ^ c[3];
	g[2] = c[1] ^ c[2];
	g[3] = c[2] ^ c[3];

	for (i = 0; i < 4; i++)
		rData = ((g[i] & 0x01) << i) | rData;

	return rData;
}

static u8 mdss_dp_calculate_parity_byte(u32 data)
{
	u8 x0 = 0;
	u8 x1 = 0;
	u8 ci = 0;
	u8 iData = 0;
	u8 i = 0;
	u8 parityByte;
	u8 num_byte = (data & 0xFF00) > 0 ? 8 : 2;

	for (i = 0; i < num_byte; i++) {
		iData = (data >> i*4) & 0xF;

		ci = iData ^ x1;
		x1 = x0 ^ mdss_dp_get_g1_value(ci);
		x0 = mdss_dp_get_g0_value(ci);
	}

	parityByte = x1 | (x0 << 4);

	return parityByte;
}

static void mdss_dp_audio_setup_audio_stream_sdp(struct dss_io_data *ctrl_io,
		u32 num_of_channels)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_STREAM_0);
	value &= 0x0000ffff;
	new_value = 0x02;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_STREAM_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);
	value &= 0xffff0000;
	new_value = 0x00;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);
	value &= 0x0000ffff;
	new_value = num_of_channels - 1;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);

}

static void mdss_dp_audio_setup_audio_timestamp_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_0);
	value &= 0x0000ffff;
	new_value = 0x1;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);
	value &= 0xffff0000;
	new_value = 0x17;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);
	value &= 0x0000ffff;
	new_value = (0x0 | (0x11 << 2));
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);
}

static void mdss_dp_audio_setup_audio_infoframe_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_0);
	value &= 0x0000ffff;
	new_value = 0x84;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);
	value &= 0xffff0000;
	new_value = 0x1b;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);
	value &= 0x0000ffff;
	new_value = (0x0 | (0x11 << 2));
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			new_value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);

	/* Config Data Byte 0 - 2 as "Refer to Stream Header" */
	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_2);
}

static void mdss_dp_audio_setup_copy_management_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_0);
	value &= 0x0000ffff;
	new_value = 0x05;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);
	value &= 0xffff0000;
	new_value = 0x0F;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);
	value &= 0x0000ffff;
	new_value = 0x0;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);

	writel_relaxed(0x0, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_2);
	writel_relaxed(0x0, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_3);
	writel_relaxed(0x0, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_4);
}

static void mdss_dp_audio_setup_isrc_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_ISRC_0);
	value &= 0x0000ffff;
	new_value = 0x06;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_ISRC_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_ISRC_1);
	value &= 0xffff0000;
	new_value = 0x0F;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_ISRC_1);

	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_ISRC_2);
	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_ISRC_3);
	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_ISRC_4);
}

void mdss_dp_audio_setup_sdps(struct dss_io_data *ctrl_io, u32 num_of_channels)
{
	u32 sdp_cfg = 0;
	u32 sdp_cfg2 = 0;

	/* AUDIO_TIMESTAMP_SDP_EN */
	sdp_cfg |= BIT(1);
	/* AUDIO_STREAM_SDP_EN */
	sdp_cfg |= BIT(2);
	/* AUDIO_COPY_MANAGEMENT_SDP_EN */
	sdp_cfg |= BIT(5);
	/* AUDIO_ISRC_SDP_EN  */
	sdp_cfg |= BIT(6);
	/* AUDIO_INFOFRAME_SDP_EN  */
	sdp_cfg |= BIT(20);

	writel_relaxed(sdp_cfg, ctrl_io->base + MMSS_DP_SDP_CFG);

	sdp_cfg2 = readl_relaxed(ctrl_io->base + MMSS_DP_SDP_CFG2);
	/* IFRM_REGSRC -> Do not use reg values */
	sdp_cfg2 &= ~BIT(0);
	/* AUDIO_STREAM_HB3_REGSRC-> Do not use reg values */
	sdp_cfg2 &= ~BIT(1);

	writel_relaxed(sdp_cfg2, ctrl_io->base + MMSS_DP_SDP_CFG2);

	mdss_dp_audio_setup_audio_stream_sdp(ctrl_io, num_of_channels);
	mdss_dp_audio_setup_audio_timestamp_sdp(ctrl_io);
	mdss_dp_audio_setup_audio_infoframe_sdp(ctrl_io);
	mdss_dp_audio_setup_copy_management_sdp(ctrl_io);
	mdss_dp_audio_setup_isrc_sdp(ctrl_io);
}

void mdss_dp_set_safe_to_exit_level(struct dss_io_data *ctrl_io,
		uint32_t lane_cnt)
{
	u32 safe_to_exit_level = 0;
	u32 mainlink_levels = 0;

	switch (lane_cnt) {
	case 1:
		safe_to_exit_level = 14;
		break;
	case 2:
		safe_to_exit_level = 8;
		break;
	case 4:
		safe_to_exit_level = 5;
		break;
	default:
		pr_debug("setting the default safe_to_exit_level = %u\n",
				safe_to_exit_level);
		safe_to_exit_level = 14;
		break;
	}

	mainlink_levels = readl_relaxed(ctrl_io->base + DP_MAINLINK_LEVELS);
	mainlink_levels &= 0xFE0;
	mainlink_levels |= safe_to_exit_level;

	pr_debug("mainlink_level = 0x%x, safe_to_exit_level = 0x%x\n",
			mainlink_levels, safe_to_exit_level);

	writel_relaxed(mainlink_levels, ctrl_io->base + DP_MAINLINK_LEVELS);
}

void mdss_dp_audio_enable(struct dss_io_data *ctrl_io, bool enable)
{
	u32 audio_ctrl = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_CFG);

	if (enable)
		audio_ctrl |= BIT(0);
	else
		audio_ctrl &= ~BIT(0);

	writel_relaxed(audio_ctrl, ctrl_io->base + MMSS_DP_AUDIO_CFG);
}

/**
 * mdss_dp_phy_send_test_pattern() - sends the requested PHY test pattern
 * @ep: Display Port Driver data
 *
 * Updates the DP controller state and sends the requested PHY test pattern
 * to the sink.
 */
void mdss_dp_phy_send_test_pattern(struct mdss_dp_drv_pdata *dp)
{
	struct dss_io_data *io = &dp->ctrl_io;
	u32 phy_test_pattern_sel = dp->test_data.phy_test_pattern_sel;
	u32 value = 0x0;

	if (!mdss_dp_is_phy_test_pattern_supported(phy_test_pattern_sel)) {
		pr_err("test pattern 0x%x not supported\n",
				phy_test_pattern_sel);
		return;
	}

	/* Initialize DP state control */
	writel_relaxed(0x0, io->base + DP_STATE_CTRL);

	pr_debug("phy_test_pattern_sel = %s\n",
			mdss_dp_get_phy_test_pattern(phy_test_pattern_sel));

	switch (phy_test_pattern_sel) {
	case PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING:
		writel_relaxed(0x1, io->base + DP_STATE_CTRL);
		break;
	case PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT:
		value &= ~(1 << 16);
		writel_relaxed(value, io->base +
				DP_HBR2_COMPLIANCE_SCRAMBLER_RESET);
		value |= 0xFC;
		writel_relaxed(value, io->base +
				DP_HBR2_COMPLIANCE_SCRAMBLER_RESET);
		writel_relaxed(0x2, io->base + DP_MAINLINK_LEVELS);
		mdss_dp_state_ctrl(io, BIT(4));
		break;
	case PHY_TEST_PATTERN_PRBS7:
		writel_relaxed(0x20, io->base + DP_STATE_CTRL);
		break;
	case PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN:
		mdss_dp_state_ctrl(io, BIT(6));
		/* 00111110000011111000001111100000 */
		writel_relaxed(0x3E0F83E0, io->base +
				DP_TEST_80BIT_CUSTOM_PATTERN_REG0);
		/* 00001111100000111110000011111000 */
		writel_relaxed(0x0F83E0F8, io->base +
				DP_TEST_80BIT_CUSTOM_PATTERN_REG1);
		/* 1111100000111110 */
		writel_relaxed(0x0000F83E, io->base +
				DP_TEST_80BIT_CUSTOM_PATTERN_REG2);
		break;
	case PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN:
		value = BIT(16);
		writel_relaxed(value, io->base +
				DP_HBR2_COMPLIANCE_SCRAMBLER_RESET);
		value |= 0xFC;
		writel_relaxed(value, io->base +
				DP_HBR2_COMPLIANCE_SCRAMBLER_RESET);
		writel_relaxed(0x2, io->base + DP_MAINLINK_LEVELS);
		mdss_dp_state_ctrl(io, BIT(4));
		break;
	default:
		pr_debug("No valid test pattern requested: 0x%x\n",
				phy_test_pattern_sel);
		return;
	}

	value = 0x0;
	value = readl_relaxed(io->base + DP_MAINLINK_READY);
	pr_info("DP_MAINLINK_READY = 0x%x\n", value);
}
