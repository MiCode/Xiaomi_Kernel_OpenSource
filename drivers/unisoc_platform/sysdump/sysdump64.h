// SPDX-License-Identifier: GPL-2.0-only
/*
 * sysdump64.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __MACH_SYSDUMP64_H
#define __MACH_SYSDUMP64_H

#include <asm/memory.h>

struct sprd_debug_mmu_reg_t {
	unsigned long sctlr_el1;
	unsigned long ttbr0_el1;
	unsigned long ttbr1_el1;
	unsigned long tcr_el1;
	unsigned long mair_el1;
	unsigned long amair_el1;
	unsigned long contextidr_el1;
};

/* ARM CORE regs mapping structure */
struct sprd_debug_core_t {
	/* COMMON */
	unsigned long x0, x1, x2, x3, x4, x5, x6, x7, x8, x9;
	unsigned long x10, x11, x12, x13, x14, x15, x16, x17, x18, x19;
	unsigned long x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;
	unsigned long x30, sp;

	/* PC & CPSR */
	unsigned long pc;
	unsigned long pstate;
};

/**
 * crash_setup_regs() - save registers for the panic kernel
 * @newregs: registers are saved here
 * @oldregs: registers to be saved (may be %NULL)
 *
 * Function copies machine registers from @oldregs to @newregs. If @oldregs is
 * %NULL then current registers are stored there.
 */
static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	unsigned long tmp = 0;
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
	} else {
		__asm__ __volatile__("stp  x0,  x1, [%[regs_base], #0x00]\n\t"
				     "stp  x2,  x3, [%[regs_base], #0x10]\n\t"
				     "stp  x4,  x5, [%[regs_base], #0x20]\n\t"
				     "stp  x6,  x7, [%[regs_base], #0x30]\n\t"
				     "stp  x8,  x9, [%[regs_base], #0x40]\n\t"
				     "stp x10, x11, [%[regs_base], #0x50]\n\t"
				     "stp x12, x13, [%[regs_base], #0x60]\n\t"
				     "stp x14, x15, [%[regs_base], #0x70]\n\t"
				     "stp x16, x17, [%[regs_base], #0x80]\n\t"
				     "stp x18, x19, [%[regs_base], #0x90]\n\t"
				     "stp x20, x21, [%[regs_base], #0xA0]\n\t"
				     "stp x22, x23, [%[regs_base], #0xB0]\n\t"
				     "stp x24, x25, [%[regs_base], #0xC0]\n\t"
				     "stp x26, x27, [%[regs_base], #0xD0]\n\t"
				     "stp x28, x29, [%[regs_base], #0xE0]\n\t"
				     "mov %[tmp], sp\n\t"
				     "stp x30, %[tmp], [%[regs_base], #0xF0]\n\t"
				     "adr %[pc], 1f\n\t"
				     "mrs %[cpsr], nzcv\n\t"
				     "mrs %[tmp], daif\n\t"
				     "orr %[cpsr], %[cpsr], %[tmp]\n\t"
				     "mrs %[tmp], CurrentEL\n\t"
				     "orr %[cpsr], %[cpsr], %[tmp]\n\t"
				     "mrs %[tmp], SPSel\n\t"
				     "orr %[cpsr], %[cpsr], %[tmp]\n\t"
				     "1:":[pc] "=r"(newregs->pc),
				     [cpsr] "=r"(newregs->pstate),
				     [tmp] "=r"(tmp) :
				     [regs_base] "r"(&newregs->regs)
				     : "memory");
	}
}

