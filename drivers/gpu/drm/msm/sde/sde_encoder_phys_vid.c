/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include "sde_encoder_phys.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"
#include "sde_formats.h"
#include "dsi_display.h"
#include "sde_trace.h"

#define SDE_DEBUG_VIDENC(e, fmt, ...) SDE_DEBUG("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) && (e)->base.hw_intf ? \
		(e)->base.hw_intf->idx - INTF_0 : -1, ##__VA_ARGS__)

#define SDE_ERROR_VIDENC(e, fmt, ...) SDE_ERROR("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) && (e)->base.hw_intf ? \
		(e)->base.hw_intf->idx - INTF_0 : -1, ##__VA_ARGS__)

#define to_sde_encoder_phys_vid(x) \
	container_of(x, struct sde_encoder_phys_vid, base)

/* maximum number of consecutive kickoff errors */
#define KICKOFF_MAX_ERRORS	2

/* Poll time to do recovery during active region */
#define POLL_TIME_USEC_FOR_LN_CNT 500
#define MAX_POLL_CNT 10

static bool sde_encoder_phys_vid_is_master(
		struct sde_encoder_phys *phys_enc)
{
	bool ret = false;

	if (phys_enc->split_role != ENC_ROLE_SLAVE)
		ret = true;

	return ret;
}

static void drm_mode_to_intf_timing_params(
		const struct sde_encoder_phys_vid *vid_enc,
		const struct drm_display_mode *mode,
		struct intf_timing_params *timing)
{
	const struct sde_encoder_phys *phys_enc = &vid_enc->base;
	enum msm_display_compression_ratio comp_ratio =
				MSM_DISPLAY_COMPRESSION_RATIO_NONE;

	memset(timing, 0, sizeof(*timing));

	if ((mode->htotal < mode->hsync_end)
			|| (mode->hsync_start < mode->hdisplay)
			|| (mode->vtotal < mode->vsync_end)
			|| (mode->vsync_start < mode->vdisplay)
			|| (mode->hsync_end < mode->hsync_start)
			|| (mode->vsync_end < mode->vsync_start)) {
		SDE_ERROR(
		    "invalid params - hstart:%d,hend:%d,htot:%d,hdisplay:%d\n",
				mode->hsync_start, mode->hsync_end,
				mode->htotal, mode->hdisplay);
		SDE_ERROR("vstart:%d,vend:%d,vtot:%d,vdisplay:%d\n",
				mode->vsync_start, mode->vsync_end,
				mode->vtotal, mode->vdisplay);
		return;
	}

	/*
	 * https://www.kernel.org/doc/htmldocs/drm/ch02s05.html
	 *  Active Region      Front Porch   Sync   Back Porch
	 * <-----------------><------------><-----><----------->
	 * <- [hv]display --->
	 * <--------- [hv]sync_start ------>
	 * <----------------- [hv]sync_end ------->
	 * <---------------------------- [hv]total ------------->
	 */
	timing->width = mode->hdisplay;	/* active width */

	if (phys_enc->hw_intf->cap->type != INTF_DP &&
		vid_enc->base.comp_type == MSM_DISPLAY_COMPRESSION_DSC) {
		comp_ratio = vid_enc->base.comp_ratio;
		if (comp_ratio == MSM_DISPLAY_COMPRESSION_RATIO_2_TO_1)
			timing->width = DIV_ROUND_UP(timing->width, 2);
		else
			timing->width = DIV_ROUND_UP(timing->width, 3);
	}

	timing->height = mode->vdisplay;	/* active height */
	timing->xres = timing->width;
	timing->yres = timing->height;
	timing->h_back_porch = mode->htotal - mode->hsync_end;
	timing->h_front_porch = mode->hsync_start - mode->hdisplay;
	timing->v_back_porch = mode->vtotal - mode->vsync_end;
	timing->v_front_porch = mode->vsync_start - mode->vdisplay;
	timing->hsync_pulse_width = mode->hsync_end - mode->hsync_start;
	timing->vsync_pulse_width = mode->vsync_end - mode->vsync_start;
	timing->hsync_polarity = (mode->flags & DRM_MODE_FLAG_NHSYNC) ? 1 : 0;
	timing->vsync_polarity = (mode->flags & DRM_MODE_FLAG_NVSYNC) ? 1 : 0;
	timing->border_clr = 0;
	timing->underflow_clr = 0xff;
	timing->hsync_skew = mode->hskew;
	timing->v_front_porch_fixed = vid_enc->base.vfp_cached;
	timing->compression_en = false;

	/* DSI controller cannot handle active-low sync signals. */
	if (phys_enc->hw_intf->cap->type == INTF_DSI) {
		timing->hsync_polarity = 0;
		timing->vsync_polarity = 0;
	}

	/* for DP/EDP, Shift timings to align it to bottom right */
	if ((phys_enc->hw_intf->cap->type == INTF_DP) ||
		(phys_enc->hw_intf->cap->type == INTF_EDP)) {
		timing->h_back_porch += timing->h_front_porch;
		timing->h_front_porch = 0;
		timing->v_back_porch += timing->v_front_porch;
		timing->v_front_porch = 0;
	}

	timing->wide_bus_en = vid_enc->base.wide_bus_en;

	/*
	 * for DP, divide the horizonal parameters by 2 when
	 * widebus or compression is enabled, irrespective of
	 * compression ratio
	 */
	if (phys_enc->hw_intf->cap->type == INTF_DP &&
		(timing->wide_bus_en || vid_enc->base.comp_ratio)) {
		timing->width = timing->width >> 1;
		timing->xres = timing->xres >> 1;
		timing->h_back_porch = timing->h_back_porch >> 1;
		timing->h_front_porch = timing->h_front_porch >> 1;
		timing->hsync_pulse_width = timing->hsync_pulse_width >> 1;

		if (vid_enc->base.comp_type == MSM_DISPLAY_COMPRESSION_DSC &&
				vid_enc->base.comp_ratio) {
			timing->compression_en = true;
			timing->extra_dto_cycles =
				vid_enc->base.dsc_extra_pclk_cycle_cnt;
			timing->width += vid_enc->base.dsc_extra_disp_width;
			timing->h_back_porch +=
				vid_enc->base.dsc_extra_disp_width;
		}
	}

