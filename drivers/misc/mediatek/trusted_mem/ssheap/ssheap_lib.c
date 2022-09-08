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
#include <linux/sched/clock.h>
#include <linux/spinlock.h>

#include <private/ssheap_priv.h>
#include <public/trusted_mem_api.h>
#include <ssmr/memory_ssmr.h>

#define independent_ssheap 0

#define HYP_PMM_ASSIGN_BUFFER (0XBB00FFA0)
#define HYP_PMM_UNASSIGN_BUFFER (0XBB00FFA1)
#define HYP_PMM_SET_CMA_REGION (0XBB00FFA8)
#define HYP_PMM_ENABLE_CMA (0XBB00FFA9)
#define HYP_PMM_DISABLE_CMA (0XBB00FFAA)

#define ENABLE_CACHE_PAGE 1

#define USE_SSMR_ZONE 1
#define MAX_SSMR_RETRY 10

#define PREFER_GRANULE SZ_2M
#define MIN_GRANULE SZ_2M

#define PGLIST_PA_BITS (48)
#define PGLIST_PA_MASK ((1ULL << PGLIST_PA_BITS) - 1)
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

static struct list_head cache_list;
static DEFINE_SPINLOCK(cache_lock);

struct ssheap_page {
	struct list_head entry;
	struct page *page;
	u32 size;
};

struct ssheap_block {
	struct list_head entry;
	void *cpu_addr;
	dma_addr_t dma_addr;
	struct page *page;
	unsigned long block_size;
};


#if USE_SSMR_ZONE
static DEFINE_MUTEX(ssmr_lock);
static struct page *cma_page;
static uint32_t hyp_pmm_cma_cookie;

static int hyp_pmm_enable_cma(void)
{
	struct arm_smccc_res smc_res;
	long ret;

	arm_smccc_smc(HYP_PMM_ENABLE_CMA, 0, 0, 0, 0, 0, 0, 0, &smc_res);

	ret = (long)smc_res.a0;
	if (ret <= 0) {
		pr_err("%s: enable cma failed %llx\n", __func__, smc_res.a0);
		return -EFAULT;
	}

	hyp_pmm_cma_cookie = (uint32_t)smc_res.a0;
	return 0;
}

static int hyp_pmm_disable_cma(void)
{
	struct arm_smccc_res smc_res;
	uint32_t cookie;

	cookie = hyp_pmm_cma_cookie;
	arm_smccc_smc(HYP_PMM_DISABLE_CMA, cookie, 0, 0, 0, 0, 0, 0, &smc_res);

	if (smc_res.a0 != 0) {
		pr_err("%s: enable cma failed %llx\n", __func__, smc_res.a0);
		return -EFAULT;
	}

	hyp_pmm_cma_cookie = 0;
	return 0;
}

static void ssmr_zone_offline(void)
{
	unsigned long long start, end, tmp;
	int retry = 0;
	uint64_t size;
	void *kaddr;
	unsigned long flags;
	struct ssheap_page *s_page = NULL;
	int res = 0;

	if (!ssheap_dev->cma_area) {
		pr_info("%s: no cma_area", __func__);
		return;
	}

	if (cma_page) {
		pr_info("%s: already offline", __func__);
		return;
	}
retry:
	/* get start time */
	start = sched_clock();
	pr_info("%s: cma_alloc size=%llx\n", __func__, ssheap_phys_size);

	cma_page = cma_alloc(ssheap_dev->cma_area, ssheap_phys_size >> PAGE_SHIFT,
			 get_order(SZ_2M), GFP_KERNEL);

	/* get end time */
	end = sched_clock();
	tmp = end - start;
	end = tmp;
	start = do_div(end, 1000000);
	//pr_info("%s: duration: %d ns (%d ms)\n", __func__, end - start, (end - start) / 1000000);
	pr_info("%s: duration: %d ns (%d ms)\n", __func__, tmp, end);

	if (cma_page == NULL) {
		pr_warn("%s: cma_alloc failed retry:%d\n", __func__, retry);
		retry++;
		if (retry <= MAX_SSMR_RETRY) {
			pr_warn("%s: sleep, then retry: %d\n", __func__, retry);
			msleep(100);
			goto retry;
		} else {
			pr_err("%s: retry failed\n", __func__);
			return;
		}
	}

	/* enable cma */
	pr_info("%s: enable cma\n", __func__);
	res = hyp_pmm_enable_cma();
	if (res) {
		pr_err("%s: enable cma failed %d\n", __func__, res);
		cma_release(ssheap_dev->cma_area, cma_page, ssheap_phys_size >> PAGE_SHIFT);
		cma_page = NULL;
		return;
	}

	pr_info("%s: cma_alloc success, cma_page=%llx pa=%llx, size=%llx\n", __func__,
		(u64)cma_page, page_to_phys(cma_page), ssheap_phys_size);
	size = (uint64_t)ssheap_phys_size;
	kaddr = page_address(cma_page);

	spin_lock_irqsave(&cache_lock, flags);
	/* put into cache page list */
	do {
		s_page = kzalloc(sizeof(struct ssheap_page), GFP_KERNEL);
		s_page->page = virt_to_page(kaddr);
		s_page->size = SZ_2M;
		list_add_tail(&s_page->entry, &cache_list);

		size -= SZ_2M;
		kaddr += SZ_2M;
	} while (size);
	pr_info("%s: ssmr page base enable: %d\n", __func__, res);
	spin_unlock_irqrestore(&cache_lock, flags);
}

