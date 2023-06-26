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

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"
#include <linux/hqsysfs.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#ifdef BUILD_LK
#define GPIO_LCM_ID0    GPIO21
#define GPIO_LCM_ID1    GPIO29
#endif

static unsigned int ENP = 494; //gpio169
static unsigned int ENN = 490; //gpio165
static unsigned int RST = 370; //gpio45
//static unsigned BKL = 485; //gpio160

#define GPIO_LCD_BIAS_ENP   ENP
#define GPIO_LCD_BIAS_ENN   ENN
#define GPIO_LCD_RST        RST
#define GPIO_BKL_EN         BKL

#define IC_id_addr			0xF1
#define IC_id				0x46

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;


#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
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
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2340)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH (69498)
#define LCM_PHYSICAL_HEIGHT (150579)
#define LCM_DENSITY (216)

#define REGFLAG_DELAY		0xFFFC
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
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting[] = {
	{0xFF, 1, {0x23} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x60} },
	{0xFF, 1, {0xF0} },
	{0xFB, 1, {0x01} },
	{0xD2, 1, {0x52} },
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	{0xB0, 1, {0x00} },
	{0xC0, 1, {0x00} },
	{0x35, 1, {0x00} },
	{0x51, 2, {0x07,0x0FF} },
	{0x53, 1, {0x2C} },
	{0x55, 1, {0x01} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} }
};
/*

	{0xFF, 1, {0x23} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x60} },
	{0xFF, 1, {0x10} },
	{0x35, 1, {0x00} },
	{0x51, 2, {0x07, 0xFF} },
	{0x53, 1, {0x2C} },
	{0x55, 1, {0x01} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} }

};
*/
static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table ata_check[] = {
	{0xFF, 1, {0x21} },
	{0xFB, 1, {0x01} }
};

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	int ret;

	ret = gpio_request(GPIO, "GPIO");
	if (ret < 0)
		pr_err("[%s]: GPIO requset fail!\n", __func__);

	if (gpio_is_valid(GPIO)) {
		ret = gpio_direction_output(GPIO, output);
			if (ret < 0)
				pr_err("[%s]: failed to set output", __func__);
	}

	gpio_free(GPIO);
}

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
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
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
		}
	}
}


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

	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	//params->density = LCM_DENSITY;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode_enable = 1;

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
	params->dsi.vertical_backporch = 15;
	params->dsi.vertical_frontporch = 18;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 8;
	params->dsi.horizontal_backporch = 60;
	params->dsi.horizontal_frontporch = 60;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

	params->dsi.PLL_CLOCK = 550;

#if 0  /*non-continuous clk*/
	params->dsi.cont_clock = 0;
	params->dsi.clk_lp_per_line_enable = 1;
#else /*continuous clk*/
	params->dsi.cont_clock = 1;
#endif

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}

static void lcm_init_power(void)
{
	LCM_LOGI("[nt36672D] %s enter\n", __func__);

	/* enable backlight*/
	//lcm_set_gpio_output(GPIO_BKL_EN, 1);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(130);

	LCM_LOGI("[nt36672D] %s exit\n", __func__);
}

extern	bool	nvt_gesture_flag;

static void lcm_suspend_power(void)
{
	LCM_LOGI("[nt36672D] %s enter\n", __func__);

	/* disable backlight*/
	//lcm_set_gpio_output(GPIO_BKL_EN, 0);
	//MDELAY(10);

	if (!nvt_gesture_flag) {

		lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 0);
		MDELAY(2);

		lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 0);
		MDELAY(5);

	} else {
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
		MDELAY(2);

		lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
		MDELAY(5);
	}

	LCM_LOGI("[nt36672D] %s exit\n", __func__);
}

