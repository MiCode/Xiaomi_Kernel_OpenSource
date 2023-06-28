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
#include <linux/gpio.h>
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
#endif

#include "lcm_drv.h"
#include "mtk_boot_common.h"
#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#ifdef mdelay
#undef mdelay
#endif

#ifdef udelay
#undef udelay
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_err("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_err("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_nt36672c 0x83
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
#include "lcm_i2c.h"

#define FRAME_WIDTH			(1080)
#define FRAME_HEIGHT			(2460)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH         (69336)
#define LCM_PHYSICAL_HEIGHT        (157932)
#define LCM_DENSITY			(440)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
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

extern bool nvt_gesture_flag;

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

static struct LCM_setting_table lcm_suspend_proximity_setting[] = {
	{0x28, 0, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0XFF, 1, {0X20}},
	{0XFB, 1, {0X01}},
	{0X69, 1, {0X90}},

	{0XFF, 1, {0X23}},
	{0XFB, 1, {0X01}},
	{0X00, 1, {0X60}}, /*set 0x51 bit: 0x00: 8bit; 0x60: 11bit; 0x80: 12bit */
	{0X01, 1, {0X84}}, //diming enable
	{0X05, 1, {0X2D}},
	{0X06, 1, {0X00}},
	{0X07, 1, {0X00}}, /* 07 08 09 set pwm frequence */
	{0X08, 1, {0X01}},
	{0X09, 1, {0XF8}},
	{0X11, 1, {0X01}}, //resolution 1080*2460
	{0X12, 1, {0X9F}},
	{0X15, 1, {0X65}},
	{0X16, 1, {0X0B}},
	{0X0A, 1, {0XFF}},
	{0X0B, 1, {0XFF}},
	{0X0C, 1, {0XFF}},
	{0X0D, 1, {0X3F}},
	{0X19, 1, {0x13}},
	{0X1A, 1, {0x13}},
	{0X1B, 1, {0x13}},
	{0X1C, 1, {0x13}},
	{0X1D, 1, {0x13}},
	{0X1E, 1, {0x15}},
	{0X1F, 1, {0x15}},
	{0X20, 1, {0x2A}},
	{0X21, 1, {0x2A}},
	{0X22, 1, {0x2A}},
	{0X23, 1, {0x2A}},
	{0X24, 1, {0x3F}},
	{0X25, 1, {0x3F}},
	{0X26, 1, {0x3F}},
	{0X27, 1, {0x3F}},
	{0X28, 1, {0x3F}},
	{0X29, 1, {0X04}}, //UI mode
	{0X2A, 1, {0X0F}}, //STILL mode
	{0X2B, 1, {0X0D}}, //MOVING mode
	{0X30, 1, {0XFF}},
	{0X31, 1, {0xFF}},
	{0X32, 1, {0XFD}},
	{0X33, 1, {0XFB}},
	{0X34, 1, {0XF8}},
	{0X35, 1, {0XF5}},
	{0X36, 1, {0XF4}},
	{0X36, 1, {0XF4}},
	{0X37, 1, {0XF3}},
	{0X38, 1, {0XF2}},
	{0X39, 1, {0XF1}},
	{0X3A, 1, {0XEF}},
	{0X3B, 1, {0XEC}},
	{0X3D, 1, {0XE9}},
	{0X3F, 1, {0XE5}},
	{0X40, 1, {0XE5}},
	{0X41, 1, {0XE5}},
	{0X45, 1, {0XFF}},
	{0X46, 1, {0XFE}},
	{0X47, 1, {0XE6}},
	{0X48, 1, {0XCE}},
	{0X49, 1, {0XB6}},
	{0X4A, 1, {0XB2}},
	{0X4B, 1, {0XA7}},
	{0X4C, 1, {0X9D}},
	{0X4D, 1, {0X92}},
	{0X4E, 1, {0X88}},
	{0X4F, 1, {0X7D}},
	{0X50, 1, {0X73}},
	{0X51, 1, {0X68}},
	{0X52, 1, {0X66}},
	{0X53, 1, {0X66}},
	{0X54, 1, {0X66}},
	{0X58, 1, {0XFF}},
	{0X59, 1, {0XFE}},
	{0X5A, 1, {0XF3}},
	{0X5B, 1, {0XE7}},
	{0X5C, 1, {0XDC}},
	{0X5D, 1, {0XD8}},
	{0X5E, 1, {0XD3}},
	{0X5F, 1, {0XCD}},
	{0X60, 1, {0XC8}},
	{0X61, 1, {0XC3}},
	{0X62, 1, {0XBE}},
	{0X63, 1, {0XB8}},
	{0X64, 1, {0XB3}},
	{0X65, 1, {0XB2}},
	{0X66, 1, {0XB2}},
	{0X67, 1, {0XB2}},

	{0XFF, 1, {0X24}},
	{0XFB, 1, {0X01}},
	{0X4D, 1, {0X03}},
	{0X4E, 1, {0X39}},
	{0X4F, 1, {0X3C}},
	{0X53, 1, {0X39}},
	{0X7B, 1, {0X8F}},
	{0X7D, 1, {0X04}},
	{0X80, 1, {0X04}},
	{0X81, 1, {0X04}},
	{0XA0, 1, {0X0F}},
	{0XA2, 1, {0X10}},
	{0XA4, 1, {0X04}},
	{0XA5, 1, {0X04}},
	{0XC6, 1, {0XC0}},

	{0XFF, 1, {0X25}},
	{0XFB, 1, {0X01}},
	{0X18, 1, {0X20}},
	{0XD7, 1, {0X82}},
	{0XDA, 1, {0X02}},
	{0XDD, 1, {0X02}},
	{0XE0, 1, {0X02}},

	{0XFF, 1, {0X26}},
	{0XFB, 1, {0X01}},
	{0X80, 1, {0X09}},

	{0XFF, 1, {0x2A}},    // extend VFP to 30Hz
	{0XFB, 1, {0X01}},
	{0X20, 1, {0X49}},
	{0X28, 1, {0xAE}},
	{0X29, 1, {0x1A}},
	{0X2A, 1, {0x6D}},
	{0X2F, 1, {0x06}},
	{0X30, 1, {0x1C}},
	{0X31, 1, {0x60}},
	{0X33, 1, {0x88}},
	{0X34, 1, {0xB0}},
	{0X36, 1, {0xBC}},
	{0X37, 1, {0xAB}},
	{0X39, 1, {0xB8}},
	{0X3A, 1, {0x1C}},

	{0XFF, 1, {0x2B}},    // version code
	{0XFB, 1, {0x01}},
	{0XB7, 1, {0x33}},
	{0XB8, 1, {0x0E}},
	{0XC0, 1, {0x02}},
	{0XFF, 1, {0XC0}},
	{0XFB, 1, {0X01}},
	{0X9C, 1, {0X11}},
	{0X9D, 1, {0X11}},

	{0XFF, 1, {0XE0}},
	{0XFB, 1, {0X01}},
	{0X35, 1, {0X82}},

	{0XFF, 1, {0XF0}},
	{0XFF, 1, {0XF0}},
	{0XFB, 1, {0X01}},
	{0X1C, 1, {0X01}},
	{0X33, 1, {0X01}},
	{0X5A, 1, {0X00}},
	{0X9C, 1, {0X17}},

	{0XFF, 1, {0X27}},
	{0XFB, 1, {0X01}},
	{0X44, 1, {0X00}},
	{0X40, 1, {0x20}},
	{0X40, 1, {0X20}},
	{0X41, 1, {0X30}},

	{0XFF, 1, {0X10}},
	{0XFB, 1, {0X01}},
	{0X3B, 5, {0X03, 0X14, 0X36, 0X04, 0X04}},
	{0X36, 1, {0X00}},
	{0XB0, 1, {0X00}},
	{0XC0, 1, {0X03}},
	{0XC1, 16, {0X89, 0X28, 0X00, 0X14, 0X00, 0XAA, 0X02, 0X0E, 0X00, 0X71, 0X00, 0X07, 0X05, 0X0E, 0X05, 0X16}},
	{0XC2, 2, {0X1B, 0XA0}},
	{0X51, 2, {0X07, 0XFF}},
	{0X53, 1, {0X2C}},
	{0X55, 1, {0X00}},

	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 100, {}},
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {}}
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0x07, 0xFF} },
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
	dfps_params[0].vertical_frontporch = 1290;
	dfps_params[0].vertical_frontporch_for_low_power = 2260;

	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	dfps_params[1].vertical_frontporch = 54;
	dfps_params[1].vertical_frontporch_for_low_power = 2260;

	/* DPFS_LEVEL2 */
	dfps_params[2].level = DFPS_LEVEL2;
	dfps_params[2].fps = 4800;/*real fps * 100, to support float*/
	dfps_params[2].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	dfps_params[2].vertical_frontporch = 2260;

	/* DPFS_LEVEL3 */
	dfps_params[3].level = DFPS_LEVEL3;
	dfps_params[3].fps = 3600;/*real fps * 100, to support float*/
	dfps_params[3].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	dfps_params[3].vertical_frontporch = 3830;

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
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
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
	params->dsi.vertical_backporch = 10;
	params->dsi.vertical_frontporch = 54;
	//params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 16;	//22
	params->dsi.horizontal_backporch = 16;		//22
	params->dsi.horizontal_frontporch = 170;	//164
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
#ifdef CONFIG_MTK_MT6382_BDG
	params->dsi.bdg_ssc_disable = 1;
	params->dsi.dsc_params.ver = 17;
	params->dsi.dsc_params.slice_mode = 1;
	params->dsi.dsc_params.rgb_swap = 0;
	params->dsi.dsc_params.dsc_cfg = 34;
	params->dsi.dsc_params.rct_on = 1;
	params->dsi.dsc_params.bit_per_channel = 8;
	params->dsi.dsc_params.dsc_line_buf_depth = 9;
	params->dsi.dsc_params.bp_enable = 1;
	params->dsi.dsc_params.bit_per_pixel = 128;//128;
	params->dsi.dsc_params.pic_height = 2460;
	params->dsi.dsc_params.pic_width = 1080;
	params->dsi.dsc_params.slice_height = 20;//8;
	params->dsi.dsc_params.slice_width = 540;
	params->dsi.dsc_params.chunk_size = 540;
	params->dsi.dsc_params.xmit_delay = 170;
	params->dsi.dsc_params.dec_delay = 526;
	params->dsi.dsc_params.scale_value = 32;
	params->dsi.dsc_params.increment_interval = 113;//43;
	params->dsi.dsc_params.decrement_interval = 7;
	params->dsi.dsc_params.line_bpg_offset = 12;
	params->dsi.dsc_params.nfl_bpg_offset = 1294;//3511;
	params->dsi.dsc_params.slice_bpg_offset = 1302;//3255;
	params->dsi.dsc_params.initial_offset = 6144;
	params->dsi.dsc_params.final_offset = 7072;
	params->dsi.dsc_params.flatness_minqp = 3;
	params->dsi.dsc_params.flatness_maxqp = 12;
	params->dsi.dsc_params.rc_model_size = 8192;
	params->dsi.dsc_params.rc_edge_factor = 6;
	params->dsi.dsc_params.rc_quant_incr_limit0 = 11;
	params->dsi.dsc_params.rc_quant_incr_limit1 = 11;
	params->dsi.dsc_params.rc_tgt_offset_hi = 3;
	params->dsi.dsc_params.rc_tgt_offset_lo = 3;
