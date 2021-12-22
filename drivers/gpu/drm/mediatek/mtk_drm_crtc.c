/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <asm/barrier.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_fourcc.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/mailbox_controller.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <soc/mediatek/smi.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>


#include "mtk_drm_arr.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"
#include "mtk_writeback.h"
#include "mtk_fence.h"
#include "mtk_sync.h"
#include "mtk_drm_session.h"
#include "mtk_dump.h"
#include "mtk_drm_fb.h"
#include "mtk_rect.h"
#include "mtk_drm_ddp_addon.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_lowpower.h"
#include "mtk_drm_fbdev.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_mmp.h"
#include "mtk_disp_recovery.h"
#include "mtk_drm_arr.h"
#include "mtk_drm_trace.h"
#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"
#include "mtk_disp_ccorr.h"
#include "mtk_debug.h"
#ifdef CONFIG_MTK_SVP_ON_MTEE_SUPPORT
#include "tz_m4u.h"
#endif
/* *****Panel_Master*********** */
#include "mtk_fbconfig_kdebug.h"
#include "mtk_layering_rule_base.h"

static struct mtk_drm_property mtk_crtc_property[CRTC_PROP_MAX] = {
	{DRM_MODE_PROP_ATOMIC, "OVERLAP_LAYER_NUM", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "LAYERING_IDX", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "PRESENT_FENCE", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "SF_PRESENT_FENCE", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "DOZE_ACTIVE", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OUTPUT_ENABLE", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OUTPUT_BUFF_IDX", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OUTPUT_X", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OUTPUT_Y", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OUTPUT_WIDTH", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OUTPUT_HEIGHT", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OUTPUT_FB_ID", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "INTF_BUFF_IDX", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "DISP_MODE_IDX", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "HBM_ENABLE", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "COLOR_TRANSFORM", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "USER_SCEN", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "HDR_ENABLE", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "OVL_DSI_SEQ", 0, UINT_MAX, 0},
};

static struct cmdq_pkt *sb_cmdq_handle;
static unsigned int sb_backlight;

bool hdr_en;
static const char * const crtc_gce_client_str[] = {
	DECLARE_GCE_CLIENT(DECLARE_STR)};
#ifdef MTK_FB_MMDVFS_SUPPORT
static struct mtk_pm_qos_request mm_freq_request;
static u64 freq_steps[MAX_FREQ_STEP];
static u32 step_size;
#endif

unsigned int te_cnt;

struct drm_crtc *_get_context(void)
{
	static int is_context_inited;
	static struct drm_crtc g_context;

	if (!is_context_inited) {
		memset((void *)&g_context, 0, sizeof(
					struct drm_crtc));
		is_context_inited = 1;
	}

	return &g_context;
}

static void mtk_drm_crtc_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	drm_crtc_send_vblank_event(crtc, mtk_crtc->event);
	drm_crtc_vblank_put(crtc);
	mtk_crtc->event = NULL;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

static void mtk_drm_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	drm_crtc_handle_vblank(&mtk_crtc->base);
	if (mtk_crtc->pending_needs_vblank) {
		mtk_drm_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}
}

static void mtk_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	mtk_disp_mutex_put(mtk_crtc->mutex[0]);

	drm_crtc_cleanup(crtc);
}

static void mtk_drm_crtc_reset(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state;

	if (crtc->state) {
		__drm_atomic_helper_crtc_destroy_state(crtc->state);

		state = to_mtk_crtc_state(crtc->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		crtc->state = &state->base;
	}

	state->base.crtc = crtc;
}

static int mtk_drm_wait_blank(struct mtk_drm_crtc *mtk_crtc,
	bool blank, long timeout)
{
	int ret;

	ret = wait_event_timeout(mtk_crtc->state_wait_queue,
		mtk_crtc->crtc_blank == blank, timeout);

	return ret;
}

int mtk_drm_crtc_wait_blank(struct mtk_drm_crtc *mtk_crtc)
{
	int ret = 0;

	DDPMSG("%s wait TUI finish\n", __func__);
	while (mtk_crtc->crtc_blank == true) {
//		DDP_MUTEX_UNLOCK(&mtk_crtc->blank_lock, __func__, __LINE__);
		ret |= mtk_drm_wait_blank(mtk_crtc, false, HZ / 5);
//		DDP_MUTEX_LOCK(&mtk_crtc->blank_lock, __func__, __LINE__);
	}
	DDPMSG("%s TUI done state=%d\n", __func__,
		mtk_crtc->crtc_blank);

	return ret;
}

static void mtk_drm_crtc_addon_dump(struct drm_crtc *crtc,
			const struct mtk_addon_scenario_data *addon_data)
{
	int i, j;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	const struct mtk_addon_path_data *addon_path;
	enum addon_module module;
	struct mtk_ddp_comp *comp;

	if (!addon_data)
		return;

	for (i = 0; i < addon_data->module_num; i++) {
		module = addon_data->module_data[i].module;
		addon_path = mtk_addon_module_get_path(module);

		for (j = 0; j < addon_path->path_len; j++) {
			if (mtk_ddp_comp_get_type(addon_path->path[j])
				== MTK_DISP_VIRTUAL)
				continue;
			comp = priv->ddp_comp[addon_path->path[j]];
			mtk_dump_reg(comp);
		}
	}
}

void mtk_drm_crtc_dump(struct drm_crtc *crtc)
{
	int i, j;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	const struct mtk_addon_scenario_data *addon_data;
	struct mtk_ddp_comp *comp;
	int crtc_id = drm_crtc_index(crtc);
	struct mtk_panel_params *panel_ext = mtk_drm_get_lcm_ext_params(crtc);
	if (!priv->power_state) {
		DDPDUMP("DRM dev is not in power on state, skip %s\n",
			__func__);
		return;
	}
	if (crtc_id < 0) {
		DDPPR_ERR("%s: Invalid crtc_id:%d\n", __func__, crtc_id);
		return;
	}
	DDPINFO("%s\n", __func__);

	switch (priv->data->mmsys_id) {
	case MMSYS_MT2701:
		break;
	case MMSYS_MT2712:
		break;
	case MMSYS_MT8173:
		break;
	case MMSYS_MT6779:
		break;
	case MMSYS_MT6885:
		mmsys_config_dump_reg_mt6885(mtk_crtc->config_regs);
		mutex_dump_reg_mt6885(mtk_crtc->mutex[0]);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_dump_reg(comp);
		break;
	case MMSYS_MT6873:
	case MMSYS_MT6853:
	case MMSYS_MT6877:
	case MMSYS_MT6833:
	case MMSYS_MT6781:
		mmsys_config_dump_reg_mt6873(mtk_crtc->config_regs);
		mutex_dump_reg_mt6873(mtk_crtc->mutex[0]);
		break;
	default:
		pr_info("%s mtk drm not support mmsys id %d\n",
			__func__, priv->data->mmsys_id);
		break;
	}

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) mtk_dump_reg(comp);

	//addon from CWB
	if (mtk_crtc->cwb_info && mtk_crtc->cwb_info->enable
		&& !mtk_crtc->cwb_info->is_sec) {
		addon_data = mtk_addon_get_scenario_data(__func__, crtc,
						mtk_crtc->cwb_info->scn);
		mtk_drm_crtc_addon_dump(crtc, addon_data);
		if (mtk_crtc->is_dual_pipe) {
			addon_data = mtk_addon_get_scenario_data_dual
				(__func__, crtc, mtk_crtc->cwb_info->scn);
			mtk_drm_crtc_addon_dump(crtc, addon_data);
		}
	}

	//addon from layering rule
	if (!crtc->state)
		DDPDUMP("%s dump nothing for null state\n", __func__);
	else {
		state = to_mtk_crtc_state(crtc->state);
		addon_data = mtk_addon_get_scenario_data(__func__, crtc,
					state->lye_state.scn[crtc_id]);
		mtk_drm_crtc_addon_dump(crtc, addon_data);
	}

	if (panel_ext &&
		panel_ext->output_mode == MTK_PANEL_DSC_SINGLE_PORT) {
		comp = priv->ddp_comp[DDP_COMPONENT_DSC0];
		mtk_dump_reg(comp);
	}
}

static void mtk_drm_crtc_addon_analysis(struct drm_crtc *crtc,
			const struct mtk_addon_scenario_data *addon_data)
{
	int i, j;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	const struct mtk_addon_path_data *addon_path;
	enum addon_module module;
	struct mtk_ddp_comp *comp;

	if (!addon_data)
		return;

	for (i = 0; i < addon_data->module_num; i++) {
		module = addon_data->module_data[i].module;
		addon_path = mtk_addon_module_get_path(module);

		for (j = 0; j < addon_path->path_len; j++) {
			if (mtk_ddp_comp_get_type(addon_path->path[j])
				== MTK_DISP_VIRTUAL)
				continue;
			comp = priv->ddp_comp[addon_path->path[j]];
			mtk_dump_analysis(comp);
		}
	}
}

void mtk_drm_crtc_analysis(struct drm_crtc *crtc)
{
	int i, j;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	const struct mtk_addon_scenario_data *addon_data;
	struct mtk_ddp_comp *comp;
	int crtc_id = drm_crtc_index(crtc);
#ifndef CONFIG_FPGA_EARLY_PORTING
	if (!priv->power_state) {
		DDPDUMP("DRM dev is not in power on state, skip %s\n",
			__func__);
		return;
	}
#endif
	DDPFUNC("crtc%d\n", crtc_id);

	switch (priv->data->mmsys_id) {
	case MMSYS_MT2701:
		break;
	case MMSYS_MT2712:
		break;
	case MMSYS_MT8173:
		break;
	case MMSYS_MT6779:
		break;
	case MMSYS_MT6885:
		mmsys_config_dump_analysis_mt6885(mtk_crtc->config_regs);
		if (mtk_crtc->is_dual_pipe) {
			DDPDUMP("anlysis dual pipe\n");
			mtk_ddp_dual_pipe_dump(mtk_crtc);
			for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
				mtk_dump_analysis(comp);
				mtk_dump_reg(comp);
			}
		}
		mutex_dump_analysis_mt6885(mtk_crtc->mutex[0]);
		break;
	case MMSYS_MT6873:
		mmsys_config_dump_analysis_mt6873(mtk_crtc->config_regs);
		mutex_dump_analysis_mt6873(mtk_crtc->mutex[0]);
		break;
	case MMSYS_MT6853:
		mmsys_config_dump_analysis_mt6853(mtk_crtc->config_regs);
		mutex_dump_analysis_mt6853(mtk_crtc->mutex[0]);
		break;
	case MMSYS_MT6877:
		mmsys_config_dump_analysis_mt6877(mtk_crtc->config_regs);
		mutex_dump_analysis_mt6877(mtk_crtc->mutex[0]);
		break;
	case MMSYS_MT6833:
		mmsys_config_dump_analysis_mt6833(mtk_crtc->config_regs);
		mutex_dump_analysis_mt6833(mtk_crtc->mutex[0]);
		break;
	case MMSYS_MT6781:
		mmsys_config_dump_analysis_mt6781(mtk_crtc->config_regs);
		mutex_dump_analysis_mt6781(mtk_crtc->mutex[0]);
		break;
	default:
		pr_info("%s mtk drm not support mmsys id %d\n",
			__func__, priv->data->mmsys_id);
		break;
	}

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_dump_analysis(comp);

	//addon from CWB
	if (mtk_crtc->cwb_info && mtk_crtc->cwb_info->enable
		&& !mtk_crtc->cwb_info->is_sec) {
		addon_data = mtk_addon_get_scenario_data(__func__, crtc,
						mtk_crtc->cwb_info->scn);
		mtk_drm_crtc_addon_analysis(crtc, addon_data);
		if (mtk_crtc->is_dual_pipe) {
			addon_data = mtk_addon_get_scenario_data_dual
				(__func__, crtc, mtk_crtc->cwb_info->scn);
			mtk_drm_crtc_addon_analysis(crtc, addon_data);
		}
	}

	//addon from layering rule
	if (!crtc->state)
		DDPDUMP("%s dump nothing for null state\n", __func__);
	else if (crtc_id < 0)
		DDPPR_ERR("%s: Invalid crtc_id:%d\n", __func__, crtc_id);
	else {
		state = to_mtk_crtc_state(crtc->state);
		addon_data = mtk_addon_get_scenario_data(__func__, crtc,
					state->lye_state.scn[crtc_id]);
		mtk_drm_crtc_addon_analysis(crtc, addon_data);
	}
}

struct mtk_ddp_comp *mtk_ddp_comp_request_output(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	int i, j;

	for_each_comp_in_crtc_path_reverse(
		comp, mtk_crtc, i,
		j)
		if (mtk_ddp_comp_is_output(comp))
			return comp;

	/* This CRTC does not contain output comp */
	return NULL;
}

void mtk_crtc_change_output_mode(struct drm_crtc *crtc, int aod_en)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp)
		return;

	DDPINFO("%s\n", __func__);
	switch (comp->id) {
	case DDP_COMPONENT_DSI0:
	case DDP_COMPONENT_DSI1:
		mtk_ddp_comp_io_cmd(comp, NULL, DSI_CHANGE_MODE, &aod_en);
		break;
	default:
		break;
	}
}

bool mtk_crtc_is_connector_enable(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	bool enable = 0;

	if (comp == NULL || mtk_ddp_comp_get_type(comp->id) != MTK_DSI)
		return enable;

	if (comp->funcs && comp->funcs->io_cmd)
		comp->funcs->io_cmd(comp, NULL, CONNECTOR_IS_ENABLE, &enable);

	return enable;
}

static struct drm_crtc_state *
mtk_drm_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state, *old_state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	if (!crtc) {
		DDPPR_ERR("NULL crtc\n");
		kfree(state);
		return NULL;
	}

	if (crtc->state)
		__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	if (state->base.crtc != crtc)
		DDPAEE("%s:%d, invalid crtc:(%p,%p)\n",
			__func__, __LINE__,
			state->base.crtc, crtc);
	state->base.crtc = crtc;

	if (crtc->state) {
		old_state = to_mtk_crtc_state(crtc->state);
		state->lye_state = old_state->lye_state;
		state->rsz_src_roi = old_state->rsz_src_roi;
		state->rsz_dst_roi = old_state->rsz_dst_roi;
		state->prop_val[CRTC_PROP_DOZE_ACTIVE] =
			old_state->prop_val[CRTC_PROP_DOZE_ACTIVE];
	}

	return &state->base;
}

static void mtk_drm_crtc_destroy_state(struct drm_crtc *crtc,
				       struct drm_crtc_state *state)
{
	struct mtk_crtc_state *s;

	s = to_mtk_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(s);
}

static int mtk_drm_crtc_set_property(struct drm_crtc *crtc,
				     struct drm_crtc_state *state,
				     struct drm_property *property,
				     uint64_t val)
{
	struct drm_device *dev = crtc->dev;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(state);
	int index = drm_crtc_index(crtc);
	int ret = 0;
	int i;

	if (index < 0 || index >= MAX_CRTC)
		return -EINVAL;

	for (i = 0; i < CRTC_PROP_MAX; i++) {
		if (private->crtc_property[index][i] == property) {
			crtc_state->prop_val[i] = (unsigned int)val;
			DDPINFO("crtc:%d set property:%s %d\n",
					index, property->name,
					(unsigned int)val);
			return ret;
		}
	}

	DDPPR_ERR("fail to set property:%s %d\n", property->name,
		  (unsigned int)val);
	return -EINVAL;
}

static int mtk_drm_crtc_get_property(struct drm_crtc *crtc,
				     const struct drm_crtc_state *state,
				     struct drm_property *property,
				     uint64_t *val)
{
	struct drm_device *dev = crtc->dev;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(state);
	int ret = 0;
	int index = drm_crtc_index(crtc);
	int i;

	if (index < 0 || index >= MAX_CRTC) {
		DDPPR_ERR("%s invalid crtc index\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < CRTC_PROP_MAX; i++) {
		if (private->crtc_property[index][i] == property) {
			*val = crtc_state->prop_val[i];
			DDPINFO("get property:%s %lld\n", property->name, *val);
			return ret;
		}
	}

	DDPPR_ERR("fail to get property:%s %p\n", property->name, val);
	return -EINVAL;
}

struct mtk_ddp_comp *mtk_crtc_get_comp(struct drm_crtc *crtc,
				       unsigned int path_id,
				       unsigned int comp_idx)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx = mtk_crtc->ddp_ctx;

	if (mtk_crtc->ddp_mode > DDP_MINOR) {
		DDPPR_ERR("invalid ddp mode:%d!\n", mtk_crtc->ddp_mode);
		return NULL;
	}
	return ddp_ctx[mtk_crtc->ddp_mode].ddp_comp[path_id][comp_idx];
}

static void mtk_drm_crtc_lfr_update(struct drm_crtc *crtc,
					struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp =
		mtk_ddp_comp_request_output(mtk_crtc);

	mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_LFR_UPDATE, NULL);
}

static bool mtk_drm_crtc_mode_fixup(struct drm_crtc *crtc,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	/* Nothing to do here, but this callback is mandatory. */
	return true;
}

static void mtk_drm_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);

	state->pending_width = crtc->mode.hdisplay;
	state->pending_height = crtc->mode.vdisplay;
	state->pending_vrefresh = crtc->mode.vrefresh;
	wmb(); /* Make sure the above parameters are set before update */
	state->pending_config = true;
}

static int mtk_crtc_enable_vblank_thread(void *data)
{
	int ret = 0;
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	do {
		ret = wait_event_interruptible(
			mtk_crtc->vblank_enable_wq,
			atomic_read(&mtk_crtc->vblank_enable_task_active));
		if (ret) {
			DDPPR_ERR("%s wait_event_interruptible failed, ret = %d\n", __func__, ret);
			return ret;
		}
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		if (mtk_crtc->enabled)
			mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		atomic_set(&mtk_crtc->vblank_enable_task_active, 0);

	} while (!kthread_should_stop());

	return 0;
}

int mtk_drm_crtc_enable_vblank(struct drm_device *drm, unsigned int pipe)
{
	struct mtk_drm_private *priv = drm->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(priv->crtc[pipe]);
	struct mtk_ddp_comp *comp = mtk_crtc_get_comp(&mtk_crtc->base, 0, 0);

	mtk_crtc->vblank_en = 1;

	if (!mtk_crtc->enabled) {
		CRTC_MMP_MARK(pipe, enable_vblank, 0xFFFFFFFF,
			0xFFFFFFFF);
		return 0;
	}

	/* We only consider CRTC0 vsync so far, need to modify to DPI, DPTX */
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLE_MGR) &&
	    drm_crtc_index(&mtk_crtc->base) == 0) {
		/* The enable vblank is called in spinlock, so we create another
		 * thread to kick idle mode for cmd mode vsync
		 */
		atomic_set(&mtk_crtc->vblank_enable_task_active, 1);
		wake_up_interruptible(&mtk_crtc->vblank_enable_wq);
	}

	CRTC_MMP_MARK(pipe, enable_vblank, (unsigned long)comp,
			(unsigned long)&mtk_crtc->base);

	return 0;
}

static void bl_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct drm_crtc *crtc = cb_data->crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	int id;
	unsigned int bl_idx = 0;

	DDPINFO("%s()\n", __func__);
	id = drm_crtc_index(crtc);
	if (id == 0) {
		bl_idx = *(unsigned int *)(cmdq_buf->va_base +
			DISP_SLOT_CUR_BL_IDX);
		CRTC_MMP_MARK(id, bl_cb, bl_idx, 0);
	}

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

int mtk_drm_setbacklight(struct drm_crtc *crtc, unsigned int level)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct mtk_cmdq_cb_data *cb_data;
	static unsigned int bl_cnt;
	struct cmdq_pkt_buffer *cmdq_buf;
	bool is_frame_mode;
	int index = drm_crtc_index(crtc);
	if (m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS])
		sb_backlight = level;

	CRTC_MMP_EVENT_START(index, backlight, (unsigned long)crtc,
			level);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!(mtk_crtc->enabled)) {
		DDPINFO("Sleep State set backlight stop --crtc not ebable\n");
		mutex_unlock(&mtk_crtc->lock);
		CRTC_MMP_EVENT_END(index, backlight, 0, 0);

		return -EINVAL;
	}

	if (!comp) {
		DDPINFO("%s no output comp\n", __func__);
		mutex_unlock(&mtk_crtc->lock);
		CRTC_MMP_EVENT_END(index, backlight, 0, 1);

		return -EINVAL;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		DDPPR_ERR("cb data creation failed\n");
		CRTC_MMP_EVENT_END(index, backlight, 0, 2);

		return 0;
	}

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);

	if (m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS] &&
		sb_cmdq_handle != NULL) {
		cmdq_handle = sb_cmdq_handle;
		sb_cmdq_handle = NULL;
	} else {
		cmdq_handle =
			cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
	}

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
	}

	/* set backlight */
	if (comp->funcs && comp->funcs->io_cmd)
		comp->funcs->io_cmd(comp, cmdq_handle, DSI_SET_BL, &level);

	if (0) {
		/* add counter to check update frequency */
		cmdq_buf = &(mtk_crtc->gce_obj.buf);
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
				cmdq_buf->pa_base +
				DISP_SLOT_CUR_BL_IDX,
				bl_cnt, ~0);
		CRTC_MMP_MARK(index, backlight, bl_cnt, 0);
		bl_cnt++;
	}

	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}


	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;

	if (cmdq_pkt_flush_threaded(cmdq_handle, bl_cmdq_cb, cb_data) < 0)
		DDPPR_ERR("failed to flush bl_cmdq_cb\n");

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	CRTC_MMP_EVENT_END(index, backlight, (unsigned long)crtc,
			level);

	return 0;
}

int mtk_drm_setbacklight_grp(struct drm_crtc *crtc, unsigned int level)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	bool is_frame_mode;
	int index = drm_crtc_index(crtc);

	CRTC_MMP_EVENT_START(index, backlight_grp, (unsigned long)crtc,
			level);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!(mtk_crtc->enabled)) {
		DDPINFO("%s:%d, crtc is slept\n", __func__,
				__LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, backlight_grp, 0, 0);

		return -EINVAL;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(crtc);
	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
	}

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);

	if (comp && comp->funcs && comp->funcs->io_cmd)
		comp->funcs->io_cmd(comp, cmdq_handle, DSI_SET_BL_GRP, &level);

	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	CRTC_MMP_EVENT_END(index, backlight_grp, (unsigned long)crtc,
			level);

	return 0;
}

static void mtk_drm_crtc_wk_lock(struct drm_crtc *crtc, bool get,
	const char *func, int line);

int mtk_drm_aod_setbacklight(struct drm_crtc *crtc, unsigned int level)
{

	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp, *comp;
	struct cmdq_pkt *cmdq_handle;
	bool is_frame_mode;
	struct cmdq_client *client;
	int i, j;
	struct mtk_crtc_state *crtc_state;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	crtc_state = to_mtk_crtc_state(crtc->state);
	if (mtk_crtc->enabled && !crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		DDPINFO("%s:%d, crtc is on and not in doze mode\n",
			__func__, __LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return -EINVAL;
	}

	CRTC_MMP_EVENT_START(0, backlight, 0x123,
			level);
	mtk_drm_crtc_wk_lock(crtc, 1, __func__, __LINE__);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		mtk_drm_crtc_wk_lock(crtc, 0, __func__, __LINE__);

		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return -ENODEV;
	}

	client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	if (!mtk_crtc->enabled) {
		/* 1. power on mtcmos */
		mtk_drm_top_clk_prepare_enable(crtc->dev);

		if (mtk_crtc_with_trigger_loop(crtc))
			mtk_crtc_start_trig_loop(crtc);

		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_ENABLE, NULL);

		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_dump_analysis(comp);
	}

	/* send LCM CMD */
	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);

	if (is_frame_mode)
		cmdq_handle =
			cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	else
		cmdq_handle =
			cmdq_pkt_create(
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_clear_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
	}

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);
	/* set backlight */
	if (output_comp->funcs && output_comp->funcs->io_cmd)
		output_comp->funcs->io_cmd(output_comp,
			cmdq_handle, DSI_SET_BL_AOD, &level);

	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	if (!mtk_crtc->enabled) {

		if (mtk_crtc_with_trigger_loop(crtc))
			mtk_crtc_stop_trig_loop(crtc);

		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_DISABLE, NULL);

		mtk_drm_top_clk_disable_unprepare(crtc->dev);
	}

	mtk_drm_crtc_wk_lock(crtc, 0, __func__, __LINE__);
	CRTC_MMP_EVENT_END(0, backlight, 0x123,
			level);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

int mtk_drm_crtc_set_panel_hbm(struct drm_crtc *crtc, bool en)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct cmdq_pkt *cmdq_handle;
	bool is_frame_mode;
	bool state = false;

	if (!(comp && comp->funcs && comp->funcs->io_cmd))
		return -EINVAL;

	comp->funcs->io_cmd(comp, NULL, DSI_HBM_GET_STATE, &state);
	if (state == en)
		return 0;

	if (!(mtk_crtc->enabled)) {
		DDPINFO("%s: skip, slept\n", __func__);
		return -EINVAL;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	DDPINFO("%s:set LCM hbm en:%d\n", __func__, en);

	is_frame_mode = mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base);
	cmdq_handle =
		cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	if (is_frame_mode) {
		cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
	}

	comp->funcs->io_cmd(comp, cmdq_handle, DSI_HBM_SET, &en);

	if (is_frame_mode) {
		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	}

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	return 0;
}

int mtk_drm_crtc_hbm_wait(struct drm_crtc *crtc, bool en)
{
	struct mtk_panel_params *panel_ext = mtk_drm_get_lcm_ext_params(crtc);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);
	bool wait = false;
	unsigned int wait_count = 0;

	if (!(comp && comp->funcs && comp->funcs->io_cmd))
		return -EINVAL;

	comp->funcs->io_cmd(comp, NULL, DSI_HBM_GET_WAIT_STATE, &wait);
	if (wait != true)
		return 0;

	if (!panel_ext)
		return -EINVAL;

	wait_count = en ? panel_ext->hbm_en_time : panel_ext->hbm_dis_time;

	DDPINFO("LCM hbm %s wait %u-TE\n", en ? "enable" : "disable",
		wait_count);

	while (wait_count) {
		mtk_drm_idlemgr_kick(__func__, crtc, 0);
		wait_count--;
		comp->funcs->io_cmd(comp, NULL, DSI_HBM_WAIT, NULL);
	}

	wait = false;
	comp->funcs->io_cmd(comp, NULL, DSI_HBM_SET_WAIT_STATE, &wait);

	return 0;
}

void mtk_drm_crtc_disable_vblank(struct drm_device *drm, unsigned int pipe)
{
	struct mtk_drm_private *priv = drm->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(priv->crtc[pipe]);
	struct mtk_ddp_comp *comp = mtk_crtc_get_comp(&mtk_crtc->base, 0, 0);

	DDPINFO("%s\n", __func__);

	mtk_crtc->vblank_en = 0;

	CRTC_MMP_MARK(pipe, disable_vblank, (unsigned long)comp,
			(unsigned long)&mtk_crtc->base);
}

bool mtk_crtc_get_vblank_timestamp(struct drm_device *dev, unsigned int pipe,
				 int *max_error,
				 ktime_t *vblank_time,
				 bool in_vblank_irq)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(priv->crtc[pipe]);

	*vblank_time = timeval_to_ktime(mtk_crtc->vblank_time);
	return true;
}

/*dp 4k resolution 3840*2160*/
bool mtk_crtc_is_dual_pipe(struct drm_crtc *crtc)
{
	struct mtk_panel_params *panel_ext =
		mtk_drm_get_lcm_ext_params(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if ((drm_crtc_index(crtc) == 0) &&
		panel_ext &&
		panel_ext->output_mode == MTK_PANEL_DUAL_PORT) {
		DDPFUNC();
		return true;
	}

	if ((drm_crtc_index(crtc) == 1) &&
		(crtc->state->adjusted_mode.hdisplay == 1920*2)) {
		DDPFUNC();
		return true;
	}

	if ((drm_crtc_index(crtc) == 0) &&
		mtk_drm_helper_get_opt(priv->helper_opt,
			    MTK_DRM_OPT_PRIM_DUAL_PIPE) &&
		panel_ext &&
		panel_ext->output_mode == MTK_PANEL_DSC_SINGLE_PORT &&
		panel_ext->dsc_params.slice_mode == 1) {
		DDPFUNC();
		return true;
	}

	return false;
}

void mtk_crtc_prepare_dual_pipe(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j;
	enum mtk_ddp_comp_id comp_id;
	struct mtk_ddp_comp *comp;
	struct device *dev = mtk_crtc->base.dev->dev;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct drm_crtc *crtc = &mtk_crtc->base;

	if (mtk_crtc_is_dual_pipe(&(mtk_crtc->base))) {
		mtk_crtc->is_dual_pipe = true;
#ifdef MTK_FB_MMDVFS_SUPPORT
		if (drm_crtc_index(&mtk_crtc->base) == 1) {
			mtk_pm_qos_update_request(&mm_freq_request, freq_steps[1]);
			DDPFUNC("current freq: %d\n",
			mtk_pm_qos_request(PM_QOS_DISP_FREQ));
		}
#endif
	} else {
		mtk_crtc->is_dual_pipe = false;

#ifdef MTK_FB_MMDVFS_SUPPORT
		if (drm_crtc_index(&mtk_crtc->base) == 1)
			mtk_pm_qos_update_request(&mm_freq_request, freq_steps[3]);
		DDPFUNC("crtc%d single pipe current freq: %d\n",
			drm_crtc_index(&mtk_crtc->base),
			mtk_pm_qos_request(PM_QOS_DISP_FREQ));
#endif
		return;
	}

	for (j = 0; j < DDP_SECOND_PATH; j++) {
		mtk_crtc->dual_pipe_ddp_ctx.ddp_comp_nr[j] =
			mtk_crtc->path_data->dual_path_len[j];
		mtk_crtc->dual_pipe_ddp_ctx.ddp_comp[j] = devm_kmalloc_array(
			dev, mtk_crtc->path_data->dual_path_len[j],
			sizeof(struct mtk_ddp_comp *), GFP_KERNEL);
		DDPFUNC("j:%d,com_nr:%d,path_len:%d\n",
			j, mtk_crtc->dual_pipe_ddp_ctx.ddp_comp_nr[j],
			mtk_crtc->path_data->dual_path_len[j]);
	}

	for_each_comp_id_in_dual_pipe(comp_id, mtk_crtc->path_data, i, j) {
		DDPFUNC("prepare comp id in dual pipe %d\n", comp_id);
		if (comp_id < 0) {
			DDPPR_ERR("%s: Invalid comp_id:%d\n", __func__, comp_id);
			return;
		}
		if (mtk_ddp_comp_get_type(comp_id) == MTK_DISP_VIRTUAL) {
			struct mtk_ddp_comp *comp;

			comp = kzalloc(sizeof(*comp), GFP_KERNEL);
			comp->id = comp_id;
			mtk_crtc->dual_pipe_ddp_ctx.ddp_comp[i][j] = comp;
			continue;
		}

		comp = priv->ddp_comp[comp_id];
		mtk_crtc->dual_pipe_ddp_ctx.ddp_comp[i][j] = comp;
		comp->mtk_crtc = mtk_crtc;
	}

	/*4k 30 use DISP_MERGE1, 4k 60 use DSC*/
	if ((drm_crtc_index(&mtk_crtc->base) == 1) &&
		(crtc->state->adjusted_mode.vrefresh == 30)) {
		comp = priv->ddp_comp[DDP_COMPONENT_MERGE1];
		mtk_crtc->dual_pipe_ddp_ctx.ddp_comp[0][2] = comp;
		comp->mtk_crtc = mtk_crtc;
	}
}

static void user_cmd_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct drm_crtc *crtc = cb_data->crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	int id;
	unsigned int user_cmd_idx = 0;

	id = drm_crtc_index(crtc);
	if (id == 0) {
		user_cmd_idx = *(unsigned int *)(cmdq_buf->va_base +
			DISP_SLOT_CUR_USER_CMD_IDX);
		CRTC_MMP_MARK(id, user_cmd_cb, user_cmd_idx, 0);
	}

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

bool mtk_crtc_in_dual_pipe(struct mtk_drm_crtc *mtk_crtc, struct mtk_ddp_comp *comp)
{
	int i = 0, j = 0;
	struct mtk_ddp_comp *comp_dual;

	for_each_comp_in_dual_pipe(comp_dual, mtk_crtc, i, j) {
		if (comp->id == comp_dual->id)
			return true;
	}
	return false;
}

int mtk_crtc_user_cmd(struct drm_crtc *crtc, struct mtk_ddp_comp *comp,
		unsigned int cmd, void *params)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_cmdq_cb_data *cb_data;
	static unsigned int user_cmd_cnt;
	struct cmdq_pkt_buffer *cmdq_buf;
	struct DRM_DISP_CCORR_COEF_T *ccorr_config;

	int index = 0;

	if (!mtk_crtc) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p\n",
				__func__, __LINE__, crtc);
		return -1;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	CRTC_MMP_EVENT_START(index, user_cmd, (unsigned long)crtc,
			(unsigned long)comp);

	if ((!crtc) || (!comp)) {
		DDPPR_ERR("%s:%d, invalid arg:(0x%p,0x%p)\n",
				__func__, __LINE__,
				crtc, comp);
		CRTC_MMP_MARK(index, user_cmd, 0, 0);
		goto err;
	}

	index = drm_crtc_index(crtc);
	if (index) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p, index:%d\n",
				__func__, __LINE__, crtc, index);
		CRTC_MMP_MARK(index, user_cmd, 0, 1);
		goto err;
	}

	if (!(mtk_crtc->enabled)) {
		DDPINFO("%s:%d, slepted\n", __func__, __LINE__);
		CRTC_MMP_EVENT_END(index, user_cmd, 0, 2);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("cb data creation failed\n");
		CRTC_MMP_MARK(index, user_cmd, 0, 3);
		goto err;
	}

	CRTC_MMP_MARK(index, user_cmd, user_cmd_cnt, 0);
	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	CRTC_MMP_MARK(index, user_cmd, user_cmd_cnt, 1);

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else {
		if (mtk_crtc->is_dual_pipe) {
			if (!mtk_crtc_in_dual_pipe(mtk_crtc, comp))
				mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
								DDP_FIRST_PATH, 0);
		} else {
			mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,	DDP_FIRST_PATH, 0);
		}
	}
	CRTC_MMP_MARK(index, user_cmd, user_cmd_cnt, 2);

	/* set user command */
	if (comp && comp->funcs && comp->funcs->user_cmd && !comp->blank_mode)
		comp->funcs->user_cmd(comp, cmdq_handle, cmd, (void *)params);
	else {
		DDPPR_ERR("%s:%d, invalid comp:(0x%p,0x%p)\n",
				__func__, __LINE__, comp, comp->funcs);
		CRTC_MMP_MARK(index, user_cmd, 0, 4);
		goto err2;
	}
	CRTC_MMP_MARK(index, user_cmd, user_cmd_cnt, 3);

	/* add counter to check update frequency */
	cmdq_buf = &(mtk_crtc->gce_obj.buf);
	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			   cmdq_buf->pa_base +
				   DISP_SLOT_CUR_USER_CMD_IDX,
			   user_cmd_cnt, ~0);
	CRTC_MMP_MARK(index, user_cmd, user_cmd_cnt, 4);
	user_cmd_cnt++;

	if (m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {
		if (comp->id == DDP_COMPONENT_CCORR0 && cmd == 0) {
			ccorr_config = params;
			if (ccorr_config->silky_bright_flag == 1 &&
				ccorr_config->FinalBacklight != sb_backlight) {
				sb_cmdq_handle = cmdq_handle;
				kfree(cb_data);
			} else {
				cb_data->crtc = crtc;
				cb_data->cmdq_handle = cmdq_handle;
				if (cmdq_pkt_flush_threaded(cmdq_handle,
					user_cmd_cmdq_cb, cb_data) < 0)
					DDPPR_ERR("failed to flush user_cmd\n");
			}
		} else {
			cb_data->crtc = crtc;
			cb_data->cmdq_handle = cmdq_handle;
			if (cmdq_pkt_flush_threaded(cmdq_handle,
				user_cmd_cmdq_cb, cb_data) < 0)
				DDPPR_ERR("failed to flush user_cmd\n");
		}
	} else {
		cb_data->crtc = crtc;
		cb_data->cmdq_handle = cmdq_handle;
		if (cmdq_pkt_flush_threaded(cmdq_handle, user_cmd_cmdq_cb, cb_data) < 0)
			DDPPR_ERR("failed to flush user_cmd\n");
	}

	CRTC_MMP_EVENT_END(index, user_cmd, (unsigned long)cmd,
			(unsigned long)params);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;

err:
	CRTC_MMP_EVENT_END(index, user_cmd, 0, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return -1;

err2:
	kfree(cb_data);
	CRTC_MMP_EVENT_END(index, user_cmd, 0, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return -1;
}

/* power on all modules on this CRTC */
void mtk_crtc_ddp_prepare(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j, k, ddp_mode;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	const struct mtk_addon_scenario_data *addon_data;
	const struct mtk_addon_path_data *addon_path;
	enum addon_module module;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_panel_params *panel_ext =
	    mtk_drm_get_lcm_ext_params(crtc);

	for_each_comp_in_all_crtc_mode(comp, mtk_crtc, i, j, ddp_mode)
		mtk_ddp_comp_prepare(comp);

	for (i = 0; i < ADDON_SCN_NR; i++) {
		addon_data = mtk_addon_get_scenario_data(__func__,
							 &mtk_crtc->base, i);
		if (!addon_data)
			break;

		for (j = 0; j < addon_data->module_num; j++) {
			module = addon_data->module_data[j].module;
			addon_path = mtk_addon_module_get_path(module);

			for (k = 0; k < addon_path->path_len; k++) {
				comp = priv->ddp_comp[addon_path->path[k]];
				mtk_ddp_comp_prepare(comp);
			}
		}
	}
	if (panel_ext &&
		panel_ext->output_mode == MTK_PANEL_DSC_SINGLE_PORT) {
		comp = priv->ddp_comp[DDP_COMPONENT_DSC0];
		mtk_ddp_comp_clk_prepare(comp);
	}
	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			mtk_ddp_comp_clk_prepare(comp);
			mtk_ddp_comp_prepare(comp);
		}
		for (i = 0; i < ADDON_SCN_NR; i++) {
			addon_data = mtk_addon_get_scenario_data_dual(__func__,
								 &mtk_crtc->base, i);
			if (!addon_data)
				break;

			for (j = 0; j < addon_data->module_num; j++) {
				module = addon_data->module_data[j].module;
				addon_path = mtk_addon_module_get_path(module);

				for (k = 0; k < addon_path->path_len; k++) {
					comp = priv->ddp_comp[addon_path->path[k]];
					mtk_ddp_comp_prepare(comp);
				}
			}

		}
	}
	/*TODO , Move to prop place rdma4/5 VDE from DP_VDE*/
	if (drm_crtc_index(crtc) == 1)
		writel_relaxed(0x220000, mtk_crtc->config_regs + 0xE10);
}

