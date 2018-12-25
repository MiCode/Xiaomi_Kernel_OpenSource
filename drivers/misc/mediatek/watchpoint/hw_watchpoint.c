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

#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <asm/signal.h>
#include <asm/ptrace.h>
#include "hw_watchpoint.h"
#include "mtk_dbg.h"
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <mt-plat/sync_write.h>
#include <asm/system_misc.h>
#include <linux/notifier.h>

struct wp_trace_context_t wp_tracer;

void smp_read_dbgdscr_callback(void *info)
{
	unsigned int tmp;
	unsigned int *val = info;
#ifdef CONFIG_ARM64
	asm volatile ("mrs %0, MDSCR_EL1":"=r" (tmp));
#else
	ARM_DBG_READ(c0, c2, 2, tmp);
#endif
	*val = tmp;
}

void smp_write_dbgdscr_callback(void *info)
{
	unsigned int *val = info;
	unsigned int tmp = *val;
#ifdef CONFIG_ARM64
	asm volatile ("msr  MDSCR_EL1, %0" : : "r" (tmp));
#else
	ARM_DBG_WRITE(c0, c2, 2, tmp);
#endif
}

void smp_read_OSLSR_EL1_callback(void *info)
{
	unsigned int tmp;
	unsigned int *val = info;
#ifdef CONFIG_ARM64
	asm volatile ("mrs %0, OSLSR_EL1":"=r" (tmp));
#else
	ARM_DBG_READ(c1, c1, 4, tmp);
#endif
	*val = tmp;
}

#ifndef CONFIG_ARM64
void smp_read_dbgvcr_callback(void *info)
{
	unsigned long tmp;
	unsigned long *val = info;

	ARM_DBG_READ(c0, c7, 0, tmp);
	*val = tmp;
}

void smp_write_dbgvcr_callback(void *info)
{
	unsigned long *val = info;
	unsigned long tmp = *val;

	ARM_DBG_WRITE(c0, c7, 0, tmp);
}
#endif

static spinlock_t wp_lock;

int register_wp_context(struct wp_trace_context_t **wp_tracer_context)
{
	*wp_tracer_context = &wp_tracer;
	return 0;
}

/*
 * enable_hw_watchpoint: Enable the H/W watchpoint.
 * Return error code.
 */
int enable_hw_watchpoint(void)
{
	int i;
	unsigned int args;

	pr_debug("[MTK WP] Hotplug disable\n");
	cpu_hotplug_disable();

	for_each_online_cpu(i) {
		cs_cpu_write(wp_tracer.debug_regs[i], EDLAR, UNLOCK_KEY);
		cs_cpu_write(wp_tracer.debug_regs[i], OSLAR_EL1, ~UNLOCK_KEY);
		smp_call_function_single(i, smp_read_dbgdscr_callback,
			 &args, 1);
		pr_debug("[MTK WP] cpu %d, EDSCR &0x%lx= 0x%x\n", i,
	 ((vmalloc_to_pfn((void *)wp_tracer.debug_regs[i]) << 12) + EDSCR),
	  args);
		if (args & HDE) {
			pr_debug("[MTK WP] halting debug mode enabled. Unable to access hardware resources.\n");
			return -EPERM;
		}
		pr_debug("[MTK WP] cpu %d, MDSCR 0x%x\n", i, args);
		if (args & MDBGEN)  /* already enabled */
			pr_debug("[MTK WP] already enabled, MDSCR = 0x%x\n",
				 args);
		/*
		 * Since Watchpoint taken from EL1 to EL1,
		 * so we have to enable KDE.
		 * refer ARMv8 architecture spec section - D2.4.1
		 * Enabling debug exceptions from the current Exception level
		 */
#ifdef CONFIG_ARM64
		args |= (MDBGEN | KDE);
#else
		args |= (MDBGEN);
#endif
		smp_call_function_single(i,
			smp_write_dbgdscr_callback, &args, 1);

	}

	pr_debug("[MTK WP] Hotplug enable\n");
	cpu_hotplug_enable();
	return 0;
}


