/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-disp-lhbm:[%s:%d] " fmt, __func__, __LINE__
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>

#include "sde_trace.h"
#include "dsi_display.h"

#include "mi_disp_feature.h"
#include "mi_disp_print.h"
#include "mi_disp_lhbm.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"

static struct disp_lhbm_fod *g_lhbm_fod[MI_DISP_MAX];

bool is_local_hbm(int disp_id)
{
	struct dsi_display *display = NULL;

	if (disp_id == MI_DISP_PRIMARY)
		display = mi_get_primary_dsi_display();
	else
		display = mi_get_secondary_dsi_display();

	if (display && display->panel)
		return display->panel->mi_cfg.local_hbm_enabled;
	else
		return false;
}

bool mi_disp_lhbm_fod_enabled(struct dsi_panel *panel)
{
	return panel ? panel->mi_cfg.local_hbm_enabled : false;
}

static void mi_disp_lhbm_fod_thread_priority_worker(struct kthread_work *work)
{
	int ret = 0;
	struct sched_param param = { 0 };
	struct task_struct *task = current->group_leader;

	/**
	 * this priority was found during empiric testing to have appropriate
	 * realtime scheduling to process display updates and interact with
	 * other real time and normal priority task
	 */
	param.sched_priority = 16;
	ret = sched_setscheduler(task, SCHED_FIFO, &param);
	if (ret)
		pr_warn("pid:%d name:%s priority update failed: %d\n",
			current->tgid, task->comm, ret);
}

int mi_disp_lhbm_fod_thread_create(struct disp_feature *df, int disp_id)
{
	int ret = 0;
	struct dsi_display *display = NULL;
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!df || !is_support_disp_id(disp_id)) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!df->d_display[disp_id].display ||
		df->d_display[disp_id].intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported display(%s intf)\n",
			get_disp_intf_type_name(df->d_display[disp_id].intf_type));
		return -EINVAL;
	}

	display = (struct dsi_display *)df->d_display[disp_id].display;
	if (!mi_disp_lhbm_fod_enabled(display->panel)) {
		DISP_INFO("%s panel is not local hbm\n", get_disp_id_name(disp_id));
		return 0;
	}

	lhbm_fod = kzalloc(sizeof(struct disp_lhbm_fod), GFP_KERNEL);
	if (!lhbm_fod) {
		DISP_ERROR("can not allocate buffer for disp_lhbm\n");
		return -ENOMEM;
	}

	df->d_display[disp_id].lhbm_fod_ptr = lhbm_fod;
	lhbm_fod->display = display;

	kthread_init_worker(&lhbm_fod->fod_worker);
	kthread_init_work(&lhbm_fod->thread_priority_work, mi_disp_lhbm_fod_thread_priority_worker);
	lhbm_fod->fod_thread = kthread_run(kthread_worker_fn,
			&lhbm_fod->fod_worker, "disp_lhbm_fod:%d", disp_id);
	kthread_queue_work(&lhbm_fod->fod_worker, &lhbm_fod->thread_priority_work);
	if (IS_ERR(lhbm_fod->fod_thread)) {
		DISP_ERROR("failed to create disp_fod:%d kthread\n", disp_id);
		ret = PTR_ERR(lhbm_fod->fod_thread);
		lhbm_fod->fod_thread = NULL;
		goto error;
	}

	init_waitqueue_head(&lhbm_fod->fod_pending_wq);

	INIT_LIST_HEAD(&lhbm_fod->work_list);
	mutex_init(&lhbm_fod->mutex_lock);
	spin_lock_init(&lhbm_fod->spinlock);

	atomic_set(&lhbm_fod->allow_tx_lhbm, 0);
	atomic_set(&lhbm_fod->fingerprint_status, FINGERPRINT_NONE);
	atomic_set(&lhbm_fod->fod_work_status, FOD_WORK_DONE);
	atomic_set(&lhbm_fod->current_fod_status, FOD_EVENT_MAX);
	atomic_set(&lhbm_fod->unset_fod_status, FOD_EVENT_MAX);
	atomic_set(&lhbm_fod->unset_from_touch, 0);

	g_lhbm_fod[disp_id] = lhbm_fod;

	DISP_INFO("create disp_lhbm_fod:%d kthread success\n", disp_id);

	return ret;

error:
	kfree(lhbm_fod);
	return ret;
}

