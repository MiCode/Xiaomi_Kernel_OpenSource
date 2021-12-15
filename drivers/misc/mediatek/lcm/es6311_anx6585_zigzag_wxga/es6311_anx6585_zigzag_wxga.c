// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#else
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
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
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
}

#ifndef BUILD_LK
static unsigned int GPIO_LCD_RST_EN; /* GPIO45 */
static unsigned int GPIO_LCD_PWR_EN; /* GPIO158 */

void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_RST_EN = of_get_named_gpio(dev->of_node, "lcd_rst_pin", 0);
	gpio_request(GPIO_LCD_RST_EN, "GPIO_LCD_RST_EN");

	GPIO_LCD_PWR_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr_en", 0);
	gpio_request(GPIO_LCD_PWR_EN, "GPIO_LCD_PWR_EN");
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	lcm_request_gpio_control(dev);

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "es,es6311_anx6585_zigzag_wxga",
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
		.name = "es6311_anx6585_zigzag_wxga",
		.owner = THIS_MODULE,
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_init(void)
{
	pr_notice("[Kernel/LCM] register lcm init driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_notice("LCM: failed to register this driver!\n");
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

/* ------------------------------------------------------------------- */
/* Local Constants */
/* ------------------------------------------------------------------- */

#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)

/* ------------------------------------------------------------------- */
/* Local Variables */
/* ------------------------------------------------------------------- */

static struct LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* ------------------------------------------------------------------- */
/* Local Functions */
/* ------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define   LCM_DSI_CMD_MODE	0

static void lcm_initial_registers(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = 0xA555AAFF;
	data_array[2] = 0x00000125;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020FC;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(2);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000000FC;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(2);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020FC;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(2);

	data_array[0] = 0x00053902;
	data_array[1] = 0x2555AAFF;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000168B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000008B6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x080201B8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x004444BB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000000BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x106802BD;
	data_array[2] = 0x00000010;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000080C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x004F4FB3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001010B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000505B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003535B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002525BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000068BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000068BD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000033BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x00000CC0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000CA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000208;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000001EE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00113902;
	data_array[1] = 0x000000B0;
	data_array[2] = 0x002A000F;
	data_array[3] = 0x00540040;
	data_array[4] = 0x00930076;
	data_array[5] = 0x000000C5;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0] = 0x00113902;
	data_array[1] = 0x01F000B1;
	data_array[2] = 0x01660132;
	data_array[3] = 0x02FF01BB;
	data_array[4] = 0x02420201;
	data_array[5] = 0x00000085;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0] = 0x00113902;
	data_array[1] = 0x02AF02B2;
	data_array[2] = 0x030503E0;
	data_array[3] = 0x03540335;
	data_array[4] = 0x03A00384;
	data_array[5] = 0x000000C4;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x03F203B3;
	data_array[2] = 0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000308;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000000B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000000B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x170008B2;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B6;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0xA00053BA;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0xA00053BB;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C0;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C1;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000060C4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x0000C0C5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000508;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x00000CB8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x00000ABB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000002BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x010103BD;
	data_array[2] = 0x00000303;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000007C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x0000A2C4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002003C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002101C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x010000CC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x010000CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0xFC0400D1;
	data_array[2] = 0x00001407;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000510D2;
	data_array[2] = 0x00001603;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006E9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000006EA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000030ED;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000608;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001117B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001016B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001812B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001913B3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003100B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003431B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002934B6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00332AB7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002D2EB8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003408B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000834BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2DBB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A34BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003429BD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003134BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000031BF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001319C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001218C1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001610C2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001711C3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434E5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001812C4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001913C5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001117C6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001016C7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003108C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003431C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002934CA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00332ACB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002E2DCC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003400CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000034CE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002D2ECF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A34D0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003429D1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003134D2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000831D3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001610D4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001711D5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001319D6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001218D7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003434E6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D8;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D9;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000000E7;
	dsi_set_cmdq(data_array, 2, 1);

#if 0
	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);
#endif

	data_array[0] = 0x00023902;
	data_array[1] = 0x00000035;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);
}


static void lcm_init_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
	MDELAY(10);
}

static void lcm_suspend_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
	MDELAY(20);
}


static void lcm_resume_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
}

/* ------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* ------------------------------------------------------------------- */
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->density = 160;
#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = BURST_VDO_MODE;
#endif

	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active    = 5;
	params->dsi.vertical_backporch      = 3;
	params->dsi.vertical_frontporch	    = 8;
	params->dsi.vertical_active_line    = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active  = 5;
	params->dsi.horizontal_backporch    = 59;
	params->dsi.horizontal_frontporch   = 16;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 210;
}

static void lcm_init_lcm(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

#ifdef BUILD_LK
	lcm_resume();
#endif
}

void lcm_suspend(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
}

void lcm_resume(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
	MDELAY(1);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
	MDELAY(2);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
	MDELAY(5);

	lcm_initial_registers();
}

#if (LCM_DSI_CMD_MODE)
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

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00290508;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
#endif

static unsigned int lcm_ata_check(unsigned char *buffer)
{
	return 1;
}

struct LCM_DRIVER es6311_anx6585_zigzag_wxga_lcm_drv = {
	.name = "es6311_anx6585_zigzag_wxga",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.ata_check = lcm_ata_check,
#if (LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
};
