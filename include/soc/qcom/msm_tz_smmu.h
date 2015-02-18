/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_TZ_SMMU_H__
#define __MSM_TZ_SMMU_H__

#include <linux/device.h>

#ifdef CONFIG_MSM_TZ_SMMU

int msm_tz_smmu_atos_start(struct device *dev, int cb_num);
int msm_tz_smmu_atos_end(struct device *dev, int cb_num);

#else

static inline int msm_tz_smmu_atos_start(struct device *dev, int cb_num)
{
	return 0;
}

static inline int msm_tz_smmu_atos_end(struct device *dev, int cb_num)
{
	return 0;
}

#endif /* CONFIG_MSM_TZ_SMMU */

#endif /* __MSM_TZ_SMMU_H__ */
