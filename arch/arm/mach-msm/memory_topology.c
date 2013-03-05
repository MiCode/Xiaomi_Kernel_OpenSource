/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <asm/pgtable.h>
#include <linux/mutex.h>
#include <linux/memory.h>
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include "smd_private.h"

#if defined(CONFIG_ARCH_MSM8960)
#include "rpm_resources.h"
#endif

static struct mem_region_t {
	u64 start;
	u64 size;
	/* reserved for future use */
	u64 num_partitions;
	int state;
} mem_regions[MAX_NR_REGIONS];

static struct mutex mem_regions_mutex;
static unsigned int nr_mem_regions;
static int mem_regions_mask;

enum {
	STATE_POWER_DOWN = 0x0,
	STATE_ACTIVE = 0x2,
	STATE_DEFAULT = STATE_ACTIVE
};

static int default_mask = ~0x0;

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
	unsigned int i, j;
	unsigned long bank_size;
	unsigned long bank_start;
	unsigned long region_size;
	struct smem_ram_ptable *ram_ptable;
	/* physical memory banks */
	unsigned int nr_mem_banks = 0;
	/* logical memory regions for dmm */
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
		/* A bank is valid only if is greater than min_bank_size. If
		 * non-valid memory (e.g. modem memory) became greater than
		 * min_bank_size, there is currently no way to differentiate.
		 */
		if (ram_ptable->parts[i].type == type &&
			ram_ptable->parts[i].size >= min_bank_size) {
			bank_start = ram_ptable->parts[i].start;
			bank_size = ram_ptable->parts[i].size;
			region_size = bank_size / NR_REGIONS_PER_BANK;

			for (j = 0; j < NR_REGIONS_PER_BANK; j++) {
				mem_regions[nr_mem_regions].start =
					bank_start;
				mem_regions[nr_mem_regions].size =
					region_size;
				mem_regions[nr_mem_regions].state =
							STATE_DEFAULT;
				bank_start += region_size;
				nr_mem_regions++;
			}
			nr_mem_banks++;
		}
	}
	mutex_init(&mem_regions_mutex);
	mem_regions_mask = default_mask;
	pr_info("Found %d memory banks grouped into %d memory regions\n",
			nr_mem_banks, nr_mem_regions);
	return 0;
}
