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

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>
#include <linux/slab.h>
#include "smi_public.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#ifdef DEC_DVFS
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>
#include <mmdvfs_config_util.h>
#include "vcodec_dvfs.h"
#define STD_VDEC_FREQ 312
static struct pm_qos_request vdec_qos_req_f;
static u64 vdec_freq;
static u32 vdec_freq_step_size;
static u64 vdec_freq_steps[MAX_FREQ_STEP];
static struct codec_history *vdec_hists;
static struct codec_job *vdec_jobs;
#endif

#ifdef DEC_EMI_BW
#include <mtk_smi.h>
static unsigned int h264_frm_scale[4] = {12, 24, 40, 12};
static unsigned int h265_frm_scale[4] = {12, 24, 40, 12};
static unsigned int vp9_frm_scale[4] = {12, 24, 40, 12};
static unsigned int vp8_frm_scale[4] = {12, 24, 40, 12};
static unsigned int mp24_frm_scale[5] = {16, 20, 32, 50, 16};

static struct plist_head vdec_rlist;
static struct mm_qos_request vdec_mc;
static struct mm_qos_request vdec_ufo;
static struct mm_qos_request vdec_pp;
static struct mm_qos_request vdec_pred_rd;
static struct mm_qos_request vdec_pred_wr;
static struct mm_qos_request vdec_ppwrap;
static struct mm_qos_request vdec_tile;
static struct mm_qos_request vdec_vld;
static struct mm_qos_request vdec_vld2;
static struct mm_qos_request vdec_avc_mv;
static struct mm_qos_request vdec_ufo_enc;
static struct mm_qos_request vdec_rg_ctrl_dma;
#endif

void mtk_dec_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
	ctx->input_driven = 0;
	ctx->user_lock_hw = 0;
}

int mtk_vcodec_init_dec_pm(struct mtk_vcodec_dev *mtkdev)
{
	int ret = 0;
#ifndef FPGA_PWRCLK_API_DISABLE
	struct device_node *node;
	struct platform_device *pdev;
	struct mtk_vcodec_pm *pm;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	pm->mtkdev = mtkdev;
	pm->chip_node = of_find_compatible_node(NULL,
		NULL, "mediatek,vdec_gcon");
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!node) {
		mtk_v4l2_err("of_parse_phandle mediatek,larb fail!");
		return -1;
	}

	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -1;
	}
	pm->larbvdec = &pdev->dev;
	pdev = mtkdev->plat_dev;
	pm->dev = &pdev->dev;

	if (pm->chip_node) {

		pm->clk_MT_SCP_SYS_VDE =
			devm_clk_get(&pdev->dev, "MT_SCP_SYS_VDE");
		if (IS_ERR(pm->clk_MT_SCP_SYS_VDE)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_VDE\n");
			return PTR_ERR(pm->clk_MT_SCP_SYS_VDE);
		}

		pm->clk_MT_SCP_SYS_VEN =
			devm_clk_get(&pdev->dev, "MT_SCP_SYS_VEN");
		if (IS_ERR(pm->clk_MT_SCP_SYS_VEN)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_VEN\n");
			return PTR_ERR(pm->clk_MT_SCP_SYS_VEN);
		}

		pm->clk_MT_SCP_SYS_DIS =
			devm_clk_get(&pdev->dev, "MT_SCP_SYS_DIS");
		if (IS_ERR(pm->clk_MT_SCP_SYS_DIS)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_SCP_SYS_DIS\n");
			return PTR_ERR(pm->clk_MT_SCP_SYS_DIS);
		}

		pm->clk_MT_CG_VDEC = devm_clk_get(&pdev->dev, "MT_CG_VDEC");
		if (IS_ERR(pm->clk_MT_CG_VDEC)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VDEC\n");
			return PTR_ERR(pm->clk_MT_CG_VDEC);
		}
	} else
		mtk_v4l2_err("[VCODEC][ERROR] DTS went wrong...");
#endif
	return ret;
}

