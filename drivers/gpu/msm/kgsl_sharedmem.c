// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/of_platform.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/random.h>

#include "kgsl_device.h"
#include "kgsl_pool.h"
#include "kgsl_sharedmem.h"

/*
 * The user can set this from debugfs to force failed memory allocations to
 * fail without trying OOM first.  This is a debug setting useful for
 * stress applications that want to test failure cases without pushing the
 * system into unrecoverable OOM panics
 */

static bool sharedmem_noretry_flag;

static DEFINE_MUTEX(kernel_map_global_lock);

#define MEMTYPE(_type, _name) { \
	.type = _type, \
	.attr = { .name = _name, .mode = 0444 } \
}

struct kgsl_memtype {
	unsigned int type;
	struct attribute attr;
};

static const struct kgsl_memtype memtypes[] = {
	MEMTYPE(KGSL_MEMTYPE_OBJECTANY, "any(0)"),
	MEMTYPE(KGSL_MEMTYPE_FRAMEBUFFER, "framebuffer"),
	MEMTYPE(KGSL_MEMTYPE_RENDERBUFFER, "renderbuffer"),
	MEMTYPE(KGSL_MEMTYPE_ARRAYBUFFER, "arraybuffer"),
	MEMTYPE(KGSL_MEMTYPE_ELEMENTARRAYBUFFER, "elementarraybuffer"),
	MEMTYPE(KGSL_MEMTYPE_VERTEXARRAYBUFFER, "vertexarraybuffer"),
	MEMTYPE(KGSL_MEMTYPE_TEXTURE, "texture"),
	MEMTYPE(KGSL_MEMTYPE_SURFACE, "surface"),
	MEMTYPE(KGSL_MEMTYPE_EGL_SURFACE, "egl_surface"),
	MEMTYPE(KGSL_MEMTYPE_GL, "gl"),
	MEMTYPE(KGSL_MEMTYPE_CL, "cl"),
	MEMTYPE(KGSL_MEMTYPE_CL_BUFFER_MAP, "cl_buffer_map"),
	MEMTYPE(KGSL_MEMTYPE_CL_BUFFER_NOMAP, "cl_buffer_nomap"),
	MEMTYPE(KGSL_MEMTYPE_CL_IMAGE_MAP, "cl_image_map"),
	MEMTYPE(KGSL_MEMTYPE_CL_IMAGE_NOMAP, "cl_image_nomap"),
	MEMTYPE(KGSL_MEMTYPE_CL_KERNEL_STACK, "cl_kernel_stack"),
	MEMTYPE(KGSL_MEMTYPE_COMMAND, "command"),
	MEMTYPE(KGSL_MEMTYPE_2D, "2d"),
	MEMTYPE(KGSL_MEMTYPE_EGL_IMAGE, "egl_image"),
	MEMTYPE(KGSL_MEMTYPE_EGL_SHADOW, "egl_shadow"),
	MEMTYPE(KGSL_MEMTYPE_MULTISAMPLE, "egl_multisample"),
	MEMTYPE(KGSL_MEMTYPE_KERNEL, "kernel"),
};

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

static ssize_t
imported_mem_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	struct kgsl_mem_entry *entry;
	uint64_t imported_mem = 0;
	int id = 0;

	spin_lock(&priv->mem_lock);
	for (entry = idr_get_next(&priv->mem_idr, &id); entry;
		id++, entry = idr_get_next(&priv->mem_idr, &id)) {

		int egl_surface_count = 0, egl_image_count = 0;
		struct kgsl_memdesc *m;

		if (kgsl_mem_entry_get(entry) == 0)
			continue;
		spin_unlock(&priv->mem_lock);

		m = &entry->memdesc;
		if (kgsl_memdesc_usermem_type(m) == KGSL_MEM_ENTRY_ION) {
			kgsl_get_egl_counts(entry, &egl_surface_count,
					&egl_image_count);

			if (kgsl_memdesc_get_memtype(m) ==
						KGSL_MEMTYPE_EGL_SURFACE)
				imported_mem += m->size;
			else if (egl_surface_count == 0) {
				uint64_t size = m->size;

				do_div(size, (egl_image_count ?
							egl_image_count : 1));
				imported_mem += size;
			}
		}

		/*
		 * If refcount on mem entry is the last refcount, we will
		 * call kgsl_mem_entry_destroy and detach it from process
		 * list. When there is no refcount on the process private,
		 * we will call kgsl_destroy_process_private to do cleanup.
		 * During cleanup, we will try to remove the same sysfs
		 * node which is in use by the current thread and this
		 * situation will end up in a deadloack.
		 * To avoid this situation, use a worker to put the refcount
		 * on mem entry.
		 */
		kgsl_mem_entry_put_deferred(entry);
		spin_lock(&priv->mem_lock);
	}
	spin_unlock(&priv->mem_lock);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", imported_mem);
}

static ssize_t
gpumem_mapped_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			atomic64_read(&priv->gpumem_mapped));
}

static ssize_t
gpumem_unmapped_show(struct kgsl_process_private *priv, int type, char *buf)
{
	u64 gpumem_total = atomic64_read(&priv->stats[type].cur);
	u64 gpumem_mapped = atomic64_read(&priv->gpumem_mapped);

	if (gpumem_mapped > gpumem_total)
		return -EIO;

	return scnprintf(buf, PAGE_SIZE, "%llu\n",
			gpumem_total - gpumem_mapped);
}