	/*
	 * For edp only:
	 * DISPLAY_V_START = (VBP * HCYCLE) + HBP
	 * DISPLAY_V_END = (VBP + VACTIVE) * HCYCLE - 1 - HFP
	 */
	/*
	 * if (vid_enc->hw->cap->type == INTF_EDP) {
	 * display_v_start += mode->htotal - mode->hsync_start;
	 * display_v_end -= mode->hsync_start - mode->hdisplay;
	 * }
	 */
}

static inline u32 get_horizontal_total(const struct intf_timing_params *timing)
{
	u32 active = timing->xres;
	u32 inactive =
	    timing->h_back_porch + timing->h_front_porch +
	    timing->hsync_pulse_width;
	return active + inactive;
}

static inline u32 get_vertical_total(const struct intf_timing_params *timing,
	bool use_fixed_vfp)
{
	u32 inactive;
	u32 active = timing->yres;
	u32 v_front_porch = use_fixed_vfp ?
		timing->v_front_porch_fixed : timing->v_front_porch;

	inactive = timing->v_back_porch + v_front_porch +
			    timing->vsync_pulse_width;
	return active + inactive;
}

/*
 * programmable_fetch_get_num_lines:
 *	Number of fetch lines in vertical front porch
 * @timing: Pointer to the intf timing information for the requested mode
 *
 * Returns the number of fetch lines in vertical front porch at which mdp
 * can start fetching the next frame.
 *
 * Number of needed prefetch lines is anything that cannot be absorbed in the
 * start of frame time (back porch + vsync pulse width).
 *
 * Some panels have very large VFP, however we only need a total number of
 * lines based on the chip worst case latencies.
 */
static u32 programmable_fetch_get_num_lines(
		struct sde_encoder_phys_vid *vid_enc,
		const struct intf_timing_params *timing,
		bool use_fixed_vfp)
{
	struct sde_encoder_phys *phys_enc = &vid_enc->base;
	u32 worst_case_needed_lines =
	    phys_enc->hw_intf->cap->prog_fetch_lines_worst_case;
	u32 start_of_frame_lines =
	    timing->v_back_porch + timing->vsync_pulse_width;
	u32 needed_vfp_lines = worst_case_needed_lines - start_of_frame_lines;
	u32 actual_vfp_lines = 0;
	u32 v_front_porch = use_fixed_vfp ?
		timing->v_front_porch_fixed : timing->v_front_porch;

	/* Fetch must be outside active lines, otherwise undefined. */
	if (start_of_frame_lines >= worst_case_needed_lines) {
		SDE_DEBUG_VIDENC(vid_enc,
				"prog fetch is not needed, large vbp+vsw\n");
		actual_vfp_lines = 0;
	} else if (v_front_porch < needed_vfp_lines) {
		/* Warn fetch needed, but not enough porch in panel config */
		pr_warn_once
			("low vbp+vfp may lead to perf issues in some cases\n");
		SDE_DEBUG_VIDENC(vid_enc,
				"less vfp than fetch req, using entire vfp\n");
		actual_vfp_lines = v_front_porch;
	} else {
		SDE_DEBUG_VIDENC(vid_enc, "room in vfp for needed prefetch\n");
		actual_vfp_lines = needed_vfp_lines;
	}

	SDE_DEBUG_VIDENC(vid_enc,
		"v_front_porch %u v_back_porch %u vsync_pulse_width %u\n",
		v_front_porch, timing->v_back_porch,
		timing->vsync_pulse_width);
	SDE_DEBUG_VIDENC(vid_enc,
		"wc_lines %u needed_vfp_lines %u actual_vfp_lines %u\n",
		worst_case_needed_lines, needed_vfp_lines, actual_vfp_lines);

	return actual_vfp_lines;
}

/*
 * programmable_fetch_config: Programs HW to prefetch lines by offsetting
 *	the start of fetch into the vertical front porch for cases where the
 *	vsync pulse width and vertical back porch time is insufficient
 *
 *	Gets # of lines to pre-fetch, then calculate VSYNC counter value.
 *	HW layer requires VSYNC counter of first pixel of tgt VFP line.
 *
 * @timing: Pointer to the intf timing information for the requested mode
 */
static void programmable_fetch_config(struct sde_encoder_phys *phys_enc,
				      const struct intf_timing_params *timing)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	struct intf_prog_fetch f = { 0 };
	u32 vfp_fetch_lines = 0;
	u32 horiz_total = 0;
	u32 vert_total = 0;
	u32 vfp_fetch_start_vsync_counter = 0;
	unsigned long lock_flags;
	struct sde_mdss_cfg *m;

	if (WARN_ON_ONCE(!phys_enc->hw_intf->ops.setup_prg_fetch))
		return;

	m = phys_enc->sde_kms->catalog;

	vfp_fetch_lines = programmable_fetch_get_num_lines(vid_enc,
							   timing, true);
	if (vfp_fetch_lines) {
		vert_total = get_vertical_total(timing, true);
		horiz_total = get_horizontal_total(timing);
		vfp_fetch_start_vsync_counter =
			(vert_total - vfp_fetch_lines) * horiz_total + 1;

		/**
		 * Check if we need to throttle the fetch to start
		 * from second line after the active region.
		 */
		if (m->delay_prg_fetch_start)
			vfp_fetch_start_vsync_counter += horiz_total;

		f.enable = 1;
		f.fetch_start = vfp_fetch_start_vsync_counter;
	}

	SDE_DEBUG_VIDENC(vid_enc,
		"vfp_fetch_lines %u vfp_fetch_start_vsync_counter %u\n",
		vfp_fetch_lines, vfp_fetch_start_vsync_counter);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	phys_enc->hw_intf->ops.setup_prg_fetch(phys_enc->hw_intf, &f);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);
}

/*
 * programmable_rot_fetch_config: Programs ROT to prefetch lines by offsetting
 *	the start of fetch into the vertical front porch for cases where the
 *	vsync pulse width and vertical back porch time is insufficient
 *
 *	Gets # of lines to pre-fetch, then calculate VSYNC counter value.
 *	HW layer requires VSYNC counter of first pixel of tgt VFP line.
 * @phys_enc: Pointer to physical encoder
 * @rot_fetch_lines: number of line to prefill, or 0 to disable
 * @is_primary: set true if the display is primary display
 */
