/* Copyright (c) 2008-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/mm.h>
#include <linux/highmem.h>

#include "kgsl.h"
#include "kgsl_mm_dump.h"

struct type_entry {
	int type;
	const char *str;
};

static const struct type_entry memtypes[] = { KGSL_MEM_TYPES };

static const char *memtype_str(int memtype)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(memtypes); i++)
		if (memtypes[i].type == memtype)
			return memtypes[i].str;
	return "unknown";
}

static char get_alignflag(const struct kgsl_memdesc *m)
{
	int align = kgsl_memdesc_get_align(m);

	if (align >= ilog2(SZ_1M))
		return 'L';
	else if (align >= ilog2(SZ_64K))
		return 'l';
	return '-';
}

static char get_cacheflag(const struct kgsl_memdesc *m)
{
	static const char table[] = {
		[KGSL_CACHEMODE_WRITECOMBINE] = '-',
		[KGSL_CACHEMODE_UNCACHED] = 'u',
		[KGSL_CACHEMODE_WRITEBACK] = 'b',
		[KGSL_CACHEMODE_WRITETHROUGH] = 't',
	};

	return table[kgsl_memdesc_get_cachemode(m)];
}

static int
print_mem_entry(struct kgsl_mem_entry *entry)
{
	char flags[10];
	char usage[16];
	struct kgsl_memdesc *m = &entry->memdesc;
	unsigned int usermem_type = kgsl_memdesc_usermem_type(m);
	int egl_surface_count = 0, egl_image_count = 0;

	if (m->flags & KGSL_MEMFLAGS_SPARSE_VIRT)
		return 0;

	flags[0] = kgsl_memdesc_is_global(m) ?  'g' : '-';
	flags[1] = '-';
	flags[2] = !(m->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 'w' : '-';
	flags[3] = get_alignflag(m);
	flags[4] = get_cacheflag(m);
	flags[5] = kgsl_memdesc_use_cpu_map(m) ? 'p' : '-';
	flags[6] = (m->useraddr) ? 'Y' : 'N';
	flags[7] = kgsl_memdesc_is_secured(m) ?  's' : '-';
	flags[8] = m->flags & KGSL_MEMFLAGS_SPARSE_PHYS ? 'P' : '-';
	flags[9] = '\0';

	kgsl_get_memory_usage(usage, sizeof(usage), m->flags);

	if (usermem_type == KGSL_MEM_ENTRY_ION)
		kgsl_get_egl_counts(entry, &egl_surface_count,
						&egl_image_count);

	pr_err("%p %p %16llu %5d %9s %10s %16s %5d %16llu %6d %6d",
			(uint64_t *)(uintptr_t) m->gpuaddr,
			(unsigned long *) m->useraddr,
			m->size, entry->id, flags,
			memtype_str(usermem_type),
			usage, (m->sgt ? m->sgt->nents : 0), m->mapsize,
			egl_surface_count, egl_image_count);

	if (entry->metadata[0] != 0)
		KGSL_CORE_ERR(" %s", entry->metadata);

	return 0;
}

void
kgsl_dump_memory_entry(struct kgsl_process_private *private)
{
	int id;
	struct kgsl_mem_entry *entry = NULL;
	char pMemFile[256] = {0};

	if (!private)
		return;

	sprintf(pMemFile, "/d/kgsl/proc/%d/mem", private->pid);
	pr_info("cat %s:", pMemFile);

	pr_err("%16s %16s %16s %5s %9s %10s %16s %5s %16s %6s %6s\n",
		"gpuaddr", "useraddr", "size", "id", "flags", "type",
		"usage", "sglen", "mapsize", "eglsrf", "eglimg");

	spin_lock(&private->mem_lock);
	idr_for_each_entry(&private->mem_idr, entry, id) {
		print_mem_entry(entry);
	}
	spin_unlock(&private->mem_lock);

	return;
}

/*
 * Indicate if the VMA is a stack for the given task; for
 * /proc/PID/maps that is the stack of the main task.
 */
static int
is_stack(struct vm_area_struct *vma)
{
	/*
	 * We make no effort to guess what a given thread considers to be
	 * its "stack".  It's not even well-defined for programs written
	 * languages like Go.
	 */
	return vma->vm_start <= vma->vm_mm->start_stack &&
		vma->vm_end >= vma->vm_mm->start_stack;
}

static int
file_path_name(const struct path *path, char *buf, size_t size)
{
	int res = -1;

	if (size) {
		char *p = d_path(path, buf, size);
		if (!IS_ERR(p)) {
			char *end = mangle_path(buf, p, "\n");
			if (end)
				res = end - buf;
		}
	}
	return res;
}

