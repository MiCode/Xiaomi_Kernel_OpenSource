/*
 * drivers/gpu/ion/ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/ion.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>

#include <mach/iommu_domains.h>
#include "ion_priv.h"
#define DEBUG

/**
 * struct ion_device - the metadata of the ion device node
 * @dev:		the actual misc device
 * @buffers:	an rb tree of all the existing buffers
 * @lock:		lock protecting the buffers & heaps trees
 * @heaps:		list of all the heaps in the system
 * @user_clients:	list of all the clients created from userspace
 */
struct ion_device {
	struct miscdevice dev;
	struct rb_root buffers;
	struct mutex lock;
	struct rb_root heaps;
	long (*custom_ioctl) (struct ion_client *client, unsigned int cmd,
			      unsigned long arg);
	struct rb_root clients;
	struct dentry *debug_root;
};

/**
 * struct ion_client - a process/hw block local address space
 * @node:		node in the tree of all clients
 * @dev:		backpointer to ion device
 * @handles:		an rb tree of all the handles in this client
 * @lock:		lock protecting the tree of handles
 * @heap_mask:		mask of all supported heaps
 * @name:		used for debugging
 * @task:		used for debugging
 *
 * A client represents a list of buffers this client may access.
 * The mutex stored here is used to protect both handles tree
 * as well as the handles themselves, and should be held while modifying either.
 */
struct ion_client {
	struct rb_node node;
	struct ion_device *dev;
	struct rb_root handles;
	struct mutex lock;
	unsigned int heap_mask;
	char *name;
	struct task_struct *task;
	pid_t pid;
	struct dentry *debug_root;
};

/**
 * ion_handle - a client local reference to a buffer
 * @ref:		reference count
 * @client:		back pointer to the client the buffer resides in
 * @buffer:		pointer to the buffer
 * @node:		node in the client's handle rbtree
 * @kmap_cnt:		count of times this client has mapped to kernel
 * @dmap_cnt:		count of times this client has mapped for dma
 *
 * Modifications to node, map_cnt or mapping should be protected by the
 * lock in the client.  Other fields are never changed after initialization.
 */
struct ion_handle {
	struct kref ref;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	unsigned int iommu_map_cnt;
};

static void ion_iommu_release(struct kref *kref);

static int ion_validate_buffer_flags(struct ion_buffer *buffer,
					unsigned long flags)
{
	if (buffer->kmap_cnt || buffer->dmap_cnt || buffer->umap_cnt ||
		buffer->iommu_map_cnt) {
		if (buffer->flags != flags) {
			pr_err("%s: buffer was already mapped with flags %lx,"
				" cannot map with flags %lx\n", __func__,
				buffer->flags, flags);
			return 1;
		}

	} else {
		buffer->flags = flags;
	}
	return 0;
}

/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);
}

static void ion_iommu_add(struct ion_buffer *buffer,
			  struct ion_iommu_map *iommu)
{
	struct rb_node **p = &buffer->iommu_maps.rb_node;
	struct rb_node *parent = NULL;
	struct ion_iommu_map *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_iommu_map, node);

		if (iommu->key < entry->key) {
			p = &(*p)->rb_left;
		} else if (iommu->key > entry->key) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer %p already has mapping for domain %d"
				" and partition %d\n", __func__,
				buffer,
				iommu_map_domain(iommu),
				iommu_map_partition(iommu));
			BUG();
		}
	}

	rb_link_node(&iommu->node, parent, p);
	rb_insert_color(&iommu->node, &buffer->iommu_maps);

}

