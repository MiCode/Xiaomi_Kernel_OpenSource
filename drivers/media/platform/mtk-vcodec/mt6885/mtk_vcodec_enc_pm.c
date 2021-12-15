/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "smi_public.h"

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

#define USE_GCE 1
#ifdef ENC_DVFS
#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <mmdvfs_pmqos.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 273
#define STD_LUMA_BW 100
#define STD_CHROMA_BW 50
static struct mtk_pm_qos_request venc_qos_req_f;
static u64 venc_freq;

static u32 venc_freq_step_size;
static u64 venc_freq_steps[MAX_FREQ_STEP];
/*
static struct codec_history *venc_hists;
static struct codec_job *venc_jobs;
*/
static u64 venc_req_freq[2];
/* 1080p60, 4k30, 4k60, 1 core 4k60*/
static u64 venc_freq_map[] = {273, 312, 416, 624};

#endif

#if ENC_EMI_BW
#include <mtk_smi.h>
#include <mtk_qos_bound.h>
#include "vcodec_bw.h"
#define CORE_NUM 2
/*
static unsigned int gVENCFrmTRAVC[3] = {6, 12, 6};
static unsigned int gVENCFrmTRHEVC[3] = {6, 12, 6};

static long long venc_start_time;
static struct vcodec_bw *venc_bw;
*/
static struct plist_head venc_rlist[CORE_NUM];
static struct mm_qos_request venc_rcpu[CORE_NUM];
static struct mm_qos_request venc_rec[CORE_NUM];
static struct mm_qos_request venc_bsdma[CORE_NUM];
static struct mm_qos_request venc_sub_w_luma[CORE_NUM];
static struct mm_qos_request venc_sv_comv[CORE_NUM];
static struct mm_qos_request venc_rd_comv[CORE_NUM];
static struct mm_qos_request venc_nbm_rdma[CORE_NUM];
static struct mm_qos_request venc_nbm_rdma_lite[CORE_NUM];
static struct mm_qos_request venc_fcs_nbm_rdma[CORE_NUM];
static struct mm_qos_request venc_nbm_wdma[CORE_NUM];
static struct mm_qos_request venc_nbm_wdma_lite[CORE_NUM];
static struct mm_qos_request venc_fcs_nbm_wdma[CORE_NUM];
static struct mm_qos_request venc_sub_r_luma[CORE_NUM];
static struct mm_qos_request venc_cur_luma[CORE_NUM];
static struct mm_qos_request venc_cur_chroma[CORE_NUM];
static struct mm_qos_request venc_ref_luma[CORE_NUM];
static struct mm_qos_request venc_ref_chroma[CORE_NUM];
#endif


#if 1
/* first scenario based version, will change to loading based */
struct temp_job {
	int ctx_id;
	int format;
	int type;
	int module;
	int visible_width; /* temp usage only, will use kcy */
	int visible_height; /* temp usage only, will use kcy */
	int operation_rate;
	int bitratemode;
	long long submit;
	int kcy;
	struct temp_job *next;
};
static struct temp_job *temp_venc_jobs[CORE_NUM];

struct temp_job *new_job_from_info(struct mtk_vcodec_ctx *ctx, int core_id)
{
	struct temp_job *new_job = kmalloc(sizeof(struct temp_job), GFP_KERNEL);

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

#endif

static struct ion_client *ion_venc_client;

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
	ctx->async_mode = 1;

	ctx->sram_data.uid = UID_MM_VENC;
	ctx->sram_data.type = TP_BUFFER;
	ctx->sram_data.size = 0;
	ctx->sram_data.flag = FG_POWER;

