// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
#include <linux/of_iommu.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/dma-contiguous.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/dma-iommu.h>

#ifdef CONFIG_ARM64_PTDUMP_CORE
#include <asm/ptdump.h>
#endif

#if defined(CONFIG_IOMMU_TESTS)

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
	case DOMAIN_ATTR_CB_STALL_DISABLE:
		return "DOMAIN_ATTR_CB_STALL_DISABLE";
	default:
		return "Unknown attr!";
	}
}
#endif

#ifdef CONFIG_IOMMU_TESTS

#ifdef CONFIG_64BIT

#define kstrtoux kstrtou64
#define kstrtox_from_user kstrtoull_from_user
#define kstrtosize_t kstrtoul

#else

#define kstrtoux kstrtou32
#define kstrtox_from_user kstrtouint_from_user
#define kstrtosize_t kstrtouint

#endif

static LIST_HEAD(iommu_debug_devices);
static struct dentry *debugfs_tests_dir;
static u32 iters_per_op = 1;
static void *test_virt_addr;

struct iommu_debug_device {
	struct device *dev;
	struct iommu_domain *domain;
	struct dma_iommu_mapping *mapping;
	u64 iova;
	u64 phys;
	size_t len;
	struct list_head list;
	struct mutex clk_lock;
	unsigned int clk_count;
	/* Protects domain */
	struct mutex state_lock;
#ifdef CONFIG_ARM64_PTDUMP_CORE
	struct ptdump_info pt_info;
#endif
};

static int iommu_debug_build_phoney_sg_table(struct device *dev,
					     struct sg_table *table,
					     unsigned long total_size,
					     unsigned long chunk_size)
{
	unsigned long nents = total_size / chunk_size;
	struct scatterlist *sg;
	int i, j;
	struct page *page;

	if (!IS_ALIGNED(total_size, PAGE_SIZE))
		return -EINVAL;
	if (!IS_ALIGNED(total_size, chunk_size))
		return -EINVAL;
	if (sg_alloc_table(table, nents, GFP_KERNEL))
		return -EINVAL;

	for_each_sg(table->sgl, sg, table->nents, i) {
		page = alloc_pages(GFP_KERNEL, get_order(chunk_size));
		if (!page)
			goto free_pages;
		sg_set_page(sg, page, chunk_size, 0);
	}

	return 0;
free_pages:
	for_each_sg(table->sgl, sg, i--, j)
		__free_pages(sg_page(sg), get_order(chunk_size));
	sg_free_table(table);
	return -ENOMEM;
}

static void iommu_debug_destroy_phoney_sg_table(struct device *dev,
						struct sg_table *table,
						unsigned long chunk_size)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(table->sgl, sg, table->nents, i)
		__free_pages(sg_page(sg), get_order(chunk_size));
	sg_free_table(table);
}

struct iommu_debug_attr {
	unsigned long dma_type;
	int vmid;
};

static struct iommu_debug_attr std_attr = {
	.dma_type = 0,
	.vmid = 0,
};

static struct iommu_debug_attr fastmap_attr = {
	.dma_type = DOMAIN_ATTR_FAST,
	.vmid = 0,
};

static struct iommu_debug_attr secure_attr = {
	.dma_type = 0,
	.vmid = VMID_CP_PIXEL,
};

static int iommu_debug_set_attrs(struct iommu_debug_device *ddev,
				 struct iommu_domain *domain,
				 struct iommu_debug_attr *attrs)
{
	int val = 1;

	/* Always set this to avoid clk latency during measurements */
	iommu_domain_set_attr(domain, DOMAIN_ATTR_ATOMIC, &val);

	if (attrs->dma_type == DOMAIN_ATTR_FAST)
		iommu_domain_set_attr(domain, DOMAIN_ATTR_FAST, &val);

	if (attrs->vmid != 0)
		iommu_domain_set_attr(domain,
			DOMAIN_ATTR_SECURE_VMID, &attrs->vmid);

	return 0;
}

static void iommu_debug_print_attrs(struct seq_file *s,
				    struct iommu_debug_attr *attrs)
{
	seq_puts(s, "Attributes:\n");
	if (attrs->dma_type == DOMAIN_ATTR_FAST)
		seq_printf(s, "%s\n",
			   iommu_debug_attr_to_string(attrs->dma_type));

	if (attrs->vmid != 0)
		seq_printf(s, "SECURE_VMID=%d\n", attrs->vmid);
}

/*
 * Set up a new dma allocator for dev
 * Caller should hold state_lock
 */
