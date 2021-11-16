/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include "audio_memory.h"
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>

static struct adsp_reserve_mblock adsp_reserve_mblock[] = {
	{
		.num = ADSP_A_IPI_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x500000,/*5MB*/
	}, {
		.num = ADSP_A_VA_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x200000,/*2MB*/
	},
};

int adsp_init_reserve_memory(phys_addr_t paddr,
			     void __iomem *vaddr,
			     u64 size)
{
	size_t i;
	struct adsp_reserve_mblock *block;
	u64 remain_size = size;
	phys_addr_t vaddr_base = (phys_addr_t)vaddr;

	for (i = 0; i < ARRAY_SIZE(adsp_reserve_mblock); i++) {
		block = &adsp_reserve_mblock[i];

		if (block->size > remain_size) {
			pr_warn("%s mem(%d) init fail remain_size(%llu) < size(%llu)\n",
				__func__, block->num, remain_size, block->size);
			return -ENOMEM;
		}

		block->start_phys = paddr + (size - remain_size);
		block->start_virt = vaddr_base + (size - remain_size);

		remain_size -= block->size;
	}

	return 0;
}

phys_addr_t adsp_get_reserve_mem_phys(enum adsp_reserve_mem_id_t id)
{
	if (id >= ADSP_NUMS_MEM_ID)
		return 0;

	return adsp_reserve_mblock[id].start_phys;
}

phys_addr_t adsp_get_reserve_mem_virt(enum adsp_reserve_mem_id_t id)
{
	if (id >= ADSP_NUMS_MEM_ID)
		return 0;

	return adsp_reserve_mblock[id].start_virt;
}

u64 adsp_get_reserve_mem_size(enum adsp_reserve_mem_id_t id)
{
	if (id >= ADSP_NUMS_MEM_ID)
		return 0;

	return adsp_reserve_mblock[id].size;
}
