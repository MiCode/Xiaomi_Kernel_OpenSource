// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt) "[TMEM] ssheap: " fmt

#include <linux/types.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <linux/dma-direct.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/arm-smccc.h>
#include <linux/parser.h>

#include <public/trusted_mem_api.h>

#define independent_ssheap 0

#define HYP_PMM_ASSIGN_BUFFER (0XBB00FFA0)
#define HYP_PMM_UNASSIGN_BUFFER (0XBB00FFA1)

#define PREFER_GRANULE SZ_2M
#define MIN_GRANULE SZ_2M

#define PGLIST_PA_BITS (48)
#define PGLIST_PA_MASK ((1UL << PGLIST_PA_BITS) - 1)
#define PGLIST_ATTR_BITS (HYP_PMM_ATTR_BITS)
#define PGLIST_SET_ATTR(pa, attr)                                              \
	(((uint64_t)pa & PGLIST_PA_MASK) | ((uint64_t)attr << PGLIST_PA_BITS))

static struct device *ssheap_dev;
static phys_addr_t ssheap_base;
static phys_addr_t ssheap_size;
atomic64_t ssheap_total_allocated_size;
static struct mutex ssheap_alloc_lock;

struct ssheap_block {
	struct list_head entry;
	void *cpu_addr;
	dma_addr_t dma_addr;
	struct page *page;
	unsigned long block_size;
};

enum {
	UT_OPT_ERR = 0,
	UT_OPT_CMD = 1 << 0,
	UT_OPT_SIZE = 1 << 1,
	UT_OPT_ALIGN = 1 << 2,
	UT_OPT_DUMP = 1 << 3,
};

static const match_table_t opt_tokens = { { UT_OPT_CMD, "cmd=%d" },
					  { UT_OPT_SIZE, "size=%x" },
					  { UT_OPT_ALIGN, "align=%x" },
					  { UT_OPT_DUMP, "dump=%d" },
					  { UT_OPT_ERR, NULL } };

static int ssheap_open(__always_unused struct inode *inode,
		       __always_unused struct file *file)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

static int ssheap_release(__always_unused struct inode *ino,
			  __always_unused struct file *file)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

static void dump_debug_info(void)
{
	int total_allocated_size;

	pr_info("%s: ssheap base=%pa, size=%pa\n", __func__, &ssheap_base,
		&ssheap_size);

	total_allocated_size = atomic64_read(&ssheap_total_allocated_size);
	pr_info("%s: ssheap_total_allocated_size: 0x%x\n", __func__,
		total_allocated_size);
	pr_info("%s: free_size: 0x%llx\n", __func__,
		ssheap_size - total_allocated_size);
}

static void dump_buf_info(struct ssheap_buf_info *info)
{
	int i;
	struct scatterlist *sg;
	uint64_t dma_addr;

	pr_info("%s: info:%p", __func__, info);
	pr_info("%s: table=%p", __func__, info->table);
	pr_info("%s: alignment: 0x%lx", __func__, info->alignment);
	pr_info("%s: req_size: 0x%lx", __func__, info->req_size);
	pr_info("%s: aligned_req_size: 0x%lx", __func__,
		info->aligned_req_size);
	pr_info("%s: allocated_size: 0x%lx", __func__, info->allocated_size);
	pr_info("%s: elemts: %ld", __func__, info->elems);

	if (info->table) {
		for_each_sg(info->table->sgl, sg, info->table->nents, i) {
			dma_addr = sg_dma_address(sg);
			pr_info("%s: sg idx:%d dma_addr=%llx size=%x\n",
				__func__, i, dma_addr, sg->length);
		}
	}
}

static u64 free_blocks(struct ssheap_buf_info *info)
{
	struct ssheap_block *block, *temp;
	u64 freed_size = 0;

	/* free blocks */
	list_for_each_entry_safe(block, temp, &info->block_list, entry) {
		pr_debug(
			"free block cpu_addr=%p dma_addr=0x%lx block_size=0x%lx\n",
			block->cpu_addr, (unsigned long)block->dma_addr,
			block->block_size);

		cma_release(ssheap_dev->cma_area, block->page,
			    block->block_size >> PAGE_SHIFT);
		freed_size += block->block_size;
		list_del(&block->entry);
		kfree(block);
	}
	return freed_size;
}

int ssheap_free_non_contig(struct ssheap_buf_info *info)
{
	unsigned long freed_size;

	if (!info)
		return -EINVAL;

	freed_size = free_blocks(info);

	if (info->pmm_msg_page)
		free_pages((unsigned long)page_address(info->pmm_msg_page),
			   get_order(PAGE_SIZE));

	atomic64_sub(freed_size, &ssheap_total_allocated_size);
	kfree(info);

	return 0;
}

