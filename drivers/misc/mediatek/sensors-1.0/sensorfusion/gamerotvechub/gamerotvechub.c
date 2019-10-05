/* gamerotvechub motion sensor driver
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

#define pr_fmt(fmt) "[gamerotvechub] " fmt

#include <hwmsensor.h>
#include "gamerotvechub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

static struct fusion_init_info gamerotvechub_init_info;

static int gamerotvec_get_data(int *x, int *y, int *z,
	int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_GAME_ROTATION_VECTOR, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	*x				= data.orientation_t.azimuth;
	*y				= data.orientation_t.pitch;
	*z				= data.orientation_t.roll;
	*scalar				= data.orientation_t.scalar;
	*status			= data.orientation_t.status;
	return 0;
}
static int gamerotvec_open_report_data(int open)
{
	return 0;
}
static int gamerotvec_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GAME_ROTATION_VECTOR, en);
}
static int gamerotvec_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GAME_ROTATION_VECTOR, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int gamerotvec_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	gamerotvec_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_GAME_ROTATION_VECTOR,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int gamerotvec_flush(void)
{
	return sensor_flush_to_hub(ID_GAME_ROTATION_VECTOR);
}

static int gamerotvec_recv_data(struct data_unit_t *event,
	void *reserved)
{
	int err = 0;

	if (event->flush_action == DATA_ACTION)
		err = grv_data_report(event->orientation_t.azimuth,
			event->orientation_t.pitch,
			event->orientation_t.roll, event->orientation_t.scalar,
			event->orientation_t.status,
			(int64_t)event->time_stamp);
	else if (event->flush_action == FLUSH_ACTION)
		err = grv_flush_report();

	return err;
}
static int gamerotvechub_local_init(void)
{
	struct fusion_control_path ctl = {0};
	struct fusion_data_path data = {0};
	int err = 0;

	ctl.open_report_data = gamerotvec_open_report_data;
	ctl.enable_nodata = gamerotvec_enable_nodata;
	ctl.set_delay = gamerotvec_set_delay;
	ctl.batch = gamerotvec_batch;
	ctl.flush = gamerotvec_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_GAME_ROTATION_VECTOR);
	if (err) {
		pr_err("register gamerotvec control path err\n");
		goto exit;
	}

	data.get_data = gamerotvec_get_data;
	data.vender_div = 1000000;
	err = fusion_register_data_path(&data, ID_GAME_ROTATION_VECTOR);
	if (err) {
		pr_err("register gamerotvec data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_GAME_ROTATION_VECTOR,
		gamerotvec_recv_data);
	if (err < 0) {
		pr_err("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int gamerotvechub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info gamerotvechub_init_info = {
	.name = "gamerotvec_hub",
	.init = gamerotvechub_local_init,
	.uninit = gamerotvechub_local_uninit,
};

static int __init gamerotvechub_init(void)
{
	fusion_driver_add(&gamerotvechub_init_info, ID_GAME_ROTATION_VECTOR);
	return 0;
}

static void __exit gamerotvechub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(gamerotvechub_init);
module_exit(gamerotvechub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gamerotvec driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
