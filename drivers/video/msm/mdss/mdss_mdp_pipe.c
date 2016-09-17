/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#include <linux/bitmap.h>
#include <linux/errno.h>
#include <linux/iopoll.h>
#include <linux/mutex.h>

#include "mdss_mdp.h"
#include "mdss_mdp_trace.h"
#include "mdss_debug.h"

#define SMP_MB_SIZE		(mdss_res->smp_mb_size)
#define SMP_MB_CNT		(mdss_res->smp_mb_cnt)
#define SMP_MB_ENTRY_SIZE	16
#define MAX_BPP 4

#define PIPE_CLEANUP_TIMEOUT_US 100000

/* following offsets are relative to ctrl register bit offset */
#define CLK_FORCE_ON_OFFSET	0x0
#define CLK_FORCE_OFF_OFFSET	0x1
/* following offsets are relative to status register bit offset */
#define CLK_STATUS_OFFSET	0x0

#define QOS_LUT_NRT_READ	0x0
#define PANIC_LUT_NRT_READ	0x0
#define ROBUST_LUT_NRT_READ	0xFFFF

#define VBLANK_PANIC_DEFAULT_CONFIG 0x200000 /* Priority 2, no panic */
#define VBLANK_PANIC_CREQ_MASK 0x300030

#define QSEED3_DEFAULT_PRELAOD_H  0x4
#define QSEED3_DEFAULT_PRELAOD_V  0x3

static DEFINE_MUTEX(mdss_mdp_sspp_lock);
static DEFINE_MUTEX(mdss_mdp_smp_lock);

static void mdss_mdp_pipe_free(struct kref *kref);
static int mdss_mdp_smp_mmb_set(int client_id, unsigned long *smp);
static void mdss_mdp_smp_mmb_free(unsigned long *smp, bool write);
static struct mdss_mdp_pipe *mdss_mdp_pipe_search_by_client_id(
	struct mdss_data_type *mdata, int client_id,
	enum mdss_mdp_pipe_rect rect_num);
static int mdss_mdp_calc_stride(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_plane_sizes *ps);
static u32 mdss_mdp_calc_per_plane_num_blks(u32 ystride,
	struct mdss_mdp_pipe *pipe);
static int mdss_mdp_pipe_program_pixel_extn(struct mdss_mdp_pipe *pipe);

/**
 * enum mdss_mdp_pipe_qos - Different qos configurations for each pipe
 *
 * @MDSS_MDP_PIPE_QOS_VBLANK_CTRL: Setup VBLANK qos for the pipe.
 * @MDSS_MDP_PIPE_QOS_VBLANK_AMORTIZE: Enables Amortization within pipe.
 *	this configuration is mutually exclusive from VBLANK_CTRL.
 * @MDSS_MDP_PIPE_QOS_PANIC_CTRL: Setup panic for the pipe.
 */
enum mdss_mdp_pipe_qos {
	MDSS_MDP_PIPE_QOS_VBLANK_CTRL = BIT(0),
	MDSS_MDP_PIPE_QOS_VBLANK_AMORTIZE = BIT(1),
	MDSS_MDP_PIPE_QOS_PANIC_CTRL = BIT(2),
};

static inline void mdss_mdp_pipe_write(struct mdss_mdp_pipe *pipe,
				       u32 reg, u32 val)
{
	writel_relaxed(val, pipe->base + reg);
}

/**
 * This function is used to decide if certain register programming can be
 * delayed or not. This is useful when multirect is used where two pipe
 * structures access same set of registers.
 */
static inline bool is_pipe_programming_delay_needed(
	struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_pipe *next_pipe = pipe->multirect.next;

	return (pipe->multirect.mode != MDSS_MDP_PIPE_MULTIRECT_NONE) &&
	       next_pipe->params_changed;
}

static inline u32 mdss_mdp_pipe_read(struct mdss_mdp_pipe *pipe, u32 reg)
{
	return readl_relaxed(pipe->base + reg);
}

static inline int mdss_calc_fill_level(struct mdss_mdp_format_params *fmt,
	u32 src_width)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 fixed_buff_size = mdata->pixel_ram_size;
	u32 total_fl;

	if (fmt->fetch_planes == MDSS_MDP_PLANE_PSEUDO_PLANAR) {
		if (fmt->chroma_sample == MDSS_MDP_CHROMA_420) {
			/* NV12 */
			total_fl = (fixed_buff_size / 2) /
				((src_width + 32) * fmt->bpp);
		} else {
			/* non NV12 */
			total_fl = (fixed_buff_size) /
				((src_width + 32) * fmt->bpp);
		}
	} else {
		total_fl = (fixed_buff_size * 2) /
			((src_width + 32) * fmt->bpp);
	}

	return total_fl;
}

static inline u32 get_qos_lut_linear(u32 total_fl)
{
	u32 qos_lut;

	if (total_fl <= 4)
		qos_lut = 0x1B;
	else if (total_fl <= 5)
		qos_lut = 0x5B;
	else if (total_fl <= 6)
		qos_lut = 0x15B;
	else if (total_fl <= 7)
		qos_lut = 0x55B;
	else if (total_fl <= 8)
		qos_lut = 0x155B;
	else if (total_fl <= 9)
		qos_lut = 0x555B;
	else if (total_fl <= 10)
		qos_lut = 0x1555B;
	else if (total_fl <= 11)
		qos_lut = 0x5555B;
	else if (total_fl <= 12)
		qos_lut = 0x15555B;
	else
		qos_lut = 0x55555B;

	return qos_lut;
}

static inline u32 get_qos_lut_macrotile(u32 total_fl)
{
	u32 qos_lut;

	if (total_fl <= 10)
		qos_lut = 0x1AAff;
	else if (total_fl <= 11)
		qos_lut = 0x5AAFF;
	else if (total_fl <= 12)
		qos_lut = 0x15AAFF;
	else
		qos_lut = 0x55AAFF;

	return qos_lut;
}

static void mdss_mdp_pipe_qos_lut(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;
	u32 qos_lut;
	u32 total_fl = 0;

	if ((ctl->intf_num == MDSS_MDP_NO_INTF) ||
			pipe->mixer_left->rotator_mode) {
		qos_lut = QOS_LUT_NRT_READ; /* low priority for nrt */
	} else {
		total_fl = mdss_calc_fill_level(pipe->src_fmt,
			pipe->src.w);

		if (mdss_mdp_is_linear_format(pipe->src_fmt))
			qos_lut = get_qos_lut_linear(total_fl);
		else
			qos_lut = get_qos_lut_macrotile(total_fl);
	}

	trace_mdp_perf_set_qos_luts(pipe->num, pipe->src_fmt->format,
		ctl->intf_num, pipe->mixer_left->rotator_mode, total_fl,
		qos_lut, mdss_mdp_is_linear_format(pipe->src_fmt));

	pr_debug("pnum:%d fmt:%d intf:%d rot:%d fl:%d lut:0x%x\n",
		pipe->num, pipe->src_fmt->format, ctl->intf_num,
		pipe->mixer_left->rotator_mode, total_fl, qos_lut);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_CREQ_LUT,
		qos_lut);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

bool is_rt_pipe(struct mdss_mdp_pipe *pipe)
{
	return pipe && pipe->mixer_left &&
		pipe->mixer_left->type == MDSS_MDP_MIXER_TYPE_INTF;
}

static void mdss_mdp_config_pipe_panic_lut(struct mdss_mdp_pipe *pipe)
{
	u32 panic_lut, robust_lut;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!is_rt_pipe(pipe)) {
		panic_lut = PANIC_LUT_NRT_READ;
		robust_lut = ROBUST_LUT_NRT_READ;
	} else if (mdss_mdp_is_linear_format(pipe->src_fmt)) {
		panic_lut = mdata->default_panic_lut_per_pipe_linear;
		robust_lut = mdata->default_robust_lut_per_pipe_linear;
	} else {
		panic_lut = mdata->default_panic_lut_per_pipe_tile;
		robust_lut = mdata->default_robust_lut_per_pipe_tile;
	}

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_DANGER_LUT,
		panic_lut);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SAFE_LUT,
		robust_lut);

	trace_mdp_perf_set_panic_luts(pipe->num, pipe->src_fmt->format,
		pipe->src_fmt->fetch_mode, panic_lut, robust_lut);

	pr_debug("pnum:%d fmt:%d mode:%d luts[0x%x, 0x%x]\n",
		pipe->num, pipe->src_fmt->format, pipe->src_fmt->fetch_mode,
		panic_lut, robust_lut);
}

static void mdss_mdp_pipe_qos_ctrl(struct mdss_mdp_pipe *pipe,
	bool enable, u32 flags)
{
	u32 per_pipe_qos;

	per_pipe_qos = mdss_mdp_pipe_read(pipe, MDSS_MDP_REG_SSPP_QOS_CTRL);

	if (flags & MDSS_MDP_PIPE_QOS_VBLANK_CTRL) {
		per_pipe_qos |= VBLANK_PANIC_DEFAULT_CONFIG;

		if (enable)
			per_pipe_qos |= BIT(16);
		else
			per_pipe_qos &= ~BIT(16);
	}

	if (flags & MDSS_MDP_PIPE_QOS_VBLANK_AMORTIZE) {
		/* this feature overrules previous VBLANK_CTRL */
		per_pipe_qos &= ~BIT(16);
		per_pipe_qos &= ~VBLANK_PANIC_CREQ_MASK; /* clear vblank bits */
	}

	if (flags & MDSS_MDP_PIPE_QOS_PANIC_CTRL) {
		if (enable)
			per_pipe_qos |= BIT(0);
		else
			per_pipe_qos &= ~BIT(0);
	}

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_QOS_CTRL,
		per_pipe_qos);
}

/**
 * @mdss_mdp_pipe_panic_vblank_signal_ctrl -
 * @pipe: pointer to a pipe
 * @enable: TRUE - enables feature FALSE - disables feature
 *
 * This function assumes that clocks are enabled, so it is callers
 * responsibility to enable clocks before calling this function.
 */
static int mdss_mdp_pipe_panic_vblank_signal_ctrl(struct mdss_mdp_pipe *pipe,
	bool enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!mdata->has_panic_ctrl)
		goto end;

	if (!is_rt_pipe(pipe))
		goto end;

	if (!test_bit(MDSS_QOS_VBLANK_PANIC_CTRL, mdata->mdss_qos_map))
		goto end;

	mutex_lock(&mdata->reg_lock);

	mdss_mdp_pipe_qos_ctrl(pipe, enable, MDSS_MDP_PIPE_QOS_VBLANK_CTRL);

	mutex_unlock(&mdata->reg_lock);

end:
	return 0;
}

int mdss_mdp_pipe_panic_signal_ctrl(struct mdss_mdp_pipe *pipe, bool enable)
{
	uint32_t panic_robust_ctrl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!mdata->has_panic_ctrl)
		goto end;

	if (!is_rt_pipe(pipe))
		goto end;

	mutex_lock(&mdata->reg_lock);
	switch (mdss_mdp_panic_signal_support_mode(mdata)) {
	case MDSS_MDP_PANIC_COMMON_REG_CFG:
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		panic_robust_ctrl = readl_relaxed(mdata->mdp_base +
				MMSS_MDP_PANIC_ROBUST_CTRL);
		if (enable)
			panic_robust_ctrl |= BIT(pipe->panic_ctrl_ndx);
		else
			panic_robust_ctrl &= ~BIT(pipe->panic_ctrl_ndx);
		writel_relaxed(panic_robust_ctrl,
				mdata->mdp_base + MMSS_MDP_PANIC_ROBUST_CTRL);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		break;
	case MDSS_MDP_PANIC_PER_PIPE_CFG:
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

		mdss_mdp_pipe_qos_ctrl(pipe, enable,
			MDSS_MDP_PIPE_QOS_PANIC_CTRL);

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		break;
	}
	mutex_unlock(&mdata->reg_lock);

end:
	return 0;
}

void mdss_mdp_bwcpanic_ctrl(struct mdss_data_type *mdata, bool enable)
{
	if (!mdata)
		return;

	mutex_lock(&mdata->reg_lock);
	if (enable) {
		writel_relaxed(0x0, mdata->mdp_base + MMSS_MDP_PANIC_LUT0);
		writel_relaxed(0x0, mdata->mdp_base + MMSS_MDP_PANIC_LUT1);
		writel_relaxed(0x0, mdata->mdp_base + MMSS_MDP_ROBUST_LUT);
	} else {
		writel_relaxed(mdata->default_panic_lut0,
			mdata->mdp_base + MMSS_MDP_PANIC_LUT0);
		writel_relaxed(mdata->default_panic_lut1,
			mdata->mdp_base + MMSS_MDP_PANIC_LUT1);
		writel_relaxed(mdata->default_robust_lut,
			mdata->mdp_base + MMSS_MDP_ROBUST_LUT);
	}
	mutex_unlock(&mdata->reg_lock);
}

/**
 * @mdss_mdp_pipe_nrt_vbif_setup -
 * @mdata: pointer to global driver data.
 * @pipe: pointer to a pipe
 *
 * This function assumes that clocks are enabled, so it is callers
 * responsibility to enable clocks before calling this function.
 */
static void mdss_mdp_pipe_nrt_vbif_setup(struct mdss_data_type *mdata,
					struct mdss_mdp_pipe *pipe)
{
	uint32_t nrt_vbif_client_sel;

	if (pipe->type != MDSS_MDP_PIPE_TYPE_DMA)
		return;

	mutex_lock(&mdata->reg_lock);
	nrt_vbif_client_sel = readl_relaxed(mdata->mdp_base +
				MMSS_MDP_RT_NRT_VBIF_CLIENT_SEL);
	if (mdss_mdp_is_nrt_vbif_client(mdata, pipe))
		nrt_vbif_client_sel |= BIT(pipe->num - MDSS_MDP_SSPP_DMA0);
	else
		nrt_vbif_client_sel &= ~BIT(pipe->num - MDSS_MDP_SSPP_DMA0);
	writel_relaxed(nrt_vbif_client_sel,
			mdata->mdp_base + MMSS_MDP_RT_NRT_VBIF_CLIENT_SEL);
	mutex_unlock(&mdata->reg_lock);

	return;
}

