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
	int mask;
	struct mutex state_mutex;
} mem_regions[MAX_NR_REGIONS];

static unsigned int nr_mem_regions;

enum {
	STATE_POWER_DOWN = 0x0,
	STATE_ACTIVE = 0x2,
	STATE_DEFAULT = STATE_ACTIVE
};

enum {
	MEM_NO_CHANGE = 0x0,
	MEM_DEEP_POWER_DOWN,
	MEM_SELF_REFRESH,
};

static unsigned int dmm_mode;

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

#if defined(CONFIG_ARCH_MSM8960)
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

static int switch_memory_state(int id, int new_state)
{
	int mask = 0;
	int power_down_masks[MAX_NR_REGIONS] = { 0xFFFFFF00, 0xFFFF00FF,
						0xFF00FFFF, 0x00FFFFFF };
	int self_refresh_masks[MAX_NR_REGIONS] = { 0xFFFFFFF0, 0xFFFFFF0F,
						0xFFFFF0FF, 0xFFFF0FFF };
	mutex_lock(&mem_regions[id].state_mutex);

	if (new_state == mem_regions[id].state)
		goto no_change;

	pr_info("request memory %d state switch (%d->%d) mode %d\n", id,
			mem_regions[id].state, new_state, dmm_mode);
	if (new_state == STATE_POWER_DOWN) {
		if (dmm_mode == MEM_DEEP_POWER_DOWN)
			mask = mem_regions[id].mask & power_down_masks[id];
		else
			mask = mem_regions[id].mask & self_refresh_masks[id];
	} else if (new_state == STATE_ACTIVE) {
		if (dmm_mode == MEM_DEEP_POWER_DOWN)
			mask = mem_regions[id].mask | (~power_down_masks[id]);
		else
			mask = mem_regions[id].mask | (~self_refresh_masks[id]);
	}

	if (rpm_change_memory_state(mask, mask) == 0) {
		mem_regions[id].state = new_state;
		mem_regions[id].mask = mask;
		pr_info("completed memory %d state switch to %d mode %d\n",
				id, new_state, dmm_mode);
		mutex_unlock(&mem_regions[id].state_mutex);
		return 0;
	}

	pr_err("failed memory %d state switch (%d->%d) mode %d\n", id,
			mem_regions[id].state, new_state, dmm_mode);
no_change:
	mutex_unlock(&mem_regions[id].state_mutex);
	return -EINVAL;
}
#else

static int switch_memory_state(int id, int new_state)
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
	int match = 0;

	/* Find the memory region starting just below start */
	for (i = 0; i < nr_mem_regions; i++) {
		if (mem_regions[i].start <= start &&
			mem_regions[i].start >= mem_regions[match].start) {
				match = i;
		}
	}

	if (start + size > mem_regions[match].start + mem_regions[match].size) {
		pr_info("passed size exceeds size of memory bank\n");
		return 0;
	}

	if (change != STATE_ACTIVE && change != STATE_POWER_DOWN) {
		pr_info("requested state transition invalid\n");
		return 0;
	}

	if (!switch_memory_state(match, change))
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
	unsigned int i;
	unsigned long bank_size;
	unsigned long bank_start;
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

	/* Determine power control mode based on the hw version */
	/* This check will be removed when PASR is fully supported */
	if (cpu_is_msm8960() &&
		SOCINFO_VERSION_MAJOR(socinfo_get_version()) < 2)
		dmm_mode = MEM_DEEP_POWER_DOWN;
	else
		dmm_mode = MEM_SELF_REFRESH;

	pr_info("meminfo_init: smem ram ptable found: ver: %d len: %d\n",
			ram_ptable->version, ram_ptable->len);

	for (i = 0; i < ram_ptable->len; i++) {
		if (ram_ptable->parts[i].type == type &&
			ram_ptable->parts[i].size >= min_bank_size) {
			bank_start = ram_ptable->parts[i].start;
			bank_size = ram_ptable->parts[i].size;
			/* Divide into logical memory regions of same size */
			while (bank_size) {
				mem_regions[nr_mem_regions].start =
					bank_start;
				mem_regions[nr_mem_regions].size =
					MIN_MEMORY_BLOCK_SIZE;
				mutex_init(&mem_regions[nr_mem_regions]
							.state_mutex);
				mem_regions[nr_mem_regions].state =
							STATE_DEFAULT;
				mem_regions[nr_mem_regions].mask = default_mask;
				bank_start += MIN_MEMORY_BLOCK_SIZE;
				bank_size -= MIN_MEMORY_BLOCK_SIZE;
				nr_mem_regions++;
			}
			nr_mem_banks++;
		}
	}
	pr_info("Found %d memory banks grouped into %d memory regions\n",
			nr_mem_banks, nr_mem_regions);
	return 0;
}
