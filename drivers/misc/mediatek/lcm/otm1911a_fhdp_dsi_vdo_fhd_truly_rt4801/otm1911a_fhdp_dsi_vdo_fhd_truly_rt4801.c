// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

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
#define FRAME_HEIGHT		(1920)
#define VIRTUAL_WIDTH		(1080)
#define VIRTUAL_HEIGHT		(2160)


/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(129000)
#define LCM_DENSITY			(480)

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


/* i2c control start */

#define LCM_I2C_ADDR 0x3E
#define LCM_I2C_BUSNUM  1	/* for I2C channel 0 */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
#define LCM_ID_OTM1911A 0x40


/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);


/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;

};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{.compatible = "mediatek,I2C_LCD_BIAS",},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = {
	{LCM_I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect               = _lcm_i2c_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = LCM_I2C_ID_NAME,
		   .of_match_table = _lcm_i2c_of_match,
		   },

};

/*****************************************************************************
 * Function
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	pr_debug(
		"[LCM][I2C]NT name=%s addr=0x%x\n", client->name, client->addr);
	_lcm_i2c_client = client;
	return 0;
}


static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}


static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/* i2c control end */

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
//OOTM1911A_H565DAN01_0F_initial_TypeB1_V2.0

	{0x00, 0x1, {0x00} },
	{0xFF, 0x3, {0x19, 0x11, 0x01} },

	{0x00, 0x1, {0x80} },
	{0xFF, 0x2, {0x19, 0x11} },
	//RTN=81 @ normal display //0x49 RTN=74
	{0x00, 0x1, {0x80} },
	{0xC0, 0x7,
		{0x51, 0x00, 0x08, 0x08, 0x51, 0x04, 0x00} },


	{0x00, 0x1, {0x92} },
	{0xB3, 0x2, {0x18, 0x06} },

	{0x00, 0x1, {0x8B} },
	{0xC0, 0x1, {0x88} },	//Panel Driving Mode  //0x88
	{0x00, 0x1, {0xb0} },
	{0xB3, 0x4, {0x04, 0x38, 0x08, 0x70} },	//1080RGBx2160
	//vst
	{0x00, 0x1, {0x80} },
	{0xc2, 0x4, {0x84, 0x01, 0x33, 0x34} },	//vst1
	{0x00, 0x1, {0x84} },

	{0xc2, 0x2, {0x00, 0x00} },	//vst2

	//ckv
	{0x00, 0x1, {0xb0} },
	{0xc2, 0xE,
		{0x85, 0x05, 0x11, 0x09, 0x00, 0x85, 0x02,
		0x22, 0x85, 0x03, 0x33, 0x85, 0x04, 0x00} },
	//ckv1 + ckv2 + ckv3 + ckv4
	{0x00, 0x1, {0xc0} },
	{0xc2, 0xE,
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },
	//ckv5 + ckv6 + ckv7 + ckv8
	{0x00, 0x1, {0xd0} },
	{0xc2, 0x5, {0x33, 0x33, 0x00, 0x00, 0xf0} },	//ckv period

	//vend
	{0x00, 0x1, {0xE0} },
	{0xc2, 0x6, {0x02, 0x01, 0x09, 0x07, 0x00, 0x00} },

	//rst
	{0x00, 0x1, {0xF0} },
	{0xc2, 0x5, {0x80, 0xff, 0x01, 0x08, 0x07} },
	//cic
	{0x00, 0x1, {0x90} },
	{0xc2, 0xB, {0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },

	//pch
	{0x00, 0x1, {0xA2} },
	{0xC5, 0x1, {0x00} },

	//pwron/pwrof/lvd
	{0x00, 0x1, {0x90} },
	{0xcb, 0x3, {0x00, 0x00, 0x00} },

	{0x00, 0x1, {0xc0} },
	{0xcb, 0xC, {0x04, 0x04, 0x04, 0xf4, 0x00,
		0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00} },

	{0x00, 0x1, {0xf0} },	// elvin
	{0xcb, 0x4, {0xff, 0x30, 0x33, 0x80} },

	{0x00, 0x1, {0x80} },
	{0xcd, 0x1, {0x01} },	//map_sel

	//ckh
	{0x00, 0x1, {0x94} },
	{0xc0, 0x7, {0x00, 0x01, 0x06,
		0x00, 0x01, 0x15, 0x05} },

	{0x00, 0x1, {0x0} },
	{0xD8, 0x2, {0x2B, 0x2B} },

	{0x00, 0x1, {0x00} },	//For K16, Vcom by OTP
	{0xD9, 0x2, {0xAD, 0x00} },	//VCOM=-0.27V

	{0x00, 0x1, {0x00} },
	{0xE0, 0x1, {0x01} },	//Gamma Separate Change

//170929-2 foc to2.2
	{0x00, 0x1, {0x00} },
	{0xE1, 0x25,
	 {0x5c, 0x88, 0xdd, 0x13, 0x40, 0x49, 0x72,
		0xb5, 0xe4, 0x55, 0xca, 0xf6, 0x1c, 0x39, 0xa5,
		0x5c, 0x7a, 0xe9, 0x08, 0x9a, 0x24, 0x42, 0x5d,
		0x7f, 0xaa, 0xa7, 0xaa, 0xd9, 0x18, 0xea,
		0x3d, 0x60, 0x9b, 0xe3, 0xff, 0xf4, 0x03} },
	//R POS
	{0x00, 0x1, {0x00} },
	{0xE2, 0x25,
	 {0x47, 0x88, 0xdd, 0x13, 0x40, 0x49, 0x72,
		0xb5, 0xe4, 0x55, 0xca, 0xf6, 0x1c, 0x39, 0xa5,
		0x5c, 0x7a, 0xe9, 0x08, 0x9a, 0x24, 0x42, 0x5d,
		0x7f, 0xaa, 0xa7, 0xaa, 0xd9, 0x18, 0xea,
		0x3d, 0x60, 0x9b, 0xe3, 0xff, 0xc4, 0x03} },
	//R NEG
	{0x00, 0x1, {0x00} },
	{0xE3, 0x25,
	 {0x5c, 0x86, 0xdb, 0x2a, 0x40, 0x4f, 0x7a, 0xb9,
		0xea, 0x55, 0xd1, 0xfb, 0x1f, 0x3d, 0xa5,
		0x5e, 0x7e, 0xed, 0x0a, 0x9a, 0x26, 0x45,
		0x60, 0x81, 0xaa, 0xaa, 0xad, 0xda, 0x17, 0xea,
		0x3c, 0x5f, 0x9c, 0xe4, 0xff, 0xf4, 0x03} },
	//G POS
	{0x00, 0x1, {0x00} },
	{0xE4, 0x25,
	 {0x47, 0x86, 0xdb, 0x2a, 0x40, 0x4f, 0x7a,
	 0xb9, 0xea, 0x55, 0xd1, 0xfb, 0x1f, 0x3d, 0xa5,
	  0x5e, 0x7e, 0xed, 0x0a, 0x9a, 0x26, 0x45,
	  0x60, 0x81, 0xaa, 0xaa, 0xad, 0xda, 0x17, 0xea,
	  0x3c, 0x5f, 0x9c, 0xe4, 0xff, 0xc4, 0x03} },
	//G NEG
	{0x00, 0x1, {0x00} },
	{0xE5, 0x25,
	 {0x5c, 0x9f, 0x01, 0x46, 0x50, 0x6c, 0x92,
	 0xce, 0xfb, 0x55, 0xe1, 0x08, 0x2a, 0x47, 0xa9,
	  0x67, 0x84, 0xf2, 0x0e, 0x9a, 0x2b, 0x49,
	  0x64, 0x85, 0xaa, 0xad, 0xb3, 0xe4, 0x20, 0xea,
	  0x43, 0x66, 0xa0, 0xe6, 0xff, 0xf4, 0x03} },
	//B POS
	{0x00, 0x1, {0x00} },
	{0xE6, 0x25,
	 {0x47, 0x9f, 0x01, 0x46, 0x50, 0x6c, 0x92,
	 0xce, 0xfb, 0x55, 0xe1, 0x08, 0x2a, 0x47, 0xa9,
	  0x67, 0x84, 0xf2, 0x0e, 0x9a, 0x2b, 0x49,
	  0x64, 0x85, 0xaa, 0xad, 0xb3, 0xe4, 0x20, 0xea,
	  0x43, 0x66, 0xa0, 0xe6, 0xff, 0xc4, 0x03} },
	//B POS
//Gamma_End--------------------------
	//--------Down Power  Consumption-----------------

	{0x00, 0x1, {0x90} },
	//GAP 8->4 ; AP 8->5, 20160420 For Power Saving Setting Modify
	{0xC5, 0x1, {0x45} },

	{0x00, 0x1, {0x91} },
	//SAP 8->A, 20160524 For special pattern horizontal band
	{0xC5, 0x1, {0xA0} },
	//VGH  0x05=AVDD*2(default)     0x0F=AVDD*2-AVEE
	//{0x39, 0, 0, 0, 0, 2,{0x00,0x81}},
	//{0x39, 0, 0, 0, 0, 2,{0xC5,0x0F}},

	{0x00, 0x1, {0x83} },	//VGH=8.8V  VGH Clamp off
	{0xC5, 0x1, {0x1B} },

	{0x00, 0x1, {0x84} },	//VGL  0xAE=-8.6V,  0xC6=-11V
	{0xC5, 0x1, {0xAE} },

	{0x00, 0x1, {0xA0} },	//VGHO  0x98=8.5V,  0xAC=10.5V,   0x84=6.5V
	{0xC5, 0x1, {0x98} },

	{0x00, 0x1, {0xA1} },	//VGLO  0xA8=-8.0V, 0xBC=-10V,    0x94=-6V
	{0xC5, 0x1, {0xA8} },

	{0x00, 0x1, {0x90} },
	{0xC3, 0x4, {0x00, 0x00, 0x00, 0x00} },

	{0x00, 0x1, {0x86} },
	{0xC3, 0x1, {0x00} },

	{0x00, 0x1, {0x91} },
	{0xC1, 0x1, {0x0F} },	//timeout open

	{0x00, 0x1, {0x80} },
	{0xC4, 0x1, {0x01} },	//Source v-blank output min.

	{0x00, 0x1, {0x81} },
	{0xC4, 0x1, {0x02} },	//Chop 2line/2frame

	{0x00, 0x1, {0xB1} },
	{0xC5, 0x1, {0x08} },	//Gamma Calibration control disable

	{0x00, 0x1, {0xB2} },	//Gamma chop = 2line/2frame
	{0xC5, 0x1, {0x22} },

	{0x00, 0x1, {0x80} },
	{0xC3, 0x8, {0x00, 0x00, 0x00,
		0x22, 0x22, 0x00, 0x22, 0x22} },	//gnd eq

	{0x00, 0x1, {0x90} },
	{0xC3, 0x4, {0x20, 0x20, 0x02, 0x02} },	//VSP_VSN EQ


	{0x00, 0x1, {0x92} },
	{0xC5, 0x1, {0x33} },	//vdd lvdsvdd //EMI improving

	{0x00, 0x1, {0x81} },
	{0xC1, 0x3, {0xB0, 0xC0, 0xF0} },	//SSC //EMI improving
	// 20170731 update follow H593 discharge source

	{0x00, 0x1, {0x89} },
	{0xc0, 0x2, {0x10, 0x14} },

	{0x00, 0x1, {0x90} },
	{0xcb, 0x3, {0x00, 0x00, 0x0C} },

	{0x00, 0x1, {0xC0} },
	{0xcb, 0xC, {0x05, 0x04, 0x04, 0xf4,
		0x00, 0x00, 0x04, 0x00, 0x04, 0xf3, 0x00, 0x00} },

	{0x00, 0x1, {0xF0} },
	{0xcb, 0x4, {0xff, 0x30, 0x33, 0x80} },

	{0x00, 0x1, {0x84} },
	{0xF5, 0x1, {0x9A} },	//Vcom active region

	//TE with mode 1
	{0x35, 0x1, {0x00} },
	//PWM BL
	{0x51, 0x1, {0xFC} },	//4B=6mA,0C=1mA
	{0x53, 0x1, {0x24} },
	{0x55, 0x1, {0x10} },

	//Sleep Out
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	//Display On
	{0x29, 0, {} },

	{REGFLAG_DELAY, 20, {} },

#endif
};

