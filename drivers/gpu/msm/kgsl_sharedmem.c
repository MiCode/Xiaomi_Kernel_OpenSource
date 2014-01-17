/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/memory_alloc.h>
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/kmemleak.h>
#include <linux/highmem.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"
#include "kgsl_device.h"

/* An attribute for showing per-process memory statistics */
struct kgsl_mem_entry_attribute {
	struct attribute attr;
	int memtype;
	ssize_t (*show)(struct kgsl_process_private *priv,
		int type, char *buf);
};

#define to_mem_entry_attr(a) \
container_of(a, struct kgsl_mem_entry_attribute, attr)

#define __MEM_ENTRY_ATTR(_type, _name, _show) \
{ \
	.attr = { .name = __stringify(_name), .mode = 0444 }, \
	.memtype = _type, \
	.show = _show, \
}

/*
 * A structure to hold the attributes for a particular memory type.
 * For each memory type in each process we store the current and maximum
 * memory usage and display the counts in sysfs.  This structure and
 * the following macro allow us to simplify the definition for those
 * adding new memory types
 */

struct mem_entry_stats {
	int memtype;
	struct kgsl_mem_entry_attribute attr;
	struct kgsl_mem_entry_attribute max_attr;
};


#define MEM_ENTRY_STAT(_type, _name) \
{ \
	.memtype = _type, \
	.attr = __MEM_ENTRY_ATTR(_type, _name, mem_entry_show), \
	.max_attr = __MEM_ENTRY_ATTR(_type, _name##_max, \
		mem_entry_max_show), \
}

/**
 * Given a kobj, find the process structure attached to it
 */

static struct kgsl_process_private *
_get_priv_from_kobj(struct kobject *kobj)
{
	struct kgsl_process_private *private;
	unsigned long name;

	if (!kobj)
		return NULL;

	if (sscanf(kobj->name, "%ld", &name) != 1)
		return NULL;

	list_for_each_entry(private, &kgsl_driver.process_list, list) {
		if (private->pid == name)
			return private;
	}

	return NULL;
}

/**
 * Show the current amount of memory allocated for the given memtype
 */

static ssize_t
mem_entry_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", priv->stats[type].cur);
}

/**
 * Show the maximum memory allocated for the given memtype through the life of
 * the process
 */

static ssize_t
mem_entry_max_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", priv->stats[type].max);
}


static void mem_entry_sysfs_release(struct kobject *kobj)
{
}

static ssize_t mem_entry_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_mem_entry_attribute *pattr = to_mem_entry_attr(attr);
	struct kgsl_process_private *priv;
	ssize_t ret;

	mutex_lock(&kgsl_driver.process_mutex);
	priv = _get_priv_from_kobj(kobj);

	if (priv && pattr->show)
		ret = pattr->show(priv, pattr->memtype, buf);
	else
		ret = -EIO;

	mutex_unlock(&kgsl_driver.process_mutex);
	return ret;
}

static const struct sysfs_ops mem_entry_sysfs_ops = {
	.show = mem_entry_sysfs_show,
};

static struct kobj_type ktype_mem_entry = {
	.sysfs_ops = &mem_entry_sysfs_ops,
	.default_attrs = NULL,
	.release = mem_entry_sysfs_release
};

static struct mem_entry_stats mem_stats[] = {
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_KERNEL, kernel),
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_PMEM, pmem),
#ifdef CONFIG_ASHMEM
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_ASHMEM, ashmem),
#endif
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_USER, user),
#ifdef CONFIG_ION
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_ION, ion),
#endif
};

void
kgsl_process_uninit_sysfs(struct kgsl_process_private *private)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		sysfs_remove_file(&private->kobj, &mem_stats[i].attr.attr);
		sysfs_remove_file(&private->kobj,
			&mem_stats[i].max_attr.attr);
	}

	kobject_put(&private->kobj);
}

/**
 * kgsl_process_init_sysfs() - Initialize and create sysfs files for a process
 *
 * @device: Pointer to kgsl device struct
 * @private: Pointer to the structure for the process
 *
 * @returns: 0 on success, error code otherwise
 *
 * kgsl_process_init_sysfs() is called at the time of creating the
 * process struct when a process opens the kgsl device for the first time.
 * This function creates the sysfs files for the process.
 */