static int iommu_debug_dma_reconfigure(struct iommu_debug_device *ddev,
					struct iommu_debug_attr *attrs,
					u64 dma_base, u64 size)
{

	const struct iommu_ops *iommu;
	struct iommu_domain *domain;
	struct device *dev = ddev->dev;
	bool coherent;

	if (ddev->domain) {
		dev_err_ratelimited(dev, "Already attached.\n");
		return -EBUSY;
	}

	iommu = of_iommu_configure(dev, dev->of_node);
	if (!iommu) {
		dev_err_ratelimited(dev, "Is not associated with an iommu\n");
		return -EINVAL;
	}

	coherent = of_dma_is_coherent(dev->of_node);

	if (!dev->iommu_group) {
		dev_err_ratelimited(dev, "Does not have an iommu group\n");
		return -EINVAL;
	}

	/* Detach from the default domain */
	domain = iommu_get_domain_for_dev(dev);
	if (domain) {
		if (domain->type != IOMMU_DOMAIN_DMA) {
			dev_err_ratelimited(dev, "Attached, but its not a default domain?\n");
			return -EINVAL;
		}
		iommu_detach_group(domain, dev->iommu_group);
	}

	domain = iommu_domain_alloc(dev->bus);
	if (!domain) {
		dev_err_ratelimited(dev, "Allocating iommu domain failed\n");
		return -EINVAL;
	}

	domain->is_debug_domain = true;

	if (iommu_debug_set_attrs(ddev, domain, attrs)) {
		dev_err_ratelimited(dev, "Setting attrs failed\n");
		goto out_free_domain;
	}

	if (iommu_attach_group(domain, dev->iommu_group)) {
		dev_err_ratelimited(dev, "attach group failed\n");
		goto out_free_domain;
	}

	/*
	 * Since arch_setup_dma_ops is void, interpret non-null dma-ops
	 * as success.
	 */
	set_dma_ops(dev, NULL);
	arch_setup_dma_ops(dev, dma_base, size, iommu, coherent);
	if (!get_dma_ops(dev)) {
		dev_err_ratelimited(dev, "arch_setup_dma_ops failed, dma ops are null.\n");
		goto out_detach_group;
	}

	ddev->domain = domain;
	return 0;

out_detach_group:
	iommu_detach_group(domain, dev->iommu_group);
out_free_domain:
	iommu_domain_free(domain);
	return -EINVAL;
}

/* Caller should hold state_lock */
static void iommu_debug_dma_deconfigure(struct iommu_debug_device *ddev)
{
	struct iommu_domain *domain;
	struct device *dev = ddev->dev;

	if (!dev->iommu_group) {
		dev_err_ratelimited(dev, "Does not have an iommu group\n");
		return;
	}

	domain = ddev->domain;
	if (!domain) {
		dev_err_ratelimited(dev, "Is not attached\n");
		return;
	}


	arch_teardown_dma_ops(dev);
	iommu_detach_group(domain, dev->iommu_group);
	iommu_domain_free(domain);

	ddev->domain = NULL;
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
	case SZ_1M:
		return "1M";
	case SZ_2M:
		return "2M";
	case SZ_1M * 12:
		return "12M";
	case SZ_1M * 20:
		return "20M";
	case SZ_1M * 24:
		return "24M";
	case SZ_1M * 32:
		return "32M";
	}
	return "unknown size, please add to _size_to_string function";
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

static void iommu_debug_device_profiling(struct seq_file *s,
					struct iommu_debug_device *ddev,
					struct iommu_debug_attr *attrs,
					const size_t sizes[])
{
	const size_t *sz;
	struct iommu_domain *domain;
	struct device *dev = ddev->dev;
	unsigned long iova = 0x10000;
	phys_addr_t paddr = 0xa000;

	if (iommu_debug_dma_reconfigure(ddev, attrs, 0, SZ_1G * 4ULL))
		return;
	domain = ddev->domain;

	iommu_debug_print_attrs(s, attrs);

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
	iommu_debug_dma_deconfigure(ddev);
}

