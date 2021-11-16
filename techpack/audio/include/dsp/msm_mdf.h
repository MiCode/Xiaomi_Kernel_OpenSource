/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
