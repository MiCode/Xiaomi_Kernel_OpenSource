/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define pr_fmt(fmt) "memory-ssmr: " fmt

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/highmem.h>

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_ARM64
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#endif
#include "ssmr_internal.h"

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#define COUNT_DOWN_MS 10000
#define	COUNT_DOWN_INTERVAL 500
#define	COUNT_DOWN_LIMIT (COUNT_DOWN_MS / COUNT_DOWN_INTERVAL)
static atomic_t svp_online_count_down;
static atomic_t svp_ref_count;
static struct task_struct *_svp_online_task; /* NULL */
static DEFINE_MUTEX(svp_online_task_lock);
#endif

/* 16 MB alignment */
#define SSMR_CMA_ALIGN_PAGE_ORDER 12
#define SSMR_ALIGN_SHIFT (SSMR_CMA_ALIGN_PAGE_ORDER + PAGE_SHIFT)
#define SSMR_ALIGN (1 << (PAGE_SHIFT + (MAX_ORDER - 1)))

static u64 ssmr_upper_limit = UPPER_LIMIT64;
static int reserved_for_unmap_ready;

/* Flag indicates if all SSMR features use reserved memory as source */
static int is_pre_reserve_memory;

#include <mt-plat/aee.h>
#include "mt-plat/mtk_meminfo.h"

static unsigned long ssmr_usage_count;
static struct cma *cma;

struct page_change_data {
	pgprot_t set_mask;
	pgprot_t clear_mask;
};

#define memory_unmapping(virt, size) set_memory_mapping(virt, size, 0)
#define memory_mapping(virt, size) set_memory_mapping(virt, size, 1)

/*
 * Use for zone-movable-cma callback
 */

#define SSMR_FEATURES_DT_UNAME "memory-ssmr-features"

static void __init get_feature_size_by_dt_prop(unsigned long node,
		const char *feature_name, u64 *fsize)
{
	const __be32 *reg;
	int l;

	reg = of_get_flat_dt_prop(node, feature_name, &l);
	if (reg) {
		/*
		 * Only override the size if we can found its associated prop
		 * in DTS node, otherwise, keep the previous settings.
		 */
		*fsize = dt_mem_next_cell(dt_root_addr_cells, &reg);
	}
}

/*
 * If target DT node is found, try to parse each feature required size.
 */
static int __init dt_scan_for_ssmr_features(unsigned long node,
		const char *uname, int depth, void *data)
{
	int i = 0;

	if (__MAX_NR_SSMR_FEATURES <= 0)
		return 0;

	if (!strncmp(uname, SSMR_FEATURES_DT_UNAME,
		strlen(SSMR_FEATURES_DT_UNAME))) {
		for (; i < __MAX_NR_SSMR_FEATURES; i++) {
			get_feature_size_by_dt_prop(node,
				_ssmr_feats[i].dt_prop_name,
				&_ssmr_feats[i].req_size);
		}

		return 1;
	}

	return 0;
}

static void __init setup_feature_size(struct reserved_mem *rmem)
{
	int i = 0;

	if (of_scan_flat_dt(dt_scan_for_ssmr_features, NULL) == 0)
		pr_info("%s, can't find DT node, %s\n",
			__func__, SSMR_FEATURES_DT_UNAME);

	pr_info("%s, rmem->size: %pa\n", __func__, &rmem->size);
	for (; i < __MAX_NR_SSMR_FEATURES; i++) {
		pr_info("%s, %s: %pa\n", __func__,
			_ssmr_feats[i].dt_prop_name,
			&_ssmr_feats[i].req_size);
	}
}

static void __init finalize_scenario_size(void)
{
	int i = 0;

	for (; i < __MAX_NR_SCHEME; i++) {
		u64 total_size = 0;
		int j = 0;
		if (__MAX_NR_SSMR_FEATURES <= 0)
			break;
		for (; j < __MAX_NR_SSMR_FEATURES; j++) {
			if (_ssmr_feats[j].scheme_flag & _ssmrscheme[i].flags)
				total_size += _ssmr_feats[j].req_size;
		}
		_ssmrscheme[i].usable_size = total_size;
		pr_info("%s, %s: %pa\n", __func__, _ssmrscheme[i].name,
			&_ssmrscheme[i].usable_size);
	}
}

static u64 __init get_final_target_size(void)
{
	int i = 0;
	u64 final_size = 0;

	for (; i < __MAX_NR_SCHEME; i++)
		final_size = max(_ssmrscheme[i].usable_size, final_size);

	return final_size;
}

/*
 * The "memory_ssmr_registration" is __initdata, and will be reclaimed
 * after init. We need to perserve this structure for zmc_cam_alloc() API.
 * (will access "prio" field)
 */
static struct single_cma_registration saved_memory_ssmr_registration = {
	.align = SSMR_ALIGN,
	.name = "memory-ssmr",
	.prio = ZMC_SSMR,
};

/*
 * Parse all feature size requirements from DTS and
 * check if reserved memory can meet the requirement.
 */
