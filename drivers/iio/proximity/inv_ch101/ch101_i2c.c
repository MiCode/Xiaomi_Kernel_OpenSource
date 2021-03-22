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

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/gfp.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/iio/iio.h>
#include <linux/gpio/driver.h>

#include "ch101_data.h"
#include "ch101_client.h"
#include "ch101_reg.h"

#define SX1508_DEVICE_ID "sx1508q"

// Device tree RP4
//
// ch101@45 {
// compatible = "invensense,ch101";
// reg = <0x45>;
// pinctrl-0 = <&chirp_int_pin>;
// rst-gpios = <&gpio 22 GPIO_ACTIVE_HIGH>;
// prg-gpios = <&gpio 27 GPIO_ACTIVE_HIGH>;
// int-gpios = <&gpio 23 GPIO_ACTIVE_HIGH>;
// interrupts = <23 1>;
// interrupt-parent = <&gpio>;
// };
//
// Device tree RB5
//&qupv3_se1_i2c {
//	status = "ok";
//	qcom,clk-freq-out = <400000>;
//	#address-cells = <1>;
//	#size-cells = <0>;
//
//	/* TDK Chirp IO Expander */
//	ch_io_expander@22 {
//		#gpio-cells = <2>;
//		#interrupt-cells = <2>;
//		compatible = "semtech,sx1508q";
//		reg = <0x22>;
//		gpio-controller;
//	};
//};
//
//&qupv3_se4_i2c {
//	#address-cells = <1>;
//	#size-cells = <0>;
//	status = "ok";
//	/* TDK Chirp 0, 1, and 2 are connected to QUP4 */
//	qcom,clk-freq-out = <400000>;
//	ch101_0: ch101_0@45 {
//		compatible = "invensense,ch101";
//		reg = <0x45>;
//		prg-gpios = <0 1 2>;
//		rst-gpios = <&tlmm 140 GPIO_ACTIVE_HIGH>;
//		rtc_rst-gpios = <&tlmm 0 GPIO_ACTIVE_HIGH>;
//		int-gpios = <&tlmm 129 GPIO_ACTIVE_HIGH>,
//			    <&tlmm 138 GPIO_ACTIVE_HIGH>,
//			    <&tlmm 113 GPIO_ACTIVE_HIGH>;
//		interrupt-parent = <&tlmm>;
//		interrupts = <129 1>,
//			     <141 1>,
//			     <113 1>;
//		interrupt-names = "ch101_0_irq";
//		pinctrl-names = "default";
//		pinctrl-0 = <&ch101_rst &ch101_tmr_rst &ch101_0_irq>;
//	};
//};
//
//&qupv3_se15_i2c {
//	#address-cells = <1>;
//	#size-cells = <0>;
//	status = "ok";
//	/* TDK Chirp 3, 4, and 5 are connected to QUP15 */
//	qcom,clk-freq-out = <400000>;
//	ch101_1: ch101_1@45 {
//		compatible = "invensense,ch101";
//		reg = <0x45>;
//		prg-gpios = <3 4 5>;
//		rst-gpios = <&tlmm 140 GPIO_ACTIVE_HIGH>;
//		rtc_rst-gpios = <&tlmm 0 GPIO_ACTIVE_HIGH>;
//		int-gpios = <&tlmm 122 GPIO_ACTIVE_HIGH>,
//			    <&tlmm 123 GPIO_ACTIVE_HIGH>,
//			    <&tlmm 66 GPIO_ACTIVE_HIGH>;
//		interrupt-parent = <&tlmm>;
//		interrupts = <122 1>,
//			     <123 1>,
//			     <66 1>;
//		interrupt-names = "ch101_1_irq";
//		pinctrl-names = "default";
//		pinctrl-0 = <&ch101_rst &ch101_tmr_rst &ch101_1_irq>;
//	};


static const struct regmap_config ch101_i2c_regmap_config = {
	.reg_bits = 8, .val_bits = 8,
};

