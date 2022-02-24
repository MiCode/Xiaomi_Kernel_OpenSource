// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

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
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif
#endif

#include "lcm_drv.h"
#include "lcm_util.h"
#include "Backlight_I2C_map_hx.h"
#include "sgm37604a.h"
#include "ocp2138.h"


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
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif

#endif

static unsigned int GPIO_LCD_RST     = 370;/* GPIO45 */
static unsigned int GPIO_LCD_PWR_ENN = 494;/* GPIO169 */
static unsigned int GPIO_LCD_PWR_ENP = 490;/* GPIO165 */
static unsigned int GPIO_LCD_PWM_EN  = 368;/* GPIO43 */
static unsigned int GPIO_LCD_BL_EN   = 483;/* GPIO158 */

#ifndef BUILD_LK
static void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	pr_info("[KE/LCM] GPIO_LCD_RST = 0x%x\n", GPIO_LCD_RST);

	GPIO_LCD_PWR_ENN = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr_enn", 0);
	gpio_request(GPIO_LCD_PWR_ENN, "GPIO_LCD_PWR_ENN");
	pr_info("[KE/LCM] GPIO_LCD_PWR_ENN = 0x%x\n", GPIO_LCD_PWR_ENN);

	GPIO_LCD_PWR_ENP = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr_enp", 0);
	gpio_request(GPIO_LCD_PWR_ENP, "GPIO_LCD_PWR_ENP");
	pr_info("[KE/LCM] GPIO_LCD_PWR_ENP = 0x%x\n", GPIO_LCD_PWR_ENP);

	GPIO_LCD_PWM_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_pwm_en", 0);
	gpio_request(GPIO_LCD_PWM_EN, "GPIO_LCD_PWM_EN");
	pr_info("[KE/LCM] GPIO_LCD_PWM_EN = 0x%x\n", GPIO_LCD_PWM_EN);

	GPIO_LCD_BL_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_bl_en", 0);
	gpio_request(GPIO_LCD_BL_EN, "GPIO_LCD_BL_EN");
	pr_info("[KE/LCM] GPIO_LCD_BL_EN = 0x%x\n", GPIO_LCD_BL_EN);
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	pr_info("[KE/LCM] %s Enter\n", __func__);

	lcm_request_gpio_control(dev);

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "hx,hx83102p",
		.data = 0,
	}, {
		/* sentinel */
	}
};

MODULE_DEVICE_TABLE(of, platform_of_match);

