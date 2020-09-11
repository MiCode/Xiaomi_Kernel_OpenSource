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

#ifndef __DFD_H__
#define __DFD_H__

#define DFD_SMC_MAGIC_SETUP (0x99716150)
#define DFD_CACHE_DUMP_ENABLE	1
#define DFD_PARITY_ERR_TRIGGER	2

struct dfd_drv {
	u64 base_addr;
	u32 base_addr_msb;
	unsigned long chain_length;
	unsigned long rg_dfd_timeout;
	unsigned int enabled;
	unsigned int cachedump_en;
	unsigned int mem_reserve;
	unsigned int l2c_trigger;
};

extern unsigned int check_dfd_support(void);
extern unsigned int dfd_infra_base(void);
extern unsigned int dfd_ap_addr_offset(void);

#endif
