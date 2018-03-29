/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * drivers/leds/leds-mt65xx.c
 *
 * mt65xx leds driver
 *
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
/* #include <linux/leds-mt65xx.h> */
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include <mach/mt_pwm.h> */
/* #include <mach/mt_pwm_hal.h> */
/* #include <mach/mt_gpio.h> */
/*
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h> */
/* #include <mach/mt_pmic_feature_api.h> */
/* #include <mach/mt_boot.h> */
#include <leds_hal.h>
#include "leds_drv.h"
#include <mt-plat/mt_pwm.h>

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <asm-generic/gpio.h>
#endif
/****************************************************************************
 * variables
 ***************************************************************************/
struct cust_mt65xx_led *bl_setting = NULL;
/* static unsigned int bl_brightness = 102;
static unsigned int bl_duty = 21;
static unsigned int bl_div = CLK_DIV1;
static unsigned int bl_frequency = 32000; */
static unsigned int bl_div = CLK_DIV1;
#define PWM_DIV_NUM 8
static unsigned int div_array[PWM_DIV_NUM];
struct mt65xx_led_data *g_leds_data[MT65XX_LED_TYPE_TOTAL];

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
static unsigned int last_level1 = 102;
static struct i2c_client *g_client;
static int I2C_SET_FOR_BACKLIGHT  = 350;
#endif
/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/
static int debug_enable_led = 1;
/* #define pr_fmt(fmt) "[LED_DRV]"fmt */
#define LEDS_DRV_DEBUG(format, args...) do { \
	if (debug_enable_led) {	\
		pr_debug(format, ##args);\
	} \
} while (0)

/****************************************************************************
 * function prototypes
 ***************************************************************************/
#ifndef CONTROL_BL_TEMPERATURE
#define CONTROL_BL_TEMPERATURE
#endif

#define MT_LED_INTERNAL_LEVEL_BIT_CNT 10

/******************************************************************************
   for DISP backlight High resolution
******************************************************************************/
#ifdef LED_INCREASE_LED_LEVEL_MTKPATCH
#define LED_INTERNAL_LEVEL_BIT_CNT 10
#endif

static int mt65xx_led_set_cust(struct cust_mt65xx_led *cust, int level);

/****************************************************************************
 * add API for temperature control
 ***************************************************************************/

#ifdef CONTROL_BL_TEMPERATURE

/* define int limit for brightness limitation */
static unsigned int limit = 255;
static unsigned int limit_flag;
static unsigned int last_level;
static unsigned int current_level;
static DEFINE_MUTEX(bl_level_limit_mutex);

/****************************************************************************
 * external functions for display
 * this API add for control the power and temperature,
 * if enabe=1, the value of brightness will smaller than max_level,
 * whatever lightservice transfers to driver.
 ***************************************************************************/
int setMaxbrightness(int max_level, int enable)
{
#if !defined(CONFIG_MTK_AAL_SUPPORT)
	struct cust_mt65xx_led *cust_led_list = mt_get_cust_led_list();

	mutex_lock(&bl_level_limit_mutex);
	if (1 == enable) {
		limit_flag = 1;
		limit = max_level;
		mutex_unlock(&bl_level_limit_mutex);
		/* if (limit < last_level){ */
		if (0 != current_level) {
			if (limit < last_level) {
				LEDS_DRV_DEBUG
				    ("mt65xx_leds_set_cust in setMaxbrightness:value control start! limit=%d\n",
				     limit);
				mt65xx_led_set_cust(&cust_led_list
						    [MT65XX_LED_TYPE_LCD],
						    limit);
			} else {
				mt65xx_led_set_cust(&cust_led_list
						    [MT65XX_LED_TYPE_LCD],
						    last_level);
			}
		}
	} else {
		limit_flag = 0;
		limit = 255;
		mutex_unlock(&bl_level_limit_mutex);

		if (0 != current_level) {
			LEDS_DRV_DEBUG("control temperature close:limit=%d\n",
				       limit);
			mt65xx_led_set_cust(&cust_led_list[MT65XX_LED_TYPE_LCD],
					    last_level);

		}
	}
#else
	LEDS_DRV_DEBUG("setMaxbrightness go through AAL\n");
	disp_bls_set_max_backlight(((((1 << LED_INTERNAL_LEVEL_BIT_CNT) -
				      1) * max_level + 127) / 255));
#endif				/* endif CONFIG_MTK_AAL_SUPPORT */
	return 0;
}
EXPORT_SYMBOL(setMaxbrightness);
#endif
/****************************************************************************
 * internal functions
 ***************************************************************************/
