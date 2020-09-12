/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_GPU_SWPM_PLATFORM_H__
#define __MTK_GPU_SWPM_PLATFORM_H__

/* numbers of unsigned int */
#define GPU_SWPM_RESERVED_SIZE 15

enum gpu_power_counter {
	gfreq,
	gvolt,
	galu_fma_urate,
	galu_cvt_urate,
	galu_sfu_urate,
	gtex_urate,
	glsc_urate,
	gl2c_urate,
	gvary_urate,
	gtiler_urate,
	grast_urate,
	gloading,
	gthermal,
	glkg,

	GPU_POWER_COUNTER_LAST
};

struct gpu_swpm_rec_data {
	/* 4(int) * 15 = 60 bytes */
	unsigned int gpu_enable;
	unsigned int gpu_counter[GPU_POWER_COUNTER_LAST];
};

/* gpu power index data, gpu_state_ratio types */
enum gpu_power_state {
	GPU_ACTIVE,
	GPU_IDLE,
	GPU_POWER_OFF,

	NR_GPU_POWER_STATE
};

/* gpu power index structure */
struct gpu_swpm_index {
	unsigned int gpu_state_ratio[NR_GPU_POWER_STATE];
	unsigned int gpu_freq_mhz;
	unsigned int gpu_volt_mv;
	unsigned int gpu_loading;
	unsigned int alu_fma_urate;
	unsigned int alu_cvt_urate;
	unsigned int alu_sfu_urate;
	unsigned int tex_urate;
	unsigned int lsc_urate;
	unsigned int l2c_urate;
	unsigned int vary_urate;
	unsigned int tiler_urate;
	unsigned int rast_urate;
	unsigned int thermal;
	unsigned int lkg;
};

#endif

