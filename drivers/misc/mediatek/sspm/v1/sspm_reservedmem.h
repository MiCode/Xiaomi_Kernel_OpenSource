// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __SSPM_RESERVED_H__
#define __SSPM_RESERVED_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "sspm_define.h"

struct sspm_reserve_mblock {
	u32 num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

phys_addr_t sspm_reserve_mem_get_phys(unsigned int id);
phys_addr_t sspm_reserve_mem_get_virt(unsigned int id);
phys_addr_t sspm_reserve_mem_get_size(unsigned int id);
int sspm_reserve_memory_init(struct platform_device *pdev);
void sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size);
void sspm_lock_emi_mpu(void);

extern struct platform_device *sspm_pdev;
phys_addr_t sspm_sbuf_get(unsigned int offset);
int sspm_sbuf_init(void);

#endif