	if (slbc_request(&ctx->sram_data) >= 0)
		ctx->use_slbc = 1;
	else
		ctx->use_slbc = 0;
	pr_debug("slbc_request %d, %p\n", &ctx->sram_data, ctx->use_slbc);
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
		NULL, "mediatek,mt6885-vcodec-enc");
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

	pm->clk_MT_CG_VENC0 = devm_clk_get(&pdev->dev, "MT_CG_VENC0");
	if (IS_ERR(pm->clk_MT_CG_VENC0)) {
		mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VENC0\n");
		ret = PTR_ERR(pm->clk_MT_CG_VENC0);
	}

	pm->clk_MT_CG_VENC1 = devm_clk_get(&pdev->dev, "MT_CG_VENC1");
	if (IS_ERR(pm->clk_MT_CG_VENC1)) {
		mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VENC1\n");
		ret = PTR_ERR(pm->clk_MT_CG_VENC1);
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
	if (ctx->use_slbc == 1) {
		pr_debug("slbc_release, %p\n", &ctx->sram_data);
		slbc_release(&ctx->sram_data);
	}
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
	if (core_id == MTK_VENC_CORE_0 ||
		core_id == MTK_VENC_CORE_1) {
		smi_bus_prepare_enable(SMI_LARB7, "VENC0");
		ret = clk_prepare_enable(pm->clk_MT_CG_VENC0);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable CG_VENC fail %d", ret);

		smi_bus_prepare_enable(SMI_LARB8, "VENC1");
		ret = clk_prepare_enable(pm->clk_MT_CG_VENC1);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable CG_VENC fail %d", ret);
	} else {
		mtk_v4l2_err("invalid core_id %d", core_id);
		time_check_end(MTK_FMT_ENC, core_id, 50);
		return;
	}
	time_check_end(MTK_FMT_ENC, core_id, 50);
#endif
	if (ctx->use_slbc == 1) {
		time_check_start(MTK_FMT_ENC, core_id);
		ret = slbc_power_on(&ctx->sram_data);
		time_check_end(MTK_FMT_ENC, core_id, 50);
	}

#ifdef CONFIG_MTK_PSEUDO_M4U
	time_check_start(MTK_FMT_ENC, core_id);
	if (core_id == MTK_VENC_CORE_0) {
		larb_port_num = SMI_LARB7_PORT_NUM;
		larb_id = 7;
	} else if (core_id == MTK_VENC_CORE_1) {
		larb_port_num = SMI_LARB8_PORT_NUM;
		larb_id = 8;
	}

	//enable 34bits port configs & sram settings
	for (i = 0; i < larb_port_num; i++) {
		if (i == 5 || i == 6 || i == 13 ||
			i == 14 || i == 21 || i == 22) {
			ret = smi_sysram_enable(MTK_M4U_ID(larb_id, i),
				true, "LARB_VENC");
			if (ret)
				mtk_v4l2_err("%#x is not ready err: %#x\n",
					i, ret);
		} else {
			port.ePortID = MTK_M4U_ID(larb_id, i);
			port.Direction = 0;
			port.Distance = 1;
			port.domain = 0;
			port.Security = 0;
			port.Virtuality = 1;
			m4u_config_port(&port);
		}
	}
	time_check_end(MTK_FMT_ENC, core_id, 50);
#endif

}

void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id)
{
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;

	if (ctx->use_slbc == 1)
		slbc_power_off(&ctx->sram_data);

#ifndef FPGA_PWRCLK_API_DISABLE
	if (core_id == MTK_VENC_CORE_0 ||
		core_id == MTK_VENC_CORE_1) {
		clk_disable_unprepare(pm->clk_MT_CG_VENC0);
		smi_bus_disable_unprepare(SMI_LARB7, "VENC0");

		clk_disable_unprepare(pm->clk_MT_CG_VENC1);
		smi_bus_disable_unprepare(SMI_LARB8, "VENC1");
	} else
		mtk_v4l2_err("invalid core_id %d", core_id);

#endif
}


void mtk_prepare_venc_dvfs(void)
{
#if ENC_DVFS
	int ret;
	int i;

	mtk_pm_qos_add_request(&venc_qos_req_f, PM_QOS_VENC_FREQ,
				PM_QOS_DEFAULT_VALUE);
	venc_freq_step_size = 1;
	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VENC_FREQ, &venc_freq_steps[0],
					&venc_freq_step_size);
	if (ret < 0)
		pr_debug("Failed to get venc freq steps (%d)\n", ret);

	for (i = 0; i < CORE_NUM; i++)
		temp_venc_jobs[i] = 0;
