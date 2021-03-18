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
#include <linux/kernel.h>
#include <linux/string.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <platform/upmu_common.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...) dprintf(0, "[LK/" LOG_TAG "]" string, ##args)
#define LCM_LOGD(string, args...) dprintf(1, "[LK/" LOG_TAG "]" string, ##args)
#else
#define LCM_LOGI(fmt, args...) pr_debug("[KERNEL/" LOG_TAG "]" fmt, ##args)
#define LCM_LOGD(fmt, args...) pr_debug("[KERNEL/" LOG_TAG "]" fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))
#define MDELAY(n) (lcm_util.mdelay(n))
#define UDELAY(n) (lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)                \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)                       \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)                          \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)                                     \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)                                  \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#endif

#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2300)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH (68690)
#define LCM_PHYSICAL_HEIGHT (146280)
#define LCM_DENSITY (480)

#define REGFLAG_DELAY 0xFFFC
#define REGFLAG_UDELAY 0xFFFB
#define REGFLAG_END_OF_TABLE 0xFFFD
#define REGFLAG_RESET_LOW 0xFFFE
#define REGFLAG_RESET_HIGH 0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


/* i2c control start */

#define LCM_I2C_ADDR 0x3e
#define LCM_I2C_BUSNUM  6	/* for I2C channel 6 */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"