int mi_disp_lhbm_fod_thread_destroy(struct disp_feature *df, int disp_id)
{
	int ret = 0;
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!df || !is_support_disp_id(disp_id)) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	lhbm_fod = df->d_display[disp_id].lhbm_fod_ptr;
	if (lhbm_fod) {
		if (lhbm_fod->fod_thread) {
			kthread_flush_worker(&lhbm_fod->fod_worker);
			kthread_stop(lhbm_fod->fod_thread);
			lhbm_fod->fod_thread = NULL;
		}
		kfree(lhbm_fod);
	}

	df->d_display[disp_id].lhbm_fod_ptr = NULL;
	g_lhbm_fod[disp_id] = NULL;

	DISP_INFO("destroy disp_lhbm_fod:%d kthread success\n", disp_id);

	return ret;
}

struct disp_lhbm_fod *mi_get_disp_lhbm_fod(int disp_id)
{
	if (is_support_disp_id(disp_id))
		return g_lhbm_fod[disp_id];
	else
		return NULL;
}

int mi_disp_lhbm_fod_allow_tx_lhbm(struct dsi_display *display,
		bool enable)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(display->panel)) {
		DISP_DEBUG("%s panel is not local hbm\n", display->display_type);
		return 0;
	}

	lhbm_fod = mi_get_disp_lhbm_fod(mi_get_disp_id(display->display_type));
	if (!lhbm_fod) {
		DISP_ERROR("Invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	if (lhbm_fod->display == display &&
		atomic_read(&lhbm_fod->allow_tx_lhbm) != enable) {
		atomic_set(&lhbm_fod->allow_tx_lhbm, enable);
		DISP_INFO("%s display allow_tx_lhbm = %d\n",
			display->display_type, enable);
	}
	return 0;
}

int mi_disp_lhbm_fod_set_fingerprint_status(struct dsi_panel *panel,
		int fingerprint_status)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!panel) {
		DISP_ERROR("Invalid panel ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(panel)) {
		DISP_DEBUG("%s panel is not local hbm\n", panel->type);
		return 0;
	}

	lhbm_fod = mi_get_disp_lhbm_fod(mi_get_disp_id(panel->type));
	if (!lhbm_fod) {
		DISP_ERROR("Invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	atomic_set(&lhbm_fod->fingerprint_status, fingerprint_status);

	DISP_UTC_INFO("set fingerprint_status = %s\n",
		get_fingerprint_status_name(fingerprint_status));

	return 0;
}

int mi_disp_lhbm_fod_update_layer_state(struct dsi_display *display,
		struct mi_layer_flags flags)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(display->panel)) {
		DISP_DEBUG("%s panel is not local hbm\n", display->display_type);
		return 0;
	}

	lhbm_fod = mi_get_disp_lhbm_fod(mi_get_disp_id(display->display_type));
	if (!lhbm_fod) {
		DISP_ERROR("Invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	spin_lock(&lhbm_fod->spinlock);
	lhbm_fod->layer_flags = flags;
	spin_unlock(&lhbm_fod->spinlock);

	return 0;
}

int mi_disp_lhbm_fod_wakeup_pending_work(struct dsi_display *display)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}
	
	if (!mi_disp_lhbm_fod_enabled(display->panel)) {
		DISP_DEBUG("%s panel is not local hbm\n", display->display_type);
		return 0;
	}

	lhbm_fod = mi_get_disp_lhbm_fod(mi_get_disp_id(display->display_type));
	if (!lhbm_fod) {
		DISP_ERROR("Invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	if (lhbm_fod->display == display) {
		DISP_DEBUG("%s pending_fod_cnt = %d\n",
			display->display_type, atomic_read(&lhbm_fod->pending_fod_cnt));
		if (atomic_read(&lhbm_fod->pending_fod_cnt)) {
			DISP_INFO("%s display wake up local hbm fod work\n",
				display->display_type);
			wake_up_interruptible(&lhbm_fod->fod_pending_wq);
		}
	}

	return 0;
}

static int mi_disp_lhbm_fod_event_notify(struct disp_lhbm_fod *lhbm_fod, int fod_status)
{
	struct dsi_display *display = NULL;
	struct disp_event d_event;
	u32 fod_ui_ready = 0, refresh_rate = 0;
	u32 ui_ready_delay_frame = 0;
	u64 delay_us = 0;

	if (!lhbm_fod || !lhbm_fod->display) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	display = lhbm_fod->display;

	if (!display->panel || !display->panel->cur_mode){
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (fod_status == FOD_EVENT_DOWN &&
		display->panel->mi_cfg.lhbm_ui_ready_delay_frame > 0) {
		refresh_rate = display->panel->cur_mode->timing.refresh_rate;
		ui_ready_delay_frame = display->panel->mi_cfg.lhbm_ui_ready_delay_frame;
		delay_us = 1000000 / refresh_rate * ui_ready_delay_frame;
		DISP_INFO("refresh_rate(%d), delay (%d) frame, delay_us(%llu)\n",
				refresh_rate, ui_ready_delay_frame, delay_us);
		usleep_range(delay_us, delay_us + 10);
	}

	if (fod_status == FOD_EVENT_DOWN) {
		if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_WHITE_110NIT)
			fod_ui_ready = LOCAL_HBM_UI_READY | FOD_LOW_BRIGHTNESS_CAPTURE;
		else
			fod_ui_ready = LOCAL_HBM_UI_READY;
	} else {
		fod_ui_ready = LOCAL_HBM_UI_NONE;
	}

	d_event.disp_id = mi_get_disp_id(display->display_type);
	d_event.type = MI_DISP_EVENT_FOD;
	d_event.length = sizeof(fod_ui_ready);
	mi_disp_feature_event_notify(&d_event, (u8 *)&fod_ui_ready);

	DISP_INFO("%s display fod_ui_ready notify=%d\n",
		display->display_type, fod_ui_ready);

	return 0;
}

static int mi_disp_lhbm_fod_get_target_brightness(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int brightness_clone = 0;
	int target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	/* Heart rate detection, local hbm green 500nit */
	if (mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == HEART_RATE_START) {
		target_brightness = LHBM_TARGET_BRIGHTNESS_GREEN_500NIT;
		return target_brightness;
	}

	/* fingerprint enroll, local hbm white 1000nit */
	if (mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == ENROLL_START) {
		target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;
		return target_brightness;
	}

	/* fingerprint unlock, check low brightness status */
	if (mi_cfg->feature_val[DISP_FEATURE_LOW_BRIGHTNESS_FOD] &&
		mi_cfg->fod_low_brightness_allow) {
		if (is_aod_and_panel_initialized(panel)) {
			if (mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] <=
				mi_cfg->fod_low_brightness_lux_threshold)
				target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_110NIT;
		} else {
			brightness_clone = mi_cfg->real_brightness_clone;
			if (brightness_clone < mi_cfg->fod_low_brightness_clone_threshold
				&& (mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] <=
					mi_cfg->fod_low_brightness_lux_threshold + 10)) {
				target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_110NIT;
			}
		}
	} else {
		target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;
	}

	return target_brightness;
}

