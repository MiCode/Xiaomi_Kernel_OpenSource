/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/dma-buf.h>
#include <linux/export.h>
#include <linux/iommu.h>
#include <linux/ion.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include <mach/iommu_domains.h>

enum {
	DI_PARTITION_NUM = 0,
	DI_DOMAIN_NUM = 1,
	DI_MAX,
};

#define iommu_map_domain(__m)           ((__m)->domain_info[1])
#define iommu_map_partition(__m)        ((__m)->domain_info[0])

/**
 * struct ion_iommu_map - represents a mapping of an ion buffer to an iommu
 * @iova_addr - iommu virtual address
 * @node - rb node to exist in the buffer's tree of iommu mappings
 * @domain_info - contains the partition number and domain number
 *		domain_info[1] = domain number
 *		domain_info[0] = partition number
 * @ref - for reference counting this mapping
 * @mapped_size - size of the iova space mapped
 *		(may not be the same as the buffer size)
 * @flags - iommu domain/partition specific flags.
 *
 * Represents a mapping of one ion buffer to a particular iommu domain
 * and address range. There may exist other mappings of this buffer in
 * different domains or address ranges. All mappings will have the same
 * cacheability and security.
 */
struct ion_iommu_map {
	unsigned long iova_addr;
	struct rb_node node;
	union {
		int domain_info[DI_MAX];
		uint64_t key;
	};
	struct ion_iommu_meta *meta;
	struct kref ref;
	int mapped_size;
	unsigned long flags;
};


struct ion_iommu_meta {
	struct rb_node node;
	struct ion_handle *handle;
	struct rb_root iommu_maps;
	struct kref ref;
	struct sg_table *table;
	unsigned long size;
	struct mutex lock;
	struct dma_buf *dbuf;
};

static struct rb_root iommu_root;
DEFINE_MUTEX(msm_iommu_map_mutex);

static void ion_iommu_meta_add(struct ion_iommu_meta *meta)
{
	struct rb_root *root = &iommu_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct ion_iommu_meta *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_iommu_meta, node);

		if (meta->table < entry->table) {
			p = &(*p)->rb_left;
		} else if (meta->table > entry->table) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: handle %p already exists\n", __func__,
				entry->handle);
			BUG();
		}
	}

	rb_link_node(&meta->node, parent, p);
	rb_insert_color(&meta->node, root);
}


static struct ion_iommu_meta *ion_iommu_meta_lookup(struct sg_table *table)
{
	struct rb_root *root = &iommu_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct ion_iommu_meta *entry = NULL;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_iommu_meta, node);

		if (table < entry->table)
			p = &(*p)->rb_left;
		else if (table > entry->table)
			p = &(*p)->rb_right;
		else
			return entry;
	}

	return NULL;
}



static void ion_iommu_add(struct ion_iommu_meta *meta,
			struct ion_iommu_map *iommu)
{
	struct rb_node **p = &meta->iommu_maps.rb_node;
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
			pr_err("%s: handle %p already has mapping for domain %d and partition %d\n",
				__func__,
				meta->handle,
				iommu_map_domain(iommu),
				iommu_map_partition(iommu));
			BUG();
		}
	}

	rb_link_node(&iommu->node, parent, p);
	rb_insert_color(&iommu->node, &meta->iommu_maps);
}


static struct ion_iommu_map *ion_iommu_lookup(
					struct ion_iommu_meta *meta,
					unsigned int domain_no,
					unsigned int partition_no)
{
	struct rb_node **p = &meta->iommu_maps.rb_node;
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

static int ion_iommu_map_iommu(struct ion_iommu_meta *meta,
					struct ion_iommu_map *data,
					unsigned int domain_num,
					unsigned int partition_num,
					unsigned long align,
					unsigned long iova_length,
					unsigned long flags)
{
	struct iommu_domain *domain;
	int ret = 0;
	unsigned long extra, size;
	struct sg_table *table;
	int prot = IOMMU_WRITE | IOMMU_READ;


	size = meta->size;
	data->mapped_size = iova_length;
	extra = iova_length - size;
	table = meta->table;

	/* Use the biggest alignment to allow bigger IOMMU mappings.
	 * Use the first entry since the first entry will always be the
	 * biggest entry. To take advantage of bigger mapping sizes both the
	 * VA and PA addresses have to be aligned to the biggest size.
	 */
	if (sg_dma_len(table->sgl) > align)
		align = sg_dma_len(table->sgl);

	ret = msm_allocate_iova_address(domain_num, partition_num,
						data->mapped_size, align,
						&data->iova_addr);

	if (ret)
		goto out;

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -ENOMEM;
		goto out1;
	}

	ret = iommu_map_range(domain, data->iova_addr,
			      table->sgl,
			      size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	if (extra) {
		unsigned long extra_iova_addr = data->iova_addr + size;
		unsigned long phys_addr = sg_phys(table->sgl);
		ret = msm_iommu_map_extra(domain, extra_iova_addr, phys_addr,
					extra, SZ_4K, prot);
		if (ret)
			goto out2;
	}
	return ret;

out2:
	iommu_unmap_range(domain, data->iova_addr, size);
out1:
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				size);

out:

	return ret;
}

