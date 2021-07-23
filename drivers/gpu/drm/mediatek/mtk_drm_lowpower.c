/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <drm/mediatek_drm.h>
#include "mtk_drm_lowpower.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_mmp.h"

#define MAX_ENTER_IDLE_RSZ_RATIO 250

#ifdef CONFIG_DRM_DFPS
#define MAX_TOUCH_FPS_BRIGHTNESS 486
#define MAX_SET_TOUCH_EVENT_TIME 300

static struct drm_device *drm_dev;
#endif

static void mtk_drm_idlemgr_enable_crtc(struct drm_crtc *crtc);
static void mtk_drm_idlemgr_disable_crtc(struct drm_crtc *crtc);

#ifdef CONFIG_DRM_DFPS
static void mtk_touch_up_event_delay_work(struct work_struct *work)
{
	struct mtk_drm_idlemgr *idlemgr = container_of(work, struct mtk_drm_idlemgr,
					touch_up_event_delay_work.work);

	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_state *mtk_state;
	struct mtk_ddp_comp *output_comp;
	int src_idx;

	if(!drm_dev) {
		DDPPR_ERR("idlemgr is not ready!\n");
		return;
	}

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	mtk_state = to_mtk_crtc_state(crtc->state);
	src_idx = mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	idlemgr = mtk_crtc->idlemgr;
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!output_comp || !idlemgr->idlemgr_ctx->dfps_en)
		return;

	if (!mtk_drm_is_idle_fps(crtc)) {
		mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_SET_IDLE_FPS, &src_idx);
		idlemgr->idlemgr_ctx->is_idle_fps = 1;
		DDPINFO("%s finish touch_up_event_delay_work\n", __func__);
	}
}

void mtk_drm_crtc_touch_notify(void)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_state *mtk_state;
	struct mtk_drm_idlemgr *idlemgr;
	struct mtk_ddp_comp *output_comp;
	int src_idx;

	if(!drm_dev) {
		DDPPR_ERR("idlemgr is not ready!\n");
		return;
	}

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
				typeof(*crtc), head);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	mtk_state = to_mtk_crtc_state(crtc->state);
	src_idx = mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	idlemgr = mtk_crtc->idlemgr;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!output_comp || !idlemgr->idlemgr_ctx->dfps_en) {
		return;

		DDPINFO("%s: src_idx = %d\n", __func__, src_idx);

		if (mtk_drm_is_idle_fps(crtc)) {
			mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_SET_NON_IDLE_FPS, &src_idx);
			idlemgr->idlemgr_ctx->is_idle_fps = 0;
		}
        }
}
EXPORT_SYMBOL(mtk_drm_crtc_touch_notify);

bool mtk_drm_idlemgr_enable_dfps(struct drm_crtc *crtc, bool en)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	bool old_en = 0;

	if (!(mtk_crtc && mtk_crtc->idlemgr && mtk_crtc->idlemgr->idlemgr_ctx))
		return 0;

	old_en = mtk_crtc->idlemgr->idlemgr_ctx->dfps_en;
	mtk_crtc->idlemgr->idlemgr_ctx->dfps_en = en;

	return old_en;
}
#endif

static void mtk_drm_vdo_mode_enter_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i, j;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct mtk_ddp_comp *comp;

	mtk_crtc_pkt_create(&handle, crtc, client);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_BY_REPAINT) &&
	    atomic_read(&state->plane_enabled_num) > 1) {
		atomic_set(&priv->idle_need_repaint, 1);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, crtc->dev);
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
		mtk_disp_mutex_inten_disable_cmdq(mtk_crtc->mutex[0], handle);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, handle, IRQ_LEVEL_IDLE, NULL);
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp)
		mtk_ddp_comp_io_cmd(comp, handle, DSI_VFP_IDLE_MODE, NULL);

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
	lcm_fps_ctx_reset(crtc);
}

static void mtk_drm_cmd_mode_enter_idle(struct drm_crtc *crtc)
{
	mtk_drm_idlemgr_disable_crtc(crtc);
	lcm_fps_ctx_reset(crtc);
}

