/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ARCH_ARM_MACH_MSM_SMEM_PRIVATE_H_
#define _ARCH_ARM_MACH_MSM_SMEM_PRIVATE_H_

#include <linux/remote_spinlock.h>

#include <mach/ramdump.h>

#define SMD_HEAP_SIZE 512

struct smem_heap_info {
	unsigned initialized;
	unsigned free_offset;
	unsigned heap_remaining;
	unsigned reserved;
};

struct smem_heap_entry {
	unsigned allocated;
	unsigned offset;
	unsigned size;
	unsigned reserved; /* bits 1:0 reserved, bits 31:2 aux smem base addr */
};
#define BASE_ADDR_MASK 0xfffffffc

struct smem_proc_comm {
	unsigned command;
	unsigned status;
	unsigned data1;
	unsigned data2;
};

struct smem_shared {
	struct smem_proc_comm proc_comm[4];
	unsigned version[32];
	struct smem_heap_info heap_info;
	struct smem_heap_entry heap_toc[SMD_HEAP_SIZE];
};

struct smem_area {
	phys_addr_t phys_addr;
	resource_size_t size;
	void __iomem *virt_addr;
};

/* used for unit testing spinlocks */
remote_spinlock_t *smem_get_remote_spinlock(void);

bool smem_initialized_check(void);

/**
 * smem_module_init_notifier_register() - Register a smem module
 *                                       init notifier block
 * @nb: Notifier block to be registered
 *
 * In order to mark the dependency on SMEM Driver module initialization
 * register a notifier using this API. Once the smem module_init is
 * done, notification will be passed to the registered module.
 */
int smem_module_init_notifier_register(struct notifier_block *nb);

/**
 * smem_module_init_notifier_register() - Unregister a smem module
 *                                       init notifier block
 * @nb: Notifier block to be unregistered
 */
int smem_module_init_notifier_unregister(struct notifier_block *nb);
#endif /* _ARCH_ARM_MACH_MSM_SMEM_PRIVATE_H_ */
