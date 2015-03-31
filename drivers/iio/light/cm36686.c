/* drivers/iio/light/cm36686.c
 *
 * Copyright (C) 2014 XiaoMi, Inc.
 * Copyright (C) 2015 XiaoMi, Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>

#define CM36686_I2C_NAME		"cm36686"
#define CM_PROX_IIO_NAME		"distance"
#define CM_LIGHT_IIO_NAME		"als"

#define I2C_RETRY_COUNT			10

#define PRX_SENSOR			0
#define ALS_SENSOR			1

/*Define Command Code*/
#define	ALS_CONF			0x00
#define	ALS_THDH			0x01
#define	ALS_THDL			0x02
#define	PS_CONF1			0x03
#define	PS_CONF3			0x04
#define	PS_CANC				0x05
#define	PS_THDL				0x06
#define	PS_THDH				0x07

#define	PS_DATA				0x08
#define	ALS_DATA			0x09
#define	ALS_DATA_RESERVE	        0x0A
#define	INT_FLAG			0x0B
#define	ID_REG				0x0C

/*cm36686*/
/*for ALS CONF command*/
#define CM36686_ALS_IT_80MS		(0 << 6)
#define CM36686_ALS_IT_160MS		(1 << 6)
#define CM36686_ALS_IT_320MS		(2 << 6)
#define CM36686_ALS_IT_640MS		(3 << 6)
#define CM36686_ALS_PERS_1		(0 << 2)
#define CM36686_ALS_PERS_2		(1 << 2)
#define CM36686_ALS_PERS_4		(2 << 2)
#define CM36686_ALS_PERS_8		(3 << 2)
#define CM36686_ALS_INT_EN		(1 << 1) /*enable/disable Interrupt*/
#define CM36686_ALS_INT_MASK		0xFFFD
#define CM36686_ALS_SD			(1 << 0) /*enable/disable ALS func, 1:disable , 0: enable*/
#define CM36686_ALS_SD_MASK		0xFFFE

#define CM36686_ALS_CONF_DEFAULT	(CM36686_ALS_SD | CM36686_ALS_IT_160MS)


/*for PS CONF2 command*/
#define CM36686_PS_12BITS		(0 << 11)
#define CM36686_PS_16BITS		(1 << 11)
#define CM36686_PS_INT_OFF		(0 << 8) /*enable/disable Interrupt*/
#define CM36686_PS_INT_IN		(1 << 8)
#define CM36686_PS_INT_OUT		(2 << 8)
#define CM36686_PS_INT_IN_AND_OUT	(3 << 8)
#define CM36686_PS_INT_MASK		0xFCFF

/*for PS CONF1 command*/
#define CM36686_PS_DR_1_40		(0 << 6)
#define CM36686_PS_DR_1_80		(1 << 6)
#define CM36686_PS_DR_1_160		(2 << 6)
#define CM36686_PS_DR_1_320		(3 << 6)
#define CM36686_PS_PERS_1		(0 << 4)
#define CM36686_PS_PERS_2		(1 << 4)
#define CM36686_PS_PERS_3		(2 << 4)
#define CM36686_PS_PERS_4		(3 << 4)
#define CM36686_PS_IT_1T		(0 << 1)
#define CM36686_PS_IT_1_5T		(1 << 2)
#define CM36686_PS_IT_2T		(2 << 2)
#define CM36686_PS_IT_2_5T		(3 << 1)
#define CM36686_PS_IT_3T		(4 << 1)
#define CM36686_PS_IT_3_5T		(5 << 1)
#define CM36686_PS_IT_4T		(6 << 1)
#define CM36686_PS_IT_8T		(7 << 1)
#define CM36686_PS_SD			(1 << 0) /*enable/disable PS func, 1:disable , 0: enable*/
#define CM36686_PS_SD_MASK		0xFFFE

#define CM36686_PS_CONF1_CONF2_DEFAULT	(CM36686_PS_IT_2_5T | CM36686_PS_DR_1_80 | \
					 CM36686_PS_SD | CM36686_PS_12BITS | CM36686_PS_INT_OFF)



