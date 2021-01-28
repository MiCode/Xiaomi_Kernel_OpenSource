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

#define LOG_TAG "LCM"

#include "lcm_drv.h"

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_NT35695 (0xf5)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define MDELAY(n)       (lcm_util.mdelay(n))
#define UDELAY(n)       (lcm_util.udelay(n))

/* ------------------------------------------------------------------------- */
/* Local Functions */
/* ------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/types.h>
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

#ifndef BUILD_LK
static unsigned int GPIO_LCD_RST; /* GPIO45: panel reset pin for control */
static unsigned int GPIO_LCD_PWR_EN; /* GPIO158: panel 1.8V for control */
static unsigned int GPIO_LCD_PWR2_EN; /* GPIO159: panel 2.8V for control */

static void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	pr_notice("[KE/LCM] GPIO_LCD_RST = 0x%x\n", GPIO_LCD_RST);

	GPIO_LCD_PWR_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr_en", 0);
	gpio_request(GPIO_LCD_PWR_EN, "GPIO_LCD_PWR_EN");
	pr_notice("[KE/LCM] GPIO_LCD_PWR_EN = 0x%x\n", GPIO_LCD_PWR_EN);

	GPIO_LCD_PWR2_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr2_en",
					     0);
	gpio_request(GPIO_LCD_PWR2_EN, "GPIO_LCD_PWR2_EN");
	pr_notice("[KE/LCM] GPIO_LCD_PWR2_EN = 0x%x\n", GPIO_LCD_PWR2_EN);
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	lcm_request_gpio_control(dev);

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "tpv,otm1901a",
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
		.name = "otm1901a_fhd_dsi_vdo_tpv",
		.owner = THIS_MODULE,
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_init(void)
{
	if (platform_driver_register(&lcm_driver)) {
		pr_notice("LCM: failed to register this driver!\n");
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

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
/* ------------------------------------------------------------------------- */
/* Local Constants */
/* ------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE                                    0
#define FRAME_WIDTH                                     (1080)
#define FRAME_HEIGHT                                    (1920)
#define GPIO_OUT_ONE  1
#define GPIO_OUT_ZERO 0

/* GPIO158 panel 1.8V */
#ifdef GPIO_LCM_PWR_EN
#define GPIO_LCD_PWR_EN	GPIO_LCM_PWR_EN
#else
#define GPIO_LCD_PWR_EN	0xffffffff
#endif

/* GPIO159 panel 2.8V */
#ifdef GPIO_LCM_PWR2_EN
#define GPIO_LCD_PWR2_EN	GPIO_LCM_PWR2_EN
#else
#define GPIO_LCD_PWR2_EN	0xffffffff
#endif

#define REGFLAG_DELAY	0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x4F, 1, {0x01} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting[] = {
	{0x00, 1, {0x00} },
	{0xFF, 4, {0x19, 0x01, 0x01, 0x00} },
	{0x00, 1, {0x80} },
	{0xFF, 2, {0x19, 0x01} },
	{0x00, 1, {0x00} },
	{0x1C, 1, {0x33} },
	{0x00, 1, {0xA0} },
	{0xC1, 1, {0xE8} },
	{0x00, 1, {0xA7} },
	{0xC1, 1, {0x00} },
	{0x00, 1, {0x90} },
	{0xC0, 6, {0x00, 0x2F, 0x00, 0x00, 0x00, 0x01} },
	{0x00, 1, {0xC0} },
	{0xC0, 6, {0x00, 0x2F, 0x00, 0x00, 0x00, 0x01} },
	{0x00, 1, {0x9A} },
	{0xC0, 1, {0x1E} },
	{0x00, 1, {0xAC} },
	{0xC0, 1, {0x06} },
	{0x00, 1, {0xDC} },
	{0xC0, 1, {0x06} },
	{0x00, 1, {0x81} },
	{0xA5, 1, {0x06} },
	{0x00, 1, {0x82} },
	{0xC4, 1, {0xF0} },
	{0x00, 1, {0x92} },
	{0xE9, 1, {0x00} },
	{0x00, 1, {0x90} },
	{0xF3, 1, {0x01} },
	{0x00, 1, {0x82} },
	{0xA5, 1, {0x1F} },
	{0x00, 1, {0x93} },
	{0xC5, 1, {0x19} },
	{0x00, 1, {0x95} },
	{0xC5, 1, {0x28} },
	{0x00, 1, {0x97} },
	{0xC5, 1, {0x18} },
	{0x00, 1, {0x99} },
	{0xC5, 1, {0x23} },
	{0x00, 1, {0x9B} },
	{0xC5, 2, {0x44, 0x40} },
	{0x00, 1, {0x00} },
	{0xD9, 2, {0x00, 0xBA} },
	{0x00, 1, {0x00} },
	{0xD8, 2, {0x1B, 0x1B} },
	{0x00, 1, {0xB3} },
	{0xC0, 1, {0xCC} },
	{0x00, 1, {0xBC} },
	{0xC0, 1, {0x00} },
	{0x00, 1, {0x84} },
	{0xC4, 1, {0x22} },
	{0x00, 1, {0x94} },
	{0xC1, 1, {0x84} },
	{0x00, 1, {0x98} },
	{0xC1, 1, {0x74} },
	{0x00, 1, {0x80} },
	{0xC4, 1, {0x38} },
	{0x00, 1, {0xCD} },
	{0xF5, 1, {0x19} },
	{0x00, 1, {0xDB} },
	{0xF5, 1, {0x19} },
	{0x00, 1, {0xF5} },
	{0xC1, 1, {0x40} },
	{0x00, 1, {0xB9} },
	{0xC0, 1, {0x11} },
	{0x00, 1, {0x8D} },
	{0xF5, 1, {0x20} },
	{0x00, 1, {0x80} },
	{0xC0, 14, {0x00, 0x86, 0x00, 0x0A, 0x0A, 0x00, 0x86, 0x0A, 0x0A, 0x00,
		    0x86, 0x00, 0x0A, 0x0A} },
	{0x00, 1, {0xF0} },
	{0xC3, 6, {0x00, 0x00, 0x00, 0x00, 0x00, 0x80} },
	{0x00, 1, {0xA0} },
	{0xC0, 7, {0x00, 0x00, 0x03, 0x00, 0x00, 0x1E, 0x06} },
	{0x00, 1, {0xD0} },
	{0xC0, 7, {0x00, 0x00, 0x03, 0x00, 0x00, 0x1E, 0x06} },
	{0x00, 1, {0x90} },
	{0xC2, 4, {0x84, 0x01, 0x3B, 0x40} },
	{0x00, 1, {0xB0} },
	{0xC2, 8, {0x02, 0x01, 0x45, 0x43, 0x02, 0x01, 0x45, 0x43} },
	{0x00, 1, {0x80} },
	{0xC3, 12, {0x84, 0x08, 0x03, 0x00, 0x02, 0x89, 0x82, 0x08, 0x03, 0x00,
		    0x02, 0x89} },
	{0x00, 1, {0x90} },
	{0xC3, 12, {0x83, 0x08, 0x03, 0x00, 0x02, 0x89, 0x81, 0x08, 0x03, 0x00,
		    0x02, 0x89} },
	{0x00, 1, {0x80} },
	{0xCC, 15, {0x09, 0x0D, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		    0x0E, 0x28, 0x28, 0x28, 0x28} },
	{0x00, 1, {0x90} },
	{0xCC, 15, {0x0D, 0x09, 0x14, 0x13, 0x12, 0x11, 0x15, 0x16, 0x17, 0x18,
		    0x0E, 0x28, 0x28, 0x28, 0x28} },
	{0x00, 1, {0xA0} },
	{0xCC, 15, {0x1D, 0x1E, 0x1F, 0x19, 0x1A, 0x1B, 0x1C, 0x20, 0x21, 0x22,
		    0x23, 0x24, 0x25, 0x26, 0x27} },
	{0x00, 1, {0xB0} },
	{0xCC, 8, {0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x04, 0x08} },
	{0x00, 1, {0xC0} },
	{0xCC, 12, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x77} },
	{0x00, 1, {0xD0} },
	{0xCC, 12, {0xFF, 0x0F, 0x30, 0xC0, 0x0F, 0x30, 0x00, 0x00, 0x33, 0x03,
		    0x00, 0x77} },
	{0x00, 1, {0xDE} },
	{0xCC, 1, {0x00} },
	{0x00, 1, {0x80} },
	{0xCB, 15, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x30, 0x00, 0x00, 0x00, 0x00} },
	{0x00, 1, {0x90} },
	{0xCB, 15, {0x30, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00} },
	{0x00, 1, {0xA0} },
	{0xCB, 15, {0x15, 0x15, 0x05, 0xF5, 0x05, 0xF5, 0x00, 0x00, 0x00, 0x00,
		    0x15, 0x00, 0x00, 0x00, 0x00} },
	{0x00, 1, {0xB0} },
	{0xCB, 15, {0x00, 0x01, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00} },
	{0x00, 1, {0xC0} },
	{0xCB, 8, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x77} },
	{0x00, 1, {0xD0} },
	{0xCB, 8, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x77} },
	{0x00, 1, {0xE0} },
	{0xCB, 8, {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x77, 0x77} },
	{0x00, 1, {0xF0} },
	{0xCB, 8, {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x77, 0x77} },
	{0x00, 1, {0x80} },
	{0xCD, 15, {0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x01, 0x12,
		    0x11, 0x03, 0x04, 0x0B, 0x17} },
	{0x00, 1, {0x90} },
	{0xCD, 11, {0x3D, 0x02, 0x3D, 0x25, 0x25, 0x25, 0x1F, 0x20, 0x21, 0x25,
		    0x25} },
	{0x00, 1, {0xA0} },
	{0xCD, 15, {0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x01, 0x12,
		    0x11, 0x05, 0x06, 0x0B, 0x17} },
	{0x00, 1, {0xB0} },
	{0xCD, 11, {0x17, 0x02, 0x3D, 0x25, 0x25, 0x25, 0x1F, 0x20, 0x21, 0x25,
		    0x25} },
	{0x00, 1, {0x00} },
	{0xE1, 24, {0x56, 0x56, 0x59, 0x60, 0x64, 0x67, 0x6d, 0x79, 0x7e, 0x8c,
		    0x93, 0x99, 0x62, 0x5e, 0x5e, 0x4f, 0x3e, 0x2f, 0x24, 0x1d,
		    0x16, 0x0c, 0x08, 0x04} },
	{0x00, 1, {0x00} },
	{0xE2, 24, {0x56, 0x56, 0x59, 0x60, 0x64, 0x67, 0x6d, 0x79, 0x7e, 0x8c,
		    0x93, 0x99, 0x62, 0x5e, 0x5a, 0x4b, 0x3e, 0x2f, 0x24, 0x1d,
		    0x16, 0x0c, 0x08, 0x04} },
	{0x00, 1, {0x00} },
	{0xE3, 24, {0x53, 0x56, 0x58, 0x5c, 0x61, 0x65, 0x6c, 0x77, 0x7c, 0x8b,
		    0x93, 0x99, 0x62, 0x5e, 0x5d, 0x4f, 0x3e, 0x2e, 0x24, 0x1d,
		    0x16, 0x0c, 0x07, 0x04} },
	{0x00, 1, {0x00} },
	{0xE4, 24, {0x53, 0x56, 0x58, 0x5c, 0x61, 0x65, 0x6c, 0x77, 0x7c, 0x8b,
		    0x93, 0x99, 0x62, 0x5e, 0x59, 0x4b, 0x3e, 0x2e, 0x24, 0x1d,
		    0x16, 0x0c, 0x07, 0x04} },
	{0x00, 1, {0x00} },
	{0xE5, 24, {0x20, 0x22, 0x29, 0x35, 0x3f, 0x45, 0x51, 0x63, 0x6e, 0x81,
		    0x8c, 0x95, 0x64, 0x5f, 0x5e, 0x4e, 0x3e, 0x2e, 0x24, 0x1d,
		    0x16, 0x0c, 0x07, 0x04} },
	{0x00, 1, {0x00} },
	{0xE6, 24, {0x20, 0x22, 0x29, 0x35, 0x3f, 0x45, 0x51, 0x63, 0x6e, 0x81,
		    0x8c, 0x95, 0x64, 0x5f, 0x5a, 0x4a, 0x3e, 0x2e, 0x24, 0x1d,
		    0x16, 0x0c, 0x07, 0x04} },
	{0x00, 1, {0xD4} },
	{0xC3, 4, {0x01, 0x01, 0x01, 0x01} },
	{0x00, 1, {0xF7} },
	{0xC3, 4, {0x03, 0x1B, 0x00, 0x00} },
	{0x00, 1, {0xF2} },
	{0xC1, 3, {0x80, 0x0F, 0x0F} },
	{0x00, 1, {0xC2} },
	{0xC5, 1, {0x12} },
	{0x00, 1, {0xA8} },
	{0xC4, 1, {0x11} },
	{0x00, 1, {0x00} },
	{0xFF, 3, {0xFF, 0xFF, 0xFF} },

	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },

	{0x29, 0, {} },
	{REGFLAG_DELAY, 50, {} },
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list,
					force_update);
		}
	}
}

