// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/memory.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/mmu_context.h>
#include <linux/mmzone.h>
#include <linux/mm_inline.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/migrate.h>
#include <linux/vmstat.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include <linux/page-isolation.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <soc/qcom/rpm-smd.h>

#define RPM_DDR_REQ 0x726464
#define AOP_MSG_ADDR_MASK		0xffffffff
#define AOP_MSG_ADDR_HIGH_SHIFT		32
#define MAX_LEN				96

/**
 * bypass_send_msg - skip mem offline/online mesg sent to rpm/aop
 */
static bool bypass_send_msg;
module_param(bypass_send_msg, bool, 0644);
MODULE_PARM_DESC(bypass_send_msg,
	"skip mem offline/online mesg sent to rpm/aop.");

static unsigned long start_section_nr, end_section_nr;
static struct kobject *kobj;
static unsigned int sections_per_block;
static atomic_t target_migrate_pages = ATOMIC_INIT(0);
static u32 offline_granule;
static bool is_rpm_controller;
static bool has_pend_offline_req;
static atomic_long_t totalram_pages_with_offline = ATOMIC_INIT(0);
static struct workqueue_struct *migrate_wq;
static unsigned long movable_bitmap;

#define MODULE_CLASS_NAME	"mem-offline"
#define MIGRATE_TIMEOUT_SEC	(20)

struct section_stat {
	unsigned long success_count;
	unsigned long fail_count;
	unsigned long avg_time;
	unsigned long best_time;
	unsigned long worst_time;
	unsigned long total_time;
	unsigned long last_recorded_time;
	ktime_t resident_time;
	ktime_t resident_since;
};

enum memory_states {
	MEMORY_ONLINE,
	MEMORY_OFFLINE,
	MAX_STATE,
};

static enum memory_states *mem_sec_state;

static struct mem_offline_mailbox {
	struct mbox_client cl;
	struct mbox_chan *mbox;
} mailbox;

struct memory_refresh_request {
	u64 start;	/* Lower bit signifies action
			 * 0 - disable self-refresh
			 * 1 - enable self-refresh
			 * upper bits are for base address
			 */
	u32 size;	/* size of memory region */
};

static struct section_stat *mem_info;

struct movable_zone_fill_control {
	struct list_head freepages;
	unsigned long start_pfn;
	unsigned long end_pfn;
	unsigned long nr_migrate_pages;
	unsigned long nr_free_pages;
	unsigned long limit;
	int target;
	struct zone *zone;
};

static void fill_movable_zone_fn(struct work_struct *work);
static DECLARE_WORK(fill_movable_zone_work, fill_movable_zone_fn);
static DEFINE_MUTEX(page_migrate_lock);

unsigned long get_totalram_pages_count_inc_offlined(void)
{
	struct sysinfo i;
	unsigned long totalram_with_offline;

	si_meminfo(&i);
	totalram_with_offline =
		(unsigned long)atomic_long_read(&totalram_pages_with_offline);

	if (i.totalram < totalram_with_offline)
		i.totalram = totalram_with_offline;

	return i.totalram;
}

static void update_totalram_snapshot(void)
{
	unsigned long totalram_with_offline;

	totalram_with_offline = get_totalram_pages_count_inc_offlined();
	atomic_long_set(&totalram_pages_with_offline, totalram_with_offline);
}

static void clear_pgtable_mapping(phys_addr_t start, phys_addr_t end)
{
	unsigned long size = end - start;
	unsigned long virt = (unsigned long)phys_to_virt(start);
	unsigned long addr_end = virt + size;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	pgd = pgd_offset_k(virt);

	while (virt < addr_end) {

		/* Check if we have PUD section mapping */
		pud = pud_offset(pgd, virt);
		if (pud_sect(*pud)) {
			pud_clear(pud);
			virt += PUD_SIZE;
			continue;
		}

		/* Check if we have PMD section mapping */
		pmd = pmd_offset(pud, virt);
		if (pmd_sect(*pmd)) {
			pmd_clear(pmd);
			virt += PMD_SIZE;
			continue;
		}

		/* Clear mapping for page entry */
		set_memory_valid(virt, 1, (int)false);
		virt += PAGE_SIZE;
	}

	virt = (unsigned long)phys_to_virt(start);
	flush_tlb_kernel_range(virt, addr_end);
}

static void init_pgtable_mapping(phys_addr_t start, phys_addr_t end)
{
	unsigned long size = end - start;

	/*
	 * When rodata_full is enabled, memory is mapped at a page size
	 * granule, as opposed to a block mapping, so restore the attribute
	 * of each PTE when rodata_full is enabled.
	 */
	if (rodata_full)
		set_memory_valid((unsigned long)phys_to_virt(start),
				 size >> PAGE_SHIFT, (int)true);
	else
		create_pgtable_mapping(start, end);
}

