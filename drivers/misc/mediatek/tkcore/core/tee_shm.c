/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/hugetlb.h>
#include <linux/version.h>
#include <linux/anon_inodes.h>

#include <linux/sched.h>
#include <linux/mm.h>

#include "tee_core_priv.h"
#include "tee_shm.h"

int __weak sg_nents(struct scatterlist *sg)
{
	int nents;

	for (nents = 0; sg; sg = sg_next(sg))
		nents++;
	return nents;
}

int __weak sg_alloc_table_from_pages(struct sg_table *sgt,
				struct page **pages, unsigned int n_pages,
				unsigned long offset, unsigned long size,
				gfp_t gfp_mask)
{
	unsigned int chunks;
	unsigned int i;
	unsigned int cur_page;
	int ret;
	struct scatterlist *s;

	/* compute number of contiguous chunks */
	chunks = 1;
	for (i = 1; i < n_pages; ++i)
		if (page_to_pfn(pages[i]) != page_to_pfn(pages[i - 1]) + 1)
			++chunks;

	ret = sg_alloc_table(sgt, chunks, gfp_mask);
	if (unlikely(ret))
		return ret;

	/* merging chunks and putting them into the scatterlist */
	cur_page = 0;
	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		unsigned long chunk_size;
		unsigned int j;

		/* look for the end of the current chunk */
		for (j = cur_page + 1; j < n_pages; ++j)
			if (page_to_pfn(pages[j]) !=
				page_to_pfn(pages[j - 1]) + 1)
				break;

		chunk_size = ((j - cur_page) << PAGE_SHIFT) - offset;
		sg_set_page(s, pages[cur_page], min(size, chunk_size), offset);
		size -= chunk_size;
		offset = 0;
		cur_page = j;
	}

	return 0;
}

struct tee_shm_attach {
	struct sg_table sgt;
	enum dma_data_direction dir;
	bool is_mapped;
};

static struct tee_shm *tee_shm_alloc_static(struct tee *tee, size_t size,
		uint32_t flags)
{
	struct tee_shm *shm;
	unsigned long pfn;
	unsigned int nr_pages;
	struct page *page;
	int ret;

	shm = tee->ops->alloc(tee, size, flags);
	if (IS_ERR_OR_NULL(shm)) {
		pr_err("allocation failed (s=%d,flags=0x%08x) err=%ld\n",
			(int) size, flags, PTR_ERR(shm));
		goto exit;
	}

	pfn = shm->resv.paddr >> PAGE_SHIFT;
	page = pfn_to_page(pfn);
	if (IS_ERR_OR_NULL(page)) {
		pr_err("pfn_to_page(%lx) failed\n", pfn);
		tee->ops->free(shm);
		return (struct tee_shm *) page;
	}

	/* Only one page of contiguous physical memory */
	nr_pages = 1;

	ret = sg_alloc_table_from_pages(&shm->resv.sgt, &page,
					nr_pages, 0,
					nr_pages * PAGE_SIZE, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table_from_pages() failed\n");
		tee->ops->free(shm);
		shm = ERR_PTR(ret);
	}

exit:
	return shm;
}

static struct tee_shm *tee_shm_alloc_ns(struct tee *tee, size_t size,
					uint32_t flags)
{
	size_t i, nr_pages;
	struct page **pages;

	struct tee_shm *shm;

	pr_debug("size: %zu flags: 0x%x\n",
		size, flags);

	if (size == 0) {
		pr_warn("invalid size %zu flags 0x%x\n",
			size, flags);
		return NULL;
	}

	shm = kzalloc(sizeof(struct tee_shm), GFP_KERNEL);
	if (shm == NULL) {
		shm = NULL;
		pr_err("bad kmalloc tee_shm: %zu\n",
			sizeof(struct tee_shm));
		return shm;
	}

	shm->ns.token = tee_core_alloc_uuid(shm);
	if (shm->ns.token <= 0) {
		pr_err("failed to alloc idr for shm\n");
		kfree(shm);
		return NULL;
	}

	/* FIXME whether it's correct? */
	nr_pages = ((size - 1) >> PAGE_SHIFT) + 1;

	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (pages == NULL)
		goto err_free_pagelist;

	for (i = 0; i < nr_pages; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (pages[i] == NULL) {
			pr_err("bad alloc page %zu\n", i);
			goto err_free_pages;
		}
	}

	shm->ns.pages = pages;
	shm->ns.nr_pages = (size_t) nr_pages;

	atomic_set(&shm->ns.ref, 1);

	shm->size_req = size;
	shm->size_alloc = nr_pages << PAGE_SHIFT;

	shm->flags = flags;

	return shm;

err_free_pages:
	for (i = 0; i < nr_pages; i++) {
		if (pages[i] == NULL)
			break;

		__free_page(pages[i]);
	}

err_free_pagelist:
	kfree(pages);

	tee_core_free_uuid(shm->ns.token);

	kfree(shm);

	return NULL;
}