static int iommu_debug_profiling_show(struct seq_file *s, void *ignored)
{
	struct iommu_debug_device *ddev = s->private;
	const size_t sizes[] = { SZ_4K, SZ_64K, SZ_1M, SZ_2M, SZ_1M * 12,
					SZ_1M * 24, SZ_1M * 32, 0 };

	mutex_lock(&ddev->state_lock);
	iommu_debug_device_profiling(s, ddev, &std_attr, sizes);
	mutex_unlock(&ddev->state_lock);

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
	const size_t sizes[] = { SZ_4K, SZ_64K, SZ_1M, SZ_2M, SZ_1M * 12,
					SZ_1M * 24, SZ_1M * 32, 0 };

	mutex_lock(&ddev->state_lock);
	iommu_debug_device_profiling(s, ddev, &secure_attr, sizes);
	mutex_unlock(&ddev->state_lock);

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

	mutex_lock(&ddev->state_lock);
	iommu_debug_device_profiling(s, ddev, &fastmap_attr, sizes);
	mutex_unlock(&ddev->state_lock);

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
	struct iommu_domain *domain;
	dma_addr_t dma_addr;
	void *virt;
	const char * const extra_labels[] = {
		"not coherent",
		"coherent",
	};
	unsigned long extra_attrs[] = {
		0,
		DMA_ATTR_SKIP_CPU_SYNC,
	};

	mutex_lock(&ddev->state_lock);

	virt = kmalloc(1518, GFP_KERNEL);
	if (!virt)
		goto out;

	if (iommu_debug_dma_reconfigure(ddev, &fastmap_attr, 0, SZ_1G * 4ULL)) {
		seq_puts(s, "setup failed\n");
		goto out_kfree;
	}
	domain = ddev->domain;

	if (iommu_enable_config_clocks(domain)) {
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
	iommu_disable_config_clocks(domain);
out_detach:
	iommu_debug_dma_deconfigure(ddev);
out_kfree:
	kfree(virt);
out:
	mutex_unlock(&ddev->state_lock);
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
	u64 iova;
	const u64  max = SZ_1G * 4ULL - 1;
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
			dev_err_ratelimited(dev, "Failed map on iter %d\n", i);
			ret = -EINVAL;
			goto out;
		}
	}

	if (dma_map_single(dev, virt, SZ_4K, DMA_TO_DEVICE) != DMA_ERROR_CODE) {
		dev_err_ratelimited(dev,
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

		dev_err_ratelimited(dev, "Unexpected dma_addr. got: %pa expected: %pa\n",
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

		dev_err_ratelimited(dev, "Unexpected dma_addr. got: %pa expected: %pa\n",
			&dma_addr, &expected);
		ret = -EINVAL;
		goto out;
	}

	if (dma_map_single(dev, virt, SZ_4K, DMA_TO_DEVICE) != DMA_ERROR_CODE) {
		dev_err_ratelimited(dev,
			"dma_map_single unexpectedly after remaps (VA should have been exhausted)\n");
		ret = -EINVAL;
		goto out;
	}

	/* we're all full again. unmap everything. */
	for (iova = 0; iova < max; iova += SZ_8K)
		dma_unmap_single(dev, (dma_addr_t)iova, SZ_8K, DMA_TO_DEVICE);

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
	const u64 max = SZ_1G * 4ULL - 1;
	int i, remapped, unmapped, ret = 0;
	void *virt;
	dma_addr_t dma_addr, dma_addr2;
	struct fib_state fib;

	virt = (void *)__get_free_pages(GFP_KERNEL, get_order(size));
	if (!virt) {
		if (size > SZ_8K) {
			dev_err_ratelimited(dev,
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
			dev_err_ratelimited(dev, "Failed map on iter %d\n", i);
			ret = -EINVAL;
			goto out;
		}
	}

	/* now unmap "random" iovas */
	unmapped = 0;
	fib_init(&fib);
	for (iova = get_next_fib(&fib) * size;
	     iova < max - size;
	     iova = (u64)get_next_fib(&fib) * size) {
		dma_addr = (dma_addr_t)(iova);
		dma_addr2 = (dma_addr_t)((max + 1) - size - iova);
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
		dev_err_ratelimited(dev,
			"Unexpected random remap count! Unmapped %d but remapped %d\n",
			unmapped, remapped);
		ret = -EINVAL;
	}

	for (iova = 0; iova < max; iova += size)
		dma_unmap_single(dev, (dma_addr_t)iova, size, DMA_TO_DEVICE);

out:
	free_pages((unsigned long)virt, get_order(size));
	return ret;
}

static int __check_mapping(struct device *dev, struct iommu_domain *domain,
			   dma_addr_t iova, phys_addr_t expected)
{
	phys_addr_t res = iommu_iova_to_phys_hard(domain, iova);
	phys_addr_t res2 = iommu_iova_to_phys(domain, iova);

	WARN(res != res2, "hard/soft iova_to_phys fns don't agree...");

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
	u64 iova;
	dma_addr_t dma_addr;
	void *virt;
	phys_addr_t phys;
	const u64 max = SZ_1G * 4ULL - 1;
	int ret = 0, i;

	virt = (void *)__get_free_pages(GFP_KERNEL, get_order(size));
	if (!virt) {
		if (size > SZ_8K) {
			dev_err_ratelimited(dev,
				"Failed to allocate %s of memory, which is a lot. Skipping test for this size\n",
				_size_to_string(size));
			return 0;
		}
		return -ENOMEM;
	}
	phys = virt_to_phys(virt);

	for (iova = 0, i = 0; iova < max; iova += size, ++i) {
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
				dev_err_ratelimited(dev, "iter: %d\n", i);
				ret = -EINVAL;
				goto out;
			}
		}
		/* and from 4G..4G-6M */
		for (iova = 0, i = 0; iova < SZ_2M * 3; iova += size, ++i) {
			phys_addr_t expected = phys;
			unsigned long theiova = ((SZ_1G * 4ULL) - size) - iova;

			if (__check_mapping(dev, domain, theiova, expected)) {
				dev_err_ratelimited(dev, "iter: %d\n", i);
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
	for (iova = 0; iova < max; iova += size)
		dma_unmap_single(dev, (dma_addr_t)iova, size, DMA_TO_DEVICE);

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
			dev_err_ratelimited(dev,
				"iova_to_phys doesn't match iova_to_phys_hard: %pa != %pa\n",
				&pa, &pa2);
			ret = -EINVAL;
			goto out;
		}
		pa2 = virt_to_phys(data);
		if (pa != pa2) {
			dev_err_ratelimited(dev,
				"iova_to_phys doesn't match virt_to_phys: %pa != %pa\n",
				&pa, &pa2);
			ret = -EINVAL;
			goto out;
		}
		dma_unmap_single(dev, iova, size, DMA_TO_DEVICE);
		for (j = 0; j < size; ++j) {
			if (data[j] != 0xa5) {
				dev_err_ratelimited(dev,
					       "data[%d] != 0xa5\n", data[j]);
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

static int __functional_dma_api_map_sg_test(struct device *dev,
					   struct seq_file *s,
					   struct iommu_domain *domain,
					   size_t sizes[])
{
	const size_t *sz;
	int i, ret = 0, count = 0;
	dma_addr_t iova;
	phys_addr_t pa, pa2;

	ds_printf(dev, s, "Map SG DMA API test\n");

	for (sz = sizes; *sz; ++sz) {
		size_t size = *sz;
		struct sg_table table;
		unsigned long chunk_size = SZ_4K;
		struct scatterlist *sg;

		/* Build us a table */
		ret = iommu_debug_build_phoney_sg_table(dev, &table, size,
				chunk_size);
		if (ret) {
			seq_puts(s,
				"couldn't build phoney sg table! bailing...\n");
			goto out;
		}
		count = dma_map_sg(dev, table.sgl, table.nents,
				DMA_BIDIRECTIONAL);
		if (!count) {
			ret = -EINVAL;
			goto destroy_table;
		}
		/* Check mappings... */
		for_each_sg(table.sgl, sg, count, i) {
			iova = sg_dma_address(sg);
			pa = iommu_iova_to_phys(domain, iova);
			pa2 = iommu_iova_to_phys_hard(domain, iova);
			if (pa != pa2) {
				dev_err_ratelimited(dev,
					"iova_to_phys doesn't match iova_to_phys_hard: %pa != %pa\n",
					&pa, &pa2);
				ret = -EINVAL;
				goto unmap;
			}
			/* check mappings at end of buffer */
			iova += sg_dma_len(sg) - 1;
			pa = iommu_iova_to_phys(domain, iova);
			pa2 = iommu_iova_to_phys_hard(domain, iova);
			if (pa != pa2) {
				dev_err_ratelimited(dev,
					"iova_to_phys doesn't match iova_to_phys_hard: %pa != %pa\n",
					&pa, &pa2);
				ret = -EINVAL;
				goto unmap;
			}
		}
unmap:
		dma_unmap_sg(dev, table.sgl, table.nents, DMA_BIDIRECTIONAL);
destroy_table:
		iommu_debug_destroy_phoney_sg_table(dev, &table, chunk_size);
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
	struct iommu_domain *domain;
	struct iommu_debug_device *ddev = s->private;
	struct device *dev = ddev->dev;
	int ret = -EINVAL;
	phys_addr_t pt_phys;

	mutex_lock(&ddev->state_lock);
	if (iommu_debug_dma_reconfigure(ddev, &fastmap_attr, 0, SZ_1G * 4ULL)) {
		seq_puts(s, "setup failed\n");
		goto out;
	}
	domain = ddev->domain;

	if (iommu_domain_get_attr(domain, DOMAIN_ATTR_PT_BASE_ADDR,
				  &pt_phys)) {
		ds_printf(dev, s, "Couldn't get page table base address\n");
		goto out_release_mapping;
	}

	dev_err_ratelimited(dev, "testing with pgtables at %pa\n", &pt_phys);
	if (iommu_enable_config_clocks(domain)) {
		ds_printf(dev, s, "Couldn't enable clocks\n");
		goto out_release_mapping;
	}
	ret = fn(dev, s, domain, priv);
	iommu_disable_config_clocks(domain);

out_release_mapping:
	iommu_debug_dma_deconfigure(ddev);
out:
	mutex_unlock(&ddev->state_lock);
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
	struct iommu_debug_device *ddev = s->private;
	struct device *dev = ddev->dev;
	size_t sizes[] = {SZ_4K, SZ_64K, SZ_2M, SZ_1M * 12, 0};
	int ret = -EINVAL;

	mutex_lock(&ddev->state_lock);
	if (iommu_debug_dma_reconfigure(ddev, &fastmap_attr, 0, SZ_1G * 4ULL))
		goto out;

	ret = __functional_dma_api_alloc_test(dev, s, ddev->domain, sizes);
	ret |= __functional_dma_api_basic_test(dev, s, ddev->domain, sizes);
	ret |= __functional_dma_api_map_sg_test(dev, s, ddev->domain, sizes);

	iommu_debug_dma_deconfigure(ddev);
out:
	mutex_unlock(&ddev->state_lock);
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

static ssize_t __iommu_debug_attach_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset,
					struct iommu_debug_attr *attrs)
{
	struct iommu_debug_device *ddev = file->private_data;
	ssize_t retval = -EINVAL;
	int val, ret;

	if (kstrtoint_from_user(ubuf, count, 0, &val)) {
		pr_err_ratelimited("Invalid format. Expected a hex or decimal integer");
		return -EFAULT;
	}

	mutex_lock(&ddev->state_lock);
	if (val) {
		ret = iommu_debug_dma_reconfigure(ddev, attrs, 0, SZ_1G * 4ULL);
		if (!ret)
			pr_err_ratelimited("Attached\n");
	} else {
		iommu_debug_dma_deconfigure(ddev);
		pr_err_ratelimited("Detached\n");
	}
	mutex_unlock(&ddev->state_lock);
	retval = count;
	return retval;
}

static ssize_t iommu_debug_secure_attach_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	return __iommu_debug_attach_write(file, ubuf, count, offset,
						&secure_attr);
}

static ssize_t iommu_debug_attach_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	return __iommu_debug_attach_write(file, ubuf, count, offset, &std_attr);

}

static ssize_t iommu_debug_attach_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	char buf[100];

	snprintf(buf, sizeof(buf), "%d\n", ddev->domain ? 1 : 0);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations iommu_debug_dma_attach_fops = {
	.open	= simple_open,
	.write	= iommu_debug_attach_write,
	.read	= iommu_debug_attach_read,
};

static ssize_t iommu_debug_test_virt_addr_read(struct file *file,
					       char __user *ubuf,
					       size_t count, loff_t *offset)
{
	char buf[100];
	size_t buf_len = sizeof(buf);

	if (*offset)
		return 0;

	memset(buf, 0, buf_len);

	if (!test_virt_addr)
		strlcpy(buf, "FAIL\n", buf_len);
	else
		snprintf(buf, buf_len, "0x%pK\n", test_virt_addr);

	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations iommu_debug_test_virt_addr_fops = {
	.open	= simple_open,
	.read	= iommu_debug_test_virt_addr_read,
};

static const struct file_operations iommu_debug_attach_fops = {
	.open	= simple_open,
	.write	= iommu_debug_attach_write,
	.read	= iommu_debug_attach_read,
};

static const struct file_operations iommu_debug_secure_attach_fops = {
	.open	= simple_open,
	.write	= iommu_debug_secure_attach_write,
	.read	= iommu_debug_attach_read,
};

static ssize_t iommu_debug_pte_write(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	dma_addr_t iova;

	if (kstrtox_from_user(ubuf, count, 0, &iova)) {
		pr_err_ratelimited("Invalid format for iova\n");
		ddev->iova = 0;
		return -EINVAL;
	}

	ddev->iova = iova;
	pr_err_ratelimited("Saved iova=%pa for future PTE commands\n", &iova);
	return count;
}


static ssize_t iommu_debug_pte_read(struct file *file, char __user *ubuf,
				     size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	uint64_t pte;
	char buf[100];

	if (kptr_restrict != 0) {
		pr_err_ratelimited("kptr_restrict needs to be disabled.\n");
		return -EPERM;
	}

	if (*offset)
		return 0;

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}

	memset(buf, 0, sizeof(buf));

	pte = iommu_iova_to_pte(ddev->domain, ddev->iova);

	if (!pte)
		strlcpy(buf, "FAIL\n", sizeof(buf));
	else
		snprintf(buf, sizeof(buf), "pte=%016llx\n", pte);
	mutex_unlock(&ddev->state_lock);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations iommu_debug_pte_fops = {
	.open	= simple_open,
	.write	= iommu_debug_pte_write,
	.read	= iommu_debug_pte_read,
};

static ssize_t iommu_debug_atos_write(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	dma_addr_t iova;
	phys_addr_t phys;
	unsigned long pfn;

	if (kstrtox_from_user(ubuf, count, 0, &iova)) {
		pr_err_ratelimited("Invalid format for iova\n");
		ddev->iova = 0;
		return -EINVAL;
	}

	ddev->iova = iova;
	phys = iommu_iova_to_phys(ddev->domain, ddev->iova);
	pfn = __phys_to_pfn(phys);
	if (!pfn_valid(pfn)) {
		dev_err(ddev->dev, "Invalid ATOS operation page %pa\n", &phys);
		return -EINVAL;
	}

	pr_err_ratelimited("Saved iova=%pa for future ATOS commands\n", &iova);
	return count;
}

static ssize_t iommu_debug_atos_read(struct file *file, char __user *ubuf,
				     size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	phys_addr_t phys;
	char buf[100];

	if (kptr_restrict != 0) {
		pr_err_ratelimited("kptr_restrict needs to be disabled.\n");
		return -EPERM;
	}

	if (*offset)
		return 0;

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}

	memset(buf, 0, 100);

	phys = iommu_iova_to_phys_hard(ddev->domain, ddev->iova);
	if (!phys) {
		strlcpy(buf, "FAIL\n", 100);
		phys = iommu_iova_to_phys(ddev->domain, ddev->iova);
		dev_err_ratelimited(ddev->dev, "ATOS for %pa failed. Software walk returned: %pa\n",
			&ddev->iova, &phys);
	} else {
		snprintf(buf, 100, "%pa\n", &phys);
	}
	mutex_unlock(&ddev->state_lock);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations iommu_debug_atos_fops = {
	.open	= simple_open,
	.write	= iommu_debug_atos_write,
	.read	= iommu_debug_atos_read,
};

static ssize_t iommu_debug_dma_atos_read(struct file *file, char __user *ubuf,
				     size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	phys_addr_t phys;
	char buf[100];

	if (kptr_restrict != 0) {
		pr_err_ratelimited("kptr_restrict needs to be disabled.\n");
		return -EPERM;
	}
	if (*offset)
		return 0;

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}

	memset(buf, 0, sizeof(buf));

	phys = iommu_iova_to_phys_hard(ddev->domain,
			ddev->iova);
	if (!phys)
		strlcpy(buf, "FAIL\n", sizeof(buf));
	else
		snprintf(buf, sizeof(buf), "%pa\n", &phys);
	mutex_unlock(&ddev->state_lock);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations iommu_debug_dma_atos_fops = {
	.open	= simple_open,
	.write	= iommu_debug_atos_write,
	.read	= iommu_debug_dma_atos_read,
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
		pr_err_ratelimited("Value too large\n");
		return -EINVAL;
	}

	memset(buf, 0, 100);

	if (copy_from_user(buf, ubuf, count)) {
		pr_err_ratelimited("Couldn't copy from user\n");
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

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}

	ret = iommu_map(ddev->domain, iova, phys, size, prot);
	if (ret) {
		pr_err_ratelimited("iommu_map failed with %d\n", ret);
		retval = -EIO;
		goto out;
	}

	retval = count;
	pr_err_ratelimited("Mapped %pa to %pa (len=0x%zx, prot=0x%x)\n",
	       &iova, &phys, size, prot);
out:
	mutex_unlock(&ddev->state_lock);
	return retval;

invalid_format:
	pr_err_ratelimited("Invalid format. Expected: iova,phys,len,prot where `prot' is the bitwise OR of IOMMU_READ, IOMMU_WRITE, etc.\n");
	return -EINVAL;
}

static const struct file_operations iommu_debug_map_fops = {
	.open	= simple_open,
	.write	= iommu_debug_map_write,
};

/*
 * Performs DMA mapping of a given virtual address and size to an iova address.
 * User input format: (addr,len,dma attr) where dma attr is:
 *				0: normal mapping
 *				1: force coherent mapping
 *				2: force non-cohernet mapping
 *				3: use system cache
 */
static ssize_t iommu_debug_dma_map_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *offset)
{
	ssize_t retval = -EINVAL;
	int ret;
	char *comma1, *comma2;
	char buf[100];
	unsigned long addr;
	void *v_addr;
	dma_addr_t iova;
	size_t size;
	unsigned int attr;
	unsigned long dma_attrs;
	struct iommu_debug_device *ddev = file->private_data;
	struct device *dev = ddev->dev;

	if (count >= sizeof(buf)) {
		pr_err_ratelimited("Value too large\n");
		return -EINVAL;
	}

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, ubuf, count)) {
		pr_err_ratelimited("Couldn't copy from user\n");
		return -EFAULT;
	}

	comma1 = strnchr(buf, count, ',');
	if (!comma1)
		goto invalid_format;

	comma2 = strnchr(comma1 + 1, count, ',');
	if (!comma2)
		goto invalid_format;

	*comma1 = *comma2 = '\0';

	if (kstrtoul(buf, 0, &addr))
		goto invalid_format;
	v_addr = (void *)addr;

	if (kstrtosize_t(comma1 + 1, 0, &size))
		goto invalid_format;

	if (kstrtouint(comma2 + 1, 0, &attr))
		goto invalid_format;

	if (v_addr < test_virt_addr || v_addr + size > test_virt_addr + SZ_1M)
		goto invalid_addr;

	if (attr == 0)
		dma_attrs = 0;
	else if (attr == 1)
		dma_attrs = DMA_ATTR_FORCE_COHERENT;
	else if (attr == 2)
		dma_attrs = DMA_ATTR_FORCE_NON_COHERENT;
	else if (attr == 3)
		dma_attrs = DMA_ATTR_IOMMU_USE_UPSTREAM_HINT;
	else
		goto invalid_format;

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}

	iova = dma_map_single_attrs(dev, v_addr, size,
					DMA_TO_DEVICE, dma_attrs);

	if (dma_mapping_error(dev, iova)) {
		pr_err_ratelimited("Failed to perform dma_map_single\n");
		ret = -EINVAL;
		goto out;
	}

	retval = count;
	pr_err_ratelimited("Mapped 0x%p to %pa (len=0x%zx)\n",
			v_addr, &iova, size);
	ddev->iova = iova;
		pr_err_ratelimited("Saved iova=%pa for future PTE commands\n",
				&iova);
out:
	mutex_unlock(&ddev->state_lock);
	return retval;

invalid_format:
	pr_err_ratelimited("Invalid format. Expected: addr,len,dma attr where 'dma attr' is\n0: normal mapping\n1: force coherent\n2: force non-cohernet\n3: use system cache\n");
	return retval;

invalid_addr:
	pr_err_ratelimited("Invalid addr given! Address should be within 1MB size from start addr returned by doing 'cat test_virt_addr'.\n");
	return retval;
}

