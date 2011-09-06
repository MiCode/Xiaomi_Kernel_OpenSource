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


#include <asm/setup.h>
#include <asm/errno.h>
#include <asm/sizes.h>
#include <linux/mutex.h>
#include <mach/msm_memtypes.h>
#include "smd_private.h"


static struct mem_region_t {
	u64 start;
	u64 size;
	/* reserved for future use */
	u64 num_partitions;
	int state;
	struct mutex state_mutex;
} mem_regions[MAX_NR_REGIONS];

static unsigned int nr_mem_regions;

/* Return the number of chipselects populated with a memory bank */
/* This is 7x30 only and will be re-implemented in the future */

#if defined(CONFIG_ARCH_MSM7X30)
unsigned int get_num_populated_chipselects()
{
	/* Currently, Linux cannot determine the memory toplogy of a target */
	/* This is a kludge until all this info is figured out from smem */

	/* There is atleast one chipselect populated for hosting the 1st bank */
	unsigned int num_chipselects = 1;
	int i;
	for (i = 0; i < meminfo.nr_banks; i++) {
		struct membank *bank = &meminfo.bank[i];
		if (bank->start == EBI1_PHYS_OFFSET)
			num_chipselects++;
	}
	return num_chipselects;
}
#endif

unsigned int get_num_memory_banks(void)
{
	return nr_mem_regions;
}

unsigned int get_memory_bank_size(unsigned int id)
{
	BUG_ON(id >= nr_mem_regions);
	return mem_regions[id].size;
}

unsigned int get_memory_bank_start(unsigned int id)
{
	BUG_ON(id >= nr_mem_regions);
	return mem_regions[id].start;
}

int __init meminfo_init(unsigned int type, unsigned int min_bank_size)
{
	unsigned int i;
	struct smem_ram_ptable *ram_ptable;
	nr_mem_regions = 0;

	ram_ptable = smem_alloc(SMEM_USABLE_RAM_PARTITION_TABLE,
				sizeof(struct smem_ram_ptable));

	if (!ram_ptable) {
		pr_err("Could not read ram partition table\n");
		return -EINVAL;
	}

	pr_info("meminfo_init: smem ram ptable found: ver: %d len: %d\n",
			ram_ptable->version, ram_ptable->len);

	for (i = 0; i < ram_ptable->len; i++) {
		if (ram_ptable->parts[i].type == type &&
			ram_ptable->parts[i].size >= min_bank_size) {
			mem_regions[nr_mem_regions].start =
				ram_ptable->parts[i].start;
			mem_regions[nr_mem_regions].size =
				ram_ptable->parts[i].size;
			mutex_init(&mem_regions[nr_mem_regions].state_mutex);
			nr_mem_regions++;
		}
	}
	pr_info("Found %d memory banks\n", nr_mem_regions);
	return 0;
}