void tee_shm_free_ns(struct tee_shm *shm)
{
	size_t i;

	if (atomic_dec_return(&shm->ns.ref) != 0)
		return;

	for (i = 0; i < shm->ns.nr_pages; i++)
		__free_page(shm->ns.pages[i]);

	kfree(shm->ns.pages);

	tee_core_free_uuid(shm->ns.token);

	kfree(shm);
}

struct tee_shm *tee_shm_alloc(struct tee *tee, size_t size, uint32_t flags)
{
	struct tee_shm *shm;

	if ((shm_test_nonsecure(flags)))
		shm = tee_shm_alloc_ns(tee, size, flags);
	else
		shm = tee_shm_alloc_static(tee, size, flags);

	if (IS_ERR_OR_NULL(shm))
		goto exit;

	shm->tee = tee;

	pr_debug("shm=%p, s=%d/%d app=\"%s\" pid=%d\n",
		shm, (int) shm->size_req, (int) shm->size_alloc,
		current->comm, current->pid);

exit:
	return shm;
}

void tee_shm_free(struct tee_shm *shm)
{
	struct tee *tee;

	if (IS_ERR_OR_NULL(shm))
		return;

	tee = shm->tee;

	if (tee == NULL) {
		pr_warn("invalid call to tee_shm_free(%p): NULL tee\n",
			shm);
		return;
	}
	if (shm->tee == NULL) {
		pr_warn("tee_shm_free(%p): NULL tee\n", shm);
		return;
	}

	if (shm_test_nonsecure(shm->flags))
		tee_shm_free_ns(shm);
	else {
		sg_free_table(&shm->resv.sgt);
		shm->tee->ops->free(shm);
	}
}

static int __tee_shm_attach_dma_buf(struct dma_buf *dmabuf,
				struct device *dev,
				struct dma_buf_attachment *attach)
{
	struct tee_shm_attach *tee_shm_attach;
	struct tee_shm *shm;
	struct tee *tee;

	shm = dmabuf->priv;
	tee = shm->tee;


	tee_shm_attach = devm_kzalloc(_DEV(tee),
				sizeof(*tee_shm_attach), GFP_KERNEL);
	if (!tee_shm_attach)
		return -ENOMEM;

	tee_shm_attach->dir = DMA_NONE;
	attach->priv = tee_shm_attach;

	return 0;
}

static void __tee_shm_detach_dma_buf(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct tee_shm_attach *tee_shm_attach = attach->priv;
	struct sg_table *sgt;
	struct tee_shm *shm;
	struct tee *tee;

	shm = dmabuf->priv;
	tee = shm->tee;


	if (!tee_shm_attach) {
		pr_err("No shm attached with this dmabuf context");
		return;
	}

	sgt = &tee_shm_attach->sgt;

	if (tee_shm_attach->dir != DMA_NONE)
		dma_unmap_sg(attach->dev, sgt->sgl, sgt->nents,
			tee_shm_attach->dir);

	sg_free_table(sgt);
	devm_kfree(_DEV(tee), tee_shm_attach);
	attach->priv = NULL;
}

