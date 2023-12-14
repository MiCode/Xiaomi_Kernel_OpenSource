// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/component.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "kgsl_device.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

static void _deferred_destroy(struct work_struct *ws)
{
	struct kgsl_pagetable *pagetable = container_of(ws,
					struct kgsl_pagetable, destroy_ws);

	WARN_ON(!list_empty(&pagetable->list));

	pagetable->pt_ops->mmu_destroy_pagetable(pagetable);
}

static void kgsl_destroy_pagetable(struct kref *kref)
{
	struct kgsl_pagetable *pagetable = container_of(kref,
		struct kgsl_pagetable, refcount);

	kgsl_mmu_detach_pagetable(pagetable);

	kgsl_schedule_work(&pagetable->destroy_ws);
}

struct kgsl_pagetable *
kgsl_get_pagetable(unsigned long name)
{
	struct kgsl_pagetable *pt, *ret = NULL;
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (name == pt->name && kref_get_unless_zero(&pt->refcount)) {
			ret = pt;
			break;
		}
	}

	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);
	return ret;
}

static struct kgsl_pagetable *
_get_pt_from_kobj(struct kobject *kobj)
{
	unsigned int ptname;

	if (!kobj)
		return NULL;

	if (kstrtou32(kobj->name, 0, &ptname))
		return NULL;

	return kgsl_get_pagetable(ptname);
}

static ssize_t
sysfs_show_entries(struct kobject *kobj,
		   struct kobj_attribute *attr,
		   char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt) {
		unsigned int val = atomic_read(&pt->stats.entries);

		ret += scnprintf(buf, PAGE_SIZE, "%d\n", val);
	}

	kref_put(&pt->refcount, kgsl_destroy_pagetable);
	return ret;
}

static ssize_t
sysfs_show_mapped(struct kobject *kobj,
		  struct kobj_attribute *attr,
		  char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt) {
		uint64_t val = atomic_long_read(&pt->stats.mapped);

		ret += scnprintf(buf, PAGE_SIZE, "%llu\n", val);
	}

	kref_put(&pt->refcount, kgsl_destroy_pagetable);
	return ret;
}

static ssize_t
sysfs_show_max_mapped(struct kobject *kobj,
		      struct kobj_attribute *attr,
		      char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt) {
		uint64_t val = atomic_long_read(&pt->stats.max_mapped);

		ret += scnprintf(buf, PAGE_SIZE, "%llu\n", val);
	}

	kref_put(&pt->refcount, kgsl_destroy_pagetable);
	return ret;
}

static struct kobj_attribute attr_entries = {
	.attr = { .name = "entries", .mode = 0444 },
	.show = sysfs_show_entries,
	.store = NULL,
};

static struct kobj_attribute attr_mapped = {
	.attr = { .name = "mapped", .mode = 0444 },
	.show = sysfs_show_mapped,
	.store = NULL,
};

static struct kobj_attribute attr_max_mapped = {
	.attr = { .name = "max_mapped", .mode = 0444 },
	.show = sysfs_show_max_mapped,
	.store = NULL,
};

static struct attribute *pagetable_attrs[] = {
	&attr_entries.attr,
	&attr_mapped.attr,
	&attr_max_mapped.attr,
	NULL,
};

static struct attribute_group pagetable_attr_group = {
	.attrs = pagetable_attrs,
};

static void
pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable)
{
	if (pagetable->kobj)
		sysfs_remove_group(pagetable->kobj,
				   &pagetable_attr_group);

	kobject_put(pagetable->kobj);
	pagetable->kobj = NULL;
}

static int
pagetable_add_sysfs_objects(struct kgsl_pagetable *pagetable)
{
	char ptname[16];
	int ret = -ENOMEM;

	snprintf(ptname, sizeof(ptname), "%d", pagetable->name);
	pagetable->kobj = kobject_create_and_add(ptname,
						 kgsl_driver.ptkobj);
	if (pagetable->kobj == NULL)
		goto err;

	ret = sysfs_create_group(pagetable->kobj, &pagetable_attr_group);

err:
	if (ret) {
		if (pagetable->kobj)
			kobject_put(pagetable->kobj);

		pagetable->kobj = NULL;
	}

	return ret;
}

