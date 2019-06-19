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

#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_util.h"
#ifdef CONFIG_MEDIATEK_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#define STD_VDEC_FREQ 312
static struct regulator *reg;
static unsigned long vdec_freq;
static u32 vdec_freq_step_size;
static unsigned long *vdec_freq_steps;
static u32 vdec_high_volt;
static struct codec_history *vdec_hists;
static struct codec_job *vdec_jobs;

#endif

#ifdef CONFIG_MEDIATEK_EMI_BW
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

int mtk_vcodec_init_dec_pm(struct mtk_vcodec_dev *mtkdev)
{
	struct platform_device *pdev;
	struct mtk_vcodec_pm *pm;
	struct mtk_vcodec_clk *dec_clk;
	struct mtk_vcodec_clk_info *clk_info;
	int i = 0, ret = 0;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	pm->mtkdev = mtkdev;
	dec_clk = &pm->vdec_clk;

	pdev = mtkdev->plat_dev;
	pm->dev = &pdev->dev;

	dec_clk->clk_num =
		of_property_count_strings(pdev->dev.of_node, "clock-names");
	if (dec_clk->clk_num > 0) {
		dec_clk->clk_info = devm_kcalloc(&pdev->dev,
			dec_clk->clk_num, sizeof(*clk_info),
			GFP_KERNEL);
		if (!dec_clk->clk_info)
			return -ENOMEM;
	} else {
		mtk_v4l2_err("Failed to get vdec clock count");
		return -EINVAL;
	}

	for (i = 0; i < dec_clk->clk_num; i++) {
		clk_info = &dec_clk->clk_info[i];
		ret = of_property_read_string_index(pdev->dev.of_node,
			"clock-names", i, &clk_info->clk_name);
		if (ret) {
			mtk_v4l2_err("Failed to get clock name id = %d", i);
			return ret;
		}
		clk_info->vcodec_clk = devm_clk_get(&pdev->dev,
			clk_info->clk_name);
		if (IS_ERR(clk_info->vcodec_clk)) {
			mtk_v4l2_err("devm_clk_get (%d)%s fail", i,
				clk_info->clk_name);
			return PTR_ERR(clk_info->vcodec_clk);
		}
	}

	pm_runtime_enable(&pdev->dev);

	return ret;
}

void mtk_vcodec_release_dec_pm(struct mtk_vcodec_dev *dev)
{
	pm_runtime_disable(dev->pm.dev);
}

void mtk_vcodec_dec_pw_on(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_get_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_get_sync fail %d", ret);
}

void mtk_vcodec_dec_pw_off(struct mtk_vcodec_pm *pm)
{
	int ret;

	ret = pm_runtime_put_sync(pm->dev);
	if (ret)
		mtk_v4l2_err("pm_runtime_put_sync fail %d", ret);
}

void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm)
{
	struct mtk_vcodec_clk *dec_clk = &pm->vdec_clk;
	int ret, i = 0;

	for (i = 0; i < dec_clk->clk_num; i++) {
		ret = clk_prepare_enable(dec_clk->clk_info[i].vcodec_clk);
		if (ret) {
			mtk_v4l2_err("clk_prepare_enable %d %s fail %d", i,
				dec_clk->clk_info[i].clk_name, ret);
			goto error;
		}
	}

	return;

error:
	for (i -= 1; i >= 0; i--)
		clk_disable_unprepare(dec_clk->clk_info[i].vcodec_clk);
}

void mtk_vcodec_dec_clock_off(struct mtk_vcodec_pm *pm)
{
	struct mtk_vcodec_clk *dec_clk = &pm->vdec_clk;
	int i = 0;

	for (i = dec_clk->clk_num - 1; i >= 0; i--)
		clk_disable_unprepare(dec_clk->clk_info[i].vcodec_clk);
}
void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *mtkdev)
{
#ifdef CONFIG_MEDIATEK_DVFS
	unsigned long freq = 0;
	int i = 0;
	struct platform_device *pdev;
	struct dev_pm_opp *opp;

	pdev = mtkdev->plat_dev;

	dev_pm_opp_of_add_table(&pdev->dev);

	reg = devm_regulator_get(&pdev->dev, "dvfsrc-vcore");
	if (!IS_ERR(reg)) {
		mtk_v4l2_err("devm_regulator_get success, regualtor:%p\n", reg);
	} else {
		mtk_v4l2_err("devm_regulator_get fail!!\n");
		return;
	}
	vdec_freq_step_size = dev_pm_opp_get_opp_count(&pdev->dev);
	vdec_freq_steps = kcalloc(
		vdec_freq_step_size, sizeof(unsigned long), GFP_KERNEL);
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(&pdev->dev, &freq))) {
		vdec_freq_steps[i] = freq;
		mtk_v4l2_err("i:%d, freq:%lld\n", i, freq);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	freq = vdec_freq_steps[vdec_freq_step_size - 1];
	opp = dev_pm_opp_find_freq_ceil(&pdev->dev, &freq);
	vdec_high_volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	mtk_v4l2_debug(1, "%s, vdec_freq_step_size:%d, high_volt:%d\n",
	    __func__, vdec_freq_step_size, vdec_high_volt);
#endif
}

