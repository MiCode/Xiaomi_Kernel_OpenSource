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

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned int w_pos;
};

struct gpueb_work_struct {
 	struct work_struct work;
 	unsigned int flags;
 	unsigned int id;
};

struct gpueb_reserve_mblock {
	enum gpueb_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

extern struct gpueb_reserve_mblock gpueb_reserve_mblock_ary[];

#endif /* __GPUEB_COMMON_RESERVED_MEM_H__ */