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
#include <linux/string.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/i2c.h>
#include <mach/upmu_sw.h>
#else
#include <string.h>
#endif

#include "lcm_drv.h"
#include "r63319_qxga_dsi_vdo_truly.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#else
#endif

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#define FRAME_WIDTH  (1536)
#define FRAME_HEIGHT (2048)

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static LCM_UTIL_FUNCS lcm_util = {
	   .set_reset_pin = NULL,
	   .udelay = NULL,
	   .mdelay = NULL,
};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

unsigned int GPIO_TPS65132_EN_P;
unsigned int GPIO_TPS65132_EN_N;
unsigned int GPIO_LCD_RST_EN;
unsigned int GPIO_BL_EN;
#define REGFLAG_DELAY                                       0xFC
#define REGFLAG_END_OF_TABLE                                0xFD

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)    lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                                   lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)               lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)                         lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define GPIO_OUT_ONE 1
#define GPIO_OUT_ZERO 0

#define TPS_I2C_BUSNUM 4
#define I2C_ID_NAME "tps65132"
#define TPS_ADDR 0x3E

/*****************************************************************************
* GLobal Variable
*****************************************************************************/
static struct i2c_client *tps65132_i2c_client;
/*****************************************************************************
* Function Prototype
*****************************************************************************/
static int tps65132_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);
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

static const struct of_device_id tps65132_of_match[] = {
	   {.compatible = "mediatek, tps65132"},
	   {},
};

static struct i2c_driver tps65132_i2c_driver = {
	   .id_table = tps65132_id,
	   .probe = tps65132_probe,
	   .remove = tps65132_remove,
	   .driver = {
	       .owner = THIS_MODULE,
	       .name = "tps65132",
	       .of_match_table = tps65132_of_match,
	       },

};

static int tps65132_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	   pr_warn("tps65132_i2c_probe\n");
	   pr_warn("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
	   tps65132_i2c_client = client;
	   return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
	   pr_warn("tps65132_remove\n");
	   tps65132_i2c_client = NULL;
	   i2c_unregister_device(client);
	   return 0;
}

static int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
	   int ret = 0;
	   struct i2c_client *client = tps65132_i2c_client;
	   char write_data[2] = { 0 };

	   write_data[0] = addr;
	   write_data[1] = value;
	   ret = i2c_master_send(client, write_data, 2);
	   if (ret < 0)
		pr_err("tps65132 write data fail !!\n");
	   return ret;
}

static int __init tps65132_i2c_init(void)
{
	   pr_warn("tps65132_i2c_init\n");
	   i2c_add_driver(&tps65132_i2c_driver);
	   pr_warn("tps65132_i2c_init success\n");
	   return 0;
}

static void __exit tps65132_i2c_exit(void)
{
	   pr_warn("tps65132_i2c_exit\n");
	   i2c_del_driver(&tps65132_i2c_driver);
}

module_init(tps65132_i2c_init);
module_exit(tps65132_i2c_exit);

MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK TPS65132 I2C Driver");
MODULE_LICENSE("GPL");

#define   LCM_DSI_CMD_MODE                                  0

/* #define  PUSH_TABLET_USING */
/* #define REGFLAG_DELAY                                       0xFFFC */
/* #define REGFLAG_END_OF_TABLE                                0xFFFD */

struct LCM_setting_table {
	   unsigned char cmd;
	   unsigned char count;
	   unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	   {0x36, 1, {0x40} },
	   {0x3A, 1, {0x70} },
	   {0xB0, 1, {0x04} },
	   {0xD6, 1, {0x01} },

	   /* PWM and CABC setting */
	   {0xB8, 6, {0x07, 0x90, 0x1E, 0x10, 0x1E, 0x32} },
	   {0xB9, 6, {0x07, 0x82, 0x3C, 0x10, 0x3C, 0x87} },
	   {0xBA, 6, {0x07, 0x78, 0x64, 0x10, 0x64, 0xB4} },
	   {0xCE, 23, {0x75, 0x40, 0x43, 0x49, 0x55, 0x62, 0x71, 0x82, 0x94, 0xA8,
		     0xB9, 0xCB, 0xDB, 0xE9, 0xF5, 0xFC, 0xFF, 0xbb, 0x00, 0x04,
		     0x04, 0x44, 0x20} },
	   {0x51, 1, {0xFF} },
	   {0x53, 1, {0x24} },
	   {0x55, 1, {0x00} },

/*
	   // BIST mode
	   {0xB0, 1, {0x04}},
	   {0xD6, 1, {0x01}},
	   {0xDE, 4, {0x01, 0x3F, 0xFF, 0x10}},
	   {0x51, 1, {0xFF}},
	   {0x53, 1, {0x24}},
	   {0x55, 1, {0x03}},
	   {0x11, 0, {}}, // sleep out
	   {REGFLAG_DELAY, 120, {}},
	   {0x29, 0, {}}, // display on
	   {REGFLAG_DELAY, 120, {}},
*/

	   {0x29, 0, {} }, /* display on */
	   {REGFLAG_DELAY, 20, {} },
	   {0x11, 0, {} }, /* sleep out */
	   {REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	   {0x28, 0, {} },
	   {0x10, 0, {} },
	   {REGFLAG_DELAY, 120, {} }
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	   unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;

		pr_warn("%s: count=%d\n", __func__, i);

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
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list,
			    force_update);
		}
	}
	pr_warn("%s-\n", __func__);
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	   memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	if (GPIO == 0xFFFFFFFF) {
#ifdef BUILD_LK
		printf("[LK/LCM] GPIO_TPS65132_EN_P =   0x%x\n", GPIO_TPS65132_EN_P);
		printf("[LK/LCM] GPIO_TPS65132_EN_N =   0x%x\n", GPIO_TPS65132_EN_N);
		printf("[LK/LCM] GPIO_LCD_RST_EN =  0x%x\n", GPIO_LCD_RST_EN);
		printf("[LK/LCM] GPIO_BL_EN =  0x%x\n", GPIO_BL_EN);
#elif (defined BUILD_UBOOT)
#else
#endif

		return;
	}
