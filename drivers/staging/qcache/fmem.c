/*
 *
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/fmem.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#ifdef CONFIG_MEMORY_HOTPLUG
#include <linux/memory.h>
#include <linux/memory_hotplug.h>
#endif
#include "tmem.h"
#include <asm/mach/map.h>

struct fmem_data fmem_data;
enum fmem_state fmem_state;
static spinlock_t fmem_state_lock;

#ifdef CONFIG_MEMORY_HOTPLUG
static unsigned int section_powered_off[NR_MEM_SECTIONS];
static unsigned int fmem_section_start, fmem_section_end;
#endif

void *fmem_map_virtual_area(int cacheability)
{
	unsigned long addr;
	const struct mem_type *type;
	int ret;

	addr = (unsigned long) fmem_data.area->addr;
	type = get_mem_type(cacheability);
	ret = ioremap_page_range(addr, addr + fmem_data.size,
			fmem_data.phys, __pgprot(type->prot_pte));
	if (ret)
		return ERR_PTR(ret);

	fmem_data.virt = fmem_data.area->addr;

	return fmem_data.virt;
}

void fmem_unmap_virtual_area(void)
{
	unmap_kernel_range((unsigned long)fmem_data.virt, fmem_data.size);
	fmem_data.virt = NULL;
}

static int fmem_probe(struct platform_device *pdev)
{
	struct fmem_platform_data *pdata = pdev->dev.platform_data;

	if (!pdata->phys)
		pdata->phys = allocate_contiguous_ebi_nomap(pdata->size,
			PAGE_SIZE);

#ifdef CONFIG_MEMORY_HOTPLUG
	fmem_section_start = pdata->phys >> PA_SECTION_SHIFT;
	fmem_section_end = (pdata->phys - 1 + pdata->size) >> PA_SECTION_SHIFT;
#endif
	fmem_data.phys = pdata->phys + pdata->reserved_size_low;
	fmem_data.size = pdata->size - pdata->reserved_size_low -
					pdata->reserved_size_high;
	fmem_data.reserved_size_low = pdata->reserved_size_low;
	fmem_data.reserved_size_high = pdata->reserved_size_high;

	if (!fmem_data.size)
		return -ENODEV;

	fmem_data.area = get_vm_area(fmem_data.size, VM_IOREMAP);
	if (!fmem_data.area)
		return -ENOMEM;

	if (!fmem_map_virtual_area(MT_DEVICE_CACHED)) {
		remove_vm_area(fmem_data.area->addr);
		return -ENOMEM;
	}
	pr_info("fmem phys %lx virt %p size %lx\n",
		fmem_data.phys, fmem_data.virt, fmem_data.size);

	spin_lock_init(&fmem_state_lock);

	return 0;
}

static int fmem_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver fmem_driver = {
	.probe = fmem_probe,
	.remove = fmem_remove,
	.driver = { .name = "fmem" }
};

#ifdef CONFIG_SYSFS
static ssize_t fmem_state_show(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    char *buf)
{
	if (fmem_state == FMEM_T_STATE)
		return snprintf(buf, 3, "t\n");
	else if (fmem_state == FMEM_C_STATE)
		return snprintf(buf, 3, "c\n");
#ifdef CONFIG_MEMORY_HOTPLUG
	else if (fmem_state == FMEM_O_STATE)
		return snprintf(buf, 3, "o\n");
#endif
	else if (fmem_state == FMEM_UNINITIALIZED)
		return snprintf(buf, 15, "uninitialized\n");
	return snprintf(buf, 3, "?\n");
}

static ssize_t fmem_state_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	int ret = -EINVAL;

	if (!strncmp(buf, "t", 1))
		ret = fmem_set_state(FMEM_T_STATE);
	else if (!strncmp(buf, "c", 1))
		ret = fmem_set_state(FMEM_C_STATE);
#ifdef CONFIG_MEMORY_HOTPLUG
	else if (!strncmp(buf, "o", 1))
		ret = fmem_set_state(FMEM_O_STATE);
#endif
	if (ret)
		return ret;
	return 1;
}

static struct kobj_attribute fmem_state_attr = {
		.attr = { .name = "state", .mode = 0644 },
		.show = fmem_state_show,
		.store = fmem_state_store,
};

static struct attribute *fmem_attrs[] = {
	&fmem_state_attr.attr,
	NULL,
};

static struct attribute_group fmem_attr_group = {
	.attrs = fmem_attrs,
	.name = "fmem",
};

static int fmem_create_sysfs(void)
{
	int ret = 0;

	ret = sysfs_create_group(mm_kobj, &fmem_attr_group);
	if (ret)
		pr_err("fmem: can't create sysfs\n");
	return ret;
}

#endif

#ifdef CONFIG_MEMORY_HOTPLUG
bool fmem_is_disjoint(unsigned long start_pfn, unsigned long nr_pages)
{
	unsigned long fmem_start_pfn, fmem_end_pfn;
	unsigned long unstable_end_pfn;
	unsigned long highest_start_pfn, lowest_end_pfn;

	fmem_start_pfn = (fmem_data.phys - fmem_data.reserved_size_low)
		>> PAGE_SHIFT;
	fmem_end_pfn = (fmem_data.phys + fmem_data.size +
		fmem_data.reserved_size_high - 1) >> PAGE_SHIFT;
	unstable_end_pfn = start_pfn + nr_pages - 1;

	highest_start_pfn = max(fmem_start_pfn, start_pfn);
	lowest_end_pfn = min(fmem_end_pfn, unstable_end_pfn);

	return lowest_end_pfn < highest_start_pfn;
}

static int fmem_mem_going_offline_callback(void *arg)
{
	struct memory_notify *marg = arg;

	if (fmem_is_disjoint(marg->start_pfn, marg->nr_pages))
		return 0;
	return fmem_set_state(FMEM_O_STATE);
}

static void fmem_mem_online_callback(void *arg)
{
	struct memory_notify *marg = arg;
	int i;

	section_powered_off[marg->start_pfn >> PFN_SECTION_SHIFT] = 0;

	if (fmem_state != FMEM_O_STATE)
		return;

	for (i = fmem_section_start; i <= fmem_section_end; i++) {
		if (section_powered_off[i])
			return;
	}

	fmem_set_state(FMEM_T_STATE);
}

static void fmem_mem_offline_callback(void *arg)
{
	struct memory_notify *marg = arg;

	section_powered_off[marg->start_pfn >> PFN_SECTION_SHIFT] = 1;
}

static int fmem_memory_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	int ret = 0;

	if (fmem_state == FMEM_UNINITIALIZED)
		return NOTIFY_OK;

	switch (action) {
	case MEM_ONLINE:
		fmem_mem_online_callback(arg);
		break;
	case MEM_GOING_OFFLINE:
		ret = fmem_mem_going_offline_callback(arg);
		break;
	case MEM_OFFLINE:
		fmem_mem_offline_callback(arg);
		break;
	case MEM_GOING_ONLINE:
	case MEM_CANCEL_ONLINE:
	case MEM_CANCEL_OFFLINE:
		break;
	}
	if (ret)
		ret = notifier_from_errno(ret);
	else
		ret = NOTIFY_OK;
	return ret;
}
#endif

static int __init fmem_init(void)
{
#ifdef CONFIG_MEMORY_HOTPLUG
	hotplug_memory_notifier(fmem_memory_callback, 0);
#endif
	return platform_driver_register(&fmem_driver);
}

static void __exit fmem_exit(void)
{
	platform_driver_unregister(&fmem_driver);
}

struct fmem_data *fmem_get_info(void)
{
	return &fmem_data;
}
EXPORT_SYMBOL(fmem_get_info);

void lock_fmem_state(void)
{
	spin_lock(&fmem_state_lock);
}

void unlock_fmem_state(void)
{
	spin_unlock(&fmem_state_lock);
}

int fmem_set_state(enum fmem_state new_state)
{
	int ret = 0;
	int create_sysfs = 0;

	lock_fmem_state();
	if (fmem_state == new_state)
		goto out;

	if (fmem_state == FMEM_UNINITIALIZED) {
		if (new_state == FMEM_T_STATE) {
			tmem_enable();
			create_sysfs = 1;
			goto out_set;
		} else {
			ret = -EINVAL;
			goto out;
		}
	}

#ifdef CONFIG_MEMORY_HOTPLUG
	if (fmem_state == FMEM_C_STATE && new_state == FMEM_O_STATE) {
		ret = -EAGAIN;
		goto out;
	}

	if (fmem_state == FMEM_O_STATE && new_state == FMEM_C_STATE) {
		pr_warn("attempting to use powered off memory as fmem\n");
		ret = -EAGAIN;
		goto out;
	}
#endif

	if (new_state == FMEM_T_STATE) {
		void *v;
		v = fmem_map_virtual_area(MT_DEVICE_CACHED);
		if (IS_ERR_OR_NULL(v)) {
			ret = PTR_ERR(v);
			goto out;
		}
		tmem_enable();
	} else {
		tmem_disable();
		fmem_unmap_virtual_area();
	}

out_set:
	fmem_state = new_state;
out:
	unlock_fmem_state();
#ifdef CONFIG_SYSFS
	if (create_sysfs)
		fmem_create_sysfs();
#endif
	return ret;
}
EXPORT_SYMBOL(fmem_set_state);

arch_initcall(fmem_init);
module_exit(fmem_exit);
