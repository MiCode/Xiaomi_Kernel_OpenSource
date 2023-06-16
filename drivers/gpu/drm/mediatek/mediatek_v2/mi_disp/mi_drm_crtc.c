// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt) "mi_drm_crtc:[%s:%d] " fmt, __func__, __LINE__

#include <drm/drm_crtc.h>
#include <linux/completion.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_trace.h"
#include "mtk_debug.h"

#include "mi_drm_crtc.h"
#include "mi_dsi_panel.h"
#include "mi_dsi_display.h"
#include "mi_disp_print.h"

#define MI_TRACE_LAYER_ID (DRM_TRACE_VSYNC_ID + 1)

static bool is_backlight_set_block(struct mtk_dsi *dsi, u32 bl_lvl)
{
	if (!dsi->mi_cfg.bl_enable && bl_lvl) {
		DISP_INFO("%s true lvl= %d bl_enable= %d\n", __func__, bl_lvl, dsi->mi_cfg.bl_enable);
		return true;
	}
	return false;
}

int mi_drm_bl_wait_for_completion(struct drm_crtc *crtc, unsigned int level)
{
	int ret = 0;

	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *ext = NULL;

	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		DISP_ERROR("invalid comp_output\n");
		return -EINVAL;
	}

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	if (!dsi) {
		DISP_ERROR("invalid display ptr\n");
		return -EINVAL;
	}

	ext = mtk_dsi_get_panel_ext(comp);
	if (ext && ext->params && !ext->params->bl_sync_enable)
		return 0;

	if (is_backlight_set_block(dsi, level)) {
		mtk_drm_trace_begin("mi_drm_bl_wait_for_completion-%d", level);

		reinit_completion(&dsi->bl_wait_completion);
		ret = wait_for_completion_timeout(&dsi->bl_wait_completion, msecs_to_jiffies(300));
		DISP_INFO("bl_wait_for_completion_timeout return %d\n", ret);

 		mtk_drm_trace_end();
	}

	return ret;
}

static int mi_drm_update_aod_status(struct mtk_dsi *dsi, bool doze_state, bool is_aod_exit)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct mtk_panel_ext *ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid display ptr\n");
		return -EINVAL;
	}

	mi_cfg = &dsi->mi_cfg;
	ext = dsi->ext;

	if (ext && ext->params && !ext->params->bl_sync_enable)
		return 0;

	if (is_aod_exit && !doze_state) {
		if (!mi_cfg->bl_enable && !completion_done(&dsi->bl_wait_completion)) {
			if (mi_cfg->bl_wait_frame) {
				/* complete bl_wait_completion to unblock backlight */
				complete(&dsi->bl_wait_completion);
				mi_cfg->bl_enable = true;
				mi_cfg->bl_wait_frame = false;
				DISP_INFO("bl_wait_completion complete\n");
			} else {
				mi_cfg->bl_wait_frame = true;
			}
		}
	}

	return 0;
}

int mi_drm_crtc_update_layer_state(struct drm_crtc *crtc)
{
	int ret = 0;
	bool doze_state = 0;
	uint32_t mi_layer_flags;
	struct mi_layer_flags cur_flags;
	struct mi_layer_flags *last_flags = NULL;

	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc_state);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_dsi *dsi = NULL;

	int crtc_id = drm_crtc_index(crtc);
	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		DISP_ERROR("invalid comp_output\n");
		return -EINVAL;
	}

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	if (!dsi) {
		DISP_ERROR("invalid display ptr\n");
		return -EINVAL;
	}

	last_flags = &dsi->mi_layer_state.layer_flags;
	mi_layer_flags = (uint32_t)state->prop_val[CRTC_PROP_MI_FOD_SYNC_INFO];
	doze_state = state->prop_val[CRTC_PROP_DOZE_ACTIVE];

	cur_flags.aod_flag = (mi_layer_flags & MI_LAYER_AOD) ? true : false;
	mtk_drm_trace_c("%d|MI_LAYER_AOD|%d", MI_TRACE_LAYER_ID, cur_flags.aod_flag);

	if (cur_flags.aod_flag != last_flags->aod_flag)
		DISP_INFO("layer AOD = %d\n", cur_flags.aod_flag);

	if (crtc_id == 0)
		mi_drm_update_aod_status(dsi, doze_state, !cur_flags.aod_flag);

	dsi->mi_layer_state.layer_flags = cur_flags;

	return ret;
}