static struct LCM_setting_table
__maybe_unused lcm_deep_sleep_mode_in_setting[] = {
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table __maybe_unused lcm_sleep_out_setting[] = {
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 50, {} },
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
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
			break;
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
	params->virtual_width = VIRTUAL_WIDTH;
	params->virtual_height = VIRTUAL_HEIGHT;
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

	params->dsi.vertical_sync_active = 3;
	params->dsi.vertical_backporch = 15;
	params->dsi.vertical_frontporch = 10;
	params->dsi.vertical_frontporch_for_low_power = 750;	//OTM no data
	params->dsi.vertical_active_line = VIRTUAL_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 42;
	params->dsi.horizontal_frontporch = 42;
	params->dsi.horizontal_active_pixel = VIRTUAL_WIDTH;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 500;
	params->dsi.PLL_CK_CMD = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;

	/* for ARR 2.0 */
	params->max_refresh_rate = 60;
	params->min_refresh_rate = 45;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	if (lcm_util.set_gpio_lcd_enp_bias) {
		lcm_util.set_gpio_lcd_enp_bias(1);

		_lcm_i2c_write_bytes(0x0, 0xf);
		_lcm_i2c_write_bytes(0x1, 0xf);
	} else
		LCM_LOGI("set_gpio_lcd_enp_bias not defined...\n");
}

