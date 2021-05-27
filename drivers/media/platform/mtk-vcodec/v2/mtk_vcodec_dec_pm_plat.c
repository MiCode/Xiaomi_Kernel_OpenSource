// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <soc/mediatek/smi.h>
#include <linux/slab.h>
//#include "smi_public.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#if DEC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#define STD_VDEC_FREQ 249
#endif

#define VDEC_DRV_UFO_ON (1 << 0)
#define VDEC_DRV_UFO_AUO_ON (1 << 1)
#if DEC_EMI_BW
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
/* LARB4, mostly core */
/* LARB5, mostly lat */
#endif

void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
#if DEC_DVFS
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		pr_debug("Failed to get opp table (%d)\n", ret);
		return;
	}

	dev->vdec_reg = devm_regulator_get(&dev->plat_dev->dev,
						"dvfsrc-vcore");
	if (dev->vdec_reg == 0) {
		pr_debug("Failed to get regulator\n");
		return;
	}

	dev->vdec_freq_cnt = dev_pm_opp_get_opp_count(&dev->plat_dev->dev);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq))) {
		dev->vdec_freqs[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
#endif
}

void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
	/* Set to lowest clock before leaving */
}

void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
	int i = 0;
	struct platform_device *pdev = 0;

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VDEC_PORT_NUM; i++)
		dev->vdec_qos_req[i] = 0;

	i = 0;
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_mc");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_ufo");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_pp");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_pred_rd");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_pred_wr");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_ppwrap");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_tile");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_vld");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_vld2");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_avc_mv");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev,
					"path_vdec_rg_ctrl_dma");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_vdec_ufo_enc");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_lat0_vld");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_lat0_vld2");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_lat0_avc_mv");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_lat0_pred_rd");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_lat0_tile");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_lat0_wdma");
	dev->vdec_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_lat0_rg_ctrl");
#endif
}

void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
#endif
}