static void record_stat(unsigned long sec, ktime_t delay, int mode)
{
	unsigned int total_sec = end_section_nr - start_section_nr + 1;
	unsigned int blk_nr = (sec - start_section_nr + mode * total_sec) /
				sections_per_block;
	ktime_t now, delta;

	if (sec > end_section_nr)
		return;

	if (delay < mem_info[blk_nr].best_time || !mem_info[blk_nr].best_time)
		mem_info[blk_nr].best_time = delay;

	if (delay > mem_info[blk_nr].worst_time)
		mem_info[blk_nr].worst_time = delay;

	++mem_info[blk_nr].success_count;
	if (mem_info[blk_nr].fail_count)
		--mem_info[blk_nr].fail_count;

	mem_info[blk_nr].total_time += delay;

	mem_info[blk_nr].avg_time =
		mem_info[blk_nr].total_time / mem_info[blk_nr].success_count;

	mem_info[blk_nr].last_recorded_time = delay;

	now = ktime_get();
	mem_info[blk_nr].resident_since = now;

	/* since other state has gone inactive, update the stats */
	mode = mode ? MEMORY_ONLINE : MEMORY_OFFLINE;
	blk_nr = (sec - start_section_nr + mode * total_sec) /
				sections_per_block;
	delta = ktime_sub(now, mem_info[blk_nr].resident_since);
	mem_info[blk_nr].resident_time =
			ktime_add(mem_info[blk_nr].resident_time, delta);
	mem_info[blk_nr].resident_since = 0;
}

static int mem_region_refresh_control(unsigned long pfn,
				      unsigned long nr_pages,
				      bool enable)
{
	struct memory_refresh_request mem_req;
	struct msm_rpm_kvp rpm_kvp;

	mem_req.start = enable;
	mem_req.start |= pfn << PAGE_SHIFT;
	mem_req.size = nr_pages * PAGE_SIZE;

	rpm_kvp.key = RPM_DDR_REQ;
	rpm_kvp.data = (void *)&mem_req;
	rpm_kvp.length = sizeof(mem_req);

	return msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET, RPM_DDR_REQ, 0,
				    &rpm_kvp, 1);
}

static int aop_send_msg(unsigned long addr, bool online)
{
	struct qmp_pkt pkt;
	char mbox_msg[MAX_LEN];
	unsigned long addr_low, addr_high;

	addr_low = addr & AOP_MSG_ADDR_MASK;
	addr_high = (addr >> AOP_MSG_ADDR_HIGH_SHIFT) & AOP_MSG_ADDR_MASK;

	snprintf(mbox_msg, MAX_LEN,
		 "{class: ddr, event: pasr, addr_hi: 0x%08lx, addr_lo: 0x%08lx, refresh: %s}",
		 addr_high, addr_low, online ? "on" : "off");

	pkt.size = MAX_LEN;
	pkt.data = mbox_msg;
	return (mbox_send_message(mailbox.mbox, &pkt) < 0);
}

/*
 * When offline_granule >= memory block size, this returns the number of
 * sections in a offlineable segment.
 * When offline_granule < memory block size, returns the sections_per_block.
 */
static unsigned long get_rounded_sections_per_segment(void)
{

	return max(((offline_granule * SZ_1M) / memory_block_size_bytes()) *
		     sections_per_block,
		     (unsigned long)sections_per_block);
}

static int send_msg(struct memory_notify *mn, bool online, int count)
{
	unsigned long segment_size = offline_granule * SZ_1M;
	unsigned long start, base_sec_nr, sec_nr, sections_per_segment;
	int ret, idx, i;

	if (bypass_send_msg)
		return 0;

	sections_per_segment = get_rounded_sections_per_segment();
	sec_nr = pfn_to_section_nr(SECTION_ALIGN_DOWN(mn->start_pfn));
	idx = (sec_nr - start_section_nr) / sections_per_segment;
	base_sec_nr = start_section_nr + (idx * sections_per_segment);
	start = section_nr_to_pfn(base_sec_nr);

	for (i = 0; i < count; ++i) {
		if (is_rpm_controller)
			ret = mem_region_refresh_control(start,
						 segment_size >> PAGE_SHIFT,
						 online);
		else
			ret = aop_send_msg(__pfn_to_phys(start), online);

		if (ret) {
			pr_err("PASR: %s %s request addr:0x%llx failed\n",
			       is_rpm_controller ? "RPM" : "AOP",
			       online ? "online" : "offline",
			       __pfn_to_phys(start));
			goto undo;
		}

		start = __phys_to_pfn(__pfn_to_phys(start) + segment_size);
	}

	return 0;
undo:
	start = section_nr_to_pfn(base_sec_nr);
	while (i-- > 0) {
		int ret;

		if (is_rpm_controller)
			ret = mem_region_refresh_control(start,
						 segment_size >> PAGE_SHIFT,
						 !online);
		else
			ret = aop_send_msg(__pfn_to_phys(start), !online);

		if (ret)
			panic("Failed to completely online/offline a hotpluggable segment. A quasi state of memblock can cause randomn system failures.");
		start = __phys_to_pfn(__pfn_to_phys(start) + segment_size);
	}

	return ret;
}

static bool need_to_send_remote_request(struct memory_notify *mn,
				    enum memory_states request)
{
	int i, idx, cur_idx;
	int base_sec_nr, sec_nr;
	unsigned long sections_per_segment;