static struct i2c_client *_lcm_i2c_client;


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
	{ .compatible = "mediatek,I2C_LCD_BIAS", },
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
	pr_debug("[LCM][I2C] NT: info==>name=%s addr=0x%x\n",
		client->name, client->addr);
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
		pr_info("[LCM][ERROR] %s: addr:0x%x,reg:0x%x,val:0x%x\n",
			__func__, client->addr, addr, value);

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
	unsigned char para_list[200];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0x00, 1, {0x00} },
	{0xFF, 3, {0x87, 0x56, 0x01} },

	{0x00, 1, {0x80} },
	{0xFF, 2, {0x87, 0x56} },


	{0x00, 1, {0xA1} },
	{0xB3, 6, {0x04, 0x38, 0x08, 0xFC, 0x00, 0xFC} },

	{0x00, 1, {0x80} },
	{0xC0, 6, {0x00, 0x92, 0x00, 0x08, 0x00, 0x24} },

	{0x00, 1, {0x90} },
	{0xC0, 6, {0x00, 0x92, 0x00, 0x08, 0x00, 0x24} },

	{0x00, 1, {0xA0} },
	{0xC0, 6, {0x01, 0x24, 0x00, 0x08, 0x00, 0x24} },

	{0x00, 1, {0xB0} },
	{0xC0, 5, {0x00, 0x92, 0x00, 0x08, 0x24} },

	{0x00, 1, {0xC1} },
	{0xC0, 8, {0x00, 0xD9, 0x00, 0xA5, 0x00, 0x91, 0x00, 0xF8} },

	{0x00, 1, {0xD7} },
	{0xC0, 6, {0x00, 0x91, 0x00, 0x08, 0x00, 0x24} },

	{0x00, 1, {0xA3} },
	{0xC1, 6, {0x00, 0x25, 0x00, 0x25, 0x00, 0x02} },

	{0x00, 1, {0x80} },
	{0xCE, 16, {0x01, 0x81, 0x09, 0x13, 0x00, 0xC8, 0x00,
			0xE0, 0x00, 0x85, 0x00, 0x95, 0x00, 0x64,
			0x00, 0x70} },

	{0x00, 1, {0x90} },
	{0xCE, 15, {0x00, 0x8E, 0x0C, 0xDF, 0x00, 0x8E, 0x80,
			0x09, 0x13, 0x00, 0x04, 0x00, 0x22, 0x20, 0x20} },

	{0x00, 1, {0xA0} },
	{0xCE, 3, {0x00, 0x00, 0x00} },


	{0x00, 1, {0xB0} },
	{0xCE, 3, {0x22, 0x00, 0x00} },

	{0x00, 1, {0xD1} },
	{0xCE, 7, {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00} },

	{0x00, 1, {0xE1} },
	{0xCE, 11, {0x08, 0x02, 0x4D, 0x02, 0x4D, 0x02, 0x4D,
		0x00, 0x00, 0x00, 0x00} },

	{0x00, 1, {0xF1} },
	{0xCE, 9, {0x12, 0x09, 0x0C, 0x01, 0x1B, 0x01, 0x1C,
			0x01, 0x37} },

	{0x00, 1, {0xB0} },
	{0xCF, 4, {0x00, 0x00, 0xB0, 0xB4} },

	{0x00, 1, {0xB5} },
	{0xCF, 4, {0x04, 0x04, 0xB8, 0xBC} },

	{0x00, 1, {0xC0} },
	{0xCF, 4, {0x08, 0x08, 0xD2, 0xD6} },

	{0x00, 1, {0xC5} },
	{0xCF, 4, {0x00, 0x00, 0x08, 0x0C} },

	{0x00, 1, {0xE8} },
	{0xC0, 1, {0x40} },

	{0x00, 1, {0x80} },
	{0xc2, 4, {0x84, 0x01, 0x3A, 0x3A} },

	{0x00, 1, {0x90} },
	{0xC2, 4, {0x02, 0x01, 0x03, 0x03} },

	{0x00, 1, {0xA0} },
	{0xC2, 15, {0x84, 0x04, 0x00, 0x03, 0x8E, 0x83, 0x04,
			0x00, 0x03, 0x8E, 0x82, 0x04, 0x00, 0x03, 0x8E} },

	{0x00, 1, {0xB0} },
	{0xC2, 5, {0x81, 0x04, 0x00, 0x03, 0x8E} },

	{0x00, 1, {0xE0} },
	{0xC2, 14, {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x00,
			0x00, 0x12, 0x00, 0x05, 0x02, 0x03, 0x03} },

	{0x00, 1, {0xC0} },
	{0xC3, 4, {0x99, 0x99, 0x99, 0x99} },

	{0x00, 1, {0xD0} },
	{0xC3, 8, {0x45, 0x00, 0x00, 0x05, 0x45, 0x00, 0x00, 0x05} },

	{0x00, 1, {0x80} },
	{0xCB, 16, {0xC1, 0xC1, 0x00, 0xC1, 0xC1, 0x00, 0x00,
			0xC1, 0xFE, 0x00, 0xC1, 0x00, 0xFD, 0xC1,
			0x00, 0xC0} },

	{0x00, 1, {0x90} },
	{0xCB, 16, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00} },

	{0x00, 1, {0xA0} },
	{0xCB, 4, {0x00, 0x00, 0x00, 0x00} },

	{0x00, 1, {0xB0} },
	{0xCB, 4, {0x55, 0x55, 0x95, 0x55} },

	{0x00, 1, {0xC0} },
	{0xCB, 4, {0x10, 0x51, 0x84, 0x50} },

	{0x00, 1, {0x80} },
	{0xCC, 16, {0x00, 0x00, 0x00, 0x25, 0x25, 0x29, 0x16,
			0x17, 0x18, 0x19, 0x1A, 0x1B, 0x22, 0x24,
			0x06, 0x06} },

	{0x00, 1, {0x90} },
	{0xCC, 8, {0x08, 0x08, 0x24, 0x02, 0x12, 0x01, 0x29, 0x29} },

	{0x00, 1, {0x80} },
	{0xCD, 16, {0x00, 0x00, 0x00, 0x25, 0x25, 0x29, 0x16,
			0x17, 0x18, 0x19, 0x1A, 0x1B, 0x22, 0x24,
			0x07, 0x07} },

	{0x00, 1, {0x90} },
	{0xCD, 8, {0x09, 0x09, 0x24, 0x02, 0x12, 0x01, 0x29, 0x29} },

	{0x00, 1, {0xA0} },
	{0xCC, 16, {0x00, 0x00, 0x00, 0x25, 0x25, 0x29, 0x16,
			0x17, 0x18, 0x19, 0x1A, 0x1B, 0x24, 0x23,
			0x09, 0x09} },

	{0x00, 1, {0xB0} },
	{0xCC, 8, {0x07, 0x07, 0x24, 0x12, 0x02, 0x01, 0x29, 0x29} },

	{0x00, 1, {0xA0} },
	{0xCD, 16, {0x00, 0x00, 0x00, 0x25, 0x25, 0x29, 0x16,
			0x17, 0x18, 0x19, 0x1A, 0x1B, 0x24, 0x23,
			0x08, 0x08} },

	{0x00, 1, {0xB0} },
	{0xCD, 8, {0x06, 0x06, 0x24, 0x12, 0x02, 0x01, 0x29, 0x29} },

	{0x00, 1, {0x80} },
	{0xA7, 1, {0x10} },

	{0x00, 1, {0x82} },
	{0xA7, 2, {0x33, 0x02} },

	{0x00, 1, {0x85} },
	{0xA7, 1, {0x10} },
	{0x00, 1, {0xA0} },
	{0xC3, 16, {0x35, 0x02, 0x41, 0x35, 0x53, 0x14, 0x20,
			0x00, 0x00, 0x00, 0x13, 0x50, 0x24, 0x42,
			0x05, 0x31} },

	{0x00, 1, {0x85} },
	{0xC4, 1, {0x1C} },
	{0x00, 1, {0x97} },
	{0xC4, 1, {0x01} },

	{0x00, 1, {0xA0} },
	{0xC4, 3, {0x2D, 0xD2, 0x2D} },

	{0x00, 1, {0x93} },
	{0xC5, 1, {0x23} },

	{0x00, 1, {0x97} },
	{0xC5, 1, {0x23} },

	{0x00, 1, {0x9A} },
	{0xC5, 1, {0x23} },

	{0x00, 1, {0x9C} },
	{0xC5, 1, {0x23} },


	{0x00, 1, {0xB6} },
	{0xC5, 2, {0x1E, 0x1E} },


	{0x00, 1, {0xB8} },
	{0xC5, 2, {0x19, 0x19} },

	{0x00, 1, {0x9B} },
	{0xF5, 1, {0x4B} },

	{0x00, 1, {0x93} },
	{0xF5, 2, {0x00, 0x00} },

	{0x00, 1, {0x9D} },
	{0xF5, 1, {0x49} },

	{0x00, 1, {0x82} },
	{0xF5, 2, {0x00, 0x00} },

	{0x00, 1, {0x8C} },
	{0xC3, 3, {0x00, 0x00, 0x00} },

	{0x00, 1, {0x84} },
	{0xC5, 2, {0x28, 0x28} },

	{0x00, 1, {0xA4} },
	{0xD7, 1, {0x00} },

	{0x00, 1, {0x80} },
	{0xF5, 2, {0x59, 0x59} },

	{0x00, 1, {0x84} },
	{0xF5, 3, {0x59, 0x59, 0x59} },

	{0x00, 1, {0x96} },
	{0xF5, 1, {0x59} },

	{0x00, 1, {0xA6} },
	{0xF5, 1, {0x59} },

	{0x00, 1, {0xCA} },
	{0xC0, 1, {0x80} },

	{0x00, 1, {0xB1} },
	{0xF5, 1, {0x1F} },


	{0x00, 1, {0x00} },
	{0xD8, 2, {0x2F, 0x2F} },


	{0x00, 1, {0x00} },
	{0xD9, 2, {0x23, 0x23} },

	{0x00, 1, {0x86} },
	{0xC0, 6, {0x01, 0x01, 0x01, 0x01, 0x10, 0x05} },
	{0x00, 1, {0x96} },
	{0xC0, 6, {0x01, 0x01, 0x01, 0x01, 0x10, 0x05} },
	{0x00, 1, {0xA6} },
	{0xC0, 6, {0x01, 0x01, 0x01, 0x01, 0x1D, 0x05} },
	{0x00, 1, {0xE9} },
	{0xC0, 6, {0x01, 0x01, 0x01, 0x01, 0x10, 0x05} },
	{0x00, 1, {0xA3} },
	{0xCE, 6, {0x01, 0x01, 0x01, 0x01, 0x10, 0x05} },
	{0x00, 1, {0xB3} },
	{0xCE, 6, {0x01, 0x01, 0x01, 0x01, 0x10, 0x05} },


	{0x00, 1, {0x00} },
	{0xE1, 40, {0x06, 0x0A, 0x0A, 0x0F, 0x6C, 0x1A, 0x21,
			0x28, 0x32, 0x61, 0x3A, 0x41, 0x47, 0x4D, 0xAC,
			0x51, 0x5A, 0x62, 0x69, 0xA6, 0x70, 0x78, 0x7F,
			0x88, 0xCD, 0x92, 0x98, 0x9E, 0xA6, 0x48, 0xAE,
			0xB9, 0xC6, 0xCE, 0x97, 0xD9, 0xE7, 0xF0, 0xF5,
			0xAB} },

	{0x00, 1, {0x00} },
	{0xE2, 40, {0x0D, 0x0A, 0x0A, 0x0F, 0x6C, 0x1A, 0x21,
			0x28, 0x32, 0x61, 0x3A, 0x41, 0x47, 0x4D, 0xAC,
			0x51, 0x5A, 0x62, 0x69, 0xA6, 0x70, 0x78, 0x7F,
			0x88, 0xCD, 0x92, 0x98, 0x9E, 0xA6, 0x48, 0xAE,
			0xB9, 0xC6, 0xCE, 0x97, 0xD9, 0xE7, 0xF0, 0xF5,
			0xAB} },

	{0x00, 1, {0x00} },
	{0xE3, 40, {0x06, 0x0A, 0x0A, 0x0F, 0x6C, 0x1A, 0x21,
			0x28, 0x32, 0x61, 0x3A, 0x41, 0x47, 0x4D, 0xAC,
			0x51, 0x5A, 0x62, 0x69, 0xA6, 0x70, 0x78, 0x7F,
			0x88, 0xCD, 0x92, 0x98, 0x9E, 0xA6, 0x48, 0xAE,
			0xB9, 0xC6, 0xCE, 0x97, 0xD9, 0xE7, 0xF0, 0xF5,
			0xAB} },

	{0x00, 1, {0x00} },
	{0xE4, 40, {0x0D, 0x0A, 0x0A, 0x0F, 0x6C, 0x1A, 0x21,
			0x28, 0x32, 0x61, 0x3A, 0x41, 0x47, 0x4D, 0xAC,
			0x51, 0x5A, 0x62, 0x69, 0xA6, 0x70, 0x78, 0x7F,
			0x88, 0xCD, 0x92, 0x98, 0x9E, 0xA6, 0x48, 0xAE,
			0xB9, 0xC6, 0xCE, 0x97, 0xD9, 0xE7, 0xF0, 0xF5,
			0xAB} },

	{0x00, 1, {0x00} },
	{0xE5, 40, {0x06, 0x0A, 0x0A, 0x0F, 0x6C, 0x1A, 0x21,
			0x28, 0x32, 0x61, 0x3A, 0x41, 0x47, 0x4D, 0xAC,
			0x51, 0x5A, 0x62, 0x69, 0xA6, 0x70, 0x78, 0x7F,
			0x88, 0xCD, 0x92, 0x98, 0x9E, 0xA6, 0x48, 0xAE,
			0xB9, 0xC6, 0xCE, 0x97, 0xD9, 0xE7, 0xF0, 0xF5,
			0xAB} },

	{0x00, 1, {0x00} },
	{0xE6, 40, {0x0D, 0x0A, 0x0A, 0x0F, 0x6C, 0x1A, 0x21,
			0x28, 0x32, 0x61, 0x3A, 0x41, 0x47, 0x4D, 0xAC,
			0x51, 0x5A, 0x62, 0x69, 0xA6, 0x70, 0x78, 0x7F,
			0x88, 0xCD, 0x92, 0x98, 0x9E, 0xA6, 0x48, 0xAE,
			0xB9, 0xC6, 0xCE, 0x97, 0xD9, 0xE7, 0xF0, 0xF5,
			0xAB} },

	{0x00, 1, {0xCC} },
	{0xC0, 1, {0x10} },


	{0x00, 1, {0xB3} },
	{0xC5, 1, {0xD1} },


	{0x00, 1, {0x80} },
	{0xCA, 12, {0xCE, 0xBB, 0xAB, 0x9F, 0x96, 0x8E, 0x87,
			0x82, 0x80, 0x80, 0x80, 0x80} },
	{0x00, 1, {0x90} },
	{0xCA, 9, {0xFD, 0xFF, 0xEA, 0xFC, 0xFF, 0xCC, 0xFA,
			0xFF, 0x66} },


	{0x00, 1, {0x00} },
	{0xFF, 3, {0xFF, 0xFF, 0xFF} },


	{0x51, 1, {0xff, 0x0f} },
	{0x53, 1, {0x24} },
	{0x55, 1, {0x01} },

	{0x11, 0, {} },
	{REGFLAG_DELAY, 180, {} },
	{0x29, 0, {} },

	{REGFLAG_DELAY, 100, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table __maybe_unused
	lcm_deep_sleep_mode_in_setting[] = {
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

static struct LCM_setting_table bl_level[] = {{0x51, 1, {0xFF} },
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

	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 32;
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 6;
	params->dsi.horizontal_backporch = 43;
	params->dsi.horizontal_frontporch = 16;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 542;
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
}

/* turn on gate ic & control voltage to 5.8V */
static void lcm_init_power(void)
{
	if (lcm_util.set_gpio_lcd_enp_bias) {
		lcm_util.set_gpio_lcd_enp_bias(1);

		_lcm_i2c_write_bytes(0x0, 0x12);
		_lcm_i2c_write_bytes(0x1, 0x12);
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
	MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);

	push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	LCM_LOGI("td4320_fhdp----tps6132----lcm mode = vdo mode :%d----\n",
		 lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting, ARRAY_SIZE(lcm_suspend_setting),
		   1);
}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int id[3] = {0x40, 0x00, 0x00};
	unsigned int data_array[3];
	unsigned char read_buf[3];

	data_array[0] = 0x00033700; /* set max return size = 3 */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, read_buf, 3); /* read lcm id */

	LCM_LOGI("ATA read = 0x%x, 0x%x, 0x%x\n", read_buf[0], read_buf[1],
		 read_buf[2]);

	if ((read_buf[0] == id[0]) && (read_buf[1] == id[1]) &&
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
	LCM_LOGI("%s,ft8756 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
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

struct LCM_DRIVER ft8756_fhdp_dsi_vdo_auo_rt4801_lcm_drv = {
	.name = "ft8756_fhdp_dsi_vdo_auo_rt4801_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
};
