/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "iommu-debug: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

static struct dentry *debugfs_top_dir;

#ifdef CONFIG_IOMMU_DEBUG_TRACKING

static DEFINE_MUTEX(iommu_debug_attachments_lock);
static LIST_HEAD(iommu_debug_attachments);
static struct dentry *debugfs_attachments_dir;

struct iommu_debug_attachment {
	struct iommu_domain *domain;
	struct device *dev;
	struct dentry *dentry;
	struct list_head list;
};

static int iommu_debug_attachment_show(struct seq_file *s, void *ignored)
{
	struct iommu_debug_attachment *attach = s->private;
	phys_addr_t pt_phys;

	seq_printf(s, "Domain: 0x%p\n", attach->domain);
	if (iommu_domain_get_attr(attach->domain, DOMAIN_ATTR_PT_BASE_ADDR,
				  &pt_phys)) {
		seq_puts(s, "PT_BASE_ADDR: (Unknown)\n");
	} else {
		void *pt_virt = phys_to_virt(pt_phys);

		seq_printf(s, "PT_BASE_ADDR: virt=0x%p phys=%pa\n",
			   pt_virt, &pt_phys);
	}

	return 0;
}

static int iommu_debug_attachment_open(struct inode *inode, struct file *file)
{
	return single_open(file, iommu_debug_attachment_show, inode->i_private);
}

