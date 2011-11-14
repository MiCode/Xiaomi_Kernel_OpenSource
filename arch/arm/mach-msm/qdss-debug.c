/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/smp.h>

#include "qdss.h"

#define debug_writel(debug, cpu, val, off)	\
			__raw_writel((val), debug.base[cpu] + off)
#define debug_readl(debug, cpu, off)		\
			__raw_readl(debug.base[cpu] + off)

#define DBGDIDR			(0x000)
#define DBGWFAR			(0x018)
#define DBGVCR			(0x01C)
#define DBGECR			(0x024)
#define DBGDTRRX		(0x080)
#define DBGITR			(0x084)
#define DBGDSCR			(0x088)
#define DBGDTRTX		(0x08C)
#define DBGDRCR			(0x090)
#define DBGEACR			(0x094)
#define DBGPCSR			(0x0A0)
#define DBGCIDSR		(0x0A4)
#define DBGVIDSR		(0x0A8)
#define DBGBVRm(n)		(0x100 + (n * 4))
#define DBGBCRm(n)		(0x140 + (n * 4))
#define DBGWVRm(n)		(0x180 + (n * 4))
#define DBGWCRm(n)		(0x1C0 + (n * 4))
#define DBGBXVRm(n)		(0x240 + (n * 4))
#define DBGOSLAR		(0x300)
#define DBGOSLSR		(0x304)
#define DBGPRCR			(0x310)
#define DBGPRSR			(0x314)


#define DEBUG_LOCK(cpu)							\
do {									\
	mb();								\
	debug_writel(debug, cpu, MAGIC2, CS_LAR);			\
} while (0)
#define DEBUG_UNLOCK(cpu)						\
do {									\
	debug_writel(debug, cpu, MAGIC1, CS_LAR);			\
	mb();								\
} while (0)

#define DEBUG_OS_LOCK(cpu)						\
do {									\
	debug_writel(debug, cpu, MAGIC1, DBGOSLAR);			\
	mb();								\
} while (0)
#define DEBUG_OS_UNLOCK(cpu)						\
do {									\
	mb();								\
	debug_writel(debug, cpu, MAGIC2, DBGOSLAR);			\
	mb();								\
} while (0)

#define MAX_DEBUG_REGS		(90)
#define MAX_STATE_SIZE		(MAX_DEBUG_REGS * num_possible_cpus())
#define DBGDSCR_MASK		(0x6C30FC3C)

struct debug_config {
	/* read only config register */
	uint32_t	dbg_id;
	/* derived values */
	uint8_t		nr_watch_pts;
	uint8_t		nr_brk_pts;
	uint8_t		nr_ctx_comp;
};

struct debug_ctx {
	struct debug_config		cfg;
	void __iomem			**base;
	uint32_t			*state;
	struct device			*dev;
};

static struct debug_ctx debug;

static void debug_save_reg(int cpu)
{
	uint32_t i;
	int j;

	DEBUG_UNLOCK(cpu);
	DEBUG_OS_LOCK(cpu);

	i = cpu * MAX_DEBUG_REGS;

	debug.state[i++] = debug_readl(debug, cpu, DBGWFAR);
	for (j = 0; j < debug.cfg.nr_brk_pts; j++) {
		debug.state[i++] = debug_readl(debug, cpu, DBGBCRm(j));
		debug.state[i++] = debug_readl(debug, cpu, DBGBVRm(j));
	}
	for (j = 0; j < debug.cfg.nr_ctx_comp; j++)
		debug.state[i++] = debug_readl(debug, cpu, DBGBXVRm(j));
	for (j = 0; j < debug.cfg.nr_watch_pts; j++) {
		debug.state[i++] = debug_readl(debug, cpu, DBGWVRm(j));
		debug.state[i++] = debug_readl(debug, cpu, DBGWCRm(j));
	}
	debug.state[i++] = debug_readl(debug, cpu, DBGVCR);
	debug.state[i++] = debug_readl(debug, cpu, CS_CLAIMSET);
	debug.state[i++] = debug_readl(debug, cpu, CS_CLAIMCLR);
	debug.state[i++] = debug_readl(debug, cpu, DBGDTRTX);
	debug.state[i++] = debug_readl(debug, cpu, DBGDTRRX);
	debug.state[i++] = debug_readl(debug, cpu, DBGDSCR);

	DEBUG_LOCK(cpu);
}

