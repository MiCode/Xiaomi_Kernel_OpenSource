/*
 * drivers/video/tegra/dc/bandwidth.c
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Jon Mayo <jmayo@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/mc.h>
#include <linux/nvhost.h>
#include <mach/latency_allowance.h>
#include <trace/events/display.h>

#include "dc_reg.h"
#include "dc_config.h"
#include "dc_priv.h"

static int use_dynamic_emc = 1;

module_param_named(use_dynamic_emc, use_dynamic_emc, int, S_IRUGO | S_IWUSR);

#ifdef CONFIG_ARCH_TEGRA_12x_SOC
static unsigned int tegra_dcs_total_bw[TEGRA_MAX_DC] = {0};
DEFINE_MUTEX(tegra_dcs_total_bw_lock);
#endif

/* windows A, B, C for first and second display */
static const enum tegra_la_id la_id_tab[2][DC_N_WINDOWS] = {
	/* first display */
	{
		TEGRA_LA_DISPLAY_0A,
		TEGRA_LA_DISPLAY_0B,
		TEGRA_LA_DISPLAY_0C,
#if defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
		TEGRA_LA_DISPLAYD,
#endif
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
		TEGRA_LA_DISPLAY_HC,
#endif
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
		TEGRA_LA_DISPLAY_T,
#endif
	},
	/* second display */
	{
		TEGRA_LA_DISPLAY_0AB,
		TEGRA_LA_DISPLAY_0BB,
		TEGRA_LA_DISPLAY_0CB,
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
		0,
		TEGRA_LA_DISPLAY_HCB,
#endif
	},
};

#ifdef CONFIG_ARCH_TEGRA_12x_SOC
static bool is_internal_win(enum tegra_la_id id)
{
	return ((id == TEGRA_LA_DISPLAY_0A) || (id == TEGRA_LA_DISPLAY_0B) ||
		(id == TEGRA_LA_DISPLAY_0C) || (id == TEGRA_LA_DISPLAYD) ||
		(id == TEGRA_LA_DISPLAY_HC) || (id == TEGRA_LA_DISPLAY_T));
}

static unsigned int num_active_internal_wins(struct tegra_dc *dc)
{
	unsigned int num_active_internal_wins = 0;
	int i = 0;

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *curr_win = &dc->windows[i];
		enum tegra_la_id curr_win_la_id =
				la_id_tab[dc->ndev->id][curr_win->idx];

		if (!is_internal_win(curr_win_la_id))
			continue;

		if (WIN_IS_ENABLED(curr_win))
			num_active_internal_wins++;
	}

	return num_active_internal_wins;
}

static unsigned int num_active_external_wins(struct tegra_dc *dc)
{
	unsigned int num_active_external_wins = 0;
	int i = 0;

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *curr_win = &dc->windows[i];
		enum tegra_la_id curr_win_la_id =
				la_id_tab[dc->ndev->id][curr_win->idx];

		if (is_internal_win(curr_win_la_id))
			continue;

		if (WIN_IS_ENABLED(curr_win))
			num_active_external_wins++;
	}

	return num_active_external_wins;
}

/*
 * Note about fixed point arithmetic:
 * ----------------------------------
 * calc_disp_params(...) contains fixed point values and arithmetic due to the
 * need to use floating point values. All fixed point values have the "_fp" or
 * "_FP" suffix in their name. Functions/values used to convert between real and
 * fixed point values are listed below:
 *    - la_params.fp_factor
 *    - la_params.la_real_to_fp(real_val)
 *    - la_params.la_fp_to_real(fp_val)
 */

#define T12X_LA_BW_DISRUPTION_TIME_EMCCLKS_FP			1342000
#define T12X_LA_STATIC_LA_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP	54000
#define T12X_LA_CONS_MEM_EFFICIENCY_FP				500
#define T12X_LA_ROW_SRT_SZ_BYTES	(64 * (T12X_LA_MC_EMEM_NUM_SLOTS + 1))
#define T12X_LA_MC_EMEM_NUM_SLOTS				63
#define T12X_LA_MAX_DRAIN_TIME_USEC				10

