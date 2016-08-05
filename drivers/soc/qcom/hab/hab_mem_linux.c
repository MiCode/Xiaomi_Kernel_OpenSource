/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
	int kernel;
	void *kva;
	void *uva;
	int refcntk;
	int refcntu;
	uint32_t userflags;
	struct file *filp_owner;
	struct file *filp_mapper;
};

struct importer_context {
	int cnt; /* pages allocated for local file */
	struct list_head imp_list;
	struct file *filp;
};

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


static int habmem_get_dma_pages(unsigned long address,
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
	if (!vma || !vma->vm_file)
		goto err;

	/* Look for the fd that matches this the vma file */
	fd = iterate_fd(current->files, 0, match_file, vma->vm_file);
	if (fd == 0) {
		pr_err("iterate_fd failed\n");
		goto err;
	}

	offset = address - vma->vm_start;
	page_offset = offset/PAGE_SIZE;

	dmabuf = dma_buf_get(fd - 1);

	attach = dma_buf_attach(dmabuf, hab_driver.dev);
	if (IS_ERR_OR_NULL(attach)) {
		pr_err("dma_buf_attach failed\n");
		goto err;
	}

	sg_table = dma_buf_map_attachment(attach, DMA_TO_DEVICE);

	if (IS_ERR_OR_NULL(sg_table)) {
		pr_err("dma_buf_map_attachment failed\n");
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

int habmem_hyp_grant_user(unsigned long address,
		int page_count,
		int flags,
		int remotedom,
		void *ppdata)
{
	int i, ret = 0;
	struct grantable *item = (struct grantable *)ppdata;
	struct page **pages;

	pages = vmalloc(page_count * sizeof(struct page *));
	if (!pages)
		return -ENOMEM;

	down_read(&current->mm->mmap_sem);

	if (HABMM_EXP_MEM_TYPE_DMA & flags) {
		ret = habmem_get_dma_pages(address,
			page_count,
			pages);
	} else {
		ret = get_user_pages(current, current->mm,
			address,
			page_count,
			1,
			1,
			pages,
			NULL);
	}

	if (ret > 0) {
		for (i = 0; i < page_count; i++)
			item[i].pfn = page_to_pfn(pages[i]);
	} else {
		pr_err("get %d user pages failed: %d\n", page_count, ret);
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
		void *ppdata)
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

	INIT_LIST_HEAD(&priv->imp_list);

	return priv;
}

void habmem_imp_hyp_close(void *imp_ctx, int kernel)
{
	struct importer_context *priv = imp_ctx;
	struct pages_list *pglist, *pglist_tmp;

	if (!priv)
		return;

	list_for_each_entry_safe(pglist, pglist_tmp, &priv->imp_list, list) {
		if (kernel && pglist->kva)
			vunmap(pglist->kva);

		list_del(&pglist->list);
		priv->cnt--;

		vfree(pglist->pages);
		kfree(pglist);
	}

	kfree(priv);
}

/*
 * setup pages, be ready for the following mmap call
 * index is output to refer to this imported buffer described by the import data
 */
long habmem_imp_hyp_map(void *imp_ctx,
		void *impdata,
		uint32_t count,
		uint32_t remotedom,
		uint64_t *index,
		void **pkva,
		int kernel,
		uint32_t userflags)
{
	struct page **pages;
	struct compressed_pfns *pfn_table = impdata;
	struct pages_list *pglist;
	struct importer_context *priv = imp_ctx;
	unsigned long pfn;
	int i, j, k = 0;

	if (!pfn_table || !priv)
		return -EINVAL;

	pages = vmalloc(count * sizeof(struct page *));
	if (!pages)
		return -ENOMEM;

	pglist = kzalloc(sizeof(*pglist), GFP_KERNEL);
	if (!pglist) {
		vfree(pages);
		return -ENOMEM;
	}

	pfn = pfn_table->first_pfn;
	for (i = 0; i < pfn_table->nregions; i++) {
		for (j = 0; j < pfn_table->region[i].size; j++) {
			pages[k] = pfn_to_page(pfn+j);
			k++;
		}
		pfn += pfn_table->region[i].size + pfn_table->region[i].space;
	}

	pglist->pages = pages;
	pglist->npages = count;
	pglist->kernel = kernel;
	pglist->index = page_to_phys(pages[0]) >> PAGE_SHIFT;
	pglist->refcntk = pglist->refcntu = 0;
	pglist->userflags = userflags;

	*index = pglist->index << PAGE_SHIFT;

	if (kernel) {
		pgprot_t prot = PAGE_KERNEL;

		if (!(userflags & HABMM_IMPORT_FLAGS_CACHED))
			prot = pgprot_writecombine(prot);

		pglist->kva = vmap(pglist->pages, pglist->npages, VM_MAP, prot);
		if (pglist->kva == NULL) {
			vfree(pages);
			kfree(pglist);
			pr_err("%ld pages vmap failed\n", pglist->npages);
			return -ENOMEM;
		}

		pglist->uva = NULL;
		pglist->refcntk++;
		*pkva = pglist->kva;
		*index = (uint64_t)((uintptr_t)pglist->kva);
	} else {
		pglist->kva = NULL;
	}

	list_add_tail(&pglist->list,  &priv->imp_list);
	priv->cnt++;

	return 0;
}

/* the input index is PHY address shifted for uhab, and kva for khab */
long habmm_imp_hyp_unmap(void *imp_ctx,
		uint64_t index,
		uint32_t count,
		int kernel)
{
	struct importer_context *priv = imp_ctx;
	struct pages_list *pglist;
	int found = 0;
	uint64_t pg_index = index >> PAGE_SHIFT;

	list_for_each_entry(pglist, &priv->imp_list, list) {
		if (kernel) {
			if (pglist->kva == (void *)((uintptr_t)index))
				found  = 1;
		} else {
			if (pglist->index == pg_index)
				found  = 1;
		}

		if (found) {
			list_del(&pglist->list);
			priv->cnt--;
			break;
		}
	}

	if (!found) {
		pr_err("failed to find export id on index %llx\n", index);
		return -EINVAL;
	}

	if (kernel)
		if (pglist->kva)
			vunmap(pglist->kva);

	vfree(pglist->pages);
	kfree(pglist);

	return 0;
}

static int hab_map_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct pages_list *pglist;

	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

	/* PHY address */
	unsigned long fault_offset =
		(unsigned long)vmf->virtual_address - vma->vm_start + offset;
	unsigned long fault_index = fault_offset>>PAGE_SHIFT;
	int page_idx;

	if (vma == NULL)
		return VM_FAULT_SIGBUS;

	pglist  = vma->vm_private_data;

	page_idx = fault_index - pglist->index;
	if (page_idx < 0 || page_idx >= pglist->npages) {
		pr_err("Out of page array. page_idx %d, pg cnt %ld",
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
}

static void hab_map_close(struct vm_area_struct *vma)
{
}

static const struct vm_operations_struct habmem_vm_ops = {

	.fault = hab_map_fault,
	.open = hab_map_open,
	.close = hab_map_close,
};

int habmem_imp_hyp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct uhab_context *ctx = (struct uhab_context *) filp->private_data;
	struct importer_context *imp_ctx = ctx->import_ctx;
	long length = vma->vm_end - vma->vm_start;
	struct pages_list *pglist;
	int bfound = 0;

	list_for_each_entry(pglist, &imp_ctx->imp_list, list) {
		if (pglist->index == vma->vm_pgoff) {
			bfound = 1;
			break;
		}
	}

	if (!bfound) {
		pr_err("Failed to find pglist vm_pgoff: %d\n", vma->vm_pgoff);
		return -EINVAL;
	}

	if (length > pglist->npages * PAGE_SIZE) {
		pr_err("Error vma length %ld not matching page list %ld\n",
			length, pglist->npages * PAGE_SIZE);
		return -EINVAL;
	}

	vma->vm_ops = &habmem_vm_ops;

	vma->vm_private_data = pglist;

	if (!(pglist->userflags & HABMM_IMPORT_FLAGS_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return 0;
}