/*for PS CONF3 command*/
#define CM36686_PS_MS_NORMAL		(0 << 14)
#define CM36686_PS_MS_LOGIC_ENABLE	(1 << 14)
#define CM36686_LED_I_50		(0 << 8)
#define CM36686_LED_I_75		(1 << 8)
#define CM36686_LED_I_100		(2 << 8)
#define CM36686_LED_I_120		(3 << 8)
#define CM36686_LED_I_140		(4 << 8)
#define CM36686_LED_I_160		(5 << 8)
#define CM36686_LED_I_180		(6 << 8)
#define CM36686_LED_I_200		(7 << 8)
#define CM36686_PS_SMART_PERS_ENABLE	(1 << 4)
#define CM36686_PS_ACTIVE_FORCE_MODE	(1 << 3)
#define CM36686_PS_ACTIVE_FORCE_TRIG	(1 << 2)
/* CMD4 */
#define CM36686_PS_CONF3_MS_DEFAULT	(CM36686_LED_I_160)


/*for INT FLAG*/
#define INT_FLAG_PS_SPFLAG		(1<<14)
#define INT_FLAG_ALS_IF_L		(1<<13)
#define INT_FLAG_ALS_IF_H		(1<<12)
#define INT_FLAG_PS_IF_CLOSE		(1<<9)
#define INT_FLAG_PS_IF_AWAY		(1<<8)

#define CM36686_CHIP_ID			0x0086

#define CM36686_PROX_DELAY		100
#define CM36686_ALS_DELAY		170


struct cm36686_data {
	/* common state */
	struct mutex		mutex;
	struct i2c_client	*client;

	bool			suspended;
	bool			dump_output;
	bool			dump_register;

	/* for proximity sensor */
	struct iio_dev		*prox_idev;
	bool			prox_enabled;
	bool			prox_continus;
	bool			prox_first_data;
	bool			prox_near;
	uint16_t		prox_offset;
	uint16_t		prox_raw;
	uint32_t		prox_thres_near;
	uint32_t		prox_thres_far;
	struct delayed_work	prox_delayed_work;

	/* for light sensor */
	struct iio_dev		*als_idev;
	bool			als_enabled;
	uint16_t		als_light;
	int			als_raw;
	struct delayed_work	als_delayed_work;
};

struct cm36686_platform_data {
	uint32_t		als_trans_ratio;

	/* default proximity sample period */
	uint32_t		prox_default_offset;
	uint32_t		prox_default_thres_near;
	uint32_t		prox_default_thres_far;
};

struct cm36686_el_data {
	uint16_t data1;
	uint16_t data2;
	int64_t timestamp;
};

#define CM_SENSORS_CHANNELS(device_type, mask, index, mod, \
					ch2, s, endian, rbits, sbits, addr) \
{ \
	.type = device_type, \
	.modified = mod, \
	.info_mask_separate = mask, \
	.scan_index = index, \
	.channel2 = ch2, \
	.address = addr, \
	.scan_type = { \
		.sign = s, \
		.realbits = rbits, \
		.shift = sbits - rbits, \
		.storagebits = sbits, \
		.endianness = endian, \
	}, \
}



static const struct iio_chan_spec cm_proximity_channels[] = {
	CM_SENSORS_CHANNELS(IIO_PROXIMITY,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_OFFSET) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		0, 0, IIO_NO_MOD, 'u', IIO_LE, 16, 16, 0),
	CM_SENSORS_CHANNELS(IIO_CUSTOM, 0,
		1, 0, IIO_NO_MOD, 'u', IIO_LE, 16, 16, 0),
	IIO_CHAN_SOFT_TIMESTAMP(2)
};

static const struct iio_chan_spec cm_light_channels[] = {
	CM_SENSORS_CHANNELS(IIO_LIGHT,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		0, 0, IIO_NO_MOD, 'u', IIO_LE, 16, 16, 0),
	IIO_CHAN_SOFT_TIMESTAMP(1)
};

