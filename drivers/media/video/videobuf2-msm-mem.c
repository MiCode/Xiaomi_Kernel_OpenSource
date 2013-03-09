/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <linux/memory_alloc.h>
#include <media/videobuf2-msm-mem.h>
#include <media/msm_camera.h>
#include <mach/memory.h>
#include <media/videobuf2-core.h>
#include <mach/iommu_domains.h>

#define MAGIC_PMEM 0x0733ac64
#define MAGIC_CHECK(is, should)               \
	if (unlikely((is) != (should))) {           \
		pr_err("magic mismatch: %x expected %x\n", (is), (should)); \
		BUG();                  \
	}

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("videobuf-msm-mem: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static unsigned long msm_mem_allocate(struct videobuf2_contig_pmem *mem)
{
	unsigned long phyaddr;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	int rc, len;
	mem->client = msm_ion_client_create(-1, "camera");
	if (IS_ERR((void *)mem->client)) {
		pr_err("%s Could not create client\n", __func__);
		goto client_failed;
	}
	mem->ion_handle = ion_alloc(mem->client, mem->size, SZ_4K,
		(0x1 << ION_CP_MM_HEAP_ID | 0x1 << ION_IOMMU_HEAP_ID), 0);
	if (IS_ERR((void *)mem->ion_handle)) {
		pr_err("%s Could not allocate\n", __func__);
		goto alloc_failed;
	}
	rc = ion_map_iommu(mem->client, mem->ion_handle,
			-1, 0, SZ_4K, 0,
			(unsigned long *)&phyaddr,
			(unsigned long *)&len, 0, 0);
	if (rc < 0) {
		pr_err("%s Could not get physical address\n", __func__);
		goto phys_failed;
	}
#else
	phyaddr = allocate_contiguous_ebi_nomap(mem->size, SZ_4K);
#endif
	return phyaddr;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
phys_failed:
	ion_free(mem->client, mem->ion_handle);
alloc_failed:
	ion_client_destroy(mem->client);
client_failed:
	return 0;
#endif
}

static int32_t msm_mem_free(struct videobuf2_contig_pmem *mem)
{
	int32_t rc = 0;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_unmap_iommu(mem->client, mem->ion_handle, -1, 0);
	ion_free(mem->client, mem->ion_handle);
	ion_client_destroy(mem->client);
#else
	free_contiguous_memory_by_paddr(mem->phyaddr);
#endif
	return rc;
}

static void videobuf2_vm_close(struct vm_area_struct *vma)
{
	struct videobuf2_contig_pmem *mem = vma->vm_private_data;
	D("vm_close %p [count=%u,vma=%08lx-%08lx]\n",
		mem, mem->count, vma->vm_start, vma->vm_end);
	mem->count--;
}
static void videobuf2_vm_open(struct vm_area_struct *vma)
{
	struct videobuf2_contig_pmem *mem = vma->vm_private_data;
	D("vm_open %p [count=%u,vma=%08lx-%08lx]\n",
		mem, mem->count, vma->vm_start, vma->vm_end);
	mem->count++;
}

static const struct vm_operations_struct videobuf2_vm_ops = {
	.open     = videobuf2_vm_open,
	.close    = videobuf2_vm_close,
};

static void *msm_vb2_mem_ops_alloc(void *alloc_ctx, unsigned long size)
{
	struct videobuf2_contig_pmem *mem;
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return ERR_PTR(-ENOMEM);

	mem->magic = MAGIC_PMEM;
	mem->size =  PAGE_ALIGN(size);
	mem->alloc_ctx = alloc_ctx;
	mem->is_userptr = 0;
	mem->phyaddr = msm_mem_allocate(mem);
	if (!mem->phyaddr) {
		pr_err("%s : pmem memory allocation failed\n", __func__);
		kfree(mem);
		return ERR_PTR(-ENOMEM);
	}
	mem->mapped_phyaddr = mem->phyaddr;
	return mem;
}
static void msm_vb2_mem_ops_put(void *buf_priv)
{
	struct videobuf2_contig_pmem *mem = buf_priv;
	if (!mem->is_userptr) {
		D("%s Freeing memory ", __func__);
		msm_mem_free(mem);
	}
	kfree(mem);
}
int videobuf2_pmem_contig_mmap_get(struct videobuf2_contig_pmem *mem,
				struct videobuf2_msm_offset *offset,
				enum videobuf2_buffer_type buffer_type,
				int path)
{
	if (offset)
		mem->offset = *offset;
	else
		memset(&mem->offset, 0, sizeof(struct videobuf2_msm_offset));
	mem->buffer_type = buffer_type;
	mem->path = path;
	return 0;
}
EXPORT_SYMBOL_GPL(videobuf2_pmem_contig_mmap_get);

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
int videobuf2_pmem_contig_user_get(struct videobuf2_contig_pmem *mem,
					struct videobuf2_msm_offset *offset,
					enum videobuf2_buffer_type buffer_type,
					uint32_t addr_offset, int path,
					struct ion_client *client,
					int domain_num)
{
	int rc = 0;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	unsigned long len;
#endif
	unsigned long paddr = 0;
	if (mem->phyaddr != 0)
		return 0;
#if defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	mem->ion_handle = ion_import_dma_buf(client, (int)mem->vaddr);
	if (IS_ERR_OR_NULL(mem->ion_handle)) {
		pr_err("%s ION import failed\n", __func__);
		return PTR_ERR(mem->ion_handle);
	}
	rc = ion_map_iommu(client, mem->ion_handle, domain_num, 0,
		SZ_4K, 0, (unsigned long *)&mem->phyaddr, &len, 0, 0);
	if (rc < 0)
		ion_free(client, mem->ion_handle);
#else
	paddr = 0;
#endif
	if (offset)
		mem->offset = *offset;
	else
		memset(&mem->offset, 0, sizeof(struct videobuf2_msm_offset));
	mem->path = path;
	mem->buffer_type = buffer_type;
	paddr = mem->phyaddr;
	mem->mapped_phyaddr = paddr + addr_offset;
	mem->addr_offset = addr_offset;
	return rc;
}
EXPORT_SYMBOL_GPL(videobuf2_pmem_contig_user_get);

void videobuf2_pmem_contig_user_put(struct videobuf2_contig_pmem *mem,
				struct ion_client *client, int domain_num)
{
	if (mem->is_userptr) {
#if defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
		ion_unmap_iommu(client, mem->ion_handle,
				domain_num, 0);
		ion_free(client, mem->ion_handle);
#endif
	}
	mem->is_userptr = 0;
	mem->phyaddr = 0;
	mem->size = 0;
	mem->mapped_phyaddr = 0;
}
EXPORT_SYMBOL_GPL(videobuf2_pmem_contig_user_put);

static void *msm_vb2_mem_ops_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write)
{
	struct videobuf2_contig_pmem *mem;
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return ERR_PTR(-ENOMEM);
	mem->magic = MAGIC_PMEM;
	mem->is_userptr = 1;
	mem->vaddr = (void *)vaddr;
	mem->size = size;
	mem->alloc_ctx = alloc_ctx;
	return mem;
}
static void msm_vb2_mem_ops_put_userptr(void *buf_priv)
{
	kfree(buf_priv);
}

