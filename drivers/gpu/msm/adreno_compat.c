/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#include "kgsl.h"
#include "kgsl_compat.h"

#include "adreno.h"

int adreno_getproperty_compat(struct kgsl_device *device,
				enum kgsl_property_type type,
				void __user *value,
				size_t sizebytes)
{
	int status = -EINVAL;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	switch (type) {
	case KGSL_PROP_DEVICE_INFO:
		{
			struct kgsl_devinfo_compat devinfo;

			if (sizebytes != sizeof(devinfo)) {
				status = -EINVAL;
				break;
			}

			memset(&devinfo, 0, sizeof(devinfo));
			devinfo.device_id = device->id + 1;
			devinfo.chip_id = adreno_dev->chipid;
			devinfo.mmu_enabled = kgsl_mmu_enabled();
			devinfo.gmem_gpubaseaddr = adreno_dev->gmem_base;
			devinfo.gmem_sizebytes = adreno_dev->gmem_size;

			if (copy_to_user(value, &devinfo, sizeof(devinfo)) !=
					0) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_DEVICE_SHADOW:
		{
			struct kgsl_shadowprop_compat shadowprop;

			if (sizebytes != sizeof(shadowprop)) {
				status = -EINVAL;
				break;
			}
			memset(&shadowprop, 0, sizeof(shadowprop));
			if (device->memstore.hostptr) {
				/*
				 * NOTE: with mmu enabled, gpuaddr doesn't mean
				 * anything to mmap().
				 */
				shadowprop.gpuaddr = device->memstore.gpuaddr;
				shadowprop.size = device->memstore.size;
				/*
				 * GSL needs this to be set, even if it
				 * appears to be meaningless
				 */
				shadowprop.flags = KGSL_FLAGS_INITIALIZED |
					KGSL_FLAGS_PER_CONTEXT_TIMESTAMPS;
			}
			if (copy_to_user(value, &shadowprop,
				sizeof(shadowprop))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	default:
		/*
		 * Call the adreno_getproperty to check if the property type
		 * was KGSL_PROP_MMU_ENABLE or KGSL_PROP_INTERRUPT_WAITS
		 */
		status = device->ftbl->getproperty(device, type, value,
						sizebytes);
	}

	return status;
}

int adreno_setproperty_compat(struct kgsl_device_private *dev_priv,
				enum kgsl_property_type type,
				void __user *value,
				unsigned int sizebytes)
{
	int status = -EINVAL;
	struct kgsl_device *device = dev_priv->device;

	switch (type) {
	case KGSL_PROP_PWR_CONSTRAINT: {
			struct kgsl_device_constraint_compat constraint32;
			struct kgsl_device_constraint constraint;
			struct kgsl_context *context;

			if (sizebytes != sizeof(constraint32))
				break;

			if (copy_from_user(&constraint32, value,
				sizeof(constraint32))) {
				status = -EFAULT;
				break;
			}

			/* Populate the real constraint type from the compat */
			constraint.type = constraint32.type;
			constraint.context_id = constraint32.context_id;
			constraint.data = compat_ptr(constraint32.data);
			constraint.size = (size_t)constraint32.size;

			context = kgsl_context_get_owner(dev_priv,
							constraint.context_id);
			if (context == NULL)
				break;
			status = adreno_set_constraint(device, context,
								&constraint);
			kgsl_context_put(context);
		}
		break;
	default:
		/*
		 * Call adreno_setproperty in case the property type was
		 * KGSL_PROP_PWRCTRL
		 */
		status = device->ftbl->setproperty(dev_priv, type, value,
						sizebytes);
	}

	return status;
}

long adreno_compat_ioctl(struct kgsl_device_private *dev_priv,
			      unsigned int cmd, void *data)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int result = 0;

	switch (cmd) {
	case IOCTL_KGSL_PERFCOUNTER_QUERY_COMPAT: {
		struct kgsl_perfcounter_query_compat *query32 = data;
		struct kgsl_perfcounter_query query;
		query.groupid = query32->groupid;
		query.countables = (unsigned int __user *)(uintptr_t)
							query32->countables;
		query.count = query32->count;
		query.max_counters = query32->max_counters;
		result = adreno_perfcounter_query_group(adreno_dev,
			query.groupid, query.countables,
			query.count, &query.max_counters);
		query32->max_counters = query.max_counters;
		break;
	}
	case IOCTL_KGSL_PERFCOUNTER_READ_COMPAT: {
		struct kgsl_perfcounter_read_compat *read32 = data;
		struct kgsl_perfcounter_read read;
		read.reads = (struct kgsl_perfcounter_read_group __user *)
				(uintptr_t)read32->reads;
		read.count = read32->count;
		result = adreno_perfcounter_read_group(adreno_dev,
			read.reads, read.count);
		break;
	}
	default:
		KGSL_DRV_INFO(dev_priv->device,
			"invalid ioctl code %08x\n", cmd);
		result = -ENOIOCTLCMD;
		break;
	}
	return result;

}

