// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/shmem_fs.h>
#include <linux/bitfield.h>

#include "kgsl_reclaim.h"
#include "kgsl_sharedmem.h"

/*
 * The user can set this from debugfs to force failed memory allocations to
 * fail without trying OOM first.  This is a debug setting useful for
 * stress applications that want to test failure cases without pushing the
 * system into unrecoverable OOM panics
 */

static bool sharedmem_noretry_flag;

static DEFINE_MUTEX(kernel_map_global_lock);

struct cp2_mem_chunks {
	unsigned int chunk_list;
	unsigned int chunk_list_size;
	unsigned int chunk_size;
} __attribute__ ((__packed__));

struct cp2_lock_req {
	struct cp2_mem_chunks chunks;
	unsigned int mem_usage;
	unsigned int lock;
} __attribute__ ((__packed__));

#define MEM_PROTECT_LOCK_ID2		0x0A
#define MEM_PROTECT_LOCK_ID2_FLAT	0x11

int kgsl_allocate_global(struct kgsl_device *device,
	struct kgsl_memdesc *memdesc, uint64_t size, uint64_t flags,
	unsigned int priv, const char *name)
{
	int ret;

	kgsl_memdesc_init(device, memdesc, flags);
	memdesc->priv |= priv;

	if (((memdesc->priv & KGSL_MEMDESC_CONTIG) != 0) ||
		(kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_NONE))
		ret = kgsl_sharedmem_alloc_contig(device, memdesc,
						(size_t) size);
	else {
		ret = kgsl_sharedmem_page_alloc_user(memdesc, (size_t) size);
		if (ret == 0) {
			if (kgsl_memdesc_map(memdesc) == NULL) {
				kgsl_sharedmem_free(memdesc);
				ret = -ENOMEM;
			}
		}
	}

	if (ret == 0)
		kgsl_mmu_add_global(device, memdesc, name);

	return ret;
}

void kgsl_free_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc)
{
	kgsl_mmu_remove_global(device, memdesc);
	kgsl_sharedmem_free(memdesc);
}

/* An attribute for showing per-process memory statistics */
struct kgsl_mem_entry_attribute {
	struct kgsl_process_attribute attr;
	int memtype;
	ssize_t (*show)(struct kgsl_process_private *priv,
		int type, char *buf);
};

static inline struct kgsl_process_attribute *to_process_attr(
		struct attribute *attr)
{
	return container_of(attr, struct kgsl_process_attribute, attr);
}

#define to_mem_entry_attr(a) \
container_of(a, struct kgsl_mem_entry_attribute, attr)

#define __MEM_ENTRY_ATTR(_type, _name, _show) \
{ \
	.attr = __ATTR(_name, 0444, mem_entry_sysfs_show, NULL), \
	.memtype = _type, \
	.show = _show, \
}

static ssize_t mem_entry_sysfs_show(struct kobject *kobj,
	struct kgsl_process_attribute *attr, char *buf)
{
	struct kgsl_mem_entry_attribute *pattr = to_mem_entry_attr(attr);
	struct kgsl_process_private *priv =
		container_of(kobj, struct kgsl_process_private, kobj);

	return pattr->show(priv, pattr->memtype, buf);
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

static void kgsl_cma_unlock_secure(struct kgsl_memdesc *memdesc);

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

		kgsl_mem_entry_put(entry);
		spin_lock(&priv->mem_lock);
	}
	spin_unlock(&priv->mem_lock);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", imported_mem);
}

static ssize_t
gpumem_mapped_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%ld\n",
			atomic_long_read(&priv->gpumem_mapped));
}

static ssize_t
gpumem_unmapped_show(struct kgsl_process_private *priv, int type, char *buf)
{
	u64 gpumem_total = atomic_long_read(&priv->stats[type].cur);
	u64 gpumem_mapped = atomic_long_read(&priv->gpumem_mapped);

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
	return scnprintf(buf, PAGE_SIZE, "%ld\n",
			atomic_long_read(&priv->stats[type].cur));
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

static ssize_t process_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_process_attribute *pattr = to_process_attr(attr);

	return pattr->show(kobj, pattr, buf);
}

static ssize_t process_sysfs_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t count)
{
	struct kgsl_process_attribute *pattr = to_process_attr(attr);

	if (pattr->store)
		return pattr->store(kobj, pattr, buf, count);
	return -EIO;
}

/* Dummy release function - we have nothing to do here */
static void process_sysfs_release(struct kobject *kobj)
{
}

static const struct sysfs_ops process_sysfs_ops = {
	.show = process_sysfs_show,
	.store = process_sysfs_store,
};

static struct kobj_type process_ktype = {
	.sysfs_ops = &process_sysfs_ops,
	.release = &process_sysfs_release,
};

