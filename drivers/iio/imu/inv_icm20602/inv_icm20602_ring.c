/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include "inv_icm20602_iio.h"
#include <linux/i2c.h>

#define combine_8_to_16(upper, lower)  ((_upper << 8) | _lower)

static void inv_clear_kfifo(struct inv_icm20602_state *st)
{
	unsigned long flags;

	/* Take the spin lock sem to avoid interrupt kick in */
	spin_lock_irqsave(&st->time_stamp_lock, flags);
	kfifo_reset(&st->timestamps);
	spin_unlock_irqrestore(&st->time_stamp_lock, flags);
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

	timestamp = iio_get_time_ns(indio_dev);

	kfifo_in_spinlocked(&st->timestamps, &timestamp, 1,
				&st->time_stamp_lock);

	return IRQ_WAKE_THREAD;
}

#define BIT_FIFO_OFLOW_INT			0x10
#define BIT_FIFO_WM_INT				0x40
static int inv_icm20602_read_data(struct iio_dev *indio_dev)
{
	int result = MPU_SUCCESS;
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int package_count;
	char *buf = st->buf;
	s64 timestamp;
	u8 int_status;
	u16 fifo_count;
	int i;

	if (!st)
		return -MPU_FAIL;
	package_count = config->fifo_waterlevel / ICM20602_PACKAGE_SIZE;
	mutex_lock(&indio_dev->mlock);
	icm20602_int_status(st, &int_status);
	if (int_status & BIT_FIFO_OFLOW_INT) {
		icm20602_fifo_count(st, &fifo_count);
		pr_debug("fifo_count = %d\n", fifo_count);
		inv_clear_kfifo(st);
		icm20602_reset_fifo(st);
		goto end_session;
	}
	if (config->fifo_enabled) {
		result = kfifo_out(&st->timestamps,
				&timestamp, 1);
		/* when there is no timestamp, put it as 0 */
		if (result == 0)
		timestamp = 0;
		for (i = 0; i < package_count; i++) {
			result = icm20602_read_fifo(st,
			buf, ICM20602_PACKAGE_SIZE);
			memcpy(st->data_push[i].raw_data,
			buf, ICM20602_PACKAGE_SIZE);
			iio_push_to_buffers_with_timestamp(indio_dev,
			st->data_push[i].raw_data, timestamp);
			buf += ICM20602_PACKAGE_SIZE;
		}
		memset(st->buf, 0, config->fifo_waterlevel);
	}
end_session:
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
