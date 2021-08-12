// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/file.h>
#include <linux/interval_tree.h>
#include <linux/seq_file.h>
#include <linux/sync_file.h>
#include <linux/slab.h>

#include "kgsl_device.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"

struct kgsl_memdesc_bind_range {
	struct kgsl_mem_entry *entry;
	struct interval_tree_node range;
};

static struct kgsl_memdesc_bind_range *bind_to_range(struct interval_tree_node *node)
{
	return container_of(node, struct kgsl_memdesc_bind_range, range);
}

static struct kgsl_memdesc_bind_range *bind_range_create(u64 start, u64 last,
		struct kgsl_mem_entry *entry)
{
	struct kgsl_memdesc_bind_range *range =
		kzalloc(sizeof(*range), GFP_KERNEL);

	if (!range)
		return ERR_PTR(-ENOMEM);

	range->range.start = start;
	range->range.last = last;
	range->entry = kgsl_mem_entry_get(entry);

	if (!range->entry) {
		kfree(range);
		return ERR_PTR(-EINVAL);
	}

	return range;
}

static u64 bind_range_len(struct kgsl_memdesc_bind_range *range)
{
	return (range->range.last - range->range.start) + 1;
}

void kgsl_memdesc_print_vbo_ranges(struct kgsl_mem_entry *entry,
		struct seq_file *s)
{
	struct  interval_tree_node *next;
	struct kgsl_memdesc *memdesc = &entry->memdesc;

	if (!(memdesc->flags & KGSL_MEMFLAGS_VBO))
		return;

	/*
	 * We are called in an atomic context so try to get the mutex but if we
	 * don't then skip this item
	 */
	if (!mutex_trylock(&memdesc->ranges_lock))
		return;

	next = interval_tree_iter_first(&memdesc->ranges, 0, ~0UL);
	while (next) {
		struct kgsl_memdesc_bind_range *range = bind_to_range(next);

		seq_printf(s, "%5d %5d 0x%16.16lx-0x%16.16lx\n",
			entry->id, range->entry->id, range->range.start,
			range->range.last);

		next = interval_tree_iter_next(next, 0, ~0UL);
	}

	mutex_unlock(&memdesc->ranges_lock);
}

static void kgsl_memdesc_remove_range(struct kgsl_mem_entry *target,
		u64 start, u64 last, struct kgsl_mem_entry *entry)
{
	struct  interval_tree_node *node, *next;
	struct kgsl_memdesc_bind_range *range;
	struct kgsl_memdesc *memdesc = &target->memdesc;

	mutex_lock(&memdesc->ranges_lock);

	next = interval_tree_iter_first(&memdesc->ranges, start, last);
	while (next) {
		node = next;
		range = bind_to_range(node);
		next = interval_tree_iter_next(node, start, last);

		/*
		 * If entry is null, consider it as a special request. Unbind
		 * the entire range between start and last in this case.
		 */
		if (!entry || range->entry->id == entry->id) {
			interval_tree_remove(node, &memdesc->ranges);
			trace_kgsl_mem_remove_bind_range(target,
				range->range.start, range->entry,
				bind_range_len(range));

			kgsl_mmu_unmap_range(memdesc->pagetable,
				memdesc, range->range.start, bind_range_len(range));

			kgsl_mmu_map_zero_page_to_range(memdesc->pagetable,
				memdesc, range->range.start, bind_range_len(range));

			kfree(range);
		}
	}

	mutex_unlock(&memdesc->ranges_lock);
}

