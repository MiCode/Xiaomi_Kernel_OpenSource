/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#define SDE_DEBUG_VIDENC(e, fmt, ...) SDE_DEBUG("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) && (e)->hw_intf ? \
		(e)->hw_intf->idx - INTF_0 : -1, ##__VA_ARGS__)

#define SDE_ERROR_VIDENC(e, fmt, ...) SDE_ERROR("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) && (e)->hw_intf ? \
		(e)->hw_intf->idx - INTF_0 : -1, ##__VA_ARGS__)

#define to_sde_encoder_phys_vid(x) \
	container_of(x, struct sde_encoder_phys_vid, base)

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
	memset(timing, 0, sizeof(*timing));
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

static inline u32 get_vertical_total(const struct intf_timing_params *timing)
{
	u32 active = timing->yres;
	u32 inactive =
	    timing->v_back_porch + timing->v_front_porch +
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
		const struct intf_timing_params *timing)
{
	u32 worst_case_needed_lines =
	    vid_enc->hw_intf->cap->prog_fetch_lines_worst_case;
	u32 start_of_frame_lines =
	    timing->v_back_porch + timing->vsync_pulse_width;
	u32 needed_vfp_lines = worst_case_needed_lines - start_of_frame_lines;
	u32 actual_vfp_lines = 0;

	/* Fetch must be outside active lines, otherwise undefined. */
	if (start_of_frame_lines >= worst_case_needed_lines) {
		SDE_DEBUG_VIDENC(vid_enc,
				"prog fetch is not needed, large vbp+vsw\n");
		actual_vfp_lines = 0;
	} else if (timing->v_front_porch < needed_vfp_lines) {
		/* Warn fetch needed, but not enough porch in panel config */
		pr_warn_once
			("low vbp+vfp may lead to perf issues in some cases\n");
		SDE_DEBUG_VIDENC(vid_enc,
				"less vfp than fetch req, using entire vfp\n");
		actual_vfp_lines = timing->v_front_porch;
	} else {
		SDE_DEBUG_VIDENC(vid_enc, "room in vfp for needed prefetch\n");
		actual_vfp_lines = needed_vfp_lines;
	}

	SDE_DEBUG_VIDENC(vid_enc,
		"v_front_porch %u v_back_porch %u vsync_pulse_width %u\n",
		timing->v_front_porch, timing->v_back_porch,
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

	if (WARN_ON_ONCE(!vid_enc->hw_intf->ops.setup_prg_fetch))
		return;

	vfp_fetch_lines = programmable_fetch_get_num_lines(vid_enc, timing);
	if (vfp_fetch_lines) {
		vert_total = get_vertical_total(timing);
		horiz_total = get_horizontal_total(timing);
		vfp_fetch_start_vsync_counter =
		    (vert_total - vfp_fetch_lines) * horiz_total + 1;
		f.enable = 1;
		f.fetch_start = vfp_fetch_start_vsync_counter;
	}

	SDE_DEBUG_VIDENC(vid_enc,
		"vfp_fetch_lines %u vfp_fetch_start_vsync_counter %u\n",
		vfp_fetch_lines, vfp_fetch_start_vsync_counter);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	vid_enc->hw_intf->ops.setup_prg_fetch(vid_enc->hw_intf, &f);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);
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

static void sde_encoder_phys_vid_setup_timing_engine(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc;
	struct drm_display_mode mode;
	struct intf_timing_params timing_params = { 0 };
	const struct sde_format *fmt = NULL;
	u32 fmt_fourcc = DRM_FORMAT_RGB888;
	unsigned long lock_flags;
	struct sde_hw_intf_cfg intf_cfg = { 0 };

	if (!phys_enc || !phys_enc->hw_ctl ||
		!phys_enc->hw_ctl->ops.setup_intf_cfg) {
		SDE_ERROR("invalid encoder %d\n", phys_enc != 0);
		return;
	}

	mode = phys_enc->cached_mode;
	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	if (!vid_enc->hw_intf->ops.setup_timing_gen) {
		SDE_ERROR("timing engine setup is not supported\n");
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "enabling mode:\n");
	drm_mode_debug_printmodeline(&mode);

	if (phys_enc->split_role != ENC_ROLE_SOLO ||
	    (mode.private_flags & MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420)) {
		mode.hdisplay >>= 1;
		mode.htotal >>= 1;
		mode.hsync_start >>= 1;
		mode.hsync_end >>= 1;
		mode.hskew >>= 1;

		SDE_DEBUG_VIDENC(vid_enc,
			"split_role %d, halve horizontal %d %d %d %d %d\n",
			phys_enc->split_role,
			mode.hdisplay, mode.htotal,
			mode.hsync_start, mode.hsync_end,
			mode.hskew);
	}

	drm_mode_to_intf_timing_params(vid_enc, &mode, &timing_params);

	fmt = sde_get_sde_format(fmt_fourcc);
	SDE_DEBUG_VIDENC(vid_enc, "fmt_fourcc 0x%X\n", fmt_fourcc);

	intf_cfg.intf = vid_enc->hw_intf->idx;
	intf_cfg.intf_mode_sel = SDE_CTL_MODE_SEL_VID;
	intf_cfg.stream_sel = 0; /* Don't care value for video mode */
	intf_cfg.mode_3d = sde_encoder_helper_get_3d_blend_mode(phys_enc);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	vid_enc->hw_intf->ops.setup_timing_gen(vid_enc->hw_intf,
			&timing_params, fmt);
	phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl, &intf_cfg);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	programmable_fetch_config(phys_enc, &timing_params);
}