static void get_div_array(void)
{
	int i = 0;
	unsigned int *temp = mt_get_div_array();

	while (i < PWM_DIV_NUM) {
		div_array[i] = *temp++;
		LEDS_DRV_DEBUG("get_div_array: div_array=%d\n", div_array[i]);
		i++;
	}
}

static int led_set_pwm(int pwm_num, struct nled_setting *led)
{

	mt_led_set_pwm(pwm_num, led);
	return 0;
}

static int brightness_set_pmic(enum mt65xx_led_pmic pmic_type, u32 level,
			       u32 div)
{
	mt_brightness_set_pmic(pmic_type, level, div);
	return -1;

}

static int mt65xx_led_set_cust(struct cust_mt65xx_led *cust, int level)
{
#ifdef CONTROL_BL_TEMPERATURE
	mutex_lock(&bl_level_limit_mutex);
	current_level = level;
	/* LEDS_DRV_DEBUG("brightness_set_cust:current_level=%d\n", current_level); */
	if (0 == limit_flag) {
		last_level = level;
		/* LEDS_DRV_DEBUG("brightness_set_cust:last_level=%d\n", last_level); */
	} else {
		if (limit < current_level) {
			level = limit;
			/* LEDS_DRV_DEBUG("backlight_set_cust: control level=%d\n", level); */
		}
	}
	mutex_unlock(&bl_level_limit_mutex);
#endif
#ifdef LED_INCREASE_LED_LEVEL_MTKPATCH
	if (MT65XX_LED_MODE_CUST_BLS_PWM == cust->mode) {
		mt_mt65xx_led_set_cust(cust,
				       ((((1 << LED_INTERNAL_LEVEL_BIT_CNT) -
					  1) * level + 127) / 255));
	} else {
		mt_mt65xx_led_set_cust(cust, level);
	}
#else
	mt_mt65xx_led_set_cust(cust, level);
#endif
	return -1;
}

static void mt65xx_led_set(struct led_classdev *led_cdev,
			   enum led_brightness level)
{
	struct mt65xx_led_data *led_data =
	    container_of(led_cdev, struct mt65xx_led_data, cdev);
#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	bool flag = FALSE;
	int value = 0;
	int retval;
	struct device_node *node = NULL;
	struct i2c_client *client = g_client;
	value = i2c_smbus_read_byte_data(g_client, 0x10);
	LEDS_DRV_DEBUG("LEDS:mt65xx_led_set:0x10 = %d\n", value);

	node = of_find_compatible_node(NULL, NULL,
						    "mediatek,lcd-backlight");
	if (node) {
		I2C_SET_FOR_BACKLIGHT = of_get_named_gpio(node, "gpios", 0);
		LEDS_DRV_DEBUG("Led_i2c gpio num for power:%d\n", I2C_SET_FOR_BACKLIGHT);
	}
#endif
	if (strcmp(led_data->cust.name, "lcd-backlight") == 0) {
#ifdef CONTROL_BL_TEMPERATURE
		mutex_lock(&bl_level_limit_mutex);
		current_level = level;
		/* LEDS_DRV_DEBUG("brightness_set_cust:current_level=%d\n", current_level); */
		if (0 == limit_flag) {
			last_level = level;
			/* LEDS_DRV_DEBUG("brightness_set_cust:last_level=%d\n", last_level); */
		} else {
			if (limit < current_level) {
				level = limit;
				LEDS_DRV_DEBUG
				    ("backlight_set_cust: control level=%d\n",
				     level);
			}
		}
		mutex_unlock(&bl_level_limit_mutex);
#endif
	}
#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	retval = gpio_request(I2C_SET_FOR_BACKLIGHT, "i2c_set_for_backlight");
	if (retval)
		LEDS_DRV_DEBUG("LEDS: request I2C gpio149 failed\n");

