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
#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <soc/mediatek/mmqos.h>
#include <soc/mediatek/mmdvfs_v3.h>

#define CRTC_NUM		3
static struct drm_crtc *dev_crtc;
/* add for mm qos */
static struct clk *mm_clk;
static struct regulator *mm_freq_request;
static unsigned long *g_freq_steps;
static unsigned int lp_freq;
static int g_freq_level[CRTC_NUM] = {-1, -1, -1};
static bool g_freq_lp[CRTC_NUM] = {false, false, false};
static long g_freq;
static int step_size = 1;

void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event)
{
	int len;

	len = snprintf(buf, buf_len, "%s_%s", mtk_dump_comp_str(comp), qos_event);
	if (!(len > -1 && len < buf_len))
		DDPINFO("%s: snprintf return error", __func__);
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
	case DDP_COMPONENT_OVLSYS_WDMA2:
		return DISP_PMQOS_OVLSYS_WDMA2_BW;
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

int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp;
	unsigned int tmp, total = 0;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	int i, j, ret = 0;

	tmp = bw;

	for (i = 0; i < DDP_PATH_NR; i++) {
		if (!(mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[i]))
			continue;
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

	/* skip same HRT BW */
	if (priv->req_hrt[crtc_idx] == tmp)
		return 0;

	priv->req_hrt[crtc_idx] = tmp;

	for (i = 0; i < MAX_CRTC; ++i)
		total += priv->req_hrt[i];

	mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(total));
	DRM_MMP_MARK(hrt_bw, 0, tmp);
	DDPINFO("set CRTC %d HRT bw %u %u\n", crtc_idx, tmp, total);

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
		hrt_idx = _layering_rule_get_hrt_idx(drm_crtc_index(dev_crtc));
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
	atomic_set(&mtk_crtc->qos_ctx->last_hrt_idx, 0);
	mtk_crtc->qos_ctx->last_hrt_req = 0;
	mtk_crtc->qos_ctx->last_mmclk_req_idx = 0;

	if (drm_crtc_index(crtc) == 0 && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		mtk_mmqos_register_bw_throttle_notifier(&pmqos_hrt_notifier);

	return 0;
}

static void mtk_drm_mmdvfs_get_avail_freq(struct device *dev)
{
	int i = 0;
	struct dev_pm_opp *opp;
	unsigned long freq;
	int ret;

	step_size = dev_pm_opp_get_opp_count(dev);
	g_freq_steps = kcalloc(step_size, sizeof(unsigned long), GFP_KERNEL);
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		g_freq_steps[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}

	ret = of_property_read_u32(dev->of_node, "lp-mmclk-freq", &lp_freq);
	DDPINFO("%s lp_freq = %d\n", __func__, lp_freq);
}

void mtk_drm_mmdvfs_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	unsigned int index;
	int ret = 0;

	dev_pm_opp_of_add_table(dev);
	mtk_drm_mmdvfs_get_avail_freq(dev);

	/* MMDVFS V3 */
	ret = of_property_read_u32(node, "dvfs_clk_idx", &index);
	if (ret == 0) {
		mm_clk = of_clk_get(node, index);
		if (IS_ERR_OR_NULL(mm_clk))
			DDPPR_ERR("%s get dvfs clk failed\n", __func__);
		return;
	}

	/* MMDVFS V2 */
	DDPINFO("%s, try to use MMDVFS V2\n", __func__);
	mm_freq_request = devm_regulator_get_optional(dev, "mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(mm_freq_request))
		DDPPR_ERR("%s, get mmdvfs-dvfsrc-vcore failed\n", __func__);
}

unsigned int mtk_drm_get_mmclk_step_size(void)
{
	return step_size;
}

void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level, bool lp_mode,
			const char *caller)
{
	struct dev_pm_opp *opp;
	unsigned long freq;
	int volt, ret, idx, i;
	int final_lp_mode = true;
	int final_level = -1;
	int cnt = 0;

	idx = drm_crtc_index(crtc);

	DDPINFO("%s[%d] g_freq_level[idx=%d]: %d, g_freq_lp[idx=%d]: %d\n",
		__func__, __LINE__, idx, level, idx, lp_mode);

	/* only for crtc = 0, 1 */
	if (idx > 2)
		return;

	if (level < 0 || level > (step_size - 1))
		level = -1;

	if (level == g_freq_level[idx] && lp_mode == g_freq_lp[idx])
		return;

	g_freq_lp[idx] = lp_mode;
	g_freq_level[idx] = level;

	for (i = 0; i < CRTC_NUM; i++) {
		if (g_freq_level[i] == -1)
			cnt++;

		if (g_freq_level[i] != -1 && !g_freq_lp[i])
			final_lp_mode = false;

		if (g_freq_level[i] > final_level)
			final_level = g_freq_level[i];
	}

	if (cnt == CRTC_NUM)
		final_lp_mode = false;

	DDPINFO("%s set lp mode:%d\n", __func__, final_lp_mode);
	mmdvfs_set_lp_mode(final_lp_mode);

	if (final_level >= 0)
		freq = g_freq_steps[final_level];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d] final_level(freq=%d, %lu)\n",
		__func__, __LINE__, final_level, freq);

	if (!IS_ERR_OR_NULL(mm_clk)) {
		ret = clk_set_rate(mm_clk, freq);
		if (ret)
			DDPPR_ERR("%s:clk_set_rate fail\n", __func__);
		return;
	}

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

	g_freq = freq;

	if (freq > g_freq_steps[step_size - 1]) {
		DDPPR_ERR("%s:pixleclk (%d) is to big for mmclk (%llu)\n",
			caller, freq, g_freq_steps[step_size - 1]);
		mtk_drm_set_mmclk(crtc, step_size - 1, false, caller);
		return;
	}
	if (!freq) {
		mtk_drm_set_mmclk(crtc, -1, false, caller);
		return;
	}
	for (i = step_size - 2 ; i >= 0; i--) {
		if (freq > g_freq_steps[i]) {
			mtk_drm_set_mmclk(crtc, i + 1, false, caller);
			break;
		}
		if (i == 0) {
			if (!lp_freq || (lp_freq && (freq > lp_freq)))
				mtk_drm_set_mmclk(crtc, 0, false, caller);
			else
				mtk_drm_set_mmclk(crtc, 0, true, caller);
		}
	}
}

unsigned long mtk_drm_get_freq(struct drm_crtc *crtc, const char *caller)
{
	return g_freq;
}

unsigned long mtk_drm_get_mmclk(struct drm_crtc *crtc, const char *caller)
{
	int idx;
	unsigned long freq;

	if (!crtc || !g_freq_steps)
		return 0;

	idx = drm_crtc_index(crtc);

	if (g_freq_level[idx] >= 0)
		freq = g_freq_steps[g_freq_level[idx]];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d]g_freq_level[idx=%d]: %d (freq=%d)\n",
		__func__, __LINE__, idx, g_freq_level[idx], freq);

	return freq;
}

