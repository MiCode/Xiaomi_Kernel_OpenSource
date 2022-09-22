// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_layering_rule.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <soc/mediatek/mmqos.h>

#include "mtk_drm_assert.h"

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
#include <linux/interconnect.h>
extern u32 *disp_perfs;
#endif

#define CRTC_NUM		3
static struct drm_crtc *dev_crtc;
/* add for mm qos */
static struct regulator *mm_freq_request;
static u32 *g_freq_steps;
static int g_freq_level[CRTC_NUM] = {-1, -1, -1};
static long g_freq;
static int step_size = 1;

void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event)
{
	int len;

	len = snprintf(buf, buf_len, "%s_%s", mtk_dump_comp_str(comp), qos_event);
	if (len < 0)
		DDPPR_ERR("%s:snprintf error: %d\n", __func__, len);
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
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL2_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_NWCG_BW;
	case DDP_COMPONENT_OVL1_2L_NWCG:
	case DDP_COMPONENT_OVL3_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_NWCG_BW;
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

	mtk_icc_set_bw(request, MBps_to_icc(bandwidth), 0);

	DRM_MMP_MARK(pmqos, comp_id, bandwidth);

	return 0;
}

void __mtk_disp_set_module_hrt(struct icc_path *request,
			       unsigned int bandwidth)
{
	if (bandwidth > 0)
		mtk_icc_set_bw(request, 0, MTK_MMQOS_MAX_BW);
	else
		mtk_icc_set_bw(request, 0, MBps_to_icc(bandwidth));
}

static bool mtk_disp_check_segment(struct mtk_drm_crtc *mtk_crtc,
				struct mtk_drm_private *priv)
{
	bool ret = true;
	int hact = 0;
	int vact = 0;
	int vrefresh = 0;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s, mtk_crtc is NULL\n", __func__);
		return ret;
	}

	if (IS_ERR_OR_NULL(priv)) {
		DDPPR_ERR("%s, private is NULL\n", __func__);
		return ret;
	}

	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);

	switch (priv->seg_id) {
	case 1:
		if (hact >= 1440)
			ret = false;
		else if (hact >= 1080 && vrefresh > 168)
			ret = false;
		break;
	case 2:
		if (hact >= 1440 && vrefresh > 120)
			ret = false;
		else if (hact >= 1080 && vrefresh > 168)
			ret = false;
		break;
	case 3:
		if (hact >= 1440 && vrefresh > 120)
			ret = false;
		else if (hact >= 1080 && vrefresh > 180)
			ret = false;
		break;
	default:
		ret = true;
		break;
	}

/*
 *	DDPMSG("%s, segment:%d, mode(%d, %d, %d)\n",
 *			__func__, priv->seg_id, hact, vact, vrefresh);
 */

	if (ret == false)
		DDPPR_ERR("%s, check sement fail: segment:%d, mode(%d, %d, %d)\n",
			__func__, priv->seg_id, hact, vact, vrefresh);

	return ret;
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
		if (mtk_crtc->ddp_mode < DDP_MODE_NR) {
			if (!(mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[i]))
				continue;
		}
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, i) {
			ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
						   &tmp);
		}
		if (!mtk_crtc->is_dual_pipe)
			continue;
		for_each_comp_in_dual_pipe(comp, mtk_crtc, j, i)
			ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
					&tmp);
	}

	if (ret == RDMA_REQ_HRT)
		tmp = mtk_drm_primary_frame_bw(crtc);

	if (priv->data->mmsys_id == MMSYS_MT6895) {
		if (mtk_disp_check_segment(mtk_crtc, priv) == false)
			tmp = 1;
	}

	if (priv->data->mmsys_id == MMSYS_MT6879) {
		if (crtc->state && crtc->state->active) {
			if (tmp == 0 && mtk_drm_dal_enable()) {
				tmp = mtk_drm_primary_frame_bw(crtc) / 2;
				DDPMSG("%s: add HRT bw %u when active and only have dal layer\n",
					__func__, mtk_drm_primary_frame_bw(crtc) / 2);
			}
		} else {
			DDPMSG("%s: set HRT bw %u when suspend\n", __func__, tmp);
		}
	}

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
	if (tmp == 0)
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
	else if (tmp > 0 && tmp <= 3500)
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL1]);
	else if (tmp > 3500)
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL0]);
	else
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
#else
	mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(tmp));