/*
 * Function outputs:
 *    - disp_params->thresh_lwm_bytes
 *    - disp_params->spool_up_buffering_adj_bytes
 *    - disp_params->total_dc0_bw
 *    - disp_params->total_dc1_bw
 */
static void calc_disp_params(struct tegra_dc *dc,
				struct tegra_dc_win *w,
				enum tegra_la_id la_id,
				unsigned int bw_mbps,
				struct dc_to_la_params *disp_params) {
	const struct disp_client *disp_clients_info =
						tegra_la_disp_clients_info;
	struct la_to_dc_params la_params = tegra_get_la_to_dc_params();
	unsigned int bw_mbps_fp = la_params.la_real_to_fp(bw_mbps);
	bool active = WIN_IS_ENABLED(w);
	bool win_rotated = false;
	unsigned int window_width = dfixed_trunc(w->w);
	unsigned int surface_width = 0;
	bool vertical_scaling_enabled = false;
	bool pitch = !WIN_IS_BLOCKLINEAR(w) && !WIN_IS_TILED(w);
	bool planar = tegra_dc_is_yuv_planar(w->fmt);
	bool packed_yuv422 = ((w->fmt == TEGRA_WIN_FMT_YCbCr422) ||
				(w->fmt == TEGRA_WIN_FMT_YUV422));
	/* all of tegra's YUV formats(420 and 422) fetch 2 bytes per pixel,
	 * but the size reported by tegra_dc_fmt_bpp for the planar version
	 * is of the luma plane's size only. */
	unsigned int bytes_per_pixel = tegra_dc_is_yuv_planar(w->fmt) ?
				2 * tegra_dc_fmt_bpp(w->fmt) / 8 :
				tegra_dc_fmt_bpp(w->fmt) / 8;
	struct tegra_dc_mode mode = dc->mode;
	unsigned int total_h = mode.h_active +
				mode.h_front_porch +
				mode.h_back_porch +
				mode.h_sync_width;
	unsigned int total_v = mode.v_active +
				mode.v_front_porch +
				mode.v_back_porch +
				mode.v_sync_width;
	unsigned int total_screen_area = total_h * total_v;
	unsigned int total_active_area = mode.h_active * mode.v_active;
	unsigned int total_blank_area = total_screen_area - total_active_area;
	unsigned int reqd_buffering_thresh_disp_bytes_fp = 0;
	unsigned int latency_buffering_available_in_reqd_buffering_fp = 0;
	struct clk *emc_clk = clk_get(NULL, "emc");
	unsigned long emc_freq_mhz = clk_get_rate(emc_clk)/1000000;
	unsigned int bw_disruption_time_usec_fp =
					T12X_LA_BW_DISRUPTION_TIME_EMCCLKS_FP /
					emc_freq_mhz;
	unsigned int effective_row_srt_sz_bytes_fp =
		min((unsigned long)la_params.la_real_to_fp(min(
					(unsigned long)T12X_LA_ROW_SRT_SZ_BYTES,
					16 * min(emc_freq_mhz + 50,
						400ul))),
			((T12X_LA_MAX_DRAIN_TIME_USEC *
			emc_freq_mhz -
			la_params.la_fp_to_real(
			T12X_LA_STATIC_LA_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP) *
			2 *
			la_params.dram_width_bits /
			8 *
			T12X_LA_CONS_MEM_EFFICIENCY_FP)));
	unsigned int drain_time_usec_fp =
			effective_row_srt_sz_bytes_fp *
			la_params.fp_factor /
			(emc_freq_mhz *
				la_params.dram_width_bits /
				4 *
				T12X_LA_CONS_MEM_EFFICIENCY_FP) +
			T12X_LA_STATIC_LA_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP /
			emc_freq_mhz;
	unsigned int total_latency_usec_fp =
		drain_time_usec_fp +
		la_params.static_la_minus_snap_arb_to_row_srt_emcclks_fp /
		emc_freq_mhz;
	unsigned int bw_disruption_buffering_bytes_fp =
					bw_mbps *
					max(bw_disruption_time_usec_fp,
						total_latency_usec_fp) +
					la_params.la_real_to_fp(1);
	unsigned int reqd_lines = 0;
	unsigned int lines_of_latency = 0;
	unsigned int thresh_lwm_bytes = 0;
	unsigned int total_buf_sz_bytes =
	disp_clients_info[DISP_CLIENT_LA_ID(la_id)].line_buf_sz_bytes +
	disp_clients_info[DISP_CLIENT_LA_ID(la_id)].mccif_size_bytes;
	unsigned int num_active_wins_to_use = is_internal_win(la_id) ?
						num_active_internal_wins(dc) :
						num_active_external_wins(dc);
	unsigned int total_active_space_bw = 0;
	unsigned int total_vblank_bw = 0;
	unsigned int bw_other_wins = 0;
	unsigned int bw_display_fp = 0;
	unsigned int bw_delta_fp = 0;
	unsigned int fill_rate_other_wins_fp = 0;
	unsigned int dvfs_time_nsec = tegra_get_dvfs_time_nsec(emc_freq_mhz);
	unsigned int data_shortfall_other_wins_fp = 0;
	unsigned int duration_usec_fp = 0;
	unsigned int spool_up_buffering_adj_bytes = 0;
	unsigned int curr_dc_head_bw = 0;

	if (w->flags & TEGRA_WIN_FLAG_SCAN_COLUMN) {
		win_rotated = true;
		surface_width = dfixed_trunc(w->h);
		vertical_scaling_enabled = (dfixed_trunc(w->w) == w->out_w) ?
					false : true;
	} else {
		win_rotated = false;
		surface_width = dfixed_trunc(w->w);
		vertical_scaling_enabled = (dfixed_trunc(w->h) == w->out_h) ?
					false : true;
	}

	if ((disp_clients_info[DISP_CLIENT_LA_ID(la_id)].line_buf_sz_bytes
									== 0) ||
		(pitch == true)) {
		reqd_lines = 0;
	} else if (win_rotated && planar) {
		if (vertical_scaling_enabled)
			reqd_lines = 17;
		else
			reqd_lines = 16;
	} else {
		if (win_rotated) {
			if (vertical_scaling_enabled)
				reqd_lines = 16 / bytes_per_pixel + 1;
			else
				reqd_lines = 16 / bytes_per_pixel;
		} else {
			if (vertical_scaling_enabled)
				reqd_lines = 3;
			else
				reqd_lines = 1;
		}
	}

	if (reqd_lines > 0 && !vertical_scaling_enabled && win_rotated)
		lines_of_latency = 1;
	else
		lines_of_latency = 0;


	if ((DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_0A) ||
		(DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_0B) ||
		(DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_0C)) {
		unsigned int c1 =
			((w->fmt == TEGRA_WIN_FMT_YUV420P &&
				win_rotated == false) ||
			(tegra_dc_is_yuv(w->fmt) && win_rotated == true)) ?
			3 : bytes_per_pixel;
		unsigned int c2 = (w->fmt == packed_yuv422 &&
					win_rotated == true) ? 2 : 1;

		reqd_buffering_thresh_disp_bytes_fp =
					la_params.la_real_to_fp(active *
							surface_width *
							reqd_lines *
							c1 *
							c2);
		latency_buffering_available_in_reqd_buffering_fp =
					la_params.la_real_to_fp(active *
							surface_width *
							lines_of_latency *
							c1 *
							c2);

		if ((w->fmt == TEGRA_WIN_FMT_YUV422R) &&
			(win_rotated == false)) {
			reqd_buffering_thresh_disp_bytes_fp =
				reqd_buffering_thresh_disp_bytes_fp * 5 / 2;
			latency_buffering_available_in_reqd_buffering_fp =
			latency_buffering_available_in_reqd_buffering_fp *
			5 /
			2;
		}
	} else if ((DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAYD) ||
			(DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_T) ||
			(DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_HC)) {
		reqd_buffering_thresh_disp_bytes_fp =
					la_params.la_real_to_fp(active *
							bytes_per_pixel *
							window_width *
							reqd_lines);
		latency_buffering_available_in_reqd_buffering_fp =
					la_params.la_real_to_fp(active *
							bytes_per_pixel *
							window_width *
							lines_of_latency);
	} else if ((DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_0AB) ||
			(DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_0BB) ||
			(DISP_CLIENT_LA_ID(la_id) == TEGRA_LA_DISPLAY_0CB)) {
		unsigned int c = (w->fmt == TEGRA_WIN_FMT_YUV420P) ?
					3 : bytes_per_pixel;
		reqd_buffering_thresh_disp_bytes_fp =
					la_params.la_real_to_fp(c *
							window_width *
							reqd_lines *
							active);
		latency_buffering_available_in_reqd_buffering_fp =
					la_params.la_real_to_fp(c *
							window_width *
							lines_of_latency *
							active);

		if (w->fmt == TEGRA_WIN_FMT_YUV422R) {
			reqd_buffering_thresh_disp_bytes_fp =
				reqd_buffering_thresh_disp_bytes_fp * 5 / 2;
			latency_buffering_available_in_reqd_buffering_fp =
			latency_buffering_available_in_reqd_buffering_fp *
			5 /
			2;
		}
	}


	thresh_lwm_bytes =
		la_params.la_fp_to_real(
			reqd_buffering_thresh_disp_bytes_fp +
			bw_disruption_buffering_bytes_fp -
			latency_buffering_available_in_reqd_buffering_fp);
	disp_params->thresh_lwm_bytes = thresh_lwm_bytes;


	if (is_internal_win(la_id)) {
		int i = 0;

		for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
			struct tegra_dc_win *curr_win = &dc->windows[i];
			enum tegra_la_id curr_win_la_id =
					la_id_tab[dc->ndev->id][curr_win->idx];
			unsigned int curr_win_bw = 0;

			if (!is_internal_win(curr_win_la_id))
				continue;

			curr_win_bw = max(curr_win->bandwidth,
						curr_win->new_bandwidth);
			/* our bandwidth is in kbytes/sec, but LA takes MBps.
			 * round up bandwidth to next 1MBps */
			if (curr_win_bw != UINT_MAX)
				curr_win_bw = curr_win_bw / 1000 + 1;

			total_active_space_bw += curr_win_bw;
		}
	} else {
		int i = 0;

		for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
			struct tegra_dc_win *curr_win = &dc->windows[i];
			enum tegra_la_id curr_win_la_id =
					la_id_tab[dc->ndev->id][curr_win->idx];
			unsigned int curr_win_bw = 0;

			if (is_internal_win(curr_win_la_id))
				continue;

			curr_win_bw = max(curr_win->bandwidth,
						curr_win->new_bandwidth);
			/* our bandwidth is in kbytes/sec, but LA takes MBps.
			 * round up bandwidth to next 1MBps */
			if (curr_win_bw != UINT_MAX)
				curr_win_bw = curr_win_bw / 1000 + 1;

			total_active_space_bw += curr_win_bw;
		}
	}


	if ((disp_clients_info[DISP_CLIENT_LA_ID(la_id)].win_type  ==
						TEGRA_LA_DISP_WIN_TYPE_FULL) ||
		(disp_clients_info[DISP_CLIENT_LA_ID(la_id)].win_type  ==
						TEGRA_LA_DISP_WIN_TYPE_FULLA) ||
		(disp_clients_info[DISP_CLIENT_LA_ID(la_id)].win_type  ==
						TEGRA_LA_DISP_WIN_TYPE_FULLB)) {
		total_vblank_bw = total_buf_sz_bytes / total_blank_area;
	} else {
		total_vblank_bw = 0;
	}


	bw_display_fp = la_params.disp_catchup_factor_fp *
			max(total_active_space_bw,
				total_vblank_bw);
	if (active)
		bw_delta_fp = bw_mbps_fp -
				(bw_display_fp /
				num_active_wins_to_use);


	bw_other_wins = total_active_space_bw - bw_mbps;

	if (num_active_wins_to_use > 0) {
		fill_rate_other_wins_fp =
				bw_display_fp *
				(num_active_wins_to_use - active) /
				num_active_wins_to_use -
				la_params.la_real_to_fp(bw_other_wins);
	} else {
		fill_rate_other_wins_fp = 0;
	}

	data_shortfall_other_wins_fp = dvfs_time_nsec *
					bw_other_wins *
					la_params.fp_factor /
					1000;

	duration_usec_fp = (fill_rate_other_wins_fp == 0) ? 0 :
				data_shortfall_other_wins_fp *
				la_params.fp_factor /
				fill_rate_other_wins_fp;


	spool_up_buffering_adj_bytes = (bw_delta_fp > 0) ?
					(bw_delta_fp *
					duration_usec_fp /
					(la_params.fp_factor *
					la_params.fp_factor)) :
					0;
	disp_params->spool_up_buffering_adj_bytes =
						spool_up_buffering_adj_bytes;

	mutex_lock(&tegra_dcs_total_bw_lock);
	curr_dc_head_bw = max(dc->new_bw_kbps, dc->bw_kbps);
	/* our bandwidth is in kbytes/sec, but LA takes MBps.
	 * round up bandwidth to next 1MBps */
	if (curr_dc_head_bw != ULONG_MAX)
		curr_dc_head_bw = curr_dc_head_bw / 1000 + 1;
	tegra_dcs_total_bw[dc->ndev->id] = curr_dc_head_bw;
	disp_params->total_dc0_bw = tegra_dcs_total_bw[0];
	disp_params->total_dc1_bw = tegra_dcs_total_bw[1];
	mutex_unlock(&tegra_dcs_total_bw_lock);
}
#endif


