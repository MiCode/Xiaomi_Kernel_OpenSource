// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/stacktrace.h>

#ifdef __aarch64__
#include <asm/pointer_auth.h>
#else
#include <asm/unwind.h>
#endif

#include "hang_unwind.h"

#ifdef __aarch64__
unsigned int hang_kernel_trace(struct task_struct *tsk,
					unsigned long *store, unsigned int size)
{
	struct stackframe frame;
	struct stack_info info;
	unsigned long fp;
	unsigned int store_len = 1;

	if (tsk == current)
		fp = (unsigned long)__builtin_frame_address(0);
	else
		fp = thread_saved_fp(tsk);
	frame.fp = fp;
	frame.pc = thread_saved_pc(tsk);
	if (!frame.pc) {
		pr_info("err stack:%lx\n", thread_saved_sp(tsk));
		return 0;
	}
	*store = frame.pc;
	while(store_len < size) {
		if (!on_task_stack(tsk, fp, &info) || !IS_ALIGNED(fp, 8))
			break;
		frame.fp = READ_ONCE_NOCHECK(*(unsigned long *)(fp));
		frame.pc = READ_ONCE_NOCHECK(*(unsigned long *)(fp + 8));
		fp = frame.fp;
		if (!frame.pc)
			continue;
		frame.pc = ptrauth_strip_insn_pac(frame.pc);
		*(++store) = frame.pc;
		store_len += 1;
	}
	return store_len;
}
EXPORT_SYMBOL(hang_kernel_trace);

const char *hang_arch_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}
#else /* __aarch64__ */
unsigned int hang_kernel_trace(struct task_struct *tsk,
					unsigned long *store, unsigned int size)
{
#if IS_ENABLED(CONFIG_ARM_UNWIND)
	struct stackframe frame;
	unsigned int store_len = 1;

	if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_stack_pointer;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)unwind_backtrace;
	} else {
		/* task blocked in __switch_to */
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		/*
		 * The function calling __switch_to cannot be a leaf function
		 * so LR is recovered from the stack.
		 */
		frame.lr = 0;
		frame.pc = thread_saved_pc(tsk);
	}
	*store = frame.pc;
	while (store_len < size) {
		int urc;

		urc = unwind_frame(&frame);
		if (urc < 0)
			break;
		*(++store) = frame.pc;
		store_len += 1;
	}
	return store_len;
#else
	return 0;
#endif
}

#ifdef MODULE
const char *hang_arch_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}
#else
const char *hang_arch_vma_name(struct vm_area_struct *vma)
{
	return arch_vma_name(vma);
}
#endif

#endif /* __aarch64__ */

EXPORT_SYMBOL(hang_arch_vma_name);

