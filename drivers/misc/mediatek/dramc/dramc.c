/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <asm/cacheflush.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>

#include "mtk_dramc.h"

void __iomem *SLEEP_BASE_ADDR;

static DEFINE_SPINLOCK(dramc_lock);
int acquire_dram_ctrl(void)
{
	unsigned int cnt;
	unsigned long save_flags;

	/* acquire SPM HW SEMAPHORE to avoid race condition */
	spin_lock_irqsave(&dramc_lock, save_flags);

	cnt = 2;
	do {
		if ((readl(PDEF_SPM_AP_SEMAPHORE) & 0x1) != 0x1) {
			writel(0x1, PDEF_SPM_AP_SEMAPHORE);
			if ((readl(PDEF_SPM_AP_SEMAPHORE) & 0x1) == 0x1)
				break;
		}

		cnt--;
		/* pr_err("[DRAMC] wait for SPM HW SEMAPHORE\n"); */
		udelay(1);
	} while (cnt > 0);

	if (cnt == 0) {
		spin_unlock_irqrestore(&dramc_lock, save_flags);
		pr_warn("[DRAMC] can NOT get SPM HW SEMAPHORE!\n");
		return -1;
	}

	/* pr_err("[DRAMC] get SPM HW SEMAPHORE success!\n"); */

	spin_unlock_irqrestore(&dramc_lock, save_flags);
	return 0;
}

int release_dram_ctrl(void)
{
	/* release SPM HW SEMAPHORE to avoid race condition */
	if ((readl(PDEF_SPM_AP_SEMAPHORE) & 0x1) == 0x0) {
		pr_err("[DRAMC] has NOT acquired SPM HW SEMAPHORE\n");
		/* BUG(); */
	}

	writel(0x1, PDEF_SPM_AP_SEMAPHORE);
	if ((readl(PDEF_SPM_AP_SEMAPHORE) & 0x1) == 0x1) {
		pr_err("[DRAMC] release SPM HW SEMAPHORE fail!\n");
		/* BUG(); */
	}
	/* pr_err("[DRAMC] release SPM HW SEMAPHORE success!\n"); */
	return 0;
}

