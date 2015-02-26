/* Copyright (c) 2002,2007-2015, The Linux Foundation. All rights reserved.
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
#include "adreno_perfcounter.h"
#include "adreno_pm4types.h"
#include "a5xx_reg.h"

/* Bit flag for RBMM_PERFCTR_CTL */
#define RBBM_PERFCTR_CTL_ENABLE		0x00000001

#define VBIF2_PERF_CNT_SEL_MASK 0x7F
/* offset of clear register from select register */
#define VBIF2_PERF_CLR_REG_SEL_OFF 8
/* offset of enable register from select register */
#define VBIF2_PERF_EN_REG_SEL_OFF 16
/* offset of high counter from low counter value */
#define VBIF2_PERF_HIGH_REG_LOW_OFF 8

/* offset of clear register from the enable register */
#define VBIF2_PERF_PWR_CLR_REG_EN_OFF 8
/* offset of high counter from low counter value */
#define VBIF2_PERF_PWR_HIGH_REG_LOW_OFF 8

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
void adreno_perfcounter_init(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->perfcounter_init)
		gpudev->perfcounter_init(adreno_dev);
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
	adreno_writereg64(adreno_dev, ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
			  ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI, reg->value);

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
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->perfcounter_close)
		gpudev->perfcounter_close(adreno_dev);
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
				adreno_perfcounter_read(adreno_dev, groupid,
								regid);
		}
	}
}

static int adreno_perfcounter_enable(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter, unsigned int countable);

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
			ret = adreno_perfcounter_enable(adreno_dev, i, j,
					  group->regs[j].countable);
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
				list[j].value = adreno_perfcounter_read(
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
	ret = adreno_perfcounter_enable(adreno_dev, groupid, empty, countable);
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

static int _perfcounter_enable_pwr(struct adreno_device *adreno_dev,
	unsigned int counter)
{
	if (counter > 1)
		return -EINVAL;

	/* PWR counters enabled by default on A3XX/A4XX so nothing to do */
	if (adreno_is_a3xx(adreno_dev) || adreno_is_a4xx(adreno_dev))
		return 0;

	/*
	 * On 5XX we have to emulate the PWR counters which are physically
	 * missing. Program countable 6 on RBBM_PERFCTR_RBBM_0 as a substitute
	 * for PWR:1. Don't emulate PWR:0 as nobody uses it and we don't want
	 * to take away too many of the generic RBBM counters.
	 */

	if (counter == 0)
		return -EINVAL;

	kgsl_regwrite(&adreno_dev->dev, A5XX_RBBM_PERFCTR_RBBM_SEL_0, 6);

	return 0;
}

static int _perfcounter_enable_vbif2(struct adreno_device *adreno_dev,
					 unsigned int counter,
					 unsigned int countable)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;

	if (counters == NULL ||
		(counter > 3 || countable > VBIF2_PERF_CNT_SEL_MASK))
		return -EINVAL;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs[counter];
	/* Write 1, followed by 0 to CLR register for clearing the counter */
	kgsl_regwrite(device, reg->select - VBIF2_PERF_CLR_REG_SEL_OFF, 1);
	kgsl_regwrite(device, reg->select - VBIF2_PERF_CLR_REG_SEL_OFF, 0);
	kgsl_regwrite(device, reg->select, countable & VBIF2_PERF_CNT_SEL_MASK);
	/* enable reg is 8 DWORDS before select reg */
	kgsl_regwrite(device, reg->select - VBIF2_PERF_EN_REG_SEL_OFF, 1);
	counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs[counter].value = 0;
	return 0;

}

static int _perfcounter_enable_vbif2_pwr(struct adreno_device *adreno_dev,
					     unsigned int counter)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;
	int val = 2;

	if (adreno_is_a4xx(adreno_dev))
		val =  3;

	if (counters == NULL || counter > val)
		return -EINVAL;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR].regs[counter];
	/* Write 1, followed by 0 to CLR register for clearing the counter */
	kgsl_regwrite(device, reg->select + VBIF2_PERF_PWR_CLR_REG_EN_OFF, 1);
	kgsl_regwrite(device, reg->select + VBIF2_PERF_PWR_CLR_REG_EN_OFF, 0);
	kgsl_regwrite(device, reg->select, 1);
	counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR]
		.regs[counter].value = 0;
	return 0;
}

