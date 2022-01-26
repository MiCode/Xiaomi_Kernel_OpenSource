/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

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
#include "lcm_drv.h"

/*
 * Panel: 5.46FHD+(1080*1920) AUO
 * Driver IC: R63350A
 * Interface: MIPI  4LANES
 * Test condition :IOVCC=1.8V
 */

/* ------------------------------------------------------------------------- */
/*  Local Constants */
/* ------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE                    0
#define FRAME_WIDTH                         (1080)
#define FRAME_HEIGHT                        (1920)
#define GPIO_OUT_ONE                        1
#define GPIO_OUT_ZERO                       0

#define REGFLAG_DELAY                       0xFE
#define REGFLAG_END_OF_TABLE                0xFF

#ifndef BUILD_LK
static unsigned int GPIO_LCD_RST; /* GPIO45: panel reset pin for control */
static unsigned int GPIO_LCD_PWR_EN; /* GPIO158: panel 1.8V for control */
static unsigned int GPIO_LCD_PWR2_EN; /* GPIO159: panel 2.8V for control */
static unsigned int GPIO_LCD_PWR; /* GPIO166: panel power enable */

static void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	pr_notice("[KE/LCM] GPIO_LCD_RST = 0x%x\n", GPIO_LCD_RST);

	GPIO_LCD_PWR = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr", 0);
	gpio_request(GPIO_LCD_PWR, "GPIO_LCD_PWR");
	pr_notice("[KE/LCM] GPIO_LCD_PWR = 0x%x\n", GPIO_LCD_PWR);

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
		.compatible = "truly,r63350a",
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
		.name = "r63350a_fhd_dsi_vdo_truly",
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

/* ------------------------------------------------------------------------- */
/*  Local Variables */
/* ------------------------------------------------------------------------- */
static struct LCM_UTIL_FUNCS lcm_util = {0};
#define SET_RESET_PIN(v)                    (lcm_util.set_reset_pin((v)))
#define UDELAY(n)                           (lcm_util.udelay(n))
#define MDELAY(n)                           (lcm_util.mdelay(n))

