/* Copyright (c) 2012, 2014, The Linux Foundation. All rights reserved.
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
	MSM_L1_CACHE,
	MSM_L2_CACHE,
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

#ifdef CONFIG_MSM_MEMORY_DUMP
extern int msm_dump_tbl_register(struct msm_client_dump *client_entry);
#else
static inline int msm_dump_tbl_register(struct msm_client_dump *entry)
{
	return -EIO;
}
#endif


#define MSM_DUMP_MAKE_VERSION(ma, mi)	((ma << 20) | mi)
#define MSM_DUMP_MAJOR(val)		(val >> 20)
#define MSM_DUMP_MINOR(val)		(val & 0xFFFFF)


#endif
