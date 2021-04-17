// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[sensor_comm] " fmt

#include <linux/module.h>
#include <linux/err.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include "sensor_comm.h"

static atomic_t sensor_comm_sequence;

static int sensor_comm_ctrl_send_retry(struct sensor_comm_ctrl *ctrl,
		unsigned int size)
{
	int ret = 0;
	struct sensor_comm_ack ack;

	memset(&ack, 0, sizeof(ack));
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

	ctrl->sequence = atomic_inc_return(&sensor_comm_sequence);

	do {
		ret = sensor_comm_ctrl_send_retry(ctrl, size);
	} while (retry++ < max_retry && ret < 0);

	return ret;
}

static int __init sensor_comm_init(void)
{
	atomic_set(&sensor_comm_sequence, 0);
	ipi_comm_init();
	return 0;
}

static void __exit sensor_comm_exit(void)
{

}

module_init(sensor_comm_init);
module_exit(sensor_comm_exit);
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("test driver");
MODULE_LICENSE("GPL");
