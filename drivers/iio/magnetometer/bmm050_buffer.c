/*!
 * @section LICENSE
 * (C) Copyright 2011~2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename    bmm050_buffer.c
 * @date        "Thu Apr 24 10:40:36 2014 +0800"
 * @id          "6d0d027"
 * @version     v1.0
 *
 * @brief       BMM050 Linux IIO Driver
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "bmm050_iio.h"

static int bmm_get_buffer(struct iio_dev *indio_dev, u8 *buf)
{
	unsigned char addr[BMM_NUMBER_DATA_CHANNELS] = {0, 0, 0};
	int i, count = 0, len = 0;
	 struct bmm_client_data *client_data = iio_priv(indio_dev);

	for (i = 0; i < BMM_NUMBER_DATA_CHANNELS; i++) {
		if (test_bit(i, indio_dev->active_scan_mask)) {
			addr[count] = indio_dev->channels[i].address;
			count++;
		}
	}

	switch (count) {
	case 1:
		len = i2c_smbus_read_i2c_block_data(client_data->client,
						addr[0], 2, buf);
		break;
	case 2:
		if ((addr[1] - addr[0]) == BMM_BYTE_PRE_CHANNEL) {
			len =  i2c_smbus_read_i2c_block_data(
				client_data->client,
				addr[0], BMM_BYTE_PRE_CHANNEL*count, buf);
		} else {
			u8 raw_data_tmp[
					BMM_BYTE_PRE_CHANNEL *
					BMM_NUMBER_DATA_CHANNELS];
			len = i2c_smbus_read_i2c_block_data(
				client_data->client, addr[0],
				BMM_BYTE_PRE_CHANNEL * BMM_NUMBER_DATA_CHANNELS,
				raw_data_tmp);
			if (len < 0)
				return len;
			for (i = 0; i < count * BMM_NUMBER_DATA_CHANNELS; i++) {
				if (i < count)
					buf[i] = raw_data_tmp[i];
				else
					buf[i] = raw_data_tmp[count + i];
			}
			len = BMM_BYTE_PRE_CHANNEL * count;
		}
		break;
	case 3:
		len = i2c_smbus_read_i2c_block_data(client_data->client,
			addr[0],
			BMM_BYTE_PRE_CHANNEL * BMM_NUMBER_DATA_CHANNELS, buf);
		break;
	default:
		len =  -EINVAL;
		goto read_data_err;

	}

	if (len != BMM_BYTE_PRE_CHANNEL * count) {
		len = -EIO;
		goto read_data_err;
	}

read_data_err:
	return len;
}

irqreturn_t bmm_buffer_handler(int irq, void *p)
{
	 struct iio_poll_func *pf = p;
	 struct iio_dev *indio_dev = pf->indio_dev;

	 int len;
	unsigned char buffer_data[10];

	 len = bmm_get_buffer(indio_dev, buffer_data);
	if (len < 0)
		goto err_get_buffer;

	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)buffer_data + ALIGN(len, sizeof(s64))) =
					pf->timestamp;

	iio_push_to_buffers(indio_dev, buffer_data);

err_get_buffer:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;

}


void bmm_deallocate_ring(struct iio_dev *indio_dev)
{
	iio_triggered_buffer_cleanup(indio_dev);
}


