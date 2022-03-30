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

void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);

void mtk_vdec_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_dvfs_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx);
#endif /* _MTK_VCODEC_DEC_PM_PLAT_H_ */
