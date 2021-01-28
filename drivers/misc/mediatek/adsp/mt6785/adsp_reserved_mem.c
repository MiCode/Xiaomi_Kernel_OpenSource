// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/io.h>
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#endif
#include "adsp_reserved_mem.h"
#include "adsp_feature_define.h"
#include "adsp_platform.h"
#include "adsp_core.h"

#if ADSP_EMI_PROTECTION_ENABLE
#include <mt_emi_api.h>
#endif

#define ADSP_RESERVE_MEMORY_BLOCK(xname) \
		{.phys_addr = 0x0, .virt_addr = NULL, \
		 .size = 0, .name = xname}

static struct adsp_reserve_mblock adsp_reserve_mem = {0};

static struct adsp_reserve_mblock adsp_reserve_mblocks[] = {
	[ADSP_A_IPI_DMA_MEM_ID]
		= ADSP_RESERVE_MEMORY_BLOCK("adsp-rsv-ipidma-a"),
	[ADSP_A_LOGGER_MEM_ID]
		= ADSP_RESERVE_MEMORY_BLOCK("adsp-rsv-logger-a"),
	[ADSP_A_DEBUG_DUMP_MEM_ID]
		= ADSP_RESERVE_MEMORY_BLOCK("adsp-rsv-dbg-dump-a"),
	[ADSP_A_CORE_DUMP_MEM_ID]
		= ADSP_RESERVE_MEMORY_BLOCK("adsp-rsv-core-dump-a"),
#ifndef CONFIG_FPGA_EARLY_PORTING
	[ADSP_AUDIO_COMMON_MEM_ID]
		= ADSP_RESERVE_MEMORY_BLOCK("adsp-rsv-audio"),
#endif
};

static struct adsp_reserve_mblock *adsp_get_reserve_mblock(
					enum adsp_reserve_mem_id_t id)
{
	void *va_start = adsp_reserve_mblocks[0].virt_addr;

	if (id >= ADSP_NUMS_MEM_ID) {
		pr_info("%s no reserve memory for %d\n", __func__, id);
		return NULL;
	}
	if (!va_start) {
		pr_info("%s va_start is NULL\n", __func__);
		return NULL;
	}

	return &adsp_reserve_mblocks[id];
}

phys_addr_t adsp_get_reserve_mem_phys(enum adsp_reserve_mem_id_t id)
{
	struct adsp_reserve_mblock *mblk = adsp_get_reserve_mblock(id);

	return mblk ? mblk->phys_addr : 0;
}

void *adsp_get_reserve_mem_virt(enum adsp_reserve_mem_id_t id)
{
	struct adsp_reserve_mblock *mblk = adsp_get_reserve_mblock(id);

	return mblk ? mblk->virt_addr : NULL;
}

size_t adsp_get_reserve_mem_size(enum adsp_reserve_mem_id_t id)
{
	struct adsp_reserve_mblock *mblk = adsp_get_reserve_mblock(id);

	return mblk ? mblk->size : 0;
}

void adsp_set_emimpu_shared_region(void)
{
#if ADSP_EMI_PROTECTION_ENABLE
	struct adsp_reserve_mblock *mem = &adsp_reserve_mem;
	struct emi_region_info_t region_info;

	region_info.start = mem->phys_addr;
	region_info.end = mem->phys_addr + mem->size - 0x1;
	region_info.region = MPU_PROCT_REGION_ADSP_SHARED;
	SET_ACCESS_PERMISSION(region_info.apc, UNLOCK,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, NO_PROTECTION, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
	emi_mpu_set_protection(&region_info);
#endif
}

int adsp_mem_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	uint32_t size;
	struct device *dev = &pdev->dev;

	for (i = 0; i < ADSP_NUMS_MEM_ID; i++) {
		ret = of_property_read_u32(dev->of_node,
		      adsp_reserve_mblocks[i].name, &size);
		if (!ret)
			adsp_reserve_mblocks[i].size = (size_t)size;
	}
	return 0;
}

void adsp_init_reserve_memory(void)
{
	enum adsp_reserve_mem_id_t id;
	struct adsp_reserve_mblock *mem = &adsp_reserve_mem;
	size_t acc_size = 0;

	if (!mem->phys_addr || !mem->size) {
		pr_info("%s() reserve memory illegal addr:%llx, size:%zx\n",
			__func__, mem->phys_addr, mem->size);
		return;
	}

	mem->virt_addr = ioremap_wc(mem->phys_addr, mem->size);

	if (!mem->virt_addr) {
		pr_info("%s() ioremap fail\n", __func__);
		return;
	}

	/* assign to each memory block */
	for (id = 0; id < ADSP_NUMS_MEM_ID; id++) {
		adsp_reserve_mblocks[id].phys_addr = mem->phys_addr + acc_size;
		adsp_reserve_mblocks[id].virt_addr = mem->virt_addr + acc_size;
		acc_size += adsp_reserve_mblocks[id].size;
#ifdef MEM_DEBUG
		pr_info("adsp_reserve_mblocks[%d] phys_addr:%llx, size:0x%zx\n",
			id,
			adsp_reserve_mblocks[id].phys_addr,
			adsp_reserve_mblocks[id].size);
#endif
	}

	WARN_ON(acc_size > mem->size);
}

ssize_t adsp_reserve_memory_dump(char *buffer, int size)
{
	int n = 0, i = 0;
	struct adsp_reserve_mblock *mem = &adsp_reserve_mem;

	n += scnprintf(buffer + n, size - n,
		"Reserve-memory-all:0x%llx 0x%p 0x%zx\n",
		mem->phys_addr, mem->virt_addr, mem->size);

	for (i = 0; i < ADSP_NUMS_MEM_ID; i++) {
		mem = &adsp_reserve_mblocks[i];
		n += scnprintf(buffer + n, size - n,
			"Reserve-memory-Block[%02d]:0x%llx 0x%p 0x%zx\n",
			i, mem->phys_addr, mem->virt_addr, mem->size);
	}
	return n;
}

#ifdef CONFIG_OF_RESERVED_MEM
#define ADSP_MEM_RESERVED_KEY "mediatek,reserve-memory-adsp_share"

static int __init adsp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	adsp_reserve_mem.phys_addr = (phys_addr_t) rmem->base;
	adsp_reserve_mem.size = (size_t) rmem->size;

	return 0;
}

RESERVEDMEM_OF_DECLARE(adsp_reserve_mem_init,
		       ADSP_MEM_RESERVED_KEY, adsp_reserve_mem_of_init);
#endif  /* defined(CONFIG_OF_RESERVED_MEM) */

void adsp_update_mpu_memory_info(struct adsp_priv *pdata)
{
	struct adsp_mpu_info_t mpu_info;

	memset(&mpu_info, 0, sizeof(struct adsp_mpu_info_t));
	adsp_copy_from_sharedmem(pdata, ADSP_SHAREDMEM_MPUINFO,
		&mpu_info, sizeof(struct adsp_mpu_info_t));

	mpu_info.share_dram_addr = (u32)adsp_reserve_mem.phys_addr;
	mpu_info.share_dram_size = (u32)adsp_reserve_mem.size;

	pr_info("[ADSP] mpu info=(0x%x, 0x%x)\n",
		 mpu_info.share_dram_addr, mpu_info.share_dram_size);
	adsp_copy_to_sharedmem(pdata, ADSP_SHAREDMEM_MPUINFO,
		&mpu_info, sizeof(struct adsp_mpu_info_t));
}