static struct mem_entry_stats mem_stats[] = {
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_KERNEL, kernel),
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_USER, user),
#ifdef CONFIG_ION
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_ION, ion),
#endif
};

static struct device_attribute dev_attr_max_reclaim_limit = {
	.attr = { .name = "max_reclaim_limit", .mode = 0644 },
	.show = kgsl_proc_max_reclaim_limit_show,
	.store = kgsl_proc_max_reclaim_limit_store,
};

void
kgsl_process_uninit_sysfs(struct kgsl_process_private *private)
{
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

	if (kobject_init_and_add(&private->kobj, &process_ktype,
		kgsl_driver.prockobj, "%d", pid_nr(private->pid))) {
		dev_err(device->dev, "Unable to add sysfs for process %d\n",
			pid_nr(private->pid));
		return;
	}

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		int ret;

		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].attr.attr.attr);
		ret |= sysfs_create_file(&private->kobj,
			&mem_stats[i].max_attr.attr.attr);

		if (ret)
			dev_err(device->dev,
				"Unable to create sysfs files for process %d\n",
				pid_nr(private->pid));
	}

	for (i = 0; i < ARRAY_SIZE(debug_memstats); i++) {
		if (sysfs_create_file(&private->kobj,
			&debug_memstats[i].attr.attr))
			WARN(1, "Couldn't create sysfs file '%s'\n",
				debug_memstats[i].attr.attr.name);
	}

	kgsl_reclaim_proc_sysfs_init(private);
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
	&dev_attr_max_reclaim_limit.attr,
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

static int kgsl_cma_alloc_secure(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t size);

static int kgsl_allocate_secure(struct kgsl_device *device,
				struct kgsl_memdesc *memdesc,
				uint64_t size)
{
	int ret;

	if (MMU_FEATURE(&device->mmu, KGSL_MMU_HYP_SECURE_ALLOC))
		ret = kgsl_sharedmem_page_alloc_user(memdesc, size);
	else
		ret = kgsl_cma_alloc_secure(device, memdesc, size);

	return ret;
}

int kgsl_allocate_user(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc,
		uint64_t size, uint64_t flags)
{
	int ret;

	kgsl_memdesc_init(device, memdesc, flags);

	if (kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_NONE)
		ret = kgsl_sharedmem_alloc_contig(device, memdesc, size);
	else if (flags & KGSL_MEMFLAGS_SECURE)
		ret = kgsl_allocate_secure(device, memdesc, size);
	else
		ret = kgsl_sharedmem_page_alloc_user(memdesc, size);

	return ret;
}

static int kgsl_page_alloc_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	int pgoff;
	unsigned int offset;
	struct page *page;
	struct kgsl_process_private *priv =
		((struct kgsl_mem_entry *)vma->vm_private_data)->priv;

	offset = vmf->address - vma->vm_start;

	if (offset >= memdesc->size)
		return VM_FAULT_SIGBUS;

	pgoff = offset >> PAGE_SHIFT;

	spin_lock(&memdesc->lock);
	if (memdesc->pages[pgoff]) {
		page = memdesc->pages[pgoff];
		get_page(page);
	}
	else {
		/* We are here because page was reclaimed */
		spin_unlock(&memdesc->lock);

		page = shmem_read_mapping_page_gfp(
			memdesc->shmem_filp->f_mapping, pgoff,
			kgsl_gfp_mask(0));
		if (IS_ERR(page))
			return VM_FAULT_SIGBUS;
		kgsl_flush_page(page);

		spin_lock(&memdesc->lock);
		/*
		 * Update the pages array only if the page was
		 * not already brought back.
		 */
		if (!memdesc->pages[pgoff]) {
			memdesc->pages[pgoff] = page;
			memdesc->reclaimed_page_count--;
			atomic_dec(&priv->reclaimed_page_count);
			get_page(page);
		}
	}
	spin_unlock(&memdesc->lock);

	vmf->page = page;

	return 0;
}

/*
 * kgsl_page_alloc_unmap_kernel() - Unmap the memory in memdesc
 *
 * @memdesc: The memory descriptor which contains information about the memory
 *
 * Unmaps the memory mapped into kernel address space
 */
static void kgsl_page_alloc_unmap_kernel(struct kgsl_memdesc *memdesc)
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

static int kgsl_lock_sgt(struct sg_table *sgt, u64 size)
{
	int dest_perms = PERM_READ | PERM_WRITE;
	int source_vm = VMID_HLOS;
	int dest_vm = VMID_CP_PIXEL;
	int ret;

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

	return 0;
}

static int kgsl_unlock_sgt(struct sg_table *sgt)
{
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int source_vm = VMID_CP_PIXEL;
	int dest_vm = VMID_HLOS;
	int ret;

	do {
		ret = hyp_assign_table(sgt, &source_vm, 1, &dest_vm,
					&dest_perms, 1);
	} while (ret == -EAGAIN);

	if (ret)
		return ret;

	return 0;
}

