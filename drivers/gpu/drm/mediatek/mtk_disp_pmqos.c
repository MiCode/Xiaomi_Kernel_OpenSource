/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mtk_layering_rule.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_drv.h"

static struct drm_crtc *dev_crtc;

/* add for mm qos */
static u64 g_freq_steps[MAX_FREQ_STEP];
static int g_freq_level = -1;
static int step_size = 1;

#ifdef MTK_FB_MMDVFS_SUPPORT
static struct pm_qos_request mm_freq_request;
int __mtk_disp_pmqos_slot_look_up(int comp_id, int mode)
{
	switch (comp_id) {
	case DDP_COMPONENT_OVL0:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_BW;
	case DDP_COMPONENT_OVL1:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_BW;
	case DDP_COMPONENT_OVL0_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_BW;
	case DDP_COMPONENT_OVL1_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_BW;
	case DDP_COMPONENT_OVL2_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL2_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL2_2L_BW;
	case DDP_COMPONENT_OVL3_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL3_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL3_2L_BW;
	case DDP_COMPONENT_RDMA0:
		return DISP_PMQOS_RDMA0_BW;
	case DDP_COMPONENT_RDMA1:
		return DISP_PMQOS_RDMA1_BW;
	case DDP_COMPONENT_RDMA2:
		return DISP_PMQOS_RDMA2_BW;
	case DDP_COMPONENT_WDMA0:
		return DISP_PMQOS_WDMA0_BW;
	case DDP_COMPONENT_WDMA1:
		return DISP_PMQOS_WDMA1_BW;
	default:
		DDPPR_ERR("%s, unknown comp %d\n", __func__, comp_id);
		break;
	}

	return -EINVAL;
}

int __mtk_disp_pmqos_port_look_up(int comp_id)
{
	switch (comp_id) {
#if defined(CONFIG_MACH_MT6779)
	case DDP_COMPONENT_OVL0:
		return SMI_PORT_DISP_OVL0;
	case DDP_COMPONENT_OVL1:
		return SMI_PORT_DISP_OVL1;
	case DDP_COMPONENT_OVL0_2L:
		return SMI_PORT_DISP_OVL0_2L;
	case DDP_COMPONENT_OVL1_2L:
		return SMI_PORT_DISP_OVL1_2L;
	case DDP_COMPONENT_OVL2_2L:
		return SMI_PORT_DISP_OVL2;
	case DDP_COMPONENT_OVL3_2L:
		return SMI_PORT_DISP_OVL3;
	case DDP_COMPONENT_RDMA0:
		return SMI_PORT_DISP_RDMA0;
	case DDP_COMPONENT_RDMA1:
		return SMI_PORT_DISP_RDMA1;
	case DDP_COMPONENT_WDMA0:
		return SMI_PORT_DISP_WDMA0;
#endif
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
	case DDP_COMPONENT_OVL0:
		return M4U_PORT_L0_OVL_RDMA0;
	case DDP_COMPONENT_OVL0_2L:
		return M4U_PORT_L1_OVL_2L_RDMA0;
	case DDP_COMPONENT_OVL1_2L:
		return M4U_PORT_L0_OVL_2L_RDMA1;
	case DDP_COMPONENT_OVL2_2L:
		return M4U_PORT_L1_OVL_2L_RDMA2;
	case DDP_COMPONENT_OVL3_2L:
		return M4U_PORT_L0_OVL_2L_RDMA3;
	case DDP_COMPONENT_RDMA0:
		return M4U_PORT_L0_DISP_RDMA0;
	case DDP_COMPONENT_RDMA1:
		return M4U_PORT_L1_DISP_RDMA1;
	case DDP_COMPONENT_WDMA0:
		return M4U_PORT_L0_DISP_WDMA0;
	case DDP_COMPONENT_WDMA1:
		return M4U_PORT_L1_DISP_WDMA1;
#endif
#if defined(CONFIG_MACH_MT6873)
	case DDP_COMPONENT_OVL0:
		return M4U_PORT_L0_OVL_RDMA0;
	case DDP_COMPONENT_OVL0_2L:
		return M4U_PORT_L1_OVL_2L_RDMA0;
	case DDP_COMPONENT_OVL2_2L:
		return M4U_PORT_L1_OVL_2L_RDMA2;
	case DDP_COMPONENT_RDMA0:
		return M4U_PORT_L0_DISP_RDMA0;
	case DDP_COMPONENT_RDMA4:
		return M4U_PORT_L1_DISP_RDMA4;
	case DDP_COMPONENT_WDMA0:
		return M4U_PORT_L0_DISP_WDMA0;
#endif

#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833) \
	|| defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6781)
	case DDP_COMPONENT_OVL0:
		return M4U_PORT_L0_OVL_RDMA0;
	case DDP_COMPONENT_OVL0_2L:
		return M4U_PORT_L1_OVL_2L_RDMA0;
	case DDP_COMPONENT_RDMA0:
		return M4U_PORT_L1_DISP_RDMA0;
	case DDP_COMPONENT_WDMA0:
		return M4U_PORT_L1_DISP_WDMA0;
#endif

#if defined(CONFIG_MACH_MT6877)
	case DDP_COMPONENT_OVL1_2L:
		return M4U_PORT_L1_OVL_2L_RDMA1;
#endif


	default:
		DDPPR_ERR("%s, unknown comp %d\n", __func__, comp_id);
		break;
	}

	return -EINVAL;
}

