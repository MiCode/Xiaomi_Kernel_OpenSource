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

#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include "lcm_drv.h"
#include "ddp_irq.h"

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif

static struct regulator *lcm_vgp;
static unsigned int GPIO_LCD_PWR_EN;
static unsigned int GPIO_LCD_RST_EN;

/* get LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
	int ret;
	struct regulator *lcm_vgp_ldo;

	pr_debug("LCM: lcm_get_vgp_supply is going\n");

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		dev_err(dev, "failed to get reg-lcm LDO, %d\n", ret);
		return ret;
	}

	pr_debug("LCM: lcm get supply ok.\n");

	/* get current voltage settings */
	ret = regulator_get_voltage(lcm_vgp_ldo);
	pr_debug("lcm LDO voltage = %d in LK stage\n", ret);

	lcm_vgp = lcm_vgp_ldo;

	return ret;
}

int lcm_vgp_supply_enable(void)
{
	int ret;
	unsigned int volt;

	pr_debug("LCM: lcm_vgp_supply_enable\n");

	if (NULL == lcm_vgp)
		return 0;

	pr_debug("LCM: set regulator voltage lcm_vgp voltage to 2.8V\n");
	/* set voltage to 1.8V */
	ret = regulator_set_voltage(lcm_vgp, 1800000, 1800000);
	if (ret != 0) {
		pr_err("LCM: lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 1800000)
		pr_err("LCM: check regulator voltage=2800000 pass!\n");
	else
		pr_err("LCM: check regulator voltage=2800000 fail! (voltage: %d)\n", volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		pr_err("LCM: Failed to enable lcm_vgp: %d\n", ret);
		return ret;
	}

	return ret;
}

int lcm_vgp_supply_disable(void)
{
	int ret = 0;
	unsigned int isenable;

	if (NULL == lcm_vgp)
		return 0;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	pr_debug("LCM: lcm query regulator enable status[0x%d]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			pr_err("LCM: lcm failed to disable lcm_vgp: %d\n", ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			pr_err("LCM: lcm regulator disable pass\n");
	}

	return ret;
}

void lcm_get_gpio_infor(void)
{
	static struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,lcm");

	GPIO_LCD_PWR_EN = of_get_named_gpio(node, "lcm_power_gpio", 0);
	GPIO_LCD_RST_EN = of_get_named_gpio(node, "lcm_reset_gpio", 0);
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
}

static int lcm_probe(struct device *dev)
{
	lcm_get_vgp_supply(dev);
	lcm_get_gpio_infor();
	return 0;
}

static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "mediatek,lcm",},
	{}
};

