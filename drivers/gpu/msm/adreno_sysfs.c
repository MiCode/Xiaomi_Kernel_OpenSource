// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/sysfs.h>

#include "adreno.h"
#include "adreno_sysfs.h"

static int _ft_policy_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	adreno_dev->ft_policy = val & KGSL_FT_POLICY_MASK;
	return 0;
}

static unsigned int _ft_policy_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->ft_policy;
}

static int _ft_pagefault_policy_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;

	mutex_lock(&device->mutex);
	val &= KGSL_FT_PAGEFAULT_MASK;

	if (device->state == KGSL_STATE_ACTIVE)
		ret = kgsl_mmu_set_pagefault_policy(&device->mmu,
			(unsigned long) val);

	if (ret == 0)
		adreno_dev->ft_pf_policy = val;

	mutex_unlock(&device->mutex);

	return 0;
}

static unsigned int _ft_pagefault_policy_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->ft_pf_policy;
}

static int _gpu_llc_slice_enable_store(struct adreno_device *adreno_dev,
		bool val)
{
	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		adreno_dev->gpu_llc_slice_enable = val;
	return 0;
}

static bool _gpu_llc_slice_enable_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->gpu_llc_slice_enable;
}

static int _gpuhtw_llc_slice_enable_store(struct adreno_device *adreno_dev,
		bool val)
{
	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		adreno_dev->gpuhtw_llc_slice_enable = val;
	return 0;
}

static bool _gpuhtw_llc_slice_enable_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->gpuhtw_llc_slice_enable;
}

static int _ft_long_ib_detect_store(struct adreno_device *adreno_dev, bool val)
{
	adreno_dev->long_ib_detect = val ? true : false;
	return 0;
}

static bool _ft_long_ib_detect_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->long_ib_detect;
}

static bool _ft_hang_intr_status_show(struct adreno_device *adreno_dev)
{
	/* Hang interrupt is always on on all targets */
	return true;
}

static int _hwcg_store(struct adreno_device *adreno_dev, bool val)
{
	if (adreno_dev->hwcg_enabled == val)
		return 0;

	return adreno_power_cycle_bool(adreno_dev, &adreno_dev->hwcg_enabled,
		val);
}

static bool _hwcg_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->hwcg_enabled;
}

static int _throttling_store(struct adreno_device *adreno_dev, bool val)
{
	if (!adreno_is_a540(adreno_dev) ||
		adreno_dev->throttling_enabled == val)
		return 0;

	return adreno_power_cycle_bool(adreno_dev,
		&adreno_dev->throttling_enabled, val);
}

static bool _throttling_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->throttling_enabled;
}

static int _sptp_pc_store(struct adreno_device *adreno_dev, bool val)
{
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC) ||
		adreno_dev->sptp_pc_enabled == val)
		return 0;

	return adreno_power_cycle_bool(adreno_dev, &adreno_dev->sptp_pc_enabled,
		val);
}

static bool _sptp_pc_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->sptp_pc_enabled;
}

static int _lm_store(struct adreno_device *adreno_dev, bool val)
{
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		adreno_dev->lm_enabled == val)
		return 0;

	return adreno_power_cycle_bool(adreno_dev, &adreno_dev->lm_enabled,
		val);
}

static bool _lm_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->lm_enabled;
}

static int _ifpc_store(struct adreno_device *adreno_dev, bool val)
{
	return gmu_core_dev_ifpc_store(KGSL_DEVICE(adreno_dev), val);
}

static bool _ifpc_show(struct adreno_device *adreno_dev)
{
	return gmu_core_dev_ifpc_show(KGSL_DEVICE(adreno_dev));
}

static unsigned int _ifpc_count_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->ifpc_count;
}

static bool _acd_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->acd_enabled;
}

static int _acd_store(struct adreno_device *adreno_dev, bool val)
{
	return gmu_core_dev_acd_set(KGSL_DEVICE(adreno_dev), val);
}

static bool _bcl_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->bcl_enabled;
}

static int _bcl_store(struct adreno_device *adreno_dev, bool val)
{
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_BCL) ||
				adreno_dev->bcl_enabled == val)
		return 0;

	return adreno_power_cycle_bool(adreno_dev, &adreno_dev->bcl_enabled,
					val);
}