void mtk_crtc_ddp_unprepare(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j, k, ddp_mode;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	const struct mtk_addon_scenario_data *addon_data;
	const struct mtk_addon_path_data *addon_path;
	enum addon_module module;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_panel_params *panel_ext =
	    mtk_drm_get_lcm_ext_params(crtc);

	for_each_comp_in_all_crtc_mode(comp, mtk_crtc, i, j, ddp_mode)
		mtk_ddp_comp_unprepare(comp);

	for (i = 0; i < ADDON_SCN_NR; i++) {
		addon_data = mtk_addon_get_scenario_data(__func__,
							 &mtk_crtc->base, i);
		if (!addon_data)
			break;

		for (j = 0; j < addon_data->module_num; j++) {
			module = addon_data->module_data[j].module;
			addon_path = mtk_addon_module_get_path(module);

			for (k = 0; k < addon_path->path_len; k++) {
				comp = priv->ddp_comp[addon_path->path[k]];
				mtk_ddp_comp_unprepare(comp);
			}
		}
	}
	if (panel_ext && panel_ext->output_mode == MTK_PANEL_DSC_SINGLE_PORT) {
		comp = priv->ddp_comp[DDP_COMPONENT_DSC0];
		mtk_ddp_comp_clk_unprepare(comp);
	}

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			mtk_ddp_comp_clk_unprepare(comp);
			mtk_ddp_comp_unprepare(comp);
		}

		for (i = 0; i < ADDON_SCN_NR; i++) {
			addon_data = mtk_addon_get_scenario_data_dual(__func__,
								 &mtk_crtc->base, i);
			if (!addon_data)
				break;

			for (j = 0; j < addon_data->module_num; j++) {
				module = addon_data->module_data[j].module;
				addon_path = mtk_addon_module_get_path(module);

				for (k = 0; k < addon_path->path_len; k++) {
					comp = priv->ddp_comp[addon_path->path[k]];
					mtk_ddp_comp_unprepare(comp);
				}
			}
		}
	}
#ifdef MTK_FB_MMDVFS_SUPPORT
	/*restore default mm freq, PM_QOS_MM_FREQ_DEFAULT_VALUE*/
	if (drm_crtc_index(&mtk_crtc->base) == 1)
		mtk_pm_qos_update_request(&mm_freq_request, 0);
#endif

}
#ifdef MTK_DRM_ADVANCE
static struct mtk_ddp_comp *
mtk_crtc_get_plane_comp(struct drm_crtc *crtc,
			struct mtk_plane_state *plane_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = NULL;
	int i, j;

	if (plane_state->comp_state.comp_id == 0)
		return mtk_crtc_get_comp(crtc, 0, 0);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i,
				       j)
		if (comp->id == plane_state->comp_state.comp_id) {
			DDPINFO("%s i:%d, ovl_comp_id:%d\n",
				__func__, i,
				plane_state->comp_state.comp_id);
			DDPINFO("lye_id:%d, ext_lye_id:%d\n",
				plane_state->comp_state.lye_id,
				plane_state->comp_state.ext_lye_id);
			return comp;
		}

	return comp;
}

static int mtk_crtc_get_dc_fb_size(struct drm_crtc *crtc)
{
	/* DC buffer color format is RGB888 */
	return crtc->state->adjusted_mode.vdisplay *
	       crtc->state->adjusted_mode.hdisplay * 3;
}

static void mtk_crtc_cwb_set_sec(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_plane_comp_state *comp_state;
	struct mtk_cwb_info *cwb_info;
	int i;
	uint32_t layer_caps;

	cwb_info = mtk_crtc->cwb_info;

	if (!cwb_info)
		return;

	cwb_info->is_sec = false;
	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		comp_state = &(plane_state->comp_state);
		layer_caps = comp_state->layer_caps;

		if (layer_caps & MTK_LAYERING_OVL_ONLY) {
			DDPDBG("[capture] plane%d has secure content, layer_caps:%d!!",
				i, layer_caps);
			cwb_info->is_sec = true;
		}
	}
}

static void _mtk_crtc_cwb_addon_module_disconnect(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	struct cmdq_pkt *cmdq_handle)
{
	int i;
	const struct mtk_addon_scenario_data *addon_data;
	const struct mtk_addon_scenario_data *addon_data_dual;
	const struct mtk_addon_module_data *addon_module;
	union mtk_addon_config addon_config;
	int index = drm_crtc_index(crtc);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_cwb_info *cwb_info;

	cwb_info = mtk_crtc->cwb_info;
	if (index != 0 || mtk_crtc_is_dc_mode(crtc) ||
		!cwb_info)
		return;

	addon_data = mtk_addon_get_scenario_data(__func__, crtc,
						cwb_info->scn);
	if (!addon_data)
		return;

	if (mtk_crtc->is_dual_pipe) {
		addon_data_dual = mtk_addon_get_scenario_data_dual
			(__func__, crtc, cwb_info->scn);

		if (!addon_data_dual)
			return;
	}

	for (i = 0; i < addon_data->module_num; i++) {
		addon_module = &addon_data->module_data[i];
		addon_config.config_type.module = addon_module->module;
		addon_config.config_type.type = addon_module->type;

		if (addon_module->type == ADDON_AFTER &&
			addon_module->module == DISP_WDMA0) {
			if (mtk_crtc->is_dual_pipe) {
				/* disconnect left pipe */
				mtk_addon_disconnect_after(crtc, ddp_mode, addon_module,
							  &addon_config, cmdq_handle);
				/* disconnect right pipe */
				addon_module = &addon_data_dual->module_data[i];
				mtk_addon_disconnect_after(crtc, ddp_mode, addon_module,
							  &addon_config, cmdq_handle);
			} else {
				mtk_addon_disconnect_after(crtc, ddp_mode, addon_module,
								  &addon_config,
								  cmdq_handle);
			}
		} else
			DDPPR_ERR("addon type1:%d + module:%d not support\n",
					  addon_module->type,
					  addon_module->module);
	}
}

static void _mtk_crtc_lye_addon_module_disconnect(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	struct mtk_lye_ddp_state *lye_state, struct cmdq_pkt *cmdq_handle)
{
	int i;
	const struct mtk_addon_scenario_data *addon_data;
	const struct mtk_addon_scenario_data *addon_data_dual;
	const struct mtk_addon_module_data *addon_module;
	union mtk_addon_config addon_config;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	addon_data = mtk_addon_get_scenario_data(__func__, crtc,
					lye_state->scn[drm_crtc_index(crtc)]);
	if (!addon_data)
		return;

	if (mtk_crtc->is_dual_pipe) {
		addon_data_dual = mtk_addon_get_scenario_data_dual
			(__func__, crtc, lye_state->scn[drm_crtc_index(crtc)]);
	}

	if (!addon_data_dual) {
		return;
	}

	for (i = 0; i < addon_data->module_num; i++) {
		addon_module = &addon_data->module_data[i];
		addon_config.config_type.module = addon_module->module;
		addon_config.config_type.type = addon_module->type;

		if (addon_module->type == ADDON_BETWEEN &&
		    (addon_module->module == DISP_RSZ ||
		    addon_module->module == DISP_RSZ_v2)) {
			int w = crtc->state->adjusted_mode.hdisplay;
			int h = crtc->state->adjusted_mode.vdisplay;
			struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
			struct mtk_ddp_comp *output_comp;
			struct mtk_rect rsz_roi = {0, 0, w, h};

			output_comp = mtk_ddp_comp_request_output(mtk_crtc);
			if (output_comp &&
				drm_crtc_index(crtc) == 0) {
				rsz_roi.width = mtk_ddp_comp_io_cmd(
					output_comp, NULL,
					DSI_GET_VIRTUAL_WIDTH, NULL);
				rsz_roi.height = mtk_ddp_comp_io_cmd(
					output_comp, NULL,
					DSI_GET_VIRTUAL_HEIGH, NULL);
			}

			if (mtk_crtc->is_dual_pipe)
				rsz_roi.width /= 2;

			addon_config.addon_rsz_config.rsz_src_roi = rsz_roi;
			addon_config.addon_rsz_config.rsz_dst_roi = rsz_roi;
			addon_config.addon_rsz_config.lc_tgt_layer =
				lye_state->lc_tgt_layer;

			mtk_addon_disconnect_between(
				crtc, ddp_mode, addon_module, &addon_config,
				cmdq_handle);

			if (mtk_crtc->is_dual_pipe) {
				addon_module = &addon_data_dual->module_data[i];
				addon_config.config_type.module = addon_module->module;
				addon_config.config_type.type = addon_module->type;

				mtk_addon_disconnect_between(
					crtc, ddp_mode, addon_module, &addon_config,
					cmdq_handle);
			}
		} else
			DDPPR_ERR("addon type:%d + module:%d not support\n",
				  addon_module->type, addon_module->module);
	}
}

static void _mtk_crtc_atmoic_addon_module_disconnect(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	struct mtk_lye_ddp_state *lye_state, struct cmdq_pkt *cmdq_handle)
{
	_mtk_crtc_cwb_addon_module_disconnect(
			crtc, ddp_mode, cmdq_handle);

	_mtk_crtc_lye_addon_module_disconnect(
			crtc, ddp_mode, lye_state, cmdq_handle);
}

static void
_mtk_crtc_cwb_addon_module_connect(
				      struct drm_crtc *crtc,
				      unsigned int ddp_mode,
				      struct cmdq_pkt *cmdq_handle)
{
	int i;
	const struct mtk_addon_scenario_data *addon_data;
	const struct mtk_addon_scenario_data *addon_data_dual;
	const struct mtk_addon_module_data *addon_module;
	union mtk_addon_config addon_config;
	int index = drm_crtc_index(crtc);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_cwb_info *cwb_info;
	unsigned int buf_idx;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	cwb_info = mtk_crtc->cwb_info;
	if (index != 0 || mtk_crtc_is_dc_mode(crtc) ||
		!cwb_info)
		return;

	mtk_crtc_cwb_set_sec(crtc);
	if (!cwb_info->enable || cwb_info->is_sec ||
			priv->need_cwb_path_disconnect)
		return;

	addon_data = mtk_addon_get_scenario_data(__func__, crtc,
					cwb_info->scn);
	if (!addon_data)
		return;

	if (mtk_crtc->is_dual_pipe) {
		addon_data_dual = mtk_addon_get_scenario_data_dual
			(__func__, crtc, cwb_info->scn);

		if (!addon_data_dual)
			return;
	}

	for (i = 0; i < addon_data->module_num; i++) {
		addon_module = &addon_data->module_data[i];
		addon_config.config_type.module = addon_module->module;
		addon_config.config_type.type = addon_module->type;

		if (addon_module->type == ADDON_AFTER &&
		    addon_module->module == DISP_WDMA0) {
			buf_idx = cwb_info->buf_idx;
			addon_config.addon_wdma_config.wdma_src_roi =
				cwb_info->src_roi;
			addon_config.addon_wdma_config.wdma_dst_roi =
				cwb_info->buffer[buf_idx].dst_roi;
			addon_config.addon_wdma_config.addr =
				cwb_info->buffer[buf_idx].addr_mva;
			addon_config.addon_wdma_config.fb =
				cwb_info->buffer[buf_idx].fb;
			addon_config.addon_wdma_config.p_golden_setting_context
				= __get_golden_setting_context(mtk_crtc);
			if (mtk_crtc->is_dual_pipe) {
				int w = crtc->state->adjusted_mode.hdisplay;
				struct mtk_rect src_roi_l;
				struct mtk_rect src_roi_r;
				struct mtk_rect dst_roi_l;
				struct mtk_rect dst_roi_r;
				unsigned int r_buff_off = 0;

				src_roi_l = src_roi_r = cwb_info->src_roi;
				dst_roi_l = dst_roi_r = cwb_info->buffer[buf_idx].dst_roi;

				src_roi_l.x = 0;
				src_roi_l.width = w/2;
				src_roi_r.x = 0;
				src_roi_r.width = w/2;

				if (cwb_info->buffer[buf_idx].dst_roi.x +
					cwb_info->buffer[buf_idx].dst_roi.width < w/2) {
				/* handle source ROI locate in left pipe*/
					dst_roi_r.x = 0;
					dst_roi_r.y = 0;
					dst_roi_r.width = 0;
					dst_roi_r.height = 0;
				} else if (cwb_info->buffer[buf_idx].dst_roi.x >= w/2) {
				/* handle source ROI locate in right pipe*/
					dst_roi_l.x = 0;
					dst_roi_l.y = 0;
					dst_roi_l.width = 0;
					dst_roi_l.height = 0;
					dst_roi_r.x = cwb_info->buffer[buf_idx].dst_roi.x - w/2;
				} else {
				/* handle source ROI locate in both display pipe*/
					dst_roi_l.width = w/2 - cwb_info->buffer[buf_idx].dst_roi.x;
					dst_roi_r.x = 0;
					dst_roi_r.width =
						cwb_info->buffer[buf_idx].dst_roi.width -
						dst_roi_l.width;
					r_buff_off = dst_roi_l.width;
				}
				DDPINFO("cwb (%u, %u %u, %u) (%u ,%u %u,%u)\n",
					cwb_info->src_roi.width, cwb_info->src_roi.height,
					cwb_info->src_roi.x, cwb_info->src_roi.y,
					cwb_info->buffer[buf_idx].dst_roi.width,
					cwb_info->buffer[buf_idx].dst_roi.height,
					cwb_info->buffer[buf_idx].dst_roi.x,
					cwb_info->buffer[buf_idx].dst_roi.y);
				DDPDBG("L s(%u,%u %u,%u), d(%u,%u %u,%u)\n",
					src_roi_l.width, src_roi_l.height,
					src_roi_l.x, src_roi_l.y,
					dst_roi_l.width, dst_roi_l.height,
					dst_roi_l.x, dst_roi_l.y);
				DDPDBG("R s(%u,%u %u,%u), d(%u,%u %u,%u)\n",
					src_roi_r.width, src_roi_r.height,
					src_roi_r.x, src_roi_r.y,
					dst_roi_r.width, dst_roi_r.height,
					dst_roi_r.x, dst_roi_r.y);
				addon_config.addon_wdma_config.wdma_src_roi =
					src_roi_l;
				addon_config.addon_wdma_config.wdma_dst_roi =
					dst_roi_l;

				/* connect left pipe */
				mtk_addon_connect_after(crtc, ddp_mode, addon_module,
							  &addon_config, cmdq_handle);

				addon_module = &addon_data_dual->module_data[i];
				addon_config.addon_wdma_config.wdma_src_roi =
					src_roi_r;
				addon_config.addon_wdma_config.wdma_dst_roi =
					dst_roi_r;
				addon_config.addon_wdma_config.addr += r_buff_off * 3;
				/* connect right pipe */
				mtk_addon_connect_after(crtc, ddp_mode, addon_module,
							  &addon_config, cmdq_handle);
			} else
				mtk_addon_connect_after(crtc, ddp_mode, addon_module,
						  &addon_config, cmdq_handle);
		} else
			DDPPR_ERR("addon type:%d + module:%d not support\n",
				  addon_module->type, addon_module->module);
	}
}

static void
_mtk_crtc_lye_addon_module_connect(
				      struct drm_crtc *crtc,
				      unsigned int ddp_mode,
				      struct mtk_lye_ddp_state *lye_state,
				      struct cmdq_pkt *cmdq_handle)
{
	int i;
	const struct mtk_addon_scenario_data *addon_data;
	const struct mtk_addon_scenario_data *addon_data_dual;
	const struct mtk_addon_module_data *addon_module;
	union mtk_addon_config addon_config;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	addon_data = mtk_addon_get_scenario_data(__func__, crtc,
					lye_state->scn[drm_crtc_index(crtc)]);
	if (!addon_data)
		return;

	if (mtk_crtc->is_dual_pipe) {
		addon_data_dual = mtk_addon_get_scenario_data_dual
			(__func__, crtc, lye_state->scn[drm_crtc_index(crtc)]);

		if (!addon_data_dual)
			return;
	}

	for (i = 0; i < addon_data->module_num; i++) {
		addon_module = &addon_data->module_data[i];
		addon_config.config_type.module = addon_module->module;
		addon_config.config_type.type = addon_module->type;

		if (addon_module->type == ADDON_BETWEEN &&
		    (addon_module->module == DISP_RSZ ||
		    addon_module->module == DISP_RSZ_v2)) {
			struct mtk_crtc_state *state =
				to_mtk_crtc_state(crtc->state);

			addon_config.addon_rsz_config.rsz_src_roi =
				state->rsz_src_roi;
			addon_config.addon_rsz_config.rsz_dst_roi =
				state->rsz_dst_roi;
			addon_config.addon_rsz_config.lc_tgt_layer =
				lye_state->lc_tgt_layer;

			if (mtk_crtc->is_dual_pipe) {
				if (state->rsz_param[0].in_len) {
					memcpy(&(addon_config.addon_rsz_config.rsz_param),
						&state->rsz_param[0], sizeof(struct mtk_rsz_param));

					addon_config.addon_rsz_config.rsz_src_roi.width =
						state->rsz_param[0].in_len;
					addon_config.addon_rsz_config.rsz_dst_roi.width =
						state->rsz_param[0].out_len;
					addon_config.addon_rsz_config.rsz_dst_roi.x =
						state->rsz_param[0].out_x;
					mtk_addon_connect_between(crtc, ddp_mode, addon_module,
							  &addon_config, cmdq_handle);
				}

				if (state->rsz_param[1].in_len) {
					memcpy(&(addon_config.addon_rsz_config.rsz_param),
						&state->rsz_param[1], sizeof(struct mtk_rsz_param));

					addon_config.addon_rsz_config.rsz_src_roi.width =
						state->rsz_param[1].in_len;
					addon_config.addon_rsz_config.rsz_dst_roi.width =
						state->rsz_param[1].out_len;
					addon_config.addon_rsz_config.rsz_dst_roi.x =
						state->rsz_param[1].out_x;
					addon_module = &addon_data_dual->module_data[i];
					addon_config.config_type.module = addon_module->module;
					addon_config.config_type.type = addon_module->type;

					mtk_addon_connect_between(crtc, ddp_mode, addon_module,
							  &addon_config, cmdq_handle);
				}
			} else {
				mtk_addon_connect_between(crtc, ddp_mode, addon_module,
							  &addon_config, cmdq_handle);
			}

		} else
			DDPPR_ERR("addon type:%d + module:%d not support\n",
				  addon_module->type, addon_module->module);
	}
}

static void
_mtk_crtc_atmoic_addon_module_connect(
				      struct drm_crtc *crtc,
				      unsigned int ddp_mode,
				      struct mtk_lye_ddp_state *lye_state,
				      struct cmdq_pkt *cmdq_handle)
{
	_mtk_crtc_cwb_addon_module_connect(
				   crtc, ddp_mode, cmdq_handle);

	_mtk_crtc_lye_addon_module_connect(
			crtc, ddp_mode, lye_state, cmdq_handle);
}

void mtk_crtc_cwb_path_disconnect(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct cmdq_pkt *handle;
	int ddp_mode = mtk_crtc->ddp_mode;

	if (!mtk_crtc->cwb_info || !mtk_crtc->cwb_info->enable) {
		priv->need_cwb_path_disconnect = true;
		priv->cwb_is_preempted = true;
		return;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	priv->need_cwb_path_disconnect = true;
	mtk_crtc_pkt_create(&handle, crtc, client);
	mtk_crtc_wait_frame_done(mtk_crtc, handle, DDP_FIRST_PATH, 0);
	_mtk_crtc_cwb_addon_module_disconnect(crtc, ddp_mode, handle);
	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
	priv->cwb_is_preempted = true;

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

static void mtk_crtc_atmoic_ddp_config(struct drm_crtc *crtc,
				struct mtk_drm_lyeblob_ids *lyeblob_ids,
				struct cmdq_pkt *cmdq_handle)
{
	struct mtk_lye_ddp_state *lye_state, *old_lye_state;
	struct drm_property_blob *blob;
	struct drm_device *dev = crtc->dev;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (lyeblob_ids->ddp_blob_id) {
		blob = drm_property_lookup_blob(dev, lyeblob_ids->ddp_blob_id);

		if (blob == NULL) {
			DDPPR_ERR("drm_property_lookup_blob return NULL\n");
			return;
		}
		lye_state = (struct mtk_lye_ddp_state *)blob->data;
		drm_property_blob_put(blob);
		old_lye_state = &state->lye_state;

		_mtk_crtc_atmoic_addon_module_disconnect(crtc,
							mtk_crtc->ddp_mode,
							old_lye_state,
							cmdq_handle);

		/* When open VDS path switch feature, Don't need RSZ */
		if (!(mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH) &&
			priv->need_vds_path_switch))
			_mtk_crtc_atmoic_addon_module_connect(crtc,
				mtk_crtc->ddp_mode,
				lye_state,
				cmdq_handle);

		state->lye_state = *lye_state;
	}
}

static void mtk_crtc_free_ddpblob_ids(struct drm_crtc *crtc,
				struct mtk_drm_lyeblob_ids *lyeblob_ids)
{
	struct drm_device *dev = crtc->dev;
	struct drm_property_blob *blob;

	if (lyeblob_ids->ddp_blob_id) {
		blob = drm_property_lookup_blob(dev, lyeblob_ids->ddp_blob_id);
		drm_property_blob_put(blob);
		if (blob && kref_read(&(blob->base.refcount)))
			drm_property_blob_put(blob);

		list_del(&lyeblob_ids->list);
		kfree(lyeblob_ids);
	}
}

static void mtk_crtc_free_lyeblob_ids(struct drm_crtc *crtc,
				struct mtk_drm_lyeblob_ids *lyeblob_ids)
{
	struct drm_device *dev = crtc->dev;
	struct drm_property_blob *blob;
	int32_t blob_id;
	int i, j;

	for (j = 0; j < MAX_CRTC; j++) {
		if (!((lyeblob_ids->ref_cnt_mask >> j) & 0x1))
			continue;

		for (i = 0; i < OVL_LAYER_NR; i++) {
			blob_id = lyeblob_ids->lye_plane_blob_id[j][i];
			if (blob_id > 0) {
				blob = drm_property_lookup_blob(dev, blob_id);
				drm_property_blob_put(blob);
				if (blob &&
					kref_read(&(blob->base.refcount)))
					drm_property_blob_put(blob);
			}
		}
	}
}

unsigned int dual_pipe_comp_mapping(unsigned int comp_id)
{
	unsigned int ret = 0;

	switch (comp_id) {
	case DDP_COMPONENT_OVL0:
		ret = DDP_COMPONENT_OVL1;
		break;
	case DDP_COMPONENT_OVL0_2L:
		ret = DDP_COMPONENT_OVL1_2L;
		break;
	case DDP_COMPONENT_OVL2_2L:
		ret = DDP_COMPONENT_OVL3_2L;
		break;
	case DDP_COMPONENT_AAL0:
		ret = DDP_COMPONENT_AAL1;
		break;
	case DDP_COMPONENT_WDMA0:
		ret = DDP_COMPONENT_WDMA1;
		break;
	default:
		DDPMSG("unknown comp %u for %s\n", comp_id, __func__);
	}

	return ret;
}

static void mtk_crtc_get_plane_comp_state(struct drm_crtc *crtc,
					  struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_plane_comp_state *comp_state;
	int i, j, k;

	for (i = mtk_crtc->layer_nr - 1; i >= 0; i--) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state;
		struct mtk_ddp_comp *comp = NULL;

		plane_state = to_mtk_plane_state(plane->state);
		comp_state = &(plane_state->comp_state);
		/* TODO: check plane_state by pending.enable */
		if (plane_state->base.visible) {
			for_each_comp_in_cur_crtc_path(
				comp, mtk_crtc, j,
				k) {
				if (comp->id != comp_state->comp_id)
					continue;

				mtk_ddp_comp_layer_off(
					comp,
					comp_state->lye_id,
					comp_state->ext_lye_id,
					cmdq_handle);

				if (mtk_crtc->is_dual_pipe) {
					struct mtk_drm_private *priv =
						mtk_crtc->base.dev->dev_private;

					comp = priv->ddp_comp
						[dual_pipe_comp_mapping
						(comp_state->comp_id)];
					mtk_ddp_comp_layer_off(
							comp,
							comp_state->lye_id,
							comp_state->ext_lye_id,
							cmdq_handle);
				}
				break;
			}

		}
		/* Set the crtc to plane state for releasing fence purpose.*/
		plane_state->crtc = crtc;

		mtk_plane_get_comp_state(plane, &plane_state->comp_state, crtc,
					 0);
	}
}
unsigned int mtk_drm_primary_frame_bw(struct drm_crtc *i_crtc)
{
	unsigned long long bw = 0;
	struct drm_crtc *crtc = i_crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	if (drm_crtc_index(i_crtc) != 0) {
		DDPPR_ERR("%s no support CRTC%u", __func__,
			drm_crtc_index(i_crtc));

		drm_for_each_crtc(crtc, i_crtc->dev) {
			if (drm_crtc_index(crtc) == 0)
				break;
		}
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL,
				GET_FRAME_HRT_BW_BY_DATARATE, &bw);

	return (unsigned int)bw;
}

static unsigned int overlap_to_bw(struct drm_crtc *crtc,
	unsigned int overlap_num)
{
	unsigned int bw_base = mtk_drm_primary_frame_bw(crtc);
	unsigned int bw = bw_base * overlap_num / 2;

	return bw;
}

static void mtk_crtc_update_hrt_state(struct drm_crtc *crtc,
				      unsigned int frame_weight,
				      struct cmdq_pkt *cmdq_handle)

{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	unsigned int bw = overlap_to_bw(crtc, frame_weight);

	DDPINFO("%s bw=%d, last_hrt_req=%d\n",
		__func__, bw, mtk_crtc->qos_ctx->last_hrt_req);

	/* Only update HRT information on path with HRT comp */
	if (bw > mtk_crtc->qos_ctx->last_hrt_req) {
#ifdef MTK_FB_MMDVFS_SUPPORT
		mtk_disp_set_hrt_bw(mtk_crtc, bw);
#endif
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			       cmdq_buf->pa_base + DISP_SLOT_CUR_HRT_LEVEL,
			       NO_PENDING_HRT, ~0);
	} else if (bw < mtk_crtc->qos_ctx->last_hrt_req) {
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			       cmdq_buf->pa_base + DISP_SLOT_CUR_HRT_LEVEL,
			       bw, ~0);
	}
	mtk_crtc->qos_ctx->last_hrt_req = bw;
	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
		       cmdq_buf->pa_base + DISP_SLOT_CUR_HRT_IDX,
		       crtc_state->prop_val[CRTC_PROP_LYE_IDX], ~0);
}

#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833) || \
	defined(CONFIG_MACH_MT6877)
static void mtk_crtc_update_hrt_state_ex(struct drm_crtc *crtc,
				      struct mtk_drm_lyeblob_ids *lyeblob_ids,
				      struct cmdq_pkt *cmdq_handle)

{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	unsigned int bw = overlap_to_bw(crtc, lyeblob_ids->frame_weight);
	int crtc_idx = drm_crtc_index(crtc);
	unsigned int  ovl0_2l_no_compress_num =
		HRT_GET_NO_COMPRESS_FLAG(lyeblob_ids->hrt_num);
	unsigned int max_fps = mtk_crtc->max_fps;

	DDPINFO("%s bw=%d, last_hrt_req=%d\n",
			__func__, bw, mtk_crtc->qos_ctx->last_hrt_req);

	DDPINFO("%s CRTC%u bw:%d, no_compress_num:%d max_fps:%d\n",
		__func__, crtc_idx, bw, ovl0_2l_no_compress_num, max_fps);

	/* Workaround for 120hz SMI larb BW limitation */
	if (crtc_idx == 0 && max_fps == 120) {
		if (ovl0_2l_no_compress_num == 1 &&
			bw < 2944) {
			bw = 2944;
			DDPINFO("%s CRTC%u dram freq to 1600hz\n",
				__func__, crtc_idx);
		} else if (ovl0_2l_no_compress_num == 2 &&
			bw < 3433) {
			bw = 3433;
			DDPINFO("%s CRTC%u dram freq to 2400hz\n",
				__func__, crtc_idx);
		}
	}

	/* Only update HRT information on path with HRT comp */
	if (bw > mtk_crtc->qos_ctx->last_hrt_req) {
#ifdef MTK_FB_MMDVFS_SUPPORT
		mtk_disp_set_hrt_bw(mtk_crtc, bw);
#endif
		mtk_crtc->qos_ctx->last_hrt_req = bw;
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			       cmdq_buf->pa_base + DISP_SLOT_CUR_HRT_LEVEL,
			       NO_PENDING_HRT, ~0);
	} else if (bw < mtk_crtc->qos_ctx->last_hrt_req) {
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			       cmdq_buf->pa_base + DISP_SLOT_CUR_HRT_LEVEL,
			       bw, ~0);
	}

	mtk_crtc->qos_ctx->last_hrt_req = bw;

	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
		       cmdq_buf->pa_base + DISP_SLOT_CUR_HRT_IDX,
		       crtc_state->prop_val[CRTC_PROP_LYE_IDX], ~0);
}
#endif

static void copy_drm_disp_mode(struct drm_display_mode *src,
	struct drm_display_mode *dst)
{
	dst->clock       = src->clock;
	dst->hdisplay    = src->hdisplay;
	dst->hsync_start = src->hsync_start;
	dst->hsync_end   = src->hsync_end;
	dst->htotal      = src->htotal;
	dst->vdisplay    = src->vdisplay;
	dst->vsync_start = src->vsync_start;
	dst->vsync_end   = src->vsync_end;
	dst->vtotal      = src->vtotal;
	dst->vrefresh    = src->vrefresh;
}

struct golden_setting_context *
__get_golden_setting_context(struct mtk_drm_crtc *mtk_crtc)
{
	static struct golden_setting_context gs_ctx[MAX_CRTC];
	struct drm_crtc *crtc = &mtk_crtc->base;
	int idx = drm_crtc_index(&mtk_crtc->base);

	/* default setting */
	gs_ctx[idx].is_dc = 0;

	/* primary_display */
	switch (idx) {
	case 0:
		gs_ctx[idx].is_vdo_mode =
				mtk_crtc_is_frame_trigger_mode(crtc) ? 0 : 1;
		gs_ctx[idx].dst_width = crtc->state->adjusted_mode.hdisplay;
		gs_ctx[idx].dst_height = crtc->state->adjusted_mode.vdisplay;
		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
			struct mtk_panel_params *params;

			params = mtk_crtc->panel_ext->params;
			if (params->dyn_fps.switch_en == 1 &&
				params->dyn_fps.vact_timing_fps != 0)
				gs_ctx[idx].vrefresh =
					params->dyn_fps.vact_timing_fps;
			else
				gs_ctx[idx].vrefresh =
					crtc->state->adjusted_mode.vrefresh;
		} else
			gs_ctx[idx].vrefresh =
				crtc->state->adjusted_mode.vrefresh;
		break;
	case 1:
		/* TO DO: need more smart judge */
		gs_ctx[idx].is_vdo_mode = 1;
		gs_ctx[idx].dst_width = crtc->state->adjusted_mode.hdisplay;
		gs_ctx[idx].dst_height = crtc->state->adjusted_mode.vdisplay;
		break;
	case 2:
		/* TO DO: need more smart judge */
		gs_ctx[idx].is_vdo_mode = 0;
		break;
	}

	return &gs_ctx[idx];
}

unsigned int mtk_crtc_get_idle_interval(struct drm_crtc *crtc, unsigned int fps)
{

	unsigned int idle_interval = mtk_drm_get_idle_check_interval(crtc);
	/*calculate the timeout to enter idle in ms*/
	if (idle_interval > 50)
		return 0;
	idle_interval = (3 * 1000) / fps + 1;

	DDPMSG("[fps]:%s,[fps->idle interval][%d fps->%d ms]\n",
		__func__, fps, idle_interval);

	return idle_interval;
}

static void mtk_crtc_disp_mode_switch_begin(struct drm_crtc *crtc,
	struct drm_crtc_state *old_state, struct mtk_crtc_state *mtk_state,
	struct cmdq_pkt *cmdq_handle)
{
	struct mtk_crtc_state *old_mtk_state = to_mtk_crtc_state(old_state);
	struct drm_display_mode *mode;
	struct mtk_ddp_config cfg;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	unsigned int fps_src, fps_dst;
	unsigned int i, j;
	unsigned int fps_chg_index = 0;
	unsigned int _idle_timeout = 50;/*ms*/
	int en = 1;
	struct mtk_ddp_comp *output_comp;
#ifdef CONFIG_MTK_MT6382_BDG
	unsigned int crtc_id = 0;
#endif
	/* Check if disp_mode_idx change */
	if (old_mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX] ==
		mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX])
		return;

	DDPMSG("%s no delay from %u to %u\n", __func__,
		old_mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX],
		mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX]);

	/* Update mode & adjusted_mode in CRTC */
	mode = mtk_drm_crtc_avail_disp_mode(crtc,
		mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX]);

	fps_src = crtc->state->mode.vrefresh;
	fps_dst = mode->vrefresh;

	mtk_drm_trace_begin("%s from %dFPS to %dFPS\n",
		__func__, fps_src, fps_dst);

	copy_drm_disp_mode(mode, &crtc->state->mode);
	drm_mode_set_crtcinfo(&crtc->state->mode, 0);

	copy_drm_disp_mode(mode, &crtc->state->adjusted_mode);
	drm_mode_set_crtcinfo(&crtc->state->adjusted_mode, 0);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp) {
		mtk_ddp_comp_io_cmd(output_comp, NULL, DYN_FPS_INDEX,
				old_state);
		fps_chg_index = output_comp->mtk_crtc->fps_change_index;
	}
	//to do fps change index adjust
	if (fps_chg_index &
		(DYNFPS_DSI_HFP | DYNFPS_DSI_MIPI_CLK)) {
		/*ToDo HFP/MIPI CLOCK solution*/
		DDPMSG("%s,Update RDMA golden_setting\n", __func__);

	/* Update RDMA golden_setting */
		cfg.w = crtc->state->adjusted_mode.hdisplay;
		cfg.h = crtc->state->adjusted_mode.vdisplay;
		cfg.vrefresh = crtc->state->adjusted_mode.vrefresh;
		cfg.bpc = mtk_crtc->bpc;
		cfg.p_golden_setting_context =
			__get_golden_setting_context(mtk_crtc);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				MTK_IO_CMD_RDMA_GOLDEN_SETTING, &cfg);
	}
	mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_LFR_SET, &en);
	/* pull up mm clk if dst fps is higher than src fps */
	if (output_comp && fps_dst >= fps_src)
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_MMCLK_BY_DATARATE,
				&en);

	/* Change DSI mipi clk & send LCM cmd */
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_TIMING_CHANGE,
				old_state);

	drm_invoke_fps_chg_callbacks(crtc->state->adjusted_mode.vrefresh);

	/* update framedur_ns for VSYNC report */
	drm_calc_timestamping_constants(crtc, &crtc->state->mode);

	/* update idle timeout*/
	_idle_timeout = mtk_crtc_get_idle_interval(crtc, fps_dst);
	if (_idle_timeout > 0)
		mtk_drm_set_idle_check_interval(crtc, _idle_timeout);

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	mtk_drm_trace_end();
#ifdef CONFIG_MTK_MT6382_BDG
	crtc_id = drm_crtc_index(crtc);
	CRTC_MMP_EVENT_END(crtc_id, mode_switch, 0, 0);
#endif
}

bool already_free;
bool mtk_crtc_frame_buffer_existed(void)
{
	DDPMSG("%s, frame buffer is freed:%d\n", __func__, already_free);
	return !already_free;
}

static void mtk_crtc_update_ddp_state(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state,
				      struct mtk_crtc_state *crtc_state,
				      struct cmdq_pkt *cmdq_handle)
{
	struct mtk_crtc_state *old_mtk_state =
		to_mtk_crtc_state(old_crtc_state);
	struct mtk_drm_lyeblob_ids *lyeblob_ids, *next;
	struct mtk_drm_private *mtk_drm = crtc->dev->dev_private;
	int index = drm_crtc_index(crtc);
	int crtc_mask = 0x1 << index;
	unsigned int prop_lye_idx;
	unsigned int pan_disp_frame_weight = 4;
	struct drm_device *dev = crtc->dev;