void mtk_vcodec_release_dec_pm(struct mtk_vcodec_dev *dev)
{
	mutex_lock(&dev->dec_dvfs_mutex);
	free_hist(&vdec_hists, 0);
	mutex_unlock(&dev->dec_dvfs_mutex);
}

void mtk_vcodec_dec_pw_on(struct mtk_vcodec_pm *pm, int hw_id)
{
}

void mtk_vcodec_dec_pw_off(struct mtk_vcodec_pm *pm, int hw_id)
{
}

void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm, int hw_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	int ret;

	smi_bus_prepare_enable(SMI_LARB2_REG_INDX, "VDEC", true);
	ret = clk_prepare_enable(pm->clk_MT_CG_VDEC);
	if (ret)
		mtk_v4l2_err("clk_prepare_enable CG_VDEC fail %d", ret);
#endif
}

void mtk_vcodec_dec_clock_off(struct mtk_vcodec_pm *pm, int hw_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	clk_disable_unprepare(pm->clk_MT_CG_VDEC);
	smi_bus_disable_unprepare(SMI_LARB2_REG_INDX, "VDEC", true);
#endif
}

void mtk_prepare_vdec_dvfs(void)
{
#ifdef DEC_DVFS
	int ret;

	pm_qos_add_request(&vdec_qos_req_f, PM_QOS_VDEC_FREQ,
				PM_QOS_DEFAULT_VALUE);
	vdec_freq_step_size = 1;
	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VDEC_FREQ, &vdec_freq_steps[0],
					&vdec_freq_step_size);
	if (ret < 0)
		pr_debug("Failed to get vdec freq steps (%d)\n", ret);
#endif
}

void mtk_unprepare_vdec_dvfs(void)
{
#ifdef DEC_DVFS
	int freq_idx = 0;

	freq_idx = (vdec_freq_step_size == 0) ? 0 : (vdec_freq_step_size - 1);
	pm_qos_update_request(&vdec_qos_req_f, vdec_freq_steps[freq_idx]);
	pm_qos_remove_request(&vdec_qos_req_f);
	free_hist(&vdec_hists, 0);
	/* TODO: jobs error handle */
#endif
}

void mtk_prepare_vdec_emi_bw(void)
{
#ifdef DEC_EMI_BW
	plist_head_init(&vdec_rlist);
	mm_qos_add_request(&vdec_rlist, &vdec_mc, SMI_PORT_VDEC_MC);
	mm_qos_add_request(&vdec_rlist, &vdec_ufo, SMI_PORT_VDEC_UFO);
	mm_qos_add_request(&vdec_rlist, &vdec_pp, SMI_PORT_VDEC_PP);
	mm_qos_add_request(&vdec_rlist, &vdec_pred_rd, SMI_PORT_VDEC_PRED_RD);
	mm_qos_add_request(&vdec_rlist, &vdec_pred_wr, SMI_PORT_VDEC_PRED_WR);
	mm_qos_add_request(&vdec_rlist, &vdec_ppwrap, SMI_PORT_VDEC_PPWRAP);
	mm_qos_add_request(&vdec_rlist, &vdec_tile, SMI_PORT_VDEC_TILE);
	mm_qos_add_request(&vdec_rlist, &vdec_vld, SMI_PORT_VDEC_VLD);
	mm_qos_add_request(&vdec_rlist, &vdec_vld2, SMI_PORT_VDEC_VLD2);
	mm_qos_add_request(&vdec_rlist, &vdec_avc_mv, SMI_PORT_VDEC_AVC_MV);
	mm_qos_add_request(&vdec_rlist, &vdec_ufo_enc, SMI_PORT_VDEC_UFO_ENC);
	mm_qos_add_request(&vdec_rlist, &vdec_rg_ctrl_dma,
				SMI_PORT_VDEC_RG_CTRL_DMA);
#endif
}

void mtk_unprepare_vdec_emi_bw(void)
{
#ifdef DEC_EMI_BW
	mm_qos_remove_all_request(&vdec_rlist);
#endif
}

