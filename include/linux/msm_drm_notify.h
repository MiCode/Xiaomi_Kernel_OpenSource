/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
/* A hardware display blank revert early change occurred */
#define MSM_DRM_R_EARLY_EVENT_BLANK		0x03

enum {
	/* panel: power on */
	MSM_DRM_BLANK_UNBLANK,
	/* panel: power off */
	MSM_DRM_BLANK_POWERDOWN,
};

struct msm_drm_notifier {
	int blank;
};

extern int msm_drm_panel_register_client(struct notifier_block *nb);
extern int msm_drm_panel_unregister_client(struct notifier_block *nb);
extern int msm_drm_panel_notifier_call_chain(unsigned long val, void *v);
#endif
