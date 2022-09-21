// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 */

#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/sde_rsc.h>

#include "msm_drv.h"
#include "sde_kms.h"
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"
#include "sde_hw_ctl.h"
#include "sde_formats.h"
#include "sde_encoder_phys.h"
#include "sde_power_handle.h"
#include "sde_hw_dsc.h"
#include "sde_hw_vdc.h"
#include "sde_crtc.h"
#include "sde_trace.h"
#include "sde_core_irq.h"
#include "sde_dsc_helper.h"
#include "sde_vdc_helper.h"

#define SDE_DEBUG_DCE(e, fmt, ...) SDE_DEBUG("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

#define SDE_ERROR_DCE(e, fmt, ...) SDE_ERROR("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

bool sde_encoder_is_dsc_merge(struct drm_encoder *drm_enc)
{
	enum sde_rm_topology_name topology;
	struct sde_encoder_virt *sde_enc;
	struct drm_connector *drm_conn;

	if (!drm_enc)
		return false;

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (!sde_enc->cur_master)
		return false;

	drm_conn = sde_enc->cur_master->connector;
	if (!drm_conn)
		return false;

	topology = sde_connector_get_topology_name(drm_conn);
	if (topology == SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE)
		return true;

	return false;
}

static int _dce_dsc_update_pic_dim(struct msm_display_dsc_info *dsc,
		int pic_width, int pic_height)
{
	if (!dsc || !pic_width || !pic_height) {
		SDE_ERROR("invalid input: pic_width=%d pic_height=%d\n",
			pic_width, pic_height);
		return -EINVAL;
	}

	if ((pic_width % dsc->config.slice_width) ||
		(pic_height % dsc->config.slice_height)) {
		SDE_ERROR("pic_dim=%dx%d has to be multiple of slice=%dx%d\n",
			pic_width, pic_height,
			dsc->config.slice_width, dsc->config.slice_height);
		return -EINVAL;
	}

	dsc->config.pic_width = pic_width;
	dsc->config.pic_height = pic_height;

	return 0;
}

static int _dce_vdc_update_pic_dim(struct msm_display_vdc_info *vdc,
		int frame_width, int frame_height)
{
	if (!vdc || !frame_width || !frame_height) {
		SDE_ERROR("invalid input: frame_width=%d frame_height=%d\n",
			frame_width, frame_height);
		return -EINVAL;
	}

	if ((frame_width % vdc->slice_width) ||
			(frame_height % vdc->slice_height)) {
		SDE_ERROR("pic_dim=%dx%d has to be multiple of slice=%dx%d\n",
			frame_width, frame_height,
			vdc->slice_width, vdc->slice_height);
		return -EINVAL;
	}

	vdc->frame_width = frame_width;
	vdc->frame_height = frame_height;

	return 0;
}

static int _dce_dsc_initial_line_calc(struct msm_display_dsc_info *dsc,
		int enc_ip_width,
		int dsc_cmn_mode)
{
	int max_ssm_delay, max_se_size, max_muxword_size;
	int compress_bpp_group, obuf_latency, input_ssm_out_latency;
	int base_hs_latency, chunk_bits, ob_data_width;
	int output_rate_extra_budget_bits, multi_hs_extra_budget_bits;
	int multi_hs_extra_latency,  mux_word_size;
	int ob_data_width_4comps, ob_data_width_3comps;
	int output_rate_ratio_complement, container_slice_width;
	int rtl_num_components, multi_hs_c, multi_hs_d;

	int bpc = dsc->config.bits_per_component;
	int bpp = DSC_BPP(dsc->config);
	int num_of_active_ss = dsc->config.slice_count;
	bool native_422 = dsc->config.native_422;
	bool native_420 = dsc->config.native_420;

	/* Hardent core config */
	int multiplex_mode_enable = 0, split_panel_enable = 0;
	int rtl_max_bpc = 10, rtl_output_data_width = 64;
	int pipeline_latency = 28;

	if (dsc_cmn_mode & DSC_MODE_MULTIPLEX)
		multiplex_mode_enable = 1;
	if (dsc_cmn_mode & DSC_MODE_SPLIT_PANEL)
		split_panel_enable = 0;
	container_slice_width = (native_422 ?
			dsc->config.slice_width / 2 : dsc->config.slice_width);
	max_muxword_size = (rtl_max_bpc >= 12) ? 64 : 48;
	max_se_size = 4 * (rtl_max_bpc + 1);
	max_ssm_delay = max_se_size + max_muxword_size - 1;
	mux_word_size = (bpc >= 12) ? 64 : 48;
	compress_bpp_group = native_422 ? (2 * bpp) : bpp;
	input_ssm_out_latency = pipeline_latency + 3 * (max_ssm_delay + 2)
			* num_of_active_ss;
	rtl_num_components = (native_420 || native_422) ? 4 : 3;
	ob_data_width_4comps = (rtl_output_data_width >= (2 *
			max_muxword_size)) ?
			rtl_output_data_width :
			(2 * rtl_output_data_width);
	ob_data_width_3comps = (rtl_output_data_width >= max_muxword_size) ?
			rtl_output_data_width : 2 * rtl_output_data_width;
	ob_data_width = (rtl_num_components == 4) ?
			ob_data_width_4comps : ob_data_width_3comps;
	obuf_latency = DIV_ROUND_UP((9 * ob_data_width + mux_word_size),
			compress_bpp_group) + 1;
	base_hs_latency = dsc->config.initial_xmit_delay +
		input_ssm_out_latency + obuf_latency;
	chunk_bits = 8 * dsc->config.slice_chunk_size;
	output_rate_ratio_complement = ob_data_width - compress_bpp_group;
	output_rate_extra_budget_bits =
		(output_rate_ratio_complement * chunk_bits) >>
		((ob_data_width == 128) ? 7 : 6);
	multi_hs_c = split_panel_enable * multiplex_mode_enable;
	multi_hs_d = (num_of_active_ss > 1) * (ob_data_width >
			compress_bpp_group);
	multi_hs_extra_budget_bits = multi_hs_c ?
				chunk_bits : (multi_hs_d ? chunk_bits :
					output_rate_extra_budget_bits);
	multi_hs_extra_latency = DIV_ROUND_UP(multi_hs_extra_budget_bits,
			compress_bpp_group);
	dsc->initial_lines = DIV_ROUND_UP((base_hs_latency +
				multi_hs_extra_latency),
			container_slice_width);

	return 0;
}

