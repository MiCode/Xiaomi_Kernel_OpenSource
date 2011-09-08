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
*/

/* The MSM Hardware supports multiple flavors of physical memory.
 * This file captures hardware specific information of these types.
*/

#ifndef __ASM_ARCH_MSM_MEMTYPES_H
#define __ASM_ARCH_MSM_MEMTYPES_H

#include <mach/memory.h>
#include <linux/init.h>

int __init meminfo_init(unsigned int, unsigned int);
/* Redundant check to prevent this from being included outside of 7x30 */
#if defined(CONFIG_ARCH_MSM7X30)
unsigned int get_num_populated_chipselects(void);
#endif

unsigned int get_num_memory_banks(void);
unsigned int get_memory_bank_size(unsigned int);
unsigned int get_memory_bank_start(unsigned int);
int soc_change_memory_power(u64, u64, int);

enum {
	MEMTYPE_NONE = -1,
	MEMTYPE_SMI_KERNEL = 0,
	MEMTYPE_SMI,
	MEMTYPE_EBI0,
	MEMTYPE_EBI1,
	MEMTYPE_MAX,
};

void msm_reserve(void);

#define MEMTYPE_FLAGS_FIXED	0x1
#define MEMTYPE_FLAGS_1M_ALIGN	0x2

struct memtype_reserve {
	unsigned long start;
	unsigned long size;
	unsigned long limit;
	int flags;
};

struct reserve_info {
	struct memtype_reserve *memtype_reserve_table;
	void (*calculate_reserve_sizes)(void);
	int (*paddr_to_memtype)(unsigned int);
	unsigned long low_unstable_address;
	unsigned long max_unstable_size;
	unsigned long bank_size;
};

extern struct reserve_info *reserve_info;
#endif
