// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/ptrace.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

#include <mt-plat/mrdump.h>

#include "mrdump_private.h"

void mrdump_save_control_register(void *creg)
{
	struct aarch64_ctrl_regs *cregs = (struct aarch64_ctrl_regs *)creg;
	u64 tmp;

	asm volatile ("mrs %0, sctlr_el1\n\t"
		      "mrs %1, tcr_el1\n\t"
		      "mrs %2, ttbr0_el1\n\t"
		      "mrs %3, ttbr1_el1\n\t"
		      "mrs %4, sp_el0\n\t"
		      "mov %5, sp\n\t"
		      : "=&r"(tmp), "=r"(cregs->tcr_el1),
		      "=r"(cregs->ttbr0_el1), "=r"(cregs->ttbr1_el1),
		      "=r"(cregs->sp_el[0]), "=r"(cregs->sp_el[1])
		      : : "memory");
	cregs->sctlr_el1 = (uint64_t) tmp;
}

void mrdump_arch_fill_machdesc(struct mrdump_machdesc *machdesc_p)
{
	machdesc_p->master_page_table = read_sysreg(ttbr1_el1) & ~(TTBR_ASID_MASK | TTBR_CNP_BIT);
	machdesc_p->tcr_el1_t1sz = (uint64_t)(read_sysreg(tcr_el1) & TCR_T1SZ_MASK)
		>> TCR_T1SZ_OFFSET;
	machdesc_p->kernel_pac_mask = (uint64_t)system_supports_address_auth() ?
		ptrauth_kernel_pac_mask() : 0;
	machdesc_p->kimage_voffset = (unsigned long)kimage_voffset;
}

static void print_pstate(const struct pt_regs *regs)
{
	u64 pstate = regs->pstate;

	if (compat_user_mode(regs)) {
		pr_info("pstate: %08llx (%c%c%c%c %c %s %s %c%c%c)\n",
			pstate,
			pstate & PSR_AA32_N_BIT ? 'N' : 'n',
			pstate & PSR_AA32_Z_BIT ? 'Z' : 'z',
			pstate & PSR_AA32_C_BIT ? 'C' : 'c',
			pstate & PSR_AA32_V_BIT ? 'V' : 'v',
			pstate & PSR_AA32_Q_BIT ? 'Q' : 'q',
			pstate & PSR_AA32_T_BIT ? "T32" : "A32",
			pstate & PSR_AA32_E_BIT ? "BE" : "LE",
			pstate & PSR_AA32_A_BIT ? 'A' : 'a',
			pstate & PSR_AA32_I_BIT ? 'I' : 'i',
			pstate & PSR_AA32_F_BIT ? 'F' : 'f');
	} else {
		pr_info("pstate: %08llx (%c%c%c%c %c%c%c%c %cPAN %cUAO)\n",
			pstate,
			pstate & PSR_N_BIT ? 'N' : 'n',
			pstate & PSR_Z_BIT ? 'Z' : 'z',
			pstate & PSR_C_BIT ? 'C' : 'c',
			pstate & PSR_V_BIT ? 'V' : 'v',
			pstate & PSR_D_BIT ? 'D' : 'd',
			pstate & PSR_A_BIT ? 'A' : 'a',
			pstate & PSR_I_BIT ? 'I' : 'i',
			pstate & PSR_F_BIT ? 'F' : 'f',
			pstate & PSR_PAN_BIT ? '+' : '-',
			pstate & PSR_UAO_BIT ? '+' : '-');
	}
}

#define MEM_FMT "%04lx: %08x %08x %08x %08x %08x %08x %08x %08x\n"
#define MEM_RANGE (128)
static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int i, j, invalid, nlines;
	u32 *p, data[8] = {0};

	if (addr < (UL(0xffffffffffffffff) - (UL(1) << VA_BITS) + 1) ||
			addr > UL(0xFFFFFFFFFFFFF000))
		return;

	pr_info("%s: %#lx:\n", name, addr);
	addr -= MEM_RANGE;
	p = (u32 *)(addr & (~0xfUL));
	nbytes += (addr & 0xf);
	nlines = (nbytes + 31) / 32;
	for (i = 0; i < nlines; i++, p += 8) {
		for (j = invalid = 0; j < 8; j++) {
			if (copy_from_kernel_nofault(&data[j], &p[j],
						     sizeof(data[0]))) {
				data[j] = 0x12345678;
				invalid++;
			}
		}
		if (invalid != 8)
			pr_info(MEM_FMT, (unsigned long)p & 0xffff,
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]);
	}
}

void mrdump_arch_show_regs(const struct pt_regs *regs)
{
	int i, top_reg;
	u64 lr, sp;

	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 12;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}

	print_pstate(regs);

	if (!user_mode(regs)) {
		pr_info("pc : [0x%lx] %pS\n", regs->pc, (void *)regs->pc);
		pr_info("lr : [0x%lx] %pS\n", lr, (void *)lr);
	} else {
		pr_info("pc : %016llx\n", regs->pc);
		pr_info("lr : %016llx\n", lr);
	}

	pr_info("sp : %016llx\n", sp);

	if (system_uses_irq_prio_masking())
		pr_info("pmr_save: %08llx\n", regs->pmr_save);

	i = top_reg;

	while (i >= 1) {
		pr_info("x%-2d: %016llx x%-2d: %016llx\n",
			i, regs->regs[i], i - 1, regs->regs[i - 1]);
		i -= 2;
	}
	if (!user_mode(regs)) {
		unsigned int i;
#if IS_ENABLED(CONFIG_SET_FS)
		mm_segment_t fs;

		fs = get_fs();
		set_fs(KERNEL_DS);
#endif
		show_data(regs->pc, MEM_RANGE * 2, "PC");
		show_data(regs->regs[30], MEM_RANGE * 2, "LR");
		show_data(regs->sp, MEM_RANGE * 2, "SP");
		for (i = 0; i < 30; i++) {
			char name[4];

			if (snprintf(name, sizeof(name), "X%u", i) > 0)
				show_data(regs->regs[i], MEM_RANGE * 2, name);
		}
#if IS_ENABLED(CONFIG_SET_FS)
		set_fs(fs);
#endif
	}
	pr_info("\n");
}
