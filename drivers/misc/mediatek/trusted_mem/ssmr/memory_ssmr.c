// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "memory-ssmr: " fmt

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
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
#include <asm/memory.h>
#endif
#include "ssmr_internal.h"

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <linux/dma-direct.h>
#include <linux/kallsyms.h>

#define SSMR_FEATURES_DT_UNAME "memory-ssmr-features"
/* memory map/unmap */
#define memory_unmapping(virt, size) set_memory_mapping(virt, size, 0)
#define memory_mapping(virt, size) set_memory_mapping(virt, size, 1)

struct page_change_data {
	pgprot_t set_mask;
	pgprot_t clear_mask;
};

static u64 ssmr_upper_limit = UPPER_LIMIT64;

static struct device *ssmr_dev;

static const struct of_device_id ssmr_of_match_table[] = {
	{ .compatible = "mediatek,memory_ssmr"},
	{},
};

/* clang-format off */
const char *const ssmr_state_text[NR_STATES] = {
	[SSMR_STATE_DISABLED]   = "[DISABLED]",
	[SSMR_STATE_ONING_WAIT] = "[ONING_WAIT]",
	[SSMR_STATE_ONING]      = "[ONING]",
	[SSMR_STATE_ON]         = "[ON]",
	[SSMR_STATE_OFFING]     = "[OFFING]",
	[SSMR_STATE_OFF]        = "[OFF]",
};

static struct SSMR_Feature _ssmr_feats[__MAX_NR_SSMR_FEATURES] = {
	[SSMR_FEAT_SVP] = {
		.dt_prop_name = "svp-size",
		.feat_name = "svp",
		.cmd_online = "svp=on",
		.cmd_offline = "svp=off",
#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) ||\
	IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) ||\
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = SVP_FLAGS
	},
	[SSMR_FEAT_PROT_SHAREDMEM] = {
		.dt_prop_name = "prot-sharedmem-size",
		.feat_name = "prot-sharedmem",
		.cmd_online = "prot_sharedmem=on",
		.cmd_offline = "prot_sharedmem=off",
#ifdef CONFIG_MTK_PROT_MEM_SUPPORT
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_WFD] = {
		.dt_prop_name = "wfd-size",
		.feat_name = "wfd",
		.cmd_online = "wfd=on",
		.cmd_offline = "wfd=off",
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = SVP_FLAGS
	},
	[SSMR_FEAT_TA_ELF] = {
		.dt_prop_name = "ta-elf-size",
		.feat_name = "ta-elf",
		.cmd_online = "ta_elf=on",
		.cmd_offline = "ta_elf=off",
#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_TA_STACK_HEAP] = {
		.dt_prop_name = "ta-stack-heap-size",
		.feat_name = "ta-stack-heap",
		.cmd_online = "ta_stack_heap=on",
		.cmd_offline = "ta_stack_heap=off",
#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_SDSP_TEE_SHAREDMEM] = {
		.dt_prop_name = "sdsp-tee-sharedmem-size",
		.feat_name = "sdsp-tee-sharedmem",
		.cmd_online = "sdsp_tee_sharedmem=on",
		.cmd_offline = "sdsp_tee_sharedmem=off",
#ifdef CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_SDSP_FIRMWARE] = {
		.dt_prop_name = "sdsp-firmware-size",
		.feat_name = "sdsp-firmware",
		.cmd_online = "sdsp_firmware=on",
		.cmd_offline = "sdsp_firmware=off",
#ifdef CONFIG_MTK_SDSP_MEM_SUPPORT
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_2D_FR] = {
		.dt_prop_name = "2d_fr-size",
		.feat_name = "2d_fr",
		.cmd_online = "2d_fr=on",
		.cmd_offline = "2d_fr=off",
#ifdef CONFIG_MTK_CAM_SECURITY_SUPPORT
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_TUI] = {
		.dt_prop_name = "tui-size",
		.feat_name = "tui",
		.cmd_online = "tui=on",
		.cmd_offline = "tui=off",
#if IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI) ||\
	IS_ENABLED(CONFIG_BLOWFISH_TUI_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = TUI_FLAGS
	}
};
/* clang-format on */

struct SSMR_HEAP_INFO _ssmr_heap_info[__MAX_NR_SSMR_FEATURES];

#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) ||\
	IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) ||\
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
static int __init dedicate_svp_memory(struct reserved_mem *rmem)
{
	struct SSMR_Feature *feature;

	feature = &_ssmr_feats[SSMR_FEAT_SVP];

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n", __func__,
		rmem->name, &rmem->base, &rmem->size);

	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = rmem->size / PAGE_SIZE;
	feature->cache_page = phys_to_page(rmem->base);

	return 0;
}
RESERVEDMEM_OF_DECLARE(svp_memory, "mediatek,memory-svp",
		       dedicate_svp_memory);