	mutex_lock(&mtk_drm->lyeblob_list_mutex);
	prop_lye_idx = crtc_state->prop_val[CRTC_PROP_LYE_IDX];
	/*set_hrt_bw for pan display ,set 4 for two RGB layer*/
	if (index == 0 && prop_lye_idx == 0) {
		DDPINFO("%s prop_lye_idx is 0, mode switch from %u to %u\n",
			__func__,
			old_mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX],
			crtc_state->prop_val[CRTC_PROP_DISP_MODE_IDX]);
		/*
		 * prop_lye_idx is 0 when suspend. Update display mode to avoid
		 * the dsi params not sync with the mode of new crtc state.
		 * And switch fps immediately when suspend or resume
		 */
		mtk_crtc_disp_mode_switch_begin(crtc,
			old_crtc_state, crtc_state,
			cmdq_handle);
		mtk_crtc_update_hrt_state(crtc, pan_disp_frame_weight,
			cmdq_handle);
	}
	list_for_each_entry_safe(lyeblob_ids, next, &mtk_drm->lyeblob_head,
				 list) {
		if (lyeblob_ids->lye_idx > prop_lye_idx) {
			DDPMSG("lyeblob lost ID:%d\n", prop_lye_idx);
			break;
		} else if (lyeblob_ids->lye_idx == prop_lye_idx) {
			if (index == 0)
				mtk_crtc_disp_mode_switch_begin(crtc,
					old_crtc_state, crtc_state,
					cmdq_handle);
			if (index == 0) {
#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833) || \
	defined(CONFIG_MACH_MT6877)
				mtk_crtc_update_hrt_state_ex(
					crtc, lyeblob_ids,
					cmdq_handle);
#else
				mtk_crtc_update_hrt_state(
					crtc, lyeblob_ids->frame_weight,
					cmdq_handle);
#endif
			}
			mtk_crtc_get_plane_comp_state(crtc, cmdq_handle);
			mtk_crtc_atmoic_ddp_config(crtc, lyeblob_ids,
						   cmdq_handle);
#ifndef CONFIG_MTK_DISP_NO_LK
			if (lyeblob_ids->lye_idx == 2 && !already_free) {
				/*free fb buf in second query valid*/
				mtk_drm_fb_gem_release(dev);
				try_free_fb_buf(dev);
				already_free = true;
#endif
			}
			break;
		} else if (lyeblob_ids->lye_idx < prop_lye_idx) {
			if (lyeblob_ids->ref_cnt) {
				DDPINFO("free:(0x%x,0x%x), cnt:%d\n",
					 lyeblob_ids->free_cnt_mask,
					 crtc_mask,
					 lyeblob_ids->ref_cnt);
				if (lyeblob_ids->free_cnt_mask & crtc_mask) {
					lyeblob_ids->free_cnt_mask &=
						(~crtc_mask);
					lyeblob_ids->ref_cnt--;
					DDPINFO("free:(0x%x,0x%x), cnt:%d\n",
						 lyeblob_ids->free_cnt_mask,
						 crtc_mask,
						 lyeblob_ids->ref_cnt);
				}
				if (!lyeblob_ids->ref_cnt) {
					DDPINFO("free lyeblob:(%d,%d)\n",
						lyeblob_ids->lye_idx,
						prop_lye_idx);
					mtk_crtc_free_lyeblob_ids(crtc,
							lyeblob_ids);
					mtk_crtc_free_ddpblob_ids(crtc,
							lyeblob_ids);
				}
			}
		}
	}
	mutex_unlock(&mtk_drm->lyeblob_list_mutex);
}

#ifdef MTK_DRM_FENCE_SUPPORT
static void mtk_crtc_release_lye_idx(struct drm_crtc *crtc)
{
	struct mtk_drm_lyeblob_ids *lyeblob_ids, *next;
	struct mtk_drm_private *mtk_drm = crtc->dev->dev_private;
	int index = drm_crtc_index(crtc);
	int crtc_mask = 0x1 << index;

	mutex_lock(&mtk_drm->lyeblob_list_mutex);
	list_for_each_entry_safe(lyeblob_ids, next, &mtk_drm->lyeblob_head,
		list) {
		if (lyeblob_ids->ref_cnt) {
			DDPINFO("%s:%d free:(0x%x,0x%x), cnt:%d\n",
				__func__, __LINE__,
				lyeblob_ids->free_cnt_mask,
				crtc_mask,
				lyeblob_ids->ref_cnt);
			if (lyeblob_ids->free_cnt_mask & crtc_mask) {
				lyeblob_ids->free_cnt_mask &= (~crtc_mask);
				lyeblob_ids->ref_cnt--;
				DDPINFO("%s:%d free:(0x%x,0x%x), cnt:%d\n",
					__func__, __LINE__,
					lyeblob_ids->free_cnt_mask,
					crtc_mask,
					lyeblob_ids->ref_cnt);
			}
			if (!lyeblob_ids->ref_cnt) {
				DDPINFO("%s:%d free lyeblob:%d\n",
					__func__, __LINE__,
					lyeblob_ids->lye_idx);
				mtk_crtc_free_lyeblob_ids(crtc,
					lyeblob_ids);
				mtk_crtc_free_ddpblob_ids(crtc,
					lyeblob_ids);
			}
		}
	}
	mutex_unlock(&mtk_drm->lyeblob_list_mutex);
}
#endif
#endif

bool mtk_crtc_with_trigger_loop(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->gce_obj.client[CLIENT_TRIG_LOOP])
		return true;
	return false;
}

/* sw workaround to fix gce hw bug */
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
bool mtk_crtc_with_sodi_loop(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = NULL;

	priv = mtk_crtc->base.dev->dev_private;
	if (mtk_crtc->gce_obj.client[CLIENT_SODI_LOOP])
		return true;
	return false;
}
#endif

bool mtk_crtc_is_frame_trigger_mode(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int crtc_id = drm_crtc_index(crtc);
	struct mtk_ddp_comp *comp = NULL;
	int i;

	if (crtc_id == 0)
		return mtk_dsi_is_cmd_mode(priv->ddp_comp[DDP_COMPONENT_DSI0]);

	for_each_comp_in_crtc_target_path(
		comp, mtk_crtc, i,
		DDP_FIRST_PATH)
		if (mtk_ddp_comp_is_output(comp))
			break;

	if (!comp) {
		DDPPR_ERR("%s, Cannot find output component\n", __func__);
		return false;
	}

	if (comp->id == DDP_COMPONENT_DP_INTF0 ||
		comp->id == DDP_COMPONENT_DPI0 ||
		comp->id == DDP_COMPONENT_DPI1) {
		pr_info("%s(%d-%d) is vdo mode\n", __func__,
			comp->id, DDP_COMPONENT_DPI0);
		return false;
	}

	return true;
}

static bool mtk_crtc_target_is_dc_mode(struct drm_crtc *crtc,
				unsigned int ddp_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (ddp_mode >= DDP_MODE_NR)
		return false;

	if (mtk_crtc->ddp_ctx[ddp_mode].ddp_comp_nr[1])
		return true;
	return false;
}

static bool mtk_crtc_support_dc_mode(struct drm_crtc *crtc)
{
	int i;

	for (i = 0; i < DDP_MODE_NR; i++)
		if (mtk_crtc_target_is_dc_mode(crtc, i))
			return true;
	return false;
}

bool mtk_crtc_is_dc_mode(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	return mtk_crtc_target_is_dc_mode(crtc, mtk_crtc->ddp_mode);
}

bool mtk_crtc_is_mem_mode(struct drm_crtc *crtc)
{
	/* for find memory session */
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!comp)
		return false;

	if (comp->id == DDP_COMPONENT_WDMA0 ||
		comp->id == DDP_COMPONENT_WDMA1)
		return true;

	return false;
}

int get_path_wait_event(struct mtk_drm_crtc *mtk_crtc,
			enum CRTC_DDP_PATH ddp_path)
{
	struct mtk_ddp_comp *comp = NULL;
	int i;

	if (ddp_path < 0 || ddp_path >= DDP_PATH_NR) {
		DDPPR_ERR("%s, invalid ddp_path value\n", __func__);
		return -EINVAL;
	}

	for_each_comp_in_crtc_target_path(
		comp, mtk_crtc, i,
		ddp_path)
		if (mtk_ddp_comp_is_output(comp))
			break;

	if (!comp) {
		DDPPR_ERR("%s, Cannot find output component\n", __func__);
		return -EINVAL;
	}

	if (comp->id == DDP_COMPONENT_DSI0 || comp->id == DDP_COMPONENT_DSI1) {
		if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
			return mtk_crtc->gce_obj.event[EVENT_STREAM_EOF];
		else
			return mtk_crtc->gce_obj.event[EVENT_CMD_EOF];

	} else if (comp->id == DDP_COMPONENT_DP_INTF0) {
		return mtk_crtc->gce_obj.event[EVENT_VDO_EOF];
	} else if (comp->id == DDP_COMPONENT_WDMA0) {
		return mtk_crtc->gce_obj.event[EVENT_WDMA0_EOF];
	} else if (comp->id == DDP_COMPONENT_WDMA1) {
		return mtk_crtc->gce_obj.event[EVENT_WDMA1_EOF];
	}

	DDPPR_ERR("The output component has not frame done event\n");
	return -EINVAL;
}

void mtk_crtc_wait_frame_done(struct mtk_drm_crtc *mtk_crtc,
			      struct cmdq_pkt *cmdq_handle,
			      enum CRTC_DDP_PATH ddp_path,
			      int clear_event)
{
	int gce_event;

	gce_event = get_path_wait_event(mtk_crtc, ddp_path);
	if (gce_event < 0)
		return;
	if (gce_event == mtk_crtc->gce_obj.event[EVENT_STREAM_EOF] ||
	    gce_event == mtk_crtc->gce_obj.event[EVENT_CMD_EOF] ||
	    gce_event == mtk_crtc->gce_obj.event[EVENT_VDO_EOF]) {
		struct mtk_drm_private *priv;
		if (clear_event)
			cmdq_pkt_wfe(cmdq_handle, gce_event);
		else
			cmdq_pkt_wait_no_clear(cmdq_handle, gce_event);
		priv = mtk_crtc->base.dev->dev_private;
		if (gce_event == mtk_crtc->gce_obj.event[EVENT_CMD_EOF] &&
		    mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_LAYER_REC) &&
		    mtk_crtc->layer_rec_en) {
			cmdq_pkt_wait_no_clear(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
		}
	} else if (gce_event == mtk_crtc->gce_obj.event[EVENT_WDMA0_EOF]) {
		/* Must clear WDMA_EOF in decouple mode */
		if (mtk_crtc_is_dc_mode(&mtk_crtc->base))
			cmdq_pkt_wfe(cmdq_handle, gce_event);
	} else if (gce_event == mtk_crtc->gce_obj.event[EVENT_WDMA1_EOF]) {
		if (mtk_crtc_is_dc_mode(&mtk_crtc->base))
			cmdq_pkt_wfe(cmdq_handle, gce_event);
	} else
		DDPPR_ERR("The output component has not frame done event\n");
}

static int _mtk_crtc_cmdq_retrig(void *data)
{
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *) data;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct sched_param param = {.sched_priority = 94 };
	int ret;

	sched_setscheduler(current, SCHED_RR, &param);

	atomic_set(&mtk_crtc->cmdq_trig, 0);
	do {
		ret = wait_event_interruptible(mtk_crtc->trigger_cmdq,
			atomic_read(&mtk_crtc->cmdq_trig));
		if (ret < 0)
			DDPPR_ERR("wait %s fail, ret=%d\n", __func__, ret);
		atomic_set(&mtk_crtc->cmdq_trig, 0);

		mtk_crtc_clear_wait_event(crtc);

	} while (!kthread_should_stop());

	return 0;
}

static void mtk_crtc_cmdq_timeout_cb(struct cmdq_cb_data data)
{
	struct drm_crtc *crtc = data.data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_client *cl;
	dma_addr_t trig_pc;
	u64 *inst;

	if (!crtc) {
		DDPPR_ERR("%s find crtc fail\n", __func__);
		return;
	}

	DDPPR_ERR("%s cmdq timeout, crtc id:%d\n", __func__,
		  drm_crtc_index(crtc));
	mtk_drm_crtc_analysis(crtc);
	mtk_drm_crtc_dump(crtc);

	if ((mtk_crtc->trig_loop_cmdq_handle) &&
			(mtk_crtc->trig_loop_cmdq_handle->cl)) {
		cl = (struct cmdq_client *)mtk_crtc->trig_loop_cmdq_handle->cl;
		DDPMSG("++++++ Dump trigger loop ++++++\n");
		cmdq_thread_dump(cl->chan, mtk_crtc->trig_loop_cmdq_handle,
				&inst, &trig_pc);
		cmdq_dump_pkt(mtk_crtc->trig_loop_cmdq_handle, trig_pc, true);

		DDPMSG("------ Dump trigger loop ------\n");
	}

	atomic_set(&mtk_crtc->cmdq_trig, 1);
	/* CMDQ driver would not trigger aee when timeout. */
	DDPAEE("%s cmdq timeout, crtc id:%d\n", __func__, drm_crtc_index(crtc));
}

void mtk_crtc_pkt_create(struct cmdq_pkt **cmdq_handle, struct drm_crtc *crtc,
	struct cmdq_client *cl)
{
	*cmdq_handle = cmdq_pkt_create(cl);
	if (IS_ERR_OR_NULL(*cmdq_handle)) {
		DDPPR_ERR("%s create handle fail, %x\n",
				__func__, *cmdq_handle);
		return;
	}

	(*cmdq_handle)->err_cb.cb = mtk_crtc_cmdq_timeout_cb;
	(*cmdq_handle)->err_cb.data = crtc;
}

static void sub_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(cb_data->crtc);
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	int session_id = -1, id = drm_crtc_index(cb_data->crtc), i;
	unsigned int intr_fence = 0;

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if ((id + 1) == MTK_SESSION_TYPE(priv->session_id[i])) {
			session_id = priv->session_id[i];
			break;
		}
	}

	/* Release output buffer fence */
	intr_fence = *(unsigned int *)(cmdq_buf->va_base +
			DISP_SLOT_CUR_INTERFACE_FENCE);

	if (intr_fence >= 1) {
		DDPINFO("intr fence_idx:%d\n", intr_fence);
		mtk_release_fence(session_id,
			mtk_fence_get_interface_timeline_id(), intr_fence - 1);
	}

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

void mtk_crtc_release_output_buffer_fence(
	struct drm_crtc *crtc, int session_id)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	unsigned int fence_idx = 0;

	fence_idx = *(unsigned int *)(cmdq_buf->va_base +
			DISP_SLOT_CUR_OUTPUT_FENCE);
	if (fence_idx) {
		DDPINFO("output fence_idx:%d\n", fence_idx);
		mtk_release_fence(session_id,
			mtk_fence_get_output_timeline_id(), fence_idx);
	}
}

struct drm_framebuffer *mtk_drm_framebuffer_lookup(struct drm_device *dev,
	unsigned int id)
{
	struct drm_framebuffer *fb = NULL;

	fb = drm_framebuffer_lookup(dev, NULL, id);

	if (!fb)
		return NULL;

	/* CRITICAL: drop the reference we picked up in framebuffer lookup */
	drm_framebuffer_put(fb);

	return fb;
}

static void mtk_crtc_dc_config_color_matrix(struct drm_crtc *crtc,
				struct cmdq_pkt *cmdq_handle)
{
	int i, j, mode, ccorr_matrix[16], all_zero = 1;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	bool set = false;
	struct mtk_ddp_comp *comp_ccorr;

	/* Get color matrix data from backup slot*/
	mode = *(int *)(cmdq_buf->va_base + DISP_SLOT_COLOR_MATRIX_PARAMS(0));
	for (i = 0; i < 16; i++)
		ccorr_matrix[i] = *(int *)(cmdq_buf->va_base +
					DISP_SLOT_COLOR_MATRIX_PARAMS(i + 1));

	for (i = 0; i <= 15; i += 5) {
		if (ccorr_matrix[i] != 0) {
			all_zero = 0;
			break;
		}
	}

	if (all_zero)
		DDPPR_ERR("CCORR color matrix backup param is zero matrix\n");
	else {
		ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
		for (i = 0; i < ddp_ctx->ddp_comp_nr[DDP_SECOND_PATH]; i++) {
			struct mtk_ddp_comp *comp =
					ddp_ctx->ddp_comp[DDP_SECOND_PATH][i];

			if (comp->id == DDP_COMPONENT_CCORR0) {
				disp_ccorr_set_color_matrix(comp, cmdq_handle,
							ccorr_matrix, mode, false);
				set = true;
				break;
			}
		}
		if (mtk_crtc->is_dual_pipe) {
			i = 0;
			j = 0;
			for_each_comp_in_dual_pipe(comp_ccorr, mtk_crtc, i, j) {
				if (comp_ccorr->id == DDP_COMPONENT_CCORR1) {
					disp_ccorr_set_color_matrix(comp_ccorr, cmdq_handle,
								ccorr_matrix, mode, false);
					set = true;
					break;
				}
			}
		}

		if (!set)
			DDPPR_ERR("Cannot not find DDP_COMPONENT_CCORR0\n");
	}
}

void mtk_crtc_dc_prim_path_update(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_plane_state plane_state;
	struct drm_framebuffer *fb;
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	struct mtk_cmdq_cb_data *cb_data;
	unsigned int fb_idx, fb_id;
	int session_id;

	DDPINFO("%s+\n", __func__);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!mtk_crtc_is_dc_mode(crtc)) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return;
	}

	session_id = mtk_get_session_id(crtc);
	mtk_crtc_release_output_buffer_fence(crtc, session_id);

	/* find fb for RDMA */
	fb_idx = *(unsigned int *)(cmdq_buf->va_base + DISP_SLOT_RDMA_FB_IDX);
	fb_id = *(unsigned int *)(cmdq_buf->va_base + DISP_SLOT_RDMA_FB_ID);

	/* 1-to-2*/
	if (!fb_id)
		goto end;

	fb = mtk_drm_framebuffer_lookup(mtk_crtc->base.dev, fb_id);
	if (fb == NULL) {
		DDPPR_ERR("%s cannot find fb fb_id:%u\n", __func__, fb_id);
		goto end;
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		DDPPR_ERR("cb data creation failed\n");
		return;
	}

	mtk_crtc_pkt_create(&cmdq_handle, crtc,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cmdq_pkt_wait_no_clear(cmdq_handle,
			       get_path_wait_event(mtk_crtc, DDP_FIRST_PATH));
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_SECOND_PATH, 0);

	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];

	plane_state.pending.enable = true;
	plane_state.pending.pitch = fb->pitches[0];
	plane_state.pending.format = fb->format->format;
	plane_state.pending.addr =
		mtk_fb_get_dma(fb) +
		(dma_addr_t)mtk_crtc_get_dc_fb_size(crtc) *
		(dma_addr_t)fb_idx;
	plane_state.pending.size = mtk_fb_get_size(fb);
	plane_state.pending.src_x = 0;
	plane_state.pending.src_y = 0;
	plane_state.pending.dst_x = 0;
	plane_state.pending.dst_y = 0;
	plane_state.pending.width = fb->width;
	plane_state.pending.height = fb->height;
	mtk_ddp_comp_layer_config(ddp_ctx->ddp_comp[DDP_SECOND_PATH][0], 0,
				  &plane_state, cmdq_handle);

	mtk_crtc_dc_config_color_matrix(crtc, cmdq_handle);
	if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);

	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;
	if (cmdq_pkt_flush_threaded(cmdq_handle, sub_cmdq_cb, cb_data) < 0)
		DDPPR_ERR("failed to flush sub\n");
end:
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

static void mtk_crtc_release_input_layer_fence(
	struct drm_crtc *crtc, int session_id)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	int i;
	unsigned int fence_idx = 0;

	cmdq_buf = &(mtk_crtc->gce_obj.buf);
	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		unsigned int subtractor = 0;

		fence_idx = *(unsigned int *)(cmdq_buf->va_base +
					DISP_SLOT_CUR_CONFIG_FENCE(i));
		subtractor = *(unsigned int *)(cmdq_buf->va_base +
					DISP_SLOT_SUBTRACTOR_WHEN_FREE(i));
		subtractor &= 0xFFFF;
		if (drm_crtc_index(crtc) == 2)
			DDPINFO("%d, fence_idx:%d, subtractor:%d\n",
					i, fence_idx, subtractor);
		mtk_release_fence(session_id, i, fence_idx - subtractor);
	}
}

static void mtk_crtc_update_hrt_qos(struct drm_crtc *crtc,
		unsigned int ddp_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	struct mtk_ddp_comp *comp;
	unsigned int cur_hrt_bw, hrt_idx;
	int i, j;

	for_each_comp_in_target_ddp_mode_bound(comp, mtk_crtc,
			i, j, ddp_mode, 0)
		mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_BW, NULL);

	if (drm_crtc_index(crtc) != 0)
		return;

	hrt_idx = *(unsigned int *)(cmdq_buf->va_base + DISP_SLOT_CUR_HRT_IDX);
	atomic_set(&mtk_crtc->qos_ctx->last_hrt_idx, hrt_idx);
	atomic_set(&mtk_crtc->qos_ctx->hrt_cond_sig, 1);
	wake_up(&mtk_crtc->qos_ctx->hrt_cond_wq);
	cur_hrt_bw = *(unsigned int *)(cmdq_buf->va_base +
				DISP_SLOT_CUR_HRT_LEVEL);
	if (cur_hrt_bw != NO_PENDING_HRT &&
		cur_hrt_bw <= mtk_crtc->qos_ctx->last_hrt_req) {

		DDPINFO("cur:%u last:%u, release HRT to last_hrt_req:%u\n",
			cur_hrt_bw,	mtk_crtc->qos_ctx->last_hrt_req,
			mtk_crtc->qos_ctx->last_hrt_req);

#ifdef MTK_FB_MMDVFS_SUPPORT
	if (mtk_crtc->enabled)
		mtk_disp_set_hrt_bw(mtk_crtc,
				mtk_crtc->qos_ctx->last_hrt_req);
#endif

		*(unsigned int *)(cmdq_buf->va_base + DISP_SLOT_CUR_HRT_LEVEL) =
				NO_PENDING_HRT;
	}
}

static void mtk_crtc_enable_iommu(struct mtk_drm_crtc *mtk_crtc,
			   struct cmdq_pkt *handle)
{
	int i, j, p_mode;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_all_crtc_mode(comp, mtk_crtc, i, j, p_mode)
		mtk_ddp_comp_iommu_enable(comp, handle);
	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
			mtk_ddp_comp_iommu_enable(comp, handle);
	}
}

void mtk_crtc_enable_iommu_runtime(struct mtk_drm_crtc *mtk_crtc,
			   struct cmdq_pkt *handle)
{
	int i, j;
	struct mtk_ddp_comp *comp;
	struct mtk_ddp_fb_info fb_info;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(priv->fbdev_bo);
	unsigned int vramsize = 0, fps = 0;
	phys_addr_t fb_base = 0;

	mtk_crtc_enable_iommu(mtk_crtc, handle);

	_parse_tag_videolfb(&vramsize, &fb_base, &fps);
	fb_info.fb_mva = mtk_gem->dma_addr;
	fb_info.fb_size =
		priv->fb_helper.fb->width * priv->fb_helper.fb->height / 3;
	fb_info.fb_pa = fb_base;
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, handle, OVL_REPLACE_BOOTUP_MVA,
				    &fb_info);
	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, handle,
				OVL_REPLACE_BOOTUP_MVA, &fb_info);
	}
}

#ifdef MTK_DRM_CMDQ_ASYNC
#ifdef MTK_DRM_FB_LEAK
static void mtk_disp_signal_fence_worker_signal(struct drm_crtc *crtc, struct cmdq_cb_data data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (unlikely(!mtk_crtc)) {
		DDPINFO("%s:invalid ESD context, crtc id:%d\n",
			__func__, drm_crtc_index(crtc));
		return;
	}
	mtk_crtc->cb_data = data;
	atomic_set(&mtk_crtc->cmdq_done, 1);
	wake_up_interruptible(&mtk_crtc->signal_fence_task_wq);
}

static void ddp_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct drm_crtc_state *crtc_state = cb_data->state;
	struct drm_crtc *crtc = crtc_state->crtc;

	mtk_disp_signal_fence_worker_signal(crtc, data);
}

static void _ddp_cmdq_cb(struct cmdq_cb_data data)
#else
static void ddp_cmdq_cb(struct cmdq_cb_data data)
#endif
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct drm_crtc_state *crtc_state = cb_data->state;
	struct drm_atomic_state *atomic_state = crtc_state->state;
	struct drm_crtc *crtc = crtc_state->crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int session_id, id;
	unsigned int ovl_status = 0;
	struct mtk_crtc_state *mtk_state;

	DDPINFO("crtc_state:%px, atomic_state:%px, crtc:%px\n",
		crtc_state,
		atomic_state,
		crtc);

	session_id = mtk_get_session_id(crtc);

	id = drm_crtc_index(crtc);

	CRTC_MMP_EVENT_START(id, frame_cfg, 0, 0);

	if (id == 0) {
		struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);

		ovl_status = *(unsigned int *)(cmdq_buf->va_base +
			DISP_SLOT_OVL_STATUS);

#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
		if (ovl_status & 1)
			DDPPR_ERR("ovl status error\n");
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
		if (ovl_status & 1) {
			DDPPR_ERR("ovl status error\n");
			mtk_drm_crtc_analysis(crtc);
			mtk_drm_crtc_dump(crtc);
		}
#endif
	}
	CRTC_MMP_MARK(id, frame_cfg, ovl_status, 0);

	if (session_id > 0)
		mtk_crtc_release_input_layer_fence(crtc, session_id);
	if (crtc->state) {
		mtk_state = to_mtk_crtc_state(crtc->state);
		if (mtk_crtc_is_frame_trigger_mode(crtc) &&
			mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
			te_cnt = 1;
		}
	}
	// release present fence
	if (drm_crtc_index(crtc) != 2) {
		struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
		unsigned int fence_idx = *(unsigned int *)(cmdq_buf->va_base +
				DISP_SLOT_PRESENT_FENCE(drm_crtc_index(crtc)));
		// only VDO mode panel use CMDQ call
		if (mtk_crtc &&
			!mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base)) {
			mtk_release_present_fence(session_id, fence_idx, mtk_crtc->eof_time);
		}
	}

	// for wfd latency debug
	if (id == 0 || id == 2) {
		struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
		unsigned int ovl_dsi_seq;

		if (cmdq_buf) {
			ovl_dsi_seq = *(unsigned int *)(cmdq_buf->va_base +
					DISP_SLOT_OVL_DSI_SEQ(id));
		}

		if (ovl_dsi_seq) {
			if (id == 0)
				mtk_drm_trace_async_end("OVL0-DSI|%d", ovl_dsi_seq);
			else if (id == 2)
				mtk_drm_trace_async_end("OVL2-WDMA|%d", ovl_dsi_seq);
		}
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc_is_dc_mode(crtc) && session_id > 0)
		mtk_crtc_release_output_buffer_fence(crtc, session_id);

	mtk_crtc_update_hrt_qos(crtc, cb_data->misc);

	if (mtk_crtc->pending_needs_vblank) {
		mtk_drm_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}

	mtk_atomic_state_put_queue(atomic_state);

	if (mtk_crtc->wb_enable == true) {
		mtk_crtc->wb_enable = false;
		drm_writeback_signal_completion(&mtk_crtc->wb_connector, 0);
	}
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
#ifdef MTK_DRM_FB_LEAK
	cmdq_pkt_wait_complete(cb_data->cmdq_handle);
#endif
	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);

	CRTC_MMP_EVENT_END(id, frame_cfg, 0, 0);
}

#ifdef MTK_DRM_FB_LEAK
static int mtk_drm_signal_fence_worker_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 87};
	int ret = 0;

	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *)data;

	sched_setscheduler(current, SCHED_RR, &param);
	while (1) {
		ret = wait_event_interruptible(
			mtk_crtc->signal_fence_task_wq
			, atomic_read(&mtk_crtc->cmdq_done));
			atomic_set(&mtk_crtc->cmdq_done, 0);
		_ddp_cmdq_cb(mtk_crtc->cb_data);
	}
	return 0;
}
#endif
#else
/* ddp_cmdq_cb_blocking should be called within locked function */
static void ddp_cmdq_cb_blocking(struct mtk_cmdq_cb_data *cb_data)
{
	struct drm_crtc_state *crtc_state = cb_data->state;
	struct drm_atomic_state *atomic_state = crtc_state->state;
	struct drm_crtc *crtc = crtc_state->crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *private;
	int session_id = -1, id, i;

	DDPINFO("%s:%d, cb_data:%px\n",
		__func__, __LINE__,
		cb_data);
	DDPINFO("crtc_state:%px, atomic_state:%px, crtc:%px\n",
		crtc_state,
		atomic_state,
		crtc);

	id = drm_crtc_index(crtc);
	private = mtk_crtc->base.dev->dev_private;
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if ((id + 1) == MTK_SESSION_TYPE(private->session_id[i])) {
			session_id = private->session_id[i];
			break;
		}
	}

	mtk_crtc_release_input_layer_fence(crtc, session_id);

	mtk_crtc_release_output_buffer_fence(crtc, session_id);

	mtk_crtc_update_hrt_qos(crtc, cb_data->misc);

	if (mtk_crtc->pending_needs_vblank) {
		mtk_drm_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}

	mtk_atomic_state_put_queue(atomic_state);

	if (mtk_crtc->wb_enable == true) {
		mtk_crtc->wb_enable = false;
		drm_writeback_signal_completion(&mtk_crtc->wb_connector, 0);
	}

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

#endif

static void mtk_crtc_ddp_config(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state = to_mtk_crtc_state(mtk_crtc->base.state);
	struct mtk_ddp_comp *comp = mtk_crtc_get_comp(crtc, 0, 0);
	struct mtk_ddp_config cfg;
	struct cmdq_pkt *cmdq_handle = state->cmdq_handle;
	unsigned int i;
	unsigned int ovl_is_busy;
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	unsigned int last_fence, cur_fence, sub;

	/*
	 * TODO: instead of updating the registers here, we should prepare
	 * working registers in atomic_commit and let the hardware command
	 * queue update module registers on vblank.
	 */

	if (!comp) {
		DDPINFO("%s fail, comp is NULL\n", __func__);
		return;
	}

	ovl_is_busy = readl(comp->regs) & 0x1UL;
	if (ovl_is_busy == 0x1UL)
		return;

	if ((state->pending_config) == true) {
		cfg.w = state->pending_width;
		cfg.h = state->pending_height;
		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
			struct mtk_panel_params *params;

			params = mtk_crtc->panel_ext->params;
			if (params->dyn_fps.switch_en == 1 &&
				params->dyn_fps.vact_timing_fps != 0)
				cfg.vrefresh =
					params->dyn_fps.vact_timing_fps;
			else
				cfg.vrefresh = state->pending_vrefresh;
		} else
			cfg.vrefresh = state->pending_vrefresh;
		cfg.bpc = 0;
		mtk_ddp_comp_config(comp, &cfg, cmdq_handle);

		state->pending_config = false;
	}

	if ((mtk_crtc->pending_planes) == false)
		return;

#ifndef CONFIG_MTK_DISPLAY_CMDQ
	mtk_wb_atomic_commit(mtk_crtc);
#endif
	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		if ((plane_state->pending.config) == false)
			continue;

		mtk_ddp_comp_layer_config(comp, i, plane_state, cmdq_handle);

		last_fence = *(unsigned int *)(cmdq_buf->va_base +
					       DISP_SLOT_CUR_CONFIG_FENCE(i));
		cur_fence =
			plane_state->pending.prop_val[PLANE_PROP_NEXT_BUFF_IDX];

		if (cur_fence != -1 && cur_fence > last_fence)
			cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
				       cmdq_buf->pa_base +
					       DISP_SLOT_CUR_CONFIG_FENCE(i),
				       cur_fence, ~0);

		sub = 1;
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			       cmdq_buf->pa_base +
				       DISP_SLOT_SUBTRACTOR_WHEN_FREE(i),
			       sub, ~0);

		plane_state->pending.config = false;
	}

	mtk_crtc->pending_planes = false;
	if (mtk_crtc->wb_enable == true) {
		mtk_crtc->wb_enable = false;
		drm_writeback_signal_completion(&mtk_crtc->wb_connector, 0);
	}
}

static void mtk_crtc_comp_trigger(struct mtk_drm_crtc *mtk_crtc,
				  struct cmdq_pkt *cmdq_handle,
				  enum mtk_ddp_comp_trigger_flag trig_flag)
{
	int i, j;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_config_trigger(comp, cmdq_handle, trig_flag);
}

int mtk_crtc_comp_is_busy(struct mtk_drm_crtc *mtk_crtc)
{
	int ret = 0;
	int i, j;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		ret = mtk_ddp_comp_is_busy(comp);
		if (ret)
			return ret;
	}

	return ret;
}

/* TODO: need to remove this in vdo mode for lowpower */
static void trig_done_cb(struct cmdq_cb_data data)
{
	CRTC_MMP_MARK((unsigned long)data.data, trig_loop_done, 0, 0);
	DDPINFO("%s()\n", __func__);
}

void mtk_crtc_clear_wait_event(struct drm_crtc *crtc)
{
	struct cmdq_pkt *cmdq_handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc_is_frame_trigger_mode(crtc)) {
		mtk_crtc_pkt_create(&cmdq_handle, crtc,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}

}

static void mtk_crtc_rec_trig_cnt(struct mtk_drm_crtc *mtk_crtc,
				  struct cmdq_pkt *cmdq_handle)
{
	struct cmdq_pkt_buffer *cmdq_buf = &mtk_crtc->gce_obj.buf;
	struct cmdq_operand lop, rop;

	lop.reg = true;
	lop.idx = CMDQ_CPR_DISP_CNT;
	rop.reg = false;
	rop.value = 1;

	cmdq_pkt_logic_command(cmdq_handle, CMDQ_LOGIC_ADD, CMDQ_CPR_DISP_CNT,
			       &lop, &rop);
	cmdq_pkt_write_reg_addr(cmdq_handle,
				cmdq_buf->pa_base + DISP_SLOT_TRIG_CNT,
				CMDQ_CPR_DISP_CNT, U32_MAX);
}

/* sw workaround to fix gce hw bug */
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
void mtk_crtc_start_sodi_loop(struct drm_crtc *crtc)
{
	struct cmdq_pkt *cmdq_handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = NULL;
	unsigned long crtc_id = (unsigned long)drm_crtc_index(crtc);

	if (crtc_id) {
		DDPDBG("%s:%d invalid crtc:%ld\n",
			__func__, __LINE__, crtc_id);
		return;
	}

	priv = mtk_crtc->base.dev->dev_private;
	mtk_crtc->sodi_loop_cmdq_handle = cmdq_pkt_create(
			mtk_crtc->gce_obj.client[CLIENT_SODI_LOOP]);
	cmdq_handle = mtk_crtc->sodi_loop_cmdq_handle;

	cmdq_pkt_wait_no_clear(cmdq_handle,
		mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);

	cmdq_pkt_write(cmdq_handle, NULL,
		GCE_BASE_ADDR + GCE_GCTL_VALUE, GCE_DDR_EN, GCE_DDR_EN);

	cmdq_pkt_wfe(cmdq_handle,
		mtk_crtc->gce_obj.event[EVENT_SYNC_TOKEN_SODI]);

	cmdq_pkt_finalize_loop(cmdq_handle);
	cmdq_pkt_flush_async(cmdq_handle, NULL, (void *)crtc_id);
}
#endif