static void programmable_rot_fetch_config(struct sde_encoder_phys *phys_enc,
		u32 rot_fetch_lines, u32 is_primary)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	struct intf_prog_fetch f = { 0 };
	struct intf_timing_params *timing;
	u32 vfp_fetch_lines = 0;
	u32 horiz_total = 0;
	u32 vert_total = 0;
	u32 rot_fetch_start_vsync_counter = 0;
	unsigned long lock_flags;

	if (!phys_enc || !phys_enc->hw_intf || !phys_enc->hw_ctl ||
			!phys_enc->hw_ctl->ops.update_bitmask_intf ||
			!phys_enc->hw_intf->ops.setup_rot_start ||
			!phys_enc->sde_kms ||
			!is_primary)
		return;

	timing = &vid_enc->timing_params;
	vfp_fetch_lines = programmable_fetch_get_num_lines(vid_enc,
							   timing, true);
	if (rot_fetch_lines) {
		vert_total = get_vertical_total(timing, true);
		horiz_total = get_horizontal_total(timing);
		if (vert_total >= (vfp_fetch_lines + rot_fetch_lines)) {
			rot_fetch_start_vsync_counter =
			    (vert_total - vfp_fetch_lines - rot_fetch_lines) *
			    horiz_total + 1;
			f.enable = 1;
			f.fetch_start = rot_fetch_start_vsync_counter;
		} else {
			SDE_ERROR_VIDENC(vid_enc,
				"vert_total %u rot_fetch_lines %u vfp_fetch_lines %u\n",
				vert_total, rot_fetch_lines, vfp_fetch_lines);
			SDE_EVT32(DRMID(phys_enc->parent), vert_total,
				rot_fetch_lines, vfp_fetch_lines,
				SDE_EVTLOG_ERROR);
		}
	}

	/* return if rot_fetch does not change since last update */
	if (vid_enc->rot_fetch_valid &&
			!memcmp(&vid_enc->rot_fetch, &f, sizeof(f)))
		return;

	SDE_DEBUG_VIDENC(vid_enc,
		"rot_fetch_lines %u vfp_fetch_lines %u rot_fetch_start_vsync_counter %u\n",
		rot_fetch_lines, vfp_fetch_lines,
		rot_fetch_start_vsync_counter);

	if (!phys_enc->cont_splash_enabled) {
		SDE_EVT32(DRMID(phys_enc->parent), f.enable, f.fetch_start);

		phys_enc->hw_ctl->ops.update_bitmask_intf(
				phys_enc->hw_ctl, phys_enc->hw_intf->idx, 1);

		spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
		phys_enc->hw_intf->ops.setup_rot_start(phys_enc->hw_intf, &f);
		spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

		vid_enc->rot_fetch = f;
		vid_enc->rot_fetch_valid = true;
	}
}

static bool sde_encoder_phys_vid_mode_fixup(
		struct sde_encoder_phys *phys_enc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	if (phys_enc)
		SDE_DEBUG_VIDENC(to_sde_encoder_phys_vid(phys_enc), "\n");

	/*
	 * Modifying mode has consequences when the mode comes back to us
	 */
	return true;
}

/* vid_enc timing_params must be configured before calling this function */
static void _sde_encoder_phys_vid_setup_avr(
		struct sde_encoder_phys *phys_enc, u32 qsync_min_fps)
{
	struct sde_encoder_phys_vid *vid_enc;
	struct drm_display_mode mode;

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	mode = phys_enc->cached_mode;
	if (vid_enc->base.hw_intf->ops.avr_setup) {
		struct intf_avr_params avr_params = {0};
		u32 default_fps = mode.vrefresh;
		int ret;

		if (!default_fps) {
			SDE_ERROR_VIDENC(vid_enc,
					"invalid default fps %d\n",
					default_fps);
			return;
		}

		if (qsync_min_fps >= default_fps) {
			SDE_ERROR_VIDENC(vid_enc,
				"qsync fps %d must be less than default %d\n",
				qsync_min_fps, default_fps);
			return;
		}

		avr_params.default_fps = default_fps;
		avr_params.min_fps = qsync_min_fps;

		ret = vid_enc->base.hw_intf->ops.avr_setup(
				vid_enc->base.hw_intf,
				&vid_enc->timing_params, &avr_params);
		if (ret)
			SDE_ERROR_VIDENC(vid_enc,
				"bad settings, can't configure AVR\n");

		SDE_EVT32(DRMID(phys_enc->parent), default_fps,
				qsync_min_fps, ret);
	}
}

static void _sde_encoder_phys_vid_avr_ctrl(struct sde_encoder_phys *phys_enc)
{
	struct intf_avr_params avr_params;
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	avr_params.avr_mode = sde_connector_get_qsync_mode(
			phys_enc->connector);

	if (vid_enc->base.hw_intf->ops.avr_ctrl) {
		vid_enc->base.hw_intf->ops.avr_ctrl(
				vid_enc->base.hw_intf,
				&avr_params);
	}

	SDE_EVT32(DRMID(phys_enc->parent),
		phys_enc->hw_intf->idx - INTF_0,
		avr_params.avr_mode);
}

static void sde_encoder_phys_vid_setup_timing_engine(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc;
	struct drm_display_mode mode;
	struct intf_timing_params timing_params = { 0 };
	const struct sde_format *fmt = NULL;
	u32 fmt_fourcc = DRM_FORMAT_RGB888;
	u32 qsync_min_fps = 0;
	unsigned long lock_flags;
	struct sde_hw_intf_cfg intf_cfg = { 0 };
	bool is_split_link = false;

	if (!phys_enc || !phys_enc->sde_kms || !phys_enc->hw_ctl ||
					!phys_enc->hw_intf) {
		SDE_ERROR("invalid encoder %d\n", phys_enc != 0);
		return;
	}

	mode = phys_enc->cached_mode;
	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	if (!phys_enc->hw_intf->ops.setup_timing_gen) {
		SDE_ERROR("timing engine setup is not supported\n");
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "enabling mode:\n");
	drm_mode_debug_printmodeline(&mode);

	is_split_link = phys_enc->hw_intf->cfg.split_link_en;
	if (phys_enc->split_role != ENC_ROLE_SOLO || is_split_link) {
		mode.hdisplay >>= 1;
		mode.htotal >>= 1;
		mode.hsync_start >>= 1;
		mode.hsync_end >>= 1;

		SDE_DEBUG_VIDENC(vid_enc,
			"split_role %d, halve horizontal %d %d %d %d\n",
			phys_enc->split_role,
			mode.hdisplay, mode.htotal,
			mode.hsync_start, mode.hsync_end);
	}

	if (!phys_enc->vfp_cached) {
		phys_enc->vfp_cached =
			sde_connector_get_panel_vfp(phys_enc->connector, &mode);
		if (phys_enc->vfp_cached <= 0)
			phys_enc->vfp_cached = mode.vsync_start - mode.vdisplay;
	}

	drm_mode_to_intf_timing_params(vid_enc, &mode, &timing_params);

	vid_enc->timing_params = timing_params;

	if (phys_enc->cont_splash_enabled) {
		SDE_DEBUG_VIDENC(vid_enc,
			"skipping intf programming since cont splash is enabled\n");
		goto exit;
	}

	fmt = sde_get_sde_format(fmt_fourcc);
	SDE_DEBUG_VIDENC(vid_enc, "fmt_fourcc 0x%X\n", fmt_fourcc);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	phys_enc->hw_intf->ops.setup_timing_gen(phys_enc->hw_intf,
			&timing_params, fmt);

	if (test_bit(SDE_CTL_ACTIVE_CFG,
				&phys_enc->hw_ctl->caps->features)) {
		sde_encoder_helper_update_intf_cfg(phys_enc);
	} else if (phys_enc->hw_ctl->ops.setup_intf_cfg) {
		intf_cfg.intf = phys_enc->hw_intf->idx;
		intf_cfg.intf_mode_sel = SDE_CTL_MODE_SEL_VID;
		intf_cfg.stream_sel = 0; /* Don't care value for video mode */
		intf_cfg.mode_3d =
			sde_encoder_helper_get_3d_blend_mode(phys_enc);

		phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl,
				&intf_cfg);
	}
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);
	if (phys_enc->hw_intf->cap->type == INTF_DSI)
		programmable_fetch_config(phys_enc, &timing_params);