static int read_reg(void *client, u16 i2c_addr, u8 reg, u16 length, u8 *data)
{
	int res;
	struct i2c_msg msg[] = {
	{ .addr = i2c_addr, .flags = 0, .len = 1, .buf = &reg },
	{ .addr = i2c_addr, .flags = I2C_M_RD,
		.len = length, .buf = (__u8 *)data }
	};

	struct i2c_client *i2c_client = (struct i2c_client *)client;
	struct i2c_adapter *i2c_adapter = i2c_client->adapter;

//	pr_info(TAG "%s: name: %s, addr: %02x, flags: %x\n", __func__,
//		i2c_client->name, i2c_client->addr, i2c_client->flags);
//	pr_info(TAG "%s: addr: %02x, reg: %02x, len: %d\n", __func__,
//		i2c_addr, reg, length);

	res = i2c_transfer(i2c_adapter, msg, 2);

//	pr_info(TAG "%s: RES: %d\n", __func__, res);

//	{
//	int i;
//	pr_info(TAG "Read Values: ");
//	for (i = 0; i < (length < 3 ? length : 3); i++)
//		pr_info(TAG " %02x ", *(u8 *)(data + i));
//	pr_info(TAG "\n");
//	}

	if (res <= 0) {
		if (res == 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}
}

static int write_reg(void *client, u16 i2c_addr, u8 reg, u16 length, u8 *data)
{
	int res;
	u8 buf[4];
	struct i2c_msg msg[] = {
	{ .addr = i2c_addr, .flags = I2C_M_STOP,
		.len = length + 1, .buf = (__u8 *)buf }
	};

	struct i2c_client *i2c_client = (struct i2c_client *)client;
	struct i2c_adapter *i2c_adapter = i2c_client->adapter;

//	pr_info(TAG "%s: name: %s, addr: %02x, flags: %x\n", __func__,
//		i2c_client->name, i2c_client->addr, i2c_client->flags);
//	pr_info(TAG "%s: addr: %02x, reg: %02x, len: %d\n", __func__,
//		i2c_addr, reg, length);

	if (length > sizeof(buf))
		return  -EIO;

	// prepend the 'register address' to the buffer
	buf[0] = reg;
	memcpy(&(buf[1]), data, length);

//	{
//	int i;
//	pr_info(TAG "Write Values: ");
//	for (i = 0; i < sizeof(buf); i++)
//		pr_info(TAG " %02x ", *(u8 *)(buf + i));
//	pr_info(TAG "\n");
//	}

	res = i2c_transfer(i2c_adapter, msg, 1);

//	pr_info(TAG "%s: RES: %d\n", __func__, res);

	if (res <= 0) {
		if (res == 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}
}

static int read_sync(void *client, u16 i2c_addr, u16 length, u8 *data)
{
	int res;
	struct i2c_msg msg[] = {
	{ .addr = i2c_addr, .flags = I2C_M_STOP | I2C_M_RD,
		.len = length, .buf = (__u8 *)data }
	};

	struct i2c_client *i2c_client = (struct i2c_client *)client;
	struct i2c_adapter *i2c_adapter = i2c_client->adapter;

//	pr_info(TAG "%s: name: %s, addr: %02x, flags: %x\n", __func__,
//		i2c_client->name, i2c_client->addr, i2c_client->flags);
//	pr_info(TAG "%s: addr: %02x, len: %d\n", __func__,
//		i2c_addr, length);

	res = i2c_transfer(i2c_adapter, msg, 1);

//	pr_info(TAG "%s: RES: %d\n", __func__, res);

	if (res <= 0) {
		if (res == 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}
}

static int write_sync(void *client, u16 i2c_addr, u16 length, u8 *data)
{
	int res;
	struct i2c_msg msg[] = {
	{ .addr = i2c_addr, .flags = I2C_M_STOP,
		.len = length, .buf = (__u8 *)data }
	};

	struct i2c_client *i2c_client = (struct i2c_client *)client;
	struct i2c_adapter *i2c_adapter = i2c_client->adapter;

//	pr_info(TAG "%s: name: %s, addr: %02x, flags: %x\n", __func__,
//		i2c_client->name, i2c_client->addr, i2c_client->flags);
//	pr_info(TAG "%s: addr: %02x, len: %d\n", __func__,
//		i2c_addr, length);

	res = i2c_transfer(i2c_adapter, msg, 1);

//	pr_info(TAG "%s: RES: %d\n", __func__, res);

	if (res <= 0) {
		if (res == 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}
}

static int match_gpio_chip_by_label(struct gpio_chip *chip, void *data)
{
	return !strcmp(chip->label, data);
}

static struct gpio_chip *get_gpio_exp(void)
{
	struct gpio_chip *chip;

	chip = gpiochip_find(SX1508_DEVICE_ID,
				match_gpio_chip_by_label);

//	pr_info(TAG "%s: name: %s gpio: %p\n", __func__,SX1508_DEVICE_ID,chip);

	return chip;
}

static void set_pin_level(ioport_pin_t pin, int value)
{
	struct gpio_chip *gpio = get_gpio_exp();

	if (!gpio)
		return;

//	pr_info(TAG "%s: pin: %d, value: %d\n", __func__, pin, value);

	gpio->set(gpio, pin, value);
}

static void set_pin_dir(ioport_pin_t pin, enum ioport_direction dir)
{
	struct gpio_chip *gpio = get_gpio_exp();

	if (!gpio)
		return;

//	pr_info(TAG "%s: pin: %d, dir: %d\n", __func__, pin, dir);

	if (dir == IOPORT_DIR_INPUT)
		gpio->direction_input(gpio, pin);
	else
		gpio->direction_output(gpio, pin, 0);
}

static int ch101_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct device *dev;
	struct gpio_chip *gpio;
	int irq;
	struct irq_desc *desc;
	struct ch101_callbacks *cbk;
	int ret = 0;

	if (!client)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA |
			I2C_FUNC_SMBUS_I2C_BLOCK)) {
		return -EPERM;
	}

	dev = &client->dev;

	cbk = devm_kmalloc(dev, sizeof(struct ch101_callbacks), GFP_KERNEL);
	if (!cbk)
		return -ENOMEM;

	irq = client->irq;
	desc = irq_to_desc(irq);

	dev_info(dev, "%s: Start: %p, %s\n", __func__,
		dev, dev ? dev->init_name : "");

	dev_info(dev, "%s: IRQ: %d\n", __func__, irq);

	gpio = get_gpio_exp();
	if (!gpio) {
		dev_err(dev, "Error initializing expander: %s\n",
			SX1508_DEVICE_ID);
		return -ENODEV;
	}

	cbk->read_reg = read_reg;
	cbk->write_reg = write_reg;
	cbk->read_sync = read_sync;
	cbk->write_sync = write_sync;
	cbk->set_pin_level = set_pin_level;
	cbk->set_pin_dir = set_pin_dir;

	regmap = devm_regmap_init_i2c(client, &ch101_i2c_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(dev, "Error initializing i2c regmap: %ld\n",
				PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	pr_info(TAG "%s: client: %p\n", __func__, client);
	pr_info(TAG "%s: adapter: %p\n", __func__, client->adapter);


	ret = ch101_core_probe(client, regmap, cbk, id ? id->name : NULL);

	dev_info(dev, "%s: End\n", __func__);

	return ret;
}

static int ch101_i2c_remove(struct i2c_client *client)
{
	struct device *dev;
	int irq;
	struct irq_desc *desc;
	int ret = 0;

	dev = &client->dev;
	irq = client->irq;
	desc = irq_to_desc(irq);

	dev_info(dev, "%s: Start: %p, %s\n", __func__,
		dev, dev ? dev->init_name : "");

	dev_info(dev, "%s: IRQ: %d, %s\n", __func__,
		irq, ((desc && desc->action) ? desc->action->name : ""));

	ret = ch101_core_remove(client);

	dev_info(dev, "%s: End: %p, %s\n", __func__,
		dev, dev ? dev->init_name : "");

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int ch101_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct ch101_data *data = iio_priv(indio_dev);
	int ret;

	dev_info(dev, "%s: Start\n", __func__);

	mutex_lock(&data->lock);
	ret = regmap_write(data->regmap, CH_PROG_REG_CPU, 0x11);
	mutex_unlock(&data->lock);

	dev_info(dev, "%s: End\n", __func__);

	return ret;
}

static int ch101_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct ch101_data *data = iio_priv(indio_dev);
	int ret;

	dev_info(dev, "%s: Start\n", __func__);

	mutex_lock(&data->lock);
	ret = regmap_write(data->regmap, CH_PROG_REG_CPU, 0x02);
	mutex_unlock(&data->lock);

	dev_info(dev, "%s: End\n", __func__);

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops ch101_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ch101_suspend, ch101_resume)
};

static const struct i2c_device_id ch101_i2c_id[] = {
{ "ch101", 0 },
{ },
};

MODULE_DEVICE_TABLE(i2c, ch101_i2c_id);

static const struct of_device_id ch101_of_match[] = {
{ .compatible = "invensense,ch101" },
{ },
};

MODULE_DEVICE_TABLE(of, ch101_of_match);

static struct i2c_driver ch101_i2c_driver = {
	.driver = {
		.name = "ch101_i2c",
		.of_match_table = ch101_of_match,
		.pm = &ch101_pm_ops,
	},
	.probe = ch101_i2c_probe,
	.remove = ch101_i2c_remove,
	.id_table = ch101_i2c_id,
};

module_i2c_driver(ch101_i2c_driver);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense CH101 I2C device driver");
MODULE_LICENSE("GPL");
