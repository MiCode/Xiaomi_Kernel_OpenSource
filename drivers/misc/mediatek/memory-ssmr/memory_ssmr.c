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
#ifdef CONFIG_ARM64
#include <asm/tlbflush.h>
#endif
#include "memory_ssmr.h"

#define COUNT_DOWN_MS 10000
#define	COUNT_DOWN_INTERVAL 500
#define	COUNT_DOWN_LIMIT (COUNT_DOWN_MS / COUNT_DOWN_INTERVAL)
static atomic_t svp_online_count_down;

/* 64 MB alignment */
#define SSMR_CMA_ALIGN_PAGE_ORDER 14
#define SSMR_ALIGN_SHIFT (SSMR_CMA_ALIGN_PAGE_ORDER + PAGE_SHIFT)
#define SSMR_ALIGN (1 << (PAGE_SHIFT + (MAX_ORDER - 1)))

static u64 ssmr_upper_limit = UPPER_LIMIT64;

/* Flag indicates if all SSMR features use reserved memory as source */
static int is_pre_reserve_memory;

#include <mt-plat/aee.h>
#include "mt-plat/mtk_meminfo.h"

static unsigned long ssmr_usage_count;
static atomic_t svp_ref_count;

enum ssmr_subtype {
	SSMR_SECMEM,
	SSMR_TUI,
	__MAX_NR_SSMRSUBS,
};

enum ssmr_state {
	SSMR_STATE_DISABLED,
	SSMR_STATE_ONING_WAIT,
	SSMR_STATE_ONING,
	SSMR_STATE_ON,
	SSMR_STATE_OFFING,
	SSMR_STATE_OFF,
	NR_STATES,
};

const char *const ssmr_state_text[NR_STATES] = {
	[SSMR_STATE_DISABLED]   = "[DISABLED]",
	[SSMR_STATE_ONING_WAIT] = "[ONING_WAIT]",
	[SSMR_STATE_ONING]      = "[ONING]",
	[SSMR_STATE_ON]         = "[ON]",
	[SSMR_STATE_OFFING]     = "[OFFING]",
	[SSMR_STATE_OFF]        = "[OFF]",
};

struct SSMR_Region {
	unsigned int state;
	unsigned long count;
	bool is_unmapping;
	bool use_cache_memory;
	struct page *page;
	struct page *cache_page;
};

static struct task_struct *_svp_online_task; /* NULL */
static DEFINE_MUTEX(svp_online_task_lock);
static struct cma *cma;
static struct SSMR_Region _ssmregs[__MAX_NR_SSMRSUBS];

#define pmd_unmapping(virt, size) set_pmd_mapping(virt, size, 0)
#define pmd_mapping(virt, size) set_pmd_mapping(virt, size, 1)

/*
 * Use for zone-movable-cma callback
 */

#define SSMR_FEAT_DT_UNAME "memory-ssmr-features"

#define DT_PROP_SVP_SZ "svp-size"
#define DT_PROP_IRIS_SZ "iris-recognition-size"
#define DT_PROP_TUI_SZ "tui-size"

/* Only support configured by DT node, kernel CONFIG is obsolete */
static u64 svp_size;
static u64 tui_size;
static u64 secmem_usable_size;
static u64 iris_size;

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
	if (!strncmp(uname, SSMR_FEAT_DT_UNAME, strlen(SSMR_FEAT_DT_UNAME))) {
		get_feature_size_by_dt_prop(node, DT_PROP_SVP_SZ, &svp_size);
		get_feature_size_by_dt_prop(node, DT_PROP_IRIS_SZ, &iris_size);
		get_feature_size_by_dt_prop(node, DT_PROP_TUI_SZ, &tui_size);

		return 1;
	}

	return 0;
}

/*
 * There are multiple features will go through secmem module.
 * But we only care the maximum required size among those features.
 */
static void __init setup_secmem_usable_size(void)
{
	secmem_usable_size = max(svp_size, iris_size);
}