	sections_per_segment = get_rounded_sections_per_segment();
	sec_nr = pfn_to_section_nr(SECTION_ALIGN_DOWN(mn->start_pfn));
	idx = (sec_nr - start_section_nr) / sections_per_segment;
	cur_idx = (sec_nr - start_section_nr) / sections_per_block;
	base_sec_nr = start_section_nr + (idx * sections_per_segment);

	/*
	 * For MEM_OFFLINE, don't send the request if there are other online
	 * blocks in the segment.
	 * For MEM_ONLINE, don't send the request if there is already one
	 * online block in the segment.
	 */
	if (request == MEMORY_OFFLINE || request == MEMORY_ONLINE) {
		for (i = base_sec_nr;
		     i < (base_sec_nr + sections_per_segment);
		     i += sections_per_block) {
			idx = (i - start_section_nr) / sections_per_block;
			/* current operating block */
			if (idx == cur_idx)
				continue;
			if (mem_sec_state[idx] == MEMORY_ONLINE)
				goto out;
		}
		return true;
	}
out:
	return false;
}

/*
 * This returns the number of hotpluggable segments in a memory block.
 */
static int get_num_memblock_hotplug_segments(void)
{
	unsigned long segment_size = offline_granule * SZ_1M;
	unsigned long block_size = memory_block_size_bytes();

	if (segment_size < block_size) {
		if (block_size % segment_size) {
			pr_warn("PASR is unusable. Offline granule size should be in multiples for memory_block_size_bytes.\n");
			return 0;
		}
		return block_size / segment_size;
	}

	return 1;
}

static int mem_change_refresh_state(struct memory_notify *mn,
				    enum memory_states state)
{
	int start = SECTION_ALIGN_DOWN(mn->start_pfn);
	unsigned long sec_nr = pfn_to_section_nr(start);
	bool online = (state == MEMORY_ONLINE) ? true : false;
	unsigned long idx = (sec_nr - start_section_nr) / sections_per_block;
	int ret, count;

	if (mem_sec_state[idx] == state) {
		/* we shouldn't be getting this request */
		pr_warn("mem-offline: state of mem%d block already in %s state. Ignoring refresh state change request\n",
				sec_nr, online ? "online" : "offline");
		return 0;
	}

	count = get_num_memblock_hotplug_segments();
	if (!count)
		return -EINVAL;

	if (!need_to_send_remote_request(mn, state))
		goto out;

	ret = send_msg(mn, online, count);
	if (ret) {
		/* online failures are critical failures */
		if (online)
			BUG_ON(IS_ENABLED(CONFIG_BUG_ON_HW_MEM_ONLINE_FAIL));
		return -EINVAL;
	}
out:
	mem_sec_state[idx] = state;
	return 0;
}

static inline void reset_page_order(struct page *page)
{
	__ClearPageBuddy(page);
	set_page_private(page, 0);
}

static int isolate_free_page(struct page *page, unsigned int order)
{
	struct zone *zone;

	zone = page_zone(page);
	list_del(&page->lru);
	zone->free_area[order].nr_free--;
	reset_page_order(page);

	return 1UL << order;
}

static void isolate_free_pages(struct movable_zone_fill_control *fc)
{
	struct page *page;
	unsigned long flags;
	unsigned int order;
	unsigned long start_pfn = fc->start_pfn;
	unsigned long end_pfn = fc->end_pfn;

	spin_lock_irqsave(&fc->zone->lock, flags);
	for (; start_pfn < end_pfn; start_pfn++) {
		unsigned long isolated;

		if (!pfn_valid(start_pfn))
			continue;

		page = pfn_to_page(start_pfn);
		if (!page)
			continue;

		if (PageCompound(page)) {
			struct page *head = compound_head(page);
			int skip;

			skip = (1 << compound_order(head)) - (page - head);
			start_pfn += skip - 1;
			continue;
		}

		if (!(start_pfn % pageblock_nr_pages) &&
			is_migrate_isolate_page(page)) {
			start_pfn += pageblock_nr_pages - 1;
			continue;
		}

		if (!PageBuddy(page))
			continue;

		order = page_private(page);
		isolated = isolate_free_page(page, order);
		set_page_private(page, order);
		list_add_tail(&page->lru, &fc->freepages);
		fc->nr_free_pages += isolated;
		__mod_zone_page_state(fc->zone, NR_FREE_PAGES, -isolated);
		start_pfn += isolated - 1;

		/*
		 * Make sure that the zone->lock is not held for long by
		 * returning once we have SWAP_CLUSTER_MAX pages in the
		 * free list for migration.
		 */
		if (!((start_pfn + 1) % pageblock_nr_pages) &&
			(fc->nr_free_pages >= SWAP_CLUSTER_MAX ||
			has_pend_offline_req))
			break;
	}
	fc->start_pfn = start_pfn + 1;
	spin_unlock_irqrestore(&fc->zone->lock, flags);

	split_map_pages(&fc->freepages);
}

static struct page *movable_page_alloc(struct page *page, unsigned long data)
{
	struct movable_zone_fill_control *fc;
	struct page *freepage;