static void lcm_resume_power(void)
{
	LCM_LOGI("[nt36672D] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(3);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
	MDELAY(5);

	LCM_LOGI("[nt36672D] %s exit\n", __func__);
}

static void lcm_init(void)
{
	LCM_LOGI("[nt36672D] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(15);

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI("[nt36672D] %s exit\n", __func__);
}

static void lcm_suspend(void)
{
	LCM_LOGI("[nt36672D] %s enter\n", __func__);

	push_table(NULL, lcm_suspend_setting,
	sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI("[nt36672D] %s exit\n", __func__);
}

static void lcm_resume(void)
{
	LCM_LOGI("[nt36672D] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(15);

	/* enable backlight*/
	//lcm_set_gpio_output(GPIO_BKL_EN, 1);

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI("[nt36672D] %s exit\n", __func__);
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

static unsigned int lcm_compare_id(void)
{
	LCM_LOGI(" %s enter\n", __func__);

#ifdef BUILD_LK
	unsigned int id0 = 0;
	unsigned int id1 = 0;
	unsigned char buffer[4];
	unsigned int array[16];

	unsigned int lcm_id0 = 0;
	unsigned int lcm_id1 = 0;

	mt_set_gpio_mode(GPIO_LCM_ID0, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_ID0, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_LCM_ID0, GPIO_PULL_ENABLE);

	mt_set_gpio_mode(GPIO_LCM_ID1, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_ID1, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_LCM_ID1, GPIO_PULL_ENABLE);

	/*pull down ID0 ID1 PIN*/
	mt_set_gpio_pull_select(GPIO_LCM_ID0, GPIO_PULL_DOWN);
	mt_set_gpio_pull_select(GPIO_LCM_ID1, GPIO_PULL_DOWN);

	/* get ID0 ID1 status*/
	lcm_id0 = mt_get_gpio_in(GPIO_LCM_ID0);
	lcm_id1 = mt_get_gpio_in(GPIO_LCM_ID1);
	LCM_LOGI("[LCM]%s, module lcm_id0 = %d,
	lcm_id1 = %d\n", __func__, lcm_id0, lcm_id1);

	array[0] = 0x00043700;  /* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xA1, buffer, 4);
	id0 = buffer[2];
	id1 = buffer[3];
	LCM_LOGI("[LCM]%s, ic id0 = 0x%x, id1 = 0x%x\n", __func__, id0, id1);
	if (id0 == 0x19 && id1 == 0x11) {
		LCM_LOGI("[LCM]%s, nt36672D ic id compare success\n", __func__);
		return 1;
	}
	LCM_LOGI("[LCM]%s, nt36672D ic id compare fail\n", __func__);

	if (lcm_id0 == 1 && lcm_id1 == 1) {
		LCM_LOGI("[LCM]%s,
		nt36672D moudle id compare success\n", __func__);
		return 1;
	}
	LCM_LOGI("[LCM]%s, nt36672D moudle compare fail\n", __func__);
#endif
	LCM_LOGI(" %s exit\n", __func__);
	return 0;
}

/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		LCM_LOGI("[LCM ERROR] [0x9C]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x9C]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned char ata_id = 0xFF;
	unsigned char buffer_ata[4];
	unsigned int array[16];

	push_table(NULL, ata_check,
	sizeof(ata_check) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00013700;/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(IC_id_addr, buffer_ata, 1);
	ata_id = buffer_ata[0];
	LCM_LOGI("%s, ata_check, nt36672D ata_id = 0x%x\n", __func__, ata_id);

	if (ata_id == IC_id) {
		LCM_LOGI("%s, nt36672D ata_id compare success\n", __func__);
		return 1;
	}

	return 0;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt36672D backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		   sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_set_hw_info(void)
{
	hq_regiser_hw_info(HWID_LCM, "incell,vendor:Dijing,IC:nt36672(novatek)");
}


struct LCM_DRIVER nt36672D_fhdp_dsi_vdo_dijing_j19_lcm_drv = {
	.name = "nt36672D_fhdp_dsi_vdo_dijing_j19_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.set_hw_info = lcm_set_hw_info,
};
