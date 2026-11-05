// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * leds-aw21024.c   aw21024 led module
 *
 * Copyright (c) 2020 Shanghai Awinic Technology Co., Ltd. All Rights Reserved
 *
 *  Author: Awinic
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/leds.h>
#include "leds-awrgb21024.h"
#include "leds-awrgb21024-reg.h"

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW21024_I2C_NAME "aw21024_led"
#define AW21024_DRIVER_VERSION "V1.2.0"
#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 1
#define AW_READ_CHIPID_RETRIES 2
#define AW_READ_CHIPID_RETRY_DELAY 1
/******************************************************
 *
 * led effect
 *
 ******************************************************/
#define AW21024_CFG_NAME_MAX		64

struct aw21024_cfg aw21024_cfg_array[] = {
	{aw21024_cfg_led_off, sizeof(aw21024_cfg_led_off)},
	{aw21024_all_leds_on, sizeof(aw21024_all_leds_on)},
	{aw21024_red_leds_on, sizeof(aw21024_red_leds_on)},
	{aw21024_green_leds_on, sizeof(aw21024_green_leds_on)},
	{aw21024_blue_leds_on, sizeof(aw21024_blue_leds_on)},
	{aw21024_breath_leds_on, sizeof(aw21024_breath_leds_on)}
};

struct aw21024_cfg aw21024e_effect_array[] = {
	{aw21024_cfg_led_off, sizeof(aw21024_cfg_led_off)},
	{aw21024e_leds_always_on, sizeof(aw21024e_leds_always_on)},
	{aw21024_breath_leds_on, sizeof(aw21024_breath_leds_on)}
};

/******************************************************
 *
 * aw21024 i2c write/read
 *
 ******************************************************/

static int
aw21024_i2c_write(struct aw21024 *aw21024, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw21024->i2c, reg_addr, reg_data);
		if (ret < 0)
			pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
		else
			break;

		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int
aw21024_i2c_read(struct aw21024 *aw21024, unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw21024->i2c, reg_addr);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw21024_i2c_write_bits(struct aw21024 *aw21024,
				unsigned char reg_addr, unsigned int mask,
				unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw21024_i2c_read(aw21024, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	aw21024_i2c_write(aw21024, reg_addr, reg_val);

	return 0;
}

/*****************************************************
 *
 * aw21024 led effect cfg
 *
 *****************************************************/
static void aw21024_update_cfg_array(struct aw21024 *aw21024,
				     unsigned char *p_cfg_data,
				     unsigned int cfg_size)
{
	unsigned int i = 0;

	for (i = 0; i < cfg_size; i += 2)
		aw21024_i2c_write(aw21024, p_cfg_data[i], p_cfg_data[i + 1]);
}

static int aw21024_cfg_update_array(struct aw21024 *aw21024)
{
	aw21024_update_cfg_array(aw21024, aw21024_cfg_array[aw21024->effect].p,
				 aw21024_cfg_array[aw21024->effect].count);
	return 0;
}

/*****************************************************
 *
 * aw21024 led init
 *
 *****************************************************/
static int aw21024_current_conversion(struct aw21024 *aw21024, unsigned int current_data)
{
	if (aw21024->cdev.max_brightness == 255) {
		aw21024->conversion_led_current = current_data;
	} else if (aw21024->cdev.max_brightness == 1023) {
		if ((current_data >= 1) && (current_data <= 4)) {
			aw21024->conversion_led_current = 1;
		} else {
			aw21024->conversion_led_current = (current_data * 255)
						/ aw21024->cdev.max_brightness;
		}
	} else {
		if ((current_data >= 1) && (current_data <= 8)) {
			aw21024->conversion_led_current = 1;
		} else {
			aw21024->conversion_led_current = (current_data * 255)
						/ aw21024->cdev.max_brightness;
		}
	}
	return 0;
}

static int aw21024_brightness_conversion(struct aw21024 *aw21024, unsigned int brightness_data)
{
	if (aw21024->cdev.max_brightness == 255) {
		aw21024->cdev.brightness = brightness_data;
	} else if (aw21024->cdev.max_brightness == 1023) {
		if ((brightness_data >= 1) && (brightness_data <= 4)) {
			aw21024->cdev.brightness = 1;
		} else {
			aw21024->cdev.brightness = (brightness_data * 255)
						/ aw21024->cdev.max_brightness;
		}
	} else {
		if ((brightness_data >= 1) && (brightness_data <= 8)) {
			aw21024->cdev.brightness = 1;
		} else {
			aw21024->cdev.brightness = (brightness_data * 255)
						/ aw21024->cdev.max_brightness;
		}
	}
	return 0;
}

static int aw21024_chip_enable(struct aw21024 *aw21024, bool flag)
{
	if (flag) {
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CHIP_EN_CLOSE_MASK,
				       AW21024_BIT_CHIP_EN);
	} else {
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CHIP_EN_CLOSE_MASK,
				       AW21024_BIT_CHIP_CLOSE);
	}
	return 0;
}