static struct sg_table *__tee_shm_dma_buf_map_dma_buf(
	struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
	struct tee_shm_attach *tee_shm_attach = attach->priv;
	struct tee_shm *tee_shm = attach->dmabuf->priv;
	struct sg_table *sgt = NULL;
	struct scatterlist *rd, *wr;
	unsigned int i;
	int nents, ret;
	struct tee *tee;

	tee = tee_shm->tee;


	/* just return current sgt if already requested. */
	if (tee_shm_attach->dir == dir && tee_shm_attach->is_mapped)
		return &tee_shm_attach->sgt;

	sgt = &tee_shm_attach->sgt;

	ret = sg_alloc_table(sgt, tee_shm->resv.sgt.orig_nents, GFP_KERNEL);
	if (ret) {
		pr_err("failed to alloc sgt.\n");
		return ERR_PTR(-ENOMEM);
	}

	rd = tee_shm->resv.sgt.sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	if (dir != DMA_NONE) {
		nents = dma_map_sg(attach->dev, sgt->sgl, sgt->orig_nents, dir);
		if (!nents) {
			pr_err("failed to map sgl with iommu.\n");
			sg_free_table(sgt);
			sgt = ERR_PTR(-EIO);
			goto err_unlock;
		}
	}

	tee_shm_attach->is_mapped = true;
	tee_shm_attach->dir = dir;
	attach->priv = tee_shm_attach;

err_unlock:
	return sgt;
}

static void __tee_shm_dma_buf_unmap_dma_buf(struct dma_buf_attachment *attach,
		struct sg_table *table,
		enum dma_data_direction dir)
{
}

static void __tee_shm_dma_buf_release(struct dma_buf *dmabuf)
{
	struct tee_shm *shm = dmabuf->priv;
	struct tee_context *ctx;
	struct tee *tee;

	tee = shm->ctx->tee;


	ctx = shm->ctx;

	pr_debug("shm=%p, paddr=%p,s=%d/%d app=\"%s\" pid=%d\n",
		shm, (void *) (unsigned long) shm->resv.paddr,
		(int)shm->size_req, (int) shm->size_alloc,
		current->comm, current->pid);

	tee_shm_free_io(shm);

}

static int __tee_shm_dma_buf_mmap(struct dma_buf *dmabuf,
				  struct vm_area_struct *vma)
{
	struct tee_shm *shm = dmabuf->priv;
	size_t size = vma->vm_end - vma->vm_start;
	struct tee *tee;
	int ret;
	pgprot_t prot;
	unsigned long pfn;

	tee = shm->ctx->tee;

	pfn = shm->resv.paddr >> PAGE_SHIFT;


	if (shm->flags & TEE_SHM_CACHED)
		prot = vma->vm_page_prot;
	else
		prot = pgprot_noncached(vma->vm_page_prot);

	ret =
		remap_pfn_range(vma, vma->vm_start, pfn, size, prot);
	if (!ret)
		vma->vm_private_data = (void *)shm;

	pr_debug("map the shm (p@=%p,s=%dKiB) => %x\n",
		(void *)(unsigned long) shm->resv.paddr,
		(int) size / 1024, (unsigned int) vma->vm_start);

	return ret;
}

static void *__tee_shm_dma_buf_kmap_atomic(struct dma_buf *dmabuf,
		unsigned long pgnum)
{
	return NULL;
}

static void *__tee_shm_dma_buf_kmap(struct dma_buf *db, unsigned long pgnum)
{
	struct tee_shm *shm = db->priv;

	pr_debug("kmap the shm (p@=%p, v@=%p, s=%zdKiB)\n",
		(void *)(unsigned long) shm->resv.paddr,
		(void *) shm->resv.kaddr, shm->size_alloc / 1024);
	/*
	 * A this stage, a shm allocated by the tee
	 * must be have a kernel address
	 */
	return shm->resv.kaddr;
}

static void __tee_shm_dma_buf_kunmap(
	struct dma_buf *db, unsigned long pfn, void *kaddr)
{
	/* unmap is done at the de init of the shm pool */
}

static const struct dma_buf_ops tee_static_shm_dma_buf_ops = {
	.attach = __tee_shm_attach_dma_buf,
	.detach = __tee_shm_detach_dma_buf,
	.map_dma_buf = __tee_shm_dma_buf_map_dma_buf,
	.unmap_dma_buf = __tee_shm_dma_buf_unmap_dma_buf,
	.release = __tee_shm_dma_buf_release,
	.kmap_atomic = __tee_shm_dma_buf_kmap_atomic,
	.kmap = __tee_shm_dma_buf_kmap,
	.kunmap = __tee_shm_dma_buf_kunmap,
	.mmap = __tee_shm_dma_buf_mmap,
};

