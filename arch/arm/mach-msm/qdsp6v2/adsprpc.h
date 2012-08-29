/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef ADSPRPC_H
#define ADSPRPC_H

#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/ion.h>
#include <mach/msm_smd.h>
#include <mach/ion.h>
#include "adsprpc_shared.h"

#ifndef ION_ADSPRPC_HEAP_ID
#define ION_ADSPRPC_HEAP_ID ION_AUDIO_HEAP_ID
#endif

#define RPC_TIMEOUT	(5 * HZ)
#define RPC_HASH_BITS	5
#define RPC_HASH_SZ	(1 << RPC_HASH_BITS)

#define ALIGN_8(a)	ALIGN(a, 8)

#define LOCK_MMAP(kernel)\
		do {\
			if (!kernel)\
				down_read(&current->mm->mmap_sem);\
		} while (0)

#define UNLOCK_MMAP(kernel)\
		do {\
			if (!kernel)\
				up_read(&current->mm->mmap_sem);\
		} while (0)


static inline uint32_t buf_page_start(void *buf)
{
	uint32_t start = (uint32_t) buf & PAGE_MASK;
	return start;
}

static inline uint32_t buf_page_offset(void *buf)
{
	uint32_t offset = (uint32_t) buf & (PAGE_SIZE - 1);
	return offset;
}

static inline int buf_num_pages(void *buf, int len)
{
	uint32_t start = buf_page_start(buf) >> PAGE_SHIFT;
	uint32_t end = (((uint32_t) buf + len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	int nPages = end - start + 1;
	return nPages;
}

static inline uint32_t buf_page_size(uint32_t size)
{
	uint32_t sz = (size + (PAGE_SIZE - 1)) & PAGE_MASK;
	return sz > PAGE_SIZE ? sz : PAGE_SIZE;
}

static inline int buf_get_pages(void *addr, int sz, int nr_pages, int access,
				  struct smq_phy_page *pages, int nr_elems)
{
	struct vm_area_struct *vma;
	uint32_t start = buf_page_start(addr);
	uint32_t len = nr_pages << PAGE_SHIFT;
	uint32_t pfn;
	int n = -1, err = 0;

	VERIFY(0 != access_ok(access ? VERIFY_WRITE : VERIFY_READ,
			      (void __user *)start, len));
	VERIFY(0 != (vma = find_vma(current->mm, start)));
	VERIFY(((uint32_t)addr + sz) <= vma->vm_end);
	n = 0;
	VERIFY(0 != (vma->vm_flags & VM_PFNMAP));
	VERIFY(0 != (vma->vm_flags & VM_PFN_AT_MMAP));
	VERIFY(nr_elems > 0);
	pfn = vma->vm_pgoff + ((start - vma->vm_start) >> PAGE_SHIFT);
	pages->addr = __pfn_to_phys(pfn);
	pages->size = len;
	n++;
 bail:
	return n;
}

#endif