static void __init finalize_ssmr_features_size(struct reserved_mem *rmem)
{
	if (of_scan_flat_dt(dt_scan_for_ssmr_features, NULL) == 0)
		pr_info("%s, can't find DT node, %s\n",
			__func__, SSMR_FEAT_DT_UNAME);

	pr_info("%s, rmem-sz: %pa, svp-sz: %pa, iris-sz: %pa, tui-sz: %pa\n",
		__func__, &rmem->size, &svp_size, &iris_size, &tui_size);

	setup_secmem_usable_size();
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
	u64 total_target_size = 0;

	finalize_ssmr_features_size(rmem);

	total_target_size = secmem_usable_size + tui_size;
	pr_info("%s, total target size: %pa\n", __func__, &total_target_size);

	memory_ssmr_registration.size = total_target_size;
	saved_memory_ssmr_registration.size = total_target_size;

	if (total_target_size == 0) {
		pr_alert("%s, SSMR init: skipped, no requirement\n", __func__);
	} else if (total_target_size > 0 && total_target_size <= rmem->size) {
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
	ret = cma_init_reserved_mem(rmem->base, rmem->size, 0, &cma);

	if (ret) {
		pr_err("%s cma failed, ret: %d\n", __func__, ret);
		return 1;
	}

	is_pre_reserve_memory = 1;

	return 0;
}
RESERVEDMEM_OF_DECLARE(memory_ssmr, "mediatek,memory-ssmr",
			memory_ssmr_init);

static int __init dedicate_tui_memory(struct reserved_mem *rmem)
{
	struct SSMR_Region *region;

	region = &_ssmregs[SSMR_TUI];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	if (ssmr_preinit(rmem))
		return 1;

	region->use_cache_memory = true;
	region->is_unmapping = true;
	region->count = rmem->size / PAGE_SIZE;
	region->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(tui_memory, "mediatek,memory-tui",
			dedicate_tui_memory);

static int __init dedicate_secmem_memory(struct reserved_mem *rmem)
{
	struct SSMR_Region *region;

	region = &_ssmregs[SSMR_SECMEM];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	if (ssmr_preinit(rmem))
		return 1;

	region->use_cache_memory = true;
	region->is_unmapping = true;
	region->count = rmem->size / PAGE_SIZE;
	region->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(secmem_memory, "mediatek,memory-secmem",
			dedicate_secmem_memory);

/*
 * Check whether memory_ssmr is initialized
 */
bool memory_ssmr_inited(void)
{
	return (_ssmregs[SSMR_SECMEM].use_cache_memory ||
		_ssmregs[SSMR_TUI].use_cache_memory ||
		cma != NULL);
}

#ifdef CONFIG_ARM64
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
static int set_pmd_mapping(unsigned long start, phys_addr_t size, int map)
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
		pr_err("[invalid parameter]: start=0x%lx, size=%pa\n",
				start, &size);
		return -1;
	}

	pr_debug("start=0x%lx, size=%pa, address=0x%p, map=%d\n",
			start, &size, (void *)address, map);
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
#else
static inline int set_pmd_mapping(unsigned long start, phys_addr_t size,
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

static int memory_region_offline(struct SSMR_Region *region,
		phys_addr_t *pa, unsigned long *size, u64 upper_limit)
{
	int offline_retry = 0;
	int ret_map;
	struct page *page;
	phys_addr_t page_phys;

	/* compare with function and system wise upper limit */
	upper_limit = min(upper_limit, ssmr_upper_limit);

	if (region->cache_page)
		page_phys = page_to_phys(region->cache_page);
	else
		page_phys = 0;

	pr_info("%s: upper_limit: %llx\n", __func__, upper_limit);
	pr_info("region{count: %lu, is_unmapping: %c}\n",
			region->count, region->is_unmapping ? 'Y' : 'N');
	pr_info("region{use_cache_memory: %c, cache_page: %pa}\n",
			region->use_cache_memory ? 'Y' : 'N', &page_phys);

	if (region->use_cache_memory) {
		if (!region->cache_page) {
			pr_info("[NO_CACHE_MEMORY]:\n");
			return -EFAULT;
		}

		page = region->cache_page;
		goto out;
	}

	do {
		pr_info("[SSMR-ALLOCATION]: retry: %d\n", offline_retry);
		page = zmc_cma_alloc(cma, region->count,
					SSMR_CMA_ALIGN_PAGE_ORDER,
					&saved_memory_ssmr_registration);

		offline_retry++;
		msleep(100);
	} while (page == NULL && offline_retry < 20);

	if (page) {
		phys_addr_t start = page_to_phys(page);
		phys_addr_t end = start + (region->count << PAGE_SHIFT);

		if (end > upper_limit) {
			pr_err("Reserve Over Limit, region end: %pa\n", &end);
			cma_release(cma, page, region->count);
			page = NULL;
		}
	}

	if (!page)
		return -EBUSY;

	ssmr_usage_count += region->count;
	ret_map = pmd_unmapping((unsigned long)__va((page_to_phys(page))),
			region->count << PAGE_SHIFT);

	if (ret_map < 0) {
		pr_err("[unmapping fail]: virt:0x%lx, size:0x%lx",
				(unsigned long)__va((page_to_phys(page))),
				region->count << PAGE_SHIFT);

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
			region->count << PAGE_SHIFT);

		region->is_unmapping = false;
	} else
		region->is_unmapping = true;

out:
	region->page = page;
	if (pa)
		*pa = page_to_phys(page);
	if (size)
		*size = region->count << PAGE_SHIFT;

	return 0;
}

static int memory_region_online(struct SSMR_Region *region)
{
	if (region->use_cache_memory) {
		region->page = NULL;
		return 0;
	}

	/* remapping if unmapping while offline */
	if (region->is_unmapping) {
		int ret_map;
		unsigned long region_page_va =
			(unsigned long)__va(page_to_phys(region->page));

		ret_map = pmd_mapping(region_page_va,
				region->count << PAGE_SHIFT);

		if (ret_map < 0) {
			pr_err("[remapping fail]: virt:0x%lx, size:0x%lx",
					region_page_va,
					region->count << PAGE_SHIFT);

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
				region_page_va, region->count << PAGE_SHIFT);

			region->use_cache_memory = true;
			region->cache_page = region->page;
		} else
			region->is_unmapping = false;
	}

	if (!region->is_unmapping && !region->use_cache_memory) {
		zmc_cma_release(cma, region->page, region->count);
		ssmr_usage_count -= region->count;
	}

	region->page = NULL;
	return 0;
}

int _tui_region_offline(phys_addr_t *pa, unsigned long *size,
		u64 upper_limit)
{
	int retval = 0;
	struct SSMR_Region *region = &_ssmregs[SSMR_TUI];

	pr_info("%s: >>>>>> state: %s, upper_limit:0x%llx\n", __func__,
			ssmr_state_text[region->state], upper_limit);

	if (region->state != SSMR_STATE_ON) {
		retval = -EBUSY;
		goto out;
	}

	region->state = SSMR_STATE_OFFING;
	retval = memory_region_offline(region, pa, size, upper_limit);
	if (retval < 0) {
		region->state = SSMR_STATE_ON;
		retval = -EAGAIN;
		goto out;
	}

	region->state = SSMR_STATE_OFF;
	pr_info("%s: [reserve done]: pa 0x%lx, size 0x%lx\n",
			__func__, (unsigned long)page_to_phys(region->page),
			region->count << PAGE_SHIFT);

out:
	pr_info("%s: <<<<<< state: %s, retval: %d\n", __func__,
			ssmr_state_text[region->state], retval);
	return retval;
}

int tui_region_online(void)
{
	struct SSMR_Region *region = &_ssmregs[SSMR_TUI];
	int retval;

	pr_info("%s: >>>>>> enter state: %s\n", __func__,
			ssmr_state_text[region->state]);

	if (region->state != SSMR_STATE_OFF) {
		retval = -EBUSY;
		goto out;
	}

	region->state = SSMR_STATE_ONING_WAIT;
	retval = memory_region_online(region);
	region->state = SSMR_STATE_ON;

out:
	pr_info("%s: <<<<<< leave state: %s, retval: %d\n",
			__func__, ssmr_state_text[region->state], retval);

	return retval;
}
EXPORT_SYMBOL(tui_region_online);

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

		if (_ssmregs[SSMR_SECMEM].state == SSMR_STATE_ON) {
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
	struct SSMR_Region *region = &_ssmregs[SSMR_SECMEM];

	pr_info("%s: >>>>>> state: %s, upper_limit:0x%llx\n", __func__,
			ssmr_state_text[region->state], upper_limit);

	if (region->state != SSMR_STATE_ON) {
		retval = -EBUSY;
		goto out;
	}

	region->state = SSMR_STATE_OFFING;
	retval = memory_region_offline(region, pa, size, upper_limit);
	if (retval < 0) {
		region->state = SSMR_STATE_ON;
		retval = -EAGAIN;
		goto out;
	}

	region->state = SSMR_STATE_OFF;
	pr_info("%s: [reserve done]: pa 0x%lx, size 0x%lx\n",
			__func__, (unsigned long)page_to_phys(region->page),
			region->count << PAGE_SHIFT);

out:
	pr_info("%s: <<<<<< state: %s, retval: %d\n", __func__,
			ssmr_state_text[region->state], retval);
	return retval;
}

int secmem_region_online(void)
{
	struct SSMR_Region *region = &_ssmregs[SSMR_SECMEM];
	int retval;

	pr_info("%s: >>>>>> enter state: %s\n", __func__,
			ssmr_state_text[region->state]);

	if (region->state != SSMR_STATE_OFF) {
		retval = -EBUSY;
		goto out;
	}

	region->state = SSMR_STATE_ONING_WAIT;
	retval = memory_region_online(region);
	region->state = SSMR_STATE_ON;

out:
	pr_info("%s: <<<<<< leave state: %s, retval: %d\n",
			__func__, ssmr_state_text[region->state], retval);
	return retval;
}
EXPORT_SYMBOL(secmem_region_online);

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

static const struct file_operations svp_cma_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = svp_cma_ioctl,
	.compat_ioctl   = svp_cma_COMPAT_ioctl,
};
static ssize_t memory_ssmr_write(struct file *file,
		const char __user *user_buf, size_t size, loff_t *ppos)
{
	char buf[64];
	int buf_size;

	buf_size = min(size, (sizeof(buf) - 1));
	if (strncpy_from_user(buf, user_buf, buf_size) < 0)
		return -EFAULT;
	buf[buf_size] = 0;

	pr_info("%s: cmd> %s\n", __func__, buf);
	if (strncmp(buf, "secmem=on", 9) == 0)
		secmem_region_online();
	else if (strncmp(buf, "secmem=off", 10) == 0)
		_secmem_region_offline(NULL, NULL, ssmr_upper_limit);
	else if (strncmp(buf, "tui=on", 6) == 0)
		tui_region_online();
	else if (strncmp(buf, "tui=off", 7) == 0)
		_tui_region_offline(NULL, NULL, ssmr_upper_limit);
	else if (strncmp(buf, "32mode", 6) == 0)
		ssmr_upper_limit = 0x100000000ULL;
	else if (strncmp(buf, "64mode", 6) == 0)
		ssmr_upper_limit = UPPER_LIMIT64;
	else
		return -EINVAL;

	*ppos += size;
	return size;
}

