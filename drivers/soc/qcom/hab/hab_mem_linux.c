/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
#include "hab.h"
#include <linux/fdtable.h>
#include <linux/dma-buf.h>
#include "hab_grantable.h"


struct pages_list {
	struct list_head list;
	struct page **pages;
	long npages;
	uint64_t index; /* for mmap first call */
	void *kva;
	uint32_t userflags;
	struct file *filp_owner;
	struct file *filp_mapper;
	int32_t export_id;
	int32_t vcid;
	struct physical_channel *pchan;
	struct kref refcount;
};

struct importer_context {
	int cnt; /* pages allocated for local file */
	struct list_head imp_list;
	struct file *filp;
	rwlock_t implist_lock;
};

static struct pages_list *pages_list_create(
	void *imp_ctx,
	struct export_desc *exp,
	uint32_t userflags)
{
	struct page **pages;
	struct compressed_pfns *pfn_table =
		(struct compressed_pfns *)exp->payload;
	struct pages_list *pglist;
	unsigned long pfn;
	int i, j, k = 0, size;

	if (!pfn_table)
		return ERR_PTR(-EINVAL);

	pfn = pfn_table->first_pfn;
	if (pfn_valid(pfn) == 0 || page_is_ram(pfn) == 0) {
		pr_err("imp sanity failed pfn %lx valid %d ram %d pchan %s\n",
			pfn, pfn_valid(pfn),
			page_is_ram(pfn), exp->pchan->name);
		return ERR_PTR(-EINVAL);
	}

	size = exp->payload_count * sizeof(struct page *);
	pages = kmalloc(size, GFP_KERNEL);
	if (!pages)
		return ERR_PTR(-ENOMEM);

	pglist = kzalloc(sizeof(*pglist), GFP_KERNEL);
	if (!pglist) {
		kfree(pages);
		return ERR_PTR(-ENOMEM);
	}

	for (i = 0; i < pfn_table->nregions; i++) {
		for (j = 0; j < pfn_table->region[i].size; j++) {
			pages[k] = pfn_to_page(pfn+j);
			k++;
		}
		pfn += pfn_table->region[i].size + pfn_table->region[i].space;
	}

	pglist->pages = pages;
	pglist->npages = exp->payload_count;
	pglist->userflags = userflags;
	pglist->export_id = exp->export_id;
	pglist->vcid = exp->vcid_remote;
	pglist->pchan = exp->pchan;

	kref_init(&pglist->refcount);

	return pglist;
}

static void pages_list_destroy(struct kref *refcount)
{
	struct pages_list *pglist = container_of(refcount,
				struct pages_list, refcount);

	if (pglist->kva)
		vunmap(pglist->kva);

	kfree(pglist->pages);

	kfree(pglist);
}

static void pages_list_get(struct pages_list *pglist)
{
	kref_get(&pglist->refcount);
}

static int pages_list_put(struct pages_list *pglist)
{
	return kref_put(&pglist->refcount, pages_list_destroy);
}

static struct pages_list *pages_list_lookup(
		struct importer_context *imp_ctx,
		uint32_t export_id, struct physical_channel *pchan)
{
	struct pages_list *pglist, *tmp;

	read_lock(&imp_ctx->implist_lock);
	list_for_each_entry_safe(pglist, tmp, &imp_ctx->imp_list, list) {
		if (pglist->export_id == export_id &&
			pglist->pchan == pchan) {
			pages_list_get(pglist);
			read_unlock(&imp_ctx->implist_lock);
			return pglist;
		}
	}
	read_unlock(&imp_ctx->implist_lock);

	return NULL;
}

static void pages_list_add(struct importer_context *imp_ctx,
		struct pages_list *pglist)
{
	pages_list_get(pglist);

	write_lock(&imp_ctx->implist_lock);
	list_add_tail(&pglist->list,  &imp_ctx->imp_list);
	imp_ctx->cnt++;
	write_unlock(&imp_ctx->implist_lock);
}

