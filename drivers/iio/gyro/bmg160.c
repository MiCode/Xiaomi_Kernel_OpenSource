/*
 * BMG160 Gyro Sensor driver
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#define BMG160_DRV_NAME			"bmg160"
#define BMG160_IRQ_NAME			"bmg160_event"
#define BMG160_GPIO_NAME		"gpio_int"

#define BMG160_REG_CHIP_ID		0x00
#define BMG160_CHIP_ID_VAL		0x0F

#define BMG160_REG_PMU_LPW		0x11
#define BMG160_PMU_SLP_MASK		0xE0
#define BMG160_PMU_SLP_SHIFT		5
#define BMG160_PMU_BIT_SLEEP_DUR_MASK	0x17
#define BMG160_PMU_BIT_SLEEP_DUR_SHIFT	1

#define BMG160_REG_RANGE		0x0F

#define BMG160_RANGE_2000DPS		0
#define BMG160_RANGE_1000DPS		1
#define BMG160_RANGE_500DPS		2
#define BMG160_RANGE_250DPS		3
#define BMG160_RANGE_125DPS		4

#define BMG160_REG_PMU_BW		0x10
#define BMG160_No_Filter		0
#define BMG160_DEF_BW			100

#define BMG160_REG_INT_MAP_0		0x17
#define BMG160_INT_MAP_0_BIT_ANY	BIT(1)

#define BMG160_REG_INT_RST_LATCH	0x21
#define BMG160_INT_MODE_LATCH_RESET	0x80
#define BMG160_INT_MODE_LATCH_INT	0x0F

#define BMG160_REG_INT_EN_0		0x15
#define BMG160_DATA_ENABLE_INT		BIT(7)

#define BMG160_REG_XOUT_L		0x02
#define BMG160_AXIS_TO_REG(axis)	(BMG160_REG_XOUT_L + (axis * 2))

#define BMG160_REG_SLOPE_THRES		0x1B
#define BMG160_SLOPE_THRES_MASK		0x0F

#define BMG160_REG_MOTION_INTR		0x1C
#define BMG160_INT_MOTION_X		BIT(0)
#define BMG160_INT_MOTION_Y		BIT(1)
#define BMG160_INT_MOTION_Z		BIT(2)
#define BMG160_ANY_DUR_MASK		0x30
#define BMG160_ANY_DUR_SHIFT		4
/*
 * We are using Any-motion detection as described in section 4.8.5.
 * The min sample duration for rate comparison, This will reduce the
 * affective rate by this factor, so for user space notification, use
 * this factor, so setting 100HZ actually results notification of
 * 25HZ only. This chip doesn't allow going below this value.
 */
#define BMG160_ANY_DUR_MIN_SAMPLE	4

#define BMG160_MAX_STARTUP_TIME_MS	80

#define BMG160_SLEEP_DELAY_MS	2000
static int bmg160_power_off_delay_ms = BMG160_SLEEP_DELAY_MS;
module_param(bmg160_power_off_delay_ms, int, 0644);
MODULE_PARM_DESC(bmg160_power_off_delay_ms,
	"BMG160 Gyroscope power of delay in milli seconds.");

struct bmg160_data {
	struct i2c_client *client;
	struct iio_trigger *trig;
	struct mutex mutex;
	s16 buffer[12];
	u8 bw_bits;
	u32 dps_range;
	int ev_enable_state;
	int slope_thres;
	bool trigger_on;
};

enum bmg160_axis {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
};

enum bmg160_operation_mode {
	BMG160_MODE_NORMAL,
	BMG160_MODE_DEEP_SUSPEND,
	BMG160_MODE_SUSPEND,
};

static const struct {
	int val;
	int bw_bits;
} bmg160_samp_freq_table[] = { {100, 0x07},
			       {200, 0x06},
			       {400, 0x03},
			       {1000, 0x02},
			       {2000, 0x01} };

static const struct {
	int scale;
	int dps_range;
} bmg160_scale_table[] = { { 1065, BMG160_RANGE_2000DPS},
			   { 532, BMG160_RANGE_1000DPS},
			   { 266, BMG160_RANGE_500DPS},
			   { 133, BMG160_RANGE_250DPS},
			   { 66, BMG160_RANGE_125DPS} };

static int bmg160_set_mode(struct bmg160_data *data,
			   enum bmg160_operation_mode mode)
{
	int ret;
	u8 lpw_bits = 0;