static void kgsl_page_alloc_free(struct kgsl_memdesc *memdesc)
{
	kgsl_page_alloc_unmap_kernel(memdesc);
	/* we certainly do not expect the hostptr to still be mapped */
	BUG_ON(memdesc->hostptr);

	/* Secure buffers need to be unlocked before being freed */
	if (memdesc->priv & KGSL_MEMDESC_TZ_LOCKED) {
		int ret;

		ret = kgsl_unlock_sgt(memdesc->sgt);
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
	} else {
		atomic_long_sub(memdesc->size, &kgsl_driver.stats.page_alloc);
	}

	/* Free pages using the pages array for non secure paged memory */
	if (memdesc->pages != NULL)
		kgsl_free_pages(memdesc);
	else
		kgsl_free_pages_from_sgt(memdesc);

	if (memdesc->shmem_filp) {
		fput(memdesc->shmem_filp);
		memdesc->shmem_filp = NULL;
	}
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

static int kgsl_contiguous_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	unsigned long offset, pfn;
	int ret;

	offset = ((unsigned long) vmf->address - vma->vm_start) >>
		PAGE_SHIFT;

	pfn = (memdesc->physaddr >> PAGE_SHIFT) + offset;
	ret = vm_insert_pfn(vma, (unsigned long) vmf->address, pfn);

	if (ret == -ENOMEM || ret == -EAGAIN)
		return VM_FAULT_OOM;
	else if (ret == -EFAULT)
		return VM_FAULT_SIGBUS;

	return VM_FAULT_NOPAGE;
}

static void kgsl_cma_coherent_free(struct kgsl_memdesc *memdesc)
{
	unsigned long attrs = 0;

	if (memdesc->hostptr) {
		if (memdesc->priv & KGSL_MEMDESC_SECURE) {
			atomic_long_sub(memdesc->size,
				&kgsl_driver.stats.secure);

			kgsl_cma_unlock_secure(memdesc);
			attrs = memdesc->attrs;
		} else
			atomic_long_sub(memdesc->size,
				&kgsl_driver.stats.coherent);

		mod_node_page_state(page_pgdat(phys_to_page(memdesc->physaddr)),
			NR_UNRECLAIMABLE_PAGES, -(memdesc->size >> PAGE_SHIFT));

		dma_free_attrs(memdesc->dev, (size_t) memdesc->size,
			memdesc->hostptr, memdesc->physaddr, attrs);
	}
}

/* Global */
static struct kgsl_memdesc_ops kgsl_page_alloc_ops = {
	.free = kgsl_page_alloc_free,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_page_alloc_vmfault,
	.map_kernel = kgsl_page_alloc_map_kernel,
	.unmap_kernel = kgsl_page_alloc_unmap_kernel,
};

/* CMA ops - used during NOMMU mode */
static struct kgsl_memdesc_ops kgsl_cma_ops = {
	.free = kgsl_cma_coherent_free,
	.vmflags = VM_DONTDUMP | VM_PFNMAP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_contiguous_vmfault,
};

#ifdef CONFIG_ARM64
/*
 * For security reasons, ARMv8 doesn't allow invalidate only on read-only
 * mapping. It would be performance prohibitive to read the permissions on
 * the buffer before the operation. Every use case that we have found does not
 * assume that an invalidate operation is invalidate only, so we feel
 * comfortable turning invalidates into flushes for these targets
 */
static inline unsigned int _fixup_cache_range_op(unsigned int op)
{
	if (op == KGSL_CACHE_OP_INV)
		return KGSL_CACHE_OP_FLUSH;
	return op;
}
#else
static inline unsigned int _fixup_cache_range_op(unsigned int op)
{
	return op;
}
#endif

static inline void _cache_op(unsigned int op,
			const void *start, const void *end)
{
	/*
	 * The dmac_xxx_range functions handle addresses and sizes that
	 * are not aligned to the cacheline size correctly.
	 */
	switch (_fixup_cache_range_op(op)) {
	case KGSL_CACHE_OP_FLUSH:
		dmac_flush_range(start, end);
		break;
	case KGSL_CACHE_OP_CLEAN:
		dmac_clean_range(start, end);
		break;
	case KGSL_CACHE_OP_INV:
		dmac_inv_range(start, end);
		break;
	}
}

