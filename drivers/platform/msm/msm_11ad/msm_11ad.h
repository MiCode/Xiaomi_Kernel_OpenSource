/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */
#ifndef __MSM_11AD_H__
#define __MSM_11AD_H__

struct device;
struct wil_platform_ops;
struct wil_platform_rops;

/*	msm_11ad_dev_init - call when binding to device, during probe()
 *	@dev:	device structure of pci device
 *	@ops:	pointer to operations supported by platform driver
 *		Will be filled by this function call
 *	@rops:	pointer to callback functions provided by wil device driver.
 *		the platform driver copies the structure contents to its
 *		internal storage. May be NULL if device driver does not
 *		support rops.
 *	@wil_handle:	context for wil device driver, will be provided
 *			when platform driver invokes any of the callback
 *			functions in rops. May be NULL if rops is also NULL
 */
void *msm_11ad_dev_init(struct device *dev, struct wil_platform_ops *ops,
			const struct wil_platform_rops *rops, void *wil_handle);

/* call on insmod */
int msm_11ad_modinit(void);

/* call on rmmod */
void msm_11ad_modexit(void);

#endif /* __MSM_11AD_H__ */
