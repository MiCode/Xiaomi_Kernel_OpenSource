/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "gate_i2c.h"

#ifdef CONFIG_LEDS_MTK_PWM
#include <leds-mtk-pwm.h>
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#elif defined CONFIG_LEDS_MTK_I2C
#include <leds-mtk-i2c.h>
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#endif

/*****************************************************************************
 * Define
 *****************************************************************************/
#define GATE_I2C_ID_NAME "gate_ic_i2c_rt4831"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :[GATE][I2C] " fmt, __func__, __LINE__

/*****************************************************************************
 * Define Register
 *****************************************************************************/
#define BACKLIGHT_CONFIG_1		0x02
#define BACKLIGHT_CONFIG_2		0x03
#define BACKLIGHT_BRIGHTNESS_LSB	0x04
#define BACKLIGHT_BRIGHTNESS_MSB	0x05
#define BACKLIGHT_AUTO_FREQ_LOW		0x06
#define BACKLIGHT_AUTO_FREQ_HIGH	0x07
#define BACKLIGHT_ENABLE		0x08
#define FLAGS				0x0F
#define BACKLIGHT_OPTION_1		0x10
#define BACKLIGHT_OPTION_2		0x11
#define BACKLIGHT_SMOOTH		0x14

#define BRIGHTNESS_HIGN_OFFSET		0x3
#define BRIGHTNESS_HIGN_MASK		0xFF
#define BRIGHTNESS_LOW_OFFSET		0x0
#define BRIGHTNESS_LOW_MASK		0x7

#define DISPLAY_BIAS_CONFIGURATION_1	0x09
#define DISPLAY_BIAS_CONFIGURATION_2	0x0A
#define DISPLAY_BIAS_CONFIGURATION_3	0x0B
#define LCM_BIAS			0x0C
#define VPOS_BIAS			0x0D
#define VNEG_BIAS			0x0E

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static const struct of_device_id _gate_ic_i2c_of_match[] = {
	{
		.compatible = "mediatek,gate-ic-i2c",
	 },
};

static struct i2c_client *_gate_ic_i2c_client;

struct gate_ic_client {
	struct i2c_client *i2c_client;
	struct gpio_desc *pinctrl;
	struct device *dev;
	bool pwm_enable;
	atomic_t gate_ic_power_status;
	atomic_t backlight_status;
};

/*****************************************************************************
 * Driver Functions
 *****************************************************************************/
