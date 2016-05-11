/* Copyright (c) 2002,2007-2016, The Linux Foundation. All rights reserved.
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

#include <linux/ioctl.h>
#include "kgsl_device.h"
#include "adreno.h"
#include "adreno_a5xx.h"

long adreno_ioctl_perfcounter_get(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_perfcounter_get *get = data;
	int result;

	mutex_lock(&device->mutex);

	/*
	 * adreno_perfcounter_get() is called by kernel clients
	 * during start(), so it is not safe to take an
	 * active count inside that function.
	 */
	result = kgsl_active_count_get(device);

	if (result == 0) {
		result = adreno_perfcounter_get(adreno_dev,
			get->groupid, get->countable, &get->offset,
			&get->offset_hi, PERFCOUNTER_FLAG_NONE);
		kgsl_active_count_put(device);
	}
	mutex_unlock(&device->mutex);

	return (long) result;
}

long adreno_ioctl_perfcounter_put(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_perfcounter_put *put = data;
	int result;

	mutex_lock(&device->mutex);
	result = adreno_perfcounter_put(adreno_dev, put->groupid,
		put->countable, PERFCOUNTER_FLAG_NONE);
	mutex_unlock(&device->mutex);

	return (long) result;
}

static long adreno_ioctl_perfcounter_query(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(dev_priv->device);
	struct kgsl_perfcounter_query *query = data;

	return (long) adreno_perfcounter_query_group(adreno_dev, query->groupid,
			query->countables, query->count, &query->max_counters);
}

static long adreno_ioctl_perfcounter_read(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(dev_priv->device);
	struct kgsl_perfcounter_read *read = data;

	return (long) adreno_perfcounter_read_group(adreno_dev, read->reads,
		read->count);
}

static long adreno_ioctl_preemption_counters_query(
		struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(dev_priv->device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_preemption_counters_query *read = data;
	int size_level = A5XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE;
	int levels_to_copy;

	if (!adreno_is_a5xx(adreno_dev) ||
		!adreno_is_preemption_enabled(adreno_dev))
		return -EOPNOTSUPP;

	if (read->size_user < size_level)
		return -EINVAL;

	/* Calculate number of preemption counter levels to copy to userspace */
	levels_to_copy = (read->size_user / size_level);
	if (levels_to_copy > gpudev->num_prio_levels)
		levels_to_copy = gpudev->num_prio_levels;

	if (copy_to_user((void __user *) (uintptr_t) read->counters,
			adreno_dev->preempt.counters.hostptr,
			levels_to_copy * size_level))
		return -EFAULT;

	read->max_priority_level = levels_to_copy;
	read->size_priority_level = size_level;

	return 0;
}

long adreno_ioctl_helper(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg,
		const struct kgsl_ioctl *cmds, int len)
{
	unsigned char data[128] = { 0 };
	long ret;
	int i;

	for (i = 0; i < len; i++) {
		if (_IOC_NR(cmd) == _IOC_NR(cmds[i].cmd))
			break;
	}

	if (i == len) {
		KGSL_DRV_INFO(dev_priv->device,
			"invalid ioctl code 0x%08X\n", cmd);
		return -ENOIOCTLCMD;
	}

	BUG_ON(_IOC_SIZE(cmds[i].cmd) > sizeof(data));

	if (_IOC_SIZE(cmds[i].cmd)) {
		ret = kgsl_ioctl_copy_in(cmds[i].cmd, cmd, arg, data);

		if (ret)
			return ret;
	} else {
		memset(data, 0, sizeof(data));
	}

	ret = cmds[i].func(dev_priv, cmd, data);

	if (ret == 0 && _IOC_SIZE(cmds[i].cmd))
		ret = kgsl_ioctl_copy_out(cmds[i].cmd, cmd, arg, data);

	return ret;
}

static struct kgsl_ioctl adreno_ioctl_funcs[] = {
	{ IOCTL_KGSL_PERFCOUNTER_GET, adreno_ioctl_perfcounter_get },
	{ IOCTL_KGSL_PERFCOUNTER_PUT, adreno_ioctl_perfcounter_put },
	{ IOCTL_KGSL_PERFCOUNTER_QUERY, adreno_ioctl_perfcounter_query },
	{ IOCTL_KGSL_PERFCOUNTER_READ, adreno_ioctl_perfcounter_read },
	{ IOCTL_KGSL_PREEMPTIONCOUNTER_QUERY,
		adreno_ioctl_preemption_counters_query },
};

long adreno_ioctl(struct kgsl_device_private *dev_priv,
			      unsigned int cmd, unsigned long arg)
{
	return adreno_ioctl_helper(dev_priv, cmd, arg,
		adreno_ioctl_funcs, ARRAY_SIZE(adreno_ioctl_funcs));
}
