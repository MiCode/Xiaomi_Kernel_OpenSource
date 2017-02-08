/* Copyright (c) 2002,2007-2017, The Linux Foundation. All rights reserved.
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

#include "adreno_perfcounter.h"
#include "a5xx_reg.h"

/* Bit flag for RBMM_PERFCTR_CTL */
#define RBBM_PERFCTR_CTL_ENABLE		0x00000001

#define VBIF2_PERF_CNT_SEL_MASK 0x7F
/* offset of clear register from select register */
#define VBIF2_PERF_CLR_REG_SEL_OFF 8
/* offset of enable register from select register */
#define VBIF2_PERF_EN_REG_SEL_OFF 16

/* offset of clear register from the enable register */
#define VBIF2_PERF_PWR_CLR_REG_EN_OFF 8

#define REG_64BIT_VAL(hi, lo, val) (((((uint64_t) hi) << 32) | lo) + val)

/*
 * Return true if the countable is used and not broken
 */
static inline int active_countable(unsigned int countable)
{
	return ((countable != DRM_MSM_PERFCOUNTER_NOT_USED) &&
		(countable != DRM_MSM_PERFCOUNTER_BROKEN));
}

static int adreno_perfcounter_enable(struct adreno_gpu *adreno_gpu,
	unsigned int group, unsigned int counter, unsigned int countable);

/**
 * adreno_perfcounter_start: Enable performance counters
 * @adreno_gpu: Adreno device to configure
 *
 * Ensure all performance counters are enabled that are allocated.  Since
 * the device was most likely stopped, we can't trust that the counters
 * are still valid so make it so.
 */

void adreno_perfcounter_start(struct adreno_gpu *adreno_gpu)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;
	struct adreno_perfcount_group *group;
	unsigned int i, j;

	if (NULL == counters)
		return;
	/* group id iter */
	for (i = 0; i < counters->group_count; i++) {
		group = &(counters->groups[i]);

		/* countable iter */
		for (j = 0; j < group->reg_count; j++) {
			if (!active_countable(group->regs[j].countable))
				continue;
			/*
			 * The GPU has to be idle before calling the perfcounter
			 * enable function, but since this function is called
			 * during start we already know the GPU is idle.
			 * Since the countable/counter pairs have already been
			 * validated, there is no way for _enable() to fail so
			 * no need to check the return code.
			 */
			adreno_perfcounter_enable(adreno_gpu, i, j,
				group->regs[j].countable);
		}
	}
}

/**
 * adreno_perfcounter_read_group() - Determine which countables are in counters
 * @adreno_gpu: Adreno device to configure
 * @reads: List of drm_perfcounter_read_group
 * @count: Length of list
 *
 * Read the performance counters for the groupid/countable pairs and return
 * the 64 bit result for each pair
 */

int adreno_perfcounter_read_group(struct adreno_gpu *adreno_gpu,
	struct drm_perfcounter_read_group __user *reads, unsigned int count)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;
	struct adreno_perfcount_group *group;
	struct drm_perfcounter_read_group *list = NULL;
	unsigned int i, j;
	int ret = 0;
	struct msm_gpu *gpu = &adreno_gpu->base;

	if (NULL == counters)
		return -EINVAL;

	/* sanity check params passed in */
	if (reads == NULL || count == 0 || count > 100)
		return -EINVAL;

	list = kmalloc_array(count, sizeof(struct drm_perfcounter_read_group),
		GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	if (copy_from_user(list, reads,
		sizeof(struct drm_perfcounter_read_group) * count)) {
		ret = -EFAULT;
		goto done;
	}

	mutex_lock(&gpu->dev->struct_mutex);

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
				list[j].value = a5xx_perfcounter_read(
					adreno_gpu, list[j].groupid, i);
				break;
			}
		}
	}

	mutex_unlock(&gpu->dev->struct_mutex);

	/* write the data */
	if (ret == 0)
		if (copy_to_user(reads, list,
			sizeof(struct drm_perfcounter_read_group) * count))
			ret = -EFAULT;

done:
	kfree(list);
	return ret;
}

/**
 * adreno_perfcounter_query_group: Determine which countables are in counters
 * @adreno_gpu: Adreno device to configure
 * @groupid: Desired performance counter group
 * @countables: Return list of all countables in the groups counters
 * @count: Max length of the array
 * @max_counters: max counters for the groupid
 *
 * Query the current state of counters for the group.
 */