	if (strcmp(led_data->cust.name, "lcd-backlight") == 0) {
		if (level == 0) {
			LEDS_DRV_DEBUG("LEDS:mt65xx_led_set:close the power\n");
			i2c_smbus_write_byte_data(client, 0x00, 0);
			gpio_direction_output(I2C_SET_FOR_BACKLIGHT, 0);
		}
		if (!last_level1 && level) {
			LEDS_DRV_DEBUG("LEDS:mt65xx_led_set:open the power\n");
			gpio_direction_output(I2C_SET_FOR_BACKLIGHT, 1);
			mdelay(100);
			i2c_smbus_write_byte_data(client, 0x10, 4);
			flag = TRUE;
		}
		last_level1 = level;
	}
	gpio_free(I2C_SET_FOR_BACKLIGHT);
#endif
	mt_mt65xx_led_set(led_cdev, level);
#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	if (strcmp(led_data->cust.name, "lcd-backlight") == 0) {
		if (flag) {
			i2c_smbus_write_byte_data(client, 0x14, 0xdf);
			i2c_smbus_write_byte_data(client, 0x04, 0xff);
			i2c_smbus_write_byte_data(client, 0x00, 1);
		}
	}
#endif
}

static int mt65xx_blink_set(struct led_classdev *led_cdev,
			    unsigned long *delay_on, unsigned long *delay_off)
{
	if (mt_mt65xx_blink_set(led_cdev, delay_on, delay_off))
		return -1;
	else
		return 0;
}

/****************************************************************************
 * external functions for display
 ***************************************************************************/
int mt65xx_leds_brightness_set(enum mt65xx_led_type type,
			       enum led_brightness level)
{
	int val;
	struct cust_mt65xx_led *cust_led_list = mt_get_cust_led_list();
#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	bool flag = FALSE;
	int value = 0;
	int retval;
	struct device_node *node = NULL;
	struct i2c_client *client = g_client;

	value = i2c_smbus_read_byte_data(g_client, 0x10);
	LEDS_DRV_DEBUG("LEDS:mt65xx_led_set:0x10 = %d\n", value);

	node = of_find_compatible_node(NULL, NULL,
						    "mediatek,lcd-backlight");
	if (node) {
		I2C_SET_FOR_BACKLIGHT = of_get_named_gpio(node, "gpios", 0);
		LEDS_DRV_DEBUG("Led_i2c gpio num for power:%d\n", I2C_SET_FOR_BACKLIGHT);
	}
#endif

	LEDS_DRV_DEBUG("#%d:%d\n", type, level);

	if (type < 0 || type >= MT65XX_LED_TYPE_TOTAL)
		return -1;

	if (level > LED_FULL)
		level = LED_FULL;
	else if (level < 0)
		level = 0;

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	retval = gpio_request(I2C_SET_FOR_BACKLIGHT, "i2c_set_for_backlight");
	if (retval)
		LEDS_DRV_DEBUG("LEDS: request I2C gpio149 failed\n");

	if (strcmp(cust_led_list[type].name, "lcd-backlight") == 0) {
		if (level == 0) {

			i2c_smbus_write_byte_data(client, 0x00, 0);
			gpio_direction_output(I2C_SET_FOR_BACKLIGHT, 0);
		}
		if (!last_level1 && level) {

			gpio_direction_output(I2C_SET_FOR_BACKLIGHT, 1);
			mdelay(100);
			i2c_smbus_write_byte_data(client, 0x10, 4);
			flag = TRUE;
		}
		last_level1 = level;
	}
	gpio_free(I2C_SET_FOR_BACKLIGHT);
#endif

