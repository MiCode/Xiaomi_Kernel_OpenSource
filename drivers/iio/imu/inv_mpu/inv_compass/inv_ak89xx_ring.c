/*
* Copyright (C) 2013 Invensense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "iio.h"
#include "kfifo_buf.h"
#include "trigger_consumer.h"
#include "sysfs.h"

#include "inv_ak89xx_iio.h"

static int put_scan_to_buf(struct iio_dev *indio_dev, unsigned char *d,
				short *s, int scan_index)
{
	struct iio_buffer *ring = indio_dev->buffer;
	int st;
	int i, d_ind;

	d_ind = 0;
	for (i = 0; i < 3; i++) {
		st = iio_scan_mask_query(indio_dev, ring, scan_index + i);
		if (st) {
			memcpy(&d[d_ind], &s[i], sizeof(s[i]));
			d_ind += sizeof(s[i]);
		}
	}

	return d_ind;
}

/**
 *  inv_read_ak89xx_fifo() - Transfer data from FIFO to ring buffer.
 */
void inv_read_ak89xx_fifo(struct iio_dev *indio_dev)
{
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	int d_ind;
	s8 *tmp;
	s64 tmp_buf[2];

	if (!ak89xx_read(st, st->compass_data)) {
		st->compass_data[0] = (short)(((int)st->compass_data[0] * (st->asa[0] + 128)) >> 8);
		st->compass_data[1] = (short)(((int)st->compass_data[1] * (st->asa[1] + 128)) >> 8);
		st->compass_data[2] = (short)(((int)st->compass_data[2] * (st->asa[2] + 128)) >> 8);
		tmp = (u8 *)tmp_buf;
		d_ind = put_scan_to_buf(indio_dev, tmp, st->compass_data,
						INV_AK89XX_SCAN_MAGN_X);
		if (ring->scan_timestamp)
			tmp_buf[(d_ind + 7)/8] = st->timestamp;
		ring->access->store_to(indio_dev->buffer, tmp, st->timestamp);
	}
}

void inv_ak89xx_unconfigure_ring(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);
};

static int inv_ak89xx_postenable(struct iio_dev *indio_dev)
{
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;

	/* when all the outputs are disabled, even though buffer/enable is on,
	   do nothing */
	if (!(iio_scan_mask_query(indio_dev, ring, INV_AK89XX_SCAN_MAGN_X) ||
		iio_scan_mask_query(indio_dev, ring, INV_AK89XX_SCAN_MAGN_Y) ||
		iio_scan_mask_query(indio_dev, ring, INV_AK89XX_SCAN_MAGN_Z)))
		return 0;

	set_ak89xx_enable(indio_dev, true);
	schedule_delayed_work(&st->work, msecs_to_jiffies(st->delay));

	return 0;
}

static int inv_ak89xx_predisable(struct iio_dev *indio_dev)
{
	struct iio_buffer *ring = indio_dev->buffer;
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);

	cancel_delayed_work_sync(&st->work);
	clear_bit(INV_AK89XX_SCAN_MAGN_X, ring->scan_mask);
	clear_bit(INV_AK89XX_SCAN_MAGN_Y, ring->scan_mask);
	clear_bit(INV_AK89XX_SCAN_MAGN_Z, ring->scan_mask);
	set_ak89xx_enable(indio_dev, false);

	return 0;
}

static const struct iio_buffer_setup_ops inv_ak89xx_ring_setup_ops = {
	.preenable  = &iio_sw_buffer_preenable,
	.postenable = &inv_ak89xx_postenable,
	.predisable = &inv_ak89xx_predisable,
};

int inv_ak89xx_configure_ring(struct iio_dev *indio_dev)
{
	int ret = 0;
	struct iio_buffer *ring;

	ring = iio_kfifo_allocate(indio_dev);
	if (!ring) {
		ret = -ENOMEM;
		return ret;
	}
	indio_dev->buffer = ring;
	/* setup ring buffer */
	ring->scan_timestamp = true;
	indio_dev->setup_ops = &inv_ak89xx_ring_setup_ops;

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	return 0;
}

