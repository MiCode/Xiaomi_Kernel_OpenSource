// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DISP_FEATURE_H_
#define _MI_DISP_FEATURE_H_

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/input.h>

#include "mi_disp_config.h"
#include <uapi/drm/mi_disp.h>
#include "mtk_panel_ext.h"

#define DISP_FEATURE_DEVICE_NAME "disp_feature"

#define to_disp_feature(d) dev_get_drvdata(d)

enum disp_intf_type {
	MI_INTF_DSI = 0,
	MI_INTF_DP = 1,
	MI_INTF_HDMI = 2,
	MI_INTF_MAX,
};

enum dsi_display_selection_type {
	DSI_PRIMARY = 0,
	DSI_SECONDARY,
	MAX_DSI_ACTIVE_DISPLAY,
};

struct disp_work {
	struct kthread_work work;
	struct kthread_delayed_work delayed_work;
	struct disp_display *dd_ptr;
	wait_queue_head_t *wq;
	void *data;
};

struct disp_thread {
	struct disp_feature_dev *feature;
	struct task_struct *thread;
	struct kthread_worker worker;
	struct disp_display *dd_ptr;
};

struct disp_display {
	struct device *dev;
	void *display;
	int intf_type;
	struct mutex mutex_lock;
	struct disp_thread d_thread;
	wait_queue_head_t pending_wq;
	atomic_t pending_doze_cnt;
#ifdef CONFIG_MI_DISP_INPUT_HANDLER
	struct input_handler *input_handler;
	struct kthread_work input_event_work;
#endif
	struct disp_lhbm_fod *lhbm_fod_ptr;
};

struct disp_feature {
	u32 version;
	dev_t dev_id;
	struct class *class;
	struct cdev *cdev;
	struct device *pdev;
	struct dentry *debug;
	struct disp_display d_display[MI_DISP_MAX];
	//struct mtk_dsi *dsi_display[MAX_DSI_ACTIVE_DISPLAY];
	//struct disp_thread disp_thread[MAX_DSI_ACTIVE_DISPLAY];
	//struct mutex mutex_lock[MAX_DSI_ACTIVE_DISPLAY];
	struct list_head client_list;
	spinlock_t client_spinlock;
	bool initialized;
};

struct disp_feature_ctl {
	u32 feature_id;
	int feature_val;
	u32 tx_len;
	u8 *tx_ptr;
	u32 rx_len;
	u8 *rx_ptr;
};

struct dsi_cmd_rw_ctl {
	u8 tx_state;
	u32 tx_len;
	u8 *tx_ptr;
	u8 rx_state;
	u32 rx_len;
	u8 *rx_ptr;
};

typedef struct disp_feature_dev disp_feature_dev_t;

enum disp_feature_sysfs {
	MI_SYSFS_DISP_PARAM = 0,
	MI_SYSFS_DYNAMIC_FPS = 1,
	MI_SYSFS_DOZE_BRIGHTNESS = 2,
	MI_SYSFS_BRIGHTNESS_CLONE = 3,
	MI_SYSFS_DC = 4,
	MI_SYSFS_MAX,
};

static inline bool is_support_disp_intf_type(int intf_type)
{
	if (MI_INTF_DSI <= intf_type && intf_type < MI_INTF_MAX)
		return true;
	else
		return false;
}

static inline const char *get_disp_intf_type_name(int intf_type)
{
	switch (intf_type) {
	case MI_INTF_DSI:
		return "DSI";
	case MI_INTF_DP:
		return "DP";
	case MI_INTF_HDMI:
		return "HDMI";
	default:
		return "Unknown";
	}
}

static inline bool is_support_disp_sysfs_node(int sysfs_node)
{
	if (MI_SYSFS_DISP_PARAM <= sysfs_node && sysfs_node < MI_SYSFS_MAX)
		return true;
	else
		return false;
}

#if MI_DISP_FEATURE_ENABLE
int mi_disp_feature_attach_display(void *display, int disp_id, int intf_type);
int mi_disp_feature_detach_display(void *display, int disp_id, int intf_type);
int mi_disp_feature_attach_dsi_display(struct mtk_dsi *display, int index);
int mi_disp_feature_detach_dsi_display(struct mtk_dsi *display, int index);

void mi_disp_feature_sysfs_notify(int sysfs_node);
void mi_disp_feature_event_notify(struct disp_event *event, u8 *payload);
int mi_disp_feature_event_notify_by_type(int disp_id, u32 type, u32 len, u64 val);
struct disp_feature *mi_get_disp_feature(void);

int mi_disp_feature_init(void);
void mi_disp_feature_exit(void);
#else  /* ! MI_DISP_FEATURE_ENABLE */
int mi_disp_feature_attach_display(void *display, int disp_id, int intf_type){ return 0; }
int mi_disp_feature_detach_display(void *display, int disp_id, int intf_type){ return 0; }
int mi_disp_feature_attach_dsi_display(struct mtk_dsi *display, int index){ return 0; }
int mi_disp_feature_detach_dsi_display(struct mtk_dsi *display, int index){ return 0; }

void mi_disp_feature_sysfs_notify(int sysfs_node) {}
void mi_disp_feature_event_notify(struct disp_event *event, u8 *payload) {}
struct disp_feature *mi_get_disp_feature(void) { return NULL; }

static inline int mi_disp_feature_init(void) { return 0; }
static inline void mi_disp_feature_exit(void) {}
#endif

#endif /* _MI_DISP_FEATURE_H_ */