static int aw21024_pwm_set(struct aw21024 *aw21024, unsigned int mode)
{
	switch (mode) {
	case AW21024_CLKPRQ_16MH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_16MH);
		break;
	case AW21024_CLKPRQ_8MH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_8MH);
		break;
	case AW21024_CLKPRQ_1MH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_1MH);
		break;
	case AW21024_CLKPRQ_512KH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_512KH);
		break;
	case AW21024_CLKPRQ_256KH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_256KH);
		break;
	case AW21024_CLKPRQ_125KH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_125KH);
		break;
	case AW21024_CLKPRQ_62KH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_62KH);
		break;
	case AW21024_CLKPRQ_31KH:
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_CLKPRQ_MASK,
				       AW21024_BIT_CLKPRQ_31KH);
		break;
	default:
		break;
	}
	return 0;
}

static int aw21024_apse_set(struct aw21024 *aw21024, bool mode)
{
	if (mode) {
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_APSE_MASK,
				       AW21024_BIT_APSE_ENABLE);
	} else {
		aw21024_i2c_write_bits(aw21024,
				       AW21024_REG_GCR,
				       AW21024_BIT_APSE_MASK,
				       AW21024_BIT_APSE_DISABLE);
	}

	return 0;
}

static int aw21024_brres_set(struct aw21024* aw21024, unsigned int mode)
{
	switch (mode) {
	case AW21024E_BRRES_8BITS:
		aw21024_i2c_write_bits(aw21024,
			AW21024_REG_GCR,
			AW21024_BIT_BRRES_MASK,
			AW21024_BIT_BRRES_8BITS);
		break;
	case AW21024E_BRRES_9BITS:
		aw21024_i2c_write_bits(aw21024,
			AW21024_REG_GCR,
			AW21024_BIT_BRRES_MASK,
			AW21024_BIT_BRRES_9BITS);
		break;
	case AW21024E_BRRES_12BITS:
		aw21024_i2c_write_bits(aw21024,
			AW21024_REG_GCR,
			AW21024_BIT_BRRES_MASK,
			AW21024_BIT_BRRES_12BITS);
		break;
	case AW21024E_BRRES_16BITS:
		aw21024_i2c_write_bits(aw21024,
			AW21024_REG_GCR,
			AW21024_BIT_BRRES_MASK,
			AW21024_BIT_BRRES_16BITS);
		break;
	default:
		break;
	}
	return 0;
}

static int aw21024_br_update(struct aw21024 *aw21024)
{
	aw21024_i2c_write(aw21024, AW21024_REG_UPDATE, 0x00);
	return 0;
}

static int aw21024_led_init(struct aw21024 *aw21024)
{
	aw21024_chip_enable(aw21024, true);
	usleep_range(200, 300);
	aw21024_pwm_set(aw21024, aw21024->clk_pwm);
	aw21024_apse_set(aw21024, aw21024->apse_mode);
	aw21024_brres_set(aw21024, AW21024E_BRRES_8BITS);
	pr_info("before aw21024_current_conversion aw21024->dts_led_current is %d\n",
						aw21024->dts_led_current);
	aw21024_current_conversion(aw21024, aw21024->dts_led_current);
	pr_info("after aw21024_current_conversion aw21024->conversion_led_current is %d\n",
					aw21024->conversion_led_current);
	aw21024_i2c_write(aw21024, AW21024_REG_GCCR, aw21024->conversion_led_current);
	aw21024_i2c_write(aw21024, AW21024_REG_WBR, aw21024->dts_led_wbr);
	aw21024_i2c_write(aw21024, AW21024_REG_WBG, aw21024->dts_led_wbg);
	aw21024_i2c_write(aw21024, AW21024_REG_WBB, aw21024->dts_led_wbb);

	aw21024->run = false;
	aw21024->color.r = aw21024e_leds_always_on[LED_COLR_VALUE_INDEX];
	aw21024->color.g = aw21024e_leds_always_on[LED_COLG_VALUE_INDEX];
	aw21024->color.b = aw21024e_leds_always_on[LED_COLB_VALUE_INDEX];
	aw21024->color.timer[0] = 0x0f;
	aw21024->color.timer[1] = 0x0f;
	aw21024->color.timer[2] = 0x0f;
	aw21024->color.timer[3] = 0x0f;

	return 0;
}