static void mtk_drm_vdo_mode_leave_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i, j;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct mtk_ddp_comp *comp;

	mtk_crtc_pkt_create(&handle, crtc, client);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
		mtk_disp_mutex_inten_enable_cmdq(mtk_crtc->mutex[0], handle);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, handle, IRQ_LEVEL_ALL, NULL);
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp)
		mtk_ddp_comp_io_cmd(comp, handle, DSI_VFP_DEFAULT_MODE, NULL);

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
	lcm_fps_ctx_reset(crtc);
}

static void mtk_drm_cmd_mode_leave_idle(struct drm_crtc *crtc)
{
	mtk_drm_idlemgr_enable_crtc(crtc);
	lcm_fps_ctx_reset(crtc);
}

static void mtk_drm_idlemgr_enter_idle_nolock(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *output_comp;
	int index = drm_crtc_index(crtc);

	bool mode;

	output_comp = priv->ddp_comp[DDP_COMPONENT_DSI0];

	if (!output_comp)
		return;

	mode = mtk_dsi_is_cmd_mode(output_comp);
	CRTC_MMP_EVENT_START(index, enter_idle, mode, 0);

	if (mode) {
		mtk_drm_cmd_mode_enter_idle(crtc);
	} else {
		mtk_drm_vdo_mode_enter_idle(crtc);
	}

	CRTC_MMP_EVENT_END(index, enter_idle, mode, 0);
}

static void mtk_drm_idlemgr_leave_idle_nolock(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *output_comp;
	int index = drm_crtc_index(crtc);
	bool mode;

	output_comp = priv->ddp_comp[DDP_COMPONENT_DSI0];

	if (!output_comp)
		return;

	mode = mtk_dsi_is_cmd_mode(output_comp);
	CRTC_MMP_EVENT_START(index, leave_idle, mode, 0);

	if (mode) {
		mtk_drm_cmd_mode_leave_idle(crtc);
	} else {
		mtk_drm_vdo_mode_leave_idle(crtc);
	}

	CRTC_MMP_EVENT_END(index, leave_idle, mode, 0);
}

bool mtk_drm_is_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;

	if (!idlemgr)
		return false;

	return idlemgr->idlemgr_ctx->is_idle;
}

#ifdef CONFIG_DRM_DFPS
bool mtk_drm_is_idle_fps(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;

	if (!idlemgr)
		return false;

	return idlemgr->idlemgr_ctx->is_idle_fps;
}
#endif

void mtk_drm_idlemgr_kick(const char *source, struct drm_crtc *crtc,
			  int need_lock)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx;

	if (!mtk_crtc->idlemgr)
		return;
	idlemgr = mtk_crtc->idlemgr;
	idlemgr_ctx = idlemgr->idlemgr_ctx;

	/* get lock to protect idlemgr_last_kick_time and is_idle */
	if (need_lock)
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	/* update kick timestamp */
	idlemgr_ctx->idlemgr_last_kick_time = sched_clock();

	if (idlemgr_ctx->is_idle) {
		DDPINFO("[LP] kick idle from [%s]\n", source);
		mtk_drm_idlemgr_leave_idle_nolock(crtc);
		idlemgr_ctx->is_idle = 0;

		/* wake up idlemgr process to monitor next idle state */
		wake_up_interruptible(&idlemgr->idlemgr_wq);
	}

	if (need_lock)
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

unsigned int mtk_drm_set_idlemgr(struct drm_crtc *crtc, unsigned int flag,
				 bool need_lock)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	unsigned int old_flag;

	if (!idlemgr)
		return 0;

	old_flag = atomic_read(&idlemgr->idlemgr_task_active);

	if (flag) {
		DDPINFO("[LP] enable idlemgr\n");
		atomic_set(&idlemgr->idlemgr_task_active, 1);
		wake_up_interruptible(&idlemgr->idlemgr_wq);
	} else {
		DDPINFO("[LP] disable idlemgr\n");
		atomic_set(&idlemgr->idlemgr_task_active, 0);
		mtk_drm_idlemgr_kick(__func__, crtc, need_lock);
	}

	return old_flag;
}