static struct platform_driver lcm_driver = {
	.driver = {
		   .name = "mtk_lcm",
		   .owner = THIS_MODULE,
		   .probe = lcm_probe,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init lcm_init(void)
{
	pr_debug("LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_err("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_debug("LCM: Unregister lcm driver done\n");
}
late_initcall(lcm_init);
module_exit(lcm_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");


/**
 * Local Constants
 */
#define FRAME_WIDTH		(600)
#define FRAME_HEIGHT		(1024)

#define REGFLAG_DELAY		0xFE
#define REGFLAG_END_OF_TABLE	0xFF   /* END OF REGISTERS MARKER */

#define LCM_DSI_CMD_MODE	0

/**
 * Local Variables
 */
static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


/**
 * Local Functions
 */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)				lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg				lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#ifndef ASSERT
#define ASSERT(expr)					\
	do {						\
		if (expr)				\
			break;				\
		pr_debug("DDP ASSERT FAILED %s, %d\n",	\
		       __FILE__, __LINE__);		\
		BUG();					\
	} while (0)
#endif

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};

/**
 * Note :
 *
 * Data ID will depends on the following rule.
 *
 * count of parameters > 1	=> Data ID = 0x39
 * count of parameters = 1	=> Data ID = 0x15
 * count of parameters = 0	=> Data ID = 0x05
 *
 * Structure Format :
 *
 * {DCS command, count of parameters, {parameter list}}
 * {REGFLAG_DELAY, milliseconds of time, {} },
 * ...
 *
 * Setting ending by predefined flag
 *
 * {REGFLAG_END_OF_TABLE, 0x00, {}}
 */
static struct LCM_setting_table lcm_initialization_setting[] = {
	/*This init code for HX9279A driver IC*/
	{0xB0, 1, {0x00} },
	{0xBA, 1, {0xA7} },
	{0xBD, 1, {0x41} },
	{0xBE, 1, {0x77} },
	{0xBF, 1, {0x1F} },
	{0xC0, 1, {0x0C} },
	{0xC1, 1, {0x78} },
	{0xC2, 1, {0x5C} },
	{0xC3, 1, {0xE8} },
	{0xC4, 1, {0x73} },
	{0xC5, 1, {0x14} },
	{0xC6, 1, {0x02} },
	{0xC7, 1, {0x14} },
	{0xC8, 1, {0x02} },
	{0xC9, 1, {0x06} },
	{0xCA, 1, {0x00} },
	{0xCB, 1, {0x00} },

	{0xB0, 1, {0x01} },
	{0xB1, 1, {0x5C} },
	{0xB2, 1, {0x5E} },
	{0xB3, 1, {0x72} },
	{0xB4, 1, {0xBC} },
	{0xB5, 1, {0xE9} },
	{0xB6, 1, {0x5E} },
	{0xB7, 1, {0x93} },
	{0xB8, 1, {0xBD} },
	{0xB9, 1, {0xFC} },
	{0xBA, 1, {0x2A} },
	{0xBB, 1, {0x54} },
	{0xBC, 1, {0x7C} },
	{0xBD, 1, {0xAE} },
	{0xBE, 1, {0xD3} },
	{0xBF, 1, {0xF2} },
	{0xC0, 1, {0x16} },
	{0xC1, 1, {0xBC} },
	{0xC2, 1, {0x00} },
	{0xC3, 1, {0x15} },
	{0xC4, 1, {0x6A} },
	{0xC5, 1, {0xAB} },
	{0xC6, 1, {0x03} },
	{0xC7, 1, {0x5C} },
	{0xC8, 1, {0x5E} },
	{0xC9, 1, {0x72} },
	{0xCA, 1, {0xBC} },
	{0xCB, 1, {0x19} },
	{0xCC, 1, {0x5E} },
	{0xCD, 1, {0x93} },
	{0xCE, 1, {0xBD} },
	{0xCF, 1, {0xFC} },
	{0xD0, 1, {0x2A} },
	{0xD1, 1, {0x54} },
	{0xD2, 1, {0x7C} },
	{0xD3, 1, {0xAE} },
	{0xD4, 1, {0xD3} },
	{0xD5, 1, {0xF2} },
	{0xD6, 1, {0x16} },
	{0xD7, 1, {0xBC} },
	{0xD8, 1, {0x00} },
	{0xD9, 1, {0x15} },
	{0xDA, 1, {0x6A} },
	{0xDB, 1, {0xAB} },
	{0xDC, 1, {0x03} },
	{0xDD, 1, {0x5C} },
	{0xDE, 1, {0x5E} },
	{0xDF, 1, {0x72} },
	{0xE0, 1, {0xBC} },
	{0xE1, 1, {0xE9} },
	{0xE2, 1, {0x5E} },
	{0xE3, 1, {0x93} },
	{0xE4, 1, {0xBD} },
	{0xE5, 1, {0xFC} },
	{0xE6, 1, {0x2A} },
	{0xE7, 1, {0x54} },
	{0xE8, 1, {0x7C} },
	{0xE9, 1, {0xAE} },
	{0xEA, 1, {0xD3} },
	{0xEB, 1, {0xF2} },
	{0xEC, 1, {0x16} },
	{0xED, 1, {0xBC} },
	{0xEE, 1, {0x00} },
	{0xEF, 1, {0x15} },
	{0xF0, 1, {0x6A} },
	{0xF1, 1, {0xAB} },
	{0xF2, 1, {0x03} },

	{0xB0, 1, {0x02} },
	{0xB1, 1, {0x5C} },
	{0xB2, 1, {0x5E} },
	{0xB3, 1, {0x72} },
	{0xB4, 1, {0xBC} },
	{0xB5, 1, {0xE9} },
	{0xB6, 1, {0x5E} },
	{0xB7, 1, {0x93} },
	{0xB8, 1, {0xBD} },
	{0xB9, 1, {0xFC} },
	{0xBA, 1, {0x2A} },
	{0xBB, 1, {0x54} },
	{0xBC, 1, {0x7C} },
	{0xBD, 1, {0xAE} },
	{0xBE, 1, {0xD3} },
	{0xBF, 1, {0xF2} },
	{0xC0, 1, {0x16} },
	{0xC1, 1, {0xBC} },
	{0xC2, 1, {0x00} },
	{0xC3, 1, {0x15} },
	{0xC4, 1, {0x6A} },
	{0xC5, 1, {0xAB} },
	{0xC6, 1, {0x03} },
	{0xC7, 1, {0x5C} },
	{0xC8, 1, {0x5E} },
	{0xC9, 1, {0x72} },
	{0xCA, 1, {0xBC} },
	{0xCB, 1, {0xE9} },
	{0xCC, 1, {0x5E} },
	{0xCD, 1, {0x93} },
	{0xCE, 1, {0xBD} },
	{0xCF, 1, {0xFC} },
	{0xD0, 1, {0x2A} },
	{0xD1, 1, {0x54} },
	{0xD2, 1, {0x7C} },
	{0xD3, 1, {0xAE} },
	{0xD4, 1, {0xD3} },
	{0xD5, 1, {0xF2} },
	{0xD6, 1, {0x16} },
	{0xD7, 1, {0xBC} },
	{0xD8, 1, {0x00} },
	{0xD9, 1, {0x15} },
	{0xDA, 1, {0x6A} },
	{0xDB, 1, {0xAB} },
	{0xDC, 1, {0x03} },
	{0xDD, 1, {0x5C} },
	{0xDE, 1, {0x5E} },
	{0xDF, 1, {0x72} },
	{0xE0, 1, {0xBC} },
	{0xE1, 1, {0xE9} },
	{0xE2, 1, {0x5E} },
	{0xE3, 1, {0x93} },
	{0xE4, 1, {0xBD} },
	{0xE5, 1, {0xFC} },
	{0xE6, 1, {0x2A} },
	{0xE7, 1, {0x54} },
	{0xE8, 1, {0x7C} },
	{0xE9, 1, {0xAE} },
	{0xEA, 1, {0xD3} },
	{0xEB, 1, {0xF2} },
	{0xEC, 1, {0x16} },
	{0xED, 1, {0xBC} },
	{0xEE, 1, {0x00} },
	{0xEF, 1, {0x15} },
	{0xF0, 1, {0x6A} },
	{0xF1, 1, {0xAB} },
	{0xF2, 1, {0x03} },

	{0xB0, 1, {0x03} },
	{0xC0, 1, {0x49} },
	{0xC1, 1, {0x10} },
	{0xC2, 1, {0x01} },
	{0xC3, 1, {0x28} },
	{0xC4, 1, {0x28} },
	{0xC5, 1, {0x0C} },
	{0xC8, 1, {0x42} },
	{0xC9, 1, {0x40} },
	{0xCA, 1, {0x01} },
	{0xCB, 1, {0x08} },
	{0xCC, 1, {0x02} },
	{0xCD, 1, {0x08} },
	{0xCE, 1, {0x09} },
	{0xD0, 1, {0x03} },
	{0xD1, 1, {0x0A} },
	{0xDC, 1, {0x00} },
	{0xDD, 1, {0x00} },
	{0xDE, 1, {0x00} },
	{0xDF, 1, {0x00} },
	{0xE0, 1, {0x12} },
	{0xE1, 1, {0x11} },
	{0xE2, 1, {0x05} },
	{0xE3, 1, {0x06} },
	{0xE4, 1, {0x07} },
	{0xE5, 1, {0x08} },
	{0xE6, 1, {0x09} },
	{0xE7, 1, {0x0A} },
	{0xE8, 1, {0x0B} },
	{0xE9, 1, {0x0C} },
	{0xEA, 1, {0x02} },
	{0xEB, 1, {0x01} },
	{0xEC, 1, {0x12} },
	{0xED, 1, {0x11} },
	{0xEE, 1, {0x05} },
	{0xEF, 1, {0x06} },
	{0xF0, 1, {0x07} },
	{0xF1, 1, {0x08} },
	{0xF2, 1, {0x09} },
	{0xF3, 1, {0x0A} },
	{0xF4, 1, {0x0B} },
	{0xF5, 1, {0x0C} },
	{0xF6, 1, {0x02} },
	{0xF7, 1, {0x01} },
	{0xF8, 1, {0x00} },
	{0xF9, 1, {0x00} },
	{0xFA, 1, {0x00} },
	{0xFB, 1, {0x00} },
	{0xB0, 1, {0x05} },
	{0xB3, 1, {0x52} },
	{0xB0, 1, {0x06} },
	{0xB8, 1, {0xA5} },
	{0xC0, 1, {0xA5} },
	{0xC5, 1, {0x44} },
	{0xC7, 1, {0x1F} },
	{0xB8, 1, {0x5A} },
	{0xC0, 1, {0x5A} },

	{0xB0, 1, {0x03} },
	{0xB2, 1, {0xA5} },
	{0xB3, 1, {0x04} },
	{0xB0, 1, {0x0F} },
	{0x11, 0, {0x00} },
	{0x29, 0, {0x00} },
	{REGFLAG_DELAY, 80, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH&0xFF) } },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT&0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 0, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Sleep Mode On */
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {

		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count,
					table[i].para_list, force_update);
		}
	}
}

