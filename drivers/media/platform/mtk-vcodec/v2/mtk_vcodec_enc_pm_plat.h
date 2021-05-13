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

/* Frame based PMQoS */
void mtk_venc_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_venc_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_venc_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int core_id);

/* GCE version PMQoS */
void mtk_venc_pmqos_gce_flush(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt);
void mtk_venc_pmqos_gce_done(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt);

#endif /* _MTK_VCODEC_ENC_PM_H_ */
