/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef MT_CPUIDLE_H
#define MT_CPUIDLE_H

#define MT_CPU_DORMANT_RESET 0
#define MT_CPU_DORMANT_ABORT 1
#define MT_CPU_DORMANT_BREAK 2
#define MT_CPU_DORMANT_BYPASS 8
#define MT_CPU_DORMANT_BREAK_V(v) (MT_CPU_DORMANT_BREAK | ((v) << 0x8))

#define CPU_PM_BREAK	0
#define IRQ_PENDING_1	1
#define IRQ_PENDING_2	2
#define IRQ_PENDING_3	3

#define DORMANT_BREAK_CHECK	(1<<0)
#define DORMANT_SKIP_1		(1<<1)
#define DORMANT_SKIP_2		(1<<2)
#define DORMANT_SKIP_3		(1<<3)
#define DORMANT_LOUIS_OFF	(1<<8)
#define DORMANT_INNER_OFF	(1<<9)
#define DORMANT_OUTER_OFF	(1<<10)
#define DORMANT_CPUSYS_OFF	(1<<11)
#define DORMANT_GIC_OFF		(1<<12)
#define DORMANT_SNOOP_OFF	(1<<13)
#define DORMANT_CCI400_CLOCK_SW	(1<<16)

#define DORMANT_ALL_OFF (DORMANT_OUTER_OFF	\
			 | DORMANT_INNER_OFF	\
			 | DORMANT_CPUSYS_OFF	\
			 | DORMANT_LOUIS_OFF	\
			 | DORMANT_GIC_OFF	\
			 | DORMANT_SNOOP_OFF)
#define DORMANT_MODE_MASK (0x0ffff00)

#define CPU_SHUTDOWN_MODE (DORMANT_ALL_OFF)

#define CPU_MCDI_MODE (DORMANT_LOUIS_OFF)
#define CPU_SODI_MODE (DORMANT_LOUIS_OFF	\
		       | DORMANT_CPUSYS_OFF	\
		       | DORMANT_GIC_OFF	\
		       | DORMANT_SNOOP_OFF)

#define CPU_DEEPIDLE_MODE (DORMANT_LOUIS_OFF	\
			   | DORMANT_CPUSYS_OFF	\
			   | DORMANT_GIC_OFF	\
			   | DORMANT_SNOOP_OFF)

#define CPU_SUSPEND_MODE (DORMANT_ALL_OFF)

#define IS_DORMANT_SKIP_1(a)		(((a) & DORMANT_SKIP_1) == DORMANT_SKIP_1)
#define IS_DORMANT_SKIP_2(a)		(((a) & DORMANT_SKIP_2) == DORMANT_SKIP_2)
#define IS_DORMANT_SKIP_3(a)		(((a) & DORMANT_SKIP_3) == DORMANT_SKIP_3)

#define IS_DORMANT_BREAK_CHECK(a)	(((a) & DORMANT_BREAK_CHECK) == DORMANT_BREAK_CHECK)
#define IS_DORMANT_SNOOP_OFF(a)		(((a) & DORMANT_SNOOP_OFF) == DORMANT_SNOOP_OFF)
#define IS_DORMANT_INNER_OFF(a)		(((a) & DORMANT_INNER_OFF) == DORMANT_INNER_OFF)
#define IS_DORMANT_CPUSYS_OFF(a)	(((a) & DORMANT_CPUSYS_OFF) == DORMANT_CPUSYS_OFF)
#define IS_DORMANT_GIC_OFF(a)		(((a) & DORMANT_GIC_OFF) == DORMANT_GIC_OFF)
#define IS_DORMANT_CCI400_CLOCK_SW(a)	(((a) & DORMANT_CCI400_CLOCK_SW) == DORMANT_CCI400_CLOCK_SW)
#define IS_CPU_SHUTDOWN_MODE(a)		(((a) & MODE_MASK) == CPU_SHUTDOWN_MODE)
#define IS_CPU_DORMANT_MODE(a)		(((a) & MODE_MASK) == CPU_DORMANT_MODE)

#define _IS_DORMANT_SET(flag, feature)	(((flag) & (feature)) == (feature))

/*
 * mt_cpu_dormant
 *
 * cpu do the context save and issue WFI to SPM for trigger power-down,
 * and finally restore context after reset
 *
 * input:
 * data - the flags to decide detail of flow a bitwise arguments
 *	-- CPU_DORMANT_MODE
 *	-- CPU_SHUTDOWN_MODE
 *	-- (optional) DORMANT_BREAK_CHECK
 *
 * return:
 * MT_CPU_DORMANT_RESET: cpu is reset from power-down state.
 * MT_CPU_DORMANT_ABORT: cpu issue WFI and return for a pending interrupt.
 * MT_CPU_DORMANT_BREAK:  cpu dormant flow broken before by validating a SPM interrupt.
 * MT_CPU_DORMANT_BYPASS: (for debug only) to bypass all dormant flow.
 */

int mt_cpu_dormant_init(void);
int mt_cpu_dormant(unsigned long data);

extern void write_cntpctl(int cntpctl);
extern int read_cntpctl(void);
extern int read_cpu_id(void);
extern int read_cluster_id(void);
extern void mt_save_generic_timer(unsigned int *container, int sw);
extern void mt_restore_generic_timer(unsigned int *container, int sw);
extern void mt_save_l2ctlr(unsigned long l2ctlr);
extern unsigned long mt_restore_l2ctlr(int);
extern void mt_goto_cpu_resume(void (*cpu_resume_phys));
extern int mt_get_data_nommu(int data, unsigned long pva);
extern void smp(void);
extern void amp(void);
extern void disable_dcache_safe(bool is_inner_off);
extern void cpu_wake_up_errata_802022(void);
extern unsigned *copy_words(volatile unsigned *destination,
			    volatile unsigned *source, unsigned num_words);

extern unsigned long *aee_rr_rec_cpu_dormant(void);
extern unsigned long *aee_rr_rec_cpu_dormant_pa(void);

#endif
