/*
 * drivers/gpu/ion/ion_secure_cma_heap.c
 *
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/msm_ion.h>
#include <mach/iommu_domains.h>

#include <asm/cacheflush.h>

/* for ion_heap_ops structure */
#include "ion_priv.h"
#include "msm/ion_cp_common.h"

#define ION_CMA_ALLOCATE_FAILED NULL

struct ion_secure_cma_buffer_info {
	dma_addr_t phys;
	struct sg_table *table;
	bool is_cached;
};

struct ion_cma_alloc_chunk {
	void *cpu_addr;
	struct list_head entry;
	dma_addr_t handle;
	unsigned long chunk_size;
	atomic_t cnt;
};

struct ion_cma_secure_heap {
	struct device *dev;
	/*
	 * Protects against races between threads allocating memory/adding to
	 * pool at the same time. (e.g. thread 1 adds to pool, thread 2
	 * allocates thread 1's memory before thread 1 knows it needs to
	 * allocate more.
	 * Admittedly this is fairly coarse grained right now but the chance for
	 * contention on this lock is unlikely right now. This can be changed if
	 * this ever changes in the future
	 */
	struct mutex alloc_lock;
	/*
	 * protects the list of memory chunks in this pool
	 */
	struct mutex chunk_lock;
	struct ion_heap heap;
	/*
	 * Bitmap for allocation. This contains the aggregate of all chunks. */
	unsigned long *bitmap;
	/*
	 * List of all allocated chunks
	 *
	 * This is where things get 'clever'. Individual allocations from
	 * dma_alloc_coherent must be allocated and freed in one chunk.
	 * We don't just want to limit the allocations to those confined
	 * within a single chunk (if clients allocate n small chunks we would
	 * never be able to use the combined size). The bitmap allocator is
	 * used to find the contiguous region and the parts of the chunks are
	 * marked off as used. The chunks won't be freed in the shrinker until
	 * the usage is actually zero.
	 */
	struct list_head chunks;
	int npages;
	ion_phys_addr_t base;
	struct work_struct work;
	unsigned long last_alloc;
	struct shrinker shrinker;
	atomic_t total_allocated;
	atomic_t total_pool_size;
	unsigned long heap_size;
};

static void ion_secure_pool_pages(struct work_struct *work);

/*
 * Create scatter-list for the already allocated DMA buffer.
 * This function could be replace by dma_common_get_sgtable
 * as soon as it will avalaible.
 */
int ion_secure_cma_get_sgtable(struct device *dev, struct sg_table *sgt,
			dma_addr_t handle, size_t size)
{
	struct page *page = phys_to_page(handle);
	int ret;

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (unlikely(ret))
		return ret;

	sg_set_page(sgt->sgl, page, PAGE_ALIGN(size), 0);
	sg_dma_address(sgt->sgl) = handle;
	return 0;
}

static int ion_secure_cma_add_to_pool(
					struct ion_cma_secure_heap *sheap,
					unsigned long len)
{
	void *cpu_addr;
	dma_addr_t handle;
	DEFINE_DMA_ATTRS(attrs);
	int ret = 0;
	struct ion_cma_alloc_chunk *chunk;

	mutex_lock(&sheap->chunk_lock);

	chunk = kzalloc(sizeof(*chunk), GFP_KERNEL);
	if (!chunk) {
		ret = -ENOMEM;
		goto out;
	}

	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);

	cpu_addr = dma_alloc_attrs(sheap->dev, len, &handle, GFP_KERNEL,
								&attrs);

	if (!cpu_addr) {
		ret = -ENOMEM;
		goto out_free;
	}

	chunk->cpu_addr = cpu_addr;
	chunk->handle = handle;
	chunk->chunk_size = len;
	atomic_set(&chunk->cnt, 0);
	list_add(&chunk->entry, &sheap->chunks);
	atomic_add(len, &sheap->total_pool_size);
	 /* clear the bitmap to indicate this region can be allocated from */
	bitmap_clear(sheap->bitmap, (handle - sheap->base) >> PAGE_SHIFT,
				len >> PAGE_SHIFT);
	goto out;

out_free:
	kfree(chunk);
out:
	mutex_unlock(&sheap->chunk_lock);
	return ret;
}