/* uses the larger of w->bandwidth or w->new_bandwidth */
static void tegra_dc_set_latency_allowance(struct tegra_dc *dc,
	struct tegra_dc_win *w)
{
	unsigned long bw;
	struct dc_to_la_params disp_params;
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* window B V-filter tap for first and second display. */
	static const enum tegra_la_id vfilter_tab[2] = {
		TEGRA_LA_DISPLAY_1B, TEGRA_LA_DISPLAY_1BB,
	};
#endif

	BUG_ON(dc->ndev->id >= ARRAY_SIZE(la_id_tab));
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	BUG_ON(dc->ndev->id >= ARRAY_SIZE(vfilter_tab));
#endif
	BUG_ON(w->idx >= ARRAY_SIZE(*la_id_tab));

	bw = max(w->bandwidth, w->new_bandwidth);

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* tegra_dc_get_bandwidth() treats V filter windows as double
	 * bandwidth, but LA has a seperate client for V filter */
	if (w->idx == 1 && win_use_v_filter(dc, w))
		bw /= 2;
#endif

	/* our bandwidth is in kbytes/sec, but LA takes MBps.
	 * round up bandwidth to next 1MBps */
	if (bw != ULONG_MAX)
		bw = bw / 1000 + 1;

#ifdef CONFIG_ARCH_TEGRA_12x_SOC
	calc_disp_params(dc,
			w,
			la_id_tab[dc->ndev->id][w->idx],
			bw,
			&disp_params);
#endif
	tegra_set_disp_latency_allowance(la_id_tab[dc->ndev->id][w->idx],
						bw,
						disp_params);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* if window B, also set the 1B client for the 2-tap V filter. */
	if (w->idx == 1)
		tegra_set_latency_allowance(vfilter_tab[dc->ndev->id], bw);
#endif
}