static void ion_iommu_heap_unmap_iommu(struct ion_iommu_map *data)
{
	unsigned int domain_num;
	unsigned int partition_num;
	struct iommu_domain *domain;

	BUG_ON(!msm_use_iommu());

	domain_num = iommu_map_domain(data);
	partition_num = iommu_map_partition(data);

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		WARN(1, "Could not get domain %d. Corruption?\n", domain_num);
		return;
	}

	iommu_unmap_range(domain, data->iova_addr, data->mapped_size);
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);

	return;
}



static struct ion_iommu_map *__ion_iommu_map(struct ion_iommu_meta *meta,
		int domain_num, int partition_num, unsigned long align,
		unsigned long iova_length, unsigned long flags,
		ion_phys_addr_t *iova)
{
	struct ion_iommu_map *data;
	int ret;

	data = kmalloc(sizeof(*data), GFP_ATOMIC);

	if (!data)
		return ERR_PTR(-ENOMEM);

	iommu_map_domain(data) = domain_num;
	iommu_map_partition(data) = partition_num;

	ret = ion_iommu_map_iommu(meta, data,
						domain_num,
						partition_num,
						align,
						iova_length,
						flags);

	if (ret)
		goto out;

	kref_init(&data->ref);
	*iova = data->iova_addr;
	data->meta = meta;

	ion_iommu_add(meta, data);

	return data;

out:
	kfree(data);
	return ERR_PTR(ret);
}

static struct ion_iommu_meta *ion_iommu_meta_create(struct ion_client *client,
						struct ion_handle *handle,
						struct sg_table *table,
						unsigned long size)
{
	struct ion_iommu_meta *meta;

	meta = kzalloc(sizeof(*meta), GFP_KERNEL);

	if (!meta)
		return ERR_PTR(-ENOMEM);

	meta->handle = handle;
	meta->table = table;
	meta->size = size;
	meta->dbuf = ion_share_dma_buf(client, handle);
	kref_init(&meta->ref);
	mutex_init(&meta->lock);
	ion_iommu_meta_add(meta);

	return meta;
}

static void ion_iommu_meta_destroy(struct kref *kref)
{
	struct ion_iommu_meta *meta = container_of(kref, struct ion_iommu_meta,
						ref);


	rb_erase(&meta->node, &iommu_root);
	dma_buf_put(meta->dbuf);
	kfree(meta);
}

static void ion_iommu_meta_put(struct ion_iommu_meta *meta)
{
	/*
	 * Need to lock here to prevent race against map/unmap
	 */
	mutex_lock(&msm_iommu_map_mutex);
	kref_put(&meta->ref, ion_iommu_meta_destroy);
	mutex_unlock(&msm_iommu_map_mutex);
}

