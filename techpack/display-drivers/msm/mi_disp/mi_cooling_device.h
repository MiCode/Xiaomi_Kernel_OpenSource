/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef __MI_SDE_THERMAL_CORE_H__
#define __MI_SDE_THERMAL_CORE_H__

#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/thermal.h>
#include <linux/notifier.h>

#include "msm_cooling_device.h"
#include "sde_connector.h"
#include "dsi_panel.h"

struct mi_sde_cdev {
	struct sde_cdev sde_cdev;
	struct notifier_block n;
	struct dsi_panel *panel;
};

#ifdef CONFIG_THERMAL_OF
int mi_sde_backlight_setup(struct sde_connector *c_conn,
		struct device *dev, struct backlight_device *bd);
void mi_backlight_cdev_unregister(struct sde_cdev *cdev);

#else
int mi_sde_backlight_setup(struct sde_connector *c_conn,
		struct device *dev, struct backlight_device *bd) { return 0; }
static inline void mi_backlight_cdev_unregister(struct sde_cdev *cdev) {}
#endif

#endif /* _MI_DISP_CONFIG_H_ */
