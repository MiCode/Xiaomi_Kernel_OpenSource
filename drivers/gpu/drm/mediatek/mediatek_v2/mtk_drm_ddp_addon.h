/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DDP_ADDON_H
#define MTK_DRM_DDP_ADDON_H

#include <drm/drm_crtc.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_rect.h"
#include "mtk_layering_rule.h"

#include "../mml/mtk-mml.h"
#include "../mml/mtk-mml-drm.h"

#define DISP_PIPE_NUM 2

enum addon_scenario {
	NONE,
	ONE_SCALING,
	TWO_SCALING,
	WDMA_WRITE_BACK,
	WDMA_WRITE_BACK_OVL,
	GAME_PQ,
	VP_PQ,
	TRIPLE_DISP,
	MML_WITH_PQ,
	MML,
	MML_SRAM_ONLY,
	ADDON_SCN_NR,
};

enum addon_module {
	DISP_RSZ,
	DISP_RSZ_v2,
	DISP_RSZ_v3,
	DISP_RSZ_v4,
	DISP_RSZ_v5,
	DISP_RSZ_v6,
	DISP_WDMA0,
	DISP_WDMA0_v2,
	DISP_WDMA1,
	DISP_WDMA2,
	DISP_WDMA2_v2,
	MML_RSZ,
	MML_RSZ_v2,
	DMDP_PQ_WITH_RDMA,
	DISP_INLINE_ROTATE,
	DISP_INLINE_ROTATE_1,
	DISP_INLINE_ROTATE_SRAM_ONLY,
	DISP_INLINE_ROTATE_SRAM_ONLY_1,
	ADDON_MODULE_NUM,
};

enum addon_type {
	ADDON_BETWEEN,
	ADDON_BEFORE,
	ADDON_AFTER,
	ADDON_CONNECT,
	ADDON_DISCONNECT,
};

struct mtk_lye_ddp_state {
	enum addon_scenario scn[HRT_DISP_TYPE_NUM];
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

struct mtk_rsz_param {
	u32 in_x;
	u32 out_x;
	u32 step;
	u32 int_offset;
	u32 sub_offset;
	u32 in_len;
	u32 out_len;
};

struct mtk_addon_rsz_config {
	struct mtk_addon_config_type config_type;
	struct mtk_rect rsz_src_roi;
	struct mtk_rect rsz_dst_roi;
	struct mtk_rsz_param rsz_param;
	uint8_t lc_tgt_layer;
};

struct mtk_addon_wdma_config {
	struct mtk_addon_config_type config_type;
	struct mtk_rect wdma_src_roi;
	struct mtk_rect wdma_dst_roi;
	int pitch;
	dma_addr_t addr;
	struct drm_framebuffer *fb;
	struct golden_setting_context *p_golden_setting_context;
};

struct mtk_addon_mml_config {
	struct mtk_addon_config_type config_type;
	struct mml_mutex_ctl mutex;	/* [IN] display mode and output port */
	struct mml_submit submit;	/* [IN] mml_drm_split_info submit_pq */
	bool dual;			/* [IN] set true if display uses dual pipe */
	struct mml_task *task;		/* [OUT] task and config for mml */
	struct mtk_rect mml_src_roi[DISP_PIPE_NUM];	/* [OUT] src roi for OVL */
	struct mtk_rect mml_dst_roi[DISP_PIPE_NUM];	/* [OUT] dst roi for OVL */
	bool is_entering;				/* [OUT] state of entering or leaving */
};

union mtk_addon_config {
	struct mtk_addon_config_type config_type;
	struct mtk_addon_rsz_config addon_rsz_config;
	struct mtk_addon_wdma_config addon_wdma_config;
	struct mtk_addon_mml_config addon_mml_config;
};

const struct mtk_addon_path_data *
mtk_addon_module_get_path(enum addon_module module);
const struct mtk_addon_scenario_data *
mtk_addon_get_scenario_data(const char *source, struct drm_crtc *crtc,
			    enum addon_scenario scn);
const struct mtk_addon_scenario_data *
mtk_addon_get_scenario_data_dual(const char *source, struct drm_crtc *crtc,
			    enum addon_scenario scn);
bool mtk_addon_scenario_support(struct drm_crtc *crtc, enum addon_scenario scn);
void mtk_addon_path_config(struct drm_crtc *crtc,
			const struct mtk_addon_module_data *module_data,
			union mtk_addon_config *addon_config,
			struct cmdq_pkt *cmdq_handle);

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
void mtk_addon_connect_after(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_after(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);

void mtk_addon_connect_external(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_external(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);

#endif
