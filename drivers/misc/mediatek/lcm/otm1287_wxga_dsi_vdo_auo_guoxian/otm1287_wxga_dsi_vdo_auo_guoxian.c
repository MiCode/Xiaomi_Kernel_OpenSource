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
static unsigned int GPIO_LCD_RST;
struct pinctrl *lcd_pinctrl;
struct pinctrl_state *lcd_disp_pwm;
struct pinctrl_state *lcd_disp_pwm_gpio;
static struct regulator *lcm_vgp;

static void lcm_request_gpio_control(struct device *dev)
{
	int ret;

	pr_notice("[Kernel/LCM] otm1287: %s\n", __func__);

	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");

	lcd_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(lcd_pinctrl)) {
		ret = PTR_ERR(lcd_pinctrl);
		pr_notice(" Cannot find lcd_pinctrl %d!\n", ret);
	}

	lcd_disp_pwm = pinctrl_lookup_state(lcd_pinctrl, "disp_pwm");
	if (IS_ERR(lcd_pinctrl)) {
		ret = PTR_ERR(lcd_pinctrl);
		pr_notice(" Cannot find lcd_disp_pwm %d!\n", ret);
	}

	lcd_disp_pwm_gpio = pinctrl_lookup_state(lcd_pinctrl, "disp_pwm_gpio");
	if (IS_ERR(lcd_pinctrl)) {
		ret = PTR_ERR(lcd_pinctrl);
		pr_notice(" Cannot find lcd_disp_pwm_gpio %d!\n", ret);
	}
}

/* get LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
	int ret;
	struct regulator *lcm_vgp_ldo;

	pr_notice("[Kernel/LCM] otm1287: %s\n", __func__);

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		dev_notice(dev, "failed to get reg-lcm LDO, %d\n", ret);
		return ret;
	}

	pr_notice("LCM: lcm get supply ok.\n");

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

	pr_notice("[Kernel/LCM] otm1287: %s\n", __func__);

	if (lcm_vgp == NULL)
		return 0;

	pr_notice("LCM: set regulator voltage lcm_vgp voltage to 3.3V\n");
	/* set voltage to 1.8V */
	ret = regulator_set_voltage(lcm_vgp, 3300000, 3300000);
	if (ret != 0) {
		pr_notice("LCM: lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 3300000)
		pr_notice("LCM: check regulator voltage=3300000 pass!\n");
	else
		pr_debug("LCM: check regulator vol=3300000 fail! (vol: %d)\n",
			volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		pr_notice("LCM: Failed to enable lcm_vgp: %d\n", ret);
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

	pr_notice("LCM: lcm query regulator enable status[%d]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			pr_notice("LCM: failed to disable lcm_vgp: %d\n", ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			pr_notice("LCM: lcm regulator disable pass\n");
	}

	return ret;
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	pr_notice("[Kernel/LCM] otm1287: %s\n", __func__);

	lcm_request_gpio_control(dev);
	lcm_get_vgp_supply(dev);
	lcm_vgp_supply_enable();

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "otm,otm1287_wxga_dsi_vdo_auo_guoxian",
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
		   .name = "otm1287_wxga_dsi_vdo_auo_guoxian",
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

/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */
#define FRAME_WIDTH		(800)
#define FRAME_HEIGHT	(1280)
#define LCM_DSI_CMD_MODE	0

#define GPIO_OUT_ONE	1
#define GPIO_OUT_ZERO	0

#define REGFLAG_DELAY	0xFE
#define REGFLAG_END_OF_TABLE	0xFF   /* END OF REGISTERS MARKER */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* ----------------------------------------------------------------- */
/* Local Variables */
/* ----------------------------------------------------------------- */
static struct LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))

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
#define read_reg lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef ASSERT
#define ASSERT(expr)					\
	do {						\
		if (expr)				\
			break;				\
		pr_debug("DDP ASSERT FAILED %s, %d\n",	\
		       __FILE__, __LINE__);		\
		WARN_ON();					\
	} while (0)
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
#endif
}

