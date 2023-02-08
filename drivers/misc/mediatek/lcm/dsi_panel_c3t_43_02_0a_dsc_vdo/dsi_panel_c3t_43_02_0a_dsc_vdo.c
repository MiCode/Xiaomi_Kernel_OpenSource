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
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 start */
#include "../../../../input/touchscreen/mediatek/nt36525b_spi/nt36xxx.h"
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 end */
/*C3T code for HQ-223975 by sunfeiting at 2022/07/30 start*/
#include <linux/hqsysfs.h>
/*C3T code for HQ-223975 by sunfeiting at 2022/07/30 end*/

/* C3T code for HQ-219022 by jiangyue at 2022/08/22 start */
#ifdef mdelay
#undef mdelay
#endif
#ifdef udelay
#undef udelay
#endif
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 end */

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
/*C3T code for HQ-219023 by sunfeiting at 2022/08/09 start*/
#define LCM_PHYSICAL_WIDTH		(68150)
#define LCM_PHYSICAL_HEIGHT		(156173)
/*C3T code for HQ-219023 by sunfeiting at 2022/08/09 end*/
#define LCM_DENSITY			(480)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

/*C3T code for HQ-219139 by chenzimo at 2022/8/15 start*/
extern bool nvt_gesture_flag;
/*C3T code for HQ-219139 by chenzimo at 2022/8/15 end*/
int32_t panel_rst_gpio;
int32_t panel_bias_enn_gpio;
int32_t panel_bias_enp_gpio;
/*C3T code for HQ-218218 by chenzimo at 2022/8/09 start*/
bool lcd_reset_keep_high = false;
/*C3T code for HQ-218218 by chenzimo at 2022/8/09 end*/
/*C3T code for HQ-226117 by jiangyue at 2022/07/28 start*/
struct pinctrl *lcd_pinctrl;
struct pinctrl_state *lcd_disp_pwm;
struct pinctrl_state *lcd_disp_pwm_gpio;
/*C3T code for HQ-226117 by jiangyue at 2022/07/28 end*/
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
extern struct nvt_ts_data *ts;
extern int32_t nvt_update_firmware(char *firmware_name);
#endif
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 end */
/*C3T code for HQ-254217 by sunfeiting at 2022/10/10 start*/
extern struct ocp2131 *ocp;
extern int ocp2131_write_reg(struct ocp2131 *ocp, u8 reg, u8 data);
/*C3T code for HQ-254217 by sunfeiting at 2022/10/10 end*/
/*C3T code for HQ-218218 by chenzimo at 2022/8/09 start*/
void set_lcd_reset_gpio_keep_high(bool en)
{
	lcd_reset_keep_high = en;
}
EXPORT_SYMBOL(set_lcd_reset_gpio_keep_high);
/*C3T code for HQ-218218 by chenzimo at 2022/8/09 end*/