static int memory_ssmr_show(struct seq_file *m, void *v)
{
	phys_addr_t cma_base = cma_get_base(cma);
	phys_addr_t cma_end = cma_base + cma_get_size(cma);
	unsigned long secmem_reg_page_pa =
		(unsigned long) page_to_phys(_ssmregs[SSMR_SECMEM].page);
	unsigned long tui_reg_page_pa =
		(unsigned long) page_to_phys(_ssmregs[SSMR_TUI].page);

	seq_printf(m, "cma info: [%pa-%pa] (0x%lx)\n",
		&cma_base, &cma_end,
		cma_get_size(cma));

	seq_printf(m, "cma info: base %pa pfn [%lu-%lu] count %lu\n",
		&cma_base,
		__phys_to_pfn(cma_base), __phys_to_pfn(cma_end),
		cma_get_size(cma) >> PAGE_SHIFT);

	seq_printf(m, "secmem region base:0x%lx, count %lu, state %s.%s\n",
		_ssmregs[SSMR_SECMEM].page == NULL ? 0 : secmem_reg_page_pa,
		_ssmregs[SSMR_SECMEM].count,
		ssmr_state_text[_ssmregs[SSMR_SECMEM].state],
		_ssmregs[SSMR_SECMEM].use_cache_memory ? "(cache memory)" : "");

	seq_printf(m, "tui region base:0x%lx, count %lu, state %s.%s\n",
		_ssmregs[SSMR_TUI].page == NULL ? 0 : tui_reg_page_pa,
		_ssmregs[SSMR_TUI].count,
		ssmr_state_text[_ssmregs[SSMR_TUI].state],
		_ssmregs[SSMR_TUI].use_cache_memory ? "(cache memory)" : "");

	seq_printf(m, "cma usage: %lu pages\n", ssmr_usage_count);

	seq_puts(m, "[CONFIG]:\n");
	seq_printf(m, "ssmr_upper_limit: 0x%llx\n", ssmr_upper_limit);

	return 0;
}