static const struct file_operations iommu_debug_attachment_fops = {
	.open	 = iommu_debug_attachment_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

void iommu_debug_attach_device(struct iommu_domain *domain,
			       struct device *dev)
{
	struct iommu_debug_attachment *attach;
	char *attach_name;
	uuid_le uuid;

	mutex_lock(&iommu_debug_attachments_lock);

	uuid_le_gen(&uuid);
	attach_name = kasprintf(GFP_KERNEL, "%s-%pUl", dev_name(dev), uuid.b);
	if (!attach_name)
		goto unlock;

	attach = kmalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		goto free_attach_name;

	attach->domain = domain;
	attach->dev = dev;

	attach->dentry = debugfs_create_file(
		attach_name, S_IRUSR, debugfs_attachments_dir, attach,
		&iommu_debug_attachment_fops);
	if (!attach->dentry) {
		pr_err("Couldn't create iommu/attachments/%s debugfs file for domain 0x%p\n",
		       attach_name, domain);
		kfree(attach);
		goto free_attach_name;
	}

	list_add(&attach->list, &iommu_debug_attachments);
free_attach_name:
	kfree(attach_name);
unlock:
	mutex_unlock(&iommu_debug_attachments_lock);
}

void iommu_debug_detach_device(struct iommu_domain *domain,
			       struct device *dev)
{
	struct iommu_debug_attachment *it;

	mutex_lock(&iommu_debug_attachments_lock);
	list_for_each_entry(it, &iommu_debug_attachments, list)
		if (it->domain == domain && it->dev == dev)
			break;

	if (&it->list == &iommu_debug_attachments) {
		WARN(1, "Couldn't find debug attachment for domain=0x%p dev=%s",
		     domain, dev_name(dev));
	} else {
		list_del(&it->list);
		debugfs_remove_recursive(it->dentry);
		kfree(it);
	}
	mutex_unlock(&iommu_debug_attachments_lock);
}

static int iommu_debug_init_tracking(void)
{
	debugfs_attachments_dir = debugfs_create_dir("attachments",
						     debugfs_top_dir);
	if (!debugfs_attachments_dir) {
		pr_err("Couldn't create iommu/attachments debugfs directory\n");
		return -ENODEV;
	}

	return 0;
}
#else
static inline int iommu_debug_init_tracking(void) { return 0; }
#endif

#ifdef CONFIG_IOMMU_TESTS

static LIST_HEAD(iommu_debug_devices);
static struct dentry *debugfs_tests_dir;

struct iommu_debug_device {
	struct device *dev;
	struct list_head list;
};

static int iommu_debug_build_phoney_sg_table(struct device *dev,
					     struct sg_table *table,
					     unsigned long total_size,
					     unsigned long chunk_size)
{
	unsigned long nents = total_size / chunk_size;
	struct scatterlist *sg;
	int i;
	struct page *page;

	BUG_ON(!IS_ALIGNED(total_size, PAGE_SIZE));
	BUG_ON(!IS_ALIGNED(total_size, chunk_size));
	BUG_ON(sg_alloc_table(table, nents, GFP_KERNEL));
	page = alloc_pages(GFP_KERNEL, get_order(chunk_size));
	if (!page)
		goto free_table;

	/* all the same page... why not. */
	for_each_sg(table->sgl, sg, table->nents, i)
		sg_set_page(sg, page, chunk_size, 0);

	return 0;

free_table:
	sg_free_table(table);
	return -ENOMEM;
}

static void iommu_debug_destroy_phoney_sg_table(struct device *dev,
						struct sg_table *table,
						unsigned long chunk_size)
{
	__free_pages(sg_page(table->sgl), get_order(chunk_size));
	sg_free_table(table);
}

static const char * const _size_to_string(unsigned long size)
{
	switch (size) {
	case SZ_4K:
		return "4K";
	case SZ_64K:
		return "64K";
	case SZ_2M:
		return "2M";
	case SZ_1M * 12:
		return "12M";
	case SZ_1M * 20:
		return "20M";
	}
	return "unknown size, please add to _size_to_string";
}

static void iommu_debug_device_profiling(struct seq_file *s, struct device *dev)
{
	unsigned long sizes[] = { SZ_4K, SZ_64K, SZ_2M, SZ_1M * 12,
				  SZ_1M * 20, 0 };
	unsigned long *sz;
	struct iommu_domain *domain;
	unsigned long iova = 0x10000;
	phys_addr_t paddr = 0xa000;
	int htw_disable = 1;

	domain = iommu_domain_alloc(&platform_bus_type);
	if (!domain) {
		seq_puts(s, "Couldn't allocate domain\n");
		return;
	}

	if (iommu_domain_set_attr(domain, DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				  &htw_disable)) {
		seq_puts(s, "Couldn't disable coherent htw\n");
		goto out_domain_free;
	}

	if (iommu_attach_device(domain, dev)) {
		seq_puts(s,
			 "Couldn't attach new domain to device. Is it already attached?\n");
		goto out_domain_free;
	}

	seq_printf(s, "%8s %15s %12s\n", "size", "iommu_map", "iommu_unmap");
	for (sz = sizes; *sz; ++sz) {
		unsigned long size = *sz;
		size_t unmapped;
		s64 map_elapsed_us, unmap_elapsed_us;
		struct timespec tbefore, tafter, diff;

		getnstimeofday(&tbefore);
		if (iommu_map(domain, iova, paddr, size,
			      IOMMU_READ | IOMMU_WRITE)) {
			seq_puts(s, "Failed to map\n");
			continue;
		}
		getnstimeofday(&tafter);
		diff = timespec_sub(tafter, tbefore);
		map_elapsed_us = div_s64(timespec_to_ns(&diff), 1000);

		getnstimeofday(&tbefore);
		unmapped = iommu_unmap(domain, iova, size);
		if (unmapped != size) {
			seq_printf(s, "Only unmapped %zx instead of %zx\n",
				unmapped, size);
			continue;
		}
		getnstimeofday(&tafter);
		diff = timespec_sub(tafter, tbefore);
		unmap_elapsed_us = div_s64(timespec_to_ns(&diff), 1000);

		seq_printf(s, "%8s %12lld us %9lld us\n", _size_to_string(size),
			map_elapsed_us, unmap_elapsed_us);
	}

	seq_putc(s, '\n');
	seq_printf(s, "%8s %15s %12s\n", "size", "iommu_map_sg", "iommu_unmap");
	for (sz = sizes; *sz; ++sz) {
		unsigned long size = *sz;
		size_t unmapped;
		s64 map_elapsed_us, unmap_elapsed_us;
		struct timespec tbefore, tafter, diff;
		struct sg_table table;
		unsigned long chunk_size = SZ_4K;

		if (iommu_debug_build_phoney_sg_table(dev, &table, size,
						      chunk_size)) {
			seq_puts(s,
				"couldn't build phoney sg table! bailing...\n");
			goto out_detach;
		}

		getnstimeofday(&tbefore);
		if (iommu_map_sg(domain, iova, table.sgl, table.nents,
				 IOMMU_READ | IOMMU_WRITE) != size) {
			seq_puts(s, "Failed to map_sg\n");
			goto next;
		}
		getnstimeofday(&tafter);
		diff = timespec_sub(tafter, tbefore);
		map_elapsed_us = div_s64(timespec_to_ns(&diff), 1000);

		getnstimeofday(&tbefore);
		unmapped = iommu_unmap(domain, iova, size);
		if (unmapped != size) {
			seq_printf(s, "Only unmapped %zx instead of %zx\n",
				unmapped, size);
			goto next;
		}
		getnstimeofday(&tafter);
		diff = timespec_sub(tafter, tbefore);
		unmap_elapsed_us = div_s64(timespec_to_ns(&diff), 1000);

		seq_printf(s, "%8s %12lld us %9lld us\n", _size_to_string(size),
			map_elapsed_us, unmap_elapsed_us);

next:
		iommu_debug_destroy_phoney_sg_table(dev, &table, chunk_size);
	}

out_detach:
	iommu_detach_device(domain, dev);
out_domain_free:
	iommu_domain_free(domain);
}

static int iommu_debug_profiling_show(struct seq_file *s, void *ignored)
{
	struct iommu_debug_device *ddev = s->private;

	iommu_debug_device_profiling(s, ddev->dev);

	return 0;
}

static int iommu_debug_profiling_open(struct inode *inode, struct file *file)
{
	return single_open(file, iommu_debug_profiling_show, inode->i_private);
}

static const struct file_operations iommu_debug_profiling_fops = {
	.open	 = iommu_debug_profiling_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

/*
 * The following will only work for drivers that implement the generic
 * device tree bindings described in
 * Documentation/devicetree/bindings/iommu/iommu.txt
 */
static int snarf_iommu_devices(struct device *dev, void *ignored)
{
	struct iommu_debug_device *ddev;
	struct dentry *dir, *profiling_dentry;

	if (!of_find_property(dev->of_node, "iommus", NULL))
		return 0;

	ddev = kmalloc(sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENODEV;
	ddev->dev = dev;
	dir = debugfs_create_dir(dev_name(dev), debugfs_tests_dir);
	if (!dir) {
		pr_err("Couldn't create iommu/devices/%s debugfs dir\n",
		       dev_name(dev));
		goto err;
	}
	profiling_dentry = debugfs_create_file("profiling", S_IRUSR, dir, ddev,
					       &iommu_debug_profiling_fops);
	if (!profiling_dentry) {
		pr_err("Couldn't create iommu/devices/%s/profiling debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	list_add(&ddev->list, &iommu_debug_devices);
	return 0;

err_rmdir:
	debugfs_remove_recursive(dir);
err:
	kfree(ddev);
	return 0;
}

static int iommu_debug_init_tests(void)
{
	debugfs_tests_dir = debugfs_create_dir("tests",
					       debugfs_top_dir);
	if (!debugfs_tests_dir) {
		pr_err("Couldn't create iommu/tests debugfs directory\n");
		return -ENODEV;
	}

	return bus_for_each_dev(&platform_bus_type, NULL, NULL,
				snarf_iommu_devices);
}
#else
static inline int iommu_debug_init_tests(void) { return 0; }
#endif

static int iommu_debug_init(void)
{
	debugfs_top_dir = debugfs_create_dir("iommu", NULL);
	if (!debugfs_top_dir) {
		pr_err("Couldn't create iommu debugfs directory\n");
		return -ENODEV;
	}

	if (iommu_debug_init_tracking())
		goto err;

	if (iommu_debug_init_tests())
		goto err;

	return 0;
err:
	debugfs_remove_recursive(debugfs_top_dir);
	return -ENODEV;
}

static void iommu_debug_exit(void)
{
	debugfs_remove_recursive(debugfs_top_dir);
}

module_init(iommu_debug_init);
module_exit(iommu_debug_exit);
