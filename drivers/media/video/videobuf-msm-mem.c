/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Based on videobuf-dma-contig.c,
 * (c) 2008 Magnus Damm
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
 * helper functions for physically contiguous pmem capture buffers
 * The functions support contiguous memory allocations using pmem
 * kernel API.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/android_pmem.h>
#include <linux/memory_alloc.h>
#include <media/videobuf-msm-mem.h>
#include <media/msm_camera.h>
#include <mach/memory.h>

#define MAGIC_PMEM 0x0733ac64
#define MAGIC_CHECK(is, should)               \
	if (unlikely((is) != (should))) {           \
		pr_err("magic mismatch: %x expected %x\n", (is), (should)); \
		BUG();                  \
	}

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) printk(KERN_DEBUG "videobuf-msm-mem: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static int32_t msm_mem_allocate(const size_t size)
{
	int32_t phyaddr;
	phyaddr = allocate_contiguous_ebi_nomap(size, SZ_4K);
	return phyaddr;
}

static int32_t msm_mem_free(const int32_t phyaddr)
{
	int32_t rc = 0;
	free_contiguous_memory_by_paddr(phyaddr);
	return rc;
}

static void
videobuf_vm_open(struct vm_area_struct *vma)
{
	struct videobuf_mapping *map = vma->vm_private_data;

	D("vm_open %p [count=%u,vma=%08lx-%08lx]\n",
		map, map->count, vma->vm_start, vma->vm_end);

	map->count++;
}

static void videobuf_vm_close(struct vm_area_struct *vma)
{
	struct videobuf_mapping *map = vma->vm_private_data;
	struct videobuf_queue *q = map->q;
	int i, rc;

	D("vm_close %p [count=%u,vma=%08lx-%08lx]\n",
		map, map->count, vma->vm_start, vma->vm_end);

	map->count--;
	if (0 == map->count) {
		struct videobuf_contig_pmem *mem;

		D("munmap %p q=%p\n", map, q);
		mutex_lock(&q->vb_lock);

		/* We need first to cancel streams, before unmapping */
		if (q->streaming)
			videobuf_queue_cancel(q);

		for (i = 0; i < VIDEO_MAX_FRAME; i++) {
			if (NULL == q->bufs[i])
				continue;

			if (q->bufs[i]->map != map)
				continue;

			mem = q->bufs[i]->priv;
			if (mem) {
				/* This callback is called only if kernel has
				 * allocated memory and this memory is mmapped.
				 * In this case, memory should be freed,
				 * in order to do memory unmap.
				 */

				MAGIC_CHECK(mem->magic, MAGIC_PMEM);

				/* vfree is not atomic - can't be
				 called with IRQ's disabled
				 */
				D("buf[%d] freeing physical %d\n",
					i, mem->phyaddr);

				rc = msm_mem_free(mem->phyaddr);
				if (rc < 0)
					D("%s: Invalid memory location\n",
								__func__);
				else {
					mem->phyaddr = 0;
				}
			}

			q->bufs[i]->map   = NULL;
			q->bufs[i]->baddr = 0;
		}

		kfree(map);

		mutex_unlock(&q->vb_lock);

		/* deallocate the q->bufs[i] structure not a good solution
		 as it will result in unnecessary iterations but right now
		 this looks like the only cleaner way  */
		videobuf_mmap_free(q);
	}
}

static const struct vm_operations_struct videobuf_vm_ops = {
	.open     = videobuf_vm_open,
	.close    = videobuf_vm_close,
};

/**
 * videobuf_pmem_contig_user_put() - reset pointer to user space buffer
 * @mem: per-buffer private videobuf-contig-pmem data
 *
 * This function resets the user space pointer
 */
static void videobuf_pmem_contig_user_put(struct videobuf_contig_pmem *mem)
{
	if (mem->phyaddr) {
		put_pmem_file(mem->file);
		mem->is_userptr = 0;
		mem->phyaddr = 0;
		mem->size = 0;
	}
}

/**
 * videobuf_pmem_contig_user_get() - setup user space memory pointer
 * @mem: per-buffer private videobuf-contig-pmem data
 * @vb: video buffer to map
 *
 * This function validates and sets up a pointer to user space memory.
 * Only physically contiguous pfn-mapped memory is accepted.
 *
 * Returns 0 if successful.
 */
static int videobuf_pmem_contig_user_get(struct videobuf_contig_pmem *mem,
					struct videobuf_buffer *vb)
{
	unsigned long kvstart;
	unsigned long len;
	int rc;

	mem->size = PAGE_ALIGN(vb->size);
	rc = get_pmem_file(vb->baddr, (unsigned long *)&mem->phyaddr,
					&kvstart, &len, &mem->file);
	if (rc < 0) {
		pr_err("%s: get_pmem_file fd %lu error %d\n",
					__func__, vb->baddr,
							rc);
		return rc;
	}
	mem->phyaddr += vb->boff;
	mem->y_off = 0;
	mem->cbcr_off = (vb->size)*2/3;
	mem->is_userptr = 1;
	return rc;
}

static struct videobuf_buffer *__videobuf_alloc(size_t size)
{
	struct videobuf_contig_pmem *mem;
	struct videobuf_buffer *vb;

	vb = kzalloc(size + sizeof(*mem), GFP_KERNEL);
	if (vb) {
		mem = vb->priv = ((char *)vb) + size;
		mem->magic = MAGIC_PMEM;
	}

	return vb;
}

static void *__videobuf_to_vaddr(struct videobuf_buffer *buf)
{
	struct videobuf_contig_pmem *mem = buf->priv;

	BUG_ON(!mem);
	MAGIC_CHECK(mem->magic, MAGIC_PMEM);

	return mem->vaddr;
}

static int __videobuf_iolock(struct videobuf_queue *q,
				struct videobuf_buffer *vb,
				struct v4l2_framebuffer *fbuf)
{
	int rc = 0;
	struct videobuf_contig_pmem *mem = vb->priv;

	BUG_ON(!mem);
	MAGIC_CHECK(mem->magic, MAGIC_PMEM);

	switch (vb->memory) {
	case V4L2_MEMORY_MMAP:
		D("%s memory method MMAP\n", __func__);

		/* All handling should be done by __videobuf_mmap_mapper() */
		break;
	case V4L2_MEMORY_USERPTR:
		D("%s memory method USERPTR\n", __func__);

		/* handle pointer from user space */
		rc = videobuf_pmem_contig_user_get(mem, vb);
		break;
	case V4L2_MEMORY_OVERLAY:
	default:
		pr_err("%s memory method OVERLAY/unknown\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}

static int __videobuf_mmap_mapper(struct videobuf_queue *q,
		struct videobuf_buffer *buf,
		struct vm_area_struct *vma)
{
	struct videobuf_contig_pmem *mem;
	struct videobuf_mapping *map;
	int retval;
	unsigned long size;

	D("%s\n", __func__);

	/* create mapping + update buffer list */
	map = kzalloc(sizeof(struct videobuf_mapping), GFP_KERNEL);
	if (!map) {
		pr_err("%s: kzalloc failed.\n", __func__);
		return -ENOMEM;
	}

	buf->map = map;
	map->q = q;

	buf->baddr = vma->vm_start;

	mem = buf->priv;
	D("mem = 0x%x\n", (u32)mem);
	D("buf = 0x%x\n", (u32)buf);
	BUG_ON(!mem);
	MAGIC_CHECK(mem->magic, MAGIC_PMEM);

	mem->size = PAGE_ALIGN(buf->bsize);
	mem->y_off = 0;
	mem->cbcr_off = (buf->bsize)*2/3;
	if (buf->i >= 0 && buf->i <= 3)
		mem->buffer_type = OUTPUT_TYPE_P;
	else
		mem->buffer_type = OUTPUT_TYPE_V;

	buf->bsize = mem->size;
	mem->phyaddr = msm_mem_allocate(mem->size);

	if (IS_ERR((void *)mem->phyaddr)) {
		pr_err("%s : pmem memory allocation failed\n", __func__);
		goto error;
	}

	/* Try to remap memory */
	size = vma->vm_end - vma->vm_start;
	size = (size < mem->size) ? size : mem->size;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	retval = remap_pfn_range(vma, vma->vm_start,
		mem->phyaddr >> PAGE_SHIFT,
		size, vma->vm_page_prot);
	if (retval) {
		pr_err("mmap: remap failed with error %d. ", retval);
		retval = msm_mem_free(mem->phyaddr);
		if (retval < 0)
			printk(KERN_ERR "%s: Invalid memory location\n",
								__func__);
		else {
			mem->phyaddr = 0;
		}
		goto error;
	}

	vma->vm_ops          = &videobuf_vm_ops;
	vma->vm_flags       |= VM_DONTEXPAND;
	vma->vm_private_data = map;

	D("mmap %p: q=%p %08lx-%08lx (%lx) pgoff %08lx buf %d\n",
		map, q, vma->vm_start, vma->vm_end,
		(long int)buf->bsize,
		vma->vm_pgoff, buf->i);

	videobuf_vm_open(vma);

	return 0;

error:
	kfree(map);
	return -ENOMEM;
}

static struct videobuf_qtype_ops qops = {
	.magic        = MAGIC_QTYPE_OPS,

	.alloc_vb     = __videobuf_alloc,
	.iolock       = __videobuf_iolock,
	.mmap_mapper  = __videobuf_mmap_mapper,
	.vaddr        = __videobuf_to_vaddr,
};

void videobuf_queue_pmem_contig_init(struct videobuf_queue *q,
	const struct videobuf_queue_ops *ops,
	struct device *dev,
	spinlock_t *irqlock,
	enum v4l2_buf_type type,
	enum v4l2_field field,
	unsigned int msize,
	void *priv,
	struct mutex *ext_lock)
{
	videobuf_queue_core_init(q, ops, dev, irqlock, type, field, msize,
							priv, &qops, ext_lock);
}
EXPORT_SYMBOL_GPL(videobuf_queue_pmem_contig_init);

int videobuf_to_pmem_contig(struct videobuf_buffer *buf)
{
	struct videobuf_contig_pmem *mem = buf->priv;

	BUG_ON(!mem);
	MAGIC_CHECK(mem->magic, MAGIC_PMEM);

	return mem->phyaddr;
}
EXPORT_SYMBOL_GPL(videobuf_to_pmem_contig);

int videobuf_pmem_contig_free(struct videobuf_queue *q,
				struct videobuf_buffer *buf)
{
	struct videobuf_contig_pmem *mem = buf->priv;

	/* mmapped memory can't be freed here, otherwise mmapped region
	 would be released, while still needed. In this case, the memory
	 release should happen inside videobuf_vm_close().
	 So, it should free memory only if the memory were allocated for
	 read() operation.
	*/
	if (buf->memory != V4L2_MEMORY_USERPTR)
		return -EINVAL;

	if (!mem)
		return -ENOMEM;

	MAGIC_CHECK(mem->magic, MAGIC_PMEM);

	/* handle user space pointer case */
	if (buf->baddr) {
		videobuf_pmem_contig_user_put(mem);
		return 0;
	} else {
		/* don't support read() method */
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(videobuf_pmem_contig_free);

MODULE_DESCRIPTION("helper module to manage video4linux PMEM contig buffers");
MODULE_LICENSE("GPL v2");
