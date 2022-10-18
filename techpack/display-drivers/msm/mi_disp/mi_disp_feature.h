/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
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

#include <drm/mi_disp.h>
#include "mi_disp_config.h"

#define DISP_FEATURE_DEVICE_NAME "disp_feature"

#define to_disp_feature(d) dev_get_drvdata(d)

enum disp_intf_type {
	MI_INTF_DSI = 0,
	MI_INTF_DP = 1,
	MI_INTF_HDMI = 2,
	MI_INTF_MAX,
};

struct disp_work {
	struct kthread_work work;
	struct disp_display *dd_ptr;
	wait_queue_head_t *wq;
	void *data;
};

struct disp_delayed_work {
	struct kthread_delayed_work delayed_work;
	struct disp_display *dd_ptr;
	wait_queue_head_t *wq;
	void *data;
};

struct disp_display {
	struct device *dev;
	void *display;
	int disp_id;
	int intf_type;
	struct mutex mutex_lock;
	struct kthread_worker *worker;
	wait_queue_head_t pending_wq;
	atomic_t pending_doze_cnt;

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
	struct kthread_work thread_priority_work;

	struct list_head client_list;
	spinlock_t client_spinlock;
};

struct disp_pending_event {
	struct list_head link;
	struct disp_event_resp event;
	u8 data[];
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

enum disp_sysfs_node {
	MI_SYSFS_DISP_PARAM = 0,
	MI_SYSFS_DYNAMIC_FPS = 1,
	MI_SYSFS_DOZE_BRIGHTNESS = 2,
	MI_SYSFS_BRIGHTNESS_CLONE = 3,
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

static inline const char *get_disp_sysfs_node_name(int sysfs_node)
{
	switch (sysfs_node) {
	case MI_SYSFS_DISP_PARAM:
		return "disp_param";
	case MI_SYSFS_DYNAMIC_FPS:
		return "dynamic_fps";
	case MI_SYSFS_DOZE_BRIGHTNESS:
		return "doze_brightness";
	case MI_SYSFS_BRIGHTNESS_CLONE:
		return "brightness_clone";
	default:
		return "Unknown";
	}
}

#if MI_DISP_FEATURE_ENABLE
int mi_disp_feature_attach_display(void *display, int disp_id, int intf_type);
int mi_disp_feature_detach_display(void *display, int disp_id, int intf_type);

void mi_disp_feature_event_notify(struct disp_event *event, u8 *payload);
int mi_disp_feature_event_notify_by_type(int disp_id, u32 type, u32 len, u64 val);
void mi_disp_feature_sysfs_notify(int disp_id, int sysfs_node);
struct disp_feature *mi_get_disp_feature(void);

int mi_disp_feature_init(void);
void mi_disp_feature_deinit(void);
#else  /* ! MI_DISP_FEATURE_ENABLE */
int mi_disp_feature_attach_display(void *display, int disp_id, int intf_type) { return 0; }
int mi_disp_feature_detach_display(void *display, int disp_id, int intf_type) { return 0; }

void mi_disp_feature_event_notify(struct disp_event *event, u8 *payload) { return 0; }
int mi_disp_feature_event_notify_by_type(int disp_id, u32 type, u32 len, u64 val) { return 0; }
void mi_disp_feature_sysfs_notify(int disp_id, int sysfs_node) {}
struct disp_feature *mi_get_disp_feature(void) { return NULL; }

static inline int mi_disp_feature_init(void) { return 0; }
static inline void mi_disp_feature_deinit(void) {}
#endif

#endif /* _MI_DISP_FEATURE_H_ */
