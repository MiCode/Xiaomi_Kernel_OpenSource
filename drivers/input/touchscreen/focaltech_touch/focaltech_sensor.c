/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
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

/*****************************************************************************
 *
 * File Name: focaltech_esdcheck.c
 *
 *    Author: Focaltech Driver Team
 *
 *   Created: 2016-08-03
 *
 *  Abstract: Sensor
 *
 *   Version: v1.0
 *
 * Revision History:
 *        v1.0:
 *            First release. By luougojin 2016-08-03
 *****************************************************************************/

/*****************************************************************************
 * Included header files
 *****************************************************************************/
#include "focaltech_core.h"

#if FTS_PSENSOR_EN
/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
/* psensor register address*/
#define FTS_REG_PSENSOR_ENABLE                  0xB0
#define FTS_REG_PSENSOR_STATUS                  0x01

/* psensor register bits*/
#define FTS_PSENSOR_ENABLE_MASK                 0x01
#define FTS_PSENSOR_STATUS_NEAR                 0xC0
#define FTS_PSENSOR_STATUS_FAR                  0xE0
#define FTS_PSENSOR_FAR_TO_NEAR                 0
#define FTS_PSENSOR_NEAR_TO_FAR                 1
#define FTS_PSENSOR_ORIGINAL_STATE_FAR          1
#define FTS_PSENSOR_WAKEUP_TIMEOUT              500

/*****************************************************************************
 * Static variables
 *****************************************************************************/
static struct sensors_classdev __maybe_unused sensors_proximity_cdev = {

	.name = "fts-proximity",
	.vendor = "FocalTech",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5.0",
	.resolution = "5.0",
	.sensor_power = "0.1",
	.min_delay = 0,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

/*****************************************************************************
 * functions body
 *****************************************************************************/
/*****************************************************************************
 *  Name: fts_psensor_support_enabled
 *  Brief:
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static inline bool fts_psensor_support_enabled(void)
{
	/*return config_enabled(CONFIG_TOUCHSCREEN_FTS_PSENSOR);*/
	return FTS_PSENSOR_EN;
}

