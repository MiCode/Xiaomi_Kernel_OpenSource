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
static unsigned int GPIO_LCD_RST_PIN;

/* get LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
	int ret;
	struct regulator *lcm_vgp_ldo;

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		pr_debug("failed to get reg-lcm LDO\n");
		return ret;
	}

	pr_debug("LCM: lcm get supply ok.\n");

	/* get current voltage settings */
	ret = regulator_get_voltage(lcm_vgp_ldo);
	pr_notice("lcm LDO voltage = %d in LK stage\n", ret);

	lcm_vgp = lcm_vgp_ldo;

	return ret;
}

int lcm_vgp_supply_enable(void)
{
	int ret;
	unsigned int volt;

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	if (lcm_vgp == NULL)
		return 0;

	pr_debug("LCM: set regulator voltage lcm_vgp voltage to 1.8V\n");
	/* set voltage to 1.8V */
	ret = regulator_set_voltage(lcm_vgp, 1800000, 1800000);
	if (ret != 0) {
		pr_debug("LCM: lcm failed to set lcm_vgp voltage\n");
		return ret;
	}

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 1800000)
		pr_notice("LCM: check regulator voltage=1800000 pass!\n");
	else
		pr_debug("LCM: voltage=1800000 fail! (voltage: %d)\n",
			volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		pr_debug("LCM: Failed to enable lcm_vgp\n");
		return ret;
	}

	return ret;
}

int lcm_vgp_supply_disable(void)
{
	int ret = 0;
	unsigned int isenable;

	if (lcm_vgp == NULL)
		return 0;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	pr_debug("LCM: lcm query regulator enable status[%d]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			pr_debug("LCM: lcm failed to disable lcm_vgp\n");
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			pr_debug("LCM: lcm regulator disable pass\n");
	}

	return ret;
}

void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_RST_PIN = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST_PIN, "GPIO_LCD_RST_PIN");
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
	 .compatible = "ili9881c,td_tchd5031_6b",
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
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		.name = "ili9881c_hd720_td_tchd5031_6b_dsi_vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_init(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	if (platform_driver_register(&lcm_driver)) {
		pr_debug("LCM: failed to register this driver!\n");
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

/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT		(1280)

#define GPIO_OUT_ONE		1
#define GPIO_OUT_ZERO		0

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* ----------------------------------------------------------------- */
/* Local Functions */
/* ----------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */
#define REGFLAG_DELAY		0xFC
#define REGFLAG_END_OF_TABLE	0xFD	/* END OF REGISTERS MARKER */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* ----------------------------------------------------------------- */
/* Local Variables */
/* ----------------------------------------------------------------- */
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	if (GPIO == 0xFFFFFFFF) {
		pr_debug("[Kernel/LCM] LCM_RST = 0x%x\n", GPIO_LCD_RST_PIN);
		return;
	}
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#else
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
#endif
}

struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

#ifndef BUILD_LK
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

static struct LCM_setting_table lcm_init_setting[] = {
	{0xFF, 3, {0x98, 0x81, 0x03} },
	{0x01, 1, {0x00} },
	{0x02, 1, {0x00} },
	{0x03, 1, {0x53} },
	{0x04, 1, {0x13} },
	{0x05, 1, {0x13} },
	{0x06, 1, {0x06} },
	{0x07, 1, {0x00} },
	{0x08, 1, {0x04} },
	{0x09, 1, {0x00} },
	{0x0a, 1, {0x00} },
	{0x0b, 1, {0x00} },
	{0x0c, 1, {0x00} },
	{0x0d, 1, {0x00} },
	{0x0e, 1, {0x00} },
	{0x0f, 1, {0x00} },
	{0x10, 1, {0x00} },
	{0x11, 1, {0x00} },
	{0x12, 1, {0x00} },
	{0x13, 1, {0x00} },
	{0x14, 1, {0x00} },
	{0x15, 1, {0x00} },
	{0x16, 1, {0x00} },
	{0x17, 1, {0x00} },
	{0x18, 1, {0x00} },
	{0x19, 1, {0x00} },
	{0x1a, 1, {0x00} },
	{0x1b, 1, {0x00} },
	{0x1c, 1, {0x00} },
	{0x1d, 1, {0x00} },

