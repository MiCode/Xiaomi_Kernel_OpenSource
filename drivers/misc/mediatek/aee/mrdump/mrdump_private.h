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

#if !defined(__MRDUMP_PRIVATE_H__)
#define __MRDUMP_PRIVATE_H__

#include <asm/cputype.h>
#include <asm/memory.h>
#include <asm/smp_plat.h>
#include <asm/cputype.h>
#include <asm-generic/sections.h>

#ifdef __aarch64__
#define mrdump_virt_addr_valid(kaddr)	\
	((((void *)(kaddr) >= (void *)PAGE_OFFSET && \
	(void *)(kaddr) < (void *)high_memory) || \
	((void *)(kaddr) >= (void *)KIMAGE_VADDR && \
	(void *)(kaddr) < (void *)_end)) && \
	pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#else
#define mrdump_virt_addr_valid(kaddr)	\
	((void *)(kaddr) >= (void *)PAGE_OFFSET && \
	(void *)(kaddr) < (void *)high_memory && \
	pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif

#ifdef CONFIG_ARM64
static inline int get_HW_cpuid(void)
{
	u64 mpidr;
	u32 id;

	mpidr = read_cpuid_mpidr();
	id = get_logical_index(mpidr & MPIDR_HWID_BITMASK);

	return id;
}
#else
static inline int get_HW_cpuid(void)
{
	int id;

	asm("mrc     p15, 0, %0, c0, c0, 5 @ Get CPUID\n":"=r"(id));
	return (id & 0x3) + ((id & 0xF00) >> 6);
}
#endif

struct pt_regs;

extern struct mrdump_rsvmem_block mrdump_sram_cb;
extern struct mrdump_control_block *mrdump_cblock;
extern const unsigned long kallsyms_addresses[] __weak;
extern const u8 kallsyms_names[] __weak;
extern const u8 kallsyms_token_table[] __weak;
extern const u16 kallsyms_token_index[] __weak;
extern const unsigned long kallsyms_markers[] __weak;
extern const unsigned long kallsyms_num_syms
__attribute__((weak, section(".rodata")));


int mrdump_hw_init(void);
void mrdump_cblock_init(void);
int mrdump_full_init(void);
int mrdump_wdt_init(void);

void mrdump_save_current_backtrace(struct pt_regs *regs);
void mrdump_save_control_register(void *creg);

extern int mrdump_rsv_conflict;
extern void dis_D_inner_fL1L2(void);
extern void __inner_flush_dcache_all(void);
extern void mrdump_mini_add_entry(unsigned long addr, unsigned long size);

int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom,
				unsigned long top);

static inline void mrdump_mini_save_regs(struct pt_regs *regs)
{
#ifdef __aarch64__
	__asm__ volatile ("stp	x0, x1, [sp,#-16]!\n\t"
			  "1: mov	x1, %0\n\t"
			  "add	x0, x1, #16\n\t"
			  "stp	x2, x3, [x0],#16\n\t"
			  "stp	x4, x5, [x0],#16\n\t"
			  "stp	x6, x7, [x0],#16\n\t"
			  "stp	x8, x9, [x0],#16\n\t"
			  "stp	x10, x11, [x0],#16\n\t"
			  "stp	x12, x13, [x0],#16\n\t"
			  "stp	x14, x15, [x0],#16\n\t"
			  "stp	x16, x17, [x0],#16\n\t"
			  "stp	x18, x19, [x0],#16\n\t"
			  "stp	x20, x21, [x0],#16\n\t"
			  "stp	x22, x23, [x0],#16\n\t"
			  "stp	x24, x25, [x0],#16\n\t"
			  "stp	x26, x27, [x0],#16\n\t"
			  "ldr	x1, [x29]\n\t"
			  "stp	x28, x1, [x0],#16\n\t"
			  "mov	x1, sp\n\t"
			  "stp	x30, x1, [x0],#16\n\t"
			  "mrs	x1, daif\n\t"
			  "adr	x30, 1b\n\t"
			  "stp	x30, x1, [x0],#16\n\t"
			  "sub	x1, x0, #272\n\t"
			  "ldr	x0, [sp]\n\t"
			  "str	x0, [x1]\n\t"
			  "ldr	x0, [sp, #8]\n\t"
			  "str	x0, [x1, #8]\n\t"
			  "ldp	x0, x1, [sp],#16\n\t" :  : "r" (regs) : "cc");
#else
	asm volatile ("stmia %1, {r0 - r15}\n\t"
		      "mrs %0, cpsr\n":"=r"
		      (regs->uregs[16]) : "r"(regs) : "memory");
#endif
}

extern void aee_rr_rec_kaslr_offset(uint64_t offset);
#if defined(CONFIG_RANDOMIZE_BASE) && defined(CONFIG_ARM64)
static inline void show_kaslr(void)
{
	u64 const kaslr_offset = kimage_vaddr - KIMAGE_VADDR;

	pr_notice("Kernel Offset: 0x%llx from 0x%lx\n",
			kaslr_offset, KIMAGE_VADDR);
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_kaslr_offset(kaslr_offset);
#endif
}
#else
static inline void show_kaslr(void)
{
	pr_notice("Kernel Offset: disabled\n");
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_kaslr_offset(0);
#endif
}
#endif

int in_fiq_handler(void);

#endif /* __MRDUMP_PRIVATE_H__ */
