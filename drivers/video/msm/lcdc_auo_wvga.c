/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/pwm.h>
#ifdef CONFIG_SPI_QUP
#include <linux/spi/spi.h>
#else
#include <mach/gpio.h>
#endif
#include "msm_fb.h"

#define MAX_BACKLIGHT_LEVEL			15
#define PANEL_CMD_BACKLIGHT_LEVEL	0x6A18
#define PANEL_CMD_FORMAT			0x3A00
#define PANEL_CMD_RGBCTRL			0x3B00
#define PANEL_CMD_BCTRL				0x5300
#define PANEL_CMD_PWM_EN			0x6A17

#define PANEL_CMD_SLEEP_OUT			0x1100
#define PANEL_CMD_DISP_ON			0x2900
#define PANEL_CMD_DISP_OFF			0x2800
#define PANEL_CMD_SLEEP_IN			0x1000

#define LCDC_AUO_PANEL_NAME			"lcdc_auo_wvga"

#ifdef CONFIG_SPI_QUP
#define LCDC_AUO_SPI_DEVICE_NAME	"lcdc_auo_nt35582"
static struct spi_device *lcdc_spi_client;
#else
static int spi_cs;
static int spi_sclk;
static int spi_mosi;
#endif

struct auo_state_type {
	boolean display_on;
	int bl_level;
};


static struct auo_state_type auo_state = { .bl_level = 10 };
static struct msm_panel_common_pdata *lcdc_auo_pdata;

#ifndef CONFIG_SPI_QUP
static void auo_spi_write_byte(u8 data)
{
	uint32 bit;
	int bnum;

	bnum = 8;			/* 8 data bits */
	bit = 0x80;
	while (bnum--) {
		gpio_set_value(spi_sclk, 0); /* clk low */
		gpio_set_value(spi_mosi, (data & bit) ? 1 : 0);
		udelay(1);
		gpio_set_value(spi_sclk, 1); /* clk high */
		udelay(1);
		bit >>= 1;
	}
	gpio_set_value(spi_mosi, 0);
}

static void auo_spi_read_byte(u16 cmd_16, u8 *data)
{
	int bnum;
	u8 cmd_hi = (u8)(cmd_16 >> 8);
	u8 cmd_low = (u8)(cmd_16);

	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(2);

	/* command byte first */
	auo_spi_write_byte(0x20);
	udelay(2);
	auo_spi_write_byte(cmd_hi);
	udelay(2);
	auo_spi_write_byte(0x00);
	udelay(2);
	auo_spi_write_byte(cmd_low);
	udelay(2);
	auo_spi_write_byte(0xc0);
	udelay(2);

	gpio_direction_input(spi_mosi);

	/* followed by data bytes */
	bnum = 1 * 8;	/* number of bits */
	*data = 0;
	while (bnum) {
		gpio_set_value(spi_sclk, 0); /* clk low */
		udelay(1);
		*data <<= 1;
		*data |= gpio_get_value(spi_mosi) ? 1 : 0;
		gpio_set_value(spi_sclk, 1); /* clk high */
		udelay(1);
		--bnum;
		if ((bnum % 8) == 0)
			++data;
	}

	gpio_direction_output(spi_mosi, 0);

	/* Chip Select - high */
	udelay(2);
	gpio_set_value(spi_cs, 1);
}
#endif

