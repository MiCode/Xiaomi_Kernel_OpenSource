/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DISP_LHBM_H_
#define _MI_DISP_LHBM_H_

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/atomic.h>

#include <drm/drm_panel.h>
#include "mi_dsi_panel.h"
#include "mi_drm_crtc.h"

static inline const char *get_fingerprint_status_name(int status)
{
	switch (status) {
	case FINGERPRINT_NONE:
		return "none";
	case ENROLL_START:
		return "enroll_start";
	case ENROLL_STOP:
		return "enroll_stop";
	case AUTH_START:
		return "authenticate_start";
	case AUTH_STOP:
		return "authenticate_stop";
	case HEART_RATE_START:
		return "heart_rate_start";
	case HEART_RATE_STOP:
		return "heart_rate_stop";
	default:
		return "Unknown";
	}
}

enum {
	FOD_WORK_DONE = 0,
	FOD_WORK_DOING = 1,
};

enum {
	FOD_EVENT_UP = 0,
	FOD_EVENT_DOWN = 1,
	FOD_EVENT_MAX
};

enum mi_panel_op_code {
	MI_FOD_HBM_ON = 0,
	MI_FOD_HBM_OFF,
	MI_FOD_AOD_TO_NORMAL,
	MI_FOD_NORMAL_TO_AOD,
};

enum lhbm_target_brightness_state {
	LHBM_TARGET_BRIGHTNESS_NONE,
	LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT,
	LHBM_TARGET_BRIGHTNESS_WHITE_110NIT,
	LHBM_TARGET_BRIGHTNESS_GREEN_500NIT,
	LHBM_TARGET_BRIGHTNESS_MAX
};

enum fod_ui_ready_state {
	LOCAL_HBM_UI_NONE = 0,
	GLOBAL_FOD_HBM_OVERLAY = BIT(0),
	GLOBAL_FOD_ICON = BIT(1),
	FOD_LOW_BRIGHTNESS_CAPTURE = BIT(2),
	LOCAL_HBM_UI_READY  = BIT(3)
};

struct disp_lhbm_fod {
	struct mtk_dsi *dsi;

	struct kthread_worker fod_worker;
	struct task_struct *fod_thread;
	wait_queue_head_t fod_pending_wq;
	atomic_t pending_fod_cnt;
	struct kthread_work thread_priority_work;

	struct list_head work_list;
	struct mutex mutex_lock;
	spinlock_t spinlock;

	atomic_t allow_tx_lhbm;
	atomic_t fingerprint_status;

	atomic_t fod_work_status;
	atomic_t current_fod_status;
	atomic_t unset_fod_status;
	atomic_t unset_from_touch;

	struct mi_layer_flags layer_flags;

	int target_brightness;
};

struct lhbm_fod_work {
	struct disp_lhbm_fod *lhbm_fod;
	struct mtk_dsi * dsi;

	struct kthread_work work;
	wait_queue_head_t *wq;

	bool from_touch;
	int fod_status;

	struct list_head node;
};

bool is_local_hbm(int disp_id);
bool mi_disp_lhbm_fod_enabled(struct mtk_dsi *dsi);
int mi_disp_lhbm_fod_thread_create(struct disp_feature *df, int disp_id);
int mi_disp_lhbm_fod_thread_destroy(struct disp_feature *df, int disp_id);
struct disp_lhbm_fod *mi_get_disp_lhbm_fod(int disp_id);
int mi_disp_lhbm_fod_set_fingerprint_status(struct mtk_dsi *dsi,
		int fingerprint_status);
int mi_disp_lhbm_fod_wakeup_pending_work(struct mtk_dsi *dsi);
int mi_disp_lhbm_fod_allow_tx_lhbm(struct mtk_dsi *dsi, bool enable);
int mi_disp_lhbm_fod_update_layer_state(struct mtk_dsi *dsi,
		struct mi_layer_flags flags);
void mi_disp_lhbm_animal_status_update(struct mtk_dsi *dsi,
		bool is_layer_exit);
int mi_disp_set_fod_queue_work(u32 fod_status, bool from_touch);
#endif /* _MI_DISP_LHBM_H_ */