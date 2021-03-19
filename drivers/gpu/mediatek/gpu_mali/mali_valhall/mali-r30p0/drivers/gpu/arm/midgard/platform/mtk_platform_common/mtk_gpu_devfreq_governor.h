// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPU_DEVFREQ_GOVERNOR_H__
#define __MTK_GPU_DEVFREQ_GOVERNOR_H__

#include <linux/devfreq.h>

/* DEVFREQ governor name */
#define MTK_GPU_DEVFREQ_GOV_DUMMY		"dummy"

extern struct devfreq_governor mtk_common_gov_dummy;

void mtk_common_devfreq_update_profile(struct devfreq_dev_profile *dp);
int mtk_common_devfreq_init(void);
int mtk_common_devfreq_term(void);

#endif /* __MTK_GPU_DEVFREQ_GOVERNOR_H__ */