int adreno_perfcounter_query_group(struct adreno_gpu *adreno_gpu,
	unsigned int groupid, unsigned int __user *countables,
		unsigned int count, unsigned int *max_counters)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;
	struct adreno_perfcount_group *group;
	unsigned int i, t;
	int ret = 0;
	unsigned int *buf;
	struct msm_gpu *gpu = &adreno_gpu->base;

	*max_counters = 0;

	if (counters == NULL || groupid >= counters->group_count)
		return -EINVAL;

	mutex_lock(&gpu->dev->struct_mutex);

	group = &(counters->groups[groupid]);
	*max_counters = group->reg_count;

	/*
	 * if NULL countable or *count of zero, return max reg_count in
	 * *max_counters and return success
	 */
	if (countables == NULL || count == 0) {
		mutex_unlock(&gpu->dev->struct_mutex);
		return 0;
	}

	t = min_t(unsigned int, group->reg_count, count);

	buf = kmalloc_array(t, sizeof(unsigned int), GFP_KERNEL);
	if (buf == NULL) {
		mutex_unlock(&gpu->dev->struct_mutex);
		return -ENOMEM;
	}

	for (i = 0; i < t; i++)
		buf[i] = group->regs[i].countable;

	mutex_unlock(&gpu->dev->struct_mutex);

	if (copy_to_user(countables, buf, sizeof(unsigned int) * t))
		ret = -EFAULT;

	kfree(buf);

	return ret;
}

/**
 * adreno_perfcounter_get_groupid() - Get the performance counter ID
 * @adreno_gpu: Adreno device
 * @name: Performance counter group name string
 *
 * Get the groupid based on the name and return this ID
 */

int adreno_perfcounter_get_groupid(struct adreno_gpu *adreno_gpu,
	const char *name)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;
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
 * @adreno_gpu: Adreno device
 * @groupid: Desired performance counter groupid
 *
 * Get the name based on the groupid and return it
 */

const char *adreno_perfcounter_get_name(struct adreno_gpu *adreno_gpu,
	unsigned int groupid)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;

	if (counters != NULL && groupid < counters->group_count)
		return counters->groups[groupid].name;

	return NULL;
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
 * @adreno_gpu: Adreno device to configure
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

int adreno_perfcounter_get(struct adreno_gpu *adreno_gpu,
	unsigned int groupid, unsigned int countable, unsigned int **offset,
		unsigned int **offset_hi, unsigned int flags)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;
	struct adreno_perfcount_group *group;
	unsigned int empty = -1;
	int ret = 0;

	/* always clear return variables */
	if (*offset)
		**offset = 0;
	if (*offset_hi)
		**offset_hi = 0;

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
				DRM_MSM_PERFCOUNTER_NOT_USED) {
				/* keep track of unused counter */
				empty = i;
			}
		}
	}

	/* no available counters, so do nothing else */
	if (empty == -1)
		return -EBUSY;

	/* enable the new counter */
	ret = adreno_perfcounter_enable(adreno_gpu, groupid, empty, countable);
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

	if (*offset)
		**offset = group->regs[empty].offset;
	if (*offset_hi)
		**offset_hi = group->regs[empty].offset_hi;

	return ret;
}

/**
 * adreno_perfcounter_put: Release a countable from counter resource
 * @adreno_gpu: Adreno device to configure
 * @groupid: Desired performance counter group
 * @countable: Countable desired to be freed from a  counter
 * @flags: Flag to determine if kernel or user space request
 *
 * Put a performance counter/countable pair that was previously received.  If
 * noone else is using the countable, free up the counter for others.
 */
int adreno_perfcounter_put(struct adreno_gpu *adreno_gpu,
	unsigned int groupid, unsigned int countable, unsigned int flags)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;
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
					DRM_MSM_PERFCOUNTER_NOT_USED;

			return 0;
		}
	}

	return -EINVAL;
}

static int _perfcounter_enable_pwr(struct adreno_gpu *adreno_gpu,
	unsigned int counter)
{
	struct msm_gpu *gpu = &adreno_gpu->base;

	/*
	* On 5XX we have to emulate the PWR counters which are physically
	* missing. Program countable 6 on RBBM_PERFCTR_RBBM_0 as a substitute
	* for PWR:1. Don't emulate PWR:0 as nobody uses it and we don't want
	* to take away too many of the generic RBBM counters.
	*/

	if (counter == 0)
		return -EINVAL;

	gpu_write(gpu, A5XX_RBBM_PERFCTR_RBBM_SEL_0, 6);

	return 0;
}

static void _perfcounter_enable_vbif(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcounters *counters, unsigned int counter,
		unsigned int countable)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;

	reg = &counters->groups[DRM_MSM_PERFCOUNTER_GROUP_VBIF].regs[counter];
	/* Write 1, followed by 0 to CLR register for clearing the counter */
	gpu_write(gpu, reg->select - VBIF2_PERF_CLR_REG_SEL_OFF, 1);
	gpu_write(gpu, reg->select - VBIF2_PERF_CLR_REG_SEL_OFF, 0);
	gpu_write(gpu, reg->select, countable & VBIF2_PERF_CNT_SEL_MASK);
	/* enable reg is 8 DWORDS before select reg */
	gpu_write(gpu, reg->select - VBIF2_PERF_EN_REG_SEL_OFF, 1);
	reg->value = 0;
}