	fc = (struct movable_zone_fill_control *)data;
	if (list_empty(&fc->freepages)) {
		isolate_free_pages(fc);
		if (list_empty(&fc->freepages))
			return NULL;
	}

	freepage = list_entry(fc->freepages.next, struct page, lru);
	list_del(&freepage->lru);
	fc->nr_free_pages--;

	return freepage;
}

static void movable_page_free(struct page *page, unsigned long data)
{
	struct movable_zone_fill_control *fc;

	fc = (struct movable_zone_fill_control *)data;
	list_add(&page->lru, &fc->freepages);
	fc->nr_free_pages++;
}

static unsigned long get_anon_movable_pages(
			struct movable_zone_fill_control *fc,
			unsigned long start_pfn,
			unsigned long end_pfn, struct list_head *list)
{
	int found = 0, pfn, ret;
	int limit = min_t(int, fc->target, (int)pageblock_nr_pages);

	fc->nr_migrate_pages = 0;
	for (pfn = start_pfn; pfn < end_pfn && found < limit; ++pfn) {
		struct page *page = pfn_to_page(pfn);

		if (!pfn_valid(pfn))
			continue;

		if (PageCompound(page)) {
			struct page *head = compound_head(page);
			int skip;

			skip = (1 << compound_order(head)) - (page - head);
			pfn += skip - 1;
			continue;
		}

		if (PageBuddy(page)) {
			unsigned long freepage_order;

			freepage_order = READ_ONCE(page_private(page));
			if (freepage_order > 0 && freepage_order < MAX_ORDER)
				pfn += (1 << page_private(page)) - 1;
			continue;
		}

		if (!(pfn % pageblock_nr_pages) &&
			get_pageblock_migratetype(page) == MIGRATE_CMA) {
			pfn += pageblock_nr_pages - 1;
			continue;
		}

		if (!PageLRU(page) || !PageAnon(page))
			continue;

		if (!get_page_unless_zero(page))
			continue;

		found++;
		ret = isolate_lru_page(page);
		if (!ret) {
			list_add_tail(&page->lru, list);
			inc_node_page_state(page, NR_ISOLATED_ANON +
					page_is_file_cache(page));
			++fc->nr_migrate_pages;
		}

		put_page(page);
	}

	return pfn;
}

static void prepare_fc(struct movable_zone_fill_control *fc)
{
	struct zone *zone;

	zone = &(NODE_DATA(0)->node_zones[ZONE_MOVABLE]);
	fc->zone = zone;
	fc->start_pfn = zone->zone_start_pfn;
	fc->end_pfn = zone_end_pfn(zone);
	fc->limit = atomic64_read(&zone->managed_pages);
	INIT_LIST_HEAD(&fc->freepages);
}

static void fill_movable_zone_fn(struct work_struct *work)
{
	unsigned long start_pfn, end_pfn;
	unsigned long movable_highmark;
	struct zone *normal_zone = &(NODE_DATA(0)->node_zones[ZONE_NORMAL]);
	struct zone *movable_zone = &(NODE_DATA(0)->node_zones[ZONE_MOVABLE]);
	LIST_HEAD(source);
	int ret, free;
	struct movable_zone_fill_control fc = { {0} };
	unsigned long timeout = MIGRATE_TIMEOUT_SEC * HZ, expire;

	start_pfn = normal_zone->zone_start_pfn;
	end_pfn = zone_end_pfn(normal_zone);
	movable_highmark = high_wmark_pages(movable_zone);

	if (has_pend_offline_req)
		return;
	lru_add_drain_all();
	drain_all_pages(normal_zone);
	if (!mutex_trylock(&page_migrate_lock))
		return;
	prepare_fc(&fc);
	if (!fc.limit)
		goto out;
	expire = jiffies + timeout;
restart:
	fc.target = atomic_xchg(&target_migrate_pages, 0);
	if (!fc.target)
		goto out;
repeat:
	cond_resched();
	if (time_after(jiffies, expire))
		goto out;
	free = zone_page_state(movable_zone, NR_FREE_PAGES);
	if (free - fc.target <= movable_highmark)
		fc.target = free - movable_highmark;
	if (fc.target <= 0)
		goto out;

	start_pfn = get_anon_movable_pages(&fc, start_pfn, end_pfn, &source);
	if (list_empty(&source) && start_pfn < end_pfn)
		goto repeat;

	ret = migrate_pages(&source, movable_page_alloc, movable_page_free,
			(unsigned long) &fc,
			MIGRATE_ASYNC, MR_MEMORY_HOTPLUG);
	if (ret)
		putback_movable_pages(&source);

	fc.target -= fc.nr_migrate_pages;
	if (ret == -ENOMEM || start_pfn >= end_pfn || has_pend_offline_req)
		goto out;
	else if (fc.target <= 0)
		goto restart;

	goto repeat;
out:
	mutex_unlock(&page_migrate_lock);
	if (fc.nr_free_pages > 0)
		release_freepages(&fc.freepages);
}