int
kgsl_process_init_sysfs(struct kgsl_device *device,
		struct kgsl_process_private *private)
{
	unsigned char name[16];
	int i, ret = 0;

	snprintf(name, sizeof(name), "%d", private->pid);

	ret = kobject_init_and_add(&private->kobj, &ktype_mem_entry,
		kgsl_driver.prockobj, name);

	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		/* We need to check the value of sysfs_create_file, but we
		 * don't really care if it passed or not */

		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].attr.attr);
		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].max_attr.attr);
	}
	return ret;
}

static int kgsl_drv_memstat_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	unsigned int val = 0;

	if (!strncmp(attr->attr.name, "vmalloc", 7))
		val = kgsl_driver.stats.vmalloc;
	else if (!strncmp(attr->attr.name, "vmalloc_max", 11))
		val = kgsl_driver.stats.vmalloc_max;
	else if (!strncmp(attr->attr.name, "page_alloc", 10))
		val = kgsl_driver.stats.page_alloc;
	else if (!strncmp(attr->attr.name, "page_alloc_max", 14))
		val = kgsl_driver.stats.page_alloc_max;
	else if (!strncmp(attr->attr.name, "coherent", 8))
		val = kgsl_driver.stats.coherent;
	else if (!strncmp(attr->attr.name, "coherent_max", 12))
		val = kgsl_driver.stats.coherent_max;
	else if (!strncmp(attr->attr.name, "mapped", 6))
		val = kgsl_driver.stats.mapped;
	else if (!strncmp(attr->attr.name, "mapped_max", 10))
		val = kgsl_driver.stats.mapped_max;

	return snprintf(buf, PAGE_SIZE, "%u\n", val);
}

static int kgsl_drv_histogram_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int len = 0;
	int i;

	for (i = 0; i < 16; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d ",
			kgsl_driver.stats.histogram[i]);

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static int kgsl_drv_full_cache_threshold_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	unsigned int thresh;
	ret = sscanf(buf, "%d", &thresh);
	if (ret != 1)
		return count;

	kgsl_driver.full_cache_threshold = thresh;

	return count;
}

static int kgsl_drv_full_cache_threshold_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
			kgsl_driver.full_cache_threshold);
}

