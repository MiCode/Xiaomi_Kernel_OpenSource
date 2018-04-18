/*
* Copyright (c) 2018, The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
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
#include "inv_icm20602_iio.h"
#include <linux/i2c.h>

#define combine_8_to_16(upper, lower)  ((_upper << 8) | _lower)

static void inv_clear_kfifo(struct inv_icm20602_state *st)
{
	unsigned long flags;

	/* take the spin lock sem to avoid interrupt kick in */
	spin_lock_irqsave(&st->time_stamp_lock, flags);
	kfifo_reset(&st->timestamps);
	spin_unlock_irqrestore(&st->time_stamp_lock, flags);
}

int inv_icm20602_reset_fifo(struct iio_dev *indio_dev)
{
	int result;
	u8 d;
	struct inv_icm20602_state  *st = iio_priv(indio_dev);

	return result;
}

/*
 * inv_icm20602_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
irqreturn_t inv_icm20602_irq_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	s64 timestamp;

	timestamp = iio_get_time_ns();

	kfifo_in_spinlocked(&st->timestamps, &timestamp, 1,
				&st->time_stamp_lock);

	return IRQ_WAKE_THREAD;
}

static int inv_icm20602_read_data(struct iio_dev *indio_dev)
{
	int result = MPU_SUCCESS;
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int package_count;
	char *buf = st->buf;
	struct struct_icm20602_data *data_push = st->data_push;
	s64 timestamp;
	int i;

	if (!st)
		return -MPU_FAIL;
	package_count = config->fifo_waterlevel / ICM20602_PACKAGE_SIZE;
	mutex_lock(&indio_dev->mlock);
	if (config->fifo_enabled) {
		result = icm20602_read_fifo(st,
				st->buf, config->fifo_waterlevel);
		if (result != config->fifo_waterlevel) {
			dev_dbgerr("icm20602 read fifo failed, result = %d\n",
				result);
			goto flush_fifo;
		}

		for (i = 0; i < package_count; i++) {
			memcpy((char *)(&st->data_push[i].raw_data),
				st->buf, ICM20602_PACKAGE_SIZE);
			result = kfifo_out(&st->timestamps,
				&timestamp, 1);
			/* when there is no timestamp, put it as 0 */
			if (0 == result)
				timestamp = 0;
			st->data_push[i].timestamps = timestamp;
			iio_push_to_buffers(indio_dev, st->data_push+i);
			st->buf += ICM20602_PACKAGE_SIZE;
		}
	}
end_session:
	mutex_unlock(&indio_dev->mlock);
	iio_trigger_notify_done(indio_dev->trig);
	return MPU_SUCCESS;

flush_fifo:
	/* Flush HW and SW FIFOs. */
	inv_clear_kfifo(st);
	inv_icm20602_reset_fifo(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	iio_trigger_notify_done(indio_dev->trig);
	return MPU_SUCCESS;
}

/*
 * inv_icm20602_read_fifo() - Transfer data from hardware FIFO to KFIFO.
 */
irqreturn_t inv_icm20602_read_fifo_fn(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;

	inv_icm20602_read_data(indio_dev);
	return IRQ_HANDLED;
}