static int __init ssmr_preinit(struct reserved_mem *rmem)
{
	int rc = 1;
	u64 final_target_size;

	setup_feature_size(rmem);
	finalize_scenario_size();
	final_target_size = get_final_target_size();
	pr_info("%s, final target size: %pa\n", __func__, &final_target_size);

	memory_ssmr_registration.size = final_target_size;
	saved_memory_ssmr_registration.size = final_target_size;

	if (final_target_size == 0) {
		/*
		 * size is 0 and along with default alignment, zmc_memory_init
		 * will bypass this registration and therefore ZONE_MOVABLE
		 * will NOT be present.
		 */
		pr_alert("%s, SSMR init: skipped, no requirement\n", __func__);
	} else if (final_target_size > 0 && final_target_size <= rmem->size) {
		memory_ssmr_registration.align = (1 << SSMR_ALIGN_SHIFT);
		saved_memory_ssmr_registration.align = (1 << SSMR_ALIGN_SHIFT);
		pr_info("%s, SSMR init: continue\n", __func__);
		rc = 0;
	} else {
		pr_err("%s, SSMR init: skip, rmem->size can't meet target\n",
			__func__);
	}

	return rc;
}

static void __init zmc_ssmr_init(struct cma *zmc_cma)
{
	phys_addr_t base = cma_get_base(zmc_cma), size = cma_get_size(zmc_cma);

	cma = zmc_cma;
	pr_info("%s, base: %pa, size: %pa\n", __func__, &base, &size);
}

/* size will be always reset in ssmr_preinit, only provide default value here */
struct single_cma_registration __initdata memory_ssmr_registration = {
	.align = SSMR_ALIGN,
	.size = SSMR_ALIGN,
	.name = "memory-ssmr",
	.preinit = ssmr_preinit,
	.init = zmc_ssmr_init,
	.prio = ZMC_SSMR,
};

/*
 * This is only used for all SSMR users want dedicated reserved memory
 */
static int __init memory_ssmr_init(struct reserved_mem *rmem)
{
	int ret;

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	if (ssmr_preinit(rmem))
		return 1;

	/* init cma area */
	ret = cma_init_reserved_mem(rmem->base, rmem->size, 0, rmem->name,
									&cma);

	if (ret) {
		pr_err("%s cma failed, ret: %d\n", __func__, ret);
		return 1;
	}

	is_pre_reserve_memory = 1;

	return 0;
}
RESERVEDMEM_OF_DECLARE(memory_ssmr, "mediatek,memory-ssmr",
			memory_ssmr_init);

/*
 * This is for unmap in 1G block descriptor
 * reserve a 4KB around movable zone to make sure it's not 1G block descriptor
 */
static int __init ssmr_reserve_init(struct reserved_mem *rmem)
{
	pr_info("%s, name: %s, base: %pa, size: %pa\n",
			__func__, rmem->name,
			&rmem->base, &rmem->size);

	if (__MAX_NR_SSMR_FEATURES <= 0) {
		memblock_free(rmem->base, rmem->size);
		memblock_add(rmem->base, rmem->size);
		return 0;
	}

	reserved_for_unmap_ready = 1;
	return 0;
}
RESERVEDMEM_OF_DECLARE(ssmr_reserve, "mediatek,reserve-memory-ssmr",
			ssmr_reserve_init);

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) ||\
	defined(CONFIG_MTK_TEE_GP_SUPPORT)
static int __init dedicate_svp_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_SVP];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	if (ssmr_preinit(rmem))
		return 1;

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(svp_memory, "mediatek,memory-svp",
			dedicate_svp_memory);
#endif

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI) ||\
	defined(CONFIG_BLOWFISH_TUI_SUPPORT)
static int __init dedicate_tui_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_TUI];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	if (ssmr_preinit(rmem))
		return 1;

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(tui_memory, "mediatek,memory-tui",
			dedicate_tui_memory);
#endif

#ifdef CONFIG_MTK_PROT_MEM_SUPPORT
static int __init dedicate_prot_sharedmem_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_PROT_SHAREDMEM];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	if (ssmr_preinit(rmem))
		return 1;

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(prot_sharedmem_memory, "mediatek,memory-prot-sharedmem",
			dedicate_prot_sharedmem_memory);
#endif

#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
static int __init dedicate_ta_elf_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_TA_ELF];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(ta_elf_memory, "mediatek,ta_elf",
			dedicate_ta_elf_memory);

static int __init dedicate_ta_stack_heap_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_TA_STACK_HEAP];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(ta_stack_heap_memory, "mediatek,ta_stack_heap",
			dedicate_ta_stack_heap_memory);
#endif

#ifdef CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT
static int __init dedicate_sdsp_sharedmem_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_SDSP_TEE_SHAREDMEM];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(sdsp_sharedmem_memory, "mediatek,sdsp_sharedmem",
			dedicate_sdsp_sharedmem_memory);
#endif

#ifdef CONFIG_MTK_SDSP_MEM_SUPPORT
static int __init dedicate_sdsp_firmware_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_SDSP_FIRMWARE];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(sdsp_firmware_memory, "mediatek,sdsp_firmware",
			dedicate_sdsp_firmware_memory);
#endif

static bool has_dedicate_resvmem_region(void)
{
	bool ret = false;
	int i = 0;

	if (__MAX_NR_SSMR_FEATURES <= 0)
		return ret;

	for (; i < __MAX_NR_SSMR_FEATURES; i++) {
		if (_ssmr_feats[i].use_cache_memory) {
			pr_info("%s uses dedicate reserved memory\n",
				_ssmr_feats[i].feat_name);
			ret = true;
		}
	}

	return ret;
}

