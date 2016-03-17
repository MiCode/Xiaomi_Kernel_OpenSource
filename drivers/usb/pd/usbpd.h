/* Copyright (c) 2016, Linux Foundation. All rights reserved.
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

#ifndef _USBPD_H
#define _USBPD_H

#include <linux/device.h>

struct usbpd;

#if IS_ENABLED(CONFIG_USB_PD_POLICY)
struct usbpd *usbpd_create(struct device *parent);
void usbpd_destroy(struct usbpd *pd);
#else
static inline struct usbpd *usbpd_create(struct device *parent)
{
	return ERR_PTR(-ENODEV);
}
static inline void usbpd_destroy(struct usbpd *pd) { }
#endif

enum data_role {
	DR_NONE = -1,
	DR_UFP = 0,
	DR_DFP = 1,
};

enum power_role {
	PR_NONE = -1,
	PR_SINK = 0,
	PR_SRC = 1,
};

#endif /* _USBPD_H */