static int tee_static_shm_export(struct tee *tee, struct tee_shm *shm,
				 int *export)
{
	struct dma_buf *dmabuf;
	int ret = 0;

#if defined(DEFINE_DMA_BUF_EXPORT_INFO)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
#endif

	if (shm_test_nonsecure(shm->flags)) {
		pr_err(
			"cannot export dmabuf for nonsecure buf flags: 0x%x\n",
			shm->flags);
		return -EINVAL;
	}

	/* Temporary fix to support both older and newer kernel versions. */
#if defined(DEFINE_DMA_BUF_EXPORT_INFO)
	exp_info.priv = shm;
	exp_info.ops = &tee_static_shm_dma_buf_ops;
	exp_info.size = shm->size_alloc;
	exp_info.flags = O_RDWR;

	dmabuf = dma_buf_export(&exp_info);
#else
	dmabuf = dma_buf_export(shm, &tee_static_shm_dma_buf_ops,
				shm->size_alloc, O_RDWR, NULL);
#endif
	if (IS_ERR_OR_NULL(dmabuf)) {
		pr_err("dmabuf: couldn't export buffer (%ld)\n",
			PTR_ERR(dmabuf));
		ret = -EINVAL;
		goto out;
	}

	*export = dma_buf_fd(dmabuf, O_CLOEXEC);
out:
	return ret;
}

static int __tee_ns_shm_vma_fault(struct vm_area_struct *vma,
				  struct vm_fault *vmf)
{
	struct tee_shm *shm = (struct tee_shm *) vma->vm_private_data;
	struct page *page;

	pr_debug("pgoff: 0x%lx\n", vmf->pgoff);

	if (vmf->pgoff >= shm->ns.nr_pages)
		return VM_FAULT_ERROR;

	page = shm->ns.pages[vmf->pgoff];
	get_page(page);

	vmf->page = page;
	return 0;
}

static const struct vm_operations_struct tee_ns_shm_vm_ops = {
	/*	.close = __tee_ns_shm_vma_close, */
	.fault = __tee_ns_shm_vma_fault,
};

static int __tee_ns_shm_release(struct inode *inode, struct file *filp)
{
	struct tee_shm *shm = filp->private_data;

	tee_shm_free_io(shm);
	return 0;
}

static int __tee_ns_shm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &tee_ns_shm_vm_ops;
	vma->vm_private_data = filp->private_data;

	return 0;
}

static const struct file_operations tee_ns_shm_fops = {
	.release = __tee_ns_shm_release,
	.mmap = __tee_ns_shm_mmap,
};

static int tee_ns_shm_export(struct tee *tee, struct tee_shm *shm, int *export)
{
	int fd;

	if (!shm_test_nonsecure(shm->flags)) {
		pr_err("cannot export for static buf flags: 0x%x\n",
			shm->flags);
		return -EINVAL;
	}

	fd = anon_inode_getfd("tz_ns_shm", &tee_ns_shm_fops,
				   (void *) shm, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		pr_err("anon_inode_getfd() failed with %d\n", fd);
		return fd;
	}

	*export = fd;
	return 0;
}

/* called inside tee->lock */
static int tee_ns_shm_inc_ref(struct tee_shm *shm)
{
	/* check old value, if old value < 1, then do not inc ref.
	 * actually this part of logic is already protected by tee->lock
	 */
	atomic_inc(&shm->ns.ref);
	return 0;
}

struct tee_shm *tee_shm_alloc_from_rpc(struct tee *tee, size_t size,
					uint32_t extra_flags)
{
	struct tee_shm *shm;


	mutex_lock(&tee->lock);
	shm = tee_shm_alloc(tee, size,
		TEE_SHM_TEMP | TEE_SHM_FROM_RPC | extra_flags);
	if (IS_ERR_OR_NULL(shm)) {
		pr_err("buffer allocation failed (%ld)\n",
			PTR_ERR(shm));
		goto out;
	}

	tee_inc_stats(&tee->stats[TEE_STATS_SHM_IDX]);
	list_add_tail(&shm->entry, &tee->list_rpc_shm);

	shm->ctx = NULL;

out:
	mutex_unlock(&tee->lock);
	return shm;
}