/*
 * Check whether memory_ssmr is initialized
 */
bool memory_ssmr_inited(void)
{
	return (has_dedicate_resvmem_region() || cma != NULL);
}

#ifdef CONFIG_ARM64
#ifdef CONFIG_DEBUG_PAGEALLOC
static int change_page_range(pte_t *ptep, pgtable_t token, unsigned long addr,
			void *data)
{
	struct page_change_data *cdata = data;
	pte_t pte = *ptep;

	pte = clear_pte_bit(pte, cdata->clear_mask);
	pte = set_pte_bit(pte, cdata->set_mask);

	set_pte(ptep, pte);
	return 0;
}
/*
 * Unmapping memory region kernel mapping
 * SSMR protect memory region with EMI MPU. While protecting, memory prefetch
 * will access memory region and trigger warning.
 * To avoid false alarm of protection, We unmap kernel mapping while protecting
 *
 * @start: start address
 * @size: memory region size
 * @map: 1 for mapping, 0 for unmapping.
 *
 * @return: success return 0, failed return -1;
 */
static int set_memory_mapping(unsigned long start, phys_addr_t size, int map)
{
	struct page_change_data data;
	int ret;

	/* flush dcache when unmap */
	if (!map)
		__flush_dcache_area((void *)start, size);

	if (map) {
		data.set_mask = __pgprot(PTE_VALID);
		data.clear_mask = __pgprot(0);
	} else {
		data.set_mask = __pgprot(0);
		data.clear_mask = __pgprot(PTE_VALID);
	}

	ret = apply_to_page_range(&init_mm, start, size, change_page_range,
					&data);
	flush_tlb_kernel_range(start, start + size);

	return ret;

}
#else
static int set_memory_mapping(unsigned long start, phys_addr_t size, int map)
{
	unsigned long address = start;
	pud_t *pud;
	pmd_t *pmd;
	pgd_t *pgd;
	spinlock_t *plt;

	if ((start != (start & PMD_MASK))
			|| (size != (size & PMD_MASK))
			|| !memblock_is_memory(virt_to_phys((void *)start))
			|| !size || !start) {
		pr_info("[invalid parameter]: start=0x%lx, size=%pa\n",
				start, &size);
		return -1;
	}

	pr_debug("start=0x%lx, size=%pa, address=0x%p, map=%d\n",
			start, &size, (void *)address, map);

	/* flush dcache when unmap */
	if (!map)
		__flush_dcache_area((void *)start, size);

	while (address < (start + size)) {


		pgd = pgd_offset_k(address);

		if (pgd_none(*pgd) || pgd_bad(*pgd)) {
			pr_info("bad pgd break\n");
			goto fail;
		}

		pud = pud_offset(pgd, address);

		if (pud_none(*pud) || pud_bad(*pud)) {
			pr_info("bad pud break\n");
			goto fail;
		}

		pmd = pmd_offset(pud, address);

		if (pmd_none(*pmd)) {
			pr_info("none ");
			goto fail;
		}

		if (pmd_table(*pmd)) {
			pr_info("pmd_table not set PMD\n");
			goto fail;
		}

		plt = pmd_lock(&init_mm, pmd);
		if (map)
			set_pmd(pmd, __pmd(pmd_val(*pmd) | PMD_SECT_VALID));
		else
			set_pmd(pmd, __pmd(pmd_val(*pmd) & ~PMD_SECT_VALID));

		spin_unlock(plt);
		address += PMD_SIZE;
	}

	flush_tlb_all();
	return 0;
fail:
	pr_info("start=0x%lx, size=%pa, address=0x%p, map=%d\n",
			start, &size, (void *)address, map);
	show_pte(NULL, address);
	return -1;
}
#endif
#else
static inline int set_memory_mapping(unsigned long start, phys_addr_t size,
					int map)
{
	pr_debug("start=0x%lx, size=%pa, map=%d\n", start, &size, map);
	if (!map) {
		pr_info("Flush kmap page table\n");
		kmap_flush_unused();
	}
	return 0;
}
#endif

