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

/* HIGH_FREQUENCY_SENSOR_GYRO udps report to hal */
#define HIGH_FREQUENCY_SENSOR_GYRO 1

#define HIGH_FREQUENCY_ACCURANCY_UNRELIALE 0
#define HIGH_FREQUENCY_ACCURANCY_LOW       1
#define HIGH_FREQUENCY_ACCURANCY_MEDIUM    2
#define HIGH_FREQUENCY_ACCURANCY_HIGH      3

#define HF_MANAGER_SENSOR_DISABLE 0
#define HF_MANAGER_SENSOR_ENABLE  1

#define HF_MANAGER_IO_IN_PROGRESS 0
#define HF_MANAGER_IO_READY       1

#define HF_DEVICE_IO_SYNC  0
#define HF_DEVICE_IO_ASYNC 1

#define HF_DEVICE_IO_INTERRUPT 0
#define HF_DEVICE_IO_POLLING   1

#define HF_MANAGER_FIFO_SIZE 128

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

struct hf_device {
	int (*enable)(struct hf_device *hfdev, int en);
	int (*batch)(struct hf_device *hfdev, int64_t delay, int64_t latency);
	int (*sample)(struct hf_device *hfdev);

	char *dev_name;
	unsigned char device_poll;
	unsigned char device_bus;
	unsigned char sensor_id;

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

struct hf_manager_fifo {
	spinlock_t buffer_lock;
	unsigned int head;
	unsigned int tail;
	unsigned int bufsize;
	unsigned int buffull;
	struct hf_manager_event *buffer;
	wait_queue_head_t wait;
};

struct hf_manager {
	struct list_head list;
	struct tasklet_struct io_work_tasklet;
	struct hrtimer io_poll_timer;
	ktime_t io_poll_interval;
	unsigned long flags;
	struct hf_device *hf_dev;
	struct hf_manager_fifo *hf_fifo;

	void (*complete)(struct hf_manager *manager,
		struct hf_manager_event *event);
	void (*interrupt)(struct hf_manager *manager);
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
