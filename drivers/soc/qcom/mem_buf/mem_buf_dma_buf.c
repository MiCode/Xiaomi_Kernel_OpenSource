// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "mem_buf_vmperm: " fmt

#include <linux/highmem.h>
#include <linux/mem-buf-exporter.h>
#include "mem-buf-private.h"

struct mem_buf_vmperm {
	u32 flags;
	int current_vm_perms;
	u32 mapcount;
	int *vmids;
	int *perms;
	unsigned int nr_acl_entries;
	unsigned int max_acl_entries;
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	hh_memparcel_handle_t memparcel_hdl;
	struct mutex lock;
};

/*
 * Ensures the vmperm can hold at least nr_acl_entries.
 * Caller must hold vmperm->lock.
 */
static int mem_buf_vmperm_resize(struct mem_buf_vmperm *vmperm,
					u32 new_size)
{
	int *vmids_copy, *perms_copy, *vmids, *perms;
	u32 old_size;

	old_size = vmperm->max_acl_entries;
	if (old_size >= new_size)
		return 0;

	vmids = vmperm->vmids;
	perms = vmperm->perms;
	vmids_copy = kcalloc(new_size, sizeof(*vmids), GFP_KERNEL);
	if (!vmids_copy)
		return -ENOMEM;
	perms_copy = kcalloc(new_size, sizeof(*perms), GFP_KERNEL);
	if (!perms_copy)
		goto out_perms;

	if (vmperm->vmids) {
		memcpy(vmids_copy, vmids, sizeof(*vmids) * old_size);
		kfree(vmids);
	}
	if (vmperm->perms) {
		memcpy(perms_copy, perms, sizeof(*perms) * old_size);
		kfree(perms);
	}
	vmperm->vmids = vmids_copy;
	vmperm->perms = perms_copy;
	vmperm->max_acl_entries = new_size;
	return 0;

out_perms:
	kfree(vmids_copy);
	return -ENOMEM;
}

/*
 * Caller should hold vmperm->lock.
 */
static void mem_buf_vmperm_update_state(struct mem_buf_vmperm *vmperm, int *vmids,
			 int *perms, u32 nr_acl_entries)
{
	int i;
	size_t size = sizeof(*vmids) * nr_acl_entries;

	WARN_ON(vmperm->max_acl_entries < nr_acl_entries);

	memcpy(vmperm->vmids, vmids, size);
	memcpy(vmperm->perms, perms, size);
	vmperm->nr_acl_entries = nr_acl_entries;

	vmperm->current_vm_perms = 0;
	for (i = 0; i < nr_acl_entries; i++) {
		if (vmids[i] == current_vmid)
			vmperm->current_vm_perms = perms[i];
	}
}

/*
 * Some types of errors may leave the memory in an unknown state.
 * Since we cannot guarantee that accessing this memory is safe,
 * acquire an extra reference count to the underlying dmabuf to
 * prevent it from being freed.
 * If this error occurs during dma_buf_release(), the file refcount
 * will already be zero. In this case handling the error is the caller's
 * responsibility.
 */
static void mem_buf_vmperm_set_err(struct mem_buf_vmperm *vmperm)
{
	get_dma_buf(vmperm->dmabuf);
	vmperm->flags |= MEM_BUF_WRAPPER_FLAG_ERR;
}

static struct mem_buf_vmperm *mem_buf_vmperm_alloc_flags(
	struct sg_table *sgt, u32 flags,
	int *vmids, int *perms, u32 nr_acl_entries)
{
	struct mem_buf_vmperm *vmperm;
	int ret;

	vmperm = kzalloc(sizeof(*vmperm), GFP_KERNEL);
	if (!vmperm)
		return ERR_PTR(-ENOMEM);

	mutex_init(&vmperm->lock);
	mutex_lock(&vmperm->lock);
	ret = mem_buf_vmperm_resize(vmperm, nr_acl_entries);
	if (ret)
		goto err_resize_state;

	mem_buf_vmperm_update_state(vmperm, vmids, perms,
					nr_acl_entries);
	mutex_unlock(&vmperm->lock);
	vmperm->sgt = sgt;
	vmperm->flags = flags;

	return vmperm;

err_resize_state:
	mutex_unlock(&vmperm->lock);
	kfree(vmperm);
	return ERR_PTR(-ENOMEM);
}

