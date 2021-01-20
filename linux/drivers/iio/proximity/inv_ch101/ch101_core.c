// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 InvenSense, Inc.
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
#include <linux/device.h>	/* for struct device */
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/kthread.h>

#include "ch101_client.h"
#include "ch101_data.h"
#include "ch101_reg.h"
#include "ch101_sysfs.h"

#include "src/init_driver.h"


#define CH101_GPIO_RESET	"rst"
#define CH101_GPIO_PROG		"prg"
#define CH101_GPIO_INT		"int"
#define CH101_GPIO_RST_PULSE	"rtc_rst"
#define CH101_IRQ_NAME		"ch101_event"

#define CH101_MIN_FREQ_HZ	1
#define CH101_MAX_FREQ_HZ	10
#define CH101_DEFAULT_FREQ	5

#define CH101_IQ_PACK		7		     // Max 8 samples (256 bits)
#define CH101_IQ_PACK_BYTES	(4 * CH101_IQ_PACK) // Max 32 bytes (256 bits)
#define TIMER_COUNTER		50
#define TIMEOUT_US			(200 * 1000) // 200 ms
#define TEST_RUN_TIME		0		// seconds


static bool start;
static bool stop;
static bool int_cal_request;
static bool prog_cal_request;
static int  cal_count = TIMER_COUNTER;
static int  ss_count = TIMER_COUNTER;
static bool current_state;

static struct ch101_i2c_bus ch101_store;

static void init_fw(struct ch101_data *data);
static void test_gpios(struct ch101_data *data);
static int test_rst_gpio(struct ch101_data *data, struct gpio_desc *rst);

static const struct iio_event_spec ch101_events[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_EITHER,
		.mask_separate = BIT(IIO_EV_INFO_ENABLE),
	},
};


#define CH101_CHANNEL_POSITION(idx) {					\
	.type = IIO_POSITIONRELATIVE,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.indexed = 1,						\
	.scan_index = idx,					\
	.channel = idx,						\
	.scan_type = {						\
		.sign = 'u',					\
		.realbits = 8,		\
		.storagebits = 8,		\
		.shift = 0,					\
	},							\
}

#define CH101_CHANNEL_IQ(idx) {					\
	.type = IIO_PROXIMITY,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),\
	.indexed = 1,						\
	.scan_index = idx,					\
	.channel = idx,						\
	.event_spec = ch101_events,				\
	.num_event_specs = ARRAY_SIZE(ch101_events),		\
	.scan_type = {						\
		.sign = 'u',					\
		.realbits = 8 * CH101_IQ_PACK_BYTES,		\
		.storagebits = 8 * CH101_IQ_PACK_BYTES,		\
		.shift = 0,					\
	},							\
}

#define CH101_CHANNEL_DISTANCE(idx) {				\
	.type = IIO_DISTANCE,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SCALE),	\
	.indexed = 1,						\
	.scan_index = idx,					\
	.channel = idx,						\
	.scan_type = {						\
		.sign = 'u',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.shift = 0,					\
	},							\
}

#define CH101_CHANNEL_INTENSITY(idx) {				\
	.type = IIO_INTENSITY,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_all =				\
		BIT(IIO_CHAN_INFO_CALIBSCALE) |			\
		BIT(IIO_CHAN_INFO_CALIBBIAS),			\
	.indexed = 1,						\
	.scan_index = idx,					\
	.channel = idx,						\
	.scan_type = {						\
		.sign = 'u',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.shift = 0,					\
	},							\
}

enum ch101_chan {
	IQ_0,
	IQ_1,
	IQ_2,
	IQ_3,
	IQ_4,
	IQ_5,
	DISTANCE_0,
	DISTANCE_1,
	DISTANCE_2,
	DISTANCE_3,
	DISTANCE_4,
	DISTANCE_5,
	INTENSITY_0,
	INTENSITY_1,
	INTENSITY_2,
	INTENSITY_3,
	INTENSITY_4,
	INTENSITY_5,
	POSITION_0,
	POSITION_1,
	POSITION_2,
	POSITION_3,
	POSITION_4,
	POSITION_5,
	TIME
};

