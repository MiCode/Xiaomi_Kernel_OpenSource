/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#ifndef _MACH_MSM_CACHE_DUMP_
#define _MACH_MSM_CACHE_DUMP_

#include <asm-generic/sizes.h>


struct l2_cache_line_dump {
	unsigned int l2dcrtr0_val;
	unsigned int l2dcrtr1_val;
	unsigned int cache_line_data[32];
	unsigned int ddr_data[32];
} __packed;

struct l2_cache_dump {
	unsigned int magic_number;
	unsigned int version;
	unsigned int tag_size;
	unsigned int line_size;
	unsigned int total_lines;
	struct l2_cache_line_dump cache[8*1024];
	unsigned int l2esr;
} __packed;


struct l1_cache_dump {
	unsigned int magic;
	unsigned int version;
	unsigned int flags;
	unsigned int cpu_count;
	unsigned int i_tag_size;
	unsigned int i_line_size;
	unsigned int i_num_sets;
	unsigned int i_num_ways;
	unsigned int d_tag_size;
	unsigned int d_line_size;
	unsigned int d_num_sets;
	unsigned int d_num_ways;
	unsigned int spare[32];
	unsigned int lines[];
} __packed;


struct msm_cache_dump_platform_data {
	unsigned int l1_size;
	unsigned int l2_size;
};

#define L1_BUFFER_SIZE	SZ_1M
#define L2_BUFFER_SIZE	(sizeof(struct l2_cache_dump))

#define CACHE_BUFFER_DUMP_SIZE (L1_BUFFER_SIZE + L2_BUFFER_SIZE)

#define L1C_SERVICE_ID 3
#define L1C_BUFFER_SET_COMMAND_ID 4
#define L1C_BUFFER_GET_SIZE_COMMAND_ID	6

#endif
