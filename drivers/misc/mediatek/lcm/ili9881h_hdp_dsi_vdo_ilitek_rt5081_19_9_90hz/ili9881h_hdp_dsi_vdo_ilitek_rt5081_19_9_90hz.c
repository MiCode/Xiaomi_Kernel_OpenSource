// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
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
#else
#include "disp_dts_gpio.h"
#endif


#define LCM_ID (0x98)

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
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH		(720)
#define FRAME_HEIGHT	(1520)

#define LCM_PHYSICAL_WIDTH		(64800)
#define LCM_PHYSICAL_HEIGHT		(129600)

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
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};


static struct LCM_setting_table init_setting_vdo[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x01} },
	{ 0x00, 0x01, {0x48} },
	{ 0x01, 0x01, {0x34} },
	{ 0x02, 0x01, {0x35} },
	{ 0x03, 0x01, {0x5E} },
	{ 0x08, 0x01, {0x86} },
	{ 0x09, 0x01, {0x01} },
	{ 0x0a, 0x01, {0x73} },
	{ 0x0b, 0x01, {0x00} },
	{ 0x0c, 0x01, {0x35} },
	{ 0x0d, 0x01, {0x35} },
	{ 0x0e, 0x01, {0x05} },
	{ 0x0f, 0x01, {0x05} },
	{ 0x28, 0x01, {0x48} },
	{ 0x29, 0x01, {0x86} },
	{ 0x2A, 0x01, {0x48} },
	{ 0x2B, 0x01, {0x86} },
	{ 0x31, 0x01, {0x07} },
	{ 0x32, 0x01, {0x23} },
	{ 0x33, 0x01, {0x00} },
	{ 0x34, 0x01, {0x0B} },
	{ 0x35, 0x01, {0x09} },
	{ 0x36, 0x01, {0x02} },
	{ 0x37, 0x01, {0x15} },
	{ 0x38, 0x01, {0x17} },
	{ 0x39, 0x01, {0x11} },
	{ 0x3A, 0x01, {0x13} },
	{ 0x3B, 0x01, {0x22} },
	{ 0x3C, 0x01, {0x01} },
	{ 0x3D, 0x01, {0x07} },
	{ 0x3E, 0x01, {0x07} },
	{ 0x3F, 0x01, {0x07} },
	{ 0x40, 0x01, {0x07} },
	{ 0x41, 0x01, {0x07} },
	{ 0x42, 0x01, {0x07} },
	{ 0x43, 0x01, {0x07} },
	{ 0x44, 0x01, {0x07} },
	{ 0x45, 0x01, {0x07} },
	{ 0x46, 0x01, {0x07} },
	{ 0x47, 0x01, {0x07} },
	{ 0x48, 0x01, {0x23} },
	{ 0x49, 0x01, {0x00} },
	{ 0x4A, 0x01, {0x0A} },
	{ 0x4B, 0x01, {0x08} },
	{ 0x4C, 0x01, {0x02} },
	{ 0x4D, 0x01, {0x14} },
	{ 0x4E, 0x01, {0x16} },
	{ 0x4F, 0x01, {0x10} },
	{ 0x50, 0x01, {0x12} },
	{ 0x51, 0x01, {0x22} },
	{ 0x52, 0x01, {0x01} },
	{ 0x53, 0x01, {0x07} },
	{ 0x54, 0x01, {0x07} },
	{ 0x55, 0x01, {0x07} },
	{ 0x56, 0x01, {0x07} },
	{ 0x57, 0x01, {0x07} },
	{ 0x58, 0x01, {0x07} },
	{ 0x59, 0x01, {0x07} },
	{ 0x5a, 0x01, {0x07} },
	{ 0x5b, 0x01, {0x07} },
	{ 0x5c, 0x01, {0x07} },
	{ 0x61, 0x01, {0x07} },
	{ 0x62, 0x01, {0x23} },
	{ 0x63, 0x01, {0x00} },
	{ 0x64, 0x01, {0x08} },
	{ 0x65, 0x01, {0x0A} },
	{ 0x66, 0x01, {0x02} },
	{ 0x67, 0x01, {0x12} },
	{ 0x68, 0x01, {0x10} },
	{ 0x69, 0x01, {0x16} },
	{ 0x6a, 0x01, {0x14} },
	{ 0x6b, 0x01, {0x22} },
	{ 0x6c, 0x01, {0x01} },
	{ 0x6d, 0x01, {0x07} },
	{ 0x6e, 0x01, {0x07} },
	{ 0x6f, 0x01, {0x07} },
	{ 0x70, 0x01, {0x07} },
	{ 0x71, 0x01, {0x07} },
	{ 0x72, 0x01, {0x07} },
	{ 0x73, 0x01, {0x07} },
	{ 0x74, 0x01, {0x07} },
	{ 0x75, 0x01, {0x07} },
	{ 0x76, 0x01, {0x07} },
	{ 0x77, 0x01, {0x07} },
	{ 0x78, 0x01, {0x23} },
	{ 0x79, 0x01, {0x00} },
	{ 0x7a, 0x01, {0x09} },
	{ 0x7b, 0x01, {0x0B} },
	{ 0x7c, 0x01, {0x02} },
	{ 0x7d, 0x01, {0x13} },
	{ 0x7e, 0x01, {0x11} },
	{ 0x7f, 0x01, {0x17} },
	{ 0x80, 0x01, {0x15} },
	{ 0x81, 0x01, {0x22} },
	{ 0x82, 0x01, {0x01} },
	{ 0x83, 0x01, {0x07} },
	{ 0x84, 0x01, {0x07} },
	{ 0x85, 0x01, {0x07} },
	{ 0x86, 0x01, {0x07} },
	{ 0x87, 0x01, {0x07} },
	{ 0x88, 0x01, {0x07} },
	{ 0x89, 0x01, {0x07} },
	{ 0x8a, 0x01, {0x07} },
	{ 0x8b, 0x01, {0x07} },
	{ 0x8c, 0x01, {0x07} },
	{ 0xd0, 0x01, {0x01} },
	{ 0xd1, 0x01, {0x00} },
	{ 0xe2, 0x01, {0x00} },
	{ 0xe6, 0x01, {0x22} },
	{ 0xe7, 0x01, {0x54} },
	{ 0xB0, 0x01, {0x33} },
	{ 0xB1, 0x01, {0x33} },
	{ 0xB2, 0x01, {0x00} },
	{ 0xE7, 0x01, {0x54} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x02} },
	{ 0x40, 0x01, {0x52} },
	{ 0x4B, 0x01, {0x5A} },
	{ 0x4D, 0x01, {0x4E} },
	{ 0x1A, 0x01, {0x48} },
	{ 0x4E, 0x01, {0x00} },
	{ 0x1A, 0x01, {0x48} },
	{ 0x70, 0x01, {0x34} },
	{ 0x73, 0x01, {0x0A} },
	{ 0x79, 0x01, {0x06} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x05} },
	{ 0x03, 0x01, {0x01} },
	{ 0x04, 0x01, {0x43} },
	{ 0x58, 0x01, {0x63} },
	{ 0x63, 0x01, {0x88} },
	{ 0x64, 0x01, {0x88} },
	{ 0x68, 0x01, {0x65} },
	{ 0x69, 0x01, {0x7F} },
	{ 0x6A, 0x01, {0xC9} },
	{ 0x6B, 0x01, {0xCF} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0x11, 0x01, {0x03} },
	{ 0x13, 0x01, {0x15} },
	{ 0x14, 0x01, {0x41} },
	{ 0x15, 0x01, {0xc2} },
	{ 0x16, 0x01, {0x40} },
	{ 0x17, 0x01, {0x48} },
	{ 0x18, 0x01, {0x3B} },
	{ 0xD6, 0x01, {0x85} },
	{ 0x27, 0x01, {0x20} },
	{ 0x28, 0x01, {0x20} },
	{ 0x2E, 0x01, {0x01} },
	{ 0xC0, 0x01, {0xF7} },
	{ 0xC1, 0x01, {0x02} },
	{ 0xC2, 0x01, {0x04} },
	{ 0x48, 0x01, {0x0F} },
	{ 0x4D, 0x01, {0x80} },
	{ 0x4E, 0x01, {0x40} },
	{ 0x7C, 0x01, {0x40} },
	{ 0x94, 0x01, {0x00} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x07} },
	{ 0x0F, 0x01, {0x02} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x08} },
	{ 0xE0, 0x27, {0x00, 0x24, 0x3C, 0x51, 0x74, 0x40,
			0x97, 0xB6, 0xDE, 0x03, 0x55, 0x42, 0x7A,
			0xAF, 0xE4, 0xAA, 0x1B, 0x5E, 0x88, 0xBB,
			0xFE, 0xE6, 0x1C, 0x5E, 0x93, 0x03, 0xEC} },
	{ 0xE1, 0x27, {0x00, 0x24, 0x3C, 0x51, 0x74, 0x40,
			0x97, 0xB6, 0xDE, 0x03, 0x55, 0x42, 0x7A,
			0xAF, 0xE4, 0xAA, 0x1B, 0x5E, 0x88, 0xBB,
			0xFE, 0xE6, 0x1C, 0x5E, 0x93, 0x03, 0xEC} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x0E} },
	{ 0x00, 0x01, {0xA0} },
	{ 0x13, 0x01, {0x05} },
	{ 0x11, 0x01, {0x90} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0xD5, 0x01, {0x34} },
	{ 0x58, 0x01, {0xBD} },
	{ 0x94, 0x01, {0x01} },
	{ 0x13, 0x01, {0x4A} },
	{ 0x14, 0x01, {0x2F} },
	{ 0x15, 0x01, {0x0E} },
	{ 0x16, 0x01, {0x2F} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x11, 0x00, {} },
	{ REGFLAG_DELAY, 120, {} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x02} },
	{ 0x47, 0x01, {0x00} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0xD5, 0x01, {0x30} },
	{ 0x58, 0x01, {0xD5} },
	{ 0x94, 0x01, {0x01} },
	{ 0x17, 0x01, {0xFF} },
	{ 0x18, 0x01, {0x00} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x29, 0x00, {} },
	{ REGFLAG_DELAY, 20, {} },
	{ 0x35, 0x01, {0x00} }
};


