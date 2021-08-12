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

#ifdef DEC_DVFS
#undef DEC_DVFS
#endif
#define DEC_DVFS 0

#ifdef DEC_EMI_BW
#undef DEC_EMI_BW
#endif
#define DEC_EMI_BW 0

#if DEC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#define STD_VDEC_FREQ 218
static u64 vdec_freq;
static struct codec_history *vdec_hists;
static struct codec_job *vdec_jobs;
#endif

#define VDEC_DRV_UFO_ON (1 << 0)
#define VDEC_DRV_UFO_AUO_ON (1 << 1)
#if DEC_EMI_BW
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
static unsigned int h264_frm_scale[4] = {12, 24, 40, 12};
static unsigned int h265_frm_scale[4] = {12, 24, 40, 12};
static unsigned int vp9_frm_scale[4] = {12, 24, 40, 12};
static unsigned int vp8_frm_scale[4] = {12, 24, 40, 12};
static unsigned int mp24_frm_scale[5] = {16, 20, 32, 50, 16};

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
	unsigned long target_freq_hz = 0;
	unsigned long target_freq_mhz = 0;
	struct codec_job *vdec_cur_job = 0;
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	vdec_cur_job = move_job_to_head(&ctx->id, &vdec_jobs);

	if (ctx->dec_params.operating_rate > 0) {
		target_freq_mhz = 312L *
				ctx->q_data[MTK_Q_DATA_DST].coded_width *
				ctx->q_data[MTK_Q_DATA_DST].coded_height *
				ctx->dec_params.operating_rate /
				3840L / 2160L / 30L;
		target_freq_hz = target_freq_mhz * 1000000UL;
		target_freq_hz = match_freq(target_freq_hz, &ctx->dev->vdec_freqs[0],
			ctx->dev->vdec_freq_cnt, ctx->dev->vdec_freqs[ctx->dev->vdec_freq_cnt - 1]);
		if (ctx->dev->vdec_reg != 0) {
			opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
						&target_freq_hz);
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);

			ret = regulator_set_voltage(ctx->dev->vdec_reg, volt, INT_MAX);
			if (ret) {
				pr_debug("%s Failed to set regulator voltage %d\n",
					 __func__, volt);
			}
		}
		target_freq_mhz = target_freq_hz / 1000000UL;
		vdec_freq = target_freq_mhz;
		if (vdec_cur_job != 0)
			vdec_cur_job->mhz = (int)target_freq_mhz;
	} else if (vdec_cur_job != 0) {
		vdec_cur_job->start = get_time_us();
		target_freq_mhz = est_freq(vdec_cur_job->handle, &vdec_jobs,
					vdec_hists);
		target_freq_hz = target_freq_mhz * 1000000UL;
		target_freq_hz = match_freq(target_freq_hz, &ctx->dev->vdec_freqs[0],
			ctx->dev->vdec_freq_cnt, ctx->dev->vdec_freqs[ctx->dev->vdec_freq_cnt - 1]);
		if (ctx->dev->vdec_reg != 0) {
			opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
						&target_freq_hz);
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);

			ret = regulator_set_voltage(ctx->dev->vdec_reg, volt, INT_MAX);
			if (ret) {
				pr_debug("%s Failed to set regulator voltage %d\n",
					 __func__, volt);
			}
		}
		target_freq_mhz = target_freq_hz / 1000000UL;
		if (target_freq_mhz > 0) {
			vdec_freq = target_freq_mhz;
			if (vdec_cur_job != 0)
				vdec_cur_job->mhz = (int)target_freq_mhz;
		}
	} else {
		target_freq_hz = ctx->dev->vdec_freqs[ctx->dev->vdec_freq_cnt - 1];
		if (ctx->dev->vdec_reg != 0) {
			opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
						&target_freq_hz);
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);

			ret = regulator_set_voltage(ctx->dev->vdec_reg, volt, INT_MAX);
			if (ret) {
				pr_debug("%s Failed to set regulator voltage %d\n",
					 __func__, volt);
			}
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
	struct codec_job *vdec_cur_job = 0;
	unsigned long target_freq_hz = 0;

	/* vdec dvfs */
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	vdec_cur_job = vdec_jobs;
	if (vdec_cur_job->handle == &ctx->id) {
		vdec_cur_job->end = get_time_us();
		update_hist(vdec_cur_job, &vdec_hists, 0);
		vdec_jobs = vdec_jobs->next;
		kfree(vdec_cur_job);
	} else {
		/* print error log */
	}

	target_freq_hz = 218000000UL;

	if (ctx->dev->vdec_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev, &target_freq_hz);
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
	struct mtk_vcodec_dev *pdev = 0;
	int f_type = 1; /* TODO */
	long emi_bw = 0;
	long emi_bw_input = 0;
	long emi_bw_output = 0;
	int is_ufo_on = 0;

	pdev = ctx->dev;
	emi_bw = 8L * 1920 * 1080 * 2 * 10 * vdec_freq;
	emi_bw_input = 8 * vdec_freq / STD_VDEC_FREQ;
	emi_bw_output = 1920 * 1088 * 3 * 20 * 10 * vdec_freq /
			2 / 3 / STD_VDEC_FREQ / 1024 / 1024;

	switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
	case V4L2_PIX_FMT_H264:
		emi_bw = emi_bw * h264_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_H265:
		emi_bw = emi_bw * h265_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_VP8:
		emi_bw = emi_bw * vp8_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_VP9:
		emi_bw = emi_bw * vp9_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_S263:
	case V4L2_PIX_FMT_XVID:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
		emi_bw = emi_bw * mp24_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	}

	is_ufo_on = (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
			V4L2_PIX_FMT_H264 ||
			ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
			V4L2_PIX_FMT_H265 ||
			ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
			V4L2_PIX_FMT_VP9) &&
			((ctx->q_data[MTK_Q_DATA_DST].coded_width *
			ctx->q_data[MTK_Q_DATA_DST].coded_height) >=
			(1920 * 1080)) ? 1 : 0;

	/* bits/s to MBytes/s */
	emi_bw = emi_bw / (1024 * 1024) / 8;

	if (is_ufo_on == 1) {    /* UFO */
		emi_bw = emi_bw * 6 / 10;
		emi_bw_output = emi_bw_output * 6 / 10;
	}

	emi_bw = emi_bw - emi_bw_output - (emi_bw_input * 2);
	if (emi_bw < 0)
		emi_bw = 0;

	if (is_ufo_on == 1) {    /* UFO */
		mtk_icc_set_bw(pdev->vdec_qos_req[1],
						MBps_to_icc(emi_bw), 0);
		mtk_icc_set_bw(pdev->vdec_qos_req[11],
						MBps_to_icc(emi_bw_output), 0);
	} else {
		mtk_icc_set_bw(pdev->vdec_qos_req[2],
						MBps_to_icc(emi_bw_output), 0);
	}
	mtk_icc_set_bw(pdev->vdec_qos_req[0],
						MBps_to_icc(emi_bw), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[3], MBps_to_icc(1), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[4], MBps_to_icc(1), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[5], MBps_to_icc(0), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[6], MBps_to_icc(0), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[7],
						MBps_to_icc(emi_bw_input), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[8],
						MBps_to_icc(0), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[9],
						MBps_to_icc(emi_bw_input * 2), 0);
	mtk_icc_set_bw(pdev->vdec_qos_req[10], MBps_to_icc(0), 0);
#endif
}

static void mtk_vdec_emi_bw_end(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_EMI_BW
	int i = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;

	if (pdev->vdec_qos_req[0] != 0) {
		for (i = 0; i < 12; i++) {
			mtk_icc_set_bw(pdev->vdec_qos_req[i],
				MBps_to_icc(0), 0);
		}
	}
#endif
}

void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	add_job(&ctx->id, &vdec_jobs);
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