static const struct iio_chan_spec ch101_channels[] = {
	CH101_CHANNEL_IQ(IQ_0),
	CH101_CHANNEL_IQ(IQ_1),
	CH101_CHANNEL_IQ(IQ_2),
	CH101_CHANNEL_IQ(IQ_3),
	CH101_CHANNEL_IQ(IQ_4),
	CH101_CHANNEL_IQ(IQ_5),
	CH101_CHANNEL_DISTANCE(DISTANCE_0),
	CH101_CHANNEL_DISTANCE(DISTANCE_1),
	CH101_CHANNEL_DISTANCE(DISTANCE_2),
	CH101_CHANNEL_DISTANCE(DISTANCE_3),
	CH101_CHANNEL_DISTANCE(DISTANCE_4),
	CH101_CHANNEL_DISTANCE(DISTANCE_5),
	CH101_CHANNEL_INTENSITY(INTENSITY_0),
	CH101_CHANNEL_INTENSITY(INTENSITY_1),
	CH101_CHANNEL_INTENSITY(INTENSITY_2),
	CH101_CHANNEL_INTENSITY(INTENSITY_3),
	CH101_CHANNEL_INTENSITY(INTENSITY_4),
	CH101_CHANNEL_INTENSITY(INTENSITY_5),
	CH101_CHANNEL_POSITION(POSITION_0),
	CH101_CHANNEL_POSITION(POSITION_1),
	CH101_CHANNEL_POSITION(POSITION_2),
	CH101_CHANNEL_POSITION(POSITION_3),
	CH101_CHANNEL_POSITION(POSITION_4),
	CH101_CHANNEL_POSITION(POSITION_5),
	IIO_CHAN_SOFT_TIMESTAMP(TIME),
};

static void ch101_read_range_data(struct ch101_data *data,
	int ind, int *range)
{
	*range = data->buffer.distance[ind];
	pr_info(TAG "%s: %d: range: %d\n", __func__, ind, *range);
}

static void ch101_read_amplitude_data(struct ch101_data *data,
	int ind, int *amplitude)
{
	*amplitude = data->buffer.amplitude[ind];
	pr_info(TAG "%s: %d: amplitude: %d\n", __func__, ind, *amplitude);
}

static void ch101_set_freq(struct ch101_data *data, int freq)
{
	if (freq < CH101_MIN_FREQ_HZ)
		freq = CH101_MIN_FREQ_HZ;
	else if (freq > CH101_MAX_FREQ_HZ)
		freq = CH101_MAX_FREQ_HZ;

	data->scan_rate = freq;
	data->period = ns_to_ktime((NSEC_PER_SEC / freq));
}

extern int is_sensor_connected(int ind);

static int ch101_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val, int *val2,
		long mask)
{
	struct ch101_data *data = iio_priv(indio_dev);
	int ret;
	int ind;
	int range;
	int amplitude;

	init_fw(data);

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = data->scan_rate;
		*val2 = 0;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBSCALE:
		*val = (0x01 * int_cal_request) & (0x02 * prog_cal_request);
		*val2 = 0;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBBIAS:
		*val = cal_count;
		*val2 = 0;
		return IIO_VAL_INT;
	default:
		break;
	}

	switch (chan->type) {
	case IIO_INTENSITY:
		switch (mask) {
		case IIO_CHAN_INFO_RAW:
			ret = iio_device_claim_direct_mode(indio_dev);
			if (ret)
				return ret;

			ind = chan->channel - INTENSITY_0;
			ch101_read_amplitude_data(data, ind, &amplitude);
			*val = amplitude;
			*val2 = 0;

			iio_device_release_direct_mode(indio_dev);
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}

	case IIO_DISTANCE:
		switch (mask) {
		case IIO_CHAN_INFO_RAW:
			ret = iio_device_claim_direct_mode(indio_dev);
			if (ret)
				return ret;

			ind = chan->channel - DISTANCE_0;
			ch101_read_range_data(data, ind, &range);
			*val = range;
			*val2 = 0;

			iio_device_release_direct_mode(indio_dev);
			return IIO_VAL_INT;

		case IIO_CHAN_INFO_SCALE:
			*val = 32;
			*val2 = 0;
			return IIO_VAL_INT;

		default:
			return -EINVAL;
		}

	case IIO_PROXIMITY:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret)
			return ret;

		ind = chan->channel - IQ_0;
		*val = data->buffer.nb_samples[ind];
		*val2 = 0;

		iio_device_release_direct_mode(indio_dev);

		return IIO_VAL_INT;

	case IIO_POSITIONRELATIVE:
		ind = chan->channel - POSITION_0;
		*val = is_sensor_connected(ind);
		*val2 = 0;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

void set_output_samples(int ind, int val);