#ifdef BUILD_LK
	   mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	   mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	   mt_set_gpio_out(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#else
	   gpio_set_value(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#endif
}

static void lcm_get_params(LCM_PARAMS *params)
{
	   memset(params, 0, sizeof(LCM_PARAMS));

	   params->type = LCM_TYPE_DSI;

	   params->width = FRAME_WIDTH;
	   params->height = FRAME_HEIGHT;
	   params->lcm_if = LCM_INTERFACE_DSI_DUAL;
	   params->lcm_cmd_if = LCM_INTERFACE_DSI0;
	   params->dsi.cont_clock = 0;
	   params->dsi.clk_lp_per_line_enable = 1;

#if (LCM_DSI_CMD_MODE)
	   params->dsi.mode = CMD_MODE;
#else
	   params->dsi.mode = BURST_VDO_MODE;
#endif

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
	   params->dsi.ssc_disable = 1;
/* video mode timing */

	   params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	   params->dsi.vertical_sync_active = 2;
	   params->dsi.vertical_backporch = 6; /* 4; */
	   params->dsi.vertical_frontporch = 30;
	   params->dsi.vertical_active_line = FRAME_HEIGHT;

	   params->dsi.horizontal_sync_active = 8;
	   params->dsi.horizontal_backporch = 100;
	   params->dsi.horizontal_frontporch = 100;
	   params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	   params->dsi.PLL_CLOCK = 430; /* this value must be in MTK suggested table */
	   params->dsi.ufoe_enable = 1;
	   params->dsi.ufoe_params.lr_mode_en = 1;
}

static void lcm_init_power(void)
{
	   pr_warn("%s\n", __func__);
}

static void lcm_resume_power(void)
{
	   unsigned char cmd = 0x0;
	   unsigned char data = 0xFF;
	   int ret = 0;

	   cmd = 0x00;
	   data = 0x0e;

	   pr_warn("%s+\n", __func__);

	   /* turn on 1.8V */
	   upmu_set_rg_vgp4_vosel(3);
	   upmu_set_rg_vgp4_sw_en(1);

	   MDELAY(10);

	   /* turn on +-5.4V */
	   lcm_set_gpio_output(GPIO_TPS65132_EN_P, GPIO_OUT_ONE);
	   lcm_set_gpio_output(GPIO_TPS65132_EN_N, GPIO_OUT_ONE);

	   ret = tps65132_write_bytes(cmd, data);
	if (ret < 0)
		pr_warn
		("[KERNEL]r63319----tps6132---cmd=%0x-- i2c write error-----\n",
		cmd);
	else
		pr_warn
		("[KERNEL]r63319----tps6132---cmd=%0x-- i2c write success-----\n",
		cmd);

	   MDELAY(20);

	   cmd = 0x01;
	   data = 0x0e;

	   ret = tps65132_write_bytes(cmd, data);
	if (ret < 0)
		pr_warn
		("[KERNEL]r63319----tps6132---cmd=%0x-- i2c write error-----\n",
		cmd);
	else
		pr_warn
		("[KERNEL]r63319----tps6132---cmd=%0x-- i2c write success-----\n",
		cmd);

	   pr_warn("%s-\n", __func__);
}

static void lcm_suspend_power(void)
{
	   pr_warn("%s+\n", __func__);

	   /* turn off +- 5.4V */
	   lcm_set_gpio_output(GPIO_TPS65132_EN_N, GPIO_OUT_ZERO);
	   lcm_set_gpio_output(GPIO_TPS65132_EN_P, GPIO_OUT_ZERO);

	   MDELAY(10);

	   /* turn off 1.8V */
	   upmu_set_rg_vgp4_sw_en(0);

	   pr_warn("%s-\n", __func__);
}

static void lcm_init(void)
{
	   pr_warn("%s\n", __func__);
}


static void lcm_suspend(void)
{
	   pr_warn("%s+\n", __func__);

	   lcm_set_gpio_output(GPIO_BL_EN, GPIO_OUT_ZERO);
	   MDELAY(10);

	   push_table(lcm_suspend_setting,
	       sizeof(lcm_suspend_setting) /
	       sizeof(struct LCM_setting_table), 1);

	   lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	   MDELAY(10);

	   pr_warn("%s-\n", __func__);
}

static void lcm_resume(void)
{
	   pr_warn("%s+\n", __func__);

	   lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	   MDELAY(10);

	   push_table(lcm_initialization_setting,
	       sizeof(lcm_initialization_setting) /
	       sizeof(struct LCM_setting_table), 1);

	   lcm_set_gpio_output(GPIO_BL_EN, GPIO_OUT_ONE);

	   pr_warn("%s-\n", __func__);
}

static void lcm_set_backlight(unsigned int level)
{
	   pr_warn("%s: level=%d\n", __func__, level);
}

LCM_DRIVER r63319_qxga_dsi_vdo_truly_lcm_drv = {
	   .name = "r63319_qxga_dsi_vdo_truly",
	   .set_util_funcs = lcm_set_util_funcs,
	   .get_params = lcm_get_params,
	   .init = lcm_init,
	   .suspend = lcm_suspend,
	   .resume = lcm_resume,
	   .set_backlight = lcm_set_backlight,
	   /*.compare_id     = lcm_compare_id, */
	   .init_power = lcm_init_power,
	   .resume_power = lcm_resume_power,
	   .suspend_power = lcm_suspend_power,
};
