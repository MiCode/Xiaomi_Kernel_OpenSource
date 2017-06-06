/*
 *  linux/arch/arm/include/asm/perf_event.h
 *
 *  Copyright (C) 2009 picoChip Designs Ltd, Jamie Iles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ARM_PERF_EVENT_H__
#define __ARM_PERF_EVENT_H__

#ifdef CONFIG_PERF_EVENTS
struct pt_regs;
extern unsigned long perf_instruction_pointer(struct pt_regs *regs);
extern unsigned long perf_misc_flags(struct pt_regs *regs);
#define perf_misc_flags(regs)	perf_misc_flags(regs)
#endif

#define perf_arch_fetch_caller_regs(regs, __ip) { \
	(regs)->ARM_pc = (__ip); \
	(regs)->ARM_fp = (unsigned long) __builtin_frame_address(0); \
	(regs)->ARM_sp = current_stack_pointer; \
	(regs)->ARM_cpsr = SVC_MODE; \
}

static inline u32 armv8pmu_pmcr_read_reg(void)
{
	u32 val;

	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r" (val));
	return val;
}

static inline u32 armv8pmu_pmccntr_read_reg(void)
{
	u32 val;

	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (val));
	return val;
}

static inline u32 armv8pmu_pmxevcntr_read_reg(void)
{
	u32 val;

	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r" (val));
	return val;
}

static inline u32 armv8pmu_pmovsclr_read_reg(void)
{
	u32 val;

	asm volatile("mrc p15, 0, %0, c9, c12, 3" : "=r" (val));
	return val;
}

static inline void armv8pmu_pmcr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r" (val));
}

static inline void armv8pmu_pmselr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (val));
}

static inline void armv8pmu_pmccntr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c13, 0" : : "r" (val));
}

static inline void armv8pmu_pmxevcntr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r" (val));
}

static inline void armv8pmu_pmxevtyper_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (val));
}

static inline void armv8pmu_pmcntenset_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (val));
}

static inline void armv8pmu_pmcntenclr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (val));
}

static inline void armv8pmu_pmintenset_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c14, 1" : : "r" (val));
}

static inline void armv8pmu_pmintenclr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (val));
}

static inline void armv8pmu_pmovsclr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (val));
}

static inline void armv8pmu_pmuserenr_write_reg(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r" (val));
}

#endif /* __ARM_PERF_EVENT_H__ */
