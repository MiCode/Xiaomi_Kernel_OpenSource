/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SENSOR_FREQ_H_
#define _SENSOR_FREQ_H_

#include "scp.h"
#include "hf_manager.h"

enum freq_level {
	LOW = 0,
	MEDIUM,
	HIGH,
	LEVEL_MAX,
};

enum freq_value {
	LOW_FREQ_VALUE = 150,
	MEDIUM_FREQ_VALUE = 300,
	HIGH_FREQ_VALUE = 700,
};

struct sensor_freq_tb {
	uint8_t sensor_type;
	uint8_t freq_level;
	uint8_t core_id;
};

int sensor_register_freq(uint8_t sensor_type);
int sensor_deregister_freq(uint8_t sensor_type);
#endif