static bool _dce_dsc_ich_reset_override_needed(bool pu_en,
		struct msm_display_dsc_info *dsc)
{
	/*
	 * As per the DSC spec, ICH_RESET can be either end of the slice line
	 * or at the end of the slice. HW internally generates ich_reset at
	 * end of the slice line if DSC_MERGE is used or encoder has two
	 * soft slices. However, if encoder has only 1 soft slice and DSC_MERGE
	 * is not used then it will generate ich_reset at the end of slice.
	 *
	 * Now as per the spec, during one PPS session, position where
	 * ich_reset is generated should not change. Now if full-screen frame
	 * has more than 1 soft slice then HW will automatically generate
	 * ich_reset at the end of slice_line. But for the same panel, if
	 * partial frame is enabled and only 1 encoder is used with 1 slice,
	 * then HW will generate ich_reset at end of the slice. This is a
	 * mismatch. Prevent this by overriding HW's decision.
	 */
	return pu_en && dsc && (dsc->config.slice_count > 1) &&
		(dsc->config.slice_width == dsc->config.pic_width);
}

static void _dce_dsc_pipe_cfg(struct sde_hw_dsc *hw_dsc,
		struct sde_hw_pingpong *hw_pp, struct msm_display_dsc_info *dsc,
		u32 common_mode, bool ich_reset,
		struct sde_hw_pingpong *hw_dsc_pp,
		enum sde_3d_blend_mode mode_3d,
		bool disable_merge_3d, bool enable,
		bool half_panel_partial_update)
{
	if (!enable) {
		/*
		 * avoid disabling dsc encoder in pp-block as it is
		 * not double-buffered and is not required to be disabled
		 * for half panel updates
		 */
		if (hw_dsc_pp && hw_dsc_pp->ops.disable_dsc
				&& !half_panel_partial_update)
			hw_dsc_pp->ops.disable_dsc(hw_dsc_pp);

		if (hw_dsc && hw_dsc->ops.dsc_disable)
			hw_dsc->ops.dsc_disable(hw_dsc);

		if (hw_dsc && hw_dsc->ops.bind_pingpong_blk)
			hw_dsc->ops.bind_pingpong_blk(hw_dsc, false,
					PINGPONG_MAX);

		if (mode_3d && hw_pp && hw_pp->ops.reset_3d_mode)
			hw_pp->ops.reset_3d_mode(hw_pp);
		return;
	}

	if (!dsc || !hw_dsc || !hw_pp) {
		SDE_ERROR("invalid params %d %d %d\n", !dsc, !hw_dsc,
				!hw_pp);
		return;
	}

	if (hw_dsc->ops.dsc_config)
		hw_dsc->ops.dsc_config(hw_dsc, dsc, common_mode, ich_reset);

	if (hw_dsc->ops.dsc_config_thresh)
		hw_dsc->ops.dsc_config_thresh(hw_dsc, dsc);

	if (hw_dsc_pp && hw_dsc_pp->ops.setup_dsc)
		hw_dsc_pp->ops.setup_dsc(hw_dsc_pp);

	if (mode_3d && disable_merge_3d && hw_pp->ops.reset_3d_mode) {
		SDE_DEBUG("disabling 3d mux \n");
		hw_pp->ops.reset_3d_mode(hw_pp);
	} else if (mode_3d && !disable_merge_3d && hw_pp->ops.setup_3d_mode) {
		SDE_DEBUG("enabling 3d mux \n");
		hw_pp->ops.setup_3d_mode(hw_pp, mode_3d);
	}

	if (hw_dsc && hw_dsc->ops.bind_pingpong_blk)
		hw_dsc->ops.bind_pingpong_blk(hw_dsc, true, hw_pp->idx);

	if (hw_dsc_pp && hw_dsc_pp->ops.enable_dsc)
		hw_dsc_pp->ops.enable_dsc(hw_dsc_pp);
}