int panel_gpio_config(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	panel_rst_gpio = of_get_named_gpio(np, "panel,gpio_lcd_rst", 0);
	if (gpio_is_valid(panel_rst_gpio)) {
		ret = gpio_request(panel_rst_gpio, "panel-tp-rst");
		if (ret) {
			printk("Failed to request panel-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
	panel_bias_enn_gpio = of_get_named_gpio(np, "panel,gpio_lcd_bias_enn", 0);
	if (gpio_is_valid(panel_bias_enn_gpio)) {
		ret = gpio_request(panel_bias_enn_gpio, "panel-bias-enn");
		if (ret) {
			printk("Failed to request panel-bias-enn GPIO\n");
			goto err_request_enn_gpio;
		}
	}
	panel_bias_enp_gpio = of_get_named_gpio(np, "panel,gpio_lcd_bias_enp", 0);
	if (gpio_is_valid(panel_bias_enp_gpio)) {
		ret = gpio_request(panel_bias_enp_gpio, "panel-bias-enp");
		if (ret) {
			printk("Failed to request panel-bias-enp GPIO\n");
			goto err_request_enp_gpio;
		}
	}

/*C3T code for HQ-226117 by jiangyue at 2022/07/28 start*/
	lcd_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(lcd_pinctrl)) {
		ret = PTR_ERR(lcd_pinctrl);
		pr_notice(" Cannot find lcd_pinctrl %d!\n", ret);
	}

	lcd_disp_pwm = pinctrl_lookup_state(lcd_pinctrl, "disp_pwm");
	if (IS_ERR(lcd_pinctrl)) {
		ret = PTR_ERR(lcd_pinctrl);
		pr_notice(" Cannot find lcd_disp_pwm %d!\n", ret);
	}

	lcd_disp_pwm_gpio = pinctrl_lookup_state(lcd_pinctrl, "disp_pwm_gpio");
	if (IS_ERR(lcd_pinctrl)) {
		ret = PTR_ERR(lcd_pinctrl);
		pr_notice(" Cannot find lcd_disp_pwm_gpio %d!\n", ret);
	}
/*C3T code for HQ-226117 by jiangyue at 2022/07/28 end*/

err_request_enp_gpio:
	gpio_free(panel_bias_enn_gpio);
err_request_enn_gpio:
	gpio_free(panel_rst_gpio);
err_request_reset_gpio:
	return 0;
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	int ret = 0;
	ret = panel_gpio_config(dev);
	if (ret) {
		printk("panel parse dt error\n");
		return -1;
	}
/*C3T code for HQ-226117 by jiangyue at 2022/07/28 start*/
	pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm);
/*C3T code for HQ-226117 by jiangyue at 2022/07/28 end*/
	/*C3T code for HQ-223975 by sunfeiting at 2022/07/30 start*/
	/*C3T code for HQ-218836 by jiangyue at 2022/07/29 start*/
	if (strstr(saved_command_line,"dsi_panel_c3t_43_02_0a_dsc_vdo")) {
		hq_regiser_hw_info(HWID_LCM, "incell,vendor:43,IC:02");
	/*C3T code for HQ-218836 by jiangyue at 2022/07/29 end*/
		pr_err("nt365625b probe match");
	} else if(strstr(saved_command_line,"dsi_panel_c3t_31_03_0b_dsc_vdo")) {
		hq_regiser_hw_info(HWID_LCM, "incell,vendor:31,IC:03");
		pr_err("ft8057 probe match");
	/*C3T code for HQ-242537 by zhangkexin at 2022/09/28 start*/
	} else if(strstr(saved_command_line,"dsi_panel_c3t_36_0f_0c_dsc_vdo")) {
		hq_regiser_hw_info(HWID_LCM, "incell,vendor:36,IC:0f");
		pr_err("icnl9916 probe match");
	/*C3T code for HQ-242537 by zhangkexin at 2022/09/28 end*/
	}
	/*C3T code for HQ-223975 by sunfeiting at 2022/07/30 end*/
	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{
		.compatible = "mediatek,panel",
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
		.name = "panel",
		.owner = THIS_MODULE,
		.of_match_table = lcm_platform_of_match,
	},
};

static int __init lcm_drv_init(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);
	if (platform_driver_register(&lcm_driver)) {
		pr_notice("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_drv_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}

late_initcall(lcm_drv_init);
module_exit(lcm_drv_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");

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

/*C3T code for HQ-218218 by chenzimo at 2022/8/09 start*/
static struct LCM_setting_table lcm_suspend_no_off_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
};
/*C3T code for HQ-218218 by chenzimo at 2022/8/09 end*/

static struct LCM_setting_table init_setting_vdo[] = {
/* C3T code for HQ-244579 by sunfeiting at 2022/09/16 start */
	{0xFF, 1, {0x24}},
	{0xFB, 1, {0x01}},
	{0x5A, 1, {0xA2}},
	{0x5B, 1, {0x9B}},
	{0x5C, 1, {0x8C}},
/* C3T code for HQ-244579 by sunfeiting at 2022/09/16 end */

/* C3T code for HQ-219022 by jiangyue at 2022/08/22 start */
	{0xFF, 1, {0x24}},
	{0xFB, 1, {0x01}},
	{0xC3, 1, {0x01}},
	{0xC4, 1, {0x25}},
	{0xC7, 1, {0x08}},

	{0xFF, 1, {0x10}},
	{0xFB, 1, {0x01}},
	{0x35, 1, {0x00}},
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 end */

/* C3T code for HQ-235672 by jiangyue at 2022/09/29 start */
/* C3T code for HQ-236129 by jiangyue at 2022/10/31 start */
	{0x11, 0, {} },
/*C3T code for HQ-262330 by jiangyue at 2022/11/08 start*/
	{REGFLAG_DELAY, 100, {} },
/*C3T code for HQ-262330 by jiangyue at 2022/11/08 end*/
	{0x29, 0, {} },
	{REGFLAG_DELAY, 40, {} }
/* C3T code for HQ-236129 by jiangyue at 2022/10/31 end */
/* C3T code for HQ-235672 by jiangyue at 2022/09/29 end */
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

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 256;
	params->dsi.vertical_frontporch = 10;
//	params->dsi.vertical_frontporch_for_low_power = 750;	//OTM no data
	params->dsi.vertical_active_line = FRAME_HEIGHT;

/* C3T code for HQ-237547 by jiangyue at 2022/09/01 start */
/* C3T code for HQ-251504 by jiangyue at 2022/09/27 start */
	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 32;
	params->dsi.horizontal_frontporch = 16;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
/* C3T code for HQ-223331 by jiangyue at 2022/08/01 start */
	params->dsi.PLL_CLOCK = 288;
/* C3T code for HQ-251504 by jiangyue at 2022/09/27 end */
/* C3T code for HQ-223331 by jiangyue at 2022/08/01 end */
/* C3T code for HQ-237547 by jiangyue at 2022/09/01 end */
	params->dsi.PLL_CK_CMD = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
	params->dsi.esd_check_enable = 1;
#endif
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0;
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 end */
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

static int lcm_bias_enable(void)
{
	gpio_set_value(panel_bias_enp_gpio, 1);
/*C3T code for HQ-254217 by sunfeiting at 2022/10/10 start*/
	MDELAY(1);
	pr_info("set bias to 5.9v");
	ocp2131_write_reg(ocp, 0x00, 0x13);
	MDELAY(3);
	gpio_set_value(panel_bias_enn_gpio, 1);
	ocp2131_write_reg(ocp, 0x01, 0x13);
/*C3T code for HQ-254217 by sunfeiting at 2022/10/10 end*/
	MDELAY(5);

	return 0;
}

static int lcm_bias_disable(void)
{
	gpio_set_value(panel_bias_enn_gpio, 0);
	MDELAY(2);

	gpio_set_value(panel_bias_enp_gpio, 0);
	MDELAY(5);
	return 0;
}

/* turn on gate ic & control voltage to 5.5V */

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	lcm_bias_enable();
}

/*C3T code for HQ-219139 by chenzimo at 2022/8/15 start*/
static void lcm_suspend_power(void)
{
	if (lcd_reset_keep_high || nvt_gesture_flag) {
		pr_info("[LCM]%s:bias_keep_on\n",__func__);
		return;
	}
	lcm_bias_disable();
}
/*C3T code for HQ-219139 by chenzimo at 2022/8/15 end*/

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	lcm_init_power();
}

static void lcm_init(void)
{
	/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  start*/
	gpio_set_value(panel_rst_gpio, 0);
	MDELAY(7);
	/*C3T code for HQ-254217 by zhangkexin at 2022/10/25  end*/
	gpio_set_value(panel_rst_gpio, 1);
	MDELAY(5);
	gpio_set_value(panel_rst_gpio, 0);
	MDELAY(10);
	gpio_set_value(panel_rst_gpio, 1);
	MDELAY(15);
	push_table(NULL,
		init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	printk("%s:nt36525b_hd-lcm mode=vdo mode:%d\n", __func__, lcm_dsi_mode);
/*C3T code for HQ-219126 by jiangyue at 2022/08/15 start*/
	pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm);
/*C3T code for HQ-219126 by jiangyue at 2022/08/15 end*/
}

/*C3T code for HQ-218218 by chenzimo at 2022/8/09 start*/
static void lcm_suspend(void)
{
/*C3T code for HQ-219126 by jiangyue at 2022/08/15 start*/
	pinctrl_select_state(lcd_pinctrl, lcd_disp_pwm_gpio);
/*C3T code for HQ-219126 by jiangyue at 2022/08/15 end*/
	if (lcd_reset_keep_high) {
		push_table(NULL, lcm_suspend_no_off_setting,
		ARRAY_SIZE(lcm_suspend_no_off_setting), 1);
		printk("%s,nt36525b_hd panel no off end!\n", __func__);
	} else {
		push_table(NULL, lcm_suspend_setting,
		ARRAY_SIZE(lcm_suspend_setting), 1);
		printk("%s,nt36525b_hd panel end!\n", __func__);
	}
}
/*C3T code for HQ-218218 by chenzimo at 2022/8/09 end*/

static void lcm_resume(void)
{
	printk("%s,nt36525b_hd panel start!\n", __func__);
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
	printk("%s,nt36525b_hd backlight: level = %d\n", __func__, level);

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
	return 1;
}
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
static unsigned int lcd_esd_recover(void)
{
	LCM_LOGI("%s, int and update tp fw..\n", __func__);
	lcm_init_power();
	lcm_init();

	mutex_lock(&ts->lock);
	nvt_update_firmware("novatek_ts_truly_fw.bin");
	mutex_unlock(&ts->lock);

	return 0;
}
#endif
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 end */

/*C3T code for HQ-218836 by jiangyue at 2022/07/29 start*/
struct LCM_DRIVER dsi_panel_c3t_43_02_0a_dsc_vdo_lcm_drv = {
	.name = "dsi_panel_c3t_43_02_0a_dsc_vdo",
/*C3T code for HQ-218836 by jiangyue at 2022/07/29 end*/
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
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
	.esd_recover = lcd_esd_recover,
#endif
/* C3T code for HQ-219022 by jiangyue at 2022/08/22 end */
};
