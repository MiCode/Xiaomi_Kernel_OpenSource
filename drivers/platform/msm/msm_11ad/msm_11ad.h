/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#ifndef __MSM_11AD_H__
#define __MSM_11AD_H__

struct device;
struct wil_platform_ops;

/* call when binding to device, during probe() */
void *msm_11ad_dev_init(struct device *dev, struct wil_platform_ops *ops);

/* call on insmod */
int msm_11ad_modinit(void);

/* call on rmmod */
void msm_11ad_modexit(void);

#endif /* __MSM_11AD_H__ */
