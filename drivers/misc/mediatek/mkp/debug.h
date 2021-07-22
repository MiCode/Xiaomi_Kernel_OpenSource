/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __DEBUG_H
#define __DEBUG_H

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


/*
 * Debug level
 * DEBUG_LEVEL_DISABLE: no log is allowed
 * DEBUG_LEVEL_ERR: only ERR is allowed
 * DEBUG_LEVEL_WARN: ERR and WARN are allowed
 * DEBUG_LEVEL_DEBUG: ERR,WARN and DEBUG are allowed
 * DEBUG_LEVEL_INFO: all log is allowed
 */

enum debug_level {
	DEBUG_LEVEL_DISABLE = 0,
	DEBUG_LEVEL_ERR,
	DEBUG_LEVEL_WARN,
	DEBUG_LEVEL_DEBUG,
	DEBUG_LEVEL_INFO
};

#ifdef DEBUG_MKP_ENABLED
#define DEBUG_SET_LEVEL(x) static int local_dbg_level = x

#define MKP_ERR(fmt, args...) \
	do { \
		if (local_dbg_level >= DEBUG_LEVEL_ERR) { \
			pr_info("MKP_ERR: "fmt, ##args); \
		} \
	} while (0)


#define MKP_WARN(fmt, args...) \
	do { \
		if (local_dbg_level >= DEBUG_LEVEL_WARN) { \
			pr_info("MKP_WARN: "fmt, ##args); \
		} \
	} while (0)

#define MKP_DEBUG(fmt, args...) \
	do { \
		if (local_dbg_level >= DEBUG_LEVEL_DEBUG) { \
			pr_info("MKP_DEBUG: "fmt, ##args); \
		} \
	} while (0)

#define MKP_INFO(fmt, args...) \
	do { \
		if (local_dbg_level >= DEBUG_LEVEL_INFO) { \
			pr_info("MKP_INFO: "fmt, ##args); \
		} \
	} while (0)
#else	/* DEBUG_MKP_ENABLED */
#define DEBUG_SET_LEVEL(x)
#define MKP_ERR(fmt, args...)
#define MKP_WARN(fmt, args...)
#define MKP_DEBUG(fmt, args...)
#define MKP_INFO(fmt, args...)
#endif	/* DEBUG_MKP_ENABLED */
#endif
