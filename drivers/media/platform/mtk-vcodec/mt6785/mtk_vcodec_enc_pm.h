/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_VCODEC_ENC_PM_H_
#define _MTK_VCODEC_ENC_PM_H_

#include "mtk_vcodec_drv.h"

#define ENC_DVFS	1
#define ENC_EMI_BW	1

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_init_enc_pm(struct mtk_vcodec_dev *dev);
void mtk_vcodec_release_enc_pm(struct mtk_vcodec_dev *dev);
void mtk_venc_deinit_ctx_pm(struct mtk_vcodec_ctx *ctx);

void mtk_vcodec_enc_clock_on(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id);

void mtk_prepare_venc_dvfs(void);
void mtk_unprepare_venc_dvfs(void);
void mtk_prepare_venc_emi_bw(void);
void mtk_unprepare_venc_emi_bw(void);

void mtk_venc_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_venc_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_venc_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int core_id);
/* GCE version PMQoS */
void mtk_venc_pmqos_gce_flush(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt);
void mtk_venc_pmqos_gce_done(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt);

#endif /* _MTK_VCODEC_ENC_PM_H_ */
