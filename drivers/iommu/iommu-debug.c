/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/qcom_iommu.h>

#ifdef CONFIG_IOMMU_DEBUG_TRACKING

static DEFINE_MUTEX(iommu_debug_attachments_lock);
static LIST_HEAD(iommu_debug_attachments);
static struct dentry *debugfs_attachments_dir;

struct iommu_debug_attachment {
	struct iommu_domain *domain;
	struct device *dev;
	struct dentry *dentry;
	struct list_head list;
	unsigned long reg_offset;
};

static int iommu_debug_attachment_info_show(struct seq_file *s, void *ignored)
{
	struct iommu_debug_attachment *attach = s->private;
	phys_addr_t pt_phys;
	int coherent_htw_disable;
	int secure_vmid;

	seq_printf(s, "Domain: 0x%p\n", attach->domain);
	if (iommu_domain_get_attr(attach->domain, DOMAIN_ATTR_PT_BASE_ADDR,
				  &pt_phys)) {
		seq_puts(s, "PT_BASE_ADDR: (Unknown)\n");
	} else {
		void *pt_virt = phys_to_virt(pt_phys);

		seq_printf(s, "PT_BASE_ADDR: virt=0x%p phys=%pa\n",
			   pt_virt, &pt_phys);
	}

	seq_puts(s, "COHERENT_HTW_DISABLE: ");
	if (iommu_domain_get_attr(attach->domain,
				  DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				  &coherent_htw_disable))
		seq_puts(s, "(Unknown)\n");
	else
		seq_printf(s, "%d\n", coherent_htw_disable);

	seq_puts(s, "SECURE_VMID: ");
	if (iommu_domain_get_attr(attach->domain,
				  DOMAIN_ATTR_SECURE_VMID,
				  &secure_vmid))
		seq_puts(s, "(Unknown)\n");
	else
		seq_printf(s, "%s (0x%x)\n",
			   msm_secure_vmid_to_string(secure_vmid), secure_vmid);

	return 0;
}

static int iommu_debug_attachment_info_open(struct inode *inode,
					    struct file *file)
{
	return single_open(file, iommu_debug_attachment_info_show,
			   inode->i_private);
}