static void _dce_vdc_pipe_cfg(struct sde_hw_vdc *hw_vdc,
		struct sde_hw_pingpong *hw_pp,
		struct msm_display_vdc_info *vdc,
		enum sde_3d_blend_mode mode_3d,
		bool disable_merge_3d, bool enable)
{

	if (!vdc || !hw_vdc || !hw_pp) {
		SDE_ERROR("invalid params %d %d %d\n", !vdc, !hw_vdc,
				!hw_pp);
		return;
	}

	if (!enable) {
		if (hw_vdc->ops.vdc_disable)
			hw_vdc->ops.vdc_disable(hw_vdc);

		if (hw_vdc->ops.bind_pingpong_blk)
			hw_vdc->ops.bind_pingpong_blk(hw_vdc, false,
					PINGPONG_MAX);

		if (mode_3d && hw_pp->ops.reset_3d_mode)
			hw_pp->ops.reset_3d_mode(hw_pp);
		return;
	}

	if (hw_vdc->ops.vdc_config)
		hw_vdc->ops.vdc_config(hw_vdc, vdc);

	if (mode_3d && disable_merge_3d && hw_pp->ops.reset_3d_mode) {
		SDE_DEBUG("disabling 3d mux\n");
		hw_pp->ops.reset_3d_mode(hw_pp);
	}

	if (mode_3d && !disable_merge_3d && hw_pp->ops.setup_3d_mode) {
		SDE_DEBUG("enabling 3d mux\n");
		hw_pp->ops.setup_3d_mode(hw_pp, mode_3d);
	}
	if (hw_vdc->ops.bind_pingpong_blk)
		hw_vdc->ops.bind_pingpong_blk(hw_vdc, true, hw_pp->idx);

}

static inline bool _dce_check_half_panel_update(int num_lm,
			struct sde_encoder_virt *sde_enc)
{
	/**
	 * partial update logic is currently supported only upto dual
	 * pipe configurations.
	 */
	return (sde_enc->cur_conn_roi.w <=
			(sde_enc->cur_master->cached_mode.hdisplay / 2));
}