static void sde_encoder_phys_vid_vblank_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_vid *vid_enc = arg;
	struct sde_encoder_phys *phys_enc;
	unsigned long lock_flags;
	int new_cnt;

	if (!vid_enc)
		return;

	phys_enc = &vid_enc->base;
	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
				phys_enc);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	new_cnt = atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);
	SDE_EVT32_IRQ(DRMID(phys_enc->parent), vid_enc->hw_intf->idx - INTF_0,
			new_cnt);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&phys_enc->pending_kickoff_wq);
}

static void sde_encoder_phys_vid_underrun_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_vid *vid_enc = arg;
	struct sde_encoder_phys *phys_enc;

	if (!vid_enc)
		return;

	phys_enc = &vid_enc->base;
	if (phys_enc->parent_ops.handle_underrun_virt)
		phys_enc->parent_ops.handle_underrun_virt(phys_enc->parent,
			phys_enc);
}

static bool sde_encoder_phys_vid_needs_single_flush(
		struct sde_encoder_phys *phys_enc)
{
	return phys_enc && phys_enc->split_role != ENC_ROLE_SOLO;
}

static int sde_encoder_phys_vid_register_irq(struct sde_encoder_phys *phys_enc,
	enum sde_intr_type intr_type, int idx,
	void (*irq_func)(void *, int), const char *irq_name)
{
	struct sde_encoder_phys_vid *vid_enc;
	int ret = 0;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	vid_enc->irq_idx[idx] = sde_core_irq_idx_lookup(phys_enc->sde_kms,
			intr_type, vid_enc->hw_intf->idx);
	if (vid_enc->irq_idx[idx] < 0) {
		SDE_ERROR_VIDENC(vid_enc,
			"failed to lookup IRQ index for %s type:%d\n", irq_name,
			intr_type);
		return -EINVAL;
	}

	vid_enc->irq_cb[idx].func = irq_func;
	vid_enc->irq_cb[idx].arg = vid_enc;
	ret = sde_core_irq_register_callback(phys_enc->sde_kms,
			vid_enc->irq_idx[idx], &vid_enc->irq_cb[idx]);
	if (ret) {
		SDE_ERROR_VIDENC(vid_enc,
			"failed to register IRQ callback for %s\n", irq_name);
		return ret;
	}

	ret = sde_core_irq_enable(phys_enc->sde_kms, &vid_enc->irq_idx[idx], 1);
	if (ret) {
		SDE_ERROR_VIDENC(vid_enc,
			"enable IRQ for intr:%s failed, irq_idx %d\n",
			irq_name, vid_enc->irq_idx[idx]);
		vid_enc->irq_idx[idx] = -EINVAL;

		/* unregister callback on IRQ enable failure */
		sde_core_irq_unregister_callback(phys_enc->sde_kms,
				vid_enc->irq_idx[idx], &vid_enc->irq_cb[idx]);
		return ret;
	}

	SDE_DEBUG_VIDENC(vid_enc, "registered irq %s idx: %d\n",
			irq_name, vid_enc->irq_idx[idx]);

	return ret;
}