static struct ion_iommu_map *ion_iommu_lookup(struct ion_buffer *buffer,
						unsigned int domain_no,
						unsigned int partition_no)
{
	struct rb_node **p = &buffer->iommu_maps.rb_node;
	struct rb_node *parent = NULL;
	struct ion_iommu_map *entry;
	uint64_t key = domain_no;
	key = key << 32 | partition_no;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_iommu_map, node);

		if (key < entry->key)
			p = &(*p)->rb_left;
		else if (key > entry->key)
			p = &(*p)->rb_right;
		else
			return entry;
	}

	return NULL;
}

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
				     struct ion_device *dev,
				     unsigned long len,
				     unsigned long align,
				     unsigned long flags)
{
	struct ion_buffer *buffer;
	struct sg_table *table;
	int ret;

	buffer = kzalloc(sizeof(struct ion_buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	kref_init(&buffer->ref);

	ret = heap->ops->allocate(heap, buffer, len, align, flags);
	if (ret) {
		kfree(buffer);
		return ERR_PTR(ret);
	}

	buffer->dev = dev;
	buffer->size = len;

	table = buffer->heap->ops->map_dma(buffer->heap, buffer);
	if (IS_ERR_OR_NULL(table)) {
		heap->ops->free(buffer);
		kfree(buffer);
		return ERR_PTR(PTR_ERR(table));
	}
	buffer->sg_table = table;

	mutex_init(&buffer->lock);
	ion_buffer_add(dev, buffer);
	return buffer;
}

/**
 * Check for delayed IOMMU unmapping. Also unmap any outstanding
 * mappings which would otherwise have been leaked.
 */
static void ion_iommu_delayed_unmap(struct ion_buffer *buffer)
{
	struct ion_iommu_map *iommu_map;
	struct rb_node *node;
	const struct rb_root *rb = &(buffer->iommu_maps);
	unsigned long ref_count;
	unsigned int delayed_unmap;

	mutex_lock(&buffer->lock);

	while ((node = rb_first(rb)) != 0) {
		iommu_map = rb_entry(node, struct ion_iommu_map, node);
		ref_count = atomic_read(&iommu_map->ref.refcount);
		delayed_unmap = iommu_map->flags & ION_IOMMU_UNMAP_DELAYED;

		if ((delayed_unmap && ref_count > 1) || !delayed_unmap) {
			pr_err("%s: Virtual memory address leak in domain %u, partition %u\n",
				__func__, iommu_map->domain_info[DI_DOMAIN_NUM],
				iommu_map->domain_info[DI_PARTITION_NUM]);
		}
		/* set ref count to 1 to force release */
		kref_init(&iommu_map->ref);
		kref_put(&iommu_map->ref, ion_iommu_release);
	}

	mutex_unlock(&buffer->lock);
}

static void ion_buffer_destroy(struct kref *kref)
{
	struct ion_buffer *buffer = container_of(kref, struct ion_buffer, ref);
	struct ion_device *dev = buffer->dev;

	if (WARN_ON(buffer->kmap_cnt > 0))
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);

	buffer->heap->ops->unmap_dma(buffer->heap, buffer);

	ion_iommu_delayed_unmap(buffer);
	buffer->heap->ops->free(buffer);
	mutex_lock(&dev->lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->lock);
	kfree(buffer);
}

static void ion_buffer_get(struct ion_buffer *buffer)
{
	kref_get(&buffer->ref);
}

static int ion_buffer_put(struct ion_buffer *buffer)
{
	return kref_put(&buffer->ref, ion_buffer_destroy);
}

static struct ion_handle *ion_handle_create(struct ion_client *client,
				     struct ion_buffer *buffer)
{
	struct ion_handle *handle;

	handle = kzalloc(sizeof(struct ion_handle), GFP_KERNEL);
	if (!handle)
		return ERR_PTR(-ENOMEM);
	kref_init(&handle->ref);
	rb_init_node(&handle->node);
	handle->client = client;
	ion_buffer_get(buffer);
	handle->buffer = buffer;

	return handle;
}

static void ion_handle_kmap_put(struct ion_handle *);

static void ion_handle_destroy(struct kref *kref)
{
	struct ion_handle *handle = container_of(kref, struct ion_handle, ref);
	struct ion_client *client = handle->client;
	struct ion_buffer *buffer = handle->buffer;

	mutex_lock(&buffer->lock);
	while (handle->kmap_cnt)
		ion_handle_kmap_put(handle);
	mutex_unlock(&buffer->lock);

	if (!RB_EMPTY_NODE(&handle->node))
		rb_erase(&handle->node, &client->handles);

	ion_buffer_put(buffer);
	kfree(handle);
}

struct ion_buffer *ion_handle_buffer(struct ion_handle *handle)
{
	return handle->buffer;
}

static void ion_handle_get(struct ion_handle *handle)
{
	kref_get(&handle->ref);
}

static int ion_handle_put(struct ion_handle *handle)
{
	return kref_put(&handle->ref, ion_handle_destroy);
}

static struct ion_handle *ion_handle_lookup(struct ion_client *client,
					    struct ion_buffer *buffer)
{
	struct rb_node *n;

	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		if (handle->buffer == buffer)
			return handle;
	}
	return NULL;
}

static bool ion_handle_validate(struct ion_client *client, struct ion_handle *handle)
{
	struct rb_node *n = client->handles.rb_node;

	while (n) {
		struct ion_handle *handle_node = rb_entry(n, struct ion_handle,
							  node);
		if (handle < handle_node)
			n = n->rb_left;
		else if (handle > handle_node)
			n = n->rb_right;
		else
			return true;
	}
	return false;
}

static void ion_handle_add(struct ion_client *client, struct ion_handle *handle)
{
	struct rb_node **p = &client->handles.rb_node;
	struct rb_node *parent = NULL;
	struct ion_handle *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_handle, node);

		if (handle < entry)
			p = &(*p)->rb_left;
		else if (handle > entry)
			p = &(*p)->rb_right;
		else
			WARN(1, "%s: buffer already found.", __func__);
	}

	rb_link_node(&handle->node, parent, p);
	rb_insert_color(&handle->node, &client->handles);
}

struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int flags)
{
	struct rb_node *n;
	struct ion_handle *handle;
	struct ion_device *dev = client->dev;
	struct ion_buffer *buffer = NULL;
	unsigned long secure_allocation = flags & ION_SECURE;
	const unsigned int MAX_DBG_STR_LEN = 64;
	char dbg_str[MAX_DBG_STR_LEN];
	unsigned int dbg_str_idx = 0;

	dbg_str[0] = '\0';

	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	if (WARN_ON(!len))
		return ERR_PTR(-EINVAL);

	len = PAGE_ALIGN(len);

	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->heaps); n != NULL; n = rb_next(n)) {
		struct ion_heap *heap = rb_entry(n, struct ion_heap, node);
		/* if the client doesn't support this heap type */
		if (!((1 << heap->type) & client->heap_mask))
			continue;
		/* if the caller didn't specify this heap type */
		if (!((1 << heap->id) & flags))
			continue;
		/* Do not allow un-secure heap if secure is specified */
		if (secure_allocation && (heap->type != ION_HEAP_TYPE_CP))
			continue;
		buffer = ion_buffer_create(heap, dev, len, align, flags);
		if (!IS_ERR_OR_NULL(buffer))
			break;
		if (dbg_str_idx < MAX_DBG_STR_LEN) {
			unsigned int len_left = MAX_DBG_STR_LEN-dbg_str_idx-1;
			int ret_value = snprintf(&dbg_str[dbg_str_idx],
						len_left, "%s ", heap->name);
			if (ret_value >= len_left) {
				/* overflow */
				dbg_str[MAX_DBG_STR_LEN-1] = '\0';
				dbg_str_idx = MAX_DBG_STR_LEN;
			} else if (ret_value >= 0) {
				dbg_str_idx += ret_value;
			} else {
				/* error */
				dbg_str[MAX_DBG_STR_LEN-1] = '\0';
			}
		}
	}
	mutex_unlock(&dev->lock);

	if (buffer == NULL)
		return ERR_PTR(-ENODEV);

	if (IS_ERR(buffer)) {
		pr_debug("ION is unable to allocate 0x%x bytes (alignment: "
			 "0x%x) from heap(s) %sfor client %s with heap "
			 "mask 0x%x\n",
			len, align, dbg_str, client->name, client->heap_mask);
		return ERR_PTR(PTR_ERR(buffer));
	}

	handle = ion_handle_create(client, buffer);

	/*
	 * ion_buffer_create will create a buffer with a ref_cnt of 1,
	 * and ion_handle_create will take a second reference, drop one here
	 */
	ion_buffer_put(buffer);

	if (!IS_ERR(handle)) {
		mutex_lock(&client->lock);
		ion_handle_add(client, handle);
		mutex_unlock(&client->lock);
	}


	return handle;
}
EXPORT_SYMBOL(ion_alloc);

