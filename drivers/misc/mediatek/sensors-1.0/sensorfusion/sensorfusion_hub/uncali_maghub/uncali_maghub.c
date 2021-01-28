// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[uncali_maghub] " fmt

#include <hwmsensor.h>
#include "uncali_maghub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>

static struct fusion_init_info uncali_maghub_init_info;

static int uncali_mag_get_data(int *x, int *y, int *z,
	int *scalar, int *status)
{
	return 0;
}
static int uncali_mag_open_report_data(int open)
{
	return 0;
}
static int uncali_mag_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_MAGNETIC_UNCALIBRATED, en);
}
static int uncali_mag_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_MAGNETIC_UNCALIBRATED, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int uncali_mag_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	uncali_mag_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_MAGNETIC_UNCALIBRATED,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int uncali_mag_flush(void)
{
	return sensor_flush_to_hub(ID_MAGNETIC_UNCALIBRATED);
}
static int uncali_mag_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	int value[6] = {0};

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	value[0] = event->uncalibrated_mag_t.x;
	value[1] = event->uncalibrated_mag_t.y;
	value[2] = event->uncalibrated_mag_t.z;
	value[3] = event->uncalibrated_mag_t.x_bias;
	value[4] = event->uncalibrated_mag_t.y_bias;
	value[5] = event->uncalibrated_mag_t.z_bias;
#elif defined CONFIG_NANOHUB
	value[0] = event->uncalibrated_mag_t.x
		+ event->uncalibrated_mag_t.x_bias;
	value[1] = event->uncalibrated_mag_t.y
		+ event->uncalibrated_mag_t.y_bias;
	value[2] = event->uncalibrated_mag_t.z
		+ event->uncalibrated_mag_t.z_bias;
	value[3] = event->uncalibrated_mag_t.x_bias;
	value[4] = event->uncalibrated_mag_t.y_bias;
	value[5] = event->uncalibrated_mag_t.z_bias;
#endif
	if (event->flush_action == DATA_ACTION)
		err = uncali_mag_data_report(value,
			event->uncalibrated_mag_t.status,
			(int64_t)event->time_stamp);
	else if (event->flush_action == FLUSH_ACTION)
		err = uncali_mag_flush_report();
	return err;
}
static int uncali_maghub_local_init(void)
{
	struct fusion_control_path ctl = {0};
	struct fusion_data_path data = {0};
	int err = 0;

	ctl.open_report_data = uncali_mag_open_report_data;
	ctl.enable_nodata = uncali_mag_enable_nodata;
	ctl.set_delay = uncali_mag_set_delay;
	ctl.batch = uncali_mag_batch;
	ctl.flush = uncali_mag_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = true;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_MAGNETIC_UNCALIBRATED);
	if (err) {
		pr_err("register uncali_mag control path err\n");
		goto exit;
	}

	data.get_data = uncali_mag_get_data;
	data.vender_div = 100;
	err = fusion_register_data_path(&data, ID_MAGNETIC_UNCALIBRATED);
	if (err) {
		pr_err("register uncali_mag data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_MAGNETIC_UNCALIBRATED,
		uncali_mag_recv_data);
	if (err < 0) {
		pr_err("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int uncali_maghub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info uncali_maghub_init_info = {
	.name = "uncali_mag_hub",
	.init = uncali_maghub_local_init,
	.uninit = uncali_maghub_local_uninit,
};

int __init uncali_maghub_init(void)
{
	fusion_driver_add(&uncali_maghub_init_info, ID_MAGNETIC_UNCALIBRATED);
	return 0;
}

void __exit uncali_maghub_exit(void)
{
	pr_debug("%s\n", __func__);
}