static int kgsl_do_cache_op(struct page *page, void *addr,
		uint64_t offset, uint64_t size, unsigned int op)
{
	if (page != NULL) {
		unsigned long pfn = page_to_pfn(page) + offset / PAGE_SIZE;
		/*
		 *  page_address() returns the kernel virtual address of page.
		 *  For high memory kernel virtual address exists only if page
		 *  has been mapped. So use a version of kmap rather than
		 *  page_address() for high memory.
		 */
		if (PageHighMem(page)) {
			offset &= ~PAGE_MASK;

			do {
				unsigned int len = size;

				if (len + offset > PAGE_SIZE)
					len = PAGE_SIZE - offset;

				page = pfn_to_page(pfn++);
				addr = kmap_atomic(page);
				_cache_op(op, addr + offset,
						addr + offset + len);
				kunmap_atomic(addr);

				size -= len;
				offset = 0;
			} while (size);

			return 0;
		}

		addr = page_address(page);
	}

	_cache_op(op, addr + offset, addr + offset + (size_t) size);
	return 0;
}

int kgsl_cache_range_op(struct kgsl_memdesc *memdesc, uint64_t offset,
		uint64_t size, unsigned int op)
{
	void *addr = NULL;
	struct sg_table *sgt = NULL;
	struct scatterlist *sg;
	unsigned int i, pos = 0;
	int ret = 0;

	if (size == 0 || size > UINT_MAX)
		return -EINVAL;

	/* Make sure that the offset + size does not overflow */
	if ((offset + size < offset) || (offset + size < size))
		return -ERANGE;

	/* Check that offset+length does not exceed memdesc->size */
	if (offset + size > memdesc->size)
		return -ERANGE;

	if (memdesc->hostptr) {
		addr = memdesc->hostptr;
		/* Make sure the offset + size do not overflow the address */
		if (addr + ((size_t) offset + (size_t) size) < addr)
			return -ERANGE;

		ret = kgsl_do_cache_op(NULL, addr, offset, size, op);
		return ret;
	}

	/*
	 * If the buffer is not to mapped to kernel, perform cache
	 * operations after mapping to kernel.
	 */
	if (memdesc->sgt != NULL)
		sgt = memdesc->sgt;
	else {
		if (memdesc->pages == NULL)
			return ret;

		sgt = kgsl_alloc_sgt_from_pages(memdesc);
		if (IS_ERR(sgt))
			return PTR_ERR(sgt);
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		uint64_t sg_offset, sg_left;

		if (offset >= (pos + sg->length)) {
			pos += sg->length;
			continue;
		}
		sg_offset = offset > pos ? offset - pos : 0;
		sg_left = (sg->length - sg_offset > size) ? size :
					sg->length - sg_offset;
		ret = kgsl_do_cache_op(sg_page(sg), NULL, sg_offset,
							sg_left, op);
		size -= sg_left;
		if (size == 0)
			break;
		pos += sg->length;
	}

	if (memdesc->sgt == NULL)
		kgsl_free_sgt(sgt);

	return ret;
}
EXPORT_SYMBOL(kgsl_cache_range_op);

void kgsl_memdesc_init(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t flags)
{
	struct kgsl_mmu *mmu = &device->mmu;
	unsigned int align;
	u32 cachemode;

	memset(memdesc, 0, sizeof(*memdesc));
	/* Turn off SVM if the system doesn't support it */
	if (!kgsl_mmu_use_cpu_map(mmu))
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Secure memory disables advanced addressing modes */
	if (flags & KGSL_MEMFLAGS_SECURE)
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Disable IO coherence if it is not supported on the chip */
	if (!MMU_FEATURE(mmu, KGSL_MMU_IO_COHERENT))
		flags &= ~((uint64_t) KGSL_MEMFLAGS_IOCOHERENT);

	/*
	 * We can't enable I/O coherency on uncached surfaces because of
	 * situations where hardware might snoop the cpu caches which can
	 * have stale data. This happens primarily due to the limitations
	 * of dma caching APIs available on arm64
	 */
	cachemode = FIELD_GET(KGSL_CACHEMODE_MASK, flags);
	if ((cachemode == KGSL_CACHEMODE_WRITECOMBINE ||
		cachemode == KGSL_CACHEMODE_UNCACHED))
		flags &= ~((u64) KGSL_MEMFLAGS_IOCOHERENT);

	if (MMU_FEATURE(mmu, KGSL_MMU_NEED_GUARD_PAGE))
		memdesc->priv |= KGSL_MEMDESC_GUARD_PAGE;

	if (flags & KGSL_MEMFLAGS_SECURE)
		memdesc->priv |= KGSL_MEMDESC_SECURE;

	if (device->flags & KGSL_FLAG_USE_SHMEM)
		memdesc->priv |= KGSL_MEMDESC_USE_SHMEM;

	memdesc->flags = flags;
	memdesc->dev = device->dev->parent;

	align = max_t(unsigned int,
		(memdesc->flags & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT,
		ilog2(PAGE_SIZE));
	kgsl_memdesc_set_align(memdesc, align);
	spin_lock_init(&memdesc->lock);
	spin_lock_init(&memdesc->gpuaddr_lock);
}

static int kgsl_shmem_alloc_page(struct page **pages,
			struct file *shmem_filp, unsigned int page_off)
{
	struct page *page;

	if (pages == NULL)
		return -EINVAL;

	page = shmem_read_mapping_page_gfp(shmem_filp->f_mapping, page_off,
			kgsl_gfp_mask(0));
	if (IS_ERR(page))
		return PTR_ERR(page);

	kgsl_zero_page(page, 0);

	*pages = page;

	return 1;
}

void kgsl_shmem_free_pages(struct kgsl_memdesc *memdesc)
{
	int i;

	for (i = 0; i < memdesc->page_count; i++)
		if (memdesc->pages[i])
			put_page(memdesc->pages[i]);
}

static int kgsl_memdesc_file_setup(struct kgsl_memdesc *memdesc, uint64_t size)
{
	int ret;

	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM) {
		memdesc->shmem_filp = shmem_file_setup("kgsl-3d0", size,
				VM_NORESERVE);
		if (IS_ERR(memdesc->shmem_filp)) {
			ret = PTR_ERR(memdesc->shmem_filp);
			pr_err("kgsl: unable to setup shmem file err %d\n",
					ret);
			memdesc->shmem_filp = NULL;
			return ret;
		}
	}

	return 0;
}

