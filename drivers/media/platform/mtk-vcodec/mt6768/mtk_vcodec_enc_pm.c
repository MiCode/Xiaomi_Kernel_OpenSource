// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
//#include <soc/mediatek/smi.h>

#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "smi_public.h"

#if ENC_DVFS
//#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <mmdvfs_pmqos.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 312
static struct mtk_pm_qos_request venc_qos_req_f;
static u64 venc_freq;
static u32 venc_freq_step_size;
static u64 venc_freq_steps[MAX_FREQ_STEP];
static struct codec_history *venc_hists;
static struct codec_job *venc_jobs;
#endif

#if ENC_EMI_BW
static unsigned int gVENCFrmTRAVC[3] = {6, 12, 6};
static unsigned int gVENCFrmTRHEVC[3] = {6, 12, 6};
struct mtk_pm_qos_request venc_qos_req_bw;
#endif

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
	ctx->async_mode = 0;
}

int mtk_vcodec_init_enc_pm(struct mtk_vcodec_dev *mtkdev)
{
	int ret = 0;
#ifndef FPGA_PWRCLK_API_DISABLE
	struct device_node *node;
	struct platform_device *pdev;
	struct device *dev;
	struct mtk_vcodec_pm *pm;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	memset(pm, 0, sizeof(struct mtk_vcodec_pm));
	pm->mtkdev = mtkdev;
	pm->dev = &pdev->dev;
	dev = &pdev->dev;

	pm->chip_node = of_find_compatible_node(NULL,
		NULL, "mediatek,venc_gcon");
	node = of_parse_phandle(dev->of_node, "mediatek,larb", 0);
	if (!node) {
		mtk_v4l2_err("no mediatek,larb found");
		return -1;
	}
	pdev = of_find_device_by_node(node);
	if (!pdev) {
		mtk_v4l2_err("no mediatek,larb device found");
		return -1;
	}
	pm->larbvenc = &pdev->dev;

	pdev = mtkdev->plat_dev;
	pm->dev = &pdev->dev;

	pm->clk_MT_SCP_SYS_VEN = devm_clk_get(&pdev->dev, "MT_SCP_SYS_VEN");
	if (IS_ERR(pm->clk_MT_SCP_SYS_VEN)) {
		mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_VEN\n");
		return PTR_ERR(pm->clk_MT_SCP_SYS_VEN);
	}

	pm->clk_MT_SCP_SYS_DIS = devm_clk_get(&pdev->dev, "MT_SCP_SYS_DIS");
	if (IS_ERR(pm->clk_MT_SCP_SYS_DIS)) {
		mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_DIS\n");
		return PTR_ERR(pm->clk_MT_SCP_SYS_DIS);
	}

	pm->clk_MT_CG_VENC = devm_clk_get(&pdev->dev, "MT_CG_VENC");
	if (IS_ERR(pm->clk_MT_CG_VENC)) {
		mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VENC\n");
		ret = PTR_ERR(pm->clk_MT_CG_VENC);
	}
#endif
	return ret;
}

void mtk_vcodec_release_enc_pm(struct mtk_vcodec_dev *mtkdev)
{
#if ENC_EMI_BW
	/* do nothing */
#endif
}

void mtk_venc_deinit_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
}

void mtk_vcodec_enc_clock_on(struct mtk_vcodec_ctx *ctx, int core_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;
	int ret;

	smi_bus_prepare_enable(SMI_LARB4, "VENC");
	ret = clk_prepare_enable(pm->clk_MT_CG_VENC);
	if (ret)
		mtk_v4l2_err("clk_prepare_enable CG_VENC fail %d", ret);
#endif
}

void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;
	clk_disable_unprepare(pm->clk_MT_CG_VENC);
	smi_bus_disable_unprepare(SMI_LARB4, "VENC");
#endif
}


void mtk_prepare_venc_dvfs(void)
{
#if ENC_DVFS
	int ret;

	mtk_pm_qos_add_request(&venc_qos_req_f, PM_QOS_VENC_FREQ,
				PM_QOS_DEFAULT_VALUE);
	venc_freq_step_size = 1;
	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VENC_FREQ, &venc_freq_steps[0],
					&venc_freq_step_size);
	if (ret < 0)
		pr_debug("Failed to get venc freq steps (%d)\n", ret);
#endif
}

void mtk_unprepare_venc_dvfs(void)
{
#if ENC_DVFS
	int freq_idx = 0;

	freq_idx = (venc_freq_step_size == 0) ? 0 : (venc_freq_step_size - 1);
	mtk_pm_qos_update_request(&venc_qos_req_f, venc_freq_steps[freq_idx]);
	mtk_pm_qos_remove_request(&venc_qos_req_f);
	free_hist(&venc_hists, 0);
	/* TODO: jobs error handle */
#endif
}

void mtk_prepare_venc_emi_bw(void)
{
#if ENC_EMI_BW
	mtk_pm_qos_add_request(&venc_qos_req_bw, MTK_PM_QOS_MM_MEMORY_BANDWIDTH,
						PM_QOS_DEFAULT_VALUE);
#endif
}

