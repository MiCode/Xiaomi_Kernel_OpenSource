/*
 * drivers/misc/tegra-profiler/backtrace.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "backtrace.h"
#include "eh_unwind.h"

#define QUADD_USER_SPACE_MIN_ADDR	0x8000

void
quadd_callchain_store(struct quadd_callchain *cc,
		      quadd_bt_addr_t ip)
{
	if (ip && cc->nr < QUADD_MAX_STACK_DEPTH)
		cc->ip[cc->nr++] = ip;
}

static unsigned long __user *
user_backtrace(unsigned long __user *tail,
	       struct quadd_callchain *cc,
	       struct vm_area_struct *stack_vma)
{
	unsigned long value, value_lr = 0, value_fp = 0;
	unsigned long __user *fp_prev = NULL;

	if (!is_vma_addr((unsigned long)tail, stack_vma))
		return NULL;

	if (__copy_from_user_inatomic(&value, tail, sizeof(unsigned long)))
		return NULL;

	if (is_vma_addr(value, stack_vma)) {
		/* gcc thumb/clang frame */
		value_fp = value;

		if (!is_vma_addr((unsigned long)(tail + 1), stack_vma))
			return NULL;

		if (__copy_from_user_inatomic(&value_lr, tail + 1,
					      sizeof(value_lr)))
			return NULL;
	} else {
		/* gcc arm frame */
		if (__copy_from_user_inatomic(&value_fp, tail - 1,
					      sizeof(value_fp)))
			return NULL;

		if (!is_vma_addr(value_fp, stack_vma))
			return NULL;

		value_lr = value;
	}

	fp_prev = (unsigned long __user *)value_fp;

	if (value_lr < QUADD_USER_SPACE_MIN_ADDR)
		return NULL;

	quadd_callchain_store(cc, value_lr);

	if (fp_prev <= tail)
		return NULL;

	return fp_prev;
}

static unsigned int
get_user_callchain_fp(struct pt_regs *regs,
		      struct quadd_callchain *cc)
{
	unsigned long fp, sp, pc, reg;
	struct vm_area_struct *vma, *vma_pc;
	unsigned long __user *tail = NULL;
	struct mm_struct *mm = current->mm;

	cc->nr = 0;
	cc->unw_method = QUADD_UNW_METHOD_FP;

	if (!regs || !mm)
		return 0;

	if (thumb_mode(regs))
		fp = regs->ARM_r7;
	else
		fp = regs->ARM_fp;

	sp = user_stack_pointer(regs);
	pc = instruction_pointer(regs);

	if (fp == 0 || fp < sp || fp & 0x3)
		return 0;

	vma = find_vma(mm, sp);
	if (!vma)
		return 0;

	if (!is_vma_addr(fp, vma))
		return 0;

	if (probe_kernel_address(fp, reg)) {
		pr_warn_once("frame error: sp/fp: %#lx/%#lx, pc/lr: %#lx/%#lx, vma: %#lx-%#lx\n",
			     sp, fp, pc, regs->ARM_lr,
			     vma->vm_start, vma->vm_end);
		return 0;
	}

	if (thumb_mode(regs)) {
		if (reg <= fp || !is_vma_addr(reg, vma))
			return 0;
	} else if (reg > fp && is_vma_addr(reg, vma)) {
		/* fp --> fp prev */
		unsigned long value;
		int read_lr = 0;

		if (is_vma_addr(fp + sizeof(unsigned long), vma)) {
			if (__copy_from_user_inatomic(
					&value,
					(unsigned long __user *)fp + 1,
					sizeof(unsigned long)))
				return 0;

			vma_pc = find_vma(mm, pc);
			read_lr = 1;
		}

		if (!read_lr || !is_vma_addr(value, vma_pc)) {
			/* gcc: fp --> short frame tail (fp) */

			if (regs->ARM_lr < QUADD_USER_SPACE_MIN_ADDR)
				return 0;

			quadd_callchain_store(cc, regs->ARM_lr);
			tail = (unsigned long __user *)reg;
		}
	}

	if (!tail)
		tail = (unsigned long __user *)fp;

	while (tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace(tail, cc, vma);

	return cc->nr;
}

unsigned int
quadd_get_user_callchain(struct pt_regs *regs,
			 struct quadd_callchain *cc,
			 struct quadd_ctx *ctx)
{
	int unw_fp, unw_eht, nr = 0;
	unsigned int extra;
	struct quadd_parameters *param = &ctx->param;

	cc->nr = 0;

	if (!regs)
		return 0;

	extra = param->reserved[QUADD_PARAM_IDX_EXTRA];

	unw_fp = extra & QUADD_PARAM_EXTRA_BT_FP;
	unw_eht = extra & QUADD_PARAM_EXTRA_BT_UNWIND_TABLES;

	cc->unw_rc = 0;

	if (unw_eht)
		nr = quadd_get_user_callchain_ut(regs, cc);

	if (!nr && unw_fp)
		nr = get_user_callchain_fp(regs, cc);

	return nr;
}