#endif
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

	/* No need to repaint when display suspend */
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return 0;
	}

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

int mtk_disp_hrt_cond_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(dev_crtc);

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s:mtk_crtc is NULL\n", __func__);
		return -EINVAL;
	}

	priv = mtk_crtc->base.dev->dev_private;

	mtk_crtc->qos_ctx = vmalloc(sizeof(struct mtk_drm_qos_ctx));
	if (mtk_crtc->qos_ctx == NULL) {
		DDPPR_ERR("%s:allocate qos_ctx failed\n", __func__);
		return -ENOMEM;
	}
	memset(mtk_crtc->qos_ctx, 0, sizeof(struct mtk_drm_qos_ctx));
	if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		mtk_mmqos_register_bw_throttle_notifier(&pmqos_hrt_notifier);

	return 0;
}

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

unsigned int mtk_drm_get_mmclk_step_size(void)
{
	return step_size;
}

void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level,
			const char *caller)
{
	struct dev_pm_opp *opp;
	unsigned long freq;
	int volt, ret, idx, i;

	idx = drm_crtc_index(crtc);

	DDPINFO("%s[%d] g_freq_level[idx=%d]: %d\n",
		__func__, __LINE__, idx, level);

	/* only for crtc = 0, 1 */
	if (idx > 2)
		return;

	if (level < 0 || level > (step_size - 1))
		level = -1;

	if (level == g_freq_level[idx])
		return;

	g_freq_level[idx] = level;

	for (i = 0 ; i < CRTC_NUM; i++) {
		if (g_freq_level[i] > g_freq_level[idx]) {
			DDPINFO("%s[%d] g_freq_level[i=%d]=%d > g_freq_level[idx=%d]=%d\n",
				__func__, __LINE__, i, g_freq_level[i], idx, g_freq_level[idx]);
			return;
		}
	}

	if (g_freq_level[idx] >= 0)
		freq = g_freq_steps[g_freq_level[idx]];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d] g_freq_level[idx=%d](freq=%d)\n",
		__func__, __LINE__, idx, g_freq_level[idx], freq);

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
	int i, ret = 0;
	unsigned int idx = drm_crtc_index(crtc);
	unsigned long freq = pixclk * 1000000;
	unsigned int fps_dst = drm_mode_vrefresh(&crtc->state->mode);
	static bool mmclk_status;

	g_freq = freq;

	DRM_MMP_EVENT_START(mmclk, idx, fps_dst);
	if (mmclk_status == true) {
		DRM_MMP_MARK(mmclk, idx, fps_dst);
		DDPINFO("%s: double set mmclk\n", __func__);
	}
	mmclk_status = true;
	if (freq > g_freq_steps[step_size - 1]) {
		DDPMSG("%s:error:pixleclk (%lu) is to big for mmclk (%u)\n",
			caller, freq, g_freq_steps[step_size - 1]);
		mtk_drm_set_mmclk(crtc, step_size - 1, caller);
		ret = step_size - 1;
		goto end;
	}
	if (!freq) {
		mtk_drm_set_mmclk(crtc, -1, caller);
		ret = -1;
		goto end;
	}
	for (i = step_size - 2 ; i >= 0; i--) {
		if (freq > g_freq_steps[i]) {
			mtk_drm_set_mmclk(crtc, i + 1, caller);
			ret = i + 1;
			break;
		}
		if (i == 0) {
			mtk_drm_set_mmclk(crtc, 0, caller);
			ret = 0;
		}
	}

end:
	mmclk_status = false;
	DRM_MMP_EVENT_END(mmclk, pixclk, ret);
}

unsigned long mtk_drm_get_freq(struct drm_crtc *crtc, const char *caller)
{
	return g_freq;
}

unsigned long mtk_drm_get_mmclk(struct drm_crtc *crtc, const char *caller)
{
	int idx;
	unsigned long freq;

	idx = drm_crtc_index(crtc);

	if (g_freq_level[idx] >= 0)
		freq = g_freq_steps[g_freq_level[idx]];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d]g_freq_level[idx=%d]: %d (freq=%lu)\n",
		__func__, __LINE__, idx, g_freq_level[idx], freq);

	return freq;
}