/* ------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* ------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->density = 360;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_frontporch_for_low_power = 620;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */
#ifndef MACH_FPGA
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 420;
#else
	params->dsi.PLL_CLOCK = 440;
#endif
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x53;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;
}

/* ------------------------------------------------------------------------- */
/* Local Functions */
/* ------------------------------------------------------------------------- */
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

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(20);
#else
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
#endif
}

static void lcm_suspend_power(void)
{
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);
}

static void lcm_resume_power(void)
{
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(20);
}

#if 0
static unsigned int lcm_compare_id(void)
{
	int   array[4];
	char  buffer[3];
	char  id0 = 0;
	char  id1 = 0;
	char  id2 = 0;

	SET_RESET_PIN(1);
	MDELAY(2);
	SET_RESET_PIN(0);
	UDELAY(11);
	SET_RESET_PIN(1);
	MDELAY(6);

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDA, buffer, 1);

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDB, buffer + 1, 1);

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDC, buffer + 2, 1);

	id0 = buffer[0]; /* should be 0x40 */
	id1 = buffer[1]; /* should be 0x00 */
	id2 = buffer[2]; /* should be 0x00 */

	pr_notice("%s, id0 = 0x%08x\n", __func__, id0);
	pr_notice("%s, id1 = 0x%08x\n", __func__, id1);
	pr_notice("%s, id2 = 0x%08x\n", __func__, id2);

	return (id0 == 0x40 && id1 == 0x0 && id2 == 0x0) ? 1 : 0;
}
#endif

static void lcm_init_lcm(void)
{
	push_table(init_setting,
		   sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) /
		   sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void)
{
	lcm_init_lcm();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
		       unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

#if 0
/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif
}
#endif

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB,
		 x1_MSB, x1_LSB);

	data_array[0] = 0x0005390A; /* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700; /* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
		&& (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A; /* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

struct LCM_DRIVER otm1901a_fhd_dsi_vdo_tpv_lcm_drv = {
	.name = "otm1901a_fhd_dsi_vdo_tpv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
/*	.compare_id = lcm_compare_id, */
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
/*	.esd_check = lcm_esd_check, */
	.ata_check = lcm_ata_check,
	.update = lcm_update,
};
