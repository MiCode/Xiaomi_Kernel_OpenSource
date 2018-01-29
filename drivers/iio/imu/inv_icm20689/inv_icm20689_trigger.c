/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* Code was copied and modified from
* drivers/iio/imu/inv_mpu6050/inv_mpu_trigger.c
*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/spi/spi.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include "inv_icm20689_iio.h"

static void inv_icm20689_scan_query(struct iio_dev *indio_dev)
{
	struct inv_icm20689_state  *st = iio_priv(indio_dev);

	st->chip_config.gyro_fifo_enable =
		test_bit(INV_ICM20689_SCAN_GYRO_X,
			indio_dev->active_scan_mask) ||
			test_bit(INV_ICM20689_SCAN_GYRO_Y,
			indio_dev->active_scan_mask) ||
			test_bit(INV_ICM20689_SCAN_GYRO_Z,
			indio_dev->active_scan_mask);

	st->chip_config.accl_fifo_enable =
		test_bit(INV_ICM20689_SCAN_ACCL_X,
			indio_dev->active_scan_mask) ||
			test_bit(INV_ICM20689_SCAN_ACCL_Y,
			indio_dev->active_scan_mask) ||
			test_bit(INV_ICM20689_SCAN_ACCL_Z,
			indio_dev->active_scan_mask);

	st->chip_config.temp_fifo_enable =
			test_bit(INV_ICM20689_SCAN_TEMP,
				indio_dev->active_scan_mask);
}

/**
 *  inv_icm20689_set_enable() - enable chip functions.
 *  @indio_dev:	Device driver instance.
 *  @enable: enable/disable
 */
static int inv_icm20689_set_enable(struct iio_dev *indio_dev, bool enable)
{
	struct inv_icm20689_state *st = iio_priv(indio_dev);
	int result;

	if (enable) {
#if INV20689_SMD_IRQ_TRIGGER
		result = imu_ts_smd_channel_init();
		if (result)
			return result;
#endif
		/***************************************************
		 * for an unknown reason, it needs to power-up twice
		 * if the fifo rate sets to 8K at the first time.
		 * just power-up twice every time for 8K sample rate.
		 ***************************************************/
		if (st->chip_config.fifo_rate == INV_ICM20689_GYRO_8K_RATE) {
			inv_icm20689_set_power_itg(st, true);
			inv_icm20689_set_power_itg(st, false);
			msleep(INV_ICM20689_SENSOR_UP_TIME);
			inv_icm20689_set_power_itg(st, true);
		} else
			result = inv_icm20689_set_power_itg(st, true);

		if (result)
			return result;
		inv_icm20689_scan_query(indio_dev);
		if (st->chip_config.gyro_fifo_enable) {
			result = inv_icm20689_switch_engine(st, true,
					INV_ICM20689_BIT_PWR_GYRO_STBY);
			if (result)
				return result;
		}
		if (st->chip_config.accl_fifo_enable) {
			result = inv_icm20689_switch_engine(st, true,
					INV_ICM20689_BIT_PWR_ACCL_STBY);
			if (result)
				return result;
		}
		result = inv_icm20689_reset_fifo(indio_dev);
		if (result)
			return result;
	} else {
#if INV20689_SMD_IRQ_TRIGGER
		imu_ts_smd_channel_close();
#endif
		st->chip_config.temp_fifo_enable = 0;
		st->chip_config.gyro_fifo_enable = 0;
		st->chip_config.accl_fifo_enable = 0;

		result = inv_icm20689_write_reg(st, st->reg->fifo_en, 0);
		if (result)
			return result;

		result = inv_icm20689_write_reg(st, st->reg->int_enable, 0);
		if (result)
			return result;

		result = inv_icm20689_write_reg(st, st->reg->user_ctrl,
				INV_ICM20689_BIT_I2C_MST_DIS);
		if (result)
			return result;

		result = inv_icm20689_switch_engine(st, false,
					INV_ICM20689_BIT_PWR_GYRO_STBY);
		if (result)
			return result;

		result = inv_icm20689_switch_engine(st, false,
					INV_ICM20689_BIT_PWR_ACCL_STBY);
		if (result)
			return result;
		result = inv_icm20689_set_power_itg(st, false);
		if (result)
			return result;
	}
	st->chip_config.enable = enable;

	return 0;
}

/**
 * inv_icm20689_data_rdy_trigger_set_state() - set data ready interrupt state
 * @trig: Trigger instance
 * @state: Desired trigger state
 */
static int inv_icm20689_data_rdy_trigger_set_state(struct iio_trigger *trig,
						bool state)
{
	return inv_icm20689_set_enable(iio_trigger_get_drvdata(trig), state);
}

static const struct iio_trigger_ops inv_icm20689_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &inv_icm20689_data_rdy_trigger_set_state,
};

int inv_icm20689_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_icm20689_state *st = iio_priv(indio_dev);

	st->trig = iio_trigger_alloc("%s-dev%d",
					indio_dev->name,
					indio_dev->id);
	if (st->trig == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

#if INV20689_DEVICE_IRQ_TRIGGER
	ret = request_irq(st->spi->irq, &iio_trigger_generic_data_rdy_poll,
				IRQF_TRIGGER_RISING,
				"inv_mpu",
				st->trig);
	if (ret)
		goto error_free_trig;
#else
	inv_trig = st->trig;
#endif

	st->trig->dev.parent = &st->spi->dev;
	st->trig->ops = &inv_icm20689_trigger_ops;
	iio_trigger_set_drvdata(st->trig, indio_dev);
	ret = iio_trigger_register(st->trig);
	if (ret)
		goto error_free_irq;
	indio_dev->trig = st->trig;

	return 0;

error_free_irq:
#if INV20689_DEVICE_IRQ_TRIGGER
	free_irq(st->spi->irq, st->trig);
#else
	inv_trig = NULL;
#endif
#if INV20689_DEVICE_IRQ_TRIGGER
error_free_trig:
#endif
	iio_trigger_free(st->trig);
error_ret:
	return ret;
}

void inv_icm20689_remove_trigger(struct inv_icm20689_state *st)
{
	iio_trigger_unregister(st->trig);
	free_irq(st->spi->irq, st->trig);
	iio_trigger_free(st->trig);
}
