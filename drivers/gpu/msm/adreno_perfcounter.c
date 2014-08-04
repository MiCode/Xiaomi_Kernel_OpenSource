/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/uaccess.h>

#include "kgsl.h"
#include "adreno.h"

/*
 * values cannot be loaded into physical performance
 * counters belonging to these groups.
 */
static inline int loadable_perfcounter_group(unsigned int groupid)
{
	return ((groupid == KGSL_PERFCOUNTER_GROUP_VBIF_PWR) ||
		(groupid == KGSL_PERFCOUNTER_GROUP_VBIF) ||
		(groupid == KGSL_PERFCOUNTER_GROUP_PWR)) ? 0 : 1;
}

/*
 * Return true if the countable is used and not broken
 */
static inline int active_countable(unsigned int countable)
{
	return ((countable != KGSL_PERFCOUNTER_NOT_USED) &&
		(countable != KGSL_PERFCOUNTER_BROKEN));
}

/**
 * adreno_perfcounter_init: Reserve kernel performance counters
 * @adreno_dev: Pointer to an adreno_device struct
 *
 * The kernel needs/wants a certain group of performance counters for
 * its own activities.  Reserve these performance counters at init time
 * to ensure that they are always reserved for the kernel.  The performance
 * counters used by the kernel can be obtained by the user, but these
 * performance counters will remain active as long as the device is alive.
 */

int adreno_perfcounter_init(struct adreno_device *adreno_dev)
{
	int ret = 0;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (counters == NULL)
		return -EINVAL;

	if (gpudev->perfcounter_init)
		ret = gpudev->perfcounter_init(adreno_dev);

	if (ret)
		return ret;

	if (adreno_dev->fast_hang_detect)
		adreno_fault_detect_start(adreno_dev);

	/* Turn on the GPU busy counter(s) and let them run free */
	/* GPU busy counts */
	ret = adreno_perfcounter_get(adreno_dev, KGSL_PERFCOUNTER_GROUP_PWR, 1,
			NULL, NULL, PERFCOUNTER_FLAG_KERNEL);

	/* Default performance counter profiling to false */
	adreno_dev->profile.enabled = false;
	return ret;
}

/**
 * adreno_perfcounter_write() - Write the physical performance
 * counter values.
 * @adreno_dev -  Adreno device whose registers are to be written to.
 * @group - group to which the physical counter belongs to.
 * @counter - register id of the physical counter to which the value is
 *		written to.
 *
 * This function loads the 64 bit saved value into the particular physical
 * counter by enabling the corresponding bit in A3XX_RBBM_PERFCTR_LOAD_CMD*
 * register.
 */
static void adreno_perfcounter_write(struct adreno_device *adreno_dev,
				unsigned int group, unsigned int counter)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg;
	unsigned int val;

	reg = &(gpudev->perfcounters->groups[group].regs[counter]);

	/* Clear the load cmd registers */
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0, 0);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1, 0);
	if (adreno_is_a4xx(adreno_dev))
		adreno_writereg(adreno_dev,
			ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2, 0);


	/* Write the saved value to PERFCTR_LOAD_VALUE* registers. */
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
			(uint32_t)reg->value);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
			(uint32_t)(reg->value >> 32));

	/*
	 * Set the load bit in PERFCTR_LOAD_CMD for the physical counter
	 * we want to restore. The value in PERFCTR_LOAD_VALUE* is loaded
	 * into the corresponding physical counter.
	 */
	if (reg->load_bit < 32)	{
		val = 1 << reg->load_bit;
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
			val);
	} else if (reg->load_bit < 64) {
		val  = 1 << (reg->load_bit - 32);
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
			val);
	} else if (reg->load_bit >= 64 && adreno_is_a4xx(adreno_dev)) {
		val = 1 << (reg->load_bit - 64);
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
			val);
	}
}

/**
 * adreno_perfcounter_close() - Release counters initialized by
 * adreno_perfcounter_close
 * @adreno_dev: Pointer to an adreno_device struct
 */
void adreno_perfcounter_close(struct adreno_device *adreno_dev)
{
	adreno_perfcounter_put(adreno_dev, KGSL_PERFCOUNTER_GROUP_PWR, 1,
		PERFCOUNTER_FLAG_KERNEL);

	if (adreno_dev->fast_hang_detect)
		adreno_fault_detect_stop(adreno_dev);
}


