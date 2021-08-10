/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014, 2017, 2020 The Linux Foundation. All rights reserved.
 */
#ifndef _AUDIO_CALIBRATION_H
#define _AUDIO_CALIBRATION_H

#include <audio/linux/msm_audio_calibration.h>

/* Used by driver in buffer_number field to notify client
 * To update all blocks, for example: freeing all memory
 */
#define ALL_CAL_BLOCKS		-1


struct audio_cal_callbacks {
	int (*alloc)(int32_t cal_type, size_t data_size, void *data);
	int (*dealloc)(int32_t cal_type, size_t data_size, void *data);
	int (*pre_cal)(int32_t cal_type, size_t data_size, void *data);
	int (*set_cal)(int32_t cal_type, size_t data_size, void *data);
	int (*get_cal)(int32_t cal_type, size_t data_size, void *data);
	int (*post_cal)(int32_t cal_type, size_t data_size, void *data);
};

struct audio_cal_reg {
	int32_t				cal_type;
	struct audio_cal_callbacks	callbacks;
};

int audio_cal_register(int num_cal_types, struct audio_cal_reg *reg_data);
int audio_cal_deregister(int num_cal_types, struct audio_cal_reg *reg_data);

#endif
