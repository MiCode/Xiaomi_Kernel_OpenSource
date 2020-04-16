/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#include "../panel_set_disp_param.h"
#include "ddp_hal.h"
#include "disp_recovery.h"

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_NT36672A (0xf5)

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
/*ARR*/
#define dfps_dsi_send_cmd(dfps_send_cmd_way, dfps_send_cmd_speed, \
		cmdq, cmd, count, para_list, force_update) \
		lcm_util.dsi_arr_send_cmd( \
		dfps_send_cmd_way, dfps_send_cmd_speed, \
		cmdq, cmd, count, para_list, force_update)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/double_click.h>

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
#define REGFLAG_UNVALID		0x0

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0xC3, 1, {0x00} },
	{0xFF, 1, {0x10} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 60, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x05, 1, {0x04} },
	{0x13, 1, {0x04} },
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0xC3, 1, {0x00} },
	{0xC4, 1, {0x20} },
	{0xC2, 1, {0x8E} },
	{0xFF, 1, {0x10} },
	{0x35, 1, {0x00} },
	{0x51, 2, {0xFF, 0x00} },
	{0x53, 1, {0x24} },
	{0x55, 1, {0x00} },
	{0x29, 0, {} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 70, {} },
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0xC3, 1, {0x01} },
	{0xC4, 1, {0x05} },
	{0xFF, 1, {0x10} }
};

#ifdef LCM_SET_DISPLAY_ON_DELAY
/* to reduce init time, we move 120ms delay to lcm_set_display_on() !! */
static struct LCM_setting_table set_display_on[] = {
	{0x29, 0, {} }
};
#endif

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

/***********************dfps-ARR start*****************************/
static struct dynamic_fps_info lcm_dynamic_fps_setting[] = {
	{DPFS_LEVEL0, 60, 12},
	{DFPS_LEVEL1, 40, 1115},
	{DFPS_LEVEL2, 30, 2376},
};

#if 0
/*
 * here just for use example
 * lcm driver can add prev_f_cmd, cur_f_cmd for each level pair
 */

#define DFPS_MAX_CMD_NUM 10

struct LCM_dfps_cmd_table {
	struct LCM_setting_table prev_f_cmd[DFPS_MAX_CMD_NUM];
	struct LCM_setting_table cur_f_cmd[DFPS_MAX_CMD_NUM];
};

static struct LCM_dfps_cmd_table
	dfps_cmd_table[DFPS_LEVELNUM][DFPS_LEVELNUM] = {

/**********level 0 to 0,1,2 cmd*********************/
[0][0] = {
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
},

[0][1] = {
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},

},