static int _dce_dsc_setup_single(struct sde_encoder_virt *sde_enc,
		struct msm_display_dsc_info *dsc,
		unsigned long affected_displays, int index,
		const struct sde_rect *roi, int dsc_common_mode,
		bool merge_3d, bool disable_merge_3d, enum sde_3d_blend_mode mode_3d,
		bool dsc_4hsmerge, bool half_panel_partial_update,
		int ich_res)
{
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_dsc *hw_dsc;
	struct sde_hw_pingpong *hw_pp;
	struct sde_hw_pingpong *hw_dsc_pp;
	struct sde_hw_intf_cfg_v1 cfg;
	bool active = !!((1 << index) & affected_displays);

	hw_ctl = sde_enc->cur_master->hw_ctl;

	/*
	 * in 3d_merge or half_panel partial update, dsc should be
	 * bound to the pp which is driving the update, else in
	 * 3d_merge dsc should be bound to left side of the pipe
	 */
	if (merge_3d || half_panel_partial_update)
		hw_pp = (active) ? sde_enc->hw_pp[0] : sde_enc->hw_pp[1];
	else
		hw_pp = sde_enc->hw_pp[index];

	hw_dsc = sde_enc->hw_dsc[index];
	hw_dsc_pp = sde_enc->hw_dsc_pp[index];

	if (!hw_pp || !hw_dsc) {
		SDE_ERROR_DCE(sde_enc, "DSC: invalid params %d %d\n", !!hw_pp,
				!!hw_dsc);
		SDE_EVT32(DRMID(&sde_enc->base), !hw_pp, !hw_dsc,
				SDE_EVTLOG_ERROR);
		return -EINVAL;
	}

	SDE_EVT32(DRMID(&sde_enc->base), roi->w, roi->h, dsc_common_mode,
			index, active, merge_3d, disable_merge_3d,
			dsc_4hsmerge);

	_dce_dsc_pipe_cfg(hw_dsc, hw_pp, dsc, dsc_common_mode, ich_res,
			hw_dsc_pp, mode_3d, disable_merge_3d, active,
			half_panel_partial_update);

	memset(&cfg, 0, sizeof(cfg));
	cfg.dsc[cfg.dsc_count++] = hw_dsc->idx;

	if (hw_ctl->ops.update_intf_cfg)
		hw_ctl->ops.update_intf_cfg(hw_ctl, &cfg, active);

	if (hw_ctl->ops.update_bitmask)
		hw_ctl->ops.update_bitmask(hw_ctl, SDE_HW_FLUSH_DSC,
				hw_dsc->idx, true);

	SDE_DEBUG_DCE(sde_enc, "update_intf_cfg hw_ctl[%d], dsc:%d, %s %d\n",
			hw_ctl->idx, cfg.dsc[0],
			active ? "enabled" : "disabled",
			half_panel_partial_update);

	if (mode_3d) {
		memset(&cfg, 0, sizeof(cfg));

		cfg.merge_3d[cfg.merge_3d_count++] = hw_pp->merge_3d->idx;

		if (hw_ctl->ops.update_intf_cfg)
			hw_ctl->ops.update_intf_cfg(hw_ctl, &cfg,
					!disable_merge_3d);

		if (hw_ctl->ops.update_bitmask)
			hw_ctl->ops.update_bitmask(
					hw_ctl, SDE_HW_FLUSH_MERGE_3D,
					hw_pp->merge_3d->idx, true);

		SDE_DEBUG("mode_3d %s, on CTL_%d PP-%d merge3d:%d\n",
				!disable_merge_3d ? "enabled" : "disabled",
				hw_ctl->idx - CTL_0, hw_pp->idx - PINGPONG_0,
				hw_pp->merge_3d->idx - MERGE_3D_0);
	}

	return 0;
}

static int _dce_dsc_setup_helper(struct sde_encoder_virt *sde_enc,
		unsigned long affected_displays,
		enum sde_rm_topology_name topology)
{
	struct sde_kms *sde_kms;
	struct sde_encoder_phys *enc_master;
	struct msm_display_dsc_info *dsc = NULL;
	const struct sde_rm_topology_def *def;
	const struct sde_rect *roi;
	enum sde_3d_blend_mode mode_3d;
	bool dsc_merge, merge_3d, dsc_4hsmerge;
	bool disable_merge_3d = false;
	int this_frame_slices;
	int intf_ip_w, enc_ip_w;
	int num_intf, num_dsc, num_lm;
	int ich_res;
	int dsc_pic_width;
	int dsc_common_mode = 0;
	int i, rc = 0;

	sde_kms = sde_encoder_get_kms(&sde_enc->base);

	def = sde_rm_topology_get_topology_def(&sde_kms->rm, topology);
	if (IS_ERR_OR_NULL(def))
		return -EINVAL;

	enc_master = sde_enc->cur_master;
	roi = &sde_enc->cur_conn_roi;
	dsc = &sde_enc->mode_info.comp_info.dsc_info;
	num_lm = def->num_lm;
	num_dsc = def->num_comp_enc;
	num_intf = def->num_intf;
	mode_3d = (num_lm > num_dsc) ? BLEND_3D_H_ROW_INT : BLEND_3D_NONE;
	merge_3d = (mode_3d != BLEND_3D_NONE) ? true : false;

	dsc->half_panel_pu = _dce_check_half_panel_update(num_lm, sde_enc);
	dsc_merge = ((num_dsc > num_intf) && !dsc->half_panel_pu) ?
			true : false;
	disable_merge_3d = (merge_3d && dsc->half_panel_pu) ?
			true : false;
	dsc_4hsmerge = (dsc_merge && num_dsc == 4 && num_intf == 1) ?
			true : false;

	/*
	 * If this encoder is driving more than one DSC encoder, they
	 * operate in tandem, same pic dimension needs to be used by
	 * each of them.(pp-split is assumed to be not supported)
	 *
	 * If encoder is driving more than 2 DSCs, each DSC pair will operate
	 * on half of the picture in tandem.
	 */
	if (num_dsc > 2) {
		dsc_pic_width = roi->w / 2;
		dsc->dsc_4hsmerge_en = dsc_4hsmerge;
	} else
		dsc_pic_width = roi->w;

	_dce_dsc_update_pic_dim(dsc, dsc_pic_width, roi->h);

	this_frame_slices = roi->w / dsc->config.slice_width;
	intf_ip_w = this_frame_slices * dsc->config.slice_width;
	enc_ip_w = intf_ip_w;