static int kgsl_memdesc_add_range(struct kgsl_mem_entry *target,
		u64 start, u64 last, struct kgsl_mem_entry *entry, u64 offset)
{
	struct  interval_tree_node *node, *next;
	struct kgsl_memdesc *memdesc = &target->memdesc;
	struct kgsl_memdesc_bind_range *range =
		bind_range_create(start, last, entry);

	if (IS_ERR(range))
		return PTR_ERR(range);

	mutex_lock(&memdesc->ranges_lock);

	/*
	 * Unmap the range first. This increases the potential for a page fault
	 * but is safer in case something goes bad while updating the interval
	 * tree
	 */
	kgsl_mmu_unmap_range(memdesc->pagetable, memdesc, start,
		last - start + 1);

	next = interval_tree_iter_first(&memdesc->ranges, start, last);

	while (next) {
		struct kgsl_memdesc_bind_range *cur;

		node = next;
		cur = bind_to_range(node);
		next = interval_tree_iter_next(node, start, last);

		trace_kgsl_mem_remove_bind_range(target, cur->range.start,
			cur->entry, bind_range_len(cur));

		interval_tree_remove(node, &memdesc->ranges);

		if (start <= cur->range.start) {
			if (last >= cur->range.last) {
				kgsl_mem_entry_put(cur->entry);
				kfree(cur);
				continue;
			}
			/* Adjust the start of the mapping */
			cur->range.start = last + 1;
			/* And put it back into the tree */
			interval_tree_insert(node, &memdesc->ranges);

			trace_kgsl_mem_add_bind_range(target,
				cur->range.start, cur->entry, bind_range_len(cur));
		} else {
			if (last < cur->range.last) {
				struct kgsl_memdesc_bind_range *temp;

				/*
				 * The range is split into two so make a new
				 * entry for the far side
				 */
				temp = bind_range_create(last + 1, cur->range.last,
					cur->entry);
				/* FIXME: Uhoh, this would be bad */
				BUG_ON(IS_ERR(temp));

				interval_tree_insert(&temp->range,
					&memdesc->ranges);

				trace_kgsl_mem_add_bind_range(target,
					temp->range.start,
					temp->entry, bind_range_len(temp));
			}

			cur->range.last = start - 1;
			interval_tree_insert(node, &memdesc->ranges);

			trace_kgsl_mem_add_bind_range(target, cur->range.start,
				cur->entry, bind_range_len(cur));
		}
	}

	/* Add the new range */
	interval_tree_insert(&range->range, &memdesc->ranges);

	trace_kgsl_mem_add_bind_range(target, range->range.start,
		range->entry, bind_range_len(range));
	mutex_unlock(&memdesc->ranges_lock);

	return kgsl_mmu_map_child(memdesc->pagetable, memdesc, start,
			&entry->memdesc, offset, last - start + 1);
}

static void kgsl_sharedmem_vbo_put_gpuaddr(struct kgsl_memdesc *memdesc)
{
	struct interval_tree_node *node, *next;
	struct kgsl_memdesc_bind_range *range;

	/* Unmap the entire pagetable region */
	kgsl_mmu_unmap_range(memdesc->pagetable, memdesc,
		0, memdesc->size);

	/* Put back the GPU address */
	kgsl_mmu_put_gpuaddr(memdesc->pagetable, memdesc);

	memdesc->gpuaddr = 0;
	memdesc->pagetable = NULL;

	/*
	 * FIXME: do we have a use after free potential here?  We might need to
	 * lock this and set a "do not update" bit
	 */

	/* Now delete each range and release the mem entries */
	next = interval_tree_iter_first(&memdesc->ranges, 0, ~0UL);

	while (next) {
		node = next;
		range = bind_to_range(node);
		next = interval_tree_iter_next(node, 0, ~0UL);

		interval_tree_remove(node, &memdesc->ranges);
		kgsl_mem_entry_put(range->entry);
		kfree(range);
	}
}

static struct kgsl_memdesc_ops kgsl_vbo_ops = {
	.put_gpuaddr = kgsl_sharedmem_vbo_put_gpuaddr,
};

