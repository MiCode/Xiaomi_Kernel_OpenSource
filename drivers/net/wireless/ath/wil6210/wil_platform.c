// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include "wil_platform.h"
#include "msm_11ad.h"

int __init wil_platform_modinit(void)
{
	return msm_11ad_modinit();
}

void wil_platform_modexit(void)
{
	msm_11ad_modexit();
}

/**
 * wil_platform_init() - wil6210 platform module init
 *
 * The function must be called before all other functions in this module.
 * It returns a handle which is used with the rest of the API
 *
 */
void *wil_platform_init(struct device *dev, struct wil_platform_ops *ops,
			const struct wil_platform_rops *rops, void *wil_handle)
{
	void *handle;

	if (!ops) {
		dev_err(dev,
			"Invalid parameter. Cannot init platform module\n");
		return NULL;
	}

	handle = msm_11ad_dev_init(dev, ops, rops, wil_handle);

	return handle;
}
