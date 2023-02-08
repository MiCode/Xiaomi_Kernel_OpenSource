// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#define LOG_TAG "LCM"
#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"
#include <linux/of_gpio.h>
#include <linux/gpio.h>
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
#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT			(1650)
/* physical size in um */
/*C3T code for HQ-229310 by zhangkexin at 2022/09/29 start*/
#define LCM_PHYSICAL_WIDTH		(68150)
#define LCM_PHYSICAL_HEIGHT		(156173)
/*C3T code for HQ-229310 by zhangkexin at 2022/09/29 end*/
#define LCM_DENSITY			(480)
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF
/*C3T code for HQ-229322 by jishen at 2022/10/12  start*/
extern bool cts_gesture_flag;
extern bool lcd_reset_keep_high;
/*C3T code for HQ-229322 by jishen at 2022/10/12  end*/
extern int32_t panel_rst_gpio;
extern int32_t panel_bias_enn_gpio;
extern int32_t panel_bias_enp_gpio;

extern struct pinctrl *lcd_pinctrl;
extern struct pinctrl_state *lcd_disp_pwm;
extern struct pinctrl_state *lcd_disp_pwm_gpio;
/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  start*/
extern struct ocp2131 *ocp;
extern int ocp2131_write_reg(struct ocp2131 *ocp, u8 reg, u8 data);
/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  end*/

extern int panel_gpio_config(struct device *dev);
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x6D, 2,{0x25,0x00}},
	{0x28, 2,{0x00,0x00}},
	{REGFLAG_DELAY, 20,{}},
	{0x10, 2,{0x00,0x00}},
	{REGFLAG_DELAY, 120,{}},
	{0xF0, 2,{0x5A,0x59}},
	{0xF1, 2,{0xA5,0xA6}},
	{0xBC, 8,{0x00,0x00,0x00,0x00,0x04,0x00,0xFF,0x30}},
	{0xBB, 8,{0x01,0x02,0x03,0x0A,0x04,0x13,0x14,0x00}},
	/*C3T code for HQ-242540 by zhangkexin at 2022/10/18 start*/
	{0xC2, 2,{0x00,0x05}},
	/*C3T code for HQ-242540 by zhangkexin at 2022/10/18 end*/
	{0xF1, 2,{0x5A,0x59}},
	{0xF0, 2,{0xA5,0xA6}},
	{REGFLAG_DELAY, 10,{}},
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xF0, 2,{0x5A,0x59}},
	{0xF1, 2,{0xA5,0xA6}},
	{0xBC, 8,{0x00,0x00,0x00,0x00,0x04,0x00,0xFF,0xF0}},
	{0xBB, 8,{0x01,0x02,0x03,0x0A,0x04,0x13,0x14,0x00}},
	/*C3T code for HQ-229305 by zhangkexin at 2022/10/14 start*/
	{0xC1,20,{0x00,0x20,0xBE,0xBE,0x04,0x20,0x20,0x04,0x72,0x06,0x22,0x70,0x35,0x30,0x07,0x11,0x84,0x4C,0x00,0x93}},
	/*C3T code for HQ-229305 by zhangkexin at 2022/10/14 end*/
	/*C3T code for HQ-242540 by zhangkexin at 2022/10/18 start*/
	{0xC2, 2,{0x80,0x45}},
	/*C3T code for HQ-242540 by zhangkexin at 2022/10/18 end*/
	{0xF1, 2,{0x5A,0x59}},
	{0xF0, 2,{0xA5,0xA6}},
	{0x35, 2,{0x00,0x00}},
	{0x51, 2,{0x0F,0xFF}},
	{0x53, 2,{0x2C,0x00}},
	{0x11, 2,{0x00,0x00}},
/*C3T code for HQ-254217 by jiangyue at 2022/11/15  start*/
	{REGFLAG_DELAY, 90,{}},
