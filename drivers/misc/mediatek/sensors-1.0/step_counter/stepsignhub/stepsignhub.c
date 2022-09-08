// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[stepsignhub] " fmt

#include <hwmsensor.h>
#include "stepsignhub.h"
#include <step_counter.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"

enum STEP_CDS_TRC {
	STEP_CDSH_TRC_INFO = 0X10,
};

static struct step_c_init_info step_cdshub_init_info;

struct step_chub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct step_chub_ipi_data obj_ipi_data;

static ssize_t trace_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct step_chub_ipi_data *obj = &obj_ipi_data;
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

static DRIVER_ATTR_WO(trace);

static struct driver_attribute *step_chub_attr_list[] = {
	&driver_attr_trace,
};

static int step_chub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(step_chub_attr_list));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, step_chub_attr_list[idx]);
		if (err != 0) {
			pr_err("driver_create_file (%s) = %d\n",
				     step_chub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int step_chub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(step_chub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, step_chub_attr_list[idx]);

	return err;
}

static int step_c_enable_nodata(int en)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (en == 1)
		ret = sensor_set_delay_to_hub(ID_STEP_COUNTER, 120);
#endif
	ret = sensor_enable_to_hub(ID_STEP_COUNTER, en);
	return ret;
}

static int step_d_enable_nodata(int en)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (en == 1)
		ret = sensor_set_delay_to_hub(ID_STEP_DETECTOR, 120);
#endif
	ret = sensor_enable_to_hub(ID_STEP_DETECTOR, en);
	return ret;
}

static int step_s_enable_nodata(int en)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (en == 1)
		ret = sensor_set_delay_to_hub(ID_SIGNIFICANT_MOTION, 120);
#endif
	ret = sensor_enable_to_hub(ID_SIGNIFICANT_MOTION, en);
	return ret;
}

static int floor_c_enable_nodata(int en)
{
	int ret = 0;

	ret = sensor_enable_to_hub(ID_FLOOR_COUNTER, en);
	return ret;
}

static int step_c_set_delay(u64 delay)
{
	unsigned int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	ret = sensor_set_delay_to_hub(ID_STEP_COUNTER, delayms);
#elif defined CONFIG_NANOHUB

#else

#endif
	return ret;
}

static int step_c_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_STEP_COUNTER,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int step_c_flush(void)
{
	return sensor_flush_to_hub(ID_STEP_COUNTER);
}

static int step_d_set_delay(u64 delay)
{
	unsigned int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	ret = sensor_set_delay_to_hub(ID_STEP_DETECTOR, delayms);
#elif defined CONFIG_NANOHUB

#else

#endif
	return ret;
}

static int step_d_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_STEP_DETECTOR,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int step_d_flush(void)
{
	return sensor_flush_to_hub(ID_STEP_DETECTOR);
}

static int floor_c_set_delay(u64 delay)
{
	unsigned int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	ret = sensor_set_delay_to_hub(ID_FLOOR_COUNTER, delayms);
#elif defined CONFIG_NANOHUB

#else

#endif
	return ret;
}
static int floor_c_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_FLOOR_COUNTER,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int floor_c_flush(void)
{
	return sensor_flush_to_hub(ID_FLOOR_COUNTER);
}

static int step_counter_get_data(uint32_t *counter, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_STEP_COUNTER, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*counter = data.step_counter_t.accumulated_step_count;
	return 0;
}

static int step_detector_get_data(uint32_t *counter, int *status)
{
	return 0;
}

static int smd_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_SIGNIFICANT_MOTION,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int smd_flush(void)
{
	return sensor_flush_to_hub(ID_SIGNIFICANT_MOTION);
}

static int significant_get_data(uint32_t *counter, int *status)
{
	return 0;
}