int ion_map_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, ion_phys_addr_t *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags)
{
	struct ion_iommu_map *iommu_map;
	struct ion_iommu_meta *iommu_meta = NULL;
	struct sg_table *table;
	struct scatterlist *sg;
	int ret = 0;
	int i;
	unsigned long size = 0;

	if (IS_ERR_OR_NULL(client)) {
		pr_err("%s: client pointer is invalid\n", __func__);
		return -EINVAL;
	}
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: handle pointer is invalid\n", __func__);
		return -EINVAL;
	}

	table = ion_sg_table(client, handle);

	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	for_each_sg(table->sgl, sg, table->nents, i)
		size += sg_dma_len(sg);

	if (!msm_use_iommu()) {
		unsigned long pa = sg_dma_address(table->sgl);
		if (pa == 0)
			pa = sg_phys(table->sgl);
		*iova = pa;
		*buffer_size = size;
	}
	/*
	 * If clients don't want a custom iova length, just use whatever
	 * the buffer size is
	 */
	if (!iova_length)
		iova_length = size;

	if (size > iova_length) {
		pr_debug("%s: iova length %lx is not at least buffer size %lx\n",
			__func__, iova_length, size);
		ret = -EINVAL;
		goto out;
	}

	if (size & ~PAGE_MASK) {
		pr_debug("%s: buffer size %lx is not aligned to %lx", __func__,
			size, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	if (iova_length & ~PAGE_MASK) {
		pr_debug("%s: iova_length %lx is not aligned to %lx", __func__,
			iova_length, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&msm_iommu_map_mutex);
	iommu_meta = ion_iommu_meta_lookup(table);

	if (!iommu_meta)
		iommu_meta = ion_iommu_meta_create(client, handle, table, size);
	else
		kref_get(&iommu_meta->ref);
	BUG_ON(iommu_meta->size != size);
	mutex_unlock(&msm_iommu_map_mutex);

	mutex_lock(&iommu_meta->lock);
	iommu_map = ion_iommu_lookup(iommu_meta, domain_num, partition_num);
	if (!iommu_map) {
		iommu_map = __ion_iommu_map(iommu_meta, domain_num,
					    partition_num, align, iova_length,
					    flags, iova);
		if (!IS_ERR_OR_NULL(iommu_map)) {
			iommu_map->flags = iommu_flags;
			ret = 0;
		} else {
			ret = PTR_ERR(iommu_map);
			goto out_unlock;
		}
	} else {
		if (iommu_map->flags != iommu_flags) {
			pr_err("%s: handle %p is already mapped with iommu flags %lx, trying to map with flags %lx\n",
				__func__, handle,
				iommu_map->flags, iommu_flags);
			ret = -EINVAL;
			goto out_unlock;
		} else if (iommu_map->mapped_size != iova_length) {
			pr_err("%s: handle %p is already mapped with length %x, trying to map with length %lx\n",
				__func__, handle, iommu_map->mapped_size,
				iova_length);
			ret = -EINVAL;
			goto out_unlock;
		} else {
			kref_get(&iommu_map->ref);
			*iova = iommu_map->iova_addr;
		}
	}
	mutex_unlock(&iommu_meta->lock);
	*buffer_size = size;
	return ret;

out_unlock:
	mutex_unlock(&iommu_meta->lock);
out:

	ion_iommu_meta_put(iommu_meta);
	return ret;
}
EXPORT_SYMBOL(ion_map_iommu);


static void ion_iommu_map_release(struct kref *kref)
{
	struct ion_iommu_map *map = container_of(kref, struct ion_iommu_map,
						ref);
	struct ion_iommu_meta *meta = map->meta;

	rb_erase(&map->node, &meta->iommu_maps);
	ion_iommu_heap_unmap_iommu(map);
	kfree(map);
}

void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num)
{
	struct ion_iommu_map *iommu_map;
	struct ion_iommu_meta *meta;
	struct sg_table *table;

	if (IS_ERR_OR_NULL(client)) {
		pr_err("%s: client pointer is invalid\n", __func__);
		return;
	}
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: handle pointer is invalid\n", __func__);
		return;
	}

	table = ion_sg_table(client, handle);

	mutex_lock(&msm_iommu_map_mutex);
	meta = ion_iommu_meta_lookup(table);
	if (!meta) {
		WARN(1, "%s: (%d,%d) was never mapped for %p\n", __func__,
				domain_num, partition_num, handle);
		mutex_unlock(&msm_iommu_map_mutex);
		goto out;

	}
	mutex_unlock(&msm_iommu_map_mutex);

	mutex_lock(&meta->lock);
	iommu_map = ion_iommu_lookup(meta, domain_num, partition_num);

	if (!iommu_map) {
		WARN(1, "%s: (%d,%d) was never mapped for %p\n", __func__,
				domain_num, partition_num, handle);
		mutex_unlock(&meta->lock);
		goto out;
	}

	kref_put(&iommu_map->ref, ion_iommu_map_release);
	mutex_unlock(&meta->lock);

	ion_iommu_meta_put(meta);

out:
	return;
}
EXPORT_SYMBOL(ion_unmap_iommu);


