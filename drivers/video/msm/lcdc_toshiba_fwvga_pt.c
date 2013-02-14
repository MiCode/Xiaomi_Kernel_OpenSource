/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <mach/gpio.h>
#include <mach/pmic.h>
#include <mach/socinfo.h>
#include "msm_fb.h"

static int spi_cs0_N;
static int spi_sclk;
static int spi_mosi;
static int spi_miso;

struct toshiba_state_type {
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

static struct toshiba_state_type toshiba_state = { 0 };
static struct msm_panel_common_pdata *lcdc_toshiba_pdata;

static int toshiba_spi_write(char data1, char data2, int rs)
{
	uint32 bitdata = 0, bnum = 24, bmask = 0x800000;

	gpio_set_value_cansleep(spi_cs0_N, 0);	/* cs* low */
	udelay(1);

	if (rs)
		bitdata = (0x72 << 16);
	else
		bitdata = (0x70 << 16);

	bitdata |= ((data1 << 8) | data2);

	while (bnum) {
		gpio_set_value_cansleep(spi_sclk, 0); /* clk low */
		udelay(1);

		if (bitdata & bmask)
			gpio_set_value_cansleep(spi_mosi, 1);
		else
			gpio_set_value_cansleep(spi_mosi, 0);

		udelay(1);
		gpio_set_value_cansleep(spi_sclk, 1); /* clk high */
		udelay(1);
		bmask >>= 1;
		bnum--;
	}

	gpio_set_value_cansleep(spi_cs0_N, 1);	/* cs* high */
	udelay(1);
	return 0;
}

static void spi_pin_assign(void)
{
	/* Setting the Default GPIO's */
	spi_mosi  = *(lcdc_toshiba_pdata->gpio_num);
	spi_miso  = *(lcdc_toshiba_pdata->gpio_num + 1);
	spi_sclk  = *(lcdc_toshiba_pdata->gpio_num + 2);
	spi_cs0_N = *(lcdc_toshiba_pdata->gpio_num + 3);
}

static void toshiba_disp_powerup(void)
{
	if (!toshiba_state.disp_powered_up && !toshiba_state.display_on) {
		/* Reset the hardware first */
		/* Include DAC power up implementation here */
	      toshiba_state.disp_powered_up = TRUE;
	}
}

static void toshiba_disp_on(void)
{
	if (toshiba_state.disp_powered_up && !toshiba_state.display_on) {
		toshiba_spi_write(0x01, 0x00, 0);
		toshiba_spi_write(0x30, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x01, 0x01, 0);
		toshiba_spi_write(0x40, 0x10, 1);

#ifdef TOSHIBA_FWVGA_FULL_INIT
		udelay(500);
		toshiba_spi_write(0x01, 0x06, 0);
		toshiba_spi_write(0x00, 0x00, 1);
		msleep(20);

		toshiba_spi_write(0x00, 0x01, 0);
		toshiba_spi_write(0x03, 0x10, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x02, 0);
		toshiba_spi_write(0x01, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x03, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x07, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x08, 0);
		toshiba_spi_write(0x00, 0x04, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x09, 0);
		toshiba_spi_write(0x00, 0x0c, 1);
#endif
		udelay(500);
		toshiba_spi_write(0x00, 0x0c, 0);
		toshiba_spi_write(0x40, 0x10, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x0e, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x20, 0);
		toshiba_spi_write(0x01, 0x3f, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x22, 0);
		toshiba_spi_write(0x76, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x23, 0);
		toshiba_spi_write(0x1c, 0x0a, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x24, 0);
		toshiba_spi_write(0x1c, 0x2c, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x25, 0);
		toshiba_spi_write(0x1c, 0x4e, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x27, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x28, 0);
		toshiba_spi_write(0x76, 0x0c, 1);

#ifdef TOSHIBA_FWVGA_FULL_INIT
		udelay(500);
		toshiba_spi_write(0x03, 0x00, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x01, 0);
		toshiba_spi_write(0x05, 0x02, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x02, 0);
		toshiba_spi_write(0x07, 0x05, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x03, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x04, 0);
		toshiba_spi_write(0x02, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x05, 0);
		toshiba_spi_write(0x07, 0x07, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x06, 0);
		toshiba_spi_write(0x10, 0x10, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x07, 0);
		toshiba_spi_write(0x02, 0x02, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x08, 0);
		toshiba_spi_write(0x07, 0x04, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x09, 0);
		toshiba_spi_write(0x07, 0x07, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x0a, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x0b, 0);
		toshiba_spi_write(0x00, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x0c, 0);
		toshiba_spi_write(0x07, 0x07, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x0d, 0);
		toshiba_spi_write(0x10, 0x10, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x10, 0);
		toshiba_spi_write(0x01, 0x04, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x11, 0);
		toshiba_spi_write(0x05, 0x03, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x12, 0);
		toshiba_spi_write(0x03, 0x04, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x15, 0);
		toshiba_spi_write(0x03, 0x04, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x16, 0);
		toshiba_spi_write(0x03, 0x1c, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x17, 0);
		toshiba_spi_write(0x02, 0x04, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x18, 0);
		toshiba_spi_write(0x04, 0x02, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x19, 0);
		toshiba_spi_write(0x03, 0x05, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x1c, 0);
		toshiba_spi_write(0x07, 0x07, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x1d, 0);
		toshiba_spi_write(0x02, 0x1f, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x20, 0);
		toshiba_spi_write(0x05, 0x07, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x21, 0);
		toshiba_spi_write(0x06, 0x04, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x22, 0);
		toshiba_spi_write(0x04, 0x05, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x27, 0);
		toshiba_spi_write(0x02, 0x03, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x28, 0);
		toshiba_spi_write(0x03, 0x00, 1);

		udelay(500);
		toshiba_spi_write(0x03, 0x29, 0);
		toshiba_spi_write(0x00, 0x02, 1);

#endif
		udelay(500);
		toshiba_spi_write(0x01, 0x00, 0);
		toshiba_spi_write(0x36, 0x3c, 1);
		udelay(500);

		toshiba_spi_write(0x01, 0x01, 0);
		toshiba_spi_write(0x40, 0x03, 1);

		udelay(500);
		toshiba_spi_write(0x01, 0x02, 0);
		toshiba_spi_write(0x00, 0x01, 1);

		udelay(500);
		toshiba_spi_write(0x01, 0x03, 0);
		toshiba_spi_write(0x3c, 0x58, 1);

		udelay(500);
		toshiba_spi_write(0x01, 0x0c, 0);
		toshiba_spi_write(0x01, 0x35, 1);

		udelay(500);
		toshiba_spi_write(0x01, 0x06, 0);
		toshiba_spi_write(0x00, 0x02, 1);

		udelay(500);
		toshiba_spi_write(0x00, 0x29, 0);
		toshiba_spi_write(0x03, 0xbf, 1);

		udelay(500);
		toshiba_spi_write(0x01, 0x06, 0);
		toshiba_spi_write(0x00, 0x03, 1);
		msleep(32);

		toshiba_spi_write(0x01, 0x01, 0);
		toshiba_spi_write(0x40, 0x10, 1);
		msleep(80);

		toshiba_state.display_on = TRUE;
	}
}

static int lcdc_toshiba_panel_on(struct platform_device *pdev)
{
	if (!toshiba_state.disp_initialized) {
		/* Configure reset GPIO that drives DAC */
		if (lcdc_toshiba_pdata->panel_config_gpio)
			lcdc_toshiba_pdata->panel_config_gpio(1);
		toshiba_disp_powerup();
		toshiba_disp_on();
		toshiba_state.disp_initialized = TRUE;
	}
	return 0;
}

static int lcdc_toshiba_panel_off(struct platform_device *pdev)
{
	if (toshiba_state.disp_powered_up && toshiba_state.display_on) {
		toshiba_spi_write(0x01, 0x06, 1);
		toshiba_spi_write(0x00, 0x02, 1);
		msleep(80);

		toshiba_spi_write(0x01, 0x06, 1);
		toshiba_spi_write(0x00, 0x00, 1);

		toshiba_spi_write(0x00, 0x29, 1);
		toshiba_spi_write(0x00, 0x02, 1);

		toshiba_spi_write(0x01, 0x00, 1);
		toshiba_spi_write(0x30, 0x00, 1);

		if (lcdc_toshiba_pdata->panel_config_gpio)
			lcdc_toshiba_pdata->panel_config_gpio(0);
		toshiba_state.display_on = FALSE;
		toshiba_state.disp_initialized = FALSE;
	}

	return 0;
}

static void lcdc_toshiba_set_backlight(struct msm_fb_data_type *mfd)
{
	int ret;
	int bl_level;

	bl_level = mfd->bl_level;

	if (lcdc_toshiba_pdata && lcdc_toshiba_pdata->pmic_backlight)
		ret = lcdc_toshiba_pdata->pmic_backlight(bl_level);
	else
		pr_err("%s(): Backlight level set failed", __func__);

	return;
}

static int __devinit toshiba_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_toshiba_pdata = pdev->dev.platform_data;
		spi_pin_assign();
		return 0;
	}
	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = toshiba_probe,
	.driver = {
		.name   = "lcdc_toshiba_fwvga_pt",
	},
};