static int memory_region_offline(struct SSMR_Feature *feature,
		phys_addr_t *pa, unsigned long *size, u64 upper_limit)
{
	int offline_retry = 0;
	int ret_map;
	struct page *page;
	unsigned long alloc_pages;
	phys_addr_t page_phys;

	/* Determine alloc pages by feature */
	if (feature->cache_page)
		alloc_pages = feature->count;
	else
		alloc_pages = feature->req_size /
				PAGE_SIZE;

	feature->alloc_pages = alloc_pages;

	/* compare with function and system wise upper limit */
	upper_limit = min(upper_limit, ssmr_upper_limit);

	if (feature->cache_page)
		page_phys = page_to_phys(feature->cache_page);
	else
		page_phys = 0;

	pr_info("%s[%d]: upper_limit: %llx, region{ alloc_pages : %lu",
			__func__, __LINE__, upper_limit, alloc_pages);
	pr_info("count: %lu, is_unmapping: %c, use_cache_memory: %c,",
		feature->count, feature->is_unmapping ? 'Y' : 'N',
		feature->use_cache_memory ? 'Y' : 'N');
	pr_info("cache_page: %pa}\n", &page_phys);

	if (feature->use_cache_memory) {
		if (!feature->cache_page) {
			pr_info("[NO_CACHE_MEMORY]:\n");
			feature->alloc_pages = 0;
			return -EFAULT;
		}

		page = feature->cache_page;
		goto out;
	}

	do {
		pr_info("[SSMR-ALLOCATION]: retry: %d\n", offline_retry);
		page = zmc_cma_alloc(cma, alloc_pages,
					SSMR_CMA_ALIGN_PAGE_ORDER,
					&saved_memory_ssmr_registration);

		if (page == NULL) {
			offline_retry++;
			msleep(100);
		}
	} while (page == NULL && offline_retry < 20);

	if (page) {
		phys_addr_t start = page_to_phys(page);
		phys_addr_t end = start + (alloc_pages << PAGE_SHIFT);

		if (end > upper_limit) {
			pr_err("Reserve Over Limit, region end: %pa\n", &end);
			cma_release(cma, page, alloc_pages);
			page = NULL;
		}
	}

	if (!page) {
		feature->alloc_pages = 0;
		return -EBUSY;
	}

	ssmr_usage_count += alloc_pages;

	ret_map = memory_unmapping((unsigned long)__va((page_to_phys(page))),
			alloc_pages << PAGE_SHIFT);

	if (ret_map < 0) {
		pr_err("[unmapping fail]: virt:0x%lx, size:0x%lx",
				(unsigned long)__va((page_to_phys(page))),
				alloc_pages << PAGE_SHIFT);

		aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT
			| DB_OPT_DUMPSYS_ACTIVITY
			| DB_OPT_LOW_MEMORY_KILLER
			| DB_OPT_PID_MEMORY_INFO /* smaps and hprof */
			| DB_OPT_PROCESS_COREDUMP
			| DB_OPT_PAGETYPE_INFO
			| DB_OPT_DUMPSYS_PROCSTATS,
			"offline unmap fail\nCRDISPATCH_KEY:SVP_SS1\n",
			"[unmapping fail]: virt:0x%lx, size:0x%lx",
			(unsigned long)__va((page_to_phys(page))),
			alloc_pages << PAGE_SHIFT);

		feature->is_unmapping = false;
	} else
		feature->is_unmapping = true;

out:
	feature->page = page;
	if (pa)
		*pa = page_to_phys(page);
	if (size)
		*size = alloc_pages << PAGE_SHIFT;

	return 0;
}

static int memory_region_online(struct SSMR_Feature *feature)
{
	unsigned long alloc_pages;

	alloc_pages = feature->alloc_pages;

	if (feature->use_cache_memory) {
		feature->alloc_pages = 0;
		feature->page = NULL;
		return 0;
	}

	/* remapping if unmapping while offline */
	if (feature->is_unmapping) {
		int ret_map;
		unsigned long region_page_va =
			(unsigned long)__va(page_to_phys(feature->page));

		ret_map = memory_mapping(region_page_va,
				feature->alloc_pages << PAGE_SHIFT);

		if (ret_map < 0) {
			pr_err("[remapping fail]: virt:0x%lx, size:0x%lx",
					region_page_va,
					feature->alloc_pages << PAGE_SHIFT);

			aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DEFAULT
				| DB_OPT_DUMPSYS_ACTIVITY
				| DB_OPT_LOW_MEMORY_KILLER
				| DB_OPT_PID_MEMORY_INFO /* smaps and hprof */
				| DB_OPT_PROCESS_COREDUMP
				| DB_OPT_PAGETYPE_INFO
				| DB_OPT_DUMPSYS_PROCSTATS,
				"online remap fail\nCRDISPATCH_KEY:SVP_SS1\n",
				"[remapping fail]: virt:0x%lx, size:0x%lx",
				region_page_va,
				feature->alloc_pages << PAGE_SHIFT);

			feature->use_cache_memory = true;
			feature->cache_page = feature->page;
		} else
			feature->is_unmapping = false;
	}

	if (!feature->is_unmapping && !feature->use_cache_memory) {
		zmc_cma_release(cma, feature->page, feature->alloc_pages);
		ssmr_usage_count -= feature->alloc_pages;
		feature->alloc_pages = 0;
	}

	feature->page = NULL;
	return 0;
}

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI) ||\
	defined(CONFIG_BLOWFISH_TUI_SUPPORT)
int _tui_region_offline(phys_addr_t *pa, unsigned long *size,
		u64 upper_limit)
{
	int retval = 0;
	struct SSMR_Feature *feature = &_ssmr_feats[SSMR_FEAT_TUI];

	pr_info("%s: >>>>>> state: %s, upper_limit:0x%llx\n", __func__,
			ssmr_state_text[feature->state], upper_limit);

	if (feature->state != SSMR_STATE_ON) {
		retval = -EBUSY;
		goto out;
	}

	feature->state = SSMR_STATE_OFFING;
	retval = memory_region_offline(feature, pa, size, upper_limit);
	if (retval < 0) {
		feature->state = SSMR_STATE_ON;
		retval = -EAGAIN;
		goto out;
	}

	feature->state = SSMR_STATE_OFF;
	pr_info("%s: [reserve done]: pa 0x%lx, size 0x%lx\n",
			__func__, (unsigned long)page_to_phys(feature->page),
			feature->count << PAGE_SHIFT);

out:
	pr_info("%s: <<<<<< state: %s, retval: %d\n", __func__,
			ssmr_state_text[feature->state], retval);
	return retval;
}