static inline bool is_unused_smp_allowed(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	switch (MDSS_GET_MAJOR_MINOR(mdata->mdp_rev)) {
	case MDSS_GET_MAJOR_MINOR(MDSS_MDP_HW_REV_103):
	case MDSS_GET_MAJOR_MINOR(MDSS_MDP_HW_REV_105):
	case MDSS_GET_MAJOR_MINOR(MDSS_MDP_HW_REV_109):
	case MDSS_GET_MAJOR_MINOR(MDSS_MDP_HW_REV_110):
		return true;
	default:
		return false;
	}
}

static u32 mdss_mdp_smp_mmb_reserve(struct mdss_mdp_pipe_smp_map *smp_map,
	size_t n, bool force_alloc)
{
	u32 i, mmb;
	u32 fixed_cnt = bitmap_weight(smp_map->fixed, SMP_MB_CNT);
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (n <= fixed_cnt)
		return fixed_cnt;
	else
		n -= fixed_cnt;

	i = bitmap_weight(smp_map->allocated, SMP_MB_CNT);

	/*
	 * SMP programming is not double buffered. Fail the request,
	 * that calls for change in smp configuration (addition/removal
	 * of smp blocks), so that fallback solution happens.
	 */
	if (i != 0 && !force_alloc &&
	    (((n < i) && !is_unused_smp_allowed()) || (n > i))) {
		pr_debug("Can't change mmb config, num_blks: %zu alloc: %d\n",
			n, i);
		return 0;
	}

	/*
	 * Clear previous SMP reservations and reserve according to the
	 * latest configuration
	 */
	mdss_mdp_smp_mmb_free(smp_map->reserved, false);

	/* Reserve mmb blocks*/
	for (; i < n; i++) {
		if (bitmap_full(mdata->mmb_alloc_map, SMP_MB_CNT))
			break;

		mmb = find_first_zero_bit(mdata->mmb_alloc_map, SMP_MB_CNT);
		set_bit(mmb, smp_map->reserved);
		set_bit(mmb, mdata->mmb_alloc_map);
	}

	return i + fixed_cnt;
}

static int mdss_mdp_smp_mmb_set(int client_id, unsigned long *smp)
{
	u32 mmb, off, data, s;
	int cnt = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	for_each_set_bit(mmb, smp, SMP_MB_CNT) {
		off = (mmb / 3) * 4;
		s = (mmb % 3) * 8;
		data = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_REG_SMP_ALLOC_W0 + off);
		data &= ~(0xFF << s);
		data |= client_id << s;
		writel_relaxed(data, mdata->mdp_base +
			MDSS_MDP_REG_SMP_ALLOC_W0 + off);
		writel_relaxed(data, mdata->mdp_base +
			MDSS_MDP_REG_SMP_ALLOC_R0 + off);
		cnt++;
	}
	return cnt;
}

static void mdss_mdp_smp_mmb_amend(unsigned long *smp, unsigned long *extra)
{
	bitmap_or(smp, smp, extra, SMP_MB_CNT);
	bitmap_zero(extra, SMP_MB_CNT);
}

static void mdss_mdp_smp_mmb_free(unsigned long *smp, bool write)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!bitmap_empty(smp, SMP_MB_CNT)) {
		if (write)
			mdss_mdp_smp_mmb_set(0, smp);
		bitmap_andnot(mdata->mmb_alloc_map, mdata->mmb_alloc_map,
			      smp, SMP_MB_CNT);
		bitmap_zero(smp, SMP_MB_CNT);
	}
}

u32 mdss_mdp_smp_calc_num_blocks(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_plane_sizes ps;
	int rc = 0;
	int i, num_blks = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->has_pixel_ram)
		return 0;

	rc = mdss_mdp_calc_stride(pipe, &ps);
	if (rc) {
		pr_err("wrong stride calc\n");
		return 0;
	}

	for (i = 0; i < ps.num_planes; i++) {
		num_blks += mdss_mdp_calc_per_plane_num_blks(ps.ystride[i],
			pipe);
		pr_debug("SMP for BW %d mmb for pnum=%d plane=%d\n",
			num_blks, pipe->num, i);
	}

	pr_debug("SMP blks %d mb_cnt for pnum=%d\n",
		num_blks, pipe->num);
	return num_blks;
}

/**
 * @mdss_mdp_smp_get_size - get allocated smp size for a pipe
 * @pipe: pointer to a pipe
 *
 * Function counts number of blocks that are currently allocated for a
 * pipe, then smp buffer size is number of blocks multiplied by block
 * size.
 */
u32 mdss_mdp_smp_get_size(struct mdss_mdp_pipe *pipe)
{
	int i, mb_cnt = 0, smp_size;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (mdata->has_pixel_ram) {
		smp_size = mdata->pixel_ram_size;
	} else {
		for (i = 0; i < MAX_PLANES; i++) {
			mb_cnt += bitmap_weight(pipe->smp_map[i].allocated,
								SMP_MB_CNT);
			mb_cnt += bitmap_weight(pipe->smp_map[i].fixed,
								SMP_MB_CNT);
		}

		smp_size = mb_cnt * SMP_MB_SIZE;
	}

	pr_debug("SMP size %d for pnum=%d\n",
		smp_size, pipe->num);

	return smp_size;
}

static void mdss_mdp_smp_set_wm_levels(struct mdss_mdp_pipe *pipe, int mb_cnt)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 useable_space, latency_bytes, val, wm[3];
	struct mdss_mdp_mixer *mixer = pipe->mixer_left;

	useable_space = mb_cnt * SMP_MB_SIZE;

	/*
	 * For 1.3.x version, when source format is macrotile then useable
	 * space within total allocated SMP space is limited to src_w *
	 * bpp * nlines. Unlike linear format, any extra space left over is
	 * not filled.
	 *
	 * All other versions, in case of linear we calculate the latency
	 * bytes as the bytes to be used for the latency buffer lines, so the
	 * transactions when filling the full SMPs have the lowest priority.
	 */

	latency_bytes = mdss_mdp_calc_latency_buf_bytes(pipe->src_fmt->is_yuv,
		pipe->bwc_mode, mdss_mdp_is_tile_format(pipe->src_fmt),
		pipe->src.w, pipe->src_fmt->bpp, false, useable_space,
		mdss_mdp_is_ubwc_format(pipe->src_fmt),
		mdss_mdp_is_nv12_format(pipe->src_fmt),
		(pipe->flags & MDP_FLIP_LR));

	if ((pipe->flags & MDP_FLIP_LR) &&
		!mdss_mdp_is_tile_format(pipe->src_fmt)) {
		/*
		 * when doing hflip, one line is reserved to be consumed down
		 * the pipeline. This line will always be marked as full even
		 * if it doesn't have any data. In order to generate proper
		 * priority levels ignore this region while setting up
		 * watermark levels
		 */
		u8 bpp = pipe->src_fmt->is_yuv ? 1 :
			pipe->src_fmt->bpp;
		latency_bytes -= (pipe->src.w * bpp);
	}

	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_103) &&
		mdss_mdp_is_tile_format(pipe->src_fmt)) {
		val = latency_bytes / SMP_MB_ENTRY_SIZE;

		wm[0] = (val * 5) / 8;
		wm[1] = (val * 6) / 8;
		wm[2] = (val * 7) / 8;
	} else if (mixer->rotator_mode ||
		(mixer->ctl->intf_num == MDSS_MDP_NO_INTF)) {
		/* any non real time pipe */
		wm[0]  = 0xffff;
		wm[1]  = 0xffff;
		wm[2]  = 0xffff;
	} else {
		/*
		 *  1/3 of the latency buffer bytes from the
		 *  SMP pool that is being fetched
		 */
		val = (latency_bytes / SMP_MB_ENTRY_SIZE) / 3;

		wm[0] = val;
		wm[1] = wm[0] + val;
		wm[2] = wm[1] + val;
	}

	trace_mdp_perf_set_wm_levels(pipe->num, useable_space, latency_bytes,
		wm[0], wm[1], wm[2], mb_cnt, SMP_MB_SIZE);

	pr_debug("pnum=%d useable_space=%u watermarks %u,%u,%u\n", pipe->num,
			useable_space, wm[0], wm[1], wm[2]);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_REQPRIO_FIFO_WM_0, wm[0]);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_REQPRIO_FIFO_WM_1, wm[1]);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_REQPRIO_FIFO_WM_2, wm[2]);
}

static void mdss_mdp_smp_free(struct mdss_mdp_pipe *pipe)
{
	int i;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->has_pixel_ram)
		return;

	mutex_lock(&mdss_mdp_smp_lock);
	for (i = 0; i < MAX_PLANES; i++) {
		mdss_mdp_smp_mmb_free(pipe->smp_map[i].reserved, false);
		mdss_mdp_smp_mmb_free(pipe->smp_map[i].allocated, true);
	}
	mutex_unlock(&mdss_mdp_smp_lock);
}

void mdss_mdp_smp_unreserve(struct mdss_mdp_pipe *pipe)
{
	int i;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->has_pixel_ram)
		return;

	mutex_lock(&mdss_mdp_smp_lock);
	for (i = 0; i < MAX_PLANES; i++)
		mdss_mdp_smp_mmb_free(pipe->smp_map[i].reserved, false);
	mutex_unlock(&mdss_mdp_smp_lock);
}

static int mdss_mdp_calc_stride(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_plane_sizes *ps)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u16 width;
	int rc = 0;
	u32 format, seg_w = 0;

	if (mdata->has_pixel_ram)
		return 0;

	width = DECIMATED_DIMENSION(pipe->src.w, pipe->horz_deci);

	if (pipe->bwc_mode) {
		rc = mdss_mdp_get_rau_strides(pipe->src.w, pipe->src.h,
			pipe->src_fmt, ps);
		if (rc)
			return rc;
		/*
		 * Override fetch strides with SMP buffer size for both the
		 * planes. BWC line buffer needs to be divided into 16
		 * segments and every segment is aligned to format
		 * specific RAU size
		 */
		seg_w = DIV_ROUND_UP(pipe->src.w, 16);
		if (pipe->src_fmt->fetch_planes == MDSS_MDP_PLANE_INTERLEAVED) {
			ps->ystride[0] = ALIGN(seg_w, 32) * 16 * ps->rau_h[0] *
					pipe->src_fmt->bpp;
			ps->ystride[1] = 0;
		} else {
			u32 bwc_width = ALIGN(seg_w, 64) * 16;
			ps->ystride[0] = bwc_width * ps->rau_h[0];
			ps->ystride[1] = bwc_width * ps->rau_h[1];
			/*
			 * Since chroma for H1V2 is not subsampled it needs
			 * to be accounted for with bpp factor
			 */
			if (pipe->src_fmt->chroma_sample ==
				MDSS_MDP_CHROMA_H1V2)
				ps->ystride[1] *= 2;
		}
		pr_debug("BWC SMP strides ystride0=%x ystride1=%x\n",
			ps->ystride[0], ps->ystride[1]);
	} else {
		format = pipe->src_fmt->format;
		/*
		 * when decimation block is present, all chroma planes
		 * are fetched on a single SMP plane for chroma pixels
		 */
		if (mdata->has_decimation) {
			switch (pipe->src_fmt->chroma_sample) {
			case MDSS_MDP_CHROMA_H2V1:
				format = MDP_Y_CRCB_H2V1;
				break;
			case MDSS_MDP_CHROMA_420:
				format = MDP_Y_CBCR_H2V2;
				break;
			default:
				break;
			}
		}
		rc = mdss_mdp_get_plane_sizes(pipe->src_fmt, width, pipe->src.h,
			ps, 0, 0);
		if (rc)
			return rc;

		if (pipe->mixer_left && (ps->num_planes == 1)) {
			ps->ystride[0] = MAX_BPP *
				max(pipe->mixer_left->width, width);
		} else if (mdata->has_decimation) {
			/*
			 * To avoid quailty loss, MDP does one less decimation
			 * on chroma components if they are subsampled.
			 * Account for this to have enough SMPs for latency
			 */
			switch (pipe->src_fmt->chroma_sample) {
			case MDSS_MDP_CHROMA_H2V1:
			case MDSS_MDP_CHROMA_420:
				ps->ystride[1] <<= 1;
				break;
			}
		}
	}

	return rc;
}

static u32 mdss_mdp_calc_per_plane_num_blks(u32 ystride,
	struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 num_blks = 0;
	u32 nlines = 0;

	if (pipe->mixer_left && (pipe->mixer_left->rotator_mode ||
		(pipe->mixer_left->type == MDSS_MDP_MIXER_TYPE_WRITEBACK))) {
		if (mdss_mdp_is_tile_format(pipe->src_fmt))
			num_blks = 4;
		else
			num_blks = 1;
	} else {
		if (mdss_mdp_is_tile_format(pipe->src_fmt))
			nlines = 8;
		else
			nlines = pipe->bwc_mode ? 1 : 2;

		num_blks = DIV_ROUND_UP(ystride * nlines,
				SMP_MB_SIZE);

		if (mdata->mdp_rev == MDSS_MDP_HW_REV_100)
			num_blks = roundup_pow_of_two(num_blks);

		if (mdata->smp_mb_per_pipe &&
			(num_blks > mdata->smp_mb_per_pipe) &&
			!(pipe->flags & MDP_FLIP_LR))
			num_blks = mdata->smp_mb_per_pipe;
	}

	pr_debug("pipenum:%d tile:%d bwc:%d ystride%d pipeblks:%d blks:%d\n",
		pipe->num, mdss_mdp_is_tile_format(pipe->src_fmt),
		pipe->bwc_mode, ystride, mdata->smp_mb_per_pipe, num_blks);

	return num_blks;
}