/**
 * adreno_perfcounter_restore() - Restore performance counters
 * @adreno_dev: adreno device to configure
 *
 * Load the physical performance counters with 64 bit value which are
 * saved on GPU power collapse.
 */
inline void adreno_perfcounter_restore(struct adreno_device *adreno_dev)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_group *group;
	unsigned int regid, groupid;

	if (counters == NULL)
		return;

	for (groupid = 0; groupid < counters->group_count; groupid++) {
		if (!loadable_perfcounter_group(groupid))
			continue;

		group = &(counters->groups[groupid]);

		/* group/counter iterator */
		for (regid = 0; regid < group->reg_count; regid++) {
			if (!active_countable(group->regs[regid].countable))
				continue;

			adreno_perfcounter_write(adreno_dev, groupid, regid);
		}
	}

	/* Clear the load cmd registers */
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0, 0);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1, 0);
	if (adreno_is_a4xx(adreno_dev))
		adreno_writereg(adreno_dev,
			ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2, 0);
}

/**
 * adreno_perfcounter_save() - Save performance counters
 * @adreno_dev: adreno device to configure
 *
 * Save the performance counter values before GPU power collapse.
 * The saved values are restored on restart.
 * This ensures physical counters are coherent across power-collapse.
 */
inline void adreno_perfcounter_save(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_perfcounters *counters = gpudev->perfcounters;
	struct adreno_perfcount_group *group;
	unsigned int regid, groupid;

	if (counters == NULL)
		return;

	for (groupid = 0; groupid < counters->group_count; groupid++) {
		group = &(counters->groups[groupid]);

		/* group/counter iterator */
		for (regid = 0; regid < group->reg_count; regid++) {
			if (!active_countable(group->regs[regid].countable))
				continue;

			/* accumulate values for non-loadable counters */
			if (loadable_perfcounter_group(groupid))
				group->regs[regid].value = 0;

			group->regs[regid].value = group->regs[regid].value +
				gpudev->perfcounter_read(adreno_dev, groupid,
					regid);
		}
	}
}


/**
 * adreno_perfcounter_start: Enable performance counters
 * @adreno_dev: Adreno device to configure
 *
 * Ensure all performance counters are enabled that are allocated.  Since
 * the device was most likely stopped, we can't trust that the counters
 * are still valid so make it so.
 * Returns 0 on success else error code
 */

int adreno_perfcounter_start(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_perfcounters *counters = gpudev->perfcounters;
	struct adreno_perfcount_group *group;
	unsigned int i, j;
	int ret = 0;

	if (NULL == counters)
		return 0;
	/* group id iter */
	for (i = 0; i < counters->group_count; i++) {
		group = &(counters->groups[i]);

		/* countable iter */
		for (j = 0; j < group->reg_count; j++) {
			if (group->regs[j].countable ==
					KGSL_PERFCOUNTER_NOT_USED ||
					group->regs[j].countable ==
					KGSL_PERFCOUNTER_BROKEN)
				continue;

			/*
			 * The GPU has to be idle before calling the perfcounter
			 * enable function, but since this function is called
			 * during start we already know the GPU is idle
			 */
			if (gpudev->perfcounter_enable)
				ret = gpudev->perfcounter_enable(adreno_dev, i,
					j, group->regs[j].countable);
				if (ret)
					goto done;
		}
	}
done:
	return ret;
}

/**
 * adreno_perfcounter_read_group() - Determine which countables are in counters
 * @adreno_dev: Adreno device to configure
 * @reads: List of kgsl_perfcounter_read_groups
 * @count: Length of list
 *
 * Read the performance counters for the groupid/countable pairs and return
 * the 64 bit result for each pair
 */

