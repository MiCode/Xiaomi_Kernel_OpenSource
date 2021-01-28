/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_IDLE_H__
#define __MTK_IDLE_H__

#include <linux/notifier.h>

enum idle_lock_spm_id {
	IDLE_SPM_LOCK_VCORE_DVFS = 0,
};
extern void idle_lock_spm(enum idle_lock_spm_id id);
extern void idle_unlock_spm(enum idle_lock_spm_id id);

/* return 0: non-active, 1:active */
extern int dpidle_active_status(void);

enum spm_idle_notify_id {
	NOTIFY_DPIDLE_ENTER = 0,
	NOTIFY_DPIDLE_LEAVE,
	NOTIFY_SOIDLE_ENTER,
	NOTIFY_SOIDLE_LEAVE,
	NOTIFY_SOIDLE3_ENTER,
	NOTIFY_SOIDLE3_LEAVE,
};

extern int mtk_idle_notifier_register(struct notifier_block *n);
extern void mtk_idle_notifier_unregister(struct notifier_block *n);

extern void idle_lock_by_ufs(unsigned int lock);
extern void idle_lock_by_gpu(unsigned int lock);

extern int soidle_enter(int cpu);
extern int dpidle_enter(int cpu);
extern int soidle3_enter(int cpu);

extern void mcdi_heart_beat_log_dump(void);

#endif