static int tegra_dc_windows_is_overlapped(struct tegra_dc_win *a,
	struct tegra_dc_win *b)
{
	if (a == b)
		return 0;

	if (!WIN_IS_ENABLED(a) || !WIN_IS_ENABLED(b))
		return 0;

	/* because memory access to load the fifo can overlap, only care
	 * if windows overlap vertically */
	return ((a->out_y + a->out_h > b->out_y) && (a->out_y <= b->out_y)) ||
		((b->out_y + b->out_h > a->out_y) && (b->out_y <= a->out_y));
}

/* check overlapping window combinations to find the max bandwidth. */
static unsigned long tegra_dc_find_max_bandwidth(struct tegra_dc_win *wins[],
						 unsigned n)
{
	unsigned i;
	unsigned j;
	unsigned long bw;
	unsigned long max = 0;

	for (i = 0; i < n; i++) {
		bw = wins[i]->new_bandwidth;
		for (j = 0; j < n; j++)
			if (tegra_dc_windows_is_overlapped(wins[i], wins[j]))
				bw += wins[j]->new_bandwidth;
		if (max < bw)
			max = bw;
	}
	return max;
}

/*
 * Calculate peak EMC bandwidth for each enabled window =
 * pixel_clock * win_bpp * (use_v_filter ? 2 : 1)) * H_scale_factor *
 * (windows_tiling ? 2 : 1)
 *
 * note:
 * (*) We use 2 tap V filter on T2x/T3x, so need double BW if use V filter
 * (*) Tiling mode on T30 and DDR3 requires double BW
 *
 * return:
 * bandwidth in kBps
 */