static void pages_list_remove(struct importer_context *imp_ctx,
		struct pages_list *pglist)
{
	write_lock(&imp_ctx->implist_lock);
	list_del(&pglist->list);
	imp_ctx->cnt--;
	write_unlock(&imp_ctx->implist_lock);

	pages_list_put(pglist);
}

void *habmm_hyp_allocate_grantable(int page_count,
		uint32_t *sizebytes)
{
	if (!sizebytes || !page_count)
		return NULL;

	*sizebytes = page_count * sizeof(struct grantable);
	return vmalloc(*sizebytes);
}

static int match_file(const void *p, struct file *file, unsigned int fd)
{
	/*
	 * We must return fd + 1 because iterate_fd stops searching on
	 * non-zero return, but 0 is a valid fd.
	 */
	return (p == file) ? (fd + 1) : 0;
}


static int habmem_get_dma_pages_from_va(unsigned long address,
		int page_count,
		struct page **pages)
{
	struct vm_area_struct *vma;
	struct dma_buf *dmabuf = NULL;
	unsigned long offset;
	unsigned long page_offset;
	struct scatterlist *s;
	struct sg_table *sg_table = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct page *page;
	int i, j, rc = 0;
	int fd;

	vma = find_vma(current->mm, address);
	if (!vma || !vma->vm_file) {
		pr_err("cannot find vma\n");
		rc = -EBADF;
		goto err;
	}

	/* Look for the fd that matches this the vma file */
	fd = iterate_fd(current->files, 0, match_file, vma->vm_file);
	if (fd == 0) {
		pr_err("iterate_fd failed\n");
		rc = -EBADF;
		goto err;
	}

	offset = address - vma->vm_start;
	page_offset = offset/PAGE_SIZE;

	dmabuf = dma_buf_get(fd - 1);
	if (IS_ERR_OR_NULL(dmabuf)) {
		pr_err("dma_buf_get failed fd %d ret %pK\n", fd, dmabuf);
		rc = -EBADF;
		goto err;
	}

	attach = dma_buf_attach(dmabuf, hab_driver.dev);
	if (IS_ERR_OR_NULL(attach)) {
		pr_err("dma_buf_attach failed\n");
		rc = -EBADF;
		goto err;
	}

	sg_table = dma_buf_map_attachment(attach, DMA_TO_DEVICE);

	if (IS_ERR_OR_NULL(sg_table)) {
		pr_err("dma_buf_map_attachment failed\n");
		rc = -EBADF;
		goto err;
	}

	for_each_sg(sg_table->sgl, s, sg_table->nents, i) {
		page = sg_page(s);

		for (j = page_offset; j < (s->length >> PAGE_SHIFT); j++) {
			pages[rc] = nth_page(page, j);
			rc++;
			if (rc >= page_count)
				break;
		}
		if (rc >= page_count)
			break;

		if (page_offset > (s->length >> PAGE_SHIFT)) {
			/* carry-over the remaining offset to next s list */
			page_offset = page_offset-(s->length >> PAGE_SHIFT);
		} else {
			/*
			 * the page_offset is within this s list
			 * there is no more offset for the next s list
			 */
			page_offset = 0;
		}

	}

err:
	if (!IS_ERR_OR_NULL(sg_table))
		dma_buf_unmap_attachment(attach, sg_table, DMA_TO_DEVICE);
	if (!IS_ERR_OR_NULL(attach))
		dma_buf_detach(dmabuf, attach);
	if (!IS_ERR_OR_NULL(dmabuf))
		dma_buf_put(dmabuf);
	return rc;
}