	{0x1e, 1, {0xC0} },
	{0x1f, 1, {0x80} },
	{0x20, 1, {0x04} },
	{0x21, 1, {0x0B} },
	{0x22, 1, {0x00} },
	{0x23, 1, {0x00} },
	{0x24, 1, {0x00} },
	{0x25, 1, {0x00} },
	{0x26, 1, {0x00} },
	{0x27, 1, {0x00} },
	{0x28, 1, {0x55} },
	{0x29, 1, {0x03} },
	{0x2a, 1, {0x00} },
	{0x2b, 1, {0x00} },
	{0x2c, 1, {0x00} },
	{0x2d, 1, {0x00} },
	{0x2e, 1, {0x00} },
	{0x2f, 1, {0x00} },
	{0x30, 1, {0x00} },
	{0x31, 1, {0x00} },
	{0x32, 1, {0x00} },
	{0x33, 1, {0x00} },

	{0x34, 1, {0x04} },
	{0x35, 1, {0x05} },
	{0x36, 1, {0x05} },
	{0x37, 1, {0x00} },
	{0x38, 1, {0x3C} },
	{0x39, 1, {0x00} },
	{0x3a, 1, {0x40} },
	{0x3b, 1, {0x40} },

	{0x3c, 1, {0x00} },
	{0x3d, 1, {0x00} },
	{0x3e, 1, {0x00} },
	{0x3f, 1, {0x00} },
	{0x40, 1, {0x00} },
	{0x41, 1, {0x00} },
	{0x42, 1, {0x00} },
	{0x43, 1, {0x00} },
	{0x44, 1, {0x00} },

	{0x50, 1, {0x01} },
	{0x51, 1, {0x23} },
	{0x52, 1, {0x45} },
	{0x53, 1, {0x67} },
	{0x54, 1, {0x89} },
	{0x55, 1, {0xAB} },
	{0x56, 1, {0x01} },
	{0x57, 1, {0x23} },
	{0x58, 1, {0x45} },
	{0x59, 1, {0x67} },
	{0x5A, 1, {0x89} },
	{0x5B, 1, {0xAB} },
	{0x5C, 1, {0xCD} },
	{0x5D, 1, {0xEF} },

	{0x5E, 1, {0x01} },
	{0x5F, 1, {0x14} },
	{0x60, 1, {0x15} },
	{0x61, 1, {0x0C} },
	{0x62, 1, {0x0D} },
	{0x63, 1, {0x0E} },
	{0x64, 1, {0x0F} },
	{0x65, 1, {0x10} },
	{0x66, 1, {0x11} },
	{0x67, 1, {0x08} },
	{0x68, 1, {0x02} },
	{0x69, 1, {0x0A} },
	{0x6A, 1, {0x02} },
	{0x6B, 1, {0x02} },
	{0x6C, 1, {0x02} },
	{0x6D, 1, {0x02} },
	{0x6E, 1, {0x02} },
	{0x6F, 1, {0x02} },
	{0x70, 1, {0x02} },
	{0x71, 1, {0x02} },
	{0x72, 1, {0x06} },
	{0x73, 1, {0x02} },
	{0x74, 1, {0x02} },
	{0x75, 1, {0x14} },
	{0x76, 1, {0x15} },
	{0x77, 1, {0x11} },
	{0x78, 1, {0x10} },
	{0x79, 1, {0x0F} },
	{0x7A, 1, {0x0E} },
	{0x7B, 1, {0x0D} },
	{0x7C, 1, {0x0C} },
	{0x7D, 1, {0x06} },
	{0x7E, 1, {0x02} },
	{0x7F, 1, {0x0A} },
	{0x80, 1, {0x02} },
	{0x81, 1, {0x02} },
	{0x82, 1, {0x02} },
	{0x83, 1, {0x02} },
	{0x84, 1, {0x02} },
	{0x85, 1, {0x02} },
	{0x86, 1, {0x02} },
	{0x87, 1, {0x02} },
	{0x88, 1, {0x08} },
	{0x89, 1, {0x02} },
	{0x8A, 1, {0x02} },

	{0xFF, 3, {0x98, 0x81, 0x04} },
	{0x6C, 1, {0x15} },
	{0x6E, 1, {0x3B} },
	{0x6F, 1, {0x53} },
	{0x3A, 1, {0xA4} },
	{0x8D, 1, {0x15} },
	{0x87, 1, {0xBA} },
	{0x26, 1, {0x76} },
	{0XB2, 1, {0XD1} },
	{0X88, 1, {0X0B} },

	{0xFF, 3, {0x98, 0x81, 0x01} },
	{0x22, 1, {0x0A} },
	{0x31, 1, {0x00} },
	{0x53, 1, {0x96} },
	{0x55, 1, {0x88} },
	{0x50, 1, {0x96} },
	{0x51, 1, {0x96} },
	{0x60, 1, {0x14} },