exit:
	if (phys_enc->parent_ops.get_qsync_fps)
		phys_enc->parent_ops.get_qsync_fps(
				phys_enc->parent, &qsync_min_fps);

	/* only panels which support qsync will have a non-zero min fps */
	if (qsync_min_fps) {
		_sde_encoder_phys_vid_setup_avr(phys_enc, qsync_min_fps);
		_sde_encoder_phys_vid_avr_ctrl(phys_enc);
	}
}

static void sde_encoder_phys_vid_vblank_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	struct sde_hw_ctl *hw_ctl;
	unsigned long lock_flags;
	u32 flush_register = ~0;
	u32 reset_status = 0;
	int new_cnt = -1, old_cnt = -1;
	u32 event = 0;
	int pend_ret_fence_cnt = 0;

	if (!phys_enc)
		return;

	hw_ctl = phys_enc->hw_ctl;
	if (!hw_ctl)
		return;

	SDE_ATRACE_BEGIN("vblank_irq");

	/*
	 * only decrement the pending flush count if we've actually flushed
	 * hardware. due to sw irq latency, vblank may have already happened
	 * so we need to double-check with hw that it accepted the flush bits
	 */
	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);

	old_cnt = atomic_read(&phys_enc->pending_kickoff_cnt);

	if (hw_ctl && hw_ctl->ops.get_flush_register)
		flush_register = hw_ctl->ops.get_flush_register(hw_ctl);

	if (flush_register)
		goto not_flushed;

	new_cnt = atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);
	pend_ret_fence_cnt = atomic_read(&phys_enc->pending_retire_fence_cnt);

	/* signal only for master, where there is a pending kickoff */
	if (sde_encoder_phys_vid_is_master(phys_enc)) {
		if (atomic_add_unless(&phys_enc->pending_retire_fence_cnt,
					-1, 0))
			event |= SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE |
				SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE;
	}

not_flushed:
	if (hw_ctl && hw_ctl->ops.get_reset)
		reset_status = hw_ctl->ops.get_reset(hw_ctl);

	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	if (event && phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
			phys_enc, event);

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
				phys_enc);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0,
			old_cnt, new_cnt, reset_status ? SDE_EVTLOG_ERROR : 0,
			flush_register, event,
			pend_ret_fence_cnt);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&phys_enc->pending_kickoff_wq);
	SDE_ATRACE_END("vblank_irq");
}

static void sde_encoder_phys_vid_underrun_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;

	if (!phys_enc)
		return;

	if (phys_enc->parent_ops.handle_underrun_virt)
		phys_enc->parent_ops.handle_underrun_virt(phys_enc->parent,
			phys_enc);
}

static void _sde_encoder_phys_vid_setup_irq_hw_idx(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_irq *irq;

	/*
	 * Initialize irq->hw_idx only when irq is not registered.
	 * Prevent invalidating irq->irq_idx as modeset may be
	 * called many times during dfps.
	 */

	irq = &phys_enc->irq[INTR_IDX_VSYNC];
	if (irq->irq_idx < 0)
		irq->hw_idx = phys_enc->intf_idx;

	irq = &phys_enc->irq[INTR_IDX_UNDERRUN];
	if (irq->irq_idx < 0)
		irq->hw_idx = phys_enc->intf_idx;
}

static void sde_encoder_phys_vid_cont_splash_mode_set(
		struct sde_encoder_phys *phys_enc,
		struct drm_display_mode *adj_mode)
{
	if (!phys_enc || !adj_mode) {
		SDE_ERROR("invalid args\n");
		return;
	}

	phys_enc->cached_mode = *adj_mode;
	phys_enc->enable_state = SDE_ENC_ENABLED;

	_sde_encoder_phys_vid_setup_irq_hw_idx(phys_enc);
}

static void sde_encoder_phys_vid_mode_set(
		struct sde_encoder_phys *phys_enc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	struct sde_rm *rm;
	struct sde_rm_hw_iter iter;
	int i, instance;
	struct sde_encoder_phys_vid *vid_enc;

	if (!phys_enc || !phys_enc->sde_kms) {
		SDE_ERROR("invalid encoder/kms\n");
		return;
	}

	rm = &phys_enc->sde_kms->rm;
	vid_enc = to_sde_encoder_phys_vid(phys_enc);

	if (adj_mode) {
		phys_enc->cached_mode = *adj_mode;
		drm_mode_debug_printmodeline(adj_mode);
		SDE_DEBUG_VIDENC(vid_enc, "caching mode:\n");
	}

	instance = phys_enc->split_role == ENC_ROLE_SLAVE ? 1 : 0;

	/* Retrieve previously allocated HW Resources. Shouldn't fail */
	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_CTL);
	for (i = 0; i <= instance; i++) {
		if (sde_rm_get_hw(rm, &iter))
			phys_enc->hw_ctl = (struct sde_hw_ctl *)iter.hw;
	}
	if (IS_ERR_OR_NULL(phys_enc->hw_ctl)) {
		SDE_ERROR_VIDENC(vid_enc, "failed to init ctl, %ld\n",
				PTR_ERR(phys_enc->hw_ctl));
		phys_enc->hw_ctl = NULL;
		return;
	}

	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_INTF);
	for (i = 0; i <= instance; i++) {
		if (sde_rm_get_hw(rm, &iter))
			phys_enc->hw_intf = (struct sde_hw_intf *)iter.hw;
	}

	if (IS_ERR_OR_NULL(phys_enc->hw_intf)) {
		SDE_ERROR_VIDENC(vid_enc, "failed to init intf: %ld\n",
				PTR_ERR(phys_enc->hw_intf));
		phys_enc->hw_intf = NULL;
		return;
	}

	_sde_encoder_phys_vid_setup_irq_hw_idx(phys_enc);
}