void ion_free(struct ion_client *client, struct ion_handle *handle)
{
	bool valid_handle;

	BUG_ON(client != handle->client);

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	if (!valid_handle) {
		mutex_unlock(&client->lock);
		WARN(1, "%s: invalid handle passed to free.\n", __func__);
		return;
	}
	ion_handle_put(handle);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_free);

int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len)
{
	struct ion_buffer *buffer;
	int ret;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

	buffer = handle->buffer;

	if (!buffer->heap->ops->phys) {
		pr_err("%s: ion_phys is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return -ENODEV;
	}
	mutex_unlock(&client->lock);
	ret = buffer->heap->ops->phys(buffer->heap, buffer, addr, len);
	return ret;
}
EXPORT_SYMBOL(ion_phys);

static void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer);
	if (IS_ERR_OR_NULL(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

static void *ion_handle_kmap_get(struct ion_handle *handle)
{
	struct ion_buffer *buffer = handle->buffer;
	void *vaddr;

	if (handle->kmap_cnt) {
		handle->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_buffer_kmap_get(buffer);
	if (IS_ERR_OR_NULL(vaddr))
		return vaddr;
	handle->kmap_cnt++;
	return vaddr;
}

static void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

static void ion_handle_kmap_put(struct ion_handle *handle)
{
	struct ion_buffer *buffer = handle->buffer;

	handle->kmap_cnt--;
	if (!handle->kmap_cnt)
		ion_buffer_kmap_put(buffer);
}

static struct ion_iommu_map *__ion_iommu_map(struct ion_buffer *buffer,
		int domain_num, int partition_num, unsigned long align,
		unsigned long iova_length, unsigned long flags,
		unsigned long *iova)
{
	struct ion_iommu_map *data;
	int ret;

	data = kmalloc(sizeof(*data), GFP_ATOMIC);

	if (!data)
		return ERR_PTR(-ENOMEM);

	data->buffer = buffer;
	iommu_map_domain(data) = domain_num;
	iommu_map_partition(data) = partition_num;

	ret = buffer->heap->ops->map_iommu(buffer, data,
						domain_num,
						partition_num,
						align,
						iova_length,
						flags);

	if (ret)
		goto out;

	kref_init(&data->ref);
	*iova = data->iova_addr;

	ion_iommu_add(buffer, data);

	return data;

out:
	kfree(data);
	return ERR_PTR(ret);
}

int ion_map_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, unsigned long *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags)
{
	struct ion_buffer *buffer;
	struct ion_iommu_map *iommu_map;
	int ret = 0;

	if (ION_IS_CACHED(flags)) {
		pr_err("%s: Cannot map iommu as cached.\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_kernel.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

	buffer = handle->buffer;
	mutex_lock(&buffer->lock);

	if (!handle->buffer->heap->ops->map_iommu) {
		pr_err("%s: map_iommu is not implemented by this heap.\n",
		       __func__);
		ret = -ENODEV;
		goto out;
	}

	/*
	 * If clients don't want a custom iova length, just use whatever
	 * the buffer size is
	 */
	if (!iova_length)
		iova_length = buffer->size;

	if (buffer->size > iova_length) {
		pr_debug("%s: iova length %lx is not at least buffer size"
			" %x\n", __func__, iova_length, buffer->size);
		ret = -EINVAL;
		goto out;
	}

	if (buffer->size & ~PAGE_MASK) {
		pr_debug("%s: buffer size %x is not aligned to %lx", __func__,
			buffer->size, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	if (iova_length & ~PAGE_MASK) {
		pr_debug("%s: iova_length %lx is not aligned to %lx", __func__,
			iova_length, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	iommu_map = ion_iommu_lookup(buffer, domain_num, partition_num);
	if (!iommu_map) {
		iommu_map = __ion_iommu_map(buffer, domain_num, partition_num,
					    align, iova_length, flags, iova);
		if (!IS_ERR_OR_NULL(iommu_map)) {
			iommu_map->flags = iommu_flags;

			if (iommu_map->flags & ION_IOMMU_UNMAP_DELAYED)
				kref_get(&iommu_map->ref);
		}
	} else {
		if (iommu_map->flags != iommu_flags) {
			pr_err("%s: handle %p is already mapped with iommu flags %lx, trying to map with flags %lx\n",
				__func__, handle,
				iommu_map->flags, iommu_flags);
			ret = -EINVAL;
		} else if (iommu_map->mapped_size != iova_length) {
			pr_err("%s: handle %p is already mapped with length"
					" %x, trying to map with length %lx\n",
				__func__, handle, iommu_map->mapped_size,
				iova_length);
			ret = -EINVAL;
		} else {
			kref_get(&iommu_map->ref);
			*iova = iommu_map->iova_addr;
		}
	}
	if (!ret)
		buffer->iommu_map_cnt++;
	*buffer_size = buffer->size;
out:
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return ret;
}
EXPORT_SYMBOL(ion_map_iommu);

static void ion_iommu_release(struct kref *kref)
{
	struct ion_iommu_map *map = container_of(kref, struct ion_iommu_map,
						ref);
	struct ion_buffer *buffer = map->buffer;

	rb_erase(&map->node, &buffer->iommu_maps);
	buffer->heap->ops->unmap_iommu(map);
	kfree(map);
}

void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num)
{
	struct ion_iommu_map *iommu_map;
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	buffer = handle->buffer;

	mutex_lock(&buffer->lock);

	iommu_map = ion_iommu_lookup(buffer, domain_num, partition_num);

	if (!iommu_map) {
		WARN(1, "%s: (%d,%d) was never mapped for %p\n", __func__,
				domain_num, partition_num, buffer);
		goto out;
	}

	kref_put(&iommu_map->ref, ion_iommu_release);

	buffer->iommu_map_cnt--;
out:
	mutex_unlock(&buffer->lock);

	mutex_unlock(&client->lock);

}
EXPORT_SYMBOL(ion_unmap_iommu);

void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle,
			unsigned long flags)
{
	struct ion_buffer *buffer;
	void *vaddr;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_kernel.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}

	buffer = handle->buffer;

	if (!handle->buffer->heap->ops->map_kernel) {
		pr_err("%s: map_kernel is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-ENODEV);
	}

	if (ion_validate_buffer_flags(buffer, flags)) {
		mutex_unlock(&client->lock);
		return ERR_PTR(-EEXIST);
	}

	mutex_lock(&buffer->lock);
	vaddr = ion_handle_kmap_get(handle);
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return vaddr;
}
EXPORT_SYMBOL(ion_map_kernel);

void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	ion_handle_kmap_put(handle);
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_unmap_kernel);

static int check_vaddr_bounds(unsigned long start, unsigned long end)
{
	struct mm_struct *mm = current->active_mm;
	struct vm_area_struct *vma;
	int ret = 1;

	if (end < start)
		goto out;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, start);
	if (vma && vma->vm_start < end) {
		if (start < vma->vm_start)
			goto out_up;
		if (end > vma->vm_end)
			goto out_up;
		ret = 0;
	}

out_up:
	up_read(&mm->mmap_sem);
out:
	return ret;
}

int ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *uaddr, unsigned long offset, unsigned long len,
			unsigned int cmd)
{
	struct ion_buffer *buffer;
	int ret = -EINVAL;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to do_cache_op.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);

	if (!ION_IS_CACHED(buffer->flags)) {
		ret = 0;
		goto out;
	}

	if (!handle->buffer->heap->ops->cache_op) {
		pr_err("%s: cache_op is not implemented by this heap.\n",
		       __func__);
		ret = -ENODEV;
		goto out;
	}


	ret = buffer->heap->ops->cache_op(buffer->heap, buffer, uaddr,
						offset, len, cmd);

out:
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return ret;

}
EXPORT_SYMBOL(ion_do_cache_op);

static int ion_debug_client_show(struct seq_file *s, void *unused)
{
	struct ion_client *client = s->private;
	struct rb_node *n;
	struct rb_node *n2;

	seq_printf(s, "%16.16s: %16.16s : %16.16s : %12.12s : %12.12s : %s\n",
			"heap_name", "size_in_bytes", "handle refcount",
			"buffer", "physical", "[domain,partition] - virt");

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		enum ion_heap_type type = handle->buffer->heap->type;

		seq_printf(s, "%16.16s: %16x : %16d : %12p",
				handle->buffer->heap->name,
				handle->buffer->size,
				atomic_read(&handle->ref.refcount),
				handle->buffer);

		if (type == ION_HEAP_TYPE_SYSTEM_CONTIG ||
			type == ION_HEAP_TYPE_CARVEOUT ||
			type == ION_HEAP_TYPE_CP)
			seq_printf(s, " : %12lx", handle->buffer->priv_phys);
		else
			seq_printf(s, " : %12s", "N/A");

		for (n2 = rb_first(&handle->buffer->iommu_maps); n2;
				   n2 = rb_next(n2)) {
			struct ion_iommu_map *imap =
				rb_entry(n2, struct ion_iommu_map, node);
			seq_printf(s, " : [%d,%d] - %8lx",
					imap->domain_info[DI_DOMAIN_NUM],
					imap->domain_info[DI_PARTITION_NUM],
					imap->iova_addr);
		}
		seq_printf(s, "\n");
	}
	mutex_unlock(&client->lock);

	return 0;
}

static int ion_debug_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_client_show, inode->i_private);
}

