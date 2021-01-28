/*
 * Copyright (C) 2018 MediaTek Inc.
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
#ifndef __MTK_GPU_POWER_MODEL_H__
#define __MTK_GPU_POWER_MODEL_H__

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>


/*reserve sspm variable
 *sspm power counter : for gpu_pm_buf offest
 *
 *enum {
 *	gpu_freq,
 *	gpu_volt,
 *	alu_urate,
 *	tex_urate,
 *	POWER_COUNTER_LAST
 *} sspm_power_counter;
 *
 *4x4 = 16 byte
 *struct dym_gpu_pm {
 *	uint32_t version;
 *	uint32_t gpu_freq;
 *	union {
 *		struct {
 *			uint32_t counter[POWER_COUNTER_LAST];
 *		} v1;
 *	};
 *};
 */

void MTKGPUPower_model_stop(void);
void MTKGPUPower_model_start(unsigned int interval_ns);
void MTKGPUPower_model_suspend(void);
void MTKGPUPower_model_resume(void);




#endif