static int mem_event_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	struct memory_notify *mn = arg;
	unsigned long start, end, sec_nr;
	static ktime_t cur;
	ktime_t delay = 0;
	phys_addr_t start_addr, end_addr;
	unsigned int idx = end_section_nr - start_section_nr + 1;

	start = SECTION_ALIGN_DOWN(mn->start_pfn);
	end = SECTION_ALIGN_UP(mn->start_pfn + mn->nr_pages);

	if ((start != mn->start_pfn) || (end != mn->start_pfn + mn->nr_pages)) {
		WARN("mem-offline: %s pfn not aligned to section\n", __func__);
		pr_err("mem-offline: start pfn = %lu end pfn = %lu\n",
			mn->start_pfn, mn->start_pfn + mn->nr_pages);
		return -EINVAL;
	}

	start_addr = __pfn_to_phys(start);
	end_addr = __pfn_to_phys(end);
	sec_nr = pfn_to_section_nr(start);

	if (sec_nr > end_section_nr || sec_nr < start_section_nr) {
		if (action == MEM_ONLINE || action == MEM_OFFLINE)
			pr_info("mem-offline: %s mem%ld, but not our block. Not performing any action\n",
				action == MEM_ONLINE ? "Onlined" : "Offlined",
				sec_nr);
		return NOTIFY_OK;
	}
	switch (action) {
	case MEM_GOING_ONLINE:
		pr_debug("mem-offline: MEM_GOING_ONLINE : start = 0x%llx end = 0x%llx\n",
				start_addr, end_addr);
		++mem_info[(sec_nr - start_section_nr + MEMORY_ONLINE *
			   idx) / sections_per_block].fail_count;
		cur = ktime_get();

		if (mem_change_refresh_state(mn, MEMORY_ONLINE))
			return NOTIFY_BAD;

		if (!debug_pagealloc_enabled()) {
			/* Create kernel page-tables */
			init_pgtable_mapping(start_addr, end_addr);
		}

		break;
	case MEM_ONLINE:
		update_totalram_snapshot();
		delay = ktime_ms_delta(ktime_get(), cur);
		record_stat(sec_nr, delay, MEMORY_ONLINE);
		cur = 0;
		pr_info("mem-offline: Onlined memory block mem%pK\n",
			(void *)sec_nr);
		break;
	case MEM_GOING_OFFLINE:
		update_totalram_snapshot();
		pr_debug("mem-offline: MEM_GOING_OFFLINE : start = 0x%llx end = 0x%llx\n",
				start_addr, end_addr);
		++mem_info[(sec_nr - start_section_nr + MEMORY_OFFLINE *
			   idx) / sections_per_block].fail_count;
		has_pend_offline_req = true;
		cancel_work_sync(&fill_movable_zone_work);
		cur = ktime_get();
		break;
	case MEM_OFFLINE:
		if (!debug_pagealloc_enabled()) {
			/* Clear kernel page-tables */
			clear_pgtable_mapping(start_addr, end_addr);
		}
		mem_change_refresh_state(mn, MEMORY_OFFLINE);
		/*
		 * Notifying that something went bad at this stage won't
		 * help since this is the last stage of memory hotplug.
		 */

		delay = ktime_ms_delta(ktime_get(), cur);
		record_stat(sec_nr, delay, MEMORY_OFFLINE);
		cur = 0;
		has_pend_offline_req = false;
		pr_info("mem-offline: Offlined memory block mem%pK\n",
			(void *)sec_nr);
		break;
	case MEM_CANCEL_ONLINE:
		pr_info("mem-offline: MEM_CANCEL_ONLINE: start = 0x%llx end = 0x%llx\n",
				start_addr, end_addr);
		mem_change_refresh_state(mn, MEMORY_OFFLINE);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int update_movable_bitmap(void)
{
	struct device_node *node;
	struct property *prop;
	int len, num_cells, num_entries;
	u64 base, size, end, section_size;
	u64 movable_start, movable_end;
	int nr_address_cells, nr_size_cells;
	const __be32 *pos;

	node = of_find_node_by_name(of_root, "memory");
	if (!node) {
		pr_err("mem-offine: memory node not found in DT\n");
		return -EINVAL;
	}

	nr_address_cells = of_n_addr_cells(of_root);
	nr_size_cells = of_n_size_cells(of_root);

	prop = of_find_property(node, "reg", &len);

	num_cells = len / sizeof(__be32);
	num_entries = num_cells / (nr_address_cells + nr_size_cells);

	pos = prop->value;

	section_size = MIN_MEMORY_BLOCK_SIZE;
	movable_start = memblock_end_of_DRAM();
	movable_end = bootloader_memory_limit - 1;

	while (num_entries--) {
		u64 new_base, new_end;
		u64 new_start_bitmap, bitmap_size;

		base = of_read_number(pos, nr_address_cells);
		size = of_read_number(pos + nr_address_cells, nr_size_cells);
		pos += nr_address_cells + nr_size_cells;
		end = base + size;

		if (end <= movable_start)
			continue;

		if (base < movable_start)
			new_base = movable_start;
		else
			new_base = base;
		new_end = end;

		new_start_bitmap = (new_base - movable_start) / section_size;
		bitmap_size = (new_end - new_base) / section_size;
		bitmap_set(&movable_bitmap, new_start_bitmap, bitmap_size);
	}

	pr_debug("mem-offline: movable_bitmap is %lx\n", movable_bitmap);
	return 0;
}

static int mem_online_remaining_blocks(void)
{
	unsigned long memblock_end_pfn = __phys_to_pfn(memblock_end_of_DRAM());
	unsigned long ram_end_pfn = __phys_to_pfn(bootloader_memory_limit - 1);
	unsigned long block_size, memblock, pfn;
	unsigned int nid;
	phys_addr_t phys_addr;
	int fail = 0;
	int ret;

	block_size = memory_block_size_bytes();
	sections_per_block = block_size / MIN_MEMORY_BLOCK_SIZE;

	start_section_nr = pfn_to_section_nr(memblock_end_pfn);
	end_section_nr = pfn_to_section_nr(ram_end_pfn);

	if (memblock_end_of_DRAM() >= bootloader_memory_limit) {
		pr_info("mem-offline: System booted with no zone movable memory blocks. Cannot perform memory offlining\n");
		return -EINVAL;
	}

	ret = update_movable_bitmap();
	if (ret < 0)
		return -ENODEV;

	for (memblock = start_section_nr; memblock <= end_section_nr;
			memblock += sections_per_block) {

		if (!test_bit(memblock - start_section_nr, &movable_bitmap))
			continue;

		pfn = section_nr_to_pfn(memblock);
		phys_addr = __pfn_to_phys(pfn);

		if (phys_addr & (((PAGES_PER_SECTION * sections_per_block)
					<< PAGE_SHIFT) - 1)) {
			fail = 1;
			pr_warn("mem-offline: PFN of mem%lu block not aligned to section start. Not adding this memory block\n",
								memblock);
			continue;
		}
		nid = memory_add_physaddr_to_nid(phys_addr);
		if (add_memory(nid, phys_addr,
				 MIN_MEMORY_BLOCK_SIZE * sections_per_block)) {
			pr_warn("mem-offline: Adding memory block mem%lu failed\n",
								memblock);
			fail = 1;
		}
	}

	max_pfn = PFN_DOWN(memblock_end_of_DRAM());
	return fail;
}

static ssize_t show_mem_offline_granule(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n",
			(unsigned long)offline_granule * SZ_1M);
}