static const struct file_operations debug_client_fops = {
	.open = ion_debug_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

struct ion_client *ion_client_create(struct ion_device *dev,
				     unsigned int heap_mask,
				     const char *name)
{
	struct ion_client *client;
	struct task_struct *task;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct ion_client *entry;
	pid_t pid;
	unsigned int name_len;

	if (!name) {
		pr_err("%s: Name cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}
	name_len = strnlen(name, 64);

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	pid = task_pid_nr(current->group_leader);
	/* don't bother to store task struct for kernel threads,
	   they can't be killed anyway */
	if (current->group_leader->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}
	task_unlock(current->group_leader);

	client = kzalloc(sizeof(struct ion_client), GFP_KERNEL);
	if (!client) {
		if (task)
			put_task_struct(current->group_leader);
		return ERR_PTR(-ENOMEM);
	}

	client->dev = dev;
	client->handles = RB_ROOT;
	mutex_init(&client->lock);

	client->name = kzalloc(name_len+1, GFP_KERNEL);
	if (!client->name) {
		put_task_struct(current->group_leader);
		kfree(client);
		return ERR_PTR(-ENOMEM);
	} else {
		strlcpy(client->name, name, name_len+1);
	}

	client->heap_mask = heap_mask;
	client->task = task;
	client->pid = pid;

	mutex_lock(&dev->lock);
	p = &dev->clients.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_client, node);

		if (client < entry)
			p = &(*p)->rb_left;
		else if (client > entry)
			p = &(*p)->rb_right;
	}
	rb_link_node(&client->node, parent, p);
	rb_insert_color(&client->node, &dev->clients);


	client->debug_root = debugfs_create_file(name, 0664,
						 dev->debug_root, client,
						 &debug_client_fops);
	mutex_unlock(&dev->lock);

	return client;
}

void ion_client_destroy(struct ion_client *client)
{
	struct ion_device *dev = client->dev;
	struct rb_node *n;

	pr_debug("%s: %d\n", __func__, __LINE__);
	while ((n = rb_first(&client->handles))) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		ion_handle_destroy(&handle->ref);
	}
	mutex_lock(&dev->lock);
	if (client->task)
		put_task_struct(client->task);
	rb_erase(&client->node, &dev->clients);
	debugfs_remove_recursive(client->debug_root);
	mutex_unlock(&dev->lock);

	kfree(client->name);
	kfree(client);
}
EXPORT_SYMBOL(ion_client_destroy);