static struct kgsl_mem_entry_attribute debug_memstats[] = {
	__MEM_ENTRY_ATTR(0, imported_mem, imported_mem_show),
	__MEM_ENTRY_ATTR(0, gpumem_mapped, gpumem_mapped_show),
	__MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_KERNEL, gpumem_unmapped,
				gpumem_unmapped_show),
};

/**
 * Show the current amount of memory allocated for the given memtype
 */

static ssize_t
mem_entry_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			atomic64_read(&priv->stats[type].cur));
}

/**
 * Show the maximum memory allocated for the given memtype through the life of
 * the process
 */

static ssize_t
mem_entry_max_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%llu\n", priv->stats[type].max);
}

static ssize_t mem_entry_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_mem_entry_attribute *pattr = to_mem_entry_attr(attr);
	struct kgsl_process_private *priv;
	ssize_t ret;

	/*
	 * sysfs_remove_file waits for reads to complete before the node is
	 * deleted and process private is freed only once kobj is released.
	 * This implies that priv will not be freed until this function
	 * completes, and no further locking is needed.
	 */
	priv = kobj ? container_of(kobj, struct kgsl_process_private, kobj) :
			NULL;

	if (priv && pattr->show)
		ret = pattr->show(priv, pattr->memtype, buf);
	else
		ret = -EIO;

	return ret;
}

static ssize_t memtype_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_process_private *priv;
	struct kgsl_memtype *memtype;
	struct kgsl_mem_entry *entry;
	u64 size = 0;
	int id = 0;

	priv = container_of(kobj, struct kgsl_process_private, kobj_memtype);
	memtype = container_of(attr, struct kgsl_memtype, attr);

	spin_lock(&priv->mem_lock);
	for (entry = idr_get_next(&priv->mem_idr, &id); entry;
		id++, entry = idr_get_next(&priv->mem_idr, &id)) {
		struct kgsl_memdesc *memdesc;
		unsigned int type;

		if (kgsl_mem_entry_get(entry) == 0)
			continue;
		spin_unlock(&priv->mem_lock);

		memdesc = &entry->memdesc;
		type = MEMFLAGS(memdesc->flags, KGSL_MEMTYPE_MASK,
				KGSL_MEMTYPE_SHIFT);

		if (type == memtype->type)
			size += memdesc->size;

		kgsl_mem_entry_put_deferred(entry);
		spin_lock(&priv->mem_lock);
	}
	spin_unlock(&priv->mem_lock);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", size);
}

/* Dummy release function - we have nothing to do here */
static void mem_entry_release(struct kobject *kobj)
{
}

static const struct sysfs_ops mem_entry_sysfs_ops = {
	.show = mem_entry_sysfs_show,
};

static struct kobj_type ktype_mem_entry = {
	.sysfs_ops = &mem_entry_sysfs_ops,
	.release = &mem_entry_release,
};

static const struct sysfs_ops memtype_sysfs_ops = {
	.show = memtype_sysfs_show,
};

static struct kobj_type ktype_memtype = {
	.sysfs_ops = &memtype_sysfs_ops,
};

static struct mem_entry_stats mem_stats[] = {
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_KERNEL, kernel),
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


	for (i = 0; i < ARRAY_SIZE(memtypes); i++)
		sysfs_remove_file(&private->kobj_memtype, &memtypes[i].attr);

	kobject_put(&private->kobj_memtype);
	kobject_put(&private->kobj);
}

/**
 * kgsl_process_init_sysfs() - Initialize and create sysfs files for a process
 *
 * @device: Pointer to kgsl device struct
 * @private: Pointer to the structure for the process
 *
 * kgsl_process_init_sysfs() is called at the time of creating the
 * process struct when a process opens the kgsl device for the first time.
 * This function creates the sysfs files for the process.
 */
void kgsl_process_init_sysfs(struct kgsl_device *device,
		struct kgsl_process_private *private)
{
	int i;

	if (kobject_init_and_add(&private->kobj, &ktype_mem_entry,
		kgsl_driver.prockobj, "%d", pid_nr(private->pid))) {
		dev_err(device->dev, "Unable to add sysfs for process %d\n",
			pid_nr(private->pid));
		return;
	}

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		int ret;

		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].attr.attr);
		ret |= sysfs_create_file(&private->kobj,
			&mem_stats[i].max_attr.attr);

		if (ret)
			dev_err(device->dev,
				"Unable to create sysfs files for process %d\n",
				pid_nr(private->pid));
	}

	for (i = 0; i < ARRAY_SIZE(debug_memstats); i++) {
		if (sysfs_create_file(&private->kobj,
			&debug_memstats[i].attr))
			WARN(1, "Couldn't create sysfs file '%s'\n",
				debug_memstats[i].attr.name);
	}

	if (kobject_init_and_add(&private->kobj_memtype, &ktype_memtype,
		&private->kobj, "memtype")) {
		dev_err(device->dev,
				"Unable to add memtype sysfs for process %d\n",
				pid_nr(private->pid));
		return;
	}

	for (i = 0; i < ARRAY_SIZE(memtypes); i++) {
		if (sysfs_create_file(&private->kobj_memtype,
			&memtypes[i].attr))
			WARN(1, "Couldn't create sysfs file '%s'\n",
				memtypes[i].attr.name);
	}
}