/* core reg dump function*/
static inline void sprd_debug_save_core_reg(struct sprd_debug_core_t *core_reg)
{
	unsigned long tmp;
	__asm__ __volatile__("stp  x0,  x1, [%[regs_base], #0x00]\n\t"
			     "stp  x2,  x3, [%[regs_base], #0x10]\n\t"
			     "stp  x4,  x5, [%[regs_base], #0x20]\n\t"
			     "stp  x6,  x7, [%[regs_base], #0x30]\n\t"
			     "stp  x8,  x9, [%[regs_base], #0x40]\n\t"
			     "stp x10, x11, [%[regs_base], #0x50]\n\t"
			     "stp x12, x13, [%[regs_base], #0x60]\n\t"
			     "stp x14, x15, [%[regs_base], #0x70]\n\t"
			     "stp x16, x17, [%[regs_base], #0x80]\n\t"
			     "stp x18, x19, [%[regs_base], #0x90]\n\t"
			     "stp x20, x21, [%[regs_base], #0xA0]\n\t"
			     "stp x22, x23, [%[regs_base], #0xB0]\n\t"
			     "stp x24, x25, [%[regs_base], #0xC0]\n\t"
			     "stp x26, x27, [%[regs_base], #0xD0]\n\t"
			     "stp x28, x29, [%[regs_base], #0xE0]\n\t"
			     "mov %[tmp], sp\n\t"
			     "stp x30, %[tmp], [%[regs_base], #0xF0]\n\t"
			     "adr %[pc], 1f\n\t"
			     "mrs %[cpsr], nzcv\n\t"
			     "mrs %[tmp], daif\n\t"
			     "orr %[cpsr], %[cpsr], %[tmp]\n\t"
			     "mrs %[tmp], CurrentEL\n\t"
			     "orr %[cpsr], %[cpsr], %[tmp]\n\t"
			     "mrs %[tmp], SPSel\n\t"
			     "orr %[cpsr], %[cpsr], %[tmp]\n\t"
			     "stp %[pc], %[cpsr], [%[regs_base], #0x100]\n\t"
			     "1:":[pc] "=r"(core_reg->pc),
			     [cpsr] "=r"(core_reg->pstate), [tmp] "=r"(tmp) :
			     [regs_base] "r"(core_reg)
			     : "memory");

	return;
}

static inline void sprd_debug_save_mmu_reg(struct sprd_debug_mmu_reg_t *mmu_reg)
{
	unsigned long tmp = 0;

	asm volatile ("mrs %1, sctlr_el1\n\t"
		      "str %1, [%0]\n\t"
		      "mrs %1, ttbr0_el1\n\t"
		      "str %1, [%0, #8]\n\t"
		      "mrs %1, ttbr1_el1\n\t"
		      "str %1, [%0, #0x10]\n\t"
		      "mrs %1, tcr_el1\n\t"
		      "str %1, [%0, #0x18]\n\t"
		      "mrs %1, mair_el1\n\t"
		      "str %1, [%0, #0x20]\n\t"
		      "mrs %1, amair_el1\n\t"
		      "str %1, [%0, #0x28]\n\t"
		      "mrs %1, contextidr_el1\n\t"
		      "str %1, [%0, #0x30]\n\t"::"r" (mmu_reg), "r"(tmp)
		      : "%0", "%1");
}

static inline void __flush_dcache_all(void)
{
	__asm__ __volatile__("dmb sy\n\t"
			"mrs x0, clidr_el1\n\t"
			"and x3, x0, #0x7000000\n\t"
			"lsr x3, x3, #23\n\t"
			"cbz x3, 5f\n\t"
			"mov x10, #0\n\t"
		"1:\n\t"
			"add x2, x10, x10, lsr #1\n\t"
			"lsr x1, x0, x2\n\t"
			"and x1, x1, #7\n\t"
			"cmp x1, #2\n\t"
			"b.lt    4f\n\t"
			"mrs x9, daif\n\t"
			"msr daifset, #2\n\t"
			"msr csselr_el1, x10\n\t"
			"isb\n\t"
			"mrs x1, ccsidr_el1\n\t"
			"msr daif, x9\n\t"
			"and x2, x1, #7\n\t"
			"add x2, x2, #4\n\t"
			"mov x4, #0x3ff\n\t"
			"and x4, x4, x1, lsr #3\n\t"
			"clz w5, w4\n\t"
			"mov x7, #0x7fff\n\t"
			"and x7,x7, x1, lsr #13\n\t"
		"2:\n\t"
			"mov x9, x4\n\t"
		"3:\n\t"
			"lsl x6, x9, x5\n\t"
			"orr x11, x10, x6\n\t"
			"lsl x6, x7, x2\n\t"
			"orr x11, x11, x6\n\t"
			"dc  cisw, x11\n\t"
			"subs    x9, x9, #1\n\t"
			"b.ge    3b\n\t"
			"subs    x7, x7, #1\n\t"
			"b.ge    2b\n\t"
		"4:\n\t"
			"add x10, x10, #2\n\t"
			"cmp x3, x10\n\t"
			"b.gt    1b\n\t"
		"5:\n\t"
			"mov x10, #0\n\t"
			"msr csselr_el1, x10\n\t"
			"dsb sy\n\t"
			"isb\n\t");

}

#define Elf_Off Elf64_Off

#define instruction_return(regs)		(regs->regs[30])

#define flush_cache_all()			__flush_dcache_all()

#endif
