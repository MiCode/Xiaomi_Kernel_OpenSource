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
/*#include <mach/mt_pm_ldo.h>*/
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
#include <cust_i2c.h>
#endif
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_NT35695 (0xf5)

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

#define set_gpio_lcd_enp(cmd) \
		lcm_util.set_gpio_lcd_enp_bias(cmd)
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


#ifndef CONFIG_FPGA_EARLY_PORTING

#define TPS_I2C_BUSNUM  I2C_I2C_LCD_BIAS_CHANNEL	/* for I2C channel 0 */
#define I2C_ID_NAME "tps65132"
#define TPS_ADDR 0x3E

#if defined(CONFIG_MTK_LEGACY)
static struct i2c_board_info tps65132_board_info __initdata = {
					I2C_BOARD_INFO(I2C_ID_NAME, TPS_ADDR) };
#endif
#if !defined(CONFIG_MTK_LEGACY)
static const struct of_device_id lcm_of_match[] = {
		{.compatible = "mediatek,I2C_LCD_BIAS"},
		{},
};
#endif

/*static struct i2c_client *tps65132_i2c_client;*/
static struct i2c_client *tps65132_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int tps65132_probe(
		struct i2c_client *client, const struct i2c_device_id *id);
static int tps65132_remove(struct i2c_client *client);
/*****************************************************************************
 * Data Structure
 *****************************************************************************/

struct tps65132_dev {
	struct i2c_client *client;

};

static const struct i2c_device_id tps65132_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

/* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)) */
/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */
/* #endif */
static struct i2c_driver tps65132_iic_driver = {
	.id_table = tps65132_id,
	.probe = tps65132_probe,
	.remove = tps65132_remove,
	/* .detect               = mt6605_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "tps65132",
#if !defined(CONFIG_MTK_LEGACY)
			.of_match_table = lcm_of_match,
#endif
		   },
};

static int tps65132_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	LCM_LOGI("tps65132_iic_probe\n");
	LCM_LOGI("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
	tps65132_i2c_client = client;
	return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
	LCM_LOGI("%s\n", __func__);
	tps65132_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

/*static int tps65132_write_bytes(unsigned char addr, unsigned char value)*/
static int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = tps65132_i2c_client;
	char write_data[2] = { 0 };

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		LCM_LOGI("tps65132 write data fail !!\n");
	return ret;
}

#if defined(CONFIG_MTK_LEGACY)
static int __init tps65132_iic_init(void)
{
	i2c_register_board_info(TPS_I2C_BUSNUM, &tps65132_board_info, 1);
	return 0;
}

static void __exit tps65132_iic_exit(void)
{
	LCM_LOGI("%s\n", __func__);
}


module_init(tps65132_iic_init);
module_exit(tps65132_iic_exit);

MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK TPS65132 I2C Driver");
MODULE_LICENSE("GPL");
#endif


static int tps65132_iic_add_driver(void)
{
	static int inited;
	int ret;

	if (inited)
		return 0;

	inited = 1;

	LCM_LOGI("tps65132_iic_init\n");
	ret = i2c_add_driver(&tps65132_iic_driver);

	if (ret)
		LCM_LOGI("error: lcm tps65132_iic_init fail !!!\n");
	else
		LCM_LOGI("tps65132_iic_init success\n");
	return 0;
}

#endif
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE				1
#define FRAME_WIDTH					(720)
#define FRAME_HEIGHT					(1440)

#define VIRTUAL_WIDTH					(1080)
#define VIRTUAL_HEIGHT					(1920)

