/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef _MT_GPU_BW_H_
#define _MT_GPU_BW_H_

#include <linux/module.h>

extern void mt_gpu_bw_toggle(int i32Restore);
extern void mt_gpu_bw_qos_vcore(unsigned int ui32BW);
extern unsigned int mt_gpu_bw_get_BW(int type);
extern void mt_gpu_bw_init(void);

#endif /* _MT_GPUFREQ_H_ */
