/*
 * A driver for pressure sensor BMP180.
 *
 * BMP180 pressure sensor driver to detect pressure
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/regmap.h>

/* 16 bit regs address initialization */
#define	BMP180_REG_AC1 0xAA
#define	BMP180_REG_AC2 0xAC
#define	BMP180_REG_AC3 0xAE
#define	BMP180_REG_AC4 0xB0
#define	BMP180_REG_AC5 0xB2
#define	BMP180_REG_AC6 0xB4
#define	BMP180_REG_B1 0xB6
#define	BMP180_REG_B2 0xB8
#define	BMP180_REG_MB 0xBA
#define	BMP180_REG_MC 0xBC
#define	BMP180_REG_MD 0xBE

/* 8 bit regs address initialization */
#define BMP180_REG_ID 0xD0
#define BMP180_REG_CTRL 0xF4
#define BMP180_REG_OUT_MSB	0xF6
#define BMP180_REG_OUT_LSB	0xF7
#define BMP180_REG_OUT_XLSB	0xF8l

/* control word init */
#define BMP180_CTRL_PINIT	0x34
#define BMP180_CTRL_TINIT	0x2E
#define BMP180_MAX_DIGIT	10
/* max pressure read by chip is 1100 hPa */

/* delay macros */
#define BMP180_DELAY_ULP 5
#define	BMP180_DELAY_ST 8
#define	BMP180_DELAY_HIGH_RES 14
#define	BMP180_DELAY_UHIGH_RES 26

struct bmp180_chip {
	struct i2c_client       *client;
	struct mutex            lock;
	struct regmap		*regmap;
	/* calibration data register values for the chip */
	s16 ac1;
	s16 ac2;
	s16 ac3;
	u16 ac4;
	u16 ac5;
	u16 ac6;
	s16 b1;
	s16 b2;
	s16 mb;
	s16 mc;
	s16 md;
	/* calibration data end */
	u8 oss;
	long UT;	/* uncompensated temperature */
	long UP;	/* uncompensated pressure */
	long pressure;	/* final pressure in hPa/100 Pa/1 mBar */
	u16 delay;
};

static int bmp180_write_data(struct i2c_client *client, u8 reg_addr,
							u8 value)
{
	struct bmp180_chip *chip = i2c_get_clientdata(client);
	return regmap_write(chip->regmap, reg_addr, value);
}

static int bmp180_read_data(struct i2c_client *client, u8 reg_addr, u16 *val)
{
	struct bmp180_chip *chip = i2c_get_clientdata(client);
	return regmap_read(chip->regmap, reg_addr, (unsigned int *)val);
}

static int bmp180_read_word(struct i2c_client *client, u8 reg_msb_addr,
							u16 *val)
{
	int ret_val;
	u16 result;
	u16 temp;

	ret_val = bmp180_read_data(client, reg_msb_addr, &temp);
	if (ret_val < 0) {
		dev_err(&client->dev, "Error msb data in sensor\n");
		return ret_val;
	} else {
		result = (u8)temp;
		ret_val = bmp180_read_data(client, reg_msb_addr+1, &temp);
		if (ret_val < 0) {
			dev_err(&client->dev, "Error lsb data in sensor\n");
			return ret_val;
		} else {
			result = (result << 8)+(u8)temp;
			*val = result;
			return 0;
		}
	}
}

static ssize_t bmp180_update_oss(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count) {
	struct bmp180_chip *chip = dev_get_drvdata(dev);
	int ret;
	u8 oss;

	ret = kstrtou8(buf, 10, &oss);
	if (ret > 0) {
		mutex_lock(&chip->lock);
		chip->oss = oss;
		switch (oss) {
		case 0:
			chip->delay = BMP180_DELAY_ULP;
			break;
		case 1:
			chip->delay = BMP180_DELAY_ST;
			break;
		case 2:
			chip->delay = BMP180_DELAY_HIGH_RES;
			break;
		case 3:
			chip->delay = BMP180_DELAY_UHIGH_RES;
			break;
		}
		mutex_unlock(&chip->lock);
		return ret;
	}
	return -1;
}

