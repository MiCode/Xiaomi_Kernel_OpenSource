/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/dma-contiguous.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/qcom_iommu.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/dma-iommu.h>
#include "iommu-debug.h"

#if defined(CONFIG_IOMMU_DEBUG_TRACKING) || defined(CONFIG_IOMMU_TESTS)

static const char *iommu_debug_attr_to_string(enum iommu_attr attr)
{
	switch (attr) {
	case DOMAIN_ATTR_GEOMETRY:
		return "DOMAIN_ATTR_GEOMETRY";
	case DOMAIN_ATTR_PAGING:
		return "DOMAIN_ATTR_PAGING";
	case DOMAIN_ATTR_WINDOWS:
		return "DOMAIN_ATTR_WINDOWS";
	case DOMAIN_ATTR_FSL_PAMU_STASH:
		return "DOMAIN_ATTR_FSL_PAMU_STASH";
	case DOMAIN_ATTR_FSL_PAMU_ENABLE:
		return "DOMAIN_ATTR_FSL_PAMU_ENABLE";
	case DOMAIN_ATTR_FSL_PAMUV1:
		return "DOMAIN_ATTR_FSL_PAMUV1";
	case DOMAIN_ATTR_NESTING:
		return "DOMAIN_ATTR_NESTING";
	case DOMAIN_ATTR_COHERENT_HTW_DISABLE:
		return "DOMAIN_ATTR_COHERENT_HTW_DISABLE";
	case DOMAIN_ATTR_PT_BASE_ADDR:
		return "DOMAIN_ATTR_PT_BASE_ADDR";
	case DOMAIN_ATTR_SECURE_VMID:
		return "DOMAIN_ATTR_SECURE_VMID";
	case DOMAIN_ATTR_ATOMIC:
		return "DOMAIN_ATTR_ATOMIC";
	case DOMAIN_ATTR_CONTEXT_BANK:
		return "DOMAIN_ATTR_CONTEXT_BANK";
	case DOMAIN_ATTR_TTBR0:
		return "DOMAIN_ATTR_TTBR0";
	case DOMAIN_ATTR_CONTEXTIDR:
		return "DOMAIN_ATTR_CONTEXTIDR";
	case DOMAIN_ATTR_PROCID:
		return "DOMAIN_ATTR_PROCID";
	case DOMAIN_ATTR_DYNAMIC:
		return "DOMAIN_ATTR_DYNAMIC";
	case DOMAIN_ATTR_NON_FATAL_FAULTS:
		return "DOMAIN_ATTR_NON_FATAL_FAULTS";
	case DOMAIN_ATTR_S1_BYPASS:
		return "DOMAIN_ATTR_S1_BYPASS";
	case DOMAIN_ATTR_FAST:
		return "DOMAIN_ATTR_FAST";
	case DOMAIN_ATTR_EARLY_MAP:
		return "DOMAIN_ATTR_EARLY_MAP";
	default:
		return "Unknown attr!";
	}
}
#endif

#ifdef CONFIG_IOMMU_DEBUG_TRACKING

static DEFINE_MUTEX(iommu_debug_attachments_lock);
static LIST_HEAD(iommu_debug_attachments);

/*
 * Each group may have more than one domain; but each domain may
 * only have one group.
 * Used by debug tools to display the name of the device(s) associated
 * with a particular domain.
 */
struct iommu_debug_attachment {
	struct iommu_domain *domain;
	struct iommu_group *group;
	struct list_head list;
};

void iommu_debug_attach_device(struct iommu_domain *domain,
			       struct device *dev)
{
	struct iommu_debug_attachment *attach;
	struct iommu_group *group;

	group = iommu_group_get(dev);
	if (!group)
		return;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return;

	attach->domain = domain;
	attach->group = group;
	INIT_LIST_HEAD(&attach->list);

	mutex_lock(&iommu_debug_attachments_lock);
	list_add(&attach->list, &iommu_debug_attachments);
	mutex_unlock(&iommu_debug_attachments_lock);
}

void iommu_debug_domain_remove(struct iommu_domain *domain)
{
	struct iommu_debug_attachment *it, *tmp;

	mutex_lock(&iommu_debug_attachments_lock);
	list_for_each_entry_safe(it, tmp, &iommu_debug_attachments, list) {
		if (it->domain != domain)
			continue;
		list_del(&it->list);
		iommu_group_put(it->group);
		kfree(it);
	}

	mutex_unlock(&iommu_debug_attachments_lock);
}

#endif

#ifdef CONFIG_IOMMU_TESTS

#ifdef CONFIG_64BIT

