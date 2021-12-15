// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "sensor_comm " fmt

#include <linux/err.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/timekeeping.h>

#include "tiny_crc8.h"
#include "sensor_comm.h"
#include "ready.h"

struct sensor_notify_handle {
	void (*handler)(struct sensor_comm_notify *n, void *private_data);
	void *private_data;
};

static bool scp_status;
static atomic_t sensor_comm_sequence;
static
struct sensor_notify_handle sens_notify_handle[MAX_SENS_COMM_NOTIFY_CMD];

static void sensor_comm_notify_handler(int id, void *data, unsigned int len)
{
	uint8_t crc = 0;
	struct sensor_comm_notify *notify = data;
	struct sensor_notify_handle *handle = NULL;

	crc = tiny_crc8((uint8_t *)notify, offsetof(typeof(*notify), crc8));
	if (notify->crc8 != crc) {
		pr_err("unrecognized packet %u %u %u %u %u %u\n",
			notify->sequence, notify->sensor_type, notify->command,
			notify->length, notify->crc8, crc);
		return;
	}
	if (sizeof(*notify) != len)
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
	uint8_t crc = 0;
	struct sensor_comm_ack ack;

	memset(&ack, 0, sizeof(ack));
	/* safe sequence given by atomic, round from 0 to 255 */
	ctrl->sequence = atomic_inc_return(&sensor_comm_sequence);
	ctrl->crc8 = tiny_crc8((uint8_t *)ctrl, offsetof(typeof(*ctrl), crc8));
	ret = ipi_comm_sync(get_ctrl_id(), (unsigned char *)ctrl, size,
		(unsigned char *)&ack, sizeof(ack));
	if (ret < 0)
		return ret;
	crc = tiny_crc8((uint8_t *)&ack, offsetof(typeof(ack), crc8));
	if (ack.crc8 != crc) {
		pr_err("unrecognized packet %u %u %u %u %u %u\n",
			ack.sequence, ack.sensor_type, ack.command,
			ack.ret_val, ack.crc8, crc);
		return -EBADMSG;
	}
	if (ctrl->sequence != ack.sequence)
		return -EILSEQ;
	if (ctrl->sensor_type != ack.sensor_type)
		return -EPROTO;
	if (ctrl->command != ack.command)
		return -EPROTO;
	if (ack.ret_val < 0)
		return -EREMOTEIO;
	return 0;
}

int sensor_comm_ctrl_send(struct sensor_comm_ctrl *ctrl, unsigned int size)
{
	int retry = 0, ret = 0;
	const int max_retry = 10;
	const int64_t timeout = 10000000000LL;
	int64_t start_time = 0, duration = 0;

	start_time = ktime_get_boottime_ns();
	if (!READ_ONCE(scp_status)) {
		pr_err_ratelimited("dropped comm %u %u\n",
			ctrl->sensor_type, ctrl->command);
		return 0;
	}

	do {
		ret = sensor_comm_ctrl_seq_send(ctrl, size);
	} while (retry++ < max_retry && ret < 0);

	duration = ktime_get_boottime_ns() - start_time;
	if (duration > timeout)
		pr_notice("running time %lld, type %u, cmd %u, retries %d\n",
			duration, ctrl->sensor_type, ctrl->command, retry);
	return ret;
}

int sensor_comm_notify(struct sensor_comm_notify *notify)
{
	if (!READ_ONCE(scp_status)) {
		pr_err_ratelimited("dropped comm %u %u\n",
			notify->sensor_type, notify->command);
		return 0;
	}

	notify->crc8 =
		tiny_crc8((uint8_t *)notify, offsetof(typeof(*notify), crc8));
	return ipi_comm_noack(get_notify_id(), (unsigned char *)notify,
		sizeof(*notify));
}

/*
 * no need check scp_status due to send ready to scp.
 */
int sensor_comm_notify_bypass(struct sensor_comm_notify *notify)
{
	notify->crc8 =
		tiny_crc8((uint8_t *)notify, offsetof(typeof(*notify), crc8));
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
	WRITE_ONCE(scp_status, !!event);
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