/*****************************************************
 *
 * aw21024 led hw reset
 *
 *****************************************************/
static int aw21024_hw_reset(struct aw21024 *aw21024)
{
	if (aw21024 && gpio_is_valid(aw21024->reset_gpio)) {
		gpio_set_value_cansleep(aw21024->reset_gpio, 0);
		usleep_range(2000, 4000);
		gpio_set_value_cansleep(aw21024->reset_gpio, 1);
		usleep_range(2000, 4000);
	} else {
		dev_err(aw21024->dev, "%s:  failed\n", __func__);
	}
	aw21024_led_init(aw21024);
	return 0;
}

static int aw21024_hw_off(struct aw21024 *aw21024)
{
	if (aw21024 && gpio_is_valid(aw21024->reset_gpio)) {
		gpio_set_value_cansleep(aw21024->reset_gpio, 0);
		usleep_range(200, 400);
	} else {
		dev_err(aw21024->dev, "%s:  failed\n", __func__);
	}
	return 0;
}

/*****************************************************
 *
 * aw21024 led brightness
 *
 *****************************************************/
static void aw21024_brightness_work(struct work_struct *work)
{
	struct aw21024 *aw21024 = container_of(work, struct aw21024, brightness_work);

	if (aw21024->cdev.brightness > 0) {
		pr_info("aw21024->cdev.brightness high 0\n");
		if (aw21024->cdev.brightness > aw21024->cdev.max_brightness)
			aw21024->cdev.brightness = aw21024->cdev.max_brightness;

		pr_info("before aw21024_brightness_conversion aw21024->cdev.brightness is %d\n",
						aw21024->cdev.brightness);
		aw21024_brightness_conversion(aw21024, aw21024->cdev.brightness);
		aw21024_current_conversion(aw21024, aw21024->cdev.brightness);
		pr_info("after aw21024_brightness_conversion aw21024->cdev.brightness is %d\n",
						aw21024->cdev.brightness);
		aw21024_i2c_write(aw21024, AW21024_REG_GCCR,
						aw21024->cdev.brightness);
	} else {
		pr_info("aw21024->cdev.brightness low 0\n");
		aw21024_i2c_write(aw21024, AW21024_REG_GCCR, 0);
	}
}

static void aw21024_set_brightness(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct aw21024 *aw21024 = container_of(cdev, struct aw21024, cdev);

	aw21024->cdev.brightness = brightness;

	schedule_work(&aw21024->brightness_work);
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw21024_read_chip_id(struct aw21024 *aw21024)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg_val = 0;

	/* hardware reset */
	aw21024_hw_reset(aw21024);
	usleep_range(200, 400);

	while (cnt++ < AW_READ_CHIPID_RETRIES) {
		ret = aw21024_i2c_read(aw21024, AW21024_REG_RESET, &reg_val);
		if (ret < 0) {
			dev_err(aw21024->dev,
				"%s: failed to read AW21024_CHIP_ID : %d\n",
				__func__, ret);
		} else {
			if (reg_val == AW21024_CHIP_ID) {
				pr_info("AW21024_CHIP_ID is : 0x%x\n", reg_val);
				return 0;
			}
		}
		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}
	pr_info("This chipid is not AW21024 and reg_data is : 0x%x\n", reg_val);

	return -EINVAL;
}

static int aw21024_read_chip_version(struct aw21024 *aw21024)
{
	int ret = -1;
	unsigned char reg_val = 0;

	ret = aw21024_i2c_read(aw21024, AW21024_REG_VER, &reg_val);
	if (ret < 0)
		dev_err(aw21024->dev, "%s: failed to read VERSION : %d\n", __func__, ret);
	else
		pr_info("THE CHIP_VERSION: 0x%x\n", reg_val);

	return ret;
}

/******************************************************
 *
 * sys group attribute
 *
 ******************************************************/

