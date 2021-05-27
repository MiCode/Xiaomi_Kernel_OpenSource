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

#define USE_GCE 0
#if ENC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 273
#define STD_LUMA_BW 100L
#define STD_CHROMA_BW 50L
#endif

#if ENC_EMI_BW
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#define CORE_NUM 2
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
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_rdma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_rdma_lite");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_wdma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_wdma_lite");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_cur_luma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_cur_chroma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_ref_luma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_ref_chroma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sub_r_luma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sub_w_luma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_fcs_nbm_rdma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_fcs_nbm_wdma");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_rcpu_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_rec_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_bsdma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sv_comv_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_rd_comv_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_rdma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_rdma_lite_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_wdma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_nbm_wdma_lite_2");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_cur_luma_2");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_cur_chroma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_ref_luma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_ref_chrom_1a");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sub_r_luma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_sub_w_luma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_fcs_nbm_rdma_1");
	dev->venc_qos_req[i++] = of_mtk_icc_get(&pdev->dev, "path_venc_fcs_nbm_wdma_1");

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

	ctx->dev->venc_freq.freq[core_id] = ctx->dev->venc_freqs[idx];
	if (ctx->dev->venc_freq.active_freq < ctx->dev->venc_freq.freq[core_id])
		ctx->dev->venc_freq.active_freq = ctx->dev->venc_freq.freq[core_id];

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

	ctx->dev->venc_freq.freq[core_id] = ctx->dev->venc_freqs[0];
	if (ctx->dev->venc_freq.freq[(core_id+1)%2] > ctx->dev->venc_freq.freq[core_id])
		ctx->dev->venc_freq.active_freq = ctx->dev->venc_freq.freq[(core_id+1)%2];
	else
		ctx->dev->venc_freq.active_freq = ctx->dev->venc_freq.freq[core_id];

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
	long cur_luma_bw = 0;
	long cur_chroma_bw = 0;
	long ref_luma_bw = 0;
	long ref_chroma_bw = 0;

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

	cur_luma_bw = STD_LUMA_BW * (pdev->venc_freq.active_freq / 1000000) *
			(100 + boost_perc) * 1 / STD_VENC_FREQ / 100 / 3;
	cur_chroma_bw = STD_CHROMA_BW * (pdev->venc_freq.active_freq / 1000000)
			* (100 + boost_perc) * 1 / STD_VENC_FREQ / 100 / 3;

	rec_bw = cur_luma_bw + cur_chroma_bw;
	if (0) { /* no UFO */
		ref_luma_bw = cur_luma_bw * 1;
		ref_chroma_bw = cur_chroma_bw * 1;
	} else {
		ref_luma_bw = 0;
		ref_chroma_bw = (cur_luma_bw * 4) + (cur_chroma_bw * 4);
	}

	if (core_id == 1)
		i = 17;

	if (pdev->venc_qos_req[0] != 0) {
		mtk_icc_set_bw(pdev->venc_qos_req[i++], MBps_to_icc(rcpu_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++], MBps_to_icc(rec_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++], MBps_to_icc(bsdma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++], MBps_to_icc(sv_comv_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++], MBps_to_icc(rd_comv_bw), 0);
		i += 4; /* No EMI BW for NBM port*/
		mtk_icc_set_bw(pdev->venc_qos_req[i++],
					MBps_to_icc(cur_luma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++],
					MBps_to_icc(cur_chroma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++],
					MBps_to_icc(ref_luma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++],
					MBps_to_icc(ref_chroma_bw), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++], MBps_to_icc(0), 0);
		mtk_icc_set_bw(pdev->venc_qos_req[i++], MBps_to_icc(0), 0);
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
			mtk_icc_set_bw(pdev->venc_qos_req[i], MBps_to_icc(0), 0);
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

