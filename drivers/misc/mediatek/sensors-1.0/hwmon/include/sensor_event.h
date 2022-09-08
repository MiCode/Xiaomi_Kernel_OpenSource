/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SENSOR_EVENT_H
#define _SENSOR_EVENT_H
#include <linux/poll.h>
#include <linux/ratelimit.h>

#define CONTINUE_SENSOR_BUF_SIZE	2048
#define BIO_SENSOR_BUF_SIZE	0x20000 /* ((512 + 1024 + 1024) * 60) */
#define OTHER_SENSOR_BUF_SIZE	16
enum flushAction {
	DATA_ACTION,
	FLUSH_ACTION,
	BIAS_ACTION,
	CALI_ACTION,
	TEMP_ACTION,
	TEST_ACTION,
};
struct sensor_event {
	int64_t time_stamp;
	int8_t handle;
	int8_t flush_action;
	int8_t status;
	int8_t reserved;
	union {
		int32_t word[6];
		int8_t byte[0];
	};
} __packed;
ssize_t sensor_event_read(unsigned char handle, struct file *file,
	char __user *buffer,
			  size_t count, loff_t *ppos);
unsigned int sensor_event_poll(unsigned char handle, struct file *file,
	poll_table *wait);
int sensor_input_event(unsigned char handle,
			 const struct sensor_event *event);
unsigned int sensor_event_register(unsigned char handle);
unsigned int sensor_event_deregister(unsigned char handle);
#endif