/* Must be freed via mem_buf_vmperm_release. */
struct mem_buf_vmperm *mem_buf_vmperm_alloc_accept(struct sg_table *sgt,
	hh_memparcel_handle_t memparcel_hdl)
{
	int vmids[1];
	int perms[1];
	struct mem_buf_vmperm *vmperm;

	vmids[0] = current_vmid;
	perms[0] = PERM_READ | PERM_WRITE | PERM_EXEC;
	vmperm = mem_buf_vmperm_alloc_flags(sgt,
		MEM_BUF_WRAPPER_FLAG_ACCEPT,
		vmids, perms, 1);
	if (IS_ERR(vmperm))
		return vmperm;

	vmperm->memparcel_hdl = memparcel_hdl;
	return vmperm;
}
EXPORT_SYMBOL(mem_buf_vmperm_alloc_accept);

struct mem_buf_vmperm *mem_buf_vmperm_alloc_staticvm(struct sg_table *sgt,
	int *vmids, int *perms, u32 nr_acl_entries)
{
	return mem_buf_vmperm_alloc_flags(sgt,
		MEM_BUF_WRAPPER_FLAG_STATIC_VM,
		vmids, perms, nr_acl_entries);
}
EXPORT_SYMBOL(mem_buf_vmperm_alloc_staticvm);

struct mem_buf_vmperm *mem_buf_vmperm_alloc(struct sg_table *sgt)
{
	int vmids[1];
	int perms[1];

	vmids[0] = current_vmid;
	perms[0] = PERM_READ | PERM_WRITE | PERM_EXEC;
	return mem_buf_vmperm_alloc_flags(sgt, 0,
		vmids, perms, 1);
}
EXPORT_SYMBOL(mem_buf_vmperm_alloc);

static int __mem_buf_vmperm_reclaim(struct mem_buf_vmperm *vmperm)
{
	int ret;
	int new_vmids[] = {current_vmid};
	int new_perms[] = {PERM_READ | PERM_WRITE | PERM_EXEC};

	ret = mem_buf_unassign_mem(vmperm->sgt, vmperm->vmids,
				vmperm->nr_acl_entries);
	if (ret) {
		pr_err_ratelimited("Reclaim failed\n");
		mem_buf_vmperm_set_err(vmperm);
		return ret;
	}

	mem_buf_vmperm_update_state(vmperm, new_vmids, new_perms, 1);
	vmperm->flags &= ~MEM_BUF_WRAPPER_FLAG_LENDSHARE;
	return 0;
}

static int mem_buf_vmperm_relinquish(struct mem_buf_vmperm *vmperm)
{
	int ret;
	struct hh_sgl_desc *sgl_desc;

	sgl_desc = dup_sgt_to_hh_sgl_desc(vmperm->sgt);
	if (IS_ERR(sgl_desc))
		return PTR_ERR(sgl_desc);

	ret = mem_buf_unmap_mem_s1(sgl_desc);
	kvfree(sgl_desc);
	if (ret)
		return ret;

	ret = mem_buf_unmap_mem_s2(vmperm->memparcel_hdl);
	return ret;
}

int mem_buf_vmperm_release(struct mem_buf_vmperm *vmperm)
{
	int ret = 0;

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_LENDSHARE)
		ret = __mem_buf_vmperm_reclaim(vmperm);
	else if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ACCEPT)
		ret = mem_buf_vmperm_relinquish(vmperm);

	mutex_unlock(&vmperm->lock);
	kfree(vmperm->perms);
	kfree(vmperm->vmids);
	mutex_destroy(&vmperm->lock);
	kfree(vmperm);
	return ret;
}
EXPORT_SYMBOL(mem_buf_vmperm_release);

int mem_buf_dma_buf_attach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
	struct mem_buf_dma_buf_ops *ops;

	ops  = container_of(dmabuf->ops, struct mem_buf_dma_buf_ops, dma_ops);
	return ops->attach(dmabuf, attachment);
}
EXPORT_SYMBOL(mem_buf_dma_buf_attach);

static struct mem_buf_vmperm *to_mem_buf_vmperm(struct dma_buf *dmabuf)
{
	struct mem_buf_dma_buf_ops *ops;

	if (dmabuf->ops->attach != mem_buf_dma_buf_attach)
		return ERR_PTR(-EINVAL);

	ops = container_of(dmabuf->ops, struct mem_buf_dma_buf_ops, dma_ops);
	return ops->lookup(dmabuf);
}

