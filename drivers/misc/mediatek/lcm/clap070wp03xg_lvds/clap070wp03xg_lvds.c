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
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#else
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif
#endif

#include "lcm_drv.h"

#ifndef BUILD_LK
static struct regulator *lcm_vgp;
static unsigned int GPIO_LCD_PWR;

/* get LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
	int ret;
	struct regulator *lcm_vgp_ldo;

	pr_info("LCM: %s enter\n", __func__);

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		pr_info("failed to get reg-lcm LDO, %d\n", ret);
		return ret;
	}

	pr_info("LCM: lcm get supply ok.\n");

	/* get current voltage settings */
	ret = regulator_get_voltage(lcm_vgp_ldo);
	pr_info("lcm LDO voltage = %d in LK stage\n", ret);

	lcm_vgp = lcm_vgp_ldo;

	return ret;
}

int lcm_vgp_supply_enable(void)
{
	int ret;
	unsigned int volt;

	pr_info("LCM: %s enter\n", __func__);

	if (!lcm_vgp)
		return 0;

	pr_info("LCM: set regulator voltage lcm_vgp voltage to 1.8V\n");
	/* set voltage to 1.8V */
	ret = regulator_set_voltage(lcm_vgp, 1800000, 1800000);
	if (ret != 0) {
		pr_info("LCM: lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 1800000)
		pr_info("LCM: check regulator voltage=1800000 pass!\n");
	else
		pr_info("LCM: check regulator voltage=1800000 fail! (voltage: %d)\n",
			volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		pr_info("LCM: Failed to enable lcm_vgp: %d\n", ret);
		return ret;
	}

	return ret;
}

int lcm_vgp_supply_disable(void)
{
	int ret = 0;
	unsigned int isenable;

	if (!lcm_vgp)
		return 0;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	pr_info("LCM: lcm query regulator enable status[%d]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			pr_info("LCM: lcm failed to disable lcm_vgp: %d\n",
				ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			pr_info("LCM: lcm regulator disable pass\n");
	}

	return ret;
}

void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_PWR = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr", 0);
	gpio_request(GPIO_LCD_PWR, "GPIO_LCD_PWR");
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	lcm_request_gpio_control(dev);
	lcm_get_vgp_supply(dev);
	lcm_vgp_supply_enable();

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "cpt,clap070wp03xg_lvds",
		.data = 0,
	}, {
		/* sentinel */
	}
};

MODULE_DEVICE_TABLE(of, platform_of_match);

static int lcm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(lcm_platform_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		   .name = "clap070wp03xg_lvds",
		   .owner = THIS_MODULE,
		   .of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_init(void)
{
	if (platform_driver_register(&lcm_driver)) {
		pr_info("LCM: failed to register this driver!\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_exit(void)
{
	platform_driver_unregister(&lcm_driver);
}

late_initcall(lcm_init);
module_exit(lcm_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("LCM display subsystem driver");
MODULE_LICENSE("GPL");
#endif
/* ------------------------------------------------------------------------ */
/* Local Constants */
/* ------------------------------------------------------------------------ */
#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)

#define GPIO_OUT_ONE  1
#define GPIO_OUT_ZERO 0

#ifdef GPIO_LCM_PWR
#define GPIO_LCD_PWR      GPIO_LCM_PWR
#endif

/* ------------------------------------------------------------------------ */
/* Local Variables */
/* ------------------------------------------------------------------------ */
static struct LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* ------------------------------------------------------------------------ */
/* Local Functions */
/* ------------------------------------------------------------------------ */
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_set_value(GPIO, output);
#endif
}

static void lcm_init_power(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] %s enter\n", __func__);
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);
	MDELAY(20);

	mt6392_upmu_set_rg_vgp2_vosel(3);
	mt6392_upmu_set_rg_vgp2_en(0x1);
#else
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
#endif
}

static void lcm_suspend_power(void)
{
#ifndef BUILD_LK
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_vgp_supply_disable();
	MDELAY(20);
#endif
}

static void lcm_resume_power(void)
{
#ifndef BUILD_LK
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_vgp_supply_enable();
	MDELAY(20);
#endif
}

/* ------------------------------------------------------------------------ */
/* LCM Driver Implementations */
/* ------------------------------------------------------------------------ */
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DPI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->density = 213;
	params->dpi.format            = LCM_DPI_FORMAT_RGB888;
	params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;

	params->dpi.clk_pol = LCM_POLARITY_FALLING;
	params->dpi.de_pol = LCM_POLARITY_RISING;
	params->dpi.vsync_pol = LCM_POLARITY_FALLING;
	params->dpi.hsync_pol = LCM_POLARITY_FALLING;

	params->dpi.hsync_pulse_width = 16;
	params->dpi.hsync_back_porch = 16;
	params->dpi.hsync_front_porch = 32;

	params->dpi.vsync_pulse_width = 2;
	params->dpi.vsync_back_porch = 2;
	params->dpi.vsync_front_porch = 4;

	params->dpi.width = FRAME_WIDTH;
	params->dpi.height = FRAME_HEIGHT;

	params->dpi.PLL_CLOCK = 67;
	params->dpi.ssc_disable = 1;

	params->dpi.lvds_tx_en = 1;
}

static void lcm_init_lcm(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] %s enter\n", __func__);
#else
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
#endif
}

void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] %s enter\n", __func__);
#else
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
#endif
}

void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] %s enter\n", __func__);
#else
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
#endif
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
	return 0;
}

struct LCM_DRIVER clap070wp03xg_lvds_lcm_drv = {
	.name = "clap070wp03xg_lvds",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.ata_check = lcm_ata_check,
};