int adreno_perfcounter_read_group(struct adreno_device *adreno_dev,
	struct kgsl_perfcounter_read_group __user *reads, unsigned int count)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_perfcounters *counters = gpudev->perfcounters;
	struct adreno_perfcount_group *group;
	struct kgsl_perfcounter_read_group *list = NULL;
	unsigned int i, j;
	int ret = 0;

	if (NULL == counters)
		return -EINVAL;

	/* sanity check for later */
	if (!gpudev->perfcounter_read)
		return -EINVAL;

	/* sanity check params passed in */
	if (reads == NULL || count == 0 || count > 100)
		return -EINVAL;

	list = kmalloc(sizeof(struct kgsl_perfcounter_read_group) * count,
			GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	if (copy_from_user(list, reads,
			sizeof(struct kgsl_perfcounter_read_group) * count)) {
		ret = -EFAULT;
		goto done;
	}

	mutex_lock(&device->mutex);
	ret = kgsl_active_count_get(device);
	if (ret) {
		mutex_unlock(&device->mutex);
		goto done;
	}

	/* list iterator */
	for (j = 0; j < count; j++) {

		list[j].value = 0;

		/* Verify that the group ID is within range */
		if (list[j].groupid >= counters->group_count) {
			ret = -EINVAL;
			break;
		}

		group = &(counters->groups[list[j].groupid]);

		/* group/counter iterator */
		for (i = 0; i < group->reg_count; i++) {
			if (group->regs[i].countable == list[j].countable) {
				list[j].value = gpudev->perfcounter_read(
					adreno_dev, list[j].groupid, i);
				break;
			}
		}
	}

	kgsl_active_count_put(device);
	mutex_unlock(&device->mutex);

	/* write the data */
	if (ret == 0)
		if (copy_to_user(reads, list,
			sizeof(struct kgsl_perfcounter_read_group) * count))
			ret = -EFAULT;

done:
	kfree(list);
	return ret;
}

/**
 * adreno_perfcounter_get_groupid() - Get the performance counter ID
 * @adreno_dev: Adreno device
 * @name: Performance counter group name string
 *
 * Get the groupid based on the name and return this ID
 */

int adreno_perfcounter_get_groupid(struct adreno_device *adreno_dev,
					const char *name)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_group *group;
	int i;

	if (name == NULL || counters == NULL)
		return -EINVAL;

	for (i = 0; i < counters->group_count; ++i) {
		group = &(counters->groups[i]);

		/* make sure there is a name for this group */
		if (group->name == NULL)
			continue;

		/* verify name and length */
		if (strlen(name) == strlen(group->name) &&
			strcmp(group->name, name) == 0)
			return i;
	}

	return -EINVAL;
}

/**
 * adreno_perfcounter_get_name() - Get the group name
 * @adreno_dev: Adreno device
 * @groupid: Desired performance counter groupid
 *
 * Get the name based on the groupid and return it
 */

const char *adreno_perfcounter_get_name(struct adreno_device *adreno_dev,
		unsigned int groupid)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);

	if (counters != NULL && groupid < counters->group_count)
		return counters->groups[groupid].name;

	return NULL;
}

/**
 * adreno_perfcounter_query_group: Determine which countables are in counters
 * @adreno_dev: Adreno device to configure
 * @groupid: Desired performance counter group
 * @countables: Return list of all countables in the groups counters
 * @count: Max length of the array
 * @max_counters: max counters for the groupid
 *
 * Query the current state of counters for the group.
 */

int adreno_perfcounter_query_group(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int __user *countables,
	unsigned int count, unsigned int *max_counters)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_group *group;
	unsigned int i, t;
	int ret = 0;
	unsigned int *buf;

	*max_counters = 0;

	if (counters == NULL || groupid >= counters->group_count)
		return -EINVAL;

	mutex_lock(&device->mutex);

	group = &(counters->groups[groupid]);
	*max_counters = group->reg_count;

	/*
	 * if NULL countable or *count of zero, return max reg_count in
	 * *max_counters and return success
	 */
	if (countables == NULL || count == 0) {
		mutex_unlock(&device->mutex);
		return 0;
	}

	t = min_t(int, group->reg_count, count);

	buf = kmalloc(t * sizeof(unsigned int), GFP_KERNEL);
	if (buf == NULL) {
		mutex_unlock(&device->mutex);
		return -ENOMEM;
	}

	for (i = 0; i < t; i++)
		buf[i] = group->regs[i].countable;

	mutex_unlock(&device->mutex);

	if (copy_to_user(countables, buf, sizeof(unsigned int) * t))
		ret = -EFAULT;

	kfree(buf);

	return ret;
}

static inline void refcount_group(struct adreno_perfcount_group *group,
	unsigned int reg, unsigned int flags,
	unsigned int *lo, unsigned int *hi)
{
	if (flags & PERFCOUNTER_FLAG_KERNEL)
		group->regs[reg].kernelcount++;
	else
		group->regs[reg].usercount++;

	if (lo)
		*lo = group->regs[reg].offset;

	if (hi)
		*hi = group->regs[reg].offset_hi;
}