void reset_watchpoint(void)
{
	int j;
	int i;
	unsigned int args;
	int offset = 4;

	for_each_online_cpu(j) {
		args = cs_cpu_read(wp_tracer.debug_regs[j], EDSCR);
		pr_debug("[MTK WP] Reset flow cpu %d, EDSCR 0x%x\n", j, args);
		if (args & HDE) {
			pr_debug("[MTK WP]Reset flow halting debug mode enabled. Unable to reset hardware resources.\n");
#ifdef CONFIG_SMP
			goto copy_exit;
#endif
			return;
		}

		cs_cpu_write(wp_tracer.debug_regs[j], EDLAR, UNLOCK_KEY);
		cs_cpu_write(wp_tracer.debug_regs[j], OSLAR_EL1, ~UNLOCK_KEY);
		for (i = 0; i < wp_tracer.wp_nr; i++) {
			if (wp_tracer.wp_events[i].in_use
				|| (wp_tracer.wp_events[i].virt != 0)) {
				wp_tracer.wp_events[i].virt = 0;
				wp_tracer.wp_events[i].phys = 0;
				wp_tracer.wp_events[i].type = 0;
				wp_tracer.wp_events[i].handler = NULL;
				wp_tracer.wp_events[i].in_use = 0;
			}
#ifdef CONFIG_ARM64
			cs_cpu_write_64(wp_tracer.debug_regs[j],
				DBGWVR + (i << offset), 0);
#else
			cs_cpu_write(wp_tracer.debug_regs[j],
				DBGWVR + (i << offset), 0);
#endif
			cs_cpu_write(wp_tracer.debug_regs[j],
				DBGWCR + (i << offset), 0);
		}
		for (i = 0; i < wp_tracer.bp_nr; i++) {
#ifdef CONFIG_ARM64
			cs_cpu_write_64(wp_tracer.debug_regs[j],
				DBGBVR + (i << offset), 0);
#else
			cs_cpu_write(wp_tracer.debug_regs[j],
				DBGBVR + (i << offset), 0);
#endif
			cs_cpu_write(wp_tracer.debug_regs[j],
				DBGBCR + (i << offset), 0);
		}
		cs_cpu_write(wp_tracer.debug_regs[j], EDLAR, ~UNLOCK_KEY);
		cs_cpu_write(wp_tracer.debug_regs[j], OSLAR_EL1, UNLOCK_KEY);
	}

	return;
copy_exit:
	for_each_online_cpu(i) {
		args = cs_cpu_read(wp_tracer.debug_regs[i], EDSCR);
		if (i != j && !(args & HDE)) {
			mt_copy_dbg_regs(i, j);
			cs_cpu_write(wp_tracer.debug_regs[i],
				 EDLAR, ~UNLOCK_KEY);
			cs_cpu_write(wp_tracer.debug_regs[i],
				 OSLAR_EL1, UNLOCK_KEY);
			cs_cpu_write(wp_tracer.debug_regs[j],
				 EDLAR, ~UNLOCK_KEY);
			cs_cpu_write(wp_tracer.debug_regs[j],
				 OSLAR_EL1, UNLOCK_KEY);
		}
	}
}

/*
 * add_hw_watchpoint: add a watch point.
 * @wp_event: pointer to the struct wp_event.
 * Return error code.
 */