	lpw_bits = mode << BMG160_PMU_SLP_SHIFT;

	dev_dbg(&data->client->dev, "Set Mode bits %x\n", lpw_bits);

	ret = i2c_smbus_write_byte_data(data->client,
					BMG160_REG_PMU_LPW, lpw_bits);
	if (ret < 0) {
		dev_err(&data->client->dev, "Error writing reg_pmu_lpw\n");
		return ret;
	}

	return 0;
}

static int bmg160_convert_freq_to_bit(int val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bmg160_samp_freq_table); ++i) {
		if (bmg160_samp_freq_table[i].val == val)
			return bmg160_samp_freq_table[i].bw_bits;
	}

	return -EINVAL;
}

static int bmg160_set_bw(struct bmg160_data *data, int val)
{
	int ret;
	int bw_bits;

	bw_bits = bmg160_convert_freq_to_bit(val);
	if (bw_bits < 0)
		return bw_bits;

	ret = i2c_smbus_write_byte_data(data->client, BMG160_REG_PMU_BW,
					bw_bits);
	if (ret < 0) {
		dev_err(&data->client->dev, "Error writing pmu_bw\n");
		return ret;
	}

	data->bw_bits = bw_bits;

	return 0;
}

static int bmg160_chip_init(struct bmg160_data *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(data->client, BMG160_REG_CHIP_ID);
	if (ret < 0) {
		dev_err(&data->client->dev,
			"Error:BMG160_REG_CHIP_ID\n");
		return ret;
	}

	dev_dbg(&data->client->dev, "Chip Id %x\n", ret);
	if (ret != BMG160_CHIP_ID_VAL) {
		dev_err(&data->client->dev, "invalid chip %x\n", ret);
		return -ENODEV;
	}

	/* Set Bandwidth */
	ret = bmg160_set_bw(data, BMG160_DEF_BW);
	if (ret < 0)
		return ret;

	/* Set Default Range */
	ret = i2c_smbus_write_byte_data(data->client,
					BMG160_REG_RANGE,
					BMG160_RANGE_500DPS);
	if (ret < 0) {
		dev_err(&data->client->dev,
					"Error writing pmu_dps_range\n");
		return ret;
	}
	data->dps_range = BMG160_RANGE_500DPS;

	ret = i2c_smbus_read_byte_data(data->client, BMG160_REG_SLOPE_THRES);
	if (ret < 0) {
		dev_err(&data->client->dev, "Error: reading slope_thres\n");
		return ret;
	}
	data->slope_thres = ret;

	return bmg160_set_mode(data, BMG160_MODE_SUSPEND);
}

static int bmg160_set_power_state(struct bmg160_data *data, bool on)
{
	int ret;

#ifdef CONFIG_PM_RUNTIME
	if (on)
		ret = pm_runtime_get_sync(&data->client->dev);
	else {
		pm_runtime_put_noidle(&data->client->dev);
		ret = pm_schedule_suspend(&data->client->dev,
					  bmg160_power_off_delay_ms);
	}
#else
	if (on) {
		ret = bmg160_set_mode(data, BMG160_MODE_NORMAL);
		if (!ret)
			msleep_interruptible(BMG160_MAX_STARTUP_TIME_MS);
	} else
		ret = bmg160_set_mode(data, BMG160_MODE_SUSPEND);
#endif

	if (ret < 0) {
		dev_err(&data->client->dev,
			"Failed: bmg160_set_power_state for %d\n", on);
		return ret;
	}

	return 0;
}