	if (!dsc->half_panel_pu)
		intf_ip_w /= num_intf;
	if (!dsc->half_panel_pu && (num_dsc > 1))
		dsc_common_mode |= DSC_MODE_SPLIT_PANEL;
	if (dsc_merge) {
		dsc_common_mode |= DSC_MODE_MULTIPLEX;
		/*
		 * in dsc merge case: when using 2 encoders for the same
		 * stream, no. of slices need to be same on both the
		 * encoders.
		 */
		enc_ip_w = intf_ip_w / 2;
	}
	if (enc_master->intf_mode == INTF_MODE_VIDEO)
		dsc_common_mode |= DSC_MODE_VIDEO;

	sde_dsc_populate_dsc_private_params(dsc, intf_ip_w);

	_dce_dsc_initial_line_calc(dsc, enc_ip_w, dsc_common_mode);

	/*
	 * __is_ich_reset_override_needed should be called only after
	 * updating pic dimension, mdss_panel_dsc_update_pic_dim.
	 */
	ich_res = _dce_dsc_ich_reset_override_needed(dsc->half_panel_pu, dsc);

	SDE_DEBUG_DCE(sde_enc, "pic_w: %d pic_h: %d mode:%d\n",
				roi->w, roi->h, dsc_common_mode);

	for (i = 0; i < num_dsc; i++) {
		rc = _dce_dsc_setup_single(sde_enc, dsc, affected_displays, i,
				roi, dsc_common_mode, merge_3d,
				disable_merge_3d, mode_3d, dsc_4hsmerge,
				dsc->half_panel_pu, ich_res);
		if (rc)
			break;
	}

	return rc;
}

static int _dce_dsc_setup(struct sde_encoder_virt *sde_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct drm_connector *drm_conn;
	enum sde_rm_topology_name topology;

	if (!sde_enc || !params || !sde_enc->phys_encs[0] ||
			!sde_enc->phys_encs[0]->connector)
		return -EINVAL;

	drm_conn = sde_enc->phys_encs[0]->connector;

	topology = sde_connector_get_topology_name(drm_conn);
	if (topology == SDE_RM_TOPOLOGY_NONE) {
		SDE_ERROR_DCE(sde_enc, "topology not set yet\n");
		return -EINVAL;
	}

	SDE_DEBUG_DCE(sde_enc, "topology:%d\n", topology);

	if (sde_kms_rect_is_equal(&sde_enc->cur_conn_roi,
			&sde_enc->prv_conn_roi))
		return 0;

	SDE_EVT32(DRMID(&sde_enc->base), topology,
			sde_enc->cur_conn_roi.x, sde_enc->cur_conn_roi.y,
			sde_enc->cur_conn_roi.w, sde_enc->cur_conn_roi.h,
			sde_enc->prv_conn_roi.x, sde_enc->prv_conn_roi.y,
			sde_enc->prv_conn_roi.w, sde_enc->prv_conn_roi.h,
			sde_enc->cur_master->cached_mode.hdisplay,
			sde_enc->cur_master->cached_mode.vdisplay);

	return _dce_dsc_setup_helper(sde_enc, params->affected_displays,
			topology);
}