int add_hw_watchpoint(struct wp_event *wp_event)
{
	int ret, i, j;
	unsigned long flags;
	unsigned int ctl;
	int offset = 4;

	if (!wp_event)
		return -EINVAL;

	if (!(wp_event->handler))
		return -EINVAL;

	ret = enable_hw_watchpoint();
	if (ret)
		return ret;

	ctl = DBGWCR_VAL;
	if (wp_event->type == WP_EVENT_TYPE_READ)
		ctl |= LSC_LDR;
	else if (wp_event->type == WP_EVENT_TYPE_WRITE)
		ctl |= LSC_STR;
	else if (wp_event->type == WP_EVENT_TYPE_ALL)
		ctl |= LSC_ALL;
	else
		return -EINVAL;

	spin_lock_irqsave(&wp_lock, flags);
	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		if (!wp_tracer.wp_events[i].in_use) {
			wp_tracer.wp_events[i].in_use = 1;
			break;
		}
		if (wp_tracer.wp_events[i].virt == (wp_event->virt & ~3)) {
			pr_err("This address have been watched in cpu%d's watchpoint\n",
				 i);
			spin_unlock_irqrestore(&wp_lock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&wp_lock, flags);

	if (i == MAX_NR_WATCH_POINT)
		return -EBUSY;

	wp_tracer.wp_events[i].virt =
		wp_event->virt & ~3; /* enforce word-aligned */
	wp_tracer.wp_events[i].phys = wp_event->phys; /* no use currently */
	wp_tracer.wp_events[i].type = wp_event->type;
	wp_tracer.wp_events[i].handler = wp_event->handler;
	wp_tracer.wp_events[i].auto_disable = wp_event->auto_disable;
	pr_debug("[MTK WP] Hotplug disable\n");
	cpu_hotplug_disable();
	pr_debug("[MTK WP] Add watchpoint %d at address %p\n",
		 i, &(wp_tracer.wp_events[i].virt));

	for_each_online_cpu(j) {
#ifdef CONFIG_ARM64
		cs_cpu_write_64(wp_tracer.debug_regs[j],
			DBGWVR + (i << offset),
			wp_tracer.wp_events[i].virt);
#else
		cs_cpu_write(wp_tracer.debug_regs[j],
			DBGWVR + (i << offset),
			wp_tracer.wp_events[i].virt);
#endif
		cs_cpu_write(wp_tracer.debug_regs[j],
			DBGWCR + (i << offset), ctl);
	}
	pr_debug("[MTK WP] Hotplug enable\n");
	cpu_hotplug_enable();
	return 0;
}
EXPORT_SYMBOL(add_hw_watchpoint);
/*
 * del_hw_watchpoint: delete a watch point.
 * @wp_event: pointer to the struct wp_event.
 * Return error code.
 */
int del_hw_watchpoint(struct wp_event *wp_event)
{
	unsigned long flags;
	int i, j;
	int offset = 4;

	if (!wp_event)
		return -EINVAL;

	pr_debug("[MTK WP] Hotplug disable\n");
	cpu_hotplug_disable();
	spin_lock_irqsave(&wp_lock, flags);
	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		if (wp_tracer.wp_events[i].in_use
			&& (wp_tracer.wp_events[i].virt == wp_event->virt)) {
			wp_tracer.wp_events[i].virt = 0;
			wp_tracer.wp_events[i].phys = 0;
			wp_tracer.wp_events[i].type = 0;
			wp_tracer.wp_events[i].handler = NULL;
			wp_tracer.wp_events[i].in_use = 0;

			for_each_online_cpu(j) {
				cs_cpu_write(wp_tracer.debug_regs[j],
					DBGWCR + (i << offset),
					cs_cpu_read(wp_tracer.debug_regs[j],
					DBGWCR + (i << offset)) & (~WP_EN));
				cs_cpu_write(wp_tracer.debug_regs[j],
					EDLAR, ~UNLOCK_KEY);
				cs_cpu_write(wp_tracer.debug_regs[j],
					OSLAR_EL1, UNLOCK_KEY);
			}
			break;
		}
	}
	spin_unlock_irqrestore(&wp_lock, flags);
	pr_debug("[MTK WP] Hotplug enable\n");
	cpu_hotplug_enable();
	if (i == MAX_NR_WATCH_POINT)
		return -EINVAL;
	else
		return 0;
}
EXPORT_SYMBOL(del_hw_watchpoint);
static int watchpoint_handler(unsigned long addr, unsigned int esr,
	struct pt_regs *regs)
{
	unsigned long wfar, daddr, iaddr;
	int i, j, ret;
/*
 * Since ARMv8 uses 16 byte as the offset of BCRs, BVRs, WCRs
 * and WVRs in both aarch32 and aarch64 mode, we have no choices
 * but referring to Chip name to configure the offset.
 */
	int offset = 4;
	/* Notes
	 * wfar is the watched data address which is accessed.
	 * and it is from FAR_EL1
	 */

	/* update PC to avoid re-execution of the instruction under watching */
#ifdef CONFIG_ARM64
	asm volatile ("mrs %0, FAR_EL1":"=r" (wfar));
	daddr = addr & ~3;
	iaddr = regs->pc; /* this is the instruction address
			   * that access the data that is watching.
			   */
	regs->pc += compat_thumb_mode(regs) ? 2 : 4;
#else
	asm volatile ("MRC p15, 0, %0, c6, c0, 0\n":"=r" (wfar) : : "cc");
	daddr = addr & ~3;
	iaddr = regs->ARM_pc;
	regs->ARM_pc += thumb_mode(regs) ? 2 : 4;
#endif
	pr_debug("[MTK WP] addr = 0x%lx, DBGWFAR/DFAR = 0x%lx\n[MTK WP] daddr = 0x%lx, iaddr = 0x%lx\n",
		 (unsigned long)addr, wfar, daddr, iaddr);

	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		if (wp_tracer.wp_events[i].in_use &&
			wp_tracer.wp_events[i].virt == (daddr)) {
			pr_debug("[MTK WP] Watchpoint %d triggers.\n", i);
			if (!(wp_tracer.wp_events[i].handler)) {
				pr_debug("[MTK WP] No watchpoint handler. Ignore.\n");
				return 0;
			}
			if (wp_tracer.wp_events[i].auto_disable) {
				for_each_online_cpu(j) {
					cs_cpu_write(wp_tracer.debug_regs[j],
					  DBGWCR + (i << offset),
					  cs_cpu_read(
					    wp_tracer.debug_regs[j],
					    DBGWCR + (i << offset)) & (~WP_EN));
				}

			}
			ret = wp_tracer.wp_events[i].handler(iaddr);
			if (wp_tracer.wp_events[i].auto_disable) {
				for_each_online_cpu(j) {
					cs_cpu_write(wp_tracer.debug_regs[j],
					  DBGWCR + (i << offset),
					  cs_cpu_read(
					    wp_tracer.debug_regs[j],
					    DBGWCR + (i << offset)) | WP_EN);
				}

			}
			return ret;
		}
	}

	return 0;
}

