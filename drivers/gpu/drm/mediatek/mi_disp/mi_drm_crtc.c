// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt) "mi_drm_crtc:[%s:%d] " fmt, __func__, __LINE__
#if defined (CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) || defined (CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
#include <drm/drm_crtc.h>
#include <linux/completion.h>
#include "mi_drm_crtc.h"
#include "mi_dsi_panel.h"
#include "mi_dsi_display.h"
#include "mi_disp_print.h"
#include "mtk_drm_crtc.h"
#include "mtk_debug.h"

int mi_drm_crtc_fod_hbm_fence(struct drm_crtc *crtc)
{
	int ret = 0;
	int crtc_id = 0;
	uint32_t fod_sync_info;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc_state);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_dsi *dsi = NULL;

	if (!(comp && comp->funcs && comp->funcs->io_cmd)) {
		DISP_ERROR("invalid comp_output\n");
		return -EINVAL;
	}

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	if (!dsi) {
		DISP_ERROR("invalid display ptr\n");
		return -EINVAL;
	}

	mi_cfg = &dsi->mi_cfg;
	fod_sync_info = (uint32_t)state->prop_val[CRTC_PROP_MI_FOD_SYNC_INFO];

	if (!(fod_sync_info & MI_DIMLAYER_AOD)) {
		if (mi_cfg->feature_val[DISP_FEATURE_DOZE_STATE] == DOZE_TO_NORMAL) {
			mi_cfg->bl_enable = true;
			crtc_id = drm_crtc_index(crtc);
		}
		if (!mi_cfg->bl_wait_frame && mi_cfg->bl_enable && crtc_id == 0 &&
			mi_cfg->feature_val[DISP_FEATURE_DOZE_STATE] == DOZE_TO_NORMAL) {
			/* set backlight */
			complete(&dsi->bl_wait_completion);
			DISP_INFO("bl_wait_completion complete\n");
		}
	}
	return ret;
}
#endif
