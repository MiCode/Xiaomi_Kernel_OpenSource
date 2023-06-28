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
#  include <linux/string.h>
#  include <linux/kernel.h>
#endif
#include <linux/gpio.h>
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
#endif

#include "lcm_drv.h"
#include "mtk_boot_common.h"
#ifdef BUILD_LK
#  include <platform/upmu_common.h>
#  include <platform/mt_gpio.h>
#  include <platform/mt_i2c.h>
#  include <platform/mt_pmic.h>
#  include <string.h>
#elif defined(BUILD_UBOOT)
#  include <asm/arch/mt_gpio.h>
#endif

#ifdef mdelay
#undef mdelay
#endif

#ifdef udelay
#undef udelay
#endif

#ifdef BUILD_LK
#  define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#  define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#  define LCM_LOGI(fmt, args...)  pr_err("[KERNEL/"LOG_TAG"]"fmt, ##args)
#  define LCM_LOGD(fmt, args...)  pr_err("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_FT8720 (0x40)
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
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define dsi_set_cmdq_vmcmd(cmdq, cmd, count, para_list, force_update) \
		lcm_util.dsi_send_vmcmd(cmdq, cmd, count, para_list, force_update)

#ifndef BUILD_LK
#  include <linux/kernel.h>
#  include <linux/module.h>
#  include <linux/fs.h>
#  include <linux/slab.h>
#  include <linux/init.h>
#  include <linux/list.h>
#  include <linux/i2c.h>
#  include <linux/irq.h>
#  include <linux/uaccess.h>
#  include <linux/interrupt.h>
#  include <linux/io.h>
#  include <linux/platform_device.h>
#endif

#define FRAME_WIDTH				(1080)
#define FRAME_HEIGHT			(2460)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(69336)
#define LCM_PHYSICAL_HEIGHT		(157932)
#define LCM_DENSITY				(440)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef CONFIG_MTK_MT6382_BDG
#define DSC_ENABLE
#endif

static unsigned ENP = 235; //gpio166
static unsigned ENN = 238; //gpio169

#define GPIO_LCD_BIAS_ENP   ENP
#define GPIO_LCD_BIAS_ENN   ENN

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_dstb_setting[] = {
	{0x00, 1, {0x00}},
	{0xFF, 3, {0x87, 0x20, 0x01}},
	{0x00, 1, {0x80}},
	{0xFF, 2, {0x87, 0x20}},

	{0x28, 0, {}},
	{REGFLAG_DELAY, 35, {}},
	{0x10, 0, {}},
	{REGFLAG_DELAY, 150, {}},

	//DSTB
	{0x00, 1, {0x00}},
	{0xF7, 4, {0x5A, 0xA5, 0x95, 0x27}},

	//cmd2 disable
	{0x00, 1, {0x00}},
	{0xFF, 3, {0xFF, 0xFF, 0xFF}},
	{REGFLAG_DELAY, 10, {}}
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0x00, 1, {0x00}},
	{0xFF, 3, {0x87, 0x20, 0x01}},

	{0x00, 1, {0x80}},
	{0xFF, 2, {0x87, 0x20}},

	{0x00, 1, {0xB0}},
	{0xB4, 14, {0x00, 0x14, 0x02, 0x00, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E, 0x05, 0x16, 0x10, 0xF0}},

	{0x00, 1, {0xB0}},
	{0xCA, 6, {0x03, 0x03, 0x0B, 0x0F, 0xFF, 0x08}}, /* 0xCA set 0x51 bit && pwm frequence */

	//CABC UI mode=90%, Still MODE =70%, moving mode=40%
	{0x00, 1, {0x80}},
	{0xCA, 12, {0xE6, 0xD1, 0xC0, 0xB4, 0xAA, 0xA1, 0x9A, 0x94, 0x8F, 0x8A, 0x86, 0x82}},

	{0x00, 1, {0x90}},
	{0xCA, 9, {0xFE, 0xFF, 0x66, 0xF5, 0xFF, 0xAD, 0xFA, 0xFF, 0xD6}},

	{0x00, 1, {0xA0}},
	{0xCA, 3, {0x06, 0x06, 0x07}},

	{0x00, 1, {0x82}},
	{0xCE, 2, {0x27,0x27}},

	{0x00, 1, {0x81}},
	{0xC0, 1, {0x56}},

	{0x00, 1, {0x91}},
	{0xC0, 1, {0x56}},

	{0x00, 1, {0x61}},
	{0xC0, 1, {0x9C}},

	{0x00, 1, {0xA4}},
	{0xC1, 1, {0x70}},

	{0x00, 1, {0x85}},
	{0xCE, 1, {0xD8}},

	{0x00, 1, {0x9A}},
	{0xCE, 4, {0x0E,0x10,0x03,0x0D}},

	{0x00, 1, {0xE1}},
	{0xCE, 1, {0x0A}},

	{0x00, 1, {0xB2}},
	{0xCF, 2, {0xB8,0xBC}},

	{0x00, 1, {0xB7}},
	{0xCF, 2, {0x28,0x2C}},

	//cmd2 disable
	{0x00, 1, {0x00}},
	{0xFF, 3, {0xFF, 0xFF, 0xFF}},

	{0X51, 2, {0XFF, 0x0E}},
	{0X53, 1, {0X2C}},
	{0X55, 1, {0X00}},

	{0x35, 1, {0x00}},
	{0x11, 1, {0x00}},
	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 95, {}},
	{0x29, 1, {0x00}},
	//{REGFLAG_DELAY, 20, {}}
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0xFF, 0x07} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table cabc_level[] = {
	{0x55, 1, {0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

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
			if (table[i].count > 1)
				MDELAY(1);
			break;
		}
	}
}

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

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;

	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 9000;/*real vact timing fps * 100*/

	/* traversing array must less than DFPS_LEVELS */
	/* DPFS_LEVEL0 */
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	dfps_params[0].vertical_frontporch = 1248;
	dfps_params[0].vertical_frontporch_for_low_power = 2200;

	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	dfps_params[1].vertical_frontporch = 10;
	dfps_params[1].vertical_frontporch_for_low_power = 2200;

	/* DPFS_LEVEL2 */
	dfps_params[2].level = DFPS_LEVEL2;
	dfps_params[2].fps = 4800;/*real fps * 100, to support float*/
	dfps_params[2].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	dfps_params[2].vertical_frontporch = 2200;

	/* DPFS_LEVEL3 */
	dfps_params[3].level = DFPS_LEVEL3;
	dfps_params[3].fps = 3600;/*real fps * 100, to support float*/
	dfps_params[3].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	dfps_params[3].vertical_frontporch = 3750;

	dsi->dfps_num = 4;
}
#endif