static ssize_t iommu_debug_dma_map_read(struct file *file, char __user *ubuf,
	     size_t count, loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	char buf[100];
	dma_addr_t iova;

	if (*offset)
		return 0;

	memset(buf, 0, sizeof(buf));

	iova = ddev->iova;
	snprintf(buf, sizeof(buf), "%pa\n", &iova);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations iommu_debug_dma_map_fops = {
	.open	= simple_open,
	.write	= iommu_debug_dma_map_write,
	.read	= iommu_debug_dma_map_read,
};

static ssize_t iommu_debug_unmap_write(struct file *file,
				       const char __user *ubuf,
				       size_t count, loff_t *offset)
{
	ssize_t retval = 0;
	char *comma1;
	char buf[100];
	dma_addr_t iova;
	size_t size;
	size_t unmapped;
	struct iommu_debug_device *ddev = file->private_data;

	if (count >= 100) {
		pr_err_ratelimited("Value too large\n");
		return -EINVAL;
	}

	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		return -EINVAL;
	}

	memset(buf, 0, 100);

	if (copy_from_user(buf, ubuf, count)) {
		pr_err_ratelimited("Couldn't copy from user\n");
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

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}

	unmapped = iommu_unmap(ddev->domain, iova, size);
	if (unmapped != size) {
		pr_err_ratelimited("iommu_unmap failed. Expected to unmap: 0x%zx, unmapped: 0x%zx",
		       size, unmapped);
		retval = -EIO;
		goto out;
	}

	retval = count;
	pr_err_ratelimited("Unmapped %pa (len=0x%zx)\n", &iova, size);
out:
	mutex_unlock(&ddev->state_lock);
	return retval;

invalid_format:
	pr_err_ratelimited("Invalid format. Expected: iova,len\n");
	return -EINVAL;
}

