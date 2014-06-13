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

#ifndef _EXTCON_SMSC375X_H_
#define _EXTCON_SMSC375X_H_

#include <linux/module.h>
#include <linux/extcon.h>

/**
 * struct smsc375x_pdata - platform data for SMSC 375x device.
 * @is_vbus_online  - call back to get VBUS present state
 */
struct smsc375x_pdata {
	int (*enable_vbus)(void);
	int (*disable_vbus)(void);
	int (*is_vbus_online)(void);
	bool charging_compliance_override;
};

#endif /* _EXTCON_SMSC375X_H_ */