static int kgsl_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			struct kgsl_memdesc *memdesc, unsigned int page_off)
{
	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM)
		return kgsl_shmem_alloc_page(pages, memdesc->shmem_filp,
						page_off);

	return kgsl_pool_alloc_page(page_size, pages, pages_len, align,
					memdesc);
}

void kgsl_free_pages(struct kgsl_memdesc *memdesc)
{
	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM)
		kgsl_shmem_free_pages(memdesc);
	else
		kgsl_pool_free_pages(memdesc->pages, memdesc->page_count);
}

static void kgsl_free_page(struct kgsl_memdesc *memdesc, struct page *p)
{
	if (memdesc->priv & KGSL_MEMDESC_USE_SHMEM)
		put_page(p);
	else
		kgsl_pool_free_page(p);
}

void kgsl_free_pages_from_sgt(struct kgsl_memdesc *memdesc)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(memdesc->sgt->sgl, sg, memdesc->sgt->nents, i) {
		/*
		 * sg_alloc_table_from_pages() will collapse any physically
		 * adjacent pages into a single scatterlist entry. We cannot
		 * just call __free_pages() on the entire set since we cannot
		 * ensure that the size is a whole order. Instead, free each
		 * page or compound page group individually.
		 */
		struct page *p = sg_page(sg), *next;
		unsigned int count;
		unsigned int j = 0;

		while (j < (sg->length/PAGE_SIZE)) {
			count = 1 << compound_order(p);
			next = nth_page(p, count);
			kgsl_free_page(memdesc, p);

			p = next;
			j += count;
		}
	}
}