#ifndef CONFIG_FPGA_EARLY_PORTING
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN
#endif

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

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
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x4F, 1, {0x01} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting_cmd[] = {
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0xC5, 1, {0x31} },


	{0xFF, 1, {0x20} },
	{0x00, 1, {0x01} },
	{0x01, 1, {0x55} },
	{0x02, 1, {0x45} },
	{0x03, 1, {0x55} },
	{0x05, 1, {0x40} },/* VGH=2xAVDD, VGL=2xAVEE */
	{0x06, 1, {0x99} },
	{0x07, 1, {0x9E} },
	{0x08, 1, {0x0C} },
	{0x0B, 1, {0x87} },
	{0x0C, 1, {0x87} },
	{0x0E, 1, {0xAB} },
	{0x0F, 1, {0xA9} },
	{0x11, 1, {0x0D} },/* VCOM */
	{0x12, 1, {0x10} },/* VCOM */
	{0x13, 1, {0x03} },
	{0x14, 1, {0x4A} },
	{0x15, 1, {0x12} },
	{0x16, 1, {0x12} },
	{0x30, 1, {0x01} },
	{0x58, 1, {0x00} },
	{0x59, 1, {0x00} },
	{0x5A, 1, {0x02} },
	{0x5B, 1, {0x00} },
	{0x5C, 1, {0x00} },
	{0x5D, 1, {0x00} },
	{0x5E, 1, {0x02} },
	{0x5F, 1, {0x02} },
	{0x72, 1, {0x31} },
	{0xFB, 1, {0x01} },

	{0xFF, 1, {0x24} },
	{0x00, 1, {0x01} },
	{0x01, 1, {0x0B} },
	{0x02, 1, {0x0C} },
	{0x03, 1, {0x03} },
	{0x04, 1, {0x05} },
	{0x05, 1, {0x1C} },
	{0x06, 1, {0x10} },
	{0x07, 1, {0x00} },
	{0x08, 1, {0x1C} },
	{0x09, 1, {0x00} },
	{0x0A, 1, {0x00} },
	{0x0B, 1, {0x00} },
	{0x0C, 1, {0x00} },
	{0x0D, 1, {0x13} },
	{0x0E, 1, {0x15} },
	{0x0F, 1, {0x17} },
	{0x10, 1, {0x01} },
	{0x11, 1, {0x0B} },
	{0x12, 1, {0x0C} },
	{0x13, 1, {0x04} },
	{0x14, 1, {0x06} },
	{0x15, 1, {0x1C} },
	{0x16, 1, {0x10} },
	{0x17, 1, {0x00} },
	{0x18, 1, {0x1C} },
	{0x19, 1, {0x00} },
	{0x1A, 1, {0x00} },
	{0x1B, 1, {0x00} },
	{0x1C, 1, {0x00} },
	{0x1D, 1, {0x13} },
	{0x1E, 1, {0x15} },
	{0x1F, 1, {0x17} },
	{0x20, 1, {0x00} },/* STV */
	{0x21, 1, {0x03} },
	{0x22, 1, {0x01} },
	{0x23, 1, {0x4A} },
	{0x24, 1, {0x4A} },
	{0x25, 1, {0x6D} },
	{0x26, 1, {0x40} },
	{0x27, 1, {0x40} },
	{0x32, 1, {0x7B} },
	{0x33, 1, {0x00} },
	{0x34, 1, {0x01} },
	{0x35, 1, {0x8E} },
	{0x39, 1, {0x01} },
	{0x3A, 1, {0x8E} },

	{0xBD, 1, {0x20} },/* VEND */
	{0xB6, 1, {0x22} },
	{0xB7, 1, {0x24} },
	{0xB8, 1, {0x07} },
	{0xB9, 1, {0x07} },
	{0xC1, 1, {0x6D} },
	{0xC2, 1, {0x00} }, /* disable Vblank protection for
			     * low fps power saving (for vdo mode)
			     */
	{0xC4, 1, {0x24} },/* updated */

	{0xBE, 1, {0x07} },
	{0xBF, 1, {0x07} },
	{0x29, 1, {0xD8} },/* UD */
	{0x2A, 1, {0x2A} },

	{0x5B, 1, {0x43} },/* CTRL */
	{0x5C, 1, {0x00} },
	{0x5F, 1, {0x73} },
	{0x60, 1, {0x73} },
	{0x63, 1, {0x22} },
	{0x64, 1, {0x00} },
	{0x67, 1, {0x08} },
	{0x68, 1, {0x04} },

	{0x7A, 1, {0x80} },/* MUX */
	{0x7B, 1, {0x91} },
	{0x7C, 1, {0xD8} },
	{0x7D, 1, {0x60} },
	{0x74, 1, {0x09} },
	{0x7E, 1, {0x09} },
	{0x75, 1, {0x21} },
	{0x7F, 1, {0x21} },
	{0x76, 1, {0x05} },
	{0x81, 1, {0x05} },
	{0x77, 1, {0x04} },
	{0x82, 1, {0x04} },

	{0x93, 1, {0x06} },/* FP,BP */
	{0x94, 1, {0x06} },
	{0xB3, 1, {0x00} },
	{0xB4, 1, {0x00} },
	{0xB5, 1, {0x00} },

	{0x78, 1, {0x00} },/* SOURCE EQ */
	{0x79, 1, {0x00} },
	{0x80, 1, {0x00} },
	{0x83, 1, {0x00} },
	{0x84, 1, {0x04} },


	{0x8A, 1, {0x33} },/* /Inversion Type// pixel column driving */
	{0x8B, 1, {0xF0} },
	{0x9B, 1, {0x0F} },
	{0xC6, 1, {0x09} },
	{0xFB, 1, {0x01} },
	{0xEC, 1, {0x00} },


	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x75, 1, {0x00} },
	{0x76, 1, {0x49} },
	{0x77, 1, {0x00} },
	{0x78, 1, {0x78} },
	{0x79, 1, {0x00} },
	{0x7A, 1, {0xA4} },
	{0x7B, 1, {0x00} },
	{0x7C, 1, {0xC2} },
	{0x7D, 1, {0x00} },
	{0x7E, 1, {0xDA} },
	{0x7F, 1, {0x00} },
	{0x80, 1, {0xED} },
	{0x81, 1, {0x00} },
	{0x82, 1, {0xFE} },
	{0x83, 1, {0x01} },
	{0x84, 1, {0x0E} },
	{0x85, 1, {0x01} },
	{0x86, 1, {0x1B} },
	{0x87, 1, {0x01} },
	{0x88, 1, {0x48} },
	{0x89, 1, {0x01} },
	{0x8A, 1, {0x6C} },
	{0x8B, 1, {0x01} },
	{0x8C, 1, {0xA2} },
	{0x8D, 1, {0x01} },
	{0x8E, 1, {0xCD} },
	{0x8F, 1, {0x02} },
	{0x90, 1, {0x0F} },
	{0x91, 1, {0x02} },
	{0x92, 1, {0x42} },
	{0x93, 1, {0x02} },
	{0x94, 1, {0x43} },
	{0x95, 1, {0x02} },
	{0x96, 1, {0x71} },
	{0x97, 1, {0x02} },
	{0x98, 1, {0xA3} },
	{0x99, 1, {0x02} },
	{0x9A, 1, {0xC5} },
	{0x9B, 1, {0x02} },
	{0x9C, 1, {0xF3} },
	{0x9D, 1, {0x03} },
	{0x9E, 1, {0x12} },
	{0x9F, 1, {0x03} },
	{0xA0, 1, {0x3A} },
	{0xA2, 1, {0x03} },
	{0xA3, 1, {0x46} },
	{0xA4, 1, {0x03} },
	{0xA5, 1, {0x52} },
	{0xA6, 1, {0x03} },
	{0xA7, 1, {0x60} },
	{0xA9, 1, {0x03} },
	{0xAA, 1, {0x6E} },
	{0xAB, 1, {0x03} },
	{0xAC, 1, {0x7D} },
	{0xAD, 1, {0x03} },
	{0xAE, 1, {0x8B} },
	{0xAF, 1, {0x03} },
	{0xB0, 1, {0x91} },
	{0xB1, 1, {0x03} },
	{0xB2, 1, {0xCF} },
	{0xB3, 1, {0x00} },/* RN GAMMA SETTING */
	{0xB4, 1, {0x49} },
	{0xB5, 1, {0x00} },
	{0xB6, 1, {0x78} },
	{0xB7, 1, {0x00} },
	{0xB8, 1, {0xA4} },
	{0xB9, 1, {0x00} },
	{0xBA, 1, {0xC2} },
	{0xBB, 1, {0x00} },
	{0xBC, 1, {0xDA} },
	{0xBD, 1, {0x00} },
	{0xBE, 1, {0xED} },
	{0xBF, 1, {0x00} },
	{0xC0, 1, {0xFE} },
	{0xC1, 1, {0x01} },
	{0xC2, 1, {0x0E} },
	{0xC3, 1, {0x01} },
	{0xC4, 1, {0x1B} },
	{0xC5, 1, {0x01} },
	{0xC6, 1, {0x48} },
	{0xC7, 1, {0x01} },
	{0xC8, 1, {0x6C} },
	{0xC9, 1, {0x01} },
	{0xCA, 1, {0xA2} },
	{0xCB, 1, {0x01} },
	{0xCC, 1, {0xCD} },
	{0xCD, 1, {0x02} },
	{0xCE, 1, {0x0F} },
	{0xCF, 1, {0x02} },
	{0xD0, 1, {0x42} },
	{0xD1, 1, {0x02} },
	{0xD2, 1, {0x43} },
	{0xD3, 1, {0x02} },
	{0xD4, 1, {0x71} },
	{0xD5, 1, {0x02} },
	{0xD6, 1, {0xA3} },
	{0xD7, 1, {0x02} },
	{0xD8, 1, {0xC5} },
	{0xD9, 1, {0x02} },
	{0xDA, 1, {0xF3} },
	{0xDB, 1, {0x03} },
	{0xDC, 1, {0x12} },
	{0xDD, 1, {0x03} },
	{0xDE, 1, {0x3A} },
	{0xDF, 1, {0x03} },
	{0xE0, 1, {0x46} },
	{0xE1, 1, {0x03} },
	{0xE2, 1, {0x52} },
	{0xE3, 1, {0x03} },
	{0xE4, 1, {0x60} },
	{0xE5, 1, {0x03} },
	{0xE6, 1, {0x6E} },
	{0xE7, 1, {0x03} },
	{0xE8, 1, {0x7D} },
	{0xE9, 1, {0x03} },
	{0xEA, 1, {0x8B} },
	{0xEB, 1, {0x03} },
	{0xEC, 1, {0x91} },
	{0xED, 1, {0x03} },
	{0xEE, 1, {0xCF} },

	{0xEF, 1, {0x00} },/* GP GAMMA SETTING */
	{0xF0, 1, {0x49} },
	{0xF1, 1, {0x00} },
	{0xF2, 1, {0x78} },
	{0xF3, 1, {0x00} },
	{0xF4, 1, {0xA4} },
	{0xF5, 1, {0x00} },
	{0xF6, 1, {0xC2} },
	{0xF7, 1, {0x00} },
	{0xF8, 1, {0xDA} },
	{0xF9, 1, {0x00} },
	{0xFA, 1, {0xED} },

	{0xFF, 1, {0x21} },/* CMD2 PAGE1 */
	{0xFB, 1, {0x01} },

	{0x00, 1, {0x00} },
	{0x01, 1, {0xFE} },
	{0x02, 1, {0x01} },
	{0x03, 1, {0x0E} },
	{0x04, 1, {0x01} },
	{0x05, 1, {0x1B} },
	{0x06, 1, {0x01} },
	{0x07, 1, {0x48} },
	{0x08, 1, {0x01} },
	{0x09, 1, {0x6C} },
	{0x0A, 1, {0x01} },
	{0x0B, 1, {0xA2} },
	{0x0C, 1, {0x01} },
	{0x0D, 1, {0xCD} },
	{0x0E, 1, {0x02} },
	{0x0F, 1, {0x0F} },
	{0x10, 1, {0x02} },
	{0x11, 1, {0x42} },
	{0x12, 1, {0x02} },
	{0x13, 1, {0x43} },
	{0x14, 1, {0x02} },
	{0x15, 1, {0x71} },
	{0x16, 1, {0x02} },
	{0x17, 1, {0xA3} },
	{0x18, 1, {0x02} },
	{0x19, 1, {0xC5} },
	{0x1A, 1, {0x02} },
	{0x1B, 1, {0xF3} },
	{0x1C, 1, {0x03} },
	{0x1D, 1, {0x12} },
	{0x1E, 1, {0x03} },
	{0x1F, 1, {0x3A} },
	{0x20, 1, {0x03} },
	{0x21, 1, {0x46} },
	{0x22, 1, {0x03} },
	{0x23, 1, {0x52} },
	{0x24, 1, {0x03} },
	{0x25, 1, {0x60} },
	{0x26, 1, {0x03} },
	{0x27, 1, {0x6E} },
	{0x28, 1, {0x03} },
	{0x29, 1, {0x7D} },
	{0x2A, 1, {0x03} },
	{0x2B, 1, {0x8B} },
	{0x2D, 1, {0x03} },
	{0x2F, 1, {0x91} },
	{0x30, 1, {0x03} },
	{0x31, 1, {0xCF} },
	{0x32, 1, {0x00} },
	{0x33, 1, {0x49} },
	{0x34, 1, {0x00} },
	{0x35, 1, {0x78} },
	{0x36, 1, {0x00} },
	{0x37, 1, {0xA4} },
	{0x38, 1, {0x00} },
	{0x39, 1, {0xC2} },
	{0x3A, 1, {0x00} },
	{0x3B, 1, {0xDA} },
	{0x3D, 1, {0x00} },
	{0x3F, 1, {0xED} },
	{0x40, 1, {0x00} },
	{0x41, 1, {0xFE} },
	{0x42, 1, {0x01} },
	{0x43, 1, {0x0E} },
	{0x44, 1, {0x01} },
	{0x45, 1, {0x1B} },
	{0x46, 1, {0x01} },
	{0x47, 1, {0x48} },
	{0x48, 1, {0x01} },
	{0x49, 1, {0x6C} },
	{0x4A, 1, {0x01} },
	{0x4B, 1, {0xA2} },
	{0x4C, 1, {0x01} },
	{0x4D, 1, {0xCD} },
	{0x4E, 1, {0x02} },
	{0x4F, 1, {0x0F} },
	{0x50, 1, {0x02} },
	{0x51, 1, {0x42} },
	{0x52, 1, {0x02} },
	{0x53, 1, {0x43} },
	{0x54, 1, {0x02} },
	{0x55, 1, {0x71} },
	{0x56, 1, {0x02} },
	{0x58, 1, {0xA3} },
	{0x59, 1, {0x02} },
	{0x5A, 1, {0xC5} },
	{0x5B, 1, {0x02} },
	{0x5C, 1, {0xF3} },
	{0x5D, 1, {0x03} },
	{0x5E, 1, {0x12} },
	{0x5F, 1, {0x03} },
	{0x60, 1, {0x3A} },
	{0x61, 1, {0x03} },
	{0x62, 1, {0x46} },
	{0x63, 1, {0x03} },
	{0x64, 1, {0x52} },
	{0x65, 1, {0x03} },
	{0x66, 1, {0x60} },
	{0x67, 1, {0x03} },
	{0x68, 1, {0x6E} },
	{0x69, 1, {0x03} },
	{0x6A, 1, {0x7D} },
	{0x6B, 1, {0x03} },
	{0x6C, 1, {0x8B} },
	{0x6D, 1, {0x03} },
	{0x6E, 1, {0x91} },
	{0x6F, 1, {0x03} },
	{0x70, 1, {0xCF} },
	{0x71, 1, {0x00} },/* BP GAMMA SETTING */
	{0x72, 1, {0x49} },
	{0x73, 1, {0x00} },
	{0x74, 1, {0x78} },
	{0x75, 1, {0x00} },
	{0x76, 1, {0xA4} },
	{0x77, 1, {0x00} },
	{0x78, 1, {0xC2} },
	{0x79, 1, {0x00} },
	{0x7A, 1, {0xDA} },
	{0x7B, 1, {0x00} },
	{0x7C, 1, {0xED} },
	{0x7D, 1, {0x00} },
	{0x7E, 1, {0xFE} },
	{0x7F, 1, {0x01} },
	{0x80, 1, {0x0E} },
	{0x81, 1, {0x01} },
	{0x82, 1, {0x1B} },
	{0x83, 1, {0x01} },
	{0x84, 1, {0x48} },
	{0x85, 1, {0x01} },
	{0x86, 1, {0x6C} },
	{0x87, 1, {0x01} },
	{0x88, 1, {0xA2} },
	{0x89, 1, {0x01} },
	{0x8A, 1, {0xCD} },
	{0x8B, 1, {0x02} },
	{0x8C, 1, {0x0F} },
	{0x8D, 1, {0x02} },
	{0x8E, 1, {0x42} },
	{0x8F, 1, {0x02} },
	{0x90, 1, {0x43} },
	{0x91, 1, {0x02} },
	{0x92, 1, {0x71} },
	{0x93, 1, {0x02} },
	{0x94, 1, {0xA3} },
	{0x95, 1, {0x02} },
	{0x96, 1, {0xC5} },
	{0x97, 1, {0x02} },
	{0x98, 1, {0xF3} },
	{0x99, 1, {0x03} },
	{0x9A, 1, {0x12} },
	{0x9B, 1, {0x03} },
	{0x9C, 1, {0x3A} },
	{0x9D, 1, {0x03} },
	{0x9E, 1, {0x46} },
	{0x9F, 1, {0x03} },
	{0xA0, 1, {0x52} },
	{0xA2, 1, {0x03} },
	{0xA3, 1, {0x60} },
	{0xA4, 1, {0x03} },
	{0xA5, 1, {0x6E} },
	{0xA6, 1, {0x03} },
	{0xA7, 1, {0x7D} },
	{0xA9, 1, {0x03} },
	{0xAA, 1, {0x8B} },
	{0xAB, 1, {0x03} },
	{0xAC, 1, {0x91} },
	{0xAD, 1, {0x03} },
	{0xAE, 1, {0xCF} },
	{0xAF, 1, {0x00} },
	{0xB0, 1, {0x49} },
	{0xB1, 1, {0x00} },
	{0xB2, 1, {0x78} },
	{0xB3, 1, {0x00} },
	{0xB4, 1, {0xA4} },
	{0xB5, 1, {0x00} },
	{0xB6, 1, {0xC2} },
	{0xB7, 1, {0x00} },
	{0xB8, 1, {0xDA} },
	{0xB9, 1, {0x00} },
	{0xBA, 1, {0xED} },
	{0xBB, 1, {0x00} },
	{0xBC, 1, {0xFE} },
	{0xBD, 1, {0x01} },
	{0xBE, 1, {0x0E} },
	{0xBF, 1, {0x01} },
	{0xC0, 1, {0x1B} },
	{0xC1, 1, {0x01} },
	{0xC2, 1, {0x48} },
	{0xC3, 1, {0x01} },
	{0xC4, 1, {0x6C} },
	{0xC5, 1, {0x01} },
	{0xC6, 1, {0xA2} },
	{0xC7, 1, {0x01} },
	{0xC8, 1, {0xCD} },
	{0xC9, 1, {0x02} },
	{0xCA, 1, {0x0F} },
	{0xCB, 1, {0x02} },
	{0xCC, 1, {0x42} },
	{0xCD, 1, {0x02} },
	{0xCE, 1, {0x43} },
	{0xCF, 1, {0x02} },
	{0xD0, 1, {0x71} },
	{0xD1, 1, {0x02} },
	{0xD2, 1, {0xA3} },
	{0xD3, 1, {0x02} },
	{0xD4, 1, {0xC5} },
	{0xD5, 1, {0x02} },
	{0xD6, 1, {0xF3} },
	{0xD7, 1, {0x03} },
	{0xD8, 1, {0x12} },
	{0xD9, 1, {0x03} },
	{0xDA, 1, {0x3A} },
	{0xDB, 1, {0x03} },
	{0xDC, 1, {0x46} },
	{0xDD, 1, {0x03} },
	{0xDE, 1, {0x52} },
	{0xDF, 1, {0x03} },
	{0xE0, 1, {0x60} },
	{0xE1, 1, {0x03} },
	{0xE2, 1, {0x6E} },
	{0xE3, 1, {0x03} },
	{0xE4, 1, {0x7D} },
	{0xE5, 1, {0x03} },
	{0xE6, 1, {0x8B} },
	{0xE7, 1, {0x03} },
	{0xE8, 1, {0x91} },
	{0xE9, 1, {0x03} },
	{0xEA, 1, {0xCF} },

	{0xFF, 1, {0x10} }, /* Return  To CMD1 */
	{REGFLAG_UDELAY, 1, {} },
	{0x3B, 3, {0x03, 0x0a, 0x0a} },

	{0x35, 1, {0x00} },
	{0x44, 2, {0x07, 0x78} },
	/* set TE event @ line 0x778(1912) for partial update */

	/* don't reload cmd1 setting from MTP
	 * when exit sleep.(or C9 will be overwritten)
	 */
	{0xFB, 1, {0x01} },
	/* set partial update option */
	{0xC9, 11, {0x49, 0x02, 0x05,
	0x00, 0x0F, 0x06, 0x67, 0x03, 0x2E, 0x10, 0xF0} },

	{0xBB, 1, {0x10} },/* 0x03:video mode  0x10:command mode */

	/*{REGFLAG_DELAY, 200, {} },*/
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	/*{REGFLAG_DELAY, 200, {} },*/
	/* ///////////////////CABC SETTING///////// */
	{0x51, 1, {0x00} },
	{0x5E, 1, {0x00} },
	{0x53, 1, {0x24} },
	{0x55, 1, {0x00} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0xC5, 1, {0x31} },


	{0xFF, 1, {0x20} },
	{0x00, 1, {0x01} },
	{0x01, 1, {0x55} },
	{0x02, 1, {0x45} },
	{0x03, 1, {0x55} },
	{0x05, 1, {0x40} },/* VGH=2xAVDD, VGL=2xAVEE */
	{0x06, 1, {0x99} },
	{0x07, 1, {0x9E} },
	{0x08, 1, {0x0C} },
	{0x0B, 1, {0x87} },
	{0x0C, 1, {0x87} },
	{0x0E, 1, {0xAB} },
	{0x0F, 1, {0xA9} },
	{0x11, 1, {0x0D} },/* VCOM */
	{0x12, 1, {0x10} },/* VCOM */
	{0x13, 1, {0x03} },
	{0x14, 1, {0x4A} },
	{0x15, 1, {0x12} },
	{0x16, 1, {0x12} },
	{0x30, 1, {0x01} },
	{0x58, 1, {0x00} },
	{0x59, 1, {0x00} },
	{0x5A, 1, {0x02} },
	{0x5B, 1, {0x00} },
	{0x5C, 1, {0x00} },
	{0x5D, 1, {0x00} },
	{0x5E, 1, {0x02} },
	{0x5F, 1, {0x02} },
	{0x72, 1, {0x31} },
	{0xFB, 1, {0x01} },

	{0xFF, 1, {0x24} },
	{0x00, 1, {0x01} },
	{0x01, 1, {0x0B} },
	{0x02, 1, {0x0C} },
	{0x03, 1, {0x03} },
	{0x04, 1, {0x05} },
	{0x05, 1, {0x1C} },
	{0x06, 1, {0x10} },
	{0x07, 1, {0x00} },
	{0x08, 1, {0x1C} },
	{0x09, 1, {0x00} },
	{0x0A, 1, {0x00} },
	{0x0B, 1, {0x00} },
	{0x0C, 1, {0x00} },
	{0x0D, 1, {0x13} },
	{0x0E, 1, {0x15} },
	{0x0F, 1, {0x17} },
	{0x10, 1, {0x01} },
	{0x11, 1, {0x0B} },
	{0x12, 1, {0x0C} },
	{0x13, 1, {0x04} },
	{0x14, 1, {0x06} },
	{0x15, 1, {0x1C} },
	{0x16, 1, {0x10} },
	{0x17, 1, {0x00} },
	{0x18, 1, {0x1C} },
	{0x19, 1, {0x00} },
	{0x1A, 1, {0x00} },
	{0x1B, 1, {0x00} },
	{0x1C, 1, {0x00} },
	{0x1D, 1, {0x13} },
	{0x1E, 1, {0x15} },
	{0x1F, 1, {0x17} },
	{0x20, 1, {0x00} },/* STV */
	{0x21, 1, {0x03} },
	{0x22, 1, {0x01} },
	{0x23, 1, {0x4A} },
	{0x24, 1, {0x4A} },
	{0x25, 1, {0x6D} },
	{0x26, 1, {0x40} },
	{0x27, 1, {0x40} },
	{0x32, 1, {0x7B} },
	{0x33, 1, {0x00} },
	{0x34, 1, {0x01} },
	{0x35, 1, {0x8E} },
	{0x39, 1, {0x01} },
	{0x3A, 1, {0x8E} },

	{0xBD, 1, {0x20} },/* VEND */
	{0xB6, 1, {0x22} },
	{0xB7, 1, {0x24} },
	{0xB8, 1, {0x07} },
	{0xB9, 1, {0x07} },
	{0xC1, 1, {0x6D} },
	{0xC2, 1, {0x00} }, /* disable Vblank protection for
			     * low fps power saving (for vdo mode)
			     */
	{0xC4, 1, {0x24} },/* updated */

	{0xBE, 1, {0x07} },
	{0xBF, 1, {0x07} },
	{0x29, 1, {0xD8} },/* UD */
	{0x2A, 1, {0x2A} },

	{0x5B, 1, {0x43} },/* CTRL */
	{0x5C, 1, {0x00} },
	{0x5F, 1, {0x73} },
	{0x60, 1, {0x73} },
	{0x63, 1, {0x22} },
	{0x64, 1, {0x00} },
	{0x67, 1, {0x08} },
	{0x68, 1, {0x04} },

	{0x7A, 1, {0x80} },/* MUX */
	{0x7B, 1, {0x91} },
	{0x7C, 1, {0xD8} },
	{0x7D, 1, {0x60} },
	{0x74, 1, {0x09} },
	{0x7E, 1, {0x09} },
	{0x75, 1, {0x21} },
	{0x7F, 1, {0x21} },
	{0x76, 1, {0x05} },
	{0x81, 1, {0x05} },
	{0x77, 1, {0x04} },
	{0x82, 1, {0x04} },

	{0x93, 1, {0x06} },/* FP,BP */
	{0x94, 1, {0x06} },
	{0xB3, 1, {0x00} },
	{0xB4, 1, {0x00} },
	{0xB5, 1, {0x00} },

	{0x78, 1, {0x00} },/* SOURCE EQ */
	{0x79, 1, {0x00} },
	{0x80, 1, {0x00} },
	{0x83, 1, {0x00} },
	{0x84, 1, {0x04} },


	{0x8A, 1, {0x33} },/* /Inversion Type// pixel column driving */
	{0x8B, 1, {0xF0} },
	{0x9B, 1, {0x0F} },
	{0xC6, 1, {0x09} },
	{0xFB, 1, {0x01} },
	{0xEC, 1, {0x00} },


	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x75, 1, {0x00} },
	{0x76, 1, {0x49} },
	{0x77, 1, {0x00} },
	{0x78, 1, {0x78} },
	{0x79, 1, {0x00} },
	{0x7A, 1, {0xA4} },
	{0x7B, 1, {0x00} },
	{0x7C, 1, {0xC2} },
	{0x7D, 1, {0x00} },
	{0x7E, 1, {0xDA} },
	{0x7F, 1, {0x00} },
	{0x80, 1, {0xED} },
	{0x81, 1, {0x00} },
	{0x82, 1, {0xFE} },
	{0x83, 1, {0x01} },
	{0x84, 1, {0x0E} },
	{0x85, 1, {0x01} },
	{0x86, 1, {0x1B} },
	{0x87, 1, {0x01} },
	{0x88, 1, {0x48} },
	{0x89, 1, {0x01} },
	{0x8A, 1, {0x6C} },
	{0x8B, 1, {0x01} },
	{0x8C, 1, {0xA2} },
	{0x8D, 1, {0x01} },
	{0x8E, 1, {0xCD} },
	{0x8F, 1, {0x02} },
	{0x90, 1, {0x0F} },
	{0x91, 1, {0x02} },
	{0x92, 1, {0x42} },
	{0x93, 1, {0x02} },
	{0x94, 1, {0x43} },
	{0x95, 1, {0x02} },
	{0x96, 1, {0x71} },
	{0x97, 1, {0x02} },
	{0x98, 1, {0xA3} },
	{0x99, 1, {0x02} },
	{0x9A, 1, {0xC5} },
	{0x9B, 1, {0x02} },
	{0x9C, 1, {0xF3} },
	{0x9D, 1, {0x03} },
	{0x9E, 1, {0x12} },
	{0x9F, 1, {0x03} },
	{0xA0, 1, {0x3A} },
	{0xA2, 1, {0x03} },
	{0xA3, 1, {0x46} },
	{0xA4, 1, {0x03} },
	{0xA5, 1, {0x52} },
	{0xA6, 1, {0x03} },
	{0xA7, 1, {0x60} },
	{0xA9, 1, {0x03} },
	{0xAA, 1, {0x6E} },
	{0xAB, 1, {0x03} },
	{0xAC, 1, {0x7D} },
	{0xAD, 1, {0x03} },
	{0xAE, 1, {0x8B} },
	{0xAF, 1, {0x03} },
	{0xB0, 1, {0x91} },
	{0xB1, 1, {0x03} },
	{0xB2, 1, {0xCF} },
	{0xB3, 1, {0x00} },/* RN GAMMA SETTING */
	{0xB4, 1, {0x49} },
	{0xB5, 1, {0x00} },
	{0xB6, 1, {0x78} },
	{0xB7, 1, {0x00} },
	{0xB8, 1, {0xA4} },
	{0xB9, 1, {0x00} },
	{0xBA, 1, {0xC2} },
	{0xBB, 1, {0x00} },
	{0xBC, 1, {0xDA} },
	{0xBD, 1, {0x00} },
	{0xBE, 1, {0xED} },
	{0xBF, 1, {0x00} },
	{0xC0, 1, {0xFE} },
	{0xC1, 1, {0x01} },
	{0xC2, 1, {0x0E} },
	{0xC3, 1, {0x01} },
	{0xC4, 1, {0x1B} },
	{0xC5, 1, {0x01} },
	{0xC6, 1, {0x48} },
	{0xC7, 1, {0x01} },
	{0xC8, 1, {0x6C} },
	{0xC9, 1, {0x01} },
	{0xCA, 1, {0xA2} },
	{0xCB, 1, {0x01} },
	{0xCC, 1, {0xCD} },
	{0xCD, 1, {0x02} },
	{0xCE, 1, {0x0F} },
	{0xCF, 1, {0x02} },
	{0xD0, 1, {0x42} },
	{0xD1, 1, {0x02} },
	{0xD2, 1, {0x43} },
	{0xD3, 1, {0x02} },
	{0xD4, 1, {0x71} },
	{0xD5, 1, {0x02} },
	{0xD6, 1, {0xA3} },
	{0xD7, 1, {0x02} },
	{0xD8, 1, {0xC5} },
	{0xD9, 1, {0x02} },
	{0xDA, 1, {0xF3} },
	{0xDB, 1, {0x03} },
	{0xDC, 1, {0x12} },
	{0xDD, 1, {0x03} },
	{0xDE, 1, {0x3A} },
	{0xDF, 1, {0x03} },
	{0xE0, 1, {0x46} },
	{0xE1, 1, {0x03} },
	{0xE2, 1, {0x52} },
	{0xE3, 1, {0x03} },
	{0xE4, 1, {0x60} },
	{0xE5, 1, {0x03} },
	{0xE6, 1, {0x6E} },
	{0xE7, 1, {0x03} },
	{0xE8, 1, {0x7D} },
	{0xE9, 1, {0x03} },
	{0xEA, 1, {0x8B} },
	{0xEB, 1, {0x03} },
	{0xEC, 1, {0x91} },
	{0xED, 1, {0x03} },
	{0xEE, 1, {0xCF} },

	{0xEF, 1, {0x00} },/* GP GAMMA SETTING */
	{0xF0, 1, {0x49} },
	{0xF1, 1, {0x00} },
	{0xF2, 1, {0x78} },
	{0xF3, 1, {0x00} },
	{0xF4, 1, {0xA4} },
	{0xF5, 1, {0x00} },
	{0xF6, 1, {0xC2} },
	{0xF7, 1, {0x00} },
	{0xF8, 1, {0xDA} },
	{0xF9, 1, {0x00} },
	{0xFA, 1, {0xED} },

	{0xFF, 1, {0x21} },/* CMD2 PAGE1 */
	{0xFB, 1, {0x01} },

	{0x00, 1, {0x00} },
	{0x01, 1, {0xFE} },
	{0x02, 1, {0x01} },
	{0x03, 1, {0x0E} },
	{0x04, 1, {0x01} },
	{0x05, 1, {0x1B} },
	{0x06, 1, {0x01} },
	{0x07, 1, {0x48} },
	{0x08, 1, {0x01} },
	{0x09, 1, {0x6C} },
	{0x0A, 1, {0x01} },
	{0x0B, 1, {0xA2} },
	{0x0C, 1, {0x01} },
	{0x0D, 1, {0xCD} },
	{0x0E, 1, {0x02} },
	{0x0F, 1, {0x0F} },
	{0x10, 1, {0x02} },
	{0x11, 1, {0x42} },
	{0x12, 1, {0x02} },
	{0x13, 1, {0x43} },
	{0x14, 1, {0x02} },
	{0x15, 1, {0x71} },
	{0x16, 1, {0x02} },
	{0x17, 1, {0xA3} },
	{0x18, 1, {0x02} },
	{0x19, 1, {0xC5} },
	{0x1A, 1, {0x02} },
	{0x1B, 1, {0xF3} },
	{0x1C, 1, {0x03} },
	{0x1D, 1, {0x12} },
	{0x1E, 1, {0x03} },
	{0x1F, 1, {0x3A} },
	{0x20, 1, {0x03} },
	{0x21, 1, {0x46} },
	{0x22, 1, {0x03} },
	{0x23, 1, {0x52} },
	{0x24, 1, {0x03} },
	{0x25, 1, {0x60} },
	{0x26, 1, {0x03} },
	{0x27, 1, {0x6E} },
	{0x28, 1, {0x03} },
	{0x29, 1, {0x7D} },
	{0x2A, 1, {0x03} },
	{0x2B, 1, {0x8B} },
	{0x2D, 1, {0x03} },
	{0x2F, 1, {0x91} },
	{0x30, 1, {0x03} },
	{0x31, 1, {0xCF} },
	{0x32, 1, {0x00} },
	{0x33, 1, {0x49} },
	{0x34, 1, {0x00} },
	{0x35, 1, {0x78} },
	{0x36, 1, {0x00} },
	{0x37, 1, {0xA4} },
	{0x38, 1, {0x00} },
	{0x39, 1, {0xC2} },
	{0x3A, 1, {0x00} },
	{0x3B, 1, {0xDA} },
	{0x3D, 1, {0x00} },
	{0x3F, 1, {0xED} },
	{0x40, 1, {0x00} },
	{0x41, 1, {0xFE} },
	{0x42, 1, {0x01} },
	{0x43, 1, {0x0E} },
	{0x44, 1, {0x01} },
	{0x45, 1, {0x1B} },
	{0x46, 1, {0x01} },
	{0x47, 1, {0x48} },
	{0x48, 1, {0x01} },
	{0x49, 1, {0x6C} },
	{0x4A, 1, {0x01} },
	{0x4B, 1, {0xA2} },
	{0x4C, 1, {0x01} },
	{0x4D, 1, {0xCD} },
	{0x4E, 1, {0x02} },
	{0x4F, 1, {0x0F} },
	{0x50, 1, {0x02} },
	{0x51, 1, {0x42} },
	{0x52, 1, {0x02} },
	{0x53, 1, {0x43} },
	{0x54, 1, {0x02} },
	{0x55, 1, {0x71} },
	{0x56, 1, {0x02} },
	{0x58, 1, {0xA3} },
	{0x59, 1, {0x02} },
	{0x5A, 1, {0xC5} },
	{0x5B, 1, {0x02} },
	{0x5C, 1, {0xF3} },
	{0x5D, 1, {0x03} },
	{0x5E, 1, {0x12} },
	{0x5F, 1, {0x03} },
	{0x60, 1, {0x3A} },
	{0x61, 1, {0x03} },
	{0x62, 1, {0x46} },
	{0x63, 1, {0x03} },
	{0x64, 1, {0x52} },
	{0x65, 1, {0x03} },
	{0x66, 1, {0x60} },
	{0x67, 1, {0x03} },
	{0x68, 1, {0x6E} },
	{0x69, 1, {0x03} },
	{0x6A, 1, {0x7D} },
	{0x6B, 1, {0x03} },
	{0x6C, 1, {0x8B} },
	{0x6D, 1, {0x03} },
	{0x6E, 1, {0x91} },
	{0x6F, 1, {0x03} },
	{0x70, 1, {0xCF} },
	{0x71, 1, {0x00} },/* BP GAMMA SETTING */
	{0x72, 1, {0x49} },
	{0x73, 1, {0x00} },
	{0x74, 1, {0x78} },
	{0x75, 1, {0x00} },
	{0x76, 1, {0xA4} },
	{0x77, 1, {0x00} },
	{0x78, 1, {0xC2} },
	{0x79, 1, {0x00} },
	{0x7A, 1, {0xDA} },
	{0x7B, 1, {0x00} },
	{0x7C, 1, {0xED} },
	{0x7D, 1, {0x00} },
	{0x7E, 1, {0xFE} },
	{0x7F, 1, {0x01} },
	{0x80, 1, {0x0E} },
	{0x81, 1, {0x01} },
	{0x82, 1, {0x1B} },
	{0x83, 1, {0x01} },
	{0x84, 1, {0x48} },
	{0x85, 1, {0x01} },
	{0x86, 1, {0x6C} },
	{0x87, 1, {0x01} },
	{0x88, 1, {0xA2} },
	{0x89, 1, {0x01} },
	{0x8A, 1, {0xCD} },
	{0x8B, 1, {0x02} },
	{0x8C, 1, {0x0F} },
	{0x8D, 1, {0x02} },
	{0x8E, 1, {0x42} },
	{0x8F, 1, {0x02} },
	{0x90, 1, {0x43} },
	{0x91, 1, {0x02} },
	{0x92, 1, {0x71} },
	{0x93, 1, {0x02} },
	{0x94, 1, {0xA3} },
	{0x95, 1, {0x02} },
	{0x96, 1, {0xC5} },
	{0x97, 1, {0x02} },
	{0x98, 1, {0xF3} },
	{0x99, 1, {0x03} },
	{0x9A, 1, {0x12} },
	{0x9B, 1, {0x03} },
	{0x9C, 1, {0x3A} },
	{0x9D, 1, {0x03} },
	{0x9E, 1, {0x46} },
	{0x9F, 1, {0x03} },
	{0xA0, 1, {0x52} },
	{0xA2, 1, {0x03} },
	{0xA3, 1, {0x60} },
	{0xA4, 1, {0x03} },
	{0xA5, 1, {0x6E} },
	{0xA6, 1, {0x03} },
	{0xA7, 1, {0x7D} },
	{0xA9, 1, {0x03} },
	{0xAA, 1, {0x8B} },
	{0xAB, 1, {0x03} },
	{0xAC, 1, {0x91} },
	{0xAD, 1, {0x03} },
	{0xAE, 1, {0xCF} },
	{0xAF, 1, {0x00} },
	{0xB0, 1, {0x49} },
	{0xB1, 1, {0x00} },
	{0xB2, 1, {0x78} },
	{0xB3, 1, {0x00} },
	{0xB4, 1, {0xA4} },
	{0xB5, 1, {0x00} },
	{0xB6, 1, {0xC2} },
	{0xB7, 1, {0x00} },
	{0xB8, 1, {0xDA} },
	{0xB9, 1, {0x00} },
	{0xBA, 1, {0xED} },
	{0xBB, 1, {0x00} },
	{0xBC, 1, {0xFE} },
	{0xBD, 1, {0x01} },
	{0xBE, 1, {0x0E} },
	{0xBF, 1, {0x01} },
	{0xC0, 1, {0x1B} },
	{0xC1, 1, {0x01} },
	{0xC2, 1, {0x48} },
	{0xC3, 1, {0x01} },
	{0xC4, 1, {0x6C} },
	{0xC5, 1, {0x01} },
	{0xC6, 1, {0xA2} },
	{0xC7, 1, {0x01} },
	{0xC8, 1, {0xCD} },
	{0xC9, 1, {0x02} },
	{0xCA, 1, {0x0F} },
	{0xCB, 1, {0x02} },
	{0xCC, 1, {0x42} },
	{0xCD, 1, {0x02} },
	{0xCE, 1, {0x43} },
	{0xCF, 1, {0x02} },
	{0xD0, 1, {0x71} },
	{0xD1, 1, {0x02} },
	{0xD2, 1, {0xA3} },
	{0xD3, 1, {0x02} },
	{0xD4, 1, {0xC5} },
	{0xD5, 1, {0x02} },
	{0xD6, 1, {0xF3} },
	{0xD7, 1, {0x03} },
	{0xD8, 1, {0x12} },
	{0xD9, 1, {0x03} },
	{0xDA, 1, {0x3A} },
	{0xDB, 1, {0x03} },
	{0xDC, 1, {0x46} },
	{0xDD, 1, {0x03} },
	{0xDE, 1, {0x52} },
	{0xDF, 1, {0x03} },
	{0xE0, 1, {0x60} },
	{0xE1, 1, {0x03} },
	{0xE2, 1, {0x6E} },
	{0xE3, 1, {0x03} },
	{0xE4, 1, {0x7D} },
	{0xE5, 1, {0x03} },
	{0xE6, 1, {0x8B} },
	{0xE7, 1, {0x03} },
	{0xE8, 1, {0x91} },
	{0xE9, 1, {0x03} },
	{0xEA, 1, {0xCF} },

	{0xFF, 1, {0x10} }, /* Return  To CMD1 */
	{REGFLAG_UDELAY, 1, {} },
	{0x3B, 3, {0x03, 0x0a, 0x0a} },

	{0x35, 1, {0x00} },
	{0x44, 2, {0x07, 0x78} },
	/* set TE event @ line 0x778(1912) for partial update */

	/* don't reload cmd1 setting from MTP when
	 * exit sleep.(or C9 will be overwritten)
	 */
	{0xFB, 1, {0x01} },
	/* set partial update option */
	{0xC9, 11, {0x49, 0x02, 0x05, 0x00,
	0x0F, 0x06, 0x67, 0x03, 0x2E, 0x10, 0xF0} },

	{0xBB, 1, {0x03} },/* 0x03:video mode  0x10:command mode */

	/*{REGFLAG_DELAY, 200, {} },*/
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	/*{REGFLAG_DELAY, 200, {} },*/
	/* ///////////////////CABC SETTING///////// */
	{0x51, 1, {0x00} },
	{0x5E, 1, {0x00} },
	{0x53, 1, {0x24} },
	{0x55, 1, {0x00} },
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH & 0xFF)} },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT & 0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	/* Sleep Mode On */
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
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
			dsi_set_cmdq_V22(cmdq,
					cmd, table[i].count,
					table[i].para_list, force_update);
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

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
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
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_frontporch_for_low_power = 620;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable  = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 420;
	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 440;
	/* this value must be in MTK suggested table */