struct dma_buf *
mem_buf_dma_buf_export(const struct dma_buf_export_info *exp_info)
{
	struct mem_buf_vmperm *vmperm;
	struct dma_buf *dmabuf;

	if (exp_info->ops->attach != mem_buf_dma_buf_attach) {
		pr_err("Invalid attach callback %ps\n", exp_info->ops);
		return ERR_PTR(-EINVAL);
	}

	dmabuf = dma_buf_export(exp_info);
	if (IS_ERR(dmabuf))
		return dmabuf;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (WARN_ON(IS_ERR(vmperm))) {
		dma_buf_put(dmabuf);
		return ERR_PTR(-EINVAL);
	}

	vmperm->dmabuf = dmabuf;
	return dmabuf;
}
EXPORT_SYMBOL(mem_buf_dma_buf_export);

void mem_buf_vmperm_pin(struct mem_buf_vmperm *vmperm)
{
	mutex_lock(&vmperm->lock);
	vmperm->mapcount++;
	mutex_unlock(&vmperm->lock);
}
EXPORT_SYMBOL(mem_buf_vmperm_pin);

void mem_buf_vmperm_unpin(struct mem_buf_vmperm *vmperm)
{
	mutex_lock(&vmperm->lock);
	if (!WARN_ON(vmperm->mapcount == 0))
		vmperm->mapcount--;
	mutex_unlock(&vmperm->lock);
}
EXPORT_SYMBOL(mem_buf_vmperm_unpin);

/*
 * DC IVAC requires write permission, so no CMO on read-only buffers.
 * We allow mapping to iommu regardless of permissions.
 * Caller must have previously called mem_buf_vmperm_pin
 */
bool mem_buf_vmperm_can_cmo(struct mem_buf_vmperm *vmperm)
{
	u32 perms = PERM_READ | PERM_WRITE;
	bool ret = false;

	mutex_lock(&vmperm->lock);
	if (((vmperm->current_vm_perms & perms) == perms) && vmperm->mapcount)
		ret = true;
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_vmperm_can_cmo);

bool mem_buf_vmperm_can_mmap(struct mem_buf_vmperm *vmperm, struct vm_area_struct *vma)
{
	bool ret = false;

	mutex_lock(&vmperm->lock);
	if (!vmperm->mapcount)
		goto unlock;
	if (!(vmperm->current_vm_perms & PERM_READ))
		goto unlock;
	if (!(vmperm->current_vm_perms & PERM_WRITE) &&
		vma->vm_flags & VM_WRITE)
		goto unlock;
	if (!(vmperm->current_vm_perms & PERM_EXEC) &&
		vma->vm_flags & VM_EXEC)
		goto unlock;
	ret = true;
unlock:
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_vmperm_can_mmap);

bool mem_buf_vmperm_can_vmap(struct mem_buf_vmperm *vmperm)
{
	u32 perms = PERM_READ | PERM_WRITE;
	bool ret = false;

	mutex_lock(&vmperm->lock);
	if (((vmperm->current_vm_perms & perms) == perms) && vmperm->mapcount)
		ret = true;
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_vmperm_can_vmap);


static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static int qcom_sg_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;

	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void qcom_sg_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

static struct sg_table *qcom_sg_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	unsigned long attrs = 0;
	int ret;
	struct qcom_sg_buffer *buffer = attachment->dmabuf->priv;

	mutex_lock(&buffer->lock);
	mem_buf_vmperm_pin(buffer->vmperm);

	if (!mem_buf_vmperm_can_cmo(buffer->vmperm))
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attrs);
	if (ret) {
		table = ERR_PTR(ret);
		goto err_map_sgtable;
	}

	a->mapped = true;
	mutex_unlock(&buffer->lock);
	return table;

err_map_sgtable:
	mem_buf_vmperm_unpin(buffer->vmperm);
	mutex_unlock(&buffer->lock);
	return table;
}

static void qcom_sg_unmap_dma_buf(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	unsigned long attrs = 0;
	struct qcom_sg_buffer *buffer = attachment->dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!mem_buf_vmperm_can_cmo(buffer->vmperm))
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attrs);
	mem_buf_vmperm_unpin(buffer->vmperm);
	mutex_unlock(&buffer->lock);
}

static int qcom_sg_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);
	if (!mem_buf_vmperm_can_cmo(buffer->vmperm)) {
		mutex_unlock(&buffer->lock);
		return 0;
	}

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sgtable_for_cpu(a->dev, a->table, direction);
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int qcom_sg_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);
	if (!mem_buf_vmperm_can_cmo(buffer->vmperm)) {
		mutex_unlock(&buffer->lock);
		return 0;
	}

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sgtable_for_device(a->dev, a->table, direction);
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static void qcom_sg_vm_ops_open(struct vm_area_struct *vma)
{
	struct mem_buf_vmperm *vmperm = vma->vm_private_data;

	mem_buf_vmperm_pin(vmperm);
}

