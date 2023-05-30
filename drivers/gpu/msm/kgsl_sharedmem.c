// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/of_platform.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/shmem_fs.h>
#include <linux/sched/signal.h>

#include "kgsl_device.h"
#include "kgsl_pool.h"
#include "kgsl_reclaim.h"
#include "kgsl_sharedmem.h"

/*
 * The user can set this from debugfs to force failed memory allocations to
 * fail without trying OOM first.  This is a debug setting useful for
 * stress applications that want to test failure cases without pushing the
 * system into unrecoverable OOM panics
 */

bool kgsl_sharedmem_noretry_flag;

static DEFINE_MUTEX(kernel_map_global_lock);

#define MEMTYPE(_type, _name) \
	static struct kgsl_memtype memtype_##_name = { \
	.type = _type, \
	.attr = { .name = __stringify(_name), .mode = 0444 } \
}

struct kgsl_memtype {
	unsigned int type;
	struct attribute attr;
};

/* We can not use macro MEMTYPE for "any(0)" because of special characters */
static struct kgsl_memtype memtype_any0 = {
	.type = KGSL_MEMTYPE_OBJECTANY,
	.attr = { .name = "any(0)", .mode = 0444 },
};

MEMTYPE(KGSL_MEMTYPE_FRAMEBUFFER, framebuffer);
MEMTYPE(KGSL_MEMTYPE_RENDERBUFFER, renderbuffer);
MEMTYPE(KGSL_MEMTYPE_ARRAYBUFFER, arraybuffer);
MEMTYPE(KGSL_MEMTYPE_ELEMENTARRAYBUFFER, elementarraybuffer);
MEMTYPE(KGSL_MEMTYPE_VERTEXARRAYBUFFER, vertexarraybuffer);
MEMTYPE(KGSL_MEMTYPE_TEXTURE, texture);
MEMTYPE(KGSL_MEMTYPE_SURFACE, surface);
MEMTYPE(KGSL_MEMTYPE_EGL_SURFACE, egl_surface);
MEMTYPE(KGSL_MEMTYPE_GL, gl);
MEMTYPE(KGSL_MEMTYPE_CL, cl);
MEMTYPE(KGSL_MEMTYPE_CL_BUFFER_MAP, cl_buffer_map);
MEMTYPE(KGSL_MEMTYPE_CL_BUFFER_NOMAP, cl_buffer_nomap);
MEMTYPE(KGSL_MEMTYPE_CL_IMAGE_MAP, cl_image_map);
MEMTYPE(KGSL_MEMTYPE_CL_IMAGE_NOMAP, cl_image_nomap);
MEMTYPE(KGSL_MEMTYPE_CL_KERNEL_STACK, cl_kernel_stack);
MEMTYPE(KGSL_MEMTYPE_COMMAND, command);
MEMTYPE(KGSL_MEMTYPE_2D, 2d);
MEMTYPE(KGSL_MEMTYPE_EGL_IMAGE, egl_image);
MEMTYPE(KGSL_MEMTYPE_EGL_SHADOW, egl_shadow);
MEMTYPE(KGSL_MEMTYPE_MULTISAMPLE, egl_multisample);
MEMTYPE(KGSL_MEMTYPE_KERNEL, kernel);

static struct attribute *memtype_attrs[] = {
	&memtype_any0.attr,
	&memtype_framebuffer.attr,
	&memtype_renderbuffer.attr,
	&memtype_arraybuffer.attr,
	&memtype_elementarraybuffer.attr,
	&memtype_vertexarraybuffer.attr,
	&memtype_texture.attr,
	&memtype_surface.attr,
	&memtype_egl_surface.attr,
	&memtype_gl.attr,
	&memtype_cl.attr,
	&memtype_cl_buffer_map.attr,
	&memtype_cl_buffer_nomap.attr,
	&memtype_cl_image_map.attr,
	&memtype_cl_image_nomap.attr,
	&memtype_cl_kernel_stack.attr,
	&memtype_command.attr,
	&memtype_2d.attr,
	&memtype_egl_image.attr,
	&memtype_egl_shadow.attr,
	&memtype_egl_multisample.attr,
	&memtype_kernel.attr,
	NULL,
};