/*C3T code for HQ-254217 by jiangyue at 2022/11/15  end*/
	{0x29, 2,{0x00,0x00}},
	{REGFLAG_DELAY, 20,{}},
	{0x6D, 2,{0x02,0x00}},
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
	printk("%s: lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
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
	params->dsi.vertical_frontporch = 190;
//	params->dsi.vertical_frontporch_for_low_power = 750;	//OTM no data
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 4;
	/*C3T code for HQ-229305 by zhangkexin at 2022/10/14 start*/
	params->dsi.horizontal_backporch = 32;
	params->dsi.horizontal_frontporch = 32;
	/*C3T code for HQ-229305 by zhangkexin at 2022/10/14 end*/
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 285;
	//params->dsi.PLL_CK_CMD = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  start*/
	//params->dsi.CLK_HS_POST = 36;
	params->dsi.LPX = 4;
	params->dsi.cont_clock = 0;
	/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  end*/
	params->dsi.clk_lp_per_line_enable = 0;
	/*C3T code for HQ-242540 by zhangkexin at 2022/10/18 start*/
	params->dsi.esd_check_enable = 1;
	/*C3T code for HQ-242540 by zhangkexin at 2022/10/18 end*/
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
	/* for ARR 2.0 */
	//params->max_refresh_rate = 60;
	//params->min_refresh_rate = 45;
}
/*static int lcm_bias_regulator_init(void)
{
	return 0;
}*/
/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  start*/
static int lcm_bias_enable(void)
{
	gpio_set_value(panel_bias_enp_gpio, 1);
	MDELAY(1);
	pr_info("set bias to 5.5v");
	ocp2131_write_reg(ocp, 0x00, 0x0F);
	MDELAY(4);
	gpio_set_value(panel_rst_gpio, 1);
	MDELAY(6);
	gpio_set_value(panel_rst_gpio, 0);
	MDELAY(6);
	gpio_set_value(panel_rst_gpio, 1);
	MDELAY(6);
	ocp2131_write_reg(ocp, 0x01, 0x0F);
	gpio_set_value(panel_bias_enn_gpio, 1);
	MDELAY(38);
	return 0;
}
/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  end*/
static int lcm_bias_disable(void)
{
	gpio_set_value(panel_bias_enn_gpio, 0);
	MDELAY(2);
	gpio_set_value(panel_bias_enp_gpio, 0);
	return 0;
}
/* turn on gate ic & control voltage to 5.5V */
/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	lcm_bias_enable();
}
static void lcm_suspend_power(void)
{
	/*C3T code for HQ-229322 by jishen at 2022/10/12  start*/
	pr_info("lcd_reset_keep_high is %d\n",lcd_reset_keep_high);
	pr_info("cts_gesture_flag is %d\n",cts_gesture_flag);
	if (lcd_reset_keep_high || cts_gesture_flag) {
		pr_info("[LCM]%s:bias_keep_on\n",__func__);
		return;
	}
	/*C3T code for HQ-229322 by jishen at 2022/10/12  end*/
	lcm_bias_disable();
}
/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
		lcm_init_power();
}

static void lcm_init(void)
{
	gpio_set_value(panel_rst_gpio, 1);
	MDELAY(6);
	gpio_set_value(panel_rst_gpio, 0);
	MDELAY(6);
	gpio_set_value(panel_rst_gpio, 1);
	/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  start*/
	MDELAY(11);
	/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  end*/
	push_table(NULL,
		init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	printk("%s:chipone-lcm mode=vdo mode:%d\n", __func__, lcm_dsi_mode);
	pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm);
}
static void lcm_suspend(void)
{
	/*C3T code for HQ-258541 by jishen at 2022/10/25  start */
	if (!lcd_reset_keep_high) {
		pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm_gpio);
		push_table(NULL, lcm_suspend_setting,ARRAY_SIZE(lcm_suspend_setting), 1);
		printk("%s,chipone panel end!\n", __func__);
		return;
	}
 	else {/*push_table(NULL, lcm_suspend_no_off_setting,
			ARRAY_SIZE(lcm_suspend_no_off_setting), 1);*/
		printk("skip normal lcm_suspend when proximity status = %d",lcd_reset_keep_high);
		return;
	}
	/*C3T code for HQ-258541 by jishen at 2022/10/25  end */
}

static void lcm_resume(void)
{
	printk("%s,chipone panel start!\n", __func__);
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
	printk("%s,chipone backlight: level = %d\n", __func__, level);
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
struct LCM_DRIVER dsi_panel_c3t_36_0f_0c_dsc_vdo_lcm_drv = {
	.name = "dsi_panel_c3t_36_0f_0c_dsc_vdo",
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

