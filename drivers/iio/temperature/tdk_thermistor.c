// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 InvenSense, Inc.
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
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/spi/spi.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/iio/kfifo_buf.h>

#define TAG "tdk-therm: "

#define TDK_THERMISTOR_DRV_NAME	"tdk_thermistor"

enum {
	TDK_THERM
};


static const struct iio_chan_spec tdk_therm_channels[] = {
	{
		.type = IIO_TEMP,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 14,
			.storagebits = 16,
			.shift = 3,
			.endianness = IIO_BE,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

struct tdk_thermistor_chip {
	const struct iio_chan_spec *channels;
	const unsigned long *scan_masks;
	u8 num_channels;
};

static const struct tdk_thermistor_chip tdk_thermistor_chips[] = {
	[TDK_THERM] = {
			.channels = tdk_therm_channels,
			.num_channels = ARRAY_SIZE(tdk_therm_channels),
		},
};

struct tdk_thermistor_data {
	struct spi_device *spi;
	struct device *dev;
	const struct tdk_thermistor_chip *chip;
	struct iio_trigger *trig;
	struct hrtimer timer;
	ktime_t period;
	u8 buffer[16] ____cacheline_aligned;
};

static bool current_state;
static int tdk_thermistor_read(struct tdk_thermistor_data *data,
			       struct iio_chan_spec const *chan, int *val);

#ifdef TEST_DRIVER
static struct task_struct *thread_st;
#endif

static int temp_trig_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct tdk_thermistor_data *data = iio_priv(indio_dev);
	struct device *dev = data->dev;

	dev_info(dev, "%s: state: %d\n", __func__, state);

	current_state = state;
	if (state)
		hrtimer_start(&data->timer, data->period, HRTIMER_MODE_REL);
	else
		hrtimer_cancel(&data->timer);

	return 0;
}

static const struct iio_trigger_ops temp_trigger_ops = {
	.set_trigger_state = temp_trig_set_state,
};

/*HR Timer callback function*/
static enum hrtimer_restart tdk_thermistor_hrtimer_handler(struct hrtimer *t)
{
	struct tdk_thermistor_data *data =
		container_of(t, struct tdk_thermistor_data, timer);

	if (!data)
		return HRTIMER_NORESTART;

	pr_info(TAG "%s: t: %lld\n",
		__func__, ktime_get_boot_ns());

	if (data->trig != NULL)
		iio_trigger_poll(data->trig);
	else
		pr_info(TAG "%s: Trigger is NULL\n", __func__);

	hrtimer_forward_now(t, data->period);

	return HRTIMER_RESTART;
}


#ifdef TEST_DRIVER /*Test thread function to read and display sensor data*/
static int test_thread_fn(void *input_data)
{
	int cycle_cnt = 0;
	int value = 0;
	struct tdk_thermistor_data *data =
		(struct tdk_thermistor_data *)input_data;
	struct device *dev = data->dev;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	if (indio_dev == NULL || dev == NULL)
		pr_info(TAG "%s: indio_dev or dev NULL\n", __func__);

	pr_info(TAG "%s: Starting test thread\n", __func__);
	pr_info(TAG "%s: input data: %p spidev: %p\n",
		__func__, input_data, data->spi);


	msleep(10000);

	pr_info(TAG "%s: Triggered calibration of TDK thermistor\n", __func__);
	tdk_thermistor_calibrate(data);
	msleep(500);

	while (1) {
		int temp_data = 0;
		int temp_voltage;
		int temp_resistance;


		cycle_cnt++;
		msleep(1000);
		pr_info(TAG "%s: Reading TDK thermistor\n", __func__);
		tdk_thermistor_read(data, NULL, &temp_data);
		pr_info(TAG "%s: Read TDK thermistor: %i\n",
			__func__, temp_data);

		/* multiply by 100*/
		temp_voltage = ((temp_data) * 330) / 16368;
		temp_resistance = (3300000 / temp_voltage) - 10000;

		pr_info(
		TAG "%s: Read TDK thermistor voltage %i resistance: %i\n",
			 __func__, temp_voltage, temp_resistance);
	}

	do_exit(0);
	return 0;
}
#endif


static int tdk_thermistor_read(struct tdk_thermistor_data *data,
			       struct iio_chan_spec const *chan, int *val)
{
	u8 raw_data[3];
	int ret;

	pr_info(TAG "%s: Reading thermistor\n", __func__);

	/* Requires 18 clock cycles to get ADC data.  First falling edge
	 * is zero, then 14 bits of data, followed by 3 additional zeroes.
	 * Use of 3 bytes provide 24 clock cycles.
	 */
	ret = spi_read(data->spi, (void *)raw_data, 3);

	if (ret) {
		pr_info(TAG "%s: Failed SPI read: %i\n", __func__, ret);
		return ret;
	}

	pr_info(TAG "%s: Read SPI: 0x%x 0x%x 0x%x\n", __func__,
		raw_data[0], raw_data[1], raw_data[2]);

	/* check to be sure this is a valid reading */
	if (raw_data[0] == 0 && raw_data[1] == 0 && raw_data[2] == 0)
		return -EINVAL;

	*val = ((raw_data[0] & 0x7F) << 7) | (raw_data[1] >> 1);

	pr_info(TAG "%s: Read temp: %i\n", __func__, *val);

	return 0;
}

static int tdk_thermistor_read_raw(struct iio_dev *indio_dev,
					struct iio_chan_spec const *chan,
					int *val, int *val2, long mask)
{
	struct tdk_thermistor_data *data = iio_priv(indio_dev);
	int ret = -EINVAL;

	pr_info(TAG "%s: Read raw TDK thermistor, mask: %li\n", __func__, mask);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret)
			return ret;

		ret = tdk_thermistor_read(data, chan, val);
		iio_device_release_direct_mode(indio_dev);

		if (!ret)
			return IIO_VAL_INT;

		break;
	case IIO_CHAN_INFO_SCALE:
		pr_info(TAG "%s: Read TDK thermistor scale chan: val: %i %i\n",
			 __func__, *val, chan->channel2);
		switch (chan->channel2) {
		case IIO_MOD_TEMP_AMBIENT:
			*val = 62;
			*val2 = 500000; /* 1000 * 0.0625 */
			ret = IIO_VAL_INT_PLUS_MICRO;
			break;
		default:
			*val = 250; /* 1000 * 0.25 */
			ret = IIO_VAL_INT;
		};
		break;
	}

	return ret;
}