static int i2c_rxdata(struct i2c_client *client, uint8_t cmd, uint8_t *rxData, int length)
{
	uint8_t loop_i;

	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		if (i2c_transfer(client->adapter, msgs, 2) > 0)
			break;
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		dev_err(&client->dev, "[PS_ERR][CM36686 error] %s retry over %d\n",
			__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int i2c_txdata(struct i2c_client *client, uint8_t *txData, int length)
{
	uint8_t loop_i;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		if (i2c_transfer(client->adapter, msg, 1) > 0)
			break;
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		dev_err(&client->dev, "[ALS+PS_ERR][CM36686 error] %s retry over %d\n",
			__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int cm_i2c_read_word(struct i2c_client *client, uint8_t cmd, uint16_t *pdata)
{
	uint8_t buffer[2];
	int ret = 0;

	if (pdata == NULL)
		return -EFAULT;

	ret = i2c_rxdata(client, cmd, buffer, 2);
	if (ret < 0) {
		dev_err(&client->dev, "[ALS+PS_ERR][CM36686 error]%s: I2C_RxData fail [0x%x]\n",
			__func__, cmd);
		return ret;
	}

	*pdata = (buffer[1]<<8)|buffer[0];

	return ret;
}

static int cm_i2c_write_word(struct i2c_client *client, uint8_t cmd, uint16_t data)
{
	char buffer[3];
	int ret = 0;

	buffer[0] = cmd;
	buffer[1] = (uint8_t)(data&0xff);
	buffer[2] = (uint8_t)((data&0xff00)>>8);

	ret = i2c_txdata(client, buffer, 3);
	if (ret < 0) {
		dev_err(&client->dev, "[ALS+PS_ERR][CM36686 error]%s: I2C_TxData fail [0x%x]\n",
			__func__, cmd);
		return -EIO;
	}

	return ret;
}

#define DUMP_REGISTER(client, cmd, data) \
		cm_i2c_read_word(client, cmd, &data); \
		dev_info(&client->dev, "%s\t: %04x", #cmd, data)

static void dump_register(struct i2c_client *client)
{
	uint16_t cmd_data;

	DUMP_REGISTER(client, ALS_CONF, cmd_data);
	DUMP_REGISTER(client, PS_CONF1, cmd_data);
	DUMP_REGISTER(client, PS_CONF3, cmd_data);
	DUMP_REGISTER(client, PS_DATA, cmd_data);
	DUMP_REGISTER(client, ALS_DATA, cmd_data);
	DUMP_REGISTER(client, ALS_DATA_RESERVE, cmd_data);
}

static int update_device(struct cm36686_data *data)
{
	struct i2c_client *client = data->client;
	int ret = 0;
	uint16_t cmd_data;

	if (data->prox_enabled && !data->suspended)
		cmd_data = CM36686_PS_CONF1_CONF2_DEFAULT & (~CM36686_PS_SD);
	else
		cmd_data = CM36686_PS_CONF1_CONF2_DEFAULT;

	ret = cm_i2c_write_word(client, PS_CONF1, cmd_data);
	if (ret < 0)
		return ret;

	cmd_data = CM36686_PS_CONF3_MS_DEFAULT;
	ret = cm_i2c_write_word(client, PS_CONF3, cmd_data);
	if (ret < 0)
		return ret;

	if (data->als_enabled && !data->suspended)
		cmd_data = CM36686_ALS_CONF_DEFAULT & (~CM36686_ALS_SD);
	else
		cmd_data = CM36686_ALS_CONF_DEFAULT;

	ret = cm_i2c_write_word(client, ALS_CONF, cmd_data);
	if (ret < 0)
		return ret;

	return 0;
}

static int cm_proximity_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	int ret = 0;
	struct cm36686_data *data;

	data = *((struct cm36686_data **)iio_priv(indio_dev));

	mutex_lock(&data->mutex);
	switch (mask) {
	case IIO_CHAN_INFO_OFFSET:
		*val = data->prox_offset;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_RAW:
		if (data->prox_enabled)
			*val = data->prox_raw;
		else
			*val = 0;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = 10;
		ret = IIO_VAL_INT;
		break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&data->mutex);

	return ret;
}

static int cm_proximity_write_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	int ret = 0;
	struct cm36686_data *data;

	data = *((struct cm36686_data **)iio_priv(indio_dev));

	switch (mask) {
	case IIO_CHAN_INFO_OFFSET:
		mutex_lock(&data->mutex);
		if (val >= 0 && val < 4096) {
			data->prox_offset = val;
		} else {
			dev_err(&indio_dev->dev, "%s: invalid offset:%d\n", __func__, val);
			ret = -EINVAL;
		}
		mutex_unlock(&data->mutex);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int cm_proximity_write_get_fmt(struct iio_dev *indio_dev,
				      struct iio_chan_spec const *chan,
				      long mask)
{
	return IIO_VAL_INT_PLUS_MICRO;
}

static int cm_light_get_scale(struct cm36686_data *data,
				int *val, int *val2)
{
	int scale;
	struct cm36686_platform_data *pdata;

	pdata = data->client->dev.platform_data;
	/* als_trans_ratio * 0.08 * 1000000 */
	scale = pdata->als_trans_ratio * 40000;
	*val = scale / 1000000;
	*val2 = scale % 1000000;

	return 0;
}

static int cm_light_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	int ret = 0;
	struct cm36686_data *data;

	data = *((struct cm36686_data **)iio_priv(indio_dev));

	mutex_lock(&data->mutex);
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		cm_light_get_scale(data, val, val2);
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	case IIO_CHAN_INFO_RAW:
		if (data->als_enabled)
			*val = data->als_raw;
		else
			*val = 0;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = 10;
		ret = IIO_VAL_INT;
		break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&data->mutex);

	return ret;
}

static int cm_light_write_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	int ret = 0;
	struct cm36686_data *data;

	data = *((struct cm36686_data **)iio_priv(indio_dev));

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int cm_light_write_get_fmt(struct iio_dev *indio_dev,
				      struct iio_chan_spec const *chan,
				      long mask)
{
	return IIO_VAL_INT_PLUS_MICRO;
}

static ssize_t cm_show_dump_output(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	mutex_lock(&data->mutex);
	value = data->dump_output;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", value);
}

static ssize_t cm_store_dump_output(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data;
	int ret = 0;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	ret = strict_strtoul(buf, 0, &value);
	if (ret >= 0) {
		mutex_lock(&data->mutex);
		data->dump_output = value;
		mutex_unlock(&data->mutex);
	}

	return ret < 0 ? ret : size;
}

static ssize_t cm_show_dump_register(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	mutex_lock(&data->mutex);
	value = data->dump_register;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", value);
}

static ssize_t cm_store_dump_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data;
	int ret = 0;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	ret = strict_strtoul(buf, 0, &value);
	if (ret >= 0) {
		mutex_lock(&data->mutex);
		data->dump_register = value;
		mutex_unlock(&data->mutex);
	}

	return ret < 0 ? ret : size;
}

static ssize_t cm_show_continus_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	mutex_lock(&data->mutex);
	value = data->prox_continus;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", value);
}

static ssize_t cm_store_continus_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data;
	int ret = 0;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	ret = strict_strtoul(buf, 0, &value);
	if (ret >= 0) {
		mutex_lock(&data->mutex);
		data->prox_continus = value;
		mutex_unlock(&data->mutex);
	}

	return ret < 0 ? ret : size;
}