static void ion_secure_pool_pages(struct work_struct *work)
{
	struct ion_cma_secure_heap *sheap = container_of(work,
			struct ion_cma_secure_heap, work);

	ion_secure_cma_add_to_pool(sheap, sheap->last_alloc);
}
/*
 * @s1: start of the first region
 * @l1: length of the first region
 * @s2: start of the second region
 * @l2: length of the second region
 *
 * Returns the total number of bytes that intersect.
 *
 * s1 is the region we are trying to clear so s2 may be subsumed by s1 but the
 * maximum size to clear should only ever be l1
 *
 */
static unsigned int intersect(unsigned long s1, unsigned long l1,
				unsigned long s2, unsigned long l2)
{
	unsigned long base1 = s1;
	unsigned long end1 = s1 + l1;
	unsigned long base2 = s2;
	unsigned long end2 = s2 + l2;

	/* Case 0: The regions don't overlap at all */
	if (!(base1 < end2 && base2 < end1))
		return 0;

	/* Case 1: region 2 is subsumed by region 1 */
	if (base1 <= base2 && end2 <= end1)
		return l2;

	/* case 2: region 1 is subsumed by region 2 */
	if (base2 <= base1 && end1 <= end2)
		return l1;

	/* case 3: region1 overlaps region2 on the bottom */
	if (base2 < end1 && base2 > base1)
		return end1 - base2;

	/* case 4: region 2 overlaps region1 on the bottom */
	if (base1 < end2 && base1 > base2)
		return end2 - base1;

	pr_err("Bad math! Did not detect chunks correctly! %lx %lx %lx %lx\n",
			s1, l1, s2, l2);
	BUG();
}

int ion_secure_cma_prefetch(struct ion_heap *heap, void *data)
{
	unsigned long len = (unsigned long)data;
	struct ion_cma_secure_heap *sheap =
		container_of(heap, struct ion_cma_secure_heap, heap);
	unsigned long diff;

	if ((int) heap->type != ION_HEAP_TYPE_SECURE_DMA)
		return -EINVAL;

	/*
	 * Only prefetch as much space as there is left in the pool so
	 * check against the current free size of the heap.
	 * This is slightly racy if someone else is allocating at the same
	 * time. CMA has a restricted size for the heap so worst case
	 * the prefetch doesn't work because the allocation fails.
	 */
	diff = sheap->heap_size - atomic_read(&sheap->total_pool_size);

	if (len > diff)
		len = diff;

	sheap->last_alloc = len;
	schedule_work(&sheap->work);

	return 0;
}

static void bad_math_dump(unsigned long len, int total_overlap,
				struct ion_cma_secure_heap *sheap,
				bool alloc, dma_addr_t paddr)
{
	struct list_head *entry;

	pr_err("Bad math! expected total was %lx actual was %x\n",
			len, total_overlap);
	pr_err("attempted %s address was %pa len %lx\n",
			alloc ? "allocation" : "free", &paddr, len);
	pr_err("chunks:\n");
	list_for_each(entry, &sheap->chunks) {
		struct ion_cma_alloc_chunk *chunk =
			container_of(entry,
				struct ion_cma_alloc_chunk, entry);
		pr_info("---   pa %pa len %lx\n",
			&chunk->handle, chunk->chunk_size);
	}
	BUG();

}

static int ion_secure_cma_alloc_from_pool(
					struct ion_cma_secure_heap *sheap,
					dma_addr_t *phys,
					unsigned long len)
{
	dma_addr_t paddr;
	unsigned long page_no;
	int ret = 0;
	int total_overlap = 0;
	struct list_head *entry;

	mutex_lock(&sheap->chunk_lock);

	page_no = bitmap_find_next_zero_area(sheap->bitmap,
				sheap->npages, 0, len >> PAGE_SHIFT, 0);
	if (page_no >= sheap->npages) {
		ret = -ENOMEM;
		goto out;
	}
	bitmap_set(sheap->bitmap, page_no, len >> PAGE_SHIFT);
	paddr = sheap->base + (page_no << PAGE_SHIFT);


	list_for_each(entry, &sheap->chunks) {
		struct ion_cma_alloc_chunk *chunk = container_of(entry,
					struct ion_cma_alloc_chunk, entry);
		int overlap = intersect(chunk->handle,
					chunk->chunk_size, paddr, len);

		atomic_add(overlap, &chunk->cnt);
		total_overlap += overlap;
	}

	if (total_overlap != len)
		bad_math_dump(len, total_overlap, sheap, 1, paddr);

	*phys = paddr;
out:
	mutex_unlock(&sheap->chunk_lock);
	return ret;
}