ATTRIBUTE_GROUPS(memtype);

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

#define MEM_ENTRY_ATTR(_type, _name, _show)  \
	static struct kgsl_mem_entry_attribute mem_entry_##_name = \
		__MEM_ENTRY_ATTR(_type, _name, _show)

static ssize_t mem_entry_sysfs_show(struct kobject *kobj,
	struct kgsl_process_attribute *attr, char *buf)
{
	struct kgsl_mem_entry_attribute *pattr = to_mem_entry_attr(attr);
	struct kgsl_process_private *priv =
		container_of(kobj, struct kgsl_process_private, kobj);

	return pattr->show(priv, pattr->memtype, buf);
}

struct deferred_work {
	struct kgsl_process_private *private;
	struct work_struct work;
};

static void process_private_deferred_put(struct work_struct *work)
{
	struct deferred_work *free_work =
		container_of(work, struct deferred_work, work);

	kgsl_process_private_put(free_work->private);
	kfree(free_work);
}

static ssize_t memtype_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_process_private *priv;
	struct kgsl_memtype *memtype;
	struct kgsl_mem_entry *entry;
	u64 size = 0;
	int id = 0;
	struct deferred_work *work = kzalloc(sizeof(struct deferred_work),
			GFP_KERNEL);

	if (!work)
		return -ENOMEM;

	priv = container_of(kobj, struct kgsl_process_private, kobj_memtype);
	memtype = container_of(attr, struct kgsl_memtype, attr);

	/*
	 * Take a process refcount here and put it back in a deferred manner.
	 * This is to avoid a deadlock where we put back last reference of the
	 * process private (via kgsl_mem_entry_put) here and end up trying to
	 * remove sysfs kobject while we are still in the middle of reading one
	 * of the sysfs files.
	 */
	if (!kgsl_process_private_get(priv)) {
		kfree(work);
		return -ENOENT;
	}

	work->private = priv;
	INIT_WORK(&work->work, process_private_deferred_put);

	spin_lock(&priv->mem_lock);
	for (entry = idr_get_next(&priv->mem_idr, &id); entry;
		id++, entry = idr_get_next(&priv->mem_idr, &id)) {
		struct kgsl_memdesc *memdesc;
		unsigned int type;

		if (!kgsl_mem_entry_get(entry))
			continue;
		spin_unlock(&priv->mem_lock);

		memdesc = &entry->memdesc;
		type = kgsl_memdesc_get_memtype(memdesc);

		if (type == memtype->type)
			size += memdesc->size;

		kgsl_mem_entry_put(entry);
		spin_lock(&priv->mem_lock);
	}
	spin_unlock(&priv->mem_lock);

	queue_work(kgsl_driver.lockless_workqueue, &work->work);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", size);
}

static const struct sysfs_ops memtype_sysfs_ops = {
	.show = memtype_sysfs_show,
};

static struct kobj_type ktype_memtype = {
	.sysfs_ops = &memtype_sysfs_ops,
	.default_groups = memtype_groups,
};

static ssize_t
imported_mem_show(struct kgsl_process_private *priv,
				int type, char *buf)
{
	struct kgsl_mem_entry *entry;
	uint64_t imported_mem = 0;
	int id = 0;
	struct deferred_work *work = kzalloc(sizeof(struct deferred_work),
		GFP_KERNEL);

	if (!work)
		return -ENOMEM;

	/*
	 * Take a process refcount here and put it back in a deferred manner.
	 * This is to avoid a deadlock where we put back last reference of the
	 * process private (via kgsl_mem_entry_put) here and end up trying to
	 * remove sysfs kobject while we are still in the middle of reading one
	 * of the sysfs files.
	 */
	if (!kgsl_process_private_get(priv)) {
		kfree(work);
		return -ENOENT;
	}

	work->private = priv;
	INIT_WORK(&work->work, process_private_deferred_put);

	spin_lock(&priv->mem_lock);
	for (entry = idr_get_next(&priv->mem_idr, &id); entry;
		id++, entry = idr_get_next(&priv->mem_idr, &id)) {

		int egl_surface_count = 0, egl_image_count = 0;
		struct kgsl_memdesc *m;

		if (!kgsl_mem_entry_get(entry))
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

	queue_work(kgsl_driver.lockless_workqueue, &work->work);

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

MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_KERNEL, kernel, mem_entry_show);
MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_KERNEL, kernel_max, mem_entry_max_show);
MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_USER, user, mem_entry_show);
MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_USER, user_max, mem_entry_max_show);
#ifdef CONFIG_ION
MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_USER, ion, mem_entry_show);
MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_USER, ion_max, mem_entry_max_show);
#endif
MEM_ENTRY_ATTR(0, imported_mem, imported_mem_show);
MEM_ENTRY_ATTR(0, gpumem_mapped, gpumem_mapped_show);
MEM_ENTRY_ATTR(KGSL_MEM_ENTRY_KERNEL, gpumem_unmapped, gpumem_unmapped_show);