static u64 dt_mem_read(int s, const __be32 **cellp)
{
	const __be32 *p = *cellp;
	u64 r = 0;

	*cellp = p + s;
	for (; s--; p++)
		r = (r << 32) | be32_to_cpu(*p);
	return r;

}

static int get_svp_memory_info(void)
{
	struct device_node *node;
	struct SSMR_Feature *feature;
	const __be32 *reg;
	u64 base, size;

	feature = &_ssmr_feats[SSMR_FEAT_SVP];

	node = of_find_compatible_node(NULL, NULL, "mediatek,memory-svp");
	if (!node) {
		pr_info("%s, svp no use static reserved memory\n", __func__);
		return 1;
	}

	reg = of_get_property(node, "reg", NULL);
	base = dt_mem_read(2, &reg);
	size = dt_mem_read(2, &reg);
	pr_info("%s, svp base : 0x%llx, size : 0x%llx", __func__,
							base, size);
	feature->use_cache_memory = true;
	feature->is_unmapping = true;
	feature->count = size / PAGE_SIZE;
	feature->cache_page = phys_to_page(base);
	pr_info("%s, feature->count 0x%lx\n", __func__, feature->count);

	return 0;
}
#endif

/* query ssmr heap name and id for ION */
int ssmr_query_total_sec_heap_count(void)
{
	int i;
	int total_heap_count = 0;

	for (i = 0; i < __MAX_NR_SSMR_FEATURES; i++) {
		if (!strncmp(_ssmr_feats[i].enable, "on", 2)) {
			_ssmr_heap_info[total_heap_count].heap_id = i;
			snprintf(_ssmr_heap_info[total_heap_count].heap_name,
				 NAME_SIZE, "ion_%s_heap",
				 _ssmr_feats[i].feat_name);
			total_heap_count++;
		}
	}
	return total_heap_count;
}

int ssmr_query_heap_info(int heap_index, char *heap_name)
{
	int heap_id = 0;
	int i;

	for (i = 0; i < __MAX_NR_SSMR_FEATURES; i++) {
		if (i == heap_index) {
			heap_id = _ssmr_heap_info[i].heap_id;
			snprintf(heap_name, NAME_SIZE, "%s",
				 _ssmr_heap_info[i].heap_name);
			return heap_id;
		}
	}
	return -1;
}

static void  setup_feature_size(void)
{

	int i = 0;
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(NULL, SSMR_FEATURES_DT_UNAME);

	if (!dt_node)
		pr_info("%s, failed to find the ssmr device tree\n",
							__func__);

	if (!strncmp(dt_node->name, SSMR_FEATURES_DT_UNAME,
		     strlen(SSMR_FEATURES_DT_UNAME))) {
		for (; i < __MAX_NR_SSMR_FEATURES; i++) {
			if (!strncmp(_ssmr_feats[i].enable, "on", 2)) {
				of_property_read_u64(
					dt_node, _ssmr_feats[i].dt_prop_name,
					&_ssmr_feats[i].req_size);
			}
		}

	}

	for (i = 0; i < __MAX_NR_SSMR_FEATURES; i++) {
		pr_info("%s, %s: %pa\n", __func__, _ssmr_feats[i].dt_prop_name,
			&_ssmr_feats[i].req_size);
	}
}

static void finalize_scenario_size(void)
{
	int i = 0;

	for (; i < __MAX_NR_SCHEME; i++) {
		u64 total_size = 0;
		int j = 0;

		for (; j < __MAX_NR_SSMR_FEATURES; j++) {
			if ((!strncmp(_ssmr_feats[j].enable, "on", 2)) &&
				(_ssmr_feats[j].scheme_flag &
				 _ssmrscheme[i].flags)) {
				total_size += _ssmr_feats[j].req_size;
			}
		}
		_ssmrscheme[i].usable_size = total_size;
		pr_info("%s, %s: %pa\n", __func__, _ssmrscheme[i].name,
			&_ssmrscheme[i].usable_size);
	}
}

static int
memory_ssmr_init_feature(char *name, u64 size, struct SSMR_Feature *feature,
			const struct file_operations *entry_fops)
{
	if (size <= 0) {
		feature->state = SSMR_STATE_DISABLED;
		return -1;
	}

	feature->state = SSMR_STATE_ON;
	pr_info("%s: %s is enable with size: %pa\n", __func__, name, &size);
	return 0;
}

static bool is_valid_feature(unsigned int feat)
{
	if (SSMR_INVALID_FEATURE(feat)) {
		pr_info("%s: invalid feature_type: %d\n", __func__, feat);
		return false;
	}

	return true;
}

