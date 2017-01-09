/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "inv_mpu9250_iio.h"

static void inv_clear_kfifo(struct inv_mpu9250_state *st)
{
	unsigned long flags;

	/* take the spin lock sem to avoid interrupt kick in */
	spin_lock_irqsave(&st->time_stamp_lock, flags);
	kfifo_reset(&st->timestamps);
	spin_unlock_irqrestore(&st->time_stamp_lock, flags);
}

int inv_mpu9250_reset_fifo(struct iio_dev *indio_dev)
{
	int result = MPU_SUCCESS;
	struct inv_mpu9250_state  *st = iio_priv(indio_dev);
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;

	config = st->config;
	reg_cfg_info = &st->reg_cfg_info;

	if (config->fifo_enabled) {
		/* disable interrupt */
		reg_cfg_info->int_en &= ~BIT_RAW_RDY_EN;
		if (inv_mpu9250_write_reg(st, st->reg->int_enable,
					reg_cfg_info->int_en)) {
			dev_dbgerr("mpu disable interrupt failed\n");
			return -MPU_FAIL;
		}

		/* disable the sensor output to FIFO */
		reg_cfg_info->fifo_enable = 0;
		if (inv_mpu9250_write_reg(st, st->reg->fifo_en,
					reg_cfg_info->fifo_enable)) {
			dev_dbgerr("mpu disable fifo_enable failed\n");
			return -MPU_FAIL;
		}

		/* disable fifo reading */
		reg_cfg_info->user_ctrl &= ~BIT_FIFO_EN;
		if (inv_mpu9250_write_reg(st, st->reg->user_ctrl,
					reg_cfg_info->user_ctrl)) {
			dev_dbgerr("mpu disable fifo reading failed\n");
			return -MPU_FAIL;
		}

		/* reset FIFO*/
		if (inv_mpu9250_write_reg(st, st->reg->user_ctrl,
				reg_cfg_info->user_ctrl | BIT_FIFO_RST)) {
			dev_dbgerr("mpu reset FIFO failed\n");
			return -MPU_FAIL;
		}

		/* clear timestamps fifo */
		inv_clear_kfifo(st);

		if (mpu9250_start_fifo(st)) {
			dev_dbgerr("mpu disable interrupt failed\n");
			return -MPU_FAIL;
		}

		/*enable the sensor output to FIFO*/
		reg_cfg_info->fifo_enable |= config->fifo_en_mask;
		if (inv_mpu9250_write_reg(st, st->reg->fifo_en,
						reg_cfg_info->fifo_enable)) {
			dev_dbgerr("mpu disable fifo_enable failed\n");
			return -MPU_FAIL;
		}
	}

	return result;
}

/**
 * inv_mpu9250_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
irqreturn_t inv_mpu9250_irq_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct inv_mpu9250_state *st = iio_priv(indio_dev);
	s64 timestamp;

	timestamp = iio_get_time_ns();
	kfifo_in_spinlocked(&st->timestamps, &timestamp, 1,
				&st->time_stamp_lock);

	return IRQ_WAKE_THREAD;
}

static int inv_mpu9250_read_fifo(struct iio_dev *indio_dev)
{
	int result = MPU_SUCCESS;
	struct inv_mpu9250_state *st = iio_priv(indio_dev);
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;
	int ts_offset = 0, read_count = 0, max_count = 0;
	int value = 0, fifo_count = 0;
	uint8_t fifo_count_h = 0, fifo_count_l = 0;
	int packet_read = 0, tmp_size = 0;
	char data_push[24] = {0x0, 0x0};
	char data[1024] = {0x0, 0x0};
	int push_count = 0;
	char *buf = NULL;
	u64 *tmp;
	s64 timestamp;

	if (!st)
		return -MPU_FAIL;

	config = st->config;
	reg_cfg_info = &st->reg_cfg_info;
	memset(&data_push[0], 0x0, 24);
	mutex_lock(&indio_dev->mlock);

	if (mpu9250_debug_enable)
		inv_mpu9250_get_interrupt_status(st);

	if (config->fifo_enabled) {
		/*read fifo count*/
		result |= inv_mpu9250_read_reg(st,
					st->reg->fifo_count_h, &fifo_count_h);
		result |= inv_mpu9250_read_reg(st,
					st->reg->fifo_count_l, &fifo_count_l);
		if (result)
			dev_dbgerr("mpu9250 read fifo failed\n");

		fifo_count = (fifo_count_h << 8) | fifo_count_l;
		read_count = fifo_count / st->fifo_packet_size;
		push_count = read_count;
		max_count = MPU9250_FIFO_SINGLE_READ_MAX_BYTES
						/ st->fifo_packet_size;

		if (fifo_count < st->fifo_cnt_threshold)
			goto end_session;

		/* fifo count can't be odd number, if it is odd, reset fifo*/
		if (fifo_count & 1)
			goto flush_fifo;

	    /* Timestamp mismatch. */
	    if (kfifo_len(&st->timestamps) >
		fifo_count / st->fifo_packet_size + INV_MPU9250_TIME_STAMP_TOR)
		inv_clear_kfifo(st);

		buf = &data[0];
		while (read_count) {
			packet_read = (read_count < max_count ?
							read_count : max_count);
			tmp_size = packet_read * st->fifo_packet_size;

			/*single read max byte is 256*/
			result = mpu9250_bulk_read(st, st->reg->fifo_r_w,
							buf, tmp_size);
			if (result) {
				dev_dbgerr("mpu9250 read fifo failed\n");
				goto flush_fifo;
			}

			push_count = packet_read;
			ts_offset = DIV_ROUND_UP(st->fifo_packet_size, 8);
			while (push_count) {
				memcpy(&data_push[0],
						 buf, st->fifo_packet_size);
				result = kfifo_out(&st->timestamps,
						&timestamp, 1);
				/* when there is no timestamp, put it as 0 */
				if (0 == result)
					timestamp = 0;
				((int64_t *)data_push)[ts_offset] = timestamp;
				iio_push_to_buffers(indio_dev, &data_push[0]);
				push_count--;
				buf += st->fifo_packet_size;
			}
			read_count = read_count - packet_read;
		}
	} else {
		/*read raw data*/
		buf = &data[0];
		/*single read max byte is 256*/
		result = mpu9250_bulk_read(st, 0x3b, buf, st->fifo_packet_size);
		if (result) {
			dev_dbgerr("mpu9250 read fifo failed\n");
			goto flush_fifo;
		}

		result = kfifo_out(&st->timestamps, &timestamp, 1);
		/* when there is no timestamp, put timestamp as 0 */
		if (0 == result)
			timestamp = 0;

		memcpy(&data_push[0], buf, st->fifo_packet_size);
		ts_offset = DIV_ROUND_UP(st->fifo_packet_size, 8);
		((int64_t *)data_push)[ts_offset] = timestamp;
		iio_push_to_buffers(indio_dev, &data_push[0]);
	}

end_session:
	mutex_unlock(&indio_dev->mlock);
	iio_trigger_notify_done(indio_dev->trig);
	return MPU_SUCCESS;

flush_fifo:
	/* Flush HW and SW FIFOs. */
	inv_clear_kfifo(st);
	inv_mpu9250_reset_fifo(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	iio_trigger_notify_done(indio_dev->trig);
	return MPU_SUCCESS;
}

/**
 * inv_mpu9250_read_fifo() - Transfer data from hardware FIFO to KFIFO.
 */
irqreturn_t inv_mpu9250_read_fifo_fn(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;

	inv_mpu9250_read_fifo(indio_dev);
	return IRQ_HANDLED;
}
