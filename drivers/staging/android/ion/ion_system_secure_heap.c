/*
 *
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/msm_ion.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include "ion.h"
#include "ion_priv.h"

struct ion_system_secure_heap {
	struct ion_heap *sys_heap;
	struct ion_heap heap;

	spinlock_t work_lock;
	bool destroy_heap;
	struct list_head prefetch_list;
	struct work_struct prefetch_work;
};

struct prefetch_info {
	struct list_head list;
	int vmid;
	size_t size;
};

static bool is_cp_flag_present(unsigned long flags)
{
	return flags && (ION_FLAG_CP_TOUCH ||
			ION_FLAG_CP_BITSTREAM ||
			ION_FLAG_CP_PIXEL ||
			ION_FLAG_CP_NON_PIXEL ||
			ION_FLAG_CP_CAMERA);
}

int ion_system_secure_heap_unassign_sg(struct sg_table *sgt, int source_vmid)
{
	u32 dest_vmid = VMID_HLOS;
	u32 dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	struct scatterlist *sg;
	int ret, i;

	ret = hyp_assign_table(sgt, &source_vmid, 1,
				&dest_vmid, &dest_perms, 1);
	if (ret) {
		pr_err("%s: Not freeing memory since assign call failed. VMID %d\n",
						__func__, source_vmid);
		return -ENXIO;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		ClearPagePrivate(sg_page(sg));
	return 0;
}

int ion_system_secure_heap_assign_sg(struct sg_table *sgt, int dest_vmid)
{
	u32 source_vmid = VMID_HLOS;
	u32 dest_perms = PERM_READ | PERM_WRITE;
	struct scatterlist *sg;
	int ret, i;

	ret = hyp_assign_table(sgt, &source_vmid, 1,
				&dest_vmid, &dest_perms, 1);
	if (ret) {
		pr_err("%s: Assign call failed. VMID %d\n",
						__func__, dest_vmid);
		return -EINVAL;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		SetPagePrivate(sg_page(sg));
	return 0;
}

static void ion_system_secure_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);
	buffer->heap = secure_heap->sys_heap;
	secure_heap->sys_heap->ops->free(buffer);
}

static int ion_system_secure_heap_allocate(struct ion_heap *heap,
					struct ion_buffer *buffer,
					unsigned long size, unsigned long align,
					unsigned long flags)
{
	int ret = 0;
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);

	if (!ion_heap_is_system_secure_heap_type(secure_heap->heap.type) ||
		!is_cp_flag_present(flags)) {
		pr_info("%s: Incorrect heap type or incorrect flags\n",
								__func__);
		return -EINVAL;
	}

	ret = secure_heap->sys_heap->ops->allocate(secure_heap->sys_heap,
						buffer, size, align, flags);
	if (ret) {
		pr_info("%s: Failed to get allocation for %s, ret = %d\n",
			__func__, heap->name, ret);
		return ret;
	}
	return ret;
}

static void ion_system_secure_heap_prefetch_work(struct work_struct *work)
{
	struct ion_system_secure_heap *secure_heap = container_of(work,
						struct ion_system_secure_heap,
						prefetch_work);
	struct ion_heap *sys_heap = secure_heap->sys_heap;
	struct prefetch_info *info, *tmp;
	unsigned long flags, size;
	struct ion_buffer *buffer;
	int ret;
	int vmid_flags;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return;

	spin_lock_irqsave(&secure_heap->work_lock, flags);
	list_for_each_entry_safe(info, tmp,
				&secure_heap->prefetch_list, list) {
		list_del(&info->list);
		spin_unlock_irqrestore(&secure_heap->work_lock, flags);
		size = info->size;
		vmid_flags = info->vmid;
		kfree(info);

		/* buffer->heap used by free() */
		buffer->heap = &secure_heap->heap;
		buffer->flags = ION_FLAG_POOL_PREFETCH;
		buffer->flags |= vmid_flags;
		ret = sys_heap->ops->allocate(sys_heap, buffer, size,
						PAGE_SIZE, 0);
		if (ret) {
			pr_debug("%s: Failed to get %zx allocation for %s, ret = %d\n",
				__func__, info->size, secure_heap->heap.name,
				ret);
			spin_lock_irqsave(&secure_heap->work_lock, flags);
			continue;
		}

		ion_system_secure_heap_free(buffer);
		spin_lock_irqsave(&secure_heap->work_lock, flags);
	}
	spin_unlock_irqrestore(&secure_heap->work_lock, flags);
	kfree(buffer);
}

static int alloc_prefetch_info(
			struct ion_prefetch_regions __user *user_regions,
			struct list_head *items)
{
	struct prefetch_info *info;
	size_t __user *user_sizes;
	int err;
	unsigned int nr_sizes, vmid, i;

