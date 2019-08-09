/* activityhub motion sensor driver
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


#define pr_fmt(fmt) "[acthub] " fmt

#include <hwmsensor.h>
#include "activityhub.h"
#include <activity.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

typedef enum {
	ACTHUB_TRC_INFO = 0X10,
} ACTHUB_TRC;

static struct act_init_info activityhub_init_info;

struct acthub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct hwm_sensor_data drv_data;
};

static struct acthub_ipi_data obj_ipi_data;

static ssize_t store_trace_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct acthub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		pr_err("obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1) {
		atomic_set(&obj->trace, trace);
	} else {
		pr_err("invalid content: '%s', length = %zu\n",
			buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(trace, 0644, NULL, store_trace_value);

static struct driver_attribute *activityhub_attr_list[] = {
	&driver_attr_trace,
};

static int activityhub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(activityhub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, activityhub_attr_list[idx]);
		if (err != 0) {
			pr_err("driver_create_file (%s) = %d\n",
				activityhub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int activityhub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(activityhub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, activityhub_attr_list[idx]);

	return err;
}

static int act_get_data(struct hwm_sensor_data *sensor_data, int *status)
{
	int err = 0;
	struct data_unit_t data;

	err = sensor_get_data_from_hub(ID_ACTIVITY, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	sensor_data->probability[STILL] =
		data.activity_data_t.probability[STILL];
	sensor_data->probability[STANDING] =
		data.activity_data_t.probability[STANDING];
	sensor_data->probability[SITTING] =
		data.activity_data_t.probability[SITTING];
	sensor_data->probability[LYING] =
		data.activity_data_t.probability[LYING];
	sensor_data->probability[ON_FOOT] =
		data.activity_data_t.probability[ON_FOOT];
	sensor_data->probability[WALKING] =
		data.activity_data_t.probability[WALKING];
	sensor_data->probability[RUNNING] =
		data.activity_data_t.probability[RUNNING];
	sensor_data->probability[CLIMBING] =
		data.activity_data_t.probability[CLIMBING];
	sensor_data->probability[ON_BICYCLE] =
		data.activity_data_t.probability[ON_BICYCLE];
	sensor_data->probability[IN_VEHICLE] =
		data.activity_data_t.probability[IN_VEHICLE];
	sensor_data->probability[TILTING] =
		data.activity_data_t.probability[TILTING];
	sensor_data->probability[UNKNOWN] =
		data.activity_data_t.probability[UNKNOWN];
	sensor_data->time = (int64_t)data.time_stamp;
	return 0;
}

static int act_open_report_data(int open)
{
	return 0;
}

static int act_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_ACTIVITY, en);
}

static int act_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_ACTIVITY, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}

static int act_batch(int flag, int64_t samplingPeriodNs,
	int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	act_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_ACTIVITY, flag,
		samplingPeriodNs, maxBatchReportLatencyNs);
}

static int act_flush(void)
{
	return sensor_flush_to_hub(ID_ACTIVITY);
}

static int activity_recv_data(struct data_unit_t *data_t, void *reserved)
{
	int err = 0;
	struct hwm_sensor_data data;

	data.sensor = data_t->sensor_type;
	data.probability[STILL] =
		data_t->activity_data_t.probability[STILL];
	data.probability[STANDING] =
		data_t->activity_data_t.probability[STANDING];
	data.probability[SITTING] =
		data_t->activity_data_t.probability[SITTING];
	data.probability[LYING] =
		data_t->activity_data_t.probability[LYING];
	data.probability[ON_FOOT] =
		data_t->activity_data_t.probability[ON_FOOT];
	data.probability[WALKING] =
		data_t->activity_data_t.probability[WALKING];
	data.probability[RUNNING] =
		data_t->activity_data_t.probability[RUNNING];
	data.probability[CLIMBING] =
		data_t->activity_data_t.probability[CLIMBING];
	data.probability[ON_BICYCLE] =
		data_t->activity_data_t.probability[ON_BICYCLE];
	data.probability[IN_VEHICLE] =
		data_t->activity_data_t.probability[IN_VEHICLE];
	data.probability[TILTING] =
		data_t->activity_data_t.probability[TILTING];
	data.probability[UNKNOWN] =
		data_t->activity_data_t.probability[UNKNOWN];
	data.time = (int64_t)data_t->time_stamp;
	if (data_t->flush_action == FLUSH_ACTION)
		err = act_flush_report();
	else if (data_t->flush_action == DATA_ACTION)
		err = act_data_report(&data, 2);
	return err;
}

static int activityhub_local_init(void)
{
	struct act_control_path ctl = { 0 };
	struct act_data_path data = { 0 };
	int err = 0;
	struct platform_driver *paddr =
		activityhub_init_info.platform_diver_addr;

	err = activityhub_create_attr(&paddr->driver);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = act_open_report_data;
	ctl.enable_nodata = act_enable_nodata;
	ctl.set_delay = act_set_delay;
	ctl.batch = act_batch;
	ctl.flush = act_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = true;
#else
#endif
	err = act_register_control_path(&ctl);
	if (err) {
		pr_err("register activity control path err\n");
		goto exit;
	}

	data.get_data = act_get_data;
	data.vender_div = 1;
	err = act_register_data_path(&data);
	if (err) {
		pr_err("register activity data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_ACTIVITY, activity_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
 exit:
	activityhub_delete_attr(&paddr->driver);
 exit_create_attr_failed:
	return -1;
}

static int activityhub_local_uninit(void)
{
	return 0;
}

static struct act_init_info activityhub_init_info = {
	.name = "activity_hub",
	.init = activityhub_local_init,
	.uninit = activityhub_local_uninit,
};

static int __init activityhub_init(void)
{
	act_driver_add(&activityhub_init_info);
	return 0;
}

static void __exit activityhub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(activityhub_init);
module_exit(activityhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