static ssize_t memstat_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	uint64_t val = 0;

	if (!strcmp(attr->attr.name, "vmalloc"))
		val = atomic_long_read(&kgsl_driver.stats.vmalloc);
	else if (!strcmp(attr->attr.name, "vmalloc_max"))
		val = atomic_long_read(&kgsl_driver.stats.vmalloc_max);
	else if (!strcmp(attr->attr.name, "page_alloc"))
		val = atomic_long_read(&kgsl_driver.stats.page_alloc);
	else if (!strcmp(attr->attr.name, "page_alloc_max"))
		val = atomic_long_read(&kgsl_driver.stats.page_alloc_max);
	else if (!strcmp(attr->attr.name, "coherent"))
		val = atomic_long_read(&kgsl_driver.stats.coherent);
	else if (!strcmp(attr->attr.name, "coherent_max"))
		val = atomic_long_read(&kgsl_driver.stats.coherent_max);
	else if (!strcmp(attr->attr.name, "secure"))
		val = atomic_long_read(&kgsl_driver.stats.secure);
	else if (!strcmp(attr->attr.name, "secure_max"))
		val = atomic_long_read(&kgsl_driver.stats.secure_max);
	else if (!strcmp(attr->attr.name, "mapped"))
		val = atomic_long_read(&kgsl_driver.stats.mapped);
	else if (!strcmp(attr->attr.name, "mapped_max"))
		val = atomic_long_read(&kgsl_driver.stats.mapped_max);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", val);
}

static ssize_t full_cache_threshold_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	unsigned int thresh = 0;

	ret = kgsl_sysfs_store(buf, &thresh);
	if (ret)
		return ret;

	kgsl_driver.full_cache_threshold = thresh;
	return count;
}

static ssize_t full_cache_threshold_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			kgsl_driver.full_cache_threshold);
}

static DEVICE_ATTR(vmalloc, 0444, memstat_show, NULL);
static DEVICE_ATTR(vmalloc_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(page_alloc, 0444, memstat_show, NULL);
static DEVICE_ATTR(page_alloc_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(coherent, 0444, memstat_show, NULL);
static DEVICE_ATTR(coherent_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(secure, 0444, memstat_show, NULL);
static DEVICE_ATTR(secure_max, 0444, memstat_show, NULL);
static DEVICE_ATTR(mapped, 0444, memstat_show, NULL);
static DEVICE_ATTR(mapped_max, 0444, memstat_show, NULL);
static DEVICE_ATTR_RW(full_cache_threshold);

static const struct attribute *drv_attr_list[] = {
	&dev_attr_vmalloc.attr,
	&dev_attr_vmalloc_max.attr,
	&dev_attr_page_alloc.attr,
	&dev_attr_page_alloc_max.attr,
	&dev_attr_coherent.attr,
	&dev_attr_coherent_max.attr,
	&dev_attr_secure.attr,
	&dev_attr_secure_max.attr,
	&dev_attr_mapped.attr,
	&dev_attr_mapped_max.attr,
	&dev_attr_full_cache_threshold.attr,
	NULL,
};

void
kgsl_sharedmem_uninit_sysfs(void)
{
	sysfs_remove_files(&kgsl_driver.virtdev.kobj, drv_attr_list);
}

int
kgsl_sharedmem_init_sysfs(void)
{
	return sysfs_create_files(&kgsl_driver.virtdev.kobj, drv_attr_list);
}

static vm_fault_t kgsl_paged_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	int pgoff;
	unsigned int offset = vmf->address - vma->vm_start;

	if (offset >= memdesc->size)
		return VM_FAULT_SIGBUS;

	pgoff = offset >> PAGE_SHIFT;

	return vmf_insert_page(vma, vmf->address, memdesc->pages[pgoff]);
}

static void kgsl_paged_unmap_kernel(struct kgsl_memdesc *memdesc)
{
	mutex_lock(&kernel_map_global_lock);
	if (!memdesc->hostptr) {
		/* If already unmapped the refcount should be 0 */
		WARN_ON(memdesc->hostptr_count);
		goto done;
	}
	memdesc->hostptr_count--;
	if (memdesc->hostptr_count)
		goto done;
	vunmap(memdesc->hostptr);

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.vmalloc);
	memdesc->hostptr = NULL;
done:
	mutex_unlock(&kernel_map_global_lock);
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)

#include <soc/qcom/secure_buffer.h>

static int lock_sgt(struct sg_table *sgt, u64 size)
{
	struct scatterlist *sg;
	int dest_perms = PERM_READ | PERM_WRITE;
	int source_vm = VMID_HLOS;
	int dest_vm = VMID_CP_PIXEL;
	int ret;
	int i;

	do {
		ret = hyp_assign_table(sgt, &source_vm, 1, &dest_vm,
					&dest_perms, 1);
	} while (ret == -EAGAIN);

	if (ret) {
		/*
		 * If returned error code is EADDRNOTAVAIL, then this
		 * memory may no longer be in a usable state as security
		 * state of the pages is unknown after this failure. This
		 * memory can neither be added back to the pool nor buddy
		 * system.
		 */
		if (ret == -EADDRNOTAVAIL)
			pr_err("Failure to lock secure GPU memory 0x%llx bytes will not be recoverable\n",
				size);

		return ret;
	}

	/* Set private bit for each sg to indicate that its secured */
	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		SetPagePrivate(sg_page(sg));

	return 0;
}

static int unlock_sgt(struct sg_table *sgt)
{
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int source_vm = VMID_CP_PIXEL;
	int dest_vm = VMID_HLOS;
	int ret;
	struct sg_page_iter sg_iter;

	do {
		ret = hyp_assign_table(sgt, &source_vm, 1, &dest_vm,
					&dest_perms, 1);
	} while (ret == -EAGAIN);

	if (ret)
		return ret;

	for_each_sg_page(sgt->sgl, &sg_iter, sgt->nents, 0)
		ClearPagePrivate(sg_page_iter_page(&sg_iter));
	return 0;
}
#endif

static int kgsl_paged_map_kernel(struct kgsl_memdesc *memdesc)
{
	int ret = 0;

	/* Sanity check - don't map more than we could possibly chew */
	if (memdesc->size > ULONG_MAX)
		return -ENOMEM;

	mutex_lock(&kernel_map_global_lock);
	if ((!memdesc->hostptr) && (memdesc->pages != NULL)) {
		pgprot_t page_prot = pgprot_writecombine(PAGE_KERNEL);

		memdesc->hostptr = vmap(memdesc->pages, memdesc->page_count,
					VM_IOREMAP, page_prot);
		if (memdesc->hostptr)
			KGSL_STATS_ADD(memdesc->size,
				&kgsl_driver.stats.vmalloc,
				&kgsl_driver.stats.vmalloc_max);
		else
			ret = -ENOMEM;
	}
	if (memdesc->hostptr)
		memdesc->hostptr_count++;

	mutex_unlock(&kernel_map_global_lock);

	return ret;
}

static vm_fault_t kgsl_contiguous_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	unsigned long offset, pfn;

	offset = ((unsigned long) vmf->address - vma->vm_start) >>
		PAGE_SHIFT;

	pfn = (memdesc->physaddr >> PAGE_SHIFT) + offset;
	return vmf_insert_pfn(vma, vmf->address, pfn);
}

