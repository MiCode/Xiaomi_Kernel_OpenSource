/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_LLC_H
#define __ADRENO_LLC_H

#ifdef CONFIG_QCOM_LLCC
#include "adreno.h"
#include <linux/soc/qcom/llcc-qcom.h>

static inline bool adreno_llc_supported(void)
{
	return true;
}

static inline void *adreno_llc_getd(struct device *dev, const char *name)
{
	return llcc_slice_getd(dev, name);
}

static inline void adreno_llc_putd(void *desc)
{
	if (!IS_ERR(desc))
		llcc_slice_putd(desc);
}

static inline int adreno_llc_deactivate_slice(void *desc)
{
	if (IS_ERR(desc))
		return PTR_ERR(desc);
	else
		return llcc_slice_deactivate(desc);
}

static inline int adreno_llc_get_scid(void *desc)
{
	return llcc_get_slice_id(desc);
}

static inline void adreno_llc_setup(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (!IS_ERR(adreno_dev->gpu_llc_slice) &&
			adreno_dev->gpu_llc_slice_enable)
		if (!llcc_slice_activate(adreno_dev->gpu_llc_slice)) {
			if (gpudev->llc_configure_gpu_scid)
				gpudev->llc_configure_gpu_scid(adreno_dev);
		}

	if (!IS_ERR(adreno_dev->gpuhtw_llc_slice) &&
			adreno_dev->gpuhtw_llc_slice_enable)
		if (!llcc_slice_activate(adreno_dev->gpuhtw_llc_slice)) {
			if (gpudev->llc_configure_gpuhtw_scid)
				gpudev->llc_configure_gpuhtw_scid(adreno_dev);
		}

	if (adreno_dev->gpu_llc_slice_enable ||
			adreno_dev->gpuhtw_llc_slice_enable)
		if (gpudev->llc_enable_overrides)
			gpudev->llc_enable_overrides(adreno_dev);
}

#else
static inline bool adreno_llc_supported(void)
{
	return false;
}

static inline void *adreno_llc_getd(struct device *dev,
		const char *name)
{
	return NULL;
}

static inline void adreno_llc_putd(void *desc)
{
}

static inline int adreno_llc_deactivate_slice(void *desc)
{
	return 0;
}

static inline int adreno_llc_get_scid(void *desc)
{
	return 0;
}

static inline void adreno_llc_setup(struct kgsl_device *device)
{
}
#endif

#endif /* __ADRENO_LLC_H */
