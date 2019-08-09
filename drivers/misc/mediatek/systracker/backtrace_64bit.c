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

#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/traps.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>
#include <mtk_ram_console.h>
#include <linux/ftrace.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include "systracker_v2.h"
#include <mt-plat/sync_write.h>

#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

unsigned int times;

struct sys_bt {
	unsigned int times;
	unsigned long long t1;
	unsigned int cpu;
	unsigned long fp0;
	unsigned long fp1;
	unsigned long fp2;
	unsigned long fp3;
	unsigned long fp4;
	unsigned long fp5;
	unsigned long fp6;
};

struct sys_bt tracker_bt;
static inline void aee_print_ip_sym
	(unsigned long ip, unsigned long long time_stamp,
		unsigned int cpu, unsigned int t);

void  __attribute__((weak)) consys_print
	(unsigned long long time_stamp,
		unsigned int cpu, unsigned int t)
{
	pr_notice("NO %s !!!\n", __func__);
}

void save_sys_bt(unsigned long long time_stamp,
		unsigned int cpu, unsigned int t)
{
	char buf[256];
	char dbg_st[128];

	memset(&tracker_bt, 0, sizeof(struct sys_bt));
	tracker_bt.t1 = time_stamp;
	tracker_bt.cpu = cpu;
	tracker_bt.times = t;
	tracker_bt.fp0 = CALLER_ADDR0;
	tracker_bt.fp1 = CALLER_ADDR1;
	tracker_bt.fp2 = CALLER_ADDR2;
	tracker_bt.fp3 = CALLER_ADDR3;
	tracker_bt.fp4 = CALLER_ADDR4;
	tracker_bt.fp5 = CALLER_ADDR5;
	tracker_bt.fp6 = CALLER_ADDR6;
	snprintf(buf, sizeof(buf),
		"CPU%d extra_dump_%d, ts=%lld\n",
			cpu, t, time_stamp);
	aee_sram_fiq_log(buf);
	aee_print_ip_sym(tracker_bt.fp0, time_stamp, cpu, t);
	aee_print_ip_sym(tracker_bt.fp1, time_stamp, cpu, t);
	aee_print_ip_sym(tracker_bt.fp2, time_stamp, cpu, t);
	aee_print_ip_sym(tracker_bt.fp3, time_stamp, cpu, t);
	aee_print_ip_sym(tracker_bt.fp4, time_stamp, cpu, t);
	aee_print_ip_sym(tracker_bt.fp5, time_stamp, cpu, t);
	aee_print_ip_sym(tracker_bt.fp6, time_stamp, cpu, t);
#ifdef CONFIG_MTK_SYSTRACKER_V2
	snprintf(dbg_st, sizeof(dbg_st),
			"CPU%d 0x10000220=0x%x - (%d)<%d>[%lld]\n",
				tracker_bt.cpu,
				readl(IOMEM(BUS_PROTECT_BASE+0x220)),
				cpu, t, time_stamp);
	aee_sram_fiq_log(dbg_st);
#endif
	consys_print(time_stamp, cpu, t);

	snprintf(dbg_st, sizeof(dbg_st),
		"CPU%d BUS_DBG_CON=0x%x - (%d)<%d>[%lld]\n",
		tracker_bt.cpu, readl(IOMEM(BUS_DBG_CON)), cpu, t, time_stamp);
	aee_sram_fiq_log(dbg_st);
	snprintf(dbg_st, sizeof(dbg_st),
		"END - (%d)<%d>[%lld]\n\n", cpu, t, time_stamp);
	aee_sram_fiq_log(dbg_st);
}


static inline void aee_print_ip_sym
	(unsigned long ip, unsigned long long time_stamp,
		unsigned int cpu, unsigned int t)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "[<%p>] %pS - (%d)<%d>[%lld]\n",
		(void *)ip, (void *)ip, cpu, t, time_stamp);
	aee_sram_fiq_log(buf);
}

static void dump_backtrace_entry(unsigned long where,
		unsigned long stack, unsigned long long time_stamp,
			unsigned int cpu, unsigned int t)
{
	aee_print_ip_sym(where, time_stamp, cpu, t);
}

void aee_dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
	struct stackframe frame;
	unsigned int cpuid;
	unsigned long long ts;
	char buf[256];

	const register unsigned long current_sp asm("sp");

	times++;
	ts = sched_clock();
	cpuid = smp_processor_id();
	pr_debug("%s(regs = %p tsk = %p)\n", __func__, regs, tsk);

	if (!tsk)
		tsk = current;

	if (regs) {
		frame.fp = regs->regs[29];
		frame.sp = regs->sp;
		frame.pc = regs->pc;
	} else if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.pc = (unsigned long)aee_dump_backtrace;
	} else {
		/*
		 * task blocked in __switch_to
		 */
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		frame.pc = thread_saved_pc(tsk);
	}

	snprintf(buf, sizeof(buf), "CPU%d trace_dump_%d, ts=%lld\n",
			cpuid, times, ts);
	aee_sram_fiq_log(buf);
	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(tsk, &frame);
		if (ret < 0)
			break;
		dump_backtrace_entry(where, frame.sp, ts, cpuid, times);
	}
	save_sys_bt(ts, cpuid, times);
}
