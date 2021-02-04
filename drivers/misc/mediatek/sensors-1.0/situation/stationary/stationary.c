/* stationary gesture sensor driver
 *
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

#define pr_fmt(fmt) "[stationary] " fmt

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
#include "situation.h"
#include "stationary.h"
#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

static struct situation_init_info stat_init_info;
static int stat_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_STATIONARY_DETECT, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	*probability	= data.gesture_data_t.probability;
	return 0;
}
static int stat_open_report_data(int open)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_STATIONARY_DETECT, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	pr_debug("%s : type=%d, open=%d\n",
		__func__, ID_STATIONARY_DETECT, open);
	ret = sensor_enable_to_hub(ID_STATIONARY_DETECT, open);
	return ret;
}
static int stat_batch(int flag, int64_t samplingPeriodNs,
	int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_STATIONARY_DETECT,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}
static int stat_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		pr_debug("stat do not support flush\n");
	else if (event->flush_action == DATA_ACTION)
		err = situation_notify(ID_STATIONARY_DETECT);
	return err;
}

static int stat_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = stat_open_report_data;
	ctl.batch = stat_batch;
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_STATIONARY_DETECT);
	if (err) {
		pr_err("register stationary control path err\n");
		goto exit;
	}

	data.get_data = stat_get_data;
	err = situation_register_data_path(&data, ID_STATIONARY_DETECT);
	if (err) {
		pr_err("register stationary data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_STATIONARY_DETECT,
		stat_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int stat_local_uninit(void)
{
	return 0;
}

static struct situation_init_info stat_init_info = {
	.name = "stat_hub",
	.init = stat_local_init,
	.uninit = stat_local_uninit,
};

static int __init stat_init(void)
{
	situation_driver_add(&stat_init_info, ID_STATIONARY_DETECT);
	return 0;
}

static void __exit stat_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(stat_init);
module_exit(stat_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Stationary Gesture driver");
MODULE_AUTHOR("qiangming.xia@mediatek.com");