static int _dce_vdc_setup(struct sde_encoder_virt *sde_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct drm_connector *drm_conn;
	struct sde_kms *sde_kms;
	struct sde_encoder_phys *enc_master;
	struct sde_hw_vdc *hw_vdc[MAX_CHANNELS_PER_ENC];
	struct sde_hw_pingpong *hw_pp[MAX_CHANNELS_PER_ENC];
	struct msm_display_vdc_info *vdc = NULL;
	enum sde_rm_topology_name topology;
	const struct sde_rect *roi;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_intf_cfg_v1 cfg;
	enum sde_3d_blend_mode mode_3d;
	bool half_panel_partial_update, merge_3d;
	bool disable_merge_3d = false;
	int this_frame_slices;
	int intf_ip_w, enc_ip_w;
	const struct sde_rm_topology_def *def;
	int num_intf, num_vdc, num_lm;
	int i;
	int ret = 0;

	if (!sde_enc || !params || !sde_enc->phys_encs[0] ||
			!sde_enc->phys_encs[0]->connector)
		return -EINVAL;

	drm_conn = sde_enc->phys_encs[0]->connector;

	topology = sde_connector_get_topology_name(drm_conn);
	if (topology == SDE_RM_TOPOLOGY_NONE) {
		SDE_ERROR_DCE(sde_enc, "topology not set yet\n");
		return -EINVAL;
	}

	SDE_DEBUG_DCE(sde_enc, "topology:%d\n", topology);
	SDE_EVT32(DRMID(&sde_enc->base), topology,
			sde_enc->cur_conn_roi.x,
			sde_enc->cur_conn_roi.y,
			sde_enc->cur_conn_roi.w,
			sde_enc->cur_conn_roi.h,
			sde_enc->prv_conn_roi.x,
			sde_enc->prv_conn_roi.y,
			sde_enc->prv_conn_roi.w,
			sde_enc->prv_conn_roi.h,
			sde_enc->cur_master->cached_mode.hdisplay,
			sde_enc->cur_master->cached_mode.vdisplay);

	if (sde_kms_rect_is_equal(&sde_enc->cur_conn_roi,
			&sde_enc->prv_conn_roi))
		return ret;

	enc_master = sde_enc->cur_master;
	roi = &sde_enc->cur_conn_roi;
	hw_ctl = enc_master->hw_ctl;
	vdc = &sde_enc->mode_info.comp_info.vdc_info;

	sde_kms = sde_encoder_get_kms(&sde_enc->base);

	def = sde_rm_topology_get_topology_def(&sde_kms->rm, topology);
	if (IS_ERR_OR_NULL(def))
		return -EINVAL;

	num_vdc = def->num_comp_enc;
	num_intf = def->num_intf;
	mode_3d = (topology == SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_VDC) ?
		BLEND_3D_H_ROW_INT : BLEND_3D_NONE;
	num_lm = def->num_lm;

	/*
	 * If this encoder is driving more than one VDC encoder, they
	 * operate in tandem, same pic dimension needs to be used by
	 * each of them.(pp-split is assumed to be not supported)
	 */
	_dce_vdc_update_pic_dim(vdc, roi->w, roi->h);
	merge_3d = (mode_3d != BLEND_3D_NONE) ? true : false;
	half_panel_partial_update = _dce_check_half_panel_update(num_lm,
		sde_enc);

	if (half_panel_partial_update && merge_3d)
		disable_merge_3d = true;

	this_frame_slices = roi->w / vdc->slice_width;
	intf_ip_w = this_frame_slices * vdc->slice_width;

	sde_vdc_populate_config(vdc, intf_ip_w, vdc->traffic_mode);

	enc_ip_w = intf_ip_w;

	SDE_DEBUG_DCE(sde_enc, "pic_w: %d pic_h: %d\n",
				roi->w, roi->h);

	for (i = 0; i < num_vdc; i++) {
		bool active = !!((1 << i) & params->affected_displays);

		/*
		 * if half_panel partial update vdc should be bound to the pp
		 * that is driving the update, in other case when both the
		 * layer mixers are driving the update, vdc should be bound
		 * to left side pp
		 */
		if (merge_3d && half_panel_partial_update)
			hw_pp[i] = (active) ? sde_enc->hw_pp[0] :
				sde_enc->hw_pp[1];
		else
			hw_pp[i] = sde_enc->hw_pp[i];
		hw_vdc[i] = sde_enc->hw_vdc[i];

		if (!hw_vdc[i]) {
			SDE_ERROR_DCE(sde_enc, "invalid params for VDC\n");
			SDE_EVT32(DRMID(&sde_enc->base), roi->w, roi->h,
				i, active);
			return -EINVAL;
		}

		_dce_vdc_pipe_cfg(hw_vdc[i], hw_pp[i],
				vdc, mode_3d, disable_merge_3d, active);

		memset(&cfg, 0, sizeof(cfg));
		cfg.vdc[cfg.vdc_count++] = hw_vdc[i]->idx;

		if (hw_ctl->ops.update_intf_cfg)
			hw_ctl->ops.update_intf_cfg(hw_ctl,
					&cfg,
					active);

		if (hw_ctl->ops.update_bitmask)
			hw_ctl->ops.update_bitmask(hw_ctl,
					SDE_HW_FLUSH_VDC,
					hw_vdc[i]->idx, active);

		SDE_DEBUG_DCE(sde_enc,
				"update_intf_cfg hw_ctl[%d], vdc:%d, %s",
				hw_ctl->idx,
				cfg.vdc[0],
				active ? "enabled" : "disabled");

		if (mode_3d) {
			memset(&cfg, 0, sizeof(cfg));

			cfg.merge_3d[cfg.merge_3d_count++] =
				hw_pp[i]->merge_3d->idx;

			if (hw_ctl->ops.update_intf_cfg)
				hw_ctl->ops.update_intf_cfg(hw_ctl,
						&cfg,
						!disable_merge_3d);

			if (hw_ctl->ops.update_bitmask)
				hw_ctl->ops.update_bitmask(
						hw_ctl, SDE_HW_FLUSH_MERGE_3D,
						hw_pp[i]->merge_3d->idx, true);

			SDE_DEBUG("mode_3d %s, on CTL_%d PP-%d merge3d:%d\n",
					disable_merge_3d ?
					"disabled" : "enabled",
					hw_ctl->idx - CTL_0,
					hw_pp[i]->idx - PINGPONG_0,
					hw_pp[i]->merge_3d ?
					hw_pp[i]->merge_3d->idx - MERGE_3D_0 :
					-1);
		}
	}

	return 0;
}