static ssize_t
reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw21024 *aw21024 = container_of(led_cdev, struct aw21024, cdev);

	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw21024_i2c_write(aw21024, databuf[0], databuf[1]);
	return count;
}

static ssize_t reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw21024 *aw21024 = container_of(led_cdev, struct aw21024, cdev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW21024_REG_MAX; i++) {
		if (!(aw21024_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw21024_i2c_read(aw21024, i, &reg_val);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x\n",
			     i, reg_val);
	}
	return len;
}

static ssize_t
hwen_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw21024 *aw21024 = container_of(led_cdev, struct aw21024, cdev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val == 1)
		aw21024_hw_reset(aw21024);
	else
		aw21024_hw_off(aw21024);

	return count;
}

static ssize_t
hwen_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw21024 *aw21024 = container_of(led_cdev, struct aw21024, cdev);
	ssize_t len = 0;

	len +=
	snprintf(buf + len, PAGE_SIZE - len, "hwen=%d\n", gpio_get_value(aw21024->reset_gpio));

	return len;
}

static ssize_t
rgbcolor_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[2] = { 0, 0 };
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw21024 *aw21024 = container_of(led_cdev, struct aw21024, cdev);


	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		/* GEn 0-7=0 */
		pr_info("%s: enter rgbcolor!!! databuf[0]=0x%x ,databuf[1]=0x%x\n", __func__,databuf[0],databuf[1]);
		aw21024_i2c_write(aw21024, AW21024_REG_GCFG0, 0x00);
		/* GEn disable */
		aw21024_i2c_write(aw21024, AW21024_REG_GCFG1, 0x10);
		/* RGBMD=1 3=1 */
		aw21024_i2c_write(aw21024, AW21024_REG_GCR2, 0x01);
		/* brightness default */
		aw21024_i2c_write(aw21024, AW21024_REG_GCCR, aw21024->conversion_led_current);//aw21024->conversion_led_current
		/* before trim */
		aw21024_i2c_write(aw21024, AW21024_REG_BR0_L + databuf[0] * 2, 0xff);
		/* after trim */
		/*aw21024_i2c_write(aw21024, AW21024_REG_BR0_L + databuf[0], 0xff);*/

		aw21024->rgbcolor = (databuf[1] & 0x00ff0000) >> 16;
		aw21024_i2c_write(aw21024, AW21024_REG_COL0 + databuf[0] * 3, aw21024->rgbcolor);
		aw21024->rgbcolor = (databuf[1] & 0x0000ff00) >> 8;
		aw21024_i2c_write(aw21024, AW21024_REG_COL0 + databuf[0] * 3 + 1,
				  aw21024->rgbcolor);
		aw21024->rgbcolor = (databuf[1] & 0x000000ff);
		aw21024_i2c_write(aw21024, AW21024_REG_COL0 + databuf[0] * 3 + 2,
				  aw21024->rgbcolor);
		aw21024_br_update(aw21024);
	}
	return len;
}

static ssize_t
color_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len)
{
	struct led_classdev* led_cdev = dev_get_drvdata(dev);
	struct aw21024* aw21024 = container_of(led_cdev, struct aw21024, cdev);
	unsigned int val = 0;
	int rc = 0;
	rc = sscanf(buf, "%x", &val);
	pr_info("%s: color_store!!! %d\n", __func__,val);
	if (rc < 0)
		return rc;

	if (val < 0x1000000) {
		aw21024->color.r = (val & 0xff0000) >> 16;
		aw21024->color.g = (val & 0x00ff00) >> 8;
		aw21024->color.b = (val & 0x0000ff);
		if (aw21024->run) {
			aw21024_i2c_write(aw21024, AW21024_REG_GCOLR, aw21024->color.r);
			aw21024_i2c_write(aw21024, AW21024_REG_GCOLG, aw21024->color.g);
			aw21024_i2c_write(aw21024, AW21024_REG_GCOLB, aw21024->color.b);
		}
	}
	else {
		pr_info("%s: invalid rgb color info!!!\n", __func__);
	}
	return len;
}