static int sde_encoder_phys_vid_unregister_irq(
	struct sde_encoder_phys *phys_enc, int idx)
{
	struct sde_encoder_phys_vid *vid_enc;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		goto end;
	}

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	sde_core_irq_disable(phys_enc->sde_kms, &vid_enc->irq_idx[idx], 1);

	sde_core_irq_unregister_callback(phys_enc->sde_kms,
			vid_enc->irq_idx[idx], &vid_enc->irq_cb[idx]);

	SDE_DEBUG_VIDENC(vid_enc, "unregistered %d\n", vid_enc->irq_idx[idx]);

end:
	return 0;
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
		SDE_ERROR("invalid encoder\n");
		return;
	}

	phys_enc->hw_ctl = NULL;
	phys_enc->hw_cdm = NULL;

	rm = &phys_enc->sde_kms->rm;
	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	phys_enc->cached_mode = *adj_mode;
	SDE_DEBUG_VIDENC(vid_enc, "caching mode:\n");
	drm_mode_debug_printmodeline(adj_mode);

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

	/* CDM is optional */
	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_CDM);
	for (i = 0; i <= instance; i++) {
		sde_rm_get_hw(rm, &iter);
		if (i == instance)
			phys_enc->hw_cdm = (struct sde_hw_cdm *) iter.hw;
	}

	if (IS_ERR(phys_enc->hw_cdm)) {
		SDE_ERROR("CDM required but not allocated: %ld\n",
				PTR_ERR(phys_enc->hw_cdm));
		phys_enc->hw_cdm = NULL;
	}
}

static int sde_encoder_phys_vid_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	int ret = 0;
	struct sde_encoder_phys_vid *vid_enc;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	vid_enc = to_sde_encoder_phys_vid(phys_enc);

	/* Slave encoders don't report vblank */
	if (!sde_encoder_phys_vid_is_master(phys_enc))
		return 0;

	SDE_DEBUG_VIDENC(vid_enc, "[%pS] enable=%d/%d\n",
			__builtin_return_address(0),
			enable, atomic_read(&phys_enc->vblank_refcount));

	SDE_EVT32(DRMID(phys_enc->parent), enable,
			atomic_read(&phys_enc->vblank_refcount));

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1)
		ret = sde_encoder_phys_vid_register_irq(phys_enc,
			SDE_IRQ_TYPE_INTF_VSYNC,
			INTR_IDX_VSYNC,
			sde_encoder_phys_vid_vblank_irq, "vsync_irq");
	else if (!enable && atomic_dec_return(&phys_enc->vblank_refcount) == 0)
		ret = sde_encoder_phys_vid_unregister_irq(phys_enc,
			INTR_IDX_VSYNC);

	if (ret)
		SDE_ERROR_VIDENC(vid_enc,
				"control vblank irq error %d, enable %d\n",
				ret, enable);

	return ret;
}

