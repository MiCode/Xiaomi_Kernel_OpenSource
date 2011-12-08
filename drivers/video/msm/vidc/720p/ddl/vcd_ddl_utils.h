/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _VCD_DDL_UTILS_H_
#define _VCD_DDL_UTILS_H_

#include "vcd_ddl_core.h"
#include "vcd_ddl.h"

extern u32 vidc_msg_pmem;
extern u32 vidc_msg_timing;

enum timing_data {
	DEC_OP_TIME,
	DEC_IP_TIME,
	ENC_OP_TIME,
	MAX_TIME_DATA
};

#define DDL_INLINE

#define DDL_ALIGN_SIZE(sz, guard_bytes, align_mask) \
  (((u32)(sz) + guard_bytes) & align_mask)

#define DDL_MALLOC(x)  kmalloc(x, GFP_KERNEL)
#define DDL_FREE(x)   { if ((x)) kfree((x)); (x) = NULL; }

#define DBG_PMEM(x...) \
do { \
	if (vidc_msg_pmem) \
		printk(KERN_DEBUG x); \
} while (0)

void ddl_set_core_start_time(const char *func_name, u32 index);

void ddl_calc_core_proc_time(const char *func_name, u32 index);

void ddl_reset_core_time_variables(u32 index);

#define DDL_ASSERT(x)
#define DDL_MEMSET(src, value, len) memset((src), (value), (len))
#define DDL_MEMCPY(dest, src, len)  memcpy((dest), (src), (len))

#define DDL_ADDR_IS_ALIGNED(addr, align_bytes) \
(!((u32)(addr) & ((align_bytes) - 1)))

#endif