static int ch101_write_raw(struct iio_dev *indio_dev,
		const struct iio_chan_spec *chan, int val, int val2, long mask)
{
	struct ch101_data *data = iio_priv(indio_dev);
	int ind;

	pr_info(TAG "%s: type: %d, mask: %lu val: %d val2: %d\n", __func__,
		chan->type, mask, val, val2);

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		ch101_set_freq(data, val);
		return 0;
	case IIO_CHAN_INFO_CALIBSCALE:
		int_cal_request = val & 0x01;
		prog_cal_request = val & 0x02;
		return 0;
	case IIO_CHAN_INFO_CALIBBIAS:
		cal_count = val;
		ss_count = val;
		data->counter = ss_count;
		return 0;
	default:
		break;
	}
	switch (chan->type) {
	case IIO_POSITIONRELATIVE:
		ind = chan->channel - POSITION_0;
		set_output_samples(ind, val);
		return 0;
	default:
		return -EINVAL;
	}
}

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL
("1 2 5 10");

static struct attribute *ch101_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group ch101_attribute_group = {
	.name = "ch101_attr",
	.attrs = ch101_attributes
};

static const struct iio_info ch101_info = {
	.attrs = &ch101_attribute_group,
	.read_raw = &ch101_read_raw,
	.write_raw = &ch101_write_raw,
};

static int ch101_trig_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct ch101_data *data = iio_priv(indio_dev);
	struct device *dev = data->dev;

	dev_info(dev, "%s: state: %d\n", __func__, state);

	init_fw(data);

	current_state = state;
	if (state)
		hrtimer_start(&data->timer, data->period, HRTIMER_MODE_REL);
	else
		hrtimer_cancel(&data->timer);

	return 0;
}

static const struct iio_trigger_ops ch101_trigger_ops = {
	.set_trigger_state = ch101_trig_set_state,
};

static irqreturn_t ch101_store_time(int irq, void *p)
{
	struct iio_poll_func *pf = p;

	pf->timestamp = ktime_get_boot_ns();
	pr_info(TAG "%s: t: %llu\n", __func__, pf->timestamp);

	return IRQ_WAKE_THREAD;
}

static char ch101_iio_buffer[256];

