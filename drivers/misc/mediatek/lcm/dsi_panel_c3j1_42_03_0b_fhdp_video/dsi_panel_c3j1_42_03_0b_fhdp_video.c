/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
//#include <linux/hqsysfs.h>
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
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#ifdef BUILD_LK
#define GPIO_LCM_ID0	GPIO171
#define GPIO_LCM_ID1	GPIO172
#endif

static unsigned ENP = 494; //gpio169
static unsigned ENN = 490; //gpio165
static unsigned RST = 370; //gpio45
//static unsigned BKL = 485; //gpio160

#define GPIO_LCD_BIAS_ENP   ENP
#define GPIO_LCD_BIAS_ENN   ENN
#define GPIO_LCD_RST		RST
#define GPIO_BKL_EN		 BKL

#define IC_id_addr			0xDA
#define IC_id				0x40

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
extern int tps65132_write_bytes(unsigned char addr, unsigned char value);

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
#define LCM_DENSITY (480)

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
	{REGFLAG_DELAY, 150, {} },	//120
	{0x00, 1, {0x00} },
	{0xF7, 4, {0x5A, 0xA5, 0x95, 0x27} }
};

static struct LCM_setting_table init_setting[] = {
#ifdef CONFIG_BACKLIGHT_SUPPORT_2047_FEATURE
	{0x00, 1, {0x00} },
	{0xFF, 3, {0x87, 0x19, 0x01} },
	{0x00, 1, {0x80} },
	{0xFF, 2, {0x87,0x19} },
	{0x00, 1, {0xB0} },
	{0xCA, 3, {0x01, 0x01, 0x0B} },//11 bit PWM 31Khz
#else
	{0x00, 1, {0x00} },
	{0xFF, 3, {0x87, 0x19, 0x01} },
	{0x00, 1, {0x80} },
	{0xFF, 2, {0x87,0x19} },	// CMD2 enable
	{0x00, 1, {0xB2} },
	{0xCA, 1, {0x09} },//8 bit PWM 29Khz
#endif
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	//{0xFF, 1, {0x10} },
	{0x35, 1, {0x00} },
#ifdef CONFIG_BACKLIGHT_SUPPORT_2047_FEATURE
	{0x51,0x02,{0x00,0x00}},
#else
	{0x51,0x01,{0x00}},
#endif	
//	{0x51, 2, {0xCC, 0x00} },
	{0x53, 1, {0x2C} },
	{0x55, 1, {0x00} },
	//{0x11, 0, {} },
	//{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} }
};

static struct LCM_setting_table bl_level[] = {
#ifdef CONFIG_BACKLIGHT_SUPPORT_2047_FEATURE
	{0x51,2,{0xFF,0x7F}},
#else
	{0x51,1,{0xFF}},
#endif
//	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table ata_check[] = {
	{0x00, 1, {0x00} },
	{0xFF, 3, {0x87, 0x19, 0x01} },
	{0x00, 1, {0x80} },
	{0xFF, 2, {0x87, 0x19} }
};

static void lcm_set_gpio_output(unsigned GPIO, unsigned int output)
{
	int ret;

	ret = gpio_request(GPIO, "GPIO");
	if (ret < 0) {
		pr_err("[%s]: GPIO requset fail!\n", __func__);
	}

	if (gpio_is_valid(GPIO)) {
		ret = gpio_direction_output(GPIO, output);
			if (ret < 0) {
				pr_err("[%s]: failed to set output", __func__);
			}
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

	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 12;
	params->dsi.vertical_frontporch = 112;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 16;
	params->dsi.horizontal_frontporch = 16;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

	params->dsi.PLL_CLOCK = 550;

	printk("[%s]: --lyd_mipi, PLL_CLOCK = %d\n", __func__, params->dsi.PLL_CLOCK);
	printk("[%s]: --lyd_mipi, ssc_disable = %d\n", __func__, params->dsi.ssc_disable);

#if 0  /*non-continous clk*/
	params->dsi.cont_clock = 0;
	params->dsi.clk_lp_per_line_enable = 1;
#else /*continuous clk*/
	params->dsi.cont_clock = 1;
#endif

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}

static void lcm_init_power(void)
{
	LCM_LOGI("[ft8719] %s enter\n", __func__);

	/* enable backlight*/
//	lcm_set_gpio_output(GPIO_BKL_EN, 1);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
	MDELAY(10);

	LCM_LOGI("[ft8719] %s exit\n", __func__);
}

//extern bool fts_gesture_flag;

static void lcm_suspend_power(void)
{
	LCM_LOGI("[ft8719] %s enter\n", __func__);

	/* disable backlight*/
//	lcm_set_gpio_output(GPIO_BKL_EN, 0);
	//if (!fts_gesture_flag) {
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 0);
		MDELAY(2);

		lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 0);
		MDELAY(1);
	//}

	LCM_LOGI("[ft8719] %s exit\n", __func__);
}