void mtk_crtc_start_trig_loop(struct drm_crtc *crtc)
{
	int ret = 0;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned long crtc_id = (unsigned long)drm_crtc_index(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
	struct cmdq_operand lop, rop;

	const u16 reg_jump = CMDQ_THR_SPR_IDX1;
	const u16 var1 = CMDQ_CPR_DDR_USR_CNT;
	const u16 var2 = 0;

	u32 inst_condi_jump;
	u64 *inst, jump_pa;

	lop.reg = true;
	lop.idx = var1;
	rop.reg = false;
	rop.idx = var2;
#endif

	if (crtc_id > 1) {
		DDPPR_ERR("%s:%d invalid crtc:%ld\n",
			__func__, __LINE__, crtc_id);
		return;
	}

	mtk_crtc->trig_loop_cmdq_handle = cmdq_pkt_create(
		mtk_crtc->gce_obj.client[CLIENT_TRIG_LOOP]);
	cmdq_handle = mtk_crtc->trig_loop_cmdq_handle;

	if (mtk_crtc_is_frame_trigger_mode(crtc)) {
		/* The STREAM BLOCK EVENT is used for stopping frame trigger if
		 * the engine is stopped
		 */
		cmdq_pkt_wait_no_clear(
			cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle,
			     mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
#ifndef CONFIG_FPGA_EARLY_PORTING
		cmdq_pkt_clear_event(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
		cmdq_pkt_wait_no_clear(cmdq_handle,
				       mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
		cmdq_pkt_wait_no_clear(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_clear_event(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_TE]);
		if (mtk_drm_lcm_is_connect())
			cmdq_pkt_wfe(cmdq_handle,
					 mtk_crtc->gce_obj.event[EVENT_TE]);

#endif
		cmdq_pkt_clear_event(
			cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
#ifndef CONFIG_FPGA_EARLY_PORTING
		cmdq_pkt_wait_no_clear(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);
		cmdq_pkt_wait_no_clear(cmdq_handle,
				       mtk_crtc->gce_obj.event[EVENT_ESD_EOF]);
#endif

		/*Trigger*/
		mtk_disp_mutex_enable_cmdq(mtk_crtc->mutex[0], cmdq_handle,
					   mtk_crtc->gce_obj.base);
		mtk_crtc_comp_trigger(mtk_crtc, cmdq_handle,
				      MTK_TRIG_FLAG_TRIGGER);

		cmdq_pkt_wfe(cmdq_handle,
			     mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);

		if (mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_LAYER_REC)) {
			mtk_crtc_comp_trigger(mtk_crtc, cmdq_handle,
					      MTK_TRIG_FLAG_LAYER_REC);

			mtk_crtc_rec_trig_cnt(mtk_crtc, cmdq_handle);

			mtk_crtc->layer_rec_en = true;
		} else {
			mtk_crtc->layer_rec_en = false;
		}

		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
	} else {
		mtk_disp_mutex_submit_sof(mtk_crtc->mutex[0]);
		if (crtc_id == 0)
			cmdq_pkt_wfe(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);
		else if (crtc_id == 1)
			cmdq_pkt_wfe(cmdq_handle,
					 mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);

/* sw workaround to fix gce hw bug */
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
		cmdq_pkt_read(cmdq_handle, NULL,
			GCE_BASE_ADDR + GCE_DEBUG_START_ADDR, var1);

		/*mark condition jump */
		inst_condi_jump = cmdq_handle->cmd_buf_size;
		cmdq_pkt_assign_command(cmdq_handle, reg_jump, 0);

		cmdq_pkt_cond_jump_abs(cmdq_handle, reg_jump, &lop, &rop,
			CMDQ_NOT_EQUAL);

		/* if condition false, will jump here */
		cmdq_pkt_write(cmdq_handle, NULL,
			GCE_BASE_ADDR + GCE_GCTL_VALUE, 0, GCE_DDR_EN);

	      /* if condition true, will jump curreent postzion */
		inst = cmdq_pkt_get_va_by_offset(cmdq_handle,  inst_condi_jump);
		jump_pa = cmdq_pkt_get_pa_by_offset(cmdq_handle,
					cmdq_handle->cmd_buf_size);
		*inst = *inst | CMDQ_REG_SHIFT_ADDR(jump_pa);

		cmdq_pkt_set_event(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_SYNC_TOKEN_SODI]);
#endif

		if (mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_LAYER_REC)) {
			cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_RDMA0_EOF]);
			cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);

			cmdq_pkt_wfe(cmdq_handle,
				     mtk_crtc->gce_obj.event[EVENT_RDMA0_EOF]);
			mtk_crtc_comp_trigger(mtk_crtc, cmdq_handle,
					      MTK_TRIG_FLAG_LAYER_REC);
			mtk_crtc_rec_trig_cnt(mtk_crtc, cmdq_handle);

			cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);

			mtk_crtc->layer_rec_en = true;
		} else {
			mtk_crtc->layer_rec_en = false;
		}
	}
	cmdq_pkt_finalize_loop(cmdq_handle);
	ret = cmdq_pkt_flush_async(cmdq_handle, trig_done_cb, (void *)crtc_id);

	mtk_crtc_clear_wait_event(crtc);
}

void mtk_crtc_hw_block_ready(struct drm_crtc *crtc)
{
	struct cmdq_pkt *cmdq_handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cmdq_pkt_set_event(cmdq_handle,
			   mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
}

void mtk_crtc_stop_trig_loop(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	cmdq_mbox_stop(mtk_crtc->gce_obj.client[CLIENT_TRIG_LOOP]);
	cmdq_pkt_destroy(mtk_crtc->trig_loop_cmdq_handle);
	mtk_crtc->trig_loop_cmdq_handle = NULL;
}

/* sw workaround to fix gce hw bug */
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
void mtk_crtc_stop_sodi_loop(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = NULL;

	if (!mtk_crtc->sodi_loop_cmdq_handle) {
		DDPDBG("%s: sodi_loop already stopped\n", __func__);
		return;
	}

	priv = mtk_crtc->base.dev->dev_private;
	cmdq_mbox_stop(mtk_crtc->gce_obj.client[CLIENT_SODI_LOOP]);
	cmdq_pkt_destroy(mtk_crtc->sodi_loop_cmdq_handle);
	mtk_crtc->sodi_loop_cmdq_handle = NULL;
}
#endif

long mtk_crtc_wait_status(struct drm_crtc *crtc, bool status, long timeout)
{
	long ret;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	ret = wait_event_timeout(mtk_crtc->crtc_status_wq,
				 mtk_crtc->enabled == status, timeout);

	return ret;
}

bool mtk_crtc_set_status(struct drm_crtc *crtc, bool status)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	bool old_status = mtk_crtc->enabled;

	mtk_crtc->enabled = status;
	wake_up(&mtk_crtc->crtc_status_wq);

	return old_status;
}

int mtk_crtc_attach_addon_path_comp(struct drm_crtc *crtc,
	const struct mtk_addon_module_data *module_data, bool is_attach)

{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);
	struct mtk_ddp_comp *comp;
	int i;

	for (i = 0; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i]) ==
		    MTK_DISP_VIRTUAL)
			continue;
		comp = priv->ddp_comp[path_data->path[i]];
		if (is_attach)
			comp->mtk_crtc = mtk_crtc;
		else
			comp->mtk_crtc = NULL;
	}

	return 0;
}

int mtk_crtc_attach_ddp_comp(struct drm_crtc *crtc, int ddp_mode,
			     bool is_attach)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	int i, j;

	if (ddp_mode < 0)
		return -EINVAL;

	for_each_comp_in_crtc_target_mode(comp, mtk_crtc, i, j, ddp_mode) {
		if (is_attach)
			comp->mtk_crtc = mtk_crtc;
		else
			comp->mtk_crtc = NULL;
	}

	return 0;
}

int mtk_crtc_update_ddp_sw_status(struct drm_crtc *crtc, int enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (enable)
		mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);
	else
		mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, false);

	return 0;
}

static void mtk_crtc_addon_connector_disconnect(struct drm_crtc *crtc,
	struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_params *panel_ext = mtk_drm_get_lcm_ext_params(crtc);
	struct mtk_ddp_comp *dsc_comp;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;

	if (panel_ext &&
		panel_ext->output_mode == MTK_PANEL_DSC_SINGLE_PORT) {
		dsc_comp = priv->ddp_comp[DDP_COMPONENT_DSC0];
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
		mtk_ddp_remove_dsc_prim_MT6885(mtk_crtc, handle);
#endif
#if defined(CONFIG_MACH_MT6873)
		mtk_ddp_remove_dsc_prim_MT6873(mtk_crtc, handle);
#endif
#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6877)
		mtk_ddp_remove_dsc_prim_MT6853(mtk_crtc, handle);
#endif
#if defined(CONFIG_MACH_MT6781)
		mtk_ddp_remove_dsc_prim_MT6781(mtk_crtc, handle);
#endif
		mtk_disp_mutex_remove_comp_with_cmdq(mtk_crtc, dsc_comp->id,
			handle, 0);
		mtk_ddp_comp_stop(dsc_comp, handle);
	}

}

void mtk_crtc_disconnect_addon_module(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];

	mtk_crtc_pkt_create(&handle, crtc, client);

	_mtk_crtc_atmoic_addon_module_disconnect(
		crtc, mtk_crtc->ddp_mode, &crtc_state->lye_state, handle);

	mtk_crtc_addon_connector_disconnect(crtc, handle);

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
}

static void mtk_crtc_addon_connector_connect(struct drm_crtc *crtc,
	struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_params *panel_ext = mtk_drm_get_lcm_ext_params(crtc);
	struct mtk_ddp_comp *dsc_comp;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_ddp_comp *output_comp;

	if (panel_ext &&
		panel_ext->output_mode == MTK_PANEL_DSC_SINGLE_PORT) {
		struct mtk_ddp_config cfg;

		dsc_comp = priv->ddp_comp[DDP_COMPONENT_DSC0];

		cfg.w = crtc->state->adjusted_mode.hdisplay;
		cfg.h = crtc->state->adjusted_mode.vdisplay;
		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (output_comp && drm_crtc_index(crtc) == 0) {
			cfg.w = mtk_ddp_comp_io_cmd(
					output_comp, NULL,
					DSI_GET_VIRTUAL_WIDTH, NULL);
			cfg.h = mtk_ddp_comp_io_cmd(
					output_comp, NULL,
					DSI_GET_VIRTUAL_HEIGH, NULL);
		}
		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
			struct mtk_panel_params *params;

			params = mtk_crtc->panel_ext->params;
			if (params->dyn_fps.switch_en == 1 &&
				params->dyn_fps.vact_timing_fps != 0)
				cfg.vrefresh =
					params->dyn_fps.vact_timing_fps;
			else
				cfg.vrefresh =
					crtc->state->adjusted_mode.vrefresh;
		} else
			cfg.vrefresh = crtc->state->adjusted_mode.vrefresh;
		cfg.bpc = mtk_crtc->bpc;
		cfg.p_golden_setting_context =
				__get_golden_setting_context(mtk_crtc);
		dsc_comp->mtk_crtc = mtk_crtc;

		/* insert DSC */
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
		mtk_ddp_insert_dsc_prim_MT6885(mtk_crtc, handle);
#endif
#if defined(CONFIG_MACH_MT6873)
		mtk_ddp_insert_dsc_prim_MT6873(mtk_crtc, handle);
#endif
#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6877)
		mtk_ddp_insert_dsc_prim_MT6853(mtk_crtc, handle);
#endif
#if defined(CONFIG_MACH_MT6781)
		mtk_ddp_insert_dsc_prim_MT6781(mtk_crtc, handle);
#endif
		mtk_disp_mutex_add_comp_with_cmdq(mtk_crtc, dsc_comp->id,
			mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base),
			handle, 0);

		mtk_ddp_comp_config(dsc_comp, &cfg, handle);
		mtk_ddp_comp_start(dsc_comp, handle);
	}

}

void mtk_crtc_connect_addon_module(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];

	mtk_crtc_pkt_create(&handle, crtc, client);

	_mtk_crtc_atmoic_addon_module_connect(crtc, mtk_crtc->ddp_mode,
					      &crtc_state->lye_state, handle);

	mtk_crtc_addon_connector_connect(crtc, handle);

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
}

/* set mutex & path mux for this CRTC default path */
void mtk_crtc_connect_default_path(struct mtk_drm_crtc *mtk_crtc)
{
	unsigned int i, j;
	struct mtk_ddp_comp *comp;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_ddp_comp **ddp_comp;
	enum mtk_ddp_comp_id prev_id, next_id;

	/* connect path */
	for_each_comp_in_crtc_path_bound(comp, mtk_crtc, i, j, 1) {
		ddp_comp = mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].ddp_comp[i];
		prev_id = (j == 0 ? DDP_COMPONENT_ID_MAX : ddp_comp[j - 1]->id);
		next_id = ddp_comp[j + 1]->id;

		mtk_ddp_add_comp_to_path(mtk_crtc, ddp_comp[j], prev_id,
					 next_id);
	}

	/* add module in mutex */
	if (mtk_crtc_is_dc_mode(crtc)) {
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, i,
						  DDP_FIRST_PATH)
			mtk_disp_mutex_add_comp(mtk_crtc->mutex[1], comp->id);
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, i,
						  DDP_SECOND_PATH)
			mtk_disp_mutex_add_comp(mtk_crtc->mutex[0], comp->id);
	} else {
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, i,
						  DDP_FIRST_PATH)
			mtk_disp_mutex_add_comp(mtk_crtc->mutex[0], comp->id);
	}

	if (mtk_crtc->is_dual_pipe) {
		mtk_ddp_connect_dual_pipe_path(mtk_crtc, mtk_crtc->mutex[0]);

		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
			mtk_disp_mutex_add_comp(mtk_crtc->mutex[0], comp->id);

		if (drm_crtc_index(crtc) == 0)
			mtk_disp_mutex_add_comp
				(mtk_crtc->mutex[0], DDP_COMPONENT_DSI1);
	}

	/* set mutex sof, eof */
	mtk_disp_mutex_src_set(mtk_crtc, mtk_crtc_is_frame_trigger_mode(crtc));

	/* if VDO mode, enable mutex by CPU here */
	if (!mtk_crtc_is_frame_trigger_mode(crtc))
		mtk_disp_mutex_enable(mtk_crtc->mutex[0]);
}

void mtk_crtc_init_plane_setting(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_plane_state *plane_state;
	struct mtk_plane_pending_state *pending;
	struct drm_plane *plane = &mtk_crtc->planes[0].base;

	plane_state = to_mtk_plane_state(plane->state);
	pending = &plane_state->pending;

	pending->pitch = mtk_crtc->base.state->adjusted_mode.hdisplay*3;
	pending->format = DRM_FORMAT_RGB888;
	pending->src_x = 0;
	pending->src_y = 0;
	pending->dst_x = 0;
	pending->dst_y = 0;
	pending->height = mtk_crtc->base.state->adjusted_mode.vdisplay;
	pending->width = mtk_crtc->base.state->adjusted_mode.hdisplay;
	pending->config = 1;
	pending->dirty = 1;
	pending->enable = true;

	/*constant color layer*/
	pending->addr = 0;
	pending->prop_val[PLANE_PROP_PLANE_ALPHA] = 0xFF;
	pending->prop_val[PLANE_PROP_COMPRESS] = 0;
	pending->prop_val[PLANE_PROP_ALPHA_CON] = 0;

	plane_state->comp_state.lye_id = 0;
	plane_state->comp_state.ext_lye_id = 0;
}

void mtk_crtc_dual_layer_config(struct mtk_drm_crtc *mtk_crtc,
		struct mtk_ddp_comp *comp, unsigned int idx,
		struct mtk_plane_state *plane_state, struct cmdq_pkt *cmdq_handle)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_plane_state plane_state_l;
	struct mtk_plane_state plane_state_r;
	struct mtk_ddp_comp *p_comp;

	mtk_drm_layer_dispatch_to_dual_pipe(plane_state,
		&plane_state_l, &plane_state_r,
		crtc->state->adjusted_mode.hdisplay);

	if (plane_state->comp_state.comp_id == 0)
		plane_state_r.comp_state.comp_id = 0;

	p_comp = priv->ddp_comp[dual_pipe_comp_mapping(comp->id)];
	mtk_ddp_comp_layer_config(p_comp, idx,
				&plane_state_r, cmdq_handle);
	DDPINFO("%s+ comp_id:%d, comp_id:%d\n",
		__func__, p_comp->id,
		plane_state_r.comp_state.comp_id);

	p_comp = comp;
	mtk_ddp_comp_layer_config(p_comp, idx, &plane_state_l,
				  cmdq_handle);
}



/* restore ovl layer config and set dal layer if any */
void mtk_crtc_restore_plane_setting(struct mtk_drm_crtc *mtk_crtc)
{
	unsigned int i, j;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp;

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	if (drm_crtc_index(crtc) == 1)
		mtk_crtc_init_plane_setting(mtk_crtc);

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct mtk_drm_private *priv = crtc->dev->dev_private;
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		if (i >= OVL_PHY_LAYER_NR && !plane_state->comp_state.comp_id)
			continue;
		if (plane_state->comp_state.comp_id)
			comp = priv->ddp_comp[plane_state->comp_state.comp_id];
		else {
			struct mtk_crtc_ddp_ctx *ddp_ctx;

			/* TODO: all plane should contain proper mtk_plane_state
			 */
			ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
			comp = ddp_ctx->ddp_comp[DDP_FIRST_PATH][0];
		}

		if (comp == NULL)
			continue;

		if (mtk_crtc->is_dual_pipe) {
			struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
			struct mtk_plane_state plane_state_l;
			struct mtk_plane_state plane_state_r;
			struct mtk_ddp_comp *p_comp;

			mtk_drm_layer_dispatch_to_dual_pipe(plane_state,
				&plane_state_l, &plane_state_r,
				crtc->state->adjusted_mode.hdisplay);

			if (plane_state->comp_state.comp_id == 0)
				plane_state_r.comp_state.comp_id = 0;

			p_comp = priv->ddp_comp[dual_pipe_comp_mapping(comp->id)];
			mtk_ddp_comp_layer_config(p_comp, i,
						&plane_state_r, cmdq_handle);
			DDPINFO("%s+ comp_id:%d, comp_id:%d\n",
				__func__, p_comp->id,
				plane_state_r.comp_state.comp_id);

			p_comp = comp;
			mtk_ddp_comp_layer_config(p_comp, i, &plane_state_l,
						  cmdq_handle);
		} else {
			mtk_ddp_comp_layer_config(comp, i, plane_state, cmdq_handle);
		}
		if (comp->id == DDP_COMPONENT_OVL2_2L
			&& mtk_crtc->is_dual_pipe) {
			struct mtk_crtc_ddp_ctx *ddp_ctx;

			DDPFUNC();
			if (plane_state->pending.addr)
				plane_state->pending.addr +=
					plane_state->pending.pitch/2;

			plane_state->pending.dst_x = 0;
			plane_state->pending.dst_y = 0;
			ddp_ctx = &mtk_crtc->dual_pipe_ddp_ctx;
			comp = ddp_ctx->ddp_comp[DDP_FIRST_PATH][0];
			plane_state->comp_state.comp_id =
						DDP_COMPONENT_OVL3_2L;
			mtk_ddp_comp_layer_config(comp,
						i, plane_state, cmdq_handle);
			//will be used next time
			plane_state->comp_state.comp_id = DDP_COMPONENT_OVL2_2L;
		}
	}

	if (mtk_drm_dal_enable() && drm_crtc_index(crtc) == 0)
		drm_set_dal(&mtk_crtc->base, cmdq_handle);

	/* Update QOS BW*/
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, cmdq_handle,
			PMQOS_UPDATE_BW, NULL);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
}
static void mtk_crtc_disable_plane_setting(struct mtk_drm_crtc *mtk_crtc)
{
	unsigned int i;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state
			= to_mtk_plane_state(plane->state);

		if (i < OVL_PHY_LAYER_NR || plane_state->comp_state.comp_id) {
			plane_state->pending.enable = 0;
		}
	}
}

static void set_dirty_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

static void mtk_crtc_set_dirty(struct mtk_drm_crtc *mtk_crtc)
{
	struct cmdq_pkt *cmdq_handle;
	struct mtk_cmdq_cb_data *cb_data;

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPINFO("%s:%d, cb data creation failed\n",
			__func__, __LINE__);
		return;
	}

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);

	cmdq_pkt_set_event(cmdq_handle,
		mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);

	cb_data->cmdq_handle = cmdq_handle;
	if (cmdq_pkt_flush_threaded(cmdq_handle,
	    set_dirty_cmdq_cb, cb_data) < 0)
		DDPPR_ERR("failed to flush set_dirty\n");
}

static int __mtk_check_trigger(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	int index = drm_crtc_index(crtc);
	struct mtk_crtc_state *mtk_state;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	CRTC_MMP_EVENT_START(index, check_trigger, 0, 0);

	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	mtk_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE] ||
		(mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE] &&
		atomic_read(&mtk_crtc->already_config))) {
		mtk_crtc_set_dirty(mtk_crtc);
	} else
		DDPINFO("%s skip mtk_crtc_set_dirty\n", __func__);

	CRTC_MMP_EVENT_END(index, check_trigger, 0, 0);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

static int _mtk_crtc_check_trigger(void *data)
{
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *) data;
	struct sched_param param = {.sched_priority = 94 };
	int ret;

	sched_setscheduler(current, SCHED_RR, &param);

	atomic_set(&mtk_crtc->trig_event_act, 0);
	do {
		ret = wait_event_interruptible(mtk_crtc->trigger_event,
			atomic_read(&mtk_crtc->trig_event_act));
		if (ret < 0)
			DDPPR_ERR("wait %s fail, ret=%d\n", __func__, ret);
		atomic_set(&mtk_crtc->trig_event_act, 0);

		__mtk_check_trigger(mtk_crtc);

	} while (!kthread_should_stop());

	return 0;
}

static int _mtk_crtc_check_trigger_delay(void *data)
{
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *) data;
	struct sched_param param = {.sched_priority = 94 };
	int ret;

	sched_setscheduler(current, SCHED_RR, &param);

	atomic_set(&mtk_crtc->trig_delay_act, 0);

	do {
		ret = wait_event_interruptible(mtk_crtc->trigger_delay,
			atomic_read(&mtk_crtc->trig_delay_act));
		if (ret < 0)
			DDPPR_ERR("wait %s fail, ret=%d\n", __func__, ret);
		atomic_set(&mtk_crtc->trig_delay_act, 0);
		atomic_set(&mtk_crtc->delayed_trig, 0);

		usleep_range(32000, 33000);
		if (!atomic_read(&mtk_crtc->delayed_trig))
			__mtk_check_trigger(mtk_crtc);

	} while (!kthread_should_stop());

	return 0;
}

void mtk_crtc_check_trigger(struct mtk_drm_crtc *mtk_crtc, bool delay,
		bool need_lock)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	int index = 0;
	struct mtk_crtc_state *mtk_state;
	struct mtk_panel_ext *panel_ext;

	if (!mtk_crtc) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p\n",
				__func__, __LINE__, crtc);
		return;
	}

	if (need_lock)
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	CRTC_MMP_EVENT_START(index, kick_trigger, (unsigned long)crtc, 0);

	index = drm_crtc_index(crtc);
	if (index) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p, index:%d\n",
				__func__, __LINE__, crtc, index);
		CRTC_MMP_MARK(index, kick_trigger, 0, 1);
		goto err;
	}

	if (!(mtk_crtc->enabled)) {
		DDPINFO("%s:%d, slepted\n", __func__, __LINE__);
		CRTC_MMP_MARK(index, kick_trigger, 0, 2);
		goto err;
	}

	if (!mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base)) {
		DDPINFO("%s:%d, not in trigger mode\n", __func__, __LINE__);
		CRTC_MMP_MARK(index, kick_trigger, 0, 3);
		goto err;
	}

	panel_ext = mtk_crtc->panel_ext;
	mtk_state = to_mtk_crtc_state(crtc->state);
	if (mtk_crtc_is_frame_trigger_mode(crtc) &&
		mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE] &&
		panel_ext && panel_ext->params->doze_delay > 1){
		DDPINFO("%s:%d, doze not to trigger\n", __func__, __LINE__);
		goto err;
	}

	if (delay) {
		/* implicit way make sure wait queue was initiated */
		if (unlikely(&mtk_crtc->trigger_delay_task == NULL)) {
			CRTC_MMP_MARK(index, kick_trigger, 0, 4);
			goto err;
		}
		atomic_set(&mtk_crtc->trig_delay_act, 1);
		wake_up_interruptible(&mtk_crtc->trigger_delay);
	} else {
		/* implicit way make sure wait queue was initiated */
		if (unlikely(&mtk_crtc->trigger_event_task == NULL)) {
			CRTC_MMP_MARK(index, kick_trigger, 0, 5);
			goto err;
		}
		atomic_set(&mtk_crtc->trig_event_act, 1);
		wake_up_interruptible(&mtk_crtc->trigger_event);
	}

err:
	CRTC_MMP_EVENT_END(index, kick_trigger, 0, 0);
	if (need_lock)
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

void mtk_crtc_config_default_path(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_config cfg;
	struct mtk_ddp_comp *comp;
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	cfg.w = crtc->state->adjusted_mode.hdisplay;
	cfg.h = crtc->state->adjusted_mode.vdisplay;
	if (output_comp && drm_crtc_index(crtc) == 0) {
		cfg.w = mtk_ddp_comp_io_cmd(output_comp, NULL,
					DSI_GET_VIRTUAL_WIDTH, NULL);
		cfg.h = mtk_ddp_comp_io_cmd(output_comp, NULL,
					DSI_GET_VIRTUAL_HEIGH, NULL);
	}
	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
		mtk_crtc->panel_ext->params->dyn_fps.switch_en == 1
		&& mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
		cfg.vrefresh =
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
	else
		cfg.vrefresh = crtc->state->adjusted_mode.vrefresh;
	cfg.bpc = mtk_crtc->bpc;
	cfg.p_golden_setting_context = __get_golden_setting_context(mtk_crtc);

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		mtk_ddp_comp_config(comp, &cfg, cmdq_handle);
		mtk_ddp_comp_start(comp, cmdq_handle);

		if (!mtk_drm_helper_get_opt(
				    priv->helper_opt,
				    MTK_DRM_OPT_USE_PQ))
			mtk_ddp_comp_bypass(comp, 1, cmdq_handle);
	}

	if (mtk_crtc->is_dual_pipe) {
		DDPFUNC();
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			mtk_ddp_comp_config(comp, &cfg, cmdq_handle);
			mtk_ddp_comp_start(comp, cmdq_handle);

			if (!mtk_drm_helper_get_opt(
				    priv->helper_opt,
				    MTK_DRM_OPT_USE_PQ))
				mtk_ddp_comp_bypass(comp, 1, cmdq_handle);
		}
	}
	/* Althought some of the m4u port may be enabled in LK stage.
	 * To make sure the driver independent, we still enable all the
	 * componets port here.
	 */
	mtk_crtc_enable_iommu(mtk_crtc, cmdq_handle);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
}

static void mtk_crtc_all_layer_off(struct mtk_drm_crtc *mtk_crtc,
				   struct cmdq_pkt *cmdq_handle)
{
	int i, j, keep_first_layer;
	struct mtk_ddp_comp *comp;

	keep_first_layer = true;
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, cmdq_handle,
			OVL_ALL_LAYER_OFF, &keep_first_layer);

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				OVL_ALL_LAYER_OFF, &keep_first_layer);
	}
}

void mtk_crtc_stop_ddp(struct mtk_drm_crtc *mtk_crtc,
		       struct cmdq_pkt *cmdq_handle)
{
	int i, j;
	struct mtk_ddp_comp *comp;

	/* If VDO mode, stop DSI mode first */
	if (!mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base) &&
	    mtk_crtc_is_connector_enable(mtk_crtc)) {
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
					    DSI_STOP_VDO_MODE, NULL);
	}

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_stop(comp, cmdq_handle);

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
			mtk_ddp_comp_stop(comp, cmdq_handle);
	}
}

/* Stop trig loop and stop all modules in this CRTC */
void mtk_crtc_stop(struct mtk_drm_crtc *mtk_crtc, bool need_wait)
{
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp;
	int i, j;

	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	struct drm_crtc *crtc = &mtk_crtc->base;

	DDPINFO("%s:%d +\n", __func__, __LINE__);

	/* 0. Waiting CLIENT_DSI_CFG thread done */
	if (crtc_id == 0) {
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);

	if (!need_wait)
		goto skip;

	if (crtc_id == 2) {
		int gce_event =
			get_path_wait_event(mtk_crtc, mtk_crtc->ddp_mode);

		if (gce_event > 0)
			cmdq_pkt_wait_no_clear(cmdq_handle, gce_event);
	} else if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base)) {
		/* 1. wait stream eof & clear tocken */
		/* clear eof token to prevent any config after this command */
		cmdq_pkt_wfe(cmdq_handle,
				 mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);

		/* clear dirty token to prevent trigger loop start */
		cmdq_pkt_clear_event(
			cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	} else if (mtk_crtc_is_connector_enable(mtk_crtc)) {
		/* In vdo mode, DSI would be stop when disable connector
		 * Do not wait frame done in this case.
		 */
		cmdq_pkt_wfe(cmdq_handle,
				 mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);
	}

skip:
	/* 2. stop all modules in this CRTC */
	mtk_crtc_stop_ddp(mtk_crtc, cmdq_handle);

	/* 3. Reset QOS BW after CRTC stop */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, cmdq_handle,
			PMQOS_UPDATE_BW, NULL);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	/* 4. Set QOS BW to 0 */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_BW, NULL);

	/* 5. Set HRT BW to 0 */
#ifdef MTK_FB_MMDVFS_SUPPORT
	if (drm_crtc_index(crtc) == 0)
		mtk_disp_set_hrt_bw(mtk_crtc, 0);
#endif

	/* 6. stop trig loop  */
	if (mtk_crtc_with_trigger_loop(crtc)) {
		mtk_crtc_stop_trig_loop(crtc);
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
		if (mtk_crtc_with_sodi_loop(crtc) &&
				(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_sodi_loop(crtc);
#endif
	}

	DDPINFO("%s:%d -\n", __func__, __LINE__);
}

/* TODO: how to remove add-on module? */
void mtk_crtc_disconnect_default_path(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_ddp_comp *comp;
	struct mtk_ddp_comp **ddp_comp;

	/* if VDO mode, disable mutex by CPU here */
	if (!mtk_crtc_is_frame_trigger_mode(crtc))
		mtk_disp_mutex_disable(mtk_crtc->mutex[0]);

	for_each_comp_in_crtc_path_bound(comp, mtk_crtc, i, j, 1) {
		ddp_comp = mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].ddp_comp[i];
		mtk_ddp_remove_comp_from_path(
			mtk_crtc->config_regs, mtk_crtc->mmsys_reg_data,
			ddp_comp[j]->id, ddp_comp[j + 1]->id);
	}

	if (mtk_crtc_is_dc_mode(crtc)) {
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, i,
						  DDP_FIRST_PATH)
			mtk_disp_mutex_remove_comp(mtk_crtc->mutex[1],
						  comp->id);
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, i,
						  DDP_SECOND_PATH)
			mtk_disp_mutex_remove_comp(mtk_crtc->mutex[0],
						  comp->id);
	} else {
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, i,
						  DDP_FIRST_PATH)
			mtk_disp_mutex_remove_comp(mtk_crtc->mutex[0],
						   comp->id);
	}

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
			mtk_disp_mutex_remove_comp(mtk_crtc->mutex[0],
				comp->id);
	}
}

void mtk_drm_crtc_enable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	unsigned int crtc_id = drm_crtc_index(crtc);
	struct cmdq_client *client;
	struct mtk_ddp_comp *comp;
	int i, j;
	struct mtk_ddp_comp *output_comp = NULL;
	int en = 1;

	CRTC_MMP_EVENT_START(crtc_id, enable,
			mtk_crtc->enabled, 0);

	if (mtk_crtc->enabled) {
		CRTC_MMP_MARK(crtc_id, enable, 0, 0);
		DDPINFO("crtc%d skip %s\n", crtc_id, __func__);
		goto end;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		CRTC_MMP_MARK(crtc_id, enable, 0, 1);
		DDPINFO("crtc%d skip %s, ddp_mode: NO_USE\n", crtc_id,
			__func__);
		goto end;
	}
	DDPINFO("crtc%d do %s\n", crtc_id, __func__);
	CRTC_MMP_MARK(crtc_id, enable, 1, 0);

	/*for dual pipe*/
	mtk_crtc_prepare_dual_pipe(mtk_crtc);

	/* attach the crtc to each componet */
	mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_MMCLK_BY_DATARATE,
				&en);
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* 1. power on mtcmos */
	mtk_drm_top_clk_prepare_enable(crtc->dev);

	/* 2. prepare modules would be used in this CRTC */
	mtk_crtc_ddp_prepare(mtk_crtc);
#endif

	/* 3. power on cmdq client */
	if (crtc_id == 2) {
		client = mtk_crtc->gce_obj.client[CLIENT_CFG];
		cmdq_mbox_enable(client->chan);
		CRTC_MMP_MARK(crtc_id, enable, 1, 1);
	}

	/* 4. start trigger loop first to keep gce alive */
	if (mtk_crtc_with_trigger_loop(crtc)) {
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_sodi_loop(crtc);
#endif
		mtk_crtc_start_trig_loop(crtc);
	}

	if (mtk_crtc_is_mem_mode(crtc) || mtk_crtc_is_dc_mode(crtc)) {
		struct golden_setting_context *ctx =
					__get_golden_setting_context(mtk_crtc);
		struct cmdq_pkt *cmdq_handle;
		int gce_event =
			get_path_wait_event(mtk_crtc, (enum CRTC_DDP_PATH)(mtk_crtc->ddp_mode));

		ctx->is_dc = 1;

		cmdq_handle =
			cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
		if (gce_event > 0)
			cmdq_pkt_set_event(cmdq_handle, gce_event);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}

	CRTC_MMP_MARK(crtc_id, enable, 1, 2);
	/* 5. connect path */
	mtk_crtc_connect_default_path(mtk_crtc);

	if (!crtc_id)
		mtk_crtc->qos_ctx->last_hrt_req = 0;

	/* 6. config ddp engine */
	mtk_crtc_config_default_path(mtk_crtc);
	CRTC_MMP_MARK(crtc_id, enable, 1, 3);

	/* 7. disconnect addon module and config */
	mtk_crtc_connect_addon_module(crtc);

	/* 8. restore OVL setting */
	mtk_crtc_restore_plane_setting(mtk_crtc);

	/* 9. Set QOS BW */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_BW, NULL);

	/* 10. set dirty for cmd mode */
	if (mtk_crtc_is_frame_trigger_mode(crtc) &&
		!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE] &&
		!mtk_state->doze_changed)
		mtk_crtc_set_dirty(mtk_crtc);

	/* 11. set vblank*/
	drm_crtc_vblank_on(crtc);

#ifdef MTK_DRM_ESD_SUPPORT
	/* 12. enable ESD check */
	if (mtk_drm_lcm_is_connect())
		mtk_disp_esd_check_switch(crtc, true);
#endif

	/* 13. enable fake vsync if need*/
	mtk_drm_fake_vsync_switch(crtc, true);

	/* 14. set CRTC SW status */
	mtk_crtc_set_status(crtc, true);

end:
	CRTC_MMP_EVENT_END(crtc_id, enable,
			mtk_crtc->enabled, 0);
}

static void mtk_drm_crtc_wk_lock(struct drm_crtc *crtc, bool get,
	const char *func, int line)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DDPMSG("CRTC%d %s wakelock %s %d\n",
		drm_crtc_index(crtc), (get ? "hold" : "release"),
		func, line);

	if (get)
		__pm_stay_awake(&mtk_crtc->wk_lock);
	else
		__pm_relax(&mtk_crtc->wk_lock);
}

unsigned int mtk_drm_dump_wk_lock(
	struct mtk_drm_private *priv, char *stringbuf, int buf_len)
{
	unsigned int len = 0;
	int i = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;

	len += scnprintf(stringbuf + len, buf_len - len,
		 "==========    wakelock Info    ==========\n");

	for (i = 0; i < 3; i++) {
		crtc = priv->crtc[i];
		if (!crtc)
			continue;
		mtk_crtc = to_mtk_crtc(crtc);

		len += scnprintf(stringbuf + len, buf_len - len,
			 "CRTC%d wk active:%d;  ", i,
			 mtk_crtc->wk_lock.active);
	}

	len += scnprintf(stringbuf + len, buf_len - len, "\n\n");

	return len;
}

void mtk_drm_crtc_atomic_resume(struct drm_crtc *crtc,
				struct drm_crtc_state *old_crtc_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int index = drm_crtc_index(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc0 = to_mtk_crtc(priv->crtc[0]);

	/* When open VDS path switch feature, After VDS created,
	 * VDS will call setcrtc, So atomic commit will be called,
	 * but OVL0_2L is in use by main disp, So we need to skip
	 * this action.
	 */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH) && (index == 2)) {
		if (atomic_read(&mtk_crtc0->already_config) &&
			(!priv->vds_path_switch_done)) {
			DDPMSG("Switch vds: VDS need skip first crtc enable\n");
			return;
		} else if (!atomic_read(&mtk_crtc0->already_config)) {
			DDPMSG("Switch vds: VDS no need skip as crtc0 disable\n");
			priv->vds_path_enable = 1;
		}
	}


	CRTC_MMP_EVENT_START(index, resume,
			mtk_crtc->enabled, index);

	/* hold wakelock */
	mtk_drm_crtc_wk_lock(crtc, 1, __func__, __LINE__);

	mtk_drm_crtc_enable(crtc);

	CRTC_MMP_EVENT_END(index, resume,
			mtk_crtc->enabled, 0);
}

bool mtk_crtc_with_sub_path(struct drm_crtc *crtc, unsigned int ddp_mode);

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
void mtk_crtc_config_round_corner(struct drm_crtc *crtc,
				  struct cmdq_pkt *handle)
{

	struct mtk_ddp_config cfg;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	int i, j;
	int cur_path_idx;

	cfg.w = crtc->mode.hdisplay;
	cfg.h = crtc->mode.vdisplay;

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		cur_path_idx = DDP_SECOND_PATH;
	else
		cur_path_idx = DDP_FIRST_PATH;

	mtk_crtc_wait_frame_done(mtk_crtc,
		handle, cur_path_idx, 0);

	for_each_comp_in_cur_crtc_path(
		comp, mtk_crtc, i, j)
		if (comp->id == DDP_COMPONENT_POSTMASK0 ||
			comp->id == DDP_COMPONENT_POSTMASK1) {
			mtk_ddp_comp_config(comp, &cfg, handle);
			break;
		}

	if (!mtk_crtc->is_dual_pipe)
		return;

	for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
		if (comp->id == DDP_COMPONENT_POSTMASK0 ||
			comp->id == DDP_COMPONENT_POSTMASK1) {
			mtk_ddp_comp_config(comp, &cfg, handle);
			break;
		}
	}
}

