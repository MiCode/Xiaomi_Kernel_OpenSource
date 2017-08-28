/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASM_PERF_EVENT_H
#define __ASM_PERF_EVENT_H

#include <asm/stack_pointer.h>

#ifdef CONFIG_PERF_EVENTS
struct pt_regs;
extern unsigned long perf_instruction_pointer(struct pt_regs *regs);
extern unsigned long perf_misc_flags(struct pt_regs *regs);
#define perf_misc_flags(regs)	perf_misc_flags(regs)
#endif

#define perf_arch_fetch_caller_regs(regs, __ip) { \
	(regs)->pc = (__ip);    \
	(regs)->regs[29] = (unsigned long) __builtin_frame_address(0); \
	(regs)->sp = current_stack_pointer; \
	(regs)->pstate = PSR_MODE_EL1h;	\
}

static inline u32 armv8pmu_pmcr_read_reg(void)
{
	u32 val;

	asm volatile("mrs %0, pmcr_el0" : "=r" (val));
	return val;
}

static inline u32 armv8pmu_pmccntr_read_reg(void)
{
	u32 val;

	asm volatile("mrs %0, pmccntr_el0" : "=r" (val));
	return val;
}

static inline u32 armv8pmu_pmxevcntr_read_reg(void)
{
	u32 val;

	asm volatile("mrs %0, pmxevcntr_el0" : "=r" (val));
	return val;
}

static inline u32 armv8pmu_pmovsclr_read_reg(void)
{
	u32 val;

	asm volatile("mrs %0, pmovsclr_el0" : "=r" (val));
	return val;
}

static inline void armv8pmu_pmcr_write_reg(u32 val)
{
	asm volatile("msr pmcr_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmselr_write_reg(u32 val)
{
	asm volatile("msr pmselr_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmccntr_write_reg(u32 val)
{
	asm volatile("msr pmccntr_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmxevcntr_write_reg(u32 val)
{
	asm volatile("msr pmxevcntr_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmxevtyper_write_reg(u32 val)
{
	asm volatile("msr pmxevtyper_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmcntenset_write_reg(u32 val)
{
	asm volatile("msr pmcntenset_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmcntenclr_write_reg(u32 val)
{
	asm volatile("msr pmcntenclr_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmintenset_write_reg(u32 val)
{
	asm volatile("msr pmintenset_el1, %0" :: "r" (val));
}

static inline void armv8pmu_pmintenclr_write_reg(u32 val)
{
	asm volatile("msr pmintenclr_el1, %0" :: "r" (val));
}

static inline void armv8pmu_pmovsclr_write_reg(u32 val)
{
	asm volatile("msr pmovsclr_el0, %0" :: "r" (val));
}

static inline void armv8pmu_pmuserenr_write_reg(u32 val)
{
	asm volatile("msr pmuserenr_el0, %0" :: "r" (val));
}

#endif