static const struct file_operations iommu_debug_unmap_fops = {
	.open	= simple_open,
	.write	= iommu_debug_unmap_write,
};

static ssize_t iommu_debug_dma_unmap_write(struct file *file,
				       const char __user *ubuf,
				       size_t count, loff_t *offset)
{
	ssize_t retval = 0;
	char *comma1, *comma2;
	char buf[100];
	size_t size;
	unsigned int attr;
	dma_addr_t iova;
	unsigned long dma_attrs;
	struct iommu_debug_device *ddev = file->private_data;
	struct device *dev = ddev->dev;

	if (count >= sizeof(buf)) {
		pr_err_ratelimited("Value too large\n");
		return -EINVAL;
	}

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, ubuf, count)) {
		pr_err_ratelimited("Couldn't copy from user\n");
		retval = -EFAULT;
		goto out;
	}

	comma1 = strnchr(buf, count, ',');
	if (!comma1)
		goto invalid_format;

	comma2 = strnchr(comma1 + 1, count, ',');
	if (!comma2)
		goto invalid_format;

	*comma1 = *comma2 = '\0';

	if (kstrtoux(buf, 0, &iova))
		goto invalid_format;

	if (kstrtosize_t(comma1 + 1, 0, &size))
		goto invalid_format;

	if (kstrtouint(comma2 + 1, 0, &attr))
		goto invalid_format;

	if (attr == 0)
		dma_attrs = 0;
	else if (attr == 1)
		dma_attrs = DMA_ATTR_FORCE_COHERENT;
	else if (attr == 2)
		dma_attrs = DMA_ATTR_FORCE_NON_COHERENT;
	else if (attr == 3)
		dma_attrs = DMA_ATTR_IOMMU_USE_UPSTREAM_HINT;
	else
		goto invalid_format;

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}
	dma_unmap_single_attrs(dev, iova, size, DMA_TO_DEVICE, dma_attrs);

	retval = count;
	pr_err_ratelimited("Unmapped %pa (len=0x%zx)\n", &iova, size);