int tui_region_offline(phys_addr_t *pa, unsigned long *size)
{
	return _tui_region_offline(pa, size, UPPER_LIMIT32);
}
EXPORT_SYMBOL(tui_region_offline);

int tui_region_offline64(phys_addr_t *pa, unsigned long *size)
{
	return _tui_region_offline(pa, size, UPPER_LIMIT64);
}
EXPORT_SYMBOL(tui_region_offline64);

int tui_region_online(void)
{
	struct SSMR_Feature *feature = &_ssmr_feats[SSMR_FEAT_TUI];
	int retval;

	pr_info("%s: >>>>>> enter state: %s\n", __func__,
			ssmr_state_text[feature->state]);

	if (feature->state != SSMR_STATE_OFF) {
		retval = -EBUSY;
		goto out;
	}

	feature->state = SSMR_STATE_ONING_WAIT;
	retval = memory_region_online(feature);
	feature->state = SSMR_STATE_ON;

out:
	pr_info("%s: <<<<<< leave state: %s, retval: %d\n",
			__func__, ssmr_state_text[feature->state], retval);

	return retval;
}
EXPORT_SYMBOL(tui_region_online);
#endif

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static void reset_svp_online_task(void)
{
	mutex_lock(&svp_online_task_lock);
	_svp_online_task = NULL;
	mutex_unlock(&svp_online_task_lock);
}

static int _svp_wdt_kthread_func(void *data)
{
	atomic_set(&svp_online_count_down, COUNT_DOWN_LIMIT);

	pr_info("[START COUNT DOWN]: %dms/%dms\n",
			COUNT_DOWN_MS, COUNT_DOWN_INTERVAL);

	for (; atomic_read(&svp_online_count_down) > 0;
		atomic_dec(&svp_online_count_down)) {
		msleep(COUNT_DOWN_INTERVAL);

		/*
		 * some component need ssmr memory,
		 * and stop count down watch dog
		 */
		if (atomic_read(&svp_ref_count) > 0) {
			pr_info("[STOP COUNT DOWN]: new request for ssmr\n");
			reset_svp_online_task();
			return 0;
		}

		if (_ssmr_feats[SSMR_FEAT_SVP].state == SSMR_STATE_ON) {
			pr_info("[STOP COUNT DOWN]: SSMR has online\n");
			reset_svp_online_task();
			return 0;
		}
		pr_info("[COUNT DOWN]: %d\n",
				atomic_read(&svp_online_count_down));

	}
	pr_err("[COUNT DOWN FAIL]\n");

	ion_sec_heap_dump_info();

	pr_debug("Shareable SecureMemoryRegion trigger kernel warning");
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT
			| DB_OPT_DUMPSYS_ACTIVITY
			| DB_OPT_LOW_MEMORY_KILLER
			| DB_OPT_PID_MEMORY_INFO /* smaps and hprof */
			| DB_OPT_PROCESS_COREDUMP
			| DB_OPT_DUMPSYS_SURFACEFLINGER
			| DB_OPT_DUMPSYS_GFXINFO
			| DB_OPT_DUMPSYS_PROCSTATS,
			"SVP online fail.\nCRDISPATCH_KEY:SVP_SS1\n",
			"[SSMR ONLINE FAIL]: online timeout.\n");

	reset_svp_online_task();
	return 0;
}

int svp_start_wdt(void)
{
	mutex_lock(&svp_online_task_lock);

	if (_svp_online_task == NULL) {
		_svp_online_task = kthread_create(_svp_wdt_kthread_func,
					NULL, "svp_online_kthread");
		if (IS_ERR_OR_NULL(_svp_online_task)) {
			mutex_unlock(&svp_online_task_lock);
			pr_warn("%s: fail to create svp_online_task\n",
					__func__);
			return -1;
		}
	} else {
		atomic_set(&svp_online_count_down, COUNT_DOWN_LIMIT);
		mutex_unlock(&svp_online_task_lock);
		pr_info("%s: svp_online_task is already created\n", __func__);
		return -1;
	}

	wake_up_process(_svp_online_task);

	mutex_unlock(&svp_online_task_lock);
	return 0;
}

int _secmem_region_offline(phys_addr_t *pa, unsigned long *size,
		u64 upper_limit)
{
	int retval = 0;
	struct SSMR_Feature *feature = &_ssmr_feats[SSMR_FEAT_SVP];

	pr_info("%s: >>>>>> state: %s, upper_limit:0x%llx\n", __func__,
			ssmr_state_text[feature->state], upper_limit);

	if (feature->state != SSMR_STATE_ON) {
		retval = -EBUSY;
		goto out;
	}

	feature->state = SSMR_STATE_OFFING;
	retval = memory_region_offline(feature, pa, size, upper_limit);
	if (retval < 0) {
		feature->state = SSMR_STATE_ON;
		retval = -EAGAIN;
		goto out;
	}

	feature->state = SSMR_STATE_OFF;
	pr_info("%s: [reserve done]: pa 0x%lx, size 0x%lx\n",
			__func__, (unsigned long)page_to_phys(feature->page),
			feature->count << PAGE_SHIFT);

out:
	pr_info("%s: <<<<<< state: %s, retval: %d\n", __func__,
			ssmr_state_text[feature->state], retval);
	return retval;
}

