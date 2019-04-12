/*
 * leds-pca9956b.c - NXP PCA9956B LED segment driver
 *
 * Copyright (C) 2017 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include "leds-pca9956b.h"
#include <linux/gpio.h>
#include <linux/delay.h>

#define PCA9956B_LED_NUM	24
#define MAX_DEVICES		32

#define DRIVER_NAME		"nxp-ledseg"
#define DRIVER_VERSION		"17.11.28"
#define LED_RESET_GPIO          95

struct pca9956b_chip {
	struct i2c_client *client;
	struct mutex lock;
	struct pca9956b_led *leds;
};

struct pca9956b_led {
	struct  led_classdev led_cdev;
	struct  pca9956b_chip *chip;
	int	 led_num;
	char	name[32];
};

static struct device *pca9956b_dev;
static int pca9956b_setup(struct pca9956b_chip *chip);

/*
 * Read one byte from given register address.
 */
static int pca9956b_read_reg(struct pca9956b_chip *chip, int reg, uint8_t *val)
{
	int ret = i2c_smbus_read_byte_data(chip->client, reg);

	if (ret < 0) {
		dev_err(&chip->client->dev, "failed reading register\n");
		return ret;
	}

	*val = (uint8_t)ret;
	return 0;
}

/*
 * Write one byte to the given register address.
 */
static int pca9956b_write_reg(struct pca9956b_chip *chip, int reg, uint8_t val)
{
	int ret = i2c_smbus_write_byte_data(chip->client, reg, val);

	if (ret < 0) {
		dev_err(&chip->client->dev, "failed writing register\n");
		return ret;
	}

	return 0;
}

/*
 * Read string from device tree property and write it to the register.
 */
static int pca9956b_readWrite_reg(struct pca9956b_chip *chip,
				char *readStr, int writeRegAddr)
{
	struct device_node *np = chip->client->dev.of_node;
	uint32_t reg_value;
	int ret;

	ret = of_property_read_u32(np, readStr, &reg_value);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"[%s]: Unable to read %s\n",
			__func__, readStr);
		return ret;
	}

	mutex_lock(&chip->lock);

	ret = pca9956b_write_reg(chip, writeRegAddr, (uint8_t)reg_value);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"[%s]: Unable to write %s , value = 0x%x\n",
			__func__, readStr, reg_value);
		mutex_unlock(&chip->lock);
		return ret;
	}

	mutex_unlock(&chip->lock);

	return ret;
}

/*
 * Store one byte to given register address.
 */
static ssize_t pca9956b_storeReg(struct device *dev,
		struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct pca9956b_chip *chip = dev_get_drvdata(dev);
	unsigned int ret, reg_value, reg_addr;

	ret = sscanf(buf, "%x %x", &reg_addr, &reg_value);
	if (ret == 0) {
		dev_err(&chip->client->dev,
			"[%s] fail to pca9956b out.\n",
			__func__);
		return count;
	}

	if (reg_addr < PCA9956B_MODE1 || reg_addr > PCA9956B_IREFALL) {
		dev_err(&chip->client->dev,
			"[%s] Out of range. Reg = 0x%x\n",
			__func__, reg_addr);
		return count;
	}

	mutex_lock(&chip->lock);

	ret = pca9956b_write_reg(chip, reg_addr, (uint8_t)reg_value);
	if (ret != 0)
		dev_err(&chip->client->dev,
			"[%s] Operation [0x%x , %d] is failed.\n",
			__func__, reg_addr, reg_value);

	mutex_unlock(&chip->lock);

	return count;
}

/*
 * Show all registers
 */
static ssize_t pca9956b_showReg(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct pca9956b_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_value = 0;
	int ret, i;
	char *bufp = buf;

	mutex_lock(&chip->lock);

	for (i = PCA9956B_MODE1; i < PCA9956B_IREFALL; i++) {
		ret = pca9956b_read_reg(chip, i, &reg_value);
		if (ret != 0)
			dev_err(&chip->client->dev,
				"[%s] Reading reg[0x%x] is failed.\n",
				__func__, i);

		bufp += snprintf(bufp, PAGE_SIZE,
				"Addr[0x%x] = 0x%x\n", i, reg_value);
	}

	mutex_unlock(&chip->lock);

	return strlen(buf);
}

static DEVICE_ATTR(reg, 0664,
		pca9956b_showReg, pca9956b_storeReg);

/*
 * Show error register.
 */
static ssize_t pca9956_showErr(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct pca9956b_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_value = 0;
	int ret, i;
	char *bufp = buf;

	mutex_lock(&chip->lock);

	for (i = PCA9956B_EFLAG0 ; i <= PCA9956B_EFLAG4 ; i++) {
		ret = pca9956b_read_reg(chip, i, &reg_value);
		if (ret != 0)
			dev_err(&chip->client->dev,
				"[%s] Reading [0x%x] is failed.\n",
				__func__, i);

		bufp += snprintf(bufp, PAGE_SIZE, "PCA9956B_EFLAG[%d] = 0x%x\n",
				i - PCA9956B_EFLAG0, reg_value);
	}

	mutex_unlock(&chip->lock);

	return strlen(buf);
}
static DEVICE_ATTR(err, 0664,
				pca9956_showErr, NULL);