#ifdef CONFIG_TRACE_GPU_MEM
static void kgsl_mmu_trace_gpu_mem_pagetable(struct kgsl_pagetable *pagetable)
{
	if (pagetable->name == KGSL_MMU_GLOBAL_PT ||
			pagetable->name == KGSL_MMU_SECURE_PT)
		return;

	trace_gpu_mem_total(0, pagetable->name,
			(u64)atomic_long_read(&pagetable->stats.mapped));
}
#else
static void kgsl_mmu_trace_gpu_mem_pagetable(struct kgsl_pagetable *pagetable)
{
}
#endif

void
kgsl_mmu_detach_pagetable(struct kgsl_pagetable *pagetable)
{
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);

	if (!list_empty(&pagetable->list))
		list_del_init(&pagetable->list);

	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	pagetable_remove_sysfs_objects(pagetable);
}

unsigned int
kgsl_mmu_log_fault_addr(struct kgsl_mmu *mmu, u64 pt_base,
		uint64_t addr)
{
	struct kgsl_pagetable *pt;
	unsigned int ret = 0;

	spin_lock(&kgsl_driver.ptlock);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (kgsl_mmu_pagetable_get_ttbr0(pt) == MMU_SW_PT_BASE(pt_base)) {
			if ((addr & ~(PAGE_SIZE-1)) == pt->fault_addr) {
				ret = 1;
				break;
			}
			pt->fault_addr = (addr & ~(PAGE_SIZE-1));
			ret = 0;
			break;
		}
	}
	spin_unlock(&kgsl_driver.ptlock);

	return ret;
}

int kgsl_mmu_start(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_start))
		return mmu->mmu_ops->mmu_start(mmu);

	return 0;
}

void kgsl_mmu_pagetable_init(struct kgsl_mmu *mmu,
		struct kgsl_pagetable *pagetable, u32 name)
{
	kref_init(&pagetable->refcount);

	spin_lock_init(&pagetable->lock);
	INIT_WORK(&pagetable->destroy_ws, _deferred_destroy);

	pagetable->mmu = mmu;
	pagetable->name = name;

	atomic_set(&pagetable->stats.entries, 0);
	atomic_long_set(&pagetable->stats.mapped, 0);
	atomic_long_set(&pagetable->stats.max_mapped, 0);
}

void kgsl_mmu_pagetable_add(struct kgsl_mmu *mmu, struct kgsl_pagetable *pagetable)
{
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_add(&pagetable->list, &kgsl_driver.pagetable_list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	/* Create the sysfs entries */
	pagetable_add_sysfs_objects(pagetable);
}

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable)
{
	if (!IS_ERR_OR_NULL(pagetable))
		kref_put(&pagetable->refcount, kgsl_destroy_pagetable);
}

/**
 * kgsl_mmu_find_svm_region() - Find a empty spot in the SVM region
 * @pagetable: KGSL pagetable to search
 * @start: start of search range, must be within kgsl_mmu_svm_range()
 * @end: end of search range, must be within kgsl_mmu_svm_range()
 * @size: Size of the region to find
 * @align: Desired alignment of the address
 */
uint64_t kgsl_mmu_find_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t start, uint64_t end, uint64_t size,
		uint64_t align)
{
	if (PT_OP_VALID(pagetable, find_svm_region))
		return pagetable->pt_ops->find_svm_region(pagetable, start,
			end, size, align);
	return -ENOMEM;
}

/**
 * kgsl_mmu_set_svm_region() - Check if a region is empty and reserve it if so
 * @pagetable: KGSL pagetable to search
 * @gpuaddr: GPU address to check/reserve
 * @size: Size of the region to check/reserve
 */