void tee_shm_free_from_rpc(struct tee_shm *shm)
{
	struct tee *tee;

	pr_debug("shm %p ctx: %p\n",
		shm, shm ? shm->ctx : NULL);

	if (shm == NULL)
		return;

	tee = shm->tee;
	mutex_lock(&tee->lock);

	if (shm->ctx == NULL) {
		tee_dec_stats(&shm->tee->stats[TEE_STATS_SHM_IDX]);
		list_del(&shm->entry);
	}

	tee_shm_free(shm);
	mutex_unlock(&tee->lock);
}

/* Buffer allocated by rpc from fw and to be accessed by the user
 * Not need to be registered as it is not allocated by the user
 */
int tee_shm_fd_for_rpc(struct tee_context *ctx, struct tee_shm_io *shm_io)
{
	struct tee_shm *shm = NULL;
	struct tee *tee = ctx->tee;
	int ret;
	struct list_head *pshm;


	shm_io->fd_shm = 0;

	mutex_lock(&tee->lock);

	if (!list_empty(&tee->list_rpc_shm)) {
		list_for_each(pshm, &tee->list_rpc_shm) {
			shm = list_entry(pshm, struct tee_shm, entry);
			if (shm_test_nonsecure(shm_io->flags)) {
				if ((void *)(unsigned long) shm->ns.token
						== shm_io->buffer)
					goto found;
			} else {
				if ((void *)(unsigned long) shm->resv.paddr
						== shm_io->buffer)
					goto found;
			}
		}
	}

	pr_err("Can't find shm for %p\n", shm_io->buffer);
	ret = -ENOMEM;
	goto out;

found:

	if (shm_test_nonsecure(shm_io->flags))
		ret = tee_ns_shm_export(tee, shm, &shm_io->fd_shm);
	else
		ret = tee_static_shm_export(tee, shm, &shm_io->fd_shm);

	if (ret) {
		ret = -ENOMEM;
		goto out;
	}

	shm->ctx = ctx;
	list_move(&shm->entry, &ctx->list_shm);

	shm->dev = get_device(_DEV(tee));
	ret = tee_get(tee);
	WARN_ON(ret);
	tee_context_get(ctx);

	if (shm_test_nonsecure(shm_io->flags)) {
		/*FIXME check for return value */
		tee_ns_shm_inc_ref(shm);
	} else
		WARN_ON(!tee->ops->shm_inc_ref(shm));
out:
	mutex_unlock(&tee->lock);
	return ret;
}

int tee_shm_alloc_io_perm(struct tee_context *ctx, struct tee_shm_io *shm_io)
{
	struct tee_shm *shm;
	struct tee *tee = ctx->tee;
	int ret;


	if (shm_test_nonsecure(shm_io->flags)) {
		pr_err("permanent shm cannot be nonsecure\n");
		return -EINVAL;
	}

	if (ctx->usr_client)
		shm_io->fd_shm = 0;

	mutex_lock(&tee->lock);
	shm = tee_shm_alloc(tee, shm_io->size, shm_io->flags);
	if (IS_ERR_OR_NULL(shm)) {
		pr_err("buffer allocation failed (%ld)\n",
			PTR_ERR(shm));
		ret = PTR_ERR(shm);
		goto out;
	}

	if (ctx->usr_client) {
		ret = tee_static_shm_export(tee, shm, &shm_io->fd_shm);
		if (ret) {
			tee_shm_free(shm);
			ret = -ENOMEM;
			goto out;
		}

		shm->flags |= TEEC_MEM_DMABUF;
	}

	shm_io->paddr = (void *)(unsigned long) shm->resv.paddr;

	shm->ctx = ctx;
	shm->dev = get_device(_DEV(tee));
	ret = tee_get(tee);
	WARN_ON(ret);
	tee_context_get(ctx);

	tee_inc_stats(&tee->stats[TEE_STATS_SHM_IDX]);
	list_add_tail(&shm->entry, &ctx->list_shm);
out:
	mutex_unlock(&tee->lock);
	return ret;
}

