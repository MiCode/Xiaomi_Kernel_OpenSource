/*
 * Copyright (C) 2018 HUAQIN Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#include <linux/platform_data/sgm37603a_bl.h>
#include <linux/platform_data/ti_reg_02.h>
#include <linux/platform_data/ti_reg_03.h>
#include "ktd_reg_04.h"
#include "ktd_reg_05.h"
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
	LCD_BL_PRINT("[LCD][BL] lcd_bl_i2c_client is null!!\n");
	return -EINVAL;
    }
    ret = i2c_master_send(lcd_bl_i2c_client, write_data, 2);

    if (ret < 0)
	LCD_BL_PRINT("[LCD][BL] i2c write data fail !!\n");

    return ret;
}

static int lcd_bl_read_byte(u8 regnum)
{
	u8 buffer[1], reg_value[1];
	int res = 0;

	if (NULL == lcd_bl_i2c_client) {
		LCD_BL_PRINT("[LCD][BL] lcd_bl_i2c_client is null!!\n");
		return -EINVAL;
	}

	mutex_lock(&read_lock);

	buffer[0] = regnum;
	res = i2c_master_send(lcd_bl_i2c_client, buffer, 0x1);
	if (res <= 0)	{
	  mutex_unlock(&read_lock);
	  LCD_BL_PRINT("read reg send res = %d\n", res);
	  return res;
	}
	res = i2c_master_recv(lcd_bl_i2c_client, reg_value, 0x1);
	if (res <= 0) {
	  mutex_unlock(&read_lock);
	  LCD_BL_PRINT("read reg recv res = %d\n", res);
	  return res;
	}
	mutex_unlock(&read_lock);

	return reg_value[0];
}

void lcd_bl_dump_reg(void)
{
	LCD_BL_PRINT("[LCD][BL] dump led enable reg:0x10 val:0x%x\n", lcd_bl_read_byte(0x10));
	LCD_BL_PRINT("[LCD][BL] dump mode reg:0x11 val:0x%x\n", lcd_bl_read_byte(0x11));
	LCD_BL_PRINT("[LCD][BL] dump reg:0x1F val:0x%x\n", lcd_bl_read_byte(0x1F));
}

static bool main_bl = false;
static int __init get_main_bl(char *str)
{

	if((strstr(str, "ft8719")!= NULL))
	    main_bl = true;

	return 0;
}

early_param("msm_drm.dsi_display0", get_main_bl);

int lcd_bl_set_mode(enum bl_mode mode)
{
    int value;

	value = lcd_bl_read_byte(BL_CONTROL_MODE_ADDRESS);
	LCD_BL_PRINT("[LCD][BL]bl mode value +++ = 0x%x\n", value);

	switch (mode) {
	case BL_BRIGHTNESS_REGISTER:
		LCD_BL_PRINT("[LCD][BL] BL_BRIGHTNESS_REGISTER mode\n");
		value = value&(~BL_CONTROL_MODE_BIT6)&(~BL_CONTROL_MODE_BIT5);
		lcd_bl_write_byte(BL_CONTROL_MODE_ADDRESS, value);
		break;

	case BL_PWM_DUTY_CYCLE:
		LCD_BL_PRINT("[LCD][BL] BL_PWM_DUTY_CYCLE mode\n");
		//lcd_bl_write_byte(BL_CONTROL_MODE_ADDRESS, reg_value);
		break;

	case BL_REGISTER_PWM_COMBINED:
		LCD_BL_PRINT("[LCD][BL] BL_REGISTER_PWM_COMBINED mode\n");
		//lcd_bl_write_byte(BL_CONTROL_MODE_ADDRESS, reg_value);
		break;

	default:
		LCD_BL_PRINT("[LCD][BL] unknown mode\n");
		break;
	}

	value = lcd_bl_read_byte(0x11);
	LCD_BL_PRINT("[LCD][BL]bl mode value --- = 0x%x\n", value);

    return 0;
}

EXPORT_SYMBOL(lcd_bl_set_mode);

int lcd_bl_set_led_enable(void)//for led1 led2 on
{
    int value;

	value = lcd_bl_read_byte(BL_LED_ENABLE_ADDRESS);
	LCD_BL_PRINT("[LCD][BL]bl led enable value +++ = 0x%x\n", value);

	value = (value&(~BL_LED_ENABLE_BIT3))|BL_LED_ENABLE_BIT2|BL_LED_ENABLE_BIT1;
	lcd_bl_write_byte(BL_LED_ENABLE_ADDRESS, value);

	value = lcd_bl_read_byte(BL_LED_ENABLE_ADDRESS);
	LCD_BL_PRINT("[LCD][BL]bl led enable value --- = 0x%x\n", value);

    return 0;
}

EXPORT_SYMBOL(lcd_bl_set_led_enable);

int lcd_bl_set_led_brightness(int value)//for led1 led2 on
{
	int sgm_value = 0;
	if (value < 0) {
		printk("%d %s chenwenmin invalid value=%d\n", __LINE__, __func__, value);
		return 0;
	}
	if(main_bl){
		if (value > 0) {
			sgm_value = value * 22 / 25;
			lcd_bl_write_byte(0x1A, ktd3137_brightness_table_reg4[sgm_value]);// lsb
			lcd_bl_write_byte(0x19, ktd3137_brightness_table_reg5[sgm_value]);// msb
		} else {
			lcd_bl_write_byte(0x1A, 0x00);// lsb
			lcd_bl_write_byte(0x19, 0x00);// msb
		}
	} else {
		if (value > 0 && value <= 20) {
			sgm_value = value - 4;
			lcd_bl_write_byte(0x18, sgm_value & 0x07);// lsb
			lcd_bl_write_byte(0x19, (sgm_value >> 3) & 0xFF);// msb
		} else if (value > 20 && value < 4096) {
			sgm_value = value * 11 / 25;
			lcd_bl_write_byte(0x18, ti_brightness_table_reg2[sgm_value]);// lsb
			lcd_bl_write_byte(0x19, ti_brightness_table_reg3[sgm_value]);// msb
		} else {
			lcd_bl_write_byte(0x18, 0x00);// lsb
			lcd_bl_write_byte(0x19, 0x00);// msb
		}
	}

	return 0;
}

EXPORT_SYMBOL(lcd_bl_set_led_brightness);

int sgm_hbm_set(enum backlight_hbm_mode hbm_mode)
{

	//ktd_hbm_mode = hbm_mode;

	switch (hbm_mode) {
	case HBM_MODE_DEFAULT://21.8mA
		lcd_bl_write_byte(0x1A, 3570 & 0xFF);// lsb
		lcd_bl_write_byte(0x19, (3570 >> 4) & 0xFF);// msb
		pr_err("[bkl] hyperThis is hbm mode 1\n");
		break;
	case HBM_MODE_LEVEL1://23.5mA
		lcd_bl_write_byte(0x1A, 3849 & 0xFF);// lsb
		lcd_bl_write_byte(0x19, (3849 >> 4) & 0xFF);// msb
		pr_err("[bkl] hyperThis is hbm mode 2\n");
		break;
	case HBM_MODE_LEVEL2://25mA
		lcd_bl_write_byte(0x1A, 4095 & 0xFF);// lsb
		lcd_bl_write_byte(0x19, (4095 >> 4) & 0xFF);// msb
		pr_err("[bkl] hyperThis is hbm mode 3\n");
		break;
	default:
		pr_err("hyper This isn't hbm mode\n");
		break;
	 }
	pr_err("[bkl] hyper %s hbm_mode=%d\n", __func__, hbm_mode);
	return 0;
}

int lm_hbm_set(enum backlight_hbm_mode hbm_mode)
{

	//ktd_hbm_mode = hbm_mode;

	switch (hbm_mode) {
	case HBM_MODE_DEFAULT://21.8mA
		lcd_bl_write_byte(0x18, 1801 & 0x07);// lsb
		lcd_bl_write_byte(0x19, (1801 >> 3) & 0xFF);// msb
		pr_err("[bkl] hyper This is lm hbm mode 1\n");
		break;
	case HBM_MODE_LEVEL1://23.5mA
		lcd_bl_write_byte(0x18, 1924 & 0xFF);// lsb
		lcd_bl_write_byte(0x19, (1924 >> 3) & 0xFF);// msb
		pr_err("[bkl] hyperThis is lm hbm mode 2\n");
		break;
	case HBM_MODE_LEVEL2://25mA
		lcd_bl_write_byte(0x18, 2047 & 0xFF);// lsb
		lcd_bl_write_byte(0x19, (2047 >> 3) & 0xFF);// msb
		pr_err("[bkl] hyperThis is lm hbm mode 3\n");
		break;
	default:
		pr_err("hyper This isn't hbm mode\n");
		break;
	 }
	pr_err("[bkl] hyper %s lm hbm_mode=%d\n", __func__, hbm_mode);
	return 0;
}

int dsi_hbm_set(enum backlight_hbm_mode hbm_mode)
{

	if(main_bl)
		sgm_hbm_set(hbm_mode);
	else
		lm_hbm_set(hbm_mode);

	return 0;
}

EXPORT_SYMBOL(dsi_hbm_set);

#ifdef CONFIG_OF
static const struct of_device_id i2c_of_match[] = {
    { .compatible = "qcom,sgm37603a_bl", },
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
      LCD_BL_PRINT("[LCD][BL] i2c_client is NULL\n");
	  return -EINVAL;
	}

	lcd_bl_i2c_client = client;

	if(main_bl){
		pr_err("hyper backlight ic is sgm37603 !\n");
		ret = lcd_bl_write_byte(0x10, 0x07);
		//ret = lcd_bl_write_byte(0x11, 0x45);
	}else{
		pr_err("hyper backlight ic is ti_lm36823 !\n");
		ret = lcd_bl_write_byte(0x10, 0x07);
		//ret = lcd_bl_write_byte(0x11, 0xF5);
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
    if (i2c_add_driver(&lcd_bl_i2c_driver)) {
	LCD_BL_PRINT("[LCD][BL] Failed to register lcd_bl_i2c_driver!\n");
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

MODULE_AUTHOR("<zuoqiquan@huaqin.com>");
MODULE_DESCRIPTION("QCOM LCD BL I2C Driver");
MODULE_LICENSE("GPL");