int kgsl_sharedmem_allocate_vbo(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags)
{
	size = PAGE_ALIGN(size);

	/* Make sure that VBOs are supported by the MMU */
	if (WARN_ON_ONCE(!kgsl_mmu_has_feature(device,
		KGSL_MMU_SUPPORT_VBO)))
		return -EOPNOTSUPP;

	kgsl_memdesc_init(device, memdesc, flags);
	memdesc->priv = 0;

	memdesc->ops = &kgsl_vbo_ops;
	memdesc->size = size;

	/* Set up the interval tree and lock */
	memdesc->ranges = RB_ROOT_CACHED;
	mutex_init(&memdesc->ranges_lock);

	return 0;
}

static bool kgsl_memdesc_check_range(struct kgsl_memdesc *memdesc,
		u64 offset, u64 length)
{
	return ((offset < memdesc->size) &&
		(offset + length > offset) &&
		(offset + length) <= memdesc->size);
}

static void kgsl_sharedmem_free_bind_op(struct kgsl_sharedmem_bind_op *op)
{
	int i;

	if (IS_ERR_OR_NULL(op))
		return;

	for (i = 0; i < op->nr_ops; i++)
		kgsl_mem_entry_put(op->ops[i].entry);

	kgsl_mem_entry_put(op->target);

	kvfree(op->ops);
	kfree(op);
}

struct kgsl_sharedmem_bind_op *
kgsl_sharedmem_create_bind_op(struct kgsl_process_private *private,
		u32 target_id, void __user *ranges, u32 ranges_nents,
		u64 ranges_size)
{
	struct kgsl_sharedmem_bind_op *op;
	struct kgsl_mem_entry *target;
	int ret, i;

	/* There must be at least one defined operation */
	if (!ranges_nents)
		return ERR_PTR(-EINVAL);

	/* Find the target memory entry */
	target = kgsl_sharedmem_find_id(private, target_id);
	if (!target)
		return ERR_PTR(-ENOENT);

	if (!(target->memdesc.flags & KGSL_MEMFLAGS_VBO)) {
		kgsl_mem_entry_put(target);
		return ERR_PTR(-EINVAL);
	}

	/* Make a container for the bind operations */
	op = kzalloc(sizeof(*op), GFP_KERNEL);
	if (!op) {
		kgsl_mem_entry_put(target);
		return ERR_PTR(-ENOMEM);
	}

	/*
	 * Make an array for the individual operations.  Use __GFP_NOWARN and
	 * __GFP_NORETRY to make sure a very large request quietly fails
	 */
	op->ops = kvcalloc(ranges_nents, sizeof(*op->ops),
		GFP_KERNEL | __GFP_NOWARN | __GFP_NORETRY);
	if (!op->ops) {
		kfree(op);
		kgsl_mem_entry_put(target);
		return ERR_PTR(-ENOMEM);
	}

	op->nr_ops = ranges_nents;
	op->target = target;

	for (i = 0; i < ranges_nents; i++) {
		struct kgsl_gpumem_bind_range range;
		struct kgsl_mem_entry *entry;
		u32 size;

		size = min_t(u32, sizeof(range), ranges_size);

		ret = -EINVAL;

		if (copy_from_user(&range, ranges, size)) {
			ret = -EFAULT;
			goto err;
		}

		/* The offset must be page aligned */
		if (!PAGE_ALIGNED(range.target_offset))
			goto err;

		/* The length of the operation must be aligned and non zero */
		if (!range.length || !PAGE_ALIGNED(range.length))
			goto err;

		/* Make sure the range fits in the target */
		if (!kgsl_memdesc_check_range(&target->memdesc,
			range.target_offset, range.length))
			goto err;

		/*
		 * Special case: Consider child id 0 as a special request incase of
		 * unbind. This helps to unbind the specified range (could span multiple
		 * child buffers) without supplying backing physical buffer information.
		 */
		if (range.child_id == 0 && range.op == KGSL_GPUMEM_RANGE_OP_UNBIND) {
			op->ops[i].entry = NULL;
			op->ops[i].start = range.target_offset;
			op->ops[i].last = range.target_offset + range.length - 1;
			/* Child offset doesn't matter for unbind. set it to 0 */
			op->ops[i].child_offset = 0;
			op->ops[i].op = range.op;

			ranges += ranges_size;
			continue;
		}

		/* Get the child object */
		op->ops[i].entry = kgsl_sharedmem_find_id(private,
			range.child_id);
		entry = op->ops[i].entry;
		if (!entry) {
			ret = -ENOENT;
			goto err;
		}

		/* Make sure the child is not a VBO */
		if ((entry->memdesc.flags & KGSL_MEMFLAGS_VBO)) {
			ret = -EINVAL;
			goto err;
		}

		/*
		 * Make sure that only secure children are mapped in secure VBOs
		 * and vice versa
		 */
		if ((target->memdesc.flags & KGSL_MEMFLAGS_SECURE) !=
		    (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE)) {
			ret = -EPERM;
			goto err;
		}

		/* Make sure the range operation is valid */
		if (range.op != KGSL_GPUMEM_RANGE_OP_BIND &&
			range.op != KGSL_GPUMEM_RANGE_OP_UNBIND)
			goto err;

		if (range.op == KGSL_GPUMEM_RANGE_OP_BIND) {
			if (!PAGE_ALIGNED(range.child_offset))
				goto err;

			/* Make sure the range fits in the child */
			if (!kgsl_memdesc_check_range(&entry->memdesc,
				range.child_offset, range.length))
				goto err;
		} else {
			/* For unop operations the child offset must be 0 */
			if (range.child_offset)
				goto err;
		}

		op->ops[i].entry = entry;
		op->ops[i].start = range.target_offset;
		op->ops[i].last = range.target_offset + range.length - 1;
		op->ops[i].child_offset = range.child_offset;
		op->ops[i].op = range.op;

		ranges += ranges_size;
	}

	kref_init(&op->ref);

	return op;

err:
	kgsl_sharedmem_free_bind_op(op);
	return ERR_PTR(ret);
}

