/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_HDMI_MHL_H__
#define __MDSS_HDMI_MHL_H__

#include <linux/platform_device.h>

struct msm_hdmi_mhl_ops {
	u8 (*tmds_enabled)(struct platform_device *pdev);
	int (*set_mhl_max_pclk)(struct platform_device *pdev, u32 max_val);
	int (*set_upstream_hpd)(struct platform_device *pdev, uint8_t on);
};

int msm_hdmi_register_mhl(struct platform_device *pdev,
			  struct msm_hdmi_mhl_ops *ops, void *data);
#endif /* __MDSS_HDMI_MHL_H__ */