static ssize_t
period_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len)
{
	struct led_classdev* led_cdev = dev_get_drvdata(dev);
	struct aw21024* aw21024 = container_of(led_cdev, struct aw21024, cdev);
	int i = 0, j = 0;
	unsigned int offset = 0;
	unsigned int timerbuf[4] = { 0, 0, 0, 0 };

	if (sscanf(buf, "%d %d %d %d", &timerbuf[0], &timerbuf[1],
		&timerbuf[2], &timerbuf[3]) == 4) {
		pr_info("%s: period_store!!! %d %d %d %d\n", __func__,timerbuf[0],timerbuf[1],timerbuf[2],timerbuf[3]);
		for (i = 0; i < 4; i++) {
			for (j = 2; j < sizeof(breath_timerms_map_reg) / sizeof(breath_timerms_map_reg[0]); j += 2) {
				if (timerbuf[i] == 0) {
					aw21024->color.timer[i] = timerbuf[i];
				}
				else if (timerbuf[i] > 0 && timerbuf[i] <= breath_timerms_map_reg[2]) {
					aw21024->color.timer[i] = breath_timerms_map_reg[3];
				}
				else if ((timerbuf[i] >= breath_timerms_map_reg[j - 2]) &&
					(timerbuf[i] <= breath_timerms_map_reg[j])) {
					offset = (breath_timerms_map_reg[j] + breath_timerms_map_reg[j - 2]) / 2;
					aw21024->color.timer[i] = (timerbuf[i] < offset) ?
						breath_timerms_map_reg[j - 1] : breath_timerms_map_reg[j + 1];
				}
			}
		}
		if (aw21024->run) {
			aw21024_i2c_write(aw21024, AW21024_REG_PATT0, (aw21024->color.timer[0] & 0x0f) << 4 | aw21024->color.timer[1]);
			aw21024_i2c_write(aw21024, AW21024_REG_PATT1, (aw21024->color.timer[2] & 0x0f) << 4 | aw21024->color.timer[3]);
		}
	}
	else {
		pr_info("%s: invalid rgb color timer info!!!\n", __func__);
	}
	return len;
}

static ssize_t
run_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len)
{
	struct led_classdev* led_cdev = dev_get_drvdata(dev);
	struct aw21024* aw21024 = container_of(led_cdev, struct aw21024, cdev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	pr_info("%s: run_store!!! %d\n", __func__,val);
	if (rc < 0)
		return rc;

	if (val < (sizeof(aw21024e_effect_array) / sizeof(struct aw21024_cfg))) {
		switch (val) {
		case 1:
			aw21024e_leds_always_on[LED_CURRENT_INDEX] = aw21024->conversion_led_current;
			aw21024e_leds_always_on[LED_COLR_VALUE_INDEX] = aw21024->color.r;
			aw21024e_leds_always_on[LED_COLR_VALUE_INDEX] = aw21024->color.g;
			aw21024e_leds_always_on[LED_COLR_VALUE_INDEX] = aw21024->color.b;
			break;
		case 2:
			aw21024e_leds_breath_on[BREATH_CURRENT_INDEX] = aw21024->conversion_led_current;
			aw21024e_leds_breath_on[BREATH_TIMER_RISE_ON_INDEX] =
				(aw21024->color.timer[0] & 0x0f) << 4 | aw21024->color.timer[1];
			aw21024e_leds_breath_on[BREATH_TIMER_FALL_OFF_INDEX] =
				(aw21024->color.timer[2] & 0x0f) << 4 | aw21024->color.timer[3];
			aw21024e_leds_breath_on[BREATH_COLR_VALUE_INDEX] = aw21024->color.r;
			aw21024e_leds_breath_on[BREATH_COLG_VALUE_INDEX] = aw21024->color.g;
			aw21024e_leds_breath_on[BREATH_COLB_VALUE_INDEX] = aw21024->color.b;
			break;
		default:
			break;
		};
		aw21024_update_cfg_array(aw21024, aw21024e_effect_array[val].p,
			aw21024e_effect_array[val].count);
		aw21024->run = val > 0 ? true : false;
	}
	else {
		pr_info("%s: invalid rgb color run info!!!\n", __func__);
	}
	return len;
}

static ssize_t
repeat_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t len)
{
	struct led_classdev* led_cdev = dev_get_drvdata(dev);
	struct aw21024* aw21024 = container_of(led_cdev, struct aw21024, cdev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw21024e_leds_breath_on[BREATH_REPEAT_NUM_L] = (unsigned char)val;

	if (aw21024->run) {
		aw21024_i2c_write(aw21024, AW21024_REG_PATT3, aw21024e_leds_breath_on[BREATH_REPEAT_NUM_L] & 0xff);
	}
	return len;
}

static ssize_t
effect_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw21024 *aw21024 = container_of(led_cdev, struct aw21024, cdev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val < (sizeof(aw21024_cfg_array) / sizeof(struct aw21024_cfg))) {
		pr_info("%s: enter effect!!!\n", __func__);
		aw21024->effect = val;
		aw21024_cfg_update_array(aw21024);
	}

	return len;
}