int mdss_mdp_smp_reserve(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 num_blks = 0, reserved = 0;
	struct mdss_mdp_plane_sizes ps;
	int i, rc = 0;
	bool force_alloc = 0;

	if (mdata->has_pixel_ram)
		return 0;

	rc = mdss_mdp_calc_stride(pipe, &ps);
	if (rc)
		return rc;

	force_alloc = pipe->flags & MDP_SMP_FORCE_ALLOC;

	mutex_lock(&mdss_mdp_smp_lock);
	if (!is_unused_smp_allowed()) {
		for (i = (MAX_PLANES - 1); i >= ps.num_planes; i--) {
			if (bitmap_weight(pipe->smp_map[i].allocated,
					  SMP_MB_CNT)) {
				pr_debug("unsed mmb for pipe%d plane%d not allowed\n",
					pipe->num, i);
				mutex_unlock(&mdss_mdp_smp_lock);
				return -EAGAIN;
			}
		}
	}

	for (i = 0; i < ps.num_planes; i++) {
		num_blks = mdss_mdp_calc_per_plane_num_blks(ps.ystride[i],
			pipe);
		pr_debug("reserving %d mmb for pnum=%d plane=%d\n",
				num_blks, pipe->num, i);
		reserved = mdss_mdp_smp_mmb_reserve(&pipe->smp_map[i],
			num_blks, force_alloc);
		if (reserved < num_blks)
			break;
	}

	if (reserved < num_blks) {
		pr_debug("insufficient MMB blocks. pnum:%d\n", pipe->num);
		for (; i >= 0; i--)
			mdss_mdp_smp_mmb_free(pipe->smp_map[i].reserved,
				false);
		rc = -ENOBUFS;
	}
	mutex_unlock(&mdss_mdp_smp_lock);

	return rc;
}
/*
 * mdss_mdp_smp_alloc() -- set smp mmb and and wm levels for a staged pipe
 * @pipe: pointer to a pipe
 *
 * Function amends reserved smp mmbs to allocated bitmap and ties respective
 * mmbs to their pipe fetch_ids. Based on the number of total allocated mmbs
 * for a staged pipe, it also sets the watermark levels (wm).
 *
 * This function will be called on every commit where pipe params might not
 * have changed. In such cases, we need to ensure that wm levels are not
 * wiped out. Also in some rare situations hw might have reset and wiped out
 * smp mmb programming but new smp reservation is not done. In such cases we
 * need to ensure that for a staged pipes, mmbs are set properly based on
 * allocated bitmap.
 */
static int mdss_mdp_smp_alloc(struct mdss_mdp_pipe *pipe)
{
	int i;
	int cnt = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (mdata->has_pixel_ram)
		return 0;

	mutex_lock(&mdss_mdp_smp_lock);
	for (i = 0; i < MAX_PLANES; i++) {
		cnt += bitmap_weight(pipe->smp_map[i].fixed, SMP_MB_CNT);

		if (bitmap_empty(pipe->smp_map[i].reserved, SMP_MB_CNT)) {
			cnt += mdss_mdp_smp_mmb_set(pipe->ftch_id + i,
				pipe->smp_map[i].allocated);
			continue;
		}

		mdss_mdp_smp_mmb_amend(pipe->smp_map[i].allocated,
			pipe->smp_map[i].reserved);
		cnt += mdss_mdp_smp_mmb_set(pipe->ftch_id + i,
			pipe->smp_map[i].allocated);
	}
	mdss_mdp_smp_set_wm_levels(pipe, cnt);
	mutex_unlock(&mdss_mdp_smp_lock);
	return 0;
}

void mdss_mdp_smp_release(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->has_pixel_ram)
		return;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	mdss_mdp_smp_free(pipe);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

int mdss_mdp_smp_setup(struct mdss_data_type *mdata, u32 cnt, u32 size)
{
	if (!mdata)
		return -EINVAL;

	mdata->smp_mb_cnt = cnt;
	mdata->smp_mb_size = size;

	return 0;
}

/**
 * mdss_mdp_smp_handoff() - Handoff SMP MMBs in use by staged pipes
 * @mdata: pointer to the global mdss data structure.
 *
 * Iterate through the list of all SMP MMBs and check to see if any
 * of them are assigned to a pipe being marked as being handed-off.
 * If so, update the corresponding software allocation map to reflect
 * this.
 *
 * This function would typically be called during MDP probe for the case
 * when certain pipes might be programmed in the bootloader to display
 * the splash screen.
 */
int mdss_mdp_smp_handoff(struct mdss_data_type *mdata)
{
	int rc = 0;
	int i, client_id, prev_id = 0;
	u32 off, s, data;
	struct mdss_mdp_pipe *pipe = NULL;

	if (mdata->has_pixel_ram)
		return 0;

	/*
	 * figure out what SMP MMBs are allocated for each of the pipes
	 * that need to be handed off.
	 */
	for (i = 0; i < SMP_MB_CNT; i++) {
		off = (i / 3) * 4;
		s = (i % 3) * 8;
		data = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_REG_SMP_ALLOC_W0 + off);
		client_id = (data >> s) & 0xFF;
		if (test_bit(i, mdata->mmb_alloc_map)) {
			/*
			 * Certain pipes may have a dedicated set of
			 * SMP MMBs statically allocated to them. In
			 * such cases, we do not need to do anything
			 * here.
			 */
			pr_debug("smp mmb %d already assigned to pipe %d (client_id %d)\n"
				, i, pipe ? pipe->num : -1, client_id);
			continue;
		}

		if (client_id) {
			if (client_id != prev_id) {
				pipe = mdss_mdp_pipe_search_by_client_id(mdata,
					client_id, MDSS_MDP_PIPE_RECT0);
				prev_id = client_id;
			}

			if (!pipe) {
				pr_warn("Invalid client id %d for SMP MMB %d\n",
					client_id, i);
				continue;
			}

			if (!pipe->is_handed_off) {
				pr_warn("SMP MMB %d assigned to a pipe not marked for handoff (client id %d)\n"
					, i, client_id);
				continue;
			}

			/*
			 * Assume that the source format only has
			 * one plane
			 */
			pr_debug("Assigning smp mmb %d to pipe %d (client_id %d)\n"
				, i, pipe->num, client_id);
			set_bit(i, pipe->smp_map[0].allocated);
			set_bit(i, mdata->mmb_alloc_map);
		}
	}

	return rc;
}

void mdss_mdp_pipe_unmap(struct mdss_mdp_pipe *pipe)
{
	if (kref_put_mutex(&pipe->kref, mdss_mdp_pipe_free,
			&mdss_mdp_sspp_lock)) {
		WARN(1, "Unexpected free pipe during unmap\n");
		mutex_unlock(&mdss_mdp_sspp_lock);
	}
}

int mdss_mdp_pipe_map(struct mdss_mdp_pipe *pipe)
{
	if (!kref_get_unless_zero(&pipe->kref))
		return -EINVAL;
	return 0;
}

/**
 * mdss_mdp_qos_vbif_remapper_setup - Program the VBIF QoS remapper
 *		registers based on real or non real time clients
 * @mdata:	Pointer to the global mdss data structure.
 * @pipe:	Pointer to source pipe struct to get xin id's.
 * @is_realtime:	To determine if pipe's client is real or
 *			non real time.
 * This function assumes that clocks are on, so it is caller responsibility to
 * call this function with clocks enabled.
 */
static void mdss_mdp_qos_vbif_remapper_setup(struct mdss_data_type *mdata,
			struct mdss_mdp_pipe *pipe, bool is_realtime)
{
	u32 mask, reg_val, reg_val_lvl, i, vbif_qos;
	u32 reg_high;
	bool is_nrt_vbif = mdss_mdp_is_nrt_vbif_client(mdata, pipe);

	if (mdata->npriority_lvl == 0)
		return;

	if (test_bit(MDSS_QOS_REMAPPER, mdata->mdss_qos_map)) {
		mutex_lock(&mdata->reg_lock);
		for (i = 0; i < mdata->npriority_lvl; i++) {
			reg_high = ((pipe->xin_id & 0x8) >> 3) * 4 + (i * 8);

			reg_val = MDSS_VBIF_READ(mdata,
				MDSS_VBIF_QOS_RP_REMAP_BASE +
				reg_high, is_nrt_vbif);
			reg_val_lvl = MDSS_VBIF_READ(mdata,
				MDSS_VBIF_QOS_LVL_REMAP_BASE + reg_high,
				is_nrt_vbif);

			mask = 0x3 << (pipe->xin_id * 4);
			vbif_qos = is_realtime ?
				mdata->vbif_rt_qos[i] : mdata->vbif_nrt_qos[i];

			reg_val &= ~(mask);
			reg_val |= vbif_qos << (pipe->xin_id * 4);

			reg_val_lvl &= ~(mask);
			reg_val_lvl |= vbif_qos << (pipe->xin_id * 4);

			pr_debug("idx:%d xin:%d reg:0x%x val:0x%x lvl:0x%x\n",
			   i, pipe->xin_id, reg_high, reg_val, reg_val_lvl);
			MDSS_VBIF_WRITE(mdata, MDSS_VBIF_QOS_RP_REMAP_BASE +
				reg_high, reg_val, is_nrt_vbif);
			MDSS_VBIF_WRITE(mdata, MDSS_VBIF_QOS_LVL_REMAP_BASE +
				reg_high, reg_val_lvl, is_nrt_vbif);
		}
		mutex_unlock(&mdata->reg_lock);
	} else {
		mutex_lock(&mdata->reg_lock);
		for (i = 0; i < mdata->npriority_lvl; i++) {
			reg_val = MDSS_VBIF_READ(mdata,
				MDSS_VBIF_QOS_REMAP_BASE + i*4, is_nrt_vbif);

			mask = 0x3 << (pipe->xin_id * 2);
			reg_val &= ~(mask);
			vbif_qos = is_realtime ?
				mdata->vbif_rt_qos[i] : mdata->vbif_nrt_qos[i];
			reg_val |= vbif_qos << (pipe->xin_id * 2);
			MDSS_VBIF_WRITE(mdata, MDSS_VBIF_QOS_REMAP_BASE + i*4,
				reg_val, is_nrt_vbif);
		}
		mutex_unlock(&mdata->reg_lock);
	}
}

/**
 * mdss_mdp_fixed_qos_arbiter_setup - Program the RT/NRT registers based on
 *              real or non real time clients
 * @mdata:      Pointer to the global mdss data structure.
 * @pipe:       Pointer to source pipe struct to get xin id's.
 * @is_realtime:        To determine if pipe's client is real or
 *                      non real time.
 * This function assumes that clocks are on, so it is caller responsibility to
 * call this function with clocks enabled.
 */
static void mdss_mdp_fixed_qos_arbiter_setup(struct mdss_data_type *mdata,
		struct mdss_mdp_pipe *pipe, bool is_realtime)
{
	u32 mask, reg_val;
	bool is_nrt_vbif = mdss_mdp_is_nrt_vbif_client(mdata, pipe);

	if (!mdata->has_fixed_qos_arbiter_enabled)
		return;

	mutex_lock(&mdata->reg_lock);
	reg_val = MDSS_VBIF_READ(mdata, MDSS_VBIF_FIXED_SORT_EN, is_nrt_vbif);
	mask = 0x1 << pipe->xin_id;
	reg_val |= mask;

	/* Enable the fixed sort for the client */
	MDSS_VBIF_WRITE(mdata, MDSS_VBIF_FIXED_SORT_EN, reg_val, is_nrt_vbif);
	reg_val = MDSS_VBIF_READ(mdata, MDSS_VBIF_FIXED_SORT_SEL0, is_nrt_vbif);
	mask = 0x1 << (pipe->xin_id * 2);
	if (is_realtime) {
		reg_val &= ~mask;
		pr_debug("Real time traffic on pipe type=%x  pnum=%d\n",
				pipe->type, pipe->num);
	} else {
		reg_val |= mask;
		pr_debug("Non real time traffic on pipe type=%x  pnum=%d\n",
				pipe->type, pipe->num);
	}
	/* Set the fixed_sort regs as per RT/NRT client */
	MDSS_VBIF_WRITE(mdata, MDSS_VBIF_FIXED_SORT_SEL0, reg_val, is_nrt_vbif);
	mutex_unlock(&mdata->reg_lock);
}

static void mdss_mdp_init_pipe_params(struct mdss_mdp_pipe *pipe)
{
	kref_init(&pipe->kref);
	init_waitqueue_head(&pipe->free_waitq);
	INIT_LIST_HEAD(&pipe->buf_queue);

	pipe->flags = 0;
	pipe->is_right_blend = false;
	pipe->src_split_req = false;
	pipe->bwc_mode = 0;

	pipe->mfd = NULL;
	pipe->mixer_left = pipe->mixer_right = NULL;
	pipe->mixer_stage = MDSS_MDP_STAGE_UNUSED;
	memset(&pipe->scaler, 0, sizeof(struct mdp_scale_data));
	memset(&pipe->layer, 0, sizeof(struct mdp_input_layer));

	pipe->multirect.mode = MDSS_MDP_PIPE_MULTIRECT_NONE;
}

static int mdss_mdp_pipe_init_config(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, bool pipe_share)
{
	int rc = 0;
	struct mdss_data_type *mdata;

	if (pipe && pipe->unhalted) {
		rc = mdss_mdp_pipe_fetch_halt(pipe, false);
		if (rc) {
			pr_err("%d failed because pipe is in bad state\n",
				pipe->num);
			goto end;
		}
	}

	mdata = mixer->ctl->mdata;

	if (pipe) {
		pr_debug("type=%x   pnum=%d  rect=%d\n",
				pipe->type, pipe->num, pipe->multirect.num);
		mdss_mdp_init_pipe_params(pipe);
	} else if (pipe_share) {
		/*
		 * when there is no dedicated wfd blk, DMA pipe can be
		 * shared as long as its attached to a writeback mixer
		 */
		pipe = mdata->dma_pipes + mixer->num;
		if (pipe->mixer_left->type != MDSS_MDP_MIXER_TYPE_WRITEBACK) {
			rc = -EINVAL;
			goto end;
		}
		kref_get(&pipe->kref);
		pr_debug("pipe sharing for pipe=%d\n", pipe->num);
	}

end:
	return rc;
}