void mtk_unprepare_vdec_dvfs(void)
{
#ifdef CONFIG_MEDIATEK_DVFS
	kfree(vdec_freq_steps);
#endif
}
void mtk_prepare_vdec_emi_bw(void)
{
#ifdef CONFIG_MEDIATEK_EMI_BW
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
#ifdef CONFIG_MEDIATEK_EMI_BW_CONTIG
	mm_qos_remove_all_request(&vdec_rlist);
#endif
}

void mtk_vdec_dvfs_begin(struct mtk_vcodec_ctx *ctx)
{
#ifdef CONFIG_MEDIATEK_DVFS
	int freq = 0;
	int low_volt = 0;
	unsigned long target_freq = 0;
	struct codec_job *vdec_cur_job = 0;
	struct platform_device *pdev;
	struct mtk_vcodec_pm *pm;
	struct dev_pm_opp *opp;

	pm = &ctx->dev->pm;
	pdev = ctx->dev->plat_dev;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	vdec_cur_job = move_job_to_head(&ctx->id, &vdec_jobs);
	if (vdec_cur_job != 0) {
		vdec_cur_job->start = get_time_us();
		freq = est_freq(vdec_cur_job->handle, &vdec_jobs,
					vdec_hists);
		target_freq = match_freq(freq, vdec_freq_steps,
					vdec_freq_step_size);
		if (freq > 0) {
			vdec_freq = freq;
			if (vdec_freq > target_freq)
				vdec_freq = target_freq;
			vdec_cur_job->mhz = (int)target_freq;
			opp = dev_pm_opp_find_freq_ceil(&pdev->dev,
				&target_freq);
			if (!IS_ERR(opp))
				opp = dev_pm_opp_find_freq_floor(&pdev->dev,
					&target_freq);
			else {
				mtk_v4l2_debug(1, "%s:%d, get opp fail\n",
					__func__, __LINE__);
				mutex_unlock(&ctx->dev->dec_dvfs_mutex);
				return;
			}
			low_volt = dev_pm_opp_get_voltage(opp);
			regulator_set_voltage(reg, low_volt, vdec_high_volt);
		}
	} else {
		target_freq = match_freq(DEFAULT_MHZ, vdec_freq_steps,
						vdec_freq_step_size);
		opp = dev_pm_opp_find_freq_ceil(&pdev->dev, &target_freq);
		if (!IS_ERR(opp))
			opp = dev_pm_opp_find_freq_floor(&pdev->dev,
				&target_freq);
		else {
			mtk_v4l2_debug(1, "%s:%d, get opp fail\n",
				__func__, __LINE__);
			mutex_unlock(&ctx->dev->dec_dvfs_mutex);
			return;
		}
		low_volt = dev_pm_opp_get_voltage(opp);
		regulator_set_voltage(reg, low_volt, vdec_high_volt);
	}
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}
void mtk_vdec_dvfs_end(struct mtk_vcodec_ctx *ctx)
{
#ifdef CONFIG_MEDIATEK_DVFS
	int low_volt = 0;

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
	regulator_set_voltage(reg, low_volt, vdec_high_volt);

	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}
void mtk_vdec_emi_bw_begin(struct mtk_vcodec_ctx *ctx)
{
#ifdef CONFIG_MEDIATEK_EMI_BW
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
	case V4L2_PIX_FMT_DIVX3:
	case V4L2_PIX_FMT_DIVX4:
	case V4L2_PIX_FMT_DIVX5:
	case V4L2_PIX_FMT_DIVX6:
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
#ifdef CONFIG_MEDIATEK_EMI_BW
bbb
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
void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx)
{
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	#ifdef CONFIG_MEDIATEK_DVFS
	add_job(&ctx->id, &vdec_jobs);
	#endif
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
}
void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx)
{
	mtk_vdec_dvfs_begin(ctx);
	mtk_vdec_emi_bw_begin(ctx);
}

void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx)
{
	mtk_vdec_dvfs_end(ctx);
	mtk_vdec_emi_bw_end();
}
