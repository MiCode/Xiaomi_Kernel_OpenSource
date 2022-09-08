/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_PM_PLAT_H_
#define _MTK_VCODEC_PM_PLAT_H_

#include "mtk_vcodec_drv.h"

void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev);

void set_venc_opp(struct mtk_vcodec_dev *dev, u32 freq);
void set_vdec_opp(struct mtk_vcodec_dev *dev, u32 freq);
void set_venc_bw(struct mtk_vcodec_dev *dev, u64 bw);
void set_vdec_bw(struct mtk_vcodec_dev *dev, u64 bw);

void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);

#endif /* _MTK_VCODEC_ENC_PM_H_ */