static struct attribute *attrs[] = {
	&dev_attr_err.attr,
	&dev_attr_reg.attr,
	NULL, /* Need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

/*
 * Individual PWM set function
 */
static void pca9956b_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct pca9956b_led *pca9956b;
	struct pca9956b_chip *chip;
	int ret;
	uint8_t reg_value;

	pca9956b = container_of(led_cdev, struct pca9956b_led, led_cdev);
	chip = pca9956b->chip;

	mutex_lock(&chip->lock);
	ret = pca9956b_read_reg(chip, PCA9956B_IREF0, &reg_value);
	mutex_unlock(&chip->lock);
	if (ret != 0)
		dev_err(&chip->client->dev,
			"[%s] Reading PCA9956B_IREF0 reg is failed\n",
			__func__);
	else if (reg_value != 0x2f) {
		ret = pca9956b_setup(chip);
		if (ret != 0)
			dev_err(&chip->client->dev,
				"[%s] pca9956b_setup = %d\n", __func__, ret);
	}

	mutex_lock(&chip->lock);
	ret = pca9956b_write_reg(chip,
				PCA9956B_PWM0 + pca9956b->led_num,
				value);
	mutex_unlock(&chip->lock);
	if (ret != 0)
		dev_err(&chip->client->dev,
			"[%s] pca9956b_write_reg failed = %d\n",
			__func__, ret);

	if (ret == -2) {
		dev_err(&chip->client->dev, "[%s] is failed = %d.\n",
			__func__, ret);

		ret = gpio_request(LED_RESET_GPIO, "LED RESET GPIO");
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"failed opening GPIO %d\n", ret);
			return;
		}

		gpio_export(LED_RESET_GPIO, 1);
		usleep_range(200000, 400000);
		ret = gpio_direction_output(LED_RESET_GPIO, 0);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"failed setting GPIO direction %d\n", ret);
			gpio_free(LED_RESET_GPIO);
			return;
		}
		usleep_range(200000, 400000);

		ret = gpio_direction_output(LED_RESET_GPIO, 1);
		gpio_set_value(LED_RESET_GPIO, 1);
		usleep_range(200000, 400000);

		ret = pca9956b_setup(chip);
		if (ret < 0)
			dev_err(&chip->client->dev, "failed pca9956b_setup\n");

		gpio_free(LED_RESET_GPIO);
	}
}

/*
 * Individual PWM get function
 */
static enum led_brightness pca9956b_brightness_get(
			struct led_classdev *led_cdev)
{
	struct pca9956b_led *pca9956b;
	struct pca9956b_chip *chip;
	int ret;
	uint8_t reg_value;

	pca9956b = container_of(led_cdev, struct pca9956b_led, led_cdev);
	chip = pca9956b->chip;

	mutex_lock(&chip->lock);
	ret = pca9956b_read_reg(chip, PCA9956B_PWM0 + pca9956b->led_num,
				&reg_value);
	if (ret != 0)
		dev_err(&chip->client->dev, "[%s] is failed = %d.\n",
			__func__, ret);

	mutex_unlock(&chip->lock);

	return reg_value;
}

static int pca9956b_registerClassDevice(struct i2c_client *client,
					struct pca9956b_chip *chip)
{
	int i = 0, err, reg;
	struct device_node *np = client->dev.of_node, *child;
	struct pca9956b_led *led;

	for_each_child_of_node(np, child) {
		err = of_property_read_u32(child, "reg", &reg);
		if (err) {
			of_node_put(child);
			pr_err(DRIVER_NAME": Failed to get child node");
			return err;
		}
		if (reg < 0 || reg >= PCA9956B_LED_NUM) {
			of_node_put(child);
			pr_err(DRIVER_NAME": Invalid reg value");
			return -EINVAL;
		}

		led = &chip->leds[reg];
		led->led_cdev.name =
			of_get_property(child, "label", NULL) ? : child->name;
		led->led_cdev.default_trigger =
			of_get_property(child, "linux,default-trigger", NULL);
		led->led_cdev.brightness_set = pca9956b_brightness_set;
		led->led_cdev.brightness_get = pca9956b_brightness_get;
		led->chip = chip;
		led->led_num = reg;
		i++;

		err = led_classdev_register(&client->dev,
					&led->led_cdev);
		if (err < 0) {
			pr_err(DRIVER_NAME": Failed to register LED class dev");
			goto exit;
		}
	}

	return 0;
exit:
	while (i--)
		led_classdev_unregister(&chip->leds[i].led_cdev);

	return err;
}

/*
 * Read properties and write it to register.
 */