int svp_region_offline64(phys_addr_t *pa, unsigned long *size)
{
	return _secmem_region_offline(pa, size, UPPER_LIMIT64);
}
EXPORT_SYMBOL(svp_region_offline64);

int svp_region_offline(phys_addr_t *pa, unsigned long *size)
{
	return _secmem_region_offline(pa, size, UPPER_LIMIT32);
}
EXPORT_SYMBOL(svp_region_offline);

int secmem_region_online(void)
{
	struct SSMR_Feature *feature = &_ssmr_feats[SSMR_FEAT_SVP];
	int retval;

	pr_info("%s: >>>>>> enter state: %s\n", __func__,
			ssmr_state_text[feature->state]);

	if (feature->state != SSMR_STATE_OFF) {
		retval = -EBUSY;
		goto out;
	}

	feature->state = SSMR_STATE_ONING_WAIT;
	retval = memory_region_online(feature);
	feature->state = SSMR_STATE_ON;

out:
	pr_info("%s: <<<<<< leave state: %s, retval: %d\n",
			__func__, ssmr_state_text[feature->state], retval);
	return retval;
}
EXPORT_SYMBOL(secmem_region_online);

int svp_region_online(void)
{
	return secmem_region_online();
}
EXPORT_SYMBOL(svp_region_online);
#endif

static bool is_valid_feature(unsigned int feat)
{
	if (SSMR_INVALID_FEATURE(feat)) {
		pr_info("%s: invalid feature_type: %d\n",
			__func__, feat);
		return false;
	}

	return true;
}

static void show_scheme_status(u64 size)
{
	int i = 0;

	if (cma) {
		pr_info("total cma size: 0x%lx, remaining size: 0x%lx\n",
				cma_get_size(cma),
				((cma_get_size(cma) >> PAGE_SHIFT) -
				ssmr_usage_count)*PAGE_SIZE);
		pr_info("request size: %pa\n", &size);
	}

	for (; i < __MAX_NR_SCHEME; i++) {
		int j = 0;

		if (__MAX_NR_SSMR_FEATURES <= 0)
			break;
		pr_info("**** %s  (size: %pa)****\n",  _ssmrscheme[i].name,
						&_ssmrscheme[i].usable_size);
		for (; j < __MAX_NR_SSMR_FEATURES; j++) {
			if (_ssmr_feats[j].scheme_flag & _ssmrscheme[i].flags) {
				pr_info("%s: size= %pa, state=%s\n",
					_ssmr_feats[j].feat_name,
					&_ssmr_feats[j].req_size,
					ssmr_state_text[_ssmr_feats[j].state]);
			}
		}
	}
}

static int _ssmr_offline_internal(phys_addr_t *pa, unsigned long *size,
		u64 upper_limit, unsigned int feat)
{
	int retval = 0;
	struct SSMR_Feature *feature = NULL;

	feature = &_ssmr_feats[feat];
	pr_info("%s %d: >>>>>> feat: %s, state: %s, upper_limit:0x%llx\n",
			__func__, __LINE__,
			feat < __MAX_NR_SSMR_FEATURES ?
			feature->feat_name : "NULL",
			ssmr_state_text[feature->state], upper_limit);

	if (feature->state != SSMR_STATE_ON) {
		retval = -EBUSY;
		goto out;
	}

	feature->state = SSMR_STATE_OFFING;
	retval = memory_region_offline(feature, pa, size, upper_limit);
	if (retval < 0) {
		feature->state = SSMR_STATE_ON;
		retval = -EAGAIN;
		goto out;
	}

	feature->state = SSMR_STATE_OFF;
	pr_info("%s %d: [reserve done]: pa 0x%lx, size 0x%lx\n",
		__func__, __LINE__, (unsigned long)page_to_phys(feature->page),
		feature->alloc_pages << PAGE_SHIFT);

out:
	pr_info("%s %d: <<<<< request feat: %s, state: %s, retval: %d\n",
		__func__, __LINE__,
		feat < __MAX_NR_SSMR_FEATURES ?
		_ssmr_feats[feat].feat_name : "NULL",
		ssmr_state_text[feature->state],
		retval);

	if (retval < 0)
		show_scheme_status(feature->req_size);

	return retval;

}

int ssmr_offline(phys_addr_t *pa, unsigned long *size, bool is_64bit,
		unsigned int feat)
{
	if (!is_valid_feature(feat))
		return -EINVAL;

	return _ssmr_offline_internal(pa, size,
		is_64bit ? UPPER_LIMIT64 : UPPER_LIMIT32,
		feat);
}
EXPORT_SYMBOL(ssmr_offline);