static void sde_encoder_phys_vid_enable(struct sde_encoder_phys *phys_enc)
{
	struct msm_drm_private *priv;
	struct sde_encoder_phys_vid *vid_enc;
	struct sde_hw_intf *intf;
	struct sde_hw_ctl *ctl;
	struct sde_hw_cdm *hw_cdm = NULL;
	struct drm_display_mode mode;
	const struct sde_format *fmt = NULL;
	u32 flush_mask = 0;
	int ret;

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->dev ||
			!phys_enc->parent->dev->dev_private) {
		SDE_ERROR("invalid encoder/device\n");
		return;
	}
	hw_cdm = phys_enc->hw_cdm;
	priv = phys_enc->parent->dev->dev_private;
	mode = phys_enc->cached_mode;

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	intf = vid_enc->hw_intf;
	ctl = phys_enc->hw_ctl;
	if (!vid_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
				vid_enc->hw_intf != 0, phys_enc->hw_ctl != 0);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	if (WARN_ON(!vid_enc->hw_intf->ops.enable_timing))
		return;

	sde_power_data_bus_bandwidth_ctrl(&priv->phandle,
			phys_enc->sde_kms->core_client, true);

	sde_encoder_helper_split_config(phys_enc, vid_enc->hw_intf->idx);

	sde_encoder_phys_vid_setup_timing_engine(phys_enc);
	ret = sde_encoder_phys_vid_control_vblank_irq(phys_enc, true);
	if (ret)
		goto end;

	ret = sde_encoder_phys_vid_register_irq(phys_enc,
		SDE_IRQ_TYPE_INTF_UNDER_RUN,
		INTR_IDX_UNDERRUN,
		sde_encoder_phys_vid_underrun_irq, "underrun");
	if (ret) {
		sde_encoder_phys_vid_control_vblank_irq(phys_enc, false);
		goto end;
	}

	if (mode.private_flags & MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420)
		fmt = sde_get_sde_format(DRM_FORMAT_YUV420);

	if (fmt) {
		struct sde_rect hdmi_roi;

		hdmi_roi.w = mode.hdisplay;
		hdmi_roi.h = mode.vdisplay;
		sde_encoder_phys_setup_cdm(phys_enc, fmt,
			CDM_CDWN_OUTPUT_HDMI, &hdmi_roi);
	}

	ctl->ops.get_bitmask_intf(ctl, &flush_mask, intf->idx);
	if (ctl->ops.get_bitmask_cdm && hw_cdm)
		ctl->ops.get_bitmask_cdm(ctl, &flush_mask, hw_cdm->idx);
	ctl->ops.update_pending_flush(ctl, flush_mask);

	SDE_DEBUG_VIDENC(vid_enc, "update pending flush ctl %d flush_mask %x\n",
		ctl->idx - CTL_0, flush_mask);

	/* ctl_flush & timing engine enable will be triggered by framework */
	if (phys_enc->enable_state == SDE_ENC_DISABLED)
		phys_enc->enable_state = SDE_ENC_ENABLING;

end:
	return;
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
	struct sde_mdss_cfg *vid_catalog;

	if (!phys_enc || !hw_res) {
		SDE_ERROR("invalid arg(s), enc %d hw_res %d conn_state %d\n",
			phys_enc != NULL, hw_res != NULL, conn_state != NULL);
		return;
	}

	vid_catalog = phys_enc->sde_kms->catalog;
	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	if (!vid_enc->hw_intf || !vid_catalog) {
		SDE_ERROR("invalid arg(s), hw_intf %d vid_catalog %d\n",
			  vid_enc->hw_intf != NULL, vid_catalog != NULL);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");
	if (vid_enc->hw_intf->idx > INTF_MAX) {
		SDE_ERROR("invalid arg(s), idx %d\n",
			  vid_enc->hw_intf->idx);
		return;
	}
	hw_res->intfs[vid_enc->hw_intf->idx - INTF_0] = INTF_MODE_VIDEO;

	if (vid_catalog->intf[vid_enc->hw_intf->idx - INTF_0].type
			== INTF_HDMI)
		hw_res->needs_cdm = true;
	SDE_DEBUG_DRIVER("[vid] needs_cdm=%d\n", hw_res->needs_cdm);
}

static int sde_encoder_phys_vid_wait_for_vblank(
		struct sde_encoder_phys *phys_enc, bool notify)
{
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);
	u32 irq_status;
	int ret;

	if (!sde_encoder_phys_vid_is_master(phys_enc)) {
		/* always signal done for slave video encoder */
		if (notify && phys_enc->parent_ops.handle_frame_done)
			phys_enc->parent_ops.handle_frame_done(
					phys_enc->parent, phys_enc,
					SDE_ENCODER_FRAME_EVENT_DONE);
		return 0;
	}

	if (phys_enc->enable_state != SDE_ENC_ENABLED) {
		SDE_ERROR("encoder not enabled\n");
		return -EWOULDBLOCK;
	}

	SDE_EVT32(DRMID(phys_enc->parent), vid_enc->hw_intf->idx - INTF_0,
			SDE_EVTLOG_FUNC_ENTRY);

	/* Wait for kickoff to complete */
	ret = sde_encoder_helper_wait_event_timeout(
			DRMID(phys_enc->parent),
			vid_enc->hw_intf->idx - INTF_0,
			&phys_enc->pending_kickoff_wq,
			&phys_enc->pending_kickoff_cnt,
			KICKOFF_TIMEOUT_MS);
	if (ret <= 0) {
		irq_status = sde_core_irq_read(phys_enc->sde_kms,
				INTR_IDX_VSYNC, true);
		if (irq_status) {
			SDE_EVT32(DRMID(phys_enc->parent),
					vid_enc->hw_intf->idx - INTF_0);
			SDE_DEBUG_VIDENC(vid_enc, "done, irq not triggered\n");
			if (notify && phys_enc->parent_ops.handle_frame_done)
				phys_enc->parent_ops.handle_frame_done(
						phys_enc->parent, phys_enc,
						SDE_ENCODER_FRAME_EVENT_DONE);
			sde_encoder_phys_vid_vblank_irq(vid_enc,
					INTR_IDX_VSYNC);
			ret = 0;
		} else {
			SDE_EVT32(DRMID(phys_enc->parent),
					vid_enc->hw_intf->idx - INTF_0);
			SDE_ERROR_VIDENC(vid_enc, "kickoff timed out\n");
			if (notify && phys_enc->parent_ops.handle_frame_done)
				phys_enc->parent_ops.handle_frame_done(
						phys_enc->parent, phys_enc,
						SDE_ENCODER_FRAME_EVENT_ERROR);
			ret = -ETIMEDOUT;
		}
	} else {
		if (notify && phys_enc->parent_ops.handle_frame_done)
			phys_enc->parent_ops.handle_frame_done(
					phys_enc->parent, phys_enc,
					SDE_ENCODER_FRAME_EVENT_DONE);
		ret = 0;
	}

	return 0;
}