DEVICE_ATTR(vmalloc, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(vmalloc_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(page_alloc, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(page_alloc_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(coherent, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(coherent_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(mapped, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(mapped_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(histogram, 0444, kgsl_drv_histogram_show, NULL);
DEVICE_ATTR(full_cache_threshold, 0644,
		kgsl_drv_full_cache_threshold_show,
		kgsl_drv_full_cache_threshold_store);

static const struct device_attribute *drv_attr_list[] = {
	&dev_attr_vmalloc,
	&dev_attr_vmalloc_max,
	&dev_attr_page_alloc,
	&dev_attr_page_alloc_max,
	&dev_attr_coherent,
	&dev_attr_coherent_max,
	&dev_attr_mapped,
	&dev_attr_mapped_max,
	&dev_attr_histogram,
	&dev_attr_full_cache_threshold,
	NULL
};

void
kgsl_sharedmem_uninit_sysfs(void)
{
	kgsl_remove_device_sysfs_files(&kgsl_driver.virtdev, drv_attr_list);
}

int
kgsl_sharedmem_init_sysfs(void)
{
	return kgsl_create_device_sysfs_files(&kgsl_driver.virtdev,
		drv_attr_list);
}

#ifdef CONFIG_OUTER_CACHE
static void _outer_cache_range_op(int op, unsigned long addr, size_t size)
{
	switch (op) {
	case KGSL_CACHE_OP_FLUSH:
		outer_flush_range(addr, addr + size);
		break;
	case KGSL_CACHE_OP_CLEAN:
		outer_clean_range(addr, addr + size);
		break;
	case KGSL_CACHE_OP_INV:
		outer_inv_range(addr, addr + size);
		break;
	}
}

static void outer_cache_range_op_sg(struct scatterlist *sg, int sglen, int op)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, sglen, i) {
		unsigned int paddr = kgsl_get_sg_pa(s);
		_outer_cache_range_op(op, paddr, s->length);
	}
}

#else
static void outer_cache_range_op_sg(struct scatterlist *sg, int sglen, int op)
{
}
#endif

static int kgsl_page_alloc_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	int i, pgoff;
	struct scatterlist *s = memdesc->sg;
	unsigned int offset;

	offset = ((unsigned long) vmf->virtual_address - vma->vm_start);

	if (offset >= memdesc->size)
		return VM_FAULT_SIGBUS;

	pgoff = offset >> PAGE_SHIFT;

	/*
	 * The sglist might be comprised of mixed blocks of memory depending
	 * on how many 64K pages were allocated.  This means we have to do math
	 * to find the actual 4K page to map in user space
	 */

	for (i = 0; i < memdesc->sglen; i++) {
		int npages = s->length >> PAGE_SHIFT;

		if (pgoff < npages) {
			struct page *page = sg_page(s);

			page = nth_page(page, pgoff);

			get_page(page);
			vmf->page = page;

			return 0;
		}

		pgoff -= npages;
		s = sg_next(s);
	}

	return VM_FAULT_SIGBUS;
}

static int kgsl_page_alloc_vmflags(struct kgsl_memdesc *memdesc)
{
	return VM_RESERVED | VM_DONTEXPAND;
}

static void kgsl_page_alloc_free(struct kgsl_memdesc *memdesc)
{
	int i = 0;
	struct scatterlist *sg;
	int sglen = memdesc->sglen;

	kgsl_driver.stats.page_alloc -= memdesc->size;

	if (memdesc->hostptr) {
		vunmap(memdesc->hostptr);
		kgsl_driver.stats.vmalloc -= memdesc->size;
	}
	if (memdesc->sg)
		for_each_sg(memdesc->sg, sg, sglen, i)
			__free_pages(sg_page(sg), get_order(sg->length));
}

static int kgsl_contiguous_vmflags(struct kgsl_memdesc *memdesc)
{
	return VM_RESERVED | VM_IO | VM_PFNMAP | VM_DONTEXPAND;
}

/*
 * kgsl_page_alloc_map_kernel - Map the memory in memdesc to kernel address
 * space
 *
 * @memdesc - The memory descriptor which contains information about the memory
 *
 * Return: 0 on success else error code
 */
static int kgsl_page_alloc_map_kernel(struct kgsl_memdesc *memdesc)
{
	if (!memdesc->hostptr) {
		pgprot_t page_prot = pgprot_writecombine(PAGE_KERNEL);
		struct page **pages = NULL;
		struct scatterlist *sg;
		int npages = PAGE_ALIGN(memdesc->size) >> PAGE_SHIFT;
		int sglen = memdesc->sglen;
		int i, count = 0;

		/* create a list of pages to call vmap */
		pages = vmalloc(npages * sizeof(struct page *));
		if (!pages) {
			KGSL_CORE_ERR("vmalloc(%d) failed\n",
				npages * sizeof(struct page *));
			return -ENOMEM;
		}

		for_each_sg(memdesc->sg, sg, sglen, i) {
			struct page *page = sg_page(sg);
			int j;

			for (j = 0; j < sg->length >> PAGE_SHIFT; j++)
				pages[count++] = page++;
		}


		memdesc->hostptr = vmap(pages, count,
					VM_IOREMAP, page_prot);
		KGSL_STATS_ADD(memdesc->size, kgsl_driver.stats.vmalloc,
				kgsl_driver.stats.vmalloc_max);
		vfree(pages);
	}
	if (!memdesc->hostptr)
		return -ENOMEM;

	return 0;
}

static int kgsl_contiguous_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	unsigned long offset, pfn;
	int ret;

	offset = ((unsigned long) vmf->virtual_address - vma->vm_start) >>
		PAGE_SHIFT;

	pfn = (memdesc->physaddr >> PAGE_SHIFT) + offset;
	ret = vm_insert_pfn(vma, (unsigned long) vmf->virtual_address, pfn);

	if (ret == -ENOMEM || ret == -EAGAIN)
		return VM_FAULT_OOM;
	else if (ret == -EFAULT)
		return VM_FAULT_SIGBUS;

	return VM_FAULT_NOPAGE;
}

static void kgsl_ebimem_free(struct kgsl_memdesc *memdesc)

{
	kgsl_driver.stats.coherent -= memdesc->size;
	if (memdesc->hostptr)
		iounmap(memdesc->hostptr);

	free_contiguous_memory_by_paddr(memdesc->physaddr);
}

static int kgsl_ebimem_map_kernel(struct kgsl_memdesc *memdesc)
{
	if (!memdesc->hostptr) {
		memdesc->hostptr = ioremap(memdesc->physaddr, memdesc->size);
		if (!memdesc->hostptr) {
			KGSL_CORE_ERR("ioremap failed, addr:0x%p, size:0x%x\n",
				memdesc->hostptr, memdesc->size);
			return -ENOMEM;
		}
	}

	return 0;
}

static void kgsl_coherent_free(struct kgsl_memdesc *memdesc)
{
	kgsl_driver.stats.coherent -= memdesc->size;
	dma_free_coherent(NULL, memdesc->size,
			  memdesc->hostptr, memdesc->physaddr);
}

/* Global - also used by kgsl_drm.c */
struct kgsl_memdesc_ops kgsl_page_alloc_ops = {
	.free = kgsl_page_alloc_free,
	.vmflags = kgsl_page_alloc_vmflags,
	.vmfault = kgsl_page_alloc_vmfault,
	.map_kernel_mem = kgsl_page_alloc_map_kernel,
};
EXPORT_SYMBOL(kgsl_page_alloc_ops);

static struct kgsl_memdesc_ops kgsl_ebimem_ops = {
	.free = kgsl_ebimem_free,
	.vmflags = kgsl_contiguous_vmflags,
	.vmfault = kgsl_contiguous_vmfault,
	.map_kernel_mem = kgsl_ebimem_map_kernel,
};

static struct kgsl_memdesc_ops kgsl_coherent_ops = {
	.free = kgsl_coherent_free,
};

void kgsl_cache_range_op(struct kgsl_memdesc *memdesc, int op)
{
	/*
	 * If the buffer is mapped in the kernel operate on that address
	 * otherwise use the user address
	 */

	void *addr = (memdesc->hostptr) ?
		memdesc->hostptr : (void *) memdesc->useraddr;

	int size = memdesc->size;

	if (addr !=  NULL) {
		switch (op) {
		case KGSL_CACHE_OP_FLUSH:
			dmac_flush_range(addr, addr + size);
			break;
		case KGSL_CACHE_OP_CLEAN:
			dmac_clean_range(addr, addr + size);
			break;
		case KGSL_CACHE_OP_INV:
			dmac_inv_range(addr, addr + size);
			break;
		}
	}
	outer_cache_range_op_sg(memdesc->sg, memdesc->sglen, op);
}
EXPORT_SYMBOL(kgsl_cache_range_op);

static int
_kgsl_sharedmem_page_alloc(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size)
{
	int pcount = 0, order, ret = 0;
	int j, len, page_size, sglen_alloc, sglen = 0;
	struct page **pages = NULL;
	pgprot_t page_prot = pgprot_writecombine(PAGE_KERNEL);
	void *ptr;
	unsigned int align;
	int step = ((VMALLOC_END - VMALLOC_START)/8) >> PAGE_SHIFT;

	align = (memdesc->flags & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT;

	page_size = (align >= ilog2(SZ_64K) && size >= SZ_64K)
			? SZ_64K : PAGE_SIZE;
	/* update align flags for what we actually use */
	if (page_size != PAGE_SIZE)
		kgsl_memdesc_set_align(memdesc, ilog2(page_size));

	/*
	 * There needs to be enough room in the sg structure to be able to
	 * service the allocation entirely with PAGE_SIZE sized chunks
	 */

	sglen_alloc = PAGE_ALIGN(size) >> PAGE_SHIFT;

	memdesc->pagetable = pagetable;
	memdesc->ops = &kgsl_page_alloc_ops;

	memdesc->sglen_alloc = sglen_alloc;
	memdesc->sg = kgsl_sg_alloc(memdesc->sglen_alloc);

	if (memdesc->sg == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/*
	 * Allocate space to store the list of pages to send to vmap.
	 * This is an array of pointers so we can track 1024 pages per page
	 * of allocation.  Since allocations can be as large as the user dares,
	 * we have to use the kmalloc/vmalloc trick here to make sure we can
	 * get the memory we need.
	 */

	if ((memdesc->sglen_alloc * sizeof(struct page *)) > PAGE_SIZE)
		pages = vmalloc(memdesc->sglen_alloc * sizeof(struct page *));
	else
		pages = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (pages == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	kmemleak_not_leak(memdesc->sg);

	sg_init_table(memdesc->sg, memdesc->sglen_alloc);

	len = size;

	while (len > 0) {
		struct page *page;
		unsigned int gfp_mask = __GFP_HIGHMEM;
		int j;

		/* don't waste space at the end of the allocation*/
		if (len < page_size)
			page_size = PAGE_SIZE;

		/*
		 * Don't do some of the more aggressive memory recovery
		 * techniques for large order allocations
		 */
		if (page_size != PAGE_SIZE)
			gfp_mask |= __GFP_COMP | __GFP_NORETRY |
				__GFP_NO_KSWAPD | __GFP_NOWARN;
		else
			gfp_mask |= GFP_KERNEL;

		page = alloc_pages(gfp_mask, get_order(page_size));

		if (page == NULL) {
			if (page_size != PAGE_SIZE) {
				page_size = PAGE_SIZE;
				continue;
			}

			/*
			 * Update sglen and memdesc size,as requested allocation
			 * not served fully. So that they can be correctly freed
			 * in kgsl_sharedmem_free().
			 */
			memdesc->sglen = sglen;
			memdesc->size = (size - len);

			KGSL_CORE_ERR(
				"Out of memory: only allocated %dKB of %dKB requested\n",
				(size - len) >> 10, size >> 10);

			ret = -ENOMEM;
			goto done;
		}

		for (j = 0; j < page_size >> PAGE_SHIFT; j++)
			pages[pcount++] = nth_page(page, j);

		sg_set_page(&memdesc->sg[sglen++], page, page_size, 0);
		len -= page_size;
	}

	memdesc->sglen = sglen;
	memdesc->size = size;

	/*
	 * All memory that goes to the user has to be zeroed out before it gets
	 * exposed to userspace. This means that the memory has to be mapped in
	 * the kernel, zeroed (memset) and then unmapped.  This also means that
	 * the dcache has to be flushed to ensure coherency between the kernel
	 * and user pages. We used to pass __GFP_ZERO to alloc_page which mapped
	 * zeroed and unmaped each individual page, and then we had to turn
	 * around and call flush_dcache_page() on that page to clear the caches.
	 * This was killing us for performance. Instead, we found it is much
	 * faster to allocate the pages without GFP_ZERO, map a chunk of the
	 * range ('step' pages), memset it, flush it and then unmap
	 * - this results in a factor of 4 improvement for speed for large
	 * buffers. There is a small decrease in speed for small buffers,
	 * but only on the order of a few microseconds at best. The 'step'
	 * size is based on a guess at the amount of free vmalloc space, but
	 * will scale down if there's not enough free space.
	 */
	for (j = 0; j < pcount; j += step) {
		step = min(step, pcount - j);

		ptr = vmap(&pages[j], step, VM_IOREMAP, page_prot);

		if (ptr != NULL) {
			memset(ptr, 0, step * PAGE_SIZE);
			dmac_flush_range(ptr, ptr + step * PAGE_SIZE);
			vunmap(ptr);
		} else {
			int k;
			/* Very, very, very slow path */

			for (k = j; k < j + step; k++) {
				ptr = kmap_atomic(pages[k]);
				memset(ptr, 0, PAGE_SIZE);
				dmac_flush_range(ptr, ptr + PAGE_SIZE);
				kunmap_atomic(ptr);
			}
			/* scale down the step size to avoid this path */
			if (step > 1)
				step >>= 1;
		}
	}

	outer_cache_range_op_sg(memdesc->sg, memdesc->sglen,
				KGSL_CACHE_OP_FLUSH);

	order = get_order(size);

	if (order < 16)
		kgsl_driver.stats.histogram[order]++;

done:
	KGSL_STATS_ADD(memdesc->size, kgsl_driver.stats.page_alloc,
		kgsl_driver.stats.page_alloc_max);

	if ((memdesc->sglen_alloc * sizeof(struct page *)) > PAGE_SIZE)
		vfree(pages);
	else
		kfree(pages);

	if (ret)
		kgsl_sharedmem_free(memdesc);

	return ret;
}

int
kgsl_sharedmem_page_alloc(struct kgsl_memdesc *memdesc,
		       struct kgsl_pagetable *pagetable, size_t size)
{
	int ret = 0;
	BUG_ON(size == 0);

	size = ALIGN(size, PAGE_SIZE * 2);

	ret =  _kgsl_sharedmem_page_alloc(memdesc, pagetable, size);
	if (!ret)
		ret = kgsl_page_alloc_map_kernel(memdesc);
	if (ret)
		kgsl_sharedmem_free(memdesc);
	return ret;
}
EXPORT_SYMBOL(kgsl_sharedmem_page_alloc);

int
kgsl_sharedmem_page_alloc_user(struct kgsl_memdesc *memdesc,
			    struct kgsl_pagetable *pagetable,
			    size_t size)
{
	return _kgsl_sharedmem_page_alloc(memdesc, pagetable, PAGE_ALIGN(size));
}
EXPORT_SYMBOL(kgsl_sharedmem_page_alloc_user);

int
kgsl_sharedmem_alloc_coherent(struct kgsl_memdesc *memdesc, size_t size)
{
	int result = 0;

	size = ALIGN(size, PAGE_SIZE);

	memdesc->size = size;
	memdesc->ops = &kgsl_coherent_ops;

	memdesc->hostptr = dma_alloc_coherent(NULL, size, &memdesc->physaddr,
					      GFP_KERNEL);
	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR("dma_alloc_coherent(%d) failed\n", size);
		result = -ENOMEM;
		goto err;
	}

	result = memdesc_sg_phys(memdesc, memdesc->physaddr, size);
	if (result)
		goto err;

	/* Record statistics */

	KGSL_STATS_ADD(size, kgsl_driver.stats.coherent,
		       kgsl_driver.stats.coherent_max);

err:
	if (result)
		kgsl_sharedmem_free(memdesc);

	return result;
}
EXPORT_SYMBOL(kgsl_sharedmem_alloc_coherent);

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	if (memdesc == NULL || memdesc->size == 0)
		return;

	if (memdesc->gpuaddr) {
		kgsl_mmu_unmap(memdesc->pagetable, memdesc);
		kgsl_mmu_put_gpuaddr(memdesc->pagetable, memdesc);
	}

	if (memdesc->ops && memdesc->ops->free)
		memdesc->ops->free(memdesc);

	kgsl_sg_free(memdesc->sg, memdesc->sglen_alloc);

	memset(memdesc, 0, sizeof(*memdesc));
}
EXPORT_SYMBOL(kgsl_sharedmem_free);

static int
_kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable, size_t size)
{
	int result = 0;

	memdesc->size = size;
	memdesc->pagetable = pagetable;
	memdesc->ops = &kgsl_ebimem_ops;
	memdesc->physaddr = allocate_contiguous_ebi_nomap(size, SZ_8K);

	if (memdesc->physaddr == 0) {
		KGSL_CORE_ERR("allocate_contiguous_ebi_nomap(%d) failed\n",
			size);
		return -ENOMEM;
	}

	result = memdesc_sg_phys(memdesc, memdesc->physaddr, size);

	if (result)
		goto err;

	KGSL_STATS_ADD(size, kgsl_driver.stats.coherent,
		kgsl_driver.stats.coherent_max);

err:
	if (result)
		kgsl_sharedmem_free(memdesc);

	return result;
}

int
kgsl_sharedmem_ebimem_user(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size)
{
	size = ALIGN(size, PAGE_SIZE);
	return _kgsl_sharedmem_ebimem(memdesc, pagetable, size);
}
EXPORT_SYMBOL(kgsl_sharedmem_ebimem_user);

int
kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable, size_t size)
{
	int result;
	size = ALIGN(size, 8192);
	result = _kgsl_sharedmem_ebimem(memdesc, pagetable, size);

	if (result)
		return result;

	memdesc->hostptr = ioremap(memdesc->physaddr, size);

	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR("ioremap failed\n");
		kgsl_sharedmem_free(memdesc);
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_ebimem);

int
kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			unsigned int offsetbytes)
{
	uint32_t *src;
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL || dst == NULL);
	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes + sizeof(uint32_t) > memdesc->size);
	if (offsetbytes + sizeof(uint32_t) > memdesc->size)
		return -ERANGE;
	src = (uint32_t *)(memdesc->hostptr + offsetbytes);
	*dst = *src;
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_readl);