static const struct iio_info tdk_thermistor_info = {
	.read_raw = tdk_thermistor_read_raw,
};

/*IIO trigger callback function, top half*/
static irqreturn_t tdk_thermistor_store_time(int irq, void *p)
{
	struct iio_poll_func *pf = p;

	pf->timestamp = ktime_get_boot_ns();
	pr_info(TAG "%s: t: %llx\n", __func__, pf->timestamp);

	return IRQ_WAKE_THREAD;
}

/*IIO trigger callback function(bottom half), */
/*reads data over SPI and pushes to buffer*/
static irqreturn_t tdk_thermistor_trigger_handler(int irq, void *private)
{
	struct iio_poll_func *pf = private;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct tdk_thermistor_data *data = iio_priv(indio_dev);
	int ret;

	pr_info(TAG "%s: Triggered read TDK thermistor\n", __func__);

	ret = spi_read(data->spi, data->buffer, 3);
	if (!ret) {
		iio_push_to_buffers_with_timestamp(indio_dev, data->buffer,
						   iio_get_time_ns(indio_dev));
	}
	pr_info(TAG "%s: Read SPI: 0x%x 0x%x 0x%x\n", __func__,
			data->buffer[0], data->buffer[1], data->buffer[2]);
#ifdef TEST_DRIVER
	/* check to be sure this is a valid reading */
	if (data->buffer[0] == 0 && data->buffer[1] == 0 &&
		data->buffer[2] == 0)
		pr_info(TAG "%s: Invalid reading\n", __func__);//return -EINVAL;

	val = ((data->buffer[0] & 0x7F) << 7) | (data->buffer[1] >> 1);

	pr_info(TAG "%s: Read temp: %i\n", __func__, val);
#endif
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}


