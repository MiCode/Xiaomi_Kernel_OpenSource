/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_ENC_PM_H_
#define _MTK_VCODEC_ENC_PM_H_

#include "mtk_vcodec_drv.h"

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_init_enc_pm(struct mtk_vcodec_dev *dev);
void mtk_vcodec_release_enc_pm(struct mtk_vcodec_dev *dev);
void mtk_venc_deinit_ctx_pm(struct mtk_vcodec_ctx *ctx);

void mtk_vcodec_enc_clock_on(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id);

void mtk_venc_translation_fault_callback_setting(struct mtk_vcodec_dev *dev);

extern void mtk_venc_do_gettimeofday(struct timespec64 *tv);

#endif /* _MTK_VCODEC_ENC_PM_H_ */