static void ion_secure_cma_free_chunk(struct ion_cma_secure_heap *sheap,
					struct ion_cma_alloc_chunk *chunk)
{
	/* This region is 'allocated' and not available to allocate from */
	bitmap_set(sheap->bitmap, (chunk->handle - sheap->base) >> PAGE_SHIFT,
			chunk->chunk_size >> PAGE_SHIFT);
	dma_free_coherent(sheap->dev, chunk->chunk_size, chunk->cpu_addr,
				chunk->handle);
	atomic_sub(chunk->chunk_size, &sheap->total_pool_size);
	list_del(&chunk->entry);
	kfree(chunk);

}

int ion_secure_cma_drain_pool(struct ion_heap *heap, void *unused)
{
	struct ion_cma_secure_heap *sheap =
		container_of(heap, struct ion_cma_secure_heap, heap);
	struct list_head *entry, *_n;

	mutex_lock(&sheap->chunk_lock);
	list_for_each_safe(entry, _n, &sheap->chunks) {
		struct ion_cma_alloc_chunk *chunk = container_of(entry,
					struct ion_cma_alloc_chunk, entry);

		if (atomic_read(&chunk->cnt) == 0)
			ion_secure_cma_free_chunk(sheap, chunk);
	}
	mutex_unlock(&sheap->chunk_lock);

	return 0;
}

static int ion_secure_cma_shrinker(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	struct ion_cma_secure_heap *sheap = container_of(shrinker,
					struct ion_cma_secure_heap, shrinker);
	int nr_to_scan = sc->nr_to_scan;
	struct list_head *entry, *_n;

	if (nr_to_scan == 0)
		return atomic_read(&sheap->total_pool_size);

	/*
	 * CMA pages can only be used for movable allocation so don't free if
	 * the allocation isn't movable
	 */
	if (!(sc->gfp_mask & __GFP_MOVABLE))
		return atomic_read(&sheap->total_pool_size);

	mutex_lock(&sheap->chunk_lock);
	list_for_each_safe(entry, _n, &sheap->chunks) {
		struct ion_cma_alloc_chunk *chunk = container_of(entry,
					struct ion_cma_alloc_chunk, entry);

		if (nr_to_scan < 0)
			break;

		if (atomic_read(&chunk->cnt) == 0) {
			nr_to_scan -= chunk->chunk_size;
			ion_secure_cma_free_chunk(sheap, chunk);
		}
	}
	mutex_unlock(&sheap->chunk_lock);

	return atomic_read(&sheap->total_pool_size);
}

static void ion_secure_cma_free_from_pool(struct ion_cma_secure_heap *sheap,
					dma_addr_t handle,
					unsigned long len)
{
	struct list_head *entry, *_n;
	int total_overlap = 0;

	mutex_lock(&sheap->chunk_lock);
	bitmap_clear(sheap->bitmap, (handle - sheap->base) >> PAGE_SHIFT,
				len >> PAGE_SHIFT);

	list_for_each_safe(entry, _n, &sheap->chunks) {
		struct ion_cma_alloc_chunk *chunk = container_of(entry,
					struct ion_cma_alloc_chunk, entry);
		int overlap = intersect(chunk->handle,
					chunk->chunk_size, handle, len);

		/*
		 * Don't actually free this from the pool list yet, let either
		 * an explicit drain call or the shrinkers take care of the
		 * pool.
		 */
		atomic_sub_return(overlap, &chunk->cnt);
		BUG_ON(atomic_read(&chunk->cnt) < 0);

		total_overlap += overlap;
	}

	BUG_ON(atomic_read(&sheap->total_pool_size) < 0);

	if (total_overlap != len)
		bad_math_dump(len, total_overlap, sheap, 0, handle);

	mutex_unlock(&sheap->chunk_lock);
}