static void _dma_cache_op(struct device *dev, struct page *page,
		unsigned int op)
{
	struct scatterlist sgl;

	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);
	sg_dma_address(&sgl) = page_to_phys(page);

	switch (op) {
	case KGSL_CACHE_OP_FLUSH:
		dma_sync_sg_for_device(dev, &sgl, 1, DMA_TO_DEVICE);
		dma_sync_sg_for_device(dev, &sgl, 1, DMA_FROM_DEVICE);
		break;
	case KGSL_CACHE_OP_CLEAN:
		dma_sync_sg_for_device(dev, &sgl, 1, DMA_TO_DEVICE);
		break;
	case KGSL_CACHE_OP_INV:
		dma_sync_sg_for_device(dev, &sgl, 1, DMA_FROM_DEVICE);
		break;
	}
}

int kgsl_cache_range_op(struct kgsl_memdesc *memdesc, uint64_t offset,
		uint64_t size, unsigned int op)
{
	struct sg_table *sgt = NULL;
	struct sg_page_iter sg_iter;

	if (memdesc->flags & KGSL_MEMFLAGS_IOCOHERENT)
		return 0;

	if (size == 0 || size > UINT_MAX)
		return -EINVAL;

	/* Make sure that the offset + size does not overflow */
	if ((offset + size < offset) || (offset + size < size))
		return -ERANGE;

	/* Check that offset+length does not exceed memdesc->size */
	if (offset + size > memdesc->size)
		return -ERANGE;

	if (memdesc->sgt != NULL)
		sgt = memdesc->sgt;
	else {
		if (memdesc->pages == NULL)
			return  0;

		sgt = kgsl_alloc_sgt_from_pages(memdesc);
		if (IS_ERR(sgt))
			return PTR_ERR(sgt);
	}

	size += offset & PAGE_MASK;
	offset &= ~PAGE_MASK;

	for_each_sg_page(sgt->sgl, &sg_iter, PAGE_ALIGN(size) >> PAGE_SHIFT,
			offset >> PAGE_SHIFT)
		_dma_cache_op(memdesc->dev, sg_page_iter_page(&sg_iter), op);

	if (memdesc->sgt == NULL)
		kgsl_free_sgt(sgt);

	return 0;
}

void kgsl_memdesc_init(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t flags)
{
	struct kgsl_mmu *mmu = &device->mmu;
	unsigned int align;

	memset(memdesc, 0, sizeof(*memdesc));
	/* Turn off SVM if the system doesn't support it */
	if (!kgsl_mmu_use_cpu_map(mmu))
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Secure memory disables advanced addressing modes */
	if (flags & KGSL_MEMFLAGS_SECURE)
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Disable IO coherence if it is not supported on the chip */
	if (!kgsl_mmu_has_feature(device, KGSL_MMU_IO_COHERENT)) {
		flags &= ~((uint64_t) KGSL_MEMFLAGS_IOCOHERENT);

		WARN_ONCE(IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT),
			"I/O coherency is not supported on this target\n");
	} else if (IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT))
		flags |= KGSL_MEMFLAGS_IOCOHERENT;

	/*
	 * We can't enable I/O coherency on uncached surfaces because of
	 * situations where hardware might snoop the cpu caches which can
	 * have stale data. This happens primarily due to the limitations
	 * of dma caching APIs available on arm64
	 */
	if (!kgsl_cachemode_is_cached(flags))
		flags &= ~((u64) KGSL_MEMFLAGS_IOCOHERENT);

	if (kgsl_mmu_has_feature(device, KGSL_MMU_NEED_GUARD_PAGE) ||
		(flags & KGSL_MEMFLAGS_GUARD_PAGE))
		memdesc->priv |= KGSL_MEMDESC_GUARD_PAGE;

	if (flags & KGSL_MEMFLAGS_SECURE)
		memdesc->priv |= KGSL_MEMDESC_SECURE;

	memdesc->flags = flags;
	memdesc->dev = &device->pdev->dev;

	align = max_t(unsigned int,
		(memdesc->flags & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT,
		ilog2(PAGE_SIZE));
	kgsl_memdesc_set_align(memdesc, align);

	spin_lock_init(&memdesc->lock);
}

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	if (!memdesc || !memdesc->size)
		return;

	/* Make sure the memory object has been unmapped */
	kgsl_mmu_put_gpuaddr(memdesc);

	/* Assume if no operations were specified something went bad early */
	if (!memdesc->ops || !memdesc->ops->free)
		return;

	memdesc->ops->free(memdesc);
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
void kgsl_free_secure_page(struct page *page)
{
	struct sg_table sgt;
	struct scatterlist sgl;

	if (!page)
		return;

	sgt.sgl = &sgl;
	sgt.nents = 1;
	sgt.orig_nents = 1;
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);

	unlock_sgt(&sgt);
	__free_page(page);
}