static ssize_t cm_show_prox_thres_far(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	mutex_lock(&data->mutex);
	value = data->prox_thres_far + data->prox_offset;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", value);
}

static ssize_t cm_store_prox_thres_far(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data;
	int ret = 0;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));
	if (value <= data->prox_offset)
		return (data->prox_offset - value);

	ret = strict_strtoul(buf, 0, &value);
	if (ret >= 0) {
		mutex_lock(&data->mutex);
		data->prox_thres_far = value - data->prox_offset;
		mutex_unlock(&data->mutex);
	}

	return ret < 0 ? ret : size;
}
static ssize_t cm_show_prox_thres_near(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cm36686_data *data;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));

	mutex_lock(&data->mutex);
	value = data->prox_thres_near + data->prox_offset;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", value);
}

static ssize_t cm_store_prox_thres_near(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36686_data *data;
	int ret = 0;
	unsigned long value;

	data = *((struct cm36686_data **)iio_priv(dev_get_drvdata(dev)));
	if (value <= data->prox_offset)
		return (data->prox_offset - value);

	ret = strict_strtoul(buf, 0, &value);
	if (ret >= 0) {
		mutex_lock(&data->mutex);
		data->prox_thres_near = value - data->prox_offset;
		mutex_unlock(&data->mutex);
	}

	return ret < 0 ? ret : size;
}
static DEVICE_ATTR(dump_output, S_IWUSR | S_IRUGO,
		cm_show_dump_output, cm_store_dump_output);
static DEVICE_ATTR(dump_register, S_IWUSR | S_IRUGO,
		cm_show_dump_register, cm_store_dump_register);
