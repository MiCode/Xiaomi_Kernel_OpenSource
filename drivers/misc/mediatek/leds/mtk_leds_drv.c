// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <mtk_leds_hal.h>
#include <mtk_leds_drv.h>
#ifdef CONFIG_MTK_PWM
#include <mt-plat/mtk_pwm.h>
#endif
#ifdef CONFIG_MTK_AAL_SUPPORT
#include <ddp_aal.h>
#endif

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <asm-generic/gpio.h>
#endif

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

/****************************************************************************
 * variables
 ***************************************************************************/
#ifndef CONFIG_MTK_PWM
#define CLK_DIV1 0
#endif

struct cust_mt65xx_led *bl_setting;
#ifndef CONFIG_MTK_AAL_SUPPORT
static unsigned int bl_div = CLK_DIV1;
#endif
#define PWM_DIV_NUM 8
static unsigned int div_array[PWM_DIV_NUM];
struct mt65xx_led_data *g_leds_data[MT65XX_LED_TYPE_TOTAL];

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
static unsigned int last_level1 = 102;
static struct i2c_client *g_client;
static int I2C_SET_FOR_BACKLIGHT  = 350;
#endif


/****************************************************************************
 * function prototypes
 ***************************************************************************/
#ifndef CONTROL_BL_TEMPERATURE
#define CONTROL_BL_TEMPERATURE
#endif

#define MT_LED_INTERNAL_LEVEL_BIT_CNT 10

/******************************************************************************
 * for DISP backlight High resolution
 *****************************************************************************/
#ifdef LED_INCREASE_LED_LEVEL_MTKPATCH
#define LED_INTERNAL_LEVEL_BIT_CNT 10
#endif
/* Fix dependency if CONFIG_MTK_LCM not ready */
void __weak disp_aal_notify_backlight_changed(int bl_1024) {};
bool __weak disp_aal_is_support(void) { return false; };
int __weak disp_bls_set_max_backlight(unsigned int level_1024) { return 0; };
int __weak disp_bls_set_backlight(int level_1024) { return 0; }
int __weak mtkfb_set_backlight_level(unsigned int level) { return 0; };
void __weak disp_pq_notify_backlight_changed(int bl_1024) {};
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
	if (enable == 1) {
		limit_flag = 1;
		limit = max_level;
		mutex_unlock(&bl_level_limit_mutex);
		if (current_level != 0) {
			if (limit < last_level) {
				pr_info
				    ("Max brightness limit=%d\n", limit);
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

		if (current_level != 0) {
			pr_info("Control temperature close:limit=%d\n",
				       limit);
			mt65xx_led_set_cust(&cust_led_list[MT65XX_LED_TYPE_LCD],
					    last_level);

		}
	}
#else
	pr_info("Set max brightness go through AAL\n");
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
		pr_debug("div_array=%d\n", div_array[i]);
		i++;
	}
}

