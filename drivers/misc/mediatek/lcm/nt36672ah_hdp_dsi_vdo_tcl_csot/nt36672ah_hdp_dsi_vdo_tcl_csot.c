// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG	"LCM"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/upmu_hw.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
//	#include <platform/gpio_const.h> /*lcm power is provided by i2c*/
#else
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
//	#include <mt-plat/mtk_gpio_core.h>
//	#include "mt-plat/upmu_common.h"
//	#include "mt-plat/mtk_gpio.h"
#include "disp_dts_gpio.h"
#include <linux/i2c.h> /*lcm power is provided by i2c*/
#endif

#include "lcm_drv.h"

// --------------------------------------------------------------------------
//  Define Print Log Level
// --------------------------------------------------------------------------
#ifdef BUILD_LK
#define LCM_LOGI(string, args...)   dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)   dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGE(string, args...)   dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)      pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)      pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGE(fmt, args...)      pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

// --------------------------------------------------------------------------
//  Extern Constants
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
//  Extern Variable
// --------------------------------------------------------------------------
//extern unsigned int g_lcm_inversion;
/* LCM inversion, for *#87# lcm flicker test */

// --------------------------------------------------------------------------
//  Extern Functions
// --------------------------------------------------------------------------


// --------------------------------------------------------------------------
//  Local Constants
// --------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE                0
#define FRAME_WIDTH						(720)
#define FRAME_HEIGHT					(1500)
#define PHYSICAL_WIDTH					(68040)
#define PHYSICAL_HEIGHT					(141750)

#define REGFLAG_PORT_SWAP               0xFFFA
#define REGFLAG_DELAY                   0xFFFC
#define REGFLAG_UDELAY                  0xFFFB
#define REGFLAG_END_OF_TABLE            0xFFFD

#ifndef GPIO_LCM_RST
#define GPIO_LCM_RST                (GPIO45 | 0x80000000)/* H624 use */
#endif

#ifndef GPIO_LCD_BIAS_ENP_PIN
#define GPIO_LCD_BIAS_ENP_PIN       (GPIO154 | 0x80000000) /* H624 use */
#endif
#ifndef GPIO_LCD_BIAS_ENN_PIN
#define GPIO_LCD_BIAS_ENN_PIN       (GPIO159 | 0x80000000) /* H624 use */
#endif

// ----------------------------------------------------------------
//  LCM power is provided by I2C
// ------------------------------------------------------------------
/* Define ----------------------------------------------------------*/
#define I2C_I2C_LCD_BIAS_CHANNEL 5  //for I2C channel 5
#define DCDC_I2C_BUSNUM  I2C_I2C_LCD_BIAS_CHANNEL//for I2C channel 5
#define DCDC_I2C_ID_NAME "nt5038"
#define DCDC_I2C_ADDR 0x3E

struct NT5038_SETTING_TABLE {
	unsigned char cmd;
	unsigned char data;
};

static struct NT5038_SETTING_TABLE nt5038_cmd_data[3] = {
	{ 0x00, 0x12 },
	{ 0x01, 0x12 },
	{ 0x03, 0x33 }
};

/* Variable --------------------------------------------------------*/
#ifndef BUILD_LK
#if defined(CONFIG_MTK_LEGACY)
static struct i2c_board_info nt5038_board_info __initdata = {
	I2C_BOARD_INFO(DCDC_I2C_ID_NAME, DCDC_I2C_ADDR)};
#else
static const struct of_device_id lcm_of_match[] = {
	{.compatible = "mediatek,I2C_LCD_BIAS"},
	{},
};
#endif

struct i2c_client *nt5038_i2c_client;

/* Functions Prototype ----------------------------------------------*/
static int nt5038_probe(struct i2c_client *client,
						const struct i2c_device_id *id);
static int nt5038_remove(struct i2c_client *client);

/* Data Structure ------------------------------------------------*/
struct nt5038_dev {
	struct i2c_client *client;
};

static const struct i2c_device_id nt5038_id[] = {
	{ DCDC_I2C_ID_NAME, 0 },
	{ }
};

/* I2C Driver  --------------------------------------------------*/
static struct i2c_driver nt5038_iic_driver = {
	.id_table		= nt5038_id,
	.probe			= nt5038_probe,
	.remove			= nt5038_remove,
	.driver			= {
		.owner			= THIS_MODULE,
		.name			= "nt5038",
#if !defined(CONFIG_MTK_LEGACY)
		.of_match_table = lcm_of_match,
#endif
	},
};