#endif
	params->dsi.PLL_CK_CMD = 420;
	params->dsi.PLL_CK_VDO = 440;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x53;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->corner_pattern_width = 32;
	params->corner_pattern_height = 32;
#endif

#ifdef CONFIG_NT35695_LANESWAP
	params->dsi.lane_swap_en = 1;

	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_0] =
							MIPITX_PHY_LANE_CK;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_1] =
							MIPITX_PHY_LANE_2;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_2] =
							MIPITX_PHY_LANE_3;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_3] =
							MIPITX_PHY_LANE_0;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_CK] =
							MIPITX_PHY_LANE_1;
	params->dsi.lane_swap[MIPITX_PHY_PORT_0][MIPITX_PHY_LANE_RX] =
							MIPITX_PHY_LANE_1;
#endif
}

#ifdef BUILD_LK
#ifndef CONFIG_FPGA_EARLY_PORTING
#define TPS65132_SLAVE_ADDR_WRITE  0x7C
static struct mt_i2c_t TPS65132_i2c;

static int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value)
{
	kal_uint32 ret_code = I2C_OK;
	kal_uint8 write_data[2];
	kal_uint16 len;

	write_data[0] = addr;
	write_data[1] = value;

	TPS65132_i2c.id = I2C_I2C_LCD_BIAS_CHANNEL;	/* I2C2; */
	/* Since i2c will left shift 1 bit,
	 * we need to set FAN5405 I2C address to >>1
	 */
	TPS65132_i2c.addr = (TPS65132_SLAVE_ADDR_WRITE >> 1);
	TPS65132_i2c.mode = ST_MODE;
	TPS65132_i2c.speed = 100;
	len = 2;

	ret_code = i2c_write(&TPS65132_i2c, write_data, len);
	/* printf("%s: i2c_write: ret_code: %d\n", __func__, ret_code); */

	return ret_code;
}

