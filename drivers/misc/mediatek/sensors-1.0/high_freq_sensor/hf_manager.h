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

#ifndef _HF_SENSOR_MANAGER_H_
#define _HF_SENSOR_MANAGER_H_

#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>

enum {
	/* HIGH_FREQUENCY_SENSOR_GYRO udps report to hal */
	HIGH_FREQUENCY_SENSOR_GYRO = 1,
	HIGH_FREQUENCY_SENSOR_ACCEL,
	HIGH_FREQUENCY_SENSOR_MAG,
	HIGH_FREQUENCY_SENSOR_PRESSURE,
	HIGH_FREQUENCY_SENSOR_MAX,
};

#define HIGH_FREQUENCY_ACCURANCY_UNRELIALE 0
#define HIGH_FREQUENCY_ACCURANCY_LOW       1
#define HIGH_FREQUENCY_ACCURANCY_MEDIUM    2
#define HIGH_FREQUENCY_ACCURANCY_HIGH      3

#define HF_MANAGER_SENSOR_DISABLE          0
#define HF_MANAGER_SENSOR_ENABLE           1
#define HF_MANAGER_SENSOR_FLUSH            2
#define HF_MANAGER_SENSOR_CALIBRATION      3
#define HF_MANAGER_SENSOR_CONFIG_CALI      4
#define HF_MANAGER_SENSOR_SELFTEST         5

#define HF_MANAGER_IO_IN_PROGRESS 0
#define HF_MANAGER_IO_READY       1

#define HF_DEVICE_IO_SYNC  0
#define HF_DEVICE_IO_ASYNC 1

#define HF_DEVICE_IO_INTERRUPT 0
#define HF_DEVICE_IO_POLLING   1

#define HF_MANAGER_FIFO_SIZE 128

#define HF_MANAGER_SENSOR_REGISTER_STATUS _IOWR('a', 1, unsigned int)

struct coordinate {
	int8_t sign[3];
	uint8_t map[3];
};

struct hf_manager_cmd {
	uint8_t sensor_id;
	uint8_t action;
	int64_t delay;
	int64_t latency;
} __packed;

struct sensor_state {
	uint8_t action;
	int64_t delay;
	int64_t latency;
	int64_t start_time;
} __packed;

struct hf_device {
	int (*sample)(struct hf_device *hfdev);
	int (*enable)(struct hf_device *hfdev, int sensor_id, int en);
	int (*batch)(struct hf_device *hfdev, int sensor_id,
		int64_t delay, int64_t latency);
	int (*flush)(struct hf_device *hfdev, int sensor_id);
	int (*calibration)(struct hf_device *hfdev, int sensor_id);
	int (*config_cali)(struct hf_device *hfdev, int sensor_id);
	int (*selftest)(struct hf_device *hfdev, int sensor_id);

	char *dev_name;
	unsigned char device_poll;
	unsigned char device_bus;

	unsigned char *support_list;
	unsigned int support_size;

	struct hf_manager *manager;
	void *private_data;
};

struct hf_manager_event {
	int64_t timestamp;
	uint8_t sensor_id;
	uint8_t accurancy;
	uint8_t action;
	uint8_t reserved;
	union {
		int32_t word[6];
		int8_t byte[0];
	};
} __packed;

struct hf_client_fifo {
	spinlock_t buffer_lock;
	unsigned int head;
	unsigned int tail;
	unsigned int bufsize;
	unsigned int buffull;
	int64_t last_time_stamp[HIGH_FREQUENCY_SENSOR_MAX];
	struct hf_manager_event *buffer;
	wait_queue_head_t wait;
};

struct hf_manager {
	struct list_head list;
	struct tasklet_struct io_work_tasklet;
	struct kthread_work io_kthread_work;
	struct hrtimer io_poll_timer;
	ktime_t io_poll_interval;
	bool io_enabled;
	unsigned long flags;
	struct hf_device *hf_dev;

	int (*report)(struct hf_manager *manager,
		struct hf_manager_event *event);
	void (*complete)(struct hf_manager *manager);
	void (*interrupt)(struct hf_manager *manager);
};

struct hf_client {
	struct list_head list;
	struct hf_client_fifo hf_fifo;
	struct sensor_state request[HIGH_FREQUENCY_SENSOR_MAX];

	/* record process info */
	char proc_comm[TASK_COMM_LEN];
	pid_t leader_pid;
	pid_t pid;
};

static inline void hf_device_set_private_data(struct hf_device *device,
		void *data)
{
	device->private_data = data;
}

static inline void *hf_device_get_private_data(struct hf_device *device)
{
	return device->private_data;
}

int hf_manager_create(struct hf_device *device);
int hf_manager_destroy(struct hf_manager *manager);
void coordinate_map(unsigned char direction, int32_t *data);

#endif