static int bmg160_chip_setup_interrupt(struct bmg160_data *data, bool status)
{
	int ret;

	/* Enable/Disable INT1 mapping */
	ret = i2c_smbus_read_byte_data(data->client,
				       BMG160_REG_INT_MAP_0);
	if (ret < 0) {
		dev_err(&data->client->dev, "Error read reg_int_map0\n");
		return ret;
	}
	if (status)
		ret |= BMG160_INT_MAP_0_BIT_ANY;
	else
		ret &= ~BMG160_INT_MAP_0_BIT_ANY;

	ret = i2c_smbus_write_byte_data(data->client,
					BMG160_REG_INT_MAP_0,
					ret);
	if (ret < 0) {
		dev_err(&data->client->dev, "Error write reg_int_map0\n");
		return ret;
	}

	/* Set latched mode interrupt and clear any latched interrupt */
	ret = i2c_smbus_write_byte_data(data->client,
					BMG160_REG_INT_RST_LATCH,
					BMG160_INT_MODE_LATCH_INT |
					BMG160_INT_MODE_LATCH_RESET);
	if (ret < 0) {
		dev_err(&data->client->dev, "Error write reg_rst_latch\n");
		return ret;
	}

	/* Enable/Disable slope interrupts */
	if (status) {

		/* Update slope thres */
		ret = i2c_smbus_write_byte_data(data->client,
						BMG160_REG_SLOPE_THRES,
						data->slope_thres);
		if (ret < 0) {
			dev_err(&data->client->dev,
				"Error write reg_slope_thres\n");
			return ret;
		}

		ret = i2c_smbus_write_byte_data(data->client,
						BMG160_REG_MOTION_INTR,
						BMG160_INT_MOTION_X |
						BMG160_INT_MOTION_Y |
						BMG160_INT_MOTION_Z);
		if (ret < 0) {
			dev_err(&data->client->dev,
				"Error write reg_motion_intr\n");
			return ret;
		}

		ret = i2c_smbus_write_byte_data(data->client,
						BMG160_REG_INT_EN_0,
						BMG160_DATA_ENABLE_INT);
	} else
		ret = i2c_smbus_write_byte_data(data->client,
						BMG160_REG_INT_EN_0,
						0);

	if (ret < 0) {
		dev_err(&data->client->dev, "Error writing reg_int_en0\n");
		return ret;
	}

	return ret;
}

static int bmg160_get_bw(struct bmg160_data *data, int *val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bmg160_samp_freq_table); ++i) {
		if (bmg160_samp_freq_table[i].bw_bits == data->bw_bits) {
			*val = bmg160_samp_freq_table[i].val;
			return IIO_VAL_INT;
		}
	}

	return -EINVAL;
}

static int bmg160_set_scale(struct bmg160_data *data, int val)
{
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(bmg160_scale_table); ++i) {
		if (bmg160_scale_table[i].scale == val) {
			ret = i2c_smbus_write_byte_data(
					data->client,
					BMG160_REG_RANGE,
					bmg160_scale_table[i].dps_range);
			if (ret < 0) {
				dev_err(&data->client->dev,
					"Error writing pmu_dps_range\n");
				return ret;
			}
			data->dps_range = bmg160_scale_table[i].dps_range;
			return 0;
		}
	}

	return -EINVAL;
}

static int bmg160_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct bmg160_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&data->mutex);
		if (iio_buffer_enabled(indio_dev))
			ret = -EBUSY;
		else {
			ret = bmg160_set_power_state(data, true);
			if (ret < 0) {
				mutex_unlock(&data->mutex);
				return ret;
			}
			ret = i2c_smbus_read_word_data(data->client,
						       BMG160_AXIS_TO_REG(
						       chan->scan_index));
			if (ret < 0) {
				bmg160_set_power_state(data, false);
				mutex_unlock(&data->mutex);
				return ret;
			}
			*val = sign_extend32(ret, 15);
			ret = bmg160_set_power_state(data, false);
		}
		mutex_unlock(&data->mutex);

		if (ret < 0)
			return ret;

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		switch (data->dps_range) {
		case BMG160_RANGE_2000DPS:
			*val2 = 1065;
			break;
		case BMG160_RANGE_1000DPS:
			*val2 = 532;
			break;
		case BMG160_RANGE_500DPS:
			*val2 = 266;
			break;
		case BMG160_RANGE_250DPS:
			*val2 = 133;
			break;
		case BMG160_RANGE_125DPS:
			*val2 = 66;
			break;
		default:
			return -EINVAL;
		}
		return IIO_VAL_INT_PLUS_MICRO;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val2 = 0;
		mutex_lock(&data->mutex);
		ret = bmg160_get_bw(data, val);
		if (ret == IIO_VAL_INT)
			*val /= BMG160_ANY_DUR_MIN_SAMPLE;
		mutex_unlock(&data->mutex);
		return ret;
	default:
		return -EINVAL;
	}
}