/**
 * LCM Driver Implementations
 */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

		params->type = LCM_TYPE_DSI;
		params->width = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode = CMD_MODE;
		params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
		params->dsi.mode = SYNC_PULSE_VDO_MODE;
#endif

		/* DSI */
		/* Command mode setting */
		params->dsi.LANE_NUM = LCM_FOUR_LANE;
		/* The following defined the fomat for data coming from LCD engine. */
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

		/* Highly depends on LCD driver capability. */
		/* Not support in MT6573 */
		params->dsi.packet_size = 256;
		params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.vertical_sync_active = 2;
		params->dsi.vertical_backporch = 8;
		params->dsi.vertical_frontporch = 14;
		params->dsi.vertical_active_line = FRAME_HEIGHT;

		params->dsi.horizontal_sync_active = 24;
		params->dsi.horizontal_backporch = 36;
		params->dsi.horizontal_frontporch = 100;
		params->dsi.horizontal_active_pixel = FRAME_WIDTH;

		params->dsi.ssc_disable = 1;
		params->dsi.PLL_CLOCK = 150;
}

static void lcm_init_lcm(void)
{
	lcm_vgp_supply_enable();
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
	SET_RESET_PIN(1);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	SET_RESET_PIN(0);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
	SET_RESET_PIN(1);
	MDELAY(100);
	push_table(lcm_initialization_setting,
			sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	/*push_table(lcm_deep_sleep_mode_in_setting,
		   sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);*/
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
	lcm_vgp_supply_disable();
	SET_RESET_PIN(0);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	MDELAY(150);
}

static void lcm_resume(void)
{
	lcm_init_lcm();
}

LCM_DRIVER ek79023_dsi_wsvga_vdo_lcm_drv = {
	.name		= "ek79023_dsi_wsvga_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init_lcm,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if (LCM_DSI_CMD_MODE)
	/*.set_backlight	= lcm_setbacklight,*/
	/* .set_pwm        = lcm_setpwm, */
	/* .get_pwm        = lcm_getpwm, */
	/*.update         = lcm_update, */
#endif
};
