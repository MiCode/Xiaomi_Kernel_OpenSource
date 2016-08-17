/*
 * drivers/mfd/tps8003x-gpadc.c
 *
 * Gpadc for TI's tps80031
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i2c/twl.h>
#include <linux/mfd/tps80031.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

#define GPADC_CTRL	0x2e
#define GPSELECT_ISB	0x35
#define GPCH0_LSB	0x3b
#define GPCH0_MSB	0x3c
#define CTRL_P1		0x36
#define TOGGLE1		0x90
#define MISC1		0xe4

#define CTRL_P1_SP1	BIT(3)
#define TOGGLE1_GPADCR	BIT(1)
#define GPADC_BUSY	(1 << 0)
#define GPADC_EOC_SW	(1 << 1)
#define SCALE		(1 << 15)

#define TPS80031_GPADC_MAX_CHANNELS 17
#define TPS80031_GPADC_IOC_MAGIC '`'
#define TPS80031_GPADC_IOCX_ADC_RAW_READ _IO(TPS80031_GPADC_IOC_MAGIC, 0)

struct tps80031_gpadc_user_parms {
	int channel;
	int status;
	u16 result;
};

struct tps80031_calibration {
	s32 gain_error;
	s32 offset_error;
};

struct tps80031_ideal_code {
	s16 code1;
	s16 code2;
};

struct tps80031_scalar_channel {
	uint8_t delta1_addr;
	uint8_t delta1_mask;
	uint8_t delta2_addr;
	uint8_t delta2_mask;
};

static struct tps80031_calibration
	tps80031_calib_tbl[TPS80031_GPADC_MAX_CHANNELS];
static const uint32_t calibration_bit_map = 0x47FF;
static const uint32_t scalar_bit_map = 0x4785;

#define TPS80031_GPADC_TRIM1	0xCD
#define TPS80031_GPADC_TRIM2	0xCE
#define TPS80031_GPADC_TRIM3	0xCF
#define TPS80031_GPADC_TRIM4	0xD0
#define TPS80031_GPADC_TRIM5	0xD1
#define TPS80031_GPADC_TRIM6	0xD2
#define TPS80031_GPADC_TRIM7	0xD3
#define TPS80031_GPADC_TRIM8	0xD4
#define TPS80031_GPADC_TRIM9	0xD5
#define TPS80031_GPADC_TRIM10	0xD6
#define TPS80031_GPADC_TRIM11	0xD7
#define TPS80031_GPADC_TRIM12	0xD8
#define TPS80031_GPADC_TRIM13	0xD9
#define TPS80031_GPADC_TRIM14	0xDA
#define TPS80031_GPADC_TRIM15	0xDB
#define TPS80031_GPADC_TRIM16	0xDC
#define TPS80031_GPADC_TRIM19	0xFD

static const struct tps80031_scalar_channel
		tps80031_trim[TPS80031_GPADC_MAX_CHANNELS] = {
	{ TPS80031_GPADC_TRIM1, 0x7, TPS80031_GPADC_TRIM2, 0x07},
	{ 0x00, },
	{ TPS80031_GPADC_TRIM3, 0x1F, TPS80031_GPADC_TRIM4, 0x3F},
	{ 0x00, },
	{ 0x00, },
	{ 0x00, },
	{ 0x00, },
	{ TPS80031_GPADC_TRIM7, 0x1F, TPS80031_GPADC_TRIM8, 0x1F },
	{ TPS80031_GPADC_TRIM9, 0x0F, TPS80031_GPADC_TRIM10, 0x1F },
	{ TPS80031_GPADC_TRIM11, 0x0F, TPS80031_GPADC_TRIM12, 0x1F },
	{ TPS80031_GPADC_TRIM13, 0x0F, TPS80031_GPADC_TRIM14, 0x1F },
	{ 0x00, },
	{ 0x00, },
	{ 0x00, },
	{ TPS80031_GPADC_TRIM15, 0x0f, TPS80031_GPADC_TRIM16, 0x1F },
	{ 0x00, },
	{ 0x00 ,},
};

/*
* actual scaler gain is multiplied by 8 for fixed point operation
* 1.875 * 8 = 15
*/
static const uint16_t tps80031_gain[TPS80031_GPADC_MAX_CHANNELS] = {
	1142,   /* CHANNEL 0 */
	8,      /* CHANNEL 1 */
	/* 1.875 */
	15,     /* CHANNEL 2 */
	8,      /* CHANNEL 3 */
	8,      /* CHANNEL 4 */
	8,      /* CHANNEL 5 */
	8,      /* CHANNEL 6 */
	/* 5 */
	40,     /* CHANNEL 7 */
	/* 6.25 */
	50,     /* CHANNEL 8 */
	/* 11.25 */
	90,     /* CHANNEL 9 */
	/* 6.875 */
	55,     /* CHANNEL 10 */
	/* 1.875 */
	15,     /* CHANNEL 11 */
	8,      /* CHANNEL 12 */
	8,      /* CHANNEL 13 */
	/* 6.875 */
	55,     /* CHANNEL 14 */
	8,      /* CHANNEL 15 */
	8,      /* CHANNEL 16 */
};

