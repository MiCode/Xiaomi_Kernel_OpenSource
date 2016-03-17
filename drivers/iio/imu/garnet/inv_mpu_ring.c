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
*/
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "inv_mpu_iio.h"
#include "inv_sh_data.h"
#include "inv_sh_sensor.h"
#include "inv_sh_misc.h"

#define MAX_IO_READ_SIZE	128

static irqreturn_t inv_irq_handler(int irq, void *dev_id)
{
	struct inv_mpu_state *st = (struct inv_mpu_state *)dev_id;

	st->timestamp = ktime_get_boottime();

	return IRQ_WAKE_THREAD;
}

static int dispatch_data(struct inv_mpu_state *st, struct inv_sh_data *data)
{
	ktime_t timestamp;
	int ret;

	/* manage command answer */
	if (data->is_answer) {
		inv_sh_misc_send_raw_data(st, data->raw,
					  data->size);
		ret = 0;
	} else {
		/* manage sensor data */
		timestamp = inv_sh_sensor_timestamp(st, data, st->timestamp);
		ret = inv_sh_sensor_dispatch_data(st, data, timestamp);
		if (ret == -ENODEV) {
			inv_sh_misc_send_sensor_data(st, data, timestamp);
			ret = 0;
		}
	}

	return ret;
}

static void recover_data(struct inv_mpu_state *st,
			 uint8_t **dptr, size_t *dsize)
{
	struct inv_sh_data sensor_data;
	uint8_t *ptr = *dptr;
	size_t size = *dsize;

	/* search for first valid data dropping byte 1 by 1 */
	while (size > 0 && inv_sh_data_parse(ptr, size, &sensor_data) != 0) {
		ptr++;
		size--;
	}

	dev_err(st->dev, "data error recovery by dropping %zu bytes\n",
		*dsize - size);

	*dptr = ptr;
	*dsize = size;
}

static int read_fifo(struct inv_mpu_state *st, struct inv_fifo *fifo)
{
	uint8_t data[2];
	uint16_t fifo_count;
	uint8_t *dptr;
	size_t size;
	size_t total_bytes, rem_bytes;
	int ret;

	ret = inv_set_bank(st, 0);
	if (ret)
		return ret;
	ret = inv_set_fifo_index(st, fifo->index);
	if (ret)
		return ret;
	ret = inv_plat_read(st, REG_FIFO_COUNTH, 2, data);
	if (ret)
		return ret;
	fifo_count = ((data[0] << 8) | data[1]);
	if (!fifo_count)
		return 0;

	if (fifo_count > st->fifo_length) {
		dev_err(st->dev, "FIFO count (%hu) internal error truncating\n",
				fifo_count);
		fifo_count = st->fifo_length;
	}

	/* losing bytes if buffer is not big enough, should never happen */
	rem_bytes = fifo->size - fifo->count;
	if (fifo_count > rem_bytes) {
		total_bytes = fifo_count - rem_bytes;
		dev_err(st->dev, "not enough space in buffer losing %zu bytes\n",
				total_bytes);
		dptr = fifo->buffer + total_bytes;
		size = fifo->count - total_bytes;
		memmove(fifo->buffer, dptr, size);
		fifo->count = size;
	}

	/* reading fifo data in buffer */
	total_bytes = fifo_count;
	while (total_bytes > 0) {
		dptr = &fifo->buffer[fifo->count];
		size = total_bytes;
		/* clip with max io read size */
		if (size > MAX_IO_READ_SIZE)
			size = MAX_IO_READ_SIZE;
		ret = inv_plat_read(st, REG_FIFO_R_W, size, dptr);
		if (ret)
			return ret;
		total_bytes -= size;
		fifo->count += size;
	}

	return 0;
}

static bool parse_fifo(struct inv_mpu_state *st, struct inv_fifo *fifo)
{
	uint8_t *dptr;
	size_t size;
	struct inv_sh_data sensor_data;
	bool retry;
	bool wake_data = false;
	int ret;

	/* parsing and dispatching data to sensors */
	dptr = fifo->buffer;
	size = fifo->count;
	do {
		while (inv_sh_data_parse(dptr, size, &sensor_data) == 0) {
			ret = dispatch_data(st, &sensor_data);
			if (ret != 0)
				dev_err(st->dev, "error %d dispatching data\n",
					ret);
			/* mark if there is wake-up sensor data */
			if (sensor_data.id & INV_SH_DATA_SENSOR_ID_FLAG_WAKE_UP)
				wake_data = true;
			dptr += sensor_data.size;
			size -= sensor_data.size;
		}
		/* try to manage incorrect frame by dropping bytes */
		if (size >= INV_SH_DATA_MAX_SIZE) {
			recover_data(st, &dptr, &size);
			retry = true;
		} else {
			retry = false;
		}
	} while (retry);

	/* update data buffer */
	memmove(fifo->buffer, dptr, size);
	fifo->count = size;

	return wake_data;
}