static struct attribute *mem_entry_attrs[] = {
	&mem_entry_kernel.attr.attr,
	&mem_entry_kernel_max.attr.attr,
	&mem_entry_user.attr.attr,
	&mem_entry_user_max.attr.attr,
#ifdef CONFIG_ION
	&mem_entry_ion.attr.attr,
	&mem_entry_ion_max.attr.attr,
#endif
	&mem_entry_imported_mem.attr.attr,
	&mem_entry_gpumem_mapped.attr.attr,
	&mem_entry_gpumem_unmapped.attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mem_entry);

static struct kobj_type process_ktype = {
	.sysfs_ops = &process_sysfs_ops,
	.release = &process_sysfs_release,
	.default_groups = mem_entry_groups,
};
#ifdef CONFIG_QCOM_KGSL_PROCESS_RECLAIM
static struct device_attribute dev_attr_max_reclaim_limit = {
	.attr = { .name = "max_reclaim_limit", .mode = 0644 },
	.show = kgsl_proc_max_reclaim_limit_show,
	.store = kgsl_proc_max_reclaim_limit_store,
};

static struct device_attribute dev_attr_page_reclaim_per_call = {
	.attr = { .name = "page_reclaim_per_call", .mode = 0644 },
	.show = kgsl_nr_to_scan_show,
	.store = kgsl_nr_to_scan_store,
};
#endif

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
	if (kobject_init_and_add(&private->kobj, &process_ktype,
		kgsl_driver.prockobj, "%d", pid_nr(private->pid))) {
		dev_err(device->dev, "Unable to add sysfs for process %d\n",
			pid_nr(private->pid));
	}

	kgsl_reclaim_proc_sysfs_init(private);

	if (kobject_init_and_add(&private->kobj_memtype, &ktype_memtype,
		&private->kobj, "memtype")) {
		dev_err(device->dev, "Unable to add memtype sysfs for process %d\n",
			pid_nr(private->pid));
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

	ret = kstrtou32(buf, 0, &thresh);
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
#ifdef CONFIG_QCOM_KGSL_PROCESS_RECLAIM
	&dev_attr_max_reclaim_limit.attr,
	&dev_attr_page_reclaim_per_call.attr,
#endif
	NULL,
};

int
kgsl_sharedmem_init_sysfs(void)
{
	return sysfs_create_files(&kgsl_driver.virtdev.kobj, drv_attr_list);
}

static vm_fault_t kgsl_paged_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	int pgoff, ret;
	struct page *page;
	unsigned int offset = vmf->address - vma->vm_start;

	if (offset >= memdesc->size)
		return VM_FAULT_SIGBUS;

	pgoff = offset >> PAGE_SHIFT;

	spin_lock(&memdesc->lock);
	if (memdesc->pages[pgoff]) {
		page = memdesc->pages[pgoff];
		get_page(page);
	} else {
		struct kgsl_process_private *priv =
			((struct kgsl_mem_entry *)vma->vm_private_data)->priv;

		/* We are here because page was reclaimed */
		memdesc->priv |= KGSL_MEMDESC_SKIP_RECLAIM;
		spin_unlock(&memdesc->lock);

		page = shmem_read_mapping_page_gfp(
			memdesc->shmem_filp->f_mapping, pgoff,
			kgsl_gfp_mask(0));
		if (IS_ERR(page))
			return VM_FAULT_SIGBUS;
		kgsl_page_sync_for_device(memdesc->dev, page, PAGE_SIZE);

		spin_lock(&memdesc->lock);
		/*
		 * Update the pages array only if the page was
		 * not already brought back.
		 */
		if (!memdesc->pages[pgoff]) {
			memdesc->pages[pgoff] = page;
			atomic_dec(&priv->unpinned_page_count);
			get_page(page);
		}
	}
	spin_unlock(&memdesc->lock);

	ret = vmf_insert_page(vma, vmf->address, page);
	put_page(page);
	return ret;
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