static unsigned int print_blk_residency_percentage(char *buf, size_t sz,
			unsigned int tot_blks, ktime_t *total_time,
			enum memory_states mode)
{
	unsigned int i;
	unsigned int c = 0;
	int percent;
	unsigned int idx = tot_blks + 1;

	for (i = 0; i <= tot_blks; i++) {
		percent = (int)ktime_divns(total_time[i + mode * idx] * 100,
			ktime_add(total_time[i + MEMORY_ONLINE * idx],
					total_time[i + MEMORY_OFFLINE * idx]));

		c += scnprintf(buf + c, sz - c, "%d%%\t\t", percent);
	}
	return c;
}
static unsigned int print_blk_residency_times(char *buf, size_t sz,
			unsigned int tot_blks, ktime_t *total_time,
			enum memory_states mode)
{
	unsigned int i;
	unsigned int c = 0;
	ktime_t now, delta;
	unsigned int idx = tot_blks + 1;

	now = ktime_get();
	for (i = 0; i <= tot_blks; i++) {
		if (mem_sec_state[i] == mode)
			delta = ktime_sub(now,
				mem_info[i + mode * idx].resident_since);
		else
			delta = 0;
		delta = ktime_add(delta,
			mem_info[i + mode * idx].resident_time);
		c += scnprintf(buf + c, sz - c, "%lus\t\t",
				ktime_to_ms(delta) / MSEC_PER_SEC);
		total_time[i + mode * idx] = delta;
	}
	return c;
}

