// SPDX-License-Identifier: GPL-2.0-only
/*
 * sysdump_x86_64.h - Unisoc platform header
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

#ifndef __MACH_SYSDUMP_X86_64_H
#define __MACH_SYSDUMP_X86_64_H

#include <asm/mv/mobilevisor.h>

struct desc_ptrg {
	unsigned short size;
	unsigned long address;
} __attribute__((packed));

struct sprd_debug_mmu_reg_t {
	struct desc_ptrg idt;
	struct desc_ptrg gdt;
};

/* X86 CORE regs mapping structure */
struct sprd_debug_core_t {
	/* COMMON */
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long rbp;
	unsigned long rbx;
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long rax;
	unsigned long rcx;
	unsigned long rdx;
	unsigned long rsi;
	unsigned long rdi;
	unsigned long rip;
	unsigned long cs;
	unsigned long rflags;
	unsigned long rsp;
	unsigned long ss;

	/* control register  */
	unsigned long CR0;
	unsigned long CR2;
	unsigned long CR3;
	unsigned long CR4;

	unsigned long IA32_EFER;
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
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
	} else {
		asm volatile("movq %%rbx,%0" : "=m"(newregs->bx));
		asm volatile("movq %%rcx,%0" : "=m"(newregs->cx));
		asm volatile("movq %%rdx,%0" : "=m"(newregs->dx));
		asm volatile("movq %%rsi,%0" : "=m"(newregs->si));
		asm volatile("movq %%rdi,%0" : "=m"(newregs->di));
		asm volatile("movq %%rbp,%0" : "=m"(newregs->bp));
		asm volatile("movq %%rax,%0" : "=m"(newregs->ax));
		asm volatile("movq %%rsp,%0" : "=m"(newregs->sp));
		asm volatile("movq %%r8,%0" : "=m"(newregs->r8));
		asm volatile("movq %%r9,%0" : "=m"(newregs->r9));
		asm volatile("movq %%r10,%0" : "=m"(newregs->r10));
		asm volatile("movq %%r11,%0" : "=m"(newregs->r11));
		asm volatile("movq %%r12,%0" : "=m"(newregs->r12));
		asm volatile("movq %%r13,%0" : "=m"(newregs->r13));
		asm volatile("movq %%r14,%0" : "=m"(newregs->r14));
		asm volatile("movq %%r15,%0" : "=m"(newregs->r15));
		asm volatile("movl %%ss, %%eax;" : "=a"(newregs->ss));
		asm volatile("movl %%cs, %%eax;" : "=a"(newregs->cs));
		asm volatile("pushfq; popq %0" : "=m"(newregs->flags));
		newregs->ip = (unsigned long)current_text_addr();
	}
}

/* core reg dump function*/
static inline void sprd_debug_save_core_reg(struct sprd_debug_core_t *core_reg)
{
	asm volatile("movq %%rbx,%0" : "=m"(core_reg->rbx));
	asm volatile("movq %%rcx,%0" : "=m"(core_reg->rcx));
	asm volatile("movq %%rdx,%0" : "=m"(core_reg->rdx));
	asm volatile("movq %%rsi,%0" : "=m"(core_reg->rsi));
	asm volatile("movq %%rdi,%0" : "=m"(core_reg->rdi));
	asm volatile("movq %%rbp,%0" : "=m"(core_reg->rbp));
	asm volatile("movq %%rax,%0" : "=m"(core_reg->rax));
	asm volatile("movq %%rsp,%0" : "=m"(core_reg->rsp));
	asm volatile("movq %%r8,%0" : "=m"(core_reg->r8));
	asm volatile("movq %%r9,%0" : "=m"(core_reg->r9));
	asm volatile("movq %%r10,%0" : "=m"(core_reg->r10));
	asm volatile("movq %%r11,%0" : "=m"(core_reg->r11));
	asm volatile("movq %%r12,%0" : "=m"(core_reg->r12));
	asm volatile("movq %%r13,%0" : "=m"(core_reg->r13));
	asm volatile("movq %%r14,%0" : "=m"(core_reg->r14));
	asm volatile("movq %%r15,%0" : "=m"(core_reg->r15));
	asm volatile("movl %%ss, %%eax;" : "=a"(core_reg->ss));
	asm volatile("movl %%cs, %%eax;" : "=a"(core_reg->cs));
	asm volatile("pushfq; popq %0" : "=m"(core_reg->rflags));
	core_reg->rip = (unsigned long)current_text_addr();

	asm volatile("movq %%cr0,%0" : "=r"(core_reg->CR0));
	asm volatile("movq %%cr2,%0" : "=r"(core_reg->CR2));
	asm volatile("movq %%cr3,%0" : "=r"(core_reg->CR3));
	asm volatile("movq %%cr4,%0" : "=r"(core_reg->CR4));

	rdmsrl(MSR_EFER, core_reg->IA32_EFER);
}

static inline void sprd_debug_save_mmu_reg(struct sprd_debug_mmu_reg_t *mmu_reg)
{
	asm volatile("sgdt %0":"=m"(mmu_reg->gdt));
	asm volatile("sidt %0":"=m"(mmu_reg->idt));
}

#define instruction_return(regs)     (regs->regs[30])

#define Elf_Off Elf64_Off

/* flush cache in X86 */
#undef flush_cache_all
#define flush_cache_all()	do { mb(); wbinvd(); mb(); } while (0)

#endif