int kgsl_mmu_set_svm_region(struct kgsl_pagetable *pagetable, uint64_t gpuaddr,
		uint64_t size)
{
	if (PT_OP_VALID(pagetable, set_svm_region))
		return pagetable->pt_ops->set_svm_region(pagetable, gpuaddr,
			size);
	return -ENOMEM;
}

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc)
{
	int size;
	struct kgsl_device *device = KGSL_MMU_DEVICE(pagetable->mmu);

	if (!memdesc->gpuaddr)
		return -EINVAL;
	/* Only global mappings should be mapped multiple times */
	if (!kgsl_memdesc_is_global(memdesc) &&
			(KGSL_MEMDESC_MAPPED & memdesc->priv))
		return -EINVAL;

	if (memdesc->flags & KGSL_MEMFLAGS_VBO)
		return -EINVAL;

	size = kgsl_memdesc_footprint(memdesc);

	if (PT_OP_VALID(pagetable, mmu_map)) {
		int ret;

		ret = pagetable->pt_ops->mmu_map(pagetable, memdesc);
		if (ret)
			return ret;

		atomic_inc(&pagetable->stats.entries);
		KGSL_STATS_ADD(size, &pagetable->stats.mapped,
				&pagetable->stats.max_mapped);
		kgsl_mmu_trace_gpu_mem_pagetable(pagetable);

		if (!kgsl_memdesc_is_global(memdesc)
				&& !(memdesc->flags & KGSL_MEMFLAGS_USERMEM_ION)) {
			kgsl_trace_gpu_mem_total(device, size);
		}

		memdesc->priv |= KGSL_MEMDESC_MAPPED;
	}

	return 0;
}

int kgsl_mmu_map_child(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 offset,
		struct kgsl_memdesc *child, u64 child_offset,
		u64 length)
{
	/* This only makes sense for virtual buffer objects */
	if (!(memdesc->flags & KGSL_MEMFLAGS_VBO))
		return -EINVAL;

	if (!memdesc->gpuaddr)
		return -EINVAL;

	if (PT_OP_VALID(pt, mmu_map_child)) {
		int ret;

		ret = pt->pt_ops->mmu_map_child(pt, memdesc,
			offset, child, child_offset, length);
		if (ret)
			return ret;

		KGSL_STATS_ADD(length, &pt->stats.mapped,
				&pt->stats.max_mapped);
	}

	return 0;
}

int kgsl_mmu_map_zero_page_to_range(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 start, u64 length)
{
	int ret = -EINVAL;

	/* This only makes sense for virtual buffer objects */
	if (!(memdesc->flags & KGSL_MEMFLAGS_VBO))
		return -EINVAL;

	if (!memdesc->gpuaddr)
		return -EINVAL;

	if (PT_OP_VALID(pt, mmu_map_zero_page_to_range)) {
		ret = pt->pt_ops->mmu_map_zero_page_to_range(pt,
			memdesc, start, length);
		if (ret)
			return ret;

		KGSL_STATS_ADD(length, &pt->stats.mapped,
				&pt->stats.max_mapped);
	}

	return 0;
}

/**
 * kgsl_mmu_svm_range() - Return the range for SVM (if applicable)
 * @pagetable: Pagetable to query the range from
 * @lo: Pointer to store the start of the SVM range
 * @hi: Pointer to store the end of the SVM range
 * @memflags: Flags from the buffer we are mapping
 */
int kgsl_mmu_svm_range(struct kgsl_pagetable *pagetable,
		uint64_t *lo, uint64_t *hi, uint64_t memflags)
{
	if (PT_OP_VALID(pagetable, svm_range))
		return pagetable->pt_ops->svm_range(pagetable, lo, hi,
			memflags);

	return -ENODEV;
}

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	int ret = 0;
	struct kgsl_device *device = KGSL_MMU_DEVICE(pagetable->mmu);

	if (memdesc->size == 0)
		return -EINVAL;

	if ((memdesc->flags & KGSL_MEMFLAGS_VBO))
		return -EINVAL;

	/* Only global mappings should be mapped multiple times */
	if (!(KGSL_MEMDESC_MAPPED & memdesc->priv))
		return -EINVAL;

	if (PT_OP_VALID(pagetable, mmu_unmap)) {
		uint64_t size;

		size = kgsl_memdesc_footprint(memdesc);

		ret = pagetable->pt_ops->mmu_unmap(pagetable, memdesc);
		if (ret)
			return ret;

		atomic_dec(&pagetable->stats.entries);
		atomic_long_sub(size, &pagetable->stats.mapped);
		kgsl_mmu_trace_gpu_mem_pagetable(pagetable);

		if (!kgsl_memdesc_is_global(memdesc)) {
			memdesc->priv &= ~KGSL_MEMDESC_MAPPED;
			if (!(memdesc->flags & KGSL_MEMFLAGS_USERMEM_ION))
				kgsl_trace_gpu_mem_total(device, -(size));
		}
	}

	return ret;
}