static int bmp180_chip_init(struct i2c_client *client)
{
	struct bmp180_chip *chip = i2c_get_clientdata(client);
	int ret;
	u16 val;

	dev_dbg(&client->dev, "%s called\n", __func__);
	mutex_lock(&chip->lock);

	chip->client = client;

	chip->UT = 0;
	chip->UP = 0;
	chip->pressure = 0;
	chip->oss = 0;
	chip->delay = BMP180_DELAY_ULP;
	ret = bmp180_read_word(client, BMP180_REG_AC1, &val);
	if (ret < 0)
		goto error;
	else
		chip->ac1 = (s16)val;
	ret = bmp180_read_word(client, BMP180_REG_AC2, &val);
	if (ret < 0)
		goto error;
	else
		chip->ac2 = (s16)val;
	ret = bmp180_read_word(client, BMP180_REG_AC3, &val);
	if (ret < 0)
		goto error;
	else
		chip->ac3 = (s16)val;
	ret = bmp180_read_word(client, BMP180_REG_AC4, &val);
	if (ret < 0)
		goto error;
	else
		chip->ac4 = (u16)val;
	ret = bmp180_read_word(client, BMP180_REG_AC5, &val);
	if (ret < 0)
		goto error;
	else
		chip->ac5 = (u16)val;
	ret = bmp180_read_word(client, BMP180_REG_AC6, &val);
	if (ret < 0)
		goto error;
	else
		chip->ac6 = (u16)val;
	ret = bmp180_read_word(client, BMP180_REG_B1, &val);
	if (ret < 0)
		goto error;
	else
		chip->b1 = (s16)val;
	ret = bmp180_read_word(client, BMP180_REG_B2, &val);
	if (ret < 0)
		goto error;
	else
		chip->b2 = (s16)val;
	ret = bmp180_read_word(client, BMP180_REG_MB, &val);
	if (ret < 0)
		goto error;
	else
		chip->mb = (s16)val;
	ret = bmp180_read_word(client, BMP180_REG_MC, &val);
	if (ret < 0)
		goto error;
	else
		chip->mc = (s16)val;
	ret = bmp180_read_word(client, BMP180_REG_MD, &val);
	if (ret < 0)
		goto error;
	else
		chip->md = (s16)val;
	mutex_unlock(&chip->lock);
	return 0;
error:
	dev_err(&client->dev, "Error in reading register\n");
	return -1;
}

static long bmp180_convert_UP(struct bmp180_chip *chip)
{
	long X1, X2, X3, B3, B4, B5, B6, B7;

	X1 = (chip->UT - chip->ac6) * (chip->ac5 >> 15);
	X2 = chip->mc * (1 << 11)/(X1 + chip->md);
	B5 = X1 + X2;
	B6 = B5 - 4000;
	X1 = (chip->b2 * (B6 * (B6 >> 12))) >> 11;
	X2 = chip->ac2 * (B6 >> 11);
	X3 = X1 + X2;
	B3 = ((((chip->ac1 << 2) + X3) << chip->oss) + 2) >> 2;
	X1 = chip->ac3 * (B6 >> 13);
	X2 = (chip->b1 * (B6 * (B6 >> 12))) >> 16;
	X3 = ((X1 + X2) + 2) >> 2;
	B4 = chip->ac4 * ((u32)(X3 + 32768)) >> 15;
	B7 = ((u32)chip->UP - B3) * (50000 >> chip->oss);
	if (B7 < 0x80000000)
		chip->pressure = (B7 * 2) / B4;
	else
		chip->pressure = (B7 / B4) * 2;
	X1 = (chip->pressure >> 8) * (chip->pressure >> 8);
	X1 = (X1 * 3038) >> 16;
	X2 = (-7357 * chip->pressure) >> 16;
	chip->pressure = chip->pressure + ((X1 + X2 + 3791) >> 14);
	return chip->pressure;
}