static struct LCM_setting_table bl_level[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{0x51, 2, {0x0F, 0xFF} },
	/* {0x53, 1, {0x2C} }, */
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
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

/*DynFPS*/
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;

	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 9000;/*real vact timing fps * 100*/

	/*traversing array must less than DFPS_LEVELS*/
	/*DPFS_LEVEL0*/
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/*if mipi clock solution*/
	/*dfps_params[0].PLL_CLOCK = xx;*/
	/*dfps_params[0].data_rate = xx; */
	/*if HFP solution*/
	/*dfps_params[0].horizontal_frontporch = xx;*/
	dfps_params[0].vertical_frontporch = 1291;
	dfps_params[0].vertical_frontporch_for_low_power = 2500;

	/*if need mipi hopping params add here*/
	/*dfps_params[0].PLL_CLOCK_dyn =xx;
	 *dfps_params[0].horizontal_frontporch_dyn =xx ;
	 * dfps_params[0].vertical_frontporch_dyn = 1291;
	 */

	/*DPFS_LEVEL1*/
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/*if mipi clock solution*/
	/*dfps_params[1].PLL_CLOCK = xx;*/
	/*dfps_params[1].data_rate = xx; */
	/*if HFP solution*/
	/*dfps_params[1].horizontal_frontporch = xx;*/
	dfps_params[1].vertical_frontporch = 54;
	dfps_params[1].vertical_frontporch_for_low_power = 2500;

	/*if need mipi hopping params add here*/
	/*dfps_params[1].PLL_CLOCK_dyn =xx;
	 *dfps_params[1].horizontal_frontporch_dyn =xx ;
	 * dfps_params[1].vertical_frontporch_dyn= 54;
	 * dfps_params[1].vertical_frontporch_for_low_power_dyn =xx;
	 */

	dsi->dfps_num = 2;
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	pr_debug("%s:lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
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

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 20;
	params->dsi.vertical_frontporch = 320;
	params->dsi.vertical_frontporch_for_low_power = 540;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 8;
	params->dsi.horizontal_backporch = 10;
	params->dsi.horizontal_frontporch = 36;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 424;
#else
	params->dsi.PLL_CLOCK = 424;
#endif

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
}

static void lcm_init_power(void)
{
	/*pr_debug("lcm_init_power\n");*/

	display_bias_enable();
	MDELAY(15);

}

static void lcm_suspend_power(void)
{
	/*pr_debug("lcm_suspend_power\n");*/

	SET_RESET_PIN(0);
	MDELAY(2);
	display_bias_disable();

}

static void lcm_resume_power(void)
{
	/*pr_debug("lcm_resume_power\n");*/

	/*lcm_init_power();*/
	display_bias_enable();
	MDELAY(15);
}

static void lcm_init(void)
{

	/*pr_debug("lcm_init\n");*/

	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);

	push_table(NULL, init_setting_vdo,
		sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table),
		1);
	pr_debug("ili9881c----rt5081----lcm mode = vdo mode :%d----\n",
		lcm_dsi_mode);

}