static int unlock_sgt(struct sg_table *sgt)
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
#endif

static int kgsl_paged_map_kernel(struct kgsl_memdesc *memdesc)
{
	int ret = 0;

	/* Sanity check - don't map more than we could possibly chew */
	if (memdesc->size > ULONG_MAX)
		return -ENOMEM;

	mutex_lock(&kernel_map_global_lock);
	if ((!memdesc->hostptr) && (memdesc->pages != NULL)) {
		pgprot_t page_prot;
		int cache;

		/* Determine user-side caching policy */
		cache = kgsl_memdesc_get_cachemode(memdesc);
		switch (cache) {
		case KGSL_CACHEMODE_WRITETHROUGH:
			page_prot = PAGE_KERNEL;
			WARN_ONCE(1, "WRITETHROUGH is deprecated for arm64");
			break;
		case KGSL_CACHEMODE_WRITEBACK:
			page_prot = PAGE_KERNEL;
			break;
		case KGSL_CACHEMODE_UNCACHED:
		case KGSL_CACHEMODE_WRITECOMBINE:
		default:
			page_prot = pgprot_writecombine(PAGE_KERNEL);
			break;
		}

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
	int i;

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

	size += offset & PAGE_MASK;
	offset &= ~PAGE_MASK;

	/* If there is a sgt, use for_each_sg_page to walk it */
	if (memdesc->sgt) {
		struct sg_page_iter sg_iter;

		for_each_sg_page(memdesc->sgt->sgl, &sg_iter,
			PAGE_ALIGN(size) >> PAGE_SHIFT, offset >> PAGE_SHIFT)
			_dma_cache_op(memdesc->dev, sg_page_iter_page(&sg_iter), op);
		return 0;
	}

	/* Otherwise just walk through the list of pages */
	for (i = 0; i < memdesc->page_count; i++) {
		u64 cur = (i << PAGE_SHIFT);

		if ((cur < offset) || (cur >= (offset + size)))
			continue;

		_dma_cache_op(memdesc->dev, memdesc->pages[i], op);
	}

	return 0;
}

void kgsl_memdesc_init(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t flags)
{
	struct kgsl_mmu *mmu = &device->mmu;
	unsigned int align;

	memset(memdesc, 0, sizeof(*memdesc));
	/* Turn off SVM if the system doesn't support it */
	if (!kgsl_mmu_is_perprocess(mmu))
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
		kgsl_memdesc_get_align(memdesc), ilog2(PAGE_SIZE));
	kgsl_memdesc_set_align(memdesc, align);

	spin_lock_init(&memdesc->lock);
}

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	if (!memdesc || !memdesc->size)
		return;

	/* Assume if no operations were specified something went bad early */
	if (!memdesc->ops)
		return;

	if (memdesc->ops->put_gpuaddr)
		memdesc->ops->put_gpuaddr(memdesc);

	if (memdesc->ops->free)
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
	unsigned int type = FIELD_GET(KGSL_MEMTYPE_MASK, memflags);
	struct kgsl_memtype *memtype;
	int i;

	for (i = 0; memtype_attrs[i]; i++) {
		memtype = container_of(memtype_attrs[i], struct kgsl_memtype, attr);
		if (memtype->type == type) {
			strlcpy(name, memtype->attr.name, name_size);
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

	_kgsl_contiguous_free(memdesc);
}

#ifdef CONFIG_QCOM_KGSL_USE_SHMEM
static int kgsl_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			unsigned int page_off, struct file *shmem_filp,
			struct device *dev)
{
	struct page *page;

	if (pages == NULL)
		return -EINVAL;

	if (fatal_signal_pending(current))
		return -ENOMEM;

