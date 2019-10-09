/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_ION_OF_H
#define _MSM_ION_OF_H

#include <linux/device.h>
#include <uapi/linux/msm_ion.h>

#if IS_ENABLED(CONFIG_ION_MSM_HEAPS)

struct device *msm_ion_heap_device_by_id(int heap_id);

#else

static inline struct device *msm_ion_heap_device_by_id(int heap_id)
{
	return ERR_PTR(-ENODEV);
}

#endif /* CONFIG_ION_MSM_HEAPS */
#endif /* _MSM_ION_OF_H */