int ion_handle_get_flags(struct ion_client *client, struct ion_handle *handle,
			unsigned long *flags)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to %s.\n",
		       __func__, __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	*flags = buffer->flags;
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);

	return 0;
}
EXPORT_SYMBOL(ion_handle_get_flags);

int ion_handle_get_size(struct ion_client *client, struct ion_handle *handle,
			unsigned long *size)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to %s.\n",
		       __func__, __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	*size = buffer->size;
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);

	return 0;
}
EXPORT_SYMBOL(ion_handle_get_size);

struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	struct sg_table *table;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_dma.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	table = buffer->sg_table;
	mutex_unlock(&client->lock);
	return table;
}
EXPORT_SYMBOL(ion_sg_table);

static struct sg_table *ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct ion_buffer *buffer = dmabuf->priv;

	return buffer->sg_table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
}

static void ion_vma_open(struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = vma->vm_private_data;

	pr_debug("%s: %d\n", __func__, __LINE__);

	mutex_lock(&buffer->lock);
	buffer->umap_cnt++;
	mutex_unlock(&buffer->lock);
}

static void ion_vma_close(struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = vma->vm_private_data;

	pr_debug("%s: %d\n", __func__, __LINE__);

	mutex_lock(&buffer->lock);
	buffer->umap_cnt--;
	mutex_unlock(&buffer->lock);

	if (buffer->heap->ops->unmap_user)
		buffer->heap->ops->unmap_user(buffer->heap, buffer);
}

static struct vm_operations_struct ion_vm_ops = {
	.open = ion_vma_open,
	.close = ion_vma_close,
};

static int ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	int ret;

	if (!buffer->heap->ops->map_user) {
		pr_err("%s: this heap does not define a method for mapping "
		       "to userspace\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma);

	if (ret) {
		mutex_unlock(&buffer->lock);
		pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);
	} else {
		buffer->umap_cnt++;
		mutex_unlock(&buffer->lock);

		vma->vm_ops = &ion_vm_ops;
		/*
		 * move the buffer into the vm_private_data so we can access it
		 * from vma_open/close
		 */
		vma->vm_private_data = buffer;
	}
	return ret;
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	ion_buffer_put(buffer);
}

static void *ion_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	struct ion_buffer *buffer = dmabuf->priv;
	return buffer->vaddr + offset;
}

static void ion_dma_buf_kunmap(struct dma_buf *dmabuf, unsigned long offset,
			       void *ptr)
{
	return;
}

static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf, size_t start,
					size_t len,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	void *vaddr;

	if (!buffer->heap->ops->map_kernel) {
		pr_err("%s: map kernel is not implemented by this heap.\n",
		       __func__);
		return -ENODEV;
	}

	mutex_lock(&buffer->lock);
	vaddr = ion_buffer_kmap_get(buffer);
	mutex_unlock(&buffer->lock);
	if (IS_ERR(vaddr))
		return PTR_ERR(vaddr);
	if (!vaddr)
		return -ENOMEM;
	return 0;
}

static void ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf, size_t start,
				       size_t len,
				       enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	ion_buffer_kmap_put(buffer);
	mutex_unlock(&buffer->lock);
}

struct dma_buf_ops dma_buf_ops = {
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.mmap = ion_mmap,
	.release = ion_dma_buf_release,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
	.kmap_atomic = ion_dma_buf_kmap,
	.kunmap_atomic = ion_dma_buf_kunmap,
	.kmap = ion_dma_buf_kmap,
	.kunmap = ion_dma_buf_kunmap,
};

