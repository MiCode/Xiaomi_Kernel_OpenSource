// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <mt-plat/mtk_ccci_common.h>

#include "ccci_debug.h"
#include "ccci_dpmaif_resv_mem.h"


static phys_addr_t   g_resv_cache_phy_addr;
static void         *g_resv_cache_vir_addr;
static unsigned int  g_resv_cache_mem_size;
static unsigned int  g_resv_cache_mem_offs;

static phys_addr_t   g_resv_nocache_phy_addr;
static void         *g_resv_nocache_vir_addr;
static unsigned int  g_resv_nocache_mem_size;
static unsigned int  g_resv_nocache_mem_offs;

static unsigned int  g_use_cache_mem_from_dts;


#define USE_RESV_CACHE_MEM     0x1
#define USE_RESV_NOCACHE_MEM   0x2

#define TAG "resv"

void ccci_dpmaif_resv_mem_init(void)
{
	struct device_node  *rmem_node = NULL;
	struct reserved_mem *rmem = NULL;

	g_use_cache_mem_from_dts = 0;

	/* for cacheable memory  */
	rmem_node = of_find_compatible_node(NULL, NULL, "mediatek,dpmaif-resv-cache-mem");
	if (!rmem_node) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: no node for reserved cache memory.\n", __func__);
		return;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: cannot lookup reserved cache memory.\n", __func__);
		return;
	}

	g_resv_cache_phy_addr = rmem->base;
	g_resv_cache_mem_size = rmem->size;
	g_resv_cache_mem_offs = 0;

	g_resv_cache_vir_addr = memremap(g_resv_cache_phy_addr, g_resv_cache_mem_size,
										MEMREMAP_WB);
	if (!g_resv_cache_vir_addr) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: g_resv_cache_vir_addr is NULL.\n", __func__);
		return;
	}

	memset(g_resv_cache_vir_addr, 0, g_resv_cache_mem_size);

	g_use_cache_mem_from_dts |= USE_RESV_CACHE_MEM;

	/* for no cacheable memory  */
	rmem_node = of_find_compatible_node(NULL, NULL, "mediatek,dpmaif-resv-nocache-mem");
	if (!rmem_node) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: no node for reserved nocache memory.\n", __func__);
		return;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: cannot lookup reserved nocache memory.\n", __func__);
		return;
	}

	g_resv_nocache_phy_addr = rmem->base;
	g_resv_nocache_mem_size = rmem->size;
	g_resv_nocache_mem_offs = 0;

	g_resv_nocache_vir_addr = ioremap_wc(g_resv_nocache_phy_addr, g_resv_nocache_mem_size);
	if (!g_resv_nocache_vir_addr) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: g_resv_nocache_vir_addr is NULL.\n", __func__);
		return;
	}

	memset_io(g_resv_nocache_vir_addr, 0, g_resv_nocache_mem_size);

	g_use_cache_mem_from_dts |= USE_RESV_NOCACHE_MEM;

	CCCI_NORMAL_LOG(0, TAG,
		"[%s] cache_mem: 0x%llX/0x%llX/%u; nocache_mem: 0x%llX/0x%llX/%u\n",
		__func__, (unsigned long long)g_resv_cache_phy_addr,
		(unsigned long long)g_resv_cache_vir_addr, g_resv_cache_mem_size,
		(unsigned long long)g_resv_nocache_phy_addr,
		(unsigned long long)g_resv_nocache_vir_addr, g_resv_nocache_mem_size);
}

int ccci_dpmaif_get_resv_cache_mem(void **vir_base,
		dma_addr_t *phy_base, unsigned int size)
{
	if ((g_use_cache_mem_from_dts & USE_RESV_CACHE_MEM) != USE_RESV_CACHE_MEM)
		return -1;

	if (g_resv_cache_mem_offs + size > g_resv_cache_mem_size) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: reserved cache mem is too small: %u+%u>%u\n",
			__func__, size, g_resv_cache_mem_offs, g_resv_cache_mem_size);
		return -1;
	}

	if (vir_base)
		(*vir_base) = g_resv_cache_vir_addr + g_resv_cache_mem_offs;
	if (phy_base)
		(*phy_base) = g_resv_cache_phy_addr + g_resv_cache_mem_offs;

	g_resv_cache_mem_offs += size;

	CCCI_NORMAL_LOG(-1, TAG,
			"[%s] size: %u; offs: %u; phy: 0x%llX; vir: 0x%llX\n",
			__func__, size, g_resv_cache_mem_offs,
			phy_base ? (unsigned long long)(*phy_base) : 0,
			vir_base ? (unsigned long long)(*vir_base) : 0);

	return 0;
}

int ccci_dpmaif_get_resv_nocache_mem(void **vir_base,
		dma_addr_t *phy_base, unsigned int size)
{
	if ((g_use_cache_mem_from_dts & USE_RESV_NOCACHE_MEM) != USE_RESV_NOCACHE_MEM)
		return -1;

	if (g_resv_nocache_mem_offs + size > g_resv_nocache_mem_size) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: reserved nocache mem is too small: %u+%u>%u\n",
			__func__, size, g_resv_nocache_mem_offs, g_resv_nocache_mem_size);
		return -1;
	}

	if (vir_base)
		(*vir_base) = g_resv_nocache_vir_addr + g_resv_nocache_mem_offs;
	if (phy_base)
		(*phy_base) = g_resv_nocache_phy_addr + g_resv_nocache_mem_offs;

	g_resv_nocache_mem_offs += size;

	CCCI_NORMAL_LOG(-1, TAG,
			"[%s] size: %u; offs: %u; phy: 0x%llX; vir: 0x%llX\n",
			__func__, size, g_resv_nocache_mem_offs,
			phy_base ? (unsigned long long)(*phy_base) : 0,
			vir_base ? (unsigned long long)(*vir_base) : 0);

	return 0;
}