static int _ssmr_online_internal(unsigned int feat)
{
	int retval;
	struct SSMR_Feature *feature = NULL;

	feature = &_ssmr_feats[feat];
	pr_info("%s %d: >>>>>> enter state: %s\n", __func__, __LINE__,
			ssmr_state_text[feature->state]);

	if (feature->state != SSMR_STATE_OFF) {
		retval = -EBUSY;
		goto out;
	}

	feature->state = SSMR_STATE_ONING_WAIT;
	retval = memory_region_online(feature);
	feature->state = SSMR_STATE_ON;

out:
	pr_info("%s %d: <<<<<< request feature: %s, ",
		__func__, __LINE__,
		feat < __MAX_NR_SSMR_FEATURES ?
		_ssmr_feats[feat].feat_name : "NULL");
	pr_info("leave state: %s, retval: %d\n",
		ssmr_state_text[feature->state], retval);

	return retval;

}

int ssmr_online(unsigned int feat)
{
	if (!is_valid_feature(feat))
		return -EINVAL;

	return _ssmr_online_internal(feat);
}
EXPORT_SYMBOL(ssmr_online);

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static long svp_cma_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	int ret = 0;

	pr_info("%s: cmd: %x\n", __func__, cmd);

	switch (cmd) {
	case SVP_REGION_ACQUIRE:
		atomic_inc(&svp_ref_count);
		break;
	case SVP_REGION_RELEASE:
		atomic_dec(&svp_ref_count);

		if (atomic_read(&svp_ref_count) == 0)
			svp_start_wdt();
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long svp_cma_COMPAT_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	pr_info("%s: cmd: %x\n", __func__, cmd);

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	return filp->f_op->unlocked_ioctl(filp, cmd,
				(unsigned long)compat_ptr(arg));
}

#else

#define svp_cma_COMPAT_ioctl  NULL

#endif

static const struct file_operations ssmr_cma_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = svp_cma_ioctl,
	.compat_ioctl   = svp_cma_COMPAT_ioctl,
};
#endif // end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT

#if CONFIG_SYSFS
static ssize_t ssmr_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *cmd, size_t count)
{
	char buf[64];
	int buf_size;
	int feat = 0, ret;

	ret = snprintf(buf, sizeof(buf), "%s", cmd);
	if (ret && ret < sizeof(buf)) {

		buf_size = min(count - 1, (sizeof(buf) - 1));
		if (buf_size < 0)
			return -EINVAL;

		buf[buf_size] = 0;

		pr_info("%s[%d]: cmd> %s\n", __func__, __LINE__, buf);

		if (0 == __MAX_NR_SSMR_FEATURES)
			return -EINVAL;

		for (feat = 0; feat < __MAX_NR_SSMR_FEATURES; feat++) {
			if (!strncmp(buf, _ssmr_feats[feat].cmd_offline,
				     strlen(buf))) {
				ssmr_offline(NULL, NULL, ssmr_upper_limit,
									feat);
				break;
			} else if (!strncmp(buf, _ssmr_feats[feat].cmd_online,
					    strlen(buf))) {
				ssmr_online(feat);
				break;
			}
		}
	} else {
		pr_info("%s[%d]: get invalid cmd\n", __func__, __LINE__);
	}

	return count;
}

static ssize_t ssmr_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	int ret = 0;
	int i = 0;
	phys_addr_t cma_base = cma_get_base(cma);
	phys_addr_t cma_end = cma_base + cma_get_size(cma);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"cma info: [%pa-%pa] (0x%lx)\n",
		&cma_base, &cma_end,
		cma_get_size(cma));

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"cma info: base %pa pfn [%lu-%lu] count %lu\n",
		&cma_base,
		__phys_to_pfn(cma_base), __phys_to_pfn(cma_end),
		cma_get_size(cma) >> PAGE_SHIFT);

	if (__MAX_NR_SSMR_FEATURES <= 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"no SSMR user enable\n");
		return ret;
	}

	for (; i < __MAX_NR_SSMR_FEATURES; i++) {
		unsigned long region_pa;

		region_pa = (unsigned long) page_to_phys(_ssmr_feats[i].page);

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"%s base:0x%lx, count %lu, ",
			_ssmr_feats[i].feat_name,
			_ssmr_feats[i].page == NULL ? 0 : region_pa,
			_ssmr_feats[i].count);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"alloc_pages %lu, state %s.%s\n",
			_ssmr_feats[i].alloc_pages,
			ssmr_state_text[_ssmr_feats[i].state],
			_ssmr_feats[i].use_cache_memory ?
					 " (cache memory)" : "");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"cma usage: %lu pages\n", ssmr_usage_count);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "[CONFIG]:\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"ssmr_upper_limit: 0x%llx\n",
		ssmr_upper_limit);

	return ret;
}

static struct kobj_attribute ssmr_attribute = {
	.attr = {
		.name = "ssmr_state",
		.mode = 0644,
	},
	.show = ssmr_show,
	.store = ssmr_store,

};

static struct kobject *ssmr_kobject;

static int memory_ssmr_sysfs_init(void)
{
	int error = 0;

	ssmr_kobject = kobject_create_and_add("memory_ssmr", kernel_kobj);

	if (ssmr_kobject) {
		error = sysfs_create_file(ssmr_kobject, &ssmr_attribute.attr);
		if (error) {
			pr_info("SSMR: sysfs create failed\n");
			return -ENOMEM;
		}
	} else {
		pr_notice("SSMR: Cannot find module %s object\n",
				KBUILD_MODNAME);
		return -EINVAL;
	}

	return 0;
}
#endif /* end of CONFIG_SYSFS */

