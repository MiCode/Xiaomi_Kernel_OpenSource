/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __SSPM_RESERVED_H__
#define __SSPM_RESERVED_H__

#include <linux/types.h>
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
int sspm_reserve_memory_init(void);
void sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size);
void sspm_lock_emi_mpu(void);

#ifdef SSPM_SHARE_BUFFER_SUPPORT
extern struct platform_device *sspm_pdev;
phys_addr_t sspm_sbuf_get(unsigned int offset);
int sspm_sbuf_init(void);
#endif /* SSPM_SHARE_BUFFER_SUPPORT */

#endif