out:
	mutex_unlock(&ddev->state_lock);
	return retval;

invalid_format:
	pr_err_ratelimited("Invalid format. Expected: iova,len, dma attr\n");
	return retval;
}

static const struct file_operations iommu_debug_dma_unmap_fops = {
	.open	= simple_open,
	.write	= iommu_debug_dma_unmap_write,
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
		dev_err_ratelimited(dev, "Invalid value\n");
		return -EINVAL;
	}

	if (!ddev->domain) {
		dev_err_ratelimited(dev, "No domain. Did you already attach?\n");
		return -EINVAL;
	}

	if (copy_from_user(&buf, ubuf, 1)) {
		dev_err_ratelimited(dev, "Couldn't copy from user\n");
		return -EFAULT;
	}

	mutex_lock(&ddev->clk_lock);
	switch (buf) {
	case '0':
		if (ddev->clk_count == 0) {
			dev_err_ratelimited(dev, "Config clocks already disabled\n");
			break;
		}

		if (--ddev->clk_count > 0)
			break;

		dev_err_ratelimited(dev, "Disabling config clocks\n");
		iommu_disable_config_clocks(ddev->domain);
		break;
	case '1':
		if (ddev->clk_count++ > 0)
			break;

		dev_err_ratelimited(dev, "Enabling config clocks\n");
		if (iommu_enable_config_clocks(ddev->domain))
			dev_err_ratelimited(dev, "Failed!\n");
		break;
	default:
		dev_err_ratelimited(dev, "Invalid value. Should be 0 or 1.\n");
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

static ssize_t iommu_debug_trigger_fault_write(
		struct file *file, const char __user *ubuf, size_t count,
		loff_t *offset)
{
	struct iommu_debug_device *ddev = file->private_data;
	unsigned long flags;

	if (kstrtoul_from_user(ubuf, count, 0, &flags)) {
		pr_err_ratelimited("Invalid flags format\n");
		return -EFAULT;
	}

	mutex_lock(&ddev->state_lock);
	if (!ddev->domain) {
		pr_err_ratelimited("No domain. Did you already attach?\n");
		mutex_unlock(&ddev->state_lock);
		return -EINVAL;
	}
	iommu_trigger_fault(ddev->domain, flags);

	mutex_unlock(&ddev->state_lock);
	return count;
}

static const struct file_operations iommu_debug_trigger_fault_fops = {
	.open	= simple_open,
	.write	= iommu_debug_trigger_fault_write,
};

#ifdef CONFIG_ARM64_PTDUMP_CORE
static int ptdump_show(struct seq_file *s, void *v)
{
	struct iommu_debug_device *ddev = s->private;
	struct ptdump_info *info = &(ddev->pt_info);
	struct mm_struct		mm;
	phys_addr_t phys;

	info->markers = (struct addr_marker[]){
		{ 0,		"start" },
	};
	info->base_addr	= 0;
	info->mm = &mm;

	if (ddev->domain) {
		iommu_domain_get_attr(ddev->domain, DOMAIN_ATTR_PT_BASE_ADDR,
			  &(phys));

		info->mm->pgd = (pgd_t *)phys_to_virt(phys);
		ptdump_walk_pgd(s, info);
	}
	return 0;
}

static int ptdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, ptdump_show, inode->i_private);
}