/* ------------------------------------------------------------------------- */
/*  Local Functions */
/* ------------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)	lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg	lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifdef BUILD_LK
#define LCM_LOGI(string, args...) \
		dprintf(CRITICAL, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(INFO, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	pr_notice("[KERNEL/LCM] GPIO_LCD_RST = 0x%x\n", GPIO_LCD_RST);
	pr_notice("[KERNEL/LCM] GPIO_LCD_PWR_EN = 0x%x\n", GPIO_LCD_PWR_EN);
	pr_notice("[KERNEL/LCM] GPIO_LCD_PWR2_EN = 0x%x\n", GPIO_LCD_PWR2_EN);

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_set_value(GPIO, output);
#endif
}

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xB0, 1, {0x00} },
	{0xD6, 1, {0x01} },
	{0xB3, 6, {0x14, 0x00, 0x00, 0x00, 0x00, 0x00} },
	{0xB4, 2, {0x0C, 0x00} },
	{0xB6, 3, {0x4B, 0xDB, 0x16} },/* 0xCB,0x16 */
	{0xBE, 2, {0x00, 0x04} },
	{0xC0, 1, {0x00} },
	{0xC1, 34, {0x04, 0x60, 0x00, 0x20, 0xA9, 0x30, 0x20, 0x63,
				0xF0, 0xFF, 0xFF, 0x9B, 0x7B, 0xCF, 0xB5, 0xFF,
				0xFF, 0x87, 0x8C, 0x41, 0x22, 0x54, 0x02, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x22, 0x33, 0x03, 0x22,
				0x00, 0xFF} },
	{0xC2, 8, {0x31, 0xf7, 0x80, 0x06, 0x04, 0x00, 0x00, 0x08} },
	{0xC3, 3, {0x00, 0x00, 0x00} },
	{0xC4, 11, {0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x02} },
	/* reduce noise*/
	/* {0xC4, 11, {0x70, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x44,
	 *			0x44, 0x00, 0x04} },
	 */
	{0xC5, 1, {0x00} },
	{0xC6, 21, {0xC8, 0x3C, 0x3C, 0x07, 0x01, 0x07, 0x01, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x0E, 0x1A, 0x07, 0xC8} },
	{0xC7, 30, {0x03, 0x15, 0x1F, 0x2A, 0x39, 0x46, 0x4E, 0x5B,
				0x3D, 0x45, 0x52, 0x5F, 0x68, 0x6D, 0x72, 0x01,
				0x15, 0x1F, 0x2A, 0x39, 0x46, 0x4E, 0x5B, 0x3D,
				0x45, 0x52, 0x5F, 0x68, 0x6D, 0x78} },
	{0xCB, 15, {0xFF, 0xE1, 0x87, 0xFF, 0x00, 0x00, 0x00, 0x00,
				0xFF, 0xE1, 0x87, 0xFF, 0xE8, 0x00, 0x00} },
	{0xCC, 1, {0x34} },
	{0xD0, 10, {0x11, 0x00, 0x00, 0x56, 0xD5, 0x40, 0x19, 0x19,
				0x09, 0x00} },
	{0xD1, 4, {0x00, 0x48, 0x16, 0x0F} },
	{0xD2, 3, {0x5C, 0x00, 0x00} },
	{0xD3, 26, {0x1B, 0x33, 0xBB, 0xBB, 0xB3, 0x33, 0x33, 0x33,
				0x33, 0x00, 0x01, 0x00, 0x00, 0xD8, 0xA0, 0x0C,
				0x4D, 0x4D, 0x33, 0x33, 0x72, 0x12, 0x8A, 0x57,
				0x3D, 0xBC} },
	{0xD5, 7, {0x06, 0x00, 0x00, 0x01, 0x39, 0x01, 0x39} },
	{0xD8, 3, {0x00, 0x00, 0x00} },
	{0xD9, 3, {0x00, 0x00, 0x00} },
	{0xFD, 4, {0x00, 0x00, 0x00, 0x30} },
	{0x35, 1, {0x00} },
	/* Test revert */
	/* {0x36, 1, {0xC0} }, */
	{0x51, 1, {0xff} },
	/*  Write CTRL Display */
	{0x53, 1, {0x24} },
	/* Write Display Brightness */
	{0x55, 1, {0x00} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 50, {} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

#if 0
static struct LCM_setting_table lcm_resume_setting[] = {
	/* Exit sleep mode */
	{0x11, 0, {} },
	{REGFLAG_DELAY, 150, {} },

	/* DISPLAY ON */
	{0x29, 0, {} },
	{REGFLAG_DELAY, 50, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

static struct LCM_setting_table lcm_suspend_setting[] = {
	/* DISPLAY OFF */
	{0x28, 0, {} },
	{REGFLAG_DELAY, 50, {} },
	/* Enter sleep mode */
	{0x10, 0, {} },
	{REGFLAG_DELAY, 150, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
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
/*  LCM Driver Implementations */
/* ------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type    = LCM_TYPE_DSI;

	params->width   = FRAME_WIDTH;
	params->height  = FRAME_HEIGHT;
	/* params->density = 380; */

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
#endif

	params->dsi.LANE_NUM                = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding		= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	/* Not support in MT6573 */
	params->dsi.packet_size = 256;

	/* Video mode setting */
	params->dsi.intermediat_buffer_num = 0;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active	= 2;
	params->dsi.vertical_backporch		= 4;
	params->dsi.vertical_frontporch		= 10;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active	= 20;
	params->dsi.horizontal_backporch	= 60;
	params->dsi.horizontal_frontporch	= 90;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;

	params->dsi.PLL_CLOCK			= 463;
	params->dsi.cont_clock			= 1;
}


static void lcm_init_power(void)
{
#ifdef BUILD_LK
	pr_notice("[LK/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(20);
#else
	pr_notice("[KERNEL/LCM] %s enter\n", __func__);
#endif
}

static void lcm_suspend_power(void)
{
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);
	MDELAY(20);
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ZERO);
}

static void lcm_resume_power(void)
{
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);
	MDELAY(20);

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
	char  buffer[5];
	char  id0 = 0;
	char  id1 = 0;
	char  id2 = 0;
	char  id3 = 0;
	char  id4 = 0;

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(2);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	UDELAY(11);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(6);

	array[0] = 0x00053700;
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xBF, buffer, 5);

	id0 = buffer[0]; /* should be 0x02 */
	id1 = buffer[1]; /* should be 0x3C */
	id2 = buffer[2]; /* should be 0x33 */
	id3 = buffer[3]; /* should be 0x50 */
	id4 = buffer[4]; /* should be 0x00 */

	pr_notice("%s, id0 = 0x%08x\n", __func__, id0);
	pr_notice("%s, id1 = 0x%08x\n", __func__, id1);
	pr_notice("%s, id2 = 0x%08x\n", __func__, id2);
	pr_notice("%s, id3 = 0x%08x\n", __func__, id3);
	pr_notice("%s, id4 = 0x%08x\n", __func__, id4);

	return (id2 == 0x33 && id3 == 0x50) ? 1 : 0;
}
#endif

static void lcm_init_lcm(void)
{
	pr_notice("--------[KERNEL] r63350a lcm_init enter--------\n");
	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) /
		   sizeof(struct LCM_setting_table), 1);
	pr_notice("--------[KERNEL] r63350a lcm_init exit--------\n");
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

struct LCM_DRIVER r63350a_fhd_dsi_vdo_truly_lcm_drv = {
	.name           = "r63350a_fhd_dsi_vdo_truly",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init_lcm,
	.resume         = lcm_resume,
	.suspend        = lcm_suspend,
	.init_power     = lcm_init_power,
	.resume_power   = lcm_resume_power,
	.suspend_power  = lcm_suspend_power,
/*	.compare_id = lcm_compare_id, */
};

