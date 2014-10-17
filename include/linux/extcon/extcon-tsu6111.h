/*
 * include/linux/extcon/extcon-smsc375x.h
 *
 * Copyright (C) 2013 Intel Corporation
 * Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _EXTCON_TSU6111_H_
#define _EXTCON_TSU6111_H_

#include <linux/module.h>
#include <linux/extcon.h>
#ifdef CONFIG_CHARGER_BQ24192
#include <linux/power/bq24192_charger.h>
#endif

struct tsu6111_pdata {
	int (*enable_vbus)(void);
	int (*disable_vbus)(void);
	int (*is_vbus_online)(void);
	bool charging_compliance_override;
};

inline int dummy_vbus_enable(void) { return -1; }
inline int dummy_vbus_disable(void) { return -1; }
inline int dummy_vbus_status(void) { return -1; }

extern int dc_ti_vbus_on_status(void);

#endif