static unsigned long tegra_dc_calc_win_bandwidth(struct tegra_dc *dc,
	struct tegra_dc_win *w)
{
	uint64_t ret;
	int tiled_windows_bw_multiplier;
	unsigned long bpp;
	unsigned in_w;

	if (!WIN_IS_ENABLED(w))
		return 0;

	if (dfixed_trunc(w->w) == 0 || dfixed_trunc(w->h) == 0 ||
	    w->out_w == 0 || w->out_h == 0)
		return 0;
	if (w->flags & TEGRA_WIN_FLAG_SCAN_COLUMN)
		/* rotated: PRESCALE_SIZE swapped, but WIN_SIZE is unchanged */
		in_w = dfixed_trunc(w->h);
	else
		in_w = dfixed_trunc(w->w); /* normal output, not rotated */

	tiled_windows_bw_multiplier =
		tegra_mc_get_tiled_memory_bandwidth_multiplier();

	/* all of tegra's YUV formats(420 and 422) fetch 2 bytes per pixel,
	 * but the size reported by tegra_dc_fmt_bpp for the planar version
	 * is of the luma plane's size only. */
	bpp = tegra_dc_is_yuv_planar(w->fmt) ?
		2 * tegra_dc_fmt_bpp(w->fmt) : tegra_dc_fmt_bpp(w->fmt);
	ret = dc->mode.pclk / 1000UL * (bpp / 8);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	ret *= (win_use_v_filter(dc, w) ? 2 : 1);
#endif
	ret *= in_w / w->out_w * (WIN_IS_TILED(w) ?
		tiled_windows_bw_multiplier : 1);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	/*
	 * Assuming 60% efficiency: i.e. if we calculate we need 70MBps, we
	 * will request 117MBps from EMC.
	 */
	ret = ret + (17 * ret / 25);
#endif
	return (unsigned long)ret;
}