static int bmg160_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
	struct bmg160_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&data->mutex);
		ret = bmg160_set_bw(data, val * BMG160_ANY_DUR_MIN_SAMPLE);
		mutex_unlock(&data->mutex);
		break;
	case IIO_CHAN_INFO_SCALE:
		if (val)
			return -EINVAL;
		mutex_lock(&data->mutex);
		ret = bmg160_set_scale(data, val2);
		mutex_unlock(&data->mutex);
		return ret;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int bmg160_read_event(struct iio_dev *indio_dev,
				   const struct iio_chan_spec *chan,
				   enum iio_event_type type,
				   enum iio_event_direction dir,
				   enum iio_event_info info,
				   int *val, int *val2)
{
	struct bmg160_data *data = iio_priv(indio_dev);

	*val2 = 0;
	switch (info) {
	case IIO_EV_INFO_VALUE:
		*val = data->slope_thres & BMG160_SLOPE_THRES_MASK;
		break;
	default:
		return -EINVAL;
	}

	return IIO_VAL_INT;
}

static int bmg160_write_event(struct iio_dev *indio_dev,
				    const struct iio_chan_spec *chan,
				    enum iio_event_type type,
				    enum iio_event_direction dir,
				    enum iio_event_info info,
				    int val, int val2)
{
	struct bmg160_data *data = iio_priv(indio_dev);

	switch (info) {
	case IIO_EV_INFO_VALUE:
		data->slope_thres &= ~BMG160_SLOPE_THRES_MASK;
		data->slope_thres |= (val & BMG160_SLOPE_THRES_MASK);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bmg160_read_event_config(struct iio_dev *indio_dev,
				       const struct iio_chan_spec *chan,
				       enum iio_event_type type,
				       enum iio_event_direction dir)
{

	struct bmg160_data *data = iio_priv(indio_dev);

	return data->ev_enable_state;
}

static int bmg160_write_event_config(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					enum iio_event_type type,
					enum iio_event_direction dir,
					int state)
{
	struct bmg160_data *data = iio_priv(indio_dev);
	int ret;

	if (data->trigger_on)
		return -EAGAIN;

	if (state && data->ev_enable_state)
		return 0;

	mutex_lock(&data->mutex);
	ret = bmg160_chip_setup_interrupt(data, state);
	if (!ret) {
		ret = bmg160_set_power_state(data, state);
		if (ret < 0) {
			mutex_unlock(&data->mutex);
			return ret;
		}
	}
	data->ev_enable_state = state;
	mutex_unlock(&data->mutex);

	return 0;
}

static int bmc150_validate_trigger(struct iio_dev *indio_dev,
				   struct iio_trigger *trig)
{
	struct bmg160_data *data = iio_priv(indio_dev);

	if (data->trig != trig)
		return -EINVAL;

	return 0;
}

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
		"25.0 50.0 100.0 250.0 500.0");

static IIO_CONST_ATTR(in_anglvel_scale_available,
		      "0.001065 0.000532 0.000133 0.000066");

static struct attribute *bmg160_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_scale_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group bmg160_attrs_group = {
	.attrs = bmg160_attributes,
};

static const struct iio_event_spec bmg160_event = {
		.type = IIO_EV_TYPE_ROC,
		.dir = IIO_EV_DIR_EITHER,
		.mask_shared_by_type = BIT(IIO_EV_INFO_VALUE) |
					   BIT(IIO_EV_INFO_ENABLE)
};

#define BMG160_CHANNEL(_axis) {					\
	.type = IIO_ANGL_VEL,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_##_axis,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |		\
				BIT(IIO_CHAN_INFO_SAMP_FREQ),		\
	.scan_index = AXIS_##_axis,					\
	.scan_type = {							\
		.sign = 's',						\
		.realbits = 16,						\
		.storagebits = 16,					\
		.shift = 0,						\
	},								\
	.event_spec = &bmg160_event,					\
	.num_event_specs = 1						\
}