static void lcm_resume_power(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret = 0;
	LCM_LOGI("[ft8719] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
	MDELAY(5);
	cmd = 0x00;
	data = 0x0f;

	ret = tps65132_write_bytes(cmd, data);
	if (ret < 0)
		LCM_LOGI("[ft8719] nt36525b--tps6132--cmd=%0x--i2c write error--\n", __func__);
	else
		LCM_LOGI("[ft8719] nt36525b--tps6132--cmd=%0x--i2c write success--\n", __func__);

	cmd = 0x01;
	data = 0x0f;

	ret = tps65132_write_bytes(cmd, data);
	if (ret < 0)
		LCM_LOGI("[ft8719] nt36525b--tps6132--cmd=%0x--i2c write error--\n", __func__);
	else
		LCM_LOGI("[ft8719] nt36525b--tps6132--cmd=%0x--i2c write success--\n", __func__);

	LCM_LOGI("[ft8719] %s exit\n", __func__);
}

static void lcm_init(void)
{
	LCM_LOGI("[ft8719] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(1);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(15);

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI("[ft8719] %s exit\n", __func__);
}

static void lcm_suspend(void)
{
	LCM_LOGI("[ft8719] %s enter\n", __func__);

	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI("[ft8719] %s exit\n", __func__);
}

static void lcm_resume(void)
{
	LCM_LOGI("[ft8719] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(1);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(15);

	/* enable backlight*/
	//lcm_set_gpio_output(GPIO_BKL_EN, 1);

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);

	LCM_LOGI("[nt36672A] %s exit\n", __func__);
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
#if 0
	unsigned int id = 0;
	unsigned int version_id;
	unsigned char buffer[8];
	unsigned int array[16];

	LCM_LOGI(" %s --lyd,enter\n", __func__);
	array[0] = 0x50001500;  /* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xF4, buffer, 8);
	id = buffer[0];	   /* we only need ID */

	//read_reg_v2(0xF4, buffer, 2);
	version_id = buffer[1];

	LCM_LOGI("%s,ft8719_fhdp_dsi_vdo_huaxing_lcm, id=0x%x,version_id=0x%x\n", __func__, id, version_id);

	if (id == 0x53 && version_id == 0x42)
	   return 1;
	else
	   return 0;
#endif
	return 1;
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

	push_table(NULL, ata_check, sizeof(ata_check) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00013700; /*read id return two byte,version and id*/
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(IC_id_addr, buffer_ata, 1);
	ata_id = buffer_ata[0];
	printk("%s, ata_check, ft8719 ata_id = 0x%x\n", __func__, ata_id);

	if ((ata_id == IC_id) || (ata_id == 0x9C)) {
		printk("%s, ata_check,ft8719 ata_id compare success\n", __func__);
		return 1;
	}
	return 0;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,ft8719 backlight: level = %d\n", __func__, level);

		if((0 != level) && (level <= 14))
			level = 14;
//		level = level*72/100;
#ifdef CONFIG_BACKLIGHT_SUPPORT_2047_FEATURE
		bl_level[0].para_list[0] = level >> 8;
		bl_level[0].para_list[1] = level & 0xFF;
#else
		bl_level[0].para_list[0] = level;
#endif

	//bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		   sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

/*static void lcm_set_hw_info(void)
{
	hq_regiser_hw_info(HWID_LCM, "incell,vendor:xinli,IC:ft8719(focaltech)");
}*/

struct LCM_DRIVER dsi_panel_c3j1_42_03_0b_fhdp_video = {
	.name = "dsi_panel_c3j1_42_03_0b_fhdp_video",
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
	//.set_hw_info = lcm_set_hw_info,
};
