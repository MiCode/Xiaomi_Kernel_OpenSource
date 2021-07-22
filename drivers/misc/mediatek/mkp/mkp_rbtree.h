/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _MKP_RBTREE_H_
#define _MKP_RBTREE_H_

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

struct mkp_rb_node {
	struct rb_node rb_node;
	phys_addr_t addr;
	phys_addr_t size;
	uint32_t handle;
};

void traverse_rbtree(struct rb_root *root);
struct mkp_rb_node *mkp_rbtree_search(struct rb_root *root, phys_addr_t addr);
int mkp_rbtree_insert(struct rb_root *root, struct mkp_rb_node *ins);
int mkp_rbtree_erase(struct rb_root *root, phys_addr_t addr);
#endif /* _MKP_RBTREE_H */
