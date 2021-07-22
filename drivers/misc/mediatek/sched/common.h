/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _SCHED_COMMON_H
#define _SCHED_COMMON_H

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
extern void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
			unsigned long cap, unsigned long *next_freq);
#else
#define mtk_map_util_freq(data, util, freq, cap, next_freq)
#endif /* CONFIG_NONLINEAR_FREQ_CTL */

#endif /* _SCHED_COMMON_H */
