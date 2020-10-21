// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
//#include <soc/mediatek/smi.h>

#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
//#include "smi_public.h"

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

#define USE_GCE 0
#if ENC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 250
#define STD_LUMA_BW 100
#define STD_CHROMA_BW 50
#endif

#if ENC_EMI_BW
#include <linux/interconnect-provider.h>
#define CORE_NUM 1
#define ENC_MAX_PORT 12
#endif

struct temp_job *new_job_from_info(struct mtk_vcodec_ctx *ctx, int core_id)
{
	struct temp_job *new_job = (struct temp_job *)
				kmalloc(sizeof(struct temp_job), GFP_KERNEL);

	if (new_job == 0)
		return 0;

	new_job->ctx_id = ctx->id;
	new_job->format = ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
	new_job->type = 1; /* temp */
	new_job->module = core_id;
	new_job->visible_width = ctx->q_data[MTK_Q_DATA_SRC].visible_width;
	new_job->visible_height = ctx->q_data[MTK_Q_DATA_SRC].visible_height;
	new_job->operation_rate = 0;
	new_job->submit = 0; /* use now - to be filled */
	new_job->kcy = 0; /* retrieve hw counter - to be filled */
	new_job->next = 0;
	return new_job;
}

void free_job(struct temp_job *job)
{
	if (job != 0)
		kfree(job);
}

int add_to_tail(struct temp_job **job_list, struct temp_job *job)
{
	int cnt = 0;
	struct temp_job *tail = 0;

	if (job == 0)
		return -1;

	tail = *job_list;
	if (tail == 0) {
		*job_list = job;
		return 1;
	}

	cnt = 1;
	while (tail->next != 0) {
		cnt++;
		tail = tail->next;
	}
	cnt++;
	tail->next = job;

	return cnt;
}

struct temp_job *remove_from_head(struct temp_job **job_list)
{
	struct temp_job *head = *job_list;

	if (head == 0)
		return 0;

	*job_list = head->next;

	return head;
}

static struct ion_client *ion_venc_client;

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
	ctx->async_mode = 1;

	ctx->sram_data.uid = UID_MM_VENC;
	ctx->sram_data.type = TP_BUFFER;
	ctx->sram_data.size = 0;
	ctx->sram_data.flag = FG_POWER;
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
		NULL, "mediatek,mt6873-vcodec-enc");
	node = of_parse_phandle(dev->of_node, "mediatek,larbs", 0);
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

	pm->clk_MT_CG_VENC0 = devm_clk_get(&pdev->dev, "MT_CG_VENC0");
	if (IS_ERR(pm->clk_MT_CG_VENC0)) {
		mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VENC0\n");
		ret = PTR_ERR(pm->clk_MT_CG_VENC0);
	}

#endif
	ion_venc_client = NULL;

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
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;
	int ret;
#ifdef CONFIG_MTK_PSEUDO_M4U
	int i, larb_port_num, larb_id;
	struct M4U_PORT_STRUCT port;
#endif

#ifndef FPGA_PWRCLK_API_DISABLE
	time_check_start(MTK_FMT_ENC, core_id);
	if (core_id == MTK_VENC_CORE_0) {
		//smi_bus_prepare_enable(SMI_LARB7, "VENC0");
		ret = clk_prepare_enable(pm->clk_MT_CG_VENC0);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable CG_VENC fail %d", ret);
	} else {
		mtk_v4l2_err("invalid core_id %d", core_id);
		time_check_end(MTK_FMT_ENC, core_id, 50);
		return;
	}
	time_check_end(MTK_FMT_ENC, core_id, 50);
#endif


#ifdef CONFIG_MTK_PSEUDO_M4U
	time_check_start(MTK_FMT_ENC, core_id);
	if (core_id == MTK_VENC_CORE_0) {
		larb_port_num = SMI_LARB7_PORT_NUM;
		larb_id = 7;
	}

	//enable 34bits port configs
	for (i = 0; i < larb_port_num; i++) {
		port.ePortID = MTK_M4U_ID(larb_id, i);
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
	}
	time_check_end(MTK_FMT_ENC, core_id, 50);
#endif

}

