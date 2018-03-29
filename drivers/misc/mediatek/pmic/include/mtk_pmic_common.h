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

#if !defined CONFIG_HAS_WAKELOCKS
#define pmic_wake_lock(lock)	__pm_stay_awake(lock)
#define pmic_wake_unlock(lock)	__pm_relax(lock)
extern struct wakeup_source pmicThread_lock;
#else
#define pmic_wake_lock(lock)	wake_lock(lock)
#define pmic_wake_unlock(lock)	wake_unlock(lock)
extern struct wake_lock pmicThread_lock;
#endif

#endif	/* _MTK_PMIC_COMMON_H_ */