static int habmem_get_dma_pages_from_fd(int32_t fd,
		int page_count,
		struct page **pages)
{
	struct dma_buf *dmabuf = NULL;
	struct scatterlist *s;
	struct sg_table *sg_table = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct page *page;
	int i, j, rc = 0;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		pr_err("dma_buf_get failed fd %d ret %pK\n", fd, dmabuf);
		rc = -EBADF;
		goto err;
	}

	attach = dma_buf_attach(dmabuf, hab_driver.dev);
	if (IS_ERR_OR_NULL(attach)) {
		pr_err("dma_buf_attach failed\n");
		rc = -EBADF;
		goto err;
	}

	sg_table = dma_buf_map_attachment(attach, DMA_TO_DEVICE);

	if (IS_ERR_OR_NULL(sg_table)) {
		pr_err("dma_buf_map_attachment failed\n");
		rc = -EBADF;
		goto err;
	}

	for_each_sg(sg_table->sgl, s, sg_table->nents, i) {
		page = sg_page(s);
		pr_debug("sgl length %d\n", s->length);

		for (j = 0; j < (s->length >> PAGE_SHIFT); j++) {
			pages[rc] = nth_page(page, j);
			rc++;
			if (rc >= page_count)
				break;
		}

		if (rc >= page_count)
			break;
	}

err:
	if (!IS_ERR_OR_NULL(sg_table))
		dma_buf_unmap_attachment(attach, sg_table, DMA_TO_DEVICE);
	if (!IS_ERR_OR_NULL(attach))
		dma_buf_detach(dmabuf, attach);
	if (!IS_ERR_OR_NULL(dmabuf))
		dma_buf_put(dmabuf);
	return rc;
}

/*
 * exporter - grant & revoke
 * degenerate sharabled page list based on CPU friendly virtual "address".
 * The result as an array is stored in ppdata to return to caller
 * page size 4KB is assumed
 */
int habmem_hyp_grant_user(unsigned long address,
		int page_count,
		int flags,
		int remotedom,
		void *ppdata,
		int *compressed,
		int *compressed_size)
{
	int i, ret = 0;
	struct grantable *item = (struct grantable *)ppdata;
	struct page **pages;

	pages = vmalloc(page_count * sizeof(struct page *));
	if (!pages)
		return -ENOMEM;

	down_read(&current->mm->mmap_sem);

	if (HABMM_EXP_MEM_TYPE_DMA & flags) {
		ret = habmem_get_dma_pages_from_va(address,
			page_count,
			pages);
	} else if (HABMM_EXPIMP_FLAGS_FD & flags) {
		ret = habmem_get_dma_pages_from_fd(address,
			page_count,
			pages);
	} else {
		ret = get_user_pages(current, current->mm,
			address,
			page_count,
			FOLL_WRITE | FOLL_FORCE,
			pages,
			NULL);
	}

	if (ret > 0) {
		for (i = 0; i < page_count; i++)
			item[i].pfn = page_to_pfn(pages[i]);
	} else {
		pr_err("get %d user pages failed %d flags %d\n",
			page_count, ret, flags);
	}

	vfree(pages);
	up_read(&current->mm->mmap_sem);
	return ret;
}
/*
 * exporter - grant & revoke
 * generate shareable page list based on CPU friendly virtual "address".
 * The result as an array is stored in ppdata to return to caller
 * page size 4KB is assumed
 */
int habmem_hyp_grant(unsigned long address,
		int page_count,
		int flags,
		int remotedom,
		void *ppdata,
		int *compressed,
		int *compressed_size)
{
	int i;
	struct grantable *item;
	void *kva = (void *)(uintptr_t)address;
	int is_vmalloc = is_vmalloc_addr(kva);

	item = (struct grantable *)ppdata;

	for (i = 0; i < page_count; i++) {
		kva = (void *)(uintptr_t)(address + i*PAGE_SIZE);
		if (is_vmalloc)
			item[i].pfn = page_to_pfn(vmalloc_to_page(kva));
		else
			item[i].pfn = page_to_pfn(virt_to_page(kva));
	}

	return 0;
}

int habmem_hyp_revoke(void *expdata, uint32_t count)
{
	return 0;
}

