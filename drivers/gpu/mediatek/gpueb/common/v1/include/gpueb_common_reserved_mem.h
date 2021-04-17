// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_COMMON_RESERVED_MEM_H__
#define __GPUEB_COMMON_RESERVED_MEM_H__

#define DRAM_BUF_LEN    (1 * 1024 * 1024)
#define GPUEB_MEM_RESERVED_KEY "mediatek,reserve-memory-gpueb_share"
#define MEMORY_TBL_ELEM_NUM (2) // Two column 

enum gpueb_reserve_mem_id_t {
	GPUEB_LOGGER_MEM_ID,
	NUMS_MEM_ID,
};

phys_addr_t gpueb_common_get_reserve_mem_phys(enum gpueb_reserve_mem_id_t id);
phys_addr_t gpueb_common_get_reserve_mem_virt(enum gpueb_reserve_mem_id_t id);
phys_addr_t gpueb_common_get_reserve_mem_size(enum gpueb_reserve_mem_id_t id);
int gpueb_common_reserved_mem_init(struct platform_device *pdev);

struct gpueb_reserve_mblock {
	enum gpueb_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

#endif /* __GPUEB_COMMON_RESERVED_MEM_H__ */