/* Functions ----------------------------------------------------*/
static int nt5038_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	nt5038_i2c_client  = client;
	return 0;
}

static int nt5038_remove(struct i2c_client *client)
{
	nt5038_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int nt5038_i2c_write_byte(unsigned char addr,
				 unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = nt5038_i2c_client;
	char write_data[2] = {0};

	if (client == NULL) {
		LCM_LOGE("ERROR!! nt5038_i2c_client is null\n");
		return 0;
	}
	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		LCM_LOGD("nt5038 write data fail !!\n");
	return ret;
}

static int __init nt5038_iic_init(void)
{
#if defined(CONFIG_MTK_LEGACY)
	i2c_register_board_info(DCDC_I2C_BUSNUM, &nt5038_board_info, 1);
#endif
	i2c_add_driver(&nt5038_iic_driver);
	return 0;
}

static void __exit nt5038_iic_exit(void)
{
	i2c_del_driver(&nt5038_iic_driver);
}

module_init(nt5038_iic_init);
module_exit(nt5038_iic_exit);
MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK NT5038 I2C Driver");
MODULE_LICENSE("GPL");
#else
#define NT5038_SLAVE_ADDR_WRITE  0x7C
static struct mt_i2c_t NT5038_i2c;
static int nt5038_i2c_write_byte(kal_uint8 addr, kal_uint8 value)
{
	kal_uint32 ret_code = I2C_OK;
	kal_uint8 write_data[2];
	kal_uint16 len;

	write_data[0] = addr;
	write_data[1] = value;
	NT5038_i2c.id = I2C_I2C_LCD_BIAS_CHANNEL;// I2C2;
	/* Since i2c will left shift 1 bit, */
	/* we need to set FAN5405 I2C address to >>1 */
	NT5038_i2c.addr = (NT5038_SLAVE_ADDR_WRITE >> 1);
	NT5038_i2c.mode = ST_MODE;
	NT5038_i2c.speed = 100;
	len = 2;
	ret_code = i2c_write(&NT5038_i2c, write_data, len);
	printf("%s: i2c_write: addr:0x%x, value:0x%x ret_code: %d\n",
		   __func__, addr, value, ret_code);
	return ret_code;
}
#endif

// ---------------------------------------------------------------------------
//  Local Variable
// ---------------------------------------------------------------------------
static struct LCM_UTIL_FUNCS lcm_util;

// ---------------------------------------------------------------------------
//  Local function
// ---------------------------------------------------------------------------
#define MDELAY(n)                       (lcm_util.mdelay(n))
#define UDELAY(n)                       (lcm_util.udelay(n))
#ifdef BUILD_LK
#define SET_RESET_PIN(v)            (mt_set_gpio_out(GPIO_LCM_RST, (v)))
#endif

#ifndef BUILD_LK
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#endif
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
		lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
		lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[120];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	//----------------------LCD initial code start----------------------//
	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x07, 1, {0xA8} },
	{0x0F, 1, {0xA4} },
	{0x61, 1, {0x82} },
	{0x62, 1, {0xA2} },
	{0x69, 1, {0x99} },
	{0x6D, 1, {0x44} },
	{0x94, 1, {0x00} },
	{0x95, 1, {0xF5} },
	{0x96, 1, {0xF5} },
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x1C} },
	{0x01, 1, {0x1C} },
	{0x02, 1, {0x1C} },
	{0x03, 1, {0x1C} },
	{0x04, 1, {0x1C} },
	{0x05, 1, {0x1C} },
	{0x06, 1, {0x1C} },
	{0x07, 1, {0x1C} },
	{0x08, 1, {0x1C} },
	{0x09, 1, {0x1C} },
	{0x0A, 1, {0x20} },
	{0x0B, 1, {0x20} },
	{0x0C, 1, {0x10} },
	{0x0D, 1, {0x12} },
	{0x0E, 1, {0x14} },
	{0x0F, 1, {0x1E} },
	{0x10, 1, {0x0D} },
	{0x11, 1, {0x0A} },
	{0x12, 1, {0x01} },
	{0x13, 1, {0x05} },
	{0x14, 1, {0x03} },
	{0x15, 1, {0x04} },
	{0x16, 1, {0x06} },
	{0x17, 1, {0x1C} },
	{0x18, 1, {0x1C} },
	{0x19, 1, {0x1C} },
	{0x1A, 1, {0x1C} },
	{0x1B, 1, {0x1C} },
	{0x1C, 1, {0x1C} },
	{0x1D, 1, {0x1C} },
	{0x1E, 1, {0x1C} },
	{0x1F, 1, {0x1C} },
	{0x20, 1, {0x1C} },
	{0x21, 1, {0x20} },
	{0x22, 1, {0x00} },
	{0x23, 1, {0x10} },
	{0x24, 1, {0x12} },
	{0x25, 1, {0x14} },
	{0x26, 1, {0x1E} },
	{0x27, 1, {0x0D} },
	{0x28, 1, {0x0A} },
	{0x29, 1, {0x01} },
	{0x2A, 1, {0x04} },
	{0x2B, 1, {0x06} },
	{0x2D, 1, {0x03} },
	{0x2F, 1, {0x05} },
	{0x31, 1, {0x04} },
	{0x32, 1, {0x08} },
	{0x33, 1, {0x04} },
	{0x34, 1, {0x08} },
	{0x35, 1, {0x00} },
	{0x37, 1, {0x02} },
	{0x38, 1, {0xA1} },
	{0x39, 1, {0xA1} },
	{0x3F, 1, {0xA1} },
	{0x41, 1, {0x04} },
	{0x42, 1, {0x08} },
	{0x4C, 1, {0x09} },
	{0x4D, 1, {0x09} },
	{0x60, 1, {0x51} },
	{0x61, 1, {0xDC} },
	{0x79, 1, {0x22} },
	{0x7A, 1, {0x0D} },
	{0x7B, 1, {0xAA} },
	{0x7C, 1, {0x80} },
	{0x7D, 1, {0x26} },
	{0x80, 1, {0x42} },
	{0x82, 1, {0x11} },
	{0x83, 1, {0x22} },
	{0x84, 1, {0x33} },
	{0x85, 1, {0x00} },
	{0x86, 1, {0x00} },
	{0x87, 1, {0x00} },
	{0x88, 1, {0x11} },
	{0x89, 1, {0x22} },
	{0x8A, 1, {0x33} },
	{0x8B, 1, {0x00} },
	{0x8C, 1, {0x00} },
	{0x8D, 1, {0x00} },
	{0x92, 1, {0xB2} },
	{0xB3, 1, {0x0A} },
	{0xB4, 1, {0x04} },
	{0xDC, 1, {0x29} },
	{0xDD, 1, {0x03} },
	{0xDE, 1, {0x03} },
	{0xDF, 1, {0x01} },
	{0xE0, 1, {0xA0} },
	{0xEB, 1, {0x10} },
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x21, 1, {0x29} },
	{0x22, 1, {0x29} },
	{0x24, 1, {0xB2} },
	{0x25, 1, {0xB2} },
	{0x28, 1, {0x00} },
	{0x29, 1, {0xB2} },
	{0x2A, 1, {0x00} },
	{0x2B, 1, {0xB2} },
	{0x69, 1, {0x10} },
	{0x71, 1, {0x6D} },
	{0x7E, 1, {0x2D} },
	{0x84, 1, {0x78} },
	{0x8D, 1, {0x00} },
	{0xC2, 1, {0x59} },
	{0xC3, 1, {0x13} },
	{0xD0, 1, {0x01} },
	{0xD4, 1, {0x00} },
	{0xD5, 1, {0x02} },
	{0xD6, 1, {0x20} },
	{0xD9, 1, {0x88} },
	{0xFF, 1, {0x26} },
	{0xFB, 1, {0x01} },
	{0x06, 1, {0xFF} },
	{0x12, 1, {0x4B} },
	{0x1A, 1, {0x71} },
	{0x1C, 1, {0xAF} },
	{0x1E, 1, {0xF4} },
	{0x98, 1, {0xF1} },
	{0xAE, 1, {0x48} },
	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0x13, 1, {0x00} },
	{0x1E, 1, {0x15} },
	{0xFF, 1, {0xF0} },
	{0xFB, 1, {0x01} },
	{0xA2, 1, {0x00} },
	{0xFF, 1, {0xE0} },
	{0xFB, 1, {0x01} },
	{0x15, 1, {0x00} },
	{0xFF, 1, {0x10} },

	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 40, {} },
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	// Display off sequence
	{0x28, 0, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	{0xFF, 1, {0xE0} },
	{0xFB, 1, {0x01} },
	{0x15, 1, {0x40} },
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0x98, 1, {0x80} },
	{0xFF, 1, {0x10} },

	// Sleep Mode On
	{0x10, 0, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
};

