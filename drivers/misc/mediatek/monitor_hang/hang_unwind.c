// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#ifdef __aarch64__
#include <asm/pointer_auth.h>
#endif
#include <asm/stacktrace.h>
#include "hang_unwind.h"


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
#ifdef __aarch64__
		frame.pc = ptrauth_strip_insn_pac(frame.pc);
#endif
		*(++store) = frame.pc;
		store_len += 1;
	}
	return store_len;
}
EXPORT_SYMBOL(hang_kernel_trace);

#ifdef __aarch64__

const char *hang_arch_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}

#else

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

#endif

EXPORT_SYMBOL(hang_arch_vma_name);
