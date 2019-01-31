/* linearaccityhub motion sensor driver
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

#define pr_fmt(fmt) "[rotatvechub] " fmt

#include <hwmsensor.h>
#include "fusion.h"
#include "rotatvechub.h"
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

typedef enum {
	ROTVECHUB_TRC_INFO = 0X10,
} ROTVECHUB_TRC;

static struct fusion_init_info rotatvechub_init_info;

static int rotatvec_get_data(int *x, int *y, int *z,
	int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_ROTATION_VECTOR, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*x = data.orientation_t.azimuth;
	*y = data.orientation_t.pitch;
	*z = data.orientation_t.roll;
	*scalar = data.orientation_t.scalar;
	*status = data.orientation_t.status;
	return 0;
}

static int rotatvec_open_report_data(int open)
{
	return 0;
}

static int rotatvec_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_ROTATION_VECTOR, en);
}

static int rotatvec_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_ROTATION_VECTOR, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int rotatvec_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	rotatvec_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_ROTATION_VECTOR,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int rotatvec_flush(void)
{
	return sensor_flush_to_hub(ID_ROTATION_VECTOR);
}
static int rotatvec_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == DATA_ACTION)
		err = rv_data_report(event->orientation_t.azimuth,
			event->orientation_t.pitch,
			event->orientation_t.roll, event->orientation_t.scalar,
			event->orientation_t.status,
			(int64_t)event->time_stamp);
	else if (event->flush_action == FLUSH_ACTION)
		err = rv_flush_report();

	return err;
}
static int rotatvechub_local_init(void)
{
	struct fusion_control_path ctl = { 0 };
	struct fusion_data_path data = { 0 };
	int err = 0;

	ctl.open_report_data = rotatvec_open_report_data;
	ctl.enable_nodata = rotatvec_enable_nodata;
	ctl.set_delay = rotatvec_set_delay;
	ctl.batch = rotatvec_batch;
	ctl.flush = rotatvec_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_ROTATION_VECTOR);
	if (err) {
		pr_err("register rotatvec control path err\n");
		goto exit;
	}

	data.get_data = rotatvec_get_data;
	data.vender_div = 1000000;
	err = fusion_register_data_path(&data, ID_ROTATION_VECTOR);
	if (err) {
		pr_err("register rotatvec data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_ROTATION_VECTOR,
		rotatvec_recv_data);
	if (err < 0) {
		pr_err("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
 exit:
	return -1;
}

static int rotatvechub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info rotatvechub_init_info = {
	.name = "rotatvec_hub",
	.init = rotatvechub_local_init,
	.uninit = rotatvechub_local_uninit,
};

static int __init rotatvechub_init(void)
{
	fusion_driver_add(&rotatvechub_init_info, ID_ROTATION_VECTOR);
	return 0;
}

static void __exit rotatvechub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(rotatvechub_init);
module_exit(rotatvechub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