void mtk_vdec_dvfs_begin(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS

	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	if ((ctx->q_data[MTK_Q_DATA_DST].coded_width *
		ctx->q_data[MTK_Q_DATA_DST].coded_height) >=
		3840*2160) {
		ctx->dev->vdec_freq.freq[hw_id] = 416000000UL;
	} else {
		ctx->dev->vdec_freq.freq[hw_id] = 249000000UL;
	}

	if (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP8)
		ctx->dev->vdec_freq.freq[hw_id] = 416000000UL;

	if (ctx->dev->dec_cnt > 1 ||
		ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_HEIF)
		ctx->dev->vdec_freq.freq[hw_id] = 546000000UL;

	ctx->dev->vdec_freq.active_freq =
		ctx->dev->vdec_freq.freq[0] > ctx->dev->vdec_freq.freq[1] ?
		ctx->dev->vdec_freq.freq[0] : ctx->dev->vdec_freq.freq[1];

	if (ctx->dev->vdec_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
					&ctx->dev->vdec_freq.active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(ctx->dev->vdec_reg, volt, INT_MAX);
		if (ret) {
			pr_debug("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
	}

	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_dvfs_end(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);

	ctx->dev->vdec_freq.freq[hw_id] = 249000000UL;
	ctx->dev->vdec_freq.active_freq =
		ctx->dev->vdec_freq.freq[0] > ctx->dev->vdec_freq.freq[1] ?
		ctx->dev->vdec_freq.freq[0] : ctx->dev->vdec_freq.freq[1];

	if (ctx->dev->vdec_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
					&ctx->dev->vdec_freq.active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(ctx->dev->vdec_reg, volt, INT_MAX);
		if (ret) {
			pr_debug("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
	}
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_emi_bw_begin(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_EMI_BW
	long emi_bw = 0;
	long emi_bw_input = 0;
	long emi_bw_output = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;
	if (hw_id == MTK_VDEC_LAT) {
		switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_H265:
			emi_bw_input = 35L * pdev->vdec_freq.active_freq /
					1000000 / STD_VDEC_FREQ;
			break;
		case V4L2_PIX_FMT_VP9:
		case V4L2_PIX_FMT_AV1:
			emi_bw_input = 15L * pdev->vdec_freq.active_freq /
					1000000 / STD_VDEC_FREQ;
			break;
		default:
			emi_bw_input = 35L * pdev->vdec_freq.active_freq /
					1000000 / STD_VDEC_FREQ;
		}

		if (pdev->vdec_qos_req[0] != 0) {
			mtk_icc_set_bw(pdev->vdec_qos_req[12],
					MBps_to_icc(emi_bw_input), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[13], MBps_to_icc(0), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[14],
					MBps_to_icc(emi_bw_input * 2), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[15], MBps_to_icc(10), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[16], MBps_to_icc(0), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[17],
					MBps_to_icc(emi_bw_input * 2), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[18], MBps_to_icc(0), 0);
		}
	} else if (hw_id == MTK_VDEC_CORE) {
		emi_bw = 8L * 1920 * 1080 * 9 * 10 * 5 *
			pdev->vdec_freq.active_freq / 1000000 / 2 / 3;
		emi_bw_output = 1920L * 1088 * 9 * 30 * 10 * 5 *
				pdev->vdec_freq.active_freq / 1000000 /
				4 / 3 / 3 / STD_VDEC_FREQ / 1024 / 1024;

		switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
		case V4L2_PIX_FMT_H264:
			emi_bw_input = 70L * pdev->vdec_freq.active_freq /
					1000000 / STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_H265:
			emi_bw_input = 70L * pdev->vdec_freq.active_freq /
					1000000 / STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_VP8:
			emi_bw_input = 15L * pdev->vdec_freq.active_freq /
					1000000 / STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_VP9:
			emi_bw_input = 30L * pdev->vdec_freq.active_freq /
					1000000 / STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_AV1:
			emi_bw_input = 30L * pdev->vdec_freq.active_freq /
					10000000 / STD_VDEC_FREQ;
			emi_bw = emi_bw * 24 /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_MPEG4:
		case V4L2_PIX_FMT_H263:
		case V4L2_PIX_FMT_S263:
		case V4L2_PIX_FMT_XVID:
		case V4L2_PIX_FMT_MPEG1:
		case V4L2_PIX_FMT_MPEG2:
			emi_bw_input = 15L * pdev->vdec_freq.active_freq /
					10000000 / STD_VDEC_FREQ;
			emi_bw = emi_bw * 20 /
					(2 * STD_VDEC_FREQ);
			break;
		}
		emi_bw = emi_bw / (1024 * 1024) / 8;
		emi_bw = emi_bw - emi_bw_output - emi_bw_input;
		if (emi_bw < 0)
			emi_bw = 0;

		if (pdev->vdec_qos_req[0] != 0) {
			if (ctx->picinfo.layout_mode & VDEC_DRV_UFO_AUO_ON ||
				ctx->picinfo.layout_mode & VDEC_DRV_UFO_ON) {
				mtk_icc_set_bw(pdev->vdec_qos_req[1],
						MBps_to_icc(emi_bw), 0);
				mtk_icc_set_bw(pdev->vdec_qos_req[11],
						MBps_to_icc(emi_bw), 0);
				mtk_icc_set_bw(pdev->vdec_qos_req[0],
						MBps_to_icc(emi_bw), 0);
			} else {
				mtk_icc_set_bw(pdev->vdec_qos_req[0],
						MBps_to_icc(emi_bw), 0);
				mtk_icc_set_bw(pdev->vdec_qos_req[2],
						MBps_to_icc(emi_bw), 0);
			}
			mtk_icc_set_bw(pdev->vdec_qos_req[3],
						MBps_to_icc(1), 0);
			if ((ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
				V4L2_PIX_FMT_AV1) ||
				(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
				V4L2_PIX_FMT_VP9)) {
				mtk_icc_set_bw(pdev->vdec_qos_req[4],
						MBps_to_icc(emi_bw), 0);
			} else {
				mtk_icc_set_bw(pdev->vdec_qos_req[4],
						MBps_to_icc(1), 0);
			}
			mtk_icc_set_bw(pdev->vdec_qos_req[5], MBps_to_icc(0), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[6], MBps_to_icc(0), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[7],
					MBps_to_icc(emi_bw_input), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[8], MBps_to_icc(0), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[9],
					MBps_to_icc(emi_bw_input), 0);
			mtk_icc_set_bw(pdev->vdec_qos_req[10], MBps_to_icc(0), 0);
		}
	} else {
		pr_debug("%s unknown hw_id %d\n", __func__, hw_id);
	}
#endif
}

static void mtk_vdec_emi_bw_end(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_EMI_BW
	int i = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;
	if (hw_id == MTK_VDEC_LAT) {
		if (pdev->vdec_qos_req[0] != 0) {
			for (i = 12; i < 19; i++) {
				mtk_icc_set_bw(pdev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
			}
		}
	} else if (hw_id == MTK_VDEC_CORE) {
		if (pdev->vdec_qos_req[0] != 0) {
			for (i = 0; i < 12; i++) {
				mtk_icc_set_bw(pdev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
			}
		}
	} else {
		pr_debug("%s unknown hw_id %d\n", __func__, hw_id);
	}
#endif
}

void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	/* add_job(&ctx->id, &vdec_jobs); */
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_begin(ctx, hw_id);
	mtk_vdec_emi_bw_begin(ctx, hw_id);
}

void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_end(ctx, hw_id);
	mtk_vdec_emi_bw_end(ctx, hw_id);
}
MODULE_LICENSE("GPL v2");

