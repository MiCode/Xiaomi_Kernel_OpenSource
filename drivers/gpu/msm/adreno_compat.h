/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_COMPAT_H
#define __ADRENO_COMPAT_H

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#include "kgsl.h"
#include "kgsl_device.h"

int adreno_getproperty_compat(struct kgsl_device *device,
			unsigned int type,
			void __user *value,
			size_t sizebytes);

int adreno_setproperty_compat(struct kgsl_device_private *dev_priv,
				unsigned int type,
				void __user *value,
				unsigned int sizebytes);

long adreno_compat_ioctl(struct kgsl_device_private *dev_priv,
			unsigned int cmd, unsigned long arg);

#else

static inline int adreno_getproperty_compat(struct kgsl_device *device,
				unsigned int type,
				void __user *value, size_t sizebytes)
{
	BUG();
}

static inline int adreno_setproperty_compat(struct kgsl_device_private
				*dev_priv, unsigned int type,
				void __user *value, unsigned int sizebytes)
{
	BUG();
}

static inline long adreno_compat_ioctl(struct kgsl_device_private *dev_priv,
				unsigned int cmd, unsigned long arg)
{
	BUG();
}

#endif /* CONFIG_COMPAT */
#endif /* __ADRENO_COMPAT_H */