#define kstrtoux kstrtou64
#define kstrtox_from_user kstrtoll_from_user
#define kstrtosize_t kstrtoul

#else

#define kstrtoux kstrtou32
#define kstrtox_from_user kstrtoint_from_user
#define kstrtosize_t kstrtouint

#endif

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
	struct mutex clk_lock;
	unsigned int clk_count;
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
	case SZ_8K:
		return "8K";
	case SZ_16K:
		return "16K";
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
					 enum iommu_attr attrs[],
					 void *attr_values[], int nattrs,
					 const size_t sizes[])
{
	int i;
	const size_t *sz;
	struct iommu_domain *domain;
	struct bus_type *bus;
	unsigned long iova = 0x10000;
	phys_addr_t paddr = 0xa000;

	bus = msm_iommu_get_bus(dev);
	if (!bus)
		return;

	domain = iommu_domain_alloc(bus);
	if (!domain) {
		seq_puts(s, "Couldn't allocate domain\n");
		return;
	}

	seq_puts(s, "Domain attributes: [ ");
	for (i = 0; i < nattrs; ++i) {
		/* not all attrs are ints, but this will get us by for now */
		seq_printf(s, "%s=%d%s", iommu_debug_attr_to_string(attrs[i]),
			   *((int *)attr_values[i]),
			   i < nattrs ? " " : "");
	}
	seq_puts(s, "]\n");
	for (i = 0; i < nattrs; ++i) {
		if (iommu_domain_set_attr(domain, attrs[i], attr_values[i])) {
			seq_printf(s, "Couldn't set %d to the value at %p\n",
				 attrs[i], attr_values[i]);
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
		size_t size = *sz;
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

		map_elapsed_ns = div_u64_rem(map_elapsed_ns, iters_per_op,
				&map_elapsed_rem);
		unmap_elapsed_ns = div_u64_rem(unmap_elapsed_ns, iters_per_op,
				&unmap_elapsed_rem);

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
		size_t size = *sz;
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

		map_elapsed_ns = div_u64_rem(map_elapsed_ns, iters_per_op,
				&map_elapsed_rem);
		unmap_elapsed_ns = div_u64_rem(unmap_elapsed_ns, iters_per_op,
				&unmap_elapsed_rem);

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
	const size_t sizes[] = { SZ_4K, SZ_64K, SZ_2M, SZ_1M * 12,
					SZ_1M * 20, 0 };
	enum iommu_attr attrs[] = {
		DOMAIN_ATTR_COHERENT_HTW_DISABLE,
		DOMAIN_ATTR_ATOMIC,
	};
	int htw_disable = 1, atomic = 1;
	void *attr_values[] = { &htw_disable, &atomic };

	iommu_debug_device_profiling(s, ddev->dev, attrs, attr_values,
				     ARRAY_SIZE(attrs), sizes);

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
	const size_t sizes[] = { SZ_4K, SZ_64K, SZ_2M, SZ_1M * 12,
					SZ_1M * 20, 0 };

	enum iommu_attr attrs[] = {
		DOMAIN_ATTR_COHERENT_HTW_DISABLE,
		DOMAIN_ATTR_ATOMIC,
		DOMAIN_ATTR_SECURE_VMID,
	};
	int one = 1, secure_vmid = VMID_CP_PIXEL;
	void *attr_values[] = { &one, &one, &secure_vmid };

	iommu_debug_device_profiling(s, ddev->dev, attrs, attr_values,
				     ARRAY_SIZE(attrs), sizes);

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

static int iommu_debug_profiling_fast_show(struct seq_file *s, void *ignored)
{
	struct iommu_debug_device *ddev = s->private;
	size_t sizes[] = {SZ_4K, SZ_8K, SZ_16K, SZ_64K, 0};
	enum iommu_attr attrs[] = {
		DOMAIN_ATTR_FAST,
		DOMAIN_ATTR_COHERENT_HTW_DISABLE,
		DOMAIN_ATTR_ATOMIC,
	};
	int one = 1;
	void *attr_values[] = { &one, &one, &one };

	iommu_debug_device_profiling(s, ddev->dev, attrs, attr_values,
				     ARRAY_SIZE(attrs), sizes);

	return 0;
}

static int iommu_debug_profiling_fast_open(struct inode *inode,
					   struct file *file)
{
	return single_open(file, iommu_debug_profiling_fast_show,
			   inode->i_private);
}

static const struct file_operations iommu_debug_profiling_fast_fops = {
	.open	 = iommu_debug_profiling_fast_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

static int iommu_debug_profiling_fast_dma_api_show(struct seq_file *s,
						 void *ignored)
{
	int i, experiment;
	struct iommu_debug_device *ddev = s->private;
	struct device *dev = ddev->dev;
	u64 map_elapsed_ns[10], unmap_elapsed_ns[10];
	struct dma_iommu_mapping *mapping;
	dma_addr_t dma_addr;
	void *virt;
	int fast = 1;
	const char * const extra_labels[] = {
		"not coherent",
		"coherent",
	};
	struct dma_attrs coherent_attrs;
	struct dma_attrs *extra_attrs[] = {
		NULL,
		&coherent_attrs,
	};

	init_dma_attrs(&coherent_attrs);
	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &coherent_attrs);

	virt = kmalloc(1518, GFP_KERNEL);
	if (!virt)
		goto out;

	mapping = arm_iommu_create_mapping(&platform_bus_type, 0, SZ_1G * 4UL);
	if (!mapping) {
		seq_puts(s, "fast_smmu_create_mapping failed\n");
		goto out_kfree;
	}

	if (iommu_domain_set_attr(mapping->domain, DOMAIN_ATTR_FAST, &fast)) {
		seq_puts(s, "iommu_domain_set_attr failed\n");
		goto out_release_mapping;
	}

	if (arm_iommu_attach_device(dev, mapping)) {
		seq_puts(s, "fast_smmu_attach_device failed\n");
		goto out_release_mapping;
	}

	if (iommu_enable_config_clocks(mapping->domain)) {
		seq_puts(s, "Couldn't enable clocks\n");
		goto out_detach;
	}
	for (experiment = 0; experiment < 2; ++experiment) {
		size_t map_avg = 0, unmap_avg = 0;

		for (i = 0; i < 10; ++i) {
			struct timespec tbefore, tafter, diff;
			u64 ns;

			getnstimeofday(&tbefore);
			dma_addr = dma_map_single_attrs(
				dev, virt, SZ_4K, DMA_TO_DEVICE,
				extra_attrs[experiment]);
			getnstimeofday(&tafter);
			diff = timespec_sub(tafter, tbefore);
			ns = timespec_to_ns(&diff);
			if (dma_mapping_error(dev, dma_addr)) {
				seq_puts(s, "dma_map_single failed\n");
				goto out_disable_config_clocks;
			}
			map_elapsed_ns[i] = ns;

			getnstimeofday(&tbefore);
			dma_unmap_single_attrs(
				dev, dma_addr, SZ_4K, DMA_TO_DEVICE,
				extra_attrs[experiment]);
			getnstimeofday(&tafter);
			diff = timespec_sub(tafter, tbefore);
			ns = timespec_to_ns(&diff);
			unmap_elapsed_ns[i] = ns;
		}

		seq_printf(s, "%13s %24s (ns): [", extra_labels[experiment],
			   "dma_map_single_attrs");
		for (i = 0; i < 10; ++i) {
			map_avg += map_elapsed_ns[i];
			seq_printf(s, "%5llu%s", map_elapsed_ns[i],
				   i < 9 ? ", " : "");
		}
		map_avg /= 10;
		seq_printf(s, "] (avg: %zu)\n", map_avg);

		seq_printf(s, "%13s %24s (ns): [", extra_labels[experiment],
			   "dma_unmap_single_attrs");
		for (i = 0; i < 10; ++i) {
			unmap_avg += unmap_elapsed_ns[i];
			seq_printf(s, "%5llu%s", unmap_elapsed_ns[i],
				   i < 9 ? ", " : "");
		}
		unmap_avg /= 10;
		seq_printf(s, "] (avg: %zu)\n", unmap_avg);
	}

out_disable_config_clocks:
	iommu_disable_config_clocks(mapping->domain);
out_detach:
	arm_iommu_detach_device(dev);
out_release_mapping:
	arm_iommu_release_mapping(mapping);
out_kfree:
	kfree(virt);
out:
	return 0;
}

static int iommu_debug_profiling_fast_dma_api_open(struct inode *inode,
						 struct file *file)
{
	return single_open(file, iommu_debug_profiling_fast_dma_api_show,
			   inode->i_private);
}

static const struct file_operations iommu_debug_profiling_fast_dma_api_fops = {
	.open	 = iommu_debug_profiling_fast_dma_api_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

static int __tlb_stress_sweep(struct device *dev, struct seq_file *s)
{
	int i, ret = 0;
	unsigned long iova;
	const unsigned long max = SZ_1G * 4UL;
	void *virt;
	phys_addr_t phys;
	dma_addr_t dma_addr;

	/*
	 * we'll be doing 4K and 8K mappings.  Need to own an entire 8K
	 * chunk that we can work with.
	 */
	virt = (void *)__get_free_pages(GFP_KERNEL, get_order(SZ_8K));
	phys = virt_to_phys(virt);

	/* fill the whole 4GB space */
	for (iova = 0, i = 0; iova < max; iova += SZ_8K, ++i) {
		dma_addr = dma_map_single(dev, virt, SZ_8K, DMA_TO_DEVICE);
		if (dma_addr == DMA_ERROR_CODE) {
			dev_err(dev, "Failed map on iter %d\n", i);
			ret = -EINVAL;
			goto out;
		}
	}

	if (dma_map_single(dev, virt, SZ_4K, DMA_TO_DEVICE) != DMA_ERROR_CODE) {
		dev_err(dev,
			"dma_map_single unexpectedly (VA should have been exhausted)\n");
		ret = -EINVAL;
		goto out;
	}

	/*
	 * free up 4K at the very beginning, then leave one 4K mapping,
	 * then free up 8K.  This will result in the next 8K map to skip
	 * over the 4K hole and take the 8K one.
	 */
	dma_unmap_single(dev, 0, SZ_4K, DMA_TO_DEVICE);
	dma_unmap_single(dev, SZ_8K, SZ_4K, DMA_TO_DEVICE);
	dma_unmap_single(dev, SZ_8K + SZ_4K, SZ_4K, DMA_TO_DEVICE);

	/* remap 8K */
	dma_addr = dma_map_single(dev, virt, SZ_8K, DMA_TO_DEVICE);
	if (dma_addr != SZ_8K) {
		dma_addr_t expected = SZ_8K;

		dev_err(dev, "Unexpected dma_addr. got: %pa expected: %pa\n",
			&dma_addr, &expected);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * now remap 4K.  We should get the first 4K chunk that was skipped
	 * over during the previous 8K map.  If we missed a TLB invalidate
	 * at that point this should explode.
	 */
	dma_addr = dma_map_single(dev, virt, SZ_4K, DMA_TO_DEVICE);
	if (dma_addr != 0) {
		dma_addr_t expected = 0;

		dev_err(dev, "Unexpected dma_addr. got: %pa expected: %pa\n",
			&dma_addr, &expected);
		ret = -EINVAL;
		goto out;
	}

	if (dma_map_single(dev, virt, SZ_4K, DMA_TO_DEVICE) != DMA_ERROR_CODE) {
		dev_err(dev,
			"dma_map_single unexpectedly after remaps (VA should have been exhausted)\n");
		ret = -EINVAL;
		goto out;
	}

	/* we're all full again. unmap everything. */
	for (dma_addr = 0; dma_addr < max; dma_addr += SZ_8K)
		dma_unmap_single(dev, dma_addr, SZ_8K, DMA_TO_DEVICE);

out:
	free_pages((unsigned long)virt, get_order(SZ_8K));
	return ret;
}

struct fib_state {
	unsigned long cur;
	unsigned long prev;
};

static void fib_init(struct fib_state *f)
{
	f->cur = f->prev = 1;
}

static unsigned long get_next_fib(struct fib_state *f)
{
	int next = f->cur + f->prev;

	f->prev = f->cur;
	f->cur = next;
	return next;
}

/*
 * Not actually random.  Just testing the fibs (and max - the fibs).
 */
static int __rand_va_sweep(struct device *dev, struct seq_file *s,
			   const size_t size)
{
	u64 iova;
	const unsigned long max = SZ_1G * 4UL;
	int i, remapped, unmapped, ret = 0;
	void *virt;
	dma_addr_t dma_addr, dma_addr2;
	struct fib_state fib;

	virt = (void *)__get_free_pages(GFP_KERNEL, get_order(size));
	if (!virt) {
		if (size > SZ_8K) {
			dev_err(dev,
				"Failed to allocate %s of memory, which is a lot. Skipping test for this size\n",
				_size_to_string(size));
			return 0;
		}
		return -ENOMEM;
	}

	/* fill the whole 4GB space */
	for (iova = 0, i = 0; iova < max; iova += size, ++i) {
		dma_addr = dma_map_single(dev, virt, size, DMA_TO_DEVICE);
		if (dma_addr == DMA_ERROR_CODE) {
			dev_err(dev, "Failed map on iter %d\n", i);
			ret = -EINVAL;
			goto out;
		}
	}

	/* now unmap "random" iovas */
	unmapped = 0;
	fib_init(&fib);
	for (iova = get_next_fib(&fib) * size;
	     iova < max - size;
	     iova = get_next_fib(&fib) * size) {
		dma_addr = iova;
		dma_addr2 = max - size - iova;
		if (dma_addr == dma_addr2) {
			WARN(1,
			"%s test needs update! The random number sequence is folding in on itself and should be changed.\n",
			__func__);
			return -EINVAL;
		}
		dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);
		dma_unmap_single(dev, dma_addr2, size, DMA_TO_DEVICE);
		unmapped += 2;
	}

	/* and map until everything fills back up */
	for (remapped = 0; ; ++remapped) {
		dma_addr = dma_map_single(dev, virt, size, DMA_TO_DEVICE);
		if (dma_addr == DMA_ERROR_CODE)
			break;
	}

	if (unmapped != remapped) {
		dev_err(dev,
			"Unexpected random remap count! Unmapped %d but remapped %d\n",
			unmapped, remapped);
		ret = -EINVAL;
	}

	for (dma_addr = 0; dma_addr < max; dma_addr += size)
		dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);

out:
	free_pages((unsigned long)virt, get_order(size));
	return ret;
}

static int __check_mapping(struct device *dev, struct iommu_domain *domain,
			   dma_addr_t iova, phys_addr_t expected)
{
	phys_addr_t res = iommu_iova_to_phys_hard(domain, iova);
	phys_addr_t res2 = iommu_iova_to_phys(domain, iova);

	BUG_ON(res != res2);

	if (res != expected) {
		dev_err_ratelimited(dev,
				    "Bad translation for %pa! Expected: %pa Got: %pa\n",
				    &iova, &expected, &res);
		return -EINVAL;
	}

	return 0;
}

static int __full_va_sweep(struct device *dev, struct seq_file *s,
			   const size_t size, struct iommu_domain *domain)
{
	unsigned long iova;
	dma_addr_t dma_addr;
	void *virt;
	phys_addr_t phys;
	int ret = 0, i;

	virt = (void *)__get_free_pages(GFP_KERNEL, get_order(size));
	if (!virt) {
		if (size > SZ_8K) {
			dev_err(dev,
				"Failed to allocate %s of memory, which is a lot. Skipping test for this size\n",
				_size_to_string(size));
			return 0;
		}
		return -ENOMEM;
	}
	phys = virt_to_phys(virt);

	for (iova = 0, i = 0; iova < SZ_1G * 4UL; iova += size, ++i) {
		unsigned long expected = iova;

		dma_addr = dma_map_single(dev, virt, size, DMA_TO_DEVICE);
		if (dma_addr != expected) {
			dev_err_ratelimited(dev,
					    "Unexpected iova on iter %d (expected: 0x%lx got: 0x%lx)\n",
					    i, expected,
					    (unsigned long)dma_addr);
			ret = -EINVAL;
			goto out;
		}
	}

	if (domain) {
		/* check every mapping from 0..6M */
		for (iova = 0, i = 0; iova < SZ_2M * 3; iova += size, ++i) {
			phys_addr_t expected = phys;

			if (__check_mapping(dev, domain, iova, expected)) {
				dev_err(dev, "iter: %d\n", i);
				ret = -EINVAL;
				goto out;
			}
		}
		/* and from 4G..4G-6M */
		for (iova = 0, i = 0; iova < SZ_2M * 3; iova += size, ++i) {
			phys_addr_t expected = phys;
			unsigned long theiova = ((SZ_1G * 4ULL) - size) - iova;

			if (__check_mapping(dev, domain, theiova, expected)) {
				dev_err(dev, "iter: %d\n", i);
				ret = -EINVAL;
				goto out;
			}
		}
	}

	/* at this point, our VA space should be full */
	dma_addr = dma_map_single(dev, virt, size, DMA_TO_DEVICE);
	if (dma_addr != DMA_ERROR_CODE) {
		dev_err_ratelimited(dev,
				    "dma_map_single succeeded when it should have failed. Got iova: 0x%lx\n",
				    (unsigned long)dma_addr);
		ret = -EINVAL;
	}

out:
	for (dma_addr = 0; dma_addr < SZ_1G * 4UL; dma_addr += size)
		dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);

	free_pages((unsigned long)virt, get_order(size));
	return ret;
}

#define ds_printf(d, s, fmt, ...) ({				\
			dev_err(d, fmt, ##__VA_ARGS__);		\
			seq_printf(s, fmt, ##__VA_ARGS__);	\
		})

static int __functional_dma_api_va_test(struct device *dev, struct seq_file *s,
				     struct iommu_domain *domain, void *priv)
{
	int i, j, ret = 0;
	size_t *sz, *sizes = priv;

	for (j = 0; j < 1; ++j) {
		for (sz = sizes; *sz; ++sz) {
			for (i = 0; i < 2; ++i) {
				ds_printf(dev, s, "Full VA sweep @%s %d",
					       _size_to_string(*sz), i);
				if (__full_va_sweep(dev, s, *sz, domain)) {
					ds_printf(dev, s, "  -> FAILED\n");
					ret = -EINVAL;
				} else {
					ds_printf(dev, s, "  -> SUCCEEDED\n");
				}
			}
		}
	}

	ds_printf(dev, s, "bonus map:");
	if (__full_va_sweep(dev, s, SZ_4K, domain)) {
		ds_printf(dev, s, "  -> FAILED\n");
		ret = -EINVAL;
	} else {
		ds_printf(dev, s, "  -> SUCCEEDED\n");
	}

	for (sz = sizes; *sz; ++sz) {
		for (i = 0; i < 2; ++i) {
			ds_printf(dev, s, "Rand VA sweep @%s %d",
				   _size_to_string(*sz), i);
			if (__rand_va_sweep(dev, s, *sz)) {
				ds_printf(dev, s, "  -> FAILED\n");
				ret = -EINVAL;
			} else {
				ds_printf(dev, s, "  -> SUCCEEDED\n");
			}
		}
	}

	ds_printf(dev, s, "TLB stress sweep");
	if (__tlb_stress_sweep(dev, s)) {
		ds_printf(dev, s, "  -> FAILED\n");
		ret = -EINVAL;
	} else {
		ds_printf(dev, s, "  -> SUCCEEDED\n");
	}

	ds_printf(dev, s, "second bonus map:");
	if (__full_va_sweep(dev, s, SZ_4K, domain)) {
		ds_printf(dev, s, "  -> FAILED\n");
		ret = -EINVAL;
	} else {
		ds_printf(dev, s, "  -> SUCCEEDED\n");
	}

	return ret;
}

static int __functional_dma_api_alloc_test(struct device *dev,
					   struct seq_file *s,
					   struct iommu_domain *domain,
					   void *ignored)
{
	size_t size = SZ_1K * 742;
	int ret = 0;
	u8 *data;
	dma_addr_t iova;

	/* Make sure we can allocate and use a buffer */
	ds_printf(dev, s, "Allocating coherent buffer");
	data = dma_alloc_coherent(dev, size, &iova, GFP_KERNEL);
	if (!data) {
		ds_printf(dev, s, "  -> FAILED\n");
		ret = -EINVAL;
	} else {
		int i;

		ds_printf(dev, s, "  -> SUCCEEDED\n");
		ds_printf(dev, s, "Using coherent buffer");
		for (i = 0; i < 742; ++i) {
			int ind = SZ_1K * i;
			u8 *p = data + ind;
			u8 val = i % 255;

			memset(data, 0xa5, size);
			*p = val;
			(*p)++;
			if ((*p) != val + 1) {
				ds_printf(dev, s,
					  "  -> FAILED on iter %d since %d != %d\n",
					  i, *p, val + 1);
				ret = -EINVAL;
			}
		}
		if (!ret)
			ds_printf(dev, s, "  -> SUCCEEDED\n");
		dma_free_coherent(dev, size, data, iova);
	}

	return ret;
}

static int __functional_dma_api_basic_test(struct device *dev,
					   struct seq_file *s,
					   struct iommu_domain *domain,
					   void *ignored)
{
	size_t size = 1518;
	int i, j, ret = 0;
	u8 *data;
	dma_addr_t iova;
	phys_addr_t pa, pa2;

	ds_printf(dev, s, "Basic DMA API test");
	/* Make sure we can allocate and use a buffer */
	for (i = 0; i < 1000; ++i) {
		data = kmalloc(size, GFP_KERNEL);
		if (!data) {
			ds_printf(dev, s, "  -> FAILED\n");
			ret = -EINVAL;
			goto out;
		}
		memset(data, 0xa5, size);
		iova = dma_map_single(dev, data, size, DMA_TO_DEVICE);
		pa = iommu_iova_to_phys(domain, iova);
		pa2 = iommu_iova_to_phys_hard(domain, iova);
		if (pa != pa2) {
			dev_err(dev,
				"iova_to_phys doesn't match iova_to_phys_hard: %pa != %pa\n",
				&pa, &pa2);
			ret = -EINVAL;
			goto out;
		}
		pa2 = virt_to_phys(data);
		if (pa != pa2) {
			dev_err(dev,
				"iova_to_phys doesn't match virt_to_phys: %pa != %pa\n",
				&pa, &pa2);
			ret = -EINVAL;
			goto out;
		}
		dma_unmap_single(dev, iova, size, DMA_TO_DEVICE);
		for (j = 0; j < size; ++j) {
			if (data[j] != 0xa5) {
				dev_err(dev, "data[%d] != 0xa5\n", data[j]);
				ret = -EINVAL;
				goto out;
			}
		}
		kfree(data);
	}

out:
	if (ret)
		ds_printf(dev, s, "  -> FAILED\n");
	else
		ds_printf(dev, s, "  -> SUCCEEDED\n");

	return ret;
}

/* Creates a fresh fast mapping and applies @fn to it */
static int __apply_to_new_mapping(struct seq_file *s,
				    int (*fn)(struct device *dev,
					      struct seq_file *s,
					      struct iommu_domain *domain,
					      void *priv),
				    void *priv)
{
	struct dma_iommu_mapping *mapping;
	struct iommu_debug_device *ddev = s->private;
	struct device *dev = ddev->dev;
	int ret = -EINVAL, fast = 1;
	phys_addr_t pt_phys;

	mapping = arm_iommu_create_mapping(&platform_bus_type, 0, SZ_1G * 4UL);
	if (!mapping)
		goto out;

	if (iommu_domain_set_attr(mapping->domain, DOMAIN_ATTR_FAST, &fast)) {
		seq_puts(s, "iommu_domain_set_attr failed\n");
		goto out_release_mapping;
	}

	if (arm_iommu_attach_device(dev, mapping))
		goto out_release_mapping;

	BUG_ON(iommu_domain_get_attr(mapping->domain, DOMAIN_ATTR_PT_BASE_ADDR,
				     &pt_phys));
	dev_err(dev, "testing with pgtables at %pa\n", &pt_phys);
	if (iommu_enable_config_clocks(mapping->domain)) {
		ds_printf(dev, s, "Couldn't enable clocks");
		goto out_release_mapping;
	}
	ret = fn(dev, s, mapping->domain, priv);
	iommu_disable_config_clocks(mapping->domain);

	arm_iommu_detach_device(dev);
out_release_mapping:
	arm_iommu_release_mapping(mapping);
out:
	seq_printf(s, "%s\n", ret ? "FAIL" : "SUCCESS");
	return 0;
}

static int iommu_debug_functional_fast_dma_api_show(struct seq_file *s,
						    void *ignored)
{
	size_t sizes[] = {SZ_4K, SZ_8K, SZ_16K, SZ_64K, 0};
	int ret = 0;

	ret |= __apply_to_new_mapping(s, __functional_dma_api_alloc_test, NULL);
	ret |= __apply_to_new_mapping(s, __functional_dma_api_basic_test, NULL);
	ret |= __apply_to_new_mapping(s, __functional_dma_api_va_test, sizes);
	return ret;
}

static int iommu_debug_functional_fast_dma_api_open(struct inode *inode,
						    struct file *file)
{
	return single_open(file, iommu_debug_functional_fast_dma_api_show,
			   inode->i_private);
}

static const struct file_operations iommu_debug_functional_fast_dma_api_fops = {
	.open	 = iommu_debug_functional_fast_dma_api_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

static int iommu_debug_functional_arm_dma_api_show(struct seq_file *s,
						   void *ignored)
{
	struct dma_iommu_mapping *mapping;
	struct iommu_debug_device *ddev = s->private;
	struct device *dev = ddev->dev;
	size_t sizes[] = {SZ_4K, SZ_64K, SZ_2M, SZ_1M * 12, 0};
	int ret = -EINVAL;

	mapping = arm_iommu_create_mapping(&platform_bus_type, 0, SZ_1G * 4UL);
	if (!mapping)
		goto out;

	if (arm_iommu_attach_device(dev, mapping))
		goto out_release_mapping;

	ret = __functional_dma_api_alloc_test(dev, s, mapping->domain, sizes);
	ret |= __functional_dma_api_basic_test(dev, s, mapping->domain, sizes);

	arm_iommu_detach_device(dev);
out_release_mapping:
	arm_iommu_release_mapping(mapping);
out:
	seq_printf(s, "%s\n", ret ? "FAIL" : "SUCCESS");
	return 0;
}

static int iommu_debug_functional_arm_dma_api_open(struct inode *inode,
						   struct file *file)
{
	return single_open(file, iommu_debug_functional_arm_dma_api_show,
			   inode->i_private);
}

static const struct file_operations iommu_debug_functional_arm_dma_api_fops = {
	.open	 = iommu_debug_functional_arm_dma_api_open,
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

	val = VMID_CP_CAMERA;
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

	if (kstrtox_from_user(ubuf, count, 0, &iova)) {
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

	if (kptr_restrict != 0) {
		pr_err("kptr_restrict needs to be disabled.\n");
		return -EPERM;
	}
	if (!ddev->domain) {
		pr_err("No domain. Did you already attach?\n");
		return -EINVAL;
	}

	if (*offset)
		return 0;

	memset(buf, 0, 100);

	phys = iommu_iova_to_phys_hard(ddev->domain, ddev->iova);
	if (!phys) {
		strlcpy(buf, "FAIL\n", 100);
		phys = iommu_iova_to_phys(ddev->domain, ddev->iova);
		dev_err(ddev->dev, "ATOS for %pa failed. Software walk returned: %pa\n",
			&ddev->iova, &phys);
	} else {
		snprintf(buf, 100, "%pa\n", &phys);
	}

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
	ssize_t retval = -EINVAL;
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

	if (kstrtoux(buf, 0, &iova))
		goto invalid_format;

	if (kstrtoux(comma1 + 1, 0, &phys))
		goto invalid_format;

	if (kstrtosize_t(comma2 + 1, 0, &size))
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

	if (kstrtoux(buf, 0, &iova))
		goto invalid_format;

	if (kstrtosize_t(comma1 + 1, 0, &size))
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

static ssize_t iommu_debug_config_clocks_write(struct file *file,
					       const char __user *ubuf,
					       size_t count, loff_t *offset)
{
	char buf;
	struct iommu_debug_device *ddev = file->private_data;
	struct device *dev = ddev->dev;

	/* we're expecting a single character plus (optionally) a newline */
	if (count > 2) {
		dev_err(dev, "Invalid value\n");
		return -EINVAL;
	}

	if (!ddev->domain) {
		dev_err(dev, "No domain. Did you already attach?\n");
		return -EINVAL;
	}

	if (copy_from_user(&buf, ubuf, 1)) {
		dev_err(dev, "Couldn't copy from user\n");
		return -EFAULT;
	}

	mutex_lock(&ddev->clk_lock);
	switch (buf) {
	case '0':
		if (ddev->clk_count == 0) {
			dev_err(dev, "Config clocks already disabled\n");
			break;
		}

		if (--ddev->clk_count > 0)
			break;

		dev_err(dev, "Disabling config clocks\n");
		iommu_disable_config_clocks(ddev->domain);
		break;
	case '1':
		if (ddev->clk_count++ > 0)
			break;

		dev_err(dev, "Enabling config clocks\n");
		if (iommu_enable_config_clocks(ddev->domain))
			dev_err(dev, "Failed!\n");
		break;
	default:
		dev_err(dev, "Invalid value. Should be 0 or 1.\n");
		mutex_unlock(&ddev->clk_lock);
		return -EINVAL;
	}
	mutex_unlock(&ddev->clk_lock);

	return count;
}

static const struct file_operations iommu_debug_config_clocks_fops = {
	.open	= simple_open,
	.write	= iommu_debug_config_clocks_write,
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
	mutex_init(&ddev->clk_lock);
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

	if (!debugfs_create_file("profiling_fast", S_IRUSR, dir, ddev,
				 &iommu_debug_profiling_fast_fops)) {
		pr_err("Couldn't create iommu/devices/%s/profiling_fast debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("profiling_fast_dma_api", S_IRUSR, dir, ddev,
				 &iommu_debug_profiling_fast_dma_api_fops)) {
		pr_err("Couldn't create iommu/devices/%s/profiling_fast_dma_api debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("functional_fast_dma_api", S_IRUSR, dir, ddev,
				 &iommu_debug_functional_fast_dma_api_fops)) {
		pr_err("Couldn't create iommu/devices/%s/functional_fast_dma_api debugfs file\n",
		       name);
		goto err_rmdir;
	}

	if (!debugfs_create_file("functional_arm_dma_api", S_IRUSR, dir, ddev,
				 &iommu_debug_functional_arm_dma_api_fops)) {
		pr_err("Couldn't create iommu/devices/%s/functional_arm_dma_api debugfs file\n",
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

	if (!debugfs_create_file("config_clocks", S_IWUSR, dir, ddev,
				 &iommu_debug_config_clocks_fops)) {
		pr_err("Couldn't create iommu/devices/%s/config_clocks debugfs file\n",
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
	if (!of_device_is_compatible(dev->of_node, "iommu-debug-test"))
		return 0;

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
		if (!of_device_is_compatible(np, "iommu-debug-test"))
			continue;

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
	if (iommu_debug_init_tests())
		return -ENODEV;

	return 0;
}

static void iommu_debug_exit(void)
{
	iommu_debug_destroy_tests();
}

module_init(iommu_debug_init);
module_exit(iommu_debug_exit);