static ssize_t
effect_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int i;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw21024 *aw21024 = container_of(led_cdev, struct aw21024, cdev);

	for (i = 0; i < sizeof(aw21024_cfg_array) / sizeof(struct aw21024_cfg); i++) {
		len +=
		snprintf(buf + len, PAGE_SIZE - len, "cfg[%x] = %ps\n", i, aw21024_cfg_array[i].p);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "current cfg = %ps\n",
		aw21024_cfg_array[aw21024->effect].p);

	return len;
}

static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(hwen);
static DEVICE_ATTR_WO(rgbcolor);
static DEVICE_ATTR_WO(color);
static DEVICE_ATTR_WO(period);
static DEVICE_ATTR_WO(run);
static DEVICE_ATTR_RW(effect);
static DEVICE_ATTR_WO(repeat);

static struct attribute *aw21024_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_hwen.attr,
	&dev_attr_rgbcolor.attr,
	&dev_attr_color.attr,
	&dev_attr_period.attr,
	&dev_attr_run.attr,
	&dev_attr_effect.attr,
	&dev_attr_repeat.attr,
	NULL
};

static struct attribute_group aw21024_attribute_group = {
	.attrs = aw21024_attributes
};

/******************************************************
 *
 * dts gpio
 *
 ******************************************************/

static int aw21024_parse_dts(struct device *dev, struct aw21024 *aw21024, struct device_node *np)
{
	int ret = 0;

	aw21024->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);

	ret = of_property_read_u32(np, "clk_pwm", &aw21024->clk_pwm);
	if (ret) {
		aw21024->clk_pwm = 1;
		dev_err(dev, "%s dts clk_pwm not found\n", __func__);
	} else {
		dev_info(dev, "%s read dts clk_pwm = %d\n", __func__, aw21024->clk_pwm);
	}
	aw21024->apse_mode = of_property_read_bool(np, "apse_mode");
	if (aw21024->apse_mode)
		dev_info(dev, "%s driver use apse_mode\n", __func__);
	else
		dev_info(dev, "%s driver use general_mode\n", __func__);

	ret = of_property_read_u32(np, "led_current", &aw21024->dts_led_current);
	if (ret) {
		aw21024->dts_led_current = 79;
		dev_err(dev, "%s dts led_current not found\n", __func__);
	} else {
		dev_info(dev, "%s read dts led_current = %d\n", __func__, aw21024->dts_led_current);
	}
	ret = of_property_read_u32(np, "led_wbr", &aw21024->dts_led_wbr);
	if (ret) {
		aw21024->dts_led_wbr = 255;
		dev_err(dev, "%s dts led_wbr not found\n", __func__);
	} else {
		dev_info(dev, "%s read dts led_wbr = %d\n", __func__, aw21024->dts_led_wbr);
	}
	ret = of_property_read_u32(np, "led_wbg", &aw21024->dts_led_wbg);
	if (ret) {
		aw21024->dts_led_wbg = 141;
		dev_err(dev, "%s dts led_wbg not found\n", __func__);
	} else {
		dev_info(dev, "%s read dts led_wbg = %d\n", __func__, aw21024->dts_led_wbg);
	}
	ret = of_property_read_u32(np, "led_wbb", &aw21024->dts_led_wbb);
	if (ret) {
		aw21024->dts_led_wbb = 122;
		dev_err(dev, "%s dts led_wbb not found\n", __func__);
	} else {
		dev_info(dev, "%s read dts led_wbb = %d\n", __func__, aw21024->dts_led_wbb);
	}
	ret = of_property_read_u32(np, "brightness", &aw21024->cdev.brightness);
	if (ret) {
		aw21024->cdev.brightness = 125;
		dev_err(dev, "%s dts brightness not found\n", __func__);
	} else {
		dev_info(dev, "%s read dts brightness = %d\n", __func__, aw21024->cdev.brightness);
	}
	ret = of_property_read_u32(np, "max_brightness", &aw21024->cdev.max_brightness);
	if (ret) {
		aw21024->cdev.max_brightness = 255;
		dev_err(dev, "%s dts max_brightness not found\n", __func__);
	} else {
		dev_info(dev, "%s read dts max_brightness = %d\n", __func__,
			aw21024->cdev.max_brightness);
	}

	return 0;
}

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw21024_i2c_probe(struct i2c_client *i2c)
{
	struct aw21024 *aw21024;
	struct device_node *np = i2c->dev.of_node;
	int ret;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -ENODEV;
	}

	aw21024 = devm_kzalloc(&i2c->dev, sizeof(struct aw21024), GFP_KERNEL);
	if (aw21024 == NULL)
		return -ENOMEM;

	aw21024->dev = &i2c->dev;
	aw21024->i2c = i2c;

	i2c_set_clientdata(i2c, aw21024);
	dev_set_drvdata(&i2c->dev, aw21024);

	/* get dts info */
	if (np) {
		ret = aw21024_parse_dts(&i2c->dev, aw21024, np);
		if (ret) {
			dev_err(&i2c->dev, "%s: failed to parse device tree node\n", __func__);
			goto err_parse_dts;
		}
	} else {
		aw21024->reset_gpio = -1;
	}

	if (gpio_is_valid(aw21024->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw21024->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "aw21024_rst");
		if (ret) {
			dev_err(&i2c->dev, "%s: rst request failed\n", __func__);
			goto err_gpio_request;
		}
	}

	/* aw21024 chip id */
	ret = aw21024_read_chip_id(aw21024);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: aw21024_read_chip_id failed ret=%d\n", __func__, ret);
		goto err_id;
	}

	ret = aw21024_read_chip_version(aw21024);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: aw21024_read_chip_version failed ret=%d\n", __func__, ret);
		goto err_id;
	}

	aw21024->cdev.name = "aw21024_led";
	INIT_WORK(&aw21024->brightness_work, aw21024_brightness_work);
	aw21024->cdev.brightness_set = aw21024_set_brightness;
	ret = led_classdev_register(aw21024->dev, &aw21024->cdev);
	if (ret) {
		dev_err(aw21024->dev, "unable to register led ret=%d\n", ret);
		goto err_class;
	}
	ret = sysfs_create_group(&aw21024->cdev.dev->kobj, &aw21024_attribute_group);
	if (ret) {
		dev_err(aw21024->dev, "led sysfs ret: %d\n", ret);
		goto err_sysfs;
	}
	pr_info("%s: probe successful!!!\n", __func__);

	return 0;
