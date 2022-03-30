/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef MTK_MEM_ALLOCATION_CONTROL_H_
#define MTK_MEM_ALLOCATION_CONTROL_H_
#include <linux/notifier.h>

enum {
	NOTIFIER_VOW_ALLOCATE_MEM = 1,
	NOTIFIER_ULTRASOUND_ALLOCATE_MEM,
	NOTIFIER_ADSP_3WAY_SEMAPHORE_GET,
	NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE,
	NOTIFIER_SCP_3WAY_SEMAPHORE_GET,
	NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE,
};

int register_afe_allocate_mem_notifier(struct notifier_block *nb);
int unregister_afe_allocate_mem_notifier(struct notifier_block *nb);
int notify_allocate_mem(unsigned long module, void *v);

int register_3way_semaphore_notifier(struct notifier_block *nb);
int unregister_3way_semaphore_notifier(struct notifier_block *nb);
int notify_3way_semaphore_control(unsigned long module, void *v);

#endif /* MTK_MEM_ALLOCATION_CONTROL_H_ */
