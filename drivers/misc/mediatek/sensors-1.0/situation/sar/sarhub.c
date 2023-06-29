/* sarhub motion sensor driver
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

#define pr_fmt(fmt) "[sarhub] " fmt

#include <hwmsensor.h>
#include "sarhub.h"
#include <situation.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"
#include "scp_helper.h"
#include "sar_factory.h"
#include <hwmsensor.h>
#include "SCP_power_monitor.h"

static struct situation_init_info sarhub_init_info;
static DEFINE_SPINLOCK(calibration_lock);
struct sarhub_ipi_data {
	struct work_struct init_done_work;
	atomic_t scp_init_done;
	atomic_t first_ready_after_boot;
	bool factory_enable;
	int32_t cali_data[3];
	int8_t cali_status;
	struct completion calibration_done;
};
static struct sarhub_ipi_data *obj_ipi_data;


static int sar_factory_enable_sensor(bool enabledisable,
					 int64_t sample_periods_ms)
{
	int err = 0;
	struct sarhub_ipi_data *obj = obj_ipi_data;

	if (enabledisable == true)
		WRITE_ONCE(obj->factory_enable, true);
	else
		WRITE_ONCE(obj->factory_enable, false);
	if (enabledisable == true) {
		err = sensor_set_delay_to_hub(ID_SAR,
					      sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_SAR, enabledisable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	return 0;
}

static int sar_factory_get_data(int32_t sensor_data[8])
{
	int err = 0;
	struct data_unit_t data;

	err = sensor_get_data_from_hub(ID_SAR, &data);
	if (err < 0) {
		pr_err_ratelimited("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	sensor_data[0] = data.data[0];
	sensor_data[1] = data.data[1];
	sensor_data[2] = data.data[2];
	sensor_data[3] = data.data[3];
	sensor_data[4] = data.data[4];
	sensor_data[5] = data.data[5];
	sensor_data[6] = data.data[6];
	sensor_data[7] = data.data[7];

	return err;
}

int sar_factory_enable_calibration(void)
{
	struct sarhub_ipi_data *obj = obj_ipi_data;
	pr_info("enter sar_factory_enable_calibration\n");
	init_completion(&obj->calibration_done);
	return sensor_calibration_to_hub(ID_SAR);
}

static int sar_factory_set_cali(int32_t data[3])
{
	return 0;
}

static int sar_factory_get_cali(int32_t data[3])
{
	int err = 0;
	struct sarhub_ipi_data *obj = obj_ipi_data;
	int8_t status = 0;

	err = wait_for_completion_timeout(&obj->calibration_done,
					  msecs_to_jiffies(6000));
	if (!err) {
		pr_err("sar factory get cali fail!\n");
		return -1;
	}
	spin_lock(&calibration_lock);
	data[0] = obj->cali_data[0];
	data[1] = obj->cali_data[1];
	data[2] = obj->cali_data[2];
	status = obj->cali_status;
	spin_unlock(&calibration_lock);
	if (status != 0) {
		pr_debug("sar cali fail!\n");
		return -2;
	}
	return 0;
}


static struct sar_factory_fops sarhub_factory_fops = {
	.enable_sensor = sar_factory_enable_sensor,
	.get_data = sar_factory_get_data,
	.enable_calibration = sar_factory_enable_calibration,
	.set_cali = sar_factory_set_cali,
	.get_cali = sar_factory_get_cali,
};

static struct sar_factory_public sarhub_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &sarhub_factory_fops,
};

static int sar_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_SAR, &data);
	if (err < 0) {
		pr_err_ratelimited("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	*probability	= data.sar_event.data[0];
	return 0;
}
static int sar_open_report_data(int open)
{
	int ret = 0;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_SAR, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	ret = sensor_enable_to_hub(ID_SAR, open);
	return ret;
}
static int sar_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_SAR,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int sar_flush(void)
{
	return sensor_flush_to_hub(ID_SAR);
}

static int sar_set_cali(uint8_t *data, uint8_t count)
{
	int32_t *buf = (int32_t *)data;
	struct sarhub_ipi_data *obj = obj_ipi_data;

	spin_lock(&calibration_lock);
	obj->cali_data[0] = buf[0];
	obj->cali_data[1] = buf[1];
	obj->cali_data[2] = buf[2];

	spin_unlock(&calibration_lock);
	pr_err("buf[0] = %d, buf[1] = %d, buf[2] = %d\n", obj->cali_data[0], obj->cali_data[1], obj->cali_data[2]);

	return sensor_cfg_to_hub(ID_SAR, data, count);
}

static int sar_set_reg(uint8_t *data, uint8_t count)
{
	int res = 0;
	res = sensor_reg_to_hub(ID_SAR, data, count);
	if (res < 0) {
		pr_err("%s is failed!!\n", __func__);
		return -1;
	}
	return 0;
}

static void sarhub_init_done_work(struct work_struct *work)
{
	struct sarhub_ipi_data *obj = obj_ipi_data;
	int err = 0;
#ifndef MTK_OLD_FACTORY_CALIBRATION
	int32_t cfg_data[2] = {0};
#endif

	if (atomic_read(&obj->scp_init_done) == 0) {
		pr_err("wait for nvram to set calibration\n");
		return;
	}
	if (atomic_xchg(&obj->first_ready_after_boot, 1) == 0)
		return;
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = sensor_set_cmd_to_hub(ID_SAR,
		CUST_ACTION_SET_CALI, &obj->cali_data);
	if (err < 0)
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_SAR, CUST_ACTION_SET_CALI);
#else
	spin_lock(&calibration_lock);
	cfg_data[0] = obj->cali_data[0];
	cfg_data[1] = obj->cali_data[1];
	pr_info("sarhub_init_done_work, cfg_data[%d %d]\n", cfg_data[0], cfg_data[1]);
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_SAR,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub sar fail\n");
#endif
}

static int sar_recv_data(struct data_unit_t *event, void *reserved)
{
	struct sarhub_ipi_data *obj = obj_ipi_data;
	int32_t value[3] = {0};
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = situation_flush_report(ID_SAR);
	else if (event->flush_action == DATA_ACTION) {
		value[0] = event->sar_event.data[0];
		value[1] = event->sar_event.data[1];
		value[2] = event->sar_event.data[2];
		err = sar_data_report_t(value, (int64_t)event->time_stamp);
	} else if (event->flush_action == CALI_ACTION) {
		value[0] = event->sar_event.x_bias;
		value[1] = event->sar_event.y_bias;
		value[2] = event->sar_event.z_bias;
		pr_info("value[0] = %d, value[1] = %d, value[2] = %d\n", value[0], value[1], value[2]);
		err = sar_cali_report(value);

		spin_lock(&calibration_lock);
		obj->cali_data[0] =
			event->sar_event.x_bias;
		obj->cali_data[1] =
			event->sar_event.y_bias;
		obj->cali_data[2] =
			event->sar_event.z_bias;
		obj->cali_status =
			(int8_t)event->sar_event.status;
		spin_unlock(&calibration_lock);
		complete(&obj->calibration_done);
	}
	return err;
}

static int scp_ready_event(uint8_t event, void *ptr)
{
	struct sarhub_ipi_data *obj = obj_ipi_data;

	switch (event) {
	case SENSOR_POWER_UP:
		atomic_set(&obj->scp_init_done, 1);
		schedule_work(&obj->init_done_work);
		break;
	case SENSOR_POWER_DOWN:
		atomic_set(&obj->scp_init_done, 0);
		break;
	}
	return 0;
}

static struct scp_power_monitor scp_ready_notifier = {
	.name = "sar",
	.notifier_call = scp_ready_event,
};

static int sarhub_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	struct sarhub_ipi_data *obj;

	pr_debug("%s\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));
	obj_ipi_data = obj;

	INIT_WORK(&obj->init_done_work, sarhub_init_done_work);

	WRITE_ONCE(obj->factory_enable, false);
	init_completion(&obj->calibration_done);

	ctl.open_report_data = sar_open_report_data;
	ctl.batch = sar_batch;
	ctl.flush = sar_flush;
	ctl.set_cali = sar_set_cali;
	ctl.set_reg = sar_set_reg;
	ctl.is_support_wake_lock = true;
	ctl.is_support_batch = false;
	scp_power_monitor_register(&scp_ready_notifier);
	err = situation_register_control_path(&ctl, ID_SAR);
	if (err) {
		pr_err("register sar control path err\n");
		goto exit;
	}

	data.get_data = sar_get_data;
	err = situation_register_data_path(&data, ID_SAR);
	if (err) {
		pr_err("register sar data path err\n");
		goto exit;
	}

	err = sar_factory_device_register(&sarhub_factory_device);
	if (err) {
		pr_err("sar_factory_device register failed\n");
		goto exit;
	}

	err = scp_sensorHub_data_registration(ID_SAR,
		sar_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int sarhub_local_uninit(void)
{
	return 0;
}

static struct situation_init_info sarhub_init_info = {
	.name = "sar_hub",
	.init = sarhub_local_init,
	.uninit = sarhub_local_uninit,
};

static int __init sarhub_init(void)
{
	situation_driver_add(&sarhub_init_info, ID_SAR);
	return 0;
}

static void __exit sarhub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(sarhub_init);
module_exit(sarhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SAR_HUB driver");
MODULE_AUTHOR("Jashon.zhang@mediatek.com");