err_sysfs:
	led_classdev_unregister(&aw21024->cdev);
err_class:
err_id:
	//devm_gpio_free(&i2c->dev, aw21024->reset_gpio);
err_gpio_request:
err_parse_dts:
	devm_kfree(&i2c->dev, aw21024);
	aw21024 = NULL;
	return ret;
}

static void aw21024_i2c_remove(struct i2c_client *i2c)
{
	struct aw21024 *aw21024 = i2c_get_clientdata(i2c);

	sysfs_remove_group(&aw21024->cdev.dev->kobj, &aw21024_attribute_group);
	led_classdev_unregister(&aw21024->cdev);

	//if (gpio_is_valid(aw21024->reset_gpio))
		//devm_gpio_free(&i2c->dev, aw21024->reset_gpio);
	devm_kfree(&i2c->dev, aw21024);
	aw21024 = NULL;
}

static const struct i2c_device_id aw21024_i2c_id[] = {
	{AW21024_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw21024_i2c_id);

static const struct of_device_id aw21024_dt_match[] = {
	{.compatible = "awinic,aw21024_led"},
	{},
};

static struct i2c_driver aw21024_i2c_driver = {
	.driver = {
		   .name = AW21024_I2C_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(aw21024_dt_match),
		   },
	.probe = aw21024_i2c_probe,
	.remove = aw21024_i2c_remove,
	.id_table = aw21024_i2c_id,
};

static int __init aw21024_i2c_init(void)
{
	int ret = 0;

	pr_info("aw21024 driver version %s\n", AW21024_DRIVER_VERSION);
	ret = i2c_add_driver(&aw21024_i2c_driver);
	if (ret) {
		pr_err("fail to add aw21024 device into i2c\n");
		return ret;
	}
	return 0;
}

module_init(aw21024_i2c_init);

static void __exit aw21024_i2c_exit(void)
{
	i2c_del_driver(&aw21024_i2c_driver);
}

module_exit(aw21024_i2c_exit);

MODULE_DESCRIPTION("AW21024 LED Driver");
MODULE_LICENSE("GPL v2");