static int __init ssmr_sanity(void)
{
	phys_addr_t start = 0;
	unsigned long size = 0;
	u64 final_target_size = 0;
	char *err_msg = NULL;

	if (has_dedicate_resvmem_region())
		return 0;

	if (!cma) {
		/*
		 * If total size is 0, which will imply cma will NOT be
		 * initialized by ZMC.
		 */
		pr_err("[INIT FAIL]: cma is not inited\n");
		err_msg = "SSMR sanity: CMA init fail\n";
		goto out;
	}

	final_target_size = get_final_target_size();
	size = cma_get_size(cma);

	if (final_target_size > size) {
		start = cma_get_base(cma);
		pr_info("Total target size: %pa, cma start: %pa, size: %pa\n",
				&final_target_size, &start, &size);
		err_msg = "SSMR sanity: not enough reserved memory\n";
		goto out;
	}

	return 0;

out:
	aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DEFAULT
			| DB_OPT_DUMPSYS_ACTIVITY
			| DB_OPT_PID_MEMORY_INFO /* smaps and hprof */
			| DB_OPT_DUMPSYS_PROCSTATS,
			"SSMR Sanity fail.\nCRDISPATCH_KEY:SVP_SS1\n",
			err_msg);
	return -1;
}


static int __init memory_ssmr_init_feature(char *name, u64 size,
	struct SSMR_Feature *feature, const struct file_operations *entry_fops)
{
	struct proc_dir_entry *procfs_entry;
	bool has_dedicate_memory = (feature->use_cache_memory);
	bool has_region = (has_dedicate_memory || size > 0);

	if (!has_region) {
		feature->state = SSMR_STATE_DISABLED;
		return -1;
	}

	if (entry_fops) {
		procfs_entry = proc_create(name, 0444, NULL, entry_fops);
		if (!procfs_entry) {
			pr_info("Failed to create procfs ssmr_region file\n");
			return -1;
		}
	}

	if (has_dedicate_memory) {
		pr_info("[%s]:Use dedicate memory as cached memory\n", name);
		size = ((u64) feature->count) * PAGE_SIZE;
		goto region_init_done;
	}

	feature->count = (unsigned long)(size >> PAGE_SHIFT);

	if (is_pre_reserve_memory) {
		int ret_map;
		struct page *page;
		unsigned long region_cache_page_va =
			(unsigned long)__va(page_to_phys(feature->cache_page));

		page = zmc_cma_alloc(cma, feature->count,
				SSMR_CMA_ALIGN_PAGE_ORDER,
				&memory_ssmr_registration);
		feature->use_cache_memory = true;
		feature->cache_page = page;
		ssmr_usage_count += feature->count;
		ret_map = memory_unmapping(region_cache_page_va,
				feature->count << PAGE_SHIFT);

		if (ret_map < 0) {
			pr_err("[unmapping fail]: virt:0x%lx, size:0x%lx",
				(unsigned long)__va((page_to_phys(page))),
				feature->count << PAGE_SHIFT);

			aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DEFAULT
				| DB_OPT_DUMPSYS_ACTIVITY
				| DB_OPT_LOW_MEMORY_KILLER
				| DB_OPT_PID_MEMORY_INFO /* smaps and hprof */
				| DB_OPT_PROCESS_COREDUMP
				| DB_OPT_PAGETYPE_INFO
				| DB_OPT_DUMPSYS_PROCSTATS,
				"offline unmap fail\nCRDISPATCH_KEY:SVP_SS1\n",
				"[unmapping fail]: virt:0x%lx, size:0x%lx",
				(unsigned long)__va((page_to_phys(page))),
				feature->count << PAGE_SHIFT);

			feature->is_unmapping = false;
		} else
			feature->is_unmapping = true;
	}

region_init_done:
	feature->state = SSMR_STATE_ON;
	pr_info("%s: %s is enable with size: %pa\n",
			__func__, name, &size);
	return 0;
}

static int __init memory_ssmr_debug_init(void)
{
	int i = 0;

	if (ssmr_sanity() < 0) {
		pr_err("SSMR sanity fail\n");
		return 1;
	}

	if (reserved_for_unmap_ready)
		pr_info("%s: reserved mem for unmap is ready\n", __func__);

	pr_info("[PASS]: SSMR sanity.\n");

	/* ssmr sys file init */
#if CONFIG_SYSFS
	memory_ssmr_sysfs_init();
#endif

	/*
	 * TODO: integrate into _svpregs[] initialization
	 */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	_ssmr_feats[i].proc_entry_fops = &ssmr_cma_fops;
#endif

	if (__MAX_NR_SSMR_FEATURES <= 0)
		return 0;

	for (; i < __MAX_NR_SSMR_FEATURES; i++) {
		memory_ssmr_init_feature(_ssmr_feats[i].feat_name,
			_ssmr_feats[i].req_size,
			&_ssmr_feats[i],
			_ssmr_feats[i].proc_entry_fops);
	}

	return 0;
}
late_initcall(memory_ssmr_debug_init);