#endif
	params->dsi.dsc_enable = 0;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
#ifdef DSC_ENABLE
	params->dsi.bdg_dsc_enable = 1;
	params->dsi.PLL_CLOCK = 385;	//with dsc
	params->dsi.data_rate = 770;
#else
	params->dsi.bdg_dsc_enable = 0;
	params->dsi.PLL_CLOCK = 550; //without dsc
#endif
	params->dsi.PLL_CK_CMD = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
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
	LCM_LOGI("[nt36672c] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(1);
	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
	MDELAY(10);

	LCM_LOGI("[nt36672c] %s exit\n", __func__);
}

extern int nvt_tp_sensor_flag;
static void lcm_suspend_power(void)
{
	if(nvt_tp_sensor_flag == 1 || nvt_gesture_flag == 1){
		LCM_LOGI("[nt36672c] %s nvt_tp_sensor_flag = %d\n", __func__, nvt_tp_sensor_flag);
	}else{
		LCM_LOGI("[nt36672c] %s nvt_tp_sensor_flag = %d\n", __func__, nvt_tp_sensor_flag);
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 0);
		MDELAY(1);
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 0);
		MDELAY(5);
	}
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	LCM_LOGI("[nt36672c][%s][%d]\n", __func__, __LINE__);
	lcm_init_power();
}