struct ssheap_buf_info *ssheap_alloc_non_contig(u32 req_size, u32 prefer_align,
						u8 mem_type)
{
	unsigned long aligned_req_size;
	unsigned long block_size;
	struct ssheap_buf_info *info;
	unsigned long allocated_size = 0;
	unsigned long elems = 0;
	unsigned long attrs = 0;
	void *cpu_addr = NULL;
	struct ssheap_block *block, *temp;
	unsigned long retry_count = 0;
	struct scatterlist *sg;
	struct page *page;
	int ret;

	if (req_size == 0) {
		pr_err("Invalid size, size=0x%lx\n", req_size);
		return NULL;
	}

	if (prefer_align == 0)
		prefer_align = PREFER_GRANULE;
	aligned_req_size = round_up(req_size, prefer_align);

	/* make sure aligned size is aligned with min granularity */
	if (!IS_ALIGNED(aligned_req_size, MIN_GRANULE)) {
		pr_err("size is not aligned with 0x%x\n", MIN_GRANULE);
		return NULL;
	}

	block_size = prefer_align;

	/* allocate buffer info */
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return NULL;
	INIT_LIST_HEAD(&info->block_list);

	mutex_lock(&ssheap_alloc_lock);

	info->alignment = prefer_align;
	info->table = kmalloc(sizeof(*info->table), GFP_KERNEL);
	if (!info->table) {
		pr_err("allocate sg_table failed\n");
		goto out_err;
	}

	/* check available size */
	if (atomic64_read(&ssheap_total_allocated_size) + aligned_req_size >
	    ssheap_size) {
		pr_err("ssheap cma memory not enough!\n");
		goto out_err;
	}

	attrs = DMA_ATTR_NO_KERNEL_MAPPING;
	info->mem_type = mem_type;

	while (allocated_size < aligned_req_size) {
retry:
		/* allocate for each block */
		page = cma_alloc(ssheap_dev->cma_area, block_size >> PAGE_SHIFT,
				 get_order(block_size), GFP_KERNEL);
		if (!page) {
			pr_warn("retry(%ld) failed\n", retry_count);
			block_size = MIN_GRANULE;
			retry_count++;
			pr_warn("retry(%ld) failed\n", retry_count);
			msleep(100);
			if (retry_count >= 5)
				goto out_err;
			goto retry;
		}

		/* allocated and add to block_list */
		block = kzalloc(sizeof(*block), GFP_KERNEL);
		if (!block)
			goto out_err;

		block->cpu_addr = cpu_addr;
		block->dma_addr = page_to_phys(page);
		block->page = page;
		block->block_size = block_size;
		list_add_tail(&block->entry, &info->block_list);

		pr_debug("dma_addr=%lx size=%lx\n", block->dma_addr,
			 block->block_size);

		allocated_size += block_size;
		elems++;
	}
	atomic64_add(allocated_size, &ssheap_total_allocated_size);
	info->req_size = req_size;
	info->aligned_req_size = aligned_req_size;
	info->allocated_size = allocated_size;
	info->elems = elems;
	mutex_unlock(&ssheap_alloc_lock);

	/* setting sg_table */
	ret = sg_alloc_table(info->table, elems, GFP_KERNEL);
	if (unlikely(ret)) {
		pr_err("sg_alloc_table failed, ret=%d elems=%ld\n", ret, elems);
		goto out_err2;
	}

	sg = info->table->sgl;
	list_for_each_entry_safe(block, temp, &info->block_list, entry) {
		sg_set_page(sg, block->page, block->block_size, 0);
		sg_dma_address(sg) = block->dma_addr;
		sg = sg_next(sg);
	}

	return info;
out_err:
	mutex_unlock(&ssheap_alloc_lock);
out_err2:
	/* free blocks */
	free_blocks(info);
	kfree(info);
	return NULL;
}

static int create_pmm_msg(struct ssheap_buf_info *info)
{
	unsigned int order = get_order(PAGE_SIZE);
	struct page *page;
	void *kaddr;
	phys_addr_t paddr;
	int i;
	struct scatterlist *sg;
	uint64_t dma_addr;
	uint64_t *pmm_msg_entry;

	page = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (!page)
		return -ENOMEM;

	info->pmm_msg_page = page;
	kaddr = page_address(page);
	paddr = page_to_phys(page);
	pmm_msg_entry = (uint64_t *)kaddr;

	pr_debug("pmm_msg_page paddr=%pa\n", &paddr);

	for_each_sg(info->table->sgl, sg, info->table->nents, i) {
		dma_addr = sg_dma_address(sg);
		*(pmm_msg_entry++) = dma_addr;
		pr_debug("%s: sg idx:%d dma_addr=%llx size=%x\n", __func__, i,
			 dma_addr, sg->length);
	}

	return 0;
}

