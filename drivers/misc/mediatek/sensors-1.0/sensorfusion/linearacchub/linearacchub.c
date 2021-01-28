// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[lacchub] " fmt

#include <hwmsensor.h>
#include "linearacchub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>

enum LNACCHUB_TRC {
	LNACCHUB_TRC_INFO = 0X10,
};

static struct fusion_init_info linearacchub_init_info;
static int linearacc_get_data(int *x, int *y, int *z,
	int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_LINEAR_ACCELERATION, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp				= data.time_stamp;
	*x = data.accelerometer_t.x;
	*y = data.accelerometer_t.y;
	*z = data.accelerometer_t.z;
	*status = data.accelerometer_t.status;
	return 0;
}
static int linearacc_open_report_data(int open)
{
	return 0;
}
static int linearacc_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_LINEAR_ACCELERATION, en);
}
static int linearacc_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_LINEAR_ACCELERATION, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int linearacc_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	linearacc_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_LINEAR_ACCELERATION,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int linearacc_flush(void)
{
	return sensor_flush_to_hub(ID_LINEAR_ACCELERATION);
}
static int linearacc_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == DATA_ACTION)
		err = la_data_report(event->accelerometer_t.x,
			event->accelerometer_t.y, event->accelerometer_t.z,
			event->accelerometer_t.status,
			(int64_t)event->time_stamp);
	else if (event->flush_action == FLUSH_ACTION)
		err = la_flush_report();

	return err;
}
static int linearacchub_local_init(void)
{
	struct fusion_control_path ctl = {0};
	struct fusion_data_path data = {0};
	int err = 0;

	ctl.open_report_data = linearacc_open_report_data;
	ctl.enable_nodata = linearacc_enable_nodata;
	ctl.set_delay = linearacc_set_delay;
	ctl.batch = linearacc_batch;
	ctl.flush = linearacc_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_LINEAR_ACCELERATION);
	if (err) {
		pr_err("register linearacc control path err\n");
		goto exit;
	}

	data.get_data = linearacc_get_data;
	data.vender_div = 1000;
	err = fusion_register_data_path(&data, ID_LINEAR_ACCELERATION);
	if (err) {
		pr_err("register linearacc data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_LINEAR_ACCELERATION,
		linearacc_recv_data);
	if (err < 0) {
		pr_err("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int linearacchub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info linearacchub_init_info = {
	.name = "linearacc_hub",
	.init = linearacchub_local_init,
	.uninit = linearacchub_local_uninit,
};

static int __init linearacchub_init(void)
{
	fusion_driver_add(&linearacchub_init_info, ID_LINEAR_ACCELERATION);
	return 0;
}

static void __exit linearacchub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(linearacchub_init);
module_exit(linearacchub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
