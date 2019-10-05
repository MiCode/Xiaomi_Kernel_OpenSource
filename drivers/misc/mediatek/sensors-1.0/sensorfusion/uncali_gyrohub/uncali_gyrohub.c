/* uncali_gyrohub motion sensor driver
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

#define pr_fmt(fmt) "[uncali_gyrohub] " fmt

#include <hwmsensor.h>
#include "uncali_gyrohub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

static struct fusion_init_info uncali_gyrohub_init_info;

static int uncali_gyro_get_data(int *x, int *y, int *z,
	int *scalar, int *status)
{
	return 0;
}
static int uncali_gyro_open_report_data(int open)
{
	return 0;
}
static int uncali_gyro_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GYROSCOPE_UNCALIBRATED, en);
}
static int uncali_gyro_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GYROSCOPE_UNCALIBRATED, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int uncali_gyro_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	uncali_gyro_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_GYROSCOPE_UNCALIBRATED,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int uncali_gyro_flush(void)
{
	return sensor_flush_to_hub(ID_GYROSCOPE_UNCALIBRATED);
}
static int uncali_gyro_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	int value[6] = {0};
	int value_temp[6] = {0};

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	value[0] = event->uncalibrated_gyro_t.x;
	value[1] = event->uncalibrated_gyro_t.y;
	value[2] = event->uncalibrated_gyro_t.z;
	value[3] = event->uncalibrated_gyro_t.x_bias;
	value[4] = event->uncalibrated_gyro_t.y_bias;
	value[5] = event->uncalibrated_gyro_t.z_bias;
#elif defined CONFIG_NANOHUB
	value[0] = event->uncalibrated_gyro_t.x
		+ event->uncalibrated_gyro_t.x_bias;
	value[1] = event->uncalibrated_gyro_t.y
		+ event->uncalibrated_gyro_t.y_bias;
	value[2] = event->uncalibrated_gyro_t.z
		+ event->uncalibrated_gyro_t.z_bias;
	value[3] = event->uncalibrated_gyro_t.x_bias;
	value[4] = event->uncalibrated_gyro_t.y_bias;
	value[5] = event->uncalibrated_gyro_t.z_bias;
	value_temp[0] = event->uncalibrated_gyro_t.temperature;
	value_temp[1] = event->uncalibrated_gyro_t.temp_result;
#endif
	if (event->flush_action == DATA_ACTION) {
		err = uncali_gyro_data_report(value,
			event->uncalibrated_gyro_t.status,
			(int64_t)event->time_stamp);
		if (value_temp[0] != 0)
			uncali_gyro_temperature_data_report(value_temp,
			event->uncalibrated_gyro_t.status,
			(int64_t)event->time_stamp);
	} else if (event->flush_action == FLUSH_ACTION)
		err = uncali_gyro_flush_report();

	return err;
}
static int uncali_gyrohub_local_init(void)
{
	struct fusion_control_path ctl = {0};
	struct fusion_data_path data = {0};
	int err = 0;

	ctl.open_report_data = uncali_gyro_open_report_data;
	ctl.enable_nodata = uncali_gyro_enable_nodata;
	ctl.set_delay = uncali_gyro_set_delay;
	ctl.batch = uncali_gyro_batch;
	ctl.flush = uncali_gyro_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_GYROSCOPE_UNCALIBRATED);
	if (err) {
		pr_err("register uncali_gyro control path err\n");
		goto exit;
	}

	data.get_data = uncali_gyro_get_data;
	data.vender_div = 7505747;
	err = fusion_register_data_path(&data, ID_GYROSCOPE_UNCALIBRATED);
	if (err) {
		pr_err("register uncali_gyro data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_GYROSCOPE_UNCALIBRATED,
		uncali_gyro_recv_data);
	if (err < 0) {
		pr_err("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int uncali_gyrohub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info uncali_gyrohub_init_info = {
	.name = "uncali_gyro_hub",
	.init = uncali_gyrohub_local_init,
	.uninit = uncali_gyrohub_local_uninit,
};

static int __init uncali_gyrohub_init(void)
{
	fusion_driver_add(&uncali_gyrohub_init_info, ID_GYROSCOPE_UNCALIBRATED);
	return 0;
}

static void __exit uncali_gyrohub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(uncali_gyrohub_init);
module_exit(uncali_gyrohub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
