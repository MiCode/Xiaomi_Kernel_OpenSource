/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef MTK_MEM_ALLOCATION_CONTROL_H_
#define MTK_MEM_ALLOCATION_CONTROL_H_
#include <linux/notifier.h>

#define NOTIFIER_VOW_ALLOCATE_MEM        0x001
#define NOTIFIER_ULTRASOUND_ALLOCATE_MEM 0x002

int register_afe_allocate_mem_notifier(struct notifier_block *nb);

int unregister_afe_allocate_mem_notifier(struct notifier_block *nb);

int notify_allocate_mem(unsigned long module, void *v);

#endif /* MTK_MEM_ALLOCATION_CONTROL_H_ */
