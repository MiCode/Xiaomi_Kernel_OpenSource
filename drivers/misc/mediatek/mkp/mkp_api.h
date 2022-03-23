/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _MKP_API_H_
#define _MKP_API_H_

#include <linux/rbtree.h>
#include <linux/types.h> // for phys_addr_t
#include <linux/random.h>

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/sizes.h>

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <uapi/linux/sched/types.h>
#include <linux/futex.h>
#include <linux/plist.h>
#include <linux/percpu-defs.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/vmalloc.h>
#include <linux/cma.h>
#include <linux/of_reserved_mem.h>
#include <linux/timer.h>

#include <asm/memory.h> // for MODULE_VADDR
#include <linux/types.h> // for phys_addr_t

#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/sched.h> // for task_struct
#include <linux/cred.h>

#include "policy.h"
#include "mkp_hvc.h"

extern bool __init prepare_grant_ticket(void);

static __always_inline int do_secure_ops(uint32_t policy, uint32_t handle,
		int (*set_memory_hvc)(uint32_t policy, uint32_t handle))
{
	unsigned long flags;
	int ret = -1;

	if (!grant_ticket)
		goto exit;

	local_irq_save(flags);
	*grant_ticket = subscribe;
	ret = set_memory_hvc(policy, handle);
	local_irq_restore(flags);

exit:
	return ret;
}

static __always_inline int mkp_set_mapping_ro(uint32_t policy, uint32_t handle)
{
	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return -1;

	return do_secure_ops(policy, handle, mkp_set_mapping_ro_hvc_call);
}

static __always_inline int mkp_set_mapping_rw(uint32_t policy, uint32_t handle)
{
	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return -1;

	return do_secure_ops(policy, handle, mkp_set_mapping_rw_hvc_call);
}

static __always_inline int mkp_set_mapping_nx(uint32_t policy, uint32_t handle)
{
	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return -1;

	return do_secure_ops(policy, handle, mkp_set_mapping_nx_hvc_call);
}

static __always_inline int mkp_set_mapping_x(uint32_t policy, uint32_t handle)
{
	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return -1;

	return do_secure_ops(policy, handle, mkp_set_mapping_x_hvc_call);
}

static __always_inline int mkp_clear_mapping(uint32_t policy, uint32_t handle)
{
	if (policy >= MKP_POLICY_NR || policy_ctrl[policy] == 0)
		return -1;

	return do_secure_ops(policy, handle, mkp_clear_mapping_hvc_call);
}

void __init mkp_set_policy(u32);
int __init mkp_set_ext_policy(uint32_t policy);
int mkp_lookup_mapping_entry(uint32_t policy, uint32_t handle,
	unsigned long *entry_size, unsigned long *permission);
int mkp_request_new_policy(unsigned long policy_char);
int mkp_change_policy_action(uint32_t policy, unsigned long policy_char_action);
int mkp_request_new_specified_policy(unsigned long policy_char, uint32_t specified_policy);
uint32_t mkp_create_ro_sharebuf(uint32_t policy, unsigned long size, struct page **pages);
uint32_t mkp_create_wo_sharebuf(uint32_t policy, unsigned long size, struct page **pages);
uint32_t mkp_create_handle(uint32_t policy, unsigned long ipa, unsigned long size);
int mkp_destroy_handle(uint32_t policy, uint32_t handle);

int mkp_configure_sharebuf(uint32_t policy, uint32_t handle, uint32_t type, unsigned long nr_entries, unsigned long size);
int mkp_update_sharebuf_1_argu(uint32_t policy, uint32_t handle, unsigned long index, unsigned long a1);
int mkp_update_sharebuf_2_argu(uint32_t policy, uint32_t handle, unsigned long index, unsigned long a1, unsigned long a2);
int mkp_update_sharebuf_3_argu(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3);
int mkp_update_sharebuf_4_argu(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4);
int mkp_update_sharebuf_5_argu(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5);
int mkp_update_sharebuf(uint32_t policy, uint32_t handle, unsigned long index/*tag*/, unsigned long ipa);
int mkp_change_policy_action(uint32_t policy, unsigned long policy_char_action);

#endif /* _MKP_API_H */