static unsigned long tegra_dc_get_bandwidth(
	struct tegra_dc_win *windows[], int n)
{
	int i;

	BUG_ON(n > DC_N_WINDOWS);

	/* emc rate and latency allowance both need to know per window
	 * bandwidths */
	for (i = 0; i < n; i++) {
		struct tegra_dc_win *w = windows[i];

		if (w)
			w->new_bandwidth =
				tegra_dc_calc_win_bandwidth(w->dc, w);
	}

	return tegra_dc_find_max_bandwidth(windows, n);
}

#ifdef CONFIG_TEGRA_ISOMGR
/* to save power, call when display memory clients would be idle */
void tegra_dc_clear_bandwidth(struct tegra_dc *dc)
{
	int latency;

	trace_clear_bandwidth(dc);
	latency = tegra_isomgr_reserve(dc->isomgr_handle, 0, 1000);
	if (latency) {
		dc->reserved_bw = 0;
		latency = tegra_isomgr_realize(dc->isomgr_handle);
		WARN_ONCE(!latency, "tegra_isomgr_realize failed\n");
	} else {
		dev_dbg(&dc->ndev->dev, "Failed to clear bw.\n");
		tegra_dc_ext_process_bandwidth_renegotiate(
				dc->ndev->id, NULL);
	}
	dc->bw_kbps = 0;
}
#else
/* to save power, call when display memory clients would be idle */
void tegra_dc_clear_bandwidth(struct tegra_dc *dc)
{
	trace_clear_bandwidth(dc);
	if (tegra_is_clk_enabled(dc->emc_clk))
		clk_disable_unprepare(dc->emc_clk);
	dc->bw_kbps = 0;
}