static ssize_t show_mem_stats(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{

	unsigned int blk_start = start_section_nr / sections_per_block;
	unsigned int blk_end = end_section_nr / sections_per_block;
	unsigned int tot_blks = blk_end - blk_start;
	ktime_t *total_time;
	unsigned int idx = tot_blks + 1;
	unsigned int c = 0;
	unsigned int i, j;

	size_t sz = PAGE_SIZE;
	ktime_t total = 0, total_online = 0, total_offline = 0;

	total_time = kcalloc(idx * MAX_STATE, sizeof(*total_time), GFP_KERNEL);

	if (!total_time)
		return -ENOMEM;

	for (j = 0; j < MAX_STATE; j++) {
		c += scnprintf(buf + c, sz - c,
			"\n\t%s\n\t\t\t", j == 0 ? "ONLINE" : "OFFLINE");
		for (i = blk_start; i <= blk_end; i++)
			c += scnprintf(buf + c, sz - c,
							"%s%d\t\t", "mem", i);
		c += scnprintf(buf + c, sz - c, "\n");
		c += scnprintf(buf + c, sz - c,
							"\tLast recd time:\t");
		for (i = 0; i <= tot_blks; i++)
			c += scnprintf(buf + c, sz - c, "%lums\t\t",
				mem_info[i + j * idx].last_recorded_time);
		c += scnprintf(buf + c, sz - c, "\n");
		c += scnprintf(buf + c, sz - c,
							"\tAvg time:\t");
		for (i = 0; i <= tot_blks; i++)
			c += scnprintf(buf + c, sz - c,
				"%lums\t\t", mem_info[i + j * idx].avg_time);
		c += scnprintf(buf + c, sz - c, "\n");
		c += scnprintf(buf + c, sz - c,
							"\tBest time:\t");
		for (i = 0; i <= tot_blks; i++)
			c += scnprintf(buf + c, sz - c,
				"%lums\t\t", mem_info[i + j * idx].best_time);
		c += scnprintf(buf + c, sz - c, "\n");
		c += scnprintf(buf + c, sz - c,
							"\tWorst time:\t");
		for (i = 0; i <= tot_blks; i++)
			c += scnprintf(buf + c, sz - c,
				"%lums\t\t", mem_info[i + j * idx].worst_time);
		c += scnprintf(buf + c, sz - c, "\n");
		c += scnprintf(buf + c, sz - c,
							"\tSuccess count:\t");
		for (i = 0; i <= tot_blks; i++)
			c += scnprintf(buf + c, sz - c,
				"%lu\t\t", mem_info[i + j * idx].success_count);
		c += scnprintf(buf + c, sz - c, "\n");
		c += scnprintf(buf + c, sz - c,
							"\tFail count:\t");
		for (i = 0; i <= tot_blks; i++)
			c += scnprintf(buf + c, sz - c,
				"%lu\t\t", mem_info[i + j * idx].fail_count);
		c += scnprintf(buf + c, sz - c, "\n");
	}

	c += scnprintf(buf + c, sz - c, "\n");
	c += scnprintf(buf + c, sz - c, "\tState:\t\t");
	for (i = 0; i <= tot_blks; i++) {
		c += scnprintf(buf + c, sz - c, "%s\t\t",
			mem_sec_state[i] == MEMORY_ONLINE ?
			"Online" : "Offline");
	}
	c += scnprintf(buf + c, sz - c, "\n");

	c += scnprintf(buf + c, sz - c, "\n");
	c += scnprintf(buf + c, sz - c, "\tOnline time:\t");
	c += print_blk_residency_times(buf + c, sz - c,
			tot_blks, total_time, MEMORY_ONLINE);


	c += scnprintf(buf + c, sz - c, "\n");
	c += scnprintf(buf + c, sz - c, "\tOffline time:\t");
	c += print_blk_residency_times(buf + c, sz - c,
			tot_blks, total_time, MEMORY_OFFLINE);

	c += scnprintf(buf + c, sz, "\n");

	c += scnprintf(buf + c, sz, "\n");
	c += scnprintf(buf + c, sz, "\tOnline %%:\t");
	c += print_blk_residency_percentage(buf + c, sz - c,
			tot_blks, total_time, MEMORY_ONLINE);

	c += scnprintf(buf + c, sz, "\n");
	c += scnprintf(buf + c, sz, "\tOffline %%:\t");
	c += print_blk_residency_percentage(buf + c, sz - c,
			tot_blks, total_time, MEMORY_OFFLINE);
	c += scnprintf(buf + c, sz, "\n");
	c += scnprintf(buf + c, sz, "\n");

	for (i = 0; i <= tot_blks; i++)
		total = ktime_add(total,
			ktime_add(total_time[i + MEMORY_ONLINE * idx],
					total_time[i + MEMORY_OFFLINE * idx]));

	for (i = 0; i <= tot_blks; i++)
		total_online =  ktime_add(total_online,
				total_time[i + MEMORY_ONLINE * idx]);

	total_offline = ktime_sub(total, total_online);

	c += scnprintf(buf + c, sz,
					"\tAvg Online %%:\t%d%%\n",
					((int)total_online * 100) / total);
	c += scnprintf(buf + c, sz,
					"\tAvg Offline %%:\t%d%%\n",
					((int)total_offline * 100) / total);

	c += scnprintf(buf + c, sz, "\n");
	kfree(total_time);
	return c;
}

static struct kobj_attribute stats_attr =
		__ATTR(stats, 0444, show_mem_stats, NULL);

static ssize_t show_anon_migrate(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n",
				atomic_read(&target_migrate_pages));
}

static ssize_t store_anon_migrate(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t size)
{
	int val = 0, ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	atomic_add(val, &target_migrate_pages);

	if (!work_pending(&fill_movable_zone_work))
		queue_work(migrate_wq, &fill_movable_zone_work);

	return size;
}

static struct kobj_attribute offline_granule_attr =
		__ATTR(offline_granule, 0444, show_mem_offline_granule, NULL);

static struct kobj_attribute anon_migration_size_attr =
		__ATTR(anon_migrate, 0644, show_anon_migrate, store_anon_migrate);

static struct attribute *mem_root_attrs[] = {
		&stats_attr.attr,
		&offline_granule_attr.attr,
		&anon_migration_size_attr.attr,
		NULL,
};