static irqreturn_t ch101_trigger_handler(int irq, void *handle)
{
	struct iio_poll_func *pf = handle;
	struct iio_dev *indio_dev = pf->indio_dev;

	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static s64 starting_ts;

static int ch101_push_to_buffer(void *input_data)
{
	struct ch101_data *data = (struct ch101_data *)input_data;
	struct device *dev = data->dev;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ch101_buffer *buffer = &(data->buffer);
	u8 *buf;
	int bit, ret;
	int i, ind, len;
	u8 mode;
	int cur_max_samples;

	pr_info(TAG "%s: mask: %02x\n", __func__,
			*indio_dev->active_scan_mask);

	mutex_lock(&data->lock);

	mode = 0;
	cur_max_samples = 0;
	for_each_set_bit(bit, indio_dev->active_scan_mask,
							indio_dev->masklength) {
		if (bit >= POSITION_0 && bit < TIME) {
			ind = bit - POSITION_0;
			mode |= buffer->mode[ind];
			if (cur_max_samples < buffer->nb_samples[ind])
				cur_max_samples = buffer->nb_samples[ind];
		}
	}
	if (mode == 0)
		goto out;

	buf = ch101_iio_buffer;
	pr_info(TAG "scan bytes: %d, ts=%lld\n", indio_dev->scan_bytes,
						starting_ts);

	for (i = 0; i < cur_max_samples; i += CH101_IQ_PACK) {
		u8 *pbuf = buf;

		memset(buf, 0, indio_dev->scan_bytes);
		len = CH101_IQ_PACK_BYTES;
		if (len > (cur_max_samples - i) * 4)
			len = (cur_max_samples - i) * 4;

		for_each_set_bit(bit, indio_dev->active_scan_mask,
					indio_dev->masklength) {
			if (bit >= IQ_0 && bit < DISTANCE_0) {
				ind = bit - IQ_0;
				memcpy(pbuf, &(buffer->iq_data[ind][i]), len);
				pbuf += CH101_IQ_PACK_BYTES;
				//pr_info("iq_data: %d %d\n",
					//buffer->iq_data[ind][i].I,
					//buffer->iq_data[ind][i].Q);
			} else if (bit >= DISTANCE_0 && bit < INTENSITY_0) {
				ind = bit - DISTANCE_0;
				memcpy(pbuf, &(buffer->distance[ind]), 2);
				pbuf += sizeof(u16);
			} else if (bit >= INTENSITY_0 && bit < POSITION_0) {
				ind = bit - INTENSITY_0;
				memcpy(pbuf, &(buffer->amplitude[ind]), 2);
				pbuf += sizeof(u16);
			} else if (bit >= POSITION_0 && bit < TIME) {
				ind = bit - POSITION_0;
				mode = buffer->mode[ind];
				memcpy(pbuf, &mode, 1);
				pbuf += sizeof(u8);
				//pr_info("mode copy=%d, %d\n", ind, mode);
			}
		}
		pbuf += sizeof(u64);
		ret = iio_push_to_buffers_with_timestamp(indio_dev, buf,
						starting_ts);

		pr_info(TAG "push tobuffer=%d, size=%d\n", i,
			(int)(pbuf-buf));
	}
out:
	mutex_unlock(&data->lock);

	return 0;
}

static int control_thread_fn(void *input_data)
{
	struct ch101_data *data = (struct ch101_data *)input_data;
	struct device *dev = data->dev;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	start = true;
	stop = false;

	dev_info(dev, "%s: Start", __func__);

	while (start) {
		wait_for_completion(&data->data_completion);

		dev_info(dev, "%s: , mode: %02x",
				__func__, indio_dev->currentmode);

		starting_ts = iio_get_time_ns(indio_dev);
		single_shot_driver();
		if (current_state)
			ch101_push_to_buffer(input_data);
		//iio_trigger_poll(indio_dev->trig);
		if (int_cal_request || prog_cal_request) {
			dev_info(dev, "%s: cal_request", __func__);
			test_gpios(data);
			int_cal_request = false;
			prog_cal_request = false;
		}
	}

	stop = true;

	return 0;
}

static enum hrtimer_restart ch101_hrtimer_handler(struct hrtimer *t)
{
	struct ch101_data *data = container_of(t, struct ch101_data, timer);

	if (!data)
		return HRTIMER_NORESTART;

	pr_info(TAG "%s: %d\n", __func__, data->counter);
	if (data->counter-- <= 0) {
		data->counter = ss_count;
		pr_info(TAG "%s: Stop\n", __func__);
		return HRTIMER_NORESTART;
	}

	pr_info(TAG "%s: t: %lld, counter: %d\n",
		__func__, ktime_get_boot_ns(), data->counter);

	complete(&data->data_completion);

	hrtimer_forward_now(t, data->period);

	return HRTIMER_RESTART;
}

static void set_gpios_bus(struct device *dev, struct ch101_i2c_bus *bus)
{
	int i;

	for (i = 0; i < MAX_DEVICES; i++) {
		of_property_read_u32_index(dev->of_node, "prg-gpios", i,
					&(bus->gpio_exp_prog_pin[i]));

		dev_info(dev, "gpio exp prog pin %d is %d\n",
			i, bus->gpio_exp_prog_pin[i]);


		if (bus->gpiod_int[i] == NULL) {
			bus->gpiod_int[i] = devm_gpiod_get_index(dev,
				CH101_GPIO_INT, i, GPIOD_OUT_LOW);
			if (IS_ERR(bus->gpiod_int[i])) {
				dev_warn(dev, "gpio get INT pin failed\n");
				bus->gpiod_int[i] = NULL;
			}
		}

		dev_info(dev, "%s: %08p %d\n", __func__,
			bus->gpiod_int[i],
			bus->gpio_exp_prog_pin[i]);
	}
}

static void set_gpios(int bus_index, struct ch101_data *data, int rst_pin)
{
	struct device *dev = data->dev;
	struct ch101_gpios *gpios = &data->client.gpios;
	struct ch101_i2c_bus *bus = &data->client.bus[bus_index];
	int offset = bus_index * MAX_DEVICES;
	int i;

	if (gpios->gpiod_rst == NULL) {
		gpios->gpiod_rst = devm_gpiod_get_index(dev,
			CH101_GPIO_RESET, rst_pin, GPIOD_OUT_HIGH);
		if (IS_ERR(gpios->gpiod_rst) || test_rst_gpio(data,
		gpios->gpiod_rst)) {
			dev_warn(dev, "gpio get reset pin %d failed\n",
				rst_pin);
			gpios->gpiod_rst = NULL;
		} else {
			gpiod_set_value(gpios->gpiod_rst, 1);
			dev_info(dev, "%s: Reset found and disabled.\n",
				__func__);
		}
	} else {
		dev_info(dev, "%s: Reset pin already found.\n", __func__);
	}

	/* Get timer pulse reset control */
	if (gpios->gpiod_rst_pulse == NULL) {
		gpios->gpiod_rst_pulse = devm_gpiod_get_index(dev,
			CH101_GPIO_RST_PULSE, 0, GPIOD_OUT_HIGH);
		if (IS_ERR(gpios->gpiod_rst_pulse)) {
			dev_warn(dev, "gpio get timer reset pin failed\n");
			gpios->gpiod_rst_pulse = NULL;
		} else {
			gpiod_set_value(gpios->gpiod_rst_pulse, 1);
			dev_info(dev, "%s: Timer Reset found and disabled.\n",
					__func__);
		}
	} else {
		dev_info(dev, "%s: Timer Reset pin already found.\n", __func__);
	}

	dev_info(dev, "%s: Reset: %08p Pulse: %08p\n", __func__,
			gpios->gpiod_rst, gpios->gpiod_rst_pulse);

	for (i = 0; i < MAX_DEVICES; i++) {
		int index = i + offset;

		of_property_read_u32_index(dev->of_node, "prg-gpios", index,
					&(bus->gpio_exp_prog_pin[i]));

		dev_info(dev, "gpio exp prog pin %d is %d\n",
			index, bus->gpio_exp_prog_pin[i]);


		if (bus->gpiod_int[i] == NULL) {
			bus->gpiod_int[i] = devm_gpiod_get_index(dev,
				CH101_GPIO_INT, index, GPIOD_OUT_LOW);
			if (IS_ERR(bus->gpiod_int[i])) {
				dev_warn(dev, "gpio get INT pin failed\n");
				bus->gpiod_int[i] = NULL;
			}
		}

		dev_info(dev, "%s: %08p %d\n", __func__,
				bus->gpiod_int[i], bus->gpio_exp_prog_pin[i]);
	}
}

static irqreturn_t ch101_event_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct ch101_data *data = iio_priv(indio_dev);
	struct device *dev = data->dev;
	int i = 0;

	dev_info(dev, "%s: irq: %d\n", __func__, irq);

	for (i = 0; i < CHBSP_MAX_DEVICES; i++) {
		if (data->irq[i] == irq)
			break;
	}

	if (i < CHBSP_MAX_DEVICES) {
//		dev_info(dev, "%s: irq: %d i: %d\n", __func__, irq, i);
		ext_ChirpINT0_handler(i);
	}

	return IRQ_HANDLED;
}

