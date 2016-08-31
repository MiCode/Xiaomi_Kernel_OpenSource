/*
* Copyright (C) 2012 Invensense, Inc.
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
*
*/

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    inv_ami306_ring.c
 *      @brief   Invensense implementation for AMI306
 *      @details This driver currently works for the AMI306
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

#include "inv_ami306_iio.h"

static int put_scan_to_buf(struct iio_dev *indio_dev, unsigned char *d,
				short *s, int scan_index) {
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
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
int inv_read_ami306_fifo(struct iio_dev *indio_dev)
{
	struct inv_ami306_state_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	int result, status, d_ind;
	char b;
	char *tmp;
	s64 tmp_buf[2];

	result = i2c_smbus_read_i2c_block_data(st->i2c, REG_AMI_STA1, 1, &b);
	if (result < 0)
		goto end_session;
	if (b & AMI_STA1_DRDY_BIT) {
		status = ami306_read_raw_data(st, st->compass_data);
		if (status) {
			pr_err("error reading raw\n");
			goto end_session;
		}
		tmp = (unsigned char *)tmp_buf;
		d_ind = put_scan_to_buf(indio_dev, tmp, st->compass_data,
						INV_AMI306_SCAN_MAGN_X);
		if (ring->scan_timestamp)
			tmp_buf[(d_ind + 7)/8] = st->timestamp;
		ring->access->store_to(indio_dev->buffer, tmp, st->timestamp);
	} else if (b & AMI_STA1_DOR_BIT)
		pr_err("not ready\n");
end_session:
	b = AMI_CTRL3_FORCE_BIT;
	result = i2c_smbus_write_i2c_block_data(st->i2c, REG_AMI_CTRL3, 1, &b);

	return IRQ_HANDLED;
}

void inv_ami306_unconfigure_ring(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);
};
static int inv_ami306_postenable(struct iio_dev *indio_dev)
{
	struct inv_ami306_state_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	int result;

	/* when all the outputs are disabled, even though buffer/enable is on,
	   do nothing */
	if (!(iio_scan_mask_query(indio_dev, ring, INV_AMI306_SCAN_MAGN_X) ||
	    iio_scan_mask_query(indio_dev, ring, INV_AMI306_SCAN_MAGN_Y) ||
	    iio_scan_mask_query(indio_dev, ring, INV_AMI306_SCAN_MAGN_Z)))
		return 0;

	result = set_ami306_enable(indio_dev, true);
	if (result)
		return result;
	schedule_delayed_work(&st->work, msecs_to_jiffies(st->delay));

	return 0;
}

static int inv_ami306_predisable(struct iio_dev *indio_dev)
{
	struct iio_buffer *ring = indio_dev->buffer;
	struct inv_ami306_state_s *st = iio_priv(indio_dev);

	cancel_delayed_work_sync(&st->work);
	clear_bit(INV_AMI306_SCAN_MAGN_X, ring->scan_mask);
	clear_bit(INV_AMI306_SCAN_MAGN_Y, ring->scan_mask);
	clear_bit(INV_AMI306_SCAN_MAGN_Z, ring->scan_mask);

	return 0;
}

static const struct iio_buffer_setup_ops inv_ami306_ring_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &inv_ami306_postenable,
	.predisable = &inv_ami306_predisable,
};

int inv_ami306_configure_ring(struct iio_dev *indio_dev)
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
	indio_dev->setup_ops = &inv_ami306_ring_setup_ops;

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	return 0;
}
/**
 *  @}
 */

