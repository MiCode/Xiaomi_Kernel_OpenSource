/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MTK_PMIC_COMMON_H_
#define _MTK_PMIC_COMMON_H_

#include <linux/device.h>
#include <linux/pm_wakeup.h>

#define PMIC_ENTRY(reg) {reg, reg##_ADDR, reg##_MASK, reg##_SHIFT}

#define pmic_init_wake_lock(lock, name)	wakeup_source_init(lock, name)
#define pmic_wake_lock(lock)	__pm_stay_awake(lock)
#define pmic_wake_unlock(lock)	__pm_relax(lock)
extern struct wakeup_source pmicThread_lock;

#endif	/* _MTK_PMIC_COMMON_H_ */
