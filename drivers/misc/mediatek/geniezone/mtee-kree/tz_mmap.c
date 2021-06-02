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

#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <kree/tz_mod.h>

#define debugFg 0

/* map user space pages */
/* control -> 0 = write, 1 = read only memory */
long _map_user_pages(struct MTIOMMU_PIN_RANGE_T *pinRange, unsigned long uaddr,
		     uint32_t size, uint32_t control)
{
	int nr_pages;
	unsigned long first, last;
	struct page **pages;
	struct vm_area_struct *vma;
	int res, j;
	uint32_t write;

	if ((!uaddr) || (!size))
		return -EFAULT;

	pinRange->start = (void *)uaddr;
	pinRange->size = size;

	first = (uaddr & PAGE_MASK) >> PAGE_SHIFT;
	last = ((uaddr + size + PAGE_SIZE - 1) & PAGE_MASK) >> PAGE_SHIFT;
	nr_pages = last - first;
	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	pinRange->pageArray = (void *)pages;
	write = (control == 0) ? 1 : 0;

	/* Try to fault in all of the necessary pages */
	down_read(&current->mm->mmap_sem);
	vma = find_vma_intersection(current->mm, uaddr, uaddr + size);
	if (!vma) {
		res = -EFAULT;
		goto out;
	}
	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP))) {
		pinRange->isPage = 1;
		/*diff with kernel-4.9(Linux modified)*/
		res = get_user_pages_remote(current, current->mm, uaddr,
					    nr_pages, write ? FOLL_WRITE : 0,
					    pages, NULL, NULL);
	} else {
		/* pfn mapped memory, don't touch page struct.
		 * the buffer manager (possibly ion) should make sure
		 * it won't be used for anything else
		 */
		pinRange->isPage = 0;
		res = 0;
		do {
			unsigned long *pfns = (void *)pages;

			while (res < nr_pages
			       && uaddr + PAGE_SIZE <= vma->vm_end) {
				j = follow_pfn(vma, uaddr, &pfns[res]);
				if (j) { /* error */
					res = j;
					goto out;
				}
				uaddr += PAGE_SIZE;
				res++;
			}
			if (res >= nr_pages || uaddr < vma->vm_end)
				break;
			vma = find_vma_intersection(current->mm, uaddr,
						    uaddr + 1);
		} while (vma && vma->vm_flags & (VM_IO | VM_PFNMAP));
	}
out:
	up_read(&current->mm->mmap_sem);
	if (res < 0) {
		pr_debug("map user pages error = %d\n", res);
		goto out_free;
	}

	pinRange->nrPages = res;
	/* Errors and no page mapped should return here */
	if (res < nr_pages)
		goto out_unmap;

	return 0;

out_unmap:
	pr_debug("map user pages fail\n");
	if (pinRange->isPage) {
		for (j = 0; j < res; j++)
			put_page(pages[j]);
	}
	res = -EFAULT;

out_free:
	kfree(pages);
	return res;
}

#if debugFg
static void _unmap_user_pages(struct MTIOMMU_PIN_RANGE_T *pinRange)
{
	int res;
	int j;
	struct page **pages;

	pages = (struct page **)pinRange->pageArray;

	if (pinRange->isPage) {
		res = pinRange->nrPages;

		if (res > 0) {
			for (j = 0; j < res; j++)
				put_page(pages[j]);
			res = 0;
		}
	}

	kfree(pages);
}
#endif