void kgsl_sharedmem_bind_range_destroy(struct kref *kref)
{
	struct kgsl_sharedmem_bind_op *op = container_of(kref,
		struct kgsl_sharedmem_bind_op, ref);

	kgsl_sharedmem_free_bind_op(op);
}

static void kgsl_sharedmem_bind_worker(struct work_struct *work)
{
	struct kgsl_sharedmem_bind_op *op = container_of(work,
		struct kgsl_sharedmem_bind_op, work);
	int i;

	for (i = 0; i < op->nr_ops; i++) {
		if (op->ops[i].op == KGSL_GPUMEM_RANGE_OP_BIND)
			kgsl_memdesc_add_range(op->target,
				op->ops[i].start,
				op->ops[i].last,
				op->ops[i].entry,
				op->ops[i].child_offset);
		else
			kgsl_memdesc_remove_range(op->target,
				op->ops[i].start,
				op->ops[i].last,
				op->ops[i].entry);

		/* Release the reference on the child entry */
		kgsl_mem_entry_put(op->ops[i].entry);
		op->ops[i].entry = NULL;
	}

	/* Release the reference on the target entry */
	kgsl_mem_entry_put(op->target);
	op->target = NULL;

	if (op->callback)
		op->callback(op);

	kref_put(&op->ref, kgsl_sharedmem_bind_range_destroy);
}

void kgsl_sharedmem_bind_ranges(struct kgsl_sharedmem_bind_op *op)
{
	/* Take a reference to the operation while it is scheduled */
	kref_get(&op->ref);

	INIT_WORK(&op->work, kgsl_sharedmem_bind_worker);
	schedule_work(&op->work);
}

struct kgsl_sharedmem_bind_fence {
	struct dma_fence base;
	spinlock_t lock;
	int fd;
	struct kgsl_sharedmem_bind_op *op;
};

static const char *bind_fence_get_driver_name(struct dma_fence *fence)
{
	return "kgsl_sharedmem_bind";
}

static const char *bind_fence_get_timeline_name(struct dma_fence *fence)
{
	return "(unbound)";
}

