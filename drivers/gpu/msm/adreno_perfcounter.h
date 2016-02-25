/* Copyright (c) 2008-2015, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_PERFCOUNTER_H
#define __ADRENO_PERFCOUNTER_H

#include "adreno.h"

struct adreno_device;

/* ADRENO_PERFCOUNTERS - Given an adreno device, return the perfcounters list */
#define ADRENO_PERFCOUNTERS(_a) \
	(ADRENO_GPU_DEVICE(_a) ? ADRENO_GPU_DEVICE(_a)->perfcounters : NULL)

#define PERFCOUNTER_FLAG_NONE 0x0
#define PERFCOUNTER_FLAG_KERNEL 0x1

/* Structs to maintain the list of active performance counters */

/**
 * struct adreno_perfcount_register: register state
 * @countable: countable the register holds
 * @kernelcount: number of user space users of the register
 * @usercount: number of kernel users of the register
 * @offset: register hardware offset
 * @load_bit: The bit number in LOAD register which corresponds to this counter
 * @select: The countable register offset
 * @value: The 64 bit countable register value
 */
struct adreno_perfcount_register {
	unsigned int countable;
	unsigned int kernelcount;
	unsigned int usercount;
	unsigned int offset;
	unsigned int offset_hi;
	int load_bit;
	unsigned int select;
	uint64_t value;
};

/**
 * struct adreno_perfcount_group: registers for a hardware group
 * @regs: available registers for this group
 * @reg_count: total registers for this group
 * @name: group name for this group
 */
struct adreno_perfcount_group {
	struct adreno_perfcount_register *regs;
	unsigned int reg_count;
	const char *name;
	unsigned long flags;
};

/*
 * ADRENO_PERFCOUNTER_GROUP_FIXED indicates that a perfcounter group is fixed -
 * instead of having configurable countables like the other groups, registers in
 * fixed groups have a hardwired countable.  So when the user requests a
 * countable in one of these groups, that countable should be used as the
 * register offset to return
 */

#define ADRENO_PERFCOUNTER_GROUP_FIXED BIT(0)

/**
 * adreno_perfcounts: all available perfcounter groups
 * @groups: available groups for this device
 * @group_count: total groups for this device
 */
struct adreno_perfcounters {
	struct adreno_perfcount_group *groups;
	unsigned int group_count;
};

/**
 * adreno_invalid_countabless: Invalid countables that do not work properly
 * @countables: List of unusable countables
 * @num_countables: Number of unusable countables
 */
struct adreno_invalid_countables {
	const unsigned int *countables;
	int num_countables;
};

#define ADRENO_PERFCOUNTER_GROUP_FLAGS(core, offset, name, flags) \
	[KGSL_PERFCOUNTER_GROUP_##offset] = { core##_perfcounters_##name, \
	ARRAY_SIZE(core##_perfcounters_##name), __stringify(name), flags }

#define ADRENO_PERFCOUNTER_GROUP(core, offset, name) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(core, offset, name, 0)

#define ADRENO_POWER_COUNTER_GROUP(core, offset, name) \
	[KGSL_PERFCOUNTER_GROUP_##offset##_PWR] = { core##_pwrcounters_##name, \
	ARRAY_SIZE(core##_pwrcounters_##name), __stringify(name##_pwr), 0}

#define ADRENO_PERFCOUNTER_INVALID_COUNTABLE(name, off) \
	[KGSL_PERFCOUNTER_GROUP_##off] = { name##_invalid_countables, \
				ARRAY_SIZE(name##_invalid_countables) }

int adreno_perfcounter_query_group(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int __user *countables,
	unsigned int count, unsigned int *max_counters);

int adreno_perfcounter_read_group(struct adreno_device *adreno_dev,
	struct kgsl_perfcounter_read_group __user *reads, unsigned int count);

void adreno_perfcounter_close(struct adreno_device *adreno_dev);

void adreno_perfcounter_restore(struct adreno_device *adreno_dev);

void adreno_perfcounter_save(struct adreno_device *adreno_dev);

void adreno_perfcounter_start(struct adreno_device *adreno_dev);

void adreno_perfcounter_init(struct adreno_device *adreno_dev);

int adreno_perfcounter_get_groupid(struct adreno_device *adreno_dev,
					const char *name);

uint64_t adreno_perfcounter_read(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter);

const char *adreno_perfcounter_get_name(struct adreno_device
					*adreno_dev, unsigned int groupid);

int adreno_perfcounter_get(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable, unsigned int *offset,
	unsigned int *offset_hi, unsigned int flags);

int adreno_perfcounter_put(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable, unsigned int flags);

#endif /* __ADRENO_PERFCOUNTER_H */