static DEVICE_ATTR(continus_mode, S_IWUSR | S_IRUGO,
		cm_show_continus_mode, cm_store_continus_mode);
static DEVICE_ATTR(prox_thres_far, S_IWUSR | S_IRUGO,
		cm_show_prox_thres_far, cm_store_prox_thres_far);
static DEVICE_ATTR(prox_thres_near, S_IWUSR | S_IRUGO,
		cm_show_prox_thres_near, cm_store_prox_thres_near);

static struct attribute *cm_prox_attributes[] = {
	&dev_attr_dump_output.attr,
	&dev_attr_dump_register.attr,
	&dev_attr_continus_mode.attr,
	&dev_attr_prox_thres_far.attr,
	&dev_attr_prox_thres_near.attr,
	NULL,
};

static struct attribute_group cm_prox_attribute_group = {
	.attrs = cm_prox_attributes,
};

static struct attribute *cm_als_attributes[] = {
	&dev_attr_dump_output.attr,
	&dev_attr_dump_register.attr,
	NULL,
};

static struct attribute_group cm_als_attribute_group = {
	.attrs = cm_als_attributes,
};

static const struct iio_info cm_proximity_info = {
	.driver_module = THIS_MODULE,
	.attrs = &cm_prox_attribute_group,
	.read_raw = cm_proximity_read_raw,
	.write_raw = cm_proximity_write_raw,
	.write_raw_get_fmt = cm_proximity_write_get_fmt,
};

static const struct iio_info cm_light_info = {
	.driver_module = THIS_MODULE,
	.attrs = &cm_als_attribute_group,
	.read_raw = cm_light_read_raw,
	.write_raw = cm_light_write_raw,
	.write_raw_get_fmt = cm_light_write_get_fmt,
};

static int cm_sensor_enable_disable(struct iio_dev *indio_dev, bool en)
{
	struct cm36686_data *data;
	int ret = 0;
	int sensor;

	data = *((struct cm36686_data **)iio_priv(indio_dev));

	if (!strncmp(indio_dev->name, CM_PROX_IIO_NAME, strlen(CM_PROX_IIO_NAME)))
		sensor = PRX_SENSOR;
	else
		sensor = ALS_SENSOR;

	mutex_lock(&data->mutex);

	if ((sensor == PRX_SENSOR) && (data->prox_enabled != en)) {
		if (en) {
			/* we should push an initial state onece opened, defalut FarAway */
			data->prox_near = false;
			data->prox_first_data = true;
		}

		data->prox_enabled = en;
	} else if ((sensor == ALS_SENSOR) && (data->als_enabled != en)) {
		data->als_enabled = en;
	} else {
		dev_err(&indio_dev->dev, "%s: unhandled case\n", __func__);
		ret = -EINVAL;
		goto exit_err_state;
	}

	ret = update_device(data);

	if (sensor == PRX_SENSOR) {
		if (en)
			schedule_delayed_work(&data->prox_delayed_work, CM36686_PROX_DELAY * HZ / 1000);
		else
			cancel_delayed_work(&data->prox_delayed_work);
	}
	else if (sensor == ALS_SENSOR) {
		if (en)
			schedule_delayed_work(&data->als_delayed_work, CM36686_ALS_DELAY * HZ / 1000);
		else
			cancel_delayed_work(&data->als_delayed_work);
	}


exit_err_state:
	mutex_unlock(&data->mutex);

	return ret;
}

static int cm_buffer_postenable(struct iio_dev *indio_dev)
{
	return cm_sensor_enable_disable(indio_dev, true);
}

static int cm_buffer_predisable(struct iio_dev *indio_dev)
{
	return cm_sensor_enable_disable(indio_dev, false);
}

static const struct iio_buffer_setup_ops cm_buffer_setup_ops = {
	.preenable = iio_sw_buffer_preenable,
	.postenable = cm_buffer_postenable,
	.predisable = cm_buffer_predisable,
};

static bool cm_is_near_far_changed(struct cm36686_data *data, uint16_t raw)
{
	int16_t adjust_raw = raw - data->prox_offset;

	raw = adjust_raw > 0 ? adjust_raw : 0;

	if (data->prox_near) {
		if (raw < data->prox_thres_far) {
			data->prox_near = false;
			return true;
		}
	} else {
		if (raw > data->prox_thres_near) {
			data->prox_near = true;
			return true;
		}
	}

	return false;
}