extern int nvt_ts_resume_func(void);
static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(10);
    nvt_ts_resume_func();
    LCM_LOGI("[nt36672c][%s][%d]\n", __func__, __LINE__);
    push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
}

static void lcm_suspend(void)
{
	if(nvt_tp_sensor_flag == 0) {
		LCM_LOGI("[nt36672c] lcm_suspend_setting [%s][%d]\n", __func__, __LINE__);
		push_table(NULL, lcm_suspend_setting,
			ARRAY_SIZE(lcm_suspend_setting), 1);
		/*SET_RESET_PIN(0);
		MDELAY(1);*/
	} else if(nvt_tp_sensor_flag == 1) {
		LCM_LOGI("[nt36672c] lcm_suspend_proximity_setting [%s][%d]\n", __func__, __LINE__);
		push_table(NULL, lcm_suspend_proximity_setting,
			ARRAY_SIZE(lcm_suspend_proximity_setting), 1);


	}
}

static void lcm_resume(void)
{
	LCM_LOGI("[nt36672c][%s][%d]\n", __func__, __LINE__);
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

static void lcm_setbacklight_vmcmd_cmdq(void *handle, unsigned int *lcm_cmd, unsigned int *lcm_count, unsigned int *level)
{
	unsigned int bl_lvl = 0;
	bl_lvl = (*level);
	if (bl_lvl > 2047) {
		bl_lvl = 2047;
	}

	LCM_LOGD("%s: backlight: bl_lvl = %d\n", __func__, bl_lvl);
	bl_level[0].para_list[0] = ((bl_lvl >> 8) & 0x07);
	bl_level[0].para_list[1] = (bl_lvl & 0xff);

	push_table_vmcmd(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static int cabc_status = 0;
static void lcm_set_cabc_vmcmd_cmdq(void *handle, unsigned int *lcm_cmd, unsigned int *lcm_count, unsigned int level)
{
	LCM_LOGI("%s,nt36672c set_cabc level = %d\n", __func__, level);

	cabc_level[0].para_list[0] = (int)(level);
	push_table_vmcmd(handle, cabc_level, ARRAY_SIZE(cabc_level), 1);
	cabc_status = level;
}

static void lcm_get_cabc_status(int *status)
{
	*status = cabc_status;
	LCM_LOGD("%s,get nt3667c status = %d\n", *status);
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

	LCM_LOGI("%s,nt36672c id = 0x%08x\n", __func__, id);

	if (id == LCM_ID_nt36672c)
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

struct LCM_DRIVER dsi_panel_m19a_36_02_0a_dsc_vdo_lcm_drv = {
	.name = "dsi_panel_m19a_36_02_0a_dsc_vdo_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.compare_id = lcm_compare_id,
	.set_lcm_cmd = lcm_setbacklight_vmcmd_cmdq,
	.set_lcm_cabc_cmd = lcm_set_cabc_vmcmd_cmdq,
	.get_cabc_status = lcm_get_cabc_status,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.esd_recover = lcd_esd_recover,
};