static struct mdss_mdp_pipe *mdss_mdp_pipe_init(struct mdss_mdp_mixer *mixer,
	u32 type, u32 off, struct mdss_mdp_pipe *left_blend_pipe)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_data_type *mdata;
	struct mdss_mdp_pipe *pipe_pool = NULL;
	u32 npipes;
	bool pipe_share = false;
	u32 i;
	int rc;

	if (!mixer || !mixer->ctl || !mixer->ctl->mdata)
		return NULL;

	mdata = mixer->ctl->mdata;

	switch (type) {
	case MDSS_MDP_PIPE_TYPE_VIG:
		pipe_pool = mdata->vig_pipes;
		npipes = mdata->nvig_pipes;
		break;

	case MDSS_MDP_PIPE_TYPE_RGB:
		pipe_pool = mdata->rgb_pipes;
		npipes = mdata->nrgb_pipes;
		break;

	case MDSS_MDP_PIPE_TYPE_DMA:
		pipe_pool = mdata->dma_pipes;
		npipes = mdata->ndma_pipes;
		if ((mdata->wfd_mode == MDSS_MDP_WFD_SHARED) &&
		   (mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK))
			pipe_share = true;
		break;

	case MDSS_MDP_PIPE_TYPE_CURSOR:
		pipe_pool = mdata->cursor_pipes;
		npipes = mdata->ncursor_pipes;
		break;

	default:
		npipes = 0;
		pr_err("invalid pipe type %d\n", type);
		break;
	}

	/* allocate lower priority right blend pipe */
	if (left_blend_pipe && (left_blend_pipe->type == type) && pipe_pool) {
		struct mdss_mdp_pipe *pool_head = pipe_pool + off;
		off += left_blend_pipe->priority - pool_head->priority + 1;
		if (off >= npipes) {
			pr_warn("priority limitation. l_pipe:%d. no low priority %d pipe type available.\n",
				left_blend_pipe->num, type);
			pipe = ERR_PTR(-EBADSLT);
			return pipe;
		}
	}

	for (i = off; i < npipes; i++) {
		pipe = pipe_pool + i;
		if (pipe && atomic_read(&pipe->kref.refcount) == 0) {
			pipe->mixer_left = mixer;
			break;
		}
		pipe = NULL;
	}

	if (pipe && type == MDSS_MDP_PIPE_TYPE_CURSOR) {
		mdss_mdp_init_pipe_params(pipe);
		pr_debug("cursor: type=%x pnum=%d\n",
			pipe->type, pipe->num);
		goto cursor_done;
	}

	rc = mdss_mdp_pipe_init_config(pipe, mixer, pipe_share);
	if (rc)
		return ERR_PTR(-EINVAL);
cursor_done:
	if (!pipe)
		pr_err("no %d type pipes available\n", type);

	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_alloc(struct mdss_mdp_mixer *mixer,
	u32 type, struct mdss_mdp_pipe *left_blend_pipe)
{
	struct mdss_mdp_pipe *pipe;
	mutex_lock(&mdss_mdp_sspp_lock);
	pipe = mdss_mdp_pipe_init(mixer, type, 0, left_blend_pipe);
	mutex_unlock(&mdss_mdp_sspp_lock);
	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_get(u32 ndx,
	enum mdss_mdp_pipe_rect rect_num)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ndx)
		return ERR_PTR(-EINVAL);

	mutex_lock(&mdss_mdp_sspp_lock);

	pipe = mdss_mdp_pipe_search(mdata, ndx, rect_num);
	if (!pipe) {
		pipe = ERR_PTR(-EINVAL);
		goto error;
	}

	if (mdss_mdp_pipe_map(pipe))
		pipe = ERR_PTR(-EACCES);

error:
	mutex_unlock(&mdss_mdp_sspp_lock);
	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_assign(struct mdss_data_type *mdata,
	struct mdss_mdp_mixer *mixer, u32 ndx, enum mdss_mdp_pipe_rect rect_num)
{
	struct mdss_mdp_pipe *pipe = NULL;
	int rc;
	int retry_count = 0;

	if (!ndx)
		return ERR_PTR(-EINVAL);

	mutex_lock(&mdss_mdp_sspp_lock);
	pipe = mdss_mdp_pipe_search(mdata, ndx, rect_num);
	if (!pipe) {
		pr_err("pipe search failed\n");
		pipe = ERR_PTR(-EINVAL);
		goto error;
	}

	if (atomic_read(&pipe->kref.refcount) != 0) {
		mutex_unlock(&mdss_mdp_sspp_lock);
		do {
			rc = wait_event_interruptible_timeout(pipe->free_waitq,
				!atomic_read(&pipe->kref.refcount),
				usecs_to_jiffies(PIPE_CLEANUP_TIMEOUT_US));
			if (rc == 0 || retry_count == 5) {
				pr_err("pipe ndx:%d free wait failed, mfd ndx:%d rc=%d\n",
					pipe->ndx,
					pipe->mfd ? pipe->mfd->index : -1, rc);
				pipe = ERR_PTR(-EBUSY);
				goto end;
			} else if (rc == -ERESTARTSYS) {
				pr_debug("interrupt signal received\n");
				retry_count++;
				continue;
			} else {
				break;
			}
		} while (true);

		mutex_lock(&mdss_mdp_sspp_lock);
	}
	pipe->mixer_left = mixer;

	rc = mdss_mdp_pipe_init_config(pipe, mixer, false);
	if (rc)
		pipe = ERR_PTR(rc);

error:
	mutex_unlock(&mdss_mdp_sspp_lock);
end:
	return pipe;
}

static struct mdss_mdp_pipe *__pipe_lookup(struct mdss_mdp_pipe *pipe_list,
		int count, enum mdss_mdp_pipe_rect rect_num,
		bool (*cmp)(struct mdss_mdp_pipe *, void *), void *data)
{
	struct mdss_mdp_pipe *pipe;
	int i, j, max_rects;

	for (i = 0, pipe = pipe_list; i < count; i++) {
		max_rects = pipe->multirect.max_rects;
		for (j = 0; j < max_rects; j++, pipe++)
			if ((rect_num == pipe->multirect.num) &&
					cmp(pipe, data))
				return pipe;
	}

	return NULL;
}

static struct mdss_mdp_pipe *mdss_mdp_pipe_lookup(
		struct mdss_data_type *mdata, enum mdss_mdp_pipe_rect rect_num,
		bool (*cmp)(struct mdss_mdp_pipe *, void *), void *data)
{
	struct mdss_mdp_pipe *pipe;

	pipe = __pipe_lookup(mdata->vig_pipes, mdata->nvig_pipes,
			rect_num, cmp, data);
	if (pipe)
		return pipe;

	pipe = __pipe_lookup(mdata->rgb_pipes, mdata->nrgb_pipes,
			rect_num, cmp, data);
	if (pipe)
		return pipe;

	pipe = __pipe_lookup(mdata->dma_pipes, mdata->ndma_pipes,
			rect_num, cmp, data);
	if (pipe)
		return pipe;

	pipe = __pipe_lookup(mdata->cursor_pipes, mdata->ncursor_pipes,
			rect_num, cmp, data);
	if (pipe)
		return pipe;

	return NULL;
}

static bool __pipe_cmp_fetch_id(struct mdss_mdp_pipe *pipe, void *data)
{
	u32 *fetch_id = data;

	return pipe->ftch_id == *fetch_id;
}

static struct mdss_mdp_pipe *mdss_mdp_pipe_search_by_client_id(
	struct mdss_data_type *mdata, int client_id,
	enum mdss_mdp_pipe_rect rect_num)
{
	return mdss_mdp_pipe_lookup(mdata, rect_num,
			__pipe_cmp_fetch_id, &client_id);
}

static bool __pipe_cmp_ndx(struct mdss_mdp_pipe *pipe, void *data)
{
	u32 *ndx = data;

	return pipe->ndx == *ndx;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_search(struct mdss_data_type *mdata,
	u32 ndx, enum mdss_mdp_pipe_rect rect_num)
{
	return mdss_mdp_pipe_lookup(mdata, rect_num, __pipe_cmp_ndx, &ndx);
}

/*
 * This API checks if pipe is stagged on mixer or not. If
 * any pipe is stagged on mixer than it will generate the
 * panic signal.
 *
 * Only pipe_free API can call this API.
 */
static void mdss_mdp_pipe_check_stage(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer)
{
	int index;

	if (pipe->mixer_stage == MDSS_MDP_STAGE_UNUSED || !mixer)
		return;

	index = (pipe->mixer_stage * MAX_PIPES_PER_STAGE);
	if (pipe->is_right_blend)
		index++;
	if (index < MAX_PIPES_PER_LM && pipe == mixer->stage_pipe[index]) {
		pr_err("pipe%d mixer:%d pipe->mixer_stage=%d src_split:%d right blend:%d\n",
			pipe->num, mixer->num, pipe->mixer_stage,
			pipe->src_split_req, pipe->is_right_blend);
		MDSS_XLOG_TOUT_HANDLER("mdp", "dbg_bus", "panic");
	}
}

static void mdss_mdp_pipe_hw_cleanup(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	mdss_mdp_pipe_panic_vblank_signal_ctrl(pipe, false);
	mdss_mdp_pipe_panic_signal_ctrl(pipe, false);

	if (pipe->play_cnt) {
		mdss_mdp_pipe_fetch_halt(pipe, false);
		mdss_mdp_pipe_pp_clear(pipe);
		mdss_mdp_smp_free(pipe);
	} else {
		mdss_mdp_smp_unreserve(pipe);
	}

	if (mdss_has_quirk(mdata, MDSS_QUIRK_BWCPANIC) && pipe->bwc_mode) {
		unsigned long pnum_bitmap = BIT(pipe->num);
		bitmap_andnot(mdata->bwc_enable_map, mdata->bwc_enable_map,
			&pnum_bitmap, MAX_DRV_SUP_PIPES);

		if (bitmap_empty(mdata->bwc_enable_map, MAX_DRV_SUP_PIPES))
			mdss_mdp_bwcpanic_ctrl(mdata, false);
	}

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_MULTI_REC_OP_MODE, 0);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

static void mdss_mdp_pipe_free(struct kref *kref)
{
	struct mdss_mdp_pipe *pipe, *next_pipe;

	pipe = container_of(kref, struct mdss_mdp_pipe, kref);

	pr_debug("ndx=%x pnum=%d rect=%d\n",
			pipe->ndx, pipe->num, pipe->multirect.num);

	next_pipe = (struct mdss_mdp_pipe *) pipe->multirect.next;
	if (!next_pipe || (atomic_read(&next_pipe->kref.refcount) == 0)) {
		mdss_mdp_pipe_hw_cleanup(pipe);
	} else {
		pr_debug("skip hw cleanup on pnum=%d rect=%d, rect%d still in use\n",
				pipe->num, pipe->multirect.num,
				next_pipe->multirect.num);
	}

	mdss_mdp_pipe_check_stage(pipe, pipe->mixer_left);
	mdss_mdp_pipe_check_stage(pipe, pipe->mixer_right);
}

static bool mdss_mdp_check_pipe_in_use(struct mdss_mdp_pipe *pipe)
{
	int i;
	bool in_use = false;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		if (!ctl || !ctl->ref_cnt)
			continue;

		mixer = ctl->mixer_left;
		if (!mixer || mixer->rotator_mode)
			continue;

		if (mdss_mdp_mixer_reg_has_pipe(mixer, pipe)) {
			in_use = true;
			pr_err("IN USE: pipe=%d mixer=%d\n",
					pipe->num, mixer->num);
			MDSS_XLOG_TOUT_HANDLER("mdp", "vbif", "vbif_nrt",
				"dbg_bus", "vbif_dbg_bus", "panic");
		}

		mixer = ctl->mixer_right;
		if (mixer && mdss_mdp_mixer_reg_has_pipe(mixer, pipe)) {
			in_use = true;
			pr_err("IN USE: pipe=%d mixer=%d\n",
					pipe->num, mixer->num);
			MDSS_XLOG_TOUT_HANDLER("mdp", "vbif", "vbif_nrt",
				"dbg_bus", "vbif_dbg_bus", "panic");
		}
	}

	return in_use;
}

static int mdss_mdp_is_pipe_idle(struct mdss_mdp_pipe *pipe,
	bool ignore_force_on, bool is_nrt_vbif)
{
	u32 reg_val;
	u32 vbif_idle_mask, forced_on_mask, clk_status_idle_mask;
	bool is_idle = false, is_forced_on;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	forced_on_mask = BIT(pipe->clk_ctrl.bit_off + CLK_FORCE_ON_OFFSET);
	reg_val = readl_relaxed(mdata->mdp_base + pipe->clk_ctrl.reg_off);
	is_forced_on = (reg_val & forced_on_mask) ? true : false;

	pr_debug("pipe#:%d clk_ctrl: 0x%x forced_on_mask: 0x%x\n", pipe->num,
		reg_val, forced_on_mask);
	/* if forced on then no need to check status */
	if (!is_forced_on) {
		clk_status_idle_mask =
			BIT(pipe->clk_status.bit_off + CLK_STATUS_OFFSET);
		reg_val = readl_relaxed(mdata->mdp_base +
			pipe->clk_status.reg_off);

		if ((reg_val & clk_status_idle_mask) == 0)
			is_idle = true;

		pr_debug("pipe#:%d clk_status:0x%x clk_status_idle_mask:0x%x\n",
			pipe->num, reg_val, clk_status_idle_mask);
	}

	if (!ignore_force_on && (is_forced_on || !is_idle))
		goto exit;

	/*
	 * skip vbif check for cursor pipes as the same xin-id is shared
	 * between cursor0, cursor1 and dsi
	 */
	if (pipe->type == MDSS_MDP_PIPE_TYPE_CURSOR) {
		if (ignore_force_on && is_forced_on)
			is_idle = true;
		goto exit;
	}

	vbif_idle_mask = BIT(pipe->xin_id + 16);
	reg_val = MDSS_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL1, is_nrt_vbif);

	if (reg_val & vbif_idle_mask)
		is_idle = true;

	pr_debug("pipe#:%d XIN_HALT_CTRL1: 0x%x, vbif_idle_mask: 0x%x\n",
			pipe->num, reg_val, vbif_idle_mask);

exit:
	return is_idle;
}

/*
 * mdss_mdp_pipe_clk_force_off() - check force off mask and reset for the pipe.
 * @pipe: pointer to the pipe data structure which needs to be checked for clk.
 *
 * This function would be called where software reset is available for pipe
 * clocks.
 */

