/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _HF_SENSOR_IO_H_
#define _HF_SENSOR_IO_H_

#include <linux/ioctl.h>
#include <linux/types.h>

enum {
	HF_MANAGER_SENSOR_DISABLE,
	HF_MANAGER_SENSOR_ENABLE,
	HF_MANAGER_SENSOR_FLUSH,
	HF_MANAGER_SENSOR_ENABLE_CALI,
	HF_MANAGER_SENSOR_CONFIG_CALI,
	HF_MANAGER_SENSOR_SELFTEST,
	HF_MANAGER_SENSOR_RAWDATA,
};

enum {
	DATA_ACTION,
	FLUSH_ACTION,
	BIAS_ACTION,
	CALI_ACTION,
	TEMP_ACTION,
	TEST_ACTION,
	RAW_ACTION,
};

struct hf_manager_cmd {
	uint8_t sensor_type;
	uint8_t action;
	int64_t delay;
	int64_t latency;
	int32_t data[12];
} __packed;

struct hf_manager_event {
	int64_t timestamp;
	uint8_t sensor_type;
	uint8_t accurancy;
	uint8_t action;
	uint8_t reserved;
	union {
		int32_t word[6];
		int8_t byte[0];
	};
} __packed;

struct ioctl_packet {
	uint8_t sensor_type;
	union {
		bool status;
		int8_t byte[64];
	};
} __packed;

#define HF_MANAGER_REQUEST_REGISTER_STATUS  _IOWR('a', 1, struct ioctl_packet)
#define HF_MANAGER_REQUEST_BIAS_DATA        _IOW('a', 2, struct ioctl_packet)
#define HF_MANAGER_REQUEST_CALI_DATA        _IOW('a', 3, struct ioctl_packet)
#define HF_MANAGER_REQUEST_TEMP_DATA        _IOW('a', 4, struct ioctl_packet)
#define HF_MANAGER_REQUEST_TEST_DATA        _IOW('a', 5, struct ioctl_packet)
#define HF_MANAGER_REQUEST_SENSOR_INFO      _IOWR('a', 6, struct ioctl_packet)
#define HF_MANAGER_REQUEST_CUST_DATA        _IOWR('a', 7, struct ioctl_packet)

#endif