static void cm_prox_work(struct work_struct *work)
{
	struct cm36686_data *data = container_of(to_delayed_work(work),
				struct cm36686_data, prox_delayed_work);
	struct i2c_client *client = data->client;
	struct cm36686_el_data el_data;
	int ret = 0;
	uint16_t raw;
	bool changed;

	el_data.timestamp = iio_get_time_ns();

	mutex_lock(&data->mutex);
	ret = cm_i2c_read_word(client, PS_DATA, &raw);
	if (ret < 0) {
		goto exit_to_prx_schedule;
	}

	changed = cm_is_near_far_changed(data, raw);
	el_data.data1 = data->prox_near ? 0 : 5;
	el_data.data2 = raw;
	data->prox_raw = raw;

	if (changed || data->prox_continus || data->prox_first_data) {
		data->prox_first_data = false;
		ret = iio_push_to_buffers(data->prox_idev, (unsigned char *)&el_data);
		if (ret < 0)
			dev_err(&client->dev, "failed to push proximity "
				"data to buffer, err=%d\n", ret);
	}

	if (data->dump_register)
		dump_register(client);
	if (data->dump_output)
		dev_info(&client->dev, "%s: distance=%d raw=%d, ts=%lld\n", __func__,
				el_data.data1, el_data.data2, el_data.timestamp);

exit_to_prx_schedule:
	schedule_delayed_work(&data->prox_delayed_work, CM36686_PROX_DELAY * HZ / 1000);
	mutex_unlock(&data->mutex);
}

static void cm_als_work(struct work_struct *work)
{
	struct cm36686_data *data = container_of(to_delayed_work(work),
				struct cm36686_data, als_delayed_work);
	struct i2c_client *client = data->client;
	struct cm36686_el_data el_data;
	int ret = 0;
	uint16_t raw;

	el_data.timestamp = iio_get_time_ns();

	mutex_lock(&data->mutex);

	ret = cm_i2c_read_word(client, ALS_DATA, &raw);
	if (ret < 0) {
		goto exit_to_als_schedule;
	}

	if (raw == 0) {
		ret = cm_i2c_read_word(client, ALS_DATA_RESERVE, &raw);
		if (ret < 0)
			goto exit_to_als_schedule;
	}
	el_data.data1 = raw;
	data->als_raw = raw;
	ret = iio_push_to_buffers(data->als_idev, (unsigned char *)&el_data);
	if (ret < 0)
		dev_err(&client->dev, "failed to push als data to "
			"buffer, err=%d\n", ret);

	if (data->dump_register)
		dump_register(client);
	if (data->dump_output)
		dev_info(&client->dev, "%s: als_raw=%d, ts=%lld\n", __func__,
				el_data.data1, el_data.timestamp);

exit_to_als_schedule:
	schedule_delayed_work(&data->als_delayed_work, CM36686_ALS_DELAY * HZ / 1000);
	mutex_unlock(&data->mutex);
}

static const struct iio_trigger_ops cm_sensor_trigger_ops = {
	.owner = THIS_MODULE,
};

int cm_setup_trigger_sensor(struct iio_dev *indio_dev)
{
	struct iio_trigger *trigger;
	int ret;

	trigger = iio_trigger_alloc("%s-dev%d", indio_dev->name, indio_dev->id);
	if (!trigger)
		return -ENOMEM;

	trigger->dev.parent = indio_dev->dev.parent;
	trigger->ops = &cm_sensor_trigger_ops;
	ret = iio_trigger_register(trigger);
	if (ret < 0)
		goto exit_free_trigger;

	indio_dev->trig = trigger;

	return 0;

exit_free_trigger:
	iio_trigger_free(trigger);
	return ret;
}