static inline struct mtk_drm_gem_obj *
__load_rc_pattern(struct drm_crtc *crtc, size_t size, void *addr)
{
	struct mtk_drm_gem_obj *gem;

	if (!size || !addr) {
		DDPPR_ERR("%s invalid round_corner size or addr\n",
			__func__);
		return NULL;
	}

	gem = mtk_drm_gem_create(
		crtc->dev, size, true);

	if (!gem) {
		DDPPR_ERR("%s gem create fail\n", __func__);
		return NULL;
	}

	memcpy(gem->kvaddr, addr,
	       size);

	return gem;
}

void mtk_crtc_load_round_corner_pattern(struct drm_crtc *crtc,
					struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_params *panel_ext = mtk_drm_get_lcm_ext_params(crtc);

	if (panel_ext && panel_ext->round_corner_en) {
		mtk_crtc->round_corner_gem =
			__load_rc_pattern(crtc, panel_ext->corner_pattern_tp_size,
						panel_ext->corner_pattern_lt_addr);

		mtk_crtc->round_corner_gem_l =
			__load_rc_pattern(crtc, panel_ext->corner_pattern_tp_size_l,
						panel_ext->corner_pattern_lt_addr_l);
		mtk_crtc->round_corner_gem_r =
			__load_rc_pattern(crtc, panel_ext->corner_pattern_tp_size_r,
						panel_ext->corner_pattern_lt_addr_r);

		mtk_crtc_config_round_corner(crtc, handle);
	}
}
#endif

struct drm_display_mode *mtk_drm_crtc_avail_disp_mode(struct drm_crtc *crtc,
	unsigned int idx)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	/* If not crtc0, use crtc0 instead. TODO: need to reconsidered for
	 * secondary display, i.e: DP, HDMI
	 */

	if (drm_crtc_index(crtc) != 0) {
		struct drm_crtc *crtc0;

		DDPPR_ERR("%s no support CRTC%u", __func__,
			drm_crtc_index(crtc));

		drm_for_each_crtc(crtc0, crtc->dev) {
			if (drm_crtc_index(crtc0) == 0)
				break;
		}

		mtk_crtc = to_mtk_crtc(crtc0);
	}

	if (idx >= mtk_crtc->avail_modes_num) {
		DDPPR_ERR("%s idx:%u exceed avail_num:%u", __func__,
			idx, mtk_crtc->avail_modes_num);
		idx = 0;
	}

	return &mtk_crtc->avail_modes[idx];
}

void mtk_drm_crtc_init_para(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	struct mtk_ddp_comp *comp;
	struct drm_display_mode *timing = NULL;
	int en = 1;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL)
		return;

	mtk_ddp_comp_io_cmd(comp, NULL, DSI_GET_TIMING, &timing);
	if (timing == NULL)
		return;

	crtc->mode.hdisplay = timing->hdisplay;
	crtc->mode.vdisplay = timing->vdisplay;
	crtc->state->adjusted_mode.clock        = timing->clock;
	crtc->state->adjusted_mode.hdisplay     = timing->hdisplay;
	crtc->state->adjusted_mode.hsync_start  = timing->hsync_start;
	crtc->state->adjusted_mode.hsync_end    = timing->hsync_end;
	crtc->state->adjusted_mode.htotal       = timing->htotal;
	crtc->state->adjusted_mode.hskew        = timing->hskew;
	crtc->state->adjusted_mode.vdisplay     = timing->vdisplay;
	crtc->state->adjusted_mode.vsync_start  = timing->vsync_start;
	crtc->state->adjusted_mode.vsync_end    = timing->vsync_end;
	crtc->state->adjusted_mode.vtotal       = timing->vtotal;
	crtc->state->adjusted_mode.vscan        = timing->vscan;
	crtc->state->adjusted_mode.vrefresh     = timing->vrefresh;

	drm_invoke_fps_chg_callbacks(timing->vrefresh);
	mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);

	/* backup display context */
	if (crtc_id == 0) {
		pgc->mode = *timing;
		DDPMSG("width:%d, height:%d\n", pgc->mode.hdisplay,
			pgc->mode.vdisplay);
	}

	/* store display mode for crtc0 only */
	if (comp && drm_crtc_index(&mtk_crtc->base) == 0) {
		mtk_ddp_comp_io_cmd(comp, NULL,
			DSI_SET_CRTC_AVAIL_MODES, mtk_crtc);
		mtk_ddp_comp_io_cmd(comp, NULL, SET_MMCLK_BY_DATARATE, &en);
		/*need enable hrt_bw for pan display*/
#ifdef MTK_FB_MMDVFS_SUPPORT
		mtk_drm_pan_disp_set_hrt_bw(crtc, __func__);
#endif
	} else {
		mtk_crtc->avail_modes_num = 0;
		mtk_crtc->avail_modes = NULL;
	}
}

void mtk_crtc_first_enable_ddp_config(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp;
	struct mtk_ddp_config cfg = {0};
	int i, j;
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	cfg.w = crtc->mode.hdisplay;
	cfg.h = crtc->mode.vdisplay;
	if (output_comp && drm_crtc_index(crtc) == 0) {
		cfg.w = mtk_ddp_comp_io_cmd(output_comp, NULL,
					DSI_GET_VIRTUAL_WIDTH, NULL);
		cfg.h = mtk_ddp_comp_io_cmd(output_comp, NULL,
					DSI_GET_VIRTUAL_HEIGH, NULL);
	}
	cfg.p_golden_setting_context =
			__get_golden_setting_context(mtk_crtc);
	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cmdq_pkt_clear_event(cmdq_handle,
			     mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);
	cmdq_pkt_clear_event(cmdq_handle,
			     mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	/*1. Show LK logo only */
	mtk_crtc_all_layer_off(mtk_crtc, cmdq_handle);

/*2. Load Round Corner */
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	mtk_crtc_load_round_corner_pattern(&mtk_crtc->base, cmdq_handle);
#endif

	/*3. Enable M4U port and replace OVL address to mva */
	mtk_crtc_enable_iommu_runtime(mtk_crtc, cmdq_handle);

	/*4. Enable Frame done IRQ &  process first config */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		mtk_ddp_comp_first_cfg(comp, &cfg, cmdq_handle);
		mtk_ddp_comp_io_cmd(comp, cmdq_handle, IRQ_LEVEL_ALL, NULL);
		mtk_ddp_comp_io_cmd(comp, cmdq_handle,
			MTK_IO_CMD_RDMA_GOLDEN_SETTING, &cfg);
	}
	if (mtk_crtc->is_dual_pipe) {
		DDPFUNC();
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			mtk_ddp_comp_first_cfg(comp, &cfg, cmdq_handle);
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				IRQ_LEVEL_ALL, NULL);
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				MTK_IO_CMD_RDMA_GOLDEN_SETTING, &cfg);
		}
	}
	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
		mtk_crtc_set_dirty(mtk_crtc);
}

void mtk_drm_crtc_first_enable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_drm_private *priv = crtc->dev->dev_private;
#endif

	if (mtk_crtc->enabled) {
		DDPINFO("crtc%d skip %s\n", crtc_id, __func__);
		return;
	}
	DDPINFO("crtc%d do %s\n", crtc_id, __func__);

	/* 1. hold wakelock */
	mtk_drm_crtc_wk_lock(crtc, 1, __func__, __LINE__);

	/*for dual pipe*/
	mtk_crtc_prepare_dual_pipe(mtk_crtc);

	mtk_drm_crtc_init_para(crtc);

	/* 2. start trigger loop first to keep gce alive */
	if (mtk_crtc_with_trigger_loop(crtc)) {
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_sodi_loop(crtc);
#endif
		mtk_crtc_start_trig_loop(crtc);
	}

	/* 3. Regsister configuration */
	mtk_crtc_first_enable_ddp_config(mtk_crtc);
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* 4. power on mtcmos */
	mtk_drm_top_clk_prepare_enable(crtc->dev);

	/* 5. prepare modules would be used in this CRTC */
	mtk_crtc_ddp_prepare(mtk_crtc);

	/* 6. sodi config */
	if (priv->data->sodi_config) {
		struct mtk_ddp_comp *comp;
		int i, j;
		bool en = 1;

		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			priv->data->sodi_config(crtc->dev, comp->id, NULL, &en);
	}
#endif

	/* 7. set vblank*/
	drm_crtc_vblank_on(crtc);

	/* 8. set CRTC SW status */
	mtk_crtc_set_status(crtc, true);

	/* 9. power off mtcmos*/
	/* Because of align lk hw power status,
	 * we power on mtcmos at the beginning of the display initialization.
	 * We power off mtcmos at the end of the display initialization.
	 * Here we only decrease ref count, the power will hold on.
	 */
	mtk_drm_top_clk_disable_unprepare(crtc->dev);
}

void mtk_drm_crtc_disable(struct drm_crtc *crtc, bool need_wait)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	struct cmdq_client *client;
	int en = 0;
	struct mtk_drm_private *private = crtc->dev->dev_private;

	CRTC_MMP_EVENT_START(crtc_id, disable,
			mtk_crtc->enabled, 0);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_MMCLK_BY_DATARATE,
				&en);

	if (!mtk_crtc->enabled) {
		CRTC_MMP_MARK(crtc_id, disable, 0, 0);
		DDPINFO("crtc%d skip %s\n", crtc_id, __func__);
		goto end;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		CRTC_MMP_MARK(crtc_id, disable, 0, 1);
		DDPINFO("crtc%d skip %s, ddp_mode: NO_USE\n", crtc_id,
			__func__);
		goto end;
	}
	DDPINFO("%s:%d crtc%d+\n", __func__, __LINE__, crtc_id);
	CRTC_MMP_MARK(crtc_id, disable, 1, 0);

	/* 1. kick idle */
	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	/* 2. disable fake vsync if need */
	mtk_drm_fake_vsync_switch(crtc, false);

	/* 3. disable ESD check */
	if (mtk_drm_lcm_is_connect())
		mtk_disp_esd_check_switch(crtc, false);

	/* 4. stop CRTC */
	mtk_crtc_stop(mtk_crtc, need_wait);
	CRTC_MMP_MARK(crtc_id, disable, 1, 1);

	/* 5. disconnect addon module and recover config */
	mtk_crtc_disconnect_addon_module(crtc);

	/* 6. disconnect path */
	mtk_crtc_disconnect_default_path(mtk_crtc);

	/* 7. disable vblank */
	drm_crtc_vblank_off(crtc);

	/* 8. power off cmdq client */
	if (crtc_id == 2) {
		client = mtk_crtc->gce_obj.client[CLIENT_CFG];
		cmdq_mbox_disable(client->chan);
		CRTC_MMP_MARK(crtc_id, disable, 1, 2);
	}

	/* If open dynamic OVL 4+2, need switch ovl back to main disp */
	if (!private) {
		DDPPR_ERR("%s private is NULL\n", __func__);
		goto end;
	}
	if (mtk_drm_helper_get_opt(private->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH) &&
		(crtc_id == 2)) {
		enum MTK_DRM_HELPER_OPT helper_opt;

		private->need_vds_path_switch = 0;
		private->vds_path_switch_done = 0;
		private->vds_path_enable = 0;

		/* Open RPO */
		mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_RPO", 1);
		helper_opt =
			mtk_drm_helper_name_to_opt(private->helper_opt,
				"MTK_DRM_OPT_RPO");
		mtk_update_layering_opt_by_disp_opt(helper_opt, 1);
		mtk_set_layering_opt(LYE_OPT_RPO, 1);

		/* OVL0_2l switch back to main path */
		if (private->need_vds_path_switch_back) {
			DDPMSG("Switch vds: crtc2 vds set ddp mode to DL\n");
			mtk_need_vds_path_switch(private->crtc[0]);
		}
	}

	/* 9. power off all modules in this CRTC */
	mtk_crtc_ddp_unprepare(mtk_crtc);

	/* 10. power off MTCMOS*/
	/* TODO: need to check how to unprepare MTCMOS */
	if (mtk_dsi_get_clk_refcnt(output_comp) != 0)
		DDPPR_ERR("%s: clk_refcnt = %d, dsi is not off when disable mtcmos\n",
			__func__, mtk_dsi_get_clk_refcnt(output_comp));
	mtk_drm_top_clk_disable_unprepare(crtc->dev);

	/* Workaround: if CRTC2, reset wdma->fb to NULL to prevent CRTC2
	 * config wdma and cause KE
	 */
	if (crtc_id == 2) {
		comp = mtk_ddp_comp_find_by_id(crtc, DDP_COMPONENT_WDMA0);
		if (!comp)
			comp = mtk_ddp_comp_find_by_id(crtc,
					DDP_COMPONENT_WDMA1);
		if (comp)
			comp->fb = NULL;
	}

	/* 11. set CRTC SW status */
	mtk_crtc_set_status(crtc, false);

end:
	CRTC_MMP_EVENT_END(crtc_id, disable,
			mtk_crtc->enabled, 0);
	DDPINFO("%s:%d -\n", __func__, __LINE__);
}

#ifdef MTK_DRM_FENCE_SUPPORT
static void mtk_drm_crtc_release_fence(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int id = drm_crtc_index(crtc), i;
	int session_id = -1;

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (id + 1 == MTK_SESSION_TYPE(priv->session_id[i])) {
			session_id = priv->session_id[i];
			break;
		}
	}

	if (session_id == -1) {
		DDPPR_ERR("%s no session for CRTC%u\n", __func__, id);
		return;
	}

	/* release input layer fence */
	DDPMSG("CRTC%u release input fence\n", id);
	for (i = 0; i < MTK_TIMELINE_OUTPUT_TIMELINE_ID; i++)
		mtk_release_layer_fence(session_id, i);

	/* release output fence for crtc2 */
	if (id == 2) {
		DDPMSG("CRTC%u release output fence\n", id);
		mtk_release_layer_fence(session_id,
					MTK_TIMELINE_OUTPUT_TIMELINE_ID);
	}

	/* release present fence */
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_PRIMARY ||
			MTK_SESSION_TYPE(session_id) == MTK_SESSION_EXTERNAL) {
		mtk_drm_suspend_release_present_fence(crtc->dev->dev, id);
		mtk_drm_suspend_release_sf_present_fence(crtc->dev->dev, id);
	}
}
#endif

void mtk_drm_crtc_suspend(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int index = drm_crtc_index(crtc);

	CRTC_MMP_EVENT_START(index, suspend,
			mtk_crtc->enabled, 0);

	mtk_drm_crtc_wait_blank(mtk_crtc);

/* disable engine secure state */
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if (index == 2 && mtk_crtc->sec_on) {
		mtk_crtc_disable_secure_state(crtc);
		mtk_crtc->sec_on = false;
	}
#endif
	mtk_drm_crtc_disable(crtc, true);

	mtk_crtc_disable_plane_setting(mtk_crtc);

/* release all fence */
#ifdef MTK_DRM_FENCE_SUPPORT
	mtk_drm_crtc_release_fence(crtc);
	mtk_crtc_release_lye_idx(crtc);
#endif
	atomic_set(&mtk_crtc->already_config, 0);

	/* release wakelock */
	mtk_drm_crtc_wk_lock(crtc, 0, __func__, __LINE__);

	CRTC_MMP_EVENT_END(index, suspend,
			mtk_crtc->enabled, 0);
}

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
int mtk_crtc_check_out_sec(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	int out_sec = 0;

	if (state->prop_val[CRTC_PROP_OUTPUT_ENABLE]) {
		/* Output buffer configuration for virtual display */

		out_sec = mtk_drm_fb_is_secure(
				mtk_drm_framebuffer_lookup(crtc->dev,
				state->prop_val[CRTC_PROP_OUTPUT_FB_ID]));

		DDPINFO("%s lookup wb fb:%u sec:%d\n", __func__,
			state->prop_val[CRTC_PROP_OUTPUT_FB_ID], out_sec);
	}

	return out_sec;
}

static u64 mtk_crtc_secure_port_lookup(struct mtk_ddp_comp *comp)
{
	u64 ret = 0;

	if (!comp)
		return ret;

	switch (comp->id) {
	case DDP_COMPONENT_WDMA0:
		ret = 1LL << CMDQ_SEC_DISP_WDMA0;
		break;
	case DDP_COMPONENT_WDMA1:
		ret = 1LL << CMDQ_SEC_DISP_WDMA1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static u64 mtk_crtc_secure_dapc_lookup(struct mtk_ddp_comp *comp)
{
	u64 ret = 0;

	if (!comp)
		return ret;

	switch (comp->id) {
	case DDP_COMPONENT_WDMA0:
		ret = 1L << CMDQ_SEC_DISP_WDMA0;
		break;
	case DDP_COMPONENT_WDMA1:
		ret = 1L << CMDQ_SEC_DISP_WDMA1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

void mtk_crtc_disable_secure_state(struct drm_crtc *crtc)
{
	struct cmdq_pkt *cmdq_handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = NULL;
	u32 sec_disp_type, idx = drm_crtc_index(crtc);
	u64 sec_disp_port;
	u64 sec_disp_dapc = 0;

	comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (idx == 0) {
		sec_disp_type =
			CMDQ_SEC_DISP_PRIMARY_DISABLE_SECURE_PATH;
		sec_disp_port = 0;
	} else {
		sec_disp_type =
			CMDQ_SEC_DISP_SUB_DISABLE_SECURE_PATH;
		sec_disp_port = (idx == 1) ? 0 :
			mtk_crtc_secure_port_lookup(comp);
		sec_disp_dapc = (idx == 1) ? 0 :
			mtk_crtc_secure_dapc_lookup(comp);
	}
	DDPDBG("%s+ crtc%d\n", __func__, drm_crtc_index(crtc));
	mtk_crtc_pkt_create(&cmdq_handle, crtc,
		mtk_crtc->gce_obj.client[CLIENT_SEC_CFG]);
	/* Secure path only support DL mode, so we just wait
	 * the first path frame done here
	 */
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	/* Disable secure path */
	cmdq_sec_pkt_set_data(cmdq_handle,
		sec_disp_dapc,
		sec_disp_port,
		sec_disp_type,
		CMDQ_METAEX_NONE);
#ifdef CONFIG_MTK_SVP_ON_MTEE_SUPPORT
	if (idx == 2)
		cmdq_sec_pkt_set_mtee(cmdq_handle, true, SEC_ID_WFD);
	else
		cmdq_sec_pkt_set_mtee(cmdq_handle, true, SEC_ID_SVP);
#endif

	if (idx == 2)
		mtk_ddp_comp_io_cmd(comp, cmdq_handle, IRQ_LEVEL_ALL, NULL);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
	DDPINFO("%s-\n", __func__);
}
#endif

struct cmdq_pkt *mtk_crtc_gce_commit_begin(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *cmdq_handle;

	if (mtk_crtc->sec_on)
		mtk_crtc_pkt_create(&cmdq_handle, crtc,
			mtk_crtc->gce_obj.client[CLIENT_SEC_CFG]);
	else if (mtk_crtc_is_dc_mode(crtc))
		mtk_crtc_pkt_create(&cmdq_handle, crtc,
			mtk_crtc->gce_obj.client[CLIENT_SUB_CFG]);
	else
		mtk_crtc_pkt_create(&cmdq_handle, crtc,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	if (mtk_crtc->sec_on) {
	#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		u32 sec_disp_type, idx = drm_crtc_index(crtc);
		u64 sec_disp_port;
		u64 sec_disp_dapc = 0;
		struct mtk_ddp_comp *comp = NULL;

		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (idx == 0) {
			sec_disp_type = CMDQ_SEC_PRIMARY_DISP;
			sec_disp_port = 0;
		} else {
			sec_disp_type = CMDQ_SEC_SUB_DISP;
			sec_disp_port = (idx == 1) ? 0 :
				mtk_crtc_secure_port_lookup(comp);
			sec_disp_dapc = (idx == 1) ? 0 :
				mtk_crtc_secure_dapc_lookup(comp);
			if (idx == 2)
				mtk_ddp_comp_io_cmd(comp, cmdq_handle,
						IRQ_LEVEL_IDLE, NULL);
		}

		DDPMSG("%s sec dapc:%d port:%u\n", __func__, sec_disp_dapc,
			sec_disp_port);
		cmdq_sec_pkt_set_data(cmdq_handle, sec_disp_dapc,
			sec_disp_port, sec_disp_type,
			CMDQ_METAEX_NONE);
#ifdef CONFIG_MTK_SVP_ON_MTEE_SUPPORT
		if (idx == 2)
			cmdq_sec_pkt_set_mtee(cmdq_handle, true, SEC_ID_WFD);
		else
			cmdq_sec_pkt_set_mtee(cmdq_handle, true, SEC_ID_SVP);
#endif
	#endif
		DDPDBG("%s:%d crtc:0x%p, sec_on:%d +\n",
			__func__, __LINE__,
			crtc,
			mtk_crtc->sec_on);
	}
	return cmdq_handle;
}

static void mtk_drm_crtc_atomic_begin(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int index = drm_crtc_index(crtc);
	struct mtk_ddp_comp *comp;
	int i, j;
	int crtc_idx = drm_crtc_index(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	/* When open VDS path switch feature, we will resume VDS crtc
	 * in it's second atomic commit, and the crtc will be resumed
	 * one time.
	 */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH) && (crtc_idx == 2))
		if (priv->vds_path_switch_done &&
			!priv->vds_path_enable) {
			DDPMSG("Switch vds: CRTC2 vds enable\n");
			mtk_drm_crtc_atomic_resume(crtc, NULL);
			priv->vds_path_enable = 1;
		}

	CRTC_MMP_EVENT_START(index, atomic_begin,
			(unsigned long)mtk_crtc->event,
			(unsigned long)state->base.event);
	if (mtk_crtc->event && state->base.event)
		DRM_ERROR("new event while there is still a pending event\n");

	if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		CRTC_MMP_MARK(index, atomic_begin, 0, 0);
		goto end;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	if (state->base.event) {
		state->base.event->pipe = index;
		if (drm_crtc_vblank_get(crtc) != 0)
			DDPAEE("%s:%d, invalid vblank:%d, crtc:%p\n",
				__func__, __LINE__,
				drm_crtc_vblank_get(crtc), crtc);
		mtk_crtc->event = state->base.event;
		state->base.event = NULL;
	}

	mtk_drm_trace_begin("mtk_drm_crtc_atomic:%d-%d",
		crtc_idx, state->prop_val[CRTC_PROP_PRES_FENCE_IDX]);

	state->cmdq_handle = mtk_crtc_gce_commit_begin(crtc);

#ifdef MTK_DRM_ADVANCE
	mtk_crtc_update_ddp_state(crtc, old_crtc_state, state,
				  state->cmdq_handle);
#endif

	/* reset BW */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		comp->qos_bw = 0;
		comp->fbdc_bw = 0;
		comp->hrt_bw = 0;
	}

end:
	CRTC_MMP_EVENT_END(index, atomic_begin,
			(unsigned long)mtk_crtc->event,
			(unsigned long)state->base.event);
}

void mtk_drm_layer_dispatch_to_dual_pipe(
	struct mtk_plane_state *plane_state,
	struct mtk_plane_state *plane_state_l,
	struct mtk_plane_state *plane_state_r,
	unsigned int w)
{
	int src_w, src_h, dst_w, dst_h;
	int left_bg = w/2;
	int right_bg = w/2;
	int roi_w = w;
	struct mtk_crtc_state *crtc_state = NULL;

	memcpy(plane_state_l,
		plane_state, sizeof(struct mtk_plane_state));
	memcpy(plane_state_r,
		plane_state, sizeof(struct mtk_plane_state));

	if (plane_state->base.crtc != NULL) {
		crtc_state = to_mtk_crtc_state(plane_state->base.crtc->state);

		src_w = drm_rect_width(&plane_state->base.src) >> 16;
		src_h = drm_rect_height(&plane_state->base.src) >> 16;
		dst_w = drm_rect_width(&plane_state->base.dst);
		dst_h = drm_rect_height(&plane_state->base.dst);

		if (src_w < dst_w || src_h < dst_h) {
			left_bg = crtc_state->rsz_param[0].in_len;
			right_bg = crtc_state->rsz_param[1].in_len;
			roi_w = crtc_state->rsz_src_roi.width;
			DDPDBG("dual rsz %u, %u, %u\n", left_bg, right_bg, roi_w);
		}
	} else {
		DDPINFO("%s crtc is NULL\n", __func__);
	}

	/*left path*/
	plane_state_l->pending.width  = left_bg -
		plane_state_l->pending.dst_x;

	if (plane_state_l->pending.width > plane_state->pending.width)
		plane_state_l->pending.width = plane_state->pending.width;

	if (w/2 <= plane_state->pending.dst_x) {
		plane_state_l->pending.dst_x = 0;
		plane_state_l->pending.width = 0;
		plane_state_l->pending.height = 0;
		plane_state_l->pending.enable = 0;
	}

	if (plane_state_l->pending.width == 0)
		plane_state_l->pending.height = 0;

	DDPDBG("plane_l (%u,%u) (%u,%u), (%u,%u)\n",
		plane_state_l->pending.src_x, plane_state_l->pending.src_y,
		plane_state_l->pending.dst_x, plane_state_l->pending.dst_y,
		plane_state_l->pending.width, plane_state_l->pending.height);

	/*right path*/

	plane_state_r->comp_state.comp_id =
		dual_pipe_comp_mapping(plane_state->comp_state.comp_id);
	plane_state_r->pending.width +=
		plane_state_r->pending.dst_x - (roi_w - right_bg);

	if (plane_state_r->pending.width
		> plane_state->pending.width)
		plane_state_r->pending.width =
		plane_state->pending.width;

	plane_state_r->pending.dst_x +=
		plane_state->pending.width -
		plane_state_r->pending.width - (roi_w - right_bg);

	plane_state_r->pending.src_x +=
		plane_state->pending.width -
		plane_state_r->pending.width;

	if ((roi_w - right_bg) > (plane_state->pending.width
		+ plane_state->pending.dst_x)) {
		plane_state_r->pending.dst_x = 0;
		plane_state_r->pending.width = 0;
		plane_state_r->pending.height = 0;
		plane_state_r->pending.enable = 0;
	}

	if (plane_state_r->pending.width == 0)
		plane_state_r->pending.height = 0;

	DDPDBG("plane_r (%u,%u) (%u,%u), (%u,%u)\n",
		plane_state_r->pending.src_x, plane_state_r->pending.src_y,
		plane_state_r->pending.dst_x, plane_state_r->pending.dst_y,
		plane_state_r->pending.width, plane_state_r->pending.height);
}


void mtk_drm_crtc_plane_update(struct drm_crtc *crtc, struct drm_plane *plane,
			       struct mtk_plane_state *plane_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int plane_index = to_crtc_plane_index(plane->index);
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc_state);
	struct mtk_ddp_comp *comp = mtk_crtc_get_comp(crtc, 0, 0);
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	unsigned int v = crtc->state->adjusted_mode.vdisplay;
	unsigned int h = crtc->state->adjusted_mode.hdisplay;
#endif
	struct cmdq_pkt *cmdq_handle = state->cmdq_handle;
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	unsigned int last_fence, cur_fence, sub;
	dma_addr_t addr;

	if (comp)
		DDPINFO("%s+ comp_id:%d, comp_id:%d\n", __func__, comp->id,
		    plane_state->comp_state.comp_id);
	else {
		DDPINFO("%s+ comp is NULL\n", __func__);
		return;
	}

	if (priv == NULL) {
		DDPINFO("%s+ priv is NULL\n", __func__);
		return;
	}

	/* When use Dynamic OVL 4+2 switch feature, need reAttach
	 * crtc's comps, In order to fix comp attach wrong crtc issue,
	 * The first problem solved was GreenScreen, that is play SVP
	 * DRM, then connect WFD, disconnect WFD, then play SVP DRM again,
	 * the video is green on phone.
	 */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH))
		mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);

	if (plane_state->pending.enable) {
		if (mtk_crtc->is_dual_pipe) {
			struct mtk_plane_state plane_state_l;
			struct mtk_plane_state plane_state_r;

			if ((plane_state->comp_state.comp_id == 0) && comp)
				plane_state->comp_state.comp_id = comp->id;

			mtk_drm_layer_dispatch_to_dual_pipe(plane_state,
				&plane_state_l, &plane_state_r,
				crtc->state->adjusted_mode.hdisplay);

			if (priv)
				comp = priv->ddp_comp[plane_state_r.comp_state.comp_id];
			mtk_ddp_comp_layer_config(comp, plane_index,
						&plane_state_r, cmdq_handle);
			DDPINFO("%s+ comp_id:%d, comp_id:%d\n",
				__func__, comp->id,
				plane_state_r.comp_state.comp_id);

			comp = mtk_crtc_get_plane_comp(crtc, &plane_state_l);

			mtk_ddp_comp_layer_config(comp, plane_index, &plane_state_l,
						  cmdq_handle);
		} else {
			comp = mtk_crtc_get_plane_comp(crtc, plane_state);

			mtk_ddp_comp_layer_config(comp, plane_index, plane_state,
						  cmdq_handle);
		}
#ifdef CONFIG_MTK_DISPLAY_CMDQ
		mtk_wb_atomic_commit(mtk_crtc, v, h, state->cmdq_handle);
#else
		mtk_wb_atomic_commit(mtk_crtc);
#endif
	} else if (state->prop_val[CRTC_PROP_USER_SCEN] &
		USER_SCEN_BLANK) {
	/* plane disable at mtk_crtc_get_plane_comp_state() actually */
	/* following statement is for disable all layers during suspend */

		if (mtk_crtc->is_dual_pipe) {
			struct mtk_plane_state plane_state_l;
			struct mtk_plane_state plane_state_r;

			if ((plane_state->comp_state.comp_id == 0) && comp)
				plane_state->comp_state.comp_id = comp->id;

			mtk_drm_layer_dispatch_to_dual_pipe(plane_state,
				&plane_state_l, &plane_state_r,
				crtc->state->adjusted_mode.hdisplay);

			if (priv)
				comp = priv->ddp_comp[plane_state_r.comp_state.comp_id];
			mtk_ddp_comp_layer_config(comp, plane_index,
						&plane_state_r, cmdq_handle);
			DDPINFO("%s+D comp_id:%d, comp_id:%d\n",
				__func__, comp->id,
				plane_state_r.comp_state.comp_id);

			comp = mtk_crtc_get_plane_comp(crtc, &plane_state_l);

			mtk_ddp_comp_layer_config(comp, plane_index, &plane_state_l,
						  cmdq_handle);
		}  else {
			comp = mtk_crtc_get_plane_comp(crtc, plane_state);

			mtk_ddp_comp_layer_config(comp, plane_index, plane_state,
						  cmdq_handle);
		}
	}
	last_fence = *(unsigned int *)(cmdq_buf->va_base +
				       DISP_SLOT_CUR_CONFIG_FENCE(plane_index));
	cur_fence = plane_state->pending.prop_val[PLANE_PROP_NEXT_BUFF_IDX];

	addr = cmdq_buf->pa_base + DISP_SLOT_CUR_CONFIG_FENCE(plane_index);
	if (cur_fence != -1 && cur_fence > last_fence)
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base, addr,
			       cur_fence, ~0);

	if (plane_state->pending.enable &&
	    plane_state->pending.format != DRM_FORMAT_C8)
		sub = 1;
	else
		sub = 0;
	addr = cmdq_buf->pa_base + DISP_SLOT_SUBTRACTOR_WHEN_FREE(plane_index);
	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base, addr, sub, ~0);

	DDPINFO("%s-\n", __func__);
}

static void mtk_crtc_wb_comp_config(struct drm_crtc *crtc,
	struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_crtc_ddp_ctx *ddp_ctx = NULL;
	struct mtk_ddp_config cfg;

	comp = mtk_ddp_comp_find_by_id(crtc, DDP_COMPONENT_WDMA0);
	if (!comp)
		comp = mtk_ddp_comp_find_by_id(crtc, DDP_COMPONENT_WDMA1);
	if (!comp) {
		DDPPR_ERR("The wb component is not exsit\n");
		return;
	}

	memset(&cfg, 0x0, sizeof(struct mtk_ddp_config));
	if (state->prop_val[CRTC_PROP_OUTPUT_ENABLE]) {
		/* Output buffer configuration for virtual display */
		DDPINFO("lookup wb fb:%u\n",
			state->prop_val[CRTC_PROP_OUTPUT_FB_ID]);
		comp->fb = mtk_drm_framebuffer_lookup(crtc->dev,
				state->prop_val[CRTC_PROP_OUTPUT_FB_ID]);
		if (comp->fb == NULL) {
			DDPPR_ERR("%s cannot find fb fb_id:%u\n", __func__,
				state->prop_val[CRTC_PROP_OUTPUT_FB_ID]);
			return;
		}
		cfg.w = state->prop_val[CRTC_PROP_OUTPUT_WIDTH];
		cfg.h = state->prop_val[CRTC_PROP_OUTPUT_HEIGHT];
		cfg.x = state->prop_val[CRTC_PROP_OUTPUT_X];
		cfg.y = state->prop_val[CRTC_PROP_OUTPUT_Y];
		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
			struct mtk_panel_params *params;

			params = mtk_crtc->panel_ext->params;
			if (params->dyn_fps.switch_en == 1 &&
				params->dyn_fps.vact_timing_fps != 0)
				cfg.vrefresh =
					params->dyn_fps.vact_timing_fps;
			else
				cfg.vrefresh =
					crtc->state->adjusted_mode.vrefresh;
		} else
			cfg.vrefresh = crtc->state->adjusted_mode.vrefresh;
		cfg.bpc = mtk_crtc->bpc;
		cfg.p_golden_setting_context =
				__get_golden_setting_context(mtk_crtc);
		cfg.p_golden_setting_context->dst_width = cfg.w;
		cfg.p_golden_setting_context->dst_height = cfg.h;
	} else {
		/* Output buffer configuration for internal decouple mode */
		ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
		comp->fb = ddp_ctx->dc_fb;
		ddp_ctx->dc_fb_idx =
			(ddp_ctx->dc_fb_idx + 1) % MAX_CRTC_DC_FB;
		ddp_ctx->dc_fb->offsets[0] =
			mtk_crtc_get_dc_fb_size(crtc) * ddp_ctx->dc_fb_idx;
		cfg.w = crtc->state->adjusted_mode.hdisplay;
		cfg.h = crtc->state->adjusted_mode.vdisplay;
		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
			struct mtk_panel_params *params;

			params = mtk_crtc->panel_ext->params;
			if (params->dyn_fps.switch_en == 1 &&
				params->dyn_fps.vact_timing_fps != 0)
				cfg.vrefresh =
					params->dyn_fps.vact_timing_fps;
			else
				cfg.vrefresh =
					crtc->state->adjusted_mode.vrefresh;
		} else
			cfg.vrefresh = crtc->state->adjusted_mode.vrefresh;
		cfg.bpc = mtk_crtc->bpc;
		cfg.p_golden_setting_context =
				__get_golden_setting_context(mtk_crtc);
	}

	mtk_ddp_comp_config(comp, &cfg, cmdq_handle);
}

static void mtk_crtc_wb_backup_to_slot(struct drm_crtc *crtc,
	struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_crtc_ddp_ctx *ddp_ctx = NULL;
	dma_addr_t addr;
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);

	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];

	addr = cmdq_buf->pa_base + DISP_SLOT_RDMA_FB_IDX;
	if (state->prop_val[CRTC_PROP_OUTPUT_ENABLE]) {
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			addr, 0, ~0);
	} else {
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			addr, ddp_ctx->dc_fb_idx, ~0);
	}

	addr = cmdq_buf->pa_base + DISP_SLOT_RDMA_FB_ID;
	if (state->prop_val[CRTC_PROP_OUTPUT_ENABLE]) {
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			addr, state->prop_val[CRTC_PROP_OUTPUT_FB_ID], ~0);
	} else {
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			addr, ddp_ctx->dc_fb->base.id, ~0);
	}

	addr = cmdq_buf->pa_base + DISP_SLOT_CUR_OUTPUT_FENCE;
	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
		addr, state->prop_val[CRTC_PROP_OUTPUT_FENCE_IDX], ~0);

	addr = cmdq_buf->pa_base + DISP_SLOT_CUR_INTERFACE_FENCE;
	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
		addr, state->prop_val[CRTC_PROP_INTF_FENCE_IDX], ~0);
}

int mtk_crtc_gec_flush_check(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_ddp_comp *output_comp = NULL;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp) {
		switch (output_comp->id) {
		case DDP_COMPONENT_WDMA0:
		case DDP_COMPONENT_WDMA1:
			if (!state->prop_val[CRTC_PROP_OUTPUT_ENABLE])
				return -EINVAL;
			break;
		default:
			break;
		}
	}

	return 0;
}

static struct disp_ccorr_config *mtk_crtc_get_color_matrix_data(
						struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	int blob_id;
	struct disp_ccorr_config *ccorr_config = NULL;
	struct drm_property_blob *blob;
	int *color_matrix;

	blob_id = state->prop_val[CRTC_PROP_COLOR_TRANSFORM];

	/* if blod_id == 0 means this time no new color matrix need to set */
	if (!blob_id)
		goto end;

	blob = drm_property_lookup_blob(crtc->dev, blob_id);
	if (!blob) {
		DDPPR_ERR("Cannot get color matrix blob: %d!\n", blob_id);
		goto end;
	}

	ccorr_config = (struct disp_ccorr_config *)blob->data;
	drm_property_blob_put(blob);

	if (ccorr_config) {
		int i = 0, all_zero = 1;

		color_matrix = ccorr_config->color_matrix;
		for (i = 0; i <= 15; i += 5) {
			if (color_matrix[i] != 0) {
				all_zero = 0;
				break;
			}
		}
		if (all_zero) {
			DDPPR_ERR("HWC set zero color matrix!\n");
			goto end;
		}
	} else
		DDPPR_ERR("Blob cannot get ccorr_config data!\n");

end:
	return ccorr_config;
}