struct page *kgsl_alloc_secure_page(void)
{
	struct page *page;
	struct sg_table sgt;
	struct scatterlist sgl;
	int status;

	page = alloc_page(GFP_KERNEL | __GFP_ZERO |
			__GFP_NORETRY | __GFP_HIGHMEM);
	if (!page)
		return NULL;

	sgt.sgl = &sgl;
	sgt.nents = 1;
	sgt.orig_nents = 1;
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, page, PAGE_SIZE, 0);

	status = lock_sgt(&sgt, PAGE_SIZE);
	if (status) {
		if (status == -EADDRNOTAVAIL)
			return NULL;

		__free_page(page);
		return NULL;
	}
	return page;
}
#else
void kgsl_free_secure_page(struct page *page)
{
}

struct page *kgsl_alloc_secure_page(void)
{
	return NULL;
}
#endif

int
kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			uint64_t offsetbytes)
{
	uint32_t *src;

	if (WARN_ON(memdesc == NULL || memdesc->hostptr == NULL ||
		dst == NULL))
		return -EINVAL;

	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes > (memdesc->size - sizeof(uint32_t)));
	if (offsetbytes > (memdesc->size - sizeof(uint32_t)))
		return -ERANGE;

	/*
	 * We are reading shared memory between CPU and GPU.
	 * Make sure reads before this are complete
	 */
	rmb();
	src = (uint32_t *)(memdesc->hostptr + offsetbytes);
	*dst = *src;
	return 0;
}

void
kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint32_t src)
{
	/* Quietly return if the memdesc isn't valid */
	if (IS_ERR_OR_NULL(memdesc) || WARN_ON(!memdesc->hostptr))
		return;

	if (WARN_ON(!IS_ALIGNED(offsetbytes, sizeof(u32))))
		return;

	if (WARN_ON(offsetbytes > (memdesc->size - sizeof(u32))))
		return;

	*((u32 *) (memdesc->hostptr + offsetbytes)) = src;

	/* Make sure the write is posted before continuing */
	wmb();
}

int
kgsl_sharedmem_readq(const struct kgsl_memdesc *memdesc,
			uint64_t *dst,
			uint64_t offsetbytes)
{
	uint64_t *src;

	if (WARN_ON(memdesc == NULL || memdesc->hostptr == NULL ||
		dst == NULL))
		return -EINVAL;

	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes > (memdesc->size - sizeof(uint32_t)));
	if (offsetbytes > (memdesc->size - sizeof(uint32_t)))
		return -ERANGE;

	/*
	 * We are reading shared memory between CPU and GPU.
	 * Make sure reads before this are complete
	 */
	rmb();
	src = (uint64_t *)(memdesc->hostptr + offsetbytes);
	*dst = *src;
	return 0;
}

void
kgsl_sharedmem_writeq(const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint64_t src)
{
	/* Quietly return if the memdesc isn't valid */
	if (IS_ERR_OR_NULL(memdesc) || WARN_ON(!memdesc->hostptr))
		return;

	if (WARN_ON(!IS_ALIGNED(offsetbytes, sizeof(u64))))
		return;

	if (WARN_ON(offsetbytes > (memdesc->size - sizeof(u64))))
		return;

	*((u64 *) (memdesc->hostptr + offsetbytes)) = src;

	/* Make sure the write is posted before continuing */
	wmb();
}

void kgsl_get_memory_usage(char *name, size_t name_size, uint64_t memflags)
{
	unsigned int type = MEMFLAGS(memflags, KGSL_MEMTYPE_MASK,
		KGSL_MEMTYPE_SHIFT);
	int i;

	for (i = 0; i < ARRAY_SIZE(memtypes); i++) {
		if (memtypes[i].type == type) {
			strlcpy(name, memtypes[i].attr.name, name_size);
			return;
		}
	}

	snprintf(name, name_size, "VK/others(%3d)", type);
}