[0][2] = {
	/*prev_frame cmd*/
	{
	/*just use adjust backlight for test
	 * lcm driver need add cmd here
	 */
	{0x51, 2, {0x0a, 0xFa} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	/*just use adjust backlight for test
	 * lcm driver need add cmd here
	 */
	{0x51, 2, {0x0a, 0x0a} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},

},


/**********level 1 to 0,1,2 cmd*********************/
[1][0] = {
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
},

[1][1] = {
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},

},

[1][2] = {
	/*prev_frame cmd*/
	{
	{0x51, 2, {0x0c, 0xFc} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{0x51, 2, {0x0d, 0xFd} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},

},

/**********level 2 to 0,1,2 cmd*********************/
[2][0] = {
	/*prev_frame cmd*/
	{
	{0x51, 2, {0x0b, 0xFb} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{0x51, 2, {0x0b, 0xF0} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
},

[2][1] = {
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},

},

[2][2] = {
	/*prev_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
	/*cur_frame cmd*/
	{
	{REGFLAG_END_OF_TABLE, 0x00, {} },
	},
},

};

/***********************dfps-ARR  end*****************************/
#endif

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd,counter;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		if(REGFLAG_UNVALID == cmd)
		{
			counter = table[i].count;
			if(REGFLAG_UNVALID == counter)
			{
				LCM_LOGI("[%s]: cmd and count is null,please check\r\n", __func__);
				break;
			}
		}

		if(REGFLAG_END_OF_TABLE == cmd)
			break;

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
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_set_disp_param(unsigned int param)
{
	int ret;
	LCM_LOGI("lcm_set_disp_paramt: param = %d\n", param);
	ret = panel_disp_param_send_lock(PANEL_CSOT_NT36672A,param, push_table);
	return;
}

static int lcm_get_lockdowninfo_for_tp(unsigned char *plockdowninfo)
{
	struct dsi_cmd_desc read_tab;
	struct dsi_cmd_desc write_tab;

	//switch to cmd2 page 1
	write_tab.dtype = 0xFF;
	write_tab.dlen = 1;
	write_tab.payload = vmalloc(1 * sizeof(unsigned char));
	write_tab.payload[0] = 0x21;
	write_tab.vc = 0;
	write_tab.link_state = 1;

	/*read lockdown info*/
	memset(&read_tab, 0, sizeof(struct dsi_cmd_desc));
	read_tab.dtype = 0xF7;
	read_tab.payload = plockdowninfo;
	memset(read_tab.payload, 0, 8);
	read_tab.dlen = 8;

	do_lcm_vdo_lp_write(&write_tab, 1);
	do_lcm_vdo_lp_read(&read_tab, 1);

	//switch to cmd1
	write_tab.dtype = 0xFF;
	write_tab.dlen = 1;
	write_tab.payload[0] = 0x10;
	write_tab.vc = 0;
	write_tab.link_state = 1;
	do_lcm_vdo_lp_write(&write_tab, 1);

	vfree(write_tab.payload);
	return 0;
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	unsigned int i = 0;
	unsigned int dynamic_fps_levels = 0;

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
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;

	LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
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
	params->dsi.vertical_backporch = 10;
	params->dsi.vertical_frontporch = 14;
	params->dsi.vertical_frontporch_for_low_power = 620;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 16;
	params->dsi.horizontal_backporch = 56;
	params->dsi.horizontal_frontporch = 64;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 553;
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
	/* for ARR 2.0 */
//	params->max_refresh_rate = 60;
//	params->min_refresh_rate = 45;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif

	/* ARR setting
	 * dfps_need_inform_lcm:
	 * whether need send cmd before and during change VFP
	 * dfps_send_cmd_way:
	 * now only LCM_DFPS_SEND_CMD_STOP_VDO supported
	 * will stop vdo mode and send cmd between vfp and vsa of next frame
	 * dfps_send_cmd_speed: now only LCM_DFPS_SEND_CMD_LP supported
	 * will send cmd in LP mode
	 */
	params->dsi.dynamic_fps_levels = 3;
	params->max_refresh_rate = 60;
	params->min_refresh_rate = 30;
	params->dsi.dfps_need_inform_lcm[LCM_DFPS_FRAME_PREV] = 0;
	params->dsi.dfps_need_inform_lcm[LCM_DFPS_FRAME_CUR] = 0;
	params->dsi.dfps_send_cmd_way = LCM_DFPS_SEND_CMD_STOP_VDO;
	params->dsi.dfps_send_cmd_speed = LCM_DFPS_SEND_CMD_LP;

#if 0
	/*vertical_frontporch should be related to the max fps*/
	params->dsi.vertical_frontporch = 20;
	/*vertical_frontporch_for_low_power
	 *should be related to the min fps
	 */
	params->dsi.vertical_frontporch_for_low_power = 750;
#endif

	dynamic_fps_levels =
		sizeof(lcm_dynamic_fps_setting)/sizeof(struct dynamic_fps_info);

	dynamic_fps_levels =
		params->dsi.dynamic_fps_levels <
		dynamic_fps_levels
		? params->dsi.dynamic_fps_levels
		: dynamic_fps_levels;

	params->dsi.dynamic_fps_levels = dynamic_fps_levels;
	for (i = 0; i < dynamic_fps_levels; i++) {
		params->dsi.dynamic_fps_table[i].fps =
			lcm_dynamic_fps_setting[i].fps;
		params->dsi.dynamic_fps_table[i].vfp =
			lcm_dynamic_fps_setting[i].vfp;
		/* params->dsi.dynamic_fps_table[i].idle_check_interval =
		 * lcm_dynamic_fps_setting[i].idle_check_interval;
		 */
	}
}

static void lcm_init_power(void)
{
	bool double_click;

	if (lcm_util.set_gpio_lcd_enp_bias) {
		lcm_util.set_gpio_lcd_enp_bias(1);
		MDELAY(1);
	} else
		LCM_LOGI("set_gpio_lcd_enp_bias not defined...\n");

	lm36273_bl_bias_conf();

	double_click = is_tp_doubleclick_enable();
	if (!double_click)
		lm36273_bias_enable(1, 1);

	MDELAY(2);
	SET_RESET_PIN(0);
	MDELAY(5);
	SET_RESET_PIN(1);
	MDELAY(5);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(11);
}

static void lcm_suspend_power(void)
{
	if (is_tp_doubleclick_enable()) {
		LCM_LOGI("keep panel power and reset pin\n");
		return;
	}

	lm36273_bias_enable(0, 10);

	if (lcm_util.set_gpio_lcd_enp_bias)
		lcm_util.set_gpio_lcd_enp_bias(0);
	else
		LCM_LOGI("set_gpio_lcd_enp_bias not defined...\n");
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	lcm_init_power();
}

static void lcm_init(void)
{
	push_table(NULL, init_setting_vdo,
			   sizeof(init_setting_vdo) /
			   sizeof(struct LCM_setting_table), 1);
	LCM_LOGI("nt36672a---lcm mode = vdo mode :%d----\n",
			 lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) /
		   sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	/* SET_RESET_PIN(0); */
}

static void lcm_resume(void)
{
	lcm_init();
}

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

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n",
		 x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
/* read id return two byte,version and id */
	data_array[0] = 0x00043700;
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

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s, nt36672a backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		   sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
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
	unsigned int id = 0, version_id = 0;
	unsigned char buffer[2];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0];		/* we only need ID */

	read_reg_v2(0xDB, buffer, 1);
	version_id = buffer[0];

	LCM_LOGI("%s,nt36672a_id=0x%08x,version_id=0x%x\n",
		 __func__, id, version_id);

	if (id == LCM_ID_NT36672A && version_id == 0x81)
		return 1;
	else
		return 0;

}

static int lcm_led_i2c_reg_op(char *buffer, int op, int count)
{
	int i, ret = -EINVAL;
	char reg_addr = *buffer;
	char *reg_val = buffer;

	if (reg_val == NULL) {
		LCM_LOGI("%s,buffer is null\n", __func__);
		return ret;
	}

	if (op == LM36273_REG_READ) {
		for (i = 0; i < count; i++) {
			ret = lm36273_reg_read_bytes(reg_addr, reg_val);
			if (ret <= 0)
				break;

			reg_addr++;
			reg_val++;
		}
	} else if (op == LM36273_REG_WRITE) {
		ret = lm36273_reg_write_bytes(reg_addr, *(reg_val + 1));
	}

	return ret;
}
#if 0
/***********************dfps-ARR  function start*****************************/
static int lcm_get_dfps_level(unsigned int fps)
{
	unsigned int i = 0;
	int dfps_level = -1;

	for (i = 0; i < DFPS_LEVELNUM; i++) {
		if (lcm_dynamic_fps_setting[i].fps == fps)
			dfps_level = lcm_dynamic_fps_setting[i].level;
	}
	return dfps_level;
}

static void dfps_dsi_push_table(
	enum LCM_DFPS_SEND_CMD_WAY dfps_send_cmd_way,
	enum LCM_DFPS_SEND_CMD_SPEED dfps_send_cmd_speed,
	void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_END_OF_TABLE:
			return;
		default:
			dfps_dsi_send_cmd(
				dfps_send_cmd_way, dfps_send_cmd_speed,
				cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
			break;
		}
	}

}

static void lcm_dfps_inform_lcm(enum LCM_DFPS_SEND_CMD_WAY dfps_send_cmd_way,
	enum LCM_DFPS_SEND_CMD_SPEED dfps_send_cmd_speed,
	void *cmdq_handle, unsigned int from_fps, unsigned int to_fps,
	enum LCM_DFPS_FRAME_ID frame_id)
{
	int from_level =  DPFS_LEVEL0;
	int to_level = DPFS_LEVEL0;

	struct LCM_dfps_cmd_table *p_dfps_cmds = NULL;

	from_level = lcm_get_dfps_level(from_fps);
	to_level = lcm_get_dfps_level(to_fps);

	if (from_level < 0 || to_level < 0) {
		LCM_LOGI("%s,no (f:%d, t:%d)\n", __func__, from_fps, to_fps);
		goto done;
	}

	p_dfps_cmds =
		&(dfps_cmd_table[from_level][to_level]);

	switch (frame_id) {
	case LCM_DFPS_FRAME_PREV:
		dfps_dsi_push_table(dfps_send_cmd_way, dfps_send_cmd_speed,
			cmdq_handle, p_dfps_cmds->prev_f_cmd,
			ARRAY_SIZE(p_dfps_cmds->prev_f_cmd), 1);
		break;
	case LCM_DFPS_FRAME_CUR:
		dfps_dsi_push_table(dfps_send_cmd_way, dfps_send_cmd_speed,
			cmdq_handle, p_dfps_cmds->cur_f_cmd,
			ARRAY_SIZE(p_dfps_cmds->cur_f_cmd), 1);
		break;
	default:
		break;

	}
done:
	LCM_LOGI("%s,done %d->%d\n", __func__, from_fps, to_fps);

}

/***********************dfps-ARR function end*****************************/
#endif

struct LCM_DRIVER csot_fhd_nt36672a_dsi_vdo_lcm_drv = {
	.name = "csot_fhd_nt36672a_dsi_vdo_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.set_disp_param = lcm_set_disp_param,
	.get_lockdowninfo_for_tp = lcm_get_lockdowninfo_for_tp,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.compare_id = lcm_compare_id,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.led_i2c_reg_op = lcm_led_i2c_reg_op,
};
