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
#ifndef __GED_GPU_TUNER_H__

#define __GED_GPU_TUNER_H__

#include <linux/list.h>
#include <linux/kernel.h>
#include <ged_bridge.h>
#include "ged_type.h"
#include "mtk_gpu_utility.h"

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

int ged_bridge_gpu_tuner_status(struct GED_BRIDGE_IN_GPU_TUNER_STATUS *in,
	struct GED_BRIDGE_OUT_GPU_TUNER_STATUS *out);

#if defined(__cplusplus)
}
#endif

#endif