/*
* calibration not needed for channel 11, 12, 13, 15 and 16
* calibration offset is same for channel 1, 3, 4, 5
*/
static const struct tps80031_ideal_code
	tps80031_ideal[TPS80031_GPADC_MAX_CHANNELS] = {
	{463,   2982},  /* CHANNEL 0 */
	{328,   3604},  /* CHANNEL 1 */
	{221,   3274},  /* CHANNEL 2 */
	{328,   3604},  /* CHANNEL 3 */
	{328,   3604},  /* CHANNEL 4 */
	{328,   3604},  /* CHANNEL 5 */
	{328,   3604},  /* CHANNEL 6 */
	{1966,  3013},  /* CHANNEL 7 */
	{328,   2754},  /* CHANNEL 8 */
	{728,   3275},  /* CHANNEL 9 */
	{596,   3274},  /* CHANNEL 10 */
	{0,     0},     /* CHANNEL 11 */
	{0,     0},     /* CHANNEL 12 */
	{0,     0},     /* CHANNEL 13 */
	{193,   2859},  /* CHANNEL 14 */
	{0,     0},     /* CHANNEL 15 */
	{0,     0},     /* CHANNEL 16 */
};

struct tps80031_gpadc_data {
	struct device		*dev;
	struct mutex		lock;
};

static struct tps80031_gpadc_data *the_gpadc;

static ssize_t show_gain(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int value;
	int status;

	value = tps80031_calib_tbl[attr->index].gain_error;
	status = sprintf(buf, "%d\n", value);
	return status;
}

static ssize_t set_gain(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	long val;
	int status = count;

	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	if ((strict_strtol(buf, 10, &val) < 0) || (val < 15000)
				     || (val > 60000))
		return -EINVAL;
	tps80031_calib_tbl[attr->index].gain_error = val;
	return status;
}

static ssize_t show_offset(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int value;
	int status;

	value = tps80031_calib_tbl[attr->index].offset_error;
	status = sprintf(buf, "%d\n", value);
	return status;
}

static ssize_t set_offset(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	long val;
	int status = count;

	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	if ((strict_strtol(buf, 10, &val) < 0) || (val < 15000)
							|| (val > 60000))
		return -EINVAL;
	tps80031_calib_tbl[attr->index].offset_error = val;
	return status;
}

static int tps80031_reg_read(struct tps80031_gpadc_data *gpadc, int sid,
				int reg, uint8_t *val)
{
	int ret;

	ret = tps80031_read(gpadc->dev->parent, sid, reg, val);
	if (ret < 0)
		dev_err(gpadc->dev, "Failed read register 0x%02x\n", reg);
	return ret;
}

static int tps80031_reg_write(struct tps80031_gpadc_data *gpadc, int sid,
				int reg, uint8_t val)
{
	int ret;

	ret = tps80031_write(gpadc->dev->parent, sid, reg, val);
	if (ret < 0)
		dev_err(gpadc->dev, "Failed write register 0x%02x\n", reg);
	return ret;
}

static int tps80031_gpadc_channel_raw_read(struct tps80031_gpadc_data *gpadc)
{
	uint8_t msb, lsb;
	int ret;
	ret = tps80031_reg_read(gpadc, SLAVE_ID2, GPCH0_LSB, &lsb);
	if (ret < 0)
		return ret;
	ret = tps80031_reg_read(gpadc, SLAVE_ID2, GPCH0_MSB, &msb);
	if (ret < 0)
		return ret;

	return (int)((msb << 8) | lsb);
}