static int sde_encoder_phys_vid_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc)
{
	int ret;

	ret = sde_encoder_phys_vid_wait_for_vblank(phys_enc, true);

	return ret;
}

static void sde_encoder_phys_vid_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc;
	struct sde_hw_ctl *ctl;
	int rc;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	vid_enc = to_sde_encoder_phys_vid(phys_enc);

	ctl = phys_enc->hw_ctl;
	if (!ctl || !ctl->ops.wait_reset_status)
		return;

	/*
	 * hw supports hardware initiated ctl reset, so before we kickoff a new
	 * frame, need to check and wait for hw initiated ctl reset completion
	 */
	rc = ctl->ops.wait_reset_status(ctl);
	if (rc) {
		SDE_ERROR_VIDENC(vid_enc, "ctl %d reset failure: %d\n",
				ctl->idx, rc);
		SDE_DBG_DUMP("panic");
	}
}

static void sde_encoder_phys_vid_disable(struct sde_encoder_phys *phys_enc)
{
	struct msm_drm_private *priv;
	struct sde_encoder_phys_vid *vid_enc;
	unsigned long lock_flags;
	int ret;

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->dev ||
			!phys_enc->parent->dev->dev_private) {
		SDE_ERROR("invalid encoder/device\n");
		return;
	}
	priv = phys_enc->parent->dev->dev_private;

	vid_enc = to_sde_encoder_phys_vid(phys_enc);
	if (!vid_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
				vid_enc->hw_intf != 0, phys_enc->hw_ctl != 0);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	if (WARN_ON(!vid_enc->hw_intf->ops.enable_timing))
		return;

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("already disabled\n");
		return;
	}

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	vid_enc->hw_intf->ops.enable_timing(vid_enc->hw_intf, 0);
	if (sde_encoder_phys_vid_is_master(phys_enc))
		sde_encoder_phys_inc_pending(phys_enc);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	/*
	 * Wait for a vsync so we know the ENABLE=0 latched before
	 * the (connector) source of the vsync's gets disabled,
	 * otherwise we end up in a funny state if we re-enable
	 * before the disable latches, which results that some of
	 * the settings changes for the new modeset (like new
	 * scanout buffer) don't latch properly..
	 */
	if (sde_encoder_phys_vid_is_master(phys_enc)) {
		ret = sde_encoder_phys_vid_wait_for_vblank(phys_enc, false);
		if (ret) {
			atomic_set(&phys_enc->pending_kickoff_cnt, 0);
			SDE_ERROR_VIDENC(vid_enc,
					"failure waiting for disable: %d\n",
					ret);
			SDE_EVT32(DRMID(phys_enc->parent),
					vid_enc->hw_intf->idx - INTF_0, ret);
		}
		sde_encoder_phys_vid_control_vblank_irq(phys_enc, false);
	}

	sde_power_data_bus_bandwidth_ctrl(&priv->phandle,
			phys_enc->sde_kms->core_client, false);

	if (atomic_read(&phys_enc->vblank_refcount))
		SDE_ERROR_VIDENC(vid_enc, "invalid vblank refcount %d\n",
				atomic_read(&phys_enc->vblank_refcount));

	if (phys_enc->hw_cdm && phys_enc->hw_cdm->ops.disable) {
		SDE_DEBUG_DRIVER("[cdm_disable]\n");
		phys_enc->hw_cdm->ops.disable(phys_enc->hw_cdm);
	}

	phys_enc->enable_state = SDE_ENC_DISABLED;
}