/*
 * adreno_perfcounter_enable - Configure a performance counter for a countable
 * @adreno_dev -  Adreno device to configure
 * @group - Desired performance counter group
 * @counter - Desired performance counter in the group
 * @countable - Desired countable
 *
 * Function is used for adreno cores
 * Physically set up a counter within a group with the desired countable
 * Return 0 on success else error code
 */

static int adreno_perfcounter_enable(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter, unsigned int countable)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;
	int i;
	int ret = 0;

	/* Special cases */
	if (group == KGSL_PERFCOUNTER_GROUP_ALWAYSON)
		/* alwayson counter is global, so init value is 0 */
		return 0;

	if (group == KGSL_PERFCOUNTER_GROUP_PWR)
		return _perfcounter_enable_pwr(adreno_dev, counter);

	if (group == KGSL_PERFCOUNTER_GROUP_VBIF)
		return _perfcounter_enable_vbif2(adreno_dev, counter,
					countable);

	if (group == KGSL_PERFCOUNTER_GROUP_VBIF_PWR)
		return _perfcounter_enable_vbif2_pwr(adreno_dev, counter);

	if (counters == NULL || group >= counters->group_count)
		return -EINVAL;

	if ((0 == counters->groups[group].reg_count) ||
		(counter >= counters->groups[group].reg_count))
		return -EINVAL;

	/*
	 * check whether the countable is valid or not by matching it against
	 * the list on invalid countables
	 */
	if (gpudev->invalid_countables) {
		struct adreno_invalid_countables invalid_countable =
			gpudev->invalid_countables[group];
		for (i = 0; i < invalid_countable.num_countables; i++)
			if (countable == invalid_countable.countables[i])
				return -EACCES;
	}
	reg = &(counters->groups[group].regs[counter]);

	if (test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv)) {
		struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[0];
		unsigned int buf[4];
		unsigned int *cmds = buf;
		int ret;

		cmds += cp_wait_for_idle(adreno_dev, cmds);
		*cmds++ = cp_register(adreno_dev, reg->select, 1);
		*cmds++ = countable;
		/* submit to highest priority RB always */
		ret = adreno_ringbuffer_issuecmds(rb, 0, buf, cmds-buf);
		if (ret)
			goto done;
		/*
		 * schedule dispatcher to make sure rb[0] is run, because
		 * if the current RB is not rb[0] and gpu is idle then
		 * rb[0] will not get scheduled to run
		 */
		if (adreno_dev->cur_rb != rb)
			adreno_dispatcher_schedule(rb->device);
		/* wait for the above commands submitted to complete */
		ret = adreno_ringbuffer_waittimestamp(rb, rb->timestamp,
				ADRENO_IDLE_TIMEOUT);
		if (ret)
			KGSL_DRV_ERR(rb->device,
			"Perfcounter %u/%u/%u start via commands failed %d\n",
			group, counter, countable, ret);
	} else {
		/* Select the desired perfcounter */
		kgsl_regwrite(&adreno_dev->dev, reg->select, countable);
	}
done:
	if (!ret)
		counters->groups[group].regs[counter].value = 0;
	return 0;
}

/*
 * perfcounter_read_alwayson() - Read alwayson counter value
 * @adreno_dev: Device on which counter is running
 * @counter: The counter to read in alwayson counter group
 *
 * Function is used for reading adreno alwayson counter
 * Returns the counter value on success else 0
 */
uint64_t perfcounter_read_alwayson(struct adreno_device *adreno_dev)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	uint64_t val = 0;

	if (adreno_is_a3xx(adreno_dev))
		return 0;

	adreno_readreg64(adreno_dev, ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
				   ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI, &val);
	if (counters)
		val +=
		counters->groups[KGSL_PERFCOUNTER_GROUP_ALWAYSON].regs[0].value;

	return val;
}

/*
 * _perfcounter_read_pwr() - Read power counter value
 * @adreno_dev: Device onwhich counter is running
 * @counter: The counter to read in power counter group
 *
 * Function is used for reading adreno power counter
 * Returns the counter value on success else 0
 */
