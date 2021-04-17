/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SHARE_MEM_H_
#define _SHARE_MEM_H_

enum flush_action {
	DATA_ACTION,
	FLUSH_ACTION,
	BIAS_ACTION,
	CALI_ACTION,
	TEMP_ACTION,
	TEST_ACTION,
	RAW_ACTION,
};

struct data_unit_t {
	uint8_t sensor_type;
	uint8_t flush_action;
	uint8_t reserve[2];
	int64_t timestamp;
	int32_t value[8];
} __packed;

#endif