unsigned long long
mtk_drm_set_idle_check_interval(struct drm_crtc *crtc,
				unsigned long long new_interval)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned long long old_interval = 0;

	if (!(mtk_crtc && mtk_crtc->idlemgr && mtk_crtc->idlemgr->idlemgr_ctx))
		return 0;

	old_interval = mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval;
	mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval = new_interval;

	return old_interval;
}

unsigned long long
mtk_drm_get_idle_check_interval(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!(mtk_crtc && mtk_crtc->idlemgr && mtk_crtc->idlemgr->idlemgr_ctx))
		return 0;

	return mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval;
}

static int mtk_drm_idlemgr_get_rsz_ratio(struct mtk_crtc_state *state)
{
	int src_w = state->rsz_src_roi.width;
	int src_h = state->rsz_src_roi.height;
	int dst_w = state->rsz_dst_roi.width;
	int dst_h = state->rsz_dst_roi.height;
	int ratio_w, ratio_h;

	if (src_w == 0 || src_h == 0)
		return 100;

	ratio_w = dst_w * 100 / src_w;
	ratio_h = dst_h * 100 / src_h;

	return ((ratio_w > ratio_h) ? ratio_w : ratio_h);
}

static bool is_yuv(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		return true;
	default:
		break;
	}

	return false;
}

static bool mtk_planes_is_yuv_fmt(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state =
			to_mtk_plane_state(plane->state);
		struct mtk_plane_pending_state *pending = &plane_state->pending;
		unsigned int fmt = pending->format;

		if (pending->enable && is_yuv(fmt))
			return true;
	}

	return false;
}

static int mtk_drm_idlemgr_monitor_thread(void *data)
{
	int ret = 0;
	long long t_to_check = 0;
	unsigned long long t_idle;
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *mtk_state = NULL;
	struct drm_vblank_crtc *vblank = NULL;
	int crtc_id = drm_crtc_index(crtc);
	static unsigned long long idlemgr_vblank_check_internal;

	msleep(16000);
	while (1) {
		ret = wait_event_interruptible(
			idlemgr->idlemgr_wq,
			atomic_read(&idlemgr->idlemgr_task_active));

		t_idle = local_clock() - idlemgr_ctx->idlemgr_last_kick_time;
		if (idlemgr_vblank_check_internal)
			t_to_check = idlemgr_vblank_check_internal *
				1000 * 1000 - t_idle;
		else
			t_to_check = idlemgr_ctx->idle_check_interval *
				1000 * 1000 - t_idle;
		do_div(t_to_check, 1000000);

		t_to_check = min(t_to_check, 1000LL);
		/* when starting up before the first time kick */
		if (idlemgr_ctx->idlemgr_last_kick_time == 0)
			msleep_interruptible(idlemgr_ctx->idle_check_interval);
		else if (t_to_check > 0)
			msleep_interruptible(t_to_check);

		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

		if (!mtk_crtc->enabled) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			mtk_crtc_wait_status(crtc, 1, MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		if (crtc->state) {
			mtk_state = to_mtk_crtc_state(crtc->state);
			if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__,
						__LINE__);
				continue;
			}
			/* do not enter VDO idle when rsz ratio >= 2.5;
			 * And When layer fmt is YUV in VP scenario, it
			 * will flicker into idle repaint, so let it not
			 * into idle repaint as workaround.
			 */
			if (mtk_crtc_is_frame_trigger_mode(crtc) == 0 &&
				((mtk_drm_idlemgr_get_rsz_ratio(mtk_state) >=
				MAX_ENTER_IDLE_RSZ_RATIO) ||
				mtk_planes_is_yuv_fmt(crtc))) {
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__,
						__LINE__);
				continue;
			}
		}

		if (idlemgr_ctx->is_idle
			|| mtk_crtc_is_dc_mode(crtc)
			|| priv->session_mode != MTK_DRM_SESSION_DL
			|| mtk_crtc->sec_on) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}

		t_idle = local_clock() - idlemgr_ctx->idlemgr_last_kick_time;
		if ((idlemgr_vblank_check_internal &&
		    t_idle < idlemgr_vblank_check_internal * 1000 * 1000) ||
		    (!idlemgr_vblank_check_internal &&
		    t_idle < idlemgr_ctx->idle_check_interval * 1000 * 1000)) {
			/* kicked in idle_check_interval msec, it's not idle */
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}
		/* double check if dynamic switch on/off */
		if (atomic_read(&idlemgr->idlemgr_task_active)) {
			DDPINFO("[LP] enter idle\n");
			crtc_id = drm_crtc_index(crtc);
			vblank = &crtc->dev->vblank[crtc_id];

			/* enter idle state */
			if (!vblank || atomic_read(&vblank->refcount) == 0) {
				mtk_drm_idlemgr_enter_idle_nolock(crtc);
				idlemgr_ctx->is_idle = 1;
				idlemgr_vblank_check_internal = 0;
			} else {
				idlemgr_ctx->idlemgr_last_kick_time =
					sched_clock();
				idlemgr_vblank_check_internal = 10;
			}
		}

		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		wait_event_interruptible(idlemgr->idlemgr_wq,
					 !idlemgr_ctx->is_idle);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