	val = mt65xx_led_set_cust(&cust_led_list[type], level);
#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	if (strcmp(cust_led_list[type].name, "lcd-backlight") == 0) {
		if (flag) {
			i2c_smbus_write_byte_data(client, 0x14, 0xdf);
			i2c_smbus_write_byte_data(client, 0x04, 0xff);
			i2c_smbus_write_byte_data(client, 0x00, 1);
		}
	}
#endif
	return val;
}
EXPORT_SYMBOL(mt65xx_leds_brightness_set);

/****************************************************************************
 * external functions for AAL
 ***************************************************************************/
int backlight_brightness_set(int level)
{
	struct cust_mt65xx_led *cust_led_list = mt_get_cust_led_list();

	if (level > ((1 << MT_LED_INTERNAL_LEVEL_BIT_CNT) - 1))
		level = ((1 << MT_LED_INTERNAL_LEVEL_BIT_CNT) - 1);
	else if (level < 0)
		level = 0;

	if (MT65XX_LED_MODE_CUST_BLS_PWM ==
	    cust_led_list[MT65XX_LED_TYPE_LCD].mode) {
#ifdef CONTROL_BL_TEMPERATURE
		mutex_lock(&bl_level_limit_mutex);
		current_level = (level >> (MT_LED_INTERNAL_LEVEL_BIT_CNT - 8));	/* 8 bits */
		if (0 == limit_flag) {
			last_level = current_level;
		} else {
			if (limit < current_level) {
				/* extend 8-bit limit to 10 bits */
				level =
				    (limit <<
				     (MT_LED_INTERNAL_LEVEL_BIT_CNT -
				      8)) | (limit >> (16 -
						       MT_LED_INTERNAL_LEVEL_BIT_CNT));
			}
		}
		mutex_unlock(&bl_level_limit_mutex);
#endif

		return
		    mt_mt65xx_led_set_cust(&cust_led_list[MT65XX_LED_TYPE_LCD],
					   level);
	} else {
		return mt65xx_led_set_cust(&cust_led_list[MT65XX_LED_TYPE_LCD],
					   (level >>
					    (MT_LED_INTERNAL_LEVEL_BIT_CNT -
					     8)));
	}

}
EXPORT_SYMBOL(backlight_brightness_set);
#if 0
static ssize_t show_duty(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	LEDS_DRV_DEBUG("get backlight duty value is:%d\n", bl_duty);
	return sprintf(buf, "%u\n", bl_duty);
}

static ssize_t store_duty(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int level = 0;
	size_t count = 0;

	bl_div = mt_get_bl_div();
	LEDS_DRV_DEBUG("set backlight duty start\n");
	level = (unsigned int) kstrtoul(buf, &pvalue, 10);
	count = pvalue - buf;
	if (*pvalue && isspace(*pvalue))
		count++;

	if (count == size) {

		if (bl_setting->mode == MT65XX_LED_MODE_PMIC) {
			/* duty:0-16 */
			if ((level >= 0) && (level <= 15)) {
				mt_brightness_set_pmic_duty_store((level * 17),
								  bl_div);
			} else {
				LEDS_DRV_DEBUG
				    ("duty value is error, please select value from [0-15]!\n");
			}

		}

		else if (bl_setting->mode == MT65XX_LED_MODE_PWM) {
			if (level == 0) {
				mt_led_pwm_disable(bl_setting->data);
			} else if (level <= 64) {
				mt_backlight_set_pwm_duty(bl_setting->data,
							  level, bl_div,
							  &bl_setting->
							  config_data);
			}
		}

		mt_set_bl_duty(level);

	}

	return size;
}

static DEVICE_ATTR(duty, 0664, show_duty, store_duty);

static ssize_t show_div(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	bl_div = mt_get_bl_div();
	LEDS_DRV_DEBUG("get backlight div value is:%d\n", bl_div);
	return sprintf(buf, "%u\n", bl_div);
}

