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
#ifndef BUILD_LK
#include <linux/string.h>
#endif

#include "lcm_drv.h"
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define FRAME_WIDTH             (540)
#define FRAME_HEIGHT            (960)

#define REGFLAG_DELAY           0xFE
#define REGFLAG_END_OF_TABLE    0xFF    /* END OF REGISTERS MARKER */

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)       lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                                      lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)                  lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg                                            lcm_util.dsi_read_reg()


struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};


static struct LCM_setting_table lcm_initialization_setting[] = {

	/*
	   Note :

	   Data ID will depends on the following rule.

	   count of parameters > 1      => Data ID = 0x39
	   count of parameters = 1      => Data ID = 0x15
	   count of parameters = 0      => Data ID = 0x05

	   Structure Format :

	   {DCS command, count of parameters, {parameter list}}
	   {REGFLAG_DELAY, milliseconds of time, {}},

	   ...

	   Setting ending by predefined flag

	   {REGFLAG_END_OF_TABLE, 0x00, {}}
	 */

	/* SET PASSWORD//10//F2//58 */
	{ 0xB9, 03, {0xFF, 0x83, 0x89} },

	/* SET POWER HI */
	{ 0xB1, 20, {0x7F, 0x10, 0x10, 0x32, 0x32, 0x50, 0x10, 0xF2, 0x58, 0x80,
		     0x20, 0x20, 0xF8, 0xAA, 0xAA, 0xA0, 0x00, 0x80, 0x30, 0x00} },

	{ 0xB2, 10, {0x80, 0x50, 0x05, 0x07, 0x40, 0x38, 0x11, 0x64, 0x5D, 0x09} },

	/* SET CYC HI */
	{ 0xB4, 11, {0x70, 0x70, 0x70, 0x70, 0x00, 0x00, 0x10, 0x76, 0x10, 0x76,
		     0xB0} },

	/* SET GAMMA */
	{ 0xD3, 35, {0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x32, 0x10, 0x00,
		     0x00, 0x00, 0x03, 0xC6, 0x03, 0xC6, 0x00, 0x00, 0x00, 0x00,
		     0x35, 0x33, 0x04, 0x04, 0x37, 0x00, 0x00, 0x00, 0x05, 0x08,
		     0x00, 0x00, 0x0A, 0x00, 0x01} },

	{ 0xD5, 38, {0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x20, 0x21,
		     0x24, 0x25, 0x18, 0x18, 0x18, 0x18, 0x00, 0x01, 0x04, 0x05,
		     0x02, 0x03, 0x06, 0x07, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		     0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },

	{ 0xD6, 38, {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x25, 0x24,
		     0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x07, 0x06, 0x03, 0x02,
		     0x05, 0x04, 0x01, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		     0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },

	{ 0xCC, 01, {0x02} },

	{ 0xC7, 04, {0x00, 0x80, 0x00, 0xC0} },

	{ 0xD2, 01, {0x33} },

	{ 0xE0, 42, {0x00, 0x0F, 0x16, 0x35, 0x3B, 0x3F, 0x21, 0x43, 0x07, 0x0B,
		     0x0D, 0x18, 0x0E, 0x10, 0x12, 0x11, 0x13, 0x06, 0x10, 0x13,
		     0x18, 0x00, 0x0F, 0x15, 0x35, 0x3B, 0x3F, 0x21, 0x42, 0x07,
		     0x0B, 0x0D, 0x18, 0x0D, 0x11, 0x13, 0x11, 0x12, 0x07, 0x11,
		     0x12, 0x17} },

	{ 0xB6, 03, {0x88, 0x88, 0x00} },

	{ 0x35, 01, {0x00} },

	{ 0x11, 00, {} },

	/* DISPLAY ON */
	{ 0x29, 00, {} },

	/* stop loop */
	{ REGFLAG_END_OF_TABLE, 0, {0x00} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Sleep Mode On */
	{ 0x10, 0, {} },
	{ REGFLAG_DELAY, 120, {} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(struct LCM_setting_table *table,
		       unsigned int count,
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
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}

}


/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	/* enable tearing-free */
	params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;
	params->dsi.mode = SYNC_PULSE_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_TWO_LANE;
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
	params->dsi.vertical_backporch = 5;
	params->dsi.vertical_frontporch = 9;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 40;
	params->dsi.horizontal_backporch = 41;
	params->dsi.horizontal_frontporch = 60;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

#ifndef CONFIG_FPGA_EARLY_PORTING
	params->dsi.PLL_CLOCK = 230;	/* this value must be in MTK suggested table */
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.cont_clock = 1;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x53;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;
}


static void lcm_init(void)
{
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_suspend(void)
{
	push_table(lcm_deep_sleep_mode_in_setting,
		   sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{
	lcm_init();
}

LCM_DRIVER hx8389c_dsi_vdo_lcm_drv = {

	.name = "hx8389c_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
};