static void lcm_suspend_power(void)
{
	SET_RESET_PIN(0);
	if (lcm_util.set_gpio_lcd_enp_bias)
		lcm_util.set_gpio_lcd_enp_bias(0);
	else
		LCM_LOGI("set_gpio_lcd_enp_bias not defined...\n");
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	SET_RESET_PIN(0);
	lcm_init_power();
}

static void lcm_init(void)
{
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(1);

	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);

	push_table(NULL,
		init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	LCM_LOGI(
		"otm1911a_fhdp-tps6132-lcm mode=vdo mode:%d\n", lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
		ARRAY_SIZE(lcm_suspend_setting), 1);
}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int id[3] = {0x40, 0, 0};
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

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	LCM_LOGI("%s,otm1911a backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static void lcm_update(unsigned int x,
	unsigned int y, unsigned int width, unsigned int height)
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

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00013700;  /* read id return 1byte */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xDA, buffer, 1);
	id = buffer[0];

	LCM_LOGI("%s,otm1911a id = 0x%08x\n", __func__, id);

	if (id == LCM_ID_OTM1911A)
		return 1;
	else
		return 0;

}

struct LCM_DRIVER otm1911a_fhdp_dsi_vdo_fhd_truly_rt4801_lcm_drv = {
	.name = "otm1911a_fhdp_dsi_vdo_fhd_truly_rt4801_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
};
