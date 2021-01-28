/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/file.h>

#include "vpubuf-core.h"
#include "vpubuf-memops.h"
#if 0	/* to do */
/**
 * vpu_create_framevec() - map virtual addresses to pfns
 * @start:	Virtual user address where we start mapping
 * @length:	Length of a range to map
 * @write:	Should we map for writing into the area
 *
 * This function allocates and fills in a vector with pfns corresponding to
 * virtual address range passed in arguments. If pfns have corresponding pages,
 * page references are also grabbed to pin pages in memory. The function
 * returns pointer to the vector on success and error pointer in case of
 * failure. Returned vector needs to be freed via vpu_destroy_pfnvec().
 */
struct frame_vector *vpu_create_framevec(unsigned long start,
					 unsigned long length, bool write)
{
	int ret;
	unsigned long first, last;
	unsigned long nr;
	struct frame_vector *vec;

	first = start >> PAGE_SHIFT;
	last = (start + length - 1) >> PAGE_SHIFT;
	nr = last - first + 1;
	vec = frame_vector_create(nr);
	if (!vec)
		return ERR_PTR(-ENOMEM);
	ret = get_vaddr_frames(start & PAGE_MASK, nr, write, true, vec);
	if (ret < 0)
		goto out_destroy;
	/* We accept only complete set of PFNs */
	if (ret != nr) {
		ret = -EFAULT;
		goto out_release;
	}
	return vec;
out_release:
	put_vaddr_frames(vec);
out_destroy:
	frame_vector_destroy(vec);
	return ERR_PTR(ret);
}

/**
 * vpu_destroy_framevec() - release vector of mapped pfns
 * @vec:	vector of pfns / pages to release
 *
 * This releases references to all pages in the vector @vec (if corresponding
 * pfns are backed by pages) and frees the passed vector.
 */
void vpu_destroy_framevec(struct frame_vector *vec)
{
	put_vaddr_frames(vec);
	frame_vector_destroy(vec);
}
#endif
/**
 * vpu_common_vm_open() - increase refcount of the vma
 * @vma:	virtual memory region for the mapping
 *
 * This function adds another user to the provided vma. It expects
 * struct vpu_vmarea_handler pointer in vma->vm_private_data.
 */
static void vpu_common_vm_open(struct vm_area_struct *vma)
{
	struct vpu_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
		 __func__, h, atomic_read(h->refcount), vma->vm_start,
		 vma->vm_end);

	atomic_inc(h->refcount);
}

/**
 * vpu_common_vm_close() - decrease refcount of the vma
 * @vma:	virtual memory region for the mapping
 *
 * This function releases the user from the provided vma. It expects
 * struct vpu_vmarea_handler pointer in vma->vm_private_data.
 */
static void vpu_common_vm_close(struct vm_area_struct *vma)
{
	struct vpu_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
		 __func__, h, atomic_read(h->refcount), vma->vm_start,
		 vma->vm_end);

	h->put(h->arg);
}

/**
 * vpu_common_vm_ops - common vm_ops used for tracking refcount of mmaped
 * video buffers
 */
const struct vm_operations_struct vpu_common_vm_ops = {
	.open = vpu_common_vm_open,
	.close = vpu_common_vm_close,
};