#endif
}

void mtk_unprepare_venc_dvfs(void)
{
#if ENC_DVFS
	int freq_idx = 0;

	freq_idx = (venc_freq_step_size == 0) ? 0 : (venc_freq_step_size - 1);
	mtk_pm_qos_update_request(&venc_qos_req_f, venc_freq_steps[freq_idx]);
	mtk_pm_qos_remove_request(&venc_qos_req_f);
	/* free_hist(&venc_hists, 0); */
	/* TODO: jobs error handle */
#endif
}

void mtk_prepare_venc_emi_bw(void)
{
#if ENC_EMI_BW
	plist_head_init(&venc_rlist[0]);
	mm_qos_add_request(&venc_rlist[0], &venc_rcpu[0],
		M4U_PORT_L7_VENC_RCPU_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_rec[0],
		M4U_PORT_L7_VENC_REC_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_bsdma[0],
		M4U_PORT_L7_VENC_BSDMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_sub_w_luma[0],
		M4U_PORT_L7_VENC_SUB_W_LUMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_sv_comv[0],
		M4U_PORT_L7_VENC_SV_COMV_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_rd_comv[0],
		M4U_PORT_L7_VENC_RD_COMV_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_nbm_rdma[0],
		M4U_PORT_L7_VENC_NBM_RDMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_nbm_rdma_lite[0],
		M4U_PORT_L7_VENC_NBM_RDMA_LITE_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_fcs_nbm_rdma[0],
		M4U_PORT_L7_VENC_FCS_NBM_RDMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_nbm_wdma[0],
		M4U_PORT_L7_VENC_NBM_WDMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_nbm_wdma_lite[0],
		M4U_PORT_L7_VENC_NBM_WDMA_LITE_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_fcs_nbm_wdma[0],
		M4U_PORT_L7_VENC_FCS_NBM_WDMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_sub_r_luma[0],
		M4U_PORT_L7_VENC_SUB_R_LUMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_cur_luma[0],
		M4U_PORT_L7_VENC_CUR_LUMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_cur_chroma[0],
		M4U_PORT_L7_VENC_CUR_CHROMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_ref_luma[0],
		M4U_PORT_L7_VENC_REF_LUMA_DISP);
	mm_qos_add_request(&venc_rlist[0], &venc_ref_chroma[0],
		M4U_PORT_L7_VENC_REF_CHROMA_DISP);

	plist_head_init(&venc_rlist[1]);
	mm_qos_add_request(&venc_rlist[1], &venc_rcpu[1],
		M4U_PORT_L8_VENC_RCPU_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_rec[1],
		M4U_PORT_L8_VENC_REC_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_bsdma[1],
		M4U_PORT_L8_VENC_BSDMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_sub_w_luma[1],
		M4U_PORT_L8_VENC_SUB_W_LUMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_sv_comv[1],
		M4U_PORT_L8_VENC_SV_COMV_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_rd_comv[1],
		M4U_PORT_L8_VENC_RD_COMV_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_nbm_rdma[1],
		M4U_PORT_L8_VENC_NBM_RDMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_nbm_rdma_lite[1],
		M4U_PORT_L8_VENC_NBM_RDMA_LITE_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_fcs_nbm_rdma[1],
		M4U_PORT_L8_VENC_FCS_NBM_RDMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_nbm_wdma[1],
		M4U_PORT_L8_VENC_NBM_WDMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_nbm_wdma_lite[1],
		M4U_PORT_L8_VENC_NBM_WDMA_LITE_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_fcs_nbm_wdma[1],
		M4U_PORT_L8_VENC_FCS_NBM_WDMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_sub_r_luma[1],
		M4U_PORT_L8_VENC_SUB_R_LUMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_cur_luma[1],
		M4U_PORT_L8_VENC_CUR_LUMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_cur_chroma[1],
		M4U_PORT_L8_VENC_CUR_CHROMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_ref_luma[1],
		M4U_PORT_L8_VENC_REF_LUMA_MDP);
	mm_qos_add_request(&venc_rlist[1], &venc_ref_chroma[1],
		M4U_PORT_L8_VENC_REF_CHROMA_MDP);
#endif
}