static irqreturn_t ch101_event_handler1(int irq, void *private)
{
	return ch101_event_handler(irq, private);
}

static irqreturn_t ch101_event_handler2(int irq, void *private)
{
	return ch101_event_handler(irq, private);
}

static irqreturn_t ch101_event_handler3(int irq, void *private)
{
	return ch101_event_handler(irq, private);
}

static irqreturn_t ch101_event_handler4(int irq, void *private)
{
	return ch101_event_handler(irq, private);
}

static irqreturn_t ch101_event_handler5(int irq, void *private)
{
	return ch101_event_handler(irq, private);
}

static irqreturn_t ch101_event_handler6(int irq, void *private)
{
	return ch101_event_handler(irq, private);
}
typedef irqreturn_t (*ch101_handler)(int irq, void *private);

static const ch101_handler int_table[] = {
	ch101_event_handler1,
	ch101_event_handler2,
	ch101_event_handler3,
	ch101_event_handler4,
	ch101_event_handler5,
	ch101_event_handler6
};

static void complete_done(struct ch101_client *client_drv)
{
	struct ch101_data *data;

	data = container_of(client_drv, struct ch101_data, client);
	complete(&data->ss_completion);
}

static int setup_int_gpio(struct ch101_client *client_drv, uint32_t pin)
{
	struct ch101_data *data;
	struct device *dev;
	struct ch101_i2c_bus *bus;
	struct iio_dev *indio_dev;
	int index = pin - CHIRP0_INT_0;
	int bus_index = index / MAX_DEVICES;
	int dev_index = index % MAX_DEVICES;
	char devname[32];
	int ret = 0;

	if (index < 0 || index >= CHBSP_MAX_DEVICES)
		return -EPERM;

	data = container_of(client_drv, struct ch101_data, client);
	dev = data->dev;
	indio_dev = dev_get_drvdata(dev);
	bus = &data->client.bus[bus_index];

	if (data->irq_set[index] == 1)
		return 0;

	if (data->irq[index] == 0)
		return -EPERM;

	snprintf(devname, sizeof(devname), "%s%d", CH101_IRQ_NAME, index);
	dev_info(dev, "%s: IRQ: %d %s\n", __func__, data->irq[index], devname);

	ret = gpiod_direction_input(bus->gpiod_int[dev_index]);
	if (ret < 0)
		dev_warn(dev, "gpiod_direction_input INT pin failed\n");

	// Enable IRQ handler
	ret = devm_request_threaded_irq(dev, data->irq[index],
			NULL, int_table[index],
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			devname, indio_dev);
	if (ret < 0)
		dev_err(dev, "devm_request_threaded_irq %d failed\n",
			data->irq[index]);

	data->irq_set[index] = (ret == 0);

	return ret;
}