#else

/* extern int mt8193_i2c_write(u16 addr, u32 data); */
/* extern int mt8193_i2c_read(u16 addr, u32 *data); */

/* #define TPS65132_write_byte(add, data)  mt8193_i2c_write(add, data) */
/* #define TPS65132_read_byte(add)  mt8193_i2c_read(add) */

#endif
#endif


static void lcm_init_power(void)
{

}

static void lcm_suspend_power(void)
{

}

static void lcm_resume_power(void)
{

}


static void lcm_init(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
#ifndef CONFIG_FPGA_EARLY_PORTING
	int ret = 0;
#endif

	cmd = 0x00;
	data = 0x0E;

	tps65132_iic_add_driver();

	SET_RESET_PIN(0);

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ONE);
#else
	set_gpio_lcd_enp(1);
#endif
	MDELAY(5);
#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35695-tps6132-cmd=%0x--i2c write error-\n", cmd);
	else
		LCM_LOGI("nt35695-tps6132-cmd=%0x--i2c write success-\n", cmd);

	cmd = 0x01;
	data = 0x0E;

#ifdef BUILD_LK
	ret = TPS65132_write_byte(cmd, data);
#else
	ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35695-tps6132-cmd=%0x--i2c write error-\n", cmd);
	else
		LCM_LOGI("nt35695-tps6132-cmd=%0x--i2c write success-\n", cmd);