static void bind_fence_release(struct dma_fence *fence)
{
	struct kgsl_sharedmem_bind_fence *bind_fence = container_of(fence,
		struct kgsl_sharedmem_bind_fence, base);

	kgsl_sharedmem_put_bind_op(bind_fence->op);
	kfree(bind_fence);
}

static void
kgsl_sharedmem_bind_fence_callback(struct kgsl_sharedmem_bind_op *op)
{
	struct kgsl_sharedmem_bind_fence *bind_fence = op->data;

	dma_fence_signal(&bind_fence->base);
	dma_fence_put(&bind_fence->base);
}

static const struct dma_fence_ops kgsl_sharedmem_bind_fence_ops = {
	.get_driver_name = bind_fence_get_driver_name,
	.get_timeline_name = bind_fence_get_timeline_name,
	.release = bind_fence_release,
};

static struct kgsl_sharedmem_bind_fence *
kgsl_sharedmem_bind_fence(struct kgsl_sharedmem_bind_op *op)
{
	struct kgsl_sharedmem_bind_fence *fence;
	struct sync_file *sync_file;
	int fd;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&fence->lock);

	dma_fence_init(&fence->base, &kgsl_sharedmem_bind_fence_ops,
		&fence->lock, dma_fence_context_alloc(1), 0);

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		kfree(fence);
		return ERR_PTR(fd);
	}

	sync_file = sync_file_create(&fence->base);
	if (!sync_file) {
		put_unused_fd(fd);
		kfree(fence);
		return ERR_PTR(-ENOMEM);
	}

	fd_install(fd, sync_file->file);

	fence->fd = fd;
	fence->op = op;

	return fence;
}

static void
kgsl_sharedmem_bind_async_callback(struct kgsl_sharedmem_bind_op *op)
{
	struct completion *comp = op->data;

	complete(comp);
}

long kgsl_ioctl_gpumem_bind_ranges(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	DECLARE_COMPLETION_ONSTACK(sync);
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpumem_bind_ranges *param = data;
	struct kgsl_sharedmem_bind_op *op;
	int ret;

	/* If ranges_size isn't set, return the expected size to the user */
	if (!param->ranges_size) {
		param->ranges_size = sizeof(struct kgsl_gpumem_bind_range);
		return 0;
	}

	/* FENCE_OUT only makes sense with ASYNC */
	if ((param->flags & KGSL_GPUMEM_BIND_FENCE_OUT) &&
	    !(param->flags & KGSL_GPUMEM_BIND_ASYNC))
		return -EINVAL;

	op = kgsl_sharedmem_create_bind_op(private, param->id,
		u64_to_user_ptr(param->ranges), param->ranges_nents,
		param->ranges_size);
	if (IS_ERR(op))
		return PTR_ERR(op);

	if (param->flags & KGSL_GPUMEM_BIND_ASYNC) {
		struct kgsl_sharedmem_bind_fence *fence;

		if (param->flags & KGSL_GPUMEM_BIND_FENCE_OUT) {
			fence = kgsl_sharedmem_bind_fence(op);

			if (IS_ERR(fence)) {
				kgsl_sharedmem_put_bind_op(op);
				return PTR_ERR(fence);
			}

			op->data = fence;
			op->callback = kgsl_sharedmem_bind_fence_callback;
			param->fence_id = fence->fd;
		}

		kgsl_sharedmem_bind_ranges(op);

		if (!(param->flags & KGSL_GPUMEM_BIND_FENCE_OUT))
			kgsl_sharedmem_put_bind_op(op);

		return 0;
	}

	/* For synchronous operations add a completion to wait on */
	op->callback = kgsl_sharedmem_bind_async_callback;
	op->data = &sync;

	init_completion(&sync);

	/*
	 * Schedule the work. All the resources will be released after
	 * the bind operation is done
	 */
	kgsl_sharedmem_bind_ranges(op);

	ret = wait_for_completion_interruptible(&sync);
	kgsl_sharedmem_put_bind_op(op);

	return ret;
}
