// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[glghub] " fmt

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
#include "glance_gesture.h"
#include <situation.h>
#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"

enum GLGHUB_TRC {
	GLGHUBH_TRC_INFO = 0X10,
};

static struct situation_init_info glghub_init_info;

static int glance_gesture_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_GLANCE_GESTURE, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	*probability	= data.gesture_data_t.probability;
	return 0;
}
static int glance_gesture_open_report_data(int open)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_GLANCE_GESTURE, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	ret = sensor_enable_to_hub(ID_GLANCE_GESTURE, open);
	return ret;
}
static int glance_gesture_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_GLANCE_GESTURE,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}
static int glance_gesture_recv_data(struct data_unit_t *event,
	void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		pr_debug("glance_gesture do not support flush\n");
	else if (event->flush_action == DATA_ACTION)
		err = situation_notify_t(ID_GLANCE_GESTURE,
				(int64_t)event->time_stamp);
	return err;
}

static int glghub_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = glance_gesture_open_report_data;
	ctl.batch = glance_gesture_batch;
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_GLANCE_GESTURE);
	if (err) {
		pr_err("register glance_gesture control path err\n");
		goto exit;
	}

	data.get_data = glance_gesture_get_data;
	err = situation_register_data_path(&data, ID_GLANCE_GESTURE);
	if (err) {
		pr_err("register glance_gesture data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_GLANCE_GESTURE,
		glance_gesture_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int glghub_local_uninit(void)
{
	return 0;
}

static struct situation_init_info glghub_init_info = {
	.name = "glance_gesture_hub",
	.init = glghub_local_init,
	.uninit = glghub_local_uninit,
};

int __init glghub_init(void)
{
	situation_driver_add(&glghub_init_info, ID_GLANCE_GESTURE);
	return 0;
}

void __exit glghub_exit(void)
{
	pr_debug("%s\n", __func__);
}

