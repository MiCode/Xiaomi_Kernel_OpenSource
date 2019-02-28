/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _MT_GPU_BW_H_
#define _MT_GPU_BW_H_

#include <linux/module.h>

extern void mt_gpu_bw_toggle(int i32Restore);
extern void mt_gpu_bw_qos_vcore(unsigned int ui32BW);
extern unsigned int mt_gpu_bw_get_BW(int type);
extern void mt_gpu_bw_init(void);

#endif /* _MT_GPUFREQ_H_ */