static ssize_t bmp180_read_pressure(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct bmp180_chip *chip;
	struct i2c_client *client;
	int ret;
	long temp_UT, temp_UP;
	u16 val;
	u8 xlsb;

	chip = dev_get_drvdata(dev);
	client = chip->client;
	dev_dbg(&client->dev, "%s\n called\n", __func__);
	ret = bmp180_write_data(client, BMP180_REG_CTRL, BMP180_CTRL_TINIT);
	if (ret < 0)
		goto exit;
	mdelay(5);
	ret = bmp180_read_word(client, BMP180_REG_OUT_MSB, &val);
	if (ret < 0)
		goto exit;
	temp_UT = (long)val;
	ret = bmp180_write_data(client, BMP180_REG_CTRL,
			BMP180_CTRL_PINIT+(chip->oss<<6));
	if (ret < 0)
		goto exit;
	mdelay(chip->delay);
	ret = bmp180_read_word(client, BMP180_REG_OUT_MSB, &val);
	if (ret < 0)
		goto exit;
	temp_UP = (long)val;
	if (chip->oss > 0) {
		ret = bmp180_read_data(client, BMP180_REG_OUT_XLSB, &val);
		if (ret < 0)
			goto exit;
		xlsb = (u8)val;
		temp_UP = (temp_UP << chip->oss) + xlsb;
	}
	mutex_lock(&chip->lock);
	chip->UT = temp_UT;
	chip->UP = temp_UP;
	bmp180_convert_UP(chip);
	dev_dbg(&client->dev, "pressure value read %lu\n\n", chip->pressure);
	ret = snprintf(buf, BMP180_MAX_DIGIT, "%lu", chip->pressure);
	mutex_unlock(&chip->lock);
	if (ret > 0)
		return ret;
exit:
	dev_err(&client->dev, "R/W operation failed\n");
	return -1;
}

static DEVICE_ATTR(pressure, 0444, bmp180_read_pressure, NULL);
static DEVICE_ATTR(oss, 0664, NULL, bmp180_update_oss);

static struct attribute *bmp180_attrs[] = {
	&dev_attr_pressure.attr,
	&dev_attr_oss.attr,
	NULL
};

static const struct attribute_group bmp180_attr_group = {
	.attrs = bmp180_attrs,
};

static bool is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case BMP180_REG_ID:
	case BMP180_REG_CTRL:
	case BMP180_REG_OUT_MSB:
	case BMP180_REG_OUT_LSB:
	case BMP180_REG_OUT_XLSB:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config bmp180_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_reg = is_volatile_reg,
	.max_register = BMP180_REG_OUT_XLSB,
	.num_reg_defaults_raw = BMP180_REG_OUT_XLSB + 1,
	.cache_type = REGCACHE_RBTREE,
};

static int bmp180_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct bmp180_chip *chip;
	int error = 0;

	dev_dbg(&client->dev, "%s\n called\n", __func__);
	chip = devm_kzalloc(&client->dev, sizeof(struct bmp180_chip),
							GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Memory Allocation Failed\n");
		error = -ENOMEM;
		goto exit;
	}
	i2c_set_clientdata(client, chip);
	mutex_init(&chip->lock);
	chip->regmap = devm_regmap_init_i2c(client, &bmp180_regmap_config);
	if (IS_ERR(chip->regmap)) {
		error = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "regmap initialization failed: %d\n",
								error);
		goto exit;
	}
	error = bmp180_chip_init(client);
	if (error < 0) {
		dev_err(&client->dev, "Probe failed in chip init\n");
		goto exit;
	}
	error = sysfs_create_group(&client->dev.kobj, &bmp180_attr_group);
	if (error) {
		dev_err(&client->dev, "Failed to create sysfs group\n");
		goto exit;
	}
	return 0;
exit:
	return error;
}

static int bmp180_remove(struct i2c_client *client)
{
	struct bmp180_chip *chip = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s\n called\n", __func__);
	sysfs_remove_group(&client->dev.kobj, &bmp180_attr_group);
	mutex_destroy(&chip->lock);
	return 0;
}


static const struct i2c_device_id bmp180_id[] = {
	{"bmp180", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmp180_id);

static struct i2c_driver bmp180_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = "bmp180",
		.owner = THIS_MODULE,
	},
	.probe = bmp180_probe,
	.remove = bmp180_remove,
	.id_table = bmp180_id
};

module_i2c_driver(bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sumit Sharma <sumsharma@nvidia.com>");
MODULE_DESCRIPTION("Pressure Sensor BMP180 driver");
