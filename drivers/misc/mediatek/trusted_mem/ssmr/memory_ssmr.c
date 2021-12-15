// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[TMEM] ssmr: " fmt

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
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <linux/dma-direct.h>
#include <linux/kallsyms.h>
#include <asm/cacheflush.h>
#include "ssmr_internal.h"

#define SSMR_FEATURES_DT_UNAME "memory-ssmr-features"
#define SVP_FEATURES_DT_UNAME "SecureVideoPath"
#define SVP_ON_MTEE_DT_UNAME "MTEE"
#define SVP_STATIC_RESERVED_DT_UNAME "mediatek,reserve-memory-svp"
#define GRANULARITY_SIZE 0x200000

static struct device *ssmr_dev;

static struct SSMR_HEAP_INFO _ssmr_heap_info[__MAX_NR_SSMR_FEATURES];


#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
static void set_svp_reserve_memory(void)
{
	struct device_node *rmem_node;
	struct reserved_mem *rmem;
	struct SSMR_Feature *feature;
	u64 base, size;

	feature = &_ssmr_feats[SSMR_FEAT_SVP_REGION];

	/* Get reserved memory */
	rmem_node = of_find_compatible_node(NULL, NULL, SVP_STATIC_RESERVED_DT_UNAME);
	if (!rmem_node)
		return;

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		pr_info("svp cannot lookup reserved memory\n");
		return;
	}

	base = rmem->base;
	size = rmem->size;
	pr_info("%s, svp reserved pa base=0x%lx, size=0x%lx\n", __func__, base, size);

	feature->use_cache_memory = true;
	feature->count = size / PAGE_SIZE;
	feature->cache_page = phys_to_page(base);
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

static void setup_feature_size(void)
{

	int i = 0;
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(NULL, SSMR_FEATURES_DT_UNAME);
	if (!dt_node)
		pr_info("%s, failed to find the ssmr device tree\n", __func__);

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
			if ((!strncmp(_ssmr_feats[j].enable, "on", 2))
			    && (_ssmr_feats[j].scheme_flag
				& _ssmrscheme[i].flags)) {
				total_size += _ssmr_feats[j].req_size;
			}
		}
		_ssmrscheme[i].usable_size = total_size;
		pr_info("%s, %s: %pa\n", __func__, _ssmrscheme[i].name,
			&_ssmrscheme[i].usable_size);
	}
}

static int memory_ssmr_init_feature(char *name, u64 size,
				    struct SSMR_Feature *feature,
				    const struct file_operations *entry_fops)
{
	if (size <= 0) {
		feature->state = SSMR_STATE_DISABLED;
		return -1;
	}

	feature->state = SSMR_STATE_ON;
	pr_debug("%s: %s is enable with size: %pa\n", __func__, name, &size);
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

		pr_info("**** %s  (size: %pa)****\n", _ssmrscheme[i].name,
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

	pr_info("cma base=%pa, size=%pa\n", &rmem->base, &rmem->size);

	/*
	 * setup init device with rmem
	 */
	of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);

	return 0;
}

static int memory_region_offline(struct SSMR_Feature *feature, phys_addr_t *pa,
				 unsigned long *size, u64 upper_limit)
{
	int offline_retry = 0;
	struct device_node *np;
	size_t alloc_size;
	struct page *page;

	if (!ssmr_dev) {
		pr_info("%s: feature: %s, No ssmr device\n", __func__, feature->feat_name);
		return -EINVAL;
	}

	if (feature->cache_page) {
		alloc_size = (feature->count) << PAGE_SHIFT;
		page = feature->cache_page;
		if (pa)
			*pa = page_to_phys(page);
		if (size)
			*size = alloc_size;

		feature->phy_addr = page_to_phys(page);
		feature->alloc_size = alloc_size;
		pr_info("%s: feature: %s, [cache memory] pa %pa(%zx) is allocated\n",
			__func__, feature->feat_name, &feature->phy_addr, alloc_size);
		return 0;
	}

	np = of_parse_phandle(ssmr_dev->of_node, "memory-region", 0);

	if (!np) {
		pr_info(" %s: feature: %s, No ssmr region\n", __func__, feature->feat_name);
		return -EINVAL;
	}

	/* Determine alloc size by feature */
	/* s2-map and s2-unamp must be 2MB alignment, so size add 2MB */
	alloc_size = feature->req_size + GRANULARITY_SIZE;

	feature->alloc_size = alloc_size;

	pr_debug("%s %d: upper_limit: %llx, feature{ alloc_size : 0x%lx",
		__func__, __LINE__, upper_limit, alloc_size);

	/*
	 * setup init device with rmem
	 */
	of_reserved_mem_device_init_by_idx(ssmr_dev, ssmr_dev->of_node, 0);

	do {
		feature->virt_addr = dma_alloc_attrs(ssmr_dev, alloc_size,
					     &feature->phy_addr, GFP_KERNEL,
					     DMA_ATTR_NO_KERNEL_MAPPING);

#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
		if (!feature->phy_addr) {
			offline_retry++;
			msleep(100);
		}
#else
		if (feature->phy_addr == U32_MAX) {
			feature->phy_addr = 0;
			offline_retry++;
			msleep(200);
		}
#endif

	} while (!feature->phy_addr && offline_retry < 20);

	if (feature->phy_addr) {
		pr_info("%s: feature: %s, pa=%pad is allocated, retry = %d\n", __func__,
				feature->feat_name, &feature->phy_addr, offline_retry);
	} else {
		pr_info("%s: feature: %s, ssmr offline failed, retry = %d\n", __func__,
				feature->feat_name, offline_retry);
		return -1;
	}

	if (pa) {
		*pa = dma_to_phys(ssmr_dev, feature->phy_addr);

		/* s2-map and s2-unamp must be 2MB alignment */
		if (feature->must_2MB_alignment && (feature->phy_addr % GRANULARITY_SIZE)) {
			/* pa add 1MB, then pa is 2MB alignment */
			*pa = *pa + GRANULARITY_SIZE/2;
			pr_info("%s: feature: %s, adjust 2MB alignment: pa=0x%lx, retry = %d\n",
				__func__, feature->feat_name, *pa, offline_retry);
		}
	}

	if (size)
		*size = feature->req_size;

	/* check pa must be 2MB alignment */
	if (feature->must_2MB_alignment && (*pa % GRANULARITY_SIZE)) {
		pr_info("%s: feature: %s, pa=%pad is not 2MB alignment\n",
				__func__, feature->feat_name, *pa);
		return -1;
	}

	return 0;
}