static int inv_process_data(struct inv_mpu_state *st, bool *wake_data)
{
	static const uint8_t it_regs[INV_FIFO_DATA_NB] = {
		REG_B0_SCRATCH_INT0_STATUS,
		REG_B0_SCRATCH_INT1_STATUS,
	};
	uint8_t it_vals[INV_FIFO_DATA_NB] = { 0 };
	bool spurious = true;
	bool wake;
	int i, ret;

	*wake_data = false;

	inv_wake_start(st);

	ret = inv_set_bank(st, 0);
	if (ret)
		goto error_wake;

	/* reading int registers */
	for (i = 0; i < INV_FIFO_DATA_NB; ++i) {
		ret = inv_plat_single_read(st, it_regs[i], &it_vals[i]);
		if (ret)
			goto error_wake;
		if (it_vals[i] != 0)
			spurious = false;
	}

	if (spurious) {
		ret = 0;
		goto error_wake;
	}

	/* reading corresponding FIFOs */
	for (i = 0; i < INV_FIFO_DATA_NB; ++i)
		if (it_vals[i] != 0) {
			ret = read_fifo(st, &st->datafifos[i]);
			if (ret)
				goto error_wake;
		}

	inv_wake_stop(st);

	/* parsing corresponding FIFOs */
	for (i = 0; i < INV_FIFO_DATA_NB; ++i)
		if (it_vals[i] != 0) {
			wake = parse_fifo(st, &st->datafifos[i]);
			if (wake)
				*wake_data = true;
		}

	return 0;
error_wake:
	inv_wake_stop(st);
	return ret;
}

/*
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
irqreturn_t inv_read_fifo(int irq, void *dev_id)
{
	struct inv_mpu_state *st = (struct inv_mpu_state *)dev_id;
	bool wake_data;
	int ret;

	mutex_lock(&st->lock);
	ret = inv_process_data(st, &wake_data);
	mutex_unlock(&st->lock);

	if (ret)
		dev_err(st->dev, "error reading fifo: %d\n", ret);

	/* Android spec: the driver must hold a "timeout wake lock" for 200 ms
	 * each time an event is being reported
	 */
	if (ret == 0 && wake_data)
		wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));

	return IRQ_HANDLED;
}

void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct inv_fifo *fifo;
	int i;

	free_irq(st->irq, st);
	for (i = 0; i < INV_FIFO_DATA_NB; ++i) {
		fifo = &st->datafifos[i];
		kfree(fifo->buffer);
	}
	iio_triggered_buffer_cleanup(indio_dev);
};

int inv_mpu_configure_ring(struct iio_dev *indio_dev)
{
	static const uint8_t fifo_indexes[INV_FIFO_DATA_NB] = {
		INV_FIFO_DATA_NORMAL_INDEX,
		INV_FIFO_DATA_WAKEUP_INDEX,
	};
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct inv_fifo *fifo;
	int i, ret;

	ret = iio_triggered_buffer_setup(indio_dev, &iio_pollfunc_store_time,
						NULL, NULL);
	if (ret)
		goto error;

	for (i = 0; i < INV_FIFO_DATA_NB; ++i) {
		fifo = &st->datafifos[i];
		fifo->index = fifo_indexes[i];
		/* Size is (FIFO size + 1 frame) for avoiding overflow */
		fifo->size = st->fifo_length + INV_SH_DATA_MAX_SIZE;
		fifo->count = 0;
		fifo->buffer = kmalloc(fifo->size, GFP_KERNEL);
		if (!fifo->buffer) {
			ret = -ENOMEM;
			goto error_fifo_free;
		}
	}

	ret = request_threaded_irq(st->irq, inv_irq_handler, inv_read_fifo,
			IRQF_TRIGGER_RISING | IRQF_SHARED | IRQF_ONESHOT,
			"inv_mpu_wake", st);
	if (ret)
		goto error_fifo_free;

	return 0;
error_fifo_free:
	for (i = 0; i < INV_FIFO_DATA_NB; ++i) {
		fifo = &st->datafifos[i];
		kfree(fifo->buffer);
	}
	iio_triggered_buffer_cleanup(indio_dev);
error:
	return ret;
}
