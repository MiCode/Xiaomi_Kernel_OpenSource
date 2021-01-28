// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[uncali_acchub] " fmt

#include <hwmsensor.h>
#include "uncali_acchub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>

static struct fusion_init_info uncali_acchub_init_info;

static int uncali_acc_get_data(int *x, int *y, int *z,
	int *scalar, int *status)
{
	return 0;
}
static int uncali_acc_open_report_data(int open)
{
	return 0;
}
static int uncali_acc_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_ACCELEROMETER_UNCALIBRATED, en);
}
static int uncali_acc_set_delay(u64 delay)
{
	return 0;
}
static int uncali_acc_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_ACCELEROMETER_UNCALIBRATED,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int uncali_acc_flush(void)
{
	return sensor_flush_to_hub(ID_ACCELEROMETER_UNCALIBRATED);
}
static int uncali_acc_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	int value[6] = {0};

	value[0] = event->uncalibrated_acc_t.x
		+ event->uncalibrated_acc_t.x_bias;
	value[1] = event->uncalibrated_acc_t.y
		+ event->uncalibrated_acc_t.y_bias;
	value[2] = event->uncalibrated_acc_t.z
		+ event->uncalibrated_acc_t.z_bias;
	value[3] = event->uncalibrated_acc_t.x_bias;
	value[4] = event->uncalibrated_acc_t.y_bias;
	value[5] = event->uncalibrated_acc_t.z_bias;
	if (event->flush_action == DATA_ACTION)
		err = uncali_acc_data_report(value,
			event->uncalibrated_mag_t.status,
			(int64_t)event->time_stamp);
	else if (event->flush_action == FLUSH_ACTION)
		err = uncali_acc_flush_report();
	return err;
}
static int uncali_acchub_local_init(void)
{
	struct fusion_control_path ctl = {0};
	struct fusion_data_path data = {0};
	int err = 0;

	ctl.open_report_data = uncali_acc_open_report_data;
	ctl.enable_nodata = uncali_acc_enable_nodata;
	ctl.set_delay = uncali_acc_set_delay;
	ctl.batch = uncali_acc_batch;
	ctl.flush = uncali_acc_flush;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
	err = fusion_register_control_path(&ctl, ID_ACCELEROMETER_UNCALIBRATED);
	if (err) {
		pr_err("register uncali_acc control path err\n");
		goto exit;
	}

	data.get_data = uncali_acc_get_data;
	data.vender_div = 1000;
	err = fusion_register_data_path(&data, ID_ACCELEROMETER_UNCALIBRATED);
	if (err) {
		pr_err("register uncali_acc data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_ACCELEROMETER_UNCALIBRATED,
		uncali_acc_recv_data);
	if (err < 0) {
		pr_err("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int uncali_acchub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info uncali_acchub_init_info = {
	.name = "uncali_acc_hub",
	.init = uncali_acchub_local_init,
	.uninit = uncali_acchub_local_uninit,
};

static int __init uncali_acchub_init(void)
{
	fusion_driver_add(&uncali_acchub_init_info,
		ID_ACCELEROMETER_UNCALIBRATED);
	return 0;
}

static void __exit uncali_acchub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(uncali_acchub_init);
module_exit(uncali_acchub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");