ssize_t adreno_sysfs_store_u32(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(dev_get_drvdata(dev));
	const struct adreno_sysfs_attribute_u32 *_attr =
		container_of(attr, struct adreno_sysfs_attribute_u32, attr);
	u32 val;
	int ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	ret = _attr->store(adreno_dev, val);
	if (ret)
		return ret;

	return count;
}

ssize_t adreno_sysfs_show_u32(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(dev_get_drvdata(dev));
	const struct adreno_sysfs_attribute_u32 *_attr =
		container_of(attr, struct adreno_sysfs_attribute_u32, attr);

	return scnprintf(buf, PAGE_SIZE, "0x%X\n", _attr->show(adreno_dev));
}

ssize_t adreno_sysfs_store_bool(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(dev_get_drvdata(dev));
	const struct adreno_sysfs_attribute_bool *_attr =
		container_of(attr, struct adreno_sysfs_attribute_bool, attr);
	bool val;
	int ret;

	ret = kstrtobool(buf, &val);
	if (ret)
		return ret;

	ret = _attr->store(adreno_dev, val);
	if (ret)
		return ret;

	return count;
}

ssize_t adreno_sysfs_show_bool(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(dev_get_drvdata(dev));
	const struct adreno_sysfs_attribute_bool *_attr =
		container_of(attr, struct adreno_sysfs_attribute_bool, attr);

	return scnprintf(buf, PAGE_SIZE, "%d\n", _attr->show(adreno_dev));
}

static ADRENO_SYSFS_U32(ft_policy);
static ADRENO_SYSFS_U32(ft_pagefault_policy);
static ADRENO_SYSFS_BOOL(ft_long_ib_detect);
static ADRENO_SYSFS_RO_BOOL(ft_hang_intr_status);
static ADRENO_SYSFS_BOOL(gpu_llc_slice_enable);
static ADRENO_SYSFS_BOOL(gpuhtw_llc_slice_enable);

static DEVICE_INT_ATTR(wake_nice, 0644, adreno_wake_nice);
static DEVICE_INT_ATTR(wake_timeout, 0644, adreno_wake_timeout);

static ADRENO_SYSFS_BOOL(sptp_pc);
static ADRENO_SYSFS_BOOL(lm);
static ADRENO_SYSFS_BOOL(hwcg);
static ADRENO_SYSFS_BOOL(throttling);
static ADRENO_SYSFS_BOOL(ifpc);
static ADRENO_SYSFS_RO_U32(ifpc_count);
static ADRENO_SYSFS_BOOL(acd);
static ADRENO_SYSFS_BOOL(bcl);


static const struct attribute *_attr_list[] = {
	&adreno_attr_ft_policy.attr.attr,
	&adreno_attr_ft_pagefault_policy.attr.attr,
	&adreno_attr_ft_long_ib_detect.attr.attr,
	&adreno_attr_ft_hang_intr_status.attr.attr,
	&dev_attr_wake_nice.attr.attr,
	&dev_attr_wake_timeout.attr.attr,
	&adreno_attr_sptp_pc.attr.attr,
	&adreno_attr_lm.attr.attr,
	&adreno_attr_hwcg.attr.attr,
	&adreno_attr_throttling.attr.attr,
	&adreno_attr_gpu_llc_slice_enable.attr.attr,
	&adreno_attr_gpuhtw_llc_slice_enable.attr.attr,
	&adreno_attr_ifpc.attr.attr,
	&adreno_attr_ifpc_count.attr.attr,
	&adreno_attr_acd.attr.attr,
	&adreno_attr_bcl.attr.attr,
	NULL,
};

/**
 * adreno_sysfs_close() - Take down the adreno sysfs files
 * @adreno_dev: Pointer to the adreno device
 *
 * Take down the sysfs files on when the device goes away
 */
void adreno_sysfs_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	sysfs_remove_files(&device->dev->kobj, _attr_list);
}

/**
 * adreno_sysfs_init() - Initialize adreno sysfs files
 * @adreno_dev: Pointer to the adreno device
 *
 * Initialize many of the adreno specific sysfs files especially for fault
 * tolerance and power control
 */
int adreno_sysfs_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	return sysfs_create_files(&device->dev->kobj, _attr_list);
}