/* ION CMA heap operations functions */
static struct ion_secure_cma_buffer_info *__ion_secure_cma_allocate(
			    struct ion_heap *heap, struct ion_buffer *buffer,
			    unsigned long len, unsigned long align,
			    unsigned long flags)
{
	struct ion_cma_secure_heap *sheap =
		container_of(heap, struct ion_cma_secure_heap, heap);
	struct ion_secure_cma_buffer_info *info;
	int ret;

	dev_dbg(sheap->dev, "Request buffer allocation len %ld\n", len);

	info = kzalloc(sizeof(struct ion_secure_cma_buffer_info), GFP_KERNEL);
	if (!info) {
		dev_err(sheap->dev, "Can't allocate buffer info\n");
		return ION_CMA_ALLOCATE_FAILED;
	}

	mutex_lock(&sheap->alloc_lock);
	ret = ion_secure_cma_alloc_from_pool(sheap, &info->phys, len);

	if (ret) {
		ret = ion_secure_cma_add_to_pool(sheap, len);
		if (ret) {
			dev_err(sheap->dev, "Fail to allocate buffer\n");
			goto err;
		}
		ret = ion_secure_cma_alloc_from_pool(sheap, &info->phys, len);
		if (ret) {
			/*
			 * We just added memory to the pool, we shouldn't be
			 * failing to get memory
			 */
			BUG();
		}
	}
	mutex_unlock(&sheap->alloc_lock);

	atomic_add(len, &sheap->total_allocated);
	info->table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!info->table) {
		dev_err(sheap->dev, "Fail to allocate sg table\n");
		goto err;
	}

	ion_secure_cma_get_sgtable(sheap->dev,
			info->table, info->phys, len);

	/* keep this for memory release */
	buffer->priv_virt = info;
	dev_dbg(sheap->dev, "Allocate buffer %p\n", buffer);
	return info;

err:
	kfree(info);
	return ION_CMA_ALLOCATE_FAILED;
}

static int ion_secure_cma_allocate(struct ion_heap *heap,
			    struct ion_buffer *buffer,
			    unsigned long len, unsigned long align,
			    unsigned long flags)
{
	unsigned long secure_allocation = flags & ION_FLAG_SECURE;
	struct ion_secure_cma_buffer_info *buf = NULL;

	if (!secure_allocation) {
		pr_err("%s: non-secure allocation disallowed from heap %s %lx\n",
			__func__, heap->name, flags);
		return -ENOMEM;
	}

	if (ION_IS_CACHED(flags)) {
		pr_err("%s: cannot allocate cached memory from secure heap %s\n",
			__func__, heap->name);
		return -ENOMEM;
	}


	buf = __ion_secure_cma_allocate(heap, buffer, len, align, flags);

	if (buf) {
		int ret;

		ret = msm_ion_secure_table(buf->table, 0, 0, true);
		if (ret) {
			/*
			 * Don't treat the secure buffer failing here as an
			 * error for backwards compatibility reasons. If
			 * the secure fails, the map will also fail so there
			 * is no security risk.
			 */
			pr_debug("%s: failed to secure buffer\n", __func__);
		}
		return 0;
	} else {
		return -ENOMEM;
	}
}


static void ion_secure_cma_free(struct ion_buffer *buffer)
{
	struct ion_cma_secure_heap *sheap =
		container_of(buffer->heap, struct ion_cma_secure_heap, heap);
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	dev_dbg(sheap->dev, "Release buffer %p\n", buffer);
	msm_ion_unsecure_table(info->table);
	atomic_sub(buffer->size, &sheap->total_allocated);
	BUG_ON(atomic_read(&sheap->total_allocated) < 0);
	/* release memory */
	ion_secure_cma_free_from_pool(sheap, info->phys, buffer->size);
	/* release sg table */
	sg_free_table(info->table);
	kfree(info->table);
	kfree(info);
}

static int ion_secure_cma_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			ion_phys_addr_t *addr, size_t *len)
{
	struct ion_cma_secure_heap *sheap =
		container_of(heap, struct ion_cma_secure_heap, heap);
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	dev_dbg(sheap->dev, "Return buffer %p physical address 0x%pa\n", buffer,
		&info->phys);

	*addr = info->phys;
	*len = buffer->size;

	return 0;
}