static int cm_proximity_iio_setup(struct cm36686_data *data)
{
	struct i2c_client *client = data->client;
	struct iio_dev *idev;
	struct cm36686_data **priv_data;
	int ret = 0;

	idev = iio_device_alloc(sizeof(*priv_data));
	if (!idev) {
		dev_err(&client->dev, "Proximity IIO memory fail\n");
		return -ENOMEM;
	}

	data->prox_idev = idev;

	idev->channels = cm_proximity_channels;
	idev->num_channels = ARRAY_SIZE(cm_proximity_channels);
	idev->dev.parent = &client->dev;
	idev->info = &cm_proximity_info;
	idev->name = CM_PROX_IIO_NAME;
	idev->modes = INDIO_DIRECT_MODE;

	priv_data = iio_priv(idev);
	*priv_data = data;

	ret = iio_triggered_buffer_setup(idev, NULL, NULL,
					&cm_buffer_setup_ops);
	if (ret < 0)
		goto free_iio_p;

	ret = cm_setup_trigger_sensor(idev);
	if (ret < 0)
		goto free_buffer_p;

	ret = iio_device_register(idev);
	if (ret) {
		dev_err(&client->dev, "Proximity IIO register fail\n");
		goto free_trigger_p;
	}
	return ret;

free_trigger_p:
	iio_trigger_unregister(idev->trig);
	iio_trigger_free(idev->trig);
free_buffer_p:
	iio_triggered_buffer_cleanup(idev);
free_iio_p:
	data->prox_idev = NULL;
	iio_device_free(idev);
	return ret;
}

static int cm_light_iio_setup(struct cm36686_data *data)
{
	struct i2c_client *client = data->client;
	struct iio_dev *idev;
	struct cm36686_data **priv_data;
	int ret = 0;

	idev = iio_device_alloc(sizeof(*priv_data));
	if (!idev) {
		dev_err(&client->dev, "ALS IIO memory fail\n");
		return -ENOMEM;
	}

	data->als_idev = idev;

	idev->channels = cm_light_channels;
	idev->num_channels = ARRAY_SIZE(cm_light_channels);
	idev->dev.parent = &client->dev;
	idev->info = &cm_light_info;
	idev->name = CM_LIGHT_IIO_NAME;
	idev->modes = INDIO_DIRECT_MODE;

	priv_data = iio_priv(idev);
	*priv_data = data;

	ret = iio_triggered_buffer_setup(idev, NULL, NULL,
					&cm_buffer_setup_ops);
	if (ret < 0)
		goto free_iio_a;

	ret = cm_setup_trigger_sensor(idev);
	if (ret < 0)
		goto free_buffer_a;

	ret = iio_device_register(idev);
	if (ret) {
		dev_err(&client->dev, "ALS IIO register fail\n");
		goto free_trigger_a;
	}
	return ret;

free_trigger_a:
	iio_trigger_unregister(idev->trig);
	iio_trigger_free(idev->trig);
free_buffer_a:
	iio_triggered_buffer_cleanup(idev);
free_iio_a:
	data->prox_idev = NULL;
	iio_device_free(idev);
	return ret;
}

static int cm_parse_dt(struct i2c_client *client)
{
	int ret = 0;
	struct device_node *np = client->dev.of_node;
	struct cm36686_platform_data *pdata;

	if (!np)
		return -ENOENT;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_property_read_u32(np, "als_trans_ratio", (u32 *)&pdata->als_trans_ratio);
	if (ret < 0)
		goto free_platform_data;

	ret = of_property_read_u32(np, "prox_default_offset", (u32 *)&pdata->prox_default_offset);
	if (ret < 0)
		goto free_platform_data;

	ret = of_property_read_u32(np, "prox_thres_near", (u32 *)&pdata->prox_default_thres_near);
	if (ret < 0)
		goto free_platform_data;

	ret = of_property_read_u32(np, "prox_thres_far", (u32 *)&pdata->prox_default_thres_far);
	if (ret < 0)
		goto free_platform_data;

	client->dev.platform_data = pdata;

	return 0;

free_platform_data:
	dev_err(&client->dev, "err=%d\n", ret);
	kfree(pdata);
	return ret;
}

static int cm_check_chip(struct i2c_client *client)
{
	int ret = 0;
	uint16_t id_reg;

	ret = cm_i2c_read_word(client, ID_REG, &id_reg);
	if (ret < 0 || ((id_reg & 0x00FF) != CM36686_CHIP_ID)) {
		dev_err(&client->dev, "check chip ID fail: ret=%d, ID=0x%x\n",
			ret, id_reg);
		ret = -EIO;
	}

	return ret;
}