static int sde_encoder_phys_vid_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	int ret = 0;
	struct sde_encoder_phys_vid *vid_enc;
	int refcount;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	mutex_lock(phys_enc->vblank_ctl_lock);
	refcount = atomic_read(&phys_enc->vblank_refcount);
	vid_enc = to_sde_encoder_phys_vid(phys_enc);

	/* Slave encoders don't report vblank */
	if (!sde_encoder_phys_vid_is_master(phys_enc))
		goto end;

	/* protect against negative */
	if (!enable && refcount == 0) {
		ret = -EINVAL;
		goto end;
	}

	SDE_DEBUG_VIDENC(vid_enc, "[%pS] enable=%d/%d\n",
			__builtin_return_address(0),
			enable, atomic_read(&phys_enc->vblank_refcount));

	SDE_EVT32(DRMID(phys_enc->parent), enable,
			atomic_read(&phys_enc->vblank_refcount));

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1) {
		ret = sde_encoder_helper_register_irq(phys_enc, INTR_IDX_VSYNC);
		if (ret)
			atomic_dec_return(&phys_enc->vblank_refcount);
	} else if (!enable &&
			atomic_dec_return(&phys_enc->vblank_refcount) == 0) {
		ret = sde_encoder_helper_unregister_irq(phys_enc,
				INTR_IDX_VSYNC);
		if (ret)
			atomic_inc_return(&phys_enc->vblank_refcount);
	}

end:
	if (ret) {
		SDE_ERROR_VIDENC(vid_enc,
				"control vblank irq error %d, enable %d\n",
				ret, enable);
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_intf->idx - INTF_0,
				enable, refcount, SDE_EVTLOG_ERROR);
	}
	mutex_unlock(phys_enc->vblank_ctl_lock);
	return ret;
}

static bool sde_encoder_phys_vid_wait_dma_trigger(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc;
	struct sde_hw_intf *intf;
	struct sde_hw_ctl *ctl;
	struct intf_status status;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return false;
	}

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	intf = phys_enc->hw_intf;
	ctl = phys_enc->hw_ctl;
	if (!phys_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
			phys_enc->hw_intf != NULL, phys_enc->hw_ctl != NULL);
		return false;
	}

	if (!intf->ops.get_status)
		return false;

	intf->ops.get_status(intf, &status);

	/* if interface is not enabled, return true to wait for dma trigger */
	return status.is_en ? false : true;
}

static void sde_encoder_phys_vid_enable(struct sde_encoder_phys *phys_enc)
{
	struct msm_drm_private *priv;
	struct sde_encoder_phys_vid *vid_enc;
	struct sde_hw_intf *intf;
	struct sde_hw_ctl *ctl;

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->dev ||
			!phys_enc->parent->dev->dev_private ||
			!phys_enc->sde_kms) {
		SDE_ERROR("invalid encoder/device\n");
		return;
	}
	priv = phys_enc->parent->dev->dev_private;

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	intf = phys_enc->hw_intf;
	ctl = phys_enc->hw_ctl;
	if (!phys_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
				phys_enc->hw_intf != 0, phys_enc->hw_ctl != 0);
		return;
	}
	if (!ctl->ops.update_bitmask_intf ||
		(test_bit(SDE_CTL_ACTIVE_CFG, &ctl->caps->features) &&
		!ctl->ops.update_bitmask_merge3d)) {
		SDE_ERROR("invalid hw_ctl ops %d\n", ctl->idx);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	if (WARN_ON(!phys_enc->hw_intf->ops.enable_timing))
		return;

	/* reset state variables until after first update */
	vid_enc->rot_fetch_valid = false;

	if (!phys_enc->cont_splash_enabled)
		sde_encoder_helper_split_config(phys_enc,
				phys_enc->hw_intf->idx);

	sde_encoder_phys_vid_setup_timing_engine(phys_enc);

	/*
	 * For cases where both the interfaces are connected to same ctl,
	 * set the flush bit for both master and slave.
	 * For single flush cases (dual-ctl or pp-split), skip setting the
	 * flush bit for the slave intf, since both intfs use same ctl
	 * and HW will only flush the master.
	 */
	if (!test_bit(SDE_CTL_ACTIVE_CFG, &ctl->caps->features) &&
			sde_encoder_phys_needs_single_flush(phys_enc) &&
		!sde_encoder_phys_vid_is_master(phys_enc))
		goto skip_flush;

	/**
	 * skip flushing intf during cont. splash handoff since bootloader
	 * has already enabled the hardware and is single buffered.
	 */
	if (phys_enc->cont_splash_enabled) {
		SDE_DEBUG_VIDENC(vid_enc,
		"skipping intf flush bit set as cont. splash is enabled\n");
		goto skip_flush;
	}

	ctl->ops.update_bitmask_intf(ctl, intf->idx, 1);

	if (ctl->ops.update_bitmask_merge3d && phys_enc->hw_pp->merge_3d)
		ctl->ops.update_bitmask_merge3d(ctl,
			phys_enc->hw_pp->merge_3d->idx, 1);

	if (phys_enc->hw_intf->cap->type == INTF_DP &&
		phys_enc->comp_type == MSM_DISPLAY_COMPRESSION_DSC &&
		phys_enc->comp_ratio && ctl->ops.update_bitmask_periph)
		ctl->ops.update_bitmask_periph(ctl, intf->idx, 1);

skip_flush:
	SDE_DEBUG_VIDENC(vid_enc, "update pending flush ctl %d intf %d\n",
		ctl->idx - CTL_0, intf->idx);
	SDE_EVT32(DRMID(phys_enc->parent),
		atomic_read(&phys_enc->pending_retire_fence_cnt));

	/* ctl_flush & timing engine enable will be triggered by framework */
	if (phys_enc->enable_state == SDE_ENC_DISABLED)
		phys_enc->enable_state = SDE_ENC_ENABLING;
}

