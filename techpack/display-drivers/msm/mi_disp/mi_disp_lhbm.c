/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
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

static int mi_disp_lhbm_fod_thread_fn(void *arg);

bool is_local_hbm(int disp_id)
{
	struct dsi_display *display = NULL;

	if (is_support_disp_id(disp_id)) {
		if (disp_id == MI_DISP_PRIMARY)
			display = mi_get_primary_dsi_display();
		else
			display = mi_get_secondary_dsi_display();

		if (display && display->panel)
			return display->panel->mi_cfg.local_hbm_enabled;
		else
			return false;
	} else {
		DISP_ERROR("unknown display id\n");
		return false;
	}
}

bool mi_disp_lhbm_fod_enabled(struct dsi_panel *panel)
{
	return panel ? panel->mi_cfg.local_hbm_enabled : false;
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

	INIT_LIST_HEAD(&lhbm_fod->event_list);
	spin_lock_init(&lhbm_fod->spinlock);

	atomic_set(&lhbm_fod->allow_tx_lhbm, 0);
	atomic_set(&lhbm_fod->fingerprint_status, FINGERPRINT_NONE);
	atomic_set(&lhbm_fod->last_fod_status, FOD_EVENT_MAX);

	init_waitqueue_head(&lhbm_fod->fod_pending_wq);

	lhbm_fod->fod_thread = kthread_run(mi_disp_lhbm_fod_thread_fn,
			lhbm_fod, "disp_lhbm_fod:%d", disp_id);
	if (IS_ERR(lhbm_fod->fod_thread)) {
		DISP_ERROR("failed to create disp_fod:%d kthread\n", disp_id);
		ret = PTR_ERR(lhbm_fod->fod_thread);
		lhbm_fod->fod_thread = NULL;
		goto error;
	}
	/* set realtime priority */
	sched_set_fifo(lhbm_fod->fod_thread);

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
	if (is_support_disp_id(disp_id)) {
		return g_lhbm_fod[disp_id];
	} else {
		DISP_ERROR("unknown display id\n");
		return NULL;
	}
}

