/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_GPU_TUNER_H__

#define __GED_GPU_TUNER_H__

#include <linux/list.h>
#include <linux/kernel.h>
#include <ged_bridge.h>
#include "ged_type.h"
#include <mt-plat/mtk_gpu_utility.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define BUF_LEN 128

struct GED_GPU_TUNER_HINT {
	char packagename[BUF_LEN];
	char cmd[BUF_LEN];
	int feature;
	int value;
};

struct GED_GPU_TUNER_ITEM {
	struct GED_GPU_TUNER_HINT status;
	struct list_head List;
};

GED_ERROR ged_gpu_tuner_hint_set(char *packagename,
	enum GPU_TUNER_FEATURE eFeature);
GED_ERROR ged_gpu_tuner_hint_restore(char *packagename,
	enum GPU_TUNER_FEATURE eFeature);
GED_ERROR ged_gpu_get_stauts_by_packagename(char *packagename,
	struct GED_GPU_TUNER_ITEM *status);
GED_ERROR ged_gpu_tuner_init(void);
GED_ERROR ged_gpu_tuner_exit(void);

#if defined(__cplusplus)
}
#endif

#endif
