/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/vmalloc.h>
#include <linux/memory_alloc.h>
#include <asm/cacheflush.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"
#include "kgsl_device.h"

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

/* sharedmem / memory sysfs files */

static ssize_t
process_show(struct kobject *kobj,
	     struct kobj_attribute *attr,
	     char *buf)
{
	struct kgsl_process_private *priv;
	unsigned int val = 0;

	mutex_lock(&kgsl_driver.process_mutex);
	priv = _get_priv_from_kobj(kobj);

	if (priv == NULL) {
		mutex_unlock(&kgsl_driver.process_mutex);
		return 0;
	}

	if (!strncmp(attr->attr.name, "user", 4))
		val = priv->stats.user;
	if (!strncmp(attr->attr.name, "user_max", 8))
		val = priv->stats.user_max;
	if (!strncmp(attr->attr.name, "mapped", 6))
		val = priv->stats.mapped;
	if (!strncmp(attr->attr.name, "mapped_max", 10))
		val = priv->stats.mapped_max;
	if (!strncmp(attr->attr.name, "flushes", 7))
		val = priv->stats.flushes;

	mutex_unlock(&kgsl_driver.process_mutex);
	return snprintf(buf, PAGE_SIZE, "%u\n", val);
}

#define KGSL_MEMSTAT_ATTR(_name, _show) \
	static struct kobj_attribute attr_##_name = \
	__ATTR(_name, 0444, _show, NULL)

KGSL_MEMSTAT_ATTR(user, process_show);
KGSL_MEMSTAT_ATTR(user_max, process_show);
KGSL_MEMSTAT_ATTR(mapped, process_show);
KGSL_MEMSTAT_ATTR(mapped_max, process_show);
KGSL_MEMSTAT_ATTR(flushes, process_show);

static struct attribute *process_attrs[] = {
	&attr_user.attr,
	&attr_user_max.attr,
	&attr_mapped.attr,
	&attr_mapped_max.attr,
	&attr_flushes.attr,
	NULL
};

static struct attribute_group process_attr_group = {
	.attrs = process_attrs,
};

void
kgsl_process_uninit_sysfs(struct kgsl_process_private *private)
{
	/* Remove the sysfs entry */
	if (private->kobj) {
		sysfs_remove_group(private->kobj, &process_attr_group);
		kobject_put(private->kobj);
	}
}

void
kgsl_process_init_sysfs(struct kgsl_process_private *private)
{
	unsigned char name[16];

	/* Add a entry to the sysfs device */
	snprintf(name, sizeof(name), "%d", private->pid);
	private->kobj = kobject_create_and_add(name, kgsl_driver.prockobj);

	/* sysfs failure isn't fatal, just annoying */
	if (private->kobj != NULL) {
		if (sysfs_create_group(private->kobj, &process_attr_group)) {
			kobject_put(private->kobj);
			private->kobj = NULL;
		}
	}
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
		len += sprintf(buf + len, "%d ",
			kgsl_driver.stats.histogram[i]);

	len += sprintf(buf + len, "\n");
	return len;
}