void *habmem_imp_hyp_open(void)
{
	struct importer_context *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return NULL;

	rwlock_init(&priv->implist_lock);
	INIT_LIST_HEAD(&priv->imp_list);

	return priv;
}

void habmem_imp_hyp_close(void *imp_ctx, int kernel)
{
	struct importer_context *priv = imp_ctx;
	struct pages_list *pglist, *pglist_tmp;

	if (!priv)
		return;

	list_for_each_entry_safe(pglist, pglist_tmp, &priv->imp_list, list)
		pages_list_remove(priv, pglist);

	kfree(priv);
}

static struct sg_table *hab_mem_map_dma_buf(
	struct dma_buf_attachment *attachment,
	enum dma_data_direction direction)
{
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct pages_list *pglist = dmabuf->priv;
	struct sg_table *sgt;
	struct scatterlist *sg;
	int i;
	int ret = 0;
	struct page **pages = pglist->pages;

	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(sgt, pglist->npages, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		return ERR_PTR(-ENOMEM);
	}

	for_each_sg(sgt->sgl, sg, pglist->npages, i) {
		sg_set_page(sg, pages[i], PAGE_SIZE, 0);
	}

	return sgt;
}


static void hab_mem_unmap_dma_buf(struct dma_buf_attachment *attachment,
	struct sg_table *sgt,
	enum dma_data_direction direction)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static int hab_map_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct pages_list *pglist;
	unsigned long offset, fault_offset, fault_index;
	int page_idx;

	if (vma == NULL)
		return VM_FAULT_SIGBUS;

	offset = vma->vm_pgoff << PAGE_SHIFT;

	/* PHY address */
	fault_offset =
		(unsigned long)vmf->virtual_address - vma->vm_start + offset;
	fault_index = fault_offset>>PAGE_SHIFT;

	pglist  = vma->vm_private_data;

	page_idx = fault_index - pglist->index;
	if (page_idx < 0 || page_idx >= pglist->npages) {
		pr_err("Out of page array! page_idx %d, pg cnt %ld",
			page_idx, pglist->npages);
		return VM_FAULT_SIGBUS;
	}

	page = pglist->pages[page_idx];
	get_page(page);
	vmf->page = page;
	return 0;
}

static void hab_map_open(struct vm_area_struct *vma)
{
	struct pages_list *pglist =
	    (struct pages_list *)vma->vm_private_data;

	pages_list_get(pglist);
}

static void hab_map_close(struct vm_area_struct *vma)
{
	struct pages_list *pglist =
	    (struct pages_list *)vma->vm_private_data;

	pages_list_put(pglist);
	vma->vm_private_data = NULL;
}

static const struct vm_operations_struct habmem_vm_ops = {
	.fault = hab_map_fault,
	.open = hab_map_open,
	.close = hab_map_close,
};

static int hab_buffer_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct pages_list *pglist = vma->vm_private_data;
	pgoff_t page_offset;
	int ret;

	page_offset = ((unsigned long)vmf->virtual_address - vma->vm_start) >>
		PAGE_SHIFT;

	if (page_offset > pglist->npages)
		return VM_FAULT_SIGBUS;

	ret = vm_insert_page(vma, (unsigned long)vmf->virtual_address,
			     pglist->pages[page_offset]);

	switch (ret) {
	case 0:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	case -EBUSY:
		return VM_FAULT_RETRY;
	case -EFAULT:
	case -EINVAL:
		return VM_FAULT_SIGBUS;
	default:
		WARN_ON(1);
		return VM_FAULT_SIGBUS;
	}
}

static void hab_buffer_open(struct vm_area_struct *vma)
{
}

static void hab_buffer_close(struct vm_area_struct *vma)
{
}

static const struct vm_operations_struct hab_buffer_vm_ops = {
	.fault = hab_buffer_fault,
	.open = hab_buffer_open,
	.close = hab_buffer_close,
};

