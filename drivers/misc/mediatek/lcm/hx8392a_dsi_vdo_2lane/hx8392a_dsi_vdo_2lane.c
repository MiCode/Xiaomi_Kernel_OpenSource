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

#define FRAME_WIDTH										(720)
#define FRAME_HEIGHT										(1280)

#define REGFLAG_DELAY								0xFE
#define REGFLAG_END_OF_TABLE							0xFF	/* END OF REGISTERS MARKER */

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

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
				lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
				lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
				lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
				lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg \
				lcm_util.dsi_read_reg()


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

	/* sleep out */
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },

	/* SET PASSWORD */
	{0xB9, 3, {0xFF, 0x83, 0x92} },
	{REGFLAG_DELAY, 10, {} },

	/* set mipi 4 lane */
	{0xBA, 17, {0x11, 0x83, 0x00, 0xD6,
		    0xC5, 0x00, 0x09, 0xFF,
		    0x0F, 0x27, 0x03, 0x21,
		    0x27, 0x25, 0x20, 0x00,
		    0x10} },

	/* SET POWER */
	{0xB1, 13, {0x7C, 0x00, 0x43, 0xBB,
		    0x00, 0x1A, 0x1A, 0x2F,
		    0x36, 0x3F, 0x3F, 0x42,
		    0x7A} },

	/* SET DISPLAY RELATED REGISTER */
	{0xB2, 12, {0x08, 0xC8, 0x06, 0x06,
		    0x04, 0x84, 0x00, 0xFF,
		    0x06, 0x06, 0x04, 0x20} },

	/* SET CYC */
	{0xB4, 23, {0x00, 0x00, 0x05, 0x0A,
		    0x8F, 0x06, 0x0A, 0x95,
		    0x01, 0x07, 0x06, 0x0C,
		    0x02, 0x08, 0x08, 0x21,
		    0x04, 0x02, 0x08, 0x01,
		    0x04, 0x1A, 0x95} },

	/* set TE on */
	{0x35, 1, {0x00} },

	{0xBF, 4, {0x05, 0x60, 0x02, 0x00} },

	/* VCOM//64 */
	{0xB6, 1, {0x6A} },

	/* SET RGB OR BGR */
	{0x36, 1, {0x08} },

	/* SET RGB OR BGR */
	{0xC0, 2, {0x03, 0x94} },

	/* SET DSI VIDEO MODE */
	{0xC2, 1, {0x03} },

	{0xC6, 4, {0x35, 0x00, 0x20, 0x04} },

	/* SET PANEL */
	{0xCC, 1, {0x09} },

	{0xD4, 1, {0x00} },

	{0xD5, 23, {0x00, 0x01, 0x04, 0x00,
		    0x01, 0x67, 0x89, 0xAB,
		    0x45, 0xCC, 0xCC, 0xCC,
		    0x00, 0x10, 0x54, 0xBA,
		    0x98, 0x76, 0xCC, 0xCC,
		    0xCC, 0x00, 0x00} },

	{0xD8, 23, {0x00, 0x00, 0x05, 0x00,
		    0x9A, 0x00, 0x02, 0x95,
		    0x01, 0x07, 0x06, 0x00,
		    0x08, 0x08, 0x00, 0x1D,
		    0x08, 0x08, 0x08, 0x00,
		    0x00, 0x00, 0x77} },

	{0xE0, 34, {0x00, 0x12, 0x19, 0x33,
		    0x36, 0x3F, 0x28, 0x47,
		    0x06, 0x0C, 0x0E, 0x12,
		    0x14, 0x12, 0x14, 0x12,
		    0x1A, 0x00, 0x12, 0x19,
		    0x33, 0x36, 0x3F, 0x28,
		    0x47, 0x06, 0x0C, 0x0E,
		    0x12, 0x14, 0x12, 0x14,
		    0x12, 0x1A} },

	{0xE1, 34, {0x00, 0x12, 0x19, 0x33,
		    0x36, 0x3F, 0x28, 0x47,
		    0x06, 0x0C, 0x0E, 0x12,
		    0x14, 0x12, 0x14, 0x12,
		    0x1A, 0x00, 0x12, 0x19,
		    0x33, 0x36, 0x3F, 0x28,
		    0x47, 0x06, 0x0C, 0x0E,
		    0x12, 0x14, 0x12, 0x14,
		    0x12, 0x1A} },

	{0xE2, 34, {0x00, 0x12, 0x19, 0x33,
		    0x36, 0x3F, 0x28, 0x47,
		    0x06, 0x0C, 0x0E, 0x12,
		    0x14, 0x12, 0x14, 0x12,
		    0x1A, 0x00, 0x12, 0x19,
		    0x33, 0x36, 0x3F, 0x28,
		    0x47, 0x06, 0x0C, 0x0E,
		    0x12, 0x14, 0x12, 0x14,
		    0x12, 0x1A} },

	/* SET PIXEL FORMAT */
	{0x3A, 1, {0x77} },

	/* sleep out */
	{0x29, 0, {} },
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0x28, 0, {} },

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
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 15;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 76;
	params->dsi.horizontal_backporch = 76;
	params->dsi.horizontal_frontporch = 76;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

#ifndef CONFIG_FPGA_EARLY_PORTING
	params->dsi.PLL_CLOCK = 475;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.cont_clock = 1;

/*
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x53;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;
*/
}


static void lcm_init(void)
{
	pr_debug("lcm_init\n");
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


LCM_DRIVER hx8392a_dsi_vdo_2lane_lcm_drv = {
	.name = "hx8392a_vdo_2lane",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
};