static struct attribute_group mem_attr_group = {
	.attrs = mem_root_attrs,
};

static int mem_sysfs_init(void)
{
	if (start_section_nr == end_section_nr)
		return -EINVAL;

	kobj = kobject_create_and_add(MODULE_CLASS_NAME, kernel_kobj);
	if (!kobj)
		return -ENOMEM;

	if (sysfs_create_group(kobj, &mem_attr_group))
		kobject_put(kobj);

	return 0;
}

static int mem_parse_dt(struct platform_device *pdev)
{
	const __be32 *val;
	struct device_node *node = pdev->dev.of_node;

	val = of_get_property(node, "granule", NULL);
	if (!val) {
		pr_err("mem-offine: granule property not found in DT\n");
		return -EINVAL;
	}
	if (!*val) {
		pr_err("mem-offine: invalid granule property\n");
		return -EINVAL;
	}
	offline_granule = be32_to_cpup(val);
	if (!offline_granule || (offline_granule & (offline_granule - 1)) ||
	    ((offline_granule * SZ_1M < MIN_MEMORY_BLOCK_SIZE) &&
	     (MIN_MEMORY_BLOCK_SIZE % (offline_granule * SZ_1M)))) {
		pr_err("mem-offine: invalid granule property\n");
		return -EINVAL;
	}

	if (!of_find_property(node, "mboxes", NULL)) {
		is_rpm_controller = true;
		return 0;
	}

	mailbox.cl.dev = &pdev->dev;
	mailbox.cl.tx_block = true;
	mailbox.cl.tx_tout = 1000;
	mailbox.cl.knows_txdone = false;

	mailbox.mbox = mbox_request_channel(&mailbox.cl, 0);
	if (IS_ERR(mailbox.mbox)) {
		if (PTR_ERR(mailbox.mbox) != -EPROBE_DEFER)
			pr_err("mem-offline: failed to get mailbox channel %pK %ld\n",
				mailbox.mbox, PTR_ERR(mailbox.mbox));
		return PTR_ERR(mailbox.mbox);
	}

	return 0;
}

static struct notifier_block hotplug_memory_callback_nb = {
	.notifier_call = mem_event_callback,
	.priority = 0,
};

static int mem_offline_driver_probe(struct platform_device *pdev)
{
	unsigned int total_blks;
	int ret, i;
	ktime_t now;

	ret = mem_parse_dt(pdev);
	if (ret)
		return ret;

	ret = mem_online_remaining_blocks();
	if (ret < 0)
		return -ENODEV;

	if (ret > 0)
		pr_err("mem-offline: !!ERROR!! Auto onlining some memory blocks failed. System could run with less RAM\n");

	total_blks = (end_section_nr - start_section_nr + 1) /
			sections_per_block;
	mem_info = kcalloc(total_blks * MAX_STATE, sizeof(*mem_info),
			   GFP_KERNEL);
	if (!mem_info)
		return -ENOMEM;

	/* record time of online for all blocks */
	now = ktime_get();
	for (i = 0; i < total_blks; i++)
		mem_info[i].resident_since = now;

	mem_sec_state = kcalloc(total_blks, sizeof(*mem_sec_state), GFP_KERNEL);
	if (!mem_sec_state) {
		ret = -ENOMEM;
		goto err_free_mem_info;
	}

	/* we assume that hardware state of mem blocks are online after boot */
	for (i = 0; i < total_blks; i++)
		mem_sec_state[i] = MEMORY_ONLINE;

	if (mem_sysfs_init()) {
		ret = -ENODEV;
		goto err_free_mem_sec_state;
	}

	if (register_hotmemory_notifier(&hotplug_memory_callback_nb)) {
		pr_err("mem-offline: Registering memory hotplug notifier failed\n");
		ret = -ENODEV;
		goto err_sysfs_remove_group;
	}
	pr_info("mem-offline: Added memory blocks ranging from mem%lu - mem%lu\n",
			start_section_nr, end_section_nr);

	if (bypass_send_msg)
		pr_info("mem-offline: bypass mode\n");

	migrate_wq = alloc_workqueue("reverse_migrate_wq",
					WQ_UNBOUND | WQ_FREEZABLE, 0);
	if (!migrate_wq) {
		pr_err("Failed to create the worker for reverse migration\n");
		ret = -ENOMEM;
		goto err_sysfs_remove_group;
	}

	return 0;

err_sysfs_remove_group:
	sysfs_remove_group(kobj, &mem_attr_group);
	kobject_put(kobj);
err_free_mem_sec_state:
	kfree(mem_sec_state);
err_free_mem_info:
	kfree(mem_info);
	return ret;
}

static const struct of_device_id mem_offline_match_table[] = {
	{.compatible = "qcom,mem-offline"},
	{}
};

static struct platform_driver mem_offline_driver = {
	.probe = mem_offline_driver_probe,
	.driver = {
		.name = "mem_offline",
		.of_match_table = mem_offline_match_table,
	},
};

static int __init mem_module_init(void)
{
	return platform_driver_register(&mem_offline_driver);
}

subsys_initcall(mem_module_init);