static void debug_restore_reg(int cpu)
{
	uint32_t i;
	int j;

	DEBUG_UNLOCK(cpu);
	DEBUG_OS_LOCK(cpu);

	i = cpu * MAX_DEBUG_REGS;

	debug_writel(debug, cpu, debug.state[i++], DBGWFAR);
	for (j = 0; j < debug.cfg.nr_brk_pts; j++) {
		debug_writel(debug, cpu, debug.state[i++], DBGBCRm(j));
		debug_writel(debug, cpu, debug.state[i++], DBGBVRm(j));
	}
	for (j = 0; j < debug.cfg.nr_ctx_comp; j++)
		debug_writel(debug, cpu, debug.state[i++], DBGBXVRm(j));
	for (j = 0; j < debug.cfg.nr_watch_pts; j++) {
		debug_writel(debug, cpu, debug.state[i++], DBGWVRm(j));
		debug_writel(debug, cpu, debug.state[i++], DBGWCRm(j));
	}
	debug_writel(debug, cpu, debug.state[i++], DBGVCR);
	debug_writel(debug, cpu, debug.state[i++], CS_CLAIMSET);
	debug_writel(debug, cpu, debug.state[i++], CS_CLAIMCLR);
	debug_writel(debug, cpu, debug.state[i++], DBGDTRTX);
	debug_writel(debug, cpu, debug.state[i++], DBGDTRRX);
	debug_writel(debug, cpu, debug.state[i++] & DBGDSCR_MASK, DBGDSCR);

	DEBUG_OS_UNLOCK(cpu);
	DEBUG_LOCK(cpu);
}

/* msm_save_jtag_debug and msm_restore_jtag_debug should be fast
 *
 * These functions will be called either from:
 * 1. per_cpu idle thread context for idle power collapses.
 * 2. per_cpu idle thread context for hotplug/suspend power collapse for
 *    nonboot cpus.
 * 3. suspend thread context for core0.
 *
 * In all cases we are guaranteed to be running on the same cpu for the
 * entire duration.
 */
void msm_save_jtag_debug(void)
{
	int cpu = smp_processor_id();
	debug_save_reg(cpu);
}

void msm_restore_jtag_debug(void)
{
	int cpu = smp_processor_id();
	debug_restore_reg(cpu);
}

static void debug_cfg_ro_init(void)
{
	/* use cpu 0 for setup */
	int cpu = 0;

	DEBUG_UNLOCK(cpu);

	debug.cfg.dbg_id = debug_readl(debug, cpu, DBGDIDR);
	debug.cfg.nr_ctx_comp = BMVAL(debug.cfg.dbg_id, 20, 23) + 1;
	debug.cfg.nr_brk_pts = BMVAL(debug.cfg.dbg_id, 24, 27) + 1;
	debug.cfg.nr_watch_pts = BMVAL(debug.cfg.dbg_id, 28, 31) + 1;

	DEBUG_LOCK(cpu);
}

static int __devinit debug_probe(struct platform_device *pdev)
{
	int i, ret;
	struct resource *res;

	debug.base = kzalloc(pdev->num_resources * sizeof(void *), GFP_KERNEL);
	if (!debug.base) {
		ret = -ENOMEM;
		goto err_base_kzalloc;
	}

	for (i = 0; i < pdev->num_resources; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			ret = -EINVAL;
			goto err_res;
		}

		debug.base[i] = ioremap_nocache(res->start, resource_size(res));
		if (!debug.base[i]) {
			ret = -EINVAL;
			goto err_ioremap;
		}
	}

	debug.dev = &pdev->dev;

	debug.state = kzalloc(MAX_STATE_SIZE * sizeof(uint32_t), GFP_KERNEL);
	if (!debug.state) {
		ret = -ENOMEM;
		goto err_state_kzalloc;
	}

	debug_cfg_ro_init();

	dev_info(debug.dev, "Debug intialized.\n");

	return 0;

err_state_kzalloc:
err_ioremap:
err_res:
	while (i) {
		iounmap(debug.base[i-1]);
		i--;
	}
	kfree(debug.base);
err_base_kzalloc:
	return ret;
}

static int __devexit debug_remove(struct platform_device *pdev)
{
	int i;

	kfree(debug.state);
	for (i = pdev->num_resources; i > 0; i--)
		iounmap(debug.base[i-1]);
	kfree(debug.base);

	return 0;
}

static struct platform_driver debug_driver = {
	.probe          = debug_probe,
	.remove         = __devexit_p(debug_remove),
	.driver         = {
		.name   = "msm_debug",
	},
};

static int __init debug_init(void)
{
	return platform_driver_register(&debug_driver);
}
module_init(debug_init);

static void __exit debug_exit(void)
{
	platform_driver_unregister(&debug_driver);
}
module_exit(debug_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Coresight Debug driver");