int kgsl_memdesc_sg_dma(struct kgsl_memdesc *memdesc,
		phys_addr_t addr, u64 size)
{
	int ret;
	struct page *page = phys_to_page(addr);

	memdesc->sgt = kmalloc(sizeof(*memdesc->sgt), GFP_KERNEL);
	if (memdesc->sgt == NULL)
		return -ENOMEM;

	ret = sg_alloc_table(memdesc->sgt, 1, GFP_KERNEL);
	if (ret) {
		kfree(memdesc->sgt);
		memdesc->sgt = NULL;
		return ret;
	}

	sg_set_page(memdesc->sgt->sgl, page, (size_t) size, 0);
	return 0;
}

static void _kgsl_contiguous_free(struct kgsl_memdesc *memdesc)
{
	dma_free_attrs(memdesc->dev, memdesc->size,
			memdesc->hostptr, memdesc->physaddr,
			memdesc->attrs);

	sg_free_table(memdesc->sgt);
	kfree(memdesc->sgt);

	memdesc->sgt = NULL;
}

static void kgsl_contiguous_free(struct kgsl_memdesc *memdesc)
{
	if (!memdesc->hostptr)
		return;

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.coherent);

#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
	mod_node_page_state(page_pgdat(phys_to_page(memdesc->physaddr)),
		NR_UNRECLAIMABLE_PAGES, -(memdesc->size >> PAGE_SHIFT));
#endif

	_kgsl_contiguous_free(memdesc);
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static void kgsl_free_secure_system_pages(struct kgsl_memdesc *memdesc)
{
	int i;
	struct scatterlist *sg;
	int ret = unlock_sgt(memdesc->sgt);
	int order = get_order(PAGE_SIZE);

	if (ret) {
		/*
		 * Unlock of the secure buffer failed. This buffer will
		 * be stuck in secure side forever and is unrecoverable.
		 * Give up on the buffer and don't return it to the
		 * pool.
		 */
		pr_err("kgsl: secure buf unlock failed: gpuaddr: %llx size: %llx ret: %d\n",
			memdesc->gpuaddr, memdesc->size, ret);
		return;
	}

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.secure);

	for_each_sg(memdesc->sgt->sgl, sg, memdesc->sgt->nents, i) {
		struct page *page = sg_page(sg);

		__free_pages(page, order);
#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
		mod_node_page_state(page_pgdat(page), NR_UNRECLAIMABLE_PAGES,
				-(1 << order));
#endif
	}

	sg_free_table(memdesc->sgt);
	kfree(memdesc->sgt);

	memdesc->sgt = NULL;
}

static void kgsl_free_secure_pool_pages(struct kgsl_memdesc *memdesc)
{
	int ret = unlock_sgt(memdesc->sgt);

	if (ret) {
		/*
		 * Unlock of the secure buffer failed. This buffer will
		 * be stuck in secure side forever and is unrecoverable.
		 * Give up on the buffer and don't return it to the
		 * pool.
		 */
		pr_err("kgsl: secure buf unlock failed: gpuaddr: %llx size: %llx ret: %d\n",
			memdesc->gpuaddr, memdesc->size, ret);
		return;
	}

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.secure);

	kgsl_pool_free_sgt(memdesc->sgt);

	sg_free_table(memdesc->sgt);
	kfree(memdesc->sgt);

	memdesc->sgt = NULL;
}
#endif

static void kgsl_free_pool_pages(struct kgsl_memdesc *memdesc)
{
	kgsl_paged_unmap_kernel(memdesc);
	WARN_ON(memdesc->hostptr);

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.page_alloc);

	kgsl_pool_free_pages(memdesc->pages, memdesc->page_count);

	memdesc->page_count = 0;
	kvfree(memdesc->pages);

	memdesc->pages = NULL;
}

static void kgsl_free_system_pages(struct kgsl_memdesc *memdesc)
{
	int i, order = get_order(PAGE_SIZE);

	kgsl_paged_unmap_kernel(memdesc);
	WARN_ON(memdesc->hostptr);

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.page_alloc);

	for (i = 0; i < memdesc->page_count; i++) {
		__free_pages(memdesc->pages[i], order);
#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
		mod_node_page_state(page_pgdat(memdesc->pages[i]),
				NR_UNRECLAIMABLE_PAGES, -(1 << order));
#endif
	}

	memdesc->page_count = 0;
	kvfree(memdesc->pages);
	memdesc->pages = NULL;
}

static const struct kgsl_memdesc_ops kgsl_contiguous_ops = {
	.free = kgsl_contiguous_free,
	.vmflags = VM_DONTDUMP | VM_PFNMAP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_contiguous_vmfault,
};

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static const struct kgsl_memdesc_ops kgsl_secure_system_ops = {
	.free = kgsl_free_secure_system_pages,
	/* FIXME: Make sure vmflags / vmfault does the right thing here */
};

static const struct kgsl_memdesc_ops kgsl_secure_pool_ops = {
	.free = kgsl_free_secure_pool_pages,
	/* FIXME: Make sure vmflags / vmfault does the right thing here */
};
#endif

static const struct kgsl_memdesc_ops kgsl_pool_ops = {
	.free = kgsl_free_pool_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY | VM_MIXEDMAP,
	.vmfault = kgsl_paged_vmfault,
	.map_kernel = kgsl_paged_map_kernel,
	.unmap_kernel = kgsl_paged_unmap_kernel,
};

static const struct kgsl_memdesc_ops kgsl_system_ops = {
	.free = kgsl_free_system_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY | VM_MIXEDMAP,
	.vmfault = kgsl_paged_vmfault,
	.map_kernel = kgsl_paged_map_kernel,
	.unmap_kernel = kgsl_paged_unmap_kernel,
};