int
kgsl_sharedmem_page_alloc_user(struct kgsl_memdesc *memdesc,
			uint64_t size)
{
	int ret = 0;
	unsigned int j, page_size, len_alloc;
	unsigned int pcount = 0;
	size_t len;
	unsigned int align;
	bool memwq_flush_done = false;

	static DEFINE_RATELIMIT_STATE(_rs,
					DEFAULT_RATELIMIT_INTERVAL,
					DEFAULT_RATELIMIT_BURST);

	size = PAGE_ALIGN(size);
	if (size == 0 || size > UINT_MAX)
		return -EINVAL;

	align = (memdesc->flags & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT;

	/*
	 * As 1MB is the max supported page size, use the alignment
	 * corresponding to 1MB page to make sure higher order pages
	 * are used if possible for a given memory size. Also, we
	 * don't need to update alignment in memdesc flags in case
	 * higher order page is used, as memdesc flags represent the
	 * virtual alignment specified by the user which is anyways
	 * getting satisfied.
	 */
	if (align < ilog2(SZ_1M))
		align = ilog2(SZ_1M);

	page_size = kgsl_get_page_size(size, align, memdesc);

	/*
	 * The alignment cannot be less than the intended page size - it can be
	 * larger however to accommodate hardware quirks
	 */

	if (align < ilog2(page_size)) {
		kgsl_memdesc_set_align(memdesc, ilog2(page_size));
		align = ilog2(page_size);
	}

	/*
	 * There needs to be enough room in the page array to be able to
	 * service the allocation entirely with PAGE_SIZE sized chunks
	 */

	len_alloc = PAGE_ALIGN(size) >> PAGE_SHIFT;

	memdesc->ops = &kgsl_page_alloc_ops;

	/*
	 * Allocate space to store the list of pages. This is an array of
	 * pointers so we can track 1024 pages per page of allocation.
	 * Keep this array around for non global non secure buffers that
	 * are allocated by kgsl. This helps with improving the vm fault
	 * routine by finding the faulted page in constant time.
	 */

	memdesc->pages = kvcalloc(len_alloc, sizeof(*memdesc->pages),
		GFP_KERNEL);
	memdesc->page_count = 0;
	memdesc->size = 0;

	if (memdesc->pages == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	len = size;

	ret = kgsl_memdesc_file_setup(memdesc, size);
	if (ret)
		goto done;

	while (len > 0) {
		int page_count;

		page_count = kgsl_alloc_page(&page_size,
					memdesc->pages + pcount,
					len_alloc - pcount,
					&align, memdesc, pcount);
		if (page_count <= 0) {
			if (page_count == -EAGAIN)
				continue;

			/* if OoM, retry once after flushing mem_wq */
			if (page_count == -ENOMEM && !memwq_flush_done) {
				flush_workqueue(kgsl_driver.mem_workqueue);
				memwq_flush_done = true;
				continue;
			}

			/*
			 * Update sglen and memdesc size,as requested allocation
			 * not served fully. So that they can be correctly freed
			 * in kgsl_sharedmem_free().
			 */
			memdesc->size = (size - len);

			if (!sharedmem_noretry_flag && __ratelimit(&_rs))
				pr_err(
					"kgsl: out of memory: only allocated %lldKB of %lldKB requested\n",
					(size - len) >> 10, size >> 10);

			ret = -ENOMEM;
			goto done;
		}

		pcount += page_count;
		len -= page_size;
		memdesc->size += page_size;
		memdesc->page_count += page_count;

		/* Get the needed page size for the next iteration */
		page_size = kgsl_get_page_size(len, align, memdesc);
	}

	/* Call to the hypervisor to lock any secure buffer allocations */
	if (memdesc->flags & KGSL_MEMFLAGS_SECURE) {
		memdesc->sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (memdesc->sgt == NULL) {
			ret = -ENOMEM;
			goto done;
		}

		ret = sg_alloc_table_from_pages(memdesc->sgt, memdesc->pages,
			memdesc->page_count, 0, memdesc->size, GFP_KERNEL);
		if (ret) {
			kfree(memdesc->sgt);
			goto done;
		}

		ret = kgsl_lock_sgt(memdesc->sgt, memdesc->size);
		if (ret) {
			sg_free_table(memdesc->sgt);
			kfree(memdesc->sgt);
			memdesc->sgt = NULL;

			if (ret == -EADDRNOTAVAIL) {
				kvfree(memdesc->pages);
				memset(memdesc, 0, sizeof(*memdesc));
				return ret;
			}

			goto done;
		}

		memdesc->priv |= KGSL_MEMDESC_TZ_LOCKED;

		/* Record statistics */
		KGSL_STATS_ADD(memdesc->size, &kgsl_driver.stats.secure,
			&kgsl_driver.stats.secure_max);

		/*
		 * We don't need the array for secure buffers because they are
		 * not mapped to CPU
		 */
		kvfree(memdesc->pages);
		memdesc->pages = NULL;
		memdesc->page_count = 0;

		/* Don't map and zero the locked secure buffer */
		goto done;
	}

	KGSL_STATS_ADD(memdesc->size, &kgsl_driver.stats.page_alloc,
		&kgsl_driver.stats.page_alloc_max);

done:
	if (ret) {
		if (memdesc->pages) {
			unsigned int count = 1;

			for (j = 0; j < pcount; j += count) {
				count = 1 << compound_order(memdesc->pages[j]);
				kgsl_free_page(memdesc, memdesc->pages[j]);
			}
		}

		kvfree(memdesc->pages);
		if (memdesc->shmem_filp)
			fput(memdesc->shmem_filp);
		memset(memdesc, 0, sizeof(*memdesc));
	}

	return ret;
}

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	if (memdesc == NULL || memdesc->size == 0)
		return;

	/* Make sure the memory object has been unmapped */
	kgsl_mmu_put_gpuaddr(memdesc);

	if (memdesc->ops && memdesc->ops->free)
		memdesc->ops->free(memdesc);

	if (memdesc->sgt) {
		sg_free_table(memdesc->sgt);
		kfree(memdesc->sgt);
		memdesc->sgt = NULL;
	}

	memdesc->page_count = 0;
	kvfree(memdesc->pages);
	memdesc->pages = NULL;
}
EXPORT_SYMBOL(kgsl_sharedmem_free);

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

	kgsl_unlock_sgt(&sgt);
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

	status = kgsl_lock_sgt(&sgt, PAGE_SIZE);
	if (status) {
		if (status == -EADDRNOTAVAIL)
			return NULL;

		__free_page(page);
		return NULL;
	}
	return page;
}

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
EXPORT_SYMBOL(kgsl_sharedmem_readl);

