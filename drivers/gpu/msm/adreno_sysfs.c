/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/sysfs.h>
#include <linux/device.h>

#include "kgsl_device.h"
#include "adreno.h"

struct adreno_sysfs_attribute {
	struct device_attribute attr;
	unsigned int (*show)(struct adreno_device *adreno_dev);
	int (*store)(struct adreno_device *adreno_dev, unsigned int val);
};

#define _ADRENO_SYSFS_ATTR(_name, __show, __store) \
struct adreno_sysfs_attribute adreno_attr_##_name = { \
	.attr = __ATTR(_name, 0644, __show, __store), \
	.show = _ ## _name ## _show, \
	.store = _ ## _name ## _store, \
}

#define ADRENO_SYSFS_ATTR(_a) \
	container_of((_a), struct adreno_sysfs_attribute, attr)

static struct adreno_device *_get_adreno_dev(struct device *dev)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	return device ? ADRENO_DEVICE(device) : NULL;
}

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

	if (test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv))
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

static int _ft_fast_hang_detect_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return 0;

	mutex_lock(&device->mutex);

	if (val) {
		if (!kgsl_active_count_get(device)) {
			adreno_fault_detect_start(adreno_dev);
			kgsl_active_count_put(device);
		}
	} else
		adreno_fault_detect_stop(adreno_dev);

	mutex_unlock(&device->mutex);

	return 0;
}

static unsigned int _ft_fast_hang_detect_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->fast_hang_detect;
}

static int _ft_long_ib_detect_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	adreno_dev->long_ib_detect = val;
	return 0;
}

static unsigned int _ft_long_ib_detect_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->long_ib_detect;
}

static int _ft_hang_intr_status_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;

	if (val == test_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv))
		return 0;

	mutex_lock(&device->mutex);
	change_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv);

	if (test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv)) {
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
		adreno_irqctrl(adreno_dev, 1);
	} else if (device->state == KGSL_STATE_INIT) {
		ret = -EACCES;
		change_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv);
	}

	mutex_unlock(&device->mutex);
	return ret;
}

static unsigned int _ft_hang_intr_status_show(struct adreno_device *adreno_dev)
{
	return test_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv);
}

static int _pwrctrl_store(struct adreno_device *adreno_dev,
		unsigned int val, unsigned int flag)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (val == test_bit(flag, &adreno_dev->pwrctrl_flag))
		return 0;

	mutex_lock(&device->mutex);

	/* Power down the GPU before changing the state */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	change_bit(flag, &adreno_dev->pwrctrl_flag);
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);

	return 0;
}

static int _preemption_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (test_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv) == val)
			return 0;

	mutex_lock(&device->mutex);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	change_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
	adreno_dev->cur_rb = &(adreno_dev->ringbuffers[0]);
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);

	return 0;
}

static unsigned int _preemption_show(struct adreno_device *adreno_dev)
{
	return adreno_is_preemption_enabled(adreno_dev);
}

static int _hwcg_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	return _pwrctrl_store(adreno_dev, val, ADRENO_HWCG_CTRL);
}

static unsigned int _hwcg_show(struct adreno_device *adreno_dev)
{
	return test_bit(ADRENO_HWCG_CTRL, &adreno_dev->pwrctrl_flag);
}

static int _sptp_pc_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	return _pwrctrl_store(adreno_dev, val, ADRENO_SPTP_PC_CTRL);
}

static unsigned int _sptp_pc_show(struct adreno_device *adreno_dev)
{
	return test_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag);
}

static int _lm_store(struct adreno_device *adreno_dev, unsigned int val)
{
	return _pwrctrl_store(adreno_dev, val, ADRENO_LM_CTRL);
}

static unsigned int _lm_show(struct adreno_device *adreno_dev)
{
	return test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag);
}

static ssize_t _sysfs_store_u32(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	struct adreno_sysfs_attribute *_attr = ADRENO_SYSFS_ATTR(attr);
	unsigned int val = 0;
	int ret;

	if (adreno_dev == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);

	if (!ret && _attr->store)
		ret = _attr->store(adreno_dev, val);

	return (ssize_t) ret < 0 ? ret : count;
}

static ssize_t _sysfs_show_u32(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	struct adreno_sysfs_attribute *_attr = ADRENO_SYSFS_ATTR(attr);
	unsigned int val = 0;

	if (adreno_dev == NULL)
		return 0;

	if (_attr->show)
		val = _attr->show(adreno_dev);

	return snprintf(buf, PAGE_SIZE, "0x%X\n", val);
}

static ssize_t _sysfs_store_bool(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	struct adreno_sysfs_attribute *_attr = ADRENO_SYSFS_ATTR(attr);
	unsigned int val = 0;
	int ret;

	if (adreno_dev == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);

	if (!ret && _attr->store)
		ret = _attr->store(adreno_dev, val ? 1 : 0);

	return (ssize_t) ret < 0 ? ret : count;
}

