/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _MKP_HVC_H_
#define _MKP_HVC_H_

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

#include "policy.h"

extern const uint64_t subscribe;
extern uint64_t *grant_ticket;

/* MKP HVC function number */
enum mkp_hvc_func_num {
	/* Policy ops */
	HVC_FUNC_NEW_POLICY = 16,
	HVC_FUNC_POLICY_ACTION = 17,
	HVC_FUNC_NEW_SPECIFIED_POLICY = 18,

	/* Handle ops */
	HVC_FUNC_CREATE_HANDLE = 32,
	HVC_FUNC_DESTROY_HANDLE = 33,

	/* Mapping ops */
	HVC_FUNC_SET_MAPPING_RO = 48,
	HVC_FUNC_SET_MAPPING_RW = 49,
	HVC_FUNC_SET_MAPPING_NX = 50,
	HVC_FUNC_SET_MAPPING_X = 51,
	HVC_FUNC_CLEAR_MAPPING = 52,
	HVC_FUNC_LOOKUP_MAPPING_ENTRY = 53,

	/* Sharebuf ops */
	HVC_FUNC_CONFIGURE_SHAREBUF = 64,
	HVC_FUNC_UPDATE_SHAREBUF_1_ARGU = 65,
	HVC_FUNC_UPDATE_SHAREBUF_2_ARGU = 66,
	HVC_FUNC_UPDATE_SHAREBUF_3_ARGU = 67,
	HVC_FUNC_UPDATE_SHAREBUF_4_ARGU = 68,
	HVC_FUNC_UPDATE_SHAREBUF_5_ARGU = 69,
	HVC_FUNC_UPDATE_SHAREBUF = 70,

	/* Essential for MKP service */
	HVC_FUNC_ESS_0 = 96,
	HVC_FUNC_ESS_1 = 97,
};

int mkp_set_mapping_ro_hvc_call(uint32_t policy, uint32_t handle);
int mkp_set_mapping_rw_hvc_call(uint32_t policy, uint32_t handle);
int mkp_set_mapping_nx_hvc_call(uint32_t policy, uint32_t handle);
int mkp_set_mapping_x_hvc_call(uint32_t policy, uint32_t handle);
int mkp_clear_mapping_hvc_call(uint32_t policy, uint32_t handle);
int mkp_lookup_mapping_entry_hvc_call(uint32_t policy, uint32_t handle,
	unsigned long *entry_size, unsigned long *permission);

int mkp_req_new_policy_hvc_call(unsigned long policy_char);
int mkp_change_policy_action_hvc_call(uint32_t policy, unsigned long policy_char_action);
int mkp_req_new_specified_policy_hvc_call(unsigned long policy_char, uint32_t specified_policy);
uint32_t mkp_create_handle_hvc_call(uint32_t policy,
	unsigned long ipa, unsigned long size);
int mkp_destroy_handle_hvc_call(uint32_t policy, uint32_t handle);

int mkp_configure_sharebuf_hvc_call(uint32_t policy, uint32_t handle, uint32_t type,
	unsigned long nr_entries, unsigned long size);
int mkp_update_sharebuf_1_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1);
int mkp_update_sharebuf_2_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2);
int mkp_update_sharebuf_3_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3);
int mkp_update_sharebuf_4_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4);
int mkp_update_sharebuf_5_argu_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5);
int mkp_update_sharebuf_hvc_call(uint32_t policy, uint32_t handle, unsigned long index,
	unsigned long ipa);

int __init mkp_setup_essential_hvc_call(unsigned long phys_offset, unsigned long fixaddr_top,
	unsigned long fixaddr_real_start);

int __init mkp_start_granting_hvc_call(void);

#endif /* _MKP_HVC_H */