static uint64_t _perfcounter_read_pwr(struct adreno_device *adreno_dev,
				unsigned int counter)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;
	unsigned int in = 0, out, lo = 0, hi = 0;
	unsigned int enable_bit;

	if (counters == NULL || counter > 1)
		return 0;

	/* Remember, counter 0 is not emulated on 5XX */
	if (adreno_is_a5xx(adreno_dev) && (counter == 0))
		return -EINVAL;

	if (adreno_is_a3xx(adreno_dev)) {
		/* On A3XX we need to freeze the counter so we can read it */
		if (0 == counter)
			enable_bit = 0x00010000;
		else
			enable_bit = 0x00020000;

		/* freeze counter */
		adreno_readreg(adreno_dev, ADRENO_REG_RBBM_RBBM_CTL, &in);
		out = (in & ~enable_bit);
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_RBBM_CTL, out);
	}

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_PWR].regs[counter];
	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* restore the counter control value */
	if (adreno_is_a3xx(adreno_dev))
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_RBBM_CTL, in);

	return ((((uint64_t) hi) << 32) | lo)
		+ counters->groups[KGSL_PERFCOUNTER_GROUP_PWR]
				.regs[counter].value;
}

static uint64_t _perfcounter_read_vbif2(struct adreno_device *adreno_dev,
				unsigned int counter)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;

	if (counters == NULL || counter > 3)
		return 0;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs[counter];

	/* freeze counter */
	if (adreno_is_a3xx(adreno_dev))
		kgsl_regwrite(device, reg->select - VBIF2_PERF_EN_REG_SEL_OFF,
							0);

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* un-freeze counter */
	if (adreno_is_a3xx(adreno_dev))
		kgsl_regwrite(device, reg->select - VBIF2_PERF_EN_REG_SEL_OFF,
							1);

	return ((((uint64_t) hi) << 32) | lo)
		+ counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF]
			.regs[counter].value;
}

static uint64_t _perfcounter_read_vbif2_pwr(struct adreno_device *adreno_dev,
				unsigned int counter)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0, val = 2;

	if (adreno_is_a4xx(adreno_dev))
		val = 3;

	if (counters == NULL || counter > val)
		return -EINVAL;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR].regs[counter];

	/* freeze counter */
	if (adreno_is_a3xx(adreno_dev))
		kgsl_regwrite(device, reg->select, 0);

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* un-freeze counter */
	if (adreno_is_a3xx(adreno_dev))
		kgsl_regwrite(device, reg->select, 1);

	return ((((uint64_t) hi) << 32) | lo)
		+ counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR]
			.regs[counter].value;
}

/*
 * adreno_perfcounter_read() - Reads a performance counter
 * @adreno_dev: The device on which the counter is running
 * @group: The group of the counter
 * @counter: The counter within the group
 *
 * Function is used to read the counter of adreno devices
 * Returns the 64 bit counter value on success else 0.
 */
uint64_t adreno_perfcounter_read(struct adreno_device *adreno_dev,
	unsigned int group, unsigned int counter)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;
	unsigned int in = 0, out;

	if (group == KGSL_PERFCOUNTER_GROUP_ALWAYSON)
		return perfcounter_read_alwayson(adreno_dev);

	if (group == KGSL_PERFCOUNTER_GROUP_VBIF_PWR)
		return _perfcounter_read_vbif2_pwr(adreno_dev, counter);

	if (group == KGSL_PERFCOUNTER_GROUP_VBIF)
		return _perfcounter_read_vbif2(adreno_dev, counter);

	if (group == KGSL_PERFCOUNTER_GROUP_PWR)
		return _perfcounter_read_pwr(adreno_dev, counter);

	if (counters == NULL || group >= counters->group_count)
		return 0;

	if ((0 == counters->groups[group].reg_count) ||
		(counter >= counters->groups[group].reg_count))
		return 0;

	reg = &(counters->groups[group].regs[counter]);

	/* Freeze the counter */
	if (adreno_is_a3xx(adreno_dev)) {
		adreno_readreg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_CTL, &in);
		out = in & ~RBBM_PERFCTR_CTL_ENABLE;
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_CTL, out);
	}

	/* Read the values */
	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* Re-Enable the counter */
	if (adreno_is_a3xx(adreno_dev))
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_CTL, in);
	return (((uint64_t) hi) << 32) | lo;
}