static int tps80031_gpadc_read_channels(struct tps80031_gpadc_data *gpadc,
						uint32_t channel)
{
	uint8_t bits;
	int gain_error;
	int offset_error;
	int raw_code;
	int corrected_code;
	int channel_value;
	int raw_channel_value;

	/* TPS80031 has 12bit ADC */
	bits = 12;
	raw_code = tps80031_gpadc_channel_raw_read(gpadc);
	if (raw_code < 0)
		return raw_code;
	/*
	 * Channels 0,2,7,8,9,10,14 offst and gain cannot
	 * be fully compensated by software
	 */
	if (channel == 7)
		return raw_code;
	/*
	 * multiply by 1000 to convert the unit to milli
	 * division by 1024 (>> bits) for 10/12 bit ADC
	 * division by 8 (>> 3) for actual scaler gain
	 */
	raw_channel_value =
		(raw_code * tps80031_gain[channel] * 1000) >> (bits + 3);

	gain_error = tps80031_calib_tbl[channel].gain_error;
	offset_error = tps80031_calib_tbl[channel].offset_error;
	corrected_code = (raw_code * SCALE - offset_error) / gain_error;
	channel_value =
		(corrected_code * tps80031_gain[channel] * 1000) >> (bits + 3);
	return channel_value;
}

static int tps80031_gpadc_wait_conversion_ready(
	struct tps80031_gpadc_data *gpadc,
		unsigned int timeout_ms)
{
	int ret;
	unsigned long timeout;
	timeout = jiffies + msecs_to_jiffies(timeout_ms);
	do {
		uint8_t reg;
		ret = tps80031_reg_read(gpadc, SLAVE_ID2, CTRL_P1, &reg);
		if (ret < 0)
			return ret;
		if (!(reg & GPADC_BUSY) &&
				(reg & GPADC_EOC_SW))
			return 0;
	} while (!time_after(jiffies, timeout));
	return -EAGAIN;
}

static inline int tps80031_gpadc_config
	(struct tps80031_gpadc_data *gpadc, int channel_no)
{
	int ret = 0;

	ret = tps80031_reg_write(gpadc, SLAVE_ID2, TOGGLE1, TOGGLE1_GPADCR);
	if (ret < 0)
		return ret;

	ret = tps80031_reg_write(gpadc, SLAVE_ID2, GPSELECT_ISB, channel_no);
	if (ret < 0)
		return ret;

	ret = tps80031_reg_write(gpadc, SLAVE_ID2, GPADC_CTRL, 0xef);
	if (ret < 0)
		return ret;

	ret = tps80031_reg_write(gpadc, SLAVE_ID1, MISC1, 0x02);
	if (ret < 0)
		return ret;

	return ret;
}

