/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_PBM_CALLBACK_
#define _MTK_PBM_CALLBACK_

#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2)
struct pbm_gpu_callback_table {
	unsigned int (*get_max_pb)(enum gpufreq_target);
	unsigned int (*get_min_pb)(enum gpufreq_target);
	unsigned int (*get_cur_pb)(enum gpufreq_target);
	unsigned int (*get_cur_vol)(enum gpufreq_target);
	int (*get_opp_by_pb)(enum gpufreq_target, unsigned int);
	int (*set_limit)(enum gpufreq_target, enum gpuppm_limiter, int, int);
};
#endif

extern void register_pbm_gpu_notify(void *cb);

#endif

