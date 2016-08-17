/*
 * drivers/misc/tegra-profiler/backtrace.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <asm-generic/uaccess.h>

#include <linux/tegra_profiler.h>

#include "backtrace.h"

#define QUADD_USER_SPACE_MIN_ADDR	0x8000

static inline void
quadd_callchain_store(struct quadd_callchain *callchain_data, u32 ip)
{
	if (callchain_data->nr < QUADD_MAX_STACK_DEPTH) {
		/* pr_debug("[%d] Add entry: %#llx\n",
			    callchain_data->nr, ip); */
		callchain_data->callchain[callchain_data->nr++] = ip;
	}
}

static int
check_vma_address(unsigned long addr, struct vm_area_struct *vma)
{
	unsigned long start, end;

	if (vma) {
		start = vma->vm_start;
		end = vma->vm_end;
		if (addr >= start && addr + sizeof(unsigned long) <= end)
			return 0;
	}
	return -EINVAL;
}

static unsigned long __user *
user_backtrace(unsigned long __user *tail,
	       struct quadd_callchain *callchain_data,
	       struct vm_area_struct *stack_vma)
{
	unsigned long value, value_lr = 0, value_fp = 0;
	unsigned long __user *fp_prev = NULL;

	if (check_vma_address((unsigned long)tail, stack_vma))
		return NULL;

	if (__copy_from_user_inatomic(&value, tail, sizeof(unsigned long)))
		return NULL;

	if (!check_vma_address(value, stack_vma)) {
		/* clang's frame */
		value_fp = value;

		if (check_vma_address((unsigned long)(tail + 1), stack_vma))
			return NULL;

		if (__copy_from_user_inatomic(&value_lr, tail + 1,
					      sizeof(unsigned long)))
			return NULL;
	} else {
		/* gcc's frame */
		if (__copy_from_user_inatomic(&value_fp, tail - 1,
					      sizeof(unsigned long)))
			return NULL;

		if (check_vma_address(value_fp, stack_vma))
			return NULL;

		value_lr = value;
	}

	fp_prev = (unsigned long __user *)value_fp;

	if (value_lr < QUADD_USER_SPACE_MIN_ADDR)
		return NULL;

	quadd_callchain_store(callchain_data, value_lr);

	if (fp_prev <= tail)
		return NULL;

	return fp_prev;
}

unsigned int
quadd_get_user_callchain(struct pt_regs *regs,
			 struct quadd_callchain *callchain_data)
{
	unsigned long fp, sp, pc, reg;
	struct vm_area_struct *vma, *vma_pc;
	unsigned long __user *tail = NULL;
	struct mm_struct *mm = current->mm;

	callchain_data->nr = 0;

	if (!regs || !user_mode(regs) || !mm)
		return 0;

	if (thumb_mode(regs))
		return 0;

	fp = regs->ARM_fp;
	sp = regs->ARM_sp;
	pc = regs->ARM_pc;

	if (fp == 0 || fp < sp || fp & 0x3)
		return 0;

	vma = find_vma(mm, sp);
	if (check_vma_address(fp, vma))
		return 0;

	if (__copy_from_user_inatomic(&reg, (unsigned long __user *)fp,
				      sizeof(unsigned long)))
		return 0;

	if (reg > fp &&
	    !check_vma_address(reg, vma)) {
		unsigned long value;
		int read_lr = 0;

		if (!check_vma_address(fp + sizeof(unsigned long), vma)) {
			if (__copy_from_user_inatomic(
					&value,
					(unsigned long __user *)fp + 1,
					sizeof(unsigned long)))
				return 0;

			vma_pc = find_vma(mm, pc);
			read_lr = 1;
		}

		if (!read_lr || check_vma_address(value, vma_pc)) {
			/* gcc: fp --> short frame tail (fp) */

			if (regs->ARM_lr < QUADD_USER_SPACE_MIN_ADDR)
				return 0;

			quadd_callchain_store(callchain_data, regs->ARM_lr);
			tail = (unsigned long __user *)reg;
		}
	}

	if (!tail)
		tail = (unsigned long __user *)fp;

	while (tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace(tail, callchain_data, vma);

	return callchain_data->nr;
}