static int mi_disp_lhbm_fod_set_disp_param(struct disp_lhbm_fod *lhbm_fod, u32 op_code)
{
	struct dsi_panel *panel = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct disp_feature_ctl ctl;
	int rc = 0;

	if (!lhbm_fod || !lhbm_fod->display || !lhbm_fod->display->panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	memset(&ctl, 0, sizeof(struct disp_feature_ctl));
	panel = lhbm_fod->display->panel;

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;

	switch (op_code) {
	case MI_FOD_HBM_ON:
		ctl.feature_id = DISP_FEATURE_LOCAL_HBM;
		lhbm_fod->target_brightness = mi_disp_lhbm_fod_get_target_brightness(panel);
		if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_GREEN_500NIT) {
			ctl.feature_val = LOCAL_HBM_NORMAL_GREEN_500NIT;
		} else if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT) {
			if (is_aod_and_panel_initialized(panel) &&
				(mi_cfg->panel_state == PANEL_STATE_DOZE_HIGH
				||mi_cfg->panel_state == PANEL_STATE_DOZE_LOW))
				ctl.feature_val = LOCAL_HBM_HLPM_WHITE_1000NIT;
			else
				ctl.feature_val = LOCAL_HBM_NORMAL_WHITE_1000NIT;
		} else if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_WHITE_110NIT) {
			if (is_aod_and_panel_initialized(panel) &&
				(mi_cfg->panel_state == PANEL_STATE_DOZE_HIGH
				||mi_cfg->panel_state == PANEL_STATE_DOZE_LOW))
				ctl.feature_val = LOCAL_HBM_HLPM_WHITE_110NIT;
			else
				ctl.feature_val = LOCAL_HBM_NORMAL_WHITE_110NIT;
		} else {
			DISP_ERROR("invalid target_brightness = %d\n", lhbm_fod->target_brightness);
		}
		break;
	case MI_FOD_HBM_OFF:
		ctl.feature_id = DISP_FEATURE_LOCAL_HBM;
		if (mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == AUTH_STOP) {
				if (is_aod_and_panel_initialized(panel))
					ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE;
				 else
					ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL;
		} else if (is_aod_and_panel_initialized(panel)) {
			ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT;
		} else {
			ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL;
		}
		break;
	default:
		break;
	}

	mutex_unlock(&panel->panel_lock);

	SDE_ATRACE_BEGIN("local_hbm_fod");
	rc = mi_dsi_display_set_disp_param(lhbm_fod->display, &ctl);
	SDE_ATRACE_BEGIN("local_hbm_fod");

	return rc;
}