unsigned long mtee_assign_buffer(struct ssheap_buf_info *info, uint8_t mem_type)
{
	struct arm_smccc_res smc_res;
	phys_addr_t paddr;
	uint64_t pmm_attr;
	uint32_t count;
	int ret;

	if (!info || !info->table)
		return -EINVAL;

	/* Fill sgtable into pmm msg */
	ret = create_pmm_msg(info);
	if (ret) {
		pr_err("create pmm pages msg failed ret=%d\n", ret);
		return ret;
	};

	if (!info->pmm_msg_page)
		return -ENOMEM;

	paddr = page_to_phys(info->pmm_msg_page);
	count = info->elems;
	pmm_attr = PGLIST_SET_ATTR(paddr, mem_type);
	pr_debug("pmm_msg_page paddr=%pa\n", &paddr);
	arm_smccc_smc(HYP_PMM_ASSIGN_BUFFER, lower_32_bits(pmm_attr),
		      upper_32_bits(pmm_attr), count, 0, 0, 0, 0, &smc_res);
	pr_debug("smc_res.a0=%x\n", smc_res.a0);

	return smc_res.a0;
}

unsigned long mtee_unassign_buffer(struct ssheap_buf_info *info, uint8_t mem_type)
{
	struct arm_smccc_res smc_res;
	phys_addr_t paddr;
	uint64_t pmm_attr;
	uint32_t count;

	if (!info || !info->pmm_msg_page)
		return -EINVAL;

	paddr = page_to_phys(info->pmm_msg_page);
	count = info->elems;
	pmm_attr = PGLIST_SET_ATTR(paddr, mem_type);
	pr_debug("pmm_msg_page paddr=%pa\n", &paddr);
	arm_smccc_smc(HYP_PMM_UNASSIGN_BUFFER, lower_32_bits(paddr),
		      upper_32_bits(paddr), count, 0, 0, 0, 0, &smc_res);
	pr_debug("smc_res.a0=%x\n", smc_res.a0);
	return smc_res.a0;
}