static int hab_mem_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct pages_list *pglist = dmabuf->priv;
	uint32_t obj_size = pglist->npages << PAGE_SHIFT;

	if (vma == NULL)
		return VM_FAULT_SIGBUS;

	/* Check for valid size. */
	if (obj_size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &hab_buffer_vm_ops;
	vma->vm_private_data = pglist;
	vma->vm_flags |= VM_MIXEDMAP;

	if (!(pglist->userflags & HABMM_IMPORT_FLAGS_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return 0;
}

static void hab_mem_dma_buf_release(struct dma_buf *dmabuf)
{
	struct pages_list *pglist = dmabuf->priv;

	pages_list_put(pglist);
}

static void *hab_mem_dma_buf_kmap(struct dma_buf *dmabuf,
		unsigned long offset)
{
	return NULL;
}

static void hab_mem_dma_buf_kunmap(struct dma_buf *dmabuf,
		unsigned long offset,
		void *ptr)
{
}

static struct dma_buf_ops dma_buf_ops = {
	.map_dma_buf = hab_mem_map_dma_buf,
	.unmap_dma_buf = hab_mem_unmap_dma_buf,
	.mmap = hab_mem_mmap,
	.release = hab_mem_dma_buf_release,
	.kmap_atomic = hab_mem_dma_buf_kmap,
	.kunmap_atomic = hab_mem_dma_buf_kunmap,
	.kmap = hab_mem_dma_buf_kmap,
	.kunmap = hab_mem_dma_buf_kunmap,
};

static int habmem_imp_hyp_map_fd(void *imp_ctx,
	struct export_desc *exp,
	uint32_t userflags,
	int32_t *pfd)
{
	struct pages_list *pglist;
	struct importer_context *priv = imp_ctx;
	int32_t fd = -1;
	int ret;
	struct dma_buf *dmabuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	if (!priv)
		return -EINVAL;

	pglist = pages_list_lookup(priv, exp->export_id, exp->pchan);
	if (pglist)
		goto buffer_ready;

	pglist = pages_list_create(imp_ctx, exp, userflags);
	if (IS_ERR(pglist))
		return PTR_ERR(pglist);

	pages_list_add(priv, pglist);

buffer_ready:
	exp_info.ops = &dma_buf_ops;
	exp_info.size = pglist->npages << PAGE_SHIFT;
	exp_info.flags = O_RDWR;
	exp_info.priv = pglist;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		pr_err("export to dmabuf failed\n");
		ret = PTR_ERR(dmabuf);
		goto proc_end;
	}
	pages_list_get(pglist);

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0) {
		pr_err("dma buf to fd failed\n");
		dma_buf_put(dmabuf);
		ret = -EINVAL;
		goto proc_end;
	}

proc_end:
	*pfd = fd;
	pages_list_put(pglist);
	return 0;
}

static int habmem_imp_hyp_map_kva(void *imp_ctx,
	struct export_desc *exp,
	uint32_t userflags,
	void **pkva)
{
	struct pages_list *pglist;
	struct importer_context *priv = imp_ctx;
	pgprot_t prot = PAGE_KERNEL;

	if (!priv)
		return -EINVAL;

	pglist = pages_list_lookup(priv, exp->export_id, exp->pchan);
	if (pglist)
		goto buffer_ready;

	pglist = pages_list_create(imp_ctx, exp, userflags);
	if (IS_ERR(pglist))
		return PTR_ERR(pglist);

	pages_list_add(priv, pglist);

buffer_ready:
	if (pglist->kva)
		goto pro_end;

	if (pglist->userflags != userflags) {
		pr_info("exp %d: userflags: 0x%x -> 0x%x\n",
			exp->export_id, pglist->userflags, userflags);
		pglist->userflags = userflags;
	}

	if (!(userflags & HABMM_IMPORT_FLAGS_CACHED))
		prot = pgprot_writecombine(prot);

	pglist->kva = vmap(pglist->pages, pglist->npages, VM_MAP, prot);
	if (pglist->kva == NULL) {
		pr_err("%ld pages vmap failed\n", pglist->npages);
		return -ENOMEM;
	}

pro_end:
	*pkva = pglist->kva;
	pages_list_put(pglist);
	return 0;
}