/*****************************************************************************
 *  Name: fts_psensor_enable
 *  Brief:
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static void fts_psensor_enable(struct fts_ts_data *data, int enable)
{
	u8 state;
	int ret = -1;

	if (data->client == NULL)
		return;

	fts_i2c_read_reg(data->client, FTS_REG_PSENSOR_ENABLE, &state);
	if (enable)
		state |= FTS_PSENSOR_ENABLE_MASK;
	else
		state &= ~FTS_PSENSOR_ENABLE_MASK;

	ret = fts_i2c_write_reg(data->client, FTS_REG_PSENSOR_ENABLE, state);
	if (ret < 0)
		FTS_ERROR("write psensor switch command failed");

}

/*****************************************************************************
 *  Name: fts_psensor_enable_set
 *  Brief:
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static int fts_psensor_enable_set(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct fts_psensor_platform_data *psensor_pdata =
			container_of(sensors_cdev,
			struct fts_psensor_platform_data, ps_cdev);
	struct fts_ts_data *data = psensor_pdata->data;
	struct input_dev *input_dev = data->psensor_pdata->input_psensor_dev;

	mutex_lock(&input_dev->mutex);
	fts_psensor_enable(data, enable);
	psensor_pdata->tp_psensor_data = FTS_PSENSOR_ORIGINAL_STATE_FAR;
	if (enable)
		psensor_pdata->tp_psensor_opened = 1;
	else
		psensor_pdata->tp_psensor_opened = 0;

	mutex_unlock(&input_dev->mutex);

	return enable;
}

/*****************************************************************************
 *  Name: fts_read_tp_psensor_data
 *  Brief:
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static int fts_read_tp_psensor_data(struct fts_ts_data *data)
{
	u8 psensor_status;
	char tmp;
	int ret = 1;

	fts_i2c_read_reg(data->client,
			FTS_REG_PSENSOR_STATUS, &psensor_status);

	tmp = data->psensor_pdata->tp_psensor_data;
	if (psensor_status == FTS_PSENSOR_STATUS_NEAR)
		data->psensor_pdata->tp_psensor_data =
			FTS_PSENSOR_FAR_TO_NEAR;
	else if (psensor_status == FTS_PSENSOR_STATUS_FAR)
		data->psensor_pdata->tp_psensor_data =
			FTS_PSENSOR_NEAR_TO_FAR;

	if (tmp != data->psensor_pdata->tp_psensor_data) {
		FTS_ERROR("%s sensor data changed", __func__);
		ret = 0;
	}

	return ret;
}


int fts_sensor_read_data(struct fts_ts_data *data)
{
	int ret = 0;

	if (fts_psensor_support_enabled()
			&& data->psensor_pdata->tp_psensor_opened) {
		ret = fts_read_tp_psensor_data(data);
		if (!ret) {
			if (data->suspended)
				pm_wakeup_event(&data->client->dev,
						FTS_PSENSOR_WAKEUP_TIMEOUT);

			input_report_abs(data->psensor_pdata->input_psensor_dev,
					ABS_DISTANCE,
					data->psensor_pdata->tp_psensor_data);
			input_sync(data->psensor_pdata->input_psensor_dev);
		}

		return 1;
	}

	return 0;
}

int fts_sensor_suspend(struct fts_ts_data *data)
{
	int ret = 0;

	if (fts_psensor_support_enabled()  &&
		 device_may_wakeup(&data->client->dev) &&
		 data->psensor_pdata->tp_psensor_opened) {
		ret = enable_irq_wake(data->client->irq);
		if (ret != 0)
			FTS_ERROR("%s: set_irq_wake failed", __func__);

		data->suspended = true;
		return 1;
	}

	return 0;
}


int fts_sensor_resume(struct fts_ts_data *data)
{
	int ret = 0;

	if (fts_psensor_support_enabled()
			&& device_may_wakeup(&data->client->dev)
			&& data->psensor_pdata->tp_psensor_opened) {
		ret = disable_irq_wake(data->client->irq);
		if (ret)
			FTS_ERROR("%s: disable_irq_wake failed",  __func__);

		data->suspended = false;
		return 1;
	}

	return 0;
}


int fts_sensor_init(struct fts_ts_data *data)
{
	struct fts_psensor_platform_data *psensor_pdata;
	struct input_dev *psensor_input_dev;
	int err;

	if (fts_psensor_support_enabled()) {
		device_init_wakeup(&data->client->dev, 1);
		psensor_pdata = devm_kzalloc(&data->client->dev,
				sizeof(struct fts_psensor_platform_data),
				GFP_KERNEL);
		if (!psensor_pdata) {
			FTS_ERROR("Failed to allocate memory");
			goto irq_free;
		}

		data->psensor_pdata = psensor_pdata;

		psensor_input_dev = input_allocate_device();
		if (!psensor_input_dev) {
			FTS_ERROR("Failed to allocate device");
			goto free_psensor_pdata;
		}

		__set_bit(EV_ABS, psensor_input_dev->evbit);
		input_set_abs_params(psensor_input_dev, ABS_DISTANCE,
				0, 1, 0, 0);
		psensor_input_dev->name = "proximity";
		psensor_input_dev->id.bustype = BUS_I2C;
		psensor_input_dev->dev.parent = &data->client->dev;
		data->psensor_pdata->input_psensor_dev = psensor_input_dev;

		err = input_register_device(psensor_input_dev);
		if (err) {
			FTS_ERROR("Unable to register device, err=%d", err);
			goto free_psensor_input_dev;
		}

		psensor_pdata->ps_cdev = sensors_proximity_cdev;
		psensor_pdata->ps_cdev.sensors_enable = fts_psensor_enable_set;
		psensor_pdata->data = data;

		err = sensors_classdev_register(&data->client->dev,
					&psensor_pdata->ps_cdev);
		if (err)
			goto unregister_psensor_input_device;
	}

	return 0;

unregister_psensor_input_device:
	if (fts_psensor_support_enabled())
		input_unregister_device(data->psensor_pdata->input_psensor_dev);
free_psensor_input_dev:
	if (fts_psensor_support_enabled())
		input_free_device(data->psensor_pdata->input_psensor_dev);
free_psensor_pdata:
	if (fts_psensor_support_enabled()) {
		devm_kfree(&data->client->dev, psensor_pdata);
		data->psensor_pdata = NULL;
	}
irq_free:
	if (fts_psensor_support_enabled())
		device_init_wakeup(&data->client->dev, 0);
	free_irq(data->client->irq, data);

	return 1;
}

int fts_sensor_remove(struct fts_ts_data *data)
{
	if (fts_psensor_support_enabled()) {
		device_init_wakeup(&data->client->dev, 0);
		sensors_classdev_unregister(&data->psensor_pdata->ps_cdev);
		input_unregister_device(data->psensor_pdata->input_psensor_dev);
		devm_kfree(&data->client->dev, data->psensor_pdata);
		data->psensor_pdata = NULL;
	}
	return 0;
}
#endif  /* FTS_PSENSOR_EN */
