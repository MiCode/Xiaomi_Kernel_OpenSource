/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_RESERVEDMEM_DEFINE_H__
#define __ADSP_RESERVEDMEM_DEFINE_H__

#include <linux/platform_device.h>

//#define MEM_DEBUG

/* emi mpu define */
#define MPU_PROCT_REGION_ADSP_SHARED      30
#define MPU_PROCT_D0_AP                   0
#define MPU_PROCT_D10_ADSP                10

/* adsp mpu alignment = 4KB */
#define RSV_BLOCK_ALIGN                   0x1000

struct adsp_reserve_mblock {
	phys_addr_t phys_addr;
	void *virt_addr;
	size_t size;
	char *name;
};

struct adsp_mpu_info_t {
	u32 share_dram_addr;
	u32 share_dram_size;
};

struct adsp_priv;

/* Reserved Memory Method */
int adsp_mem_device_probe(struct platform_device *pdev);
ssize_t adsp_reserve_memory_dump(char *buffer, int size);

void adsp_update_mpu_memory_info(struct adsp_priv *pdata);

#endif /* __ADSP_RESERVEDMEM_DEFINE_H__ */