static void lcm_get_params(struct LCM_PARAMS *params)
{
	// unsigned int i = 0;
	// unsigned int dynamic_fps_levels = 0;

	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	//lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	LCM_LOGI("%s: lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
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

	params->dsi.vertical_sync_active = 10;
	params->dsi.vertical_backporch = 20;
	params->dsi.vertical_frontporch = 10;	//1248
	/*params->dsi.vertical_frontporch_for_low_power = 750;*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 16;	//16
	params->dsi.horizontal_backporch = 22;		//22
	params->dsi.horizontal_frontporch = 172;	//164
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.dsc_enable = 0;
#ifdef DSC_ENABLE	//CONFIG_MTK_MT6382_BDG
	params->dsi.bdg_ssc_disable = 1;
	params->dsi.bdg_dsc_enable = 1;
	params->dsi.dsc_params.ver = 17;
	params->dsi.dsc_params.slice_mode = 1;
	params->dsi.dsc_params.rgb_swap = 0;
	params->dsi.dsc_params.dsc_cfg = 34;
	params->dsi.dsc_params.rct_on = 1;
	params->dsi.dsc_params.bit_per_channel = 8;
	params->dsi.dsc_params.dsc_line_buf_depth = 9;
	params->dsi.dsc_params.bp_enable = 1;
	params->dsi.dsc_params.bit_per_pixel = 128;
	params->dsi.dsc_params.pic_height = 2460;
	params->dsi.dsc_params.pic_width = 1080;
	params->dsi.dsc_params.slice_height = 20;
	params->dsi.dsc_params.slice_width = 540;
	params->dsi.dsc_params.chunk_size = 540;
	params->dsi.dsc_params.xmit_delay = 512;
	params->dsi.dsc_params.dec_delay = 526;
	params->dsi.dsc_params.scale_value = 32;
	params->dsi.dsc_params.increment_interval = 488;
	params->dsi.dsc_params.decrement_interval = 7;
	params->dsi.dsc_params.line_bpg_offset = 12;
	params->dsi.dsc_params.nfl_bpg_offset = 1294;
	params->dsi.dsc_params.slice_bpg_offset = 1302;
	params->dsi.dsc_params.initial_offset = 6144;
	params->dsi.dsc_params.final_offset = 4336;
	params->dsi.dsc_params.flatness_minqp = 3;
	params->dsi.dsc_params.flatness_maxqp = 12;
	params->dsi.dsc_params.rc_model_size = 8192;
	params->dsi.dsc_params.rc_edge_factor = 6;
	params->dsi.dsc_params.rc_quant_incr_limit0 = 11;
	params->dsi.dsc_params.rc_quant_incr_limit1 = 11;
	params->dsi.dsc_params.rc_tgt_offset_hi = 3;
	params->dsi.dsc_params.rc_tgt_offset_lo = 3;

/*with dsc*/
	params->dsi.PLL_CLOCK = 385;
	params->dsi.data_rate = 770;
#else
	params->dsi.bdg_dsc_enable = 0;
/*without dsc*/
	params->dsi.PLL_CLOCK = 500;
	params->dsi.data_rate = 1000;
#endif

	//params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;

	/* for ARR 2.0 */
	// params->max_refresh_rate = 60;
	// params->min_refresh_rate = 45;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif

	#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
	#endif
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	LCM_LOGI("[FT8720] %s enter\n", __func__);

	SET_RESET_PIN(0);
	MDELAY(3);
	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(2);
	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
	MDELAY(2);

	LCM_LOGI("[FT8720] %s exit\n", __func__);
}