static int pca9956b_setup(struct pca9956b_chip *chip)
{
	struct device_node *np = chip->client->dev.of_node;
	int ret;
	uint32_t reg_value;

	ret = of_property_read_u32(np, "pca9956b,support_initialize",
					&reg_value);
	if (ret < 0) {
		pr_err("[%s]: Unable to pca9956b,support_initialize\n",
			__func__);
		return ret;
	}

	if (reg_value == 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,mode1",
					PCA9956B_MODE1);
	if (ret < 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,mode2",
					PCA9956B_MODE2);
	if (ret < 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,ledout0",
					PCA9956B_LEDOUT0);
	if (ret < 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,ledout1",
					PCA9956B_LEDOUT1);
	if (ret < 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,ledout2",
					PCA9956B_LEDOUT2);
	if (ret < 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,ledout3",
					PCA9956B_LEDOUT3);
	if (ret < 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,ledout4",
					PCA9956B_LEDOUT4);
	if (ret < 0)
		return ret;

	ret = pca9956b_readWrite_reg(chip, "pca9956b,ledout5",
					PCA9956B_LEDOUT5);
	if (ret < 0)
		return ret;

	/* set default IREF to all IREF */
	{
		int reg_addr;

		ret = of_property_read_u32(np, "pca9956b,defaultiref",
					&reg_value);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"[%s]: Unable to read pca9956b,defaultiref\n",
				__func__);
			return ret;
		}
		mutex_lock(&chip->lock);

		for (reg_addr = PCA9956B_IREF0;
			reg_addr <= PCA9956B_IREF23; reg_addr++) {
			ret = pca9956b_write_reg(chip, reg_addr,
						(uint8_t)reg_value);
			if (ret < 0) {
				dev_err(&chip->client->dev,
					"[%s]: Unable to write reg0x%x[0x%x]\n",
					__func__, reg_addr, reg_value);
				mutex_unlock(&chip->lock);
				return ret;
			}
		}
		mutex_unlock(&chip->lock);
	}

	/* set IREF0 ~ IREF23 if required */

	return ret;
}


static int pca9956b_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct pca9956b_chip *chip;
	struct pca9956b_led *led;
	int ret;
	int i;

	pr_info(DRIVER_NAME": (I2C) "DRIVER_VERSION"\n");

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "SMBUS Byte Data not Supported\n");
		return -EIO;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->leds = devm_kzalloc(&client->dev, sizeof(*led)*PCA9956B_LED_NUM,
				GFP_KERNEL);
	if (!chip->leds) {
		devm_kfree(&client->dev, chip);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, chip);

	mutex_init(&chip->lock);
	chip->client = client;

	/* LED device class registration */
	ret = pca9956b_registerClassDevice(client, chip);
	if (ret < 0)
		goto exit;

	/* Configuration setup */
	ret = pca9956b_setup(chip);
	if (ret < 0)
		goto err_setup;

	pca9956b_dev = &client->dev;

	ret = sysfs_create_group(&pca9956b_dev->kobj, &attr_group);
	if (ret) {
		dev_err(&client->dev,
				"Failed to create sysfs group for pca9956b\n");
		goto err_setup;
	}

	return 0;

err_setup:
	for (i = 0; i < PCA9956B_LED_NUM; i++)
		led_classdev_unregister(&chip->leds[i].led_cdev);
exit:
	mutex_destroy(&chip->lock);
	devm_kfree(&client->dev, chip->leds);
	devm_kfree(&client->dev, chip);
	return ret;
}

static int pca9956b_remove(struct i2c_client *client)
{
	struct pca9956b_chip *dev = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < PCA9956B_LED_NUM; i++)
		led_classdev_unregister(&dev->leds[i].led_cdev);

	sysfs_remove_group(&pca9956b_dev->kobj, &attr_group);

	mutex_destroy(&dev->lock);
	devm_kfree(&client->dev, dev->leds);
	devm_kfree(&client->dev, dev);
	return 0;
}

static const struct of_device_id pca9956b_dt_ids[] = {
	{ .compatible = "nxp,pca9956b",},
};

static const struct i2c_device_id pca9956b_id[] = {
	{DRIVER_NAME"-i2c", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pca9956b_id);

static struct i2c_driver pca9956b_driver = {
	.driver = {
		   .name = DRIVER_NAME"-i2c",
		   .of_match_table = of_match_ptr(pca9956b_dt_ids),
	},
	.probe = pca9956b_probe,
	.remove = pca9956b_remove,
	.id_table = pca9956b_id,
};

static int __init pca9956b_init(void)
{
	return i2c_add_driver(&pca9956b_driver);
}

static void __exit pca9956b_exit(void)
{
	i2c_del_driver(&pca9956b_driver);
}
module_init(pca9956b_init);
module_exit(pca9956b_exit);
MODULE_AUTHOR("NXP Semiconductors");
MODULE_DESCRIPTION("PCA9956B : 24-channel constant current LED driver");
MODULE_LICENSE("GPL v2");

