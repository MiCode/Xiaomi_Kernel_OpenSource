// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[sensor_comm] " fmt

#include <linux/err.h>
#include <linux/atomic.h>
#include <linux/string.h>

#include "sensor_comm.h"
#include "ready.h"

struct sensor_notify_handle {
	void (*handler)(struct sensor_comm_notify *n, void *private_data);
	void *private_data;
};

static atomic_t sensor_comm_sequence;
static
struct sensor_notify_handle sens_notify_handle[MAX_SENS_COMM_NOTIFY_CMD];

static void sensor_comm_notify_handler(int id, void *data, unsigned int len)
{
	struct sensor_comm_notify *notify = data;
	struct sensor_notify_handle *handle = NULL;

	if (sizeof(struct sensor_comm_notify) != len)
		return;
	if (notify->command >= MAX_SENS_COMM_NOTIFY_CMD)
		return;
	handle = &sens_notify_handle[notify->command];
	if (!handle->handler)
		return;
	handle->handler(notify, handle->private_data);
}

static int sensor_comm_ctrl_seq_send(struct sensor_comm_ctrl *ctrl,
		unsigned int size)
{
	int ret = 0;
	struct sensor_comm_ack ack;

	memset(&ack, 0, sizeof(ack));
	ctrl->sequence = atomic_inc_return(&sensor_comm_sequence);
	ret = ipi_comm_sync(get_ctrl_id(), (unsigned char *)ctrl, size,
		(unsigned char *)&ack, sizeof(ack));
	if (ret < 0)
		return ret;
	if (ctrl->sequence != ack.sequence)
		return -EIO;
	if (ctrl->sensor_type != ack.sensor_type)
		return -EIO;
	if (ctrl->command != ack.command)
		return -EIO;
	if (ctrl->length != ack.length)
		return -EIO;
	if (ack.ret_val < 0)
		return -EIO;
	return 0;
}

int sensor_comm_ctrl_send(struct sensor_comm_ctrl *ctrl, unsigned int size)
{
	int retry = 0, ret = 0;
	const int max_retry = 10;

	do {
		ret = sensor_comm_ctrl_seq_send(ctrl, size);
	} while (retry++ < max_retry && ret < 0);

	return ret;
}

int sensor_comm_notify(struct sensor_comm_notify *notify)
{
	return ipi_comm_noack(get_notify_id(), (unsigned char *)notify,
		sizeof(*notify));
}

void sensor_comm_notify_handler_register(uint8_t cmd,
		void (*f)(struct sensor_comm_notify *n, void *private_data),
		void *private_data)
{
	if (cmd >= MAX_SENS_COMM_NOTIFY_CMD)
		return;

	sens_notify_handle[cmd].private_data = private_data;
	sens_notify_handle[cmd].handler = f;
}

void sensor_comm_notify_handler_unregister(uint8_t cmd)
{
	if (cmd >= MAX_SENS_COMM_NOTIFY_CMD)
		return;

	sens_notify_handle[cmd].handler = NULL;
	sens_notify_handle[cmd].private_data = NULL;
}

static int sensor_comm_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	return NOTIFY_DONE;
}

static struct notifier_block sensor_comm_ready_notifier = {
	.notifier_call = sensor_comm_ready_notifier_call,
	.priority = READY_HIGHESTPRI,
};

int sensor_comm_init(void)
{
	atomic_set(&sensor_comm_sequence, 0);
	ipi_comm_init();
	ipi_comm_notify_handler_register(sensor_comm_notify_handler);
	sensor_ready_notifier_chain_register(&sensor_comm_ready_notifier);
	return 0;
}

void sensor_comm_exit(void)
{
	sensor_ready_notifier_chain_unregister(&sensor_comm_ready_notifier);
	ipi_comm_notify_handler_unregister();
	ipi_comm_exit();
}
