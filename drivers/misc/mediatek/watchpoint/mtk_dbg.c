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


#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <mt-plat/mtk_boot.h>
#ifdef CONFIG_SMP
#include <linux/of.h>
#include <linux/cpu.h>
#endif
#include "hw_watchpoint.h"
#include "mtk_dbg.h"

#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/notifier.h>

struct dbgreg_set dbgregs[MAX_NR_CPU];
#ifdef CONFIG_SMP
static int dbgregs_hotplug_callback(struct notifier_block *nfb,
	unsigned long action, void *hcpu);
#endif

#ifdef DBG_REG_DUMP
void dump_dbgregs(int cpuid)
{
	struct wp_trace_context_t *wp_context;
	int i;
	int oslsr;
	int offset = 4;

	register_wp_context(&wp_context);
	cs_cpu_write(wp_context->debug_regs[cpuid], EDLAR, UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[cpuid], OSLAR_EL1, ~UNLOCK_KEY);

	smp_call_function_single(cpuid, smp_read_OSLSR_EL1_callback,
		&oslsr, 1);
	isb();

	smp_call_function_single(cpuid, smp_read_dbgdscr_callback,
		&dbgregs[cpuid].DBGDSCRext, 1);
	dbgregs[cpuid].DBGEDSCR =
		cs_cpu_read(wp_context->debug_regs[cpuid], EDSCR);

	for (i = 1; i < 1 + (wp_context->bp_nr); i++) {
#ifdef CONFIG_ARM64
		dbgregs[cpuid].regs[i] =
			cs_cpu_read_64(wp_context->debug_regs[cpuid],
				(DBGBVR + ((i - 1) << offset)));
#else
		dbgregs[cpuid].regs[i] =
			cs_cpu_read(wp_context->debug_regs[cpuid],
				(DBGBVR + ((i - 1) << offset)));
#endif
	}

	for (i = 7; i < 7 + (wp_context->bp_nr); i++) {
		dbgregs[cpuid].regs[i] =
			cs_cpu_read(wp_context->debug_regs[cpuid],
				(DBGBCR + ((i - 7) << offset)));
	}

	for (i = 13; i < 13 + (wp_context->wp_nr); i++) {
#ifdef CONFIG_ARM64
		dbgregs[cpuid].regs[i] =
			cs_cpu_read_64(wp_context->debug_regs[cpuid],
				(DBGWVR + ((i - 13) << offset)));
#else
		dbgregs[cpuid].regs[i] =
			cs_cpu_read(wp_context->debug_regs[cpuid],
				(DBGWVR + ((i - 13) << offset)));
#endif
	}

	for (i = 17; i < 17 + (wp_context->wp_nr); i++) {
		dbgregs[cpuid].regs[i] =
			cs_cpu_read(wp_context->debug_regs[cpuid],
				(DBGWCR + ((i - 17) << offset)));
	}
	mb(); /* sync memory data before dump reg */
	isb();

}
EXPORT_SYMBOL(dump_dbgregs);
void print_dbgregs(int cpuid)
{
	pr_debug("[MTK WP] cpu %d, EDSCR 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGEDSCR);
	pr_debug("[MTK WP] cpu %d, DBGDSCRext 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGDSCRext);
	pr_debug("[MTK WP] cpu %d, DBGBVR0 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBVR0);
	pr_debug("[MTK WP] cpu %d, DBGBVR1 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBVR1);
	pr_debug("[MTK WP] cpu %d, DBGBVR2 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBVR2);
	pr_debug("[MTK WP] cpu %d, DBGBVR3 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBVR3);
	pr_debug("[MTK WP] cpu %d, DBGBVR4 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBVR4);
	pr_debug("[MTK WP] cpu %d, DBGBVR5 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBVR5);

	pr_debug("[MTK WP] cpu %d, DBGBCR0 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBCR0);
	pr_debug("[MTK WP] cpu %d, DBGBCR1 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBCR1);
	pr_debug("[MTK WP] cpu %d, DBGBCR2 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBCR2);
	pr_debug("[MTK WP] cpu %d, DBGBCR3 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBCR3);
	pr_debug("[MTK WP] cpu %d, DBGBCR4 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBCR4);
	pr_debug("[MTK WP] cpu %d, DBGBCR5 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGBCR5);

	pr_debug("[MTK WP] cpu %d, DBGWVR0 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWVR0);
	pr_debug("[MTK WP] cpu %d, DBGWVR1 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWVR1);
	pr_debug("[MTK WP] cpu %d, DBGWVR2 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWVR2);
	pr_debug("[MTK WP] cpu %d, DBGWVR3 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWVR3);

	pr_debug("[MTK WP] cpu %d, DBGWCR0 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWCR0);
	pr_debug("[MTK WP] cpu %d, DBGWCR1 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWCR1);
	pr_debug("[MTK WP] cpu %d, DBGWCR2 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWCR2);
	pr_debug("[MTK WP] cpu %d, DBGWCR3 0x%lx\n",
		 cpuid, dbgregs[cpuid].DBGWCR3);

}
EXPORT_SYMBOL(print_dbgregs);
#endif
unsigned long *mt_save_dbg_regs(unsigned long *p, unsigned int cpuid)
{

	struct wp_trace_context_t *wp_context;

	register_wp_context(&wp_context);

	cs_cpu_write(wp_context->debug_regs[cpuid], EDLAR, UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[cpuid], OSLAR_EL1, ~UNLOCK_KEY);
#ifdef DBG_REG_DUMP
	pr_debug("[MTK WP] %s\n", __func__);
#endif
	if (*p == ~0x0) {
		pr_err("[MTK WP]restore pointer is NULL\n");
		return 0;
	}

	isb();
	/* save register */
#ifdef CONFIG_ARM64
	__asm__ __volatile__("mrs x0, MDSCR_EL1\n\t"
			     "str x0, [%0],#0x8\n\t"
			     "mrs x0, DBGBVR0_EL1\n\t"
			     "mrs x1, DBGBVR1_EL1\n\t"
			     "mrs x2, DBGBVR2_EL1\n\t"
			     "mrs x3, DBGBVR3_EL1\n\t"
			     "mrs x4, DBGBVR4_EL1\n\t"
			     "mrs x5, DBGBVR5_EL1\n\t"
			     "str x0 , [%0],#0x8\n\t"
			     "str x1 , [%0],#0x8\n\t"
			     "str x2 , [%0],#0x8\n\t"
			     "str x3 , [%0],#0x8\n\t"
			     "str x4 , [%0],#0x8\n\t"
			     "str x5 , [%0],#0x8\n\t"
			     "mrs x0, DBGBCR0_EL1\n\t"
			     "mrs x1, DBGBCR1_EL1\n\t"
			     "mrs x2, DBGBCR2_EL1\n\t"
			     "mrs x3, DBGBCR3_EL1\n\t"
			     "mrs x4, DBGBCR4_EL1\n\t"
			     "mrs x5, DBGBCR5_EL1\n\t"
			     "str x0 , [%0],#0x8\n\t"
			     "str x1 , [%0],#0x8\n\t"
			     "str x2 , [%0],#0x8\n\t"
			     "str x3 , [%0],#0x8\n\t"
			     "str x4 , [%0],#0x8\n\t"
			     "str x5 , [%0],#0x8\n\t"
			     "mrs x0, DBGWVR0_EL1\n\t"
			     "mrs x1, DBGWVR1_EL1\n\t"
			     "mrs x2, DBGWVR2_EL1\n\t"
			     "mrs x3, DBGWVR3_EL1\n\t"
			     "str x0 , [%0],#0x8\n\t"
			     "str x1 , [%0],#0x8\n\t"
			     "str x2 , [%0],#0x8\n\t"
			     "str x3 , [%0],#0x8\n\t"
			     "mrs x0, DBGWCR0_EL1\n\t"
			     "mrs x1, DBGWCR1_EL1\n\t"
			     "mrs x2, DBGWCR2_EL1\n\t"
			     "mrs x3, DBGWCR3_EL1\n\t"
			     "str x0 , [%0],#0x8\n\t"
			     "str x1 , [%0],#0x8\n\t"
			     "str x2 , [%0],#0x8\n\t"
			     "str x3 , [%0],#0x8\n\t":"+r"(p)
			     : : "x0", "x1", "x2", "x3", "x4", "x5");
#else
	__asm__ __volatile__("mrc p14, 0, r4, c0, c2, 2  @DBGDSCR_ext\n\t"
			     "stm %0!, {r4}\n\t"
			     "mrc p14, 0, r4, c0, c0, 4  @DBGBVR\n\t"
			     "mrc p14, 0, r5, c0, c1, 4  @DBGBVR\n\t"
			     "mrc p14, 0, r6, c0, c2, 4  @DBGBVR\n\t"
			     "mrc p14, 0, r7, c0, c3, 4  @DBGBVR\n\t"
			     "mrc p14, 0, r8, c0, c4, 4  @DBGBVR\n\t"
			     "mrc p14, 0, r9, c0, c5, 4  @DBGBVR\n\t"
			     "stm %0!, {r4-r9}\n\t"
			     "mrc p14, 0, r4, c0, c0, 5  @DBGBCR\n\t"
			     "mrc p14, 0, r5, c0, c1, 5  @DBGBCR\n\t"
			     "mrc p14, 0, r6, c0, c2, 5  @DBGBCR\n\t"
			     "mrc p14, 0, r7, c0, c3, 5  @DBGBCR\n\t"
			     "mrc p14, 0, r8, c0, c4, 5  @DBGBCR\n\t"
			     "mrc p14, 0, r9, c0, c5, 5  @DBGBCR\n\t"
			     "stm %0!, {r4-r9}\n\t"
			     "mrc p14, 0, r4, c0, c0, 6  @DBGWVR\n\t"
			     "mrc p14, 0, r5, c0, c1, 6  @DBGWVR\n\t"
			     "mrc p14, 0, r6, c0, c2, 6  @DBGWVR\n\t"
			     "mrc p14, 0, r7, c0, c3, 6  @DBGWVR\n\t"
			     "stm %0!, {r4-r7}\n\t"
			     "mrc p14, 0, r4, c0, c0, 7  @DBGWCR\n\t"
			     "mrc p14, 0, r5, c0, c1, 7  @DBGWCR\n\t"
			     "mrc p14, 0, r6, c0, c2, 7  @DBGWCR\n\t"
			     "mrc p14, 0, r7, c0, c3, 7  @DBGWCR\n\t"
			     "stm %0!, {r4-r7}\n\t"
			     "mrc p14, 0, r4, c0, c7, 0  @DBGVCR\n\t"
			     "stm %0!, {r4}\n\t":"+r"(p)
			     : : "r4", "r5", "r6", "r7", "r8", "r9");
#endif
	isb();

	return p;

}

void mt_restore_dbg_regs(unsigned long *p, unsigned int cpuid)
{
	unsigned long dscr;
	struct wp_trace_context_t *wp_context;

#ifdef DBG_REG_DUMP
	pr_debug("[MTK WP] %s\n", __func__);
#endif
	register_wp_context(&wp_context);
	/* the dbg container is invalid */
	if (*p == ~0x0) {
		pr_err("[MTK WP]restore pointer is NULL\n");
		return;
	}
	cs_cpu_write(wp_context->debug_regs[cpuid], EDLAR, UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[cpuid], OSLAR_EL1, ~UNLOCK_KEY);

	isb();
	/* restore register */
#ifdef CONFIG_ARM64
	__asm__ __volatile__("ldr x0, [%0],#0x8\n\t"
		"mov %1, x0\n\t"
		"msr MDSCR_EL1,x0\n\t"
		"ldr x0 , [%0],#0x8\n\t"
		"ldr x1 , [%0],#0x8\n\t"
		"ldr x2 , [%0],#0x8\n\t"
		"ldr x3 , [%0],#0x8\n\t"
		"ldr x4 , [%0],#0x8\n\t"
		"ldr x5 , [%0],#0x8\n\t"
		"msr DBGBVR0_EL1,x0\n\t"
		"msr DBGBVR1_EL1,x1\n\t"
		"msr DBGBVR2_EL1,x2\n\t"
		"msr DBGBVR3_EL1,x3\n\t"
		"msr DBGBVR4_EL1,x4\n\t"
		"msr DBGBVR5_EL1,x5\n\t"
		"ldr x0 , [%0],#0x8\n\t"
		"ldr x1 , [%0],#0x8\n\t"
		"ldr x2 , [%0],#0x8\n\t"
		"ldr x3 , [%0],#0x8\n\t"
		"ldr x4 , [%0],#0x8\n\t"
		"ldr x5 , [%0],#0x8\n\t"
		"msr DBGBCR0_EL1,x0\n\t"
		"msr DBGBCR1_EL1,x1\n\t"
		"msr DBGBCR2_EL1,x2\n\t"
		"msr DBGBCR3_EL1,x3\n\t"
		"msr DBGBCR4_EL1,x4\n\t"
		"msr DBGBCR5_EL1,x5\n\t"
		"ldr x0 , [%0],#0x8\n\t"
		"ldr x1 , [%0],#0x8\n\t"
		"ldr x2 , [%0],#0x8\n\t"
		"ldr x3 , [%0],#0x8\n\t"
		"msr DBGWVR0_EL1,x0\n\t"
		"msr DBGWVR1_EL1,x1\n\t"
		"msr DBGWVR2_EL1,x2\n\t"
		"msr DBGWVR3_EL1,x3\n\t"
		"ldr x0 , [%0],#0x8\n\t"
		"ldr x1 , [%0],#0x8\n\t"
		"ldr x2 , [%0],#0x8\n\t"
		"ldr x3 , [%0],#0x8\n\t"
		"msr DBGWCR0_EL1,x0\n\t"
		"msr DBGWCR1_EL1,x1\n\t"
		"msr DBGWCR2_EL1,x2\n\t"
		"msr DBGWCR3_EL1,x3\n\t":"+r"(p), "=r"(dscr)
		: : "x0", "x1", "x2", "x3", "x4", "x5");
#else
	__asm__ __volatile__("ldm %0!, {r4}\n\t"
		"mov %1, r4\n\t"
		"mcr p14, 0, r4, c0, c2, 2  @DBGDSCR\n\t"
		"ldm %0!, {r4-r9}\n\t"
		"mcr p14, 0, r4, c0, c0, 4  @DBGBVR\n\t"
		"mcr p14, 0, r5, c0, c1, 4  @DBGBVR\n\t"
		"mcr p14, 0, r6, c0, c2, 4  @DBGBVR\n\t"
		"mcr p14, 0, r7, c0, c3, 4  @DBGBVR\n\t"
		"mcr p14, 0, r8, c0, c4, 4  @DBGBVR\n\t"
		"mcr p14, 0, r9, c0, c5, 4  @DBGBVR\n\t"
		"ldm %0!, {r4-r9}\n\t"
		"mcr p14, 0, r4, c0, c0, 5  @DBGBCR\n\t"
		"mcr p14, 0, r5, c0, c1, 5  @DBGBCR\n\t"
		"mcr p14, 0, r6, c0, c2, 5  @DBGBCR\n\t"
		"mcr p14, 0, r7, c0, c3, 5  @DBGBCR\n\t"
		"mcr p14, 0, r8, c0, c4, 5  @DBGBCR\n\t"
		"mcr p14, 0, r9, c0, c5, 5  @DBGBCR\n\t"
		"ldm %0!, {r4-r7}\n\t"
		"mcr p14, 0, r4, c0, c0, 6  @DBGWVR\n\t"
		"mcr p14, 0, r5, c0, c1, 6  @DBGWVR\n\t"
		"mcr p14, 0, r6, c0, c2, 6  @DBGWVR\n\t"
		"mcr p14, 0, r7, c0, c3, 6  @DBGWVR\n\t"
		"ldm %0!, {r4-r7}\n\t"
		"mcr p14, 0, r4, c0, c0, 7  @DBGWCR\n\t"
		"mcr p14, 0, r5, c0, c1, 7  @DBGWCR\n\t"
		"mcr p14, 0, r6, c0, c2, 7  @DBGWCR\n\t"
		"mcr p14, 0, r7, c0, c3, 7  @DBGWCR\n\t"
		"ldm %0!, {r4}\n\t"
		"mcr p14, 0, r4, c0, c7, 0  @DBGVCR\n\t":"+r"(p), "=r"(dscr)
		: : "r4", "r5", "r6", "r7", "r8", "r9");
#endif
	isb();
}


/** to copy dbg registers from FROM to TO within the same cluster.
 * DBG_BASE is the common base of 2 cores debug register space.
 **/
void mt_copy_dbg_regs(int to, int from)
{
	unsigned long base_to, base_from;
	unsigned long args;
	struct wp_trace_context_t *wp_context;
	int i;
	int offset = 4;

	register_wp_context(&wp_context);
	base_to = (unsigned long)wp_context->debug_regs[to];
	base_from = (unsigned long)wp_context->debug_regs[from];

	/* os unlock */
	cs_cpu_write(wp_context->debug_regs[to], EDLAR, UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[to], OSLAR_EL1, ~UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[from], EDLAR, UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[from], OSLAR_EL1, ~UNLOCK_KEY);

	isb();

	args = cs_cpu_read(wp_context->debug_regs[from], EDSCR);
	cs_cpu_write(wp_context->debug_regs[to], EDSCR, args);
	isb();
	for (i = 0; i < wp_context->bp_nr; i++) {
		dbg_reg_copy(DBGBCR + (i << offset), base_to, base_from);
#ifdef CONFIG_ARM64
		dbg_reg_copy_64(DBGBVR + (i << offset), base_to, base_from);
#else
		dbg_reg_copy(DBGBVR + (i << offset), base_to, base_from);
#endif
	}

	for (i = 0; i < wp_context->wp_nr; i++) {
		dbg_reg_copy(DBGWCR + (i << offset), base_to, base_from);
#ifdef CONFIG_ARM64
		dbg_reg_copy_64(DBGWVR + (i << offset), base_to, base_from);
#else
		dbg_reg_copy(DBGWVR + (i << offset), base_to, base_from);
#endif
	}
#ifndef CONFIG_ARM64	/* for AARCH32 */
	smp_call_function_single(from, smp_read_dbgvcr_callback, &args, 1);
	smp_call_function_single(to, smp_write_dbgvcr_callback, &args, 1);
#endif
	isb();
}



#ifdef CONFIG_SMP
static int dbgregs_hotplug_callback(struct notifier_block *nfb,
	unsigned long action, void *hcpu)
{
	unsigned long this_cpu = (unsigned long)hcpu;
	unsigned int args = 0;
	struct wp_trace_context_t *wp_context;
	unsigned long base_to, base_from = 0;
	int i, j;
	int offset = 4;

	action = action & 0xf;
	if (action != CPU_ONLINE)
		return NOTIFY_OK;
	register_wp_context(&wp_context);
	cs_cpu_write(wp_context->debug_regs[this_cpu], EDLAR, UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[this_cpu], OSLAR_EL1, ~UNLOCK_KEY);
	for_each_online_cpu(i) {
		cs_cpu_write(wp_context->debug_regs[i],
			 EDLAR,
			 UNLOCK_KEY);
		cs_cpu_write(wp_context->debug_regs[i],
			 OSLAR_EL1,
			 ~UNLOCK_KEY);
		base_from = (unsigned long)wp_context->debug_regs[i];
		break;
	}
	base_to = (unsigned long)wp_context->debug_regs[this_cpu];
#ifdef DBG_REG_DUMP
	pr_debug("[MTK WP] cpu %lx do %s,action: 0x%lx\n",
		 this_cpu, __func__, action);
#endif

	/* restore EDSCR */
	args = cs_cpu_read(wp_context->debug_regs[i], EDSCR);
	cs_cpu_write(wp_context->debug_regs[this_cpu], EDSCR, args);
	args = MDBGEN | KDE | TDCC;
	smp_write_dbgdscr_callback(&args);
	isb();

#ifdef DBG_REG_DUMP
	pr_debug("[MTK WP] cpu %lx do %s, CPU%d's dbgdscr=0x%x\n",
		 this_cpu, __func__, i, args);
#endif
	for (j = 0; j < wp_context->bp_nr; j++) {
		dbg_reg_copy(DBGBCR + (j << offset), base_to, base_from);
#ifdef CONFIG_ARM64
		dbg_reg_copy_64(DBGBVR + (j << offset), base_to, base_from);
#else
		dbg_reg_copy(DBGBVR + (j << offset), base_to, base_from);
#endif
	}

	for (j = 0; j < wp_context->wp_nr; j++) {
		dbg_reg_copy(DBGWCR + (j << offset), base_to, base_from);
#ifdef CONFIG_ARM64
		dbg_reg_copy_64(DBGWVR + (j << offset), base_to, base_from);
#else
		dbg_reg_copy(DBGWVR + (j << offset), base_to, base_from);
#endif
	}

	isb();

	return NOTIFY_OK;
}

struct notifier_block cpu_nfb = {
	.notifier_call = dbgregs_hotplug_callback
};

#endif