	page = shmem_read_mapping_page_gfp(shmem_filp->f_mapping, page_off,
			kgsl_gfp_mask(0));
	if (IS_ERR(page))
		return PTR_ERR(page);

	kgsl_zero_page(page, 0, dev);
	*pages = page;

	return 1;
}

static int kgsl_memdesc_file_setup(struct kgsl_memdesc *memdesc, uint64_t size)
{
	int ret;

	memdesc->shmem_filp = shmem_file_setup("kgsl-3d0", size,
			VM_NORESERVE);
	if (IS_ERR(memdesc->shmem_filp)) {
		ret = PTR_ERR(memdesc->shmem_filp);
		pr_err("kgsl: unable to setup shmem file err %d\n",
				ret);
		memdesc->shmem_filp = NULL;
		return ret;
	}

	mapping_set_unevictable(memdesc->shmem_filp->f_mapping);
	return 0;
}

static void kgsl_free_page(struct page *p)
{
	put_page(p);
}

static void _kgsl_free_pages(struct kgsl_memdesc *memdesc, unsigned int pcount)
{
	int i;

	for (i = 0; i < memdesc->page_count; i++)
		if (memdesc->pages[i])
			put_page(memdesc->pages[i]);

	fput(memdesc->shmem_filp);
}
#else
static int kgsl_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			unsigned int page_off, struct file *shmem_filp,
			struct device *dev)
{
	if (fatal_signal_pending(current))
		return -ENOMEM;

	return kgsl_pool_alloc_page(page_size, pages,
			pages_len, align, dev);
}

static int kgsl_memdesc_file_setup(struct kgsl_memdesc *memdesc, uint64_t size)
{
	return 0;
}

static void kgsl_free_page(struct page *p)
{
	kgsl_pool_free_page(p);
}

static void _kgsl_free_pages(struct kgsl_memdesc *memdesc, unsigned int pcount)
{
	kgsl_pool_free_pages(memdesc->pages, pcount);
}
#endif

static void kgsl_free_pages_from_sgt(struct kgsl_memdesc *memdesc)
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
			kgsl_free_page(p);

			p = next;
			j += count;
		}
	}

	if (memdesc->shmem_filp)
		fput(memdesc->shmem_filp);
}

void kgsl_page_sync_for_device(struct device *dev, struct page *page,
		size_t size)
{
	struct scatterlist sg;

	/* The caller may choose not to specify a device on purpose */
	if (!dev)
		return;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	sg_dma_address(&sg) = page_to_phys(page);

	dma_sync_sg_for_device(dev, &sg, 1, DMA_BIDIRECTIONAL);
}

void kgsl_zero_page(struct page *p, unsigned int order,
		struct device *dev)
{
	int i;

	for (i = 0; i < (1 << order); i++) {
		struct page *page = nth_page(p, i);

		clear_highpage(page);
	}

	kgsl_page_sync_for_device(dev, p, PAGE_SIZE << order);
}

gfp_t kgsl_gfp_mask(int page_order)
{
	gfp_t gfp_mask = __GFP_HIGHMEM;

	if (page_order > 0) {
		gfp_mask |= __GFP_COMP | __GFP_NORETRY | __GFP_NOWARN;
		gfp_mask &= ~__GFP_RECLAIM;
	} else
		gfp_mask |= GFP_KERNEL;

	if (kgsl_sharedmem_noretry_flag)
		gfp_mask |= __GFP_NORETRY | __GFP_NOWARN;

	return gfp_mask;
}

