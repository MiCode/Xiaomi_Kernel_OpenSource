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

#ifndef AUDIO_MEMORY_H
#define AUDIO_MEMORY_H

#include <linux/types.h>
#include <linux/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum adsp_reserve_mem_id_t {
	ADSP_A_IPI_MEM_ID,
	ADSP_A_VA_MEM_ID,
	ADSP_NUMS_MEM_ID,
};

struct adsp_reserve_mblock {
	enum adsp_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

int adsp_init_reserve_memory(phys_addr_t paddr,
			     void __iomem *vaddr,
			     u64 size);
phys_addr_t adsp_get_reserve_mem_phys(enum adsp_reserve_mem_id_t id);
phys_addr_t adsp_get_reserve_mem_virt(enum adsp_reserve_mem_id_t id);
u64 adsp_get_reserve_mem_size(enum adsp_reserve_mem_id_t id);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of AUDIO_MEMORY_H */