static void init_lcm_registers(void)
{
	unsigned int data_array[16];

	/* SET password */
	data_array[0] = 0x00043902;
	data_array[1] = 0x9483FFB9;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00033902;
	data_array[1] = 0x008333BA;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00053902;
	data_array[1] = 0x7d0000b0;
	data_array[2] = 0x0000000c;
	dsi_set_cmdq(data_array, 3, 1);
	MDELAY(10);

	/* Set Power */
	data_array[0] = 0x00103902;
	data_array[1] = 0x15156cB1;
	data_array[2] = 0xf1110424;
	data_array[3] = 0x2397E480;
	data_array[4] = 0x58D2C080;
	dsi_set_cmdq(data_array, 5, 1);
	MDELAY(10);

	/* SET CYC */
	data_array[0] = 0x000C3902;
	data_array[1] = 0x106400B2;
	data_array[2] = 0x081C2207;
	data_array[3] = 0x004D1C08;
	dsi_set_cmdq(data_array, 4, 1);
	MDELAY(10);

	/* SET CYC */
	data_array[0] = 0x000D3902;
	data_array[1] = 0x03FF00B4;
	data_array[2] = 0x035A035A;
	data_array[3] = 0x306a015A;
	data_array[4] = 0x0000006a;
	dsi_set_cmdq(data_array, 5, 1);
	MDELAY(10);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000007BC;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00043902;
	data_array[1] = 0x010E41BF;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	/* Set VCOM */
	data_array[0] = 0x00033902;
	data_array[1] = 0x005c5cB6;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	/* Set panel */
	data_array[0] = 0x00023902;
	data_array[1] = 0x000009CC;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	/* SET GIP */
	data_array[0] = 0x001F3902;
	data_array[1] = 0x000600D3;
	data_array[2] = 0x00080740;
	data_array[3] = 0x00071032;
	data_array[4] = 0x0F155407;
	data_array[5] = 0x12020405;
	data_array[6] = 0x33070510;
	data_array[7] = 0x370B0B33;
	data_array[8] = 0x00070710;
	dsi_set_cmdq(data_array, 9, 1);
	MDELAY(10);

	/* SET GIP */
	data_array[0] = 0x002d3902;
	data_array[1] = 0x060504D5;
	data_array[2] = 0x02010007;
	data_array[3] = 0x22212003;
	data_array[4] = 0x18181823;
	data_array[5] = 0x18181818;
	data_array[6] = 0x18191918;
	data_array[7] = 0x1B181818;
	data_array[8] = 0x181A1A1B;
	data_array[9] = 0x18181818;
	data_array[10] = 0x18181818;
	data_array[11] = 0x18181818;
	data_array[12] = 0x00000018;
	dsi_set_cmdq(data_array, 13, 1);
	MDELAY(10);

	/* SET GIP */
	data_array[0] = 0x002D3902;
	data_array[1] = 0x010203D6;
	data_array[2] = 0x05060700;
	data_array[3] = 0x21222304;
	data_array[4] = 0x18181820;
	data_array[5] = 0x58181818;
	data_array[6] = 0x19181858;
	data_array[7] = 0x1B181819;
	data_array[8] = 0x181A1A1B;
	data_array[9] = 0x18181818;
	data_array[10] = 0x18181818;
	data_array[11] = 0x18181818;
	data_array[12] = 0x00000018;
	dsi_set_cmdq(data_array, 13, 1);
	MDELAY(10);

	/* R Gamma */
	data_array[0] = 0x002B3902;
	data_array[1] = 0x161000E0;
	data_array[2] = 0x233F332D;
	data_array[3] = 0x0D0B073E;
	data_array[4] = 0x14120E17;
	data_array[5] = 0x11061312;
	data_array[6] = 0x10001813;
	data_array[7] = 0x3F332D16;
	data_array[8] = 0x0B073E23;
	data_array[9] = 0x120E170D;
	data_array[10] = 0x06131214;
	data_array[11] = 0x00181311;
	dsi_set_cmdq(data_array, 12, 1);
	MDELAY(10);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001430C0;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00053902;
	data_array[1] = 0x40C000C7;
	data_array[2] = 0x000000C0;
	dsi_set_cmdq(data_array, 3, 1);
	MDELAY(10);

	data_array[0] = 0x00023902;
	data_array[1] = 0x00008edf;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000066d2;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(200);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(50);
}

static struct LCM_setting_table sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 0, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table sleep_in_setting[] = {
	/* Sleep Mode On */
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
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
			dsi_set_cmdq_V2(cmd, table[i].count,
				table[i].para_list, force_update);
		}
	}
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

	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->density = 160;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = BURST_VDO_MODE;
#endif

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding	= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format	= LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* Video mode timing */
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active    = 4;/* 2; */
	params->dsi.vertical_backporch      = 12;/* 16; */
	params->dsi.vertical_frontporch     = 20;/* 9; */
	params->dsi.vertical_active_line    = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active  = 20;/* 42; */
	params->dsi.horizontal_backporch    = 20;/* 42; */
	params->dsi.horizontal_frontporch   = 30;/* 69; */
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.ssc_disable	= 1;
	params->dsi.cont_clock	= 0;
	params->dsi.PLL_CLOCK   = 219;

	params->dsi.clk_lp_per_line_enable   = 0;
	/* params->dsi.esd_check_enable = 0; */
	/* params->dsi.customization_esd_check_enable = 0; */
	/* params->dsi.lcm_esd_check_table[0].cmd = 0x53; */
	/* params->dsi.lcm_esd_check_table[0].count = 1; */
	/* params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24; */
}

static void lcm_init_lcm(void)
{
	pr_notice("[Kernel/LCM] otm1287: %s\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(1);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(5);

	pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm);

	init_lcm_registers();
}

static void lcm_suspend(void)
{
	pr_notice("[Kernel/LCM] otm1287: %s\n", __func__);

	push_table(sleep_in_setting,
		sizeof(sleep_in_setting) / sizeof(struct LCM_setting_table),
		1);

	pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm_gpio);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	lcm_vgp_supply_disable();
	MDELAY(20);
}

static void lcm_resume(void)
{
	pr_notice("[Kernel/LCM] otm1287: %s\n", __func__);

	lcm_vgp_supply_enable();
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(1);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(5);
	pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm);
	MDELAY(20);

	push_table(sleep_out_setting,
		sizeof(sleep_out_setting) / sizeof(struct LCM_setting_table),
		1);
}

struct LCM_DRIVER otm1287_wxga_dsi_vdo_auo_guoxian_lcm_drv = {
	.name		= "otm1287_wxga_dsi_vdo_auo_guoxian",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init_lcm,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
};

