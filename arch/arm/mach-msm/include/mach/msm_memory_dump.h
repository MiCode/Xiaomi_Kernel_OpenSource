/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_MEMORY_DUMP_H
#define __MSM_MEMORY_DUMP_H

#include <linux/types.h>

enum dump_client_type {
	MSM_CPU_CTXT = 0,
	MSM_CACHE,
	MSM_OCMEM,
	MSM_TMC_ETFETB,
	MSM_ETM0_REG,
	MSM_ETM1_REG,
	MSM_ETM2_REG,
	MSM_ETM3_REG,
	MSM_TMC0_REG, /* TMC_ETR */
	MSM_TMC1_REG, /* TMC_ETF */
	MAX_NUM_CLIENTS,
};

struct msm_client_dump {
	enum dump_client_type id;
	unsigned long start_addr;
	unsigned long end_addr;
};

struct msm_dump_table {
	u32 version;
	u32 num_entries;
	struct msm_client_dump client_entries[MAX_NUM_CLIENTS];
};

struct msm_memory_dump {
	unsigned long dump_table_phys;
	struct msm_dump_table *dump_table_ptr;
};

#define TABLE_MAJOR(val)	(val >> 20)
#define TABLE_MINOR(val)	(val & 0xFFFFF)
#define MK_TABLE(ma, mi)	((ma << 20) | mi)

#ifndef CONFIG_MSM_MEMORY_DUMP
static inline int msm_dump_table_register(struct msm_client_dump *entry)
{
	return -EIO;
}
#else
int msm_dump_table_register(struct msm_client_dump *client_entry);
#endif
#endif