void mdss_mdp_pipe_clk_force_off(struct mdss_mdp_pipe *pipe)
{
	u32 reg_val, force_off_mask;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	force_off_mask =
		BIT(pipe->clk_ctrl.bit_off + CLK_FORCE_OFF_OFFSET);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	mutex_lock(&mdata->reg_lock);
	reg_val = readl_relaxed(mdata->mdp_base +
			pipe->clk_ctrl.reg_off);
	if (reg_val & force_off_mask) {
		reg_val &= ~force_off_mask;
		writel_relaxed(reg_val,
				mdata->mdp_base + pipe->clk_ctrl.reg_off);
	}
	mutex_unlock(&mdata->reg_lock);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

/**
 * mdss_mdp_pipe_fetch_halt() - Halt VBIF client corresponding to specified pipe
 * @pipe: pointer to the pipe data structure which needs to be halted.
 *
 * Check if VBIF client corresponding to specified pipe is idle or not. If not
 * send a halt request for the client in question and wait for it be idle.
 *
 * This function would typically be called after pipe is unstaged or before it
 * is initialized. On success it should be assumed that pipe is in idle state
 * and would not fetch any more data. This function cannot be called from
 * interrupt context.
 */
int mdss_mdp_pipe_fetch_halt(struct mdss_mdp_pipe *pipe, bool is_recovery)
{
	bool is_idle, forced_on = false, in_use = false;
	int rc = 0;
	u32 reg_val, idle_mask, clk_val, clk_mask;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool sw_reset_avail = mdss_mdp_pipe_is_sw_reset_available(mdata);
	bool is_nrt_vbif = mdss_mdp_is_nrt_vbif_client(mdata, pipe);
	u32 sw_reset_off = pipe->sw_reset.reg_off;
	u32 clk_ctrl_off = pipe->clk_ctrl.reg_off;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	is_idle = mdss_mdp_is_pipe_idle(pipe, true, is_nrt_vbif);
	/*
	 * avoid pipe_in_use check in recovery path as the pipes would not
	 * have been unstaged at this point.
	 */
	if (!is_idle && !is_recovery)
		in_use = mdss_mdp_check_pipe_in_use(pipe);

	if (!is_idle && !in_use) {

		pr_err("%pS: pipe%d is not idle. xin_id=%d\n",
			__builtin_return_address(0), pipe->num, pipe->xin_id);

		mutex_lock(&mdata->reg_lock);
		idle_mask = BIT(pipe->xin_id + 16);

		/*
		 * make sure client clock is not gated while halting by forcing
		 * it ON only if it was not previously forced on
		 */
		clk_val = readl_relaxed(mdata->mdp_base + clk_ctrl_off);
		clk_mask = BIT(pipe->clk_ctrl.bit_off + CLK_FORCE_ON_OFFSET);
		if (!(clk_val & clk_mask)) {
			clk_val |= clk_mask;
			writel_relaxed(clk_val, mdata->mdp_base + clk_ctrl_off);
			wmb(); /* ensure write is finished before progressing */
			forced_on = true;
		}

		reg_val = MDSS_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
								is_nrt_vbif);
		MDSS_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
				reg_val | BIT(pipe->xin_id), is_nrt_vbif);

		if (sw_reset_avail) {
			reg_val = readl_relaxed(mdata->mdp_base + sw_reset_off);
			writel_relaxed(reg_val | BIT(pipe->sw_reset.bit_off),
					mdata->mdp_base + sw_reset_off);
			wmb();
		}
		mutex_unlock(&mdata->reg_lock);

		rc = mdss_mdp_wait_for_xin_halt(pipe->xin_id, is_nrt_vbif);

		mutex_lock(&mdata->reg_lock);
		reg_val = MDSS_VBIF_READ(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
								is_nrt_vbif);
		MDSS_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0,
				reg_val & ~BIT(pipe->xin_id), is_nrt_vbif);

		clk_val = readl_relaxed(mdata->mdp_base + clk_ctrl_off);
		if (forced_on)
			clk_val &= ~clk_mask;

		if (sw_reset_avail) {
			reg_val = readl_relaxed(mdata->mdp_base + sw_reset_off);
			writel_relaxed(reg_val & ~BIT(pipe->sw_reset.bit_off),
				mdata->mdp_base + sw_reset_off);
			wmb();

			clk_val |= BIT(pipe->clk_ctrl.bit_off +
				CLK_FORCE_OFF_OFFSET);
		}
		writel_relaxed(clk_val, mdata->mdp_base + clk_ctrl_off);
		mutex_unlock(&mdata->reg_lock);
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	return rc;
}

int mdss_mdp_pipe_destroy(struct mdss_mdp_pipe *pipe)
{
	if (!kref_put_mutex(&pipe->kref, mdss_mdp_pipe_free,
			&mdss_mdp_sspp_lock)) {
		pr_err("unable to free pipe %d while still in use\n",
				pipe->num);
		return -EBUSY;
	}

	wake_up_all(&pipe->free_waitq);
	mutex_unlock(&mdss_mdp_sspp_lock);

	return 0;
}

/**
 * mdss_mdp_pipe_handoff() - Handoff staged pipes during bootup
 * @pipe: pointer to the pipe to be handed-off
 *
 * Populate the software structures for the pipe based on the current
 * configuration of the hardware pipe by the reading the appropriate MDP
 * registers.
 *
 * This function would typically be called during MDP probe for the case
 * when certain pipes might be programmed in the bootloader to display
 * the splash screen.
 */
int mdss_mdp_pipe_handoff(struct mdss_mdp_pipe *pipe)
{
	int rc = 0;
	u32 src_fmt, reg = 0, bpp = 0;

	/*
	 * todo: for now, only reading pipe src and dest size details
	 * from the registers. This is needed for appropriately
	 * calculating perf metrics for the handed off pipes.
	 * We may need to parse some more details at a later date.
	 */
	reg = mdss_mdp_pipe_read(pipe, MDSS_MDP_REG_SSPP_SRC_SIZE);
	pipe->src.h = reg >> 16;
	pipe->src.w = reg & 0xFFFF;
	reg = mdss_mdp_pipe_read(pipe, MDSS_MDP_REG_SSPP_OUT_SIZE);
	pipe->dst.h = reg >> 16;
	pipe->dst.w = reg & 0xFFFF;

	/* Assume that the source format is RGB */
	reg = mdss_mdp_pipe_read(pipe, MDSS_MDP_REG_SSPP_SRC_FORMAT);
	bpp = ((reg >> 9) & 0x3) + 1;
	switch (bpp) {
	case 4:
		src_fmt = MDP_RGBA_8888;
		break;
	case 3:
		src_fmt = MDP_RGB_888;
		break;
	case 2:
		src_fmt = MDP_RGB_565;
		break;
	default:
		pr_err("Invalid bpp=%d found\n", bpp);
		rc = -EINVAL;
		goto error;
	}
	pipe->src_fmt = mdss_mdp_get_format_params(src_fmt);
	if (!pipe->src_fmt) {
		pr_err("%s: failed to retrieve format parameters\n",
			__func__);
		rc = -EINVAL;
		goto error;
	}

	pr_debug("Pipe settings: src.h=%d src.w=%d dst.h=%d dst.w=%d bpp=%d\n"
		, pipe->src.h, pipe->src.w, pipe->dst.h, pipe->dst.w,
		pipe->src_fmt->bpp);

	pipe->is_handed_off = true;
	pipe->play_cnt = 1;
	mdss_mdp_init_pipe_params(pipe);

error:
	return rc;
}

void mdss_mdp_pipe_position_update(struct mdss_mdp_pipe *pipe,
		struct mdss_rect *src, struct mdss_rect *dst)
{
	u32 src_size, src_xy, dst_size, dst_xy;
	u32 tmp_src_size, tmp_src_xy, reg_data;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	src_size = (src->h << 16) | src->w;
	src_xy = (src->y << 16) | src->x;
	dst_size = (dst->h << 16) | dst->w;

	/*
	 * base layer requirements are different compared to other layers
	 * located at different stages. If source split is enabled and base
	 * layer is used, base layer on the right LM's x offset is relative
	 * to right LM's co-ordinate system unlike other layers which are
	 * relative to left LM's top-left.
	 */
	if (pipe->mixer_stage == MDSS_MDP_STAGE_BASE && mdata->has_src_split
			&& dst->x >= left_lm_w_from_mfd(pipe->mfd))
		dst->x -= left_lm_w_from_mfd(pipe->mfd);
	dst_xy = (dst->y << 16) | dst->x;

	/*
	 * Software overfetch is used when scalar pixel extension is
	 * not enabled
	 */
	if (pipe->overfetch_disable && !pipe->scaler.enable) {
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_LEFT)
			src_xy &= ~0xFFFF;
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_TOP)
			src_xy &= ~(0xFFFF << 16);
	}

	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_103) &&
		pipe->bwc_mode) {
		/* check source dimensions change */
		tmp_src_size = mdss_mdp_pipe_read(pipe,
						 MDSS_MDP_REG_SSPP_SRC_SIZE);
		tmp_src_xy = mdss_mdp_pipe_read(pipe,
						 MDSS_MDP_REG_SSPP_SRC_XY);
		if (src_xy != tmp_src_xy || tmp_src_size != src_size) {
			reg_data = readl_relaxed(mdata->mdp_base +
							 AHB_CLK_OFFSET);
			reg_data |= BIT(28);
			writel_relaxed(reg_data,
					 mdata->mdp_base + AHB_CLK_OFFSET);
		}
	}

	if (pipe->multirect.num == MDSS_MDP_PIPE_RECT0) {
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_SIZE, src_size);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_XY, src_xy);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_SIZE, dst_size);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_XY, dst_xy);
	} else {
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_SIZE_REC1,
				    src_size);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_XY_REC1,
				    src_xy);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_SIZE_REC1,
				    dst_size);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_XY_REC1,
				    dst_xy);
	}

	MDSS_XLOG(pipe->num, pipe->multirect.num, src_size, src_xy,
		  dst_size, dst_xy, pipe->multirect.mode);
}

static void mdss_mdp_pipe_stride_update(struct mdss_mdp_pipe *pipe)
{
	u32 reg0, reg1;
	u32 ystride[MAX_PLANES] = {0};
	struct mdss_mdp_pipe *rec0_pipe, *rec1_pipe;
	u32 secure = 0;

	/*
	 * since stride registers are shared between both rectangles in
	 * multirect mode, delayed programming allows programming of both
	 * together
	 */
	if (is_pipe_programming_delay_needed(pipe)) {
		pr_debug("skip stride programming for pipe%d rec%d\n",
			pipe->num, pipe->multirect.num);
		return;
	}

	if (pipe->multirect.mode == MDSS_MDP_PIPE_MULTIRECT_NONE) {
		memcpy(&ystride, &pipe->src_planes.ystride,
		       sizeof(u32) * MAX_PLANES);
		if (pipe->flags & MDP_SECURE_OVERLAY_SESSION)
			secure = 0xF;
	} else {
		if (pipe->multirect.num == MDSS_MDP_PIPE_RECT0) {
			rec0_pipe = pipe;
			rec1_pipe = pipe->multirect.next;
		} else {
			rec1_pipe = pipe;
			rec0_pipe = pipe->multirect.next;
		}

		ystride[0] = rec0_pipe->src_planes.ystride[0];
		ystride[2] = rec0_pipe->src_planes.ystride[2];
		if (rec0_pipe->flags & MDP_SECURE_OVERLAY_SESSION)
			secure |= 0x5;

		ystride[1] = rec1_pipe->src_planes.ystride[0];
		ystride[3] = rec1_pipe->src_planes.ystride[2];
		if (rec1_pipe->flags & MDP_SECURE_OVERLAY_SESSION)
			secure |= 0xA;
	}

	reg0 =  (ystride[0]) | (ystride[1] << 16);
	reg1 =  (ystride[2]) | (ystride[3] << 16);

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_ADDR_SW_STATUS, secure);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_YSTRIDE0, reg0);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_YSTRIDE1, reg1);

	pr_debug("pipe%d multirect:num%d mode=%d, ystride0=0x%x ystride1=0x%x\n",
		pipe->num, pipe->multirect.num, pipe->multirect.mode,
		reg0, reg1);
	MDSS_XLOG(pipe->num, pipe->multirect.num,
		  pipe->multirect.mode, reg0, reg1);
}

static int mdss_mdp_image_setup(struct mdss_mdp_pipe *pipe,
					struct mdss_mdp_data *data)
{
	u32 img_size;
	u32 width, height, decimation;
	int ret = 0;
	struct mdss_rect dst, src;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool rotation = false;

	pr_debug("ctl: %d pnum=%d wh=%dx%d src={%d,%d,%d,%d} dst={%d,%d,%d,%d}\n",
			pipe->mixer_left->ctl->num, pipe->num,
			pipe->img_width, pipe->img_height,
			pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
			pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	width = pipe->img_width;
	height = pipe->img_height;

	if (pipe->flags & MDP_SOURCE_ROTATED_90)
		rotation = true;

	mdss_mdp_get_plane_sizes(pipe->src_fmt, width, height,
			&pipe->src_planes, pipe->bwc_mode, rotation);

	if (data != NULL) {
		ret = mdss_mdp_data_check(data, &pipe->src_planes,
			pipe->src_fmt);
		if (ret)
			return ret;
	}

	if ((pipe->flags & MDP_DEINTERLACE) &&
			!(pipe->flags & MDP_SOURCE_ROTATED_90)) {
		int i;
		for (i = 0; i < pipe->src_planes.num_planes; i++)
			pipe->src_planes.ystride[i] *= 2;
		width *= 2;
		height /= 2;
	}

	decimation = ((1 << pipe->horz_deci) - 1) << 8;
	decimation |= ((1 << pipe->vert_deci) - 1);
	if (decimation)
		pr_debug("Image decimation h=%d v=%d\n",
				pipe->horz_deci, pipe->vert_deci);

	dst = pipe->dst;
	src = pipe->src;

	if (!pipe->mixer_left->ctl->is_video_mode &&
	    (pipe->mixer_left->type != MDSS_MDP_MIXER_TYPE_WRITEBACK)) {

		struct mdss_rect roi = pipe->mixer_left->roi;
		bool is_right_mixer = pipe->mixer_left->is_right_mixer;
		struct mdss_mdp_ctl *main_ctl;

		if (pipe->mixer_left->ctl->is_master)
			main_ctl = pipe->mixer_left->ctl;
		else
			main_ctl = mdss_mdp_get_main_ctl(pipe->mixer_left->ctl);

		if (!main_ctl) {
			pr_err("Error: couldn't find main_ctl for pipe%d\n",
				pipe->num);
			return -EINVAL;
		}

		if (pipe->src_split_req && main_ctl->mixer_right->valid_roi) {
			/*
			 * pipe is staged on both mixers, expand roi to span
			 * both mixers before cropping pipe's dimensions.
			 */
			roi.w += main_ctl->mixer_right->roi.w;
		} else if (mdata->has_src_split && is_right_mixer) {
			/*
			 * pipe is only on right mixer but since source-split
			 * is enabled, its dst_x is full panel coordinate
			 * aligned where as ROI is mixer coordinate aligned.
			 * Modify dst_x before applying ROI crop.
			 */
			dst.x -= left_lm_w_from_mfd(pipe->mfd);
		}

		mdss_mdp_crop_rect(&src, &dst, &roi);

		if (mdata->has_src_split && is_right_mixer) {
			/*
			 * re-adjust dst_x only if both mixers are active,
			 * meaning right mixer will be working in source
			 * split mode.
			 */
			if (mdss_mdp_is_both_lm_valid(main_ctl))
				dst.x += main_ctl->mixer_left->roi.w;
		}

		if (pipe->flags & MDP_FLIP_LR) {
			src.x = pipe->src.x + (pipe->src.x + pipe->src.w)
				- (src.x + src.w);
		}
		if (pipe->flags & MDP_FLIP_UD) {
			src.y = pipe->src.y + (pipe->src.y + pipe->src.h)
				- (src.y + src.h);
		}
	}

	/*
	 * Software overfetch is used when scalar pixel extension is
	 * not enabled
	 */
	if (pipe->overfetch_disable && !pipe->scaler.enable) {
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_BOTTOM) {
			height = pipe->src.h;
			if (!(pipe->overfetch_disable & OVERFETCH_DISABLE_TOP))
				height += pipe->src.y;
		}
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_RIGHT) {
			width = pipe->src.w;
			if (!(pipe->overfetch_disable & OVERFETCH_DISABLE_LEFT))
				width += pipe->src.x;
		}