void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id)
{
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;

#ifndef FPGA_PWRCLK_API_DISABLE
	if (core_id == MTK_VENC_CORE_0) {
		clk_disable_unprepare(pm->clk_MT_CG_VENC0);
		//smi_bus_disable_unprepare(SMI_LARB7, "VENC0");

	} else
		mtk_v4l2_err("invalid core_id %d", core_id);

#endif
}


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
	for (i = 0; i < MTK_VDEC_PORT_NUM; i++)
		dev->vdec_qos_req[i] = 0;

	i = 0;
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_rcpu");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_rec");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_bsdma");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_sv_comv");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_rd_comv");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_cur_luma");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_cur_chroma");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_ref_luma");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_ref_chroma");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_sub_r_luma");
	dev->venc_qos_req[i++] = of_icc_get(&pdev->dev, "path_venc_sub_w_luma");
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
	struct temp_job *job = ctx->dev->temp_venc_jobs[core_id];
	int area = 0;
	int idx = 0;
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	if (job == 0)
		return;

	area = job->visible_width * job->visible_height;

	if (area >= 3840 * 2160)
		idx = 2;
	else if (area >= 1920 * 1080)
		if (job->operation_rate > 30)
			idx = 2;
		else
			idx = 0;
	else
		idx = 0;

	if (job->operation_rate >= 120)
		idx = 2;

	if (job->format == V4L2_PIX_FMT_HEIF)
		idx = 3;

	ctx->dev->venc_freq.active_freq = ctx->dev->venc_freqs[idx];

	if (ctx->dev->venc_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
					&ctx->dev->venc_freq.active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(ctx->dev->venc_reg, volt, INT_MAX);
		if (ret) {
			pr_debug("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
	}
#endif
}

void mtk_venc_dvfs_end(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if ENC_DVFS
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	if (ctx->dev->temp_venc_jobs[core_id] == 0)
		return;

	ctx->dev->venc_freq.active_freq = ctx->dev->venc_freqs[0];

	if (ctx->dev->venc_reg != 0) {
		opp = dev_pm_opp_find_freq_ceil(&ctx->dev->plat_dev->dev,
					&ctx->dev->venc_freq.active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(ctx->dev->venc_reg, volt, INT_MAX);
		if (ret) {
			pr_debug("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
	}
#endif
}

void mtk_venc_emi_bw_begin(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if ENC_EMI_BW
	struct temp_job *job = 0;

	int id = 0;
	int boost_perc = 0;

	int rcpu_bw = 5 * 4 / 3;
	int rec_bw = 0;
	int bsdma_bw = 20 * 4 / 3;
	int sv_comv_bw = 4 * 4 / 3;
	int rd_comv_bw = 16 * 4 / 3;
	int cur_luma_bw = 0;
	int cur_chroma_bw = 0;
	int ref_luma_bw = 0;
	int ref_chroma_bw = 0;

	int i = 0;
	struct mtk_vcodec_dev *pdev = 0;

	pdev = ctx->dev;

	if (ctx->dev->temp_venc_jobs[0] == 0)
		return;

	job = ctx->dev->temp_venc_jobs[core_id];
	id = job->module;

	if (job->operation_rate > 60)
		boost_perc = 100;

	if (job->format == V4L2_PIX_FMT_H265 ||
		job->format == V4L2_PIX_FMT_HEIF ||
		(job->format == V4L2_PIX_FMT_H264 &&
		 job->visible_width >= 2160)) {
		boost_perc = 150;
	}

	cur_luma_bw = STD_LUMA_BW * pdev->venc_freq.active_freq *
			(100 + boost_perc) * 4 / STD_VENC_FREQ / 100 / 3;
	cur_chroma_bw = STD_CHROMA_BW * pdev->venc_freq.active_freq *
			(100 + boost_perc) * 4 / STD_VENC_FREQ / 100 / 3;

	rec_bw = cur_luma_bw + cur_chroma_bw;
	if (0) { /* no UFO */
		ref_luma_bw = cur_luma_bw * 1;
		ref_chroma_bw = cur_chroma_bw * 1;
	} else {
		ref_luma_bw = 0;
		ref_chroma_bw = (cur_luma_bw * 1) + (cur_chroma_bw * 1);
	}

	if (pdev->venc_qos_req[0] != 0) {
		icc_set_bw(pdev->venc_qos_req[i++], Mbps_to_icc(rcpu_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++], Mbps_to_icc(rec_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++], Mbps_to_icc(bsdma_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++], Mbps_to_icc(sv_comv_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++], Mbps_to_icc(rd_comv_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++],
					Mbps_to_icc(cur_luma_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++],
					Mbps_to_icc(cur_chroma_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++],
					Mbps_to_icc(ref_luma_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++],
					Mbps_to_icc(ref_chroma_bw), 0);
		icc_set_bw(pdev->venc_qos_req[i++], Mbps_to_icc(0), 0);
		icc_set_bw(pdev->venc_qos_req[i++], Mbps_to_icc(0), 0);
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
		for (i = 0; i < 11; i++) {
			/* TODO: Support 2 core */
			icc_set_bw(pdev->venc_qos_req[i], Mbps_to_icc(0), 0);
		}
	}
#endif
}

void mtk_venc_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int core_id)
{
}

void mtk_venc_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int core_id)
{
}

void mtk_venc_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int core_id)
{
}

struct temp_job *mtk_venc_queue_job(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
	int cnt = 0;
	struct temp_job *job = new_job_from_info(ctx, core_id);

	if (job != 0)
		cnt = add_to_tail(&ctx->dev->temp_venc_jobs[0], job);

	return job;
}

struct temp_job *mtk_venc_dequeue_job(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
	struct temp_job *job = remove_from_head(&ctx->dev->temp_venc_jobs[0]);

	if (job != 0)
		return job;

	/* print error message */
	return 0;
}


/* Total job count after this one is inserted */
void mtk_venc_pmqos_gce_flush(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
	/* mutex_lock(&ctx->dev->enc_dvfs_mutex); */
	struct temp_job *job = 0;
	int frame_rate = 0;

	job = mtk_venc_queue_job(ctx, core_id, job_cnt);

	frame_rate = ctx->enc_params.operationrate;
	if (frame_rate == 0) {
		frame_rate = ctx->enc_params.framerate_num /
				ctx->enc_params.framerate_denom;
	}
	if (job != NULL)
		job->operation_rate = frame_rate;

	if (job_cnt == 0) {
		// Adjust dvfs immediately
		mtk_venc_dvfs_begin(ctx, core_id);
		mtk_venc_emi_bw_begin(ctx, core_id);
	}
	/* mutex_unlock(&ctx->dev_enc_dvfs_mutex); */
}

/* Remaining job count after this one is done */
void mtk_venc_pmqos_gce_done(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
	struct temp_job *job = mtk_venc_dequeue_job(ctx, core_id, job_cnt);

	mtk_venc_dvfs_end(ctx, core_id);
	mtk_venc_emi_bw_end(ctx, job);
	free_job(job);

	if (job_cnt > 1) {
		mtk_venc_dvfs_begin(ctx, core_id);
		mtk_venc_emi_bw_begin(ctx, core_id);
	}
}


int mtk_venc_ion_config_buff(struct dma_buf *dmabuf)
{
/* for dma-buf using ion buffer, ion will check portid in dts
 * So, don't need to config buffer at user side, but remember
 * set iommus attribute in dts file.
 */
#ifdef ion_config_buff
	struct ion_handle *handle = NULL;
	struct ion_mm_data mm_data;
	int count = 0;

	mtk_v4l2_debug(4, "%p", dmabuf);

	if (!ion_venc_client)
		ion_venc_client = ion_client_create(g_ion_device, "venc");

	handle = ion_import_dma_buf(ion_venc_client, dmabuf);
	if (IS_ERR(handle)) {
		mtk_v4l2_err("import ion handle failed!\n");
		return -1;
	}
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = M4U_PORT_L7_VENC_RD_COMV_DISP;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;

	while (1) {
		int ion_config = 0;

		ion_config = ion_kernel_ioctl(ion_venc_client,
			ION_CMD_MULTIMEDIA, (unsigned long)&mm_data);

		if (ion_config != 0) {
			udelay(1000);
			count++;
			mtk_v4l2_err("re-configure buffer! count: %d\n", count);
		} else {
			break;
		}
	}
	if (count > 0)
		mtk_v4l2_err("re-configure buffer done! count: %d\n", count);

	/* dma hold ref, ion directly free */
	ion_free(ion_venc_client, handle);
#endif
	return 0;
}