static void mtk_crtc_backup_color_matrix_data(struct drm_crtc *crtc,
				struct disp_ccorr_config *ccorr_config,
				struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	dma_addr_t addr;
	int i;

	if (!ccorr_config)
		return;

	addr = cmdq_buf->pa_base + DISP_SLOT_COLOR_MATRIX_PARAMS(0);
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			addr, ccorr_config->mode, ~0);

	for (i = 0; i < 16; i++) {
		addr = cmdq_buf->pa_base +
				DISP_SLOT_COLOR_MATRIX_PARAMS(i + 1);
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
		addr, ccorr_config->color_matrix[i], ~0);
	}
}

static void mtk_crtc_dl_config_color_matrix(struct drm_crtc *crtc,
				struct disp_ccorr_config *ccorr_config,
				struct cmdq_pkt *cmdq_handle)
{

	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	bool set =  false;
	int i, j;
	struct mtk_ddp_comp *comp_ccorr;

	if (!ccorr_config)
		return;

	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
	for (i = 0; i < ddp_ctx->ddp_comp_nr[DDP_FIRST_PATH]; i++) {
		struct mtk_ddp_comp *comp =
		ddp_ctx->ddp_comp[DDP_FIRST_PATH][i];

		if (comp->id == DDP_COMPONENT_CCORR0) {
			disp_ccorr_set_color_matrix(comp, cmdq_handle,
					ccorr_config->color_matrix,
					ccorr_config->mode, ccorr_config->featureFlag);
			set = true;
			break;
		}
	}
	if (mtk_crtc->is_dual_pipe) {
		i = 0;
		j = 0;
		for_each_comp_in_dual_pipe(comp_ccorr, mtk_crtc, i, j) {
			if (comp_ccorr->id == DDP_COMPONENT_CCORR1) {
				disp_ccorr_set_color_matrix(comp_ccorr, cmdq_handle,
						ccorr_config->color_matrix,
						ccorr_config->mode, ccorr_config->featureFlag);
				set = true;
				break;
			}
		}
	}

	if (set)
		mtk_crtc_backup_color_matrix_data(crtc, ccorr_config,
						cmdq_handle);
	else
		DDPPR_ERR("Cannot not find DDP_COMPONENT_CCORR0\n");
}

int mtk_crtc_gce_flush(struct drm_crtc *crtc, void *gce_cb,
	void *cb_data, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct disp_ccorr_config *ccorr_config = NULL;

	if (mtk_crtc_gec_flush_check(crtc) < 0)	{
		if (cb_data) {
			struct drm_crtc_state *crtc_state;
			struct drm_atomic_state *atomic_state;

			crtc_state = ((struct mtk_cmdq_cb_data *)cb_data)->state;
			atomic_state = crtc_state->state;
			drm_atomic_state_put(atomic_state);
		}
		cmdq_pkt_destroy(cmdq_handle);
		kfree(cb_data);
		DDPPR_ERR("flush check failed\n");
		return -1;
	}

	/* apply color matrix if crtc0 is DL */
	ccorr_config = mtk_crtc_get_color_matrix_data(crtc);
	if (drm_crtc_index(crtc) == 0 && (!mtk_crtc_is_dc_mode(crtc)))
		mtk_crtc_dl_config_color_matrix(crtc, ccorr_config,
						cmdq_handle);

	if (mtk_crtc_is_dc_mode(crtc) ||
		state->prop_val[CRTC_PROP_OUTPUT_ENABLE]) {
		int gce_event =
			get_path_wait_event(mtk_crtc, mtk_crtc->ddp_mode);

		mtk_crtc_wb_comp_config(crtc, cmdq_handle);

		if (mtk_crtc_is_dc_mode(crtc))
			/* Decouple and Decouple mirror mode */
			mtk_disp_mutex_enable_cmdq(mtk_crtc->mutex[1],
					cmdq_handle, mtk_crtc->gce_obj.base);
		else {
			/* For virtual display write-back path */
			cmdq_pkt_clear_event(cmdq_handle, gce_event);
			mtk_disp_mutex_enable_cmdq(mtk_crtc->mutex[0],
					cmdq_handle, mtk_crtc->gce_obj.base);
		}

		cmdq_pkt_wait_no_clear(cmdq_handle, gce_event);
	} else if (mtk_crtc_is_frame_trigger_mode(crtc) &&
					mtk_crtc_with_trigger_loop(crtc)) {
		/* DL with trigger loop */
		cmdq_pkt_set_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);

	} else {
		/* DL without trigger loop */
		mtk_disp_mutex_enable_cmdq(mtk_crtc->mutex[0],
			cmdq_handle, mtk_crtc->gce_obj.base);
	}

	if (mtk_crtc_is_dc_mode(crtc) ||
		state->prop_val[CRTC_PROP_OUTPUT_ENABLE]) {
		mtk_crtc_wb_backup_to_slot(crtc, cmdq_handle);

		/* backup color matrix for DC and DC Mirror for RDMA update*/
		if (mtk_crtc_is_dc_mode(crtc))
			mtk_crtc_backup_color_matrix_data(crtc, ccorr_config,
							cmdq_handle);
	}

#ifdef MTK_DRM_CMDQ_ASYNC
#ifdef MTK_DRM_FB_LEAK
	if (cmdq_pkt_flush_async(cmdq_handle,
		gce_cb, cb_data) < 0)
#else
	if (cmdq_pkt_flush_threaded(cmdq_handle,
		gce_cb, cb_data) < 0)
#endif
		DDPPR_ERR("failed to flush gce_cb\n");
#else
	cmdq_pkt_flush(cmdq_handle);
#endif

	return 0;
}

static void mtk_drm_crtc_enable_fake_layer(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_plane *plane;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_pending_state *pending;
	struct mtk_ddp_comp *comp;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_fake_layer *fake_layer = &mtk_crtc->fake_layer;
	int i, idx, layer_num;

	if (drm_crtc_index(crtc) != 0)
		return;

	DDPINFO("%s\n", __func__);

	for (i = 0 ; i < PRIMARY_OVL_PHY_LAYER_NR ; i++) {
		plane = &mtk_crtc->planes[i].base;
		plane_state = to_mtk_plane_state(plane->state);
		pending = &plane_state->pending;

		pending->addr = mtk_fb_get_dma(fake_layer->fake_layer_buf[i]);
		pending->size = mtk_fb_get_size(fake_layer->fake_layer_buf[i]);
		pending->pitch = fake_layer->fake_layer_buf[i]->pitches[0];
		pending->format = fake_layer->fake_layer_buf[i]->format->format;
		pending->modifier = fake_layer->fake_layer_buf[i]->modifier;
		pending->src_x = 0;
		pending->src_y = 0;
		pending->dst_x = 0;
		pending->dst_y = 0;
		pending->height = fake_layer->fake_layer_buf[i]->height;
		pending->width = fake_layer->fake_layer_buf[i]->width;
		pending->config = 1;
		pending->dirty = 1;

		if (mtk_crtc->fake_layer.fake_layer_mask & BIT(i))
			pending->enable = true;
		else
			pending->enable = false;

		pending->prop_val[PLANE_PROP_ALPHA_CON] = 0x1;
		pending->prop_val[PLANE_PROP_PLANE_ALPHA] = 0xFF;
		pending->prop_val[PLANE_PROP_COMPRESS] = 0;

		layer_num = mtk_ovl_layer_num(
				priv->ddp_comp[DDP_COMPONENT_OVL0_2L]);
		if (layer_num < 0) {
			DDPPR_ERR("invalid layer num:%d\n", layer_num);
			continue;
		}
		if (i < layer_num) {
			comp = priv->ddp_comp[DDP_COMPONENT_OVL0_2L];
			idx = i;
		} else {
			comp = priv->ddp_comp[DDP_COMPONENT_OVL0];
			idx = i - layer_num;
		}
		plane_state->comp_state.comp_id = comp->id;
		plane_state->comp_state.lye_id = idx;
		plane_state->comp_state.ext_lye_id = 0;

		mtk_ddp_comp_layer_config(comp, plane_state->comp_state.lye_id,
					plane_state, state->cmdq_handle);
	}

	for (i = 0 ; i < PRIMARY_OVL_EXT_LAYER_NR ; i++) {
		plane = &mtk_crtc->planes[i + PRIMARY_OVL_PHY_LAYER_NR].base;
		plane_state = to_mtk_plane_state(plane->state);
		pending = &plane_state->pending;

		pending->dirty = 1;
		pending->enable = false;

		if (i < (PRIMARY_OVL_EXT_LAYER_NR / 2)) {
			comp = priv->ddp_comp[DDP_COMPONENT_OVL0_2L];
			idx = i + 1;
		} else {
			comp = priv->ddp_comp[DDP_COMPONENT_OVL0];
			idx = i + 1 - (PRIMARY_OVL_EXT_LAYER_NR / 2);
		}
		plane_state->comp_state.comp_id = comp->id;
		plane_state->comp_state.lye_id = 0;
		plane_state->comp_state.ext_lye_id = idx;

		mtk_ddp_comp_layer_config(comp, plane_state->comp_state.lye_id,
					plane_state, state->cmdq_handle);
	}
}

static void mtk_drm_crtc_disable_fake_layer(struct drm_crtc *crtc,
				struct drm_crtc_state *old_crtc_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_plane *plane;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_pending_state *pending;
	struct mtk_ddp_comp *comp;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	int i, idx, layer_num;

	if (drm_crtc_index(crtc) != 0)
		return;

	DDPINFO("%s\n", __func__);

	for (i = 0 ; i < PRIMARY_OVL_PHY_LAYER_NR ; i++) {
		plane = &mtk_crtc->planes[i].base;
		plane_state = to_mtk_plane_state(plane->state);
		pending = &plane_state->pending;

		pending->dirty = 1;
		pending->enable = false;

		layer_num = mtk_ovl_layer_num(
				priv->ddp_comp[DDP_COMPONENT_OVL0_2L]);
		if (layer_num < 0) {
			DDPPR_ERR("invalid layer num:%d\n", layer_num);
			continue;
		}
		if (i < layer_num) {
			comp = priv->ddp_comp[DDP_COMPONENT_OVL0_2L];
			idx = i;
		} else {
			comp = priv->ddp_comp[DDP_COMPONENT_OVL0];
			idx = i - layer_num;
		}
		plane_state->comp_state.comp_id = comp->id;
		plane_state->comp_state.lye_id = idx;
		plane_state->comp_state.ext_lye_id = 0;

		mtk_ddp_comp_layer_config(comp, plane_state->comp_state.lye_id,
					plane_state, state->cmdq_handle);
	}
}

static void mtk_atomic_hbm_bypass_pq(struct drm_crtc *crtc,
		struct cmdq_pkt *handle, int en)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	int i, j;

	DDPINFO("%s: enter\n", __func__);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (comp && (comp->id == DDP_COMPONENT_AAL0 ||
			comp->id == DDP_COMPONENT_CCORR0)) {
			if (comp->funcs && comp->funcs->bypass)
				mtk_ddp_comp_bypass(comp, en, handle);
		}
	}

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			if (comp && (comp->id == DDP_COMPONENT_AAL1 ||
				comp->id == DDP_COMPONENT_CCORR1)) {
				if (comp->funcs && comp->funcs->bypass)
					mtk_ddp_comp_bypass(comp, en, handle);
			}
		}
	}
}

static void sf_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

#if defined(CONFIG_MTK_HDR_COLOR_GAIN_RGB) && (CONFIG_MTK_HDR_COLOR_GAIN_RGB > 0)
static int mtk_hdr_color_gain_setted;
#endif

static void mtk_drm_crtc_atomic_flush(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int index = drm_crtc_index(crtc);
	unsigned int pending_planes = 0;
	unsigned int i, j;
	unsigned int ret = 0;
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc_state);
	struct cmdq_pkt *cmdq_handle = state->cmdq_handle;
	struct mtk_cmdq_cb_data *cb_data;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_crtc *mtk_crtc0 = to_mtk_crtc(priv->crtc[0]);

	CRTC_MMP_EVENT_START(index, atomic_flush, (unsigned long)crtc_state,
			(unsigned long)old_crtc_state);
	if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		CRTC_MMP_MARK(index, atomic_flush, 0, 0);
		goto end;
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("cb data creation failed\n");
		CRTC_MMP_MARK(index, atomic_flush, 0, 1);
		goto end;
	}

	if (mtk_crtc->event)
		mtk_crtc->pending_needs_vblank = true;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		if (plane_state->pending.dirty) {
			plane_state->pending.config = true;
			plane_state->pending.dirty = false;
			pending_planes |= BIT(i);
		}
	}

	if (pending_planes)
		mtk_crtc->pending_planes = true;

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_HBM)) {
		bool hbm_en = false;

		hbm_en = (bool)state->prop_val[CRTC_PROP_HBM_ENABLE];
		mtk_drm_crtc_set_panel_hbm(crtc, hbm_en);
		mtk_drm_crtc_hbm_wait(crtc, hbm_en);

		if (!state->prop_val[CRTC_PROP_DOZE_ACTIVE])
			mtk_atomic_hbm_bypass_pq(crtc, cmdq_handle, hbm_en);
	}

	hdr_en = (bool)state->prop_val[CRTC_PROP_HDR_ENABLE];

#if defined(CONFIG_MTK_HDR_COLOR_GAIN_RGB) && (CONFIG_MTK_HDR_COLOR_GAIN_RGB > 0)
	DDPINFO("crtc CONFIG_HDR_COLOR_GAIN_RED =%ld!\n", CONFIG_MTK_HDR_COLOR_GAIN_RGB);
	if (hdr_en && !mtk_hdr_color_gain_setted &&
	    state->prop_val[CRTC_PROP_HDR_ENABLE] & MTK_HDR10P_PROPERTY_FLAG) {
		struct mtk_crtc_ddp_ctx *ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];

		for (i = 0; i < ddp_ctx->ddp_comp_nr[DDP_FIRST_PATH]; i++) {
			struct mtk_ddp_comp *comp = ddp_ctx->ddp_comp[DDP_FIRST_PATH][i];

			if (comp->id == DDP_COMPONENT_CCORR0) {
				disp_ccorr_set_RGB_Gain(comp, cmdq_handle,
					CONFIG_MTK_HDR_COLOR_GAIN_RGB >> 24 & 0xFFF,
					CONFIG_MTK_HDR_COLOR_GAIN_RGB >> 12 & 0xFFF,
					CONFIG_MTK_HDR_COLOR_GAIN_RGB & 0xFFF);
					mtk_hdr_color_gain_setted = 1;
				break;
			}
		}
	} else if (mtk_hdr_color_gain_setted) {
		struct mtk_crtc_ddp_ctx *ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];

		for (i = 0; i < ddp_ctx->ddp_comp_nr[DDP_FIRST_PATH]; i++) {
			struct mtk_ddp_comp *comp = ddp_ctx->ddp_comp[DDP_FIRST_PATH][i];

			if (comp->id == DDP_COMPONENT_CCORR0) {
				disp_ccorr_set_RGB_Gain(comp, cmdq_handle, 1024, 1024, 1024);
				mtk_hdr_color_gain_setted = 0;
				break;
			}
		}
	}
#endif

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (crtc->state->color_mgmt_changed)
			mtk_ddp_gamma_set(comp, crtc->state, cmdq_handle);
		mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				PMQOS_UPDATE_BW, NULL);
		mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				FRAME_DIRTY, NULL);
	}
	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			if (crtc->state->color_mgmt_changed)
				mtk_ddp_gamma_set(comp, crtc->state, cmdq_handle);
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
					PMQOS_UPDATE_BW, NULL);
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
					FRAME_DIRTY, NULL);

		}
	}

	if ((priv->data->shadow_register) == true) {
		mtk_disp_mutex_acquire(mtk_crtc->mutex[0]);
		mtk_crtc_ddp_config(crtc);
		mtk_disp_mutex_release(mtk_crtc->mutex[0]);
	}

	if (mtk_crtc->fake_layer.fake_layer_mask)
		mtk_drm_crtc_enable_fake_layer(crtc, old_crtc_state);
	else if (mtk_crtc->fake_layer.first_dis) {
		mtk_drm_crtc_disable_fake_layer(crtc, old_crtc_state);
		mtk_crtc->fake_layer.first_dis = false;
	}

	/* backup ovl0 2l status for crtc0 */
	if (index == 0) {
		comp = mtk_ddp_comp_find_by_id(crtc, DDP_COMPONENT_OVL0_2L);
		if (comp != NULL)
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				BACKUP_OVL_STATUS, NULL);
	}

	/* backup present fence */
	if (state->prop_val[CRTC_PROP_PRES_FENCE_IDX] != (unsigned int)-1) {
		struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
		dma_addr_t addr =
			cmdq_buf->pa_base +
			DISP_SLOT_PRESENT_FENCE(index);

		cmdq_pkt_write(cmdq_handle,
			mtk_crtc->gce_obj.base, addr,
			state->prop_val[CRTC_PROP_PRES_FENCE_IDX], ~0);
		CRTC_MMP_MARK(index, update_present_fence, 0,
			state->prop_val[CRTC_PROP_PRES_FENCE_IDX]);
	}

	/* for wfd latency debug */
	if (index == 0 || index == 2) {
		struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
		dma_addr_t addr =
			cmdq_buf->pa_base +
			DISP_SLOT_OVL_DSI_SEQ(index);

		cmdq_pkt_write(cmdq_handle,
			mtk_crtc->gce_obj.base, addr,
			state->prop_val[CRTC_PROP_OVL_DSI_SEQ], ~0);

		if (state->prop_val[CRTC_PROP_OVL_DSI_SEQ]) {
			if (index == 0)
				mtk_drm_trace_async_begin("OVL0-DSI|%d",
					state->prop_val[CRTC_PROP_OVL_DSI_SEQ]);
			else if (index == 2)
				mtk_drm_trace_async_begin("OVL2-WDMA|%d",
					state->prop_val[CRTC_PROP_OVL_DSI_SEQ]);
		}
	}

	atomic_set(&mtk_crtc->delayed_trig, 1);
	cb_data->state = old_crtc_state;
	cb_data->cmdq_handle = cmdq_handle;
	cb_data->misc = mtk_crtc->ddp_mode;

	/* This refcnt would be release in ddp_cmdq_cb */
	drm_atomic_state_get(old_crtc_state->state);
	mtk_drm_crtc_lfr_update(crtc, cmdq_handle);


	/* backup ovl0 2l status for crtc0
	 * do not insert code between back up ovl status and gce flush
	 */
	if (index == 0) {
		comp = mtk_ddp_comp_find_by_id(crtc, DDP_COMPONENT_OVL0_2L);
		if (comp != NULL)
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				BACKUP_OVL_STATUS, NULL);
	}


	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_SF_PF) &&
	   (state->prop_val[CRTC_PROP_SF_PRES_FENCE_IDX] != (unsigned int)-1)) {
		if (index == 0)
			cmdq_pkt_clear_event(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_DSI0_SOF]);
	}

#ifdef MTK_DRM_CMDQ_ASYNC
	ret = mtk_crtc_gce_flush(crtc, ddp_cmdq_cb, cb_data, cmdq_handle);
	if (ret) {
		DDPPR_ERR("mtk_crtc_gce_flush failed!\n");
		goto end;
	}
	CRTC_MMP_MARK(index, atomic_flush, (unsigned long)cmdq_handle,
			(unsigned long)cmdq_handle->cmd_buf_size);
#else
	ret = mtk_crtc_gce_flush(crtc, NULL, NULL, cmdq_handle);
	if (ret) {
		DDPPR_ERR("mtk_crtc_gce_flush failed!\n");
		goto end;
	}
	ddp_cmdq_cb_blocking(cb_data);
#endif


	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_SF_PF) &&
	   (state->prop_val[CRTC_PROP_SF_PRES_FENCE_IDX] != (unsigned int)-1)) {
		struct cmdq_pkt *cmdq_handle;
		struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
		struct mtk_cmdq_cb_data *sf_cb_data;
		dma_addr_t addr;

		mtk_crtc_pkt_create(&cmdq_handle, crtc,
				mtk_crtc->gce_obj.client[CLIENT_CFG]);

		sf_cb_data = kmalloc(sizeof(*sf_cb_data), GFP_KERNEL);
		if (!sf_cb_data) {
			DDPPR_ERR("cb data creation failed\n");
			CRTC_MMP_MARK(index, atomic_flush, 1, 1);
			goto end;
		}

		if (index == 0)
			cmdq_pkt_wait_no_clear(cmdq_handle,
				mtk_crtc->gce_obj.event[EVENT_DSI0_SOF]);

		addr = cmdq_buf->pa_base + DISP_SLOT_SF_PRESENT_FENCE(index);

		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base, addr,
			       state->prop_val[CRTC_PROP_SF_PRES_FENCE_IDX],
			       ~0);
		CRTC_MMP_MARK(index, update_sf_present_fence, 0,
			      state->prop_val[CRTC_PROP_SF_PRES_FENCE_IDX]);

		sf_cb_data->cmdq_handle = cmdq_handle;

		if (cmdq_pkt_flush_threaded(cmdq_handle,
				sf_cmdq_cb, sf_cb_data) < 0)
			DDPPR_ERR("failed to flush sf_cmdq_cb\n");
	}

end:
	/* When open VDS path switch feature, After VDS created
	 * we need take away the OVL0_2L from main display.
	 */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH) &&
		priv->need_vds_path_switch &&
		priv->vds_path_switch_dirty &&
		(!priv->need_vds_path_switch_back) &&
		!priv->vds_path_switch_done) {
		if ((index == 0) && atomic_read(&mtk_crtc0->already_config)) {
			DDPMSG("Switch vds: mtk_crtc0 enable:%d\n",
				atomic_read(&mtk_crtc0->already_config));
			mtk_need_vds_path_switch(crtc);
		}

		if ((index == 2) && (!atomic_read(&mtk_crtc0->already_config))) {
			DDPMSG("Switch vds: mtk_crtc0 enable:%d\n",
				atomic_read(&mtk_crtc0->already_config));
			mtk_need_vds_path_switch(priv->crtc[0]);
		}
	}

	CRTC_MMP_EVENT_END(index, atomic_flush, (unsigned long)crtc_state,
			(unsigned long)old_crtc_state);
	mtk_drm_trace_end();
}

static const struct drm_crtc_funcs mtk_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = mtk_drm_crtc_destroy,
	.reset = mtk_drm_crtc_reset,
	.atomic_duplicate_state = mtk_drm_crtc_duplicate_state,
	.atomic_destroy_state = mtk_drm_crtc_destroy_state,
	.atomic_set_property = mtk_drm_crtc_set_property,
	.atomic_get_property = mtk_drm_crtc_get_property,
	.gamma_set = drm_atomic_helper_legacy_gamma_set,
};

static const struct drm_crtc_helper_funcs mtk_crtc_helper_funcs = {
	.mode_fixup = mtk_drm_crtc_mode_fixup,
	.mode_set_nofb = mtk_drm_crtc_mode_set_nofb,
	.atomic_enable = mtk_drm_crtc_atomic_resume,
	.disable = mtk_drm_crtc_suspend,
	.atomic_begin = mtk_drm_crtc_atomic_begin,
	.atomic_flush = mtk_drm_crtc_atomic_flush,
};

static void mtk_drm_crtc_attach_property(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_property *prop;
	static struct drm_property *mtk_crtc_prop[CRTC_PROP_MAX];
	struct mtk_drm_property *crtc_prop;
	int index = drm_crtc_index(crtc);
	int i;
	static int num;

	DDPINFO("%s:%d crtc:%d\n", __func__, __LINE__, index);

	if (num == 0) {
		for (i = 0; i < CRTC_PROP_MAX; i++) {
			crtc_prop = &(mtk_crtc_property[i]);
			mtk_crtc_prop[i] = drm_property_create_range(
				dev, crtc_prop->flags, crtc_prop->name,
				crtc_prop->min, crtc_prop->max);
			if (!mtk_crtc_prop[i]) {
				DDPPR_ERR("fail to create property:%s\n",
					  crtc_prop->name);
				return;
			}
			DDPINFO("create property:%s, flags:0x%x\n",
				crtc_prop->name, mtk_crtc_prop[i]->flags);
		}
		num++;
	}

	for (i = 0; i < CRTC_PROP_MAX; i++) {
		prop = private->crtc_property[index][i];
		crtc_prop = &(mtk_crtc_property[i]);
		DDPINFO("%s:%d prop:%p\n", __func__, __LINE__, prop);
		if (!prop) {
			prop = mtk_crtc_prop[i];
		      private
			->crtc_property[index][i] = prop;
			drm_object_attach_property(&crtc->base, prop,
						   crtc_prop->val);
		}
	}
}

static int mtk_drm_crtc_init(struct drm_device *drm,
			     struct mtk_drm_crtc *mtk_crtc,
			     struct drm_plane *primary,
			     struct drm_plane *cursor, unsigned int pipe)
{
	int ret;

	DDPINFO("%s+\n", __func__);
	ret = drm_crtc_init_with_planes(drm, &mtk_crtc->base, primary, cursor,
					&mtk_crtc_funcs, NULL);
	if (ret)
		goto err_cleanup_crtc;

	drm_crtc_helper_add(&mtk_crtc->base, &mtk_crtc_helper_funcs);

	mtk_drm_crtc_attach_property(&mtk_crtc->base);
	DDPINFO("%s-\n", __func__);

	return 0;

err_cleanup_crtc:
	drm_crtc_cleanup(&mtk_crtc->base);
	return ret;
}

void mtk_crtc_ddp_irq(struct drm_crtc *crtc, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (priv->data->shadow_register == false)
		mtk_crtc_ddp_config(crtc);

	mtk_drm_finish_page_flip(mtk_crtc);
}

void mtk_crtc_vblank_irq(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
/*	char tag_name[100] = {'\0'}; */
	ktime_t ktime = ktime_get();

	mtk_crtc->vblank_time = ktime_to_timeval(ktime);

/*
 *	sprintf(tag_name, "%d|HW_VSYNC|%lld",
 *		hwc_pid, ktime);
 *	mtk_drm_trace_c("%s", tag_name);
 */
	mtk_drm_trace_c("%d|DISP-HW_Vsync|%lld",
		hwc_pid, ktime);

/*
 *	DDPMSG("%s CRTC%d %s\n", __func__,
 *			drm_crtc_index(crtc), tag_name);
 */
	drm_crtc_handle_vblank(&mtk_crtc->base);

/*
 *	sprintf(tag_name, "%d|HW_Vsync|%d",
 *		hwc_pid, 0);
 *	mtk_drm_trace_c("%s", tag_name);
 */
	mtk_drm_trace_c("%d|DISP-HW_Vsync|%lld",
		hwc_pid, 0);
}

static void mtk_crtc_get_output_comp_name(struct mtk_drm_crtc *mtk_crtc,
					  char *buf, int buf_len)
{
	int i, j;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_crtc_path_reverse(comp, mtk_crtc, i,
					   j)
		if (mtk_ddp_comp_is_output(comp)) {
			mtk_ddp_comp_get_name(comp, buf, buf_len);
			return;
		}

	DDPPR_ERR("%s(), no output comp found for crtc%d, set buf to 0\n",
		  __func__, drm_crtc_index(&mtk_crtc->base));
	memset(buf, 0, buf_len);
}
static void mtk_crtc_get_event_name(struct mtk_drm_crtc *mtk_crtc, char *buf,
				    int buf_len, int event_id)
{
	int crtc_id, len;
	char output_comp[20];

	/* TODO: remove hardcode comp event */
	crtc_id = drm_crtc_index(&mtk_crtc->base);
	switch (event_id) {
	case EVENT_STREAM_DIRTY:
		len = snprintf(buf, buf_len, "disp_token_stream_dirty%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
	case EVENT_SYNC_TOKEN_SODI:
		len = snprintf(buf, buf_len, "disp_token_sodi%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
#endif
	case EVENT_STREAM_EOF:
		len = snprintf(buf, buf_len, "disp_token_stream_eof%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_VDO_EOF:
		len = snprintf(buf, buf_len, "disp_mutex%d_eof",
			       drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_CMD_EOF:
		mtk_crtc_get_output_comp_name(mtk_crtc, output_comp,
					      sizeof(output_comp));
		len = snprintf(buf, buf_len, "disp_%s_eof", output_comp);
		break;
	case EVENT_TE:
		mtk_crtc_get_output_comp_name(mtk_crtc, output_comp,
					      sizeof(output_comp));
		len = snprintf(buf, buf_len, "disp_wait_%s_te", output_comp);
		break;
	case EVENT_ESD_EOF:
		len = snprintf(buf, buf_len, "disp_token_esd_eof%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_RDMA0_EOF:
		len = snprintf(buf, buf_len, "disp_rdma0_eof%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_WDMA0_EOF:
		len = snprintf(buf, buf_len, "disp_wdma0_eof%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_WDMA1_EOF:
		len = snprintf(buf, buf_len, "disp_wdma1_eof%d",
				   drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_STREAM_BLOCK:
		len = snprintf(buf, buf_len, "disp_token_stream_block%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_CABC_EOF:
		len = snprintf(buf, buf_len, "disp_token_cabc_eof%d",
					drm_crtc_index(&mtk_crtc->base));
		break;
	case EVENT_DSI0_SOF:
		len = snprintf(buf, buf_len, "disp_dsi0_sof%d",
			       drm_crtc_index(&mtk_crtc->base));
		break;
	default:
		DDPPR_ERR("%s invalid event_id:%d\n", __func__, event_id);
		memset(output_comp, 0, sizeof(output_comp));
	}
}

static void mtk_crtc_init_color_matrix_data_slot(
					struct mtk_drm_crtc *mtk_crtc)
{
	struct cmdq_pkt *cmdq_handle;
	struct disp_ccorr_config ccorr_config = {.mode = 1,
						 .color_matrix = {
						 1024, 0, 0, 0,
						 0, 1024, 0, 0,
						 0, 0, 1024, 0,
						 0, 0, 0, 1024},
						 .featureFlag = false };

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
	mtk_crtc_backup_color_matrix_data(&mtk_crtc->base, &ccorr_config,
					cmdq_handle);
	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
}

static void mtk_crtc_init_gce_obj(struct drm_device *drm_dev,
				  struct mtk_drm_crtc *mtk_crtc)
{
	struct device *dev = drm_dev->dev;
	struct mtk_drm_private *priv = drm_dev->dev_private;
	struct cmdq_pkt_buffer *cmdq_buf;
	char buf[50];
	int len, index, i;

	/* Load CRTC GCE client */
	for (i = 0; i < CLIENT_TYPE_MAX; i++) {
		DRM_INFO("%s(), %s, %d", __func__,
			 crtc_gce_client_str[i],
			 drm_crtc_index(&mtk_crtc->base));
		len = snprintf(buf, sizeof(buf), "%s%d", crtc_gce_client_str[i],
			       drm_crtc_index(&mtk_crtc->base));
		if (len < 0) {
			/* Handle snprintf() error */
			DDPPR_ERR("%s:snprintf error\n", __func__);
			return;
		}
		index = of_property_match_string(dev->of_node,
						 "gce-client-names", buf);
		if (index < 0) {
			mtk_crtc->gce_obj.client[i] = NULL;
			continue;
		}
		mtk_crtc->gce_obj.client[i] =
			cmdq_mbox_create(dev, index);
		if (i != CLIENT_SEC_CFG)
			continue;

		if (drm_crtc_index(&mtk_crtc->base) == 0)
			continue;

		/* crtc1 & crtc2 share same secure gce thread */
		if (priv->ext_sec_client == NULL)
			priv->ext_sec_client = mtk_crtc->gce_obj.client[i];
		else
			mtk_crtc->gce_obj.client[i] = priv->ext_sec_client;
	}

	/* Load CRTC GCE event */
	for (i = 0; i < EVENT_TYPE_MAX; i++) {
		mtk_crtc_get_event_name(mtk_crtc, buf, sizeof(buf), i);
		mtk_crtc->gce_obj.event[i] = cmdq_dev_get_event(dev, buf);
	}

	cmdq_buf = &(mtk_crtc->gce_obj.buf);
	if (mtk_crtc->gce_obj.client[CLIENT_CFG]) {
		DDPINFO("[CRTC][CHECK-1]0x%p\n",
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
		if (mtk_crtc->gce_obj.client[CLIENT_CFG]->chan) {
			DDPINFO("[CRTC][CHECK-2]0x%p\n",
				mtk_crtc->gce_obj.client[CLIENT_CFG]->chan);
			if (mtk_crtc->gce_obj.client[CLIENT_CFG]->chan->mbox) {
				DDPINFO("[CRTC][CHECK-3]0x%p\n",
					mtk_crtc->gce_obj.client[CLIENT_CFG]
						->chan->mbox);
				if (mtk_crtc->gce_obj.client[CLIENT_CFG]
					    ->chan->mbox->dev) {
					DDPINFO("[CRTC][CHECK-4]0x%p\n",
						mtk_crtc->gce_obj
							.client[CLIENT_CFG]
							->chan->mbox->dev);
				}
			}
		}
	}
	cmdq_buf->va_base = cmdq_mbox_buf_alloc(
		mtk_crtc->gce_obj.client[CLIENT_CFG]->chan->mbox->dev,
		&(cmdq_buf->pa_base));

	if (!cmdq_buf->va_base) {
		DDPPR_ERR("va base is NULL\n");
		return;
	}

	memset(cmdq_buf->va_base, 0, DISP_SLOT_SIZE);

	mtk_crtc_init_color_matrix_data_slot(mtk_crtc);

	mtk_crtc->gce_obj.base = cmdq_register_device(dev);
}

static int mtk_drm_cwb_copy_buf(struct drm_crtc *crtc,
			  struct mtk_cwb_info *cwb_info,
			  void *buffer, unsigned int buf_idx)
{
	unsigned long addr_va = cwb_info->buffer[buf_idx].addr_va;
	enum CWB_BUFFER_TYPE type = cwb_info->type;
	int width, height, pitch, size;
	unsigned long long time = sched_clock();

	//double confirm user_buffer still exists
	if (!cwb_info->funcs || !cwb_info->funcs->get_buffer) {
		if (!cwb_info->user_buffer)
			return -1;
		buffer = cwb_info->user_buffer;
		cwb_info->user_buffer = 0;
	}

	width = cwb_info->copy_w;
	height = cwb_info->copy_h;
	pitch = width * 3;
	size = pitch * height;

	if (type == IMAGE_ONLY) {
		u8 *tmp = (u8 *)buffer;

		memcpy(tmp, (void *)addr_va, size);
	} else if (type == CARRY_METADATA) {
		struct user_cwb_buffer *tmp = (struct user_cwb_buffer *)buffer;

		tmp->data.width = width;
		tmp->data.height = height;
		tmp->meta.frameIndex = cwb_info->count;
		tmp->meta.timestamp = cwb_info->buffer[buf_idx].timestamp;
		memcpy(tmp->data.image, (void *)addr_va, size);
	}
	DDPMSG("[capture] copy buf from 0x%x, (w,h)=(%d,%d), ts:%llu done\n",
			addr_va, width, height, time);

	return 0;
}

static void mtk_drm_cwb_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

static void mtk_drm_cwb_give_buf(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_cwb_info *cwb_info;
	struct mtk_ddp_comp *comp;
	unsigned int target_idx, next_idx, addr = 0;
	void *user_buffer;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct mtk_cmdq_cb_data *cb_data;
	const struct mtk_cwb_funcs *funcs;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return;
	}
	cwb_info = mtk_crtc->cwb_info;
	user_buffer = 0;
	funcs = cwb_info->funcs;
	if (funcs && funcs->get_buffer)
		funcs->get_buffer(&user_buffer);
	else
		user_buffer = cwb_info->user_buffer;

	if (!user_buffer) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return;
	}

	target_idx = cwb_info->buf_idx;
	next_idx = 1 - cwb_info->buf_idx;

	//1. config next addr to wdma addr
	addr = cwb_info->buffer[next_idx].addr_mva;
	comp = cwb_info->comp;
	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("%s:%d, cb data creation failed\n",
			__func__, __LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return;
	}
	mtk_crtc_pkt_create(&handle, crtc, client);
	mtk_crtc_wait_frame_done(mtk_crtc, handle, DDP_FIRST_PATH, 0);
	mtk_ddp_comp_io_cmd(comp, handle, WDMA_WRITE_DST_ADDR0, &addr);
	if (mtk_crtc->is_dual_pipe) {
		comp = priv->ddp_comp[dual_pipe_comp_mapping(comp->id)];
		mtk_ddp_comp_io_cmd(comp, handle, WDMA_WRITE_DST_ADDR0, &addr);
	}
	cb_data->cmdq_handle = handle;
	if (cmdq_pkt_flush_threaded(handle, mtk_drm_cwb_cb, cb_data) < 0)
		DDPPR_ERR("failed to flush write_back\n");

	//2. idx change
	cwb_info->buf_idx = next_idx;
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	//3. copy target_buf_idx context
	DDP_MUTEX_LOCK(&mtk_crtc->cwb_lock, __func__, __LINE__);
	mtk_drm_cwb_copy_buf(crtc, cwb_info, user_buffer, target_idx);
	mtk_dprec_mmp_dump_cwb_buffer(crtc, user_buffer, target_idx);
	DDP_MUTEX_UNLOCK(&mtk_crtc->cwb_lock, __func__, __LINE__);

	//4. notify user
	if (funcs && funcs->copy_done)
		funcs->copy_done(user_buffer, cwb_info->type);
	else
		DDPPR_ERR("User has no notify callback funciton/n");
}

static int mtk_drm_cwb_monitor_thread(void *data)
{
	int ret = 0;
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_cwb_info *cwb_info;

	msleep(16000);
	do {
		ret = wait_event_interruptible(
			mtk_crtc->cwb_wq,
			atomic_read(&mtk_crtc->cwb_task_active));
		if (ret)
			DDPMSG("%s wait_event_interruptible failed, ret = %d\n", __func__, ret);
		atomic_set(&mtk_crtc->cwb_task_active, 0);

		cwb_info = mtk_crtc->cwb_info;

		if (!cwb_info || !cwb_info->enable)
			continue;

		mtk_drm_cwb_give_buf(crtc);

	} while (!kthread_should_stop());

	return 0;
}

static int mtk_drm_cwb_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	const int len = 50;
	char name[len];

	if (snprintf(name, len, "mtk_drm_cwb")) {
		mtk_crtc->cwb_task =
			kthread_create(mtk_drm_cwb_monitor_thread, crtc, name);
	}
	init_waitqueue_head(&mtk_crtc->cwb_wq);
	atomic_set(&mtk_crtc->cwb_task_active, 0);

	wake_up_process(mtk_crtc->cwb_task);

	return 0;
}

void mtk_drm_fake_vsync_switch(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_fake_vsync *fake_vsync = mtk_crtc->fake_vsync;

	if (drm_crtc_index(crtc) != 0 || mtk_drm_lcm_is_connect() ||
		!mtk_crtc_is_frame_trigger_mode(crtc))
		return;

	if (unlikely(!fake_vsync)) {
		DDPPR_ERR("%s:invalid fake_vsync pointer\n", __func__);
		return;
	}

	atomic_set(&fake_vsync->fvsync_active, enable);
	if (enable)
		wake_up_interruptible(&fake_vsync->fvsync_wq);
}

static int mtk_drm_fake_vsync_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_fake_vsync *fake_vsync = mtk_crtc->fake_vsync;
	int ret = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		ret = wait_event_interruptible(fake_vsync->fvsync_wq,
				atomic_read(&fake_vsync->fvsync_active));

		if (ret) {
			DDPPR_ERR("%s wait_event_interruptible failed, ret = %d\n", __func__, ret);
			return ret;
		}
		mtk_crtc_vblank_irq(crtc);
		usleep_range(16700, 17700);

	} while (!kthread_should_stop());
	return 0;
}

void mtk_drm_fake_vsync_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_fake_vsync *fake_vsync =
		kzalloc(sizeof(struct mtk_drm_fake_vsync), GFP_KERNEL);
	const int len = 50;
	char name[len];

	if (drm_crtc_index(crtc) != 0 || mtk_drm_lcm_is_connect() ||
		!mtk_crtc_is_frame_trigger_mode(crtc)) {
		kfree(fake_vsync);
		return;
	}

	snprintf(name, len, "mtk_drm_fake_vsync:%d", drm_crtc_index(crtc));
	fake_vsync->fvsync_task = kthread_create(mtk_drm_fake_vsync_kthread,
					crtc, name);
	init_waitqueue_head(&fake_vsync->fvsync_wq);
	atomic_set(&fake_vsync->fvsync_active, 1);
	mtk_crtc->fake_vsync = fake_vsync;

	wake_up_process(fake_vsync->fvsync_task);
}

static int dc_main_path_commit_thread(void *data)
{
	int ret;
	struct sched_param param = {.sched_priority = 94 };
	struct drm_crtc *crtc = data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		ret = wait_event_interruptible(mtk_crtc->dc_main_path_commit_wq,
			atomic_read(&mtk_crtc->dc_main_path_commit_event));
		if (ret == 0) {
			atomic_set(&mtk_crtc->dc_main_path_commit_event, 0);
			mtk_crtc_dc_prim_path_update(crtc);
		} else {
			DDPINFO("wait dc commit event interrupted, ret = %d\n",
				     ret);
		}

	} while (!kthread_should_stop());

	return 0;
}

static int mtk_drm_pf_release_thread(void *data)
{
	int ret;
	struct sched_param param = {.sched_priority = 87};
	struct mtk_drm_private *private;
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *)data;
	struct drm_crtc *crtc;
	struct cmdq_pkt_buffer *cmdq_buf;
	unsigned int fence_idx, crtc_idx;

	crtc = &mtk_crtc->base;
	private = crtc->dev->dev_private;
	crtc_idx = drm_crtc_index(crtc);
	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(mtk_crtc->present_fence_wq,
			atomic_read(&mtk_crtc->pf_event));
		if (ret) {
			DDPPR_ERR("%s wait_event_interruptible failed, ret = %d\n", __func__, ret);
			return ret;
		}
		atomic_set(&mtk_crtc->pf_event, 0);

		mutex_lock(&private->commit.lock);
		cmdq_buf = &(mtk_crtc->gce_obj.buf);
		fence_idx = *(unsigned int *)(cmdq_buf->va_base +
				DISP_SLOT_PRESENT_FENCE(crtc_idx));

		mtk_release_present_fence(private->session_id[crtc_idx],
					  fence_idx, 0);
		mutex_unlock(&private->commit.lock);
	}

	return 0;
}

static int mtk_drm_sf_pf_release_thread(void *data)
{
	struct sched_param param = {.sched_priority = 87};
	struct mtk_drm_private *private;
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *)data;
	struct drm_crtc *crtc;
	struct cmdq_pkt_buffer *cmdq_buf;
	unsigned int fence_idx, crtc_idx;
	int ret = 0;

	crtc = &mtk_crtc->base;
	private = crtc->dev->dev_private;
	crtc_idx = drm_crtc_index(crtc);
	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(mtk_crtc->sf_present_fence_wq,
					 atomic_read(&mtk_crtc->sf_pf_event));
		if (ret) {
			DDPPR_ERR("%s wait_event_interruptible failed, ret = %d\n", __func__, ret);
			return ret;
		}
		atomic_set(&mtk_crtc->sf_pf_event, 0);

		mutex_lock(&private->commit.lock);
		cmdq_buf = &(mtk_crtc->gce_obj.buf);
		fence_idx = *(unsigned int *)(cmdq_buf->va_base +
				DISP_SLOT_SF_PRESENT_FENCE(crtc_idx));

		mtk_release_sf_present_fence(private->session_id[crtc_idx],
					     fence_idx);
		mutex_unlock(&private->commit.lock);
	}

	return 0;
}

