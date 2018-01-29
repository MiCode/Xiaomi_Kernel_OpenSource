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
* drivers/iio/imu/inv_mpu6050/inv_mpu_ring.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/spi/spi.h>

#include "inv_icm20689_iio.h"

static void inv_clear_kfifo(struct inv_icm20689_state *st)
{
	unsigned long flags;

	/* take the spin lock sem to avoid interrupt kick in */
	spin_lock_irqsave(&st->time_stamp_lock, flags);
	kfifo_reset(&st->timestamps);
	spin_unlock_irqrestore(&st->time_stamp_lock, flags);
}

int inv_icm20689_reset_fifo(struct iio_dev *indio_dev)
{
	int result;
	u8 d;
	struct inv_icm20689_state  *st = iio_priv(indio_dev);

	/* disable interrupt */
	result = inv_icm20689_write_reg(st, st->reg->int_enable, 0);
	if (result) {
		dev_err(&st->spi->dev, "int_enable failed %d\n", result);
		return result;
	}
	/* disable the sensor output to FIFO */
	result = inv_icm20689_write_reg(st, st->reg->fifo_en, 0);
	if (result)
		goto reset_fifo_fail;
	/* disable fifo reading */
	d = INV_ICM20689_BIT_I2C_MST_DIS;
	result = inv_icm20689_write_reg(st, st->reg->user_ctrl, d);
	if (result)
		goto reset_fifo_fail;

	/* reset FIFO*/
	d = INV_ICM20689_BIT_FIFO_RST|INV_ICM20689_BIT_I2C_MST_DIS;
	result = inv_icm20689_write_reg(st, st->reg->user_ctrl, d);
	if (result)
		goto reset_fifo_fail;

	inv_clear_kfifo(st);

	/* enable FIFO reading */
	d = INV_ICM20689_BIT_I2C_MST_DIS | INV_ICM20689_BIT_FIFO_EN;
	result = inv_icm20689_write_reg(st, st->reg->user_ctrl, d);
	if (result)
		goto reset_fifo_fail;
	/* enable sensor output to FIFO */
	d = 0;
	if (st->chip_config.gyro_fifo_enable)
		d |= INV_ICM20689_BITS_GYRO_OUT;
	if (st->chip_config.accl_fifo_enable)
		d |= INV_ICM20689_BIT_ACCEL_OUT;
	if (st->chip_config.temp_fifo_enable)
		d |= INV_ICM20689_BITS_TEMP_OUT;
	result = inv_icm20689_write_reg(st, st->reg->fifo_en, d);
	if (result)
		goto reset_fifo_fail;

	/* enable interrupt */
	if (st->chip_config.accl_fifo_enable ||
	    st->chip_config.gyro_fifo_enable ||
		st->chip_config.temp_fifo_enable) {
		result = inv_icm20689_write_reg(st, st->reg->int_enable,
					INV_ICM20689_BIT_DATA_RDY_EN);
		if (result)
			return result;
	}

	return 0;

reset_fifo_fail:
	dev_err(&st->spi->dev, "reset fifo failed %d\n", result);
	result = inv_icm20689_write_reg(st, st->reg->int_enable,
					INV_ICM20689_BIT_DATA_RDY_EN);

	return result;
}

/**
 * inv_icm20689_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
irqreturn_t inv_icm20689_irq_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct inv_icm20689_state *st = iio_priv(indio_dev);
	s64 timestamp;

	timestamp = iio_get_time_ns();
	kfifo_in_spinlocked(&st->timestamps, &timestamp, 1,
				&st->time_stamp_lock);

	return IRQ_WAKE_THREAD;
}

static int inv_icm20689_read_fifo(struct iio_dev *indio_dev)
{
	struct inv_icm20689_state *st = iio_priv(indio_dev);
	size_t bytes_per_datum;
	int result;
	u8 data[INV_ICM20689_OUTPUT_DATA_SIZE];
	u8 ori_data[INV_ICM20689_STORED_DATA];
	u16 fifo_count;
	u16 read_count;
	u16 left_count;
	u16 index;
	s64 timestamp;
	u64 *tmp;
	int t_fifo_len;

	mutex_lock(&indio_dev->mlock);
	if (!(st->chip_config.accl_fifo_enable |
		st->chip_config.gyro_fifo_enable |
		st->chip_config.temp_fifo_enable))
		goto end_session;

	bytes_per_datum = 0;
	if (st->chip_config.accl_fifo_enable)
		bytes_per_datum += INV_ICM20689_BYTES_PER_3AXIS_SENSOR;

	if (st->chip_config.temp_fifo_enable)
		bytes_per_datum += 2;

	if (st->chip_config.gyro_fifo_enable)
		bytes_per_datum += INV_ICM20689_BYTES_PER_3AXIS_SENSOR;

	/*
	 * read fifo_count register to know how many bytes inside FIFO
	 * right now
	 */
	result = inv_icm20689_spi_bulk_read(st, st->reg->fifo_count_h, 2, data);

	if (result)
		goto end_session;
	fifo_count = be16_to_cpup((__be16 *)(&data[0]));

	if (fifo_count < bytes_per_datum)
		goto end_session;
	/* fifo count can't be odd number, if it is odd, reset fifo*/
	if (fifo_count & 1)
		goto flush_fifo;

	/* max fifo data */
	if (fifo_count >  INV_ICM20689_FIFO_THRESHOLD)
		goto flush_fifo;


	t_fifo_len = kfifo_len(&st->timestamps);
	/* Timestamp mismatch. */
	if (t_fifo_len >
		fifo_count / bytes_per_datum + INV_ICM20689_TIME_STAMP_TOR)
		inv_clear_kfifo(st);

	left_count = fifo_count;
	index = 0;

	while (left_count > 0) {
		if (left_count >= INV_ICM20689_MAX_FIFO_OUTPUT)
			read_count = INV_ICM20689_MAX_FIFO_OUTPUT;
		else
			read_count = left_count;

		result = inv_icm20689_spi_bulk_read(st, st->reg->fifo_r_w,
				read_count, ori_data + index);

		if (result)
			goto flush_fifo;

		index += read_count;
		left_count -= read_count;
	}

	/*************************************
	 *  some data would have the same timestamp
	 *  if using SMD to trigger
	 *************************************/
	result = kfifo_out(&st->timestamps, &timestamp, 1);
	/* when there is no timestamp, put timestamp as 0 */
	if (0 == result)
		timestamp = 0;

	index = 0;
	while (fifo_count >= bytes_per_datum) {
		memcpy(data, ori_data + index, bytes_per_datum);
		index += bytes_per_datum;
		tmp = (u64 *)data;
		tmp[DIV_ROUND_UP(bytes_per_datum, 8)] = timestamp;
		iio_push_to_buffers(indio_dev, data);
		fifo_count -= bytes_per_datum;
	}

end_session:
	mutex_unlock(&indio_dev->mlock);
	iio_trigger_notify_done(indio_dev->trig);

	return 0;

flush_fifo:
	/* Flush HW and SW FIFOs. */
	inv_icm20689_reset_fifo(indio_dev);
	inv_clear_kfifo(st);
	mutex_unlock(&indio_dev->mlock);
	iio_trigger_notify_done(indio_dev->trig);

	return 0;
}

/**
 * inv_icm20689_read_fifo() - Transfer data from hardware FIFO to KFIFO.
 */
irqreturn_t inv_icm20689_read_fifo_fn(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;

	inv_icm20689_read_fifo(indio_dev);
	return IRQ_HANDLED;
}
