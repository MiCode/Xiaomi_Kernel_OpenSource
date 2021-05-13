/* SPDX-License-Identifier: GPL-2.0 */
/*
 *
 * Copyright (c) 2019 MediaTek Inc.
 *
 */
#ifndef __CPUFREQ_H__
#define __CPUFREQ_H__

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
extern void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
			unsigned long cap, unsigned long *next_freq);
#else
#define mtk_map_util_freq(data, util, freq, cap, next_freq)
#endif /* CONFIG_NONLINEAR_FREQ_CTL */
#endif /* __CPUFREQ_H__ */
