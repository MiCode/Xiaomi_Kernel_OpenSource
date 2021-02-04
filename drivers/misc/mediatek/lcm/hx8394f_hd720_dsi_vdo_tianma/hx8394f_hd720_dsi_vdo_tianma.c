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
/* ---------------------------------------- */
/* Local Constants */
/* ---------------------------------------- */

#define FRAME_WIDTH	(720)
#define FRAME_HEIGHT	(1440)
#define LCM_DENSITY	(320)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH    (62000)
#define LCM_PHYSICAL_HEIGHT   (110000)

#define REGFLAG_DELAY	0xFE
#define REGFLAG_END_OF_TABLE 0xFF
/* END OF REGISTERS MARKER */

/* ------------------------------------------ */
/* Local Variables */
/* ------------------------------------------ */

static struct LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


/* ------------------------------------------- */
/* Local Functions */
/* ------------------------------------------- */
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)\
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
	 lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg lcm_util.dsi_read_reg()


struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};


static struct LCM_setting_table lcm_initialization_setting[] = {

	/*
	 *  Note :

	 *  Data ID will depends on the following rule.

	 *  count of parameters > 1      => Data ID = 0x39
	 *  count of parameters = 1      => Data ID = 0x15
	 *  count of parameters = 0      => Data ID = 0x05

	 *  Structure Format :

	 * {DCS command, count of parameters, {parameter list}}
	 *  {REGFLAG_DELAY, milliseconds of time, {}},

	 *  ...

	 * Setting ending by predefined flag

	 * {REGFLAG_END_OF_TABLE, 0x00, {}}
	 */

	/* SET PASSWORD */
	{ 0xB9, 3, {0xFF, 0x83, 0x94} },


	/* sleep out */
	{ 0x11, 0, {} },
	{ REGFLAG_DELAY, 120, {} },

	/* write pwm frequence */
	{ 0x51, 1, {0xFF} },
	{ 0xC9, 9, {0x13, 0x00, 0x21, 0x1E, 0x31, 0x1E, 0x00, 0x91, 0x00} },
	{ REGFLAG_DELAY, 5, {} },
	{ 0x53, 1, {0x2C} },
	{ REGFLAG_DELAY, 5, {} },

	/* sleep out */
	{ 0x29, 0, {} },
	{ REGFLAG_DELAY, 20, {} },
};

static struct LCM_setting_table lcm_deep_sleep_setting[] = {
	/* Sleep Mode On */
	{ 0x28, 0, {} },
	{ REGFLAG_DELAY, 50, {} },

	{ 0x10, 0, {} },
	{ REGFLAG_DELAY, 120, {} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table_cmdq(void *cmdq, struct LCM_setting_table *table,
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

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
		}
	}
}

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


/* ------------------------------------- */
/* LCM Driver Implementations */
/* ------------------------------------- */

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
	//params->density = LCM_DENSITY;

	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;

	/* enable tearing-free */
	params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;
	params->dsi.mode = SYNC_PULSE_VDO_MODE;

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

	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 12;
	params->dsi.vertical_frontporch = 15;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 24;
	params->dsi.horizontal_backporch = 160;
	params->dsi.horizontal_frontporch = 160;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

#ifndef CONFIG_FPGA_EARLY_PORTING
	params->dsi.PLL_CLOCK = 218;
/* this value must be in MTK suggested table */
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.cont_clock = 1;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x53;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	int count = 0;

	pr_debug("%s,hx8394f backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	count = sizeof(bl_level) / sizeof(struct LCM_setting_table);

	push_table_cmdq(handle, bl_level, count, 1);
}

static void lcm_init(void)
{
	int a = 0;

	SET_RESET_PIN(1);
	MDELAY(50);
	SET_RESET_PIN(0);
	MDELAY(20);
	SET_RESET_PIN(1);
	MDELAY(5);

	a = sizeof(lcm_initialization_setting)/sizeof(struct LCM_setting_table);
	push_table(lcm_initialization_setting, a, 1);
}


static void lcm_suspend(void)
{
	int a = 0;

	a = sizeof(lcm_deep_sleep_setting)/sizeof(struct LCM_setting_table);
	push_table(lcm_deep_sleep_setting, a, 1);
}


static void lcm_resume(void)
{
	lcm_init();
}

struct LCM_DRIVER hx8394f_hd720_dsi_vdo_tianma_lcm_drv = {

	.name = "hx8394f_hd720_dsi_vdo_tianma",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
};