static const struct file_operations iommu_debug_attachment_info_fops = {
	.open	 = iommu_debug_attachment_info_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

static ssize_t iommu_debug_attachment_trigger_fault_write(
	struct file *file, const char __user *ubuf, size_t count,
	loff_t *offset)
{
	struct iommu_debug_attachment *attach = file->private_data;
	unsigned long flags;

	if (kstrtoul_from_user(ubuf, count, 0, &flags)) {
		pr_err("Invalid flags format\n");
		return -EFAULT;
	}

	iommu_trigger_fault(attach->domain, flags);

	return count;
}

static const struct file_operations
iommu_debug_attachment_trigger_fault_fops = {
	.open	= simple_open,
	.write	= iommu_debug_attachment_trigger_fault_write,
};

static ssize_t iommu_debug_attachment_reg_offset_write(
	struct file *file, const char __user *ubuf, size_t count,
	loff_t *offset)
{
	struct iommu_debug_attachment *attach = file->private_data;
	unsigned long reg_offset;

	if (kstrtoul_from_user(ubuf, count, 0, &reg_offset)) {
		pr_err("Invalid reg_offset format\n");
		return -EFAULT;
	}

	attach->reg_offset = reg_offset;

	return count;
}

static const struct file_operations iommu_debug_attachment_reg_offset_fops = {
	.open	= simple_open,
	.write	= iommu_debug_attachment_reg_offset_write,
};

static ssize_t iommu_debug_attachment_reg_read_read(
	struct file *file, char __user *ubuf, size_t count, loff_t *offset)
{
	struct iommu_debug_attachment *attach = file->private_data;
	unsigned long val;
	char *val_str;
	ssize_t val_str_len;

	if (*offset)
		return 0;

	val = iommu_reg_read(attach->domain, attach->reg_offset);
	val_str = kasprintf(GFP_KERNEL, "0x%lx\n", val);
	if (!val_str)
		return -ENOMEM;
	val_str_len = strlen(val_str);

	if (copy_to_user(ubuf, val_str, val_str_len)) {
		pr_err("copy_to_user failed\n");
		val_str_len = -EFAULT;
		goto out;
	}
	*offset = 1;		/* non-zero means we're done */

out:
	kfree(val_str);
	return val_str_len;
}

static const struct file_operations iommu_debug_attachment_reg_read_fops = {
	.open	= simple_open,
	.read	= iommu_debug_attachment_reg_read_read,
};

static ssize_t iommu_debug_attachment_reg_write_write(
	struct file *file, const char __user *ubuf, size_t count,
	loff_t *offset)
{
	struct iommu_debug_attachment *attach = file->private_data;
	unsigned long val;

	if (kstrtoul_from_user(ubuf, count, 0, &val)) {
		pr_err("Invalid val format\n");
		return -EFAULT;
	}

	iommu_reg_write(attach->domain, attach->reg_offset, val);

	return count;
}

static const struct file_operations iommu_debug_attachment_reg_write_fops = {
	.open	= simple_open,
	.write	= iommu_debug_attachment_reg_write_write,
};

/* should be called with iommu_debug_attachments_lock locked */
static int iommu_debug_attach_add_debugfs(
	struct iommu_debug_attachment *attach)
{
	const char *attach_name;
	struct device *dev = attach->dev;
	struct iommu_domain *domain = attach->domain;
	int is_dynamic;

	if (iommu_domain_get_attr(domain, DOMAIN_ATTR_DYNAMIC, &is_dynamic))
		is_dynamic = 0;

	if (is_dynamic) {
		uuid_le uuid;

		uuid_le_gen(&uuid);
		attach_name = kasprintf(GFP_KERNEL, "%s-%pUl", dev_name(dev),
					uuid.b);
		if (!attach_name)
			return -ENOMEM;
	} else {
		attach_name = dev_name(dev);
	}

	attach->dentry = debugfs_create_dir(attach_name,
					    debugfs_attachments_dir);
	if (!attach->dentry) {
		pr_err("Couldn't create iommu/attachments/%s debugfs directory for domain 0x%p\n",
		       attach_name, domain);
		if (is_dynamic)
			kfree(attach_name);
		return -EIO;
	}

	if (is_dynamic)
		kfree(attach_name);

	if (!debugfs_create_file(
		    "info", S_IRUSR, attach->dentry, attach,
		    &iommu_debug_attachment_info_fops)) {
		pr_err("Couldn't create iommu/attachments/%s/info debugfs file for domain 0x%p\n",
		       dev_name(dev), domain);
		goto err_rmdir;
	}

	if (!debugfs_create_file(
		    "trigger_fault", S_IRUSR, attach->dentry, attach,
		    &iommu_debug_attachment_trigger_fault_fops)) {
		pr_err("Couldn't create iommu/attachments/%s/trigger_fault debugfs file for domain 0x%p\n",
		       dev_name(dev), domain);
		goto err_rmdir;
	}

	if (!debugfs_create_file(
		    "reg_offset", S_IRUSR, attach->dentry, attach,
		    &iommu_debug_attachment_reg_offset_fops)) {
		pr_err("Couldn't create iommu/attachments/%s/reg_offset debugfs file for domain 0x%p\n",
		       dev_name(dev), domain);
		goto err_rmdir;
	}

	if (!debugfs_create_file(
		    "reg_read", S_IRUSR, attach->dentry, attach,
		    &iommu_debug_attachment_reg_read_fops)) {
		pr_err("Couldn't create iommu/attachments/%s/reg_read debugfs file for domain 0x%p\n",
		       dev_name(dev), domain);
		goto err_rmdir;
	}

	if (!debugfs_create_file(
		    "reg_write", S_IRUSR, attach->dentry, attach,
		    &iommu_debug_attachment_reg_write_fops)) {
		pr_err("Couldn't create iommu/attachments/%s/reg_write debugfs file for domain 0x%p\n",
		       dev_name(dev), domain);
		goto err_rmdir;
	}

	return 0;

err_rmdir:
	debugfs_remove_recursive(attach->dentry);
	return -EIO;
}

void iommu_debug_domain_add(struct iommu_domain *domain)
{
	struct iommu_debug_attachment *attach;

	mutex_lock(&iommu_debug_attachments_lock);

	attach = kmalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		goto out_unlock;

	attach->domain = domain;
	attach->dev = NULL;
	list_add(&attach->list, &iommu_debug_attachments);

out_unlock:
	mutex_unlock(&iommu_debug_attachments_lock);
}

void iommu_debug_domain_remove(struct iommu_domain *domain)
{
	struct iommu_debug_attachment *it;

	mutex_lock(&iommu_debug_attachments_lock);
	list_for_each_entry(it, &iommu_debug_attachments, list)
		if (it->domain == domain && it->dev == NULL)
			break;

	if (&it->list == &iommu_debug_attachments) {
		WARN(1, "Couldn't find debug attachment for domain=0x%p",
				domain);
	} else {
		list_del(&it->list);
		kfree(it);
	}
	mutex_unlock(&iommu_debug_attachments_lock);
}

void iommu_debug_attach_device(struct iommu_domain *domain,
			       struct device *dev)
{
	struct iommu_debug_attachment *attach;

	mutex_lock(&iommu_debug_attachments_lock);

	list_for_each_entry(attach, &iommu_debug_attachments, list)
		if (attach->domain == domain && attach->dev == NULL)
			break;

	if (&attach->list == &iommu_debug_attachments) {
		WARN(1, "Couldn't find debug attachment for domain=0x%p dev=%s",
		     domain, dev_name(dev));
	} else {
		attach->dev = dev;

		/*
		 * we might not init until after other drivers start calling
		 * iommu_attach_device. Only set up the debugfs nodes if we've
		 * already init'd to avoid polluting the top-level debugfs
		 * directory (by calling debugfs_create_dir with a NULL
		 * parent). These will be flushed out later once we init.
		 */

		if (debugfs_attachments_dir)
			iommu_debug_attach_add_debugfs(attach);
	}

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
		/*
		 * Just remove debugfs entry and mark dev as NULL on
		 * iommu_detach call. We would remove the actual
		 * attachment entry from the list only on domain_free call.
		 * This is to ensure we keep track of unattached domains too.
		 */

		debugfs_remove_recursive(it->dentry);
		it->dev = NULL;
	}
	mutex_unlock(&iommu_debug_attachments_lock);
}