static void sde_encoder_phys_vid_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	SDE_DEBUG_VIDENC(vid_enc, "\n");
	kfree(vid_enc);
}

static void sde_encoder_phys_vid_get_hw_resources(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_phys_vid *vid_enc;

	if (!phys_enc || !hw_res) {
		SDE_ERROR("invalid arg(s), enc %d hw_res %d conn_state %d\n",
				phys_enc != 0, hw_res != 0, conn_state != 0);
		return;
	}

	if ((phys_enc->intf_idx - INTF_0) >= INTF_MAX) {
		SDE_ERROR("invalid intf idx:%d\n", phys_enc->intf_idx);
		return;
	}

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	SDE_DEBUG_VIDENC(vid_enc, "\n");
	hw_res->intfs[phys_enc->intf_idx - INTF_0] = INTF_MODE_VIDEO;
}

static int _sde_encoder_phys_vid_wait_for_vblank(
		struct sde_encoder_phys *phys_enc, bool notify)
{
	struct sde_encoder_wait_info wait_info;
	int ret = 0;
	u32 event = 0;
	u32 event_helper = 0;

	if (!phys_enc) {
		pr_err("invalid encoder\n");
		return -EINVAL;
	}

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_kickoff_cnt;
	wait_info.timeout_ms = KICKOFF_TIMEOUT_MS;

	if (!sde_encoder_phys_vid_is_master(phys_enc)) {
		/* signal done for slave video encoder, unless it is pp-split */
		if (!_sde_encoder_phys_is_ppsplit(phys_enc) && notify) {
			event = SDE_ENCODER_FRAME_EVENT_DONE;
			goto end;
		}
		return 0;
	}

	/* Wait for kickoff to complete */
	ret = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_VSYNC,
			&wait_info);

	event_helper = SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE
			| SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE;

	if (notify) {
		if (ret == -ETIMEDOUT) {
			event = SDE_ENCODER_FRAME_EVENT_ERROR;
			if (atomic_add_unless(
				&phys_enc->pending_retire_fence_cnt, -1, 0))
				event |= event_helper;
		} else if (!ret) {
			event = SDE_ENCODER_FRAME_EVENT_DONE;
		}
	}

end:
	SDE_EVT32(DRMID(phys_enc->parent), event, notify, ret,
			ret ? SDE_EVTLOG_FATAL : 0);
	if (phys_enc->parent_ops.handle_frame_done && event)
		phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc,
				event);
	return ret;
}

static int sde_encoder_phys_vid_wait_for_vblank(
		struct sde_encoder_phys *phys_enc)
{
	return _sde_encoder_phys_vid_wait_for_vblank(phys_enc, true);
}

static int sde_encoder_phys_vid_wait_for_vblank_no_notify(
		struct sde_encoder_phys *phys_enc)
{
	return _sde_encoder_phys_vid_wait_for_vblank(phys_enc, false);
}

static int sde_encoder_phys_vid_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct sde_encoder_phys_vid *vid_enc;
	struct sde_hw_ctl *ctl;
	bool recovery_events;
	struct drm_connector *conn;
	int event;
	int rc;

	if (!phys_enc || !params || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid encoder/parameters\n");
		return -EINVAL;
	}
	vid_enc = to_sde_encoder_phys_vid(phys_enc);

	ctl = phys_enc->hw_ctl;
	if (!ctl->ops.wait_reset_status)
		return 0;

	conn = phys_enc->connector;
	recovery_events = sde_encoder_recovery_events_enabled(
			phys_enc->parent);
	/*
	 * hw supports hardware initiated ctl reset, so before we kickoff a new
	 * frame, need to check and wait for hw initiated ctl reset completion
	 */
	rc = ctl->ops.wait_reset_status(ctl);
	if (rc) {
		SDE_ERROR_VIDENC(vid_enc, "ctl %d reset failure: %d\n",
				ctl->idx, rc);

		++vid_enc->error_count;

		/* to avoid flooding, only log first time, and "dead" time */
		if (vid_enc->error_count == 1) {
			SDE_EVT32(DRMID(phys_enc->parent), SDE_EVTLOG_FATAL);

			sde_encoder_helper_unregister_irq(
					phys_enc, INTR_IDX_VSYNC);
			SDE_DBG_DUMP("all", "dbg_bus", "vbif_dbg_bus");
			sde_encoder_helper_register_irq(
					phys_enc, INTR_IDX_VSYNC);
		}

		/*
		 * if the recovery event is registered by user, don't panic
		 * trigger panic on first timeout if no listener registered
		 */
		if (recovery_events) {
			event = vid_enc->error_count > KICKOFF_MAX_ERRORS ?
				SDE_RECOVERY_HARD_RESET : SDE_RECOVERY_CAPTURE;
			sde_connector_event_notify(conn,
					DRM_EVENT_SDE_HW_RECOVERY,
					sizeof(uint8_t), event);
		} else {
			SDE_DBG_DUMP("panic");
		}

		/* request a ctl reset before the next flush */
		phys_enc->enable_state = SDE_ENC_ERR_NEEDS_HW_RESET;
	} else {
		if (recovery_events && vid_enc->error_count)
			sde_connector_event_notify(conn,
					DRM_EVENT_SDE_HW_RECOVERY,
					sizeof(uint8_t),
					SDE_RECOVERY_SUCCESS);
		vid_enc->error_count = 0;
	}

	if (sde_connector_is_qsync_updated(phys_enc->connector))
		_sde_encoder_phys_vid_avr_ctrl(phys_enc);

	programmable_rot_fetch_config(phys_enc,
			params->inline_rotate_prefill, params->is_primary);

	return rc;
}

static void sde_encoder_phys_vid_single_vblank_wait(
		struct sde_encoder_phys *phys_enc)
{
	int ret;
	struct sde_encoder_phys_vid *vid_enc
					= to_sde_encoder_phys_vid(phys_enc);