static void ssmr_zone_online(void)
{
	if (!ssheap_dev->cma_area) {
		pr_info("%s: no cma_area", __func__);
		return;
	}

	if (!cma_page) {
		pr_info("%s: already online\n", __func__);
		return;
	}

	pr_info("%s: free %llx cma_page=%llx\n", __func__, ssheap_phys_size, (u64)cma_page);
	if (hyp_pmm_disable_cma() == 0) {
		cma_release(ssheap_dev->cma_area, cma_page, ssheap_phys_size >> PAGE_SHIFT);
		cma_page = NULL;
	} else {
		pr_err("%s: disable cma failed\n", __func__);
	}
}
#endif

static inline void free_system_mem(struct page *page, u32 size)
{
#if ENABLE_CACHE_PAGE
	unsigned long flags;
	struct ssheap_page *s_page = NULL;

	spin_lock_irqsave(&cache_lock, flags);
	s_page = kzalloc(sizeof(struct ssheap_page), GFP_KERNEL);
	s_page->page = page;
	s_page->size = size;
	list_add_tail(&s_page->entry, &cache_list);
	spin_unlock_irqrestore(&cache_lock, flags);
#else
	if (!ssheap_dev->cma_area)
		__free_pages(page, get_order(size));
	else
		cma_release(ssheap_dev->cma_area, page, size >> PAGE_SHIFT);
#endif
}

static inline struct page *alloc_system_mem(u32 block_size)
{
	struct page *page = NULL;
#if ENABLE_CACHE_PAGE
	struct ssheap_page *s_page = NULL;
	unsigned long flags;
#endif

#if USE_SSMR_ZONE
again:
#endif

#if ENABLE_CACHE_PAGE
	spin_lock_irqsave(&cache_lock, flags);
	if (!list_empty(&cache_list)) {
		s_page = list_first_entry(&cache_list, struct ssheap_page, entry);
		list_del(&s_page->entry);
		page = s_page->page;
		kfree(s_page);
		s_page = NULL;
	}
	spin_unlock_irqrestore(&cache_lock, flags);

	if (page)
		return page;

#if USE_SSMR_ZONE
	mutex_lock(&ssmr_lock);
	if (!cma_page)
		ssmr_zone_offline();
	mutex_unlock(&ssmr_lock);

	if (cma_page)
		goto again;
	else
		return NULL;
#endif

#endif

	if (!ssheap_dev->cma_area)
		page = alloc_pages(GFP_KERNEL, get_order(SZ_2M));
	else {
#if USE_SSMR_ZONE
		/* SSMR allocation, no need to call cma_alloc */
#else
		page = cma_alloc(ssheap_dev->cma_area, block_size >> PAGE_SHIFT,
				 get_order(block_size), GFP_KERNEL);
#endif
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
		freed_size += block->block_size;
		list_del(&block->entry);
		kfree(block);
	}
	return freed_size;
}

int ssheap_free_non_contig(struct ssheap_buf_info *info)
{
	unsigned long freed_size;
# if ENABLE_CACHE_PAGE
	struct ssheap_page *s_page = NULL;
	struct page *page = NULL;
	u32 size;
	unsigned long flags;
#endif

	if (!info)
		return -EINVAL;

	freed_size = free_blocks(info);

	if (info->pmm_msg_page)
		__free_pages(info->pmm_msg_page, get_order(PAGE_SIZE));

#if ENABLE_CACHE_PAGE

#if USE_SSMR_ZONE
	mutex_lock(&ssmr_lock);
#endif

	spin_lock_irqsave(&cache_lock, flags);

	/* Free cache pages if size is 0 */
	if (atomic64_sub_return(freed_size, &total_alloced_size) == 0x0) {
		pr_info("free all pooling pages");
		while (!list_empty(&cache_list)) {
			s_page = list_first_entry(&cache_list, struct ssheap_page, entry);
			list_del(&s_page->entry);
			page = s_page->page;
			size = s_page->size;
			kfree(s_page);
			s_page = NULL;
			if (!ssheap_dev->cma_area)
				__free_pages(page, get_order(size));
			else {
#if USE_SSMR_ZONE
				/* No need for ssmr zone */
#else
				cma_release(ssheap_dev->cma_area, page, size >> PAGE_SHIFT);
#endif
			}
			page = NULL;
		}
#if USE_SSMR_ZONE
		ssmr_zone_online();
#endif
	}
	spin_unlock_irqrestore(&cache_lock, flags);

#if USE_SSMR_ZONE
	mutex_unlock(&ssmr_lock);
#endif

#else
	atomic64_sub(freed_size, &total_alloced_size);
#endif
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

	//mutex_lock(&ssheap_alloc_lock);

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
		if (!block) {
			free_system_mem(page, block_size);
			goto out_err;
		}

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
	struct arm_smccc_res smc_res;

	pr_info("use cma base=%pa size=%pa\n", &base, &size);
	use_cma = true;
	ssheap_phys_base = base;
	ssheap_phys_size = size;
	arm_smccc_smc(HYP_PMM_SET_CMA_REGION, ssheap_phys_base >> 20,
		      ssheap_phys_size >> 20, 0, 0, 0, 0, 0, &smc_res);
}

void ssheap_set_dev(struct device *dev)
{
	ssheap_dev = dev;
	INIT_LIST_HEAD(&cache_list);
}