extern int tp_sensor_flag;
extern bool gesture_support;
static void lcm_suspend_power(void)
{
	if(tp_sensor_flag == 1 || gesture_support == 1) {
		LCM_LOGI("[FT8720] %s exit tp_sensor_flag = %d\n", __func__, tp_sensor_flag);
	}else{
		LCM_LOGI("[FT8720] %s exit tp_sensor_flag = %d\n", __func__, tp_sensor_flag);
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 0);
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 0);
		MDELAY(5);
	}
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	LCM_LOGI("[FT8720][%s][%d]\n", __func__, __LINE__);
	lcm_init_power();
}

extern int fts_resume(void);
extern fts_fwresume_work(void);
static void lcm_init(void)
{
	MDELAY(2);
	SET_RESET_PIN(1);
	MDELAY(3);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(12);
	if(tp_sensor_flag == 0){
		LCM_LOGI("[FT8720][%s]tp_sensor_flag = 0[%d]\n", __func__, __LINE__);
		fts_fwresume_work();
	}
	fts_resume();
	LCM_LOGI("[FT8720][%s][%d]\n", __func__, __LINE__);
	push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
}
extern int fts_suspend(void);
static void lcm_suspend(void)
{
	LCM_LOGI("[FT8720][%s][%d]\n", __func__, __LINE__);
	if(tp_sensor_flag == 0) {
		push_table(NULL, lcm_suspend_dstb_setting,
			ARRAY_SIZE(lcm_suspend_dstb_setting), 1);
	}
	fts_suspend();
}

static void lcm_resume(void)
{
	LCM_LOGI("[FT8720][%s][%d]\n", __func__, __LINE__);
	lcm_init();
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int id[3] = {0x83, 0x11, 0x2B};
	unsigned int data_array[3];
	unsigned char read_buf[3];

	data_array[0] = 0x00033700; /* set max return size = 3 */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, read_buf, 3); /* read lcm id */

	LCM_LOGI("ATA read = 0x%x, 0x%x, 0x%x\n",
		 read_buf[0], read_buf[1], read_buf[2]);

	if ((read_buf[0] == id[0]) &&
	    (read_buf[1] == id[1]) &&
	    (read_buf[2] == id[2]))
		ret = 1;
	else
		ret = 0;

	return ret;
#else
	return 0;
#endif
}

static void push_table_vmcmd(void *cmdq, struct LCM_setting_table *table,
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
			dsi_set_cmdq_vmcmd(cmdq, cmd, table[i].count,
					table[i].para_list, force_update);
			if (table[i].count > 1)
				MDELAY(1);
			break;
		}
	}
}

static void lcm_setbacklight_vmcmd_cmdq(void *handle, unsigned int *lcm_cmd, unsigned int *lcm_count, unsigned int *level)
{
	int bl_lvl;
	bl_lvl = (*level);
	if (bl_lvl > 2047) {
		bl_lvl = 2047;
	}

	LCM_LOGD("%s, backlight: level = %d\n", __func__, bl_lvl);
	bl_level[0].para_list[0] = ((bl_lvl >> 3) & 0xFF);
	bl_level[0].para_list[1] = ((bl_lvl << 1) & 0x0E);
	push_table_vmcmd(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static int cabc_status = 0;
static void lcm_set_cabc_vmcmd_cmdq(void *handle, unsigned int *lcm_cmd, unsigned int *lcm_count, unsigned int level)
{
	LCM_LOGD("%s,ft8720 set_cabc level = %d\n", __func__, level);

	cabc_level[0].para_list[0] = (int)(level);
	push_table_vmcmd(handle, cabc_level, ARRAY_SIZE(cabc_level), 1);
	cabc_status = level;
}

static void lcm_get_cabc_status(int *status)
{
	*status = cabc_status;
	LCM_LOGD("%s, ft8720 get_cabc level = %d\n", *status);
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

#ifdef LCM_SET_DISPLAY_ON_DELAY
	lcm_set_display_on();
#endif

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
	unsigned int id = 0;
	unsigned char buffer[1];
	unsigned int array[16];

//	SET_RESET_PIN(1);
//	SET_RESET_PIN(0);
	MDELAY(1);

//	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00013700;  /* read id return 1byte */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xDA, buffer, 1);
	id = buffer[0];     /* we only need ID */

	LCM_LOGI("%s,ft8720 id = 0x%08x\n", __func__, id);

	if (id == LCM_ID_FT8720)
		return 1;
	else
		return 0;
}

static unsigned int lcd_esd_recover(void)
{
	LCM_LOGI("%s, do lcd esd recovery...\n", __func__);
	lcm_init_power();
	lcm_init();

	return 0;
}

struct LCM_DRIVER dsi_panel_m19a_42_03_0b_dsc_vdo_lcm_drv = {
	.name = "dsi_panel_m19a_42_03_0b_dsc_vdo_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_lcm_cmd = lcm_setbacklight_vmcmd_cmdq,
	.set_lcm_cabc_cmd = lcm_set_cabc_vmcmd_cmdq,
	.get_cabc_status = lcm_get_cabc_status,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.esd_recover = lcd_esd_recover,

};

