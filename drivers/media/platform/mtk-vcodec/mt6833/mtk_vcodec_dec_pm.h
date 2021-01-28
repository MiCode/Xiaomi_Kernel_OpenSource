/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
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

#ifndef _MTK_VCODEC_DEC_PM_H_
#define _MTK_VCODEC_DEC_PM_H_

#include "mtk_vcodec_drv.h"

#define DEC_DVFS	1
#define DEC_EMI_BW	1
#define VIDEO_USE_IOVA

void mtk_dec_init_ctx_pm(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_init_dec_pm(struct mtk_vcodec_dev *dev);
void mtk_vcodec_release_dec_pm(struct mtk_vcodec_dev *dev);

void mtk_vcodec_dec_pw_on(struct mtk_vcodec_pm *pm, int hw_id);
void mtk_vcodec_dec_pw_off(struct mtk_vcodec_pm *pm, int hw_id);
void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm, int hw_id);
void mtk_vcodec_dec_clock_off(struct mtk_vcodec_pm *pm, int hw_id);
void mtk_vdec_hw_break(struct mtk_vcodec_dev *dev, int hw_id);

void mtk_prepare_vdec_dvfs(void);
void mtk_unprepare_vdec_dvfs(void);
void mtk_prepare_vdec_emi_bw(void);
void mtk_unprepare_vdec_emi_bw(void);

void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id);

#endif /* _MTK_VCODEC_DEC_PM_H_ */
