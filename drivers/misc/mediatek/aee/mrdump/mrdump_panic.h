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

#if !defined(__AEE_IPANIC_H__)
#define __AEE_IPANIC_H__

#include <generated/autoconf.h>
#include <mt-plat/aee.h>

/* for WDT timeout case : dump timer/schedule/irq/softirq etc...
 * debug information
 */
void aee_disable_api(void);

extern int ipanic_atflog_buffer(void *data, unsigned char *buffer,
		size_t sz_buf);
extern void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs,
		struct task_struct *tsk);
extern void mrdump_mini_ke_cpu_regs(struct pt_regs *regs);
extern void mrdump_mini_add_misc(unsigned long addr, unsigned long size,
		unsigned long start, char *name);
extern int mrdump_task_info(unsigned char *buffer, size_t sz_buf);
extern int mrdump_modules_info(unsigned char *buffer, size_t sz_buf);
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_exp_type(unsigned int type);
extern unsigned int aee_rr_curr_exp_type(void);
extern void aee_rr_rec_scp(void);
extern void aee_rr_rec_fiq_step(u8 step);
extern void aee_rr_rec_kaslr_offset(u64 value64);
#else
__weak unsigned int aee_rr_curr_exp_type(void)
{
	return -1;
}
#endif
#ifdef CONFIG_SCHED_DEBUG
extern void sysrq_sched_debug_show_at_AEE(void);
#endif
#ifdef CONFIG_MTK_WQ_DEBUG
extern void wq_debug_dump(void);
#endif
extern void dis_D_inner_fL1L2(void);
extern int console_trylock(void);

#endif