	/*
	 * Wait for a vsync so we know the ENABLE=0 latched before
	 * the (connector) source of the vsync's gets disabled,
	 * otherwise we end up in a funny state if we re-enable
	 * before the disable latches, which results that some of
	 * the settings changes for the new modeset (like new
	 * scanout buffer) don't latch properly..
	 */
	ret = sde_encoder_phys_vid_control_vblank_irq(phys_enc, true);
	if (ret) {
		SDE_ERROR_VIDENC(vid_enc,
				"failed to enable vblank irq: %d\n",
				ret);
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_intf->idx - INTF_0, ret,
				SDE_EVTLOG_FUNC_CASE1,
				SDE_EVTLOG_ERROR);
	} else {
		ret = _sde_encoder_phys_vid_wait_for_vblank(phys_enc, false);
		if (ret) {
			atomic_set(&phys_enc->pending_kickoff_cnt, 0);
			SDE_ERROR_VIDENC(vid_enc,
					"failure waiting for disable: %d\n",
					ret);
			SDE_EVT32(DRMID(phys_enc->parent),
					phys_enc->hw_intf->idx - INTF_0, ret,
					SDE_EVTLOG_FUNC_CASE2,
					SDE_EVTLOG_ERROR);
		}
		sde_encoder_phys_vid_control_vblank_irq(phys_enc, false);
	}
}

static void sde_encoder_phys_vid_disable(struct sde_encoder_phys *phys_enc)
{
	struct msm_drm_private *priv;
	struct sde_encoder_phys_vid *vid_enc;
	unsigned long lock_flags;
	struct intf_status intf_status = {0};

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->dev ||
			!phys_enc->parent->dev->dev_private) {
		SDE_ERROR("invalid encoder/device\n");
		return;
	}
	priv = phys_enc->parent->dev->dev_private;

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	if (!phys_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
				phys_enc->hw_intf != 0, phys_enc->hw_ctl != 0);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	if (WARN_ON(!phys_enc->hw_intf->ops.enable_timing))
		return;
	else if (!sde_encoder_phys_vid_is_master(phys_enc))
		goto exit;

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("already disabled\n");
		return;
	}

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	phys_enc->hw_intf->ops.enable_timing(phys_enc->hw_intf, 0);
	sde_encoder_phys_inc_pending(phys_enc);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	sde_encoder_phys_vid_single_vblank_wait(phys_enc);
	if (phys_enc->hw_intf->ops.get_status)
		phys_enc->hw_intf->ops.get_status(phys_enc->hw_intf,
			&intf_status);

	if (intf_status.is_en) {
		spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
		sde_encoder_phys_inc_pending(phys_enc);
		spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

		sde_encoder_phys_vid_single_vblank_wait(phys_enc);
	}

	sde_encoder_helper_phys_disable(phys_enc, NULL);
exit:
	SDE_EVT32(DRMID(phys_enc->parent),
		atomic_read(&phys_enc->pending_retire_fence_cnt));
	phys_enc->vfp_cached = 0;
	phys_enc->enable_state = SDE_ENC_DISABLED;
}

static void sde_encoder_phys_vid_handle_post_kickoff(
		struct sde_encoder_phys *phys_enc)
{
	unsigned long lock_flags;
	struct sde_encoder_phys_vid *vid_enc;
	u32 avr_mode;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	SDE_DEBUG_VIDENC(vid_enc, "enable_state %d\n", phys_enc->enable_state);

	/*
	 * Video mode must flush CTL before enabling timing engine
	 * Video encoders need to turn on their interfaces now
	 */
	if (phys_enc->enable_state == SDE_ENC_ENABLING) {
		if (sde_encoder_phys_vid_is_master(phys_enc)) {
			SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_intf->idx - INTF_0);
			spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
			phys_enc->hw_intf->ops.enable_timing(phys_enc->hw_intf,
				1);
			spin_unlock_irqrestore(phys_enc->enc_spinlock,
				lock_flags);
		}
		phys_enc->enable_state = SDE_ENC_ENABLED;
	}

	avr_mode = sde_connector_get_qsync_mode(phys_enc->connector);

	if (avr_mode && vid_enc->base.hw_intf->ops.avr_trigger) {
		vid_enc->base.hw_intf->ops.avr_trigger(vid_enc->base.hw_intf);
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_intf->idx - INTF_0,
				SDE_EVTLOG_FUNC_CASE9);
	}
}

static void sde_encoder_phys_vid_irq_control(struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct sde_encoder_phys_vid *vid_enc;
	int ret;

	if (!phys_enc)
		return;

	vid_enc = to_sde_encoder_phys_vid(phys_enc);

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0,
			enable, atomic_read(&phys_enc->vblank_refcount));

	if (enable) {
		ret = sde_encoder_phys_vid_control_vblank_irq(phys_enc, true);
		if (ret)
			return;

		sde_encoder_helper_register_irq(phys_enc, INTR_IDX_UNDERRUN);
	} else {
		sde_encoder_phys_vid_control_vblank_irq(phys_enc, false);
		sde_encoder_helper_unregister_irq(phys_enc, INTR_IDX_UNDERRUN);
	}
}

static void sde_encoder_phys_vid_setup_misr(struct sde_encoder_phys *phys_enc,
						bool enable, u32 frame_count)
{
	if (!phys_enc)
		return;

	if (phys_enc->hw_intf && phys_enc->hw_intf->ops.setup_misr)
		phys_enc->hw_intf->ops.setup_misr(phys_enc->hw_intf,
							enable, frame_count);
}

static u32 sde_encoder_phys_vid_collect_misr(struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc)
		return 0;

	return phys_enc->hw_intf && phys_enc->hw_intf->ops.collect_misr ?
		phys_enc->hw_intf->ops.collect_misr(phys_enc->hw_intf) : 0;
}

static int sde_encoder_phys_vid_get_line_count(
		struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc)
		return -EINVAL;

	if (!sde_encoder_phys_vid_is_master(phys_enc))
		return -EINVAL;

	if (!phys_enc->hw_intf || !phys_enc->hw_intf->ops.get_line_count)
		return -EINVAL;

	return phys_enc->hw_intf->ops.get_line_count(phys_enc->hw_intf);
}

static int sde_encoder_phys_vid_wait_for_active(
			struct sde_encoder_phys *phys_enc)
{
	struct drm_display_mode mode;
	struct sde_encoder_phys_vid *vid_enc;
	u32 ln_cnt, min_ln_cnt, active_lns_cnt;
	u32 clk_period, time_of_line;
	u32 delay, retry = MAX_POLL_CNT;

	vid_enc =  to_sde_encoder_phys_vid(phys_enc);

