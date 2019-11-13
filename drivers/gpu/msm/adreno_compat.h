/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2015, 2017, 2019 The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_COMPAT_H
#define __ADRENO_COMPAT_H

#ifdef CONFIG_COMPAT

struct kgsl_device;
struct kgsl_device_private;

int adreno_getproperty_compat(struct kgsl_device *device,
		struct kgsl_device_getproperty *param);

int adreno_setproperty_compat(struct kgsl_device_private *dev_priv,
				unsigned int type,
				void __user *value,
				unsigned int sizebytes);

long adreno_compat_ioctl(struct kgsl_device_private *dev_priv,
			unsigned int cmd, unsigned long arg);

#else

static inline int adreno_getproperty_compat(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	return -EINVAL;
}

static inline int adreno_setproperty_compat(struct kgsl_device_private
				*dev_priv, unsigned int type,
				void __user *value, unsigned int sizebytes)
{
	return -EINVAL;
}

static inline long adreno_compat_ioctl(struct kgsl_device_private *dev_priv,
				unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

#endif /* CONFIG_COMPAT */
#endif /* __ADRENO_COMPAT_H */