static int habmem_imp_hyp_map_uva(void *imp_ctx,
	struct export_desc *exp,
	uint32_t userflags,
	uint64_t *index)
{
	struct pages_list *pglist;
	struct importer_context *priv = imp_ctx;

	if (!priv)
		return -EINVAL;

	pglist = pages_list_lookup(priv, exp->export_id, exp->pchan);
	if (pglist)
		goto buffer_ready;

	pglist = pages_list_create(imp_ctx, exp, userflags);
	if (IS_ERR(pglist))
		return PTR_ERR(pglist);

	pages_list_add(priv, pglist);

buffer_ready:
	if (pglist->index)
		goto proc_end;

	pglist->index = page_to_phys(pglist->pages[0]) >> PAGE_SHIFT;

proc_end:
	*index = pglist->index << PAGE_SHIFT;
	pages_list_put(pglist);
	return 0;
}

int habmem_imp_hyp_map(void *imp_ctx, struct hab_import *param,
		struct export_desc *exp, int kernel)
{
	int ret = 0;

	if (kernel)
		ret = habmem_imp_hyp_map_kva(imp_ctx, exp,
					param->flags,
					(void **)&param->kva);
	else if (param->flags & HABMM_EXPIMP_FLAGS_FD)
		ret = habmem_imp_hyp_map_fd(imp_ctx, exp,
					param->flags,
					(int32_t *)&param->kva);
	else
		ret = habmem_imp_hyp_map_uva(imp_ctx, exp,
					param->flags,
					&param->index);

	return ret;
}

int habmm_imp_hyp_unmap(void *imp_ctx, struct export_desc *exp, int kernel)
{
	struct importer_context *priv = imp_ctx;
	struct pages_list *pglist;

	pglist = pages_list_lookup(priv, exp->export_id, exp->pchan);
	if (!pglist) {
		pr_err("failed to find export id %u\n", exp->export_id);
		return -EINVAL;
	}

	pages_list_remove(priv, pglist);

	pages_list_put(pglist);

	return 0;
}

int habmem_imp_hyp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct uhab_context *ctx = (struct uhab_context *) filp->private_data;
	struct importer_context *imp_ctx = ctx->import_ctx;
	long length = vma->vm_end - vma->vm_start;
	struct pages_list *pglist;
	int bfound = 0;
	int ret = 0;

	read_lock(&imp_ctx->implist_lock);
	list_for_each_entry(pglist, &imp_ctx->imp_list, list) {
		if ((pglist->index == vma->vm_pgoff) &&
			((length <= pglist->npages * PAGE_SIZE))) {
			bfound = 1;
			pages_list_get(pglist);
			break;
		}
	}
	read_unlock(&imp_ctx->implist_lock);

	if (!bfound) {
		pr_err("Failed to find pglist vm_pgoff: %ld\n", vma->vm_pgoff);
		return -EINVAL;
	}

	if (length > pglist->npages * PAGE_SIZE) {
		pr_err("Error vma length %ld not matching page list %ld\n",
			length, pglist->npages * PAGE_SIZE);
		ret = -EINVAL;
		goto proc_end;
	}

	vma->vm_ops = &habmem_vm_ops;

	vma->vm_private_data = pglist;

	if (!(pglist->userflags & HABMM_IMPORT_FLAGS_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return 0;

proc_end:
	pages_list_put(pglist);
	return ret;
}

int habmm_imp_hyp_map_check(void *imp_ctx, struct export_desc *exp)
{
	struct importer_context *priv = imp_ctx;
	struct pages_list *pglist;
	int found = 0;

	pglist = pages_list_lookup(priv, exp->export_id, exp->pchan);
	if (pglist) {
		found = 1;
		pages_list_put(pglist);
	}

	return found;
}