void mtk_unprepare_venc_emi_bw(void)
{
#if ENC_EMI_BW
	mtk_pm_qos_remove_request(&venc_qos_req_bw);
#endif
}


void mtk_venc_dvfs_begin(struct mtk_vcodec_ctx *ctx)
{
#if ENC_DVFS
	int target_freq = 0;
	u64 target_freq_64 = 0;
	struct codec_job *venc_cur_job = 0;

	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	venc_cur_job = move_job_to_head(&ctx->id, &venc_jobs);
	if (venc_cur_job != 0) {
		venc_cur_job->start = get_time_us();
		target_freq = est_freq(venc_cur_job->handle, &venc_jobs,
					venc_hists);
		target_freq_64 = match_freq(target_freq, &venc_freq_steps[0],
					venc_freq_step_size);

		if (ctx->async_mode == 1 && target_freq_64 > 416)
			target_freq_64 = 416;

		if (target_freq > 0) {
			venc_freq = target_freq;
			if (venc_freq > target_freq_64)
				venc_freq = target_freq_64;

			venc_cur_job->mhz = (int)target_freq_64;
			mtk_pm_qos_update_request(&venc_qos_req_f, target_freq_64);
		}
	} else {
		target_freq_64 = match_freq(DEFAULT_MHZ, &venc_freq_steps[0],
						venc_freq_step_size);
		mtk_pm_qos_update_request(&venc_qos_req_f, target_freq_64);
	}
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

void mtk_venc_dvfs_end(struct mtk_vcodec_ctx *ctx)
{
#if ENC_DVFS
	int freq_idx = 0;
	long long interval = 0;
	struct codec_job *venc_cur_job = 0;

	/* venc dvfs */
	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	venc_cur_job = venc_jobs;
	if (venc_cur_job != 0 && (venc_cur_job->handle == &ctx->id)) {
		venc_cur_job->end = get_time_us();
		if (ctx->async_mode == 0) {
			update_hist(venc_cur_job, &venc_hists, 0);
		} else {
			/* Set allowed time for slowmotion 4 buffer pack */
			if (ctx->enc_params.operationrate > 0) {
				interval = (long long)(1000 * 4 /
						(int)ctx->enc_params.operationrate);
			} else {
				if (ctx->enc_params.framerate_denom == 0)
					ctx->enc_params.framerate_denom = 1;
				if (ctx->enc_params.operationrate == 0 &&
					ctx->enc_params.framerate_num == 0)
					ctx->enc_params.framerate_num =
					ctx->enc_params.framerate_denom * 30;
				interval = (long long)(1000 * 4 /
						(int)(ctx->enc_params.framerate_num /
						ctx->enc_params.framerate_denom));
			}
			update_hist(venc_cur_job, &venc_hists, interval*1000);
		}
		venc_jobs = venc_jobs->next;
		kfree(venc_cur_job);
	} else {
		/* print error log */
		pr_debug("no job at venc_dvfs_end, reset freq only");
	}

	freq_idx = (venc_freq_step_size == 0) ? 0 : (venc_freq_step_size - 1);
	mtk_pm_qos_update_request(&venc_qos_req_f, venc_freq_steps[freq_idx]);
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

void mtk_venc_emi_bw_begin(struct mtk_vcodec_ctx *ctx)
{
#if ENC_EMI_BW
	int f_type = 1; /* TODO */
	int boost_perc = 0;
	long emi_bw = 0;

	if (ctx->async_mode == 1)
		boost_perc = 100;

	if (ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H265)
		boost_perc = 150;

	emi_bw = 8L * 1920 * 1080 * 3 * 10 * venc_freq;
	switch (ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc) {
	case V4L2_PIX_FMT_H264:
		emi_bw = emi_bw * gVENCFrmTRAVC[f_type] *
			(100 + boost_perc) / (2 * STD_VENC_FREQ) / 100;
		break;
	case V4L2_PIX_FMT_H265:
		emi_bw = emi_bw * gVENCFrmTRHEVC[f_type] *
			(100 + boost_perc) / (2 * STD_VENC_FREQ) / 100;
		break;
	default:
		emi_bw = 0;
		pr_debug("Unsupported encoder type for BW");
	}

	/* transaction bytes to occupied BW */
	emi_bw = emi_bw * 4 / 3;

	/* bits/s to mbytes/s */
	emi_bw = emi_bw / (1024 * 1024) / 8;

	mtk_pm_qos_update_request(&venc_qos_req_bw, (int)emi_bw);
#endif
}

void mtk_venc_emi_bw_end(struct mtk_vcodec_ctx *ctx)
{
#if ENC_EMI_BW
	mtk_pm_qos_update_request(&venc_qos_req_bw, 0);
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
	mtk_venc_dvfs_begin(ctx);
	mtk_venc_emi_bw_begin(ctx);
}

void mtk_venc_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int core_id)
{
	mtk_venc_dvfs_end(ctx);
	mtk_venc_emi_bw_end(ctx);
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