static void _gate_ic_backlight_enable(void)
{
	/*BL enable*/
	struct gate_ic_client *gate_client = i2c_get_clientdata(_gate_ic_i2c_client);

	pr_info("Backlight enable\n");

	if (gate_client->pwm_enable) {
		_gate_ic_i2c_write_bytes(BACKLIGHT_CONFIG_1, 0x6B);
	} else {
		_gate_ic_i2c_write_bytes(BACKLIGHT_CONFIG_1, 0x68);
		if (!atomic_read(&gate_client->backlight_status))
			_gate_ic_backlight_set(0);
	}
	_gate_ic_i2c_write_bytes(BACKLIGHT_CONFIG_2, 0x9D);
	_gate_ic_i2c_write_bytes(BACKLIGHT_OPTION_1, 0x06);
	_gate_ic_i2c_write_bytes(BACKLIGHT_OPTION_2, 0xB7);
	/*bias enable*/
	_gate_ic_i2c_write_bytes(BACKLIGHT_ENABLE, 0x15);
	_gate_ic_i2c_write_bytes(BACKLIGHT_SMOOTH, 0x03);
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int _backlight_changed_event(struct notifier_block *nb, unsigned long event,
	void *v)
{
	struct led_conf_info *led_conf;
	struct gate_ic_client *gate_client = i2c_get_clientdata(_gate_ic_i2c_client);

	led_conf = (struct led_conf_info *)v;

	switch (event) {
	case 1:
		if (led_conf->cdev.brightness > 0)
			atomic_set(&gate_client->backlight_status, 1);
		else
			atomic_set(&gate_client->backlight_status, 0);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block leds_init_notifier = {
	.notifier_call = _backlight_changed_event,
};
#endif

/*****************************************************************************
 * Extern Area
 *****************************************************************************/

void _gate_ic_backlight_set(unsigned int hw_level)
{
	int level_l, level_h;
	struct i2c_client *client = _gate_ic_i2c_client;
	char cmd_buf[3] = { 0x00, 0x00, 0x00 };
	int ret = 0;

	level_h = (hw_level >> BRIGHTNESS_HIGN_OFFSET) & BRIGHTNESS_HIGN_MASK;
	level_l = (hw_level >> BRIGHTNESS_LOW_OFFSET) & BRIGHTNESS_LOW_MASK;

	cmd_buf[0] = BACKLIGHT_BRIGHTNESS_LSB;
	cmd_buf[1] = level_l;
	cmd_buf[2] = level_h;

	ret = i2c_master_send(client, cmd_buf, 3);
	if (ret < 0)
		pr_info("ERROR %d!! i2c write data fail 0x%0x, 0x%0x, 0x%0x !!\n",
				ret, cmd_buf[0], cmd_buf[1], cmd_buf[2]);
}
EXPORT_SYMBOL_GPL(_gate_ic_backlight_set);

int _gate_ic_i2c_read_bytes(unsigned char addr, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;
	struct i2c_client *client = _gate_ic_i2c_client;

	if (client == NULL) {
		pr_info("ERROR!! _gate_ic_i2c_client is null\n");
		return 0;
	}

	cmd_buf[0] = addr;
	ret = i2c_master_send(client, &cmd_buf[0], 1);
	ret = i2c_master_recv(client, &cmd_buf[1], 1);
	if (ret < 0)
		pr_info("ERROR %d!! i2c read data 0x%0x fail !!\n", ret, addr);

	readData = cmd_buf[1];
	*returnData = readData;

	return ret;
}
EXPORT_SYMBOL_GPL(_gate_ic_i2c_read_bytes);

int _gate_ic_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _gate_ic_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_info("ERROR!! _gate_ic_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("ERROR %d!! i2c write data fail 0x%0x, 0x%0x !!\n",
				ret, addr, value);

	return ret;
}
EXPORT_SYMBOL_GPL(_gate_ic_i2c_write_bytes);

void _gate_ic_Power_on(void)
{
	struct gate_ic_client *gate_client = i2c_get_clientdata(_gate_ic_i2c_client);

	pr_info("Status: %d, %d\n",
		atomic_read(&gate_client->gate_ic_power_status),
		atomic_read(&gate_client->backlight_status));

	if (IS_ERR(gate_client->pinctrl)) {
		pr_info("ERROR!! pinctrl is error!\n");
	} else if (!atomic_read(&gate_client->gate_ic_power_status)) {
		gpiod_set_value(gate_client->pinctrl, 1);
		devm_gpiod_put(gate_client->dev, gate_client->pinctrl);

		atomic_set(&gate_client->gate_ic_power_status, 1);
		_gate_ic_backlight_enable();
	}
}
EXPORT_SYMBOL_GPL(_gate_ic_Power_on);

void _gate_ic_Power_off(void)
{
	struct gate_ic_client *gate_client = i2c_get_clientdata(_gate_ic_i2c_client);

	pr_info("Status: %d, %d\n",
		atomic_read(&gate_client->gate_ic_power_status),
		atomic_read(&gate_client->backlight_status));

	if (IS_ERR(gate_client->pinctrl)) {
		pr_info("ERROR!! pinctrl is error!\n");
	} else if (atomic_read(&gate_client->gate_ic_power_status) &&
			!atomic_read(&gate_client->backlight_status)) {
		gpiod_set_value(gate_client->pinctrl, 0);
		devm_gpiod_put(gate_client->dev, gate_client->pinctrl);

		atomic_set(&gate_client->gate_ic_power_status, 0);
	}
}
EXPORT_SYMBOL_GPL(_gate_ic_Power_off);

void _gate_ic_i2c_panel_bias_enable(unsigned int power_status)
{

	pr_info("Panel bias enable\n");

	if (power_status) {
		_gate_ic_i2c_write_bytes(DISPLAY_BIAS_CONFIGURATION_2, 0x11);
		_gate_ic_i2c_write_bytes(DISPLAY_BIAS_CONFIGURATION_3, 0x00);
		/*set bias to 5.4v*/
		_gate_ic_i2c_write_bytes(LCM_BIAS, 0x24);
		_gate_ic_i2c_write_bytes(VPOS_BIAS, 0x1c);
		_gate_ic_i2c_write_bytes(VNEG_BIAS, 0x1c);
		/* set dsv FPWM mode */
		_gate_ic_i2c_write_bytes(0xF0, 0x69);
		_gate_ic_i2c_write_bytes(0xB1, 0x6c);
		/*bias enable*/
		_gate_ic_i2c_write_bytes(DISPLAY_BIAS_CONFIGURATION_1, 0x9e);
	} else {
		_gate_ic_i2c_write_bytes(DISPLAY_BIAS_CONFIGURATION_1, 0x18);
	}
}
EXPORT_SYMBOL_GPL(_gate_ic_i2c_panel_bias_enable);

/*****************************************************************************
 * Function
 *****************************************************************************/

static int _gate_ic_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct gate_ic_client *gate_client;
	struct device *dev = &client->dev;
	int status;
	int pwm_enable;

	pr_info("%s NT: info==>name=%s addr=0x%x\n",
		__func__, client->name, client->addr);

	gate_client = devm_kzalloc(&client->dev, sizeof(struct gate_ic_client), GFP_KERNEL);

	if (!gate_client)
		return -ENOMEM;

	gate_client->dev = &client->dev;
	gate_client->i2c_client = client;
	gate_client->pinctrl = devm_gpiod_get(&client->dev, "gate-power",
				   GPIOD_OUT_HIGH);
	if (IS_ERR(gate_client->pinctrl)) {
		status = PTR_ERR(gate_client->pinctrl);
		pr_info("ERROR!! Failed to enable gpio: %d\n", status);
		return status;
	}
	i2c_set_clientdata(client, gate_client);
	_gate_ic_i2c_client = client;
	atomic_set(&gate_client->gate_ic_power_status, 1);
	atomic_set(&gate_client->gate_ic_power_status, 1);

	status = of_property_read_u32(dev->of_node, "pwm-enable", &pwm_enable);
	if (status) {
		gate_client->pwm_enable = 0;
		pr_info("ERROR!! Failed to get pwm-enable, use default value 0");
	} else {
		gate_client->pwm_enable = pwm_enable;
	}
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	mtk_leds_register_notifier(&leds_init_notifier);
#endif

	pr_info("Probe success! pwm-enable is %d", pwm_enable);

	return 0;
}

static int _gate_ic_i2c_remove(struct i2c_client *client)
{
	struct gate_ic_client *gate_client;

	pr_info("Gate ic remove\n");
	gate_client = i2c_get_clientdata(client);

	i2c_unregister_device(client);

	kfree(gate_client);
	gate_client = NULL;
	_gate_ic_i2c_client = NULL;
	i2c_unregister_device(client);
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	mtk_leds_unregister_notifier(&leds_init_notifier);
#endif
	return 0;
}

/*****************************************************************************
 * Data Structure
 *****************************************************************************/

static const struct i2c_device_id _gate_ic_i2c_id[] = {
	{GATE_I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver _gate_ic_i2c_driver = {
	.id_table = _gate_ic_i2c_id,
	.probe = _gate_ic_i2c_probe,
	.remove = _gate_ic_i2c_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = GATE_I2C_ID_NAME,
		   .of_match_table = _gate_ic_i2c_of_match,
		   },
};

module_i2c_driver(_gate_ic_i2c_driver);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK RT4831 I2C Driver");
MODULE_LICENSE("GPL");


