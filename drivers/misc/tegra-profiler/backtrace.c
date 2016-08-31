/*
 * drivers/misc/tegra-profiler/backtrace.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

static inline int
is_thumb_mode(struct pt_regs *regs)
{
#ifdef CONFIG_ARM64
	return compat_thumb_mode(regs);
#else
	return thumb_mode(regs);
#endif
}

unsigned long
quadd_user_stack_pointer(struct pt_regs *regs)
{
#ifdef CONFIG_ARM64
	if (compat_user_mode(regs))
		return regs->compat_sp;
#endif

	return user_stack_pointer(regs);
}

static inline unsigned long
get_user_frame_pointer(struct pt_regs *regs)
{
	unsigned long fp;

#ifdef CONFIG_ARM64
	if (compat_user_mode(regs))
		fp = is_thumb_mode(regs) ?
			regs->compat_usr(7) : regs->compat_usr(11);
	else
		fp = regs->regs[29];
#else
	fp = is_thumb_mode(regs) ? regs->ARM_r7 : regs->ARM_fp;
#endif
	return fp;
}

unsigned long
quadd_user_link_register(struct pt_regs *regs)
{
#ifdef CONFIG_ARM64
	return compat_user_mode(regs) ?
		regs->compat_lr : regs->regs[30];
#else
	return regs->ARM_lr;
#endif
}

int
quadd_callchain_store(struct quadd_callchain *cc,
			unsigned long ip)
{
	if (ip && cc->nr < QUADD_MAX_STACK_DEPTH) {
		if (cc->cs_64)
			cc->ip_64[cc->nr++] = ip;
		else
			cc->ip_32[cc->nr++] = ip;

		return 1;
	}
	return 0;
}

static unsigned long __user *
user_backtrace(unsigned long __user *tail,
		struct quadd_callchain *cc,
		struct vm_area_struct *stack_vma)
{
	unsigned long value, value_lr = 0, value_fp = 0;
	unsigned long __user *fp_prev = NULL;

	if (!is_vma_addr((unsigned long)tail, stack_vma, sizeof(*tail)))
		return NULL;

	if (__copy_from_user_inatomic(&value, tail, sizeof(unsigned long)))
		return NULL;

	if (is_vma_addr(value, stack_vma, sizeof(value))) {
		/* gcc thumb/clang frame */
		value_fp = value;

		if (!is_vma_addr((unsigned long)(tail + 1), stack_vma,
		    sizeof(*tail)))
			return NULL;

		if (__copy_from_user_inatomic(&value_lr, tail + 1,
						sizeof(value_lr)))
			return NULL;
	} else {
		/* gcc arm frame */
		if (__copy_from_user_inatomic(&value_fp, tail - 1,
						sizeof(value_fp)))
			return NULL;

		if (!is_vma_addr(value_fp, stack_vma, sizeof(value_fp)))
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
			struct quadd_callchain *cc,
			struct task_struct *task)
{
	unsigned long fp, sp, pc, reg;
	struct vm_area_struct *vma, *vma_pc;
	unsigned long __user *tail = NULL;
	struct mm_struct *mm = task->mm;

	cc->nr = 0;
	cc->unw_method = QUADD_UNW_METHOD_FP;

	if (!regs || !mm)
		return 0;

	sp = quadd_user_stack_pointer(regs);
	pc = instruction_pointer(regs);
	fp = get_user_frame_pointer(regs);

	if (fp == 0 || fp < sp || fp & 0x3)
		return 0;

	vma = find_vma(mm, sp);
	if (!vma)
		return 0;

	if (!is_vma_addr(fp, vma, sizeof(fp)))
		return 0;

	if (probe_kernel_address(fp, reg)) {
		pr_warn_once("frame error: sp/fp: %#lx/%#lx, pc/lr: %#lx/%#lx, vma: %#lx-%#lx\n",
			     sp, fp, pc, quadd_user_link_register(regs),
			     vma->vm_start, vma->vm_end);
		return 0;
	}