void mi_disp_lhbm_animal_status_update(struct dsi_display *display,
		bool is_layer_exit)
{
	struct disp_feature_ctl ctl;

	if (!display->panel->mi_cfg.need_fod_animal_in_normal)
		return;

	ctl.feature_id = DISP_FEATURE_AOD_TO_NORMAL;
	if (is_layer_exit)
		ctl.feature_val = FEATURE_ON;
	else
		ctl.feature_val = FEATURE_OFF;

	mi_dsi_display_set_disp_param(display, &ctl);
}

static void mi_disp_lhbm_fod_work_handler(struct kthread_work *work)
{
	int rc = 0;
	struct lhbm_fod_work *fod_work = container_of(work, struct lhbm_fod_work, work);
	struct disp_lhbm_fod *lhbm_fod = fod_work->lhbm_fod;
	int fod_status = FOD_EVENT_MAX;
	u32 op_code;

	DISP_INFO("fod_work_handler --- start\n");

	mutex_lock(&lhbm_fod->mutex_lock);
	atomic_set(&lhbm_fod->fod_work_status, FOD_WORK_DOING);

	DISP_INFO("from_touch(%d), fod_status(%d)\n",
			fod_work->from_touch, fod_work->fod_status);

	spin_lock(&lhbm_fod->spinlock);
	if (!atomic_read(&lhbm_fod->allow_tx_lhbm)) {
		atomic_inc(&lhbm_fod->pending_fod_cnt);
		spin_unlock(&lhbm_fod->spinlock);
		mutex_unlock(&lhbm_fod->mutex_lock);

		DISP_INFO("wait until sde connector bridge enable\n");
		rc = wait_event_interruptible(lhbm_fod->fod_pending_wq,
				atomic_read(&lhbm_fod->allow_tx_lhbm));
		if (rc) {
			/* Some event woke us up, so let's quit */
			DISP_INFO("wait_event_interruptible rc = %d\n", rc);
			spin_lock(&lhbm_fod->spinlock);
			atomic_add_unless(&lhbm_fod->pending_fod_cnt, -1, 0);
			atomic_set(&lhbm_fod->fod_work_status, FOD_WORK_DONE);
			list_del_init(&fod_work->node);
			spin_unlock(&lhbm_fod->spinlock);
			goto exit;
		}

		mutex_lock(&lhbm_fod->mutex_lock);
		spin_lock(&lhbm_fod->spinlock);
		atomic_add_unless(&lhbm_fod->pending_fod_cnt, -1, 0);
		/* only update newest status when panel is off */
		if (atomic_read(&lhbm_fod->unset_fod_status) != FOD_EVENT_MAX) {
			if (atomic_read(&lhbm_fod->current_fod_status) !=
				atomic_read(&lhbm_fod->unset_fod_status)) {
				DISP_INFO("unset_fod_status = %d, fod_status = %d\n",
					atomic_read(&lhbm_fod->unset_fod_status),
					fod_work->fod_status);
				fod_work->fod_status = atomic_read(&lhbm_fod->unset_fod_status);
				atomic_set(&lhbm_fod->unset_fod_status, FOD_EVENT_MAX);
				atomic_set(&lhbm_fod->unset_from_touch, 0);
			} else {
				DISP_INFO("current/unset_fod_status = %d, fod_status = %d, skip\n",
					atomic_read(&lhbm_fod->unset_fod_status),
					fod_work->fod_status);
				atomic_set(&lhbm_fod->unset_fod_status, FOD_EVENT_MAX);
				atomic_set(&lhbm_fod->unset_from_touch, 0);
				list_del_init(&fod_work->node);
				spin_unlock(&lhbm_fod->spinlock);
				mutex_unlock(&lhbm_fod->mutex_lock);
				goto exit;
			}
		}
	}

	atomic_set(&lhbm_fod->current_fod_status, fod_work->fod_status);
	list_del_init(&fod_work->node);
	if (atomic_read(&lhbm_fod->current_fod_status) == FOD_EVENT_DOWN) {
		op_code = MI_FOD_HBM_ON;
		fod_status = FOD_EVENT_DOWN;
	} else {
		op_code = MI_FOD_HBM_OFF;
		fod_status = FOD_EVENT_UP;
	}
	spin_unlock(&lhbm_fod->spinlock);

	if (fod_status == FOD_EVENT_UP) {
		mi_disp_lhbm_fod_event_notify(lhbm_fod, FOD_EVENT_UP);
	}

	rc = mi_disp_lhbm_fod_set_disp_param(lhbm_fod, op_code);
	if (rc) {
		DISP_ERROR("lhbm_fod failed to set_disp_param, rc = %d\n", rc);
	} else if (fod_status == FOD_EVENT_DOWN) {
		mi_disp_lhbm_fod_event_notify(lhbm_fod, FOD_EVENT_DOWN);
	}

	atomic_set(&lhbm_fod->fod_work_status, FOD_WORK_DONE);
	mutex_unlock(&lhbm_fod->mutex_lock);

	DISP_INFO("fod_work_handler --- end\n");

exit:
	kfree(fod_work);
}