static void show_scheme_status(u64 size)
{
	int i = 0;

	for (; i < __MAX_NR_SCHEME; i++) {
		int j = 0;

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

static int get_reserved_cma_memory(struct device *dev)
{
	struct device_node *np;
	struct reserved_mem *rmem;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);

	if (!np) {
		pr_info("%s, no ssmr region\n", __func__);
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(np);
	of_node_put(np);

	if (!rmem) {
		pr_info("%s, no ssmr device info\n", __func__);
		return -EINVAL;
	}

	pr_info("resource base=%pa, size=%pa\n", &rmem->base, &rmem->size);

	/*
	 * setup init device with rmem
	 */
	of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);

	return 0;
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

	if ((start != (start & PMD_MASK)) || (size != (size & PMD_MASK))
	    || !memblock_is_memory(virt_to_phys((void *)start)) || !size
	    || !start) {
		pr_info("[invalid parameter]: start=0x%lx, size=%pa\n", start,
			&size);
		return -1;
	}

	pr_debug("start=0x%lx, size=%pa, address=0x%p, map=%d\n", start, &size,
		 (void *)address, map);

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
	pr_info("start=0x%lx, size=%pa, address=0x%p, map=%d\n", start, &size,
		(void *)address, map);
	//show_pte(NULL, address);
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

static int memory_region_offline(struct SSMR_Feature *feature, phys_addr_t *pa,
				 unsigned long *size, u64 upper_limit)
{
	int ret_map;
	int offline_retry = 0;
	struct device_node *np;
	size_t alloc_size;
	struct page *page;

	if (!ssmr_dev) {
		pr_info("%s: No ssmr device\n", __func__);
		return -EINVAL;
	}

	if (feature->cache_page) {
		alloc_size = (feature->count) << PAGE_SHIFT;
		page =  feature->cache_page;
		if (pa)
			*pa = page_to_phys(page);
		if (size)
			*size = alloc_size;

		feature->phy_addr = page_to_phys(page);
		feature->alloc_size = alloc_size;
		pr_info("%s: [cache memory] pa %pa(%zx) is allocated\n",
				__func__, &feature->phy_addr, alloc_size);
		return 0;
	}

	np = of_parse_phandle(ssmr_dev->of_node, "memory-region", 0);

	if (!np) {
		pr_info(" %s, no ssmr region\n", __func__);
		return -EINVAL;
	}

	/* Determine alloc size by feature */
	alloc_size = feature->req_size;

	feature->alloc_size = alloc_size;

	pr_info("%s[%d]: upper_limit: %llx, feature{ alloc_size : %lu",
		__func__, __LINE__, upper_limit, alloc_size);

	/*
	 * setup init device with rmem
	 */
	of_reserved_mem_device_init_by_idx(ssmr_dev, ssmr_dev->of_node, 0);

	do {
		pr_info("[SSMR-ALLOCATION]: retry: %d\n", offline_retry);
		feature->virt_addr = dma_alloc_attrs(ssmr_dev, alloc_size,
					&feature->phy_addr, GFP_KERNEL, 0);
		if (!feature->phy_addr) {
			offline_retry++;
			msleep(100);
		}
	} while (!feature->phy_addr && offline_retry < 20);

	if (feature->phy_addr) {
		pr_info("%s: pa=%pad is allocated\n", __func__,
						&feature->phy_addr);
		pr_info("%s: virt 0x%lx\n", __func__,
				(unsigned long)phys_to_virt(
				dma_to_phys(ssmr_dev, feature->phy_addr)));
	} else {
		pr_info("%s: ssmr offline failed\n", __func__);
		return -1;
	}

	/*unmap offline memory */
	ret_map = memory_unmapping((unsigned long)
			phys_to_virt(dma_to_phys(ssmr_dev,
			feature->phy_addr)), alloc_size);
	if (ret_map < 0)
		feature->is_unmapping = false;
	else
		feature->is_unmapping = true;

	if (pa)
		*pa = dma_to_phys(ssmr_dev, feature->phy_addr);
	if (size)
		*size = alloc_size;

	return 0;
}

static int _ssmr_offline_internal(phys_addr_t *pa, unsigned long *size,
				  u64 upper_limit, unsigned int feat)
{
	int retval = 0;
	struct SSMR_Feature *feature = NULL;

	feature = &_ssmr_feats[feat];
	pr_info("%s %d: >>>>>> feat: %s, state: %s, upper_limit:0x%llx\n",
		__func__, __LINE__,
		feat < __MAX_NR_SSMR_FEATURES ? feature->feat_name
					      : "NULL",
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
	pr_info("%s %d: [reserve done]: pa: %pad, size: 0x%lx\n", __func__,
		__LINE__, &feature->phy_addr,
		feature->alloc_size);

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

	return _ssmr_offline_internal(
		pa, size, is_64bit ? UPPER_LIMIT64 : UPPER_LIMIT32, feat);
}
EXPORT_SYMBOL(ssmr_offline);

static int memory_region_online(struct SSMR_Feature *feature)
{
	size_t alloc_size;

	if (!ssmr_dev)
		pr_info("%s: No ssmr device\n", __func__);

	if (feature->use_cache_memory) {
		feature->alloc_pages = 0;
		feature->alloc_size = 0;
		feature->phy_addr = 0;
		return 0;
	}

	alloc_size = feature->alloc_size;

	if (feature->is_unmapping) {
		int ret_map;

		/* do remapping before free cma */
		ret_map = memory_mapping((unsigned long)
			phys_to_virt(dma_to_phys(ssmr_dev, feature->phy_addr)),
			alloc_size);
		if (ret_map < 0)
			pr_info("[remapping fail]: virt:0x%lx, size:0x%lx",
			(unsigned long)
			phys_to_virt(dma_to_phys(ssmr_dev, feature->phy_addr)),
			alloc_size);
		else
			feature->is_unmapping = false;
	}

	if (feature->phy_addr && !feature->is_unmapping) {
		dma_free_attrs(ssmr_dev, alloc_size,
				feature->virt_addr, feature->phy_addr, 0);
		feature->alloc_size = 0;
		feature->phy_addr = 0;
	}
	return 0;
}

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

#if IS_ENABLED(CONFIG_SYSFS)
static ssize_t ssmr_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	int ret = 0;
	int i = 0;

	if (__MAX_NR_SSMR_FEATURES <= 0) {
		ret += sprintf(buf + ret, "no SSMR user enable\n");
		return ret;
	}

	for (; i < __MAX_NR_SSMR_FEATURES; i++) {
		unsigned long region_pa;

		region_pa = (unsigned long)page_to_phys(_ssmr_feats[i].page);

		ret += sprintf(buf + ret, "%s base:0x%lx, size 0x%lx, ",
			   _ssmr_feats[i].feat_name,
			   _ssmr_feats[i].phy_addr == 0 ? 0 :
			(unsigned long)dma_to_phys(ssmr_dev,
				_ssmr_feats[i].phy_addr),
				_ssmr_feats[i].alloc_size);
		ret += sprintf(buf + ret, "alloc_pages %lu, state %s.%s\n",
			   _ssmr_feats[i].alloc_size >> PAGE_SHIFT,
			   ssmr_state_text[_ssmr_feats[i].state],
			   _ssmr_feats[i].use_cache_memory ? " (cache memory)"
							: "");
	}

	ret += sprintf(buf + ret, "[CONFIG]:\n");
	ret += sprintf(buf + ret, "ssmr_upper_limit: 0x%llx\n",
						 ssmr_upper_limit);

	return ret;
}

static ssize_t ssmr_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *cmd, size_t count)
{
	char buf[64];
	int buf_size;
	int feat = 0, ret;

	ret = sscanf(cmd, "%s", buf);
	if (ret) {

		buf_size = min(count - 1, (sizeof(buf) - 1));
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
		pr_info("SSMR: Cannot find module %s object\n",
				KBUILD_MODNAME);
		return -EINVAL;
	}

	return 0;
}
#endif /* end of CONFIG_SYSFS */

static int ssmr_probe(struct platform_device *pdev)
{
	int i;

	pr_info("memory_ssmr driver probe done\n");

	ssmr_dev = &pdev->dev;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	/* setup secure feature size */
	setup_feature_size();

	/* ssmr region init */
	finalize_scenario_size();

	/* check svp statis reserved status */
	get_svp_memory_info();

	for (i = 0; i < __MAX_NR_SSMR_FEATURES; i++) {
		memory_ssmr_init_feature(_ssmr_feats[i].feat_name,
					_ssmr_feats[i].req_size,
					&_ssmr_feats[i],
					_ssmr_feats[i].proc_entry_fops);
	}

	/* ssmr sys file init */
#if IS_ENABLED(CONFIG_SYSFS)
	memory_ssmr_sysfs_init();
#endif

	get_reserved_cma_memory(&pdev->dev);

	return 0;
}

static struct platform_driver ssmr_driver = {
	.probe = ssmr_probe,
	.driver = {
		.name = "driver-ssmr",
		.of_match_table = ssmr_of_match_table,
	},
};
module_platform_driver(ssmr_driver);

MODULE_AUTHOR("James Hsu <james.hsu@medaitek.com>");
MODULE_DESCRIPTION("ssmr driver");
MODULE_LICENSE("GPL v2");

