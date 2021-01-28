// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
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
#include <mt-plat/sync_write.h>

#include <dramc_io.h>
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
		/* pr_info("[DRAMC] wait for SPM HW SEMAPHORE\n"); */
		udelay(1);
	} while (cnt > 0);

	if (cnt == 0) {
		spin_unlock_irqrestore(&dramc_lock, save_flags);
		pr_info("[DRAMC] can NOT get SPM HW SEMAPHORE!\n");
		return -1;
	}

	/* pr_info("[DRAMC] get SPM HW SEMAPHORE success!\n"); */

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
	/* pr_info("[DRAMC] release SPM HW SEMAPHORE success!\n"); */
	return 0;
}

