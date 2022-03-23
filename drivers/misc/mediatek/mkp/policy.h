/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __POLICY_H
#define __POLICY_H

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
#include "debug.h"

/*
 * Bit usage of HVC fast call for MKP:
 *
 *  31 30 29:24   23:17    16 15:11  10:8 7:0
 * +--+--+-------+--------+--+------+----+---------+
 * |1 |0 |000010 |0000000 |0 |      |000 |         |
 * +--+--+-------+--------+--+------+----+---------+
 *           |                  |     |       |
 *           |                  |     |       v
 *           |                  |     |     [7:0]   function bits
 *           |                  |     +---> [10:8]  reserved
 *           |                  +---------> [15:11] policy bits
 *           +----------------------------> [29:24] entity bits, we use 2 (SiP service call)
 */

#define FUNCTION_BITS	(8)
#define RESERVE_BITS	(3)
#define POLICY_BITS	(5)

#define FUNCTION_SHIFT	(0)
#define RESERVE_SHIFT	(FUNCTION_SHIFT + FUNCTION_BITS)
#define POLICY_SHIFT	(RESERVE_SHIFT + RESERVE_BITS)

#define FUNCTION_MAXNR	(1UL << (FUNCTION_BITS))
#define POLICY_MAXNR	(1UL << (POLICY_BITS))
#define FUNCTION_MASK	(FUNCTION_MAXNR - 1)
#define POLICY_MASK	(POLICY_MAXNR - 1)
#define FUNCTION_ID(x)	(x >> FUNCTION_SHIFT) & FUNCTION_MASK
#define POLICY_ID(x)	(x >> POLICY_SHIFT) & POLICY_MASK

#define MKP_HVC_CALL_ID(policy, hvc_func_num) (0x82000000|(((policy & POLICY_MASK) << POLICY_SHIFT)|\
	((hvc_func_num & FUNCTION_MASK) << FUNCTION_SHIFT)))

/* MKP Policy ID */
enum mkp_policy_id {
	/* MKP default policies (0 ~ 15) */
	MKP_POLICY_MKP = 0,			/* Policy ID for MKP itself */
	MKP_POLICY_DRV,				/* Policy ID for kernel drivers */
	MKP_POLICY_SELINUX_STATE,		/* Policy ID for selinux_state */
	MKP_POLICY_SELINUX_AVC,			/* Policy ID for selinux avc */
	MKP_POLICY_TASK_CRED,			/* Policy ID for task credential */
	MKP_POLICY_KERNEL_CODE,			/* Policy ID for kernel text */
	MKP_POLICY_KERNEL_RODATA,		/* Policy ID for kernel rodata */
	MKP_POLICY_KERNEL_PAGES,		/* Policy ID for other mapped kernel pages */
	MKP_POLICY_PGTABLE,			/* Policy ID for page table */
	MKP_POLICY_S1_MMU_CTRL,			/* Policy ID for stage-1 MMU control */
	MKP_POLICY_FILTER_SMC_HVC,		/* Policy ID for HVC/SMC call filtering */
	MKP_POLICY_DEFAULT_MAX = 15,

	/* Policies for vendors start from here (16 ~ 31) */
	MKP_POLICY_VENDOR_START = 16,
	MKP_POLICY_NR = POLICY_MAXNR,
};

/* Characteristic for Policy */
enum mkp_policy_char {
	NO_MAP_TO_DEVICE	= 0x00000001,	/* Requested PA range is not allowed to be mmaped by device MMU(IOMMU) */
	NO_UPGRADE_TO_WRITE 	= 0x00000002,   /* Requested PA range is not allowed to be set as WRITE */
	NO_UPGRADE_TO_EXEC	= 0x00000004,	/* Requested PA range is not allowed to be set as EXEC */
	HANDLE_PERMANENT	= 0x00000100,	/* Registered handle can not be destroyed */
	SB_ENTRY_DISORDERED	= 0x00010000,	/* When using share buffer, input instance should be manipulated by scanning */
	SB_ENTRY_ORDERED	= 0x00020000,	/* When using share buffer, input instance should be manipulated by index */
	ACTION_NOTIFICATION	= 0x00100000,	/* When some threat is detected, just forwarding a notification to users. No warning, no panic*/
	ACTION_WARNING		= 0x00200000,	/* When some threat is detectd, MKP may need counting,. When its count exceeds some threshold, triggering panic. */
	ACTION_PANIC		= 0x00400000,	/* When some threat is detected, MKP will trigger panic directly */
	ACTION_BITS			= 0x00700000,	/* Bits for actions */
	CHAR_POLICY_INVALID	= 0x40000000,	/* Indicate the policy is not initialized for further use */
	CHAR_POLICY_AVAILABLE	= 0x80000000,	/* Indicate the policy is available for vendor use */
};

extern int policy_ctrl[MKP_POLICY_NR];
extern uint32_t mkp_policy_action[MKP_POLICY_NR];
void __init set_policy(u32 policy);
int __init set_ext_policy(uint32_t policy);
void __init enable_action_panic(void);
void handle_mkp_err_action(uint32_t policy);
#endif