struct sg_table *ion_secure_cma_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	struct ion_secure_cma_buffer_info *info = buffer->priv_virt;

	return info->table;
}

void ion_secure_cma_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}

static int ion_secure_cma_mmap(struct ion_heap *mapper,
			struct ion_buffer *buffer,
			struct vm_area_struct *vma)
{
	pr_info("%s: mmaping from secure heap %s disallowed\n",
		__func__, mapper->name);
	return -EINVAL;
}

static void *ion_secure_cma_map_kernel(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
	pr_info("%s: kernel mapping from secure heap %s disallowed\n",
		__func__, heap->name);
	return NULL;
}

static void ion_secure_cma_unmap_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	return;
}

static int ion_secure_cma_print_debug(struct ion_heap *heap, struct seq_file *s,
			const struct rb_root *mem_map)
{
	struct ion_cma_secure_heap *sheap =
		container_of(heap, struct ion_cma_secure_heap, heap);

	if (mem_map) {
		struct rb_node *n;

		seq_printf(s, "\nMemory Map\n");
		seq_printf(s, "%16.s %14.s %14.s %14.s\n",
			   "client", "start address", "end address",
			   "size (hex)");

		for (n = rb_first(mem_map); n; n = rb_next(n)) {
			struct mem_map_data *data =
					rb_entry(n, struct mem_map_data, node);
			const char *client_name = "(null)";


			if (data->client_name)
				client_name = data->client_name;

			seq_printf(s, "%16.s %14pa %14pa %14lu (%lx)\n",
				   client_name, &data->addr,
				   &data->addr_end,
				   data->size, data->size);
		}
	}
	seq_printf(s, "Total allocated: %x\n",
				atomic_read(&sheap->total_allocated));
	seq_printf(s, "Total pool size: %x\n",
				atomic_read(&sheap->total_pool_size));
	return 0;
}

static struct ion_heap_ops ion_secure_cma_ops = {
	.allocate = ion_secure_cma_allocate,
	.free = ion_secure_cma_free,
	.map_dma = ion_secure_cma_heap_map_dma,
	.unmap_dma = ion_secure_cma_heap_unmap_dma,
	.phys = ion_secure_cma_phys,
	.map_user = ion_secure_cma_mmap,
	.map_kernel = ion_secure_cma_map_kernel,
	.unmap_kernel = ion_secure_cma_unmap_kernel,
	.print_debug = ion_secure_cma_print_debug,
};

struct ion_heap *ion_secure_cma_heap_create(struct ion_platform_heap *data)
{
	struct ion_cma_secure_heap *sheap;
	int map_size = BITS_TO_LONGS(data->size >> PAGE_SHIFT) * sizeof(long);

	sheap = kzalloc(sizeof(*sheap), GFP_KERNEL);
	if (!sheap)
		return ERR_PTR(-ENOMEM);

	sheap->dev = data->priv;
	mutex_init(&sheap->chunk_lock);
	mutex_init(&sheap->alloc_lock);
	sheap->heap.ops = &ion_secure_cma_ops;
	sheap->heap.type = ION_HEAP_TYPE_SECURE_DMA;
	sheap->npages = data->size >> PAGE_SHIFT;
	sheap->base = data->base;
	sheap->heap_size = data->size;
	sheap->bitmap = kmalloc(map_size, GFP_KERNEL);
	INIT_LIST_HEAD(&sheap->chunks);
	INIT_WORK(&sheap->work, ion_secure_pool_pages);
	sheap->shrinker.seeks = DEFAULT_SEEKS;
	sheap->shrinker.batch = 0;
	sheap->shrinker.shrink = ion_secure_cma_shrinker;
	register_shrinker(&sheap->shrinker);


	if (!sheap->bitmap) {
		kfree(sheap);
		return ERR_PTR(-ENOMEM);
	}

	/*
	 * we initially mark everything in the allocator as being free so that
	 * allocations can come in later
	 */
	bitmap_fill(sheap->bitmap, sheap->npages);

	return &sheap->heap;
}

void ion_secure_cma_heap_destroy(struct ion_heap *heap)
{
	struct ion_cma_secure_heap *sheap =
		container_of(heap, struct ion_cma_secure_heap, heap);

	kfree(sheap);
}