static int led_set_pwm(int pwm_num, struct nled_setting *led)
{

	mt_led_set_pwm(pwm_num, led);
	return 0;
}
static int mt65xx_led_set_cust(struct cust_mt65xx_led *cust, int level)
{
#ifdef CONTROL_BL_TEMPERATURE
	mutex_lock(&bl_level_limit_mutex);
	current_level = level;
	if (limit_flag == 0) {
		last_level = level;
	} else {
		if (limit < current_level)
			level = limit;
	}
	mutex_unlock(&bl_level_limit_mutex);
#endif
#ifdef LED_INCREASE_LED_LEVEL_MTKPATCH
	if (cust->mode == MT65XX_LED_MODE_CUST_BLS_PWM) {
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
	pr_debug("read_byte_data: 0x10 = %d\n", value);

	node = of_find_compatible_node(NULL, NULL,
						    "mediatek,lcd-backlight");
	if (node) {
		I2C_SET_FOR_BACKLIGHT = of_get_named_gpio(node, "gpios", 0);
		pr_debug("Led_i2c gpio num for power:%d\n",
				I2C_SET_FOR_BACKLIGHT);
	}
#endif
	if (strcmp(led_data->cust.name, "lcd-backlight") == 0) {
#ifdef CONTROL_BL_TEMPERATURE
		mutex_lock(&bl_level_limit_mutex);
		current_level = level;
		if (limit_flag == 0) {
			last_level = level;
		} else {
			if (limit < current_level) {
				level = limit;
				pr_debug
				    ("set backlight: control level=%d\n",
				     level);
			}
		}
		mutex_unlock(&bl_level_limit_mutex);
#endif
	}
#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	retval = gpio_request(I2C_SET_FOR_BACKLIGHT, "i2c_set_for_backlight");
	if (retval)
		pr_debug("Request I2C gpio149 failed\n");

	if (strcmp(led_data->cust.name, "lcd-backlight") == 0) {
		if (level == 0) {
			pr_debug("Level = %d, close the power\n", level);
			i2c_smbus_write_byte_data(client, 0x00, 0);
			gpio_direction_output(I2C_SET_FOR_BACKLIGHT, 0);
		}
		if (!last_level1 && level) {
			pr_debug("Level = %d, open the power\n", level);
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
	pr_debug("LEDS:mt65xx_led_set:0x10 = %d\n", value);

	node = of_find_compatible_node(NULL, NULL,
						    "mediatek,lcd-backlight");
	if (node) {
		I2C_SET_FOR_BACKLIGHT = of_get_named_gpio(node, "gpios", 0);
		pr_debug("Led_i2c gpio num for power:%d\n",
				I2C_SET_FOR_BACKLIGHT);
	}
#endif

	pr_debug("light type: #%d, level: %d\n", type, level);

	if (type < 0 || type >= MT65XX_LED_TYPE_TOTAL)
		return -1;

	if (level > LED_FULL)
		level = LED_FULL;
	else if (level < 0)
		level = 0;

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	retval = gpio_request(I2C_SET_FOR_BACKLIGHT, "i2c_set_for_backlight");
	if (retval)
		pr_debug("Request I2C gpio149 failed\n");

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
		current_level = (level >> (MT_LED_INTERNAL_LEVEL_BIT_CNT - 8));
		if (limit_flag == 0) {
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
#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
static int led_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int led_i2c_remove(struct i2c_client *client);

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

static int led_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
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
	int ret;
	struct cust_mt65xx_led *cust_led_list = mt_get_cust_led_list();

	if (!cust_led_list) {
		pr_info("Get dts fail! Probe exit.\n");
		ret = -1;
		goto err_dts;
	}

#ifdef CONFIG_BACKLIGHT_SUPPORT_LP8557
	/*i2c_register_board_info(4, &leds_board_info, 1);*/
	if (i2c_add_driver(&led_i2c_driver)) {
		pr_debug("Unable to add led-i2c driver.\n");
		return -1;
	}
	#endif
	pr_debug("Probe begain!\n");
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
		g_leds_data[i]->cust.config_data = cust_led_list[i].config_data;

		g_leds_data[i]->cdev.brightness_set = mt65xx_led_set;
		g_leds_data[i]->cdev.blink_set = mt65xx_blink_set;

		INIT_WORK(&g_leds_data[i]->work, mt_mt65xx_led_work);

		ret = led_classdev_register(&pdev->dev, &g_leds_data[i]->cdev);
		if (ret)
			goto err;

	}
#ifdef CONTROL_BL_TEMPERATURE

	last_level = 0;
	limit = 255;
	limit_flag = 0;
	current_level = 0;
	pr_debug
	    ("last_level= %d, limit= %d, limit_flag= %d, current_level= %d\n",
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

err_dts:
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

static void mt65xx_leds_shutdown(struct platform_device *pdev)
{
	int i;
	struct nled_setting led_tmp_setting = { NLED_OFF, 0, 0 };

	pr_debug("Turn off backlight\n");

	for (i = 0; i < MT65XX_LED_TYPE_TOTAL; i++) {
		if (!g_leds_data[i])
			continue;
		switch (g_leds_data[i]->cust.mode) {

		case MT65XX_LED_MODE_PWM:
			if (strcmp(g_leds_data[i]->cust.name, "lcd-backlight")
			    == 0) {
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
			pr_debug("not support set brightness through pmic!!1\n");
			break;
		case MT65XX_LED_MODE_CUST_LCM:
			pr_debug("Backlight control through LCM!!1\n");
#ifdef CONFIG_MTK_AAL_SUPPORT
			disp_aal_notify_backlight_changed(0);
#else
			((cust_brightness_set) (g_leds_data[i]->cust.data)) (0,
					bl_div);
#endif
			break;
		case MT65XX_LED_MODE_CUST_BLS_PWM:
			pr_debug("Backlight control through BLS!!1\n");
#ifdef CONFIG_MTK_AAL_SUPPORT
			disp_aal_notify_backlight_changed(0);
#else
			((cust_set_brightness) (g_leds_data[i]->cust.data)) (0);
#endif
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

	pr_debug("Leds init\n");

#ifdef CONFIG_OF
	ret = platform_device_register(&mt65xx_leds_device);
	if (ret)
		pr_debug("dev:E%d\n", ret);
#endif
	ret = platform_driver_register(&mt65xx_leds_driver);

	if (ret) {
		pr_debug("drv:E%d\n", ret);
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

//module_param(debug_enable_led, int, 0644);
/* delay leds init, for (1)display has delayed to use clock upstream.
 * (2)to fix repeat switch battary and power supply caused BL KE issue,
 * battary calling bl .shutdown whitch need to call disp_pwm and display
 * function and they not yet probe.
 */
late_initcall(mt65xx_leds_init);
module_exit(mt65xx_leds_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("LED driver for MediaTek MT65xx chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("leds-mt65xx");
