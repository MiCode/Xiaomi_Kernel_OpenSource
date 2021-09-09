/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPU_MISC_H__
#define __GPU_MISC_H__

/**************************************************
 * Platform Implementation
 **************************************************/
struct gpudfd_platform_fp {
	unsigned int (*get_dfd_force_dump_mode)(void);
	void (*set_dfd_force_dump_mode)(unsigned int mode);
	void (*config_dfd)(unsigned int enable);
};

/**************************************************
 * External Function
 **************************************************/
void gpufreq_hardstop_dump_slog(void);
void gpu_misc_register_gpudfd_fp(struct gpudfd_platform_fp *dfd_platform_fp);
unsigned int gpufreq_get_dfd_force_dump_mode(void);
unsigned int gpufreq_set_dfd_force_dump_mode(unsigned int mode);
void gpufreq_config_dfd(unsigned int enable);

#endif /* __GPU_MISC_H__ */