#endif
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
	if (lcm_dsi_mode == CMD_MODE) {
		push_table(NULL, init_setting_cmd,
		sizeof(init_setting_cmd) / sizeof(struct LCM_setting_table), 1);
		LCM_LOGI(
		"nt35695-tps6132-lcm mode = cmd mode :%d-\n", lcm_dsi_mode);
	} else {
		push_table(NULL, init_setting_vdo,
		sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
		LCM_LOGI(
		"nt35695-tps6132-lcm mode = vdo mode :%d-\n", lcm_dsi_mode);
	}
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
	sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_65132_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_EN, GPIO_OUT_ZERO);
#else
	set_gpio_lcd_enp(0);
#endif
#endif
	/* SET_RESET_PIN(0); */
}

static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y,
			unsigned int width, unsigned int height)
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
	id = buffer[0];     /* we only need ID */

	read_reg_v2(0xDB, buffer, 1);
	version_id = buffer[0];

	LCM_LOGI("%s,nt35695_id=0x%08x,version_id=0x%x\n",
	__func__, id, version_id);

	if (id == LCM_ID_NT35695 && version_id == 0x81)
		return 1;
	else
		return 0;

}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

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

	data_array[0] = 0x00043700; /* read id return two byte,version and id */
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

	LCM_LOGI("%s,nt35695 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
	sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value;
 * 2. config mode control register
 */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB; /* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;
		/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;
		/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;
		/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}

#if (0)

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty.
	 *In this case we need to use minimu roi
	 */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		pr_info("%s calc error,assign full roi:y=%d,h=%d\n",
		__func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*	*x, *y, *width, *height, x1, y1, w, h);*/

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif

#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER nt35695B_fhd_dsi_cmd_auo_nt50358_hdp_lcm_drv = {
	.name = "nt35695B_fhd_dsi_cmd_auo_nt50358_hdp_drv",
#else

struct LCM_DRIVER nt35695B_fhd_dsi_vdo_auo_nt50358_hdp_lcm_drv = {
	.name = "nt35695B_fhd_dsi_vdo_auo_nt50358_hdp_drv",
#endif
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
#if (0)
	.validate_roi = lcm_validate_roi,
#endif

};