int mtk_drm_crtc_create(struct drm_device *drm_dev,
			const struct mtk_crtc_path_data *path_data)
{
	struct mtk_drm_private *priv = drm_dev->dev_private;
	struct device *dev = drm_dev->dev;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *output_comp;
	enum drm_plane_type type;
	unsigned int zpos;
	int pipe = priv->num_pipes;
	int ret;
	int i, j, p_mode;
#ifdef MTK_FB_MMDVFS_SUPPORT
	u32 result;
#endif
	enum mtk_ddp_comp_id comp_id;

	DDPMSG("%s+\n", __func__);

	if (!path_data)
		return 0;

	for_each_comp_id_in_path_data(comp_id, path_data, i, j, p_mode) {
		struct device_node *node;

		if (mtk_ddp_comp_get_type(comp_id) == MTK_DISP_VIRTUAL)
			continue;

		if (comp_id < 0) {
			DDPPR_ERR("%s: Invalid comp_id:%d\n", __func__, comp_id);
			return 0;
		}

		node = priv->comp_node[comp_id];
		if (!node) {
			dev_info(
				dev,
				"Not creating crtc %d because component %d is disabled or missing\n",
				pipe, comp_id);
			DDPPR_ERR(
				"Not creating crtc %d because component %d is disabled or missing\n",
				pipe, comp_id);

			return 0;
		}
	}

	mtk_crtc = devm_kzalloc(dev, sizeof(*mtk_crtc), GFP_KERNEL);
	if (!mtk_crtc)
		return -ENOMEM;

	// TODO: It should use platform_driverdata or device tree to define it.
	// It's just for P90 temp workaround, will be modifyed later.
	if (pipe == 0)
		mtk_crtc->layer_nr = OVL_LAYER_NR;
	else if (pipe == 1)
		mtk_crtc->layer_nr = EXTERNAL_INPUT_LAYER_NR;
	else if (pipe == 2)
		mtk_crtc->layer_nr = MEMORY_INPUT_LAYER_NR;

	mutex_init(&mtk_crtc->lock);
	mutex_init(&mtk_crtc->cwb_lock);
	mtk_crtc->config_regs = priv->config_regs;
	mtk_crtc->config_regs_pa = priv->config_regs_pa;
	mtk_crtc->mmsys_reg_data = priv->reg_data;
	mtk_crtc->path_data = path_data;
	mtk_crtc->is_dual_pipe = false;

	for (i = 0; i < DDP_MODE_NR; i++) {
		for (j = 0; j < DDP_PATH_NR; j++) {
			mtk_crtc->ddp_ctx[i].ddp_comp_nr[j] =
				path_data->path_len[i][j];
			mtk_crtc->ddp_ctx[i].ddp_comp[j] = devm_kmalloc_array(
				dev, path_data->path_len[i][j],
				sizeof(struct mtk_ddp_comp *), GFP_KERNEL);
			mtk_crtc->ddp_ctx[i].req_hrt[j] =
				path_data->path_req_hrt[i][j];
		}
		mtk_crtc->ddp_ctx[i].wb_comp_nr = path_data->wb_path_len[i];
		mtk_crtc->ddp_ctx[i].wb_comp = devm_kmalloc_array(
			dev, path_data->wb_path_len[i],
			sizeof(struct mtk_ddp_comp *), GFP_KERNEL);
	}

	for (i = 0; i < DDP_PATH_NR; i++) {
		mtk_crtc->mutex[i] = mtk_disp_mutex_get(priv->mutex_dev,
							pipe * DDP_PATH_NR + i);
		if (IS_ERR(mtk_crtc->mutex[i])) {
			ret = PTR_ERR(mtk_crtc->mutex[i]);
			dev_err(dev, "Failed to get mutex: %d\n", ret);
			return ret;
		}
	}

	for_each_comp_id_in_path_data(comp_id, path_data, i, j, p_mode) {
		struct mtk_ddp_comp *comp;
		struct device_node *node;
		bool *rdma_memory_mode;

		if (comp_id < 0) {
			DDPPR_ERR("%s: Invalid comp_id:%d\n", __func__, comp_id);
			return 0;
		}

		if (mtk_ddp_comp_get_type(comp_id) == MTK_DISP_VIRTUAL) {
			struct mtk_ddp_comp *comp;

			comp = kzalloc(sizeof(*comp), GFP_KERNEL);
			comp->id = comp_id;
			mtk_crtc->ddp_ctx[p_mode].ddp_comp[i][j] = comp;
			continue;
		}

		node = priv->comp_node[comp_id];
		comp = priv->ddp_comp[comp_id];

		if (!comp) {
			dev_err(dev, "Component %s not initialized\n",
				node->full_name);
			return -ENODEV;
		}

		if ((p_mode == DDP_MAJOR) && (comp_id == DDP_COMPONENT_WDMA0 ||
					      comp_id == DDP_COMPONENT_WDMA1)) {
			ret = mtk_wb_connector_init(drm_dev, mtk_crtc);
			if (ret != 0)
				return ret;

			ret = mtk_wb_set_possible_crtcs(drm_dev, mtk_crtc,
							BIT(pipe));
			if (ret != 0)
				return ret;
		}

		if ((j == 0) && (p_mode == DDP_MAJOR) &&
		    (comp_id == DDP_COMPONENT_RDMA0 ||
		     comp_id == DDP_COMPONENT_RDMA1 ||
		     comp_id == DDP_COMPONENT_RDMA2 ||
		     comp_id == DDP_COMPONENT_RDMA3 ||
		     comp_id == DDP_COMPONENT_RDMA4 ||
		     comp_id == DDP_COMPONENT_RDMA5)) {
			rdma_memory_mode = comp->comp_mode;
			*rdma_memory_mode = true;
			mtk_crtc->layer_nr = RDMA_LAYER_NR;
		}

		mtk_crtc->ddp_ctx[p_mode].ddp_comp[i][j] = comp;
	}

	for_each_wb_comp_id_in_path_data(comp_id, path_data, i, p_mode) {
		struct mtk_ddp_comp *comp;
		struct device_node *node;

		if (comp_id < 0) {
			DDPPR_ERR("%s: Invalid comp_id:%d\n", __func__, comp_id);
			return 0;
		}

		if (mtk_ddp_comp_get_type(comp_id) == MTK_DISP_VIRTUAL) {
			struct mtk_ddp_comp *comp;

			comp = kzalloc(sizeof(*comp), GFP_KERNEL);
			comp->id = comp_id;
			mtk_crtc->ddp_ctx[p_mode].wb_comp[i] = comp;
			continue;
		}
		if (comp_id < 0) {
			DDPPR_ERR("%s: Invalid comp_id:%d\n", __func__, comp_id);
			return 0;
		}
		node = priv->comp_node[comp_id];
		comp = priv->ddp_comp[comp_id];

		if (!comp) {
			dev_err(dev, "Component %s not initialized\n",
				node->full_name);
			return -ENODEV;
		}
		mtk_crtc->ddp_ctx[p_mode].wb_comp[i] = comp;
	}

	mtk_crtc->planes = devm_kzalloc(dev,
			mtk_crtc->layer_nr * sizeof(struct mtk_drm_plane),
			GFP_KERNEL);
	if (!mtk_crtc->planes)
		return -ENOMEM;

	for (zpos = 0; zpos < mtk_crtc->layer_nr; zpos++) {
		type = (zpos == 0) ? DRM_PLANE_TYPE_PRIMARY
				   : (zpos == (mtk_crtc->layer_nr - 1UL))
					     ? DRM_PLANE_TYPE_CURSOR
					     : DRM_PLANE_TYPE_OVERLAY;
		ret = mtk_plane_init(drm_dev, &mtk_crtc->planes[zpos], zpos,
				     BIT(pipe), type);
		mtk_crtc->planes[zpos].base.crtc = &mtk_crtc->base;
		if (ret)
			return ret;
	}

	if (mtk_crtc->layer_nr == 1UL) {
		ret = mtk_drm_crtc_init(drm_dev, mtk_crtc,
					&mtk_crtc->planes[0].base, NULL, pipe);
	} else {
		ret = mtk_drm_crtc_init(
			drm_dev, mtk_crtc, &mtk_crtc->planes[0].base,
			&mtk_crtc->planes[mtk_crtc->layer_nr - 1UL].base, pipe);
	}
	if (ret < 0)
		return ret;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, REQ_PANEL_EXT,
				    &mtk_crtc->panel_ext);

	drm_mode_crtc_set_gamma_size(&mtk_crtc->base, MTK_LUT_SIZE);
	/* TODO: Skip color mgmt first */
	// drm_crtc_enable_color_mgmt(&mtk_crtc->base, 0, false, MTK_LUT_SIZE);
	priv->crtc[pipe] = &mtk_crtc->base;
	priv->num_pipes++;

	dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	mtk_crtc_init_gce_obj(drm_dev, mtk_crtc);

	mtk_crtc->vblank_en = 1;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLE_MGR) &&
	    drm_crtc_index(&mtk_crtc->base) == 0) {
		char name[50];

		mtk_drm_idlemgr_init(&mtk_crtc->base,
				     drm_crtc_index(&mtk_crtc->base));

		snprintf(name, sizeof(name), "enable_vblank");
		mtk_crtc->vblank_enable_task = kthread_create(
			mtk_crtc_enable_vblank_thread, priv->crtc[pipe], name);
		init_waitqueue_head(&mtk_crtc->vblank_enable_wq);
		wake_up_process(mtk_crtc->vblank_enable_task);
	}


	init_waitqueue_head(&mtk_crtc->crtc_status_wq);
	if (drm_crtc_index(&mtk_crtc->base) == 0) {
		mtk_disp_hrt_cond_init(&mtk_crtc->base);
		atomic_set(&mtk_crtc->qos_ctx->last_hrt_idx, 0);
		mtk_crtc->qos_ctx->last_hrt_req = 0;
		init_waitqueue_head(&mtk_crtc->qos_ctx->hrt_cond_wq);
	}

	if (drm_crtc_index(&mtk_crtc->base) == 0)
		mtk_drm_cwb_init(&mtk_crtc->base);

	mtk_disp_chk_recover_init(&mtk_crtc->base);

	mtk_drm_fake_vsync_init(&mtk_crtc->base);

#ifdef MTK_FB_MMDVFS_SUPPORT
	if (drm_crtc_index(&mtk_crtc->base) == 1) {
		mtk_pm_qos_add_request(&mm_freq_request, PM_QOS_DISP_FREQ,
			PM_QOS_MM_FREQ_DEFAULT_VALUE);
		result = mmdvfs_qos_get_freq_steps(PM_QOS_DISP_FREQ,
			freq_steps, &step_size);
		if (result < 0)
			DDPPR_ERR("Get mmdvfs steps fail, result:%d\n", result);
	}
#endif
	if (mtk_crtc_support_dc_mode(&mtk_crtc->base)) {
		mtk_crtc->dc_main_path_commit_task = kthread_create(
				dc_main_path_commit_thread,
				&mtk_crtc->base, "decouple_update_rdma_cfg");
		atomic_set(&mtk_crtc->dc_main_path_commit_event, 1);
		init_waitqueue_head(&mtk_crtc->dc_main_path_commit_wq);
		wake_up_process(mtk_crtc->dc_main_path_commit_task);
	}

	if (drm_crtc_index(&mtk_crtc->base) == 0) {
		init_waitqueue_head(&mtk_crtc->trigger_event);
		mtk_crtc->trigger_event_task =
			kthread_create(_mtk_crtc_check_trigger,
						mtk_crtc, "ddp_trig");
		wake_up_process(mtk_crtc->trigger_event_task);

		init_waitqueue_head(&mtk_crtc->trigger_delay);
		mtk_crtc->trigger_delay_task =
			kthread_create(_mtk_crtc_check_trigger_delay,
						mtk_crtc, "ddp_trig_d");
		wake_up_process(mtk_crtc->trigger_delay_task);
		/* For protect crtc blank state */
		mutex_init(&mtk_crtc->blank_lock);
		init_waitqueue_head(&mtk_crtc->state_wait_queue);

		init_waitqueue_head(&mtk_crtc->trigger_cmdq);
		mtk_crtc->trig_cmdq_task =
			kthread_run(_mtk_crtc_cmdq_retrig,
					mtk_crtc, "ddp_cmdq_trig");
	}

	init_waitqueue_head(&mtk_crtc->present_fence_wq);
	atomic_set(&mtk_crtc->pf_event, 0);
	mtk_crtc->pf_release_thread =
		kthread_run(mtk_drm_pf_release_thread,
						mtk_crtc, "pf_release_thread");

	init_waitqueue_head(&mtk_crtc->sf_present_fence_wq);
	atomic_set(&mtk_crtc->sf_pf_event, 0);
	mtk_crtc->sf_pf_release_thread =
				kthread_run(mtk_drm_sf_pf_release_thread,
					    mtk_crtc, "sf_pf_release_thread");

	/* init wakelock resources */
	{
		unsigned int len = 21;

		mtk_crtc->wk_lock_name = vzalloc(len * sizeof(char));

		snprintf(mtk_crtc->wk_lock_name, len * sizeof(char),
			 "disp_crtc%u_wakelock",
			 drm_crtc_index(&mtk_crtc->base));

#ifdef CONFIG_PM_SLEEP
		wakeup_source_prepare(&mtk_crtc->wk_lock, mtk_crtc->wk_lock_name);
#endif
		wakeup_source_add(&mtk_crtc->wk_lock);
	}

#ifdef MTK_DRM_FB_LEAK
	mtk_crtc->signal_present_fece_task = kthread_create(
		mtk_drm_signal_fence_worker_kthread, mtk_crtc, "signal_fence");
	init_waitqueue_head(&mtk_crtc->signal_fence_task_wq);
	atomic_set(&mtk_crtc->cmdq_done, 0);
	wake_up_process(mtk_crtc->signal_present_fece_task);
#endif
	DDPMSG("%s-CRTC%d create successfully\n", __func__,
		priv->num_pipes - 1);

	return 0;
}

/**
 * wakeup_source_prepare - Prepare a new wakeup source for initialization.
 * @ws: Wakeup source to prepare.
 * @name: Pointer to the name of the new wakeup source.
 *
 * Callers must ensure that the @name string won't be freed when @ws is still in
 * use.
 */
void wakeup_source_prepare(struct wakeup_source *ws, const char *name)
{
	if (ws) {
		memset(ws, 0, sizeof(*ws));
		ws->name = name;
	}
}

int mtk_drm_crtc_getfence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_mtk_fence *args = data;
	struct mtk_drm_private *private;
	struct fence_data fence;
	unsigned int fence_idx;
	struct mtk_fence_info *l_info = NULL;
	int tl, idx;

	crtc = drm_crtc_find(dev, file_priv, args->crtc_id);
	if (!crtc) {
		DDPPR_ERR("Unknown CRTC ID %d\n", args->crtc_id);
		ret = -ENOENT;
		return ret;
	}
	DDPDBG("[CRTC:%d:%s]\n", crtc->base.id, crtc->name);

	idx = drm_crtc_index(crtc);
	if (!crtc->dev) {
		DDPPR_ERR("%s:%d dev is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	if (!crtc->dev->dev_private) {
		DDPPR_ERR("%s:%d dev private is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	private = crtc->dev->dev_private;
	fence_idx = atomic_read(&private->crtc_present[idx]);
	tl = mtk_fence_get_present_timeline_id(mtk_get_session_id(crtc));
	l_info = mtk_fence_get_layer_info(mtk_get_session_id(crtc), tl);
	if (!l_info) {
		DDPPR_ERR("%s:%d layer_info is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	/* create fence */
	fence.fence = MTK_INVALID_FENCE_FD;
	fence.value = ++fence_idx;
	atomic_inc(&private->crtc_present[idx]);
	ret = mtk_sync_fence_create(l_info->timeline, &fence);
	if (ret) {
		DDPPR_ERR("%d,L%d create Fence Object failed!\n",
			  MTK_SESSION_DEV(mtk_get_session_id(crtc)), tl);
		ret = -EFAULT;
	}

	args->fence_fd = fence.fence;
	args->fence_idx = fence.value;

	DDPFENCE("P+/%d/L%d/idx%d/fd%d\n",
		 MTK_SESSION_DEV(mtk_get_session_id(crtc)),
		 tl, args->fence_idx,
		 args->fence_fd);
	return ret;
}

int mtk_drm_crtc_get_sf_fence_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_mtk_fence *args = data;
	struct mtk_drm_private *private;
	struct fence_data fence;
	struct mtk_crtc_state *crtc_state;
	unsigned int fence_idx;
	struct mtk_fence_info *l_info = NULL;
	int tl, idx;

	crtc = drm_crtc_find(dev, file_priv, args->crtc_id);
	if (!crtc) {
		DDPPR_ERR("Unknown CRTC ID %d\n", args->crtc_id);
		ret = -ENOENT;
		return ret;
	}
	DDPDBG("[CRTC:%d:%s]\n", crtc->base.id, crtc->name);

	idx = drm_crtc_index(crtc);
	if (!crtc->dev) {
		DDPPR_ERR("%s:%d dev is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	if (!crtc->dev->dev_private) {
		DDPPR_ERR("%s:%d dev private is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	private = crtc->dev->dev_private;
	fence_idx = atomic_read(&private->crtc_sf_present[idx]);
	tl = mtk_fence_get_sf_present_timeline_id(mtk_get_session_id(crtc));
	l_info = mtk_fence_get_layer_info(mtk_get_session_id(crtc), tl);
	if (!l_info) {
		DDPPR_ERR("%s:%d layer_info is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_SF_PF)) {
		/* create fence */
		fence.fence = MTK_INVALID_FENCE_FD;
		fence.value = ++fence_idx;
		atomic_inc(&private->crtc_sf_present[idx]);
		ret = mtk_sync_fence_create(l_info->timeline, &fence);
		if (ret) {
			DDPPR_ERR("%d,L%d create Fence Object failed!\n",
				  MTK_SESSION_DEV(mtk_get_session_id(crtc)), tl);
			ret = -EFAULT;
		}

		args->fence_fd = fence.fence;
		args->fence_idx = fence.value;
		crtc_state = to_mtk_crtc_state(crtc->state);

		if (crtc_state && crtc_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
			DDPINFO("%s:%d DOZE mode not support SF rensent fence\n",
				__func__, __LINE__);
			ret = -EFAULT;
		}
	} else {
		args->fence_fd = MTK_INVALID_FENCE_FD;
		args->fence_idx = fence_idx;
	}

	DDPFENCE("P+/%d/L%d/idx%d/fd%d\n",
		 MTK_SESSION_DEV(mtk_get_session_id(crtc)),
		 tl, args->fence_idx,
		 args->fence_fd);
	return ret;
}

static int __crtc_need_composition_wb(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx;

	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
	if (ddp_ctx->wb_comp_nr != 0)
		return true;
	else
		return false;
}

static void mtk_crtc_disconnect_single_path_cmdq(struct drm_crtc *crtc,
						 struct cmdq_pkt *cmdq_handle,
						 unsigned int path_idx,
						 unsigned int ddp_mode,
						 unsigned int mutex_id)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	int i;

	ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx] - 1; i++)
		mtk_ddp_remove_comp_from_path_with_cmdq(
			mtk_crtc, ddp_ctx->ddp_comp[path_idx][i]->id,
			ddp_ctx->ddp_comp[path_idx][i + 1]->id, cmdq_handle);

	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx]; i++)
		mtk_disp_mutex_remove_comp_with_cmdq(
			mtk_crtc, ddp_ctx->ddp_comp[path_idx][i]->id,
			cmdq_handle, mutex_id);
}

static void mtk_crtc_connect_single_path_cmdq(struct drm_crtc *crtc,
					      struct cmdq_pkt *cmdq_handle,
					      unsigned int path_idx,
					      unsigned int ddp_mode,
					      unsigned int mutex_id)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	int i;

	ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx] - 1; i++)
		mtk_ddp_add_comp_to_path_with_cmdq(
			mtk_crtc, ddp_ctx->ddp_comp[path_idx][i]->id,
			ddp_ctx->ddp_comp[path_idx][i + 1]->id, cmdq_handle);

	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx]; i++)
		mtk_disp_mutex_add_comp_with_cmdq(
			mtk_crtc, ddp_ctx->ddp_comp[path_idx][i]->id,
			mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base),
			cmdq_handle, mutex_id);
}

static void mtk_crtc_config_dual_pipe_cmdq(struct mtk_drm_crtc *mtk_crtc,
					     struct cmdq_pkt *cmdq_handle,
					     unsigned int path_idx,
					     struct mtk_ddp_config *cfg)
{
	//struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	int i;

	DDPFUNC();
	ddp_ctx = &mtk_crtc->dual_pipe_ddp_ctx;
	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx]; i++) {
		struct mtk_ddp_comp *comp = ddp_ctx->ddp_comp[path_idx][i];

		if (comp->id == DDP_COMPONENT_RDMA4 ||
		    comp->id == DDP_COMPONENT_RDMA5) {
			bool *rdma_memory_mode =
				ddp_ctx->ddp_comp[path_idx][i]->comp_mode;

				*rdma_memory_mode = false;
			break;
		}
	}

	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx]; i++) {
		struct mtk_ddp_comp *comp = ddp_ctx->ddp_comp[path_idx][i];

		mtk_ddp_comp_config(comp, cfg, cmdq_handle);
		mtk_ddp_comp_start(comp, cmdq_handle);
	}
}

static void mtk_crtc_config_single_path_cmdq(struct drm_crtc *crtc,
					     struct cmdq_pkt *cmdq_handle,
					     unsigned int path_idx,
					     unsigned int ddp_mode,
					     struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	int i;

	ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx]; i++) {
		struct mtk_ddp_comp *comp = ddp_ctx->ddp_comp[path_idx][i];

		if (comp->id == DDP_COMPONENT_RDMA0 ||
		    comp->id == DDP_COMPONENT_RDMA1 ||
		    comp->id == DDP_COMPONENT_RDMA2 ||
		    comp->id == DDP_COMPONENT_RDMA4 ||
		    comp->id == DDP_COMPONENT_RDMA5) {
			bool *rdma_memory_mode =
				ddp_ctx->ddp_comp[path_idx][i]->comp_mode;

			if (i == 0)
				*rdma_memory_mode = true;
			else
				*rdma_memory_mode = false;
			break;
		}
	}

	for (i = 0; i < ddp_ctx->ddp_comp_nr[path_idx]; i++) {
		struct mtk_ddp_comp *comp = ddp_ctx->ddp_comp[path_idx][i];

		mtk_ddp_comp_config(comp, cfg, cmdq_handle);
		mtk_ddp_comp_start(comp, cmdq_handle);
		if (!mtk_drm_helper_get_opt(
				    priv->helper_opt,
				    MTK_DRM_OPT_USE_PQ))
			mtk_ddp_comp_bypass(comp, 1, cmdq_handle);
	}

	if (mtk_crtc->is_dual_pipe)
		mtk_crtc_config_dual_pipe_cmdq(mtk_crtc, cmdq_handle,
					     path_idx, cfg);
}

static void mtk_crtc_create_wb_path_cmdq(struct drm_crtc *crtc,
					 struct cmdq_pkt *cmdq_handle,
					 unsigned int ddp_mode,
					 unsigned int mutex_id,
					 struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	struct mtk_drm_gem_obj *mtk_gem;
	struct mtk_ddp_comp *comp;
	dma_addr_t addr;
	struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
	int i;

	ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	for (i = 0; i < ddp_ctx->wb_comp_nr - 1; i++)
		mtk_ddp_add_comp_to_path_with_cmdq(
			mtk_crtc, ddp_ctx->wb_comp[i]->id,
			ddp_ctx->wb_comp[i + 1]->id, cmdq_handle);

	for (i = 0; i < ddp_ctx->wb_comp_nr; i++)
		mtk_disp_mutex_add_comp_with_cmdq(
			mtk_crtc, ddp_ctx->wb_comp[i]->id,
			mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base),
			cmdq_handle, mutex_id);

	if (!ddp_ctx->wb_fb) {
		struct drm_mode_fb_cmd2 mode = {0};

		mode.width = crtc->state->adjusted_mode.hdisplay;
		mode.height = crtc->state->adjusted_mode.vdisplay;
		mtk_gem = mtk_drm_gem_create(
			crtc->dev, mode.width * mode.height * 3, true);
		mode.pixel_format = DRM_FORMAT_RGB888;
		mode.pitches[0] = mode.width * 3;

		ddp_ctx->wb_fb = mtk_drm_framebuffer_create(
			crtc->dev, &mode, &mtk_gem->base);
	}

	for (i = 0; i < ddp_ctx->wb_comp_nr; i++) {
		comp = ddp_ctx->wb_comp[i];
		if (comp->id == DDP_COMPONENT_WDMA0 ||
		    comp->id == DDP_COMPONENT_WDMA1) {
			comp->fb = ddp_ctx->wb_fb;
			break;
		}
	}

	/* All the 1to2 path shoulbe be real-time */
	for (i = 0; i < ddp_ctx->wb_comp_nr; i++) {
		struct mtk_ddp_comp *comp = ddp_ctx->wb_comp[i];

		mtk_ddp_comp_config(comp, cfg, cmdq_handle);
		mtk_ddp_comp_start(comp, cmdq_handle);
		if (!mtk_drm_helper_get_opt(
				    priv->helper_opt,
				    MTK_DRM_OPT_USE_PQ))
			mtk_ddp_comp_bypass(comp, 1, cmdq_handle);
	}

	addr = cmdq_buf->pa_base + DISP_SLOT_RDMA_FB_ID;
	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
		addr, 0, ~0);
}

static void mtk_crtc_destroy_wb_path_cmdq(struct drm_crtc *crtc,
					  struct cmdq_pkt *cmdq_handle,
					  unsigned int mutex_id,
					  struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	int i;

	if (!__crtc_need_composition_wb(crtc))
		return;

	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
	for (i = 0; i < ddp_ctx->wb_comp_nr - 1; i++)
		mtk_ddp_remove_comp_from_path_with_cmdq(
			mtk_crtc, ddp_ctx->wb_comp[i]->id,
			ddp_ctx->wb_comp[i + 1]->id, cmdq_handle);

	for (i = 0; i < ddp_ctx->wb_comp_nr; i++)
		mtk_disp_mutex_remove_comp_with_cmdq(mtk_crtc,
						     ddp_ctx->wb_comp[i]->id,
						     cmdq_handle, mutex_id);
}

static void mtk_crtc_config_wb_path_cmdq(struct drm_crtc *crtc,
					 struct cmdq_pkt *cmdq_handle,
					 unsigned int path_idx,
					 unsigned int ddp_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	struct mtk_plane_state plane_state;
	struct drm_framebuffer *fb = NULL;
	struct mtk_ddp_comp *comp;
	int i;

	if (!__crtc_need_composition_wb(crtc))
		return;

	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
	for (i = 0; i < ddp_ctx->wb_comp_nr; i++) {
		comp = ddp_ctx->wb_comp[i];
		if (comp->id == DDP_COMPONENT_WDMA0 ||
		    comp->id == DDP_COMPONENT_WDMA1) {
			fb = comp->fb;
			break;
		}
	}

	if (!fb) {
		DDPPR_ERR("%s, fb is empty\n", __func__);
		return;
	}
	plane_state.pending.enable = true;
	plane_state.pending.pitch = fb->pitches[0];
	plane_state.pending.format = fb->format->format;
	plane_state.pending.addr = mtk_fb_get_dma(fb);
	plane_state.pending.size = mtk_fb_get_size(fb);
	plane_state.pending.src_x = 0;
	plane_state.pending.src_y = 0;
	plane_state.pending.dst_x = 0;
	plane_state.pending.dst_y = 0;
	plane_state.pending.width = fb->width;
	plane_state.pending.height = fb->height;
	ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	mtk_ddp_comp_layer_config(ddp_ctx->ddp_comp[path_idx][0], 0,
				  &plane_state, cmdq_handle);
}

static int __mtk_crtc_composition_wb(
				     struct drm_crtc *crtc,
				     unsigned int ddp_mode,
				     struct mtk_ddp_config *cfg)
{
	struct cmdq_pkt *cmdq_handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int gce_event;

	if (!__crtc_need_composition_wb(crtc))
		return 0;

	DDPINFO("%s\n", __func__);

	gce_event = get_path_wait_event(mtk_crtc, mtk_crtc->ddp_mode);
	mtk_crtc_pkt_create(&cmdq_handle, crtc,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);
	mtk_crtc_create_wb_path_cmdq(crtc, cmdq_handle, mtk_crtc->ddp_mode, 0,
				     cfg);
	if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
	if (gce_event > 0) {
		cmdq_pkt_clear_event(cmdq_handle, gce_event);
		cmdq_pkt_wait_no_clear(cmdq_handle, gce_event);
	}
	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	return 0;
}

bool mtk_crtc_with_sub_path(struct drm_crtc *crtc, unsigned int ddp_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	return mtk_crtc->ddp_ctx[ddp_mode].ddp_comp_nr[1];
}

static void __mtk_crtc_prim_path_switch(struct drm_crtc *crtc,
					unsigned int ddp_mode,
					struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct cmdq_pkt *cmdq_handle;
	int cur_path_idx, next_path_idx;

	DDPINFO("%s\n", __func__);

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		cur_path_idx = DDP_SECOND_PATH;
	else
		cur_path_idx = DDP_FIRST_PATH;
	if (mtk_crtc_with_sub_path(crtc, ddp_mode))
		next_path_idx = DDP_SECOND_PATH;
	else
		next_path_idx = DDP_FIRST_PATH;

	mtk_crtc_pkt_create(&cmdq_handle,
		crtc, mtk_crtc->gce_obj.client[CLIENT_CFG]);
	if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
		cmdq_pkt_clear_event(
			cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, cur_path_idx, 0);

	/* 1. Disconnect current path and remove component mutexs */
	if (!mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode)) {
		_mtk_crtc_atmoic_addon_module_disconnect(
			crtc, mtk_crtc->ddp_mode, &crtc_state->lye_state,
			cmdq_handle);
	}
	mtk_crtc_addon_connector_disconnect(crtc, cmdq_handle);
	mtk_crtc_disconnect_single_path_cmdq(crtc, cmdq_handle, cur_path_idx,
					     mtk_crtc->ddp_mode, 0);

	/* 2. Remove composition path and cooresonding mutexs */
	mtk_crtc_destroy_wb_path_cmdq(crtc, cmdq_handle, 0, cfg);

	/* 3. Connect new primary path and add component mutexs */
	mtk_crtc_connect_single_path_cmdq(crtc, cmdq_handle, next_path_idx,
					  ddp_mode, 0);
	/* TODO: refine addon_connector */
	mtk_crtc_addon_connector_connect(crtc, cmdq_handle);

	/* 4. Primary path configurations */
	cfg->p_golden_setting_context =
				__get_golden_setting_context(mtk_crtc);
	if (mtk_crtc_target_is_dc_mode(crtc, ddp_mode))
		cfg->p_golden_setting_context->is_dc = 1;
	else
		cfg->p_golden_setting_context->is_dc = 0;

	mtk_crtc_config_single_path_cmdq(crtc, cmdq_handle, next_path_idx,
					 ddp_mode, cfg);

	if (!mtk_crtc_with_sub_path(crtc, ddp_mode))
		_mtk_crtc_atmoic_addon_module_connect(
			crtc, ddp_mode, &crtc_state->lye_state, cmdq_handle);

	/* 5. Set composed write back buffer */
	mtk_crtc_config_wb_path_cmdq(crtc, cmdq_handle, next_path_idx,
				     ddp_mode);

	if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
		cmdq_pkt_set_event(cmdq_handle,
				   mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
}