int
kgsl_mmu_unmap_range(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc, u64 offset, u64 length)
{
	int ret = 0;

	/* Only allow virtual buffer objects to use this function */
	if (!(memdesc->flags & KGSL_MEMFLAGS_VBO))
		return -EINVAL;

	if (PT_OP_VALID(pagetable, mmu_unmap_range)) {
		ret = pagetable->pt_ops->mmu_unmap_range(pagetable, memdesc,
			offset, length);

		if (!ret)
			atomic_long_sub(length, &pagetable->stats.mapped);
	}

	return ret;
}

void kgsl_mmu_map_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u32 padding)
{
	struct kgsl_mmu *mmu = &(device->mmu);

	if (MMU_OP_VALID(mmu, mmu_map_global))
		mmu->mmu_ops->mmu_map_global(mmu, memdesc, padding);
}

int kgsl_mmu_pagetable_get_context_bank(struct kgsl_pagetable *pagetable,
	struct kgsl_context *context)
{
	if (PT_OP_VALID(pagetable, get_context_bank))
		return pagetable->pt_ops->get_context_bank(pagetable, context);

	return -ENOENT;
}

int kgsl_mmu_pagetable_get_asid(struct kgsl_pagetable *pagetable,
		struct kgsl_context *context)
{
	if (PT_OP_VALID(pagetable, get_asid))
		return pagetable->pt_ops->get_asid(pagetable, context);

	return -ENOENT;
}

enum kgsl_mmutype kgsl_mmu_get_mmutype(struct kgsl_device *device)
{
	return device ? device->mmu.type : KGSL_MMU_TYPE_NONE;
}

bool kgsl_mmu_gpuaddr_in_range(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	if (PT_OP_VALID(pagetable, addr_in_range))
		return pagetable->pt_ops->addr_in_range(pagetable, gpuaddr, size);

	return false;
}

/*
 * NOMMU definitions - NOMMU really just means that the MMU is kept in pass
 * through and the GPU directly accesses physical memory. Used in debug mode
 * and when a real MMU isn't up and running yet.
 */

static bool nommu_gpuaddr_in_range(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	return (gpuaddr != 0) ? true : false;
}

static int nommu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	if (WARN_ONCE(memdesc->sgt->nents > 1,
		"Attempt to map non-contiguous memory with NOMMU\n"))
		return -EINVAL;

	memdesc->gpuaddr = (uint64_t) sg_phys(memdesc->sgt->sgl);

	if (memdesc->gpuaddr) {
		memdesc->pagetable = pagetable;
		return 0;
	}

	return -ENOMEM;
}

static void nommu_destroy_pagetable(struct kgsl_pagetable *pt)
{
	kfree(pt);
}

static const struct kgsl_mmu_pt_ops nommu_pt_ops = {
	.get_gpuaddr = nommu_get_gpuaddr,
	.addr_in_range = nommu_gpuaddr_in_range,
	.mmu_destroy_pagetable = nommu_destroy_pagetable,
};