	err = get_user(nr_sizes, &user_regions->nr_sizes);
	err |= get_user(user_sizes, &user_regions->sizes);
	err |= get_user(vmid, &user_regions->vmid);
	if (err)
		return -EFAULT;

	if (!is_secure_vmid_valid(get_secure_vmid(vmid)))
		return -EINVAL;

	for (i = 0; i < nr_sizes; i++) {
		info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			return -ENOMEM;

		err = get_user(info->size, &user_sizes[i]);
		if (err)
			goto out_free;

		info->vmid = vmid;
		INIT_LIST_HEAD(&info->list);
		list_add_tail(&info->list, items);
	}
	return err;
out_free:
	kfree(info);
	return err;
}

int ion_system_secure_heap_prefetch(struct ion_heap *heap, void *ptr)
{
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);
	struct ion_prefetch_data *data = ptr;
	int i, ret = 0;
	struct prefetch_info *info, *tmp;
	unsigned long flags;
	LIST_HEAD(items);

	if ((int) heap->type != ION_HEAP_TYPE_SYSTEM_SECURE)
		return -EINVAL;

	for (i = 0; i < data->nr_regions; i++) {
		ret = alloc_prefetch_info(&data->regions[i], &items);
		if (ret)
			goto out_free;
	}

	spin_lock_irqsave(&secure_heap->work_lock, flags);
	if (secure_heap->destroy_heap) {
		spin_unlock_irqrestore(&secure_heap->work_lock, flags);
		goto out_free;
	}
	list_splice_init(&items, &secure_heap->prefetch_list);
	schedule_work(&secure_heap->prefetch_work);
	spin_unlock_irqrestore(&secure_heap->work_lock, flags);

	return 0;

out_free:
	list_for_each_entry_safe(info, tmp, &items, list) {
		list_del(&info->list);
		kfree(info);
	}
	return ret;
}

static struct sg_table *ion_system_secure_heap_map_dma(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);

	return secure_heap->sys_heap->ops->map_dma(secure_heap->sys_heap,
							buffer);
}

static void ion_system_secure_heap_unmap_dma(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);

	secure_heap->sys_heap->ops->unmap_dma(secure_heap->sys_heap,
							buffer);
}

static void *ion_system_secure_heap_map_kernel(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
	pr_info("%s: Kernel mapping from secure heap %s disallowed\n",
		__func__, heap->name);
	return ERR_PTR(-EINVAL);
}

static void ion_system_secure_heap_unmap_kernel(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
}

static int ion_system_secure_heap_map_user(struct ion_heap *mapper,
					struct ion_buffer *buffer,
					struct vm_area_struct *vma)
{
	pr_info("%s: Mapping from secure heap %s disallowed\n",
		__func__, mapper->name);
	return -EINVAL;
}

static int ion_system_secure_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
						int nr_to_scan)
{
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);

	return secure_heap->sys_heap->ops->shrink(secure_heap->sys_heap,
						gfp_mask, nr_to_scan);
}

static struct ion_heap_ops system_secure_heap_ops = {
	.allocate = ion_system_secure_heap_allocate,
	.free = ion_system_secure_heap_free,
	.map_dma = ion_system_secure_heap_map_dma,
	.unmap_dma = ion_system_secure_heap_unmap_dma,
	.map_kernel = ion_system_secure_heap_map_kernel,
	.unmap_kernel = ion_system_secure_heap_unmap_kernel,
	.map_user = ion_system_secure_heap_map_user,
	.shrink = ion_system_secure_heap_shrink,
};

struct ion_heap *ion_system_secure_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_secure_heap *heap;

	heap = kzalloc(sizeof(struct ion_system_secure_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_secure_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM_SECURE;
	heap->sys_heap = get_ion_heap(ION_SYSTEM_HEAP_ID);

	heap->destroy_heap = false;
	heap->work_lock = __SPIN_LOCK_UNLOCKED(heap->work_lock);
	INIT_LIST_HEAD(&heap->prefetch_list);
	INIT_WORK(&heap->prefetch_work, ion_system_secure_heap_prefetch_work);
	return &heap->heap;
}

void ion_system_secure_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);
	unsigned long flags;
	LIST_HEAD(items);
	struct prefetch_info *info, *tmp;

	/* Stop any pending/future work */
	spin_lock_irqsave(&secure_heap->work_lock, flags);
	secure_heap->destroy_heap = true;
	list_splice_init(&secure_heap->prefetch_list, &items);
	spin_unlock_irqrestore(&secure_heap->work_lock, flags);

	cancel_work_sync(&secure_heap->prefetch_work);

	list_for_each_entry_safe(info, tmp, &items, list) {
		list_del(&info->list);
		kfree(info);
	}

	kfree(heap);
}
