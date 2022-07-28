// SPDX-License-Identifier: GPL-2.0
/*
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>

#include "ktz8866.h"

#define BL_I2C_ADDRESS			  0x11

#define LCD_BL_I2C_ID_NAME "lcd_bl"

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_client *lcd_bl_i2c_client;
static DEFINE_MUTEX(read_lock);
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int lcd_bl_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lcd_bl_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Extern Area
 *****************************************************************************/

static int lcd_bl_write_byte(unsigned char addr, unsigned char value)
{
    int ret = 0;
    unsigned char write_data[2] = {0};

    write_data[0] = addr;
    write_data[1] = value;

    if (NULL == lcd_bl_i2c_client) {
	pr_debug("[LCD][BL] lcd_bl_i2c_client is null!!\n");
	return -EINVAL;
    }
    ret = i2c_master_send(lcd_bl_i2c_client, write_data, 2);

    if (ret < 0)
	pr_debug("[LCD][BL] i2c write data fail !!\n");

    return ret;
}

static int lcd_bl_read_byte(u8 regnum)
{
	u8 buffer[1], reg_value[1];
	int res = 0;

	if (NULL == lcd_bl_i2c_client) {
		pr_debug("[LCD][BL] lcd_bl_i2c_client is null!!\n");
		return -EINVAL;
	}

	mutex_lock(&read_lock);

	buffer[0] = regnum;
	res = i2c_master_send(lcd_bl_i2c_client, buffer, 0x1);
	if (res <= 0)	{
	  mutex_unlock(&read_lock);
	  pr_debug("read reg send res = %d\n", res);
	  return res;
	}
	res = i2c_master_recv(lcd_bl_i2c_client, reg_value, 0x1);
	if (res <= 0) {
	  mutex_unlock(&read_lock);
	  pr_debug("read reg recv res = %d\n", res);
	  return res;
	}
	mutex_unlock(&read_lock);

	return reg_value[0];
}

int lcd_bl_set_led_brightness(int value)//for set bringhtness
{
	pr_debug("%s:hyper bl = %d\n", __func__, value);

	if (value < 0) {
		pr_debug("%d %s --wlc invalid value=%d\n", __LINE__, __func__, value);
		return 0;
	}

	if (value > 0) {
		//lcd_bl_write_byte(0x08, 0x5F); /* BL enabled and Current sink 1/2/3/4 /5 enabled；*/
		lcd_bl_write_byte(0x04, value & 0x07);// lsb
		lcd_bl_write_byte(0x05, (value >> 3) & 0xFF);// msb
	} else {
		lcd_bl_write_byte(0x04, 0x00);// lsb
		lcd_bl_write_byte(0x05, 0x00);// msb
		//lcd_bl_write_byte(0x08, 0x00); /* BL disabled and Current sink 1/2/3/4 /5 enabled；*/
	}

	return 0;
}

EXPORT_SYMBOL(lcd_bl_set_led_brightness);

int lcd_set_bias(int enable)
{
	pr_debug("--wlc, enter lcd_disable_bias function,value = %d", enable);
	if (enable) {
		lcd_bl_write_byte(0x09, 0x9C);/* enable OUTP */
	    mdelay(5);
		lcd_bl_write_byte(0x09, 0x9E);/* enable OUTN */
	} else {
		lcd_bl_write_byte(0x09, 0x9C);/* Disable OUTN */
		mdelay(5);
		lcd_bl_write_byte(0x09, 0x98);/* Disable OUTP */
	}
	return 0;
}