static struct msm_fb_panel_data toshiba_panel_data = {
	.on = lcdc_toshiba_panel_on,
	.off = lcdc_toshiba_panel_off,
	.set_backlight = lcdc_toshiba_set_backlight,
};

static struct platform_device this_device = {
	.name   = "lcdc_toshiba_fwvga_pt",
	.id	= 1,
	.dev	= {
		.platform_data = &toshiba_panel_data,
	}
};

static int __init lcdc_toshiba_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	ret = msm_fb_detect_client("lcdc_toshiba_fwvga_pt");
	if (ret)
		return 0;


	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &toshiba_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 864;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	/* 30Mhz mdp_lcdc_pclk and mdp_lcdc_pad_pcl */
	pinfo->clk_rate = 30720000;
	pinfo->bl_max = 100;
	pinfo->bl_min = 1;

	if (cpu_is_msm7x25a() || cpu_is_msm7x25aa() || cpu_is_msm7x25ab()) {
		pinfo->yres = 320;
		pinfo->lcdc.h_back_porch = 10;
		pinfo->lcdc.h_front_porch = 21;
		pinfo->lcdc.h_pulse_width = 5;
		pinfo->lcdc.v_back_porch = 8;
		pinfo->lcdc.v_front_porch = 540;
		pinfo->lcdc.v_pulse_width = 42;
		pinfo->lcdc.border_clr = 0;     /* blk */
		pinfo->lcdc.underflow_clr = 0xff;       /* blue */
		pinfo->lcdc.hsync_skew = 0;
	} else {
		pinfo->lcdc.h_back_porch = 8;
		pinfo->lcdc.h_front_porch = 16;
		pinfo->lcdc.h_pulse_width = 8;
		pinfo->lcdc.v_back_porch = 2;
		pinfo->lcdc.v_front_porch = 2;
		pinfo->lcdc.v_pulse_width = 2;
		pinfo->lcdc.border_clr = 0;     /* blk */
		pinfo->lcdc.underflow_clr = 0xff;       /* blue */
		pinfo->lcdc.hsync_skew = 0;
	}

	ret = platform_device_register(&this_device);
	if (ret) {
		printk(KERN_ERR "%s not able to register the device\n",
			 __func__);
		platform_driver_unregister(&this_driver);
	}
	return ret;
}

device_initcall(lcdc_toshiba_panel_init);
