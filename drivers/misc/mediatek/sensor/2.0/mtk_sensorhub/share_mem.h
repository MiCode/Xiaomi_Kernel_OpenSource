/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SHARE_MEM_H_
#define _SHARE_MEM_H_

enum share_mem_action {
	DATA_ACTION,
	FLUSH_ACTION,
	BIAS_ACTION,
	CALI_ACTION,
	TEMP_ACTION,
	TEST_ACTION,
	RAW_ACTION,
};

struct share_mem_data {
	uint8_t sensor_type;
	uint8_t action;
	uint8_t reserve[2];
	int64_t timestamp;
	int32_t value[6];
} __packed;

struct share_mem_data_tag {
	uint32_t rp;
	uint32_t wp;
	uint32_t size;
	uint32_t reserve;
	struct share_mem_data data[0];
} __packed;

struct share_mem_debug {
	uint8_t sensor_type;
	uint8_t reserve[3];
	uint8_t buffer[960]; //512+256+128+64
} __packed;

struct share_mem_debug_tag {
	uint32_t rp;
	uint32_t wp;
	uint32_t size;
	uint32_t reserve;
	struct share_mem_debug data[0];
} __packed;

struct share_mem_info {
	uint8_t sensor_type;
	uint32_t gain;
	uint8_t name[16];
	uint8_t vendor[16];
} __packed;

struct share_mem_info_tag {
	uint32_t rp;
	uint32_t wp;
	uint32_t size;
	uint32_t reserve;
	struct share_mem_info data[0];
} __packed;

#endif
