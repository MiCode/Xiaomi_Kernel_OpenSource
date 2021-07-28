// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPU_IPA_H__
#define __MTK_GPU_IPA_H__

#include <linux/devfreq.h>
#include <linux/devfreq_cooling.h>

extern struct devfreq_cooling_power mtk_common_cooling_power_ops;

unsigned long mtk_common_get_static_power(struct devfreq *devfreq,
                                          unsigned long voltage_mv);
unsigned long mtk_common_get_dynamic_power(struct devfreq *devfreq,
                                           unsigned long freqHz,
                                           unsigned long voltage_mv);

#endif /* __MTK_GPU_IPA_H__ */
