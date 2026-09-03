/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRD_PHY_H__
#define __SPRD_PHY_H__

#include <linux/usb/phy.h>
#include <linux/notifier.h>
#include <linux/kallsyms.h>

enum sprd_usbphy_event_mode {
	SPRD_USBPHY_EVENT_TYPEC = 0,
	SPRD_USBPHY_EVENT_MAX,
};

struct sprd_hsphy_ops {
	void (*dpdm_switch_to_phy)(struct usb_phy *x, bool enable);
	bool (*get_dpdm_from_phy)(struct usb_phy *x);
};

/* the first three phy structures are the same for all
 * phy structures and used for indexing
 */

struct sprd_common_hsphy {
	struct device		*dev;
	struct usb_phy		 phy;
	struct sprd_hsphy_ops		ops;
};

#if IS_ENABLED(CONFIG_SPRD_COMMONPHY)
extern int call_sprd_usbphy_event_notifiers(unsigned int id, unsigned long val, void *v);
extern int register_sprd_usbphy_notifier(struct notifier_block *nb, unsigned int id);
#else
static inline int call_sprd_usbphy_event_notifiers(unsigned int id, unsigned long val, void *v)
{
	return 0;
}
static inline int register_sprd_usbphy_notifier(struct notifier_block *nb, unsigned int id)
{
	return 0;
}
#endif /* IS_ENABLED(CONFIG_SPRD_COMMONPHY) */

#endif /* __LINUX_USB_SPRD_PHY_H */