DEVICE_ATTR(vmalloc, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(vmalloc_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(coherent, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(coherent_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(mapped, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(mapped_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(histogram, 0444, kgsl_drv_histogram_show, NULL);

static const struct device_attribute *drv_attr_list[] = {
	&dev_attr_vmalloc,
	&dev_attr_vmalloc_max,
	&dev_attr_coherent,
	&dev_attr_coherent_max,
	&dev_attr_mapped,
	&dev_attr_mapped_max,
	&dev_attr_histogram,
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
#endif

static unsigned long kgsl_vmalloc_physaddr(struct kgsl_memdesc *memdesc,
					   unsigned int offset)
{
	unsigned int addr;

	if (offset > memdesc->size)
		return 0;

	addr = vmalloc_to_pfn(memdesc->hostptr + offset);
	return addr << PAGE_SHIFT;
}

#ifdef CONFIG_OUTER_CACHE
static void kgsl_vmalloc_outer_cache(struct kgsl_memdesc *memdesc, int op)
{
	void *vaddr = memdesc->hostptr;
	for (; vaddr < (memdesc->hostptr + memdesc->size); vaddr += PAGE_SIZE) {
		unsigned long paddr = page_to_phys(vmalloc_to_page(vaddr));
		_outer_cache_range_op(op, paddr, PAGE_SIZE);
	}
}
#endif

static int kgsl_vmalloc_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	unsigned long offset, pg;
	struct page *page;

	offset = (unsigned long) vmf->virtual_address - vma->vm_start;
	pg = (unsigned long) memdesc->hostptr + offset;

	page = vmalloc_to_page((void *) pg);
	if (page == NULL)
		return VM_FAULT_SIGBUS;

	get_page(page);

	vmf->page = page;
	return 0;
}

static int kgsl_vmalloc_vmflags(struct kgsl_memdesc *memdesc)
{
	return VM_RESERVED | VM_DONTEXPAND;
}

static void kgsl_vmalloc_free(struct kgsl_memdesc *memdesc)
{
	kgsl_driver.stats.vmalloc -= memdesc->size;
	vfree(memdesc->hostptr);
}

static int kgsl_contiguous_vmflags(struct kgsl_memdesc *memdesc)
{
	return VM_RESERVED | VM_IO | VM_PFNMAP | VM_DONTEXPAND;
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

static void kgsl_coherent_free(struct kgsl_memdesc *memdesc)
{
	kgsl_driver.stats.coherent -= memdesc->size;
	dma_free_coherent(NULL, memdesc->size,
			  memdesc->hostptr, memdesc->physaddr);
}

static unsigned long kgsl_contiguous_physaddr(struct kgsl_memdesc *memdesc,
					unsigned int offset)
{
	if (offset > memdesc->size)
		return 0;

	return memdesc->physaddr + offset;
}

#ifdef CONFIG_OUTER_CACHE
static void kgsl_contiguous_outer_cache(struct kgsl_memdesc *memdesc, int op)
{
	_outer_cache_range_op(op, memdesc->physaddr, memdesc->size);
}
#endif

#ifdef CONFIG_OUTER_CACHE
static void kgsl_userptr_outer_cache(struct kgsl_memdesc *memdesc, int op)
{
	void *vaddr = memdesc->hostptr;
	for (; vaddr < (memdesc->hostptr + memdesc->size); vaddr += PAGE_SIZE) {
		unsigned long paddr = kgsl_virtaddr_to_physaddr(vaddr);
		if (paddr)
			_outer_cache_range_op(op, paddr, PAGE_SIZE);
	}
}
#endif

static unsigned long kgsl_userptr_physaddr(struct kgsl_memdesc *memdesc,
					   unsigned int offset)
{
	return kgsl_virtaddr_to_physaddr(memdesc->hostptr + offset);
}

/* Global - also used by kgsl_drm.c */
struct kgsl_memdesc_ops kgsl_vmalloc_ops = {
	.physaddr = kgsl_vmalloc_physaddr,
	.free = kgsl_vmalloc_free,
	.vmflags = kgsl_vmalloc_vmflags,
	.vmfault = kgsl_vmalloc_vmfault,
#ifdef CONFIG_OUTER_CACHE
	.outer_cache = kgsl_vmalloc_outer_cache,
#endif
};
EXPORT_SYMBOL(kgsl_vmalloc_ops);

static struct kgsl_memdesc_ops kgsl_ebimem_ops = {
	.physaddr = kgsl_contiguous_physaddr,
	.free = kgsl_ebimem_free,
	.vmflags = kgsl_contiguous_vmflags,
	.vmfault = kgsl_contiguous_vmfault,
#ifdef CONFIG_OUTER_CACHE
	.outer_cache = kgsl_contiguous_outer_cache,
#endif
};

static struct kgsl_memdesc_ops kgsl_coherent_ops = {
	.physaddr = kgsl_contiguous_physaddr,
	.free = kgsl_coherent_free,
#ifdef CONFIG_OUTER_CACHE
	.outer_cache = kgsl_contiguous_outer_cache,
#endif
};

/* Global - also used by kgsl.c and kgsl_drm.c */
struct kgsl_memdesc_ops kgsl_contiguous_ops = {
	.physaddr = kgsl_contiguous_physaddr,
#ifdef CONFIG_OUTER_CACHE
	.outer_cache = kgsl_contiguous_outer_cache
#endif
};
EXPORT_SYMBOL(kgsl_contiguous_ops);

/* Global - also used by kgsl.c */
struct kgsl_memdesc_ops kgsl_userptr_ops = {
	.physaddr = kgsl_userptr_physaddr,
#ifdef CONFIG_OUTER_CACHE
	.outer_cache = kgsl_userptr_outer_cache,
#endif
};
EXPORT_SYMBOL(kgsl_userptr_ops);

void kgsl_cache_range_op(struct kgsl_memdesc *memdesc, int op)
{
	void *addr = memdesc->hostptr;
	int size = memdesc->size;

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

	if (memdesc->ops->outer_cache)
		memdesc->ops->outer_cache(memdesc, op);
}
EXPORT_SYMBOL(kgsl_cache_range_op);

static int
_kgsl_sharedmem_vmalloc(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			void *ptr, size_t size, unsigned int protflags)
{
	int result;

	memdesc->size = size;
	memdesc->pagetable = pagetable;
	memdesc->priv = KGSL_MEMFLAGS_CACHED;
	memdesc->ops = &kgsl_vmalloc_ops;
	memdesc->hostptr = (void *) ptr;

	kgsl_cache_range_op(memdesc, KGSL_CACHE_OP_INV);

	result = kgsl_mmu_map(pagetable, memdesc, protflags);

	if (result) {
		kgsl_sharedmem_free(memdesc);
	} else {
		int order;

		KGSL_STATS_ADD(size, kgsl_driver.stats.vmalloc,
			kgsl_driver.stats.vmalloc_max);

		order = get_order(size);

		if (order < 16)
			kgsl_driver.stats.histogram[order]++;
	}

	return result;
}

int
kgsl_sharedmem_vmalloc(struct kgsl_memdesc *memdesc,
		       struct kgsl_pagetable *pagetable, size_t size)
{
	void *ptr;

	BUG_ON(size == 0);

	size = ALIGN(size, PAGE_SIZE * 2);
	ptr = vmalloc(size);

	if (ptr  == NULL) {
		KGSL_CORE_ERR("vmalloc(%d) failed\n", size);
		return -ENOMEM;
	}

	return _kgsl_sharedmem_vmalloc(memdesc, pagetable, ptr, size,
		GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);
}
EXPORT_SYMBOL(kgsl_sharedmem_vmalloc);

int
kgsl_sharedmem_vmalloc_user(struct kgsl_memdesc *memdesc,
			    struct kgsl_pagetable *pagetable,
			    size_t size, int flags)
{
	void *ptr;
	unsigned int protflags;

	BUG_ON(size == 0);
	ptr = vmalloc_user(size);

	if (ptr == NULL) {
		KGSL_CORE_ERR("vmalloc_user(%d) failed: allocated=%d\n",
			      size, kgsl_driver.stats.vmalloc);
		return -ENOMEM;
	}

	protflags = GSL_PT_PAGE_RV;
	if (!(flags & KGSL_MEMFLAGS_GPUREADONLY))
		protflags |= GSL_PT_PAGE_WV;

	return _kgsl_sharedmem_vmalloc(memdesc, pagetable, ptr, size,
		protflags);
}
EXPORT_SYMBOL(kgsl_sharedmem_vmalloc_user);

int
kgsl_sharedmem_alloc_coherent(struct kgsl_memdesc *memdesc, size_t size)
{
	size = ALIGN(size, PAGE_SIZE);

	memdesc->hostptr = dma_alloc_coherent(NULL, size, &memdesc->physaddr,
					      GFP_KERNEL);
	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR("dma_alloc_coherent(%d) failed\n", size);
		return -ENOMEM;
	}

	memdesc->size = size;
	memdesc->ops = &kgsl_coherent_ops;

	/* Record statistics */

	KGSL_STATS_ADD(size, kgsl_driver.stats.coherent,
		       kgsl_driver.stats.coherent_max);

	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_alloc_coherent);

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	if (memdesc == NULL || memdesc->size == 0)
		return;

	if (memdesc->gpuaddr)
		kgsl_mmu_unmap(memdesc->pagetable, memdesc);

	if (memdesc->ops->free)
		memdesc->ops->free(memdesc);

	memset(memdesc, 0, sizeof(*memdesc));
}
EXPORT_SYMBOL(kgsl_sharedmem_free);

static int
_kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable, size_t size)
{
	int result;

	memdesc->physaddr = allocate_contiguous_ebi_nomap(size, SZ_8K);

	if (memdesc->physaddr == 0) {
		KGSL_CORE_ERR("allocate_contiguous_ebi_nomap(%d) failed\n",
			size);
		return -ENOMEM;
	}

	memdesc->size = size;
	memdesc->pagetable = pagetable;
	memdesc->ops = &kgsl_ebimem_ops;

	result = kgsl_mmu_map(pagetable, memdesc,
		GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);

	if (result)
		kgsl_sharedmem_free(memdesc);

	KGSL_STATS_ADD(size, kgsl_driver.stats.coherent,
		kgsl_driver.stats.coherent_max);

	return result;
}

int
kgsl_sharedmem_ebimem_user(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size, int flags)
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
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL || dst == NULL);
	WARN_ON(offsetbytes + sizeof(unsigned int) > memdesc->size);

	if (offsetbytes + sizeof(unsigned int) > memdesc->size)
		return -ERANGE;

	*dst = readl_relaxed(memdesc->hostptr + offsetbytes);
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_readl);

int
kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src)
{
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL);
	BUG_ON(offsetbytes + sizeof(unsigned int) > memdesc->size);

	kgsl_cffdump_setmem(memdesc->physaddr + offsetbytes,
		src, sizeof(uint));
	writel_relaxed(src, memdesc->hostptr + offsetbytes);
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_writel);

int
kgsl_sharedmem_set(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes,
			unsigned int value, unsigned int sizebytes)
{
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL);
	BUG_ON(offsetbytes + sizebytes > memdesc->size);

	kgsl_cffdump_setmem(memdesc->physaddr + offsetbytes, value,
		sizebytes);
	memset(memdesc->hostptr + offsetbytes, value, sizebytes);
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_set);