int __mtk_disp_set_module_bw(struct mm_qos_request *request, int comp_id,
			     unsigned int bandwidth, unsigned int bw_mode)
{
	int mode;

	if (bw_mode == DISP_BW_FBDC_MODE)
		mode = BW_COMP_DEFAULT;
	else
		mode = BW_COMP_NONE;

	DDPINFO("set module %d, bw %u\n", comp_id, bandwidth);
	bandwidth = bandwidth * 133 / 100;
	mm_qos_set_bw_request(request, bandwidth, mode);

	DRM_MMP_MARK(pmqos, comp_id, bandwidth);

	return 0;
}

void __mtk_disp_set_module_hrt(struct mm_qos_request *request,
			       unsigned int bandwidth)
{
	mm_qos_set_hrt_request(request, bandwidth);
}

int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp;
	unsigned int tmp;
	int i, j, ret = 0;

	tmp = bw;

	for (i = 0; i < DDP_PATH_NR; i++) {
		if (!(mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[i]))
			continue;
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, i) {
			ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
						   &tmp);
		}
	}

	if (ret == RDMA_REQ_HRT)
		tmp = mtk_drm_primary_frame_bw(crtc);

	mm_qos_set_hrt_request(&priv->hrt_bw_request, tmp);
	DRM_MMP_MARK(hrt_bw, 0, tmp);
	DDPINFO("set HRT bw %u\n", tmp);
	mm_qos_update_all_request(&priv->hrt_request_list);

	return ret;
}

void mtk_drm_pan_disp_set_hrt_bw(struct drm_crtc *crtc, const char *caller)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_display_mode *mode;
	unsigned int bw = 0;

	dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(dev_crtc);
	mode = &crtc->state->adjusted_mode;

	bw = _layering_get_frame_bw(crtc, mode);
	mtk_disp_set_hrt_bw(mtk_crtc, bw);
	DDPINFO("%s:pan_disp_set_hrt_bw: %u\n", caller, bw);
}

int mtk_disp_hrt_cond_change_cb(struct notifier_block *nb, unsigned long value,
				void *v)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dev_crtc);
	int i, ret;
	unsigned int hrt_idx;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	switch (value) {
	case BW_THROTTLE_START: /* CAM on */
		DDPMSG("DISP BW Throttle start\n");
		/* TODO: concider memory session */
		DDPINFO("CAM trigger repaint\n");
		hrt_idx = _layering_rule_get_hrt_idx();
		hrt_idx++;
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev_crtc->dev);
		for (i = 0; i < 5; ++i) {
			ret = wait_event_timeout(
				mtk_crtc->qos_ctx->hrt_cond_wq,
				atomic_read(&mtk_crtc->qos_ctx->hrt_cond_sig),
				HZ / 5);
			if (ret == 0)
				DDPINFO("wait repaint timeout %d\n", i);
			atomic_set(&mtk_crtc->qos_ctx->hrt_cond_sig, 0);
			if (atomic_read(&mtk_crtc->qos_ctx->last_hrt_idx) >=
			    hrt_idx)
				break;
		}
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		break;
	case BW_THROTTLE_END: /* CAM off */
		DDPMSG("DISP BW Throttle end\n");
		/* TODO: switch DC */
		break;
	default:
		break;
	}

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

struct notifier_block pmqos_hrt_notifier = {
	.notifier_call = mtk_disp_hrt_cond_change_cb,
};

int mtk_disp_hrt_bw_dbg(void)
{
	mtk_disp_hrt_cond_change_cb(NULL, BW_THROTTLE_START, NULL);

	return 0;
}
#endif

int mtk_disp_hrt_cond_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc;

	dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(dev_crtc);

	mtk_crtc->qos_ctx = vmalloc(sizeof(struct mtk_drm_qos_ctx));
	if (mtk_crtc->qos_ctx == NULL) {
		DDPPR_ERR("%s:allocate qos_ctx failed\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
#ifdef MTK_FB_MMDVFS_SUPPORT
void mtk_drm_mmdvfs_init(void)
{

	pm_qos_add_request(&mm_freq_request, PM_QOS_DISP_FREQ,
			   PM_QOS_MM_FREQ_DEFAULT_VALUE);

	mmdvfs_qos_get_freq_steps(PM_QOS_DISP_FREQ, g_freq_steps, &step_size);
}
#endif
static void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level,
			const char *caller)
{
	if (drm_crtc_index(crtc) != 0)
		return;

	if (level < 0 || level >= MAX_FREQ_STEP)
		level = -1;

	if (level == g_freq_level)
		return;

	g_freq_level = level;

	DDPINFO("%s set mmclk level: %d\n", caller, g_freq_level);

#ifdef MTK_FB_MMDVFS_SUPPORT
	if (g_freq_level >= 0)
		pm_qos_update_request(&mm_freq_request,
			g_freq_steps[g_freq_level]);
	else
		pm_qos_update_request(&mm_freq_request, 0);
#endif
}

void mtk_drm_set_mmclk_by_pixclk(struct drm_crtc *crtc,
	unsigned int pixclk, const char *caller)
{
	int i;

	if (pixclk >= g_freq_steps[0]) {
		DDPMSG("%s:error:pixleclk (%d) is to big for mmclk (%llu)\n",
			caller, pixclk, g_freq_steps[0]);
		mtk_drm_set_mmclk(crtc, 0, caller);
		return;
	}
	if (!pixclk) {
		mtk_drm_set_mmclk(crtc, -1, caller);
		return;
	}
	for (i = 1; i < step_size; i++) {
		if (pixclk >= g_freq_steps[i]) {
			mtk_drm_set_mmclk(crtc, i-1, caller);
			break;
		}
		if (i == step_size - 1)
			mtk_drm_set_mmclk(crtc, -1, caller);
	}
}
