/*
 * drivers/misc/tegra-profiler/mmap.c
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
#include <linux/mm.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <linux/tegra_profiler.h>

#include "mmap.h"
#include "hrt.h"
#include "debug.h"

static struct quadd_mmap_ctx mmap_ctx;

static int binary_search_and_add(unsigned int *array,
			unsigned int length, unsigned int key)
{
	unsigned int i_min, i_max, mid;

	if (length == 0) {
		array[0] = key;
		return 1;
	} else if (length == 1 && array[0] == key) {
		return 0;
	}

	i_min = 0;
	i_max = length;

	if (array[0] > key) {
		memmove((char *)((unsigned int *)array + 1), array,
			length * sizeof(unsigned int));
		array[0] = key;
		return 1;
	} else if (array[length - 1] < key) {
		array[length] = key;
		return 1;
	}

	while (i_min < i_max) {
		mid = i_min + (i_max - i_min) / 2;

		if (key <= array[mid])
			i_max = mid;
		else
			i_min = mid + 1;
	}

	if (array[i_max] == key) {
		return 0;
	} else {
		memmove((char *)((unsigned int *)array + i_max + 1),
			(char *)((unsigned int *)array + i_max),
			(length - i_max) * sizeof(unsigned int));
		array[i_max] = key;
		return 1;
	}
}

static int check_hash(u32 key)
{
	int res;
	unsigned long flags;

	spin_lock_irqsave(&mmap_ctx.lock, flags);

	if (mmap_ctx.nr_hashes >= QUADD_MMAP_SIZE_ARRAY) {
		spin_unlock_irqrestore(&mmap_ctx.lock, flags);
		return 1;
	}

	res = binary_search_and_add(mmap_ctx.hash_array,
				    mmap_ctx.nr_hashes, key);
	if (res > 0) {
		mmap_ctx.nr_hashes++;
		spin_unlock_irqrestore(&mmap_ctx.lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&mmap_ctx.lock, flags);
	return 1;
}

char *quadd_get_mmap(struct quadd_cpu_context *cpu_ctx,
		     struct pt_regs *regs, struct quadd_mmap_data *sample,
		     unsigned int *extra_length)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	struct file *vm_file;
	struct path *path;
	char *file_name = NULL;
	int length, length_aligned;
	u32 crc;
	unsigned long ip;

	if (!mm) {
		*extra_length = 0;
		return NULL;
	}

	ip = instruction_pointer(regs);

	if (user_mode(regs)) {
		for (vma = find_vma(mm, ip); vma; vma = vma->vm_next) {
			if (ip < vma->vm_start || ip >= vma->vm_end)
				continue;

			vm_file = vma->vm_file;
			if (!vm_file)
				break;

			path = &vm_file->f_path;

			file_name = d_path(path, mmap_ctx.tmp_buf, PATH_MAX);
			if (file_name) {
				sample->addr = vma->vm_start;
				sample->len = vma->vm_end - vma->vm_start;
				sample->pgoff =
					(u64)vma->vm_pgoff << PAGE_SHIFT;
			}
			break;
		}
	} else {
		struct module *mod;

		preempt_disable();
		mod = __module_address(ip);
		preempt_enable();

		if (mod) {
			file_name = mod->name;
			if (file_name) {
				sample->addr = (u32) mod->module_core;
				sample->len = mod->core_size;
				sample->pgoff = 0;
			}
		}
	}

	if (file_name) {
		length = strlen(file_name);
		if (length >= PATH_MAX) {
			*extra_length = 0;
			return NULL;
		}

		crc = crc32_le(~0, file_name, length);
		crc = crc32_le(crc, (unsigned char *)&sample->addr,
			       sizeof(sample->addr));
		crc = crc32_le(crc, (unsigned char *)&sample->len,
			       sizeof(sample->len));

		if (!check_hash(crc)) {
			strcpy(cpu_ctx->mmap_filename, file_name);
			length_aligned = (length + 1 + 7) & (~7);
			*extra_length = length_aligned;

			return cpu_ctx->mmap_filename;
		}
	}

	*extra_length = 0;
	return NULL;
}

struct quadd_mmap_ctx *quadd_mmap_init(struct quadd_ctx *quadd_ctx)
{
	u32 *hash;
	char *tmp;

	mmap_ctx.quadd_ctx = quadd_ctx;

	hash = kzalloc(QUADD_MMAP_SIZE_ARRAY * sizeof(unsigned int),
		       GFP_KERNEL);
	if (!hash) {
		pr_err("Alloc error\n");
		return NULL;
	}
	mmap_ctx.hash_array = hash;

	mmap_ctx.nr_hashes = 0;
	spin_lock_init(&mmap_ctx.lock);

	tmp = kzalloc(PATH_MAX + sizeof(unsigned long long),
		      GFP_KERNEL);
	if (!tmp) {
		pr_err("Alloc error\n");
		return NULL;
	}
	mmap_ctx.tmp_buf = tmp;

	return &mmap_ctx;
}

void quadd_mmap_reset(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mmap_ctx.lock, flags);
	mmap_ctx.nr_hashes = 0;
	spin_unlock_irqrestore(&mmap_ctx.lock, flags);
}

void quadd_mmap_deinit(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mmap_ctx.lock, flags);

	kfree(mmap_ctx.hash_array);
	mmap_ctx.hash_array = NULL;

	kfree(mmap_ctx.tmp_buf);
	mmap_ctx.tmp_buf = NULL;

	spin_unlock_irqrestore(&mmap_ctx.lock, flags);
}