static void _dce_dsc_disable(struct sde_encoder_virt *sde_enc)
{
	int i;
	struct sde_hw_pingpong *hw_pp = NULL;
	struct sde_hw_pingpong *hw_dsc_pp = NULL;
	struct sde_hw_dsc *hw_dsc = NULL;
	struct sde_hw_ctl *hw_ctl = NULL;
	struct sde_hw_intf_cfg_v1 cfg;

	if (!sde_enc || !sde_enc->phys_encs[0]) {
		SDE_ERROR("invalid params %d %d\n",
			!sde_enc, sde_enc ? !sde_enc->phys_encs[0] : -1);
		return;
	}

	/*
	 * Connector can be null if the first virt modeset after suspend
	 * is called with dynamic clock or dms enabled.
	 */
	if (!sde_enc->phys_encs[0]->connector)
		return;

	if (sde_enc->cur_master)
		hw_ctl = sde_enc->cur_master->hw_ctl;

	memset(&cfg, 0, sizeof(cfg));

	/* Disable DSC for all the pp's present in this topology */
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		hw_pp = sde_enc->hw_pp[i];
		hw_dsc = sde_enc->hw_dsc[i];
		hw_dsc_pp = sde_enc->hw_dsc_pp[i];

		_dce_dsc_pipe_cfg(hw_dsc, hw_pp, NULL,
					0, 0, hw_dsc_pp,
					BLEND_3D_NONE, false, false, false);

		if (hw_dsc) {
			sde_enc->dirty_dsc_ids[i] = hw_dsc->idx;
			cfg.dsc[cfg.dsc_count++] = hw_dsc->idx;
		}
	}

	/* Clear the DSC ACTIVE config for this CTL */
	if (hw_ctl && hw_ctl->ops.update_intf_cfg)
		hw_ctl->ops.update_intf_cfg(hw_ctl, &cfg, false);

	/**
	 * Since pending flushes from previous commit get cleared
	 * sometime after this point, setting DSC flush bits now
	 * will have no effect. Therefore dirty_dsc_ids track which
	 * DSC blocks must be flushed for the next trigger.
	 */
}


static void _dce_vdc_disable(struct sde_encoder_virt *sde_enc)
{
	int i;
	struct sde_hw_pingpong *hw_pp = NULL;
	struct sde_hw_vdc *hw_vdc = NULL;
	struct sde_hw_ctl *hw_ctl = NULL;
	struct sde_hw_intf_cfg_v1 cfg;

	if (!sde_enc || !sde_enc->phys_encs[0] ||
			!sde_enc->phys_encs[0]->connector) {
		SDE_ERROR("invalid params %d %d\n",
			!sde_enc, sde_enc ? !sde_enc->phys_encs[0] : -1);
		return;
	}

	if (sde_enc->cur_master)
		hw_ctl = sde_enc->cur_master->hw_ctl;

	memset(&cfg, 0, sizeof(cfg));

	/* Disable VDC for all the pp's present in this topology */
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		hw_pp = sde_enc->hw_pp[i];
		hw_vdc = sde_enc->hw_vdc[i];

		_dce_vdc_pipe_cfg(hw_vdc, hw_pp, NULL,
						BLEND_3D_NONE, false,
						false);

		if (hw_vdc) {
			sde_enc->dirty_vdc_ids[i] = hw_vdc->idx;
			cfg.vdc[cfg.vdc_count++] = hw_vdc->idx;
		}
	}

	/* Clear the VDC ACTIVE config for this CTL */
	if (hw_ctl && hw_ctl->ops.update_intf_cfg)
		hw_ctl->ops.update_intf_cfg(hw_ctl, &cfg, false);

	/**
	 * Since pending flushes from previous commit get cleared
	 * sometime after this point, setting VDC flush bits now
	 * will have no effect. Therefore dirty_vdc_ids track which
	 * VDC blocks must be flushed for the next trigger.
	 */
}

bool _dce_dsc_is_dirty(struct sde_encoder_virt *sde_enc)
{
	int i;

	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		/**
		 * This dirty_dsc_hw field is set during DSC disable to
		 * indicate which DSC blocks need to be flushed
		 */
		if (sde_enc->dirty_dsc_ids[i])
			return true;
	}

	return false;
}

