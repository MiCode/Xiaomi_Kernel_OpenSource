/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_RPM_LOG_H
#define __ARCH_ARM_MACH_MSM_RPM_LOG_H

#include <linux/types.h>

enum {
	MSM_RPM_LOG_PAGE_INDICES,
	MSM_RPM_LOG_PAGE_BUFFER,
	MSM_RPM_LOG_PAGE_COUNT
};

struct msm_rpm_log_platform_data {
	u32 reg_offsets[MSM_RPM_LOG_PAGE_COUNT];
	u32 log_len;
	u32 log_len_mask;
	phys_addr_t phys_addr_base;
	u32 phys_size;
	void __iomem *reg_base;
};

#endif /* __ARCH_ARM_MACH_MSM_RPM_LOG_H */