int mi_disp_set_fod_queue_work(u32 fod_status, bool from_touch)
{
	struct disp_lhbm_fod *lhbm_fod = mi_get_disp_lhbm_fod(MI_DISP_PRIMARY);
	struct dsi_display *display = mi_get_primary_dsi_display();
	struct lhbm_fod_work *fod_work = NULL, *entry = NULL;
	unsigned long flags;
	int rc = 0;

#ifdef DISPLAY_FACTORY_BUILD
	return 0;
#endif

	if (!display) {
		DISP_ERROR("invalid display ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(display->panel)) {
		DISP_DEBUG("%s panel is not local hbm\n", display->display_type);
		return 0;
	}

	if (!lhbm_fod || lhbm_fod->display != display) {
		DISP_ERROR("invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	DISP_INFO("start: from_touch(%d), fod_status(%d)\n", from_touch, fod_status);

	spin_lock_irqsave(&lhbm_fod->spinlock, flags);
	if (atomic_read(&lhbm_fod->pending_fod_cnt)) {
		DISP_INFO("pending event: current_fod_status(%d), new fod_status(%d), skip\n",
			atomic_read(&lhbm_fod->current_fod_status), fod_status);
		atomic_set(&lhbm_fod->unset_fod_status, fod_status);
		atomic_set(&lhbm_fod->unset_from_touch, from_touch);
		goto exit;
	} else {
		if (!list_empty(&lhbm_fod->work_list)) {
			entry = list_last_entry(&lhbm_fod->work_list,
					struct lhbm_fod_work, node);
			if (entry->fod_status == fod_status &&
				entry->from_touch == from_touch) {
				DISP_INFO("same event: equal to the last member in list,"
					"from_touch(%d), fod_status(%d), skip\n",
					from_touch, fod_status);
				goto exit;
			}
		}
	}

	fod_work = kzalloc(sizeof(struct lhbm_fod_work), GFP_ATOMIC);
	if (!fod_work) {
		DISP_ERROR("failed to allocate memory\n");
		rc = ENOMEM;
		goto exit;
	}

	fod_work->lhbm_fod = lhbm_fod;
	fod_work->dsi_display = display;
	kthread_init_work(&fod_work->work, mi_disp_lhbm_fod_work_handler);
	fod_work->wq = &lhbm_fod->fod_pending_wq;
	fod_work->from_touch = from_touch;
	fod_work->fod_status = fod_status;
	INIT_LIST_HEAD(&fod_work->node);
	list_add_tail(&fod_work->node, &lhbm_fod->work_list);

	list_for_each_entry(entry, &lhbm_fod->work_list, node) {
		DISP_DEBUG("in list, from_touch(%d), fod_status(%d)\n",
			entry->from_touch, entry->fod_status);
	}

	DISP_UTC_INFO("queue_work: from_touch(%d), fod_status(%d)\n", from_touch, fod_status);
	kthread_queue_work(&lhbm_fod->fod_worker, &fod_work->work);

exit:
	spin_unlock_irqrestore(&lhbm_fod->spinlock, flags);
	return rc;
}
EXPORT_SYMBOL_GPL(mi_disp_set_fod_queue_work);
