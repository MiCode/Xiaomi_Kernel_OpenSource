/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_ENC_PM_PLAT_H_
#define _MTK_VCODEC_ENC_PM_PLAT_H_

#include "mtk_vcodec_drv.h"

#define ENC_DVFS	1
#define ENC_EMI_BW	1

void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev);

void mtk_venc_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_dvfs_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_pmqos_end_inst(struct mtk_vcodec_ctx *ctx);
#endif /* _MTK_VCODEC_ENC_PM_H_ */