int tee_shm_alloc_io(struct tee_context *ctx, struct tee_shm_io *shm_io)
{
	struct tee_shm *shm;
	struct tee *tee = ctx->tee;
	int ret;


	if (ctx->usr_client)
		shm_io->fd_shm = 0;

	mutex_lock(&tee->lock);
	shm = tee_shm_alloc(tee, shm_io->size, shm_io->flags);
	if (IS_ERR_OR_NULL(shm)) {
		pr_err("buffer allocation failed (%ld)\n",
			PTR_ERR(shm));
		ret = PTR_ERR(shm);
		goto out;
	}

	if (ctx->usr_client) {
		if (shm_test_nonsecure(shm_io->flags))
			ret = tee_ns_shm_export(tee, shm, &shm_io->fd_shm);
		else
			ret = tee_static_shm_export(tee, shm, &shm_io->fd_shm);

		if (ret) {
			tee_shm_free(shm);
			ret = -ENOMEM;
			goto out;
		}

		shm->flags |= TEEC_MEM_DMABUF;
	}

	shm->ctx = ctx;
	shm->dev = get_device(_DEV(tee));
	ret = tee_get(tee);
	WARN_ON(ret);		/* tee_core_get must not issue */
	tee_context_get(ctx);

	tee_inc_stats(&tee->stats[TEE_STATS_SHM_IDX]);
	list_add_tail(&shm->entry, &ctx->list_shm);
out:
	mutex_unlock(&tee->lock);
	return ret;
}

void tee_shm_free_io(struct tee_shm *shm)
{
	struct tee_context *ctx = shm->ctx;
	struct tee *tee = ctx->tee;
	struct device *dev = shm->dev;

	pr_debug("free shm %p\n", shm);

	mutex_lock(&ctx->tee->lock);
	tee_dec_stats(&tee->stats[TEE_STATS_SHM_IDX]);
	list_del(&shm->entry);

	tee_shm_free(shm);
	tee_put(ctx->tee);
	tee_context_put(ctx);
	if (dev)
		put_device(dev);
	mutex_unlock(&tee->lock);
}

static int tee_shm_db_get(struct tee *tee, struct tee_shm *shm, int fd,
			  unsigned int flags, size_t size, int offset)
{
	struct tee_shm_dma_buf *sdb;
	struct dma_buf *dma_buf;
	int ret = 0;

	pr_debug(" > db_get fd=%d flags=%08x\n", fd, flags);

	dma_buf = dma_buf_get(fd);
	if (IS_ERR(dma_buf)) {
		ret = PTR_ERR(dma_buf);
		goto exit;
	}

	sdb = kzalloc(sizeof(*sdb), GFP_KERNEL);
	if (IS_ERR_OR_NULL(sdb)) {
		pr_err("can't alloc tee_shm_dma_buf\n");
		ret = PTR_ERR(sdb);
		goto buf_put;
	}
	shm->resv.sdb = sdb;

	if (dma_buf->size < size + offset) {
		pr_err("dma_buf too small %zd < %zd + %d\n",
			dma_buf->size, size, offset);
		ret = -EINVAL;
		goto free_sdb;
	}

	sdb->attach = dma_buf_attach(dma_buf, _DEV(tee));
	if (IS_ERR_OR_NULL(sdb->attach)) {
		ret = PTR_ERR(sdb->attach);
		goto free_sdb;
	}

	sdb->sgt = dma_buf_map_attachment(sdb->attach, DMA_NONE);
	if (IS_ERR_OR_NULL(sdb->sgt)) {
		ret = PTR_ERR(sdb->sgt);
		goto buf_detach;
	}

	if (sg_nents(sdb->sgt->sgl) != 1) {
		ret = -EINVAL;
		goto buf_unmap;
	}

	shm->resv.paddr = sg_phys(sdb->sgt->sgl) + offset;
	if (dma_buf->ops->attach == __tee_shm_attach_dma_buf)
		sdb->tee_allocated = true;
	else
		sdb->tee_allocated = false;

	shm->flags |= TEEC_MEM_DMABUF;

	pr_debug("fd=%d @p=%p is_tee=%d db=%p\n", fd,
		(void *) (unsigned long) shm->resv.paddr,
		sdb->tee_allocated, dma_buf);

	goto exit;

buf_unmap:
	dma_buf_unmap_attachment(sdb->attach, sdb->sgt, DMA_NONE);
buf_detach:
	dma_buf_detach(dma_buf, sdb->attach);
free_sdb:
	kfree(sdb);
buf_put:
	dma_buf_put(dma_buf);
exit:
	return ret;
}