static void
print_vma_name(struct vm_area_struct *vma, char *buf, size_t size)
{
	const char __user *name = vma_get_anon_name(vma);
	struct mm_struct *mm = vma->vm_mm;

	unsigned long page_start_vaddr;
	unsigned long page_offset;
	unsigned long num_pages;
	unsigned long max_len = NAME_MAX;
	int i;
	size_t len_buf = 0;

	page_start_vaddr = (unsigned long)name & PAGE_MASK;
	page_offset = (unsigned long)name - page_start_vaddr;
	num_pages = DIV_ROUND_UP(page_offset + max_len, PAGE_SIZE);

	strcpy(buf, "[anon:");
	len_buf += strlen("[anon:");

	for (i = 0; i < num_pages; i++) {
		int len;
		int write_len;
		const char *kaddr;
		long pages_pinned;
		struct page *page;

		pages_pinned = get_user_pages_remote(current, mm,
				page_start_vaddr, 1, 0, &page, NULL);
		if (pages_pinned < 1) {
			strcpy(buf+len_buf, "<fault>]");
			return;
		}

		kaddr = (const char *)kmap(page);
		len = min(max_len, PAGE_SIZE - page_offset);
		write_len = strnlen(kaddr + page_offset, len);
		strncpy(buf+len_buf, kaddr + page_offset, write_len);
		len_buf += write_len;
		kunmap(page);
		put_page(page);

		/* if strnlen hit a null terminator then we're done */
		if (write_len != len)
			break;

		max_len -= len;
		page_offset = 0;
		page_start_vaddr += PAGE_SIZE;
	}

	strcpy(buf+len_buf, "]");
}

static void
show_map_vma(struct vm_area_struct *vma)
{
	struct mm_struct *mm = vma->vm_mm;
	struct file *file = vma->vm_file;
	vm_flags_t flags = vma->vm_flags;
	unsigned long ino = 0;
	unsigned long long pgoff = 0;
	unsigned long start, end;
	dev_t dev = 0;
	const char *name = NULL;
	char *pMemTmp;
	char *file_name;
	pMemTmp = (char *)kzalloc(1024, GFP_KERNEL);
	file_name = (char *)kzalloc(1024, GFP_KERNEL);

	if (file) {
		struct inode *inode = file_inode(vma->vm_file);
		dev = inode->i_sb->s_dev;
		ino = inode->i_ino;
		pgoff = ((loff_t)vma->vm_pgoff) << PAGE_SHIFT;
	}

	/* We don't show the stack guard page in /proc/maps */
	start = vma->vm_start;
	end = vma->vm_end;

	/*
	 * Print the dentry name for named mappings, and a
	 * special [heap] marker for the heap:
	 */
	if (file) {
		//seq_pad(m, ' ');
		//seq_file_path(m, file, "\n");
		int ret = file_path_name(&file->f_path, file_name, 1024);
		if (ret > 0) {
			name = file_name;
		}
		goto done;
	}

	if (vma->vm_ops && vma->vm_ops->name) {
		name = vma->vm_ops->name(vma);
		if (name)
			goto done;
	}

	name = arch_vma_name(vma);
	if (!name) {
		if (!mm) {
			name = "[vdso]";
			goto done;
		}

		if (vma->vm_start <= mm->brk &&
		    vma->vm_end >= mm->start_brk) {
			name = "[heap]";
			goto done;
		}

		if (is_stack(vma)) {
			name = "[stack]";
			goto done;
		}

		if (vma_get_anon_name(vma)) {
			//seq_pad(m, ' ');
			print_vma_name(vma, file_name, 1024);
			name = file_name;
		}
	}

done:

	sprintf(pMemTmp, "%08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s\n",
			start,
			end,
			flags & VM_READ ? 'r' : '-',
			flags & VM_WRITE ? 'w' : '-',
			flags & VM_EXEC ? 'x' : '-',
			flags & VM_MAYSHARE ? 's' : 'p',
			pgoff,
			MAJOR(dev), MINOR(dev), ino, name);

	pr_err("%s", pMemTmp);
	kfree(pMemTmp);
	kfree(file_name);
}

void
kgsl_dump_mmap(struct kgsl_process_private *private)
{
	/*
	struct file *filp = NULL;
	int nSize = 0;
	char pMemFile[1024] = {0};
	unsigned char *buf;
	int file_size = 0;
	*/

	char pMemFile[1024] = {0};
	sprintf(pMemFile, "/proc/%d/maps", private->pid);
	pr_info("cat %s:", pMemFile);
#if 0
	filp = filp_open(pMemFile, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("can NOT open /proc/%d/maps", private->pid);
	} else {
		file_size = filp->f_path.dentry->d_inode->i_size;
		buf = (unsigned char *)kzalloc(file_size, GFP_KERNEL);
		if (!buf) {
			filp_close(filp, NULL);
			return;
		}

		pr_info("read %s", pMemFile);
		nSize = kernel_read(filp, 0, buf, file_size);
		if (nSize < 0) {
			pr_err("read failed %s", pMemFile);
		} else {
			pr_err("%s", buf);
		}

		kfree(buf);
		filp_close(filp, NULL);
	}
#else
	{
		struct vm_area_struct *vma;
		struct mm_struct *mm = current->mm;

		for (vma = mm->mmap; vma; vma = vma->vm_next) {
			show_map_vma(vma);
		}
	}
#endif
	return;
}
#if 0
bool
switch_kptr_restrict(unsigned char *pWrite, unsigned char *pBackup)
{
	int nFile;
	/*mm_segment_t fs;*/
	int nSize = 0;
	bool ret = true;

	/*fs = get_fs();
	set_fs(KERNEL_DS);*/

	nFile = sys_open("/proc/sys/kernel/kptr_restrict", O_RDWR, 0);

	if (nFile >= 0) {
		if (pBackup)
			nSize = sys_read(nFile, pBackup, 8);

		nSize = sys_write(nFile, pWrite, 8);

		sys_close(nFile);
	} else {
		pr_err("can NOT open /proc/sys/kernel/kptr_restrict");
		ret = false;
	}

	/*set_fs(fs);*/
	return ret;
}
#endif