static int free_int_gpio(struct ch101_client *client_drv, uint32_t pin)
{
	struct ch101_data *data;
	struct device *dev;
	struct ch101_i2c_bus *bus;
	struct iio_dev *indio_dev;
	int index = pin - CHIRP0_INT_0;
	int bus_index = index / MAX_DEVICES;
	int dev_index = index % MAX_DEVICES;
	int ret = 0;

	if (index < 0 || index >= CHBSP_MAX_DEVICES)
		return -EPERM;

	data = container_of(client_drv, struct ch101_data, client);
	dev = data->dev;
	indio_dev = dev_get_drvdata(dev);
	bus = &data->client.bus[bus_index];

//	dev_info(dev, "%s: IRQ: %d, set: %d\n",
//		__func__, data->irq[index], data->irq_set[index]);

	if (data->irq_set[index] == 0)
		return 0;

	if (data->irq[index] == 0)
		return -EPERM;

	dev_info(dev, "%s: IRQ: %d\n", __func__, data->irq[index]);

	free_irq(data->irq[index], indio_dev);

	ret = gpiod_direction_output(bus->gpiod_int[dev_index], 0);
	if (ret < 0) {
		dev_warn(dev, "gpiod_direction_input for IRQ %d failed\n",
			data->irq[index]);
	}

	data->irq_set[index] = 0;

	return ret;
}

static void test_gpios(struct ch101_data *data)
{
	int cycle_cnt = 0;
	int index;
	int bus_index;
	int dev_index;
	struct ch101_i2c_bus *bus;
	struct gpio_desc *gpio_int;
	struct ch101_callbacks *cbk = data->client.cbk;

	for (index = 0; index < CHBSP_MAX_DEVICES; index++) {
		bus_index = index / MAX_DEVICES;
		dev_index = index % MAX_DEVICES;
		bus = &data->client.bus[bus_index];
		if (bus->gpiod_int[dev_index] == NULL) {
			dev_err(data->dev, "%s: No valid INT pin\n", __func__);
			return;
		}
		if (bus->gpio_exp_prog_pin[dev_index] > CHBSP_MAX_DEVICES) {
			dev_err(data->dev, "%s: No valid PROG pin\n", __func__);
			return;
		}
	}

	for (index = 0; index < CHBSP_MAX_DEVICES; index++)
		free_int_gpio(&data->client, data->irq_map[index]);

	while (cycle_cnt++ < 2 * cal_count &&
		(int_cal_request || prog_cal_request)) {
		for (index = 0; index < CHBSP_MAX_DEVICES; index++) {
			int set_value;
			int raw_value;
			int value;
			int dir;
			int is_active_low;

			bus = &data->client.bus[bus_index];
			gpio_int = bus->gpiod_int[dev_index];

			bus_index = index / MAX_DEVICES;
			dev_index = index % MAX_DEVICES;

			if (int_cal_request) {

				dev_info(data->dev, "%s: %d %d\n",
					__func__, index, data->irq_map[index]);

				set_value = (cycle_cnt % 2);

				dev_info(data->dev, "%s: INT set to %d\n",
					__func__, set_value);
				gpiod_set_value(gpio_int, set_value);

				msleep(100);

				raw_value = gpiod_get_raw_value(gpio_int);
				value = gpiod_get_value(gpio_int);
				dir = gpiod_get_direction(gpio_int);
				is_active_low = gpiod_is_active_low(gpio_int);

				if (set_value == raw_value &&
					set_value == value) {
					dev_info(data->dev,
			"%s: Current INT pin state: rv: %i v: %i d: %i al: %i\n",
			__func__, raw_value, value, dir, is_active_low);
				} else {
					dev_err(data->dev,
				"%s: Error INT pin state: rv: %i v: %i d: %i al: %i\n",
				__func__, raw_value, value, dir, is_active_low);
				}
			}

			if (prog_cal_request) {
				int pin = bus->gpio_exp_prog_pin[dev_index];

				dev_info(data->dev, "%s: %d pin: %d\n",
					__func__, index, pin);

				set_value = (cycle_cnt % 2);

				dev_info(data->dev, "%s: PROG set to %d\n",
					__func__, set_value);
				cbk->set_pin_level(pin, set_value);

				msleep(100);
			}
		}
	}

	msleep(500);
}