static int _kgsl_alloc_pages(struct kgsl_memdesc *memdesc,
		u64 size, struct page ***pages, struct device *dev)
{
	int count = 0;
	int npages = size >> PAGE_SHIFT;
	struct page **local = kvcalloc(npages, sizeof(*local), GFP_KERNEL);
	u32 page_size, align;
	u64 len = size;

	if (!local)
		return -ENOMEM;

	count = kgsl_memdesc_file_setup(memdesc, size);
	if (count) {
		kvfree(local);
		return count;
	}

	/* Start with 1MB alignment to get the biggest page we can */
	align = ilog2(SZ_1M);

	page_size = kgsl_get_page_size(len, align);

	while (len) {
		int ret = kgsl_alloc_page(&page_size, &local[count],
			npages, &align, count, memdesc->shmem_filp, dev);

		if (ret == -EAGAIN)
			continue;
		else if (ret <= 0) {
			int i;

			for (i = 0; i < count; ) {
				int n = 1 << compound_order(local[i]);

				kgsl_free_page(local[i]);
				i += n;
			}
			kvfree(local);

			if (!kgsl_sharedmem_noretry_flag)
				pr_err_ratelimited("kgsl: out of memory: only allocated %lldKb of %lldKb requested\n",
					(size - len) >> 10, size >> 10);

			if (memdesc->shmem_filp)
				fput(memdesc->shmem_filp);

			return -ENOMEM;
		}

		count += ret;
		npages -= ret;
		len -= page_size;

		page_size = kgsl_get_page_size(len, align);
	}

	*pages = local;

	return count;
}

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static void kgsl_free_secure_system_pages(struct kgsl_memdesc *memdesc)
{
	int i;
	struct scatterlist *sg;
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

	for_each_sg(memdesc->sgt->sgl, sg, memdesc->sgt->nents, i) {
		struct page *page = sg_page(sg);

		__free_pages(page, get_order(PAGE_SIZE));
	}

	sg_free_table(memdesc->sgt);
	kfree(memdesc->sgt);

	memdesc->sgt = NULL;
}

static void kgsl_free_secure_pages(struct kgsl_memdesc *memdesc)
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

	kgsl_free_pages_from_sgt(memdesc);

	sg_free_table(memdesc->sgt);
	kfree(memdesc->sgt);

	memdesc->sgt = NULL;
}
#endif

static void kgsl_free_pages(struct kgsl_memdesc *memdesc)
{
	kgsl_paged_unmap_kernel(memdesc);
	WARN_ON(memdesc->hostptr);

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.page_alloc);

	_kgsl_free_pages(memdesc, memdesc->page_count);

	memdesc->page_count = 0;
	kvfree(memdesc->pages);

	memdesc->pages = NULL;
}


static void kgsl_free_system_pages(struct kgsl_memdesc *memdesc)
{
	int i;

	kgsl_paged_unmap_kernel(memdesc);
	WARN_ON(memdesc->hostptr);

	atomic_long_sub(memdesc->size, &kgsl_driver.stats.page_alloc);

	for (i = 0; i < memdesc->page_count; i++)
		__free_pages(memdesc->pages[i], get_order(PAGE_SIZE));

	memdesc->page_count = 0;
	kvfree(memdesc->pages);
	memdesc->pages = NULL;
}

void kgsl_unmap_and_put_gpuaddr(struct kgsl_memdesc *memdesc)
{
	if (!memdesc->size || !memdesc->gpuaddr)
		return;

	/*
	 * Don't release the GPU address if the memory fails to unmap because
	 * the IOMMU driver will BUG later if we reallocated the address and
	 * tried to map it
	 */
	if (!kgsl_memdesc_is_reclaimed(memdesc) &&
		kgsl_mmu_unmap(memdesc->pagetable, memdesc))
		return;

	kgsl_mmu_put_gpuaddr(memdesc->pagetable, memdesc);

	memdesc->gpuaddr = 0;
	memdesc->pagetable = NULL;
}

static const struct kgsl_memdesc_ops kgsl_contiguous_ops = {
	.free = kgsl_contiguous_free,
	.vmflags = VM_DONTDUMP | VM_PFNMAP | VM_DONTEXPAND | VM_DONTCOPY,
	.vmfault = kgsl_contiguous_vmfault,
	.put_gpuaddr = kgsl_unmap_and_put_gpuaddr,
};

#if IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER)
static const struct kgsl_memdesc_ops kgsl_secure_system_ops = {
	.free = kgsl_free_secure_system_pages,
	/* FIXME: Make sure vmflags / vmfault does the right thing here */
	.put_gpuaddr = kgsl_unmap_and_put_gpuaddr,
};

static const struct kgsl_memdesc_ops kgsl_secure_page_ops = {
	.free = kgsl_free_secure_pages,
	/* FIXME: Make sure vmflags / vmfault does the right thing here */
	.put_gpuaddr = kgsl_unmap_and_put_gpuaddr,
};
#endif

