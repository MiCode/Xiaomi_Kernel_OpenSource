/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/hwinfo.h>
#include <asm-generic/gpio.h>

#include "lcm_define.h"
#include "lcm_common.h"
#include "lcm_i2c.h"
#include "ddp_irq.h"

int tps65132_write_bytes(unsigned char addr, unsigned char value);

#define LOG_TAG "LCM"
#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static unsigned int GPIO_LCD_VSP_EN;
static unsigned int GPIO_LCD_VSN_EN;
static unsigned int GPIO_LCD_RST_EN;
static unsigned int GPIO_LCD_RST_EN_NEW;
static unsigned int GPIO_LCD_1V8_EN;

/**
 * Local Constants
 */
#define FRAME_WIDTH             (1536)
#define FRAME_HEIGHT            (2048)

#define GPIO_OUT_ONE            1
#define GPIO_OUT_ZERO           0

#define REGFLAG_DELAY           0xFE
#define REGFLAG_END_OF_TABLE    0xFF   /* END OF REGISTERS MARKER */

#define LCM_DSI_CMD_MODE        0

/**
 * Local Variables
 */
static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))
#define MDELAY(n) (lcm_util.mdelay(n))
#define UDELAY(n) (lcm_util.udelay(n))


/**
 * Local Functions
 */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)    lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                                   lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)               lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg                                         lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)            lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

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
	/* Sleep Out */
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x05} },
	{0xCE, 2, {0xF0, 0x1F} },
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x01} },
	{0xCE, 1, {0x06} },
	{0xD7, 2, {0x00, 0x2F} },
	{0x51, 1, {0xFF} },
	{0x55, 1, {0x02} },
	{0x53, 1, {0x24} },
	/* Display On */
	{0x29, 0, {} },
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x00, 0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Sleep Mode On */
	{0x53, 1, {0x00} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 100, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 10, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

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
void lcm_get_gpio_infor(void)
{
	int ret;
	static struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-lcm");

	GPIO_LCD_1V8_EN = of_get_named_gpio(node, "gpio_lcm_1v8_en", 0);
	GPIO_LCD_VSP_EN = of_get_named_gpio(node, "gpio_lcm_vsp_en", 0);
	GPIO_LCD_VSN_EN = of_get_named_gpio(node, "gpio_lcm_vsn_en", 0);
	GPIO_LCD_RST_EN = of_get_named_gpio(node, "gpio_lcm_rst_en", 0);
	GPIO_LCD_RST_EN_NEW = of_get_named_gpio(node, "gpio_lcm_rst_en_new", 0);

	ret = gpio_request(GPIO_LCD_1V8_EN, "GPIO_LCD_1V8_EN");
	if (ret) {
		LCM_LOGD("gpio request GPIO_LCD_1V8_EN = 0x%x fail with %d\n", GPIO_LCD_1V8_EN, ret);
		goto out;
	}

	ret = gpio_request(GPIO_LCD_VSP_EN, "GPIO_LCD_VSP_EN");
	if (ret) {
		LCM_LOGD("gpio request GPIO_LCD_VSP_EN = 0x%x fail with %d\n", GPIO_LCD_VSP_EN, ret);
		goto out;
	}

	ret = gpio_request(GPIO_LCD_VSN_EN, "GPIO_LCD_VSN_EN");
	if (ret) {
		LCM_LOGD("gpio request GPIO_LCD_VSN_EN = 0x%x fail with %d\n", GPIO_LCD_VSN_EN, ret);
		goto out;
	}

	ret = gpio_request(GPIO_LCD_RST_EN, "GPIO_LCD_RST_EN");
	if (ret) {
		LCM_LOGD("gpio request GPIO_LCD_RST_EN = 0x%x fail with %d\n", GPIO_LCD_RST_EN, ret);
		goto out;
	}

	ret = gpio_request(GPIO_LCD_RST_EN_NEW, "GPIO_LCD_RST_EN_NEW");
	if (ret) {
		LCM_LOGD("gpio request GPIO_LCD_RST_EN_NEW = 0x%x fail with %d\n", GPIO_LCD_RST_EN_NEW, ret);
		goto out;
	}

out:
	return;
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
}

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	LCM_LOGI("%s enter\n", __func__);
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->lcm_if = LCM_INTERFACE_DSI_DUAL;
	params->lcm_cmd_if = LCM_INTERFACE_DSI_DUAL;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = BURST_VDO_MODE;
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

	params->dsi.horizontal_sync_active = 2;
	params->dsi.horizontal_backporch = 55;
	params->dsi.horizontal_frontporch = 55;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.vertical_vfp_lp = 200; /* For dynamic fps, in idle LP mode set fps to 55Hz */

	params->dsi.ssc_disable = 1;
	params->dsi.cont_clock = 0;
	params->dsi.PLL_CLOCK = 395;
	params->dsi.HS_TRAIL = 1; /* 2; */
	params->dsi.CLK_TRAIL = 1; /* 8; */
	params->dsi.clk_lp_per_line_enable = 1;

	params->dsi.ufoe_enable = 1;
	params->dsi.ufoe_params.lr_mode_en = 1;

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}


