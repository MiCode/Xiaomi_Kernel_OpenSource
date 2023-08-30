/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MI_DRM_NOTIFIER_H_
#define _MI_DRM_NOTIFIER_H_

#include <linux/notifier.h>

/* A hardware display power mode state change occurred */
#define MI_DISP_DPMS_EVENT             0x01
/* A hardware display power mode state early change occurred */
#define MI_DISP_DPMS_EARLY_EVENT       0x02
/* A hardware display mode state after fps changed */
#define MI_DISP_FPS_CHANGE_EVENT       0xF628

enum {
	/* panel: power on */
	MI_DISP_DPMS_ON   = 0,
	MI_DISP_DPMS_LP1       = 1,
	MI_DISP_DPMS_LP2       = 2,
	MI_DISP_DPMS_STANDBY   = 3,
	MI_DISP_DPMS_SUSPEND   = 4,
	/* panel: power off */
	MI_DISP_DPMS_POWERDOWN = 5,
};

enum mi_disp_id {
	MI_DISPLAY_PRIMARY = 0,
	MI_DISPLAY_SECONDARY,
	MI_DISPLAY_MAX,
};

struct mi_disp_notifier {
	int disp_id;
	void *data;
};

int mi_disp_register_client(struct notifier_block *nb);
int mi_disp_unregister_client(struct notifier_block *nb);
int mi_disp_notifier_call_chain(unsigned long val, void *v);

#endif /* _MI_DRM_NOTIFIER_H_ */