static int auo_serigo(u8 *input_data, int input_len)
{
#ifdef CONFIG_SPI_QUP
	int                 rc;
	struct spi_message  m;
	struct spi_transfer t;

	if (!lcdc_spi_client) {
		pr_err("%s lcdc_spi_client is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&t, 0, sizeof t);

	t.tx_buf = input_data;
	t.len = input_len;
	t.bits_per_word = 16;

	spi_setup(lcdc_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	rc = spi_sync(lcdc_spi_client, &m);

	return rc;
#else
	int i;

	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(2);

	for (i = 0; i < input_len; ++i) {
		auo_spi_write_byte(input_data[i]);
		udelay(2);
	}

	/* Chip Select - high */
	gpio_set_value(spi_cs, 1);

	return 0;
#endif
}

#ifndef CONFIG_SPI_QUP
static void auo_spi_init(void)
{
	spi_sclk = *(lcdc_auo_pdata->gpio_num);
	spi_cs   = *(lcdc_auo_pdata->gpio_num + 1);
	spi_mosi = *(lcdc_auo_pdata->gpio_num + 2);

	/* Set the output so that we don't disturb the slave device */
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_mosi, 0);

	/* Set the Chip Select deasserted (active low) */
	gpio_set_value(spi_cs, 1);
}
#endif

static struct work_struct disp_on_delayed_work;
static void auo_write_cmd(u16  cmd)
{
	u8  local_data[4];

	local_data[0] = 0x20;
	local_data[1] = (u8)(cmd >> 8);
	local_data[2] = 0;
	local_data[3] = (u8)cmd;
	auo_serigo(local_data, 4);
}
static void auo_write_cmd_1param(u16  cmd, u8  para1)
{
	u8  local_data[6];

	local_data[0] = 0x20;
	local_data[1] = (u8)(cmd >> 8);
	local_data[2] = 0;
	local_data[3] = (u8)cmd;
	local_data[4] = 0x40;
	local_data[5] = para1;
	auo_serigo(local_data, 6);
}
static void lcdc_auo_set_backlight(struct msm_fb_data_type *mfd)
{
	int bl_level;

	bl_level = mfd->bl_level;
	if (auo_state.display_on) {
		auo_write_cmd_1param(PANEL_CMD_BACKLIGHT_LEVEL,
			bl_level * 255 / MAX_BACKLIGHT_LEVEL);
		auo_state.bl_level = bl_level;
	}

}
static void auo_disp_on_delayed_work(struct work_struct *work_ptr)
{
	/* 0x1100: Sleep Out */
	auo_write_cmd(PANEL_CMD_SLEEP_OUT);

	msleep(180);

	/* SET_PIXEL_FORMAT: Set how many bits per pixel are used (3A00h)*/
	auo_write_cmd_1param(PANEL_CMD_FORMAT, 0x66); /* 18 bits */

	/* RGBCTRL: RGB Interface Signal Control (3B00h) */
	auo_write_cmd_1param(PANEL_CMD_RGBCTRL, 0x2B);

	/* Display ON command */
	auo_write_cmd(PANEL_CMD_DISP_ON);
	msleep(20);

	/*Backlight on */
	auo_write_cmd_1param(PANEL_CMD_BCTRL, 0x24); /*BCTRL, BL */
	auo_write_cmd_1param(PANEL_CMD_PWM_EN, 0x01); /*Enable PWM Level */

	msleep(20);
}

static void auo_disp_on(void)
{
	if (!auo_state.display_on) {
		INIT_WORK(&disp_on_delayed_work, auo_disp_on_delayed_work);
#ifdef CONFIG_SPI_QUP
		if (lcdc_spi_client)
#endif
			schedule_work(&disp_on_delayed_work);
		auo_state.display_on = TRUE;
	}
}

static int lcdc_auo_panel_on(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	if (!auo_state.display_on) {
#ifndef CONFIG_SPI_QUP
		lcdc_auo_pdata->panel_config_gpio(1);
		auo_spi_init();
#endif
		auo_disp_on();
	}
	return 0;
}

static int lcdc_auo_panel_off(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	if (auo_state.display_on) {
		/* 0x2800: Display Off */
		auo_write_cmd(PANEL_CMD_DISP_OFF);
		msleep(120);
		/* 0x1000: Sleep In */
		auo_write_cmd(PANEL_CMD_SLEEP_IN);
		msleep(120);

		auo_state.display_on = FALSE;
	}
	return 0;
}

static int auo_probe(struct platform_device *pdev)
{
	pr_info("%s: id=%d\n", __func__, pdev->id);
	if (pdev->id == 0) {
		lcdc_auo_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}

#ifdef CONFIG_SPI_QUP
static int __devinit lcdc_auo_spi_probe(struct spi_device *spi)
{
	pr_info("%s\n", __func__);
	lcdc_spi_client = spi;
	lcdc_spi_client->bits_per_word = 32;
	if (auo_state.display_on)
		schedule_work(&disp_on_delayed_work);
	return 0;
}
static int __devexit lcdc_auo_spi_remove(struct spi_device *spi)
{
	lcdc_spi_client = NULL;
	return 0;
}
static struct spi_driver lcdc_auo_spi_driver = {
	.driver.name   = LCDC_AUO_SPI_DEVICE_NAME,
	.driver.owner  = THIS_MODULE,
	.probe         = lcdc_auo_spi_probe,
	.remove        = __devexit_p(lcdc_auo_spi_remove),
};
#endif

static struct platform_driver this_driver = {
	.probe		= auo_probe,
	.driver.name	= LCDC_AUO_PANEL_NAME,
};

static struct msm_fb_panel_data auo_panel_data = {
	.on = lcdc_auo_panel_on,
	.off = lcdc_auo_panel_off,
	.set_backlight = lcdc_auo_set_backlight,
};

static struct platform_device this_device = {
	.name	= LCDC_AUO_PANEL_NAME,
	.id	= 1,
	.dev.platform_data = &auo_panel_data,
};

static int __init lcdc_auo_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	if (msm_fb_detect_client(LCDC_AUO_PANEL_NAME)) {
		pr_err("%s: detect failed\n", __func__);
		return 0;
	}

	ret = platform_driver_register(&this_driver);
	if (ret) {
		pr_err("%s: driver register failed, rc=%d\n", __func__, ret);
		return ret;
	}

	pinfo = &auo_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 25600000;
	pinfo->bl_max = MAX_BACKLIGHT_LEVEL;
	pinfo->bl_min = 1;

	pinfo->lcdc.h_back_porch = 16-2;	/* HBP-HLW */
	pinfo->lcdc.h_front_porch = 16;
	pinfo->lcdc.h_pulse_width = 2;

	pinfo->lcdc.v_back_porch = 3-2;		/* VBP-VLW */
	pinfo->lcdc.v_front_porch = 28;
	pinfo->lcdc.v_pulse_width = 2;

	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;

	ret = platform_device_register(&this_device);
	if (ret) {
		pr_err("%s: device register failed, rc=%d\n", __func__, ret);
		goto fail_driver;
	}
#ifdef CONFIG_SPI_QUP
	ret = spi_register_driver(&lcdc_auo_spi_driver);

	if (ret) {
		pr_err("%s: spi register failed: rc=%d\n", __func__, ret);
		goto fail_device;
	}
	pr_info("%s: SUCCESS (SPI)\n", __func__);
#else
	pr_info("%s: SUCCESS (BitBang)\n", __func__);
#endif
	return ret;

#ifdef CONFIG_SPI_QUP
fail_device:
	platform_device_unregister(&this_device);
#endif
fail_driver:
	platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lcdc_auo_panel_init);
static void __exit lcdc_auo_panel_exit(void)
{
	pr_info("%s\n", __func__);
	platform_device_unregister(&this_device);
	platform_driver_unregister(&this_driver);
#ifdef CONFIG_SPI_QUP
	spi_unregister_driver(&lcdc_auo_spi_driver);
#endif
}
module_exit(lcdc_auo_panel_exit);