int mtk_drm_idlemgr_init(struct drm_crtc *crtc, int index)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr =
		kzalloc(sizeof(struct mtk_drm_idlemgr), GFP_KERNEL);
	struct mtk_drm_idlemgr_context *idlemgr_ctx =
		kzalloc(sizeof(struct mtk_drm_idlemgr_context), GFP_KERNEL);
	const int len = 50;
	char name[len];

	if (!idlemgr) {
		DDPPR_ERR("struct mtk_drm_idlemgr allocate fail\n");
		return -ENOMEM;
		;
	}

	if (!idlemgr_ctx) {

		DDPPR_ERR("struct mtk_drm_idlemgr_context allocate fail\n");
		return -ENOMEM;
	}

	idlemgr->idlemgr_ctx = idlemgr_ctx;
	mtk_crtc->idlemgr = idlemgr;

	idlemgr_ctx->session_mode_before_enter_idle = MTK_DRM_SESSION_INVALID;
	idlemgr_ctx->is_idle = 0;
	idlemgr_ctx->is_idle_fps = 0;
#ifdef CONFIG_DRM_DFPS
	idlemgr_ctx->dfps_en = 1;
#endif
	idlemgr_ctx->enterulps = 0;
	idlemgr_ctx->idlemgr_last_kick_time = ~(0ULL);
	idlemgr_ctx->cur_lp_cust_mode = 0;
	idlemgr_ctx->idle_check_interval = 50;

	snprintf(name, len, "mtk_drm_disp_idlemgr-%d", index);
	idlemgr->idlemgr_task =
		kthread_create(mtk_drm_idlemgr_monitor_thread, crtc, name);
	init_waitqueue_head(&idlemgr->idlemgr_wq);
	atomic_set(&idlemgr->idlemgr_task_active, 1);

#ifdef CONFIG_DRM_DFPS
	INIT_DELAYED_WORK(&idlemgr->touch_up_event_delay_work, mtk_touch_up_event_delay_work);
	drm_dev = crtc->dev;
#endif

	wake_up_process(idlemgr->idlemgr_task);

	return 0;
}

static void mtk_drm_idlemgr_disable_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_DISABLE, NULL);
}

static void mtk_drm_idlemgr_enable_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_ENABLE, NULL);
}

