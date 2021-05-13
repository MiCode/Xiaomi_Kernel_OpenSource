/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_DRM_DDP_H
#define MTK_DRM_DDP_H

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include <linux/soc/mediatek/mtk-cmdq.h>

#define DISP_MUTEX_TOTAL (16)
#define DISP_MUTEX_DDP_FIRST (0)
#define DISP_MUTEX_DDP_LAST (5)
#define DISP_MUTEX_DDP_COUNT (DISP_MUTEX_DDP_LAST - DISP_MUTEX_DDP_FIRST + 1)
#define __DISP_MUTEX_INT_MSK ((1 << (DISP_MUTEX_DDP_COUNT)) - 1)
#define DISP_MUTEX_INT_MSK                                                     \
	((__DISP_MUTEX_INT_MSK << DISP_MUTEX_TOTAL) | __DISP_MUTEX_INT_MSK)

struct regmap;
struct device;
struct mtk_disp_mutex;
struct mtk_mmsys_reg_data;

const struct mtk_mmsys_reg_data *
mtk_ddp_get_mmsys_reg_data(enum mtk_mmsys_id mmsys_id);

void mtk_disp_ultra_offset(void __iomem *config_regs,
			enum mtk_ddp_comp_id comp, bool is_dc);
void mtk_ddp_add_comp_to_path(struct mtk_drm_crtc *mtk_crtc,
			      struct mtk_ddp_comp *comp,
			      enum mtk_ddp_comp_id prev,
			      enum mtk_ddp_comp_id next);
void mtk_ddp_add_comp_to_path_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					enum mtk_ddp_comp_id cur,
					enum mtk_ddp_comp_id next,
					struct cmdq_pkt *handle);
void mtk_ddp_remove_comp_from_path(void __iomem *config_regs,
				   const struct mtk_mmsys_reg_data *reg_data,
				   enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next);
void mtk_ddp_remove_comp_from_path_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					     enum mtk_ddp_comp_id cur,
					     enum mtk_ddp_comp_id next,
					     struct cmdq_pkt *handle);

struct mtk_disp_mutex *mtk_disp_mutex_get(struct device *dev, unsigned int id);
int mtk_disp_mutex_prepare(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_add_comp(struct mtk_disp_mutex *mutex,
			     enum mtk_ddp_comp_id id);
void mtk_disp_mutex_add_comp_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
				       enum mtk_ddp_comp_id id, bool is_cmd_mod,
				       struct cmdq_pkt *handle,
				       unsigned int mutex_id);
void mtk_disp_mutex_enable(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_disable(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_remove_comp(struct mtk_disp_mutex *mutex,
				enum mtk_ddp_comp_id id);
void mtk_disp_mutex_remove_comp_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					  enum mtk_ddp_comp_id id,
					  struct cmdq_pkt *handle,
					  unsigned int mutex_id);
void mtk_disp_mutex_unprepare(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_put(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_acquire(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_release(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_trigger(struct mtk_disp_mutex *mutex, void *handle);

void mtk_disp_mutex_enable_cmdq(struct mtk_disp_mutex *mutex,
				struct cmdq_pkt *cmdq_handle,
				struct cmdq_base *cmdq_base);
void mtk_disp_mutex_src_set(struct mtk_drm_crtc *mtk_crtc, bool is_cmd_mode);
void mtk_disp_mutex_inten_enable_cmdq(struct mtk_disp_mutex *mutex,
				      void *handle);
void mtk_disp_mutex_inten_disable_cmdq(struct mtk_disp_mutex *mutex,
				       void *handle);

void mutex_dump_reg_mt6885(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6885(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_reg_mt6885(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6885(void __iomem *config_regs);
void mtk_ddp_insert_dsc_prim_MT6885(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6885(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_connect_dual_pipe_path(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_submit_sof(struct mtk_disp_mutex *mutex);
void mtk_ddp_dual_pipe_dump(struct mtk_drm_crtc *mtk_crtc);

void mutex_dump_reg_mt6873(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6873(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_reg_mt6873(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6873(void __iomem *config_regs);

void mtk_ddp_insert_dsc_prim_MT6873(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6873(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mmsys_config_dump_analysis_mt6853(void __iomem *config_regs);
void mutex_dump_analysis_mt6853(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6877(void __iomem *config_regs);
void mutex_dump_analysis_mt6877(struct mtk_disp_mutex *mutex);

void mtk_ddp_insert_dsc_prim_MT6853(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6853(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mmsys_config_dump_analysis_mt6833(void __iomem *config_regs);
void mutex_dump_analysis_mt6833(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6781(void __iomem *config_regs);
void mutex_dump_analysis_mt6781(struct mtk_disp_mutex *mutex);

void mtk_ddp_insert_dsc_prim_MT6781(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6781(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

#endif /* MTK_DRM_DDP_H */