static void push_table(struct LCM_setting_table *table,
	   unsigned int count, unsigned char force_update)
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
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list,
							force_update);
		}
	}
}

// -------------------------------------------------------------------
//  LCM Driver Implementations
// -----------------------------------------------------------------
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type                = LCM_TYPE_DSI;
	params->width               = FRAME_WIDTH;
	params->height              = FRAME_HEIGHT;
	params->lcm_if              = LCM_INTERFACE_DSI0;
	params->lcm_cmd_if          = LCM_INTERFACE_DSI0;
	params->physical_width      = PHYSICAL_WIDTH / 1000;
	params->physical_height     = PHYSICAL_HEIGHT / 1000;
	params->physical_width_um	= PHYSICAL_WIDTH;
	params->physical_height_um  = PHYSICAL_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode            = CMD_MODE;
#else
	params->dsi.mode            = BURST_VDO_MODE;
#endif

	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding		= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability */
	/* video mode timing */
	params->dsi.PS                                  =
		LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.vertical_sync_active                = 2;
	params->dsi.vertical_backporch                  = 8;
	params->dsi.vertical_frontporch                 = 14;
	params->dsi.vertical_active_line                = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active              = 10;
	params->dsi.horizontal_backporch                = 12;
	params->dsi.horizontal_frontporch               = 50;
	params->dsi.horizontal_active_pixel             = FRAME_WIDTH;
	params->dsi.LANE_NUM                            = LCM_FOUR_LANE;
	params->dsi.PLL_CLOCK                           =
		233; //this value must be in MTK suggested table

	params->dsi.ssc_disable                         = 0;
	params->dsi.ssc_range                           = 4;

	params->dsi.HS_TRAIL                            = 15;
	params->dsi.noncont_clock                       = 1;
	params->dsi.noncont_clock_period                = 1;

	/* ESD check function */
	params->dsi.esd_check_enable                    = 0;
	params->dsi.customization_esd_check_enable      = 0;
	params->dsi.clk_lp_per_line_enable              = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x0A;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}