int mi_disp_lhbm_fod_allow_tx_lhbm(struct dsi_display *display,
		bool enable)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;
	unsigned long flags;

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
		DISP_INFO("%s display allow_tx_lhbm = %d\n", display->display_type, enable);
		if (enable) {
			/* If the last fod fingerprint status is that the finger is pressed
			 * and the finger is not lifted when display off,
			 * the display is turned on again, will restor the local hbm on */
			spin_lock_irqsave(&lhbm_fod->spinlock, flags);
			if (atomic_read(&lhbm_fod->fingerprint_status) == AUTH_START &&
				atomic_read(&lhbm_fod->last_fod_status) == FOD_EVENT_DOWN &&
				list_empty(&lhbm_fod->event_list)) {
				spin_unlock_irqrestore(&lhbm_fod->spinlock, flags);
				mi_disp_lhbm_fod_set_finger_event(
					mi_get_disp_id(display->display_type),
					FOD_EVENT_DOWN, false);
			} else {
				spin_unlock_irqrestore(&lhbm_fod->spinlock, flags);
				wake_up_interruptible(&lhbm_fod->fod_pending_wq);
				DISP_INFO("%s display wake up local disp_fod kthread\n",
					display->display_type);
			}
		}
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

	DISP_TIME_INFO("set fingerprint_status = %s\n",
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

static int mi_disp_lhbm_fod_event_notify(struct disp_lhbm_fod *lhbm_fod, int fod_status)
{
	struct dsi_display *display = NULL;
	int disp_id = MI_DISP_PRIMARY;
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

	if (atomic_read(&lhbm_fod->allow_tx_lhbm)) {
		disp_id = mi_get_disp_id(display->display_type);
		mi_disp_feature_event_notify_by_type(disp_id, MI_DISP_EVENT_FOD,
				sizeof(fod_ui_ready), fod_ui_ready);

		DISP_INFO("%s display fod_ui_ready notify=%d\n",
			display->display_type, fod_ui_ready);
	}

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
			brightness_clone = atomic_read(&mi_cfg->real_brightness_clone);
			if (brightness_clone < mi_cfg->fod_low_brightness_clone_threshold
				&& (mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] <=
					mi_cfg->fod_low_brightness_lux_threshold)) {
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

	rc = mi_dsi_display_set_disp_param(lhbm_fod->display, &ctl);

	return rc;
}

int mi_disp_lhbm_aod_to_normal_optimize(struct dsi_display *display,
		bool enable)
{
	struct disp_feature_ctl ctl;
	int rc = 0;

	if (!display || !display->panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->mi_cfg.need_fod_animal_in_normal)
		return 0;

	memset(&ctl, 0, sizeof(struct disp_feature_ctl));
	ctl.feature_id = DISP_FEATURE_AOD_TO_NORMAL;
	ctl.feature_val = enable ? FEATURE_ON : FEATURE_OFF;

	rc = mi_dsi_display_set_disp_param(display, &ctl);

	return rc;
}

static bool mi_disp_lhbm_fod_thread_should_wake(struct disp_lhbm_fod *lhbm_fod)
{
	bool should_wake = false;
	unsigned long flags;

	spin_lock_irqsave(&lhbm_fod->spinlock, flags);

	if (list_empty(&lhbm_fod->event_list) || !atomic_read(&lhbm_fod->allow_tx_lhbm))
		should_wake = false;
	else
		should_wake = true;

	spin_unlock_irqrestore(&lhbm_fod->spinlock, flags);

	return should_wake;
}

static int mi_disp_lhbm_fod_thread_fn(void *arg)
{
	int rc = 0;
	struct disp_lhbm_fod *lhbm_fod = (struct disp_lhbm_fod *)arg;
	struct lhbm_fod_event *entry = NULL, *temp = NULL;
	struct lhbm_fod_event fod_event;
	unsigned long flag = 0;
	u32 op_code;

	while (!kthread_should_stop()) {
		rc = wait_event_interruptible(lhbm_fod->fod_pending_wq,
				mi_disp_lhbm_fod_thread_should_wake(lhbm_fod));
		if (rc) {
			/* Some event woke us up */
			DISP_WARN("wait_event_interruptible rc = %d\n", rc);
			continue;
		}

		spin_lock_irqsave(&lhbm_fod->spinlock, flag);
		entry = list_last_entry(&lhbm_fod->event_list, struct lhbm_fod_event, link);
		DISP_INFO("from_touch(%d), fod_status(%d)\n", entry->from_touch, entry->fod_status);
		memcpy(&fod_event, entry, sizeof(fod_event));
		list_for_each_entry_safe(entry, temp, &lhbm_fod->event_list, link) {
			DISP_DEBUG("in list, from_touch(%d), fod_status(%d)\n",
					entry->from_touch, entry->fod_status);
			list_del(&entry->link);
			kfree(entry);
		}
		if (atomic_read(&lhbm_fod->last_fod_status) != fod_event.fod_status ||
			(!fod_event.from_touch && fod_event.fod_status == FOD_EVENT_DOWN)) {
			atomic_set(&lhbm_fod->last_fod_status, fod_event.fod_status);
			if (fod_event.fod_status == FOD_EVENT_DOWN) {
				op_code = MI_FOD_HBM_ON;
			} else {
				op_code = MI_FOD_HBM_OFF;
			}
			spin_unlock_irqrestore(&lhbm_fod->spinlock, flag);

			if (fod_event.fod_status == FOD_EVENT_UP) {
				mi_disp_lhbm_fod_event_notify(lhbm_fod, FOD_EVENT_UP);
			}

			rc = mi_disp_lhbm_fod_set_disp_param(lhbm_fod, op_code);
			if (rc) {
				DISP_ERROR("lhbm_fod failed to set_disp_param, rc = %d\n", rc);
			} else if (fod_event.fod_status == FOD_EVENT_DOWN) {
				mi_disp_lhbm_fod_event_notify(lhbm_fod, FOD_EVENT_DOWN);
			}
		} else{
			spin_unlock_irqrestore(&lhbm_fod->spinlock, flag);
			DISP_INFO("same fod event: %d, return\n", fod_event.fod_status);
		}
	}

	return 0;
}

int mi_disp_lhbm_fod_set_finger_event(int disp_id, u32 fod_status, bool from_touch)
{
	struct disp_lhbm_fod *lhbm_fod = mi_get_disp_lhbm_fod(disp_id);
	struct lhbm_fod_event *fod_event = NULL, *entry = NULL;
	unsigned long flags;
	int fingerprint_status = FINGERPRINT_NONE;
	int rc = 0;

#ifdef DISPLAY_FACTORY_BUILD
	return 0;
#endif

	if (!is_local_hbm(disp_id)) {
		DISP_DEBUG("%s panel is not local hbm\n", get_disp_id_name(disp_id));
		return 0;
	}

	if (!lhbm_fod) {
		DISP_ERROR("invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&lhbm_fod->spinlock, flags);

	fingerprint_status = atomic_read(&lhbm_fod->fingerprint_status);
	if (fingerprint_status == ENROLL_STOP ||
		fingerprint_status == AUTH_STOP ||
		fingerprint_status == HEART_RATE_STOP) {
		if (fod_status == FOD_EVENT_DOWN) {
			DISP_ERROR("fingerprint_status[%s], skip FOD_EVENT_DOWN\n",
				get_fingerprint_status_name(fingerprint_status));
			goto exit;
		}
	}

	fod_event = kzalloc(sizeof(struct lhbm_fod_event), GFP_ATOMIC);
	if (!fod_event) {
		DISP_ERROR("failed to allocate memory for fod_event\n");
		rc = ENOMEM;
		goto exit;
	}

	fod_event->from_touch = from_touch;
	fod_event->fod_status = fod_status;
	INIT_LIST_HEAD(&fod_event->link);
	list_add_tail(&fod_event->link, &lhbm_fod->event_list);

	list_for_each_entry(entry, &lhbm_fod->event_list, link) {
		DISP_DEBUG("in list, from_touch(%d), fod_status(%d)\n",
				entry->from_touch, entry->fod_status);
	}

	DISP_TIME_INFO("from_touch(%d), fod_status(%d)\n", from_touch, fod_status);
	wake_up_interruptible(&lhbm_fod->fod_pending_wq);

exit:
	spin_unlock_irqrestore(&lhbm_fod->spinlock, flags);
	return rc;
}
EXPORT_SYMBOL_GPL(mi_disp_lhbm_fod_set_finger_event);