static int lcm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(lcm_platform_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		.name = "hx83102p_wxga_vdo_incell_boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_init(void)
{
	if (platform_driver_register(&lcm_driver)) {
		pr_info("LCM: failed to register this driver!\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_exit(void)
{
	platform_driver_unregister(&lcm_driver);
}

late_initcall(lcm_init);
module_exit(lcm_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("LCM display subsystem driver");
MODULE_LICENSE("GPL");
#endif

/* --------------------------------------------------------------------- */
/*  Local Constants */
/* --------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE                                    0
#define FRAME_WIDTH                                         (1200)
#define FRAME_HEIGHT                                        (2000)
#define LCM_ID_NT35595                                      (0x95)
#define GPIO_OUT_ONE                                        1
#define GPIO_OUT_ZERO                                       0

#define REGFLAG_DELAY                                       0xFC
#define REGFLAG_END_OF_TABLE                                0xFD

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* --------------------------------------------------------------------- */
/*  Local Variables */
/* --------------------------------------------------------------------- */
static struct LCM_UTIL_FUNCS lcm_util = {0};
#define SET_RESET_PIN(v)     (lcm_util.set_reset_pin((v)))
#define UDELAY(n)            (lcm_util.udelay(n))
#define MDELAY(n)            (lcm_util.mdelay(n))

/* --------------------------------------------------------------------- */
/*  Local Functions */
/* --------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)      lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)       lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL, fmt)
#else
#define LCD_DEBUG(fmt)  pr_debug(fmt)
#endif
static unsigned short s_last_backlight_level = 255; //default level, must align with lk

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, output);
#else
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
#endif
}

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 60, {} },
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 80, {} },
	{0xB1, 1, {0x21} },
	{REGFLAG_DELAY, 50, {} },
};

static struct LCM_setting_table lcm_initinal_setting[] = {
	{ 0xB9, 3, { 0x83, 0x10, 0x2E } },
	{ 0xE9, 1, { 0xCD } },
	{ 0xBB, 1, { 0x01 } },
	{ REGFLAG_DELAY, 5, { } },
	{ 0xE9, 1, { 0x00 } },
	{ 0xD1, 4, { 0x67, 0x0C, 0xFF, 0x05 } },
	{ 0xB1, 17, { 0x10, 0xFA, 0xAF, 0xAF, 0x2B, 0x2B, 0xC1, 0x75, 0x39,
			0x36, 0x36, 0x36, 0x36, 0x22, 0x21, 0x15, 0x00 } },
	{ 0xB2, 16, { 0x00, 0xB0, 0x47, 0xD0, 0x00, 0x2C, 0x50, 0x2C, 0x00,
			0x00, 0x00, 0x00, 0x15, 0x20, 0xD7, 0x00 } },
	{ 0xB4, 16, { 0x38, 0x47, 0x38, 0x47, 0x66, 0x4E, 0x00, 0x00, 0x01,
			0x72, 0x01, 0x58, 0x00, 0xFF, 0x00, 0xFF } },
	{ 0xBF, 3, { 0xFC, 0x85, 0x80 } },
	{ 0xD2, 2, { 0x2B, 0x2B } },
	{ 0xD3, 43, { 0x00, 0x00, 0x00, 0x00, 0x78, 0x04, 0x00, 0x14, 0x00,
			0x27, 0x00, 0x44, 0x4F, 0x29, 0x29, 0x00, 0x00, 0x32, 0x10,
			0x25, 0x00, 0x25, 0x32, 0x10, 0x1F, 0x00, 0x1F, 0x32, 0x18,
			0x10, 0x08, 0x10, 0x00, 0x00, 0x20, 0x30, 0x01, 0x55, 0x21,
			0x2E, 0x01, 0x55, 0x0F } },
	{ REGFLAG_DELAY, 5, { } },
	{ 0xE0, 46, { 0x00, 0x04, 0x0B, 0x11, 0x17, 0x26, 0x3D, 0x45, 0x4D,
			0x4A, 0x65, 0x6D, 0x75, 0x87, 0x86, 0x92, 0x9D, 0xB0, 0xAF,
			0x56, 0x5E, 0x68, 0x70, 0x00, 0x04, 0x0B, 0x11, 0x17, 0x26,
			0x3D, 0x45, 0x4D, 0x4A, 0x65, 0x6D, 0x75, 0x87, 0x86, 0x92,
			0x9D, 0xB0, 0xAF, 0x56, 0x5E, 0x68, 0x70 } },
	{ REGFLAG_DELAY, 5, { } },
	{ 0xCB, 5, { 0x00, 0x13, 0x08, 0x02, 0x34 } },
	{ 0xBD, 1, { 0x01 } },
	{ 0xB1, 4, { 0x01, 0x9B, 0x01, 0x31 } },
	{ 0xCB, 10, { 0xF4, 0x36, 0x12, 0x16, 0xC0, 0x28, 0x6C, 0x85, 0x3F, 0x04 } },
	{ 0xD3, 11, { 0x01, 0x00, 0x3C, 0x00, 0x00, 0x11, 0x10, 0x00, 0x0E,
			0x00, 0x01 } },
	{ REGFLAG_DELAY, 5, { } },
	{ 0xBD, 1, { 0x02 } },
	{ 0xB4, 6, { 0x4E, 0x00, 0x33, 0x11, 0x33, 0x88 } },
	{ 0xBF, 3, { 0xF2, 0x00, 0x02 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xC0, 14, { 0x23, 0x23, 0x22, 0x11, 0xA2, 0x17, 0x00, 0x80, 0x00,
			0x00, 0x08, 0x00, 0x63, 0x63 } },
	{ 0xC6, 1, { 0xF9 } },
	{ 0xC7, 1, { 0x30 } },
	{ 0xC8, 8, { 0x00, 0x04, 0x04, 0x00, 0x00, 0x85, 0x43, 0xFF } },
	{ 0xD0, 3, { 0x07, 0x04, 0x05 } },
	{ 0xD5, 44, { 0x21, 0x20, 0x21, 0x20, 0x25, 0x24, 0x25, 0x24, 0x18,
			0x18, 0x18, 0x18, 0x1A, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B,
			0x1B, 0x03, 0x02, 0x03, 0x02, 0x01, 0x00, 0x01, 0x00, 0x07,
			0x06, 0x07, 0x06, 0x05, 0x04, 0x05, 0x04, 0x18, 0x18, 0x18,
			0x18, 0x18, 0x18, 0x18, 0x18 } },
	{ REGFLAG_DELAY, 5, { } },
	{ 0xE7, 23, { 0x12, 0x13, 0x02, 0x02, 0x57, 0x57, 0x0E, 0x0E, 0x1B,
			0x28, 0x29, 0x74, 0x28, 0x74, 0x01, 0x07, 0x00, 0x00, 0x00,
			0x00, 0x17, 0x00, 0x68 } },
	{ 0xBD, 1, { 0x01 } },
	{ 0xE7, 7, { 0x02, 0x38, 0x01, 0x93, 0x0D, 0xD9, 0x0E } },
	{ 0xBD, 1, { 0x02 } },
	{ 0xE7, 28, { 0xFF, 0x01, 0xFF, 0x01, 0x00, 0x00, 0x22, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x00, 0x02, 0x40 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xBA, 8, { 0x70, 0x03, 0xA8, 0x83, 0xF2, 0x00, 0xC0, 0x0D } },
	{ 0xBD, 1, { 0x02 } },
	{ 0xD8, 12, { 0xAF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0xAF, 0xFF, 0xFF,
			0xFF, 0xF0, 0x00 } },
	{ 0xBD, 1, { 0x03 } },
	{ 0xD8, 24, { 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0x00, 0xAA, 0xAA, 0xAA,
			0xAA, 0xA0, 0x00, 0x55, 0x55, 0x55, 0x55, 0x50, 0x00, 0x55,
			0x55, 0x55, 0x55, 0x50, 0x00 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xE1, 2, { 0x01, 0x04 } },
	{ 0xCC, 1, { 0x02 } },
	{ 0xBD, 1, { 0x03 } },
	{ 0xB2, 1, { 0x80 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0x35, 1, { 0x00 } },
	{ 0xB2, 16, { 0x00, 0xB0, 0x47, 0xD0, 0x00, 0x2C, 0x50, 0x2C, 0x00,
		0x00, 0x00, 0x00, 0x15, 0x20, 0xD7, 0x00 } },
	{ 0x11, 0, { 0x11 } },
	{ REGFLAG_DELAY, 60, { } },
	{ 0x29, 0, { 0x29 } },
	{ REGFLAG_DELAY, 20, { } },
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
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
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

/* --------------------------------------------------------------------- */
/*  LCM Driver Implementations */
/* --------------------------------------------------------------------- */
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.cont_clock = 1;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.mode = BURST_VDO_MODE;

	params->dsi.LANE_NUM                = LCM_FOUR_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size             = 256;

	params->dsi.PS                      = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active    = 8;
	params->dsi.vertical_backporch      = 38;
	params->dsi.vertical_frontporch     = 80;
	params->dsi.vertical_active_line    = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active  = 8;
	params->dsi.horizontal_backporch    = 28;
	params->dsi.horizontal_frontporch   = 42;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable             = 1; */
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK               = 500;
#else
	params->dsi.PLL_CLOCK               = 525;
#endif
}

static void lcm_init_power(void)
{
	pr_info("[Kernel/LCM] %s()\n", __func__);
	/* set AVDD*/
	/*5.7V + 0.1* 100mV*/
	ocp2138_write_byte(0x00, 0x11);
	/* set AVEE */
	/*-5.70V - 0.1* 100mV*/
	ocp2138_write_byte(0x01, 0x11);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_PWR_ENP, GPIO_OUT_ONE);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_PWR_ENN, GPIO_OUT_ONE);
	MDELAY(5);
}

static void lcm_init_lcm(void)
{
	pr_info("[KERNEL/LCM] hx83102p %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(50);

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
	MDELAY(5);
	//register
	sgm37604a_write_byte(0x11, 0x00);
	MDELAY(10);

	push_table(lcm_initinal_setting,
		sizeof(lcm_initinal_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	pr_info("[Kernel/LCM] hx83102p %s() enter\n", __func__);
	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
	push_table(lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(15);
}

static void lcm_suspend_power(void)
{
	pr_info("[Kernel/LCM] %s\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(3);
	lcm_set_gpio_output(GPIO_LCD_PWR_ENN, GPIO_OUT_ZERO);
	MDELAY(3);
	lcm_set_gpio_output(GPIO_LCD_PWR_ENP, GPIO_OUT_ZERO);
	MDELAY(15);
}
static void lcm_resume_power(void)
{
	lcm_init_power();
}

static void lcm_resume(void)
{
	pr_info("[Kernel/LCM] hx83102p %s enter\n", __func__);
	lcm_init_lcm();

	pr_info("[Kernel/LCM] %s s_last_backlight_level=%d\n", __func__, s_last_backlight_level);
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[2];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00023700; /* read id return two byte, version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0]; /* we only need ID */
#ifdef BUILD_LK
	dprintf(0, "%s, [LK/LCM] hx83102p: id = 0x%08x\n", __func__, id);
#else
	pr_info("%s, [Kernel/LCM] hx83102p: id = 0x%08x\n", __func__, id);
#endif

	if (id == LCM_ID_NT35595)
		return 1;
	else
		return 0;
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	int sgm_level = 0;

	if (level > 255)
		level = 255;

	sgm_level = backlight_i2c_map_hx[level];
	pr_info("lsy_kernel %s,hx83102p backlight: level = %d sgm_level = %d\n",
			__func__, level, sgm_level);

	while (level != s_last_backlight_level) {
		if (level > s_last_backlight_level)
			s_last_backlight_level++;
		else if (level < s_last_backlight_level)
			s_last_backlight_level--;
		sgm37604a_write_byte(0x1A, (backlight_i2c_map_hx[s_last_backlight_level] & 0x0F));
		sgm37604a_write_byte(0x19, (backlight_i2c_map_hx[s_last_backlight_level] >> 4));
	}
}

struct LCM_DRIVER hx83102p_wxga_vdo_incell_boe_lcm_drv = {
	.name               = "hx83102p_wxga_vdo_incell_boe",
	.set_util_funcs     = lcm_set_util_funcs,
	.get_params         = lcm_get_params,
	.init               = lcm_init_lcm,
	.resume             = lcm_resume,
	.suspend            = lcm_suspend,
	.init_power         = lcm_init_power,
	.resume_power       = lcm_resume_power,
	.suspend_power      = lcm_suspend_power,
	.compare_id         = lcm_compare_id,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
};

