/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef MTK_DRM_DDP_ADDON_H
#define MTK_DRM_DDP_ADDON_H

#include <drm/drm_crtc.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include "mtk_rect.h"
#include "mtk_layering_rule.h"

enum addon_scenario {
	NONE,
	ONE_SCALING,
	TWO_SCALING,
	GAME_PQ,
	VP_PQ,
	TRIPLE_DISP,
	ADDON_SCN_NR,
};

enum addon_module {
	DISP_RSZ,
	DISP_RSZ_v2,
	DMDP_PQ_WITH_RDMA,
	ADDON_MODULE_NUM,
};

enum addon_type {
	ADDON_BETWEEN,
	ADDON_BEFORE,
};

struct mtk_lye_ddp_state {
	 enum addon_scenario scn[HRT_TYPE_NUM];
	 uint8_t lc_tgt_layer;
	 uint8_t dmdp_tgt_layer;
};

struct mtk_addon_path_data {
	const int *path;
	unsigned int path_len;
};

struct mtk_addon_module_data {
	enum addon_module module;
	enum addon_type type;
	int attach_comp;
};

struct mtk_addon_scenario_data {
	unsigned int module_num;
	const struct mtk_addon_module_data *module_data;
	enum HRT_TB_TYPE hrt_type;
};

struct mtk_addon_config_type {
	enum addon_module module;
	enum addon_type type;
};

struct mtk_addon_rsz_config {
	struct mtk_addon_config_type config_type;
	struct mtk_rect rsz_src_roi;
	struct mtk_rect rsz_dst_roi;
	uint8_t lc_tgt_layer;
};

union mtk_addon_config {
	struct mtk_addon_config_type config_type;
	struct mtk_addon_rsz_config addon_rsz_config;
};

const struct mtk_addon_path_data *
mtk_addon_module_get_path(enum addon_module module);
const struct mtk_addon_scenario_data *
mtk_addon_get_scenario_data(const char *source, struct drm_crtc *crtc,
			    enum addon_scenario scn);
bool mtk_addon_scenario_support(struct drm_crtc *crtc, enum addon_scenario scn);
void mtk_addon_connect_between(struct drm_crtc *crtc, unsigned int ddp_mode,
			       const struct mtk_addon_module_data *module_data,
			       union mtk_addon_config *addon_config,
			       struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_between(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);
void mtk_addon_connect_before(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_before(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);
#endif