/**
 * adreno_perfcounter_get: Try to put a countable in an available counter
 * @adreno_dev: Adreno device to configure
 * @groupid: Desired performance counter group
 * @countable: Countable desired to be in a counter
 * @offset: Return offset of the LO counter assigned
 * @offset_hi: Return offset of the HI counter assigned
 * @flags: Used to setup kernel perf counters
 *
 * Try to place a countable in an available counter.  If the countable is
 * already in a counter, reference count the counter/countable pair resource
 * and return success
 */

int adreno_perfcounter_get(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable, unsigned int *offset,
	unsigned int *offset_hi, unsigned int flags)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_perfcounters *counters = gpudev->perfcounters;
	struct adreno_perfcount_group *group;
	unsigned int empty = -1;
	int ret = 0;

	/* always clear return variables */
	if (offset)
		*offset = 0;
	if (offset_hi)
		*offset_hi = 0;

	if (NULL == counters)
		return -EINVAL;

	if (groupid >= counters->group_count)
		return -EINVAL;

	group = &(counters->groups[groupid]);

	if (group->flags & ADRENO_PERFCOUNTER_GROUP_FIXED) {
		/*
		 * In fixed groups the countable equals the fixed register the
		 * user wants. First make sure it is in range
		 */

		if (countable >= group->reg_count)
			return -EINVAL;

		/* If it is already reserved, just increase the refcounts */
		if ((group->regs[countable].kernelcount != 0) ||
			(group->regs[countable].usercount != 0)) {
				refcount_group(group, countable, flags,
					offset, offset_hi);
				return 0;
		}

		empty = countable;
	} else {
		unsigned int i;

		/*
		 * Check if the countable is already associated with a counter.
		 * Refcount and return the offset, otherwise, try and find an
		 * empty counter and assign the countable to it.
		 */

		for (i = 0; i < group->reg_count; i++) {
			if (group->regs[i].countable == countable) {
				refcount_group(group, i, flags,
					offset, offset_hi);
				return 0;
			} else if (group->regs[i].countable ==
			KGSL_PERFCOUNTER_NOT_USED) {
				/* keep track of unused counter */
				empty = i;
			}
		}
	}

	/* no available counters, so do nothing else */
	if (empty == -1)
		return -EBUSY;

	/* enable the new counter */
	ret = gpudev->perfcounter_enable(adreno_dev, groupid, empty, countable);
	if (ret)
		return ret;
	/* initialize the new counter */
	group->regs[empty].countable = countable;

	/* set initial kernel and user count */
	if (flags & PERFCOUNTER_FLAG_KERNEL) {
		group->regs[empty].kernelcount = 1;
		group->regs[empty].usercount = 0;
	} else {
		group->regs[empty].kernelcount = 0;
		group->regs[empty].usercount = 1;
	}

	if (offset)
		*offset = group->regs[empty].offset;
	if (offset_hi)
		*offset_hi = group->regs[empty].offset_hi;

	return ret;
}


/**
 * adreno_perfcounter_put: Release a countable from counter resource
 * @adreno_dev: Adreno device to configure
 * @groupid: Desired performance counter group
 * @countable: Countable desired to be freed from a  counter
 * @flags: Flag to determine if kernel or user space request
 *
 * Put a performance counter/countable pair that was previously received.  If
 * noone else is using the countable, free up the counter for others.
 */
int adreno_perfcounter_put(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable, unsigned int flags)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_group *group;
	unsigned int i;

	if (counters == NULL || groupid >= counters->group_count)
		return -EINVAL;

	group = &(counters->groups[groupid]);

	/*
	 * Find if the counter/countable pair is used currently.
	 * Start cycling through registers in the bank.
	 */
	for (i = 0; i < group->reg_count; i++) {
		/* check if countable assigned is what we are looking for */
		if (group->regs[i].countable == countable) {
			/* found pair, book keep count based on request type */
			if (flags & PERFCOUNTER_FLAG_KERNEL &&
					group->regs[i].kernelcount > 0)
				group->regs[i].kernelcount--;
			else if (group->regs[i].usercount > 0)
				group->regs[i].usercount--;
			else
				break;

			/* mark available if not used anymore */
			if (group->regs[i].kernelcount == 0 &&
					group->regs[i].usercount == 0)
				group->regs[i].countable =
					KGSL_PERFCOUNTER_NOT_USED;

			return 0;
		}
	}

	return -EINVAL;
}