int
kgsl_sharedmem_writel(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint32_t src)
{
	uint32_t *dst;

	if (WARN_ON(memdesc == NULL || memdesc->hostptr == NULL))
		return -EINVAL;

	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes > (memdesc->size - sizeof(uint32_t)));
	if (offsetbytes > (memdesc->size - sizeof(uint32_t)))
		return -ERANGE;
	dst = (uint32_t *)(memdesc->hostptr + offsetbytes);
	*dst = src;

	/*
	 * We are writing to shared memory between CPU and GPU.
	 * Make sure write above is posted immediately
	 */
	wmb();

	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_writel);

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
EXPORT_SYMBOL(kgsl_sharedmem_readq);

int
kgsl_sharedmem_writeq(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint64_t src)
{
	uint64_t *dst;

	if (WARN_ON(memdesc == NULL || memdesc->hostptr == NULL))
		return -EINVAL;

	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes > (memdesc->size - sizeof(uint32_t)));
	if (offsetbytes > (memdesc->size - sizeof(uint32_t)))
		return -ERANGE;
	dst = (uint64_t *)(memdesc->hostptr + offsetbytes);
	*dst = src;

	/*
	 * We are writing to shared memory between CPU and GPU.
	 * Make sure write above is posted immediately
	 */
	wmb();

	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_writeq);

int
kgsl_sharedmem_set(struct kgsl_device *device,
		const struct kgsl_memdesc *memdesc, uint64_t offsetbytes,
		unsigned int value, uint64_t sizebytes)
{
	if (WARN_ON(memdesc == NULL || memdesc->hostptr == NULL))
		return -EINVAL;

	if (WARN_ON(offsetbytes + sizebytes > memdesc->size))
		return -EINVAL;

	memset(memdesc->hostptr + offsetbytes, value, sizebytes);
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_set);

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

void kgsl_get_memory_usage(char *name, size_t name_size, uint64_t memflags)
{
	unsigned int type = MEMFLAGS(memflags, KGSL_MEMTYPE_MASK,
		KGSL_MEMTYPE_SHIFT);

	if (type == KGSL_MEMTYPE_KERNEL)
		strlcpy(name, "kernel", name_size);
	else if (type < ARRAY_SIZE(memtype_str) && memtype_str[type] != NULL)
		strlcpy(name, memtype_str[type], name_size);
	else
		snprintf(name, name_size, "VK/others(%3d)", type);
}
EXPORT_SYMBOL(kgsl_get_memory_usage);

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

int kgsl_sharedmem_alloc_contig(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t size)
{
	int result = 0;

	size = PAGE_ALIGN(size);
	if (size == 0 || size > SIZE_MAX)
		return -EINVAL;

	memdesc->size = size;
	memdesc->ops = &kgsl_cma_ops;
	memdesc->dev = device->dev->parent;

	memdesc->hostptr = dma_alloc_attrs(memdesc->dev, (size_t) size,
		&memdesc->physaddr, GFP_KERNEL, 0);

	if (memdesc->hostptr == NULL) {
		result = -ENOMEM;
		goto err;
	}

	result = kgsl_memdesc_sg_dma(memdesc, memdesc->physaddr, size);
	if (result)
		goto err;

	/* Record statistics */

	if (kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_NONE)
		memdesc->gpuaddr = memdesc->physaddr;

	KGSL_STATS_ADD(size, &kgsl_driver.stats.coherent,
		&kgsl_driver.stats.coherent_max);

	mod_node_page_state(page_pgdat(phys_to_page(memdesc->physaddr)),
			NR_UNRECLAIMABLE_PAGES, (size >> PAGE_SHIFT));
err:
	if (result)
		kgsl_sharedmem_free(memdesc);

	return result;
}
EXPORT_SYMBOL(kgsl_sharedmem_alloc_contig);

