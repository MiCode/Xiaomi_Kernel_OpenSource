/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_DEC_PM_PLAT_H_
#define _MTK_VCODEC_DEC_PM_PLAT_H_

#include "mtk_vcodec_drv.h"

#define DEC_DVFS	1
#define DEC_EMI_BW	1
#define MTK_VDEC_CHECK_ACTIVE_INTERVAL 2000 // ms

#ifdef DEC_DVFS
#define VDEC_CHECK_ALIVE 1 /* vdec check alive have to enable DEC_DVFS first */
#endif

void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_vdec_force_update_freq(struct mtk_vcodec_dev *dev);

void mtk_vdec_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_dvfs_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_prepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, unsigned long *in);
void mtk_vdec_unprepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, unsigned long *in);
void mtk_vdec_dvfs_sync_vsi_data(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_update_dvfs_params(struct mtk_vcodec_ctx *ctx);
bool mtk_vdec_dvfs_is_pw_always_on(struct mtk_vcodec_dev *dev);
#endif /* _MTK_VCODEC_DEC_PM_PLAT_H_ */