static void mtk_drm_idlemgr_disable_crtc(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	bool mode = mtk_crtc_is_dc_mode(crtc);

#ifdef CONFIG_DRM_DFPS
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	int src_idx = mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_ddp_comp *output_comp;
#endif
	DDPINFO("%s, crtc%d+\n", __func__, crtc_id);

	if (mode) {
		DDPINFO("crtc%d mode:%d bypass enter idle\n", crtc_id, mode);
		DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
		return;
	}

#ifdef CONFIG_DRM_DFPS
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (src_idx && output_comp && idlemgr->idlemgr_ctx->dfps_en
		&& mtk_show_brightness_clone(output_comp) > MAX_TOUCH_FPS_BRIGHTNESS) {
		if (!mtk_drm_is_idle_fps(crtc)) {
			schedule_delayed_work(&idlemgr->touch_up_event_delay_work,
				msecs_to_jiffies(MAX_SET_TOUCH_EVENT_TIME));
			DDPINFO("start touch_up_event_delay_work");
		}
	}
#endif

	/* 1. stop CRTC */
	mtk_crtc_stop(mtk_crtc, true);

	/* 2. disconnect addon module and recover config */
	mtk_crtc_disconnect_addon_module(crtc);

	/* 3. set HRT BW to 0 */
#ifdef MTK_FB_MMDVFS_SUPPORT
	mtk_disp_set_hrt_bw(mtk_crtc, 0);
#endif

	/* 4. disconnect path */
	mtk_crtc_disconnect_default_path(mtk_crtc);

	/* 5. power off all modules in this CRTC */
	mtk_crtc_ddp_unprepare(mtk_crtc);
	mtk_drm_idlemgr_disable_connector(crtc);

	drm_crtc_vblank_off(crtc);

	mtk_crtc_vblank_irq(&mtk_crtc->base);
	/* 6. power off MTCMOS */
	mtk_drm_top_clk_disable_unprepare(crtc->dev);

	/* 7. disable fake vsync if need */
	mtk_drm_fake_vsync_switch(crtc, false);

	DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
}

/* TODO: we should restore the current setting rather than default setting */
static void mtk_drm_idlemgr_enable_crtc(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(crtc);
	bool mode = mtk_crtc_is_dc_mode(crtc);
	struct mtk_ddp_comp *comp;
	unsigned int i, j;

#ifdef CONFIG_DRM_DFPS
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	int src_idx = mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_ddp_comp *output_comp;
#endif

	DDPINFO("crtc%d do %s+\n", crtc_id, __func__);

	if (mode) {
		DDPINFO("crtc%d mode:%d bypass exit idle\n", crtc_id, mode);
		DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
		return;
	}

	/* 1. power on mtcmos */
	mtk_drm_top_clk_prepare_enable(crtc->dev);

	/* 2. prepare modules would be used in this CRTC */
	mtk_drm_idlemgr_enable_connector(crtc);
	mtk_crtc_ddp_prepare(mtk_crtc);

	/* 3. start trigger loop first to keep gce alive */
	if (crtc_id == 0) {
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
		if (!mtk_crtc_is_frame_trigger_mode(crtc))
			mtk_crtc_start_sodi_loop(crtc);
#endif
		mtk_crtc_start_trig_loop(crtc);
		mtk_crtc_hw_block_ready(crtc);
	}

	/* 4. connect path */
	mtk_crtc_connect_default_path(mtk_crtc);

	/* 5. config ddp engine & set dirty for cmd mode */
	mtk_crtc_config_default_path(mtk_crtc);

#ifdef CONFIG_DRM_DFPS
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (src_idx && output_comp && idlemgr->idlemgr_ctx->dfps_en
		&& mtk_show_brightness_clone(output_comp) > MAX_TOUCH_FPS_BRIGHTNESS) {
		if (!mtk_drm_is_idle_fps(crtc)) {
			cancel_delayed_work(&idlemgr->touch_up_event_delay_work);
			DDPINFO("cancel touch_up_event_delay_work");
		}
	}
#endif

	/* 6. conect addon module and config */
	mtk_crtc_connect_addon_module(crtc);

	/* 7. restore OVL setting */
	mtk_crtc_restore_plane_setting(mtk_crtc);

	/* 8. Set QOS BW */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_BW, NULL);

	/* 9. restore HRT BW */
#ifdef MTK_FB_MMDVFS_SUPPORT
	mtk_disp_set_hrt_bw(mtk_crtc, mtk_crtc->qos_ctx->last_hrt_req);
#endif

	/* 10. set vblank */
	drm_crtc_vblank_on(crtc);

	/* 11. enable fake vsync if need */
	mtk_drm_fake_vsync_switch(crtc, true);

	DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
}