static int iommu_debug_init_tracking(void)
{
	int ret = 0;
	struct iommu_debug_attachment *attach;

	mutex_lock(&iommu_debug_attachments_lock);
	debugfs_attachments_dir = debugfs_create_dir("attachments",
						     iommu_debugfs_top);
	if (!debugfs_attachments_dir) {
		pr_err("Couldn't create iommu/attachments debugfs directory\n");
		ret = -ENODEV;
		goto out_unlock;
	}

	/* set up debugfs entries for attachments made during early boot */
	list_for_each_entry(attach, &iommu_debug_attachments, list)
		if (attach->dev)
			iommu_debug_attach_add_debugfs(attach);

out_unlock:
	mutex_unlock(&iommu_debug_attachments_lock);
	return ret;
}

static void iommu_debug_destroy_tracking(void)
{
	debugfs_remove_recursive(debugfs_attachments_dir);
}
#else
static inline int iommu_debug_init_tracking(void) { return 0; }
static inline void iommu_debug_destroy_tracking(void) { }
#endif

#ifdef CONFIG_IOMMU_TESTS

static LIST_HEAD(iommu_debug_devices);
static struct dentry *debugfs_tests_dir;
static u32 iters_per_op = 1;

struct iommu_debug_device {
	struct device *dev;
	struct iommu_domain *domain;
	u64 iova;
	u64 phys;
	size_t len;
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

static int nr_iters_set(void *data, u64 val)
{
	if (!val)
		val = 1;
	if (val > 10000)
		val = 10000;
	*(u32 *)data = val;
	return 0;
}

static int nr_iters_get(void *data, u64 *val)
{
	*val = *(u32 *)data;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(iommu_debug_nr_iters_ops,
			nr_iters_get, nr_iters_set, "%llu\n");

static void iommu_debug_device_profiling(struct seq_file *s, struct device *dev,
					 bool secure)
{
	unsigned long sizes[] = { SZ_4K, SZ_64K, SZ_2M, SZ_1M * 12,
				  SZ_1M * 20, 0 };
	unsigned long *sz;
	struct iommu_domain *domain;
	struct bus_type *bus;
	unsigned long iova = 0x10000;
	phys_addr_t paddr = 0xa000;
	int htw_disable = 1, atomic_domain = 1;

	bus = msm_iommu_get_bus(dev);
	if (!bus)
		return;

	domain = iommu_domain_alloc(bus);
	if (!domain) {
		seq_puts(s, "Couldn't allocate domain\n");
		return;
	}

	if (iommu_domain_set_attr(domain, DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				  &htw_disable)) {
		seq_puts(s, "Couldn't disable coherent htw\n");
		goto out_domain_free;
	}

	if (iommu_domain_set_attr(domain, DOMAIN_ATTR_ATOMIC,
				  &atomic_domain)) {
		seq_printf(s, "Couldn't set atomic_domain to %d\n",
			   atomic_domain);
		goto out_domain_free;
	}

	if (secure) {
		int secure_vmid = VMID_CP_PIXEL;

		if (iommu_domain_set_attr(domain, DOMAIN_ATTR_SECURE_VMID,
					  &secure_vmid)) {
			seq_printf(s, "Couldn't set secure vmid to %d\n",
				   secure_vmid);
			goto out_domain_free;
		}
	}

	if (iommu_attach_device(domain, dev)) {
		seq_puts(s,
			 "Couldn't attach new domain to device. Is it already attached?\n");
		goto out_domain_free;
	}

	seq_printf(s, "(average over %d iterations)\n", iters_per_op);
	seq_printf(s, "%8s %19s %16s\n", "size", "iommu_map", "iommu_unmap");
	for (sz = sizes; *sz; ++sz) {
		unsigned long size = *sz;
		size_t unmapped;
		u64 map_elapsed_ns = 0, unmap_elapsed_ns = 0;
		u64 map_elapsed_us = 0, unmap_elapsed_us = 0;
		u32 map_elapsed_rem = 0, unmap_elapsed_rem = 0;
		struct timespec tbefore, tafter, diff;
		int i;

		for (i = 0; i < iters_per_op; ++i) {
			getnstimeofday(&tbefore);
			if (iommu_map(domain, iova, paddr, size,
				      IOMMU_READ | IOMMU_WRITE)) {
				seq_puts(s, "Failed to map\n");
				continue;
			}
			getnstimeofday(&tafter);
			diff = timespec_sub(tafter, tbefore);
			map_elapsed_ns += timespec_to_ns(&diff);

			getnstimeofday(&tbefore);
			unmapped = iommu_unmap(domain, iova, size);
			if (unmapped != size) {
				seq_printf(s,
					   "Only unmapped %zx instead of %zx\n",
					   unmapped, size);
				continue;
			}
			getnstimeofday(&tafter);
			diff = timespec_sub(tafter, tbefore);
			unmap_elapsed_ns += timespec_to_ns(&diff);
		}

		map_elapsed_ns /= iters_per_op;
		unmap_elapsed_ns /= iters_per_op;

		map_elapsed_us = div_u64_rem(map_elapsed_ns, 1000,
						&map_elapsed_rem);
		unmap_elapsed_us = div_u64_rem(unmap_elapsed_ns, 1000,
						&unmap_elapsed_rem);

		seq_printf(s, "%8s %12lld.%03d us %9lld.%03d us\n",
			_size_to_string(size),
			map_elapsed_us, map_elapsed_rem,
			unmap_elapsed_us, unmap_elapsed_rem);
	}

	seq_putc(s, '\n');
	seq_printf(s, "%8s %19s %16s\n", "size", "iommu_map_sg", "iommu_unmap");
	for (sz = sizes; *sz; ++sz) {
		unsigned long size = *sz;
		size_t unmapped;
		u64 map_elapsed_ns = 0, unmap_elapsed_ns = 0;
		u64 map_elapsed_us = 0, unmap_elapsed_us = 0;
		u32 map_elapsed_rem = 0, unmap_elapsed_rem = 0;
		struct timespec tbefore, tafter, diff;
		struct sg_table table;
		unsigned long chunk_size = SZ_4K;
		int i;

		if (iommu_debug_build_phoney_sg_table(dev, &table, size,
						      chunk_size)) {
			seq_puts(s,
				"couldn't build phoney sg table! bailing...\n");
			goto out_detach;
		}

		for (i = 0; i < iters_per_op; ++i) {
			getnstimeofday(&tbefore);
			if (iommu_map_sg(domain, iova, table.sgl, table.nents,
					 IOMMU_READ | IOMMU_WRITE) != size) {
				seq_puts(s, "Failed to map_sg\n");
				goto next;
			}
			getnstimeofday(&tafter);
			diff = timespec_sub(tafter, tbefore);
			map_elapsed_ns += timespec_to_ns(&diff);

			getnstimeofday(&tbefore);
			unmapped = iommu_unmap(domain, iova, size);
			if (unmapped != size) {
				seq_printf(s,
					   "Only unmapped %zx instead of %zx\n",
					   unmapped, size);
				goto next;
			}
			getnstimeofday(&tafter);
			diff = timespec_sub(tafter, tbefore);
			unmap_elapsed_ns += timespec_to_ns(&diff);
		}

		map_elapsed_ns /= iters_per_op;
		unmap_elapsed_ns /= iters_per_op;

		map_elapsed_us = div_u64_rem(map_elapsed_ns, 1000,
						&map_elapsed_rem);
		unmap_elapsed_us = div_u64_rem(unmap_elapsed_ns, 1000,
						&unmap_elapsed_rem);

		seq_printf(s, "%8s %12lld.%03d us %9lld.%03d us\n",
			_size_to_string(size),
			map_elapsed_us, map_elapsed_rem,
			unmap_elapsed_us, unmap_elapsed_rem);

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

	iommu_debug_device_profiling(s, ddev->dev, false);

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

static int iommu_debug_secure_profiling_show(struct seq_file *s, void *ignored)
{
	struct iommu_debug_device *ddev = s->private;

	iommu_debug_device_profiling(s, ddev->dev, true);

	return 0;
}

static int iommu_debug_secure_profiling_open(struct inode *inode,
					     struct file *file)
{
	return single_open(file, iommu_debug_secure_profiling_show,
			   inode->i_private);
}

static const struct file_operations iommu_debug_secure_profiling_fops = {
	.open	 = iommu_debug_secure_profiling_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

static int iommu_debug_attach_do_attach(struct iommu_debug_device *ddev,
					int val, bool is_secure)
{
	int htw_disable = 1;
	struct bus_type *bus;

	bus = msm_iommu_get_bus(ddev->dev);
	if (!bus)
		return -EINVAL;

	ddev->domain = iommu_domain_alloc(bus);
	if (!ddev->domain) {
		pr_err("Couldn't allocate domain\n");
		return -ENOMEM;
	}

	if (iommu_domain_set_attr(ddev->domain,
				  DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				  &htw_disable)) {
		pr_err("Couldn't disable coherent htw\n");
		goto out_domain_free;
	}

	if (is_secure && iommu_domain_set_attr(ddev->domain,
					       DOMAIN_ATTR_SECURE_VMID,
					       &val)) {
		pr_err("Couldn't set secure vmid to %d\n", val);
		goto out_domain_free;
	}

	if (iommu_attach_device(ddev->domain, ddev->dev)) {
		pr_err("Couldn't attach new domain to device. Is it already attached?\n");
		goto out_domain_free;
	}

	return 0;

out_domain_free:
	iommu_domain_free(ddev->domain);
	ddev->domain = NULL;
	return -EIO;
}

static ssize_t __iommu_debug_attach_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset,
					  bool is_secure)
{
	struct iommu_debug_device *ddev = file->private_data;
	ssize_t retval;
	int val;

	if (kstrtoint_from_user(ubuf, count, 0, &val)) {
		pr_err("Invalid format. Expected a hex or decimal integer");
		retval = -EFAULT;
		goto out;
	}

	if (val) {
		if (ddev->domain) {
			pr_err("Already attached.\n");
			retval = -EINVAL;
			goto out;
		}
		if (WARN(ddev->dev->archdata.iommu,
			 "Attachment tracking out of sync with device\n")) {
			retval = -EINVAL;
			goto out;
		}
		if (iommu_debug_attach_do_attach(ddev, val, is_secure)) {
			retval = -EIO;
			goto out;
		}
		pr_err("Attached\n");
	} else {
		if (!ddev->domain) {
			pr_err("No domain. Did you already attach?\n");
			retval = -EINVAL;
			goto out;
		}
		iommu_detach_device(ddev->domain, ddev->dev);
		iommu_domain_free(ddev->domain);
		ddev->domain = NULL;
		pr_err("Detached\n");
	}

	retval = count;
out:
	return retval;
}

static ssize_t iommu_debug_attach_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	return __iommu_debug_attach_write(file, ubuf, count, offset,
					  false);

}

static ssize_t iommu_debug_attach_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	char c[2];

	if (*offset)
		return 0;

	c[0] = ddev->domain ? '1' : '0';
	c[1] = '\n';
	if (copy_to_user(ubuf, &c, 2)) {
		pr_err("copy_to_user failed\n");
		return -EFAULT;
	}
	*offset = 1;		/* non-zero means we're done */

	return 2;
}

static const struct file_operations iommu_debug_attach_fops = {
	.open	= simple_open,
	.write	= iommu_debug_attach_write,
	.read	= iommu_debug_attach_read,
};

static ssize_t iommu_debug_attach_write_secure(struct file *file,
					       const char __user *ubuf,
					       size_t count, loff_t *offset)
{
	return __iommu_debug_attach_write(file, ubuf, count, offset,
					  true);

}

static const struct file_operations iommu_debug_secure_attach_fops = {
	.open	= simple_open,
	.write	= iommu_debug_attach_write_secure,
	.read	= iommu_debug_attach_read,
};

static ssize_t iommu_debug_atos_write(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	dma_addr_t iova;

	if (kstrtoll_from_user(ubuf, count, 0, &iova)) {
		pr_err("Invalid format for iova\n");
		ddev->iova = 0;
		return -EINVAL;
	}

	ddev->iova = iova;
	pr_err("Saved iova=%pa for future ATOS commands\n", &iova);
	return count;
}

static ssize_t iommu_debug_atos_read(struct file *file, char __user *ubuf,
				     size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	phys_addr_t phys;
	char buf[100];
	ssize_t retval;
	size_t buflen;

	if (!ddev->domain) {
		pr_err("No domain. Did you already attach?\n");
		return -EINVAL;
	}

	if (*offset)
		return 0;

	memset(buf, 0, 100);

	phys = iommu_iova_to_phys_hard(ddev->domain, ddev->iova);
	if (!phys)
		strlcpy(buf, "FAIL\n", 100);
	else
		snprintf(buf, 100, "%pa\n", &phys);

	buflen = strlen(buf);
	if (copy_to_user(ubuf, buf, buflen)) {
		pr_err("Couldn't copy_to_user\n");
		retval = -EFAULT;
	} else {
		*offset = 1;	/* non-zero means we're done */
		retval = buflen;
	}

	return retval;
}

static const struct file_operations iommu_debug_atos_fops = {
	.open	= simple_open,
	.write	= iommu_debug_atos_write,
	.read	= iommu_debug_atos_read,
};

static ssize_t iommu_debug_map_write(struct file *file, const char __user *ubuf,
				     size_t count, loff_t *offset)
{
	ssize_t retval;
	int ret;
	char *comma1, *comma2, *comma3;
	char buf[100];
	dma_addr_t iova;
	phys_addr_t phys;
	size_t size;
	int prot;
	struct iommu_debug_device *ddev = file->private_data;

	if (count >= 100) {
		pr_err("Value too large\n");
		return -EINVAL;
	}

	if (!ddev->domain) {
		pr_err("No domain. Did you already attach?\n");
		return -EINVAL;
	}

	memset(buf, 0, 100);

	if (copy_from_user(buf, ubuf, count)) {
		pr_err("Couldn't copy from user\n");
		retval = -EFAULT;
	}

	comma1 = strnchr(buf, count, ',');
	if (!comma1)
		goto invalid_format;

	comma2 = strnchr(comma1 + 1, count, ',');
	if (!comma2)
		goto invalid_format;

	comma3 = strnchr(comma2 + 1, count, ',');
	if (!comma3)
		goto invalid_format;

	/* split up the words */
	*comma1 = *comma2 = *comma3 = '\0';

	if (kstrtou64(buf, 0, &iova))
		goto invalid_format;

	if (kstrtou64(comma1 + 1, 0, &phys))
		goto invalid_format;

	if (kstrtoul(comma2 + 1, 0, &size))
		goto invalid_format;

	if (kstrtoint(comma3 + 1, 0, &prot))
		goto invalid_format;

	ret = iommu_map(ddev->domain, iova, phys, size, prot);
	if (ret) {
		pr_err("iommu_map failed with %d\n", ret);
		retval = -EIO;
		goto out;
	}

	retval = count;
	pr_err("Mapped %pa to %pa (len=0x%zx, prot=0x%x)\n",
	       &iova, &phys, size, prot);
out:
	return retval;

invalid_format:
	pr_err("Invalid format. Expected: iova,phys,len,prot where `prot' is the bitwise OR of IOMMU_READ, IOMMU_WRITE, etc.\n");
	return retval;
}

static const struct file_operations iommu_debug_map_fops = {
	.open	= simple_open,
	.write	= iommu_debug_map_write,
};

static ssize_t iommu_debug_unmap_write(struct file *file,
				       const char __user *ubuf,
				       size_t count, loff_t *offset)
{
	ssize_t retval;
	char *comma1;
	char buf[100];
	dma_addr_t iova;
	size_t size;
	size_t unmapped;
	struct iommu_debug_device *ddev = file->private_data;

	if (count >= 100) {
		pr_err("Value too large\n");
		return -EINVAL;
	}

	if (!ddev->domain) {
		pr_err("No domain. Did you already attach?\n");
		return -EINVAL;
	}

	memset(buf, 0, 100);

	if (copy_from_user(buf, ubuf, count)) {
		pr_err("Couldn't copy from user\n");
		retval = -EFAULT;
		goto out;
	}

	comma1 = strnchr(buf, count, ',');
	if (!comma1)
		goto invalid_format;

	/* split up the words */
	*comma1 = '\0';

	if (kstrtou64(buf, 0, &iova))
		goto invalid_format;

	if (kstrtoul(comma1 + 1, 0, &size))
		goto invalid_format;

	unmapped = iommu_unmap(ddev->domain, iova, size);
	if (unmapped != size) {
		pr_err("iommu_unmap failed. Expected to unmap: 0x%zx, unmapped: 0x%zx",
		       size, unmapped);
		return -EIO;
	}

	retval = count;
	pr_err("Unmapped %pa (len=0x%zx)\n", &iova, size);
out:
	return retval;

invalid_format:
	pr_err("Invalid format. Expected: iova,len\n");
	return retval;
}

static const struct file_operations iommu_debug_unmap_fops = {
	.open	= simple_open,
	.write	= iommu_debug_unmap_write,
};

/*
 * The following will only work for drivers that implement the generic
 * device tree bindings described in
 * Documentation/devicetree/bindings/iommu/iommu.txt
 */
static int snarf_iommu_devices(struct device *dev, const char *name)
{
	struct iommu_debug_device *ddev;
	struct dentry *dir;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	ddev = kzalloc(sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENODEV;
	ddev->dev = dev;
	dir = debugfs_create_dir(name, debugfs_tests_dir);
	if (!dir) {
		pr_err("Couldn't create iommu/devices/%s debugfs dir\n",
		       name);
		goto err;
	}

	if (!debugfs_create_file("nr_iters", S_IRUSR, dir, &iters_per_op,
				&iommu_debug_nr_iters_ops)) {
		pr_err("Couldn't create iommu/devices/%s/nr_iters debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("profiling", S_IRUSR, dir, ddev,
				 &iommu_debug_profiling_fops)) {
		pr_err("Couldn't create iommu/devices/%s/profiling debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("secure_profiling", S_IRUSR, dir, ddev,
				 &iommu_debug_secure_profiling_fops)) {
		pr_err("Couldn't create iommu/devices/%s/secure_profiling debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("attach", S_IRUSR, dir, ddev,
				 &iommu_debug_attach_fops)) {
		pr_err("Couldn't create iommu/devices/%s/attach debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("secure_attach", S_IRUSR, dir, ddev,
				 &iommu_debug_secure_attach_fops)) {
		pr_err("Couldn't create iommu/devices/%s/secure_attach debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("atos", S_IWUSR, dir, ddev,
				 &iommu_debug_atos_fops)) {
		pr_err("Couldn't create iommu/devices/%s/atos debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("map", S_IWUSR, dir, ddev,
				 &iommu_debug_map_fops)) {
		pr_err("Couldn't create iommu/devices/%s/map debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("unmap", S_IWUSR, dir, ddev,
				 &iommu_debug_unmap_fops)) {
		pr_err("Couldn't create iommu/devices/%s/unmap debugfs file\n",
		       name);
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

static int pass_iommu_devices(struct device *dev, void *ignored)
{
	if (!of_find_property(dev->of_node, "iommus", NULL))
		return 0;

	return snarf_iommu_devices(dev, dev_name(dev));
}

static int iommu_debug_populate_devices(void)
{
	int ret;
	struct device_node *np;
	const char *cb_name;

	for_each_compatible_node(np, NULL, "qcom,msm-smmu-v2-ctx") {
		ret = of_property_read_string(np, "label", &cb_name);
		if (ret)
			return ret;

		ret = snarf_iommu_devices(msm_iommu_get_ctx(cb_name), cb_name);
		if (ret)
			return ret;
	}

	return bus_for_each_dev(&platform_bus_type, NULL, NULL,
			pass_iommu_devices);
}

static int iommu_debug_init_tests(void)
{
	debugfs_tests_dir = debugfs_create_dir("tests",
					       iommu_debugfs_top);
	if (!debugfs_tests_dir) {
		pr_err("Couldn't create iommu/tests debugfs directory\n");
		return -ENODEV;
	}

	return iommu_debug_populate_devices();
}

static void iommu_debug_destroy_tests(void)
{
	debugfs_remove_recursive(debugfs_tests_dir);
}
#else
static inline int iommu_debug_init_tests(void) { return 0; }
static inline void iommu_debug_destroy_tests(void) { }
#endif

static int iommu_debug_init(void)
{
	if (iommu_debug_init_tracking())
		return -ENODEV;

	if (iommu_debug_init_tests())
		return -ENODEV;

	return 0;
}

static void iommu_debug_exit(void)
{
	iommu_debug_destroy_tracking();
	iommu_debug_destroy_tests();
}

module_init(iommu_debug_init);
module_exit(iommu_debug_exit);