static ssize_t store_div(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int div = 0;
	size_t count = 0;

	bl_duty = mt_get_bl_duty();
	LEDS_DRV_DEBUG("set backlight div start\n");
	div = kstrtoul(buf, &pvalue, 10);
	count = pvalue - buf;

	if (*pvalue && isspace(*pvalue))
		count++;

	if (count == size) {
		if (div < 0 || (div > 7)) {
			LEDS_DRV_DEBUG
			    ("set backlight div parameter error: %d[div:0~7]\n",
			     div);
			return 0;
		}

		if (bl_setting->mode == MT65XX_LED_MODE_PWM) {
			LEDS_DRV_DEBUG
			    ("set PWM backlight div OK: div=%d, duty=%d\n", div,
			     bl_duty);
			mt_backlight_set_pwm_div(bl_setting->data, bl_duty, div,
						 &bl_setting->config_data);
		}

		else if (bl_setting->mode == MT65XX_LED_MODE_CUST_LCM) {
			bl_brightness = mt_get_bl_brightness();
			LEDS_DRV_DEBUG
			    ("set cust backlight div OK: div=%d, brightness=%d\n",
			     div, bl_brightness);
			((cust_brightness_set) (bl_setting->data))
			    (bl_brightness, div);
		}
		mt_set_bl_div(div);

	}

	return size;
}

static DEVICE_ATTR(div, 0664, show_div, store_div);

static ssize_t show_frequency(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	bl_div = mt_get_bl_div();
	bl_frequency = mt_get_bl_frequency();

	if (bl_setting->mode == MT65XX_LED_MODE_PWM) {
		mt_set_bl_frequency(32000 / div_array[bl_div]);
	} else if (bl_setting->mode == MT65XX_LED_MODE_CUST_LCM) {
		/* mtkfb_get_backlight_pwm(bl_div, &bl_frequency); */
		mt_backlight_get_pwm_fsel(bl_div, &bl_frequency);
	}

	LEDS_DRV_DEBUG("get backlight PWM frequency value is:%d\n",
		       bl_frequency);

	return sprintf(buf, "%u\n", bl_frequency);
}

static DEVICE_ATTR(frequency, 0444, show_frequency, NULL);

static ssize_t store_pwm_register(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	if (buf != NULL && size != 0) {
		/* LEDS_DRV_DEBUG("store_pwm_register: size:%d,address:0x%s\n", size, buf); */
		reg_address = kstrtoul(buf, &pvalue, 16);

		if (*pvalue && (*pvalue == '#')) {
			reg_value = kstrtoul((pvalue + 1), NULL, 16);
			LEDS_DRV_DEBUG("set pwm register:[0x%x]= 0x%x\n",
				       reg_address, reg_value);
			/* OUTREG32(reg_address,reg_value); */
			mt_store_pwm_register(reg_address, reg_value);

		} else if (*pvalue && (*pvalue == '@')) {
			LEDS_DRV_DEBUG("get pwm register:[0x%x]=0x%x\n",
				       reg_address,
				       mt_show_pwm_register(reg_address));
		}
	}

	return size;
}

static ssize_t show_pwm_register(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(pwm_register, 0664, show_pwm_register, store_pwm_register);
#endif

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
static int led_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int led_i2c_remove(struct i2c_client *client);
/*
static struct i2c_board_info leds_board_info __initdata = {
	I2C_BOARD_INFO("lp8557_led", 0x2c),
};*/

static const struct of_device_id lp855x_id[] = {
	{.compatible = "mediatek,8173led_i2c"},
	{.compatible = "ti,lp8557_led"},
	{},
};
MODULE_DEVICE_TABLE(OF, lp855x_id);

static const struct i2c_device_id lp855x_i2c_id[] = {{"lp8557_led", 0}, {} };

struct i2c_driver led_i2c_driver = {
	.probe = led_i2c_probe,
	.remove = led_i2c_remove,
	.driver = {
		.name = "lp8557_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lp855x_id),
	},
	.id_table = lp855x_i2c_id,
};

static int led_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	g_client = client;

	return 0;
}

