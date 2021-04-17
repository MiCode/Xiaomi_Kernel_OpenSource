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
	SENS_COMM_NOTIFY_READY_CMD,
	SENS_COMM_NOTIFY_LIST_CMD,
	SENS_COMM_NOTIFY_DEBUG_CMD,
	MAX_SENS_COMM_NOTIFY_CMD,
};

struct sensor_comm_batch {
	int64_t delay;
	int64_t latency;
} __packed;

struct sensor_comm_timesync {
	int64_t host_timestamp;
	int64_t host_archcounter;
} __packed;

struct sensor_comm_share_mem {
	struct {
		uint32_t notify_cmd;
		uint32_t buffer_base;
	} base_info[4];
} __packed;

struct sensor_comm_ctrl {
	uint32_t sequence;
	uint8_t sensor_type;
	uint8_t command;
	uint8_t length;
	int8_t ret_val;
	uint8_t data[0];
} __packed;

struct sensor_comm_ack {
	uint32_t sequence;
	uint8_t sensor_type;
	uint8_t command;
	uint8_t length;
	int8_t ret_val;
} __packed;

struct data_notify {
	int32_t write_position;
	int64_t scp_timestamp;
	int64_t scp_archcounter;
} __packed;

struct sensor_comm_notify {
	uint8_t sensor_type;
	uint8_t command;
	uint8_t length;
	uint8_t sequence;
	union {
		struct data_notify dnotify;
		int32_t value[5];
	};
} __packed;

int sensor_comm_ctrl_send(struct sensor_comm_ctrl *ctrl, unsigned int size);
int sensor_comm_notify(struct sensor_comm_notify *notify);
void sensor_comm_notify_handler_register(uint8_t cmd,
		void (*f)(struct sensor_comm_notify *n, void *private_data),
		void *private_data);
void sensor_comm_notify_handler_unregister(uint8_t cmd);
int sensor_comm_init(void);
void sensor_comm_exit(void);

#endif
