// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <soc/mediatek/smi.h>

#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_enc_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#ifdef ENC_DVFS
#undef ENC_DVFS
#endif
#define ENC_DVFS 0

#ifdef ENC_EMI_BW
#undef ENC_EMI_BW
#endif
#define ENC_EMI_BW 0

#if ENC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 249
#define STD_LUMA_BW 70
#define STD_CHROMA_BW 35


static u64 venc_freq;
static struct codec_history *venc_hists;
static struct codec_job *venc_jobs;

#endif

#if ENC_EMI_BW
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#endif

void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if ENC_DVFS
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		pr_debug("Failed to get opp table (%d)\n", ret);
		return;
	}

	dev->venc_reg = devm_regulator_get(&dev->plat_dev->dev,
						"dvfsrc-vcore");
	if (dev->venc_reg == 0) {
		pr_debug("Failed to get regulator\n");
		return;
	}

	dev->venc_freq_cnt = dev_pm_opp_get_opp_count(&dev->plat_dev->dev);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq))) {
		dev->venc_freqs[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}

	dev->temp_venc_jobs[0] = 0;
#endif
}

void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if ENC_DVFS
	/* free_hist(&venc_hists, 0); */
	/* TODO: jobs error handle */
#endif
}

void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if ENC_EMI_BW
	int i = 0;
	struct platform_device *pdev = 0;

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VENC_PORT_NUM; i++)
		dev->venc_qos_req[i] = 0;

	i = 0;
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_rcpu");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_rec");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_bsdma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sv_comv");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_rd_comv");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_cur_luma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_cur_chroma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_ref_luma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_ref_chroma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sub_r_luma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sub_w_luma");
#endif
}

void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if ENC_EMI_BW
#endif
}


void mtk_venc_dvfs_begin(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if ENC_DVFS
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long target_freq_hz = 0;
	unsigned long target_freq_mhz = 0;
	struct codec_job *venc_cur_job = 0;

	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	venc_cur_job = move_job_to_head(&ctx->id, &venc_jobs);
	if (venc_cur_job != 0) {
		venc_cur_job->start = get_time_us();
		target_freq_mhz = est_freq(venc_cur_job->handle, &venc_jobs,
					venc_hists);
		if (ctx->enc_params.operationrate >= 120 &&
			target_freq_mhz > 458)
			target_freq_mhz = 458;

		target_freq_hz = target_freq_mhz * 1000000UL;
		target_freq_hz = match_freq(target_freq_hz, &ctx->dev->venc_freqs[0],
			ctx->dev->venc_freq_cnt, ctx->dev->venc_freqs[ctx->dev->venc_freq_cnt - 1]);
		if (ctx->dev->venc_reg != 0) {
			opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev, &target_freq_hz);
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);

			ret = regulator_set_voltage(ctx->dev->venc_reg, volt, INT_MAX);
			if (ret) {
				pr_debug("%s Failed to set regulator voltage %d\n",
					 __func__, volt);
			}
		}

		target_freq_mhz = target_freq_hz / 1000000UL;
		if (target_freq_mhz > 0) {
			venc_freq = target_freq_mhz;
			venc_cur_job->mhz = (int)target_freq_mhz;
		}
	} else {
		target_freq_hz = ctx->dev->venc_freqs[ctx->dev->venc_freq_cnt - 1];
		if (ctx->dev->venc_reg != 0) {
			opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev, &target_freq_hz);
			volt = dev_pm_opp_get_voltage(opp);
			dev_pm_opp_put(opp);

			ret = regulator_set_voltage(ctx->dev->venc_reg, volt, INT_MAX);
			if (ret) {
				pr_debug("%s Failed to set regulator voltage %d\n",
					 __func__, volt);
			}
		}
	}
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

#if ENC_DVFS
static int mtk_venc_get_exec_cnt(struct mtk_vcodec_ctx *ctx)
{
	return (int)((readl(ctx->dev->enc_reg_base[VENC_SYS] + 0x17C) &
			0x7FFFFFFF) / 1000);
}
#endif

