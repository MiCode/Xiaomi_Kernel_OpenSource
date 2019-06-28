/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_MSM_MDF_H
#define _LINUX_MSM_MDF_H

#ifdef CONFIG_MSM_MDF

/**
 *  msm_mdf_mem_init - allocate and map memory to ADSP be shared
 *                     across multiple remote DSPs.
 */
int msm_mdf_mem_init(void);

/**
 *  msm_mdf_mem_init - unmap and free memory to ADSP.
 */
int msm_mdf_mem_deinit(void);

#else

static inline int msm_mdf_mem_init(void)
{
	return 0;
}

static inline int msm_mdf_mem_deinit(void)
{
	return 0;
}

#endif /* CONFIG_MSM_MDF */

#endif /* _LINUX_MSM_MDF_H */