bool _dce_vdc_is_dirty(struct sde_encoder_virt *sde_enc)
{
	int i;

	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		/**
		 * This dirty_vdc_hw field is set during VDC disable to
		 * indicate which VDC blocks need to be flushed
		 */
		if (sde_enc->dirty_vdc_ids[i])
			return true;
	}

	return false;
}

static void _dce_helper_flush_dsc(struct sde_encoder_virt *sde_enc)
{
	int i;
	struct sde_hw_ctl *hw_ctl = NULL;
	enum sde_dsc dsc_idx;

	if (sde_enc->cur_master)
		hw_ctl = sde_enc->cur_master->hw_ctl;

	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		dsc_idx = sde_enc->dirty_dsc_ids[i];
		if (dsc_idx && hw_ctl && hw_ctl->ops.update_bitmask)
			hw_ctl->ops.update_bitmask(hw_ctl, SDE_HW_FLUSH_DSC,
					dsc_idx, 1);

		sde_enc->dirty_dsc_ids[i] = DSC_NONE;
	}
}

void _dce_helper_flush_vdc(struct sde_encoder_virt *sde_enc)
{
	int i;
	struct sde_hw_ctl *hw_ctl = NULL;
	enum sde_vdc vdc_idx;

	if (sde_enc->cur_master)
		hw_ctl = sde_enc->cur_master->hw_ctl;

	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		vdc_idx = sde_enc->dirty_vdc_ids[i];
		if (vdc_idx && hw_ctl && hw_ctl->ops.update_bitmask)
			hw_ctl->ops.update_bitmask(hw_ctl, SDE_HW_FLUSH_VDC,
					vdc_idx, 1);

		sde_enc->dirty_vdc_ids[i] = VDC_NONE;
	}
}

void sde_encoder_dce_set_bpp(struct msm_mode_info mode_info,
		struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	enum msm_display_compression_type comp_type;
	int src_bpp, target_bpp;

	if (!sde_crtc) {
		SDE_DEBUG("invalid sde_crtc\n");
		return;
	}

	comp_type = mode_info.comp_info.comp_type;
	/**
	 * In cases where DSC or VDC compression type is not found, set
	 * src and target bpp to get compression ratio 8/8 (default).
	 */
	if (comp_type == MSM_DISPLAY_COMPRESSION_DSC) {
		struct msm_display_dsc_info dsc_info =
				mode_info.comp_info.dsc_info;
		src_bpp = msm_get_src_bpc(dsc_info.chroma_format,
				dsc_info.config.bits_per_component);
		target_bpp = dsc_info.config.bits_per_pixel >> 4;
	} else if (comp_type == MSM_DISPLAY_COMPRESSION_VDC) {
		struct msm_display_vdc_info vdc_info =
				mode_info.comp_info.vdc_info;
		src_bpp = msm_get_src_bpc(vdc_info.chroma_format,
				vdc_info.bits_per_component);
		target_bpp = vdc_info.bits_per_pixel >> 4;
	} else {
		src_bpp = 8;
		target_bpp = 8;
	}

	sde_crtc_set_bpp(sde_crtc, src_bpp, target_bpp);

	SDE_DEBUG("sde_crtc src_bpp = %d, target_bpp = %d\n",
			sde_crtc->src_bpp, sde_crtc->target_bpp);
}

void sde_encoder_dce_disable(struct sde_encoder_virt *sde_enc)
{
	enum msm_display_compression_type comp_type;

	if (!sde_enc)
		return;

	comp_type = sde_enc->mode_info.comp_info.comp_type;

	if (comp_type == MSM_DISPLAY_COMPRESSION_DSC)
		_dce_dsc_disable(sde_enc);
	else if (comp_type == MSM_DISPLAY_COMPRESSION_VDC)
		_dce_vdc_disable(sde_enc);
}

int sde_encoder_dce_flush(struct sde_encoder_virt *sde_enc)
{
	int rc = 0;

	if (!sde_enc)
		return -EINVAL;

	if (_dce_dsc_is_dirty(sde_enc))
		_dce_helper_flush_dsc(sde_enc);
	else if (_dce_vdc_is_dirty(sde_enc))
		_dce_helper_flush_vdc(sde_enc);

	return rc;
}

int sde_encoder_dce_setup(struct sde_encoder_virt *sde_enc,
		struct sde_encoder_kickoff_params *params)
{
	enum msm_display_compression_type comp_type;
	int rc = 0;

	if (!sde_enc)
		return -EINVAL;

	comp_type = sde_enc->mode_info.comp_info.comp_type;

	if (comp_type == MSM_DISPLAY_COMPRESSION_DSC)
		rc = _dce_dsc_setup(sde_enc, params);
	else if (comp_type == MSM_DISPLAY_COMPRESSION_VDC)
		rc = _dce_vdc_setup(sde_enc, params);

	return rc;
}