		pr_debug("overfetch w=%d/%d h=%d/%d\n", width,
			pipe->img_width, height, pipe->img_height);
	}
	img_size = (height << 16) | width;

	/*
	 * in solid fill, there is no src rectangle, but hardware needs to
	 * be programmed same as dst to avoid issues in scaling blocks
	 */
	if (data == NULL) {
		src = (struct mdss_rect) {0, 0, dst.w, dst.h};
		decimation = 0;
	}

	mdss_mdp_pipe_position_update(pipe, &src, &dst);
	mdss_mdp_pipe_stride_update(pipe);

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_IMG_SIZE, img_size);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_DECIMATION_CONFIG,
			decimation);

	return 0;
}

static void mdss_mdp_set_pipe_cdp(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 cdp_settings = 0x0;
	bool is_rotator = (pipe->mixer_left && pipe->mixer_left->rotator_mode);

	/* Disable CDP for rotator pipe in v1 */
	if (is_rotator && mdss_has_quirk(mdata, MDSS_QUIRK_ROTCDP))
		goto exit;

	cdp_settings = MDSS_MDP_CDP_ENABLE;

	if (!mdss_mdp_is_linear_format(pipe->src_fmt)) {
		/* Enable Amortized for non-linear formats */
		cdp_settings |= MDSS_MDP_CDP_ENABLE_UBWCMETA;
		cdp_settings |= MDSS_MDP_CDP_AMORTIZED;
	} else {
		/* 64-transactions for line mode otherwise we keep 32 */
		if (!is_rotator)
			cdp_settings |= MDSS_MDP_CDP_AHEAD_64;
	}

exit:
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_CDP_CTRL, cdp_settings);
}

static int mdss_mdp_format_setup(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_format_params *fmt;
	u32 chroma_samp, unpack, src_format;
	u32 opmode;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	fmt = pipe->src_fmt;

	opmode = pipe->bwc_mode;
	if (pipe->flags & MDP_FLIP_LR)
		opmode |= MDSS_MDP_OP_FLIP_LR;
	if (pipe->flags & MDP_FLIP_UD)
		opmode |= MDSS_MDP_OP_FLIP_UD;

	pr_debug("pnum=%d format=%d opmode=%x\n", pipe->num, fmt->format,
			opmode);

	chroma_samp = fmt->chroma_sample;
	if (pipe->flags & MDP_SOURCE_ROTATED_90) {
		if (chroma_samp == MDSS_MDP_CHROMA_H2V1)
			chroma_samp = MDSS_MDP_CHROMA_H1V2;
		else if (chroma_samp == MDSS_MDP_CHROMA_H1V2)
			chroma_samp = MDSS_MDP_CHROMA_H2V1;
	}

	src_format = (chroma_samp << 23) |
		     (fmt->fetch_planes << 19) |
		     (fmt->bits[C3_ALPHA] << 6) |
		     (fmt->bits[C2_R_Cr] << 4) |
		     (fmt->bits[C1_B_Cb] << 2) |
		     (fmt->bits[C0_G_Y] << 0);

	if (mdss_mdp_is_tile_format(fmt))
		src_format |= BIT(30);

	if (pipe->flags & MDP_ROT_90)
		src_format |= BIT(11); /* ROT90 */

	if (fmt->alpha_enable &&
			fmt->fetch_planes != MDSS_MDP_PLANE_INTERLEAVED)
		src_format |= BIT(8); /* SRCC3_EN */

	unpack = (fmt->element[3] << 24) | (fmt->element[2] << 16) |
			(fmt->element[1] << 8) | (fmt->element[0] << 0);
	src_format |= ((fmt->unpack_count - 1) << 12) |
			(fmt->unpack_tight << 17) |
			(fmt->unpack_align_msb << 18) |
			((fmt->bpp - 1) << 9);

	if (mdss_mdp_is_ubwc_format(fmt)) {
		opmode |= BIT(0);
		src_format |= BIT(31);
	}

	if (fmt->is_yuv && test_bit(MDSS_CAPS_YUV_CONFIG, mdata->mdss_caps_map))
		src_format |= BIT(15);

	src_format |= (fmt->unpack_dx_format << 14);

	mdss_mdp_pipe_sspp_setup(pipe, &opmode);
	if (fmt->fetch_mode != MDSS_MDP_FETCH_LINEAR
		&& mdata->highest_bank_bit) {
		u32 fetch_config = MDSS_MDP_FETCH_CONFIG_RESET_VALUE;

		fetch_config |= (mdata->highest_bank_bit << 18);
		if (fmt->format == MDP_Y_CBCR_H2V2_TP10_UBWC)
			fetch_config |= (2 << 16);

		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_FETCH_CONFIG,
			fetch_config);
	}
	if (pipe->scaler.enable)
		opmode |= (1 << 31);

	if (pipe->multirect.num == MDSS_MDP_PIPE_RECT0) {
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_FORMAT, src_format);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_UNPACK_PATTERN, unpack);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_OP_MODE, opmode);
	} else {
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_FORMAT_REC1, src_format);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_UNPACK_PATTERN_REC1, unpack);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_OP_MODE_REC1, opmode);
	}

	/* clear UBWC error */
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_UBWC_ERROR_STATUS, BIT(31));

	/* configure CDP */
	if (test_bit(MDSS_QOS_CDP, mdata->mdss_qos_map))
		mdss_mdp_set_pipe_cdp(pipe);

	return 0;
}

int mdss_mdp_pipe_addr_setup(struct mdss_data_type *mdata,
	struct mdss_mdp_pipe *head, u32 *offsets, u32 *ftch_id, u32 *xin_id,
	u32 type, const int *pnums, u32 len, u32 rects_per_sspp,
	u8 priority_base)
{
	u32 i, j;

	if (!head || !mdata) {
		pr_err("unable to setup pipe type=%d: invalid input\n", type);
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		struct mdss_mdp_pipe *pipe = head + (i * rects_per_sspp);

		pipe->type = type;
		pipe->ftch_id  = ftch_id[i];
		pipe->xin_id = xin_id[i];
		pipe->num = pnums[i];
		pipe->ndx = BIT(pnums[i]);
		pipe->priority = i + priority_base;
		pipe->base = mdata->mdss_io.base + offsets[i];
		pipe->multirect.num = MDSS_MDP_PIPE_RECT0;
		pipe->multirect.mode = MDSS_MDP_PIPE_MULTIRECT_NONE;
		pipe->multirect.max_rects = rects_per_sspp;
		pipe->multirect.next = NULL;

		pr_info("type:%d ftchid:%d xinid:%d num:%d rect:%d ndx:0x%x prio:%d\n",
			pipe->type, pipe->ftch_id, pipe->xin_id, pipe->num,
			pipe->multirect.num, pipe->ndx, pipe->priority);

		for (j = 1; j < rects_per_sspp; j++) {
			struct mdss_mdp_pipe *next = pipe + j;

			pipe[j-1].multirect.next = next;
			*next = pipe[j-1];
			next->multirect.num++;
			next->multirect.next = pipe;

			pr_info("type:%d ftchid:%d xinid:%d num:%d rect:%d ndx:0x%x prio:%d\n",
				next->type, next->ftch_id, next->xin_id,
				next->num, next->multirect.num, next->ndx,
				next->priority);
		}

	}

	return 0;
}

static int mdss_mdp_src_addr_setup(struct mdss_mdp_pipe *pipe,
				   struct mdss_mdp_data *src_data)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int i, ret = 0;
	u32 addr[MAX_PLANES] = { 0 };

	pr_debug("pnum=%d\n", pipe->num);

	ret = mdss_mdp_data_check(src_data, &pipe->src_planes, pipe->src_fmt);
	if (ret)
		return ret;

	if (pipe->overfetch_disable && !pipe->scaler.enable) {
		u32 x = 0, y = 0;

		if (pipe->overfetch_disable & OVERFETCH_DISABLE_LEFT)
			x = pipe->src.x;
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_TOP)
			y = pipe->src.y;

		mdss_mdp_data_calc_offset(src_data, x, y,
			&pipe->src_planes, pipe->src_fmt);
	}

	for (i = 0; i < MAX_PLANES; i++)
		addr[i] = src_data->p[i].addr;

	/* planar format expects YCbCr, swap chroma planes if YCrCb */
	if (mdata->mdp_rev < MDSS_MDP_HW_REV_102 &&
			(pipe->src_fmt->fetch_planes == MDSS_MDP_PLANE_PLANAR)
				&& (pipe->src_fmt->element[0] == C1_B_Cb))
		swap(addr[1], addr[2]);

	if (pipe->multirect.mode == MDSS_MDP_PIPE_MULTIRECT_NONE) {
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC0_ADDR, addr[0]);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC1_ADDR, addr[1]);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC2_ADDR, addr[2]);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC3_ADDR, addr[3]);
	} else if (pipe->multirect.num == MDSS_MDP_PIPE_RECT0) {
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC0_ADDR, addr[0]);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC2_ADDR, addr[2]);
	} else { /* RECT1 */
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC1_ADDR, addr[0]);
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC3_ADDR, addr[2]);
	}

	return 0;
}

static int mdss_mdp_pipe_solidfill_setup(struct mdss_mdp_pipe *pipe)
{
	int ret;
	u32 secure, format, unpack, opmode = 0;

	pr_debug("solid fill setup on pnum=%d\n", pipe->num);

	ret = mdss_mdp_image_setup(pipe, NULL);
	if (ret) {
		pr_err("image setup error for pnum=%d\n", pipe->num);
		return ret;
	}

	format = MDSS_MDP_FMT_SOLID_FILL;
	secure = (pipe->flags & MDP_SECURE_OVERLAY_SESSION ? 0xF : 0x0);

	/* support ARGB color format only */
	unpack = (C3_ALPHA << 24) | (C2_R_Cr << 16) |
		(C1_B_Cb << 8) | (C0_G_Y << 0);
	if (pipe->scaler.enable)
		opmode |= (1 << 31);

	if (pipe->multirect.num == MDSS_MDP_PIPE_RECT0) {
		/*
		 * rect0 will drive whether to secure the pipeline, even though
		 * no secure content is being fetched
		 */
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_ADDR_SW_STATUS, secure);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_FORMAT, format);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_CONSTANT_COLOR, pipe->bg_color);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_UNPACK_PATTERN, unpack);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_OP_MODE, opmode);
	} else {
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_FORMAT_REC1, format);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_CONSTANT_COLOR_REC1,
			pipe->bg_color);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_UNPACK_PATTERN_REC1, unpack);
		mdss_mdp_pipe_write(pipe,
			MDSS_MDP_REG_SSPP_SRC_OP_MODE_REC1, opmode);
	}

	if (pipe->type != MDSS_MDP_PIPE_TYPE_DMA) {
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SCALE_CONFIG, 0);
		if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG)
			mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_VIG_OP_MODE, 0);
	}

	return 0;
}

static void mdss_mdp_set_ot_limit_pipe(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_set_ot_params ot_params;
	struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;

	ot_params.xin_id = pipe->xin_id;
	ot_params.num = pipe->num;
	ot_params.width = pipe->src.w;
	ot_params.height = pipe->src.h;
	ot_params.reg_off_vbif_lim_conf = MMSS_VBIF_RD_LIM_CONF;
	ot_params.reg_off_mdp_clk_ctrl = pipe->clk_ctrl.reg_off;
	ot_params.bit_off_mdp_clk_ctrl = pipe->clk_ctrl.bit_off +
		CLK_FORCE_ON_OFFSET;
	ot_params.is_rot = pipe->mixer_left->rotator_mode;
	ot_params.is_wb = ctl->intf_num == MDSS_MDP_NO_INTF;
	ot_params.is_yuv = pipe->src_fmt->is_yuv;
	ot_params.frame_rate = pipe->frame_rate;

	/* rotator read uses nrt vbif */
	if (mdss_mdp_is_nrt_vbif_base_defined(ctl->mdata) &&
			pipe->mixer_left->rotator_mode)
		ot_params.is_vbif_nrt = true;
	else
		ot_params.is_vbif_nrt = false;

	mdss_mdp_set_ot_limit(&ot_params);
}

bool mdss_mdp_is_amortizable_pipe(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, struct mdss_data_type *mdata)
{
	/* do not apply for rotator or WB */
	return ((pipe->src.y > mdata->prefill_data.ts_threshold) &&
		(mixer->type == MDSS_MDP_MIXER_TYPE_INTF));
}

