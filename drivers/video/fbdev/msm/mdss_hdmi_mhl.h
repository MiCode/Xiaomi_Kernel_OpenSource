/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013, 2018, 2020, The Linux Foundation. All rights reserved. */

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
