/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "ipanic.h"

char NativeInfo[MAX_NATIVEINFO];	/* check that 32k is enought?? */
unsigned long User_Stack[MAX_NATIVEHEAP];	/* 8K Heap */

void _LOG(const char *fmt, ...)
{
	char buf[256];
	int len = 0;
	va_list ap;

	va_start(ap, fmt);
	len = strlen(NativeInfo);
	if ((len + sizeof(buf)) < MAX_NATIVEINFO)
		vsnprintf(&NativeInfo[len], sizeof(buf), fmt, ap);
	va_end(ap);

}

void dump_registers(struct pt_regs *r)
{
	if (r != NULL) {
		_LOG(" r0 %08x  r1 %08x  r2 %08x  r3 %08x\n",
		     r->ARM_r0, r->ARM_r1, r->ARM_r2, r->ARM_r3);
		_LOG(" r4 %08x  r5 %08x  r6 %08x  r7 %08x\n",
		     r->ARM_r4, r->ARM_r5, r->ARM_r6, r->ARM_r7);
		_LOG(" r8 %08x  r9 %08x  10 %08x  fp %08x\n",
		     r->ARM_r8, r->ARM_r9, r->ARM_r10, r->ARM_fp);
		_LOG(" ip %08x  sp %08x  lr %08x  pc %08x  cpsr %08x\n",
		     r->ARM_ip, r->ARM_sp, r->ARM_lr, r->ARM_pc, r->ARM_cpsr);
	}
}

int DumpNativeInfo(void)
{
	struct task_struct *current_task;
	struct pt_regs *user_ret;
	struct vm_area_struct *vma;
	unsigned long userstack_start = 0;
	unsigned long userstack_end = 0, length = 0;
	int mapcount = 0;
	struct file *file;
	int flags;
	struct mm_struct *mm;
	int ret = 0, i = 0;

	current_task = get_current();
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret))
		return 0;

	dump_registers(user_ret);

	LOGI("pc/lr/sp 0x%08lx/0x%08lx/0x%08lx\n", user_ret->ARM_pc, user_ret->ARM_lr,
	     user_ret->ARM_sp);
	_LOG("pc/lr/sp 0x%08lx/0x%08lx/0x%08lx\n", user_ret->ARM_pc, user_ret->ARM_lr,
	     user_ret->ARM_sp);
	userstack_start = (unsigned long)user_ret->ARM_sp;

	if (current_task->mm == NULL)
		return 0;

	vma = current_task->mm->mmap;
	while (vma && (mapcount < current_task->mm->map_count)) {
		file = vma->vm_file;
		flags = vma->vm_flags;
		if (file) {
			_LOG("%08lx-%08lx %c%c%c%c    %s\n", vma->vm_start, vma->vm_end,
			     flags & VM_READ ? 'r' : '-',
			     flags & VM_WRITE ? 'w' : '-',
			     flags & VM_EXEC ? 'x' : '-',
			     flags & VM_MAYSHARE ? 's' : 'p', file->f_path.dentry->d_iname);
		} else {
			const char *name = arch_vma_name(vma);

			mm = vma->vm_mm;
			if (!name) {
				if (mm) {
					if (vma->vm_start <= mm->start_brk &&
					    vma->vm_end >= mm->brk) {
						name = "[heap]";
					} else if (vma->vm_start <= mm->start_stack &&
						   vma->vm_end >= mm->start_stack) {
						name = "[stack]";
					}
				} else {
					name = "[vdso]";
				}
			}
			/* if (name) */
			{

				_LOG("%08lx-%08lx %c%c%c%c    %s\n", vma->vm_start, vma->vm_end,
				     flags & VM_READ ? 'r' : '-',
				     flags & VM_WRITE ? 'w' : '-',
				     flags & VM_EXEC ? 'x' : '-',
				     flags & VM_MAYSHARE ? 's' : 'p', name);


			}
		}
		vma = vma->vm_next;
		mapcount++;

	}
	vma = current_task->mm->mmap;
	while (vma != NULL) {
		if (vma->vm_start <= userstack_start && vma->vm_end >= userstack_start) {
			userstack_end = vma->vm_end;
			break;
		}
		vma = vma->vm_next;
		if (vma == current_task->mm->mmap)
			break;
	}
	if (userstack_end == 0) {
		LOGE("Dump native stack failed:\n");
		return 0;
	}

	LOGI("Dump stack range (0x%08lx:0x%08lx)\n", userstack_start, userstack_end);
	_LOG("Dump stack range (0x%08lx:0x%08lx)\n", userstack_start, userstack_end);

	length =
	    ((userstack_end - userstack_start) <
	     (sizeof(User_Stack) - 1)) ? (userstack_end - userstack_start) : (sizeof(User_Stack) -
									      1);
	ret = copy_from_user((void *)(User_Stack), (const void __user *)(userstack_start), length);
	LOGI("copy_from_user ret(0x%08x),len:%lx\n", ret, length);
	i = 0;
	while ((i < (length / 4)) && (userstack_start < userstack_end)) {
		_LOG("0x%08x: 0x%08x\n", userstack_start, User_Stack[i]);
		userstack_start += 4;
		i++;
	}

	LOGD("end dump native stack:\n");
	_LOG("end dump native stack:\n");
	return 0;
}

void aee_dumpnative(void)
{
	char *printk_buf = NativeInfo;
	int log_length = 0;

	memset(NativeInfo, 0, MAX_NATIVEINFO);
	DumpNativeInfo();
	log_length = strlen(NativeInfo);
	LOGD("\n DumpNativeInfo : %d\n", log_length);
	/* printk temporary buffer printk_buf[1024]. To avoid char loss, add 4 bytes here */
	while (log_length > 0) {
		LOGE("%s", printk_buf);
		printk_buf += 1020;
		log_length -= 1020;
	}
}
