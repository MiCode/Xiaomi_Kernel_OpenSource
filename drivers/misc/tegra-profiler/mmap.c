/*
 * drivers/misc/tegra-profiler/mmap.c
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

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/sched.h>

#include <linux/tegra_profiler.h>

#include "mmap.h"
#include "comm.h"
#include "hrt.h"

static void
put_mmap_sample(struct quadd_mmap_data *s, char *filename,
		size_t length, unsigned long pgoff)
{
	struct quadd_record_data r;
	struct quadd_iovec vec[2];
	u64 pgoff_val = pgoff << PAGE_SHIFT;

	r.record_type = QUADD_RECORD_TYPE_MMAP;

	memcpy(&r.mmap, s, sizeof(*s));
	r.mmap.filename_length = length;

	vec[0].base = &pgoff_val;
	vec[0].len = sizeof(pgoff_val);

	vec[1].base = filename;
	vec[1].len = length;

	pr_debug("MMAP: pid: %u, file_name: '%s', addr: %#llx - %#llx, len: %llx, pgoff: %#lx\n",
		 s->pid, filename, s->addr, s->addr + s->len, s->len, pgoff);

	quadd_put_sample(&r, vec, ARRAY_SIZE(vec));
}

void quadd_process_mmap(struct vm_area_struct *vma, pid_t pid)
{
	struct file *vm_file;
	struct path *path;
	char *file_name, *tmp_buf;
	struct quadd_mmap_data sample;
	size_t length, length_aligned;

	if (!vma)
		return;

	if (!(vma->vm_flags & VM_EXEC))
		return;

	vm_file = vma->vm_file;
	if (!vm_file)
		return;

	path = &vm_file->f_path;

	tmp_buf = kzalloc(PATH_MAX + sizeof(u64), GFP_KERNEL);
	if (!tmp_buf)
		return;

	file_name = d_path(path, tmp_buf, PATH_MAX);
	if (IS_ERR(file_name))
		goto out;

	if (strstr(file_name, " (deleted)"))
		goto out;

	sample.pid = pid;
	sample.user_mode = 1;

	sample.addr = vma->vm_start;
	sample.len = vma->vm_end - vma->vm_start;

	length = strlen(file_name) + 1;
	length_aligned = ALIGN(length, sizeof(u64));

	put_mmap_sample(&sample, file_name, length_aligned, vma->vm_pgoff);

out:
	kfree(tmp_buf);
}

int quadd_get_current_mmap(pid_t pid)
{
	struct vm_area_struct *vma;
	struct file *vm_file;
	struct path *path;
	char *file_name;
	struct task_struct *task;
	struct mm_struct *mm;
	struct quadd_mmap_data sample;
	size_t length, length_aligned;
	char *tmp_buf;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	rcu_read_unlock();
	if (!task) {
		pr_err("Process not found: %d\n", pid);
		return -ESRCH;
	}

	mm = task->mm;
	if (!mm) {
		pr_warn("mm is not existed for task: %d\n", pid);
		return 0;
	}

	pr_info("Get mapped memory objects\n");

	tmp_buf = kzalloc(PATH_MAX + sizeof(u64), GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (!(vma->vm_flags & VM_EXEC))
			continue;

		vm_file = vma->vm_file;
		if (!vm_file)
			continue;

		path = &vm_file->f_path;

		file_name = d_path(path, tmp_buf, PATH_MAX);
		if (IS_ERR(file_name))
			continue;

		if (strstr(file_name, " (deleted)"))
			continue;

		length = strlen(file_name) + 1;
		length_aligned = ALIGN(length, sizeof(u64));

		sample.pid = pid;
		sample.user_mode = 1;

		sample.addr = vma->vm_start;
		sample.len = vma->vm_end - vma->vm_start;

		put_mmap_sample(&sample, file_name, length_aligned,
				vma->vm_pgoff);
	}
	kfree(tmp_buf);

	return 0;
}