static int test_rst_gpio(struct ch101_data *data, struct gpio_desc *rst)
{
	int error = 0;
	int i;

	for (i = 0; i < 2; i++) {
		int set_value;
		int raw_value;
		int value;
		int dir;
		int is_active_low;

		set_value = i % 2;

		dev_info(data->dev, "%s: RST set to %d\n", __func__, set_value);
		gpiod_set_value(rst, set_value);

		msleep(100);

		raw_value = gpiod_get_raw_value(rst);
		value = gpiod_get_value(rst);
		dir = gpiod_get_direction(rst);
		is_active_low = gpiod_is_active_low(rst);

		if (set_value == raw_value &&
		     set_value == value) {
			dev_info(data->dev,
			"%s: Current RST pin state: rv: %i v: %i d: %i al: %i\n",
			__func__, raw_value, value, dir, is_active_low);
		} else {
			error = 1;
			dev_err(data->dev,
			"%s: Error RST pin state: rv: %i v: %i d: %i al: %i\n",
			__func__, raw_value, value, dir, is_active_low);
		}
	}
	return error;
}

static void init_fw(struct ch101_data *data)
{
	if (data && !data->fw_initialized) {
		init_driver();
		config_driver();
		data->fw_initialized = 1;

		if (TEST_RUN_TIME) {
			start_driver(100, TEST_RUN_TIME * 1000);
			stop_driver();
		}
	}
}

int ch101_core_probe(struct i2c_client *client, struct regmap *regmap,
		struct ch101_callbacks *cbk, const char *name)
{
	struct ch101_data *data;
	struct ch101_i2c_bus *bus;
	struct device *dev;
	struct iio_dev *indio_dev;
	int ret = 0;
	int i = 0;

	dev = &client->dev;

	dev_info(dev, "%s: Start v.1.60 s: %d", __func__, sizeof(*data));

	if (ch101_store.i2c_client == NULL) {
		ch101_store.i2c_client = client;
		set_gpios_bus(dev, &ch101_store);
		return 0;
	}

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	dev_info(dev, "%s: indio_dev: %p, client: %p",
		__func__, indio_dev, client);

	data = iio_priv(indio_dev);
	dev_set_drvdata(dev, indio_dev);

	data->dev = dev;
	data->regmap = regmap;
	data->client.bus[0].i2c_client = client;
	data->client.cbk = cbk;
	data->client.cbk->data_complete = complete_done;
	data->client.cbk->setup_int_gpio = setup_int_gpio;
	data->client.cbk->free_int_gpio = free_int_gpio;

	if (ch101_store.i2c_client) {
		bus = &data->client.bus[1];
		bus->i2c_client = ch101_store.i2c_client;
		for (i = 0; i < MAX_DEVICES; i++) {
			bus->gpiod_int[i] =
				ch101_store.gpiod_int[i];
			bus->gpio_exp_prog_pin[i] =
				ch101_store.gpio_exp_prog_pin[i];
			dev_info(dev, "%s: %08p %d\n", __func__,
				bus->gpiod_int[i], bus->gpio_exp_prog_pin[i]);
		}
	}

	set_chirp_data(&data->client);
	set_chirp_buffer(&data->buffer);

	set_gpios(0, data, 0);

	ret = find_sensors();
	if (ret < 0) {
		dev_err(dev, "find_sensors: %d\n", ret);
		data->client.gpios.gpiod_rst = NULL;
	}

	if (ret < 0) {
		dev_err(dev, "No RST pin on sensors found\n");
		goto error_find_sensors;
	}

	msleep(50);

	indio_dev->dev.parent = dev;
	indio_dev->name = name;
	indio_dev->info = &ch101_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = ch101_channels;
	indio_dev->num_channels = ARRAY_SIZE(ch101_channels);

