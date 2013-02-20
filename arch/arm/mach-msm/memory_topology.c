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

#if (defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_ARCH_MSM8930)) \
	&& defined(CONFIG_ENABLE_DMM)
static int rpm_change_memory_state(int retention_mask,
					int active_mask)
{
	int ret;
	struct msm_rpm_iv_pair cmd[2];
	struct msm_rpm_iv_pair status[2];

	cmd[0].id = MSM_RPM_ID_DDR_DMM_0;
	cmd[1].id = MSM_RPM_ID_DDR_DMM_1;

	status[0].id = MSM_RPM_STATUS_ID_DDR_DMM_0;
	status[1].id = MSM_RPM_STATUS_ID_DDR_DMM_1;

	cmd[0].value = retention_mask;
	cmd[1].value = active_mask;

	ret = msm_rpm_set(MSM_RPM_CTX_SET_0, cmd, 2);
	if (ret < 0) {
		pr_err("rpm set failed");
		return -EINVAL;
	}

	ret = msm_rpm_get_status(status, 2);
	if (ret < 0) {
		pr_err("rpm status failed");
		return -EINVAL;
	}
	if (status[0].value == retention_mask &&
		status[1].value == active_mask)
		return 0;
	else {
		pr_err("rpm failed to change memory state");
		return -EINVAL;
	}
}

static int switch_memory_state(int mask, int new_state, int start_region,
				int end_region)
{
	int final_mask = 0;
	int i;

	mutex_lock(&mem_regions_mutex);

	for (i = start_region; i <= end_region; i++) {
		if (new_state == mem_regions[i].state)
			goto no_change;
		/* All region states must be the same to change them */
		if (mem_regions[i].state != mem_regions[start_region].state)
			goto no_change;
	}

	if (new_state == STATE_POWER_DOWN)
		final_mask = mem_regions_mask & mask;
	else if (new_state == STATE_ACTIVE)
		final_mask = mem_regions_mask | ~mask;
	else
		goto no_change;

	pr_info("request memory %d to %d state switch (%d->%d)\n",
		start_region, end_region, mem_regions[start_region].state,
		new_state);
	if (rpm_change_memory_state(final_mask, final_mask) == 0) {
		for (i = start_region; i <= end_region; i++)
			mem_regions[i].state = new_state;
		mem_regions_mask = final_mask;

		pr_info("completed memory %d to %d state switch to %d\n",
			start_region, end_region, new_state);
		mutex_unlock(&mem_regions_mutex);
		return 0;
	}

	pr_err("failed memory %d to %d state switch (%d->%d)\n",
		start_region, end_region, mem_regions[start_region].state,
		new_state);

no_change:
	mutex_unlock(&mem_regions_mutex);
	return -EINVAL;
}
#else

static int switch_memory_state(int mask, int new_state, int start_region,
				int end_region)
{
	return -EINVAL;
}
#endif

/* The hotplug code expects the number of bytes that switched state successfully
 * as the return value, so a return value of zero indicates an error
*/
int soc_change_memory_power(u64 start, u64 size, int change)
{
	int i = 0;
	int mask = default_mask;
	u64 end = start + size;
	int start_region = 0;
	int end_region = 0;

	if (change != STATE_ACTIVE && change != STATE_POWER_DOWN) {
		pr_info("requested state transition invalid\n");
		return 0;
	}
	/* Find the memory regions that fall within the range */
	for (i = 0; i < nr_mem_regions; i++) {
		if (mem_regions[i].start <= start &&
			mem_regions[i].start >=
			mem_regions[start_region].start) {
			start_region = i;
		}
		if (end <= mem_regions[i].start + mem_regions[i].size) {
			end_region = i;
			break;
		}
	}

	/* Set the bitmask for each region in the range */
	for (i = start_region; i <= end_region; i++)
		mask &= ~(0x1 << i);

	if (!switch_memory_state(mask, change, start_region, end_region))
		return size;
	else
		return 0;
}

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
