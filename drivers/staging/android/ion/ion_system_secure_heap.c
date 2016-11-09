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

	/* Protects prefetch_list */
	spinlock_t work_lock;
	bool destroy_heap;
	struct list_head prefetch_list;
	struct delayed_work prefetch_work;
};

struct prefetch_info {
	struct list_head list;
	int vmid;
	size_t size;
	bool shrink;
};

/*
 * The video client may not hold the last reference count on the
 * ion_buffer(s). Delay for a short time after the video client sends
 * the IOC_DRAIN event to increase the chance that the reference
 * count drops to zero. Time in milliseconds.
 */
#define SHRINK_DELAY 1000

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

static int ion_system_secure_heap_allocate(
					struct ion_heap *heap,
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

static void process_one_prefetch(struct ion_heap *sys_heap,
				 struct prefetch_info *info)
{
	struct ion_buffer buffer;
	struct sg_table *sg_table;
	int ret;

	buffer.heap = sys_heap;
	buffer.flags = 0;

	ret = sys_heap->ops->allocate(sys_heap, &buffer, info->size,
						PAGE_SIZE, buffer.flags);
	if (ret) {
		pr_debug("%s: Failed to prefetch 0x%zx, ret = %d\n",
			 __func__, info->size, ret);
		return;
	}

	sg_table = sys_heap->ops->map_dma(sys_heap, &buffer);
	if (IS_ERR_OR_NULL(sg_table))
		goto out;

	ret = ion_system_secure_heap_assign_sg(sg_table,
					       get_secure_vmid(info->vmid));
	if (ret)
		goto unmap;

	/* Now free it to the secure heap */
	buffer.heap = sys_heap;
	buffer.flags = info->vmid;

unmap:
	sys_heap->ops->unmap_dma(sys_heap, &buffer);
out:
	sys_heap->ops->free(&buffer);
}

static void process_one_shrink(struct ion_heap *sys_heap,
			       struct prefetch_info *info)
{
	struct ion_buffer buffer;
	size_t pool_size, size;
	int ret;

	buffer.heap = sys_heap;
	buffer.flags = info->vmid;

	pool_size = ion_system_heap_secure_page_pool_total(sys_heap,
							   info->vmid);
	size = min(pool_size, info->size);
	ret = sys_heap->ops->allocate(sys_heap, &buffer, size, PAGE_SIZE,
				      buffer.flags);
	if (ret) {
		pr_debug("%s: Failed to shrink 0x%zx, ret = %d\n",
			 __func__, info->size, ret);
		return;
	}

	buffer.private_flags = ION_PRIV_FLAG_SHRINKER_FREE;
	sys_heap->ops->free(&buffer);
}

static void ion_system_secure_heap_prefetch_work(struct work_struct *work)
{
	struct ion_system_secure_heap *secure_heap = container_of(work,
						struct ion_system_secure_heap,
						prefetch_work.work);
	struct ion_heap *sys_heap = secure_heap->sys_heap;
	struct prefetch_info *info, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&secure_heap->work_lock, flags);
	list_for_each_entry_safe(info, tmp,
				 &secure_heap->prefetch_list, list) {
		list_del(&info->list);
		spin_unlock_irqrestore(&secure_heap->work_lock, flags);

		if (info->shrink)
			process_one_shrink(sys_heap, info);
		else
			process_one_prefetch(sys_heap, info);

		kfree(info);
		spin_lock_irqsave(&secure_heap->work_lock, flags);
	}
	spin_unlock_irqrestore(&secure_heap->work_lock, flags);
}

static int alloc_prefetch_info(
			struct ion_prefetch_regions __user *user_regions,
			bool shrink, struct list_head *items)
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

	if (nr_sizes > 0x10)
		return -EINVAL;

	for (i = 0; i < nr_sizes; i++) {
		info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			return -ENOMEM;

		err = get_user(info->size, &user_sizes[i]);
		if (err)
			goto out_free;

		info->vmid = vmid;
		info->shrink = shrink;
		INIT_LIST_HEAD(&info->list);
		list_add_tail(&info->list, items);
	}
	return err;
out_free:
	kfree(info);
	return err;
}

static int __ion_system_secure_heap_resize(struct ion_heap *heap, void *ptr,
					   bool shrink)
{
	struct ion_system_secure_heap *secure_heap = container_of(heap,
						struct ion_system_secure_heap,
						heap);
	struct ion_prefetch_data *data = ptr;
	int i, ret = 0;
	struct prefetch_info *info, *tmp;
	unsigned long flags;
	LIST_HEAD(items);

	if ((int)heap->type != ION_HEAP_TYPE_SYSTEM_SECURE)
		return -EINVAL;

	if (data->nr_regions > 0x10)
		return -EINVAL;

	for (i = 0; i < data->nr_regions; i++) {
		ret = alloc_prefetch_info(&data->regions[i], shrink, &items);
		if (ret)
			goto out_free;
	}

	spin_lock_irqsave(&secure_heap->work_lock, flags);
	if (secure_heap->destroy_heap) {
		spin_unlock_irqrestore(&secure_heap->work_lock, flags);
		goto out_free;
	}
	list_splice_init(&items, &secure_heap->prefetch_list);
	schedule_delayed_work(&secure_heap->prefetch_work,
			      shrink ? msecs_to_jiffies(SHRINK_DELAY) : 0);
	spin_unlock_irqrestore(&secure_heap->work_lock, flags);

	return 0;

out_free:
	list_for_each_entry_safe(info, tmp, &items, list) {
		list_del(&info->list);
		kfree(info);
	}
	return ret;
}

int ion_system_secure_heap_prefetch(struct ion_heap *heap, void *ptr)
{
	return __ion_system_secure_heap_resize(heap, ptr, false);
}

int ion_system_secure_heap_drain(struct ion_heap *heap, void *ptr)
{
	return __ion_system_secure_heap_resize(heap, ptr, true);
}

static struct sg_table *ion_system_secure_heap_map_dma(
			struct ion_heap *heap, struct ion_buffer *buffer)
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

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_secure_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM_SECURE;
	heap->sys_heap = get_ion_heap(ION_SYSTEM_HEAP_ID);

	heap->destroy_heap = false;
	heap->work_lock = __SPIN_LOCK_UNLOCKED(heap->work_lock);
	INIT_LIST_HEAD(&heap->prefetch_list);
	INIT_DELAYED_WORK(&heap->prefetch_work,
			  ion_system_secure_heap_prefetch_work);
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

	cancel_delayed_work_sync(&secure_heap->prefetch_work);

	list_for_each_entry_safe(info, tmp, &items, list) {
		list_del(&info->list);
		kfree(info);
	}

	kfree(heap);
}