/* bw in kByte/second. returns Hz for EMC frequency */
static inline unsigned long tegra_dc_kbps_to_emc(unsigned long bw)
{
	unsigned long freq;

	if (bw == ULONG_MAX)
		return ULONG_MAX;

	freq = tegra_emc_bw_to_freq_req(bw);
	if (freq >= (ULONG_MAX / 1000))
		return ULONG_MAX; /* freq too big - clamp at max */

	if (WARN_ONCE((freq * 1000) < freq, "Bandwidth Overflow"))
		return ULONG_MAX; /* should never occur because of above. */
	return freq * 1000;
}
#endif

/* use the larger of dc->bw_kbps or dc->new_bw_kbps, and copies
 * dc->new_bw_kbps into dc->bw_kbps.
 * calling this function both before and after a flip is sufficient to select
 * the best possible frequency and latency allowance.
 * set use_new to true to force dc->new_bw_kbps programming.
 */
void tegra_dc_program_bandwidth(struct tegra_dc *dc, bool use_new)
{
	unsigned i;

	if (use_new || dc->bw_kbps != dc->new_bw_kbps) {
		long bw = max(dc->bw_kbps, dc->new_bw_kbps);

#ifdef CONFIG_TEGRA_ISOMGR
		int latency;

		/* reserve atleast the minimum bandwidth. */
		bw = max(bw, tegra_dc_calc_min_bandwidth(dc));
		latency = tegra_isomgr_reserve(dc->isomgr_handle, bw, 1000);
		if (latency) {
			dc->reserved_bw = bw;
			latency = tegra_isomgr_realize(dc->isomgr_handle);
			WARN_ONCE(!latency, "tegra_isomgr_realize failed\n");
		} else {
			dev_dbg(&dc->ndev->dev, "Failed to reserve bw %ld.\n",
									bw);
			tegra_dc_ext_process_bandwidth_renegotiate(
				dc->ndev->id, NULL);
		}
#else /* EMC version */
		int emc_freq;

		/* going from 0 to non-zero */
		if (!dc->bw_kbps && dc->new_bw_kbps &&
			!tegra_is_clk_enabled(dc->emc_clk))
			clk_prepare_enable(dc->emc_clk);

		emc_freq = tegra_dc_kbps_to_emc(bw);
		clk_set_rate(dc->emc_clk, emc_freq);

		/* going from non-zero to 0 */
		if (dc->bw_kbps && !dc->new_bw_kbps &&
			tegra_is_clk_enabled(dc->emc_clk))
			clk_disable_unprepare(dc->emc_clk);
#endif
		dc->bw_kbps = dc->new_bw_kbps;
	}

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *w = &dc->windows[i];

		if ((use_new || w->bandwidth != w->new_bandwidth) &&
			w->new_bandwidth != 0)
			tegra_dc_set_latency_allowance(dc, w);
		trace_program_bandwidth(dc);
		w->bandwidth = w->new_bandwidth;
	}
}

int tegra_dc_set_dynamic_emc(struct tegra_dc *dc)
{
	unsigned long new_rate;
	struct tegra_dc_win *windows[DC_N_WINDOWS];
	unsigned i;
	unsigned len;
	unsigned win_status = 0;

	if (!use_dynamic_emc)
		return 0;

	for (i = 0, len = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);
		if (win) {
			windows[len++] = win;
			if (win->flags && TEGRA_WIN_FLAG_ENABLED)
				win_status |= 1 << i;
		}
	}
#ifdef CONFIG_TEGRA_ISOMGR
	new_rate = tegra_dc_get_bandwidth(windows, len);