static inline void __get_ordered_rects(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_pipe **low_pipe,
	struct mdss_mdp_pipe **high_pipe)
{
	*low_pipe = pipe;

	if (pipe->multirect.mode == MDSS_MDP_PIPE_MULTIRECT_NONE) {
		*high_pipe = NULL;
		return;
	}

	*high_pipe = pipe->multirect.next;

	/* if pipes are not in order, order them according to position */
	if ((*low_pipe)->src.y > (*high_pipe)->src.y) {
		*low_pipe = pipe->multirect.next;
		*high_pipe = pipe;
	}
}

static u32 __get_ts_count(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, bool is_low_pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 ts_diff, ts_ypos;
	struct mdss_mdp_pipe *low_pipe, *high_pipe;
	u32 ts_count = 0;
	u32 v_total, fps, h_total, xres;

	if (mdss_mdp_get_panel_params(pipe, mixer, &fps, &v_total,
			&h_total, &xres)) {
		pr_err(" error retreiving the panel params!\n");
		return -EINVAL;
	}

	if (is_low_pipe) {
		/* only calculate count if lower pipe is amortizable */
		if (mdss_mdp_is_amortizable_pipe(pipe, mixer, mdata)) {
			ts_diff = mdata->prefill_data.ts_threshold -
					mdata->prefill_data.ts_end;
			ts_ypos = pipe->src.y - ts_diff;
			ts_count = mult_frac(ts_ypos, 19200000, fps * v_total);
		}
	} else { /* high pipe */

		/* only calculate count for high pipe in serial mode */
		if (pipe &&
		    pipe->multirect.mode == MDSS_MDP_PIPE_MULTIRECT_SERIAL) {
			__get_ordered_rects(pipe, &low_pipe, &high_pipe);
			ts_count = high_pipe->src.y - low_pipe->src.y - 1;
			ts_count = mult_frac(ts_count, 19200000, fps * v_total);
		}
	}

	return ts_count;
}

static u32 __calc_ts_bytes(struct mdss_rect *src, u32 fps, u32 bpp)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 ts_bytes;

	ts_bytes = src->h * src->w *
		bpp * fps;
	ts_bytes = mult_frac(ts_bytes,
		mdata->prefill_data.ts_rate.numer,
		mdata->prefill_data.ts_rate.denom);
	ts_bytes /= 19200000;

	return ts_bytes;
}

static u32 __get_ts_bytes(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_pipe *low_pipe, *high_pipe;
	u32 v_total, fps, h_total, xres;
	u64 low_pipe_bw, high_pipe_bw, temp;
	u32 ts_bytes_low, ts_bytes_high;
	u64 ts_bytes = 0;

	if (mdss_mdp_get_panel_params(pipe, mixer, &fps, &v_total,
			&h_total, &xres)) {
		pr_err(" error retreiving the panel params!\n");
		return -EINVAL;
	}

	switch (pipe->multirect.mode) {
	case MDSS_MDP_PIPE_MULTIRECT_NONE:

		/* do not amortize if pipe is not amortizable */
		if (!mdss_mdp_is_amortizable_pipe(pipe, mixer, mdata)) {
			ts_bytes = 0;
			goto exit;
		}

		ts_bytes = __calc_ts_bytes(&pipe->src, fps,
			pipe->src_fmt->bpp);

		break;
	case MDSS_MDP_PIPE_MULTIRECT_PARALLEL:

		__get_ordered_rects(pipe, &low_pipe, &high_pipe);

		/* do not amortize if low_pipe is not amortizable */
		if (!mdss_mdp_is_amortizable_pipe(low_pipe, mixer, mdata)) {
			ts_bytes = 0;
			goto exit;
		}

		/* calculate ts bytes as the sum of both rects */
		ts_bytes_low = __calc_ts_bytes(&low_pipe->src, fps,
			low_pipe->src_fmt->bpp);
		ts_bytes_high = __calc_ts_bytes(&low_pipe->src, fps,
			high_pipe->src_fmt->bpp);

		ts_bytes = ts_bytes_low + ts_bytes_high;
		break;
	case MDSS_MDP_PIPE_MULTIRECT_SERIAL:

		__get_ordered_rects(pipe, &low_pipe, &high_pipe);

		/* calculate amortization using per-pipe bw */
		mdss_mdp_get_pipe_overlap_bw(low_pipe,
			&low_pipe->mixer_left->roi,
			&low_pipe_bw, &temp, 0);
		mdss_mdp_get_pipe_overlap_bw(high_pipe,
			&high_pipe->mixer_left->roi,
			&high_pipe_bw, &temp, 0);

		/* amortize depending on the lower pipe amortization */
		if (mdss_mdp_is_amortizable_pipe(low_pipe, mixer, mdata))
			ts_bytes = DIV_ROUND_UP_ULL(max(low_pipe_bw,
				high_pipe_bw), 19200000);
		else
			ts_bytes = DIV_ROUND_UP_ULL(high_pipe_bw, 19200000);
		break;
	default:
		pr_err("unknown multirect mode!\n");
		goto exit;
	break;
	};

	ts_bytes &= 0xFF;
	ts_bytes |= BIT(27) | BIT(31);
exit:
	return (u32) ts_bytes;
}

static int mdss_mdp_set_ts_pipe(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_mixer *mixer;
	u32 ts_count_low = 0, ts_count_high = 0;
	u32 ts_rec0, ts_rec1;
	u32 ts_bytes = 0;
	struct mdss_mdp_pipe *low_pipe = NULL;
	struct mdss_mdp_pipe *high_pipe = NULL;

	if (!test_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map))
		return 0;

	mixer = pipe->mixer_left;
	if (!mixer)
		return -EINVAL;

	if (!mixer->ctl)
		return -EINVAL;

	if (!mdata->prefill_data.ts_threshold ||
	    (mdata->prefill_data.ts_threshold < mdata->prefill_data.ts_end)) {
		pr_err("invalid ts data!\n");
		return -EINVAL;
	}

	/* high pipe will be null for non-multi rect cases */
	__get_ordered_rects(pipe, &low_pipe, &high_pipe);

	ts_count_low  = __get_ts_count(low_pipe, mixer, true);
	if (high_pipe != NULL)
		ts_count_high = __get_ts_count(high_pipe, mixer, false);
	ts_bytes = __get_ts_bytes(pipe, mixer);

	if (low_pipe->multirect.num == MDSS_MDP_PIPE_RECT0) {
		ts_rec0 = ts_count_low;
		ts_rec1 = ts_count_high;
	} else {
		ts_rec0 = ts_count_high;
		ts_rec1 = ts_count_low;
	}

	mdss_mdp_pipe_qos_ctrl(pipe, false, MDSS_MDP_PIPE_QOS_VBLANK_AMORTIZE);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_TRAFFIC_SHAPER, ts_bytes);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_TRAFFIC_SHAPER_PREFILL,
		ts_rec0);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_TRAFFIC_SHAPER_REC1_PREFILL,
		ts_rec1);
	MDSS_XLOG(pipe->num, ts_bytes, ts_rec0, ts_rec1);
	pr_debug("ts: pipe:%d bytes=0x%x count0=0x%x count1=0x%x\n",
		pipe->num, ts_bytes, ts_rec0, ts_rec1);
	return 0;
}

int mdss_mdp_pipe_queue_data(struct mdss_mdp_pipe *pipe,
			     struct mdss_mdp_data *src_data)
{
	int ret = 0;
	struct mdss_mdp_ctl *ctl;
	u32 params_changed;
	u32 opmode = 0, multirect_opmode = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool roi_changed = false;
	bool delayed_programming;

	if (!pipe) {
		pr_err("pipe not setup properly for queue\n");
		return -ENODEV;
	}

	if (!pipe->mixer_left || !pipe->mixer_left->ctl) {
		if (src_data)
			pr_err("pipe%d mixer not setup properly\n", pipe->num);
		return -ENODEV;
	}

	if (pipe->src_split_req && !mdata->has_src_split) {
		pr_err("src split can't be requested on mdp:0x%x\n",
			mdata->mdp_rev);
		return -EINVAL;
	}

	pr_debug("pnum=%x mixer=%d play_cnt=%u\n", pipe->num,
		 pipe->mixer_left->num, pipe->play_cnt);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	ctl = pipe->mixer_left->ctl;
	roi_changed = pipe->mixer_left->roi_changed;

	/*
	 * if pipe is staged on 2 mixers then it is possible that only
	 * right mixer roi has changed.
	 */
	if (pipe->mixer_right)
		roi_changed |= pipe->mixer_right->roi_changed;

	delayed_programming = is_pipe_programming_delay_needed(pipe);

	/*
	 * Reprogram the pipe when there is no dedicated wfd blk and
	 * virtual mixer is allocated for the DMA pipe during concurrent
	 * line and block mode operations
	 */
	params_changed = (pipe->params_changed) ||
		((pipe->type == MDSS_MDP_PIPE_TYPE_DMA) &&
		 (pipe->mixer_left->type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
		 (ctl->mdata->mixer_switched)) || roi_changed;

	/* apply changes that are common in case of multi rects only once */
	if (params_changed && !delayed_programming) {
		bool is_realtime = !((ctl->intf_num == MDSS_MDP_NO_INTF)
				|| pipe->mixer_left->rotator_mode);

		mdss_mdp_pipe_panic_vblank_signal_ctrl(pipe, false);
		mdss_mdp_pipe_panic_signal_ctrl(pipe, false);

		mdss_mdp_qos_vbif_remapper_setup(mdata, pipe, is_realtime);
		mdss_mdp_fixed_qos_arbiter_setup(mdata, pipe, is_realtime);

		if (mdata->vbif_nrt_io.base)
			mdss_mdp_pipe_nrt_vbif_setup(mdata, pipe);

		if (pipe && mdss_mdp_pipe_is_sw_reset_available(mdata))
			mdss_mdp_pipe_clk_force_off(pipe);

		if (pipe->scaler.enable)
			mdss_mdp_pipe_program_pixel_extn(pipe);
	}

	if ((!(pipe->flags & MDP_VPU_PIPE) && (src_data == NULL)) ||
	    (pipe->flags & MDP_SOLID_FILL)) {
		pipe->params_changed = 0;
		mdss_mdp_pipe_solidfill_setup(pipe);

		MDSS_XLOG(pipe->num, pipe->mixer_left->num, pipe->play_cnt,
			0x111);

		goto update_nobuf;
	}

	MDSS_XLOG(pipe->num, pipe->mixer_left->num, pipe->play_cnt, 0x222);

	if (params_changed) {
		pipe->params_changed = 0;

		ret = mdss_mdp_pipe_pp_setup(pipe, &opmode);
		if (ret) {
			pr_err("pipe pp setup error for pnum=%d\n", pipe->num);
			goto done;
		}

		ret = mdss_mdp_image_setup(pipe, src_data);
		if (ret) {
			pr_err("image setup error for pnum=%d\n", pipe->num);
			goto done;
		}

		ret = mdss_mdp_format_setup(pipe);
		if (ret) {
			pr_err("format %d setup error pnum=%d\n",
			       pipe->src_fmt->format, pipe->num);
			goto done;
		}

		if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG)
			mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_VIG_OP_MODE,
			opmode);

		if (!delayed_programming) {
			if (test_bit(MDSS_QOS_PER_PIPE_LUT,
				     mdata->mdss_qos_map))
				mdss_mdp_pipe_qos_lut(pipe);

			if (mdss_mdp_panic_signal_support_mode(mdata) ==
					MDSS_MDP_PANIC_PER_PIPE_CFG)
				mdss_mdp_config_pipe_panic_lut(pipe);

			if (pipe->type != MDSS_MDP_PIPE_TYPE_CURSOR) {
				mdss_mdp_pipe_panic_vblank_signal_ctrl(pipe, 1);
				mdss_mdp_pipe_panic_signal_ctrl(pipe, true);
				mdss_mdp_set_ot_limit_pipe(pipe);
				mdss_mdp_set_ts_pipe(pipe);
			}
		}
	}

	/*
	 * enable multirect only when both RECT0 and RECT1 are enabled,
	 * othwerise expect to work in non-multirect only in RECT0
	 */
	if (pipe->multirect.mode != MDSS_MDP_PIPE_MULTIRECT_NONE) {
		multirect_opmode = BIT(0) | BIT(1);

		if (pipe->multirect.mode == MDSS_MDP_PIPE_MULTIRECT_SERIAL)
			multirect_opmode |= BIT(2);
	}

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_MULTI_REC_OP_MODE,
			    multirect_opmode);
	if (src_data == NULL) {
		pr_debug("src_data=%pK pipe num=%dx\n",
				src_data, pipe->num);
		goto update_nobuf;
	}

	if (pipe->type != MDSS_MDP_PIPE_TYPE_CURSOR)
		mdss_mdp_smp_alloc(pipe);

	ret = mdss_mdp_src_addr_setup(pipe, src_data);
	if (ret) {
		pr_err("addr setup error for pnum=%d\n", pipe->num);
		goto done;
	}

update_nobuf:
	if (pipe->src_split_req) {
		pr_debug("src_split_enabled. pnum:%d\n", pipe->num);
		mdss_mdp_mixer_pipe_update(pipe, ctl->mixer_left,
			params_changed);
		mdss_mdp_mixer_pipe_update(pipe, ctl->mixer_right,
			params_changed);
		pipe->mixer_right = ctl->mixer_right;
	} else {
		mdss_mdp_mixer_pipe_update(pipe, pipe->mixer_left,
			params_changed);
	}

	pipe->play_cnt++;

	if (mdss_has_quirk(mdata, MDSS_QUIRK_BWCPANIC)) {
		unsigned long pnum_bitmap = BIT(pipe->num);
		if (pipe->bwc_mode)
			bitmap_or(mdata->bwc_enable_map, mdata->bwc_enable_map,
				&pnum_bitmap, MAX_DRV_SUP_PIPES);
		else
			bitmap_andnot(mdata->bwc_enable_map,
				mdata->bwc_enable_map, &pnum_bitmap,
				MAX_DRV_SUP_PIPES);
	}

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	return ret;
}