static void __mtk_crtc_old_sub_path_destroy(struct drm_crtc *crtc,
					    struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct cmdq_pkt *cmdq_handle;
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	int i;
	struct mtk_ddp_comp *comp = NULL;
	int index = drm_crtc_index(crtc);

	if (!mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		return;

	mtk_crtc_pkt_create(&cmdq_handle, crtc,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);
	_mtk_crtc_atmoic_addon_module_disconnect(crtc, mtk_crtc->ddp_mode,
					&crtc_state->lye_state,
					cmdq_handle);

	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
	for (i = 0; i < ddp_ctx->ddp_comp_nr[DDP_FIRST_PATH]; i++) {
		struct mtk_ddp_comp *comp =
				ddp_ctx->ddp_comp[DDP_FIRST_PATH][i];

		mtk_ddp_comp_stop(comp, cmdq_handle);
	}

	mtk_crtc_disconnect_single_path_cmdq(crtc, cmdq_handle,
		DDP_FIRST_PATH,	mtk_crtc->ddp_mode, 1);

	/* Workaround: if CRTC0, reset wdma->fb to NULL to prevent CRTC2
	 * config wdma and cause KE
	 */
	if (index == 0) {
		comp = mtk_ddp_comp_find_by_id(crtc, DDP_COMPONENT_WDMA0);
		if (!comp)
			comp = mtk_ddp_comp_find_by_id(crtc,
					DDP_COMPONENT_WDMA1);
		if (comp)
			comp->fb = NULL;
	}

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
}

static void __mtk_crtc_sub_path_create(
				       struct drm_crtc *crtc,
				       unsigned int ddp_mode,
				       struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct cmdq_pkt *cmdq_handle;

	if (!mtk_crtc_with_sub_path(crtc, ddp_mode))
		return;

	mtk_crtc_pkt_create(&cmdq_handle, crtc,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
		cmdq_pkt_clear_event(
			cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);

	/* 1. Connect Sub Path*/
	mtk_crtc_connect_single_path_cmdq(crtc, cmdq_handle, DDP_FIRST_PATH,
					  ddp_mode, 1);

	/* 2. Sub path configuration */
	mtk_crtc_config_single_path_cmdq(crtc, cmdq_handle, DDP_FIRST_PATH,
					 ddp_mode, cfg);

	_mtk_crtc_atmoic_addon_module_connect(
		crtc, ddp_mode, &crtc_state->lye_state, cmdq_handle);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
}

static void mtk_crtc_dc_fb_control(struct drm_crtc *crtc,
				unsigned int ddp_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_ddp_ctx *ddp_ctx;
	struct drm_mode_fb_cmd2 mode = {0};
	struct mtk_drm_gem_obj *mtk_gem;

	DDPINFO("%s\n", __func__);

	ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	if (mtk_crtc_target_is_dc_mode(crtc, ddp_mode) &&
		ddp_ctx->dc_fb == NULL) {
		ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
		mode.width = crtc->state->adjusted_mode.hdisplay;
		mode.height = crtc->state->adjusted_mode.vdisplay;
		mtk_gem = mtk_drm_gem_create(
			crtc->dev,
			mtk_crtc_get_dc_fb_size(crtc) * MAX_CRTC_DC_FB, true);
		mode.pixel_format = DRM_FORMAT_RGB888;
		mode.pitches[0] = mode.width * 3;
		ddp_ctx->dc_fb = mtk_drm_framebuffer_create(crtc->dev, &mode,
							    &mtk_gem->base);
	}

	/* do not create wb_fb & dc buffer repeatedly */
#if 0
	ddp_ctx = &mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode];
	if (!mtk_crtc_target_is_dc_mode(crtc, mtk_crtc->ddp_mode)
		&& ddp_ctx->dc_fb) {
		drm_framebuffer_cleanup(ddp_ctx->dc_fb);
		ddp_ctx->dc_fb_idx = 0;
	}

	if (ddp_ctx->wb_fb) {
		drm_framebuffer_cleanup(ddp_ctx->wb_fb);
		ddp_ctx->wb_fb = NULL;
	}
#endif
}

void mtk_crtc_path_switch_prepare(struct drm_crtc *crtc, unsigned int ddp_mode,
				  struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	cfg->w = crtc->state->adjusted_mode.hdisplay;
	cfg->h = crtc->state->adjusted_mode.vdisplay;
	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
		mtk_crtc->panel_ext->params->dyn_fps.switch_en == 1
		&& mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
		cfg->vrefresh =
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
	else
		cfg->vrefresh = crtc->state->adjusted_mode.vrefresh;
	cfg->bpc = mtk_crtc->bpc;
	cfg->x = 0;
	cfg->y = 0;
	cfg->p_golden_setting_context = __get_golden_setting_context(mtk_crtc);

	/* The components in target ddp_mode may be used during path switching,
	 * so attach
	 * the CRTC to them.
	 */
	mtk_crtc_attach_ddp_comp(crtc, ddp_mode, true);
}

void mtk_crtc_path_switch_update_ddp_status(struct drm_crtc *crtc,
					    unsigned int ddp_mode)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, false);
	mtk_crtc_attach_ddp_comp(crtc, ddp_mode, true);
}

void mtk_need_vds_path_switch(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int index = drm_crtc_index(crtc);
	int i = 0;
	int comp_nr = 0;

	if (!(priv && mtk_crtc && (index >= 0))) {
		DDPMSG("Error Invalid params\n");
		return;
	}

	/* In order to confirm it will be called one time,
	 * when switch to or switch back, So need some flags
	 * to control it.
	 */
	if (priv->vds_path_switch_dirty &&
		!priv->vds_path_switch_done && (index == 0)) {
		/* kick idle */
		mtk_drm_idlemgr_kick(__func__, crtc, 0);
		CRTC_MMP_EVENT_START(index, path_switch, mtk_crtc->ddp_mode, 0);

		/* Switch main display path, take away ovl0_2l from main display */
		if (priv->need_vds_path_switch) {
			struct mtk_ddp_comp *comp_ovl0 = NULL;
			struct mtk_ddp_comp *comp_ovl0_2l = NULL;
			struct cmdq_pkt *cmdq_handle = NULL;
			struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
			int keep_first_layer = false;

			if (mtk_crtc->sec_on) {
				mtk_crtc_pkt_create(&cmdq_handle, crtc,
					mtk_crtc->gce_obj.client[CLIENT_SEC_CFG]);
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
				cmdq_sec_pkt_set_data(cmdq_handle, 0, 0,
					CMDQ_SEC_PRIMARY_DISP, CMDQ_METAEX_NONE);

#endif
				DDPMSG("Switch vds: Primary display is sec\n");
			} else {
				cmdq_handle =
					cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

				DDPMSG("Switch vds: Primary display is not sec\n");
			}

			if (IS_ERR_OR_NULL(cmdq_handle)) {
				DDPMSG("Switch vds: cmdq_handle is null or err\n");
				CRTC_MMP_EVENT_END(index, path_switch, crtc->enabled, 0);
				return;
			}
			if (atomic_read(&mtk_crtc->already_config)) {
				mtk_crtc_wait_frame_done(mtk_crtc,
					cmdq_handle, DDP_FIRST_PATH, 0);
				/* Disconnect addon component */
				_mtk_crtc_atmoic_addon_module_disconnect(
					crtc, mtk_crtc->ddp_mode,
					&crtc_state->lye_state, cmdq_handle);
				/* Stop ovl 2l */
				comp_ovl0_2l = priv->ddp_comp[DDP_COMPONENT_OVL0_2L];
				mtk_ddp_comp_stop(comp_ovl0_2l, cmdq_handle);
				mtk_ddp_comp_io_cmd(comp_ovl0_2l, cmdq_handle,
					OVL_ALL_LAYER_OFF, &keep_first_layer);
			}
			/* Change ovl0 bg mode frmoe DL mode to const mode */
			comp_ovl0 = priv->ddp_comp[DDP_COMPONENT_OVL0];
			cmdq_pkt_write(cmdq_handle, comp_ovl0->cmdq_base,
				comp_ovl0->regs_pa + DISP_OVL_DATAPATH_CON, 0x0, 0x4);
			/* Disconnect current path and remove component mutexs */
			if (atomic_read(&mtk_crtc->already_config)) {
				mtk_ddp_remove_comp_from_path_with_cmdq(
					mtk_crtc, DDP_COMPONENT_OVL0_2L,
					DDP_COMPONENT_OVL0, cmdq_handle);
				mtk_disp_mutex_remove_comp_with_cmdq(
					mtk_crtc, DDP_COMPONENT_OVL0_2L, cmdq_handle, 0);
			}

			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);

			/* unprepare ovl 2l */
			if (atomic_read(&mtk_crtc->already_config))
				mtk_ddp_comp_unprepare(comp_ovl0_2l);

			CRTC_MMP_MARK(index, path_switch, 0xFFFF, 1);
			DDPMSG("Switch vds: Switch ovl0_2l to vds\n");

			if (mtk_crtc->ddp_ctx[
					DDP_MAJOR].ddp_comp[DDP_FIRST_PATH][0]->id !=
					DDP_COMPONENT_OVL0) {
				/* Update ddp ctx ddp_comp_nr */
				mtk_crtc->ddp_ctx[DDP_MAJOR].ddp_comp_nr[DDP_FIRST_PATH]
					= mtk_crtc->path_data->path_len[
					DDP_MAJOR][DDP_FIRST_PATH] - 1;
				/* Update ddp ctx ddp_comp */
				comp_nr = mtk_crtc->path_data->path_len[
					DDP_MAJOR][DDP_FIRST_PATH];
				for (i = 0; i < comp_nr - 1; i++)
					mtk_crtc->ddp_ctx[
					DDP_MAJOR].ddp_comp[DDP_FIRST_PATH][i] =
					mtk_crtc->ddp_ctx[
					DDP_MAJOR].ddp_comp[DDP_FIRST_PATH][i+1];
			}
			mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);
			/* Update Switch done flag */
			priv->vds_path_switch_done = 1;
			priv->need_vds_path_switch_back = 1;

		/* Switch main display path, take back ovl0_2l to main display */
		} else {
			struct mtk_ddp_comp *comp_ovl0;
			struct mtk_ddp_comp *comp_ovl0_2l;
			int width = crtc->state->adjusted_mode.hdisplay;
			int height = crtc->state->adjusted_mode.vdisplay;
			struct cmdq_pkt *cmdq_handle;
			int keep_first_layer = false;

			if (mtk_crtc->sec_on) {
				mtk_crtc_pkt_create(&cmdq_handle, crtc,
					mtk_crtc->gce_obj.client[CLIENT_SEC_CFG]);
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
				cmdq_sec_pkt_set_data(cmdq_handle, 0, 0,
					CMDQ_SEC_PRIMARY_DISP, CMDQ_METAEX_NONE);
#endif
				DDPMSG("Switch vds: Primary display is sec\n");
			} else {
				cmdq_handle =
					cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

				DDPMSG("Switch vds: Primary display is not sec\n");
			}

			if (atomic_read(&mtk_crtc->already_config))
				mtk_crtc_wait_frame_done(mtk_crtc,
				cmdq_handle, DDP_FIRST_PATH, 0);

			/* Change ovl0 bg mode frmoe const mode to DL mode */
			comp_ovl0 = priv->ddp_comp[DDP_COMPONENT_OVL0];
			cmdq_pkt_write(cmdq_handle, comp_ovl0->cmdq_base,
				comp_ovl0->regs_pa + DISP_OVL_DATAPATH_CON, 0x4, 0x4);

			/* Change ovl0 ROI size */
			comp_ovl0_2l = priv->ddp_comp[DDP_COMPONENT_OVL0_2L];
			cmdq_pkt_write(cmdq_handle, comp_ovl0_2l->cmdq_base,
				comp_ovl0_2l->regs_pa + DISP_OVL_ROI_SIZE,
				height << 16 | width, ~0);

			if (atomic_read(&mtk_crtc->already_config)) {
				/* Connect cur path components */
				mtk_ddp_add_comp_to_path_with_cmdq(
					mtk_crtc, DDP_COMPONENT_OVL0_2L,
					DDP_COMPONENT_OVL0, cmdq_handle);
				mtk_disp_mutex_add_comp_with_cmdq(
					mtk_crtc, DDP_COMPONENT_OVL0_2L,
					mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base),
					cmdq_handle, 0);

				/* Switch back need reprepare ovl0_2l */
				mtk_ddp_comp_prepare(comp_ovl0_2l);
				mtk_ddp_comp_start(comp_ovl0_2l, cmdq_handle);
				/* ovl 2l should not have old config, so disable
				 * all ovl 2l layer when flush first frame after
				 * switch ovl 2l back to main disp
				 */
				mtk_ddp_comp_io_cmd(comp_ovl0_2l, cmdq_handle,
					OVL_ALL_LAYER_OFF, &keep_first_layer);
			}

			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);

			CRTC_MMP_MARK(index, path_switch, 0xFFFF, 2);
			DDPMSG("Switch vds: Switch ovl0_2l to main disp\n");

			if (mtk_crtc->ddp_ctx[
					DDP_MAJOR].ddp_comp[DDP_FIRST_PATH][0]->id !=
					DDP_COMPONENT_OVL0_2L) {
				/* Update ddp ctx ddp_comp_nr */
				mtk_crtc->ddp_ctx[DDP_MAJOR].ddp_comp_nr[DDP_FIRST_PATH]
					= mtk_crtc->path_data->path_len[
					DDP_MAJOR][DDP_FIRST_PATH];
				/* Update ddp ctx ddp_comp */
				comp_nr = mtk_crtc->path_data->path_len[
					DDP_MAJOR][DDP_FIRST_PATH];
				for (i = comp_nr - 1; i > 0; i--)
					mtk_crtc->ddp_ctx[
					DDP_MAJOR].ddp_comp[DDP_FIRST_PATH][i] =
					mtk_crtc->ddp_ctx[
					DDP_MAJOR].ddp_comp[DDP_FIRST_PATH][i-1];
				mtk_crtc->ddp_ctx[
					DDP_MAJOR].ddp_comp[DDP_FIRST_PATH][0] =
					priv->ddp_comp[DDP_COMPONENT_OVL0_2L];
			}
			mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);
			/* Update Switch done flag */
			priv->vds_path_switch_dirty = 0;
			priv->need_vds_path_switch_back = 0;
		}

		DDPMSG("Switch vds: Switch ovl0_2l Done\n");
		CRTC_MMP_EVENT_END(index, path_switch, crtc->enabled, 0);
	}
}

int mtk_crtc_path_switch(struct drm_crtc *crtc, unsigned int ddp_mode,
			 int need_lock)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_config cfg;
	int index = drm_crtc_index(crtc);
	bool need_wait;

	CRTC_MMP_EVENT_START(index, path_switch, mtk_crtc->ddp_mode,
			ddp_mode);

	if (need_lock)
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (ddp_mode == mtk_crtc->ddp_mode || !crtc->enabled) {
		DDPINFO("CRTC%d skip path switch %u->%u, enable:%d\n",
			index, mtk_crtc->ddp_mode,
			ddp_mode, crtc->enabled);
		CRTC_MMP_MARK(index, path_switch, 0, 0);
		goto done;
	}
	if ((index == 0) && !mtk_crtc->enabled) {
		DDPINFO("CRTC%d skip path switch %u->%u, enable:%d\n",
			index, mtk_crtc->ddp_mode,
			ddp_mode, mtk_crtc->enabled);
		CRTC_MMP_MARK(index, path_switch, 0, 1);
		goto done2;
	}

	mtk_drm_crtc_wait_blank(mtk_crtc);

	DDPINFO("%s crtc%d path switch(%d->%d)\n", __func__, index,
		mtk_crtc->ddp_mode, ddp_mode);
	mtk_drm_idlemgr_kick(__func__, crtc, 0);
	CRTC_MMP_MARK(index, path_switch, 1, 0);

	/* 0. Special NO_USE ddp mode control. In NO_USE ddp mode, the HW path
	 *     is disabled currently or to be disabled. So the control use the
	 *     CRTC enable/disable function to create/destroy path.
	 */
	if (ddp_mode == DDP_NO_USE) {
		CRTC_MMP_MARK(index, path_switch, 0, 2);
		if ((mtk_crtc->ddp_mode == DDP_MAJOR) && (index == 2))
			need_wait = false;
		else
			need_wait = true;
		mtk_drm_crtc_disable(crtc, need_wait);
		goto done;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		CRTC_MMP_MARK(index, path_switch, 0, 3);
		mtk_crtc->ddp_mode = ddp_mode;
		mtk_drm_crtc_enable(crtc);
		goto done;
	}

	mtk_crtc_path_switch_prepare(crtc, ddp_mode, &cfg);

	/* 1 Destroy original sub path */
	__mtk_crtc_old_sub_path_destroy(crtc, &cfg);

	/* 2 Composing planes and write back to buffer */
	__mtk_crtc_composition_wb(crtc, ddp_mode, &cfg);

	/* 3. Composing planes and write back to buffer */
	__mtk_crtc_prim_path_switch(crtc, ddp_mode, &cfg);
	CRTC_MMP_MARK(index, path_switch, 1, 1);

	/* 4. Create new sub path */
	__mtk_crtc_sub_path_create(crtc, ddp_mode, &cfg);

	/* 5. create and destroy the fb used in ddp */
	mtk_crtc_dc_fb_control(crtc, ddp_mode);

done:
	mtk_crtc->ddp_mode = ddp_mode;
	if (crtc->enabled && mtk_crtc->ddp_mode != DDP_NO_USE)
		mtk_crtc_update_ddp_sw_status(crtc, true);

	if (need_lock)
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	CRTC_MMP_EVENT_END(index, path_switch, crtc->enabled,
			need_lock);

	DDPINFO("%s:%d -\n", __func__, __LINE__);
	return 0;

done2:
	if (need_lock)
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	CRTC_MMP_EVENT_END(index, path_switch, crtc->enabled,
			need_lock);

	DDPINFO("%s:%d -\n", __func__, __LINE__);
	return 0;
}

int mtk_crtc_get_mutex_id(struct drm_crtc *crtc, unsigned int ddp_mode,
			  enum mtk_ddp_comp_id find_comp)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int crtc_id = drm_crtc_index(crtc);
	int i, j;
	struct mtk_ddp_comp *comp;
	int find = -1;
	bool has_connector = true;

	i = j = 0;
	for_each_comp_in_target_ddp_mode(comp, mtk_crtc, i, j,
					 ddp_mode)
		if (comp->id == find_comp) {
			find = i;
			break;
		}

	if (find == -1 && mtk_crtc->is_dual_pipe) {
		i = j = 0;
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
			if (comp->id == find_comp) {
				find = i;
				break;
			}
	}

	if (find == -1) {
		DDPPR_ERR("component %d is not found in path %d of crtc %d\n",
			  find_comp, ddp_mode, crtc_id);
		return -1;
	}

	if (mtk_crtc_with_sub_path(crtc, ddp_mode) && find == 0)
		has_connector = false;

	switch (crtc_id) {
	case 0:
		if (has_connector)
			return 0;
		else
			return 1;
	case 1:
		if (has_connector)
			return 2;
		else
			return 3;
	case 2:
		if (has_connector)
			return 4;
		else
			return 5;
	default:
		DDPPR_ERR("not define mutex id in crtc %d\n", crtc_id);
	}

	return -1;
}

void mtk_crtc_disconnect_path_between_component(struct drm_crtc *crtc,
						unsigned int ddp_mode,
						enum mtk_ddp_comp_id prev,
						enum mtk_ddp_comp_id next,
						struct cmdq_pkt *cmdq_handle)
{
	int i, j;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int mutex_id = mtk_crtc_get_mutex_id(crtc, ddp_mode, prev);
	struct mtk_crtc_ddp_ctx *ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	int find_idx = -1;

	if (mutex_id < 0) {
		DDPPR_ERR("invalid mutex id:%d\n", mutex_id);
		return;
	}

	for_each_comp_in_target_ddp_mode(comp, mtk_crtc, i, j, ddp_mode) {
		if (comp->id == prev)
			find_idx = j;

		if (find_idx > -1 && j > find_idx) {
			mtk_ddp_remove_comp_from_path_with_cmdq(
				mtk_crtc, ddp_ctx->ddp_comp[i][j - 1]->id,
				comp->id, cmdq_handle);

			if (comp->id != next)
				mtk_disp_mutex_remove_comp_with_cmdq(
					mtk_crtc, comp->id, cmdq_handle,
					mutex_id);
		}

		if (comp->id == next)
			return;
	}


	if (!mtk_crtc->is_dual_pipe)
		return;

	i = j = 0;
	ddp_ctx = &mtk_crtc->dual_pipe_ddp_ctx;
	for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
		if (comp->id == prev)
			find_idx = j;

		if (find_idx > -1 && j > find_idx) {
			mtk_ddp_remove_comp_from_path_with_cmdq(
					mtk_crtc, ddp_ctx->ddp_comp[i][j - 1]->id,
					comp->id, cmdq_handle);

			if (comp->id != next)
				mtk_disp_mutex_remove_comp_with_cmdq(
						mtk_crtc, comp->id, cmdq_handle,
						mutex_id);
		}

		if (comp->id == next)
			return;
	}
}

void mtk_crtc_connect_path_between_component(struct drm_crtc *crtc,
					     unsigned int ddp_mode,
					     enum mtk_ddp_comp_id prev,
					     enum mtk_ddp_comp_id next,
					     struct cmdq_pkt *cmdq_handle)
{
	int i, j;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int mutex_id = mtk_crtc_get_mutex_id(crtc, ddp_mode, prev);
	struct mtk_crtc_ddp_ctx *ddp_ctx = &mtk_crtc->ddp_ctx[ddp_mode];
	int find_idx = -1;

	if (mutex_id < 0) {
		DDPPR_ERR("invalid mutex id:%d\n", mutex_id);
		return;
	}

	for_each_comp_in_target_ddp_mode(comp, mtk_crtc, i, j, ddp_mode) {
		if (comp->id == prev)
			find_idx = j;

		if (find_idx > -1 && j > find_idx) {
			mtk_ddp_add_comp_to_path_with_cmdq(
				mtk_crtc, ddp_ctx->ddp_comp[i][j - 1]->id,
				comp->id, cmdq_handle);

			if (comp->id != next)
				mtk_disp_mutex_add_comp_with_cmdq(
					mtk_crtc, comp->id,
					mtk_crtc_is_frame_trigger_mode(crtc),
					cmdq_handle, mutex_id);
		}

		if (comp->id == next)
			return;
	}

	if (!mtk_crtc->is_dual_pipe)
		return;

	i = j = 0;
	ddp_ctx = &mtk_crtc->dual_pipe_ddp_ctx;
	for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
		if (comp->id == prev)
			find_idx = j;

		if (find_idx > -1 && j > find_idx) {
			mtk_ddp_add_comp_to_path_with_cmdq(
					mtk_crtc, ddp_ctx->ddp_comp[i][j - 1]->id,
					comp->id, cmdq_handle);

			if (comp->id != next)
				mtk_disp_mutex_add_comp_with_cmdq(
						mtk_crtc, comp->id,
						mtk_crtc_is_frame_trigger_mode(crtc),
						cmdq_handle, mutex_id);
		}

		if (comp->id == next)
			return;
	}
}

int mtk_crtc_find_comp(struct drm_crtc *crtc, unsigned int ddp_mode,
		       enum mtk_ddp_comp_id comp_id)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	int i, j;

	for_each_comp_in_target_ddp_mode(
		comp, mtk_crtc, i, j,
		ddp_mode)
		if (comp->id == comp_id &&
			mtk_ddp_comp_get_type(comp_id) !=
			MTK_DISP_VIRTUAL)
			return comp->id;

	if (!mtk_crtc->is_dual_pipe)
		return -1;

	i = j = 0;
	for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j)
		if (comp->id == comp_id &&
				mtk_ddp_comp_get_type(comp_id) !=
				MTK_DISP_VIRTUAL)
			return comp->id;

	return -1;
}

int mtk_crtc_find_next_comp(struct drm_crtc *crtc, unsigned int ddp_mode,
			    enum mtk_ddp_comp_id comp_id)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	int id;
	int i, j, k;

	for_each_comp_in_target_ddp_mode(comp, mtk_crtc, i, j, ddp_mode) {
		if (comp->id == comp_id) {
			struct mtk_crtc_ddp_ctx *ddp_ctx =
				&mtk_crtc->ddp_ctx[ddp_mode];

			for (k = j + 1; k < ddp_ctx->ddp_comp_nr[i]; k++) {
				id = ddp_ctx->ddp_comp[i][k]->id;
				if (mtk_ddp_comp_get_type(id) !=
				    MTK_DISP_VIRTUAL)
					return ddp_ctx->ddp_comp[i][k]->id;
			}

			return -1;
		}
	}

	if (!mtk_crtc->is_dual_pipe)
		return -1;

	i = j = 0;
	for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
		if (comp->id == comp_id) {
			struct mtk_crtc_ddp_ctx *ddp_ctx =
				&mtk_crtc->dual_pipe_ddp_ctx;

			for (k = j + 1; k < ddp_ctx->ddp_comp_nr[i]; k++) {
				id = ddp_ctx->ddp_comp[i][k]->id;
				if (mtk_ddp_comp_get_type(id) !=
						MTK_DISP_VIRTUAL)
					return ddp_ctx->ddp_comp[i][k]->id;
			}

			return -1;
		}
	}

	return -1;
}

int mtk_crtc_find_prev_comp(struct drm_crtc *crtc, unsigned int ddp_mode,
			    enum mtk_ddp_comp_id comp_id)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	int real_comp_id = -1;
	int real_comp_path_idx = -1;
	int i, j;

	for_each_comp_in_target_ddp_mode(comp, mtk_crtc, i, j, ddp_mode) {
		if (comp->id == comp_id) {
			if (i == real_comp_path_idx)
				return real_comp_id;
			else
				return -1;
		}

		if (mtk_ddp_comp_get_type(comp->id) != MTK_DISP_VIRTUAL) {
			real_comp_id = comp->id;
			real_comp_path_idx = i;
		}
	}

	if (!mtk_crtc->is_dual_pipe)
		return -1;

	i = j = 0;
	real_comp_id = -1;
	real_comp_path_idx = -1;
	for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
		if (comp->id == comp_id) {
			if (i == real_comp_path_idx)
				return real_comp_id;
			else
				return -1;
		}

		if (mtk_ddp_comp_get_type(comp->id) != MTK_DISP_VIRTUAL) {
			real_comp_id = comp->id;
			real_comp_path_idx = i;
		}
	}

	return -1;
}

int mtk_crtc_mipi_freq_switch(struct drm_crtc *crtc, unsigned int en,
			unsigned int userdata)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct mtk_panel_ext *ext = mtk_crtc->panel_ext;

	if (mtk_crtc->mipi_hopping_sta == en)
		return 0;

	if (!(ext && ext->params &&
			ext->params->dyn.switch_en == 1))
		return 0;

	DDPMSG("%s, userdata=%d, en=%d\n", __func__, userdata, en);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	mtk_crtc->mipi_hopping_sta = en;


	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp) {
		DDPPR_ERR("request output fail\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return -EINVAL;
	}

	mtk_ddp_comp_io_cmd(comp,
			NULL, MIPI_HOPPING, &en);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

char *mtk_crtc_index_spy(int crtc_index)
{
	switch (crtc_index) {
	case 0:
		return "P";
	case 1:
		return "E";
	case 2:
		return "M";
	default:
		return "Unknown";
	}
}

int mtk_crtc_osc_freq_switch(struct drm_crtc *crtc, unsigned int en,
			unsigned int userdata)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct mtk_panel_ext *ext = mtk_crtc->panel_ext;

	if (mtk_crtc->panel_osc_hopping_sta == en)
		return 0;

	if (!(ext && ext->params))
		return 0;

	DDPMSG("%s, userdata=%d, en=%d\n", __func__, userdata, en);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	mtk_crtc->panel_osc_hopping_sta = en;

	if (!mtk_crtc->enabled)
		goto done;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp) {
		DDPPR_ERR("request output fail\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return -EINVAL;
	}

	/* Following section is for customization */
	/* Start */
	/* e.g. lmtk_ddp_comp_io_cmd(comp,
	 *	NULL, PANEL_OSC_HOPPING, &en);
	 */
	/* End */

done:
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

int mtk_crtc_enter_tui(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int hrt_idx;
	int i;

	DDPMSG("%s\n", __func__);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	mtk_crtc->crtc_blank = true;
	mtk_disp_esd_check_switch(crtc, 0);

	mtk_drm_set_idlemgr(crtc, 0, 0);

	hrt_idx = _layering_rule_get_hrt_idx();
	hrt_idx++;
	atomic_set(&priv->rollback_all, 1);
	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, crtc->dev);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	/* TODO: Potential risk, display suspend after release lock */
	for (i = 0; i < 60; ++i) {
		usleep_range(16667, 17000);
		if (atomic_read(&mtk_crtc->qos_ctx->last_hrt_idx) >=
			hrt_idx)
			break;
	}
	if (i >= 60)
		DDPPR_ERR("wait repaint %d\n", i);

	DDP_MUTEX_LOCK(&mtk_crtc->blank_lock, __func__, __LINE__);

	/* TODO: HardCode select OVL0, maybe store in platform data */
	priv->ddp_comp[DDP_COMPONENT_OVL0]->blank_mode = true;

	DDP_MUTEX_UNLOCK(&mtk_crtc->blank_lock, __func__, __LINE__);

	wake_up(&mtk_crtc->state_wait_queue);

	return 0;
}

int mtk_crtc_exit_tui(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	DDPMSG("%s\n", __func__);

	DDP_MUTEX_LOCK(&mtk_crtc->blank_lock, __func__, __LINE__);

	/* TODO: Hard Code select OVL0, maybe store in platform data */
	priv->ddp_comp[DDP_COMPONENT_OVL0]->blank_mode = false;

	mtk_crtc->crtc_blank = false;

	atomic_set(&priv->rollback_all, 0);

	DDP_MUTEX_UNLOCK(&mtk_crtc->blank_lock, __func__, __LINE__);

	wake_up(&mtk_crtc->state_wait_queue);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	mtk_drm_set_idlemgr(crtc, 1, 0);

	mtk_disp_esd_check_switch(crtc, 1);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

/********************** Legacy DISP API ****************************/
unsigned int DISP_GetScreenWidth(void)
{
	if (!pgc) {
		DDPPR_ERR("LCM is not registered.\n");
		return 0;
	}

	return pgc->mode.hdisplay;
}

unsigned int DISP_GetScreenHeight(void)
{
	if (!pgc) {
		DDPPR_ERR("LCM is not registered.\n");
		return 0;
	}

	return pgc->mode.vdisplay;
}

int mtk_crtc_lcm_ATA(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_panel_params *panel_ext =
		mtk_drm_get_lcm_ext_params(crtc);
	int ret = 0;

	DDPINFO("%s\n", __func__);

	mtk_disp_esd_check_switch(crtc, 0);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		ret = -EINVAL;
		goto out;
	}
	if (unlikely(!panel_ext)) {
		DDPPR_ERR("%s:invalid panel_ext\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	DDPINFO("[ATA_LCM]primary display path stop[begin]\n");
	if (!mtk_crtc_is_frame_trigger_mode(crtc)) {
		mtk_crtc_pkt_create(&cmdq_handle, crtc,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
		mtk_ddp_comp_io_cmd(output_comp,
			cmdq_handle, DSI_STOP_VDO_MODE, NULL);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}
	DDPINFO("[ATA_LCM]primary display path stop[end]\n");

	mtk_ddp_comp_io_cmd(output_comp, NULL, LCM_ATA_CHECK, &ret);

	if (!mtk_crtc_is_frame_trigger_mode(crtc)) {
		mtk_crtc_pkt_create(&cmdq_handle, crtc,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
		mtk_ddp_comp_io_cmd(output_comp,
			cmdq_handle, DSI_START_VDO_MODE, NULL);
		mtk_disp_mutex_trigger(mtk_crtc->mutex[0], cmdq_handle);
		mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, COMP_REG_START,
				    NULL);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}
out:
	mtk_disp_esd_check_switch(crtc, 1);

	return ret;
}
unsigned int mtk_drm_primary_display_get_debug_state(
	struct mtk_drm_private *priv, char *stringbuf, int buf_len)
{
	int len = 0;

	struct drm_crtc *crtc = priv->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	struct mtk_ddp_comp *comp;
	char *panel_name;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!comp))
		DDPPR_ERR("%s:invalid output comp\n", __func__);

	mtk_ddp_comp_io_cmd(comp, NULL, GET_PANEL_NAME,
				    &panel_name);

	len += scnprintf(stringbuf + len, buf_len - len,
			 "==========    Primary Display Info    ==========\n");

	len += scnprintf(stringbuf + len, buf_len - len,
			 "LCM Driver=[%s] Resolution=%ux%u, Connected:%s\n",
			  panel_name, crtc->state->adjusted_mode.hdisplay,
			  crtc->state->adjusted_mode.vdisplay,
			  (mtk_drm_lcm_is_connect() ? "Y" : "N"));

	len += scnprintf(stringbuf + len, buf_len - len,
			 "FPS = %d, display mode idx = %u, %s mode\n",
			 crtc->state->adjusted_mode.vrefresh,
			 mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX],
			 (mtk_crtc_is_frame_trigger_mode(crtc) ?
			  "cmd" : "vdo"));

	len += scnprintf(stringbuf + len, buf_len - len,
		"================================================\n\n");

	return len;
}

int MMPathTraceCrtcPlanes(struct drm_crtc *crtc,
	char *str, int strlen, int n)
{
	unsigned int i = 0, addr;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct mtk_plane_state *state =
		    to_mtk_plane_state(mtk_crtc->planes[i].base.state);
		struct mtk_plane_pending_state *pending = &state->pending;

		if (pending->enable == 0)
			continue;

		addr = (unsigned int)pending->addr;

		n += scnprintf(str + n, strlen - n, "in_%d=0x%x, ",
			i, addr);
		n += scnprintf(str + n, strlen - n, "in_%d_width=%u, ",
			i, pending->width);
		n += scnprintf(str + n, strlen - n, "in_%d_height=%u, ",
			i, pending->height);
		n += scnprintf(str + n, strlen - n, "in_%d_fmt=%s, ",
			i, mtk_get_format_name(pending->format));
		n += scnprintf(str + n, strlen - n, "in_%d_bpp=%u, ",
			i, mtk_get_format_bpp(pending->format));
		n += scnprintf(str + n, strlen - n, "in_%d_compr=%u, ",
			i, pending->prop_val[PLANE_PROP_COMPRESS]);
	}

	return n;
}

/* ************   Panel Master   **************** */

void mtk_crtc_start_for_pm(struct drm_crtc *crtc)
{
	int i, j;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct cmdq_pkt *cmdq_handle;
	/* start trig loop */
	if (mtk_crtc_with_trigger_loop(crtc))
		mtk_crtc_start_trig_loop(crtc);
	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	/*if VDO mode start DSI MODE */
	if (!mtk_crtc_is_frame_trigger_mode(crtc) &&
		mtk_crtc_is_connector_enable(mtk_crtc)) {
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
			mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				DSI_START_VDO_MODE, NULL);
		}
	}
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		mtk_ddp_comp_start(comp, cmdq_handle);
	}
	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

}

void mtk_crtc_stop_for_pm(struct mtk_drm_crtc *mtk_crtc, bool need_wait)
{
	struct cmdq_pkt *cmdq_handle;

	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	struct drm_crtc *crtc = &mtk_crtc->base;

	DDPINFO("%s:%d +\n", __func__, __LINE__);

	/* 0. Waiting CLIENT_DSI_CFG thread done */
	if (crtc_id == 0) {
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);

	if (!need_wait)
		goto skip;

	if (crtc_id == 2) {
		cmdq_pkt_wait_no_clear(cmdq_handle,
				 mtk_crtc->gce_obj.event[EVENT_WDMA0_EOF]);
	} else if (mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base)) {
		/* 1. wait stream eof & clear tocken */
		/* clear eof token to prevent any config after this command */
		cmdq_pkt_wfe(cmdq_handle,
				 mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);

		/* clear dirty token to prevent trigger loop start */
		cmdq_pkt_clear_event(
			cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
	} else if (mtk_crtc_is_connector_enable(mtk_crtc)) {
		/* In vdo mode, DSI would be stop when disable connector
		 * Do not wait frame done in this case.
		 */
		cmdq_pkt_wfe(cmdq_handle,
				 mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);
	}

skip:
	/* 2. stop all modules in this CRTC */
	mtk_crtc_stop_ddp(mtk_crtc, cmdq_handle);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	/* 3. stop trig loop  */
	if (mtk_crtc_with_trigger_loop(crtc)) {
		mtk_crtc_stop_trig_loop(crtc);
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833)
		if (mtk_crtc_with_sodi_loop(crtc) &&
				(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_sodi_loop(crtc);
#endif
	}

	DDPINFO("%s:%d -\n", __func__, __LINE__);
}
/* ***********  Panel Master end ************** */

bool mtk_drm_get_hdr_property(void)
{
	return hdr_en;
}