static int ion_share_set_flags(struct ion_client *client,
				struct ion_handle *handle,
				unsigned long flags)
{
	struct ion_buffer *buffer;
	bool valid_handle;
	unsigned long ion_flags = ION_SET_CACHE(CACHED);
	if (flags & O_DSYNC)
		ion_flags = ION_SET_CACHE(UNCACHED);

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	mutex_unlock(&client->lock);
	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to set_flags.\n", __func__);
		return -EINVAL;
	}

	buffer = handle->buffer;

	mutex_lock(&buffer->lock);
	if (ion_validate_buffer_flags(buffer, ion_flags)) {
		mutex_unlock(&buffer->lock);
		return -EEXIST;
	}
	mutex_unlock(&buffer->lock);
	return 0;
}


int ion_share_dma_buf(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	struct dma_buf *dmabuf;
	bool valid_handle;
	int fd;

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	mutex_unlock(&client->lock);
	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to share.\n", __func__);
		return -EINVAL;
	}

	buffer = handle->buffer;
	ion_buffer_get(buffer);
	dmabuf = dma_buf_export(buffer, &dma_buf_ops, buffer->size, O_RDWR);
	if (IS_ERR(dmabuf)) {
		ion_buffer_put(buffer);
		return PTR_ERR(dmabuf);
	}
	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0)
		dma_buf_put(dmabuf);

	return fd;
}
EXPORT_SYMBOL(ion_share_dma_buf);

struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	struct ion_handle *handle;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf))
		return ERR_PTR(PTR_ERR(dmabuf));
	/* if this memory came from ion */

	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not import dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return ERR_PTR(-EINVAL);
	}
	buffer = dmabuf->priv;

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR_OR_NULL(handle)) {
		ion_handle_get(handle);
		goto end;
	}
	handle = ion_handle_create(client, buffer);
	if (IS_ERR_OR_NULL(handle))
		goto end;
	ion_handle_add(client, handle);
end:
	mutex_unlock(&client->lock);
	dma_buf_put(dmabuf);
	return handle;
}
EXPORT_SYMBOL(ion_import_dma_buf);

static long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ion_client *client = filp->private_data;

	switch (cmd) {
	case ION_IOC_ALLOC:
	{
		struct ion_allocation_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		data.handle = ion_alloc(client, data.len, data.align,
					     data.flags);

		if (IS_ERR(data.handle))
			return PTR_ERR(data.handle);

		if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
			ion_free(client, data.handle);
			return -EFAULT;
		}
		break;
	}
	case ION_IOC_FREE:
	{
		struct ion_handle_data data;
		bool valid;

		if (copy_from_user(&data, (void __user *)arg,
				   sizeof(struct ion_handle_data)))
			return -EFAULT;
		mutex_lock(&client->lock);
		valid = ion_handle_validate(client, data.handle);
		mutex_unlock(&client->lock);
		if (!valid)
			return -EINVAL;
		ion_free(client, data.handle);
		break;
	}
	case ION_IOC_MAP:
	case ION_IOC_SHARE:
	{
		struct ion_fd_data data;
		int ret;
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;

		ret = ion_share_set_flags(client, data.handle, filp->f_flags);
		if (ret)
			return ret;

		data.fd = ion_share_dma_buf(client, data.handle);
		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
			return -EFAULT;
		if (data.fd < 0)
			return data.fd;
		break;
	}
	case ION_IOC_IMPORT:
	{
		struct ion_fd_data data;
		int ret = 0;
		if (copy_from_user(&data, (void __user *)arg,
				   sizeof(struct ion_fd_data)))
			return -EFAULT;
		data.handle = ion_import_dma_buf(client, data.fd);
		if (IS_ERR(data.handle))
			data.handle = NULL;
		if (copy_to_user((void __user *)arg, &data,
				 sizeof(struct ion_fd_data)))
			return -EFAULT;
		if (ret < 0)
			return ret;
		break;
	}
	case ION_IOC_CUSTOM:
	{
		struct ion_device *dev = client->dev;
		struct ion_custom_data data;

		if (!dev->custom_ioctl)
			return -ENOTTY;
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(struct ion_custom_data)))
			return -EFAULT;
		return dev->custom_ioctl(client, data.cmd, data.arg);
	}
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
	{
		struct ion_flush_data data;
		unsigned long start, end;
		struct ion_handle *handle = NULL;
		int ret;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(struct ion_flush_data)))
			return -EFAULT;

		start = (unsigned long) data.vaddr;
		end = (unsigned long) data.vaddr + data.length;

		if (check_vaddr_bounds(start, end)) {
			pr_err("%s: virtual address %p is out of bounds\n",
				__func__, data.vaddr);
			return -EINVAL;
		}

		if (!data.handle) {
			handle = ion_import_dma_buf(client, data.fd);
			if (IS_ERR(handle)) {
				pr_info("%s: Could not import handle: %d\n",
					__func__, (int)handle);
				return -EINVAL;
			}
		}

		ret = ion_do_cache_op(client,
					data.handle ? data.handle : handle,
					data.vaddr, data.offset, data.length,
					cmd);

		if (!data.handle)
			ion_free(client, handle);

		if (ret < 0)
			return ret;
		break;

	}
	case ION_IOC_GET_FLAGS:
	{
		struct ion_flag_data data;
		int ret;
		if (copy_from_user(&data, (void __user *)arg,
				   sizeof(struct ion_flag_data)))
			return -EFAULT;

		ret = ion_handle_get_flags(client, data.handle, &data.flags);
		if (ret < 0)
			return ret;
		if (copy_to_user((void __user *)arg, &data,
				 sizeof(struct ion_flag_data)))
			return -EFAULT;
		break;
	}
	default:
		return -ENOTTY;
	}
	return 0;
}

