/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SENSOR_COMM_H_
#define _SENSOR_COMM_H_

#include "ipi_comm.h"

enum sensor_comm_ctrl_cmd {
	SENS_COMM_CTRL_DISABLE_CMD,
	SENS_COMM_CTRL_ENABLE_CMD,
	SENS_COMM_CTRL_FLUSH_CMD,
	SENS_COMM_CTRL_CALI_CMD,
	SENS_COMM_CTRL_CONFIG_CMD,
	SENS_COMM_CTRL_SELF_TEST_CMD,
	SENS_COMM_CTRL_ENABLE_RAW_CMD,
	SENS_COMM_CTRL_DISABLE_RAW_CMD,
	SENS_COMM_CTRL_MASK_NOTIFY_CMD,
	SENS_COMM_CTRL_UNMASK_NOTIFY_CMD,
	SENS_COMM_CTRL_TIMESYNC_CMD,
	SENS_COMM_CTRL_SHARE_MEMORY_CMD,
	SENS_COMM_CTRL_DEBUG_CMD,
	MAX_SENS_COMM_CTRL_CMD,
};

enum sensor_comm_notify_cmd {
	SENS_COMM_NOTIFY_DATA_CMD,
	SENS_COMM_NOTIFY_FULL_CMD,
	SENS_COMM_NOTIFY_SUPER_DATA_CMD,
	SENS_COMM_NOTIFY_SUPER_FULL_CMD,
	SENS_COMM_NOTIFY_READY_CMD,
	SENS_COMM_NOTIFY_LIST_CMD,
	SENS_COMM_NOTIFY_DEBUG_CMD,
	SENS_COMM_NOTIFY_CUSTOM_CMD,
	SENS_COMM_NOTIFY_FAST_CUST_CMD,
	MAX_SENS_COMM_NOTIFY_CMD,
};

struct sensor_comm_batch {
	int64_t delay;
	int64_t latency;
} __packed __aligned(4);

struct sensor_comm_timesync {
	int64_t host_timestamp;
	int64_t host_archcounter;
} __packed __aligned(4);

struct sensor_comm_share_mem {
	uint8_t available_num;
	struct {
		uint8_t payload_type;
		uint32_t payload_base;
	} __aligned(4) base_info[4];
} __packed __aligned(4);

struct sensor_comm_ctrl {
	uint8_t sequence;
	uint8_t sensor_type;
	uint8_t command;
	uint8_t length;
	uint8_t crc8;
	uint8_t data[0] __aligned(4);
} __packed __aligned(4);

struct sensor_comm_ack {
	uint8_t sequence;
	uint8_t sensor_type;
	uint8_t command;
	int8_t ret_val;
	uint8_t crc8;
} __packed __aligned(4);

struct data_notify {
	int32_t write_position;
	int64_t scp_timestamp;
	int64_t scp_archcounter;
} __packed __aligned(4);

struct sensor_comm_notify {
	uint8_t sequence;
	uint8_t sensor_type;
	uint8_t command;
	uint8_t length;
	uint8_t crc8;
	int32_t value[5] __aligned(4);
} __packed __aligned(4);

int sensor_comm_ctrl_send(struct sensor_comm_ctrl *ctrl, unsigned int size);
int sensor_comm_notify(struct sensor_comm_notify *notify);
int sensor_comm_notify_bypass(struct sensor_comm_notify *notify);
void sensor_comm_notify_handler_register(uint8_t cmd,
		void (*f)(struct sensor_comm_notify *n, void *private_data),
		void *private_data);
void sensor_comm_notify_handler_unregister(uint8_t cmd);
int sensor_comm_init(void);
void sensor_comm_exit(void);

#endif
