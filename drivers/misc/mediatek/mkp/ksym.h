/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _KSYM_H_
#define _KSYM_H_

#include <linux/types.h> // for phys_addr_t
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

#define KV		kimage_vaddr
#define S_MAX		SZ_128M
#define SM_SIZE		28
#define TT_SIZE		256
#define NAME_LEN	128

int __init mkp_ka_init(void);
void __init mkp_get_krn_code(void **p_stext, void **p_etext);
void __init mkp_get_krn_rodata(void **p_etext, void **p__init_begin);
void __init mkp_get_krn_info(void **p_stext, void **p_etext, void **p__init_begin);
#endif /* _KSYM_H */