static void lcm_suspend(void)
{
	/*pr_debug("lcm_suspend\n");*/

	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
}

static void lcm_resume(void)
{
	/*pr_debug("lcm_resume\n");*/

	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y,
	unsigned int width, unsigned int height)
{

}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0, version_id = 0;
	unsigned char buffer[2];
	unsigned int array[16];
	struct LCM_setting_table switch_table_page1[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x01} }
	};
	struct LCM_setting_table switch_table_page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(NULL, switch_table_page1,
		sizeof(switch_table_page1) / sizeof(struct LCM_setting_table),
		1);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x00, buffer, 1);
	id = buffer[0];		/* we only need ID */

	read_reg_v2(0x01, buffer, 1);
	version_id = buffer[0];

	pr_debug("%s,ili9881c_id=0x%08x,version_id=0x%x\n",
		__func__, id, version_id);
	push_table(NULL, switch_table_page0,
		sizeof(switch_table_page0) / sizeof(struct LCM_setting_table),
		1);

	if (id == LCM_ID && version_id == 0x81)
		return 1;
	else
		return 0;

}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		pr_debug("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	pr_debug("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;

	unsigned char x0_MSB = (x0 & 0xFF);

	unsigned int data_array[2];
	unsigned char read_buf[2];
	unsigned int num1 = 0, num2 = 0;

	struct LCM_setting_table switch_table_page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};

	struct LCM_setting_table switch_table_page2[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x02} }
	};


	MDELAY(20);

	num1 = sizeof(switch_table_page2) / sizeof(struct LCM_setting_table);

	push_table(NULL, switch_table_page2, num1, 1);
	pr_debug("before read ATA check x0_MSB = 0x%x\n", x0_MSB);
	pr_debug("before read ATA check read buff = 0x%x\n", read_buf[0]);

	data_array[0] = 0x00023902;
	data_array[1] = x0_MSB << 8 | 0x30;

	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00013700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x30, read_buf, 1);

	pr_debug("after read ATA check size = 0x%x\n", read_buf[0]);

	num2 = sizeof(switch_table_page0) / sizeof(struct LCM_setting_table);

	push_table(NULL, switch_table_page0, num2, 1);

	if (read_buf[0] == x0_MSB)
		ret = 1;
	else
		ret = 0;

	return ret;
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	pr_debug("%s,ili9881c backlight: level = %d\n", __func__, level);

	bl_level[1].para_list[0] = (level&0xFF) >> 4;
	bl_level[1].para_list[1] = (level&0x0F) << 4;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
	return NULL;
}

#if (LCM_DSI_CMD_MODE)
/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{

}
#endif

struct LCM_DRIVER ili9881h_hdp_dsi_vdo_ilitek_rt5081_19_9_90hz_lcm_drv = {
	.name = "ili9881h_hdp_dsi_vdo_ilitek_rt5081_19_9_90hz_drv",
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
	.switch_mode = lcm_switch_mode,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