static void qcom_sg_vm_ops_close(struct vm_area_struct *vma)
{
	struct mem_buf_vmperm *vmperm = vma->vm_private_data;

	mem_buf_vmperm_unpin(vmperm);
}

static const struct vm_operations_struct qcom_sg_vm_ops = {
	.open = qcom_sg_vm_ops_open,
	.close = qcom_sg_vm_ops_close,
};

static int qcom_sg_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct sg_table *table = buffer->sg_table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	mem_buf_vmperm_pin(buffer->vmperm);
	if (!mem_buf_vmperm_can_mmap(buffer->vmperm, vma)) {
		mem_buf_vmperm_unpin(buffer->vmperm);
		return -EPERM;
	}

	vma->vm_ops = &qcom_sg_vm_ops;
	vma->vm_private_data = buffer->vmperm;

	for_each_sgtable_page(table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
				      vma->vm_page_prot);
		if (ret) {
			mem_buf_vmperm_unpin(buffer->vmperm);
			return ret;
		}
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static void *qcom_sg_do_vmap(struct qcom_sg_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;
	void *vaddr;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	vaddr = vmap(pages, npages, VM_MAP, PAGE_KERNEL);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static void *qcom_sg_vmap(struct dma_buf *dmabuf)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	void *vaddr;

	mem_buf_vmperm_pin(buffer->vmperm);
	if (!mem_buf_vmperm_can_vmap(buffer->vmperm)) {
		mem_buf_vmperm_unpin(buffer->vmperm);
		return ERR_PTR(-EPERM);
	}

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		vaddr = buffer->vaddr;
		goto out;
	}

	vaddr = qcom_sg_do_vmap(buffer);
	if (IS_ERR(vaddr)) {
		mem_buf_vmperm_unpin(buffer->vmperm);
		goto out;
	}

	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
out:
	mutex_unlock(&buffer->lock);

	return vaddr;
}

static void qcom_sg_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
	mem_buf_vmperm_unpin(buffer->vmperm);
}

static void qcom_sg_release(struct dma_buf *dmabuf)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	int ret;

	ret = mem_buf_vmperm_release(buffer->vmperm);
	if (!ret)
		buffer->free(buffer);
}

static struct mem_buf_vmperm *qcom_sg_lookup(struct dma_buf *dmabuf)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;

	return buffer->vmperm;
}

const struct mem_buf_dma_buf_ops mem_buf_dma_buf_ops = {
	.lookup = qcom_sg_lookup,
	.attach = qcom_sg_attach,
	.dma_ops = {
		.attach = mem_buf_dma_buf_attach,
		.detach = qcom_sg_detach,
		.map_dma_buf = qcom_sg_map_dma_buf,
		.unmap_dma_buf = qcom_sg_unmap_dma_buf,
		.begin_cpu_access = qcom_sg_dma_buf_begin_cpu_access,
		.end_cpu_access = qcom_sg_dma_buf_end_cpu_access,
		.mmap = qcom_sg_mmap,
		.vmap = qcom_sg_vmap,
		.vunmap = qcom_sg_vunmap,
		.release = qcom_sg_release,
	},
};
EXPORT_SYMBOL(mem_buf_dma_buf_ops);

static int validate_lend_vmids(struct mem_buf_lend_kernel_arg *arg,
				bool is_lend)
{
	int i;
	bool found = false;

	for (i = 0; i < arg->nr_acl_entries; i++) {
		if (arg->vmids[i] == current_vmid) {
			found = true;
			break;
		}
	}

	if (found && is_lend) {
		pr_err_ratelimited("Lend cannot target the current VM\n");
		return -EINVAL;
	} else if (!found && !is_lend) {
		pr_err_ratelimited("Share must target the current VM\n");
		return -EINVAL;
	}
	return 0;
}