static void _perfcounter_enable_vbif_pwr(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcounters *counters, unsigned int counter)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;

	reg = &counters->groups[
		DRM_MSM_PERFCOUNTER_GROUP_VBIF_PWR].regs[counter];
	/* Write 1, followed by 0 to CLR register for clearing the counter */
	gpu_write(gpu, reg->select + VBIF2_PERF_PWR_CLR_REG_EN_OFF, 1);
	gpu_write(gpu, reg->select + VBIF2_PERF_PWR_CLR_REG_EN_OFF, 0);
	gpu_write(gpu, reg->select, 1);
	reg->value = 0;
}

static void _power_counter_enable_alwayson(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcounters *counters)
{
	struct msm_gpu *gpu = &adreno_gpu->base;

	gpu_write(gpu, A5XX_GPMU_ALWAYS_ON_COUNTER_RESET, 1);
	counters->groups[
	DRM_MSM_PERFCOUNTER_GROUP_ALWAYSON_PWR].regs[0].value = 0;
}

static void _power_counter_enable_gpmu(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcounters *counters, unsigned int group,
		unsigned int counter, unsigned int countable)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;

	if (adreno_is_a530(adreno_gpu)) {
		if (countable > 43)
			return;
	} else if (adreno_is_a540(adreno_gpu)) {
		if (countable > 47)
			return;
	} else
	/* return on platforms that have no GPMU */
		return;

	reg = &counters->groups[group].regs[counter];

	/* Move the countable to the correct byte offset */
	countable = countable << ((counter % 4) * 8);

	gpu_write(gpu, reg->select, countable);

	gpu_write(gpu, A5XX_GPMU_POWER_COUNTER_ENABLE, 1);
	reg->value = 0;
}

static void _power_counter_enable_default(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcounters *counters, unsigned int group,
	unsigned int counter, unsigned int countable)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;

	reg = &counters->groups[group].regs[counter];
	gpu_write(gpu, reg->select, countable);
	gpu_write(gpu, A5XX_GPMU_POWER_COUNTER_ENABLE, 1);
	reg->value = 0;
}

static int _perfcounter_enable_default(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcounters *counters, unsigned int group,
	unsigned int counter, unsigned int countable)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;
	int i;
	int ret = 0;

	reg = &(counters->groups[group].regs[counter]);

	gpu_write(gpu, reg->select, countable);
	return 0;
}

/**
 * adreno_perfcounter_enable - Configure a performance counter for a countable
 * @adreno_gpu -  Adreno device to configure
 * @group - Desired performance counter group
 * @counter - Desired performance counter in the group
 * @countable - Desired countable
 *
 * Function is used for adreno cores
 * Physically set up a counter within a group with the desired countable
 * Return 0 on success else error code
 */
static int adreno_perfcounter_enable(struct adreno_gpu *adreno_gpu,
	unsigned int group, unsigned int counter, unsigned int countable)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;

	if (counters == NULL)
		return -EINVAL;

	if (group >= counters->group_count)
		return -EINVAL;

	if (counter >= counters->groups[group].reg_count)
		return -EINVAL;

	switch (group) {
	case DRM_MSM_PERFCOUNTER_GROUP_ALWAYSON:
		/* alwayson counter is global, so init value is 0 */
		break;
	case DRM_MSM_PERFCOUNTER_GROUP_PWR:
		return _perfcounter_enable_pwr(adreno_gpu, counter);
	case DRM_MSM_PERFCOUNTER_GROUP_VBIF:
		if (countable > VBIF2_PERF_CNT_SEL_MASK)
			return -EINVAL;
		_perfcounter_enable_vbif(adreno_gpu, counters, counter,
							countable);
		break;
	case DRM_MSM_PERFCOUNTER_GROUP_VBIF_PWR:
		_perfcounter_enable_vbif_pwr(adreno_gpu, counters, counter);
		break;
	case DRM_MSM_PERFCOUNTER_GROUP_SP_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_TP_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_RB_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_CCU_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_UCHE_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_CP_PWR:
		_power_counter_enable_default(adreno_gpu, counters, group,
						counter, countable);
		break;
	case DRM_MSM_PERFCOUNTER_GROUP_GPMU_PWR:
		_power_counter_enable_gpmu(adreno_gpu, counters, group, counter,
				countable);
		break;
	case DRM_MSM_PERFCOUNTER_GROUP_ALWAYSON_PWR:
		_power_counter_enable_alwayson(adreno_gpu, counters);
		break;
	default:
		return _perfcounter_enable_default(adreno_gpu, counters, group,
				counter, countable);
	}

	return 0;
}