static int ion_release(struct inode *inode, struct file *file)
{
	struct ion_client *client = file->private_data;

	pr_debug("%s: %d\n", __func__, __LINE__);
	ion_client_destroy(client);
	return 0;
}

static int ion_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct ion_device *dev = container_of(miscdev, struct ion_device, dev);
	struct ion_client *client;
	char debug_name[64];

	pr_debug("%s: %d\n", __func__, __LINE__);
	snprintf(debug_name, 64, "%u", task_pid_nr(current->group_leader));
	client = ion_client_create(dev, -1, debug_name);
	if (IS_ERR_OR_NULL(client))
		return PTR_ERR(client);
	file->private_data = client;

	return 0;
}

static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.open           = ion_open,
	.release        = ion_release,
	.unlocked_ioctl = ion_ioctl,
};

static size_t ion_debug_heap_total(struct ion_client *client,
				   enum ion_heap_ids id)
{
	size_t size = 0;
	struct rb_node *n;

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n,
						     struct ion_handle,
						     node);
		if (handle->buffer->heap->id == id)
			size += handle->buffer->size;
	}
	mutex_unlock(&client->lock);
	return size;
}

/**
 * Searches through a clients handles to find if the buffer is owned
 * by this client. Used for debug output.
 * @param client pointer to candidate owner of buffer
 * @param buf pointer to buffer that we are trying to find the owner of
 * @return 1 if found, 0 otherwise
 */
static int ion_debug_find_buffer_owner(const struct ion_client *client,
				       const struct ion_buffer *buf)
{
	struct rb_node *n;

	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		const struct ion_handle *handle = rb_entry(n,
						     const struct ion_handle,
						     node);
		if (handle->buffer == buf)
			return 1;
	}
	return 0;
}

/**
 * Adds mem_map_data pointer to the tree of mem_map
 * Used for debug output.
 * @param mem_map The mem_map tree
 * @param data The new data to add to the tree
 */
static void ion_debug_mem_map_add(struct rb_root *mem_map,
				  struct mem_map_data *data)
{
	struct rb_node **p = &mem_map->rb_node;
	struct rb_node *parent = NULL;
	struct mem_map_data *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct mem_map_data, node);

		if (data->addr < entry->addr) {
			p = &(*p)->rb_left;
		} else if (data->addr > entry->addr) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: mem_map_data already found.", __func__);
			BUG();
		}
	}
	rb_link_node(&data->node, parent, p);
	rb_insert_color(&data->node, mem_map);
}

/**
 * Search for an owner of a buffer by iterating over all ION clients.
 * @param dev ion device containing pointers to all the clients.
 * @param buffer pointer to buffer we are trying to find the owner of.
 * @return name of owner.
 */
const char *ion_debug_locate_owner(const struct ion_device *dev,
					 const struct ion_buffer *buffer)
{
	struct rb_node *j;
	const char *client_name = NULL;

	for (j = rb_first(&dev->clients); j && !client_name;
			  j = rb_next(j)) {
		struct ion_client *client = rb_entry(j, struct ion_client,
						     node);
		if (ion_debug_find_buffer_owner(client, buffer))
			client_name = client->name;
	}
	return client_name;
}

/**
 * Create a mem_map of the heap.
 * @param s seq_file to log error message to.
 * @param heap The heap to create mem_map for.
 * @param mem_map The mem map to be created.
 */
void ion_debug_mem_map_create(struct seq_file *s, struct ion_heap *heap,
			      struct rb_root *mem_map)
{
	struct ion_device *dev = heap->dev;
	struct rb_node *n;

	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer =
				rb_entry(n, struct ion_buffer, node);
		if (buffer->heap->id == heap->id) {
			struct mem_map_data *data =
					kzalloc(sizeof(*data), GFP_KERNEL);
			if (!data) {
				seq_printf(s, "ERROR: out of memory. "
					   "Part of memory map will not be logged\n");
				break;
			}
			data->addr = buffer->priv_phys;
			data->addr_end = buffer->priv_phys + buffer->size-1;
			data->size = buffer->size;
			data->client_name = ion_debug_locate_owner(dev, buffer);
			ion_debug_mem_map_add(mem_map, data);
		}
	}
}

/**
 * Free the memory allocated by ion_debug_mem_map_create
 * @param mem_map The mem map to free.
 */
static void ion_debug_mem_map_destroy(struct rb_root *mem_map)
{
	if (mem_map) {
		struct rb_node *n;
		while ((n = rb_first(mem_map)) != 0) {
			struct mem_map_data *data =
					rb_entry(n, struct mem_map_data, node);
			rb_erase(&data->node, mem_map);
			kfree(data);
		}
	}
}

/**
 * Print heap debug information.
 * @param s seq_file to log message to.
 * @param heap pointer to heap that we will print debug information for.
 */
static void ion_heap_print_debug(struct seq_file *s, struct ion_heap *heap)
{
	if (heap->ops->print_debug) {
		struct rb_root mem_map = RB_ROOT;
		ion_debug_mem_map_create(s, heap, &mem_map);
		heap->ops->print_debug(heap, s, &mem_map);
		ion_debug_mem_map_destroy(&mem_map);
	}
}

static int ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_heap *heap = s->private;
	struct ion_device *dev = heap->dev;
	struct rb_node *n;

	mutex_lock(&dev->lock);
	seq_printf(s, "%16.s %16.s %16.s\n", "client", "pid", "size");

	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);
		size_t size = ion_debug_heap_total(client, heap->id);
		if (!size)
			continue;
		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			seq_printf(s, "%16.s %16u %16u\n", task_comm,
				   client->pid, size);
		} else {
			seq_printf(s, "%16.s %16u %16u\n", client->name,
				   client->pid, size);
		}
	}
	ion_heap_print_debug(s, heap);
	mutex_unlock(&dev->lock);
	return 0;
}