	mutex_init(&data->lock);
	init_completion(&data->ss_completion);
	init_completion(&data->data_completion);

	for (i = 0; i < CHBSP_MAX_DEVICES; i++) {
		int bus_index = i / MAX_DEVICES;
		int dev_index = i % MAX_DEVICES;
		struct gpio_desc *gpiod_int;

		gpiod_int = data->client.bus[bus_index].gpiod_int[dev_index];

		if (gpiod_int != NULL) {
			data->irq[i] = gpiod_to_irq(gpiod_int);
			dev_info(dev, "%s: IRQ: %d\n", __func__, data->irq[i]);
		} else {
			data->irq[i] = 0;
		}

		data->irq_map[i] = i + CHIRP0_INT_0;

		// Enable IRQ handler
		ret = setup_int_gpio(&data->client, data->irq_map[i]);
		if (ret < 0) {
			dev_err(dev, "devm_request_threaded_irq failed\n",
				data->irq[i]);
			break;
		}
	}

	if (ret < 0)
		goto error_request_threaded_irq;

	ret = iio_triggered_buffer_setup(indio_dev, ch101_store_time,
			ch101_trigger_handler, NULL);
	if (ret) {
		dev_err(&client->dev, "iio triggered buffer error %d\n", ret);
		goto error_triggered_buffer_setup;
	}

	data->trig = iio_trigger_alloc("%s-hrtimer%d", indio_dev->name,
			indio_dev->id);
	if (data->trig == NULL) {
		ret = -ENOMEM;
		dev_err(&client->dev, "iio trigger alloc error\n");
		goto error_trigger_alloc;
	}
	data->trig->dev.parent = &client->dev;
	data->trig->ops = &ch101_trigger_ops;
	iio_trigger_set_drvdata(data->trig, indio_dev);

	ret = iio_trigger_register(data->trig);
	if (ret) {
		dev_err(&client->dev, "iio trigger register error %d\n", ret);
		goto error_trigger_register;
	}
	iio_trigger_get(data->trig);
	indio_dev->trig = data->trig;

	for (i = 0; i < CHBSP_MAX_DEVICES; i++)
		free_int_gpio(&data->client, data->irq_map[i]);

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(dev, "iio_device_register failed: %d\n", ret);
		goto error_device_register;
	}

	ret = ch101_create_dmp_sysfs(indio_dev);
	if (ret) {
		dev_err(dev, "create dmp sysfs failed\n");
		goto error_dmp_sysfs;
	}

	data->fw_initialized = 0;
	if (!CH101_EXTERNAL_FW)
		init_fw(data);

	// Set timer count
	data->counter = ss_count;

	// Init the hrtimer after driver init --yd
	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ch101_set_freq(data, CH101_DEFAULT_FREQ);
	data->timer.function = ch101_hrtimer_handler;

	dev_info(dev, "%s: data->period: %d\n", __func__, data->period);

	{
	static struct task_struct *thread_st;

	thread_st = kthread_run(control_thread_fn, data,
		"CH101 Control Thread");
	}

	dev_info(dev, "%s: End\n", __func__);

	return ret;

error_device_register:
error_dmp_sysfs:
	iio_trigger_unregister(data->trig);

error_trigger_register:
	iio_trigger_free(data->trig);

error_trigger_alloc:
error_triggered_buffer_setup:
error_request_threaded_irq:
error_find_sensors:
	devm_iio_device_free(&client->dev, indio_dev);
	dev_err(dev, "%s: Error %d:\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(ch101_core_probe);

int ch101_core_remove(struct i2c_client *client)
{
	int ret = 0;

	struct device *dev = &client->dev;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ch101_data *data = iio_priv(indio_dev);
	struct ch101_callbacks *cbk = data->client.cbk;

	dev_info(dev, "%s: Start: %p\n", __func__, data);

	start = false;
	while (!stop)
		msleep(20);

	{
	// Enable IRQ handler
	int i = 0;

	for (i = 0; i < CHBSP_MAX_DEVICES; i++)
		setup_int_gpio(&data->client, data->irq_map[i]);
	}

	dev_info(dev, "%s: trig: %p\n", __func__, data->trig);
	iio_trigger_unregister(data->trig);
	iio_trigger_free(data->trig);

	iio_device_unregister(indio_dev);
	devm_iio_device_free(dev, indio_dev);
	devm_kfree(dev, cbk);

	dev_info(dev, "%s: End\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(ch101_core_remove);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense CH101 core device driver");
MODULE_LICENSE("GPL");
