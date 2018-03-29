#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <asm-generic/gpio.h>

#include "lcm_drv.h"
#ifdef CONFIG_OF
#include <linux/regulator/consumer.h>
#endif

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(800)
#define FRAME_HEIGHT										(1280)

/* #define fps_120_on */
#define fps_60_on

#ifdef fps_120_on
static int lcm_fps = 120;
#endif
#ifdef fps_60_on
static int lcm_fps = 60;
#endif

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)							lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)							lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara,\
											force_update)

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#define REGFLAG_DELAY								0xFC
#define REGFLAG_END_OF_TABLE							0xFD	/* END OF REGISTERS MARKER */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

/* update initial param for IC nt35520 0.01 */
static struct LCM_setting_table lcm_initialization_setting[] = {
	/* page 0 */
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00} },
	/* 0x85->120Hz,0x05->60Hz, default set to 120hz is ok for 60 to 120 flash issue */
	{0xB5, 2, {0x85, 0x00} },
	/* TE = GPO1 */
	{0xC0, 1, {0x17} },

	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x01} },
	/* CP4 CP5 ON */
	{0xCE, 1, {0x04} },
	/* VGMP VGMN setting 4.3V/0.3V */
	{0xBC, 2, {0x68, 0x01} },
	{0xBD, 2, {0x68, 0x01} },

	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x02} },
	{0xB0, 1, {0x40} },
	{0xD1, 16,
	 {0x00, 0x00, 0x00, 0x11, 0x00, 0x30, 0x00, 0x49, 0x00, 0x5F, 0x00, 0x84, 0x00, 0xA1, 0x00,
	  0xD3} },
	{0xD2, 16,
	 {0x00, 0xFB, 0x01, 0x3B, 0x01, 0x6E, 0x01, 0xBF, 0x02, 0x00, 0x02, 0x02, 0x02, 0x3E, 0x02,
	  0x7E} },
	{0xD3, 16,
	 {0x02, 0xA6, 0x02, 0xDC, 0x03, 0x01, 0x03, 0x34, 0x03, 0x56, 0x03, 0x80, 0x03, 0x9E, 0x03,
	  0xC0} },
	{0xD4, 4, {0x03, 0xEE, 0x03, 0xFF} },

	{0xFF, 4, {0xAA, 0x55, 0xA5, 0x80} },
	/* For ESD enhancement */
	{0x6F, 1, {0x09} },
	{0xF7, 1, {0x82} },
	{0x6F, 1, {0x0B} },
	{0xF7, 1, {0xE0} },

	/* Turn on CBC */
	{0x55, 1, {0x81} },
	/* CABC dimming setting */
	{0x53, 1, {0x2C} },
	/* Backlght conrol */
	{0x51, 1, {0xFF} },

	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_fps_60_setting[] = {
	/* Switch to 60 */
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00} },
	{0xB5, 2, {0x05, 0x00} },

	/* Sleep In */
	{0x10, 0, {} },
	/* Sleep out */
	{0x11, 0, {} }
};

static struct LCM_setting_table lcm_fps_120_setting[] = {
	/* Switch to 120 */
	{0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00} },
	{0xB5, 2, {0x85, 0x00} },

	/* Sleep In */
	{0x10, 0, {} },
	/* Sleep out */
	{0x11, 0, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

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
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list,
					 force_update);
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

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
#endif

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_BGR;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 4;
	params->dsi.vertical_frontporch = 8;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 16;
	params->dsi.horizontal_backporch = 48;
	params->dsi.horizontal_frontporch = 16;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* video mode timing */
	params->dsi.word_count = FRAME_WIDTH * 3;

#ifdef fps_120_on
	params->dsi.PLL_CLOCK = 480;	/* 120hz */
#else
	params->dsi.PLL_CLOCK = 225;	/* 60hz */
#endif
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_init() enter\n");

	SET_RESET_PIN(1);
	MDELAY(100);

	SET_RESET_PIN(0);
	MDELAY(100);

	SET_RESET_PIN(1);
	MDELAY(100);

	push_table(0, lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

#else
	pr_debug("[Kernel/LCM] lcm_init() enter\n");
#endif
}

void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_suspend() enter\n");

	push_table(0, lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

	SET_RESET_PIN(1);
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);
#else
	pr_debug("[Kernel/LCM] lcm_suspend() enter\n");

	push_table(0, lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

	SET_RESET_PIN(1);
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);
#endif
}

void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_resume() enter\n");

	SET_RESET_PIN(1);
	MDELAY(100);

	SET_RESET_PIN(0);
	MDELAY(100);

	SET_RESET_PIN(1);
	MDELAY(100);

	push_table(0, lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

#else
	pr_debug("[Kernel/LCM] lcm_resume() enter\n");
	SET_RESET_PIN(1);
	MDELAY(100);

	SET_RESET_PIN(0);
	MDELAY(100);

	SET_RESET_PIN(1);
	MDELAY(100);

	push_table(0, lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
#endif
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
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
#endif
static int lcm_adjust_fps(void *cmdq, int fps, LCM_PARAMS *params)
{
#ifdef BUILD_LK
	dprintf(0, "%s:from %d to %d\n", __func__, lcm_fps, fps);
#else
	pr_debug("%s:from %d to %d\n", __func__, lcm_fps, fps);
#endif

	if (lcm_fps == fps)
		return 0;

	if (fps == 60) {
		lcm_fps = 60;
		push_table(cmdq, lcm_fps_60_setting,
			   sizeof(lcm_fps_60_setting) / sizeof(struct LCM_setting_table), 1);
	} else if (fps == 120) {
		lcm_fps = 120;
		push_table(cmdq, lcm_fps_120_setting,
			   sizeof(lcm_fps_120_setting) / sizeof(struct LCM_setting_table), 1);
	} else {
		return -1;
	}
	return 0;
}

LCM_DRIVER nt35523_wxga_dsi_vdo_8163_lcm_drv = {
	.name = "nt35523_wxga_dsi_vdo_8163",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
#if (LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
#if defined(fps_120_on) || defined(fps_60_on)
	.adjust_fps = lcm_adjust_fps,
#endif
};