void mtk_venc_dvfs_end(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if ENC_DVFS
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	long long interval = 0;
	struct codec_job *venc_cur_job = 0;
	unsigned long target_freq_hz = 0;

	/* venc dvfs */
	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	venc_cur_job = venc_jobs;
	if (venc_cur_job != 0 && (venc_cur_job->handle == &ctx->id)) {
		venc_cur_job->end = get_time_us();
		if (ctx->enc_params.operationrate < 120) {
			if (ctx->enc_params.framerate_denom == 0)
				ctx->enc_params.framerate_denom = 1;

			if (ctx->enc_params.operationrate == 0 &&
				ctx->enc_params.framerate_num == 0)
				ctx->enc_params.framerate_num =
				ctx->enc_params.framerate_denom * 30;

			if ((ctx->enc_params.operationrate == 60 ||
				((ctx->enc_params.framerate_num /
				ctx->enc_params.framerate_denom) >= 59 &&
				(ctx->enc_params.framerate_num /
				ctx->enc_params.framerate_denom) <= 60) ||
				(ctx->q_data[MTK_Q_DATA_SRC].visible_width
					== 3840 &&
				ctx->q_data[MTK_Q_DATA_SRC].visible_height
					== 2160))) {
				/* Set allowed time for 60fps/4K recording */
				/* Use clock cycle if single instance */
				if (venc_hists != 0 && venc_hists->next == 0) {
					venc_cur_job->hw_kcy =
						mtk_venc_get_exec_cnt(ctx);
				}
				if (ctx->enc_params.operationrate > 0) {
					interval = (long long)(1000 * 1000 /
					(int)ctx->enc_params.operationrate);
				} else {
					interval = (long long)(1000 * 1000 /
					(int)(ctx->enc_params.framerate_num /
					ctx->enc_params.framerate_denom));
				}
				update_hist(venc_cur_job, &venc_hists,
					interval);
			} else {
				update_hist(venc_cur_job, &venc_hists, 0);
			}
		} else {
			/* Set allowed time for slowmotion 4 buffer pack */
			interval = (long long)(1000 * 1000 * 4 /
					(int)ctx->enc_params.operationrate);
			update_hist(venc_cur_job, &venc_hists, interval);
		}
		venc_jobs = venc_jobs->next;
		kfree(venc_cur_job);
	} else {
		/* print error log */
		pr_debug("no job at venc_dvfs_end, reset freq only");
	}

	target_freq_hz = 249000000UL;
	if (ctx->dev->venc_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev, &target_freq_hz);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(ctx->dev->venc_reg, volt, INT_MAX);
		if (ret) {
			pr_debug("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
	}
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

void mtk_venc_emi_bw_begin(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if ENC_EMI_BW
	int f_type = 1; /* TODO */
	int boost_perc = 0;
	int rcpu_bw = 5;
	int rec_bw = 0;
	int bsdma_bw = 10;
	int sv_comv_bw = 5;
	int rd_comv_bw = 10;
	int cur_luma_bw = 0;
	int cur_chroma_bw = 0;
	int ref_luma_bw = 0;
	int ref_chroma_bw = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;

	if (ctx->enc_params.operationrate >= 120 ||
		ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H265 ||
		(ctx->q_data[MTK_Q_DATA_SRC].visible_width == 3840 &&
		ctx->q_data[MTK_Q_DATA_SRC].visible_height == 2160)) {
		boost_perc = 100;
	} else if (ctx->enc_params.operationrate == 60) {
		boost_perc = 30;
	}

	/* Input BW scaling to venc_freq & config */
	cur_luma_bw = STD_LUMA_BW * venc_freq * (100 + boost_perc)
			/ STD_VENC_FREQ / 100;
	cur_chroma_bw = STD_CHROMA_BW * venc_freq * (100 + boost_perc)
			/ STD_VENC_FREQ / 100;

	/* BW in venc_freq */
	if (f_type == 0) {
		rec_bw = cur_luma_bw + cur_chroma_bw;
	} else {
		rec_bw = cur_luma_bw + cur_chroma_bw;
		ref_luma_bw = cur_luma_bw * 4;
		ref_chroma_bw = cur_chroma_bw * 4;
	}

	if (1) { /* UFO */
		ref_chroma_bw += ref_luma_bw;
		ref_luma_bw = 0;
	}

	if (pdev->venc_qos_req[0] != 0) {
		mtk_icc_set_bw(pdev->venc_qos_req[0], MBps_to_icc(rcpu_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[1], MBps_to_icc(rec_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[2], MBps_to_icc(bsdma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[3], MBps_to_icc(sv_comv_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[4], MBps_to_icc(rd_comv_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[5], MBps_to_icc(cur_luma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[6], MBps_to_icc(cur_chroma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[7], MBps_to_icc(ref_luma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[8], MBps_to_icc(ref_chroma_bw), 0);
	}
#endif
}

void mtk_venc_emi_bw_end(struct mtk_vcodec_ctx *ctx, struct temp_job *job)
{
#if ENC_EMI_BW
	int i = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;

	if (pdev->venc_qos_req[0] != 0) {
		for (i = 0; i < 11; i++)
			mtk_icc_set_bw(pdev->venc_qos_req[i], MBps_to_icc(0), 0);
	}
#endif
}

void mtk_venc_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if ENC_DVFS
	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	add_job(&ctx->id, &venc_jobs);
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

void mtk_venc_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int core_id)
{
	mtk_venc_dvfs_begin(ctx, core_id);
	mtk_venc_emi_bw_begin(ctx, core_id);
}

void mtk_venc_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int core_id)
{
	mtk_venc_dvfs_end(ctx, core_id);
	mtk_venc_emi_bw_end(ctx, NULL);
}

/* Total job count after this one is inserted */
void mtk_venc_pmqos_gce_flush(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
}

/* Remaining job count after this one is done */
void mtk_venc_pmqos_gce_done(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
}