static int tdk_thermistor_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct iio_dev *indio_dev;
	struct tdk_thermistor_data *data;
	const struct tdk_thermistor_chip *chip =
			&tdk_thermistor_chips[id->driver_data];
	struct iio_buffer *buffer;
	int ret = 0;

	pr_info(TAG "%s: Probing call for TDK thermistor\n", __func__);
	pr_info(TAG "%s: SPI Device detected: %s offset: %lu\n",
	__func__, id->name, id->driver_data);

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*data));
	if (!indio_dev) {
		pr_info(
		TAG "%s: Failed to alloc memory TDK thermistor\n", __func__);
		return -ENOMEM;
	}

	buffer = devm_iio_kfifo_allocate(&spi->dev);
	if (!buffer) {
		pr_info(
		TAG "%s: Failed to alloc memory TDK thermistor\n", __func__);
		return -ENOMEM;
	}


	iio_device_attach_buffer(indio_dev, buffer);

	indio_dev->info = &tdk_thermistor_info;
	indio_dev->name = TDK_THERMISTOR_DRV_NAME;
	indio_dev->channels = chip->channels;
	indio_dev->available_scan_masks = chip->scan_masks;
	indio_dev->num_channels = chip->num_channels;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->dev.parent = &spi->dev;

	data = iio_priv(indio_dev);
	data->spi = spi;
	data->dev = &spi->dev;
	data->chip = chip;

	/*IIO Triggered buffer setting*/
	ret = iio_triggered_buffer_setup(indio_dev, tdk_thermistor_store_time,
				tdk_thermistor_trigger_handler, NULL);
	if (ret)
		return ret;

	data->trig = iio_trigger_alloc("%s-hrtimer%d", indio_dev->name,
				indio_dev->id);
	if (data->trig == NULL) {
		ret = -ENOMEM;
		dev_err(&spi->dev, "iio trigger alloc error\n");
		goto error_unreg_buffer;
	}

	data->trig->dev.parent = &spi->dev;
	data->trig->ops = &temp_trigger_ops;
	iio_trigger_set_drvdata(data->trig, indio_dev);

	ret = iio_trigger_register(data->trig);
	if (ret) {
		dev_err(&spi->dev, "iio trigger register error %d\n", ret);
		goto error_unreg_buffer;
	}

	iio_trigger_get(data->trig);
	indio_dev->trig = data->trig;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_unreg_buffer;

	/*Initialize timer for iio buffer updating */
	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->period = ns_to_ktime((NSEC_PER_SEC / 1));
	/*TBD: test and set appropriate period for timer interrupt*/
	data->timer.function = tdk_thermistor_hrtimer_handler;

#ifdef TEST_DRIVER
	pr_info(
TAG  "%s: Creating test thread for thermistor with data: %p spidev: %p\n",
		__func__, data, data->spi);

	thread_st = kthread_run(test_thread_fn, data,
		"TDK Thermistor Test Thread");

	if (thread_st)
		pr_info(TAG  "%s: Thread Created successfully\n", __func__);
	else
		pr_info(TAG  "%s: Thread creation failed\n", __func__);
#endif
	return 0;

error_unreg_buffer:
	iio_triggered_buffer_cleanup(indio_dev);

	return ret;
}

static int tdk_thermistor_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);

	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);

	return 0;
}

static const struct spi_device_id tdk_thermistor_id[] = {
	{"tdktherm", TDK_THERM},
	{},
};
MODULE_DEVICE_TABLE(spi, tdk_thermistor_id);

static struct spi_driver tdk_thermistor_driver = {
	.driver = {
		.name	= TDK_THERMISTOR_DRV_NAME,
	},
	.probe		= tdk_thermistor_probe,
	.remove		= tdk_thermistor_remove,
	.id_table	= tdk_thermistor_id,
};

module_spi_driver(tdk_thermistor_driver);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense thermistor driver");
MODULE_LICENSE("GPL");

