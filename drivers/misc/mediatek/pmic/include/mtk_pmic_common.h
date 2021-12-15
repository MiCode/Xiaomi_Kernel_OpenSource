/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