static const struct iio_chan_spec bmg160_channels[] = {
	BMG160_CHANNEL(X),
	BMG160_CHANNEL(Y),
	BMG160_CHANNEL(Z),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_info bmg160_info = {
	.attrs			= &bmg160_attrs_group,
	.read_raw		= bmg160_read_raw,
	.write_raw		= bmg160_write_raw,
	.read_event_value	= bmg160_read_event,
	.write_event_value	= bmg160_write_event,
	.write_event_config	= bmg160_write_event_config,
	.read_event_config	= bmg160_read_event_config,
	.validate_trigger	= bmc150_validate_trigger,
	.driver_module		= THIS_MODULE,
};

static irqreturn_t bmg160_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct bmg160_data *data = iio_priv(indio_dev);
	int bit, ret, i = 0;

	mutex_lock(&data->mutex);
	for_each_set_bit(bit, indio_dev->buffer->scan_mask,
			 indio_dev->masklength) {
		ret = i2c_smbus_read_word_data(data->client,
						BMG160_AXIS_TO_REG(bit));
		if (ret < 0) {
			mutex_unlock(&data->mutex);
			goto err;
		}
		data->buffer[i++] = ret;
	}
	mutex_unlock(&data->mutex);

	iio_push_to_buffers_with_timestamp(indio_dev, data->buffer,
					   pf->timestamp);
err:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int bmg160_trig_try_reen(struct iio_trigger *trig)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct bmg160_data *data = iio_priv(indio_dev);
	int ret;

	/* Set latched mode interrupt and clear any latched interrupt */
	ret = i2c_smbus_write_byte_data(data->client,
					BMG160_REG_INT_RST_LATCH,
					BMG160_INT_MODE_LATCH_INT |
					BMG160_INT_MODE_LATCH_RESET);
	if (ret < 0) {
		dev_err(&data->client->dev, "Error writing reg_rst_latch\n");
		return ret;
	}

	return 0;
}

static int bmg160_data_rdy_trigger_set_state(struct iio_trigger *trig,
					     bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct bmg160_data *data = iio_priv(indio_dev);
	int ret;

	if (data->ev_enable_state)
		return -EAGAIN;

	if (state && data->trigger_on)
		return 0;

	mutex_lock(&data->mutex);
	ret = bmg160_chip_setup_interrupt(data, state);
	if (!ret) {
		ret = bmg160_set_power_state(data, state);
		if (ret < 0) {
			mutex_unlock(&data->mutex);
			return ret;
		}
	}
	data->trigger_on = state;
	mutex_unlock(&data->mutex);

	return 0;
}

static const struct iio_trigger_ops bmg160_trigger_ops = {
	.set_trigger_state = bmg160_data_rdy_trigger_set_state,
	.try_reenable = bmg160_trig_try_reen,
	.owner = THIS_MODULE,
};

static irqreturn_t bmg160_event_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct bmg160_data *data = iio_priv(indio_dev);
	int ret;

	iio_push_event(indio_dev, IIO_MOD_EVENT_CODE(IIO_ANGL_VEL,
						     0,
						     IIO_MOD_X_OR_Y_OR_Z,
						     IIO_EV_TYPE_ROC,
						     IIO_EV_DIR_EITHER),
						     iio_get_time_ns());

	ret = i2c_smbus_write_byte_data(data->client,
					BMG160_REG_INT_RST_LATCH,
					BMG160_INT_MODE_LATCH_INT |
					BMG160_INT_MODE_LATCH_RESET);
	if (ret < 0)
		dev_err(&data->client->dev, "Error writing reg_rst_latch\n");

	return IRQ_HANDLED;
}

static irqreturn_t bmg160_data_rdy_trig_poll(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct bmg160_data *data = iio_priv(indio_dev);

	if (data->trigger_on) {
		iio_trigger_poll(data->trig, 0);
		return IRQ_HANDLED;
	} else
		return IRQ_WAKE_THREAD;
}

static int bmg160_acpi_gpio_probe(struct i2c_client *client,
				  struct bmg160_data *data)
{
	const struct acpi_device_id *id;
	struct device *dev;
	struct gpio_desc *gpio;
	int ret;

	if (!client)
		return -EINVAL;

	dev = &client->dev;
	if (!ACPI_HANDLE(dev))
		return -ENODEV;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id)
		return -ENODEV;

	/* data ready gpio interrupt pin */
	gpio = devm_gpiod_get_index(dev, BMG160_GPIO_NAME, 0);
	if (IS_ERR(gpio)) {
		dev_err(dev, "acpi gpio get index failed\n");
		return PTR_ERR(gpio);
	}

	ret = gpiod_direction_input(gpio);
	if (ret)
		return ret;

	ret = gpiod_to_irq(gpio);

	dev_dbg(dev, "GPIO resource, no:%d irq:%d\n", desc_to_gpio(gpio), ret);

	return ret;
}