static void sde_encoder_phys_vid_handle_post_kickoff(
		struct sde_encoder_phys *phys_enc)
{
	unsigned long lock_flags;
	struct sde_encoder_phys_vid *vid_enc;

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
		SDE_EVT32(DRMID(phys_enc->parent),
				vid_enc->hw_intf->idx - INTF_0);
		spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
		vid_enc->hw_intf->ops.enable_timing(vid_enc->hw_intf, 1);
		spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);
		phys_enc->enable_state = SDE_ENC_ENABLED;
	}
}

static void sde_encoder_phys_vid_setup_misr(struct sde_encoder_phys *phys_enc,
			struct sde_misr_params *misr_map)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);

	if (vid_enc && vid_enc->hw_intf && vid_enc->hw_intf->ops.setup_misr)
		vid_enc->hw_intf->ops.setup_misr(vid_enc->hw_intf, misr_map);
}

static void sde_encoder_phys_vid_collect_misr(struct sde_encoder_phys *phys_enc,
			struct sde_misr_params *misr_map)
{
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	if (vid_enc && vid_enc->hw_intf && vid_enc->hw_intf->ops.collect_misr)
		vid_enc->hw_intf->ops.collect_misr(vid_enc->hw_intf, misr_map);
}

static void sde_encoder_phys_vid_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->is_master = sde_encoder_phys_vid_is_master;
	ops->mode_set = sde_encoder_phys_vid_mode_set;
	ops->mode_fixup = sde_encoder_phys_vid_mode_fixup;
	ops->enable = sde_encoder_phys_vid_enable;
	ops->disable = sde_encoder_phys_vid_disable;
	ops->destroy = sde_encoder_phys_vid_destroy;
	ops->get_hw_resources = sde_encoder_phys_vid_get_hw_resources;
	ops->control_vblank_irq = sde_encoder_phys_vid_control_vblank_irq;
	ops->wait_for_commit_done = sde_encoder_phys_vid_wait_for_commit_done;
	ops->prepare_for_kickoff = sde_encoder_phys_vid_prepare_for_kickoff;
	ops->handle_post_kickoff = sde_encoder_phys_vid_handle_post_kickoff;
	ops->needs_single_flush = sde_encoder_phys_vid_needs_single_flush;
	ops->setup_misr = sde_encoder_phys_vid_setup_misr;
	ops->collect_misr = sde_encoder_phys_vid_collect_misr;
}

struct sde_encoder_phys *sde_encoder_phys_vid_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc = NULL;
	struct sde_encoder_phys_vid *vid_enc = NULL;
	struct sde_rm_hw_iter iter;
	struct sde_hw_mdp *hw_mdp;
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

	/**
	 * hw_intf resource permanently assigned to this encoder
	 * Other resources allocated at atomic commit time by use case
	 */
	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_INTF);
	while (sde_rm_get_hw(&p->sde_kms->rm, &iter)) {
		struct sde_hw_intf *hw_intf = (struct sde_hw_intf *)iter.hw;

		if (hw_intf->idx == p->intf_idx) {
			vid_enc->hw_intf = hw_intf;
			break;
		}
	}

	if (!vid_enc->hw_intf) {
		ret = -EINVAL;
		SDE_ERROR("failed to get hw_intf\n");
		goto fail;
	}

	phys_enc->misr_map = kzalloc(sizeof(struct sde_misr_params),
						GFP_KERNEL);
	if (!phys_enc->misr_map)
		SDE_ERROR("sde misr map allocation failed\n");

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	sde_encoder_phys_vid_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_VIDEO;
	phys_enc->enc_spinlock = p->enc_spinlock;
	for (i = 0; i < INTR_IDX_MAX; i++)
		INIT_LIST_HEAD(&vid_enc->irq_cb[i].list);
	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
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