	{0xA0, 1, {0x08} },
	{0xA1, 1, {0x1C} },
	{0xA2, 1, {0x29} },
	{0xA3, 1, {0x13} },
	{0xA4, 1, {0x16} },
	{0xA5, 1, {0x28} },
	{0xA6, 1, {0x1C} },
	{0xA7, 1, {0x1D} },
	{0xA8, 1, {0x80} },
	{0xA9, 1, {0x1a} },
	{0xAA, 1, {0x27} },
	{0xAB, 1, {0x6A} },
	{0xAC, 1, {0x1a} },
	{0xAD, 1, {0x19} },
	{0xAE, 1, {0x4b} },
	{0xAF, 1, {0x21} },
	{0xB0, 1, {0x25} },
	{0xB1, 1, {0x4A} },
	{0xB2, 1, {0x59} },
	{0xB3, 1, {0x2C} },

	{0xC0, 1, {0x08} },
	{0xC1, 1, {0x1C} },
	{0xC2, 1, {0x29} },
	{0xC3, 1, {0x13} },
	{0xC4, 1, {0x17} },
	{0xC5, 1, {0x28} },
	{0xC6, 1, {0x1C} },
	{0xC7, 1, {0x1D} },
	{0xC8, 1, {0x80} },
	{0xC9, 1, {0x1a} },
	{0xCA, 1, {0x27} },
	{0xCB, 1, {0x6A} },
	{0xCC, 1, {0x1A} },
	{0xCD, 1, {0x19} },
	{0xCE, 1, {0x4b} },
	{0xCF, 1, {0x21} },
	{0xD0, 1, {0x25} },
	{0xD1, 1, {0x4A} },
	{0xD2, 1, {0x5B} },
	{0xD3, 1, {0x2C} },

	{0xFF, 3, {0x98, 0x81, 0x00} },

	{0x35, 1, {0x00} },

	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} },

	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void dsi_send_cmdq_tinno(unsigned int cmd, unsigned char count,
	unsigned char *para_list, unsigned char force_update)
{
	unsigned int item[16];
	unsigned char dsi_cmd = (unsigned char)cmd;
	unsigned char dc;
	int index = 0, length = 0;

	memset(item, 0, sizeof(item));
	if (count + 1 > 60)
		return;

	if (count == 0) {
		item[0] = 0x0500 | (dsi_cmd << 16);
		length = 1;
	} else if (count == 1) {
		item[0] = 0x1500 | (dsi_cmd << 16) | (para_list[0] << 24);
		length = 1;
	} else {
		item[0] = 0x3902 | ((count + 1) << 16);
		++length;
		while (1) {
			if (index == count + 1)
				break;
			if (index == 0)
				dc = cmd;
			else
				dc = para_list[index - 1];
			/* an item make up of 4data. */
			item[index / 4 + 1] |= (dc << (8 * (index % 4)));
			if (index % 4 == 0)
				++length;
			++index;
		}
	}

	dsi_set_cmdq(item, length, force_update);
}


static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_send_cmdq_tinno(cmd,
				table[i].count,
				table[i].para_list,
				force_update);
		}
	}
}

static void init_lcm_registers(void)
{
	push_table(lcm_init_setting,
		sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table),
		1);
}

/* ----------------------------------------------------------------- */
/* LCM Driver Implementations */
/* ----------------------------------------------------------------- */
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
	/* params->density = 213; */
#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
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

	params->dsi.intermediat_buffer_num = 2;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

#ifdef BUILD_LK
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
#endif
	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 16;
	params->dsi.vertical_frontporch = 40;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 80;
	params->dsi.horizontal_frontporch = 80;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	/* video mode timing */
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.PLL_CLOCK = 238;
}

static void lcm_init_lcm(void)
{
#ifndef BUILD_LK
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
#endif
}

void lcm_suspend(void)
{
#ifndef BUILD_LK
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	push_table(lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);

	lcm_set_gpio_output(GPIO_LCD_RST_PIN, GPIO_OUT_ZERO);
	MDELAY(5);

	lcm_vgp_supply_disable();
#endif
}

void lcm_resume(void)
{
#ifndef BUILD_LK
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_vgp_supply_enable();
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST_PIN, GPIO_OUT_ZERO);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST_PIN, GPIO_OUT_ONE);
	MDELAY(5);

	init_lcm_registers();
#endif
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
	return 1;
}

struct LCM_DRIVER ili9881c_hd720_td_tchd5031_6b_dsi_vdo_lcm_drv = {
	.name = "ili9881c_hd720_td_tchd5031_6b_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.ata_check = lcm_ata_check,
};