static int floor_counter_get_data(uint32_t *counter, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_FLOOR_COUNTER, &data);
	if (err < 0) {
		pr_err("%s fail!!\n", __func__);
		return -1;
	}
	time_stamp = data.time_stamp;
	*counter = data.floor_counter_t.accumulated_floor_count;
	return 0;
}

static int step_cds_open_report_data(int open)
{
	return 0;
}

static int step_detect_recv_data(struct data_unit_t *event,
				void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = step_d_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = step_notify_t(TYPE_STEP_DETECTOR,
			(int64_t)event->time_stamp);
	return err;
}

static int step_count_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = step_c_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = step_c_data_report_t(
			event->step_counter_t.accumulated_step_count,
			2, (int64_t)event->time_stamp);
	return err;
}

static int sign_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		pr_err("sign do not support flush\n");
	else if (event->flush_action == DATA_ACTION)
		err = step_notify_t(TYPE_SIGNIFICANT,
			(int64_t)event->time_stamp);
	return err;
}

static int floor_count_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct floor_counter_event_t *pfloor_counter = &event->floor_counter_t;

	if (event->flush_action == FLUSH_ACTION)
		err = floor_c_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = floor_c_data_report_t(
			pfloor_counter->accumulated_floor_count, 2,
			(int64_t)event->time_stamp);
	return err;
}

static int step_chub_local_init(void)
{
	struct step_c_control_path ctl = { 0 };
	struct step_c_data_path data = { 0 };
	int err = 0;
	struct platform_driver *paddr =
		step_cdshub_init_info.platform_diver_addr;

	err = step_chub_create_attr(&paddr->driver);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = step_cds_open_report_data;
	ctl.enable_nodata = step_c_enable_nodata;
	ctl.enable_step_detect = step_d_enable_nodata;
	ctl.enable_significant = step_s_enable_nodata;
	ctl.enable_floor_c = floor_c_enable_nodata;
	ctl.step_c_set_delay = step_c_set_delay;
	ctl.step_d_set_delay = step_d_set_delay;
	ctl.floor_c_set_delay = floor_c_set_delay;
	ctl.step_c_batch = step_c_batch;
	ctl.step_c_flush = step_c_flush;
	ctl.step_d_batch = step_d_batch;
	ctl.step_d_flush = step_d_flush;
	ctl.floor_c_batch = floor_c_batch;
	ctl.floor_c_flush = floor_c_flush;
	ctl.smd_batch = smd_batch;
	ctl.smd_flush = smd_flush;
	ctl.is_report_input_direct = true;
	ctl.is_counter_support_batch = false;
	ctl.is_detector_support_batch = true;
	ctl.is_smd_support_batch = false;
	err = step_c_register_control_path(&ctl);
	if (err) {
		pr_err("register step_cds control path err\n");
		goto exit;
	}

	data.get_data = step_counter_get_data;
	data.get_data_step_d = step_detector_get_data;
	data.get_data_significant = significant_get_data;
	data.get_data_floor_c = floor_counter_get_data;
	err = step_c_register_data_path(&data);
	if (err) {
		pr_err("register step_cds data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_STEP_DETECTOR,
		step_detect_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	err =
	    scp_sensorHub_data_registration(ID_SIGNIFICANT_MOTION,
					   sign_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	err = scp_sensorHub_data_registration(ID_STEP_COUNTER,
		step_count_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	err = scp_sensorHub_data_registration(ID_FLOOR_COUNTER,
		floor_count_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	step_chub_delete_attr(&paddr->driver);
exit_create_attr_failed:
	return -1;
}

static int step_chub_local_uninit(void)
{
	return 0;
}

static struct step_c_init_info step_cdshub_init_info = {
	.name = "step_cds_hub",
	.init = step_chub_local_init,
	.uninit = step_chub_local_uninit,
};

static int __init step_chub_init(void)
{
	step_c_driver_add(&step_cdshub_init_info);
	return 0;
}

static void __exit step_chub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(step_chub_init);
module_exit(step_chub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
