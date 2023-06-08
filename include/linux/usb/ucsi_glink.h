/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UCSI_GLINK_H__
#define __UCSI_GLINK_H__

#include <linux/errno.h>
#include <linux/usb/typec.h>

enum cid_accessory {
	CID_ACCESSORY_UNKNOWN,
	CID_ACCESSORY_CNT,
	CID_ACCESSORY_DISCNT,
};

enum charging_accessory {
	CHG_ACCESSORY_UNKNOWN,
	CHG_ACCESSORY_CHARGING,
	CHG_ACCESSORY_DISCHARGING,
};

struct ucsi_glink_constat_info {
	enum typec_accessory acc;
	enum cid_accessory cid_status;
	enum charging_accessory charging_sts;
};

struct notifier_block;

#if IS_ENABLED(CONFIG_UCSI_QTI_GLINK)

int register_ucsi_glink_notifier(struct notifier_block *nb);
int unregister_ucsi_glink_notifier(struct notifier_block *nb);

#else

static inline int register_ucsi_glink_notifier(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int unregister_ucsi_glink_notifier(struct notifier_block *nb)
{
	return -ENODEV;
}

#endif
#endif