int
kgsl_sharedmem_writel(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src)
{
	uint32_t *dst;
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL);
	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes + sizeof(uint32_t) > memdesc->size);
	if (offsetbytes + sizeof(uint32_t) > memdesc->size)
		return -ERANGE;
	kgsl_cffdump_setmem(device,
		memdesc->gpuaddr + offsetbytes,
		src, sizeof(uint32_t));
	dst = (uint32_t *)(memdesc->hostptr + offsetbytes);
	*dst = src;
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_writel);

int
kgsl_sharedmem_set(struct kgsl_device *device,
		const struct kgsl_memdesc *memdesc, unsigned int offsetbytes,
		unsigned int value, unsigned int sizebytes)
{
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL);
	BUG_ON(offsetbytes + sizebytes > memdesc->size);

	kgsl_cffdump_setmem(device,
		memdesc->gpuaddr + offsetbytes, value,
		sizebytes);
	memset(memdesc->hostptr + offsetbytes, value, sizebytes);
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_set);

/*
 * kgsl_sharedmem_map_vma - Map a user vma to physical memory
 *
 * @vma - The user vma to map
 * @memdesc - The memory descriptor which contains information about the
 * physical memory
 *
 * Return: 0 on success else error code
 */
int
kgsl_sharedmem_map_vma(struct vm_area_struct *vma,
			const struct kgsl_memdesc *memdesc)
{
	unsigned long addr = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	int ret, i = 0;

	if (!memdesc->sg || (size != memdesc->size) ||
		(memdesc->sglen != (size / PAGE_SIZE)))
		return -EINVAL;

	for (; addr < vma->vm_end; addr += PAGE_SIZE, i++) {
		ret = vm_insert_page(vma, addr, sg_page(&memdesc->sg[i]));
		if (ret)
			return ret;
	}
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_map_vma);

