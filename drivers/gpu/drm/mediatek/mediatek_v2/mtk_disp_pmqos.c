// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_layering_rule.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#ifdef MTK_DISP_MMQOS_SUPPORT
#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <soc/mediatek/mmqos.h>
#endif

static struct drm_crtc *dev_crtc;

/* add for mm qos */
#ifdef MTK_DISP_MMDVFS_SUPPORT
static struct regulator *mm_freq_request;
static u32 *g_freq_steps;
static int g_freq_level = -1;
static int step_size = 1;
#endif

#ifdef MTK_DISP_MMQOS_SUPPORT
void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event)
{
	int len;

	len = snprintf(buf, buf_len, "%s_%s", mtk_dump_comp_str(comp), qos_event);
}

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

int __mtk_disp_set_module_bw(struct icc_path *request, int comp_id,
			     unsigned int bandwidth, unsigned int bw_mode)
{
	DDPINFO("%s set %d bw = %u\n", __func__, comp_id, bandwidth);
	bandwidth = bandwidth * 133 / 100;

	icc_set_bw(request, MBps_to_icc(bandwidth), 0);

	DRM_MMP_MARK(pmqos, comp_id, bandwidth);

	return 0;
}

void __mtk_disp_set_module_hrt(struct icc_path *request,
			       unsigned int bandwidth)
{
	if (bandwidth > 0)
		icc_set_bw(request, 0, MTK_MMQOS_MAX_BW);
	else
		icc_set_bw(request, 0, MBps_to_icc(bandwidth));
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

	icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(tmp));
	DRM_MMP_MARK(hrt_bw, 0, tmp);
	DDPINFO("set HRT bw %u\n", tmp);

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

#ifdef MTK_DISP_MMDVFS_SUPPORT
static void mtk_drm_mmdvfs_get_avail_freq(struct device *dev)
{
	int i = 0;
	struct dev_pm_opp *opp;
	unsigned long freq;

	step_size = dev_pm_opp_get_opp_count(dev);
	g_freq_steps = kcalloc(step_size, sizeof(u32), GFP_KERNEL);
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		g_freq_steps[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
}
void mtk_drm_mmdvfs_init(struct device *dev)
{
	dev_pm_opp_of_add_table(dev);
	mm_freq_request = devm_regulator_get(dev, "dvfsrc-vcore");

	mtk_drm_mmdvfs_get_avail_freq(dev);
}

static void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level,
			const char *caller)
{
	struct dev_pm_opp *opp;
	unsigned long freq;
	int volt, ret;

	if (drm_crtc_index(crtc) != 0)
		return;

	if (level < 0 || level > (step_size - 1))
		level = -1;

	if (level == g_freq_level)
		return;

	g_freq_level = level;

	DDPINFO("%s set mmclk level: %d\n", caller, g_freq_level);

	if (g_freq_level >= 0)
		freq = g_freq_steps[g_freq_level];
	else
		freq = g_freq_steps[0];

	opp = dev_pm_opp_find_freq_ceil(crtc->dev->dev, &freq);
	volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	ret = regulator_set_voltage(mm_freq_request, volt, INT_MAX);

	if (ret)
		DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
}

void mtk_drm_set_mmclk_by_pixclk(struct drm_crtc *crtc,
	unsigned int pixclk, const char *caller)
{
	int i;
	unsigned long freq = pixclk * 1000000;

	if (freq > g_freq_steps[step_size - 1]) {
		DDPMSG("%s:error:pixleclk (%d) is to big for mmclk (%llu)\n",
			caller, freq, g_freq_steps[step_size - 1]);
		mtk_drm_set_mmclk(crtc, step_size - 1, caller);
		return;
	}
	if (!freq) {
		mtk_drm_set_mmclk(crtc, -1, caller);
		return;
	}
	for (i = step_size - 2 ; i >= 0; i--) {
		if (freq > g_freq_steps[i]) {
			mtk_drm_set_mmclk(crtc, i + 1, caller);
			break;
		}
		if (i == 0)
			mtk_drm_set_mmclk(crtc, -1, caller);
	}
}
#endif