int mdss_mdp_pipe_is_staged(struct mdss_mdp_pipe *pipe)
{
	return (pipe == pipe->mixer_left->stage_pipe[pipe->mixer_stage]);
}

static inline void __mdss_mdp_pipe_program_pixel_extn_helper(
	struct mdss_mdp_pipe *pipe, u32 plane, u32 off)
{
	u32 src_h = DECIMATED_DIMENSION(pipe->src.h, pipe->vert_deci);
	u32 mask = 0xFF;
	u32 lr_pe, tb_pe, tot_req_pixels;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	/*
	 * CB CR plane required pxls need to be accounted
	 * for chroma decimation.
	 */
	if (plane == 1)
		src_h >>= pipe->chroma_sample_v;

	lr_pe = ((pipe->scaler.right_ftch[plane] & mask) << 24)|
		((pipe->scaler.right_rpt[plane] & mask) << 16)|
		((pipe->scaler.left_ftch[plane] & mask) << 8)|
		(pipe->scaler.left_rpt[plane] & mask);

	tb_pe = ((pipe->scaler.btm_ftch[plane] & mask) << 24)|
		((pipe->scaler.btm_rpt[plane] & mask) << 16)|
		((pipe->scaler.top_ftch[plane] & mask) << 8)|
		(pipe->scaler.top_rpt[plane] & mask);

	writel_relaxed(lr_pe, pipe->base +
			MDSS_MDP_REG_SSPP_SW_PIX_EXT_C0_LR + off);
	writel_relaxed(tb_pe, pipe->base +
			MDSS_MDP_REG_SSPP_SW_PIX_EXT_C0_TB + off);

	mask = 0xFFFF;
	if (test_bit(MDSS_CAPS_QSEED3, mdata->mdss_caps_map))
		tot_req_pixels = ((pipe->scaler.num_ext_pxls_top[plane] &
				mask) << 16 |
			(pipe->scaler.num_ext_pxls_left[plane] & mask));
	else
		tot_req_pixels =
			(((src_h + pipe->scaler.num_ext_pxls_top[plane] +
			pipe->scaler.num_ext_pxls_btm[plane]) & mask) << 16) |
			((pipe->scaler.roi_w[plane] +
			pipe->scaler.num_ext_pxls_left[plane] +
			pipe->scaler.num_ext_pxls_right[plane]) & mask);

	writel_relaxed(tot_req_pixels, pipe->base +
			MDSS_MDP_REG_SSPP_SW_PIX_EXT_C0_REQ_PIXELS + off);

	MDSS_XLOG(pipe->num, plane, lr_pe, tb_pe, tot_req_pixels);
	pr_debug("pipe num=%d, plane=%d, LR PE=0x%x, TB PE=0x%x, req_pixels=0x0%x\n",
		pipe->num, plane, lr_pe, tb_pe, tot_req_pixels);
}

/**
 * mdss_mdp_pipe_program_pixel_extn - Program the source pipe's
 *				      sw pixel extension
 * @pipe:	Source pipe struct containing pixel extn values
 *
 * Function programs the pixel extn values calculated during
 * scale setup.
 */
static int mdss_mdp_pipe_program_pixel_extn(struct mdss_mdp_pipe *pipe)
{
	/* Y plane pixel extn */
	__mdss_mdp_pipe_program_pixel_extn_helper(pipe, 0, 0);
	/* CB CR plane pixel extn */
	__mdss_mdp_pipe_program_pixel_extn_helper(pipe, 1, 16);
	/* Alpha plane pixel extn */
	__mdss_mdp_pipe_program_pixel_extn_helper(pipe, 3, 32);
	return 0;
}


static int __pxl_extn_helper(int residue)
{
	int tmp = 0;
	if (residue == 0) {
		return tmp;
	} else if (residue > 0) {
		tmp = (uint32_t) residue;
		tmp >>= PHASE_STEP_SHIFT;
		return -tmp;
	} else {
		tmp = (uint32_t)(-residue);
		tmp >>= PHASE_STEP_SHIFT;
		if ((tmp << PHASE_STEP_SHIFT) != (-residue))
			tmp++;
		return tmp;
	}
}

/**
 * mdss_mdp_calc_pxl_extn - Calculate source pipe's sw pixel extension
 *
 * @pipe:	Source pipe struct containing pixel extn values
 *
 * Function calculates the pixel extn values during scale setup.
 */
void mdss_mdp_pipe_calc_pixel_extn(struct mdss_mdp_pipe *pipe)
{
	int caf, i;
	uint32_t src_h;
	bool unity_scale_x = false, upscale_x = false;
	bool unity_scale_y, upscale_y;

	if (!(pipe->src_fmt->is_yuv))
		unity_scale_x = (pipe->src.w == pipe->dst.w);

	if (!unity_scale_x)
		upscale_x = (pipe->src.w <= pipe->dst.w);

	pr_debug("pipe=%d, src(%d, %d, %d, %d), dest(%d, %d, %d, %d)\n",
			pipe->num,
			pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
			pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	for (i = 0; i < MAX_PLANES; i++) {
		int64_t left = 0, right = 0, top = 0, bottom = 0;
		caf = 0;

		/*
		 * phase step x,y for 0 plane should be calculated before
		 * this
		 */
		if (pipe->src_fmt->is_yuv && (i == 1 || i == 2)) {
			pipe->scaler.phase_step_x[i] =
				pipe->scaler.phase_step_x[0]
					>> pipe->chroma_sample_h;
			pipe->scaler.phase_step_y[i] =
				pipe->scaler.phase_step_y[0]
					>> pipe->chroma_sample_v;
		} else if (i > 0) {
			pipe->scaler.phase_step_x[i] =
				pipe->scaler.phase_step_x[0];
			pipe->scaler.phase_step_y[i] =
				pipe->scaler.phase_step_y[0];
		}
		/* Pixel extension calculations for X direction */
		pipe->scaler.roi_w[i] = DECIMATED_DIMENSION(pipe->src.w,
			pipe->horz_deci);

		if (pipe->src_fmt->is_yuv)
			pipe->scaler.roi_w[i] &= ~0x1;

		/* CAF filtering on only luma plane */
		if (i == 0 && pipe->src_fmt->is_yuv)
			caf = 1;
		if (i == 1 || i == 2)
			pipe->scaler.roi_w[i] >>= pipe->chroma_sample_h;

		pr_debug("roi_w[%d]=%d, caf=%d\n", i, pipe->scaler.roi_w[i],
			caf);
		if (unity_scale_x) {
			left = 0;
			right = 0;
		} else if (!upscale_x) {
			left = 0;
			right = (pipe->dst.w - 1) *
				pipe->scaler.phase_step_x[i];
			right -= (pipe->scaler.roi_w[i] - 1) *
				PHASE_STEP_UNIT_SCALE;
			right += pipe->scaler.phase_step_x[i];
			right = -(right);
		} else {
			left = (1 << PHASE_RESIDUAL);
			left -= (caf * PHASE_STEP_UNIT_SCALE);

			right = (1 << PHASE_RESIDUAL);
			right += (pipe->dst.w - 1) *
				pipe->scaler.phase_step_x[i];
			right -= ((pipe->scaler.roi_w[i] - 1) *
				PHASE_STEP_UNIT_SCALE);
			right += (caf * PHASE_STEP_UNIT_SCALE);
			right = -(right);
		}
		pr_debug("left=%lld, right=%lld\n", left, right);
		pipe->scaler.num_ext_pxls_left[i] = __pxl_extn_helper(left);
		pipe->scaler.num_ext_pxls_right[i] = __pxl_extn_helper(right);

		/* Pixel extension calculations for Y direction */
		unity_scale_y = false;
		upscale_y = false;

		src_h = DECIMATED_DIMENSION(pipe->src.h, pipe->vert_deci);

		/* Subsampling of chroma components is factored */
		if (i == 1 || i == 2)
			src_h >>= pipe->chroma_sample_v;

		if (!(pipe->src_fmt->is_yuv))
			unity_scale_y = (src_h == pipe->dst.h);

		if (!unity_scale_y)
			upscale_y = (src_h <= pipe->dst.h);

		if (unity_scale_y) {
			top = 0;
			bottom = 0;
		} else if (!upscale_y) {
			top = 0;
			bottom = (pipe->dst.h - 1) *
				pipe->scaler.phase_step_y[i];
			bottom -= (src_h - 1) * PHASE_STEP_UNIT_SCALE;
			bottom += pipe->scaler.phase_step_y[i];
			bottom = -(bottom);
		} else {
			top = (1 << PHASE_RESIDUAL);
			top -= (caf * PHASE_STEP_UNIT_SCALE);

			bottom = (1 << PHASE_RESIDUAL);
			bottom += (pipe->dst.h - 1) *
				pipe->scaler.phase_step_y[i];
			bottom -= (src_h - 1) * PHASE_STEP_UNIT_SCALE;
			bottom += (caf * PHASE_STEP_UNIT_SCALE);
			bottom = -(bottom);
		}

		pipe->scaler.num_ext_pxls_top[i] = __pxl_extn_helper(top);
		pipe->scaler.num_ext_pxls_btm[i] = __pxl_extn_helper(bottom);

		/* Single pixel rgb scale adjustment */
		if ((!(pipe->src_fmt->is_yuv)) &&
			((pipe->src.h - pipe->dst.h) == 1)) {

			uint32_t residue = pipe->scaler.phase_step_y[i] -
				PHASE_STEP_UNIT_SCALE;
			uint32_t result = (pipe->dst.h * residue) + residue;
			if (result < PHASE_STEP_UNIT_SCALE)
				pipe->scaler.num_ext_pxls_btm[i] -= 1;
		}

		if (pipe->scaler.num_ext_pxls_left[i] >= 0)
			pipe->scaler.left_rpt[i] =
				pipe->scaler.num_ext_pxls_left[i];
		else
			pipe->scaler.left_ftch[i] =
				pipe->scaler.num_ext_pxls_left[i];

		if (pipe->scaler.num_ext_pxls_right[i] >= 0)
			pipe->scaler.right_rpt[i] =
				pipe->scaler.num_ext_pxls_right[i];
		else
			pipe->scaler.right_ftch[i] =
				pipe->scaler.num_ext_pxls_right[i];

		if (pipe->scaler.num_ext_pxls_top[i] >= 0)
			pipe->scaler.top_rpt[i] =
				pipe->scaler.num_ext_pxls_top[i];
		else
			pipe->scaler.top_ftch[i] =
				pipe->scaler.num_ext_pxls_top[i];

		if (pipe->scaler.num_ext_pxls_btm[i] >= 0)
			pipe->scaler.btm_rpt[i] =
				pipe->scaler.num_ext_pxls_btm[i];
		else
			pipe->scaler.btm_ftch[i] =
				pipe->scaler.num_ext_pxls_btm[i];

		pr_debug("plane repeat=%d, left=%d, right=%d, top=%d, btm=%d\n",
				i, pipe->scaler.left_rpt[i],
				pipe->scaler.right_rpt[i],
				pipe->scaler.top_rpt[i],
				pipe->scaler.btm_rpt[i]);
		pr_debug("plane overfetch=%d, left=%d, right=%d, top=%d, btm=%d\n",
				i, pipe->scaler.left_ftch[i],
				pipe->scaler.right_ftch[i],
				pipe->scaler.top_ftch[i],
				pipe->scaler.btm_ftch[i]);
	}

	pipe->scaler.enable = 1;
}

/**
 * mdss_mdp_pipe_calc_qseed3_cfg - Calculate source pipe's sw qseed3 filter
 * configuration
 *
 * @pipe:	Source pipe struct
 *
 * Function set the qseed3 filter configuration to bilenear configuration
 * also calclulates pixel extension for qseed3
 */
void mdss_mdp_pipe_calc_qseed3_cfg(struct mdss_mdp_pipe *pipe)
{
	int i;
	int roi_h;

	/* calculate qseed3 pixel extension values */
	for (i = 0; i < MAX_PLANES; i++) {

		/* Pixel extension calculations for X direction */
		pipe->scaler.roi_w[i] = DECIMATED_DIMENSION(pipe->src.w,
			pipe->horz_deci);

		if (pipe->src_fmt->is_yuv)
			pipe->scaler.roi_w[i] &= ~0x1;
		/*
		 * phase step x,y for 0 plane should be calculated before
		 * this
		 */
		if (pipe->src_fmt->is_yuv && (i == 1 || i == 2)) {
			pipe->scaler.phase_step_x[i] =
				pipe->scaler.phase_step_x[0]
					>> pipe->chroma_sample_h;

			pipe->scaler.phase_step_y[i] =
				pipe->scaler.phase_step_y[0]
					>> pipe->chroma_sample_v;

			pipe->scaler.roi_w[i] >>= pipe->chroma_sample_h;
		}

		pipe->scaler.preload_x[i] = QSEED3_DEFAULT_PRELAOD_H;
		pipe->scaler.src_width[i] = pipe->scaler.roi_w[i];
		pipe->scaler.num_ext_pxls_left[i] = pipe->scaler.roi_w[i];

		/* Pixel extension calculations for Y direction */
		roi_h = DECIMATED_DIMENSION(pipe->src.h, pipe->vert_deci);

		/* Subsampling of chroma components is factored */
		if (i == 1 || i == 2)
			roi_h >>= pipe->chroma_sample_v;

		pipe->scaler.preload_y[i] = QSEED3_DEFAULT_PRELAOD_V;
		pipe->scaler.src_height[i] = roi_h;
		pipe->scaler.num_ext_pxls_top[i] = roi_h;

		pr_debug("QSEED3 params=%d, preload_x=%d, preload_y=%d,src_w=%d,src_h=%d\n",
				i, pipe->scaler.preload_x[i],
				pipe->scaler.preload_y[i],
				pipe->scaler.src_width[i],
				pipe->scaler.src_height[i]);
	}

	pipe->scaler.dst_width = pipe->dst.w;
	pipe->scaler.dst_height = pipe->dst.h;
	/* assign filters */
	pipe->scaler.y_rgb_filter_cfg = FILTER_BILINEAR;
	pipe->scaler.uv_filter_cfg = FILTER_BILINEAR;
	pipe->scaler.alpha_filter_cfg = FILTER_ALPHA_BILINEAR;
	pipe->scaler.lut_flag = 0;
	pipe->scaler.enable = ENABLE_SCALE;
}
