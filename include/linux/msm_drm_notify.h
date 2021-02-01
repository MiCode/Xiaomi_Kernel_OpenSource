/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MSM_DRM_NOTIFY_H_
#define _MSM_DRM_NOTIFY_H_

#include <linux/notifier.h>

/* A hardware display blank change occurred */
#define MSM_DRM_EVENT_BLANK			0x01
/* A hardware display blank early change occurred */
#define MSM_DRM_EARLY_EVENT_BLANK		0x02

#define		DRM_EARLY_EVENT_BLANK   0x01
#define		DRM_EVENT_BLANK         0x02


enum {
	/* panel: power on */
	MSM_DRM_BLANK_UNBLANK,
	/* panel: power off */
	MSM_DRM_BLANK_POWERDOWN,
};

enum {
	DRM_BLANK_UNBLANK = 0,
	DRM_BLANK_LP1,
	DRM_BLANK_LP2,
	DRM_BLANK_STANDBY,
	DRM_BLANK_SUSPEND,
	DRM_BLANK_POWERDOWN,
};


enum msm_drm_display_id {
	/* primary display */
	MSM_DRM_PRIMARY_DISPLAY,
	/* external display */
	MSM_DRM_EXTERNAL_DISPLAY,
	MSM_DRM_DISPLAY_MAX
};

struct msm_drm_notifier {
	enum msm_drm_display_id id;
	void *data;
};
struct drm_notify_data {
	bool is_primary;
	void *data;
};

int msm_drm_register_client(struct notifier_block *nb);
int msm_drm_unregister_client(struct notifier_block *nb);
int drm_register_client(struct notifier_block *nb);
int drm_unregister_client(struct notifier_block *nb);
int drm_notifier_call_chain(unsigned long val, void *v);

#endif