EXPORT_SYMBOL(lcd_set_bias);
int lcd_set_bl_bias_reg(struct device *pdev, int enable)
{
	struct device *dev = pdev;
	struct gpio_desc *hw_led_en;
	int res = 0;

	if (enable) {
		hw_led_en = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
		if (IS_ERR(hw_led_en))
			pr_debug("could not get pm-enable gpio\n");

		gpiod_set_value(hw_led_en, 1);
		devm_gpiod_put(dev, hw_led_en);
		usleep_range(125, 130);

		//write vsp/vsn reg
		lcd_bl_write_byte(0x0C, 0x30); /* LCD_BOOST_CFG */
		lcd_bl_write_byte(0x0D, 0x28); /* OUTP_CFG，OUTP = 6.0V */
		lcd_bl_write_byte(0x0E, 0x28); /* OUTN_CFG，OUTN = -6.0V */

		lcd_bl_write_byte(0x09, 0x9C); /* enable OUTP */
		mdelay(5); /* delay 5ms */
		lcd_bl_write_byte(0x09, 0x9E); /* enable OUTN */

		//write backlight reg
		/* BL_CFG1；OVP=34V，线性调光，PWM Disabled */
		lcd_bl_write_byte(0x02, 0X3B);
		/* BL_OPTION2；电感4.7uH，BL_CURRENT_LIMIT 2.5A；*/
		lcd_bl_write_byte(0x11, 0x37);
		 /* Backlight Full-scale LED Current 22.8mA/CH；*/
		lcd_bl_write_byte(0x15, 0xB0);
		/* BL enabled and Current sink 1/2/3/4 /5 enabled；*/
		lcd_bl_write_byte(0x08, 0x5F);

	} else {
		hw_led_en = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
		if (IS_ERR(hw_led_en))
			pr_debug("could not get pm-enable gpio\n");

		lcd_bl_write_byte(0x09, 0x9C);/* Disable OUTN */
		mdelay(5);
		lcd_bl_write_byte(0x09, 0x98);/* Disable OUTP */
		/* BL disabled and Current sink 1/2/3/4 /5 enabled；*/
		lcd_bl_write_byte(0x08, 0x00);

		gpiod_set_value(hw_led_en, 0);
		devm_gpiod_put(dev, hw_led_en);


	}
	res = lcd_bl_read_byte(0x0f);
	pr_debug("%s:ktz8866 0x0f = 0x%x\n", __func__, res);

	return 0;
}
EXPORT_SYMBOL(lcd_set_bl_bias_reg);
#ifdef CONFIG_OF
static const struct of_device_id i2c_of_match[] = {
    { .compatible = "ktz,ktz8866", },
    {},
};
#endif

static const struct i2c_device_id lcd_bl_i2c_id[] = {
    {LCD_BL_I2C_ID_NAME, 0},
    {},
};

static struct i2c_driver lcd_bl_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
    .id_table = lcd_bl_i2c_id,
    .probe = lcd_bl_i2c_probe,
    .remove = lcd_bl_i2c_remove,
    .driver = {
	.owner = THIS_MODULE,
	.name = LCD_BL_I2C_ID_NAME,
#ifdef CONFIG_OF
	.of_match_table = i2c_of_match,
#endif
    },
};

static int lcd_bl_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	if (NULL == client) {
      pr_debug("[LCD][BL] i2c_client is NULL\n");
	  return -EINVAL;
	}

	lcd_bl_i2c_client = client;

	pr_debug("--wlc, i2c led\n");
	pr_debug("--wlc, i2c address: %0x\n", client->addr);

	//write vsp/vsn reg
	ret = lcd_bl_write_byte(0x0C, 0x30); /* LCD_BOOST_CFG */
	ret = lcd_bl_write_byte(0x0D, 0x28); /* OUTP_CFG，OUTP = 6.0V */
	ret = lcd_bl_write_byte(0x0E, 0x28); /* OUTN_CFG，OUTN = -6.0V */

	ret = lcd_bl_write_byte(0x09, 0x9C); /* enable OUTP */
	mdelay(5); /* delay 5ms */
	ret = lcd_bl_write_byte(0x09, 0x9E); /* enable OUTN */

	//write backlight reg
	ret = lcd_bl_write_byte(0x02, 0X3B); /* BL_CFG1；OVP=34V，线性调光，PWM Disabled */
	ret = lcd_bl_write_byte(0x11, 0x37); /* BL_OPTION2；电感4.7uH，BL_CURRENT_LIMIT 2.5A；*/
	ret = lcd_bl_write_byte(0x15, 0xB0); /* Backlight Full-scale LED Current 22.8mA/CH；*/
	ret = lcd_bl_write_byte(0x08, 0x5F); /* BL enabled and Current sink 1/2/3/4 /5 enabled；*/

	if (ret < 0) {
		pr_debug("--wlc,[%s]:I2C write reg is fail!", __func__);
		return -EINVAL;
	} else {
		pr_debug("--wlc,[%s]:I2C write reg is success!", __func__);
	}

    return 0;
}

static int lcd_bl_i2c_remove(struct i2c_client *client)
{
    lcd_bl_i2c_client = NULL;
    i2c_unregister_device(client);

    return 0;
}

static int __init lcd_bl_init(void)
{
	pr_debug("lcd_bl_init\n");

	if (i2c_add_driver(&lcd_bl_i2c_driver)) {
		pr_debug("[LCD][BL] Failed to register lcd_bl_i2c_driver!\n");
		return -EINVAL;
	}

    return 0;
}

static void __exit lcd_bl_exit(void)
{
    i2c_del_driver(&lcd_bl_i2c_driver);
}

module_init(lcd_bl_init);
module_exit(lcd_bl_exit);

MODULE_AUTHOR("wulongchao <wulongchao@huanqin.com>");
MODULE_DESCRIPTION("Mediatek LCD BL I2C Driver");
MODULE_LICENSE("GPL");