static int _ssmr_offline_internal(phys_addr_t *pa, unsigned long *size,
				  u64 upper_limit, unsigned int feat)
{
	int retval = 0;
	struct SSMR_Feature *feature = NULL;

	feature = &_ssmr_feats[feat];
	pr_info("%s: START: feature: %s, state: %s\n",
		__func__,
		feat < __MAX_NR_SSMR_FEATURES ? feature->feat_name : "NULL",
		ssmr_state_text[feature->state]);

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
	pr_info("%s: feature: %s, pa: 0x%lx, size: 0x%lx\n", __func__,
		feature->feat_name, *pa, *size);

out:
	pr_info("%s: END: feature: %s, state: %s, retval: %d\n",
		__func__,
		feat < __MAX_NR_SSMR_FEATURES ? _ssmr_feats[feat].feat_name : "NULL",
		ssmr_state_text[feature->state], retval);

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

	if (feature->phy_addr) {
		dma_free_attrs(ssmr_dev, alloc_size, feature->virt_addr,
			       feature->phy_addr, DMA_ATTR_NO_KERNEL_MAPPING);
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
	pr_info("%s: START: feature: %s, state: %s\n", __func__,
		feat < __MAX_NR_SSMR_FEATURES ? _ssmr_feats[feat].feat_name : "NULL",
		ssmr_state_text[feature->state]);

	if (feature->state != SSMR_STATE_OFF) {
		retval = -EBUSY;
		goto out;
	}

	feature->state = SSMR_STATE_ONING_WAIT;
	retval = memory_region_online(feature);
	feature->state = SSMR_STATE_ON;

out:
	pr_info("%s: END: feature: %s, state: %s, retval: %d",
		__func__,
		feat < __MAX_NR_SSMR_FEATURES ? _ssmr_feats[feat].feat_name : "NULL",
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

bool is_page_based_memory(enum TRUSTED_MEM_TYPE mem_type)
{
	return _ssmr_feats[mem_type].is_page_based &&
		   (_ssmr_feats[mem_type].req_size > 0);
}

bool is_svp_on_mtee(void)
{
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(NULL, SVP_ON_MTEE_DT_UNAME);
	if (!dt_node)
		return false;

	return true;
}

bool is_svp_enabled(void)
{
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(NULL, SVP_FEATURES_DT_UNAME);
	if (!dt_node)
		return false;

	return true;
}

int ssmr_init(struct platform_device *pdev)
{
	int i;

	ssmr_dev = &pdev->dev;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	/* setup secure feature size */
	setup_feature_size();

	/* ssmr region init */
	finalize_scenario_size();

#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) ||\
	IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) ||\
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
	/* check svp statis reserved status */
	set_svp_reserve_memory();
#endif

	for (i = 0; i < __MAX_NR_SSMR_FEATURES; i++) {
		memory_ssmr_init_feature(
			_ssmr_feats[i].feat_name, _ssmr_feats[i].req_size,
			&_ssmr_feats[i], _ssmr_feats[i].proc_entry_fops);
	}

	get_reserved_cma_memory(&pdev->dev);

	pr_info("ssmr init done\n");

	return 0;
}