static void lcm_init(void)
{
	LCM_LOGI("%s enter\n", __func__);
	lcm_get_gpio_infor();
	update_hardware_info(TYPE_PANEL, 5);
}

extern int ddp_dsi_power_off(DISP_MODULE_ENUM module, void *cmdq_handle);
extern int primary_display_set_panel_param(unsigned int param);

static void lcm_dimming_enable_delayed_work(struct work_struct *work)
{
	LCM_LOGI("turn on lcm dimming\n");
	primary_display_set_panel_param(0x500);
}

static DECLARE_DELAYED_WORK(open_lcm_dimming, lcm_dimming_enable_delayed_work);

static void lcm_suspend(void)
{
	LCM_LOGI("%s enter\n", __func__);

	cancel_delayed_work_sync(&open_lcm_dimming);

	LCM_LOGI("push_table\n");
	push_table(lcm_deep_sleep_mode_in_setting,
		   sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);

	ddp_dsi_power_off(32, NULL);
	MDELAY(140);

	LCM_LOGI("GPIO_LCD_RST_EN down\n");
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	lcm_set_gpio_output(GPIO_LCD_RST_EN_NEW, GPIO_OUT_ZERO);
	MDELAY(10);

	LCM_LOGI("GPIO_LCD_VSN_EN down\n");
	lcm_set_gpio_output(GPIO_LCD_VSN_EN, GPIO_OUT_ZERO);
	MDELAY(10);

	LCM_LOGI("GPIO_LCD_VSP_EN down\n");
	lcm_set_gpio_output(GPIO_LCD_VSP_EN, GPIO_OUT_ZERO);
	MDELAY(10);

	LCM_LOGI("GPIO_LCD_1V8_EN down\n");
	lcm_set_gpio_output(GPIO_LCD_1V8_EN, GPIO_OUT_ZERO);

	LCM_LOGI("%s exit\n", __func__);
}

static void lcm_resume(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0x10;
	int ret = 0;

	LCM_LOGI("%s enter\n", __func__);

	LCM_LOGI("enable vsp\n");
	lcm_set_gpio_output(GPIO_LCD_VSP_EN, GPIO_OUT_ONE);
	MDELAY(2);
	ret = tps65132_write_bytes(cmd, data);
	if (ret < 0)
		LCM_LOGI("tps65132-cmd=%0x i2c write error\n", cmd);
	else
		LCM_LOGI("tps65132-cmd=%0x i2c write success\n", cmd);
	MDELAY(12);

	LCM_LOGI("enable vsn\n");
	lcm_set_gpio_output(GPIO_LCD_VSN_EN, GPIO_OUT_ONE);
	MDELAY(2);
	cmd = 0x01;
	ret = tps65132_write_bytes(cmd, data);
	if (ret < 0)
		LCM_LOGI("tps65132-cmd=%0x i2c write error\n", cmd);
	else
		LCM_LOGI("tps65132-cmd=%0x i2c write success\n", cmd);
	MDELAY(24);

	LCM_LOGI("rst up1\n");
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	lcm_set_gpio_output(GPIO_LCD_RST_EN_NEW, GPIO_OUT_ONE);
	MDELAY(12);

	LCM_LOGI("rst down\n");
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	lcm_set_gpio_output(GPIO_LCD_RST_EN_NEW, GPIO_OUT_ZERO);
	MDELAY(12);

	LCM_LOGI("rst up2\n");
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	lcm_set_gpio_output(GPIO_LCD_RST_EN_NEW, GPIO_OUT_ONE);
	MDELAY(32);

	LCM_LOGI("push table\n");
	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

	schedule_delayed_work(&open_lcm_dimming, msecs_to_jiffies(500));

	LCM_LOGI("%s exit\n", __func__);
}

typedef void (*send_cmd)(struct LCM_setting_table *table, unsigned int count, unsigned char force_update);
extern int set_panel_disp_param(unsigned int param, send_cmd p_func);

static void lcm_set_disp_param(unsigned int param)
{
	int ret;
	LCM_LOGI("lcm_set_disp_paramt: param = %d\n", param);

	ret = set_panel_disp_param(param, push_table);
	return;
}

static void lcm_resume_power(void)
{
	LCM_LOGI("%s, begin\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_1V8_EN, GPIO_OUT_ONE);
	MDELAY(12);

	LCM_LOGI("%s, end\n", __func__);
}

LCM_DRIVER nt35523_wqxga_dsi_vdo_sharp_lcm_drv = {
	.name           = "nt35523_wqxga_dsi_vdo_sharp",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.resume_power   = lcm_resume_power,
	.set_disp_param = lcm_set_disp_param,
#if (LCM_DSI_CMD_MODE)
	/*.set_backlight	= lcm_setbacklight,*/
	/*.set_pwm		= lcm_setpwm, */
	/*.get_pwm		= lcm_getpwm, */
	/*.update		= lcm_update, */
#endif
};