int tps80031_gpadc_conversion(int channel_no)
{
	int ret = 0;
	int read_value;

	mutex_lock(&the_gpadc->lock);

	ret = tps80031_gpadc_config(the_gpadc, channel_no);
	if (ret < 0)
		goto err;

	/* start ADC conversion */
	ret = tps80031_reg_write(the_gpadc, SLAVE_ID2, CTRL_P1, CTRL_P1_SP1);
	if (ret < 0)
		goto err;

	/* Wait until conversion is ready (ctrl register returns EOC) */
	ret = tps80031_gpadc_wait_conversion_ready(the_gpadc, 5);
	if (ret) {
		dev_dbg(the_gpadc->dev, "conversion timeout!\n");
		goto err;
	}

	read_value = tps80031_gpadc_read_channels(the_gpadc, channel_no);
	mutex_unlock(&the_gpadc->lock);
	return read_value;
err:
	mutex_unlock(&the_gpadc->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(tps80031_gpadc_conversion);

static SENSOR_DEVICE_ATTR(in0_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 0);
static SENSOR_DEVICE_ATTR(in0_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 0);
static SENSOR_DEVICE_ATTR(in1_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 1);
static SENSOR_DEVICE_ATTR(in1_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 1);
static SENSOR_DEVICE_ATTR(in2_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 2);
static SENSOR_DEVICE_ATTR(in2_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 2);
static SENSOR_DEVICE_ATTR(in3_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 3);
static SENSOR_DEVICE_ATTR(in3_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 3);
static SENSOR_DEVICE_ATTR(in4_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 4);
static SENSOR_DEVICE_ATTR(in4_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 4);
static SENSOR_DEVICE_ATTR(in5_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 5);
static SENSOR_DEVICE_ATTR(in5_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 5);
static SENSOR_DEVICE_ATTR(in6_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 6);
static SENSOR_DEVICE_ATTR(in6_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 6);
static SENSOR_DEVICE_ATTR(in7_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 7);
static SENSOR_DEVICE_ATTR(in7_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 7);
static SENSOR_DEVICE_ATTR(in8_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 8);
static SENSOR_DEVICE_ATTR(in8_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 8);
static SENSOR_DEVICE_ATTR(in9_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 9);
static SENSOR_DEVICE_ATTR(in9_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 9);
static SENSOR_DEVICE_ATTR(in10_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 10);
static SENSOR_DEVICE_ATTR(in10_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 10);
static SENSOR_DEVICE_ATTR(in11_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 11);
static SENSOR_DEVICE_ATTR(in11_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 11);
static SENSOR_DEVICE_ATTR(in12_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 12);
static SENSOR_DEVICE_ATTR(in12_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 12);
static SENSOR_DEVICE_ATTR(in13_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 13);
static SENSOR_DEVICE_ATTR(in13_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 13);
static SENSOR_DEVICE_ATTR(in14_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 14);
static SENSOR_DEVICE_ATTR(in14_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 14);
static SENSOR_DEVICE_ATTR(in15_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 15);
static SENSOR_DEVICE_ATTR(in15_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 15);
static SENSOR_DEVICE_ATTR(in16_gain, S_IRUGO|S_IWUSR, show_gain, set_gain, 16);
static SENSOR_DEVICE_ATTR(in16_offset, S_IRUGO|S_IWUSR,
						show_offset, set_offset, 16);

#define IN_ATTRS(X)\
	&sensor_dev_attr_in##X##_gain.dev_attr.attr,    \
	&sensor_dev_attr_in##X##_offset.dev_attr.attr   \

static struct attribute *tps80031_gpadc_attributes[] = {
	IN_ATTRS(0),
	IN_ATTRS(1),
	IN_ATTRS(2),
	IN_ATTRS(3),
	IN_ATTRS(4),
	IN_ATTRS(5),
	IN_ATTRS(6),
	IN_ATTRS(7),
	IN_ATTRS(8),
	IN_ATTRS(9),
	IN_ATTRS(10),
	IN_ATTRS(11),
	IN_ATTRS(12),
	IN_ATTRS(13),
	IN_ATTRS(14),
	IN_ATTRS(15),
	IN_ATTRS(16),
	NULL
};

static const struct attribute_group tps80031_gpadc_group = {
	.attrs = tps80031_gpadc_attributes,
};

static long tps80031_gpadc_ioctl(struct file *filp, unsigned int cmd,
						unsigned long arg)
{
	struct tps80031_gpadc_user_parms par;
	int val, ret, channel_no;

	ret = copy_from_user(&par, (void __user *) arg, sizeof(par));
	if (ret) {
		dev_dbg(the_gpadc->dev, "copy_from_user: %d\n", ret);
		return -EACCES;
	}
	switch (cmd) {
	case TPS80031_GPADC_IOCX_ADC_RAW_READ:
		channel_no = par.channel;
		val = tps80031_gpadc_conversion(channel_no);
		if (likely(val > 0)) {
			par.status = 0;
			par.result = val;
		} else if (val == 0) {
			par.status = -ENODATA;
		} else {
			par.status = val;
		}
		break;
	default:
		return -EINVAL;
	}
	ret = copy_to_user((void __user *) arg, &par, sizeof(par));
	if (ret) {
		dev_dbg(the_gpadc->dev, "copy_to_user: %d\n", ret);
		return -EACCES;
	}
	return 0;
}

static const struct file_operations tps80031_gpadc_fileops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tps80031_gpadc_ioctl,
};

static struct miscdevice tps80031_gpadc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tps80031-gpadc",
	.fops = &tps80031_gpadc_fileops
};