struct tee_shm *tee_shm_get(struct tee_context *ctx,
				struct TEEC_SharedMemory *c_shm,
				size_t size, int offset)
{
	struct tee_shm *shm;
	struct tee *tee = ctx->tee;
	int ret;

	if (shm_test_nonsecure(c_shm->flags)) {
		pr_err("invalid shared memory flags: 0x%x\n",
			c_shm->flags);
		return NULL;
	}

	pr_debug("> tee_shm_get fd=%d flags=%08x\n",
		c_shm->d.fd, c_shm->flags);

	mutex_lock(&tee->lock);
	shm = kzalloc(sizeof(*shm), GFP_KERNEL);
	if (IS_ERR_OR_NULL(shm)) {
		pr_err("can't alloc tee_shm\n");
		ret = -ENOMEM;
		goto err;
	}

	shm->ctx = ctx;
	shm->tee = tee;
	shm->dev = _DEV(tee);
	shm->flags = c_shm->flags | TEE_SHM_MEMREF;
	shm->size_req = size;
	shm->size_alloc = 0;

	if (c_shm->flags & TEEC_MEM_KAPI) {
		struct tee_shm *kc_shm = (struct tee_shm *)c_shm->d.ptr;

		if (!kc_shm) {
			pr_err("kapi fd null\n");
			ret = -EINVAL;
			goto err;
		}
		shm->resv.paddr = kc_shm->resv.paddr;

		if (kc_shm->size_alloc < size + offset) {
			pr_err("kapi buff too small %zd < %zd + %d\n",
				kc_shm->size_alloc, size, offset);
			ret = -EINVAL;
			goto err;
		}

		pr_debug("fd=%d @p=%p\n",
			c_shm->d.fd, (void *)(unsigned long) shm->resv.paddr);
	} else if (c_shm->d.fd) {
		ret = tee_shm_db_get(tee, shm,
			c_shm->d.fd, c_shm->flags, size, offset);
		if (ret)
			goto err;
	} else if (!c_shm->buffer) {
		pr_debug("null buffer, pass 'as is'\n");
	} else {
		ret = -EINVAL;
		goto err;
	}

	mutex_unlock(&tee->lock);
	return shm;

err:
	kfree(shm);
	mutex_unlock(&tee->lock);
	return ERR_PTR(ret);
}

void tee_shm_put(struct tee_context *ctx, struct tee_shm *shm)
{
	struct tee *tee;

	WARN_ON(!ctx);
	if (!ctx)
		return;

	tee = ctx->tee;
	WARN_ON(!tee);
	if (!tee)
		return;

	WARN_ON(!shm);
	if (!shm)
		return;

	pr_debug("> shm=%p flags=%08x\n",
		(void *) shm, shm->flags);

	WARN_ON(!(shm->flags & TEE_SHM_MEMREF));

	if (shm_test_nonsecure(shm->flags)) {
		pr_warn("invalid shared memory flags: 0x%x\n",
			shm->flags);
		return;
	}

	mutex_lock(&tee->lock);
	if (shm->flags & TEEC_MEM_DMABUF) {
		struct tee_shm_dma_buf *sdb;
		struct dma_buf *dma_buf;

		sdb = shm->resv.sdb;
		dma_buf = sdb->attach->dmabuf;

		pr_debug("db=%p\n", (void *) dma_buf);

		dma_buf_unmap_attachment(sdb->attach, sdb->sgt, DMA_NONE);
		dma_buf_detach(dma_buf, sdb->attach);
		dma_buf_put(dma_buf);

		kfree(sdb);
		sdb = 0;
	}

	kfree(shm);
	mutex_unlock(&tee->lock);
}