#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
static void ssheap_ut(const char *buf)
{
	unsigned long attrs = 0;
	void *ptr = NULL;
	dma_addr_t handle;
	struct ssheap_buf_info *info;
	static struct ssheap_buf_info *last_info;
	int token;
	substring_t args[MAX_OPT_ARGS];
	uint32_t ut_cmd = 0;
	uint32_t ut_size = 0;
	uint32_t ut_align = 0;
	uint32_t ut_dump = 0;
	char *options, *p, *o;
	struct page *page = NULL;
	unsigned long smc_ret;

	options = o = kstrdup(buf, GFP_KERNEL);

	while ((p = strsep(&o, " \t\n")) != NULL) {
		if (!*p)
			continue;
		pr_info("args: %s\n", p);
		token = match_token(p, opt_tokens, args);
		switch (token) {
		case UT_OPT_CMD:
			if (match_int(args, &token))
				break;
			ut_cmd = token;
			break;
		case UT_OPT_SIZE:
			if (match_hex(args, &token))
				break;
			ut_size = token;
			break;
		case UT_OPT_ALIGN:
			if (match_hex(args, &token))
				break;
			ut_align = token;
			break;
		case UT_OPT_DUMP:
			if (match_hex(args, &token))
				break;
			ut_dump = token;
			break;
		default:
			break;
		}
	}

	pr_info("cmd=%d size=%#x align=%#x\n", ut_cmd, ut_size, ut_align);
	if (ut_cmd == 1) {
		info = ssheap_alloc_non_contig(ut_size, ut_align, 0xff);
		if (info == NULL) {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		} else {
			smc_ret = mtee_assign_buffer(info, 0xff);
			pr_debug("secure buffer ret:%d (0x%x)\n", smc_ret,
				 smc_ret);
			smc_ret = mtee_unassign_buffer(info, 0xff);
			pr_debug("unsecure buffer ret:%d (0x%x)\n", smc_ret,
				 smc_ret);
			if (ut_dump)
				dump_buf_info(info);
			ssheap_free_non_contig(info);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		}
	}
	if (ut_cmd == 11) {
		if (last_info) {
			pr_info("cmd=%d FAILED (last_info should be NULL)\n",
				ut_cmd);
			goto out;
		}
		last_info = ssheap_alloc_non_contig(ut_size, ut_align, 0xff);
		if (last_info == NULL) {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		} else {
			smc_ret = mtee_assign_buffer(last_info, 0xff);
			pr_info("secure buffer ret:%d (0x%x)\n", smc_ret,
				smc_ret);
			if (ut_dump)
				dump_buf_info(last_info);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		}
	}
	if (ut_cmd == 12) {
		if (last_info == NULL) {
			pr_info("last_info is NULL\n");
			pr_info("cmd=%d FAILED\n", ut_cmd);
			goto out;
		}
		smc_ret = mtee_unassign_buffer(last_info, 0xff);
		pr_info("unsecure buffer ret:%d (0x%x)\n", smc_ret, smc_ret);

		ssheap_free_non_contig(last_info);
		last_info = NULL;
		pr_info("cmd=%d PASSED\n", ut_cmd);
	}
	if (ut_cmd == 3) {
		attrs = DMA_ATTR_NO_KERNEL_MAPPING;
		ptr = dma_alloc_attrs(ssheap_dev, ut_size, &handle, GFP_KERNEL,
				      attrs);
		pr_info("ptr=%p dma_handle=%lx dma_phys=%llx\n", ptr,
			(unsigned long)handle, dma_to_phys(ssheap_dev, handle));
		if (ptr) {
			pr_info("free ptr=%p dma_handle=%lx\n", handle);
			dma_free_attrs(ssheap_dev, ut_size, ptr, handle, attrs);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		} else {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		}
	}
	if (ut_cmd == 4) {
		page = cma_alloc(ssheap_dev->cma_area, ut_size >> PAGE_SHIFT,
				 get_order(ut_align), GFP_KERNEL);
		if (page) {
			pr_info("allocated phys_addr=%lx\n",
				page_to_phys(page));
			cma_release(ssheap_dev->cma_area, page,
				    ut_size >> PAGE_SHIFT);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		} else {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		}
	}

out:
	if (ut_dump)
		dump_debug_info();

	kfree(options);
}

static ssize_t ssheap_write(struct file *file, const char __user *buffer,
			    size_t count, loff_t *data)
{
	char desc[128];

	if (copy_from_user(desc, buffer, count))
		return 0;
	pr_info("write count:%d\n", count);
	ssheap_ut(desc);
	return count;
}

static const struct proc_ops ssheap_fops = {
	.proc_open = ssheap_open,
	.proc_release = ssheap_release,
	.proc_ioctl = NULL,
	.proc_write = ssheap_write,
};

#endif

static int get_reserved_cma_memory(struct device *dev)
{
	struct device_node *np;
	struct reserved_mem *rmem;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);

	if (!np) {
		pr_info("%s, no ssheap region\n", __func__);
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(np);
	of_node_put(np);

	if (!rmem) {
		pr_info("%s, no ssmr device info\n", __func__);
		return -EINVAL;
	}

	ssheap_base = rmem->base;
	ssheap_size = rmem->size;
	pr_info("cma base=%pa, size=%pa\n", &rmem->base, &rmem->size);

	/*
	 * setup init device with rmem
	 */
	of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);

	return 0;
}

uint64_t ssheap_get_used_size(void)
{
	uint64_t used_size;
	uint64_t free_size;

	free_size = atomic64_read(&ssheap_total_allocated_size);
	used_size = ssheap_size - free_size;
	return used_size;
}

int ssheap_init(struct platform_device *pdev)
{
	pr_info("%s:%d\n", __func__, __LINE__);

	ssheap_dev = &pdev->dev;

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
	proc_create("ssheap0", 0664, NULL, &ssheap_fops);
#endif

	mutex_init(&ssheap_alloc_lock);

	get_reserved_cma_memory(ssheap_dev);

	return 0;
}

int ssheap_exit(struct platform_device *pdev)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

#if independent_ssheap
static const struct of_device_id tm_of_match_table[] = {
	{ .compatible = "mediatek,trusted_mem_ssheap" },
	{},
};

static struct platform_driver ssheap_driver = {
	.probe = ssheap_init,
	.remove = ssheap_exit,
	.driver = {
			.name = "trusted_mem_ssheap",
			.of_match_table = tm_of_match_table,
	},
};
module_platform_driver(ssheap_driver);

MODULE_DESCRIPTION("Mediatek Trusted Secure Subsystem Heap Driver");
MODULE_LICENSE("GPL");
#endif