static int led_i2c_remove(struct i2c_client *client)
{
	return 0;
}
#endif
/****************************************************************************
 * driver functions
 ***************************************************************************/
static int mt65xx_leds_probe(struct platform_device *pdev)
{
	int i;
	int ret;/* rc; */
	struct cust_mt65xx_led *cust_led_list = mt_get_cust_led_list();
	#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557

	/*i2c_register_board_info(4, &leds_board_info, 1);*/
	if (i2c_add_driver(&led_i2c_driver)) {
		LEDS_DRV_DEBUG("unable to add led-i2c driver.\n");
		return -1;
	}
	#endif
	LEDS_DRV_DEBUG("%s\n", __func__);
	get_div_array();
	for (i = 0; i < MT65XX_LED_TYPE_TOTAL; i++) {
		if (cust_led_list[i].mode == MT65XX_LED_MODE_NONE) {
			g_leds_data[i] = NULL;
			continue;
		}

		g_leds_data[i] =
		    kzalloc(sizeof(struct mt65xx_led_data), GFP_KERNEL);
		if (!g_leds_data[i]) {
			ret = -ENOMEM;
			goto err;
		}

		g_leds_data[i]->cust.mode = cust_led_list[i].mode;
		g_leds_data[i]->cust.data = cust_led_list[i].data;
		g_leds_data[i]->cust.name = cust_led_list[i].name;

		g_leds_data[i]->cdev.name = cust_led_list[i].name;
		g_leds_data[i]->cust.config_data = cust_led_list[i].config_data;	/* bei add */

		g_leds_data[i]->cdev.brightness_set = mt65xx_led_set;
		g_leds_data[i]->cdev.blink_set = mt65xx_blink_set;

		INIT_WORK(&g_leds_data[i]->work, mt_mt65xx_led_work);

		ret = led_classdev_register(&pdev->dev, &g_leds_data[i]->cdev);
		#if 0
		if (strcmp(g_leds_data[i]->cdev.name, "lcd-backlight") == 0) {
			rc = device_create_file(g_leds_data[i]->cdev.dev,
						&dev_attr_duty);
			if (rc) {
				LEDS_DRV_DEBUG
				    ("device_create_file duty fail!\n");
			}

			rc = device_create_file(g_leds_data[i]->cdev.dev,
						&dev_attr_div);
			if (rc) {
				LEDS_DRV_DEBUG
				    ("device_create_file duty fail!\n");
			}

			rc = device_create_file(g_leds_data[i]->cdev.dev,
						&dev_attr_frequency);
			if (rc) {
				LEDS_DRV_DEBUG
				    ("device_create_file duty fail!\n");
			}

			rc = device_create_file(g_leds_data[i]->cdev.dev,
						&dev_attr_pwm_register);
			if (rc) {
				LEDS_DRV_DEBUG
				    ("device_create_file duty fail!\n");
			}
			bl_setting = &g_leds_data[i]->cust;
		}
		#endif
		if (ret)
			goto err;

	}
#ifdef CONTROL_BL_TEMPERATURE

	last_level = 0;
	limit = 255;
	limit_flag = 0;
	current_level = 0;
	LEDS_DRV_DEBUG
	    ("led probe last_level = %d, limit = %d, limit_flag = %d, current_level = %d\n",
	     last_level, limit, limit_flag, current_level);
#endif

	return 0;

 err:
	if (i) {
		for (i = i - 1; i >= 0; i--) {
			if (!g_leds_data[i])
				continue;
			led_classdev_unregister(&g_leds_data[i]->cdev);
			cancel_work_sync(&g_leds_data[i]->work);
			kfree(g_leds_data[i]);
			g_leds_data[i] = NULL;
		}
	}

	return ret;
}