	if (is_thumb_mode(regs)) {
		if (reg <= fp || !is_vma_addr(reg, vma, sizeof(reg)))
			return 0;
	} else if (reg > fp && is_vma_addr(reg, vma, sizeof(reg))) {
		/* fp --> fp prev */
		unsigned long value;
		int read_lr = 0;

		if (is_vma_addr(fp + sizeof(unsigned long), vma, sizeof(fp))) {
			if (__copy_from_user_inatomic(
					&value,
					(unsigned long __user *)fp + 1,
					sizeof(unsigned long)))
				return 0;

			vma_pc = find_vma(mm, pc);
			read_lr = 1;
		}

		if (!read_lr || !is_vma_addr(value, vma_pc, sizeof(value))) {
			/* gcc: fp --> short frame tail (fp) */
			unsigned long lr = quadd_user_link_register(regs);

			if (lr < QUADD_USER_SPACE_MIN_ADDR)
				return 0;

			quadd_callchain_store(cc, lr);
			tail = (unsigned long __user *)reg;
		}
	}

	if (!tail)
		tail = (unsigned long __user *)fp;

	while (tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace(tail, cc, vma);

	return cc->nr;
}

static unsigned int
__user_backtrace(struct quadd_callchain *cc, struct task_struct *task)
{
	struct mm_struct *mm = task->mm;
	struct vm_area_struct *vma;
	unsigned long __user *tail;

	if (!mm)
		goto out;

	vma = find_vma(mm, cc->curr_sp);
	if (!vma)
		goto out;

	tail = (unsigned long __user *)cc->curr_fp;

	while (tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace(tail, cc, vma);

out:
	return cc->nr;
}

#ifdef CONFIG_ARM64
static u32 __user *
user_backtrace_compat(u32 __user *tail,
		struct quadd_callchain *cc,
		struct vm_area_struct *stack_vma)
{
	u32 value, value_lr = 0, value_fp = 0;
	u32 __user *fp_prev = NULL;

	if (!is_vma_addr((unsigned long)tail, stack_vma, sizeof(*tail)))
		return NULL;

	if (__copy_from_user_inatomic(&value, tail, sizeof(value)))
		return NULL;

	if (is_vma_addr(value, stack_vma, sizeof(value))) {
		/* gcc thumb/clang frame */
		value_fp = value;

		if (!is_vma_addr((unsigned long)(tail + 1), stack_vma,
		    sizeof(*tail)))
			return NULL;

		if (__copy_from_user_inatomic(&value_lr, tail + 1,
						sizeof(value_lr)))
			return NULL;
	} else {
		/* gcc arm frame */
		if (__copy_from_user_inatomic(&value_fp, tail - 1,
						sizeof(value_fp)))
			return NULL;

		if (!is_vma_addr(value_fp, stack_vma, sizeof(value_fp)))
			return NULL;

		value_lr = value;
	}

	fp_prev = (u32 __user *)(unsigned long)value_fp;

	if (value_lr < QUADD_USER_SPACE_MIN_ADDR)
		return NULL;

	quadd_callchain_store(cc, value_lr);

	if (fp_prev <= tail)
		return NULL;

	return fp_prev;
}

static unsigned int
get_user_callchain_fp_compat(struct pt_regs *regs,
			     struct quadd_callchain *cc,
			     struct task_struct *task)
{
	u32 fp, sp, pc, reg;
	struct vm_area_struct *vma, *vma_pc;
	u32 __user *tail = NULL;
	struct mm_struct *mm = task->mm;

	cc->nr = 0;

	if (!regs || !mm)
		return 0;

	sp = quadd_user_stack_pointer(regs);
	pc = instruction_pointer(regs);
	fp = get_user_frame_pointer(regs);

	if (fp == 0 || fp < sp || fp & 0x3)
		return 0;

	vma = find_vma(mm, sp);
	if (!vma)
		return 0;

	if (!is_vma_addr(fp, vma, sizeof(fp)))
		return 0;

	if (probe_kernel_address((unsigned long)fp, reg)) {
		pr_warn_once("frame error: sp/fp: %#x/%#x, pc/lr: %#x/%#x, vma: %#lx-%#lx\n",
			     sp, fp, pc, (u32)quadd_user_link_register(regs),
			     vma->vm_start, vma->vm_end);
		return 0;
	}

	if (is_thumb_mode(regs)) {
		if (reg <= fp || !is_vma_addr(reg, vma, sizeof(reg)))
			return 0;
	} else if (reg > fp && is_vma_addr(reg, vma, sizeof(reg))) {
		/* fp --> fp prev */
		u32 value;
		int read_lr = 0;

		if (is_vma_addr(fp + sizeof(u32), vma, sizeof(fp))) {
			if (__copy_from_user_inatomic(
					&value,
					(u32 __user *)(fp + sizeof(u32)),
					sizeof(value)))
				return 0;

			vma_pc = find_vma(mm, pc);
			read_lr = 1;
		}

		if (!read_lr || !is_vma_addr(value, vma_pc, sizeof(value))) {
			/* gcc: fp --> short frame tail (fp) */
			u32 lr = quadd_user_link_register(regs);

			if (lr < QUADD_USER_SPACE_MIN_ADDR)
				return 0;

			quadd_callchain_store(cc, lr);
			tail = (u32 __user *)(unsigned long)reg;
		}
	}

	if (!tail)
		tail = (u32 __user *)(unsigned long)fp;

	while (tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace_compat(tail, cc, vma);

	return cc->nr;
}

static unsigned int
__user_backtrace_compat(struct quadd_callchain *cc, struct task_struct *task)
{
	struct mm_struct *mm = task->mm;
	struct vm_area_struct *vma;
	u32 __user *tail;

	if (!mm)
		goto out;

	vma = find_vma(mm, cc->curr_sp);
	if (!vma)
		goto out;

	tail = (u32 __user *)cc->curr_fp;

	while (tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace_compat(tail, cc, vma);

out:
	return cc->nr;
}

#endif	/* CONFIG_ARM64 */

static unsigned int
__get_user_callchain_fp(struct pt_regs *regs,
			struct quadd_callchain *cc,
			struct task_struct *task)
{
	if (cc->nr > 0) {
		int nr, nr_prev = cc->nr;
#ifdef CONFIG_ARM64
		if (compat_user_mode(regs))
			nr = __user_backtrace_compat(cc, task);
		else
			nr = __user_backtrace(cc, task);
#else
		nr = __user_backtrace(cc, task);
#endif
		if (nr != nr_prev)
			cc->unw_method = QUADD_UNW_METHOD_MIXED;

		return nr;
	}

	cc->unw_method = QUADD_UNW_METHOD_FP;

#ifdef CONFIG_ARM64
	if (compat_user_mode(regs))
		return get_user_callchain_fp_compat(regs, cc, task);
#endif
	return get_user_callchain_fp(regs, cc, task);
}

unsigned int
quadd_get_user_callchain(struct pt_regs *regs,
			 struct quadd_callchain *cc,
			 struct quadd_ctx *ctx,
			 struct task_struct *task)
{
	int unw_fp, unw_eht, unw_mix, nr = 0;
	unsigned int extra;
	struct quadd_parameters *param = &ctx->param;

	cc->nr = 0;

	if (!regs)
		return 0;

	cc->curr_sp = 0;
	cc->curr_fp = 0;

#ifdef CONFIG_ARM64
	cc->cs_64 = compat_user_mode(regs) ? 0 : 1;
#else
	cc->cs_64 = 0;
#endif

	extra = param->reserved[QUADD_PARAM_IDX_EXTRA];

	unw_fp = extra & QUADD_PARAM_EXTRA_BT_FP;
	unw_eht = extra & QUADD_PARAM_EXTRA_BT_UNWIND_TABLES;
	unw_mix = extra & QUADD_PARAM_EXTRA_BT_MIXED;

	cc->unw_rc = 0;

	if (unw_eht)
		nr = quadd_get_user_callchain_ut(regs, cc, task);

	if (unw_fp) {
		if (!nr || unw_mix)
			nr = __get_user_callchain_fp(regs, cc, task);
	}

	return nr;
}