static const struct kgsl_memdesc_ops kgsl_page_ops = {
	.free = kgsl_free_pages,
	.vmflags = VM_DONTDUMP | VM_DONTEXPAND | VM_DONTCOPY | VM_MIXEDMAP,
	.vmfault = kgsl_paged_vmfault,
	.map_kernel = kgsl_paged_map_kernel,
	.unmap_kernel = kgsl_paged_unmap_kernel,
	.put_gpuaddr = kgsl_unmap_and_put_gpuaddr,
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

	local = kvcalloc(npages, sizeof(*pages), GFP_KERNEL | __GFP_NORETRY);
	if (!local)
		return -ENOMEM;

	for (i = 0; i < npages; i++) {
		gfp_t gfp = __GFP_ZERO | __GFP_HIGHMEM |
			GFP_KERNEL | __GFP_NORETRY;

		if (!fatal_signal_pending(current))
			local[i] = alloc_pages(gfp, get_order(PAGE_SIZE));
		else
			local[i] = NULL;

		if (!local[i]) {
			for (i = i - 1; i >= 0; i--)
				__free_pages(local[i], get_order(PAGE_SIZE));
			kvfree(local);
			return -ENOMEM;
		}

		/* Make sure the cache is clean */
		sg_init_table(&sg, 1);
		sg_set_page(&sg, local[i], PAGE_SIZE, 0);
		sg_dma_address(&sg) = page_to_phys(local[i]);

		dma_sync_sg_for_device(dev, &sg, 1, DMA_BIDIRECTIONAL);
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
		memdesc->ops = &kgsl_secure_page_ops;
		count = _kgsl_alloc_pages(memdesc, size, &pages, device->dev);
	}

	if (count < 0)
		return count;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		_kgsl_free_pages(memdesc, count);
		kvfree(pages);
		return -ENOMEM;
	}

	ret = sg_alloc_table_from_pages(sgt, pages, count, 0, size, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		_kgsl_free_pages(memdesc, count);
		kvfree(pages);
		return ret;
	}

	/* Now that we've moved to a sg table don't need the pages anymore */
	kvfree(pages);

	ret = lock_sgt(sgt, size);
	if (ret) {
		if (ret != -EADDRNOTAVAIL)
			kgsl_free_pages_from_sgt(memdesc);
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
		memdesc->ops = &kgsl_page_ops;
		count = _kgsl_alloc_pages(memdesc, size, &pages, device->dev);
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

	if (!ret)
		KGSL_STATS_ADD(size, &kgsl_driver.stats.coherent,
			&kgsl_driver.stats.coherent_max);

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

int kgsl_memdesc_init_fixed(struct kgsl_device *device,
		struct platform_device *pdev, const char *resource,
		struct kgsl_memdesc *memdesc)
{
	u32 entry[2];

	if (of_property_read_u32_array(pdev->dev.of_node,
		resource, entry, 2))
		return -ENODEV;

	kgsl_memdesc_init(device, memdesc, 0);
	memdesc->physaddr = entry[0];
	memdesc->size = entry[1];

	return kgsl_memdesc_sg_dma(memdesc, entry[0], entry[1]);
}

struct kgsl_memdesc *kgsl_allocate_global_fixed(struct kgsl_device *device,
		const char *resource, const char *name)
{
	struct kgsl_global_memdesc *gmd = kzalloc(sizeof(*gmd), GFP_KERNEL);
	int ret;

	if (!gmd)
		return ERR_PTR(-ENOMEM);

	ret = kgsl_memdesc_init_fixed(device, device->pdev, resource,
			&gmd->memdesc);
	if (ret) {
		kfree(gmd);
		return ERR_PTR(ret);
	}

	gmd->memdesc.priv = KGSL_MEMDESC_GLOBAL;
	gmd->name = name;

	/*
	 * No lock here, because this function is only called during probe/init
	 * while the caller is holding the mutex
	 */
	list_add_tail(&gmd->node, &device->globals);
	kgsl_mmu_map_global(device, &gmd->memdesc, 0);

	return &gmd->memdesc;
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