static ssize_t _sysfs_show_bool(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	struct adreno_sysfs_attribute *_attr = ADRENO_SYSFS_ATTR(attr);
	unsigned int val = 0;

	if (adreno_dev == NULL)
		return 0;

	if (_attr->show)
		val = _attr->show(adreno_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

#define ADRENO_SYSFS_BOOL(_name) \
	_ADRENO_SYSFS_ATTR(_name, _sysfs_show_bool, _sysfs_store_bool)

#define ADRENO_SYSFS_U32(_name) \
	_ADRENO_SYSFS_ATTR(_name, _sysfs_show_u32, _sysfs_store_u32)

static ADRENO_SYSFS_U32(ft_policy);
static ADRENO_SYSFS_U32(ft_pagefault_policy);
static ADRENO_SYSFS_BOOL(ft_fast_hang_detect);
static ADRENO_SYSFS_BOOL(ft_long_ib_detect);
static ADRENO_SYSFS_BOOL(ft_hang_intr_status);

static DEVICE_INT_ATTR(wake_nice, 0644, adreno_wake_nice);
static DEVICE_INT_ATTR(wake_timeout, 0644, adreno_wake_timeout);

static ADRENO_SYSFS_BOOL(sptp_pc);
static ADRENO_SYSFS_BOOL(lm);
static ADRENO_SYSFS_BOOL(preemption);
static ADRENO_SYSFS_BOOL(hwcg);


static const struct device_attribute *_attr_list[] = {
	&adreno_attr_ft_policy.attr,
	&adreno_attr_ft_pagefault_policy.attr,
	&adreno_attr_ft_fast_hang_detect.attr,
	&adreno_attr_ft_long_ib_detect.attr,
	&adreno_attr_ft_hang_intr_status.attr,
	&dev_attr_wake_nice.attr,
	&dev_attr_wake_timeout.attr,
	&adreno_attr_sptp_pc.attr,
	&adreno_attr_lm.attr,
	&adreno_attr_preemption.attr,
	&adreno_attr_hwcg.attr,
	NULL,
};

/* Add a ppd directory for controlling different knobs from sysfs */
struct adreno_ppd_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kgsl_device *device, char *buf);
	ssize_t (*store)(struct kgsl_device *device, const char *buf,
		size_t count);
};

#define PPD_ATTR(_name, _mode, _show, _store) \
struct adreno_ppd_attribute attr_##_name = { \
	.attr = { .name = __stringify(_name), .mode = _mode }, \
	.show = _show, \
	.store = _store, \
}

#define to_ppd_attr(a) \
container_of((a), struct adreno_ppd_attribute, attr)

#define kobj_to_device(a) \
container_of((a), struct kgsl_device, ppd_kobj)

static ssize_t ppd_enable_store(struct kgsl_device *device,
				const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int ppd_on = 0;
	int ret;

	if (!adreno_is_a430v2(adreno_dev) ||
		!ADRENO_FEATURE(adreno_dev, ADRENO_PPD))
		return count;

	ret = kgsl_sysfs_store(buf, &ppd_on);
	if (ret < 0)
		return ret;

	ppd_on = (ppd_on) ? 1 : 0;

	if (ppd_on == test_bit(ADRENO_PPD_CTRL, &adreno_dev->pwrctrl_flag))
		return count;

	mutex_lock(&device->mutex);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	change_bit(ADRENO_PPD_CTRL, &adreno_dev->pwrctrl_flag);
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);
	return count;
}

static ssize_t ppd_enable_show(struct kgsl_device *device,
					char *buf)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	return snprintf(buf, PAGE_SIZE, "%u\n",
		test_bit(ADRENO_PPD_CTRL, &adreno_dev->pwrctrl_flag));
}
/* Add individual ppd attributes here */
static PPD_ATTR(enable, 0644, ppd_enable_show, ppd_enable_store);

static ssize_t ppd_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct adreno_ppd_attribute *pattr = to_ppd_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret = -EIO;

	if (device != NULL && pattr->show != NULL)
		ret = pattr->show(device, buf);

	return ret;
}

static ssize_t ppd_sysfs_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t count)
{
	struct adreno_ppd_attribute *pattr = to_ppd_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret = -EIO;

	if (device != NULL && pattr->store != NULL)
		ret = pattr->store(device, buf, count);

	return ret;
}

static const struct sysfs_ops ppd_sysfs_ops = {
	.show = ppd_sysfs_show,
	.store = ppd_sysfs_store,
};

static struct kobj_type ktype_ppd = {
	.sysfs_ops = &ppd_sysfs_ops,
};

static void ppd_sysfs_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PPD))
		return;

	sysfs_remove_file(&device->ppd_kobj, &attr_enable.attr);
	kobject_put(&device->ppd_kobj);
}

static int ppd_sysfs_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PPD))
		return -ENODEV;

	ret = kobject_init_and_add(&device->ppd_kobj, &ktype_ppd,
		&device->dev->kobj, "ppd");

	if (ret == 0)
		ret = sysfs_create_file(&device->ppd_kobj, &attr_enable.attr);

	return ret;
}

/**
 * adreno_sysfs_close() - Take down the adreno sysfs files
 * @adreno_dev: Pointer to the adreno device
 *
 * Take down the sysfs files on when the device goes away
 */
void adreno_sysfs_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	ppd_sysfs_close(adreno_dev);
	kgsl_remove_device_sysfs_files(device->dev, _attr_list);
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
	int ret;

	ret = kgsl_create_device_sysfs_files(device->dev, _attr_list);

	/* Add the PPD directory and files */
	if (ret == 0)
		ppd_sysfs_init(adreno_dev);

	return 0;
}