void mtk_unprepare_venc_emi_bw(void)
{
#if ENC_EMI_BW
	mm_qos_remove_all_request(&venc_rlist[0]);
	mm_qos_remove_all_request(&venc_rlist[1]);
#endif
}


void mtk_venc_dvfs_begin(struct temp_job **job_list)
{
#if 0
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
#if ENC_DVFS
	struct temp_job *job = *job_list;
	int area = 0;
	int idx = 0;

	if (job == 0)
		return;

	area = job->visible_width * job->visible_height;

	if (area >= 3840 * 2160) {
		if (job->operation_rate > 30)
			idx = 2;
		else
			idx = 0;
	} else if (area >= 1920 * 1080) {
		if (job->operation_rate > 30) {
			if (job->format == V4L2_PIX_FMT_H265)
				idx = 1;
			else /* H.264 */
				idx = 2;
		} else {
			if (job->bitratemode == 1) /* CBR */
				idx = 1;
			else
				idx = 0;
		}
	} else {
		idx = 0;
	}

	if (job->operation_rate > 240 ||
		(area >= 1920 * 1080 && job->operation_rate == 240))
		idx = 2;
	else if (job->operation_rate >= 120)
		idx = 0;

	if (job->format == V4L2_PIX_FMT_HEIF)
		idx = 3;

	mtk_v4l2_debug(2, "[Debug] freq idx %d (%d, %d, %d)",
		idx, area, job->operation_rate, job->bitratemode);

	venc_req_freq[job->module] = venc_freq_map[idx];
	venc_freq = venc_req_freq[0];
	if (venc_freq < venc_req_freq[1])
		venc_freq = venc_req_freq[1];

	mtk_pm_qos_update_request(&venc_qos_req_f, venc_freq);
#endif
}

void mtk_venc_dvfs_end(struct temp_job *job)
{
#if 0
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
			interval = (long long)(1000 * 4 /
					(int)ctx->enc_params.operationrate);
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
#if ENC_DVFS
	if (job == 0)
		return;

	venc_req_freq[job->module] = 0;

	venc_freq = venc_req_freq[0];
	if (venc_freq < venc_req_freq[1])
		venc_freq = venc_req_freq[1];

	mtk_pm_qos_update_request(&venc_qos_req_f, venc_freq);
#endif
}