static int bmg160_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct bmg160_data *data;
	struct iio_dev *indio_dev;
	struct iio_trigger *trig = NULL;
	int ret;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
	data->client = client;

	ret = bmg160_chip_init(data);
	if (ret < 0)
		return ret;

	mutex_init(&data->mutex);

	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = bmg160_channels;
	indio_dev->num_channels = ARRAY_SIZE(bmg160_channels);
	indio_dev->name = BMG160_DRV_NAME;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &bmg160_info;

	if (client->irq <= 0)
		client->irq = bmg160_acpi_gpio_probe(client, data);

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(
					&client->dev, client->irq,
					bmg160_data_rdy_trig_poll,
					bmg160_event_handler,
					IRQF_TRIGGER_RISING,
					BMG160_IRQ_NAME,
					indio_dev);
		if (ret)
			return ret;

		trig = iio_trigger_alloc("%s-dev%d", indio_dev->name,
					 indio_dev->id);
		if (!trig)
			return -ENOMEM;

		trig->dev.parent = &client->dev;
		trig->ops = &bmg160_trigger_ops;
		iio_trigger_set_drvdata(trig, indio_dev);
		data->trig = trig;

		ret = iio_trigger_register(trig);
		if (ret)
			goto err_trigger_free;

		ret = iio_triggered_buffer_setup(indio_dev,
						 &iio_pollfunc_store_time,
						 bmg160_trigger_handler,
						 NULL);
		if (ret < 0) {
			dev_err(&client->dev,
				"iio triggered buffer setup failed\n");
			goto err_trigger_unregister;
		}
	}

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&client->dev, "unable to register iio device\n");
		goto err_buffer_cleanup;
	}
	ret = pm_runtime_set_active(&client->dev);
	if (ret)
		goto err_iio_unregister;

	pm_runtime_enable(&client->dev);

	return 0;

err_iio_unregister:
	iio_device_unregister(indio_dev);
err_buffer_cleanup:
	if (data->trig)
		iio_triggered_buffer_cleanup(indio_dev);
err_trigger_unregister:
	if (data->trig)
		iio_trigger_unregister(trig);
err_trigger_free:
	if (data->trig)
		iio_trigger_free(trig);

	return ret;
}

static int bmg160_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct bmg160_data *data = iio_priv(indio_dev);

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	pm_runtime_put_noidle(&client->dev);

	iio_device_unregister(indio_dev);

	if (data->trig) {
		iio_triggered_buffer_cleanup(indio_dev);
		iio_trigger_unregister(data->trig);
		iio_trigger_free(data->trig);
	}

	mutex_lock(&data->mutex);
	bmg160_set_mode(data, BMG160_MODE_DEEP_SUSPEND);
	mutex_unlock(&data->mutex);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bmg160_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct bmg160_data *data = iio_priv(indio_dev);

	mutex_lock(&data->mutex);
	bmg160_set_mode(data, BMG160_MODE_SUSPEND);
	mutex_unlock(&data->mutex);

	return 0;
}

static int bmg160_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct bmg160_data *data = iio_priv(indio_dev);

	mutex_lock(&data->mutex);
	if (data->trigger_on || data->ev_enable_state)
		bmg160_set_mode(data, BMG160_MODE_NORMAL);
	mutex_unlock(&data->mutex);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int bmg160_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct bmg160_data *data = iio_priv(indio_dev);

	return bmg160_set_mode(data, BMG160_MODE_SUSPEND);
}

static int bmg160_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct bmg160_data *data = iio_priv(indio_dev);
	int ret;

	ret = bmg160_set_mode(data, BMG160_MODE_NORMAL);
	if (ret < 0)
		return ret;

	msleep_interruptible(BMG160_MAX_STARTUP_TIME_MS);

	return 0;
}
#endif

static const struct dev_pm_ops bmg160_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bmg160_suspend, bmg160_resume)
	SET_RUNTIME_PM_OPS(bmg160_runtime_suspend,
			   bmg160_runtime_resume, NULL)
};

static const struct acpi_device_id bmg160_acpi_match[] = {
	{"BMG0160", 0},
	{"BMBG0160", 0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, bmg160_acpi_match);

static const struct i2c_device_id bmg160_id[] = {
	{"bmg160", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmg160_id);

static struct i2c_driver bmg160_driver = {
	.driver = {
		.name	= BMG160_DRV_NAME,
		.acpi_match_table = ACPI_PTR(bmg160_acpi_match),
		.pm	= &bmg160_pm_ops,
	},
	.probe		= bmg160_probe,
	.remove		= bmg160_remove,
	.id_table	= bmg160_id,
};
module_i2c_driver(bmg160_driver);

MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BMG160 Gyro driver");
