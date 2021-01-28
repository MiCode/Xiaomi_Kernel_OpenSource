// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ancallhub] " fmt

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <hwmsensor.h>
#include <sensors_io.h>
#include "ancallhub.h"
#include <situation.h>
#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>

enum ANCALLHUB_TRC {
	ANCALLHUBH_TRC_INFO = 0X10,
};

static struct situation_init_info ancallhub_init_info;

static int answer_call_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_ANSWER_CALL, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	*probability	= data.gesture_data_t.probability;
	return 0;
}
static int answer_call_open_report_data(int open)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_ANSWER_CALL, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	pr_debug("%s : type=%d, open=%d\n", __func__, ID_ANSWER_CALL, open);
	ret = sensor_enable_to_hub(ID_ANSWER_CALL, open);
	return ret;
}
static int answer_call_gesture_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_ANSWER_CALL,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}
static int answer_call_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		pr_debug("answer_call do not support flush\n");
	else if (event->flush_action == DATA_ACTION)
		err = situation_notify_t(ID_ANSWER_CALL,
				(int64_t)event->time_stamp);
	return err;
}

static int ancallhub_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = answer_call_open_report_data;
	ctl.batch = answer_call_gesture_batch;
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_ANSWER_CALL);
	if (err) {
		pr_err("register answer_call control path err\n");
		goto exit;
	}

	data.get_data = answer_call_get_data;
	err = situation_register_data_path(&data, ID_ANSWER_CALL);
	if (err) {
		pr_err("register answer_call data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_ANSWER_CALL,
		answer_call_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int ancallhub_local_uninit(void)
{
	return 0;
}

static struct situation_init_info ancallhub_init_info = {
	.name = "answer_call_hub",
	.init = ancallhub_local_init,
	.uninit = ancallhub_local_uninit,
};

int __init ancallhub_init(void)
{
	situation_driver_add(&ancallhub_init_info, ID_ANSWER_CALL);
	return 0;
}

void __exit ancallhub_exit(void)
{
	pr_debug("%s\n", __func__);
}

