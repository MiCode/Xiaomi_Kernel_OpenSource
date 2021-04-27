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
#include <linux/ioctl.h>
#include <linux/types.h>

#include "hf_sensor_type.h"
#include "hf_sensor_io.h"

#define HF_MANAGER_IO_IN_PROGRESS 0
#define HF_MANAGER_IO_READY       1

#define HF_DEVICE_IO_SYNC  0
#define HF_DEVICE_IO_ASYNC 1

#define HF_DEVICE_IO_INTERRUPT 0
#define HF_DEVICE_IO_POLLING   1

#define HF_CLIENT_FIFO_SIZE 128

struct sensor_state {
	bool enable;
	bool bias;
	bool cali;
	bool temp;
	bool test;
	bool raw;
	int64_t delay;
	int64_t latency;
	atomic_t flush;
	atomic64_t start_time;
};

struct hf_core {
	struct mutex manager_lock;
	struct list_head manager_list;
	struct sensor_state state[SENSOR_TYPE_SENSOR_MAX];

	spinlock_t client_lock;
	struct list_head client_list;

	struct mutex device_lock;
	struct list_head device_list;

	struct kthread_worker kworker;
};

struct hf_device {
	int (*sample)(struct hf_device *hfdev);
	int (*enable)(struct hf_device *hfdev, int sensor_type, int en);
	int (*batch)(struct hf_device *hfdev, int sensor_type,
		int64_t delay, int64_t latency);
	int (*flush)(struct hf_device *hfdev, int sensor_type);
	int (*calibration)(struct hf_device *hfdev, int sensor_type);
	int (*config_cali)(struct hf_device *hfdev,
		int sensor_type, void *data, uint8_t length);
	int (*selftest)(struct hf_device *hfdev, int sensor_type);
	int (*rawdata)(struct hf_device *hfdev, int sensor_type, int en);
	int (*debug)(struct hf_device *hfdev, int sensor_type,
		uint8_t *buffer, unsigned int len);
	int (*custom_cmd)(struct hf_device *hfdev, int sensor_type,
		struct custom_cmd *cust_cmd);

	unsigned char device_poll;
	unsigned char device_bus;
	struct sensor_info *support_list;
	unsigned int support_size;

	char *dev_name;

	struct hf_manager *manager;
	struct list_head list;
	bool ready;

	void *private_data;
};

struct hf_client_fifo {
	spinlock_t buffer_lock;
	unsigned int head;
	unsigned int tail;
	unsigned int bufsize;
	unsigned int buffull;
	int64_t hang_begin;
	int64_t client_active;
	int64_t last_time_stamp[SENSOR_TYPE_SENSOR_MAX];
	struct hf_manager_event *buffer;
	wait_queue_head_t wait;
};

struct hf_manager {
	struct list_head list;
	struct tasklet_struct io_work_tasklet;
	struct kthread_work io_kthread_work;
	struct hrtimer io_poll_timer;
	atomic64_t io_poll_interval;
	atomic64_t timestamp;
	atomic_t io_enabled;
	unsigned long flags;
	struct hf_device *hf_dev;
	struct hf_core *core;

	int (*report)(struct hf_manager *manager,
		struct hf_manager_event *event);
	void (*complete)(struct hf_manager *manager);
	void (*interrupt)(struct hf_manager *manager, int64_t timestamp);
};

struct hf_client {
	struct list_head list;
	struct hf_client_fifo hf_fifo;
	struct sensor_state request[SENSOR_TYPE_SENSOR_MAX];
	spinlock_t request_lock;
	struct hf_core *core;

	/* record process info */
	char proc_comm[TASK_COMM_LEN]; /* process name */
	pid_t leader_pid;              /* process pid */
	pid_t pid;                     /* control thread pid */
	pid_t ppid;                    /* poll thread pid */
};

#define set_interrupt_timestamp(m, t) (atomic64_set(&m->timestamp, t))
#define get_interrupt_timestamp(m) (atomic64_read(&m->timestamp))

static inline void hf_device_set_private_data(struct hf_device *device,
		void *data)
{
	device->private_data = data;
}

static inline void *hf_device_get_private_data(struct hf_device *device)
{
	return device->private_data;
}

int hf_device_register(struct hf_device *device);
void hf_device_unregister(struct hf_device *device);
int hf_manager_create(struct hf_device *device);
void hf_manager_destroy(struct hf_manager *manager);
int hf_device_register_manager_create(struct hf_device *device);
void hf_device_unregister_manager_destroy(struct hf_device *device);
void coordinate_map(unsigned char direction, int32_t *data);
struct hf_client *hf_client_create(void);
void hf_client_destroy(struct hf_client *client);
int hf_client_find_sensor(struct hf_client *client, uint8_t sensor_type);
int hf_client_get_sensor_info(struct hf_client *client,
		uint8_t sensor_type, struct sensor_info *info);
int hf_client_request_sensor_cali(struct hf_client *client,
		uint8_t sensor_type, unsigned int cmd, bool status);
int hf_client_control_sensor(struct hf_client *client,
		struct hf_manager_cmd *cmd);
int hf_client_poll_sensor_timeout(struct hf_client *client,
		struct hf_manager_event *data, int count, long timeout);
#define hf_client_poll_sensor(client, data, count)		\
	hf_client_poll_sensor_timeout(client, data, count,	\
		MAX_SCHEDULE_TIMEOUT)
int hf_client_custom_cmd(struct hf_client *client,
		uint8_t sensor_type, struct custom_cmd *cust_cmd);

#endif
