// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "ZMC: " fmt

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/highmem.h>

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_ARM64
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <asm/memory.h>
#endif
#include <mt-plat/mtk_zmc.h>

phys_addr_t zmc_movable_min = ULONG_MAX;
phys_addr_t zmc_movable_max;

bool zmc_reserved_mem_inited;

void zmc_get_range(phys_addr_t *base, phys_addr_t *size)
{
	if (zmc_movable_max > zmc_movable_min) {
		pr_info("Query return: [%pa,%pa)\n",
				&zmc_movable_min, &zmc_movable_max);
		*base = zmc_movable_min;
		*size = zmc_movable_max - zmc_movable_min;
	} else {
		*base = *size = 0;
	}
}

bool is_zmc_inited(void)
{
	if (zmc_reserved_mem_inited)
		pr_info("zmc is inited\n");
	else
		pr_info("zmc is inited fail\n");
	return zmc_reserved_mem_inited;
}