int mem_buf_lend_internal(struct dma_buf *dmabuf,
			struct mem_buf_lend_kernel_arg *arg,
			bool is_lend)
{
	struct mem_buf_vmperm *vmperm;
	struct sg_table *sgt;
	int ret;
	int api;

	if (!(mem_buf_capability & MEM_BUF_CAP_SUPPLIER))
		return -EOPNOTSUPP;

	if (!arg->nr_acl_entries || !arg->vmids || !arg->perms)
		return -EINVAL;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm)) {
		pr_err_ratelimited("dmabuf ops %ps are not a mem_buf_dma_buf_ops\n",
				dmabuf->ops);
		return -EINVAL;
	}
	sgt = vmperm->sgt;

	api = mem_buf_vm_get_backend_api(arg->vmids, arg->nr_acl_entries);
	if (api < 0)
		return -EINVAL;

	if (api == MEM_BUF_API_HAVEN) {
		/* Due to hyp-assign batching */
		if (sgt->nents > 1) {
			pr_err_ratelimited("Operation requires physically contiguous memory\n");
			return -EINVAL;
		}

		/* Due to memory-hotplug */
		if ((sg_virt(sgt->sgl) && (SUBSECTION_SIZE - 1)) ||
				(sgt->sgl->length % SUBSECTION_SIZE)) {
			pr_err_ratelimited("Operation requires SUBSECTION_SIZE alignemnt\n");
			return -EINVAL;
		}
	}

	ret = validate_lend_vmids(arg, is_lend);
	if (ret)
		return ret;

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_STATIC_VM) {
		pr_err_ratelimited("dma-buf is staticvm type!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_LENDSHARE) {
		pr_err_ratelimited("dma-buf already lent or shared!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ACCEPT) {
		pr_err_ratelimited("dma-buf not owned by current vm!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->mapcount) {
		pr_err_ratelimited("dma-buf is pinned!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	/*
	 * Although it would be preferrable to require clients to decide
	 * whether they require cache maintenance prior to caling this function
	 * for backwards compatibility with ion we will always do CMO.
	 */
	dma_map_sgtable(mem_buf_dev, vmperm->sgt, DMA_TO_DEVICE, 0);
	dma_unmap_sgtable(mem_buf_dev, vmperm->sgt, DMA_TO_DEVICE, 0);

	ret = mem_buf_vmperm_resize(vmperm, arg->nr_acl_entries);
	if (ret)
		goto err_resize;

	ret = mem_buf_assign_mem(vmperm->sgt, arg->vmids, arg->perms,
				arg->nr_acl_entries);
	if (ret) {
		if (ret == -EADDRNOTAVAIL)
			mem_buf_vmperm_set_err(vmperm);
		goto err_assign;
	}

	if (api == MEM_BUF_API_HAVEN) {
		ret = mem_buf_retrieve_memparcel_hdl(vmperm->sgt, arg->vmids,
				arg->perms, arg->nr_acl_entries,
				&arg->memparcel_hdl);
		if (ret)
			goto err_retrieve_hdl;
	} else {
		arg->memparcel_hdl = 0;
	}

	mem_buf_vmperm_update_state(vmperm, arg->vmids, arg->perms,
			arg->nr_acl_entries);
	vmperm->flags |= MEM_BUF_WRAPPER_FLAG_LENDSHARE;
	vmperm->memparcel_hdl = arg->memparcel_hdl;

	mutex_unlock(&vmperm->lock);
	return 0;

err_retrieve_hdl:
	__mem_buf_vmperm_reclaim(vmperm);
err_assign:
err_resize:
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_lend);

/*
 * Kernel API for Sharing, Lending, Recieving or Reclaiming
 * a dma-buf from a remote Virtual Machine.
 */
int mem_buf_lend(struct dma_buf *dmabuf,
			struct mem_buf_lend_kernel_arg *arg)
{
	return mem_buf_lend_internal(dmabuf, arg, true);
}

int mem_buf_share(struct dma_buf *dmabuf,
			struct mem_buf_lend_kernel_arg *arg)
{
	return mem_buf_lend_internal(dmabuf, arg, false);
}
EXPORT_SYMBOL(mem_buf_share);

void mem_buf_retrieve_release(struct qcom_sg_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	kfree(buffer);
}

struct dma_buf *mem_buf_retrieve(struct mem_buf_retrieve_kernel_arg *arg)
{
	int ret;
	struct qcom_sg_buffer *buffer;
	struct hh_acl_desc *acl_desc;
	struct hh_sgl_desc *sgl_desc;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	struct sg_table *sgt;

	if (!(mem_buf_capability & MEM_BUF_CAP_CONSUMER))
		return ERR_PTR(-EOPNOTSUPP);

	if (arg->fd_flags & ~MEM_BUF_VALID_FD_FLAGS)
		return ERR_PTR(-EINVAL);

	if (!arg->nr_acl_entries || !arg->vmids || !arg->perms)
		return ERR_PTR(-EINVAL);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	acl_desc = mem_buf_vmid_perm_list_to_hh_acl(arg->vmids, arg->perms,
				arg->nr_acl_entries);
	if (IS_ERR(acl_desc)) {
		ret = PTR_ERR(acl_desc);
		goto err_hh_acl;
	}

	sgl_desc = mem_buf_map_mem_s2(arg->memparcel_hdl, acl_desc);
	if (IS_ERR(sgl_desc)) {
		ret = PTR_ERR(sgl_desc);
		goto err_map_s2;
	}

	ret = mem_buf_map_mem_s1(sgl_desc);
	if (ret < 0)
		goto err_map_mem_s1;

	sgt = dup_hh_sgl_desc_to_sgt(sgl_desc);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto err_dup_sgt;
	}

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->len = mem_buf_get_sgl_buf_size(sgl_desc);
	buffer->sg_table = sgt;
	buffer->free = mem_buf_retrieve_release;
	buffer->vmperm = mem_buf_vmperm_alloc_accept(sgt, arg->memparcel_hdl);

	exp_info.ops = &mem_buf_dma_buf_ops.dma_ops;
	exp_info.size = buffer->len;
	exp_info.flags = arg->fd_flags;
	exp_info.priv = buffer;

	dmabuf = mem_buf_dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf))
		goto err_export_dma_buf;

	/* sgt & qcom_sg_buffer will be freed by mem_buf_retrieve_release */
	kfree(sgl_desc);
	kfree(acl_desc);
	return dmabuf;

err_export_dma_buf:
	sg_free_table(sgt);
	kfree(sgt);
err_dup_sgt:
	mem_buf_unmap_mem_s1(sgl_desc);
err_map_mem_s1:
	kfree(sgl_desc);
	mem_buf_unmap_mem_s2(arg->memparcel_hdl);
err_map_s2:
	kfree(acl_desc);
err_hh_acl:
	kfree(buffer);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(mem_buf_retrieve);

int mem_buf_reclaim(struct dma_buf *dmabuf)
{
	struct mem_buf_vmperm *vmperm;
	int ret;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm)) {
		pr_err_ratelimited("dmabuf ops %ps are not a mem_buf_dma_buf_ops\n",
				dmabuf->ops);
		return -EINVAL;
	}

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_STATIC_VM) {
		pr_err_ratelimited("dma-buf is staticvm type!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (!(vmperm->flags & MEM_BUF_WRAPPER_FLAG_LENDSHARE)) {
		pr_err_ratelimited("dma-buf isn't lent or shared!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ACCEPT) {
		pr_err_ratelimited("dma-buf not owned by current vm!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->mapcount) {
		pr_err_ratelimited("dma-buf is pinned!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	ret = __mem_buf_vmperm_reclaim(vmperm);
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_reclaim);

bool mem_buf_dma_buf_exclusive_owner(struct dma_buf *dmabuf)
{
	struct mem_buf_vmperm *vmperm;
	bool ret = false;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (WARN_ON(IS_ERR(vmperm)))
		return false;

	mutex_lock(&vmperm->lock);
	ret = !vmperm->flags;
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_dma_buf_exclusive_owner);

int mem_buf_dma_buf_copy_vmperm(struct dma_buf *dmabuf, int **vmids,
		int **perms, int *nr_acl_entries)
{
	struct mem_buf_vmperm *vmperm;
	size_t size;
	int *vmids_copy, *perms_copy;
	int ret;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm))
		return PTR_ERR(vmperm);

	mutex_lock(&vmperm->lock);
	size = sizeof(*vmids_copy) * vmperm->nr_acl_entries;
	vmids_copy = kmemdup(vmperm->vmids, size, GFP_KERNEL);
	if (!vmids_copy) {
		ret = -ENOMEM;
		goto err_vmids;
	}

	perms_copy = kmemdup(vmperm->perms, size, GFP_KERNEL);
	if (!perms_copy) {
		ret = -ENOMEM;
		goto err_perms;
	}

	*vmids = vmids_copy;
	*perms = perms_copy;
	*nr_acl_entries = vmperm->nr_acl_entries;

	mutex_unlock(&vmperm->lock);
	return 0;

err_perms:
	kfree(vmids_copy);
err_vmids:
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_dma_buf_copy_vmperm);