static int ion_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_heap_show, inode->i_private);
}

static const struct file_operations debug_heap_fops = {
	.open = ion_debug_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap)
{
	struct rb_node **p = &dev->heaps.rb_node;
	struct rb_node *parent = NULL;
	struct ion_heap *entry;

	if (!heap->ops->allocate || !heap->ops->free || !heap->ops->map_dma ||
	    !heap->ops->unmap_dma)
		pr_err("%s: can not add heap with invalid ops struct.\n",
		       __func__);

	heap->dev = dev;
	mutex_lock(&dev->lock);
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_heap, node);

		if (heap->id < entry->id) {
			p = &(*p)->rb_left;
		} else if (heap->id > entry->id ) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: can not insert multiple heaps with "
				"id %d\n", __func__, heap->id);
			goto end;
		}
	}

	rb_link_node(&heap->node, parent, p);
	rb_insert_color(&heap->node, &dev->heaps);
	debugfs_create_file(heap->name, 0664, dev->debug_root, heap,
			    &debug_heap_fops);
end:
	mutex_unlock(&dev->lock);
}

int ion_secure_heap(struct ion_device *dev, int heap_id, int version,
			void *data)
{
	struct rb_node *n;
	int ret_val = 0;

	/*
	 * traverse the list of heaps available in this system
	 * and find the heap that is specified.
	 */
	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->heaps); n != NULL; n = rb_next(n)) {
		struct ion_heap *heap = rb_entry(n, struct ion_heap, node);
		if (heap->type != ION_HEAP_TYPE_CP)
			continue;
		if (ION_HEAP(heap->id) != heap_id)
			continue;
		if (heap->ops->secure_heap)
			ret_val = heap->ops->secure_heap(heap, version, data);
		else
			ret_val = -EINVAL;
		break;
	}
	mutex_unlock(&dev->lock);
	return ret_val;
}
EXPORT_SYMBOL(ion_secure_heap);

int ion_unsecure_heap(struct ion_device *dev, int heap_id, int version,
			void *data)
{
	struct rb_node *n;
	int ret_val = 0;

	/*
	 * traverse the list of heaps available in this system
	 * and find the heap that is specified.
	 */
	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->heaps); n != NULL; n = rb_next(n)) {
		struct ion_heap *heap = rb_entry(n, struct ion_heap, node);
		if (heap->type != ION_HEAP_TYPE_CP)
			continue;
		if (ION_HEAP(heap->id) != heap_id)
			continue;
		if (heap->ops->secure_heap)
			ret_val = heap->ops->unsecure_heap(heap, version, data);
		else
			ret_val = -EINVAL;
		break;
	}
	mutex_unlock(&dev->lock);
	return ret_val;
}
EXPORT_SYMBOL(ion_unsecure_heap);

static int ion_debug_leak_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = s->private;
	struct rb_node *n;
	struct rb_node *n2;

	/* mark all buffers as 1 */
	seq_printf(s, "%16.s %16.s %16.s %16.s\n", "buffer", "heap", "size",
		"ref cnt");
	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buf = rb_entry(n, struct ion_buffer,
						     node);

		buf->marked = 1;
	}

	/* now see which buffers we can access */
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);

		mutex_lock(&client->lock);
		for (n2 = rb_first(&client->handles); n2; n2 = rb_next(n2)) {
			struct ion_handle *handle = rb_entry(n2,
						struct ion_handle, node);

			handle->buffer->marked = 0;

		}
		mutex_unlock(&client->lock);

	}

	/* And anyone still marked as a 1 means a leaked handle somewhere */
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buf = rb_entry(n, struct ion_buffer,
						     node);

		if (buf->marked == 1)
			seq_printf(s, "%16.x %16.s %16.x %16.d\n",
				(int)buf, buf->heap->name, buf->size,
				atomic_read(&buf->ref.refcount));
	}
	mutex_unlock(&dev->lock);
	return 0;
}

static int ion_debug_leak_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_leak_show, inode->i_private);
}

static const struct file_operations debug_leak_fops = {
	.open = ion_debug_leak_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};



struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg))
{
	struct ion_device *idev;
	int ret;

	idev = kzalloc(sizeof(struct ion_device), GFP_KERNEL);
	if (!idev)
		return ERR_PTR(-ENOMEM);

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = NULL;
	ret = misc_register(&idev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		return ERR_PTR(ret);
	}

	idev->debug_root = debugfs_create_dir("ion", NULL);
	if (IS_ERR_OR_NULL(idev->debug_root))
		pr_err("ion: failed to create debug files.\n");

	idev->custom_ioctl = custom_ioctl;
	idev->buffers = RB_ROOT;
	mutex_init(&idev->lock);
	idev->heaps = RB_ROOT;
	idev->clients = RB_ROOT;
	debugfs_create_file("check_leaked_fds", 0664, idev->debug_root, idev,
			    &debug_leak_fops);
	return idev;
}

void ion_device_destroy(struct ion_device *dev)
{
	misc_deregister(&dev->dev);
	/* XXX need to free the heaps and clients ? */
	kfree(dev);
}

void __init ion_reserve(struct ion_platform_data *data)
{
	int i, ret;

	for (i = 0; i < data->nr; i++) {
		if (data->heaps[i].size == 0)
			continue;
		ret = memblock_reserve(data->heaps[i].base,
				       data->heaps[i].size);
		if (ret)
			pr_err("memblock reserve of %x@%lx failed\n",
			       data->heaps[i].size,
			       data->heaps[i].base);
	}
}