void mtk_vdec_dvfs_begin(struct mtk_vcodec_ctx *ctx)
{
#ifdef DEC_DVFS
	int target_freq = 0;
	u64 target_freq_64 = 0;
	struct codec_job *vdec_cur_job = 0;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	vdec_cur_job = move_job_to_head(&ctx->id, &vdec_jobs);
	if (vdec_cur_job != 0) {
		vdec_cur_job->start = get_time_us();
		target_freq = est_freq(vdec_cur_job->handle, &vdec_jobs,
					vdec_hists);
		target_freq_64 = match_freq(target_freq, &vdec_freq_steps[0],
					vdec_freq_step_size);
		if (target_freq > 0) {
			vdec_freq = target_freq;
			if (vdec_freq > target_freq_64)
				vdec_freq = target_freq_64;
			vdec_cur_job->mhz = (int)target_freq_64;
			pm_qos_update_request(&vdec_qos_req_f, target_freq_64);
		}
	} else {
		target_freq_64 = match_freq(DEFAULT_MHZ, &vdec_freq_steps[0],
						vdec_freq_step_size);
		pm_qos_update_request(&vdec_qos_req_f, target_freq_64);
	}
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_dvfs_end(struct mtk_vcodec_ctx *ctx)
{
#ifdef DEC_DVFS
	int freq_idx = 0;
	struct codec_job *vdec_cur_job = 0;

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

	freq_idx = (vdec_freq_step_size == 0) ? 0 : (vdec_freq_step_size - 1);
	pm_qos_update_request(&vdec_qos_req_f, vdec_freq_steps[freq_idx]);
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_emi_bw_begin(struct mtk_vcodec_ctx *ctx)
{
#ifdef DEC_EMI_BW
	int b_freq_idx = 0;
	int f_type = 1; /* TODO */
	long emi_bw = 0;
	long emi_bw_input = 0;
	long emi_bw_output = 0;

	if (vdec_freq_step_size > 1)
		b_freq_idx = vdec_freq_step_size - 1;

	emi_bw = 8L * 1920 * 1080 * 3 * 10 * vdec_freq;
	emi_bw_input = 8 * vdec_freq / STD_VDEC_FREQ;
	emi_bw_output = 1920 * 1088 * 3 * 30 * 10 * vdec_freq /
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

	/* bits/s to MBytes/s */
	emi_bw = emi_bw / (1024 * 1024) / 8;

	if (0) {    /* UFO */
		emi_bw = emi_bw * 6 / 10;
		emi_bw_output = emi_bw_output * 6 / 10;
	}

	emi_bw = emi_bw - emi_bw_output - (emi_bw_input * 2);
	if (emi_bw < 0)
		emi_bw = 0;

	if (0) {    /* UFO */
		mm_qos_set_request(&vdec_ufo, emi_bw, 0, BW_COMP_DEFAULT);
		mm_qos_set_request(&vdec_ufo_enc, emi_bw_output, 0,
					BW_COMP_DEFAULT);
	} else {
		mm_qos_set_request(&vdec_mc, emi_bw, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_pp, emi_bw_output, 0, BW_COMP_NONE);
	}
	mm_qos_set_request(&vdec_pred_rd, 1, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_pred_wr, 1, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_ppwrap, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_tile, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_vld, emi_bw_input, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_vld2, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_avc_mv, emi_bw_input * 2, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_rg_ctrl_dma, 0, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&vdec_rlist);
#endif
}

static void mtk_vdec_emi_bw_end(void)
{
#ifdef DEC_EMI_BW
	mm_qos_set_request(&vdec_mc, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_ufo, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_pp, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_pred_rd, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_pred_wr, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_ppwrap, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_tile, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_vld, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_vld2, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_avc_mv, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_ufo_enc, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&vdec_rg_ctrl_dma, 0, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&vdec_rlist);
#endif
}

void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	add_job(&ctx->id, &vdec_jobs);
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
}

void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_begin(ctx);
	mtk_vdec_emi_bw_begin(ctx);
}

void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_end(ctx);
	mtk_vdec_emi_bw_end();
}