static void *msm_vb2_mem_ops_vaddr(void *buf_priv)
{
	struct videobuf2_contig_pmem *mem = buf_priv;
	return mem->vaddr;
}
static void *msm_vb2_mem_ops_cookie(void *buf_priv)
{
	return buf_priv;
}
static unsigned int msm_vb2_mem_ops_num_users(void *buf_priv)
{
	struct videobuf2_contig_pmem *mem = buf_priv;
	MAGIC_CHECK(mem->magic, MAGIC_PMEM);
	return mem->count;
}
static int msm_vb2_mem_ops_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct videobuf2_contig_pmem *mem;
	int retval;
	unsigned long size;
	D("%s\n", __func__);
	mem = buf_priv;
	D("mem = 0x%x\n", (u32)mem);
	BUG_ON(!mem);
	MAGIC_CHECK(mem->magic, MAGIC_PMEM);
	/* Try to remap memory */
	size = vma->vm_end - vma->vm_start;
	size = (size < mem->size) ? size : mem->size;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	retval = remap_pfn_range(vma, vma->vm_start,
			mem->phyaddr >> PAGE_SHIFT,
			size, vma->vm_page_prot);
	if (retval) {
		pr_err("mmap: remap failed with error %d. ", retval);
		goto error;
	}
	mem->vaddr = (void *)vma->vm_start;
	vma->vm_ops = &videobuf2_vm_ops;
	vma->vm_flags |= VM_DONTEXPAND;
	vma->vm_private_data = mem;

	D("mmap %p: %08lx-%08lx (%lx) pgoff %08lx\n",
		map, vma->vm_start, vma->vm_end,
		(long int)mem->bsize, vma->vm_pgoff);
	videobuf2_vm_open(vma);
	return 0;
error:
	return -ENOMEM;
}

static struct vb2_mem_ops msm_vb2_mem_ops = {
	.alloc = msm_vb2_mem_ops_alloc,
	.put = msm_vb2_mem_ops_put,
	.get_userptr = msm_vb2_mem_ops_get_userptr,
	.put_userptr = msm_vb2_mem_ops_put_userptr,
	.vaddr = msm_vb2_mem_ops_vaddr,
	.cookie = msm_vb2_mem_ops_cookie,
	.num_users = msm_vb2_mem_ops_num_users,
	.mmap = msm_vb2_mem_ops_mmap
};

void videobuf2_queue_pmem_contig_init(struct vb2_queue *q,
					enum v4l2_buf_type type,
					const struct vb2_ops *ops,
					unsigned int size,
					void *priv)
{
	memset(q, 0, sizeof(struct vb2_queue));
	q->mem_ops = &msm_vb2_mem_ops;
	q->ops = ops;
	q->drv_priv = priv;
	q->type = type;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->io_flags = 0;
	q->buf_struct_size = size;
	vb2_queue_init(q);
}
EXPORT_SYMBOL_GPL(videobuf2_queue_pmem_contig_init);

unsigned long videobuf2_to_pmem_contig(struct vb2_buffer *vb,
				unsigned int plane_no)
{
	struct videobuf2_contig_pmem *mem;
	mem = vb2_plane_cookie(vb, plane_no);
	BUG_ON(!mem);
	MAGIC_CHECK(mem->magic, MAGIC_PMEM);
	return mem->mapped_phyaddr;
}
EXPORT_SYMBOL_GPL(videobuf2_to_pmem_contig);

MODULE_DESCRIPTION("helper module to manage video4linux PMEM contig buffers");
MODULE_LICENSE("GPL v2");
