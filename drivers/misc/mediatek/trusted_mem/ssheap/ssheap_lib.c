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

#include <private/ssheap_priv.h>
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

static bool use_buddy_system;
static bool use_cma;
static struct device *ssheap_dev;
static phys_addr_t ssheap_phys_base;
static phys_addr_t ssheap_phys_size;
static atomic64_t total_alloced_size;
static DEFINE_MUTEX(ssheap_alloc_lock);

struct ssheap_block {
	struct list_head entry;
	void *cpu_addr;
	dma_addr_t dma_addr;
	struct page *page;
	unsigned long block_size;
};

static inline void free_system_mem(struct page *page, u32 size)
{
	if (!ssheap_dev->cma_area)
		__free_pages(page, get_order(size));
	else
		cma_release(ssheap_dev->cma_area, page, size >> PAGE_SHIFT);
}

static inline struct page *alloc_system_mem(u32 block_size)
{
	struct page *page = NULL;

	if (!ssheap_dev->cma_area)
		page = alloc_pages(GFP_KERNEL, get_order(SZ_2M));
	else {
		page = cma_alloc(ssheap_dev->cma_area, block_size >> PAGE_SHIFT,
				 get_order(block_size), GFP_KERNEL);
	}

	return page;
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

		free_system_mem(block->page, block->block_size);
		//cma_release(ssheap_dev->cma_area, block->page,
		//	    block->block_size >> PAGE_SHIFT);
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
		__free_pages(info->pmm_msg_page, get_order(PAGE_SIZE));

	atomic64_sub(freed_size, &total_alloced_size);
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

	if (!ssheap_dev) {
		pr_err("Invalid dev\n");
		return NULL;
	}

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
	if (use_cma) {
		if (atomic64_read(&total_alloced_size) + aligned_req_size >
		    ssheap_phys_size) {
			pr_err("ssheap cma memory not enough!\n");
			goto out_err;
		}
	} else if (use_buddy_system) {
		/* Check in TMEM, no need to check here */
	} else {
		pr_err("ssheap memory not ready\n");
		goto out_err;
	}

	attrs = DMA_ATTR_NO_KERNEL_MAPPING;
	info->mem_type = mem_type;

	while (allocated_size < aligned_req_size) {
retry:
		/* allocate for each block */
		page = alloc_system_mem(block_size);
		if (!page) {
			block_size = MIN_GRANULE;
			retry_count++;
			pr_warn("retry(%ld) failed\n", retry_count);
			msleep(100);
			if (retry_count >= 2)
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
	atomic64_add(allocated_size, &total_alloced_size);
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

	dma_sync_sgtable_for_cpu(ssheap_dev, info->table, DMA_BIDIRECTIONAL);

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
	arm_smccc_smc(HYP_PMM_ASSIGN_BUFFER, lower_32_bits(pmm_attr),
		      upper_32_bits(pmm_attr), count, 0, 0, 0, 0, &smc_res);
	pr_debug("pmm_msg_page paddr=%pa smc_res.a0=%x\n", &paddr, smc_res.a0);

	return smc_res.a0;
}

unsigned long mtee_unassign_buffer(struct ssheap_buf_info *info,
				   uint8_t mem_type)
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

void ssheap_dump_mem_info(void)
{
	long long total_allocated_size;

	pr_info("%s: ssheap base=%pa, size=%pa\n", __func__, &ssheap_phys_base,
		&ssheap_phys_size);

	total_allocated_size = atomic64_read(&total_alloced_size);
	pr_info("%s: total_alloced_size: 0x%x free: 0x%x\n", __func__,
		total_allocated_size, ssheap_phys_size - total_allocated_size);
}

long long ssheap_get_used_size(void)
{
	return atomic64_read(&total_alloced_size);
}

void ssheap_enable_buddy_system(bool enable)
{
	pr_info("enable buddy system allocation\n");
	use_buddy_system = enable;
}

void ssheap_set_cma_region(phys_addr_t base, phys_addr_t size)
{
	pr_info("use cma base=%pa size=%pa\n", &base, &size);
	use_cma = true;
	ssheap_phys_base = base;
	ssheap_phys_size = size;
}

void ssheap_set_dev(struct device *dev)
{
	ssheap_dev = dev;
}