static uint64_t _perfcounter_read_alwayson(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcount_group *group, unsigned int counter)
{
	uint64_t val = 0;

	val = adreno_gpu_read64(adreno_gpu, REG_ADRENO_RBBM_ALWAYSON_COUNTER_LO,
		REG_ADRENO_RBBM_ALWAYSON_COUNTER_HI);

	return val + group->regs[counter].value;
}

static uint64_t _perfcounter_read_vbif_pwr(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcount_group *group, unsigned int counter)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;

	reg = &group->regs[counter];

	/* Read the values */
	lo = gpu_read(gpu, reg->offset);
	hi = gpu_read(gpu, reg->offset_hi);

	return REG_64BIT_VAL(hi, lo, reg->value);
}

static uint64_t _perfcounter_read_vbif(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcount_group *group, unsigned int counter)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;

	reg = &group->regs[counter];


	/* Read the values */
	lo = gpu_read(gpu, reg->offset);
	hi = gpu_read(gpu, reg->offset_hi);

	return REG_64BIT_VAL(hi, lo, reg->value);
}

static uint64_t _perfcounter_read_pwr(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcount_group *group, unsigned int counter)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;
	unsigned int in = 0, out, lo = 0, hi = 0;
	unsigned int enable_bit;

	reg = &group->regs[counter];

	/* Remember, counter 0 is not emulated on 5XX */
	/* if (adreno_is_a5xx(adreno_gpu) && (counter == 0)) */
	if (adreno_is_a530(adreno_gpu) && (counter == 0))
		return -EINVAL;

	/* Read the values */
	lo = gpu_read(gpu, reg->offset);
	hi = gpu_read(gpu, reg->offset_hi);

	return REG_64BIT_VAL(hi, lo, reg->value);
}

static uint64_t _perfcounter_read_pwrcntr(struct adreno_gpu *adreno_gpu,
	struct adreno_perfcount_group *group, unsigned int counter)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;

	reg = &group->regs[counter];

	/* Read the values */
	lo = gpu_read(gpu, reg->offset);
	hi = gpu_read(gpu, reg->offset_hi);

	return REG_64BIT_VAL(hi, lo, reg->value);
}

static uint64_t _perfcounter_read_default(struct adreno_gpu *adreno_gpu,
		struct adreno_perfcount_group *group, unsigned int counter)
{
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;
	unsigned int in = 0, out;

	reg = &group->regs[counter];

	/* Read the values */
	lo = gpu_read(gpu, reg->offset);
	hi = gpu_read(gpu, reg->offset_hi);

	return REG_64BIT_VAL(hi, lo, 0);
}

/**
 * adreno_perfcounter_read() - Reads a performance counter
 * @adreno_gpu: The device on which the counter is running
 * @group: The group of the counter
 * @counter: The counter within the group
 *
 * Function is used to read the counter of adreno devices
 * Returns the 64 bit counter value on success else 0.
 */
uint64_t a5xx_perfcounter_read(struct adreno_gpu *adreno_gpu,
	unsigned int groupid, unsigned int counter)
{
	struct adreno_perfcounters *counters = adreno_gpu->perfcounters;
	struct adreno_perfcount_group *group;

	/* Lets hope this doesn't fail. Now subfunctions don't need to check */
	if (counters == NULL)
		return 0;

	if (groupid >= counters->group_count)
		return 0;

	group = &counters->groups[groupid];

	if (counter >= group->reg_count)
		return 0;
	switch (groupid) {
	case DRM_MSM_PERFCOUNTER_GROUP_ALWAYSON:
		return _perfcounter_read_alwayson(adreno_gpu, group, counter);
	case DRM_MSM_PERFCOUNTER_GROUP_VBIF_PWR:
		return _perfcounter_read_vbif_pwr(adreno_gpu, group, counter);
	case DRM_MSM_PERFCOUNTER_GROUP_VBIF:
		return _perfcounter_read_vbif(adreno_gpu, group, counter);
	case DRM_MSM_PERFCOUNTER_GROUP_PWR:
		return _perfcounter_read_pwr(adreno_gpu, group, counter);
	case DRM_MSM_PERFCOUNTER_GROUP_SP_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_TP_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_RB_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_CCU_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_UCHE_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_CP_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_GPMU_PWR:
	case DRM_MSM_PERFCOUNTER_GROUP_ALWAYSON_PWR:
		return _perfcounter_read_pwrcntr(adreno_gpu, group, counter);
	default:
		return _perfcounter_read_default(adreno_gpu, group, counter);
	}
}