static const struct file_operations ptdump_fops = {
	.open		= ptdump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

/*
 * The following will only work for drivers that implement the generic
 * device tree bindings described in
 * Documentation/devicetree/bindings/iommu/iommu.txt
 */
static int snarf_iommu_devices(struct device *dev, void *ignored)
{
	struct iommu_debug_device *ddev;
	struct dentry *dir;

	if (!of_find_property(dev->of_node, "iommus", NULL))
		return 0;

	if (!of_device_is_compatible(dev->of_node, "iommu-debug-test"))
		return 0;

	/* Hold a reference count */
	if (!iommu_group_get(dev))
		return 0;

	ddev = kzalloc(sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENODEV;

	mutex_init(&ddev->clk_lock);
	mutex_init(&ddev->state_lock);
	ddev->dev = dev;
	dir = debugfs_create_dir(dev_name(dev), debugfs_tests_dir);
	if (!dir) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s debugfs dir\n",
		       dev_name(dev));
		goto err;
	}

	if (!debugfs_create_file("nr_iters", 0400, dir, &iters_per_op,
				&iommu_debug_nr_iters_ops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/nr_iters debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("test_virt_addr", 0400, dir, ddev,
				&iommu_debug_test_virt_addr_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/test_virt_addr debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("profiling", 0400, dir, ddev,
				 &iommu_debug_profiling_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/profiling debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("secure_profiling", 0400, dir, ddev,
				 &iommu_debug_secure_profiling_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/secure_profiling debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("profiling_fast", 0400, dir, ddev,
				 &iommu_debug_profiling_fast_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/profiling_fast debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("profiling_fast_dma_api", 0400, dir, ddev,
				 &iommu_debug_profiling_fast_dma_api_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/profiling_fast_dma_api debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("functional_fast_dma_api", 0400, dir, ddev,
				 &iommu_debug_functional_fast_dma_api_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/functional_fast_dma_api debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("functional_arm_dma_api", 0400, dir, ddev,
				 &iommu_debug_functional_arm_dma_api_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/functional_arm_dma_api debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("dma_attach", 0600, dir, ddev,
				 &iommu_debug_dma_attach_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/dma_attach debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("attach", 0400, dir, ddev,
				 &iommu_debug_attach_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/attach debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("secure_attach", 0400, dir, ddev,
				 &iommu_debug_secure_attach_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/secure_attach debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("atos", 0200, dir, ddev,
				 &iommu_debug_atos_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/atos debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("dma_atos", 0600, dir, ddev,
				 &iommu_debug_dma_atos_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/dma_atos debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("map", 0200, dir, ddev,
				 &iommu_debug_map_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/map debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("dma_map", 0600, dir, ddev,
					 &iommu_debug_dma_map_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/dma_map debugfs file\n",
		       dev_name(dev));
			goto err_rmdir;
	}

	if (!debugfs_create_file("unmap", 0200, dir, ddev,
				 &iommu_debug_unmap_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/unmap debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("dma_unmap", 0200, dir, ddev,
					 &iommu_debug_dma_unmap_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/dma_unmap debugfs file\n",
		       dev_name(dev));
			goto err_rmdir;
	}

	if (!debugfs_create_file("pte", 0600, dir, ddev,
			&iommu_debug_pte_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/pte debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("config_clocks", 0200, dir, ddev,
				 &iommu_debug_config_clocks_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/config_clocks debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

	if (!debugfs_create_file("trigger-fault", 0200, dir, ddev,
				 &iommu_debug_trigger_fault_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/trigger-fault debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}

#ifdef CONFIG_ARM64_PTDUMP_CORE
	if (!debugfs_create_file("iommu_page_tables", 0200, dir, ddev,
			   &ptdump_fops)) {
		pr_err_ratelimited("Couldn't create iommu/devices/%s/trigger-fault debugfs file\n",
		       dev_name(dev));
		goto err_rmdir;
	}
#endif

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
					       iommu_debugfs_top);
	if (!debugfs_tests_dir) {
		pr_err_ratelimited("Couldn't create iommu/tests debugfs directory\n");
		return -ENODEV;
	}

	test_virt_addr = kzalloc(SZ_1M, GFP_KERNEL);

	if (!test_virt_addr)
		return -ENOMEM;

	return bus_for_each_dev(&platform_bus_type, NULL, NULL,
				snarf_iommu_devices);
}

static void iommu_debug_destroy_tests(void)
{
	debugfs_remove_recursive(debugfs_tests_dir);
}
#else
static inline int iommu_debug_init_tests(void) { return 0; }
static inline void iommu_debug_destroy_tests(void) { }
#endif

/*
 * This isn't really a "driver", we just need something in the device tree
 * so that our tests can run without any client drivers, and our tests rely
 * on parsing the device tree for nodes with the `iommus' property.
 */
static int iommu_debug_pass(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id iommu_debug_of_match[] = {
	{ .compatible = "iommu-debug-test" },
	{ },
};

static struct platform_driver iommu_debug_driver = {
	.probe = iommu_debug_pass,
	.remove = iommu_debug_pass,
	.driver = {
		.name = "iommu-debug",
		.of_match_table = iommu_debug_of_match,
	},
};

static int iommu_debug_init(void)
{
	if (platform_driver_register(&iommu_debug_driver))
		return -ENODEV;

	if (iommu_debug_init_tests())
		return -ENODEV;

	return 0;
}

static void iommu_debug_exit(void)
{
	platform_driver_unregister(&iommu_debug_driver);
	iommu_debug_destroy_tests();
}

module_init(iommu_debug_init);
module_exit(iommu_debug_exit);