static int kgsl_system_alloc_pages(u64 size, struct page ***pages,
		struct device *dev)
{
	struct scatterlist sg;
	struct page **local;
	int i, npages = size >> PAGE_SHIFT;
	int order = get_order(PAGE_SIZE);

	local = kvcalloc(npages, sizeof(*pages), GFP_KERNEL);
	if (!local)
		return -ENOMEM;

	for (i = 0; i < npages; i++) {
		gfp_t gfp = __GFP_ZERO | __GFP_HIGHMEM |
			GFP_KERNEL | __GFP_NORETRY;

		local[i] = alloc_pages(gfp, get_order(PAGE_SIZE));
		if (!local[i]) {
			for (i = i - 1; i >= 0; i--) {
#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
				mod_node_page_state(page_pgdat(local[i]),
					NR_UNRECLAIMABLE_PAGES, -(1 << order));
#endif
				__free_pages(local[i], order);
			}
			kvfree(local);
			return -ENOMEM;
		}

		/* Make sure the cache is clean */
		sg_init_table(&sg, 1);
		sg_set_page(&sg, local[i], PAGE_SIZE, 0);
		sg_dma_address(&sg) = page_to_phys(local[i]);

		dma_sync_sg_for_device(dev, &sg, 1, DMA_BIDIRECTIONAL);
#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
		mod_node_page_state(page_pgdat(local[i]), NR_UNRECLAIMABLE_PAGES,
				(1 << order));
#endif
	}

	*pages = local;
	return npages;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static int kgsl_alloc_secure_pages(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	struct page **pages;
	int count;
	struct sg_table *sgt;
	int ret;

	size = PAGE_ALIGN(size);

	if (!size || size > UINT_MAX)
		return -EINVAL;

	kgsl_memdesc_init(device, memdesc, flags);
	memdesc->priv |= priv;

	if (priv & KGSL_MEMDESC_SYSMEM) {
		memdesc->ops = &kgsl_secure_system_ops;
		count = kgsl_system_alloc_pages(size, &pages, device->dev);
	} else {
		memdesc->ops = &kgsl_secure_pool_ops;
		count = kgsl_pool_alloc_pages(size, &pages, device->dev);
	}

	if (count < 0)
		return count;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		kgsl_pool_free_pages(pages, count);
		kvfree(pages);
		return -ENOMEM;
	}

	ret = sg_alloc_table_from_pages(sgt, pages, count, 0, size, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		kgsl_pool_free_pages(pages, count);
		kvfree(pages);
		return ret;
	}

	/* Now that we've moved to a sg table don't need the pages anymore */
	kvfree(pages);

	ret = lock_sgt(sgt, size);
	if (ret) {
		if (ret != -EADDRNOTAVAIL)
			kgsl_pool_free_sgt(sgt);
		sg_free_table(sgt);
		kfree(sgt);
		return ret;
	}

	memdesc->sgt = sgt;
	memdesc->size = size;

	KGSL_STATS_ADD(size, &kgsl_driver.stats.secure,
		&kgsl_driver.stats.secure_max);

	return 0;
}
#endif

static int kgsl_alloc_pages(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	struct page **pages;
	int count;

	size = PAGE_ALIGN(size);

	if (!size || size > UINT_MAX)
		return -EINVAL;

	kgsl_memdesc_init(device, memdesc, flags);
	memdesc->priv |= priv;

	if (priv & KGSL_MEMDESC_SYSMEM) {
		memdesc->ops = &kgsl_system_ops;
		count = kgsl_system_alloc_pages(size, &pages, device->dev);
	} else {
		memdesc->ops = &kgsl_pool_ops;
		count = kgsl_pool_alloc_pages(size, &pages, device->dev);
	}

	if (count < 0)
		return count;

	memdesc->pages = pages;
	memdesc->size = size;
	memdesc->page_count = count;

	KGSL_STATS_ADD(size, &kgsl_driver.stats.page_alloc,
		&kgsl_driver.stats.page_alloc_max);

	return 0;
}

static int _kgsl_alloc_contiguous(struct device *dev,
		struct kgsl_memdesc *memdesc, u64 size, unsigned long attrs)
{
	int ret;
	phys_addr_t phys;
	void *ptr;

	ptr = dma_alloc_attrs(dev, (size_t) size, &phys,
		GFP_KERNEL, attrs);
	if (!ptr)
		return -ENOMEM;

	memdesc->size = size;
	memdesc->dev = dev;
	memdesc->hostptr = ptr;
	memdesc->physaddr = phys;
	memdesc->gpuaddr = phys;
	memdesc->attrs = attrs;

	ret = kgsl_memdesc_sg_dma(memdesc, phys, size);
	if (ret)
		dma_free_attrs(dev, (size_t) size, ptr, phys, attrs);

	return ret;
}

