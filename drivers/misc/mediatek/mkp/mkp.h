/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _MKP_H
#define _MKP_H

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

#include "mkp_rbtree.h"
#include "ksym.h"
#include "mkp_api.h"
#include "mkp_hvc.h"
#include "helper.h"
#include "mkp_module.h"
#include "mkp_demo.h"

#endif /* _MKP_H */