static int cm_prox_teardown(struct cm36686_data *data)
{
	cancel_delayed_work_sync(&data->prox_delayed_work);
	iio_device_unregister(data->prox_idev);
	iio_trigger_unregister(data->prox_idev->trig);
	iio_trigger_free(data->prox_idev->trig);
	iio_triggered_buffer_cleanup(data->prox_idev);
	iio_device_free(data->prox_idev);

	return 0;
}

static int cm_als_teardown(struct cm36686_data *data)
{
	cancel_delayed_work_sync(&data->als_delayed_work);
	iio_device_unregister(data->als_idev);
	iio_trigger_unregister(data->als_idev->trig);
	iio_trigger_free(data->als_idev->trig);
	iio_triggered_buffer_cleanup(data->als_idev);
	iio_device_free(data->als_idev);

	return 0;
}

static int cm36686_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct cm36686_data *data;
	struct cm36686_platform_data *pdata;

	dev_info(&client->dev, "start cm36686 PLSensor probe\n");

	ret = cm_check_chip(client);
	if (ret < 0)
		return ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);

	data->client = client;

	ret = cm_parse_dt(client);
	if (ret < 0)
		goto err_free_data;

	mutex_init(&data->mutex);
	data->suspended = false;
	data->dump_register = false;
	data->dump_output = false;
	pdata = client->dev.platform_data;
	data->prox_offset = pdata->prox_default_offset;
	data->prox_thres_near = pdata->prox_default_thres_near;
	data->prox_thres_far = pdata->prox_default_thres_far;
	INIT_DELAYED_WORK(&data->prox_delayed_work, cm_prox_work);
	INIT_DELAYED_WORK(&data->als_delayed_work, cm_als_work);

	ret = cm_proximity_iio_setup(data);
	if (ret < 0)
		goto err_free_pdata;

	ret = cm_light_iio_setup(data);
	if (ret < 0)
		goto err_free_prx_iio;

	dev_info(&client->dev, "cm36686 PLSensor probe successful\n");

	return ret;

err_free_prx_iio:
	cm_prox_teardown(data);
err_free_pdata:
	kfree(client->dev.platform_data);
err_free_data:
	kfree(data);

	dev_info(&client->dev, "cm36686 PLSensor probe failed\n");
	return ret;
}

static int cm36686_remove(struct i2c_client *client)
{
	struct cm36686_platform_data *pdata = client->dev.platform_data;
	struct cm36686_data *data = i2c_get_clientdata(client);

	cm_prox_teardown(data);
	cm_als_teardown(data);
	kfree(pdata);
	kfree(data);

	return 0;
}

static int cm36686_suspend(struct device *dev)
{
	int ret = 0;
	struct cm36686_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	data->suspended = true;
	ret = update_device(data);

	if (data->prox_enabled)
		cancel_delayed_work(&data->prox_delayed_work);

	if (data->als_enabled)
		cancel_delayed_work(&data->als_delayed_work);

	mutex_unlock(&data->mutex);

	return ret;
}

static int cm36686_resume(struct device *dev)
{
	int ret = 0;
	struct cm36686_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	data->suspended = false;
	ret = update_device(data);

	if (data->prox_enabled)
		schedule_delayed_work(&data->prox_delayed_work, CM36686_PROX_DELAY * HZ / 1000);

	if (data->als_enabled)
		schedule_delayed_work(&data->als_delayed_work, CM36686_ALS_DELAY * HZ / 1000);

	mutex_unlock(&data->mutex);

	return ret;
}

static SIMPLE_DEV_PM_OPS(cm36686_pm, cm36686_suspend, cm36686_resume);

static const struct i2c_device_id cm36686_i2c_id[] = {
	{CM36686_I2C_NAME, 0},
	{}
};

static const struct of_device_id cm36686_of_match[] = {
	{ .compatible = "cm,cm36686" },
	{},
};
MODULE_DEVICE_TABLE(of, cm36686_of_match);

static struct i2c_driver cm36686_driver = {
	.driver = {
		.name = CM36686_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cm36686_pm,
		.of_match_table = of_match_ptr(cm36686_of_match),
	},

	.id_table = cm36686_i2c_id,
	.probe = cm36686_probe,
	.remove	= cm36686_remove,
};

module_i2c_driver(cm36686_driver);

MODULE_AUTHOR("Qian Wenfa <qianwenfa@xiaomi.com>");
MODULE_DESCRIPTION("cm36686 PL sensor IIO driver");
MODULE_LICENSE("GPL");