static int scm_lock_chunk(struct kgsl_memdesc *memdesc, int lock)
{
	struct cp2_lock_req request;
	unsigned int resp;
	unsigned int *chunk_list;
	struct scm_desc desc = {0};
	int result;

	/*
	 * Flush the virt addr range before sending the memory to the
	 * secure environment to ensure the data is actually present
	 * in RAM
	 *
	 * Chunk_list holds the physical address of secure memory.
	 * Pass in the virtual address of chunk_list to flush.
	 * Chunk_list size is 1 because secure memory is physically
	 * contiguous.
	 */
	chunk_list = kzalloc(sizeof(unsigned int), GFP_KERNEL);
	if (!chunk_list)
		return -ENOMEM;

	chunk_list[0] = memdesc->physaddr;
	dmac_flush_range((void *)chunk_list, (void *)chunk_list + 1);

	request.chunks.chunk_list = virt_to_phys(chunk_list);
	/*
	 * virt_to_phys(chunk_list) may be an address > 4GB. It is guaranteed
	 * that when using scm_call (the older interface), the phys addresses
	 * will be restricted to below 4GB.
	 */
	desc.args[0] = virt_to_phys(chunk_list);
	desc.args[1] = request.chunks.chunk_list_size = 1;
	desc.args[2] = request.chunks.chunk_size = (unsigned int) memdesc->size;
	desc.args[3] = request.mem_usage = 0;
	desc.args[4] = request.lock = lock;
	desc.args[5] = 0;
	desc.arginfo = SCM_ARGS(6, SCM_RW, SCM_VAL, SCM_VAL, SCM_VAL, SCM_VAL,
				SCM_VAL);
	kmap_flush_unused();
	kmap_atomic_flush_unused();
	/*
	 * scm_call2 now supports both 32 and 64 bit calls
	 * so we dont need scm_call separately.
	 */
		result = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				   MEM_PROTECT_LOCK_ID2_FLAT), &desc);
		resp = desc.ret[0];

	kfree(chunk_list);
	return result;
}

static int kgsl_cma_alloc_secure(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t size)
{
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	int result = 0;
	size_t aligned;

	/* Align size to 1M boundaries */
	aligned = ALIGN(size, SZ_1M);

	/* The SCM call uses an unsigned int for the size */
	if (aligned == 0 || aligned > UINT_MAX)
		return -EINVAL;

	/*
	 * If there is more than a page gap between the requested size and the
	 * aligned size we don't need to add more memory for a guard page. Yay!
	 */

	if (memdesc->priv & KGSL_MEMDESC_GUARD_PAGE)
		if (aligned - size >= SZ_4K)
			memdesc->priv &= ~KGSL_MEMDESC_GUARD_PAGE;

	memdesc->size = aligned;
	memdesc->ops = &kgsl_cma_ops;
	memdesc->dev = iommu->ctx[KGSL_IOMMU_CONTEXT_SECURE].dev;

	memdesc->attrs |= DMA_ATTR_STRONGLY_ORDERED;

	memdesc->hostptr = dma_alloc_attrs(memdesc->dev, aligned,
		&memdesc->physaddr, GFP_KERNEL, memdesc->attrs);

	if (memdesc->hostptr == NULL) {
		result = -ENOMEM;
		goto err;
	}

	result = kgsl_memdesc_sg_dma(memdesc, memdesc->physaddr, aligned);
	if (result)
		goto err;

	result = scm_lock_chunk(memdesc, 1);

	if (result != 0)
		goto err;

	memdesc->priv |= KGSL_MEMDESC_TZ_LOCKED;

	/* Record statistics */
	KGSL_STATS_ADD(aligned, &kgsl_driver.stats.secure,
	       &kgsl_driver.stats.secure_max);

	mod_node_page_state(page_pgdat(phys_to_page(memdesc->physaddr)),
			NR_UNRECLAIMABLE_PAGES, (aligned >> PAGE_SHIFT));
err:
	if (result)
		kgsl_sharedmem_free(memdesc);

	return result;
}

/**
 * kgsl_cma_unlock_secure() - Unlock secure memory by calling TZ
 * @memdesc: memory descriptor
 */
static void kgsl_cma_unlock_secure(struct kgsl_memdesc *memdesc)
{
	if (memdesc->size == 0 || !(memdesc->priv & KGSL_MEMDESC_TZ_LOCKED))
		return;

	scm_lock_chunk(memdesc, 0);
}

void kgsl_sharedmem_set_noretry(bool val)
{
	sharedmem_noretry_flag = val;
}

bool kgsl_sharedmem_get_noretry(void)
{
	return sharedmem_noretry_flag;
}

void kgsl_zero_page(struct page *p, unsigned int order)
{
	int i;

	for (i = 0; i < (1 << order); i++) {
		struct page *page = nth_page(p, i);
		void *addr = kmap_atomic(page);

		memset(addr, 0, PAGE_SIZE);
		dmac_flush_range(addr, addr + PAGE_SIZE);
		kunmap_atomic(addr);
	}
}

void kgsl_flush_page(struct page *page)
{
	void *addr = kmap_atomic(page);

	dmac_flush_range(addr, addr + PAGE_SIZE);
	kunmap_atomic(addr);
}

unsigned int kgsl_gfp_mask(unsigned int page_order)
{
	unsigned int gfp_mask = __GFP_HIGHMEM;

	if (page_order > 0) {
		gfp_mask |= __GFP_COMP | __GFP_NORETRY | __GFP_NOWARN;
		gfp_mask &= ~__GFP_RECLAIM;
	} else
		gfp_mask |= GFP_KERNEL;

	if (kgsl_sharedmem_get_noretry())
		gfp_mask |= __GFP_NORETRY | __GFP_NOWARN;

	return gfp_mask;
}
