/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

/* als_strm sensor driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

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
#include "als_factory_stream.h"
#include "situation.h"

#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


static int als_strm_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_ALS_FACTORY_STRM, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*probability = data.gesture_data_t.probability;
	pr_err("recv ipi: timestamp: %lld, probability: %d!\n", time_stamp,
		*probability);
	return 0;
}

static int als_strm_open_report_data(int open)
{
	int ret = 0;

	pr_info("%s : enable=%d\n", __func__, open);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_ALS_FACTORY_STRM, 120);
#elif defined CONFIG_NANOHUB

#else

#endif

	ret = sensor_enable_to_hub(ID_ALS_FACTORY_STRM, open);
	return ret;
}

static int als_strm_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	pr_info("%s : flag=%d\n", __func__, flag);

	return sensor_batch_to_hub(ID_ALS_FACTORY_STRM, flag, samplingPeriodNs,
			maxBatchReportLatencyNs);
}

static int als_strm_recv_data(struct data_unit_t *event, void *reserved)
{
	int32_t value[3] = {0};
	int err = 0;
	pr_err("als_factory_strm_recv_data action: %d\n", event->flush_action);

	if (event->flush_action == FLUSH_ACTION)
		err = situation_flush_report(ID_ALS_FACTORY_STRM);
	else if (event->flush_action == DATA_ACTION) {
		value[0] = event->als_factory_strm_event.data[0];
		value[1] = event->als_factory_strm_event.data[1];
		value[2] = event->als_factory_strm_event.data[2];
		pr_err("als_factory_strm_recv_data : { %d, %d, %d }\n", value[0], value[1], value[2]);
		err = als_factory_strm_data_report_t(value, (int64_t)event->time_stamp);
	}
	return err;
}

static int als_strm_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = als_strm_open_report_data;
	ctl.batch = als_strm_batch;
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_ALS_FACTORY_STRM);
	if (err) {
		pr_err("register als_strm control path err\n");
		goto exit;
	}

	data.get_data = als_strm_get_data;
	err = situation_register_data_path(&data, ID_ALS_FACTORY_STRM);
	if (err) {
		pr_err("register als_strm data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_ALS_FACTORY_STRM, als_strm_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int als_strm_local_uninit(void)
{
	return 0;
}

static struct situation_init_info als_strm_init_info = {
	.name = "als_strm_hub",
	.init = als_strm_local_init,
	.uninit = als_strm_local_uninit,
};

static int __init als_strm_init(void)
{
	situation_driver_add(&als_strm_init_info, ID_ALS_FACTORY_STRM);
	return 0;
}

static void __exit als_strm_exit(void)
{
	pr_info("als_strm exit\n");
}

module_init(als_strm_init);
module_exit(als_strm_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("xiongyucong@xiaomi.com");