int wp_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;

	pr_debug("[MTK WP] watchpoint_probe\n");
	of_property_read_u32(pdev->dev.of_node, "num", &wp_tracer.nr_dbg);
	pr_debug("[MTK WP] get %d debug interface\n", wp_tracer.nr_dbg);
	wp_tracer.debug_regs =
		kmalloc(sizeof(void *) * (unsigned long)wp_tracer.nr_dbg,
			GFP_KERNEL);
	if (!wp_tracer.debug_regs) {
		pr_err("[MTK WP] Failed to allocate watchpoint register array\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < wp_tracer.nr_dbg; i++) {
		wp_tracer.debug_regs[i] = of_iomap(pdev->dev.of_node, i);

		if (wp_tracer.debug_regs[i] == NULL)
			pr_err("[MTK WP] debug_interface %d devicetree mapping failed\n",
				 i);
		else
			pr_debug("[MTK WP] debug_interface %d @vm:0x%p pm:0x%lx\n",
				 i, wp_tracer.debug_regs[i],
#ifdef CONFIG_ARM64
		      vmalloc_to_pfn((void *)wp_tracer.debug_regs[i]) << 12);
#else
		      IO_VIRT_TO_PHYS((unsigned long)wp_tracer.debug_regs[i]));
#endif
	}
#ifdef CONFIG_ARM64
	asm volatile ("mrs %0, ID_AA64DFR0_EL1":"=r"
		(wp_tracer.id_aa64dfr0_el1));
	wp_tracer.wp_nr =
		((wp_tracer.id_aa64dfr0_el1 & (0xf << 20)) >> 20) + 1;
	wp_tracer.bp_nr =
		((wp_tracer.id_aa64dfr0_el1 & (0xf << 12)) >> 12) + 1;

#else
	ARM_DBG_READ(c0, c0, 0, wp_tracer.dbgdidr);
	wp_tracer.wp_nr = ((wp_tracer.dbgdidr & (0xf << 28)) >> 28);
	wp_tracer.bp_nr = ((wp_tracer.dbgdidr & (0xf << 24)) >> 24);
#endif
	pr_debug("[MTK_WP] wp_nr : %d , bp_nr : %d\n",
		 wp_tracer.wp_nr, wp_tracer.bp_nr);

	reset_watchpoint();

	register_cpu_notifier(&cpu_nfb);

out:
	return ret;
}

static const struct of_device_id dbg_of_ids[] = {
	{.compatible = "mediatek,hw_dbg",},
	{}
};

static struct platform_driver wp_driver = {
	.probe = wp_probe,
	.driver = {
			 .name = "wp",
			 .bus = &platform_bus_type,
			 .owner = THIS_MODULE,
			 .of_match_table = dbg_of_ids,
			 },
};

static int __init hw_watchpoint_init(void)
{
	int err;

	spin_lock_init(&wp_lock);
	err = platform_driver_register(&wp_driver);
	if (err) {
		pr_err("[MTK WP] watchpoint registration failed\n");
		return err;
	}
#ifdef CONFIG_ARM64
	hook_debug_fault_code(DBG_ESR_EVT_HWWP, watchpoint_handler,
	 SIGTRAP, TRAP_HWBKPT, "hw-watchpoint handler");
#else
#ifdef CONFIG_ARM_LPAE
	hook_fault_code(0x22, watchpoint_handler, SIGTRAP, 0,
		"watchpoint debug exception");
#else
	hook_fault_code(0x02, watchpoint_handler, SIGTRAP, 0,
		"watchpoint debug exception");
#endif
#endif

	pr_debug("[MTK WP] watchpoint handler init.\n");

	return 0;
}
arch_initcall(hw_watchpoint_init);