	if (!phys_enc->hw_intf || !phys_enc->hw_intf->ops.get_line_count) {
		SDE_ERROR_VIDENC(vid_enc, "invalid vid_enc params\n");
		return -EINVAL;
	}

	mode = phys_enc->cached_mode;

	/*
	 * calculate clk_period as pico second to maintain good
	 * accuracy with high pclk rate and this number is in 17 bit
	 * range.
	 */
	clk_period = DIV_ROUND_UP_ULL(1000000000, mode.clock);
	if (!clk_period) {
		SDE_ERROR_VIDENC(vid_enc, "Unable to calculate clock period\n");
		return -EINVAL;
	}

	min_ln_cnt = (mode.vtotal - mode.vsync_start) +
		(mode.vsync_end - mode.vsync_start);
	active_lns_cnt = mode.vdisplay;
	time_of_line = mode.htotal * clk_period;

	/* delay in micro seconds */
	delay = (time_of_line * (min_ln_cnt +
		(mode.vsync_start - mode.vdisplay))) / 1000000;

	/*
	 * Wait for max delay before
	 * polling to check active region
	 */
	if (delay > POLL_TIME_USEC_FOR_LN_CNT)
		delay = POLL_TIME_USEC_FOR_LN_CNT;

	while (retry) {
		ln_cnt = phys_enc->hw_intf->ops.get_line_count(
				phys_enc->hw_intf);

		if ((ln_cnt >= min_ln_cnt) &&
			(ln_cnt < (active_lns_cnt + min_ln_cnt))) {
			SDE_DEBUG_VIDENC(vid_enc,
					"Needed lines left line_cnt=%d\n",
					ln_cnt);
			return 0;
		}

		SDE_ERROR_VIDENC(vid_enc, "line count is less. line_cnt = %d\n",
				ln_cnt);
		/* Add delay so that line count is in active region */
		udelay(delay);
		retry--;
	}

	return -EINVAL;
}

static void sde_encoder_phys_vid_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->is_master = sde_encoder_phys_vid_is_master;
	ops->mode_set = sde_encoder_phys_vid_mode_set;
	ops->cont_splash_mode_set = sde_encoder_phys_vid_cont_splash_mode_set;
	ops->mode_fixup = sde_encoder_phys_vid_mode_fixup;
	ops->enable = sde_encoder_phys_vid_enable;
	ops->disable = sde_encoder_phys_vid_disable;
	ops->destroy = sde_encoder_phys_vid_destroy;
	ops->get_hw_resources = sde_encoder_phys_vid_get_hw_resources;
	ops->control_vblank_irq = sde_encoder_phys_vid_control_vblank_irq;
	ops->wait_for_commit_done = sde_encoder_phys_vid_wait_for_vblank;
	ops->wait_for_vblank = sde_encoder_phys_vid_wait_for_vblank_no_notify;
	ops->wait_for_tx_complete = sde_encoder_phys_vid_wait_for_vblank;
	ops->irq_control = sde_encoder_phys_vid_irq_control;
	ops->prepare_for_kickoff = sde_encoder_phys_vid_prepare_for_kickoff;
	ops->handle_post_kickoff = sde_encoder_phys_vid_handle_post_kickoff;
	ops->needs_single_flush = sde_encoder_phys_needs_single_flush;
	ops->setup_misr = sde_encoder_phys_vid_setup_misr;
	ops->collect_misr = sde_encoder_phys_vid_collect_misr;
	ops->trigger_flush = sde_encoder_helper_trigger_flush;
	ops->hw_reset = sde_encoder_helper_hw_reset;
	ops->get_line_count = sde_encoder_phys_vid_get_line_count;
	ops->get_wr_line_count = sde_encoder_phys_vid_get_line_count;
	ops->wait_dma_trigger = sde_encoder_phys_vid_wait_dma_trigger;
	ops->wait_for_active = sde_encoder_phys_vid_wait_for_active;
}

struct sde_encoder_phys *sde_encoder_phys_vid_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc = NULL;
	struct sde_encoder_phys_vid *vid_enc = NULL;
	struct sde_hw_mdp *hw_mdp;
	struct sde_encoder_irq *irq;
	int i, ret = 0;

	if (!p) {
		ret = -EINVAL;
		goto fail;
	}

	vid_enc = kzalloc(sizeof(*vid_enc), GFP_KERNEL);
	if (!vid_enc) {
		ret = -ENOMEM;
		goto fail;
	}

	phys_enc = &vid_enc->base;

	hw_mdp = sde_rm_get_mdp(&p->sde_kms->rm);
	if (IS_ERR_OR_NULL(hw_mdp)) {
		ret = PTR_ERR(hw_mdp);
		SDE_ERROR("failed to get mdptop\n");
		goto fail;
	}
	phys_enc->hw_mdptop = hw_mdp;
	phys_enc->intf_idx = p->intf_idx;

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	sde_encoder_phys_vid_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_VIDEO;
	phys_enc->enc_spinlock = p->enc_spinlock;
	phys_enc->vblank_ctl_lock = p->vblank_ctl_lock;
	phys_enc->comp_type = p->comp_type;
	for (i = 0; i < INTR_IDX_MAX; i++) {
		irq = &phys_enc->irq[i];
		INIT_LIST_HEAD(&irq->cb.list);
		irq->irq_idx = -EINVAL;
		irq->hw_idx = -EINVAL;
		irq->cb.arg = phys_enc;
	}

	irq = &phys_enc->irq[INTR_IDX_VSYNC];
	irq->name = "vsync_irq";
	irq->intr_type = SDE_IRQ_TYPE_INTF_VSYNC;
	irq->intr_idx = INTR_IDX_VSYNC;
	irq->cb.func = sde_encoder_phys_vid_vblank_irq;

	irq = &phys_enc->irq[INTR_IDX_UNDERRUN];
	irq->name = "underrun";
	irq->intr_type = SDE_IRQ_TYPE_INTF_UNDER_RUN;
	irq->intr_idx = INTR_IDX_UNDERRUN;
	irq->cb.func = sde_encoder_phys_vid_underrun_irq;

	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);
	init_waitqueue_head(&phys_enc->pending_kickoff_wq);
	phys_enc->enable_state = SDE_ENC_DISABLED;

	SDE_DEBUG_VIDENC(vid_enc, "created intf idx:%d\n", p->intf_idx);

	return phys_enc;

fail:
	SDE_ERROR("failed to create encoder\n");
	if (vid_enc)
		sde_encoder_phys_vid_destroy(phys_enc);

	return ERR_PTR(ret);
}