void mtk_venc_emi_bw_begin(struct temp_job **jobs)
{
#if 0
	int f_type = 1; /* TODO */
	int boost_perc = 0;
	long emi_bw = 0;

	if (ctx->async_mode == 1)
		boost_perc = 100;

	if (ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H265 ||
		ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_HEIF)
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

	/* New Ports */
	int sub_w_luma_bw = 0;
	int nbm_rdma_bw = 0;
	int nbm_rdma_lite_bw = 0;
	int fcs_nbm_rdma_bw = 0;
	int nbm_wdma_bw = 0;
	int nbm_wdma_lite_bw = 0;
	int fcs_nbm_wdma_bw = 0;
	int sub_r_luma_bw = 0;

	if (*jobs == 0)
		return;

	job = *jobs;
	id = job->module;

	if (job->operation_rate > 60)
		boost_perc = 100;

	if (job->format == V4L2_PIX_FMT_H265 ||
		job->format == V4L2_PIX_FMT_HEIF ||
		(job->format == V4L2_PIX_FMT_H264 &&
		 job->visible_width >= 2160)) {
		boost_perc = 150;
	}

	cur_luma_bw = STD_LUMA_BW * venc_freq * (100 + boost_perc) * 4/
			STD_VENC_FREQ / 100 / 3;
	cur_chroma_bw = STD_CHROMA_BW * venc_freq * (100 + boost_perc) * 4 /
			STD_VENC_FREQ / 100 / 3;

	rec_bw = cur_luma_bw + cur_chroma_bw;
	if (0) { /* no UFO */
		ref_luma_bw = cur_luma_bw * 1;
		ref_chroma_bw = cur_chroma_bw * 1;
	} else {
		ref_luma_bw = 0;
		ref_chroma_bw = (cur_luma_bw * 1) + (cur_chroma_bw * 1);
	}

	mm_qos_set_request(&venc_rcpu[id], rcpu_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rec[id], rec_bw, 0, BW_COMP_VENC);
	mm_qos_set_request(&venc_bsdma[id], bsdma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_sub_w_luma[id], sub_w_luma_bw, 0,
						BW_COMP_NONE);
	mm_qos_set_request(&venc_sv_comv[id], sv_comv_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rd_comv[id], rd_comv_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_rdma[id], nbm_rdma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_rdma_lite[id], nbm_rdma_lite_bw, 0,
						BW_COMP_NONE);
	mm_qos_set_request(&venc_fcs_nbm_rdma[id], fcs_nbm_rdma_bw, 0,
						BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_wdma[id], nbm_wdma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_wdma_lite[id], nbm_wdma_lite_bw, 0,
						BW_COMP_NONE);
	mm_qos_set_request(&venc_fcs_nbm_wdma[id], fcs_nbm_wdma_bw, 0,
						BW_COMP_NONE);
	mm_qos_set_request(&venc_sub_r_luma[id], sub_r_luma_bw, 0,
						BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_luma[id], cur_luma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_chroma[id], cur_chroma_bw, 0,
						BW_COMP_NONE);
	mm_qos_set_request(&venc_ref_luma[id], ref_luma_bw, 0, BW_COMP_VENC);
	mm_qos_set_request(&venc_ref_chroma[id], ref_chroma_bw, 0,
						BW_COMP_VENC);
	mm_qos_update_all_request(&venc_rlist[id]);
#endif
}

void mtk_venc_emi_bw_end(struct temp_job *job)
{
#if ENC_EMI_BW
	int core_id;

	if (job == 0)
		return;

	core_id = job->module;

	mm_qos_set_request(&venc_rcpu[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rec[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_bsdma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_sub_w_luma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_sv_comv[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rd_comv[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_rdma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_rdma_lite[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_fcs_nbm_rdma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_wdma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_nbm_wdma_lite[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_fcs_nbm_wdma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_sub_r_luma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_luma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_chroma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_ref_luma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_ref_chroma[core_id], 0, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&venc_rlist[core_id]);
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
		cnt = add_to_tail(&temp_venc_jobs[core_id], job);

	return job;
}

struct temp_job *mtk_venc_dequeue_job(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
	struct temp_job *job = remove_from_head(&temp_venc_jobs[core_id]);

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

	if (job != NULL) {
		job->operation_rate = frame_rate;
		job->bitratemode = ctx->enc_params.bitratemode;
	}

	if (job_cnt == 0) {
		// Adjust dvfs immediately
		mtk_venc_dvfs_begin(&temp_venc_jobs[core_id]);
		mtk_venc_emi_bw_begin(&temp_venc_jobs[core_id]);
	}
	/* mutex_unlock(&ctx->dev_enc_dvfs_mutex); */
}

/* Remaining job count after this one is done */
void mtk_venc_pmqos_gce_done(struct mtk_vcodec_ctx *ctx, int core_id,
				int job_cnt)
{
	struct temp_job *job = mtk_venc_dequeue_job(ctx, core_id, job_cnt);

	mtk_venc_dvfs_end(job);
	mtk_venc_emi_bw_end(job);
	free_job(job);

	if (job_cnt > 1) {
		mtk_venc_dvfs_begin(&temp_venc_jobs[core_id]);
		mtk_venc_emi_bw_begin(&temp_venc_jobs[core_id]);
	}
}


int mtk_venc_ion_config_buff(struct dma_buf *dmabuf)
{
/* for dma-buf using ion buffer, ion will check portid in dts
 * So, don't need to config buffer at user side, but remember
 * set iommus attribute in dts file.
 */
#if 0

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

