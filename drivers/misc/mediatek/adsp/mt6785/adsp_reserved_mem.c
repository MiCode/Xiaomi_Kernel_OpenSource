/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/io.h>
#include "adsp_reserved_mem.h"
#include "adsp_helper.h"
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#endif

#define ADSP_RESERVE_MEMORY_BLOCK(xsize) {.phys_addr = 0x0, .virt_addr = NULL, \
					  .size = xsize}

static struct adsp_reserve_mblock adsp_reserve_mem = {0};

static struct adsp_reserve_mblock adsp_reserve_mblocks[] = {
#ifdef FPGA_EARLY_DEVELOPMENT
	[ADSP_A_SYSTEM_MEM_ID]      = ADSP_RESERVE_MEMORY_BLOCK(0x40000),
	[ADSP_A_IPI_MEM_ID]         = ADSP_RESERVE_MEMORY_BLOCK(0x80000),
	[ADSP_A_LOGGER_MEM_ID]      = ADSP_RESERVE_MEMORY_BLOCK(0x80000),
	[ADSP_A_DEBUG_DUMP_MEM_ID]  = ADSP_RESERVE_MEMORY_BLOCK(0x80000),
	[ADSP_A_CORE_DUMP_MEM_ID]   = ADSP_RESERVE_MEMORY_BLOCK(0x400),
#else
	[ADSP_A_SYSTEM_MEM_ID]      = ADSP_RESERVE_MEMORY_BLOCK(0x700000),
	[ADSP_A_IPI_MEM_ID]         = ADSP_RESERVE_MEMORY_BLOCK(0x500000),
	[ADSP_A_LOGGER_MEM_ID]      = ADSP_RESERVE_MEMORY_BLOCK(0x200000),
	[ADSP_A_TRAX_MEM_ID]        = ADSP_RESERVE_MEMORY_BLOCK(0x1000),
	[ADSP_SPK_PROTECT_MEM_ID]   = ADSP_RESERVE_MEMORY_BLOCK(0x20000),
	[ADSP_VOIP_MEM_ID]          = ADSP_RESERVE_MEMORY_BLOCK(0x30000),
	[ADSP_A2DP_PLAYBACK_MEM_ID] = ADSP_RESERVE_MEMORY_BLOCK(0x40000),
	[ADSP_OFFLOAD_MEM_ID]       = ADSP_RESERVE_MEMORY_BLOCK(0x400000),
	[ADSP_EFFECT_MEM_ID]        = ADSP_RESERVE_MEMORY_BLOCK(0x60000),
	[ADSP_VOICE_CALL_MEM_ID]    = ADSP_RESERVE_MEMORY_BLOCK(0x60000),
	[ADSP_AFE_MEM_ID]           = ADSP_RESERVE_MEMORY_BLOCK(0x40000),
	[ADSP_PLAYBACK_MEM_ID]      = ADSP_RESERVE_MEMORY_BLOCK(0x30000),
	[ADSP_DEEPBUF_MEM_ID]       = ADSP_RESERVE_MEMORY_BLOCK(0x30000),
	[ADSP_PRIMARY_MEM_ID]       = ADSP_RESERVE_MEMORY_BLOCK(0x30000),
	[ADSP_CAPTURE_UL1_MEM_ID]   = ADSP_RESERVE_MEMORY_BLOCK(0x20000),
	[ADSP_DATAPROVIDER_MEM_ID]  = ADSP_RESERVE_MEMORY_BLOCK(0x30000),
	[ADSP_CALL_FINAL_MEM_ID]    = ADSP_RESERVE_MEMORY_BLOCK(0x30000),
	[ADSP_A_DEBUG_DUMP_MEM_ID]  = ADSP_RESERVE_MEMORY_BLOCK(0x80000),
	[ADSP_A_CORE_DUMP_MEM_ID]   = ADSP_RESERVE_MEMORY_BLOCK(0x400),
#endif
};

static struct adsp_reserve_mblock *adsp_get_reserve_mblock(
					enum adsp_reserve_mem_id_t id)
{
	if (id >= ADSP_NUMS_MEM_ID) {
		pr_info("[ADSP] no reserve memory for %d", id);
		return NULL;
	}

	return &adsp_reserve_mblocks[id];
}

int adsp_set_reserve_mblock(
		enum adsp_reserve_mem_id_t id, phys_addr_t phys_addr,
		void *virt_addr, size_t size)
{
	if (id >= ADSP_NUMS_MEM_ID) {
		pr_info("[ADSP] no reserve memory for %d", id);
		return -1;
	}

	adsp_reserve_mblocks[id].phys_addr = phys_addr;
	adsp_reserve_mblocks[id].virt_addr = virt_addr;
	adsp_reserve_mblocks[id].size = size;

	return 0;
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

void *adsp_reserve_memory_ioremap(phys_addr_t phys_addr, size_t size)
{
	enum adsp_reserve_mem_id_t id;
	struct adsp_reserve_mblock *mem = &adsp_reserve_mem;
	size_t acc_size = 0;

	if (!phys_addr || !size) {
		pr_info("[ADSP] set reserve memory illegal addr:%llu, size:%zu",
			phys_addr, size);
		return NULL;
	}

	mem->phys_addr = phys_addr;
	mem->size = size;
	mem->virt_addr = ioremap_wc(mem->phys_addr, mem->size);

	if (!mem->virt_addr)
		return NULL;

	/* assign to each memory block */
	for (id = ADSP_A_SHARED_MEM_BEGIN; id < ADSP_NUMS_MEM_ID; id++) {
		adsp_reserve_mblocks[id].phys_addr = mem->phys_addr + acc_size;
		adsp_reserve_mblocks[id].virt_addr = mem->virt_addr + acc_size;
		acc_size += adsp_reserve_mblocks[id].size;
	}

	WARN_ON(acc_size > mem->size);

	return mem->virt_addr;
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
	adspreg.sharedram = (phys_addr_t) rmem->base;
	adspreg.shared_size = (size_t) rmem->size;

	return 0;
}

RESERVEDMEM_OF_DECLARE(adsp_reserve_mem_init,
		       ADSP_MEM_RESERVED_KEY, adsp_reserve_mem_of_init);
#endif  /* defined(CONFIG_OF_RESERVED_MEM) */