static void lcm_init_power(void)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
	MDELAY(5);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
#else
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP1);
	MDELAY(5);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN1);
#endif
	nt5038_i2c_write_byte(nt5038_cmd_data[0].cmd,
						  nt5038_cmd_data[0].data);
	MDELAY(1);
	nt5038_i2c_write_byte(nt5038_cmd_data[1].cmd,
						  nt5038_cmd_data[1].data);
	MDELAY(1);
	nt5038_i2c_write_byte(nt5038_cmd_data[2].cmd,
						  nt5038_cmd_data[2].data);
	MDELAY(15);
}

static void lcm_suspend_power(void)
{
#ifndef BUILD_LK
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN0);
	MDELAY(5);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP0);
#endif
}

static void lcm_resume_power(void)
{
#ifndef BUILD_LK
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP1);
	MDELAY(5);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN1);

	nt5038_i2c_write_byte(nt5038_cmd_data[0].cmd,
						  nt5038_cmd_data[0].data);
	MDELAY(1);
	nt5038_i2c_write_byte(nt5038_cmd_data[1].cmd,
						  nt5038_cmd_data[1].data);
	MDELAY(1);
	nt5038_i2c_write_byte(nt5038_cmd_data[2].cmd,
						  nt5038_cmd_data[2].data);
	MDELAY(15);
#endif
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
#else
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(10);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
	MDELAY(10);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
#endif
	MDELAY(15); //spec show need at least 10ms delay for lcm initial reload

	/* when phone initial , config output high, enable backlight drv chip */
	push_table(lcm_initialization_setting,
			   sizeof(lcm_initialization_setting)
			   / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(lcm_deep_sleep_mode_in_setting,
			   sizeof(lcm_deep_sleep_mode_in_setting)
			   / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void)
{
#ifndef BUILD_LK
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(10);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
	MDELAY(10);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(15);
	push_table(lcm_initialization_setting,
			   sizeof(lcm_initialization_setting)
			   / sizeof(struct LCM_setting_table), 1);
#endif
}

static unsigned int lcm_compare_id(void)
{
	return 1;
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
	/*skip ata check*/
	return 1;
}
struct LCM_DRIVER nt36672ah_hdp_dsi_vdo_tcl_csot_lcm_drv = {
	.name		= "nt36672ah_hdp_dsi_vdo_tcl_csot",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params	= lcm_get_params,
	.init		= lcm_init,
	.suspend	= lcm_suspend,
	.resume		= lcm_resume,
	.compare_id	= lcm_compare_id,
	.init_power	= lcm_init_power,
#ifndef BUILD_LK
	.resume_power	= lcm_resume_power,
	.suspend_power	= lcm_suspend_power,
	.ata_check	= lcm_ata_check,
#endif
};