static int mt65xx_leds_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < MT65XX_LED_TYPE_TOTAL; i++) {
		if (!g_leds_data[i])
			continue;
		led_classdev_unregister(&g_leds_data[i]->cdev);
		cancel_work_sync(&g_leds_data[i]->work);
		kfree(g_leds_data[i]);
		g_leds_data[i] = NULL;
	}

	return 0;
}

/*
static int mt65xx_leds_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}
*/

static void mt65xx_leds_shutdown(struct platform_device *pdev)
{
	int i;
	struct nled_setting led_tmp_setting = { NLED_OFF, 0, 0 };

	LEDS_DRV_DEBUG("%s\n", __func__);
	LEDS_DRV_DEBUG("mt65xx_leds_shutdown: turn off backlight\n");

	for (i = 0; i < MT65XX_LED_TYPE_TOTAL; i++) {
		if (!g_leds_data[i])
			continue;
		switch (g_leds_data[i]->cust.mode) {

		case MT65XX_LED_MODE_PWM:
			if (strcmp(g_leds_data[i]->cust.name, "lcd-backlight")
			    == 0) {
				/* mt_set_pwm_disable(g_leds_data[i]->cust.data); */
				/* mt_pwm_power_off (g_leds_data[i]->cust.data); */
				mt_led_pwm_disable(g_leds_data[i]->cust.data);
			} else {
				led_set_pwm(g_leds_data[i]->cust.data,
					    &led_tmp_setting);
			}
			break;

			/* case MT65XX_LED_MODE_GPIO: */
			/* brightness_set_gpio(g_leds_data[i]->cust.data, 0); */
			/* break; */

		case MT65XX_LED_MODE_PMIC:
			brightness_set_pmic(g_leds_data[i]->cust.data, 0, 0);
			break;
		case MT65XX_LED_MODE_CUST_LCM:
			LEDS_DRV_DEBUG("backlight control through LCM!!1\n");
			((cust_brightness_set) (g_leds_data[i]->cust.data)) (0,
									     bl_div);
			break;
		case MT65XX_LED_MODE_CUST_BLS_PWM:
			LEDS_DRV_DEBUG("backlight control through BLS!!1\n");
			((cust_set_brightness) (g_leds_data[i]->cust.data)) (0);
			break;
		case MT65XX_LED_MODE_NONE:
		default:
			break;
		}
	}

}

static struct platform_driver mt65xx_leds_driver = {
	.driver = {
		   .name = "leds-mt65xx",
		   .owner = THIS_MODULE,
		   },
	.probe = mt65xx_leds_probe,
	.remove = mt65xx_leds_remove,
	/* .suspend      = mt65xx_leds_suspend, */
	.shutdown = mt65xx_leds_shutdown,
};

#ifdef CONFIG_OF
static struct platform_device mt65xx_leds_device = {
	.name = "leds-mt65xx",
	.id = -1
};

#endif

static int __init mt65xx_leds_init(void)
{
	int ret;

	LEDS_DRV_DEBUG("%s\n", __func__);

#ifdef CONFIG_OF
	ret = platform_device_register(&mt65xx_leds_device);
	if (ret)
		LEDS_DRV_DEBUG("mt65xx_leds_init:dev:E%d\n", ret);
#endif
	ret = platform_driver_register(&mt65xx_leds_driver);

	if (ret) {
		LEDS_DRV_DEBUG("mt65xx_leds_init:drv:E%d\n", ret);
/* platform_device_unregister(&mt65xx_leds_device); */
		return ret;
	}

	mt_leds_wake_lock_init();

	return ret;
}

static void __exit mt65xx_leds_exit(void)
{
	platform_driver_unregister(&mt65xx_leds_driver);
/* platform_device_unregister(&mt65xx_leds_device); */
}

module_param(debug_enable_led, int, 0644);

module_init(mt65xx_leds_init);
module_exit(mt65xx_leds_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("LED driver for MediaTek MT65xx chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("leds-mt65xx");
