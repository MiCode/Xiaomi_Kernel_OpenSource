// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>

#include "adreno.h"
#include "adreno_perfcounter.h"

static inline int active_countable(unsigned int countable)
{
	return ((countable != KGSL_PERFCOUNTER_NOT_USED) &&
		(countable != KGSL_PERFCOUNTER_BROKEN));
}

/**
 * adreno_perfcounter_restore() - Restore performance counters
 * @adreno_dev: adreno device to configure
 *
 * Load the physical performance counters with 64 bit value which are
 * saved on GPU power collapse.
 */
void adreno_perfcounter_restore(struct adreno_device *adreno_dev)
{
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
	unsigned int counter, groupid;

	if (counters == NULL)
		return;

	for (groupid = 0; groupid < counters->group_count; groupid++) {
		group = &(counters->groups[groupid]);

		if (!group->load)
			continue;

		/* Restore the counters for the group */
		for (counter = 0; counter < group->reg_count; counter++) {
			/* If not active or broken, skip this counter */
			if (!active_countable(group->regs[counter].countable))
				continue;

			group->load(adreno_dev, &group->regs[counter]);
		}
	}
}

/**
 * adreno_perfcounter_save() - Save performance counters
 * @adreno_dev: adreno device to configure
 *
 * Save the performance counter values before GPU power collapse.
 * The saved values are restored on restart.
 * This ensures physical counters are coherent across power-collapse.
 * This function must be called with the oob_gpu set request.
 */
inline void adreno_perfcounter_save(struct adreno_device *adreno_dev)
{
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
	unsigned int counter, groupid;

	if (counters == NULL)
		return;

	for (groupid = 0; groupid < counters->group_count; groupid++) {
		group = &(counters->groups[groupid]);

		/* Save the counter values for the group */
		for (counter = 0; counter < group->reg_count; counter++) {
			/* If not active or broken, skip this counter */
			if (!active_countable(group->regs[counter].countable))
				continue;

			/* accumulate values for non-loadable counters */
			if (group->regs[counter].load_bit >= 0)
				group->regs[counter].value = 0;

			group->regs[counter].value =
				group->regs[counter].value +
				adreno_perfcounter_read(adreno_dev, groupid,
								counter);
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
 */

void adreno_perfcounter_start(struct adreno_device *adreno_dev)
{
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
	unsigned int i, j;

	if (counters == NULL)
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
			adreno_perfcounter_enable(adreno_dev, i, j,
					  group->regs[j].countable);
		}
	}
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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
	struct kgsl_perfcounter_read_group *list = NULL;
	unsigned int i, j;
	int ret = 0;

	if (counters == NULL)
		return -EINVAL;

	/* sanity check params passed in */
	if (reads == NULL || count == 0 || count > 100)
		return -EINVAL;

	list = kmalloc_array(count, sizeof(struct kgsl_perfcounter_read_group),
			GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	if (copy_from_user(list, reads,
			sizeof(struct kgsl_perfcounter_read_group) * count)) {
		ret = -EFAULT;
		goto done;
	}

	mutex_lock(&device->mutex);

	ret = adreno_perfcntr_active_oob_get(adreno_dev);
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

	adreno_perfcntr_active_oob_put(adreno_dev);

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
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
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
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);

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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
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

	t = min_t(unsigned int, group->reg_count, count);

	buf = kmalloc_array(t, sizeof(unsigned int), GFP_KERNEL);
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

static inline void refcount_group(const struct adreno_perfcount_group *group,
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
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
	unsigned int empty = -1;
	int ret = 0;

	/* always clear return variables */
	if (offset)
		*offset = 0;
	if (offset_hi)
		*offset_hi = 0;

	if (counters == NULL)
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

	/* initialize the new counter */
	group->regs[empty].countable = countable;

	/* enable the new counter */
	ret = adreno_perfcounter_enable(adreno_dev, groupid, empty, countable);
	if (ret) {
		/* Put back the perfcounter */
		if (!(group->flags & ADRENO_PERFCOUNTER_GROUP_FIXED))
			group->regs[empty].countable =
				KGSL_PERFCOUNTER_NOT_USED;
		return ret;
	}

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
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;
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

/**
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
	unsigned int groupid, unsigned int counter, unsigned int countable)
{
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;

	if (counters == NULL)
		return -EINVAL;

	if (groupid >= counters->group_count)
		return -EINVAL;

	group = &counters->groups[groupid];

	if (counter >= group->reg_count)
		return -EINVAL;

	return group->enable(adreno_dev, group, counter, countable);
}

/**
 * adreno_perfcounter_read() - Reads a performance counter
 * @adreno_dev: The device on which the counter is running
 * @group: The group of the counter
 * @counter: The counter within the group
 *
 * Function is used to read the counter of adreno devices
 * Returns the 64 bit counter value on success else 0.
 */
uint64_t adreno_perfcounter_read(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int counter)
{
	const struct adreno_perfcounters *counters =
		ADRENO_PERFCOUNTERS(adreno_dev);
	const struct adreno_perfcount_group *group;

	/* Lets hope this doesn't fail. Now subfunctions don't need to check */
	if (counters == NULL)
		return 0;

	if (groupid >= counters->group_count)
		return 0;

	group = &counters->groups[groupid];

	if (counter >= group->reg_count)
		return 0;

	return group->read(adreno_dev, group, counter);
}