static int __devinit tps80031_gpadc_probe(struct platform_device *pdev)
{
	struct tps80031_gpadc_data *gpadc;

	s16 delta_error1 = 0, delta_error2 = 0;
	s16 ideal_code1, ideal_code2;
	s16 scalar_delta1 = 0, scalar_delta2 = 0;
	s32 gain_error_1;
	s32 offset_error;
	uint8_t l_delta1, l_delta2, h_delta2;
	uint8_t l_scalar1, l_scalar2;
	uint8_t sign;
	uint8_t index;
	int ret;

	gpadc = devm_kzalloc(&pdev->dev, sizeof *gpadc, GFP_KERNEL);
	if (!gpadc)
		return -ENOMEM;

	gpadc->dev = &pdev->dev;
	ret = misc_register(&tps80031_gpadc_device);
	if (ret) {
		dev_dbg(&pdev->dev, "could not register misc_device\n");
		return ret;
	}

	platform_set_drvdata(pdev, gpadc);
	mutex_init(&gpadc->lock);

	for (index = 0; index < TPS80031_GPADC_MAX_CHANNELS; index++) {
		if (~calibration_bit_map & (1 << index))
			continue;

		if (~scalar_bit_map & (1 << index)) {
			ret = tps80031_reg_read(gpadc, SLAVE_ID2,
				tps80031_trim[index].delta1_addr, &l_scalar1);
			if (ret < 0)
				goto err;
			ret = tps80031_reg_read(gpadc, SLAVE_ID2,
				tps80031_trim[index].delta2_addr, &l_scalar2);
			if (ret < 0)
				goto err;

			l_scalar1 &= tps80031_trim[index].delta1_mask;
			sign = l_scalar1 & 1;
			scalar_delta1 = l_scalar1 >> 1;
			if (sign)
				scalar_delta1 = 0 - scalar_delta1;
			l_scalar2 &= tps80031_trim[index].delta2_mask;
			sign = l_scalar2 & 1;
			scalar_delta2 = l_scalar2 >> 1;
			if (sign)
				scalar_delta2 = 0 - scalar_delta2;
		} else {
			scalar_delta1 = 0;
			scalar_delta2 = 0;
		}
		ret = tps80031_reg_read(gpadc, SLAVE_ID2, TPS80031_GPADC_TRIM5,
							&l_delta1);
		if (ret < 0)
			goto err;
		ret = tps80031_reg_read(gpadc, SLAVE_ID2, TPS80031_GPADC_TRIM6,
							&l_delta2);
		if (ret < 0)
			goto err;
		ret = tps80031_reg_read(gpadc, SLAVE_ID2, TPS80031_GPADC_TRIM19,
							&h_delta2);
		if (ret < 0)
			goto err;

		sign = l_delta1 & 1;

		delta_error1 = l_delta1 >> 1;
		if (sign)
			delta_error1 = (0 - delta_error1);
		sign = l_delta2 & 1;

		delta_error2 = (l_delta2 >> 1) | (h_delta2 << 7);
		if (sign)
			delta_error2 = (0 - delta_error2);
		ideal_code1 = tps80031_ideal[index].code1 * 4;
		ideal_code2 = tps80031_ideal[index].code2 * 4;

		gain_error_1 = ((delta_error2 + scalar_delta2) -
				(delta_error1 - scalar_delta1)) *
				SCALE / (ideal_code2 - ideal_code1);
		offset_error = (delta_error1 + scalar_delta1) *
				SCALE - gain_error_1 *  ideal_code1;

		tps80031_calib_tbl[index].gain_error = gain_error_1 + SCALE;
		tps80031_calib_tbl[index].offset_error = offset_error;
	}

	the_gpadc = gpadc;
	ret = sysfs_create_group(&pdev->dev.kobj, &tps80031_gpadc_group);
	if (ret) {
		dev_err(&pdev->dev, "could not create sysfs files\n");
		goto err;
	}
	return 0;
err:
	misc_deregister(&tps80031_gpadc_device);
	return ret;
}

static int __devexit tps80031_gpadc_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &tps80031_gpadc_group);
	misc_deregister(&tps80031_gpadc_device);
	return 0;
}

static struct platform_driver tps80031_gpadc_driver = {
	.probe		= tps80031_gpadc_probe,
	.remove		= __devexit_p(tps80031_gpadc_remove),
	.driver		= {
		.name	= "tps80031-gpadc",
		.owner	= THIS_MODULE,
	},
};

static int __init tps80031_gpadc_init(void)
{
	return platform_driver_register(&tps80031_gpadc_driver);
}

module_init(tps80031_gpadc_init);

static void __exit tps80031_gpadc_exit(void)
{
	platform_driver_unregister(&tps80031_gpadc_driver);
}

module_exit(tps80031_gpadc_exit);
MODULE_ALIAS("platform:tps80031-gpadc");
MODULE_DESCRIPTION("tps80031 ADC driver");