static struct kgsl_pagetable *nommu_getpagetable(struct kgsl_mmu *mmu,
		unsigned long name)
{
	struct kgsl_device *device = KGSL_MMU_DEVICE(mmu);
	struct kgsl_pagetable *pagetable;
	struct kgsl_global_memdesc *md;

	pagetable = kgsl_get_pagetable(KGSL_MMU_GLOBAL_PT);

	if (pagetable == NULL) {
		pagetable = kzalloc(sizeof(*pagetable), GFP_KERNEL);
		if (!pagetable)
			return ERR_PTR(-ENOMEM);

		kgsl_mmu_pagetable_init(mmu, pagetable, KGSL_MMU_GLOBAL_PT);
		pagetable->pt_ops = &nommu_pt_ops;

		list_for_each_entry(md, &device->globals, node)
			md->memdesc.gpuaddr =
				(uint64_t) sg_phys(md->memdesc.sgt->sgl);

		kgsl_mmu_pagetable_add(mmu, pagetable);
	}

	return pagetable;
}

static struct kgsl_mmu_ops kgsl_nommu_ops = {
	.mmu_getpagetable = nommu_getpagetable,
};

static int kgsl_mmu_cb_bind(struct device *dev, struct device *master, void *data)
{
	return 0;
}

static void kgsl_mmu_cb_unbind(struct device *dev, struct device *master,
		void *data)
{
}

static int kgsl_mmu_bind(struct device *dev, struct device *master, void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);
	struct kgsl_mmu *mmu = &device->mmu;
	int ret;

	/*
	 * Try to bind the IOMMU and if it doesn't exist for some reason
	 * go for the NOMMU option instead
	 */
	ret = kgsl_iommu_bind(device, to_platform_device(dev));

	if (!ret || ret == -EPROBE_DEFER)
		return ret;

	mmu->mmu_ops = &kgsl_nommu_ops;
	mmu->type = KGSL_MMU_TYPE_NONE;
	return 0;
}

static void kgsl_mmu_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_close))
		mmu->mmu_ops->mmu_close(mmu);
}

static const struct component_ops kgsl_mmu_cb_component_ops = {
	.bind = kgsl_mmu_cb_bind,
	.unbind = kgsl_mmu_cb_unbind,
};

static const struct component_ops kgsl_mmu_component_ops = {
	.bind = kgsl_mmu_bind,
	.unbind = kgsl_mmu_unbind,
};

static int kgsl_mmu_dev_probe(struct platform_device *pdev)
{
	/*
	 * Add kgsl-smmu and context bank as a component device to establish
	 * correct probe order with smmu driver.
	 *
	 * As context bank node in DT contains "iommus" property. fw_devlink
	 * ensures that context bank is probed only after corresponding
	 * supplier (smmu driver) probe is done.
	 *
	 * Adding context bank as a component device ensures master bind
	 * (adreno_bind) is called only once component (context bank) probe
	 * is done thus ensuring correct probe order with smmu driver.
	 *
	 * kgsl-smmu also need to be a component because we need kgsl-smmu
	 * device info in order to initialize the context banks.
	 */
	if (of_device_is_compatible(pdev->dev.of_node,
				"qcom,smmu-kgsl-cb")) {
		return component_add(&pdev->dev, &kgsl_mmu_cb_component_ops);
	}

	/* Fill out the rest of the devices in the node */
	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	return component_add(&pdev->dev, &kgsl_mmu_component_ops);
}

static int kgsl_mmu_dev_remove(struct platform_device *pdev)
{
	if (of_device_is_compatible(pdev->dev.of_node,
				"qcom,smmu-kgsl-cb")) {
		component_del(&pdev->dev, &kgsl_mmu_cb_component_ops);
		return 0;
	}

	component_del(&pdev->dev, &kgsl_mmu_component_ops);

	of_platform_depopulate(&pdev->dev);
	return 0;
}

static const struct of_device_id mmu_match_table[] = {
	{ .compatible = "qcom,kgsl-smmu-v2" },
	{ .compatible = "qcom,smmu-kgsl-cb" },
	{},
};

static struct platform_driver kgsl_mmu_driver = {
	.probe = kgsl_mmu_dev_probe,
	.remove = kgsl_mmu_dev_remove,
	.driver = {
		.name = "kgsl-iommu",
		.of_match_table = mmu_match_table,
	}
};

int __init kgsl_mmu_init(void)
{
	return platform_driver_register(&kgsl_mmu_driver);
}

void kgsl_mmu_exit(void)
{
	platform_driver_unregister(&kgsl_mmu_driver);
}