#else
	if (tegra_dc_has_multiple_dc())
		new_rate = ULONG_MAX;
	else
		new_rate = tegra_dc_get_bandwidth(windows, len);
#endif

	dc->new_bw_kbps = new_rate;
	trace_set_dynamic_emc(dc);

	/* if low_v_win is set, we can lower vdd_core when
		that windows is the only one active */
	if (dc->pdata->low_v_win != 0) {
		if (win_status == dc->pdata->low_v_win &&
			dc->win_status != dc->pdata->low_v_win) {
			tegra_dvfs_use_alt_freqs_on_clk(dc->clk, true);
			dc->win_status = dc->pdata->low_v_win;
		} else if (win_status != dc->pdata->low_v_win &&
			dc->win_status == dc->pdata->low_v_win) {
			tegra_dvfs_use_alt_freqs_on_clk(dc->clk, false);
			dc->win_status = win_status;
		}
	}
	return 0;
}

/* return the minimum bandwidth in kbps for display to function */
long tegra_dc_calc_min_bandwidth(struct tegra_dc *dc)
{
	unsigned pclk = tegra_dc_get_out_max_pixclock(dc);

	if (WARN_ONCE(!dc, "dc is NULL") ||
		WARN_ONCE(!dc->out, "dc->out is NULL!"))
		return 0;
	if (!pclk) {
		 if (dc->out->type == TEGRA_DC_OUT_HDMI) {
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
			pclk = KHZ2PICOS(300000); /* 300MHz max */
#else
			pclk = KHZ2PICOS(150000); /* 150MHz max */
#endif
		} else {
			pclk = KHZ2PICOS(dc->mode.pclk / 1000);
		}
	}

	return PICOS2KHZ(pclk) * 4; /* support a single 32bpp window */
}

#ifdef CONFIG_TEGRA_ISOMGR
int tegra_dc_bandwidth_negotiate_bw(struct tegra_dc *dc,
			struct tegra_dc_win *windows[], int n)
{
	int latency;
	u32 bw;

	mutex_lock(&dc->lock);
	/*
	 * isomgr will update available bandwidth through a callback.
	 * If available bandwidth is less than proposed bw fail the ioctl.
	 * If proposed bw is larger than reserved bw, make it in effect
	 * immediately. Otherwise, bandwidth will be adjusted in flips.
	 */
	bw = tegra_dc_get_bandwidth(windows, n);
	if (bw > dc->available_bw) {
		mutex_unlock(&dc->lock);
		return -1;
	} else if (bw <= dc->reserved_bw) {
		mutex_unlock(&dc->lock);
		return 0;
	}

	latency = tegra_isomgr_reserve(dc->isomgr_handle, bw, 1000);
	if (!latency) {
		dev_dbg(&dc->ndev->dev, "Failed to reserve proposed bw %d.\n",
									bw);
		mutex_unlock(&dc->lock);
		return -1;
	}

	/*
	 * Only realize if bw required > reserved bw
	 * If realized, update dc->bw_kbps
	 */
	if (dc->reserved_bw < bw) {
		dc->reserved_bw = bw;
		latency = tegra_isomgr_realize(dc->isomgr_handle);
		if (!latency) {
			WARN_ONCE(!latency, "tegra_isomgr_realize failed\n");
			mutex_unlock(&dc->lock);
			return -1;
		}
		dc->bw_kbps = bw;
	}

	mutex_unlock(&dc->lock);

	return 0;
}

void tegra_dc_bandwidth_renegotiate(void *p, u32 avail_bw)
{
	struct tegra_dc_bw_data data;
	struct tegra_dc *dc = p;

	if (dc->available_bw == avail_bw)
		return;

	if (WARN_ONCE(!dc, "dc is NULL!"))
		return;

	data.total_bw = tegra_isomgr_get_total_iso_bw();
	data.avail_bw = avail_bw;
	data.resvd_bw = dc->reserved_bw;

	tegra_dc_ext_process_bandwidth_renegotiate(dc->ndev->id, &data);

	mutex_lock(&dc->lock);
	dc->available_bw = avail_bw;
	mutex_unlock(&dc->lock);
}
#endif
