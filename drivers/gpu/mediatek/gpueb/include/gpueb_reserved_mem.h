// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_RESERVED_MEM_H__
#define __GPUEB_RESERVED_MEM_H__

#define DRAM_BUF_LEN    (1 * 1024 * 1024)
#define GPUEB_MEM_RESERVED_KEY "mediatek,reserve-memory-gpueb_share"
#define MEMORY_TBL_ELEM_NUM (2) // Two column

phys_addr_t gpueb_get_reserve_mem_phys(unsigned int id);
phys_addr_t gpueb_get_reserve_mem_virt(unsigned int id);
phys_addr_t gpueb_get_reserve_mem_size(unsigned int id);
phys_addr_t gpueb_get_reserve_mem_phys_by_name(char *mem_id_name);
phys_addr_t gpueb_get_reserve_mem_virt_by_name(char *mem_id_name);
phys_addr_t gpueb_get_reserve_mem_size_by_name(char *mem_id_name);
int gpueb_reserved_mem_init(struct platform_device *pdev);

struct gpueb_reserve_mblock {
	unsigned int num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

extern struct gpueb_reserve_mblock *gpueb_reserve_mblock_ary;

#endif /* __GPUEB_RESERVED_MEM_H__ */