static int kgsl_alloc_contiguous(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	int ret;

	size = PAGE_ALIGN(size);

	if (!size || size > UINT_MAX)
		return -EINVAL;

	kgsl_memdesc_init(device, memdesc, flags);
	memdesc->priv |= priv;

	memdesc->ops = &kgsl_contiguous_ops;
	ret = _kgsl_alloc_contiguous(&device->pdev->dev, memdesc, size, 0);

	if (!ret) {
		KGSL_STATS_ADD(size, &kgsl_driver.stats.coherent,
			&kgsl_driver.stats.coherent_max);
#ifdef CONFIG_MM_STAT_UNRECLAIMABLE_PAGES
		mod_node_page_state(page_pgdat(phys_to_page(memdesc->physaddr)),
			NR_UNRECLAIMABLE_PAGES, (size >> PAGE_SHIFT));
#endif
	}

	return ret;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static int kgsl_allocate_secure(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	return kgsl_alloc_secure_pages(device, memdesc, size, flags, priv);
}
#else
static int kgsl_allocate_secure(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	return -ENODEV;
}
#endif

int kgsl_allocate_user(struct kgsl_device *device, struct kgsl_memdesc *memdesc,
		u64 size, u64 flags, u32 priv)
{
	if (device->mmu.type == KGSL_MMU_TYPE_NONE)
		return kgsl_alloc_contiguous(device, memdesc, size, flags,
			priv);
	else if (flags & KGSL_MEMFLAGS_SECURE)
		return kgsl_allocate_secure(device, memdesc, size, flags, priv);

	return kgsl_alloc_pages(device, memdesc, size, flags, priv);
}

int kgsl_allocate_kernel(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv)
{
	int ret;

	ret = kgsl_allocate_user(device, memdesc, size, flags, priv);
	if (ret)
		return ret;

	if (memdesc->ops->map_kernel) {
		ret = memdesc->ops->map_kernel(memdesc);
		if (ret) {
			kgsl_sharedmem_free(memdesc);
			return ret;
		}
	}

	return 0;
}

struct kgsl_memdesc *kgsl_allocate_global_fixed(struct kgsl_device *device,
		const char *resource, const char *name)
{
	struct kgsl_global_memdesc *md;
	u32 entry[2];
	int ret;

	if (of_property_read_u32_array(device->pdev->dev.of_node,
		resource, entry, 2))
		return ERR_PTR(-ENODEV);

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return ERR_PTR(-ENOMEM);

	kgsl_memdesc_init(device, &md->memdesc, 0);
	md->memdesc.priv = KGSL_MEMDESC_GLOBAL;
	md->memdesc.physaddr = entry[0];
	md->memdesc.size = entry[1];

	ret = kgsl_memdesc_sg_dma(&md->memdesc, entry[0], entry[1]);
	if (ret) {
		kfree(md);
		return ERR_PTR(ret);
	}

	md->name = name;

	/*
	 * No lock here, because this function is only called during probe/init
	 * while the caller is holding the mutex
	 */
	list_add_tail(&md->node, &device->globals);

	kgsl_mmu_map_global(device, &md->memdesc, 0);

	return &md->memdesc;
}

static struct kgsl_memdesc *
kgsl_allocate_secure_global(struct kgsl_device *device,
		u64 size, u64 flags, u32 priv, const char *name)
{
	struct kgsl_global_memdesc *md;
	int ret;

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return ERR_PTR(-ENOMEM);

	/* Make sure that we get global memory from system memory */
	priv |= KGSL_MEMDESC_GLOBAL | KGSL_MEMDESC_SYSMEM;

	ret = kgsl_allocate_secure(device, &md->memdesc, size, flags, priv);
	if (ret) {
		kfree(md);
		return ERR_PTR(ret);
	}

	md->name = name;

	/*
	 * No lock here, because this function is only called during probe/init
	 * while the caller is holding the mutex
	 */
	list_add_tail(&md->node, &device->globals);

	/*
	 * No offset needed, we'll get an address inside of the pagetable
	 * normally
	 */
	kgsl_mmu_map_global(device, &md->memdesc, 0);
	kgsl_trace_gpu_mem_total(device, md->memdesc.size);

	return &md->memdesc;
}

struct kgsl_memdesc *kgsl_allocate_global(struct kgsl_device *device,
		u64 size, u32 padding, u64 flags, u32 priv, const char *name)
{
	int ret;
	struct kgsl_global_memdesc *md;

	if (flags & KGSL_MEMFLAGS_SECURE)
		return kgsl_allocate_secure_global(device, size, flags, priv,
			name);

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return ERR_PTR(-ENOMEM);

	/*
	 * Make sure that we get global memory from system memory to keep from
	 * taking up pool memory for the life of the driver
	 */
	priv |= KGSL_MEMDESC_GLOBAL | KGSL_MEMDESC_SYSMEM;

	ret = kgsl_allocate_kernel(device, &md->memdesc, size, flags, priv);
	if (ret) {
		kfree(md);
		return ERR_PTR(ret);
	}

	md->name = name;

	/*
	 * No lock here, because this function is only called during probe/init
	 * while the caller is holding the mute
	 */
	list_add_tail(&md->node, &device->globals);

	kgsl_mmu_map_global(device, &md->memdesc, padding);
	kgsl_trace_gpu_mem_total(device, md->memdesc.size);

	return &md->memdesc;
}

void kgsl_free_globals(struct kgsl_device *device)
{
	struct kgsl_global_memdesc *md, *tmp;

	list_for_each_entry_safe(md, tmp, &device->globals, node) {
		kgsl_sharedmem_free(&md->memdesc);
		list_del(&md->node);
		kfree(md);
	}
}

void kgsl_sharedmem_set_noretry(bool val)
{
	sharedmem_noretry_flag = val;
}

bool kgsl_sharedmem_get_noretry(void)
{
	return sharedmem_noretry_flag;
}