static int memory_ssmr_open(struct inode *inode, struct file *file)
{
	return single_open(file, &memory_ssmr_show, NULL);
}

static const struct file_operations memory_ssmr_fops = {
	.open		= memory_ssmr_open,
	.read		= seq_read,
	.write		= memory_ssmr_write,
	.release	= single_release,
};

static int __init ssmr_sanity(void)
{
	phys_addr_t start;
	unsigned long size;
	u64 total_target_size = secmem_usable_size + tui_size;
	char *err_msg = NULL;

	if (_ssmregs[SSMR_SECMEM].use_cache_memory ||
		_ssmregs[SSMR_TUI].use_cache_memory) {
		pr_info("%s, dedicate reserved memory as source\n", __func__);
		return 0;
	}

	if (!cma) {
		pr_err("[INIT FAIL]: cma is not inited\n");
		err_msg = "SSMR sanity: CMA init fail\n";
		goto out;
	}


	start = cma_get_base(cma);
	size = cma_get_size(cma);

	if (start + total_target_size > ssmr_upper_limit) {
		pr_err("[INVALID REGION]: CMA PA over 32 bit\n");
		pr_info("Total target size: %pa, cma start: %pa, size: %pa\n",
				&total_target_size, &start, &size);
		err_msg = "SSMR sanity: invalid CMA region due to over 32bit\n";
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


static int __init memory_ssmr_init_region(char *name, u64 size,
		struct SSMR_Region *region, char *proc_entry_name,
		const struct file_operations *entry_fops)
{
	struct proc_dir_entry *procfs_entry;
	bool has_dedicate_memory = (region->use_cache_memory);
	bool has_region = (has_dedicate_memory || size > 0);

	if (!has_region) {
		region->state = SSMR_STATE_DISABLED;
		return -1;
	}

	if (entry_fops && proc_entry_name) {
		procfs_entry = proc_create(proc_entry_name, 0444, NULL,
						entry_fops);
		if (!procfs_entry) {
			pr_err("Failed to create procfs %s file\n",
					proc_entry_name);
			return -1;
		}
	}

	if (has_dedicate_memory) {
		pr_info("[%s]:Use dedicate memory as cached memory\n", name);
		size = ((u64) region->count) * PAGE_SIZE;
		goto region_init_done;
	}

	region->count = (unsigned long)(size >> PAGE_SHIFT);

	if (is_pre_reserve_memory) {
		int ret_map;
		struct page *page;
		unsigned long region_cache_page_va =
			(unsigned long)__va(page_to_phys(region->cache_page));

		page = zmc_cma_alloc(cma, region->count,
				SSMR_CMA_ALIGN_PAGE_ORDER,
				&memory_ssmr_registration);
		region->use_cache_memory = true;
		region->cache_page = page;
		ssmr_usage_count += region->count;
		ret_map = pmd_unmapping(region_cache_page_va,
				region->count << PAGE_SHIFT);

		if (ret_map < 0) {
			pr_err("[unmapping fail]: virt:0x%lx, size:0x%lx",
				(unsigned long)__va((page_to_phys(page))),
				region->count << PAGE_SHIFT);

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
				region->count << PAGE_SHIFT);

			region->is_unmapping = false;
		} else
			region->is_unmapping = true;
	}

region_init_done:
	region->state = SSMR_STATE_ON;
	pr_info("%s: %s is enable with size: %pa\n",
			__func__, name, &size);
	return 0;
}

static int __init memory_ssmr_debug_init(void)
{
	struct dentry *dentry;

	if (ssmr_sanity() < 0) {
		pr_err("SSMR sanity fail\n");
		return 1;
	}

	pr_info("[PASS]: SSMR sanity.\n");

	dentry = debugfs_create_file("memory-ssmr", 0644, NULL, NULL,
			&memory_ssmr_fops);
	if (!dentry)
		pr_warn("Failed to create debugfs memory_ssmr file\n");

	memory_ssmr_init_region("secmem_region", secmem_usable_size,
			&_ssmregs[SSMR_SECMEM], "svp_region", &svp_cma_fops);
	memory_ssmr_init_region("tui_region", tui_size,
			&_ssmregs[SSMR_TUI], NULL, NULL);

	return 0;
}
late_initcall(memory_ssmr_debug_init);