static const char * const memtype_str[] = {
	[KGSL_MEMTYPE_OBJECTANY] = "any(0)",
	[KGSL_MEMTYPE_FRAMEBUFFER] = "framebuffer",
	[KGSL_MEMTYPE_RENDERBUFFER] = "renderbuffer",
	[KGSL_MEMTYPE_ARRAYBUFFER] = "arraybuffer",
	[KGSL_MEMTYPE_ELEMENTARRAYBUFFER] = "elementarraybuffer",
	[KGSL_MEMTYPE_VERTEXARRAYBUFFER] = "vertexarraybuffer",
	[KGSL_MEMTYPE_TEXTURE] = "texture",
	[KGSL_MEMTYPE_SURFACE] = "surface",
	[KGSL_MEMTYPE_EGL_SURFACE] = "egl_surface",
	[KGSL_MEMTYPE_GL] = "gl",
	[KGSL_MEMTYPE_CL] = "cl",
	[KGSL_MEMTYPE_CL_BUFFER_MAP] = "cl_buffer_map",
	[KGSL_MEMTYPE_CL_BUFFER_NOMAP] = "cl_buffer_nomap",
	[KGSL_MEMTYPE_CL_IMAGE_MAP] = "cl_image_map",
	[KGSL_MEMTYPE_CL_IMAGE_NOMAP] = "cl_image_nomap",
	[KGSL_MEMTYPE_CL_KERNEL_STACK] = "cl_kernel_stack",
	[KGSL_MEMTYPE_COMMAND] = "command",
	[KGSL_MEMTYPE_2D] = "2d",
	[KGSL_MEMTYPE_EGL_IMAGE] = "egl_image",
	[KGSL_MEMTYPE_EGL_SHADOW] = "egl_shadow",
	[KGSL_MEMTYPE_MULTISAMPLE] = "egl_multisample",
	/* KGSL_MEMTYPE_KERNEL handled below, to avoid huge array */
};

void kgsl_get_memory_usage(char *name, size_t name_size, unsigned int memflags)
{
	unsigned char type;

	type = (memflags & KGSL_MEMTYPE_MASK) >> KGSL_MEMTYPE_SHIFT;
	if (type == KGSL_MEMTYPE_KERNEL)
		strlcpy(name, "kernel", name_size);
	else if (type < ARRAY_SIZE(memtype_str) && memtype_str[type] != NULL)
		strlcpy(name, memtype_str[type], name_size);
	else
		snprintf(name, name_size, "unknown(%3d)", type);
}
EXPORT_SYMBOL(kgsl_get_memory_usage);
