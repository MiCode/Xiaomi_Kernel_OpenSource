/*
 * system-pmic.h -- Interface to access system PMIC functionality for
 * system power off/reset.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __LINUX_POWER_RESET_SYSTEM_PMIC_H__
#define __LINUX_POWER_RESET_SYSTEM_PMIC_H__

struct system_pmic_dev;

enum system_pmic_power_on_event {
	SYSTEM_PMIC_USB_VBUS_INSERTION,
	SYSTEM_PMIC_RTC_ALARM,
	SYSTEM_PMIC_MAX_POWER_ON_EVENT,
};

struct system_pmic_config {
	bool allow_power_reset;
	bool allow_power_off;
};

struct system_pmic_rtc_data {
	int power_on_after_sec;
};

struct system_pmic_ops {
	int (*configure_power_on)(void *pmic_data,
		enum system_pmic_power_on_event event, void *event_data);
	void (*power_reset)(void *pmic_data);
	void (*power_off)(void *pmic_data);
};

extern struct system_pmic_dev *system_pmic_register(struct device *dev,
	struct system_pmic_ops *ops, struct system_pmic_config *config,
	void *drv_data);

extern void system_pmic_unregister(struct system_pmic_dev *pmic_dev);
extern int system_pmic_set_power_on_event(enum system_pmic_power_on_event event,
	void *data);

#endif /* __LINUX_POWER_RESET_SYSTEM_PMIC_H__ */

