/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/module.h>
#ifdef CONFIG_SPI_QUP
#include <linux/spi/spi.h>
#endif
#include <mach/gpio.h>
#include <mach/pmic.h>
#include "msm_fb.h"

#define LCDC_NT35582_PANEL_NAME		"lcdc_nt35582_wvga"

#define WRITE_FIRST_TRANS	0x20
#define WRITE_SECOND_TRANS	0x00
#define WRITE_THIRD_TRANS	0x40
#define READ_FIRST_TRANS	0x20
#define READ_SECOND_TRANS	0x00
#define READ_THIRD_TRANS	0xC0

#ifdef CONFIG_SPI_QUP
#define LCDC_NT35582_SPI_DEVICE_NAME		"lcdc_nt35582_spi"
static struct spi_device *spi_client;
#endif

struct nt35582_state_type {
	boolean display_on;
	int bl_level;
};

static struct nt35582_state_type nt35582_state = { 0 };
static int gpio_backlight_en;
static struct msm_panel_common_pdata *lcdc_nt35582_pdata;

static int spi_write_2bytes(struct spi_device *spi,
	unsigned char reg_high_addr, unsigned char reg_low_addr)
{
	char tx_buf[4];
	int rc;
	struct spi_message m;
	struct spi_transfer t;

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;

	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = WRITE_FIRST_TRANS;
	tx_buf[1] = reg_high_addr;
	tx_buf[2] = WRITE_SECOND_TRANS;
	tx_buf[3] = reg_low_addr;
	t.rx_buf = NULL;
	t.len = 4;
	t.bits_per_word = 16;
	rc = spi_sync(spi, &m);
	if (rc)
		pr_err("write spi command failed!\n");

	return rc;
}

static int spi_write_3bytes(struct spi_device *spi, unsigned char reg_high_addr,
	unsigned char reg_low_addr, unsigned char write_data)
{
	char tx_buf[6];
	int rc;
	struct spi_message  m;
	struct spi_transfer t;

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;

	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = WRITE_FIRST_TRANS;
	tx_buf[1] = reg_high_addr;
	tx_buf[2] = WRITE_SECOND_TRANS;
	tx_buf[3] = reg_low_addr;
	tx_buf[4] = WRITE_THIRD_TRANS;
	tx_buf[5] = write_data;
	t.rx_buf = NULL;
	t.len = 6;
	t.bits_per_word = 16;
	rc = spi_sync(spi, &m);

	if (rc)
		pr_err("write spi command failed!\n");

	return rc;
}

static int spi_read_bytes(struct spi_device *spi, unsigned char reg_high_addr,
	unsigned char reg_low_addr, unsigned char *read_value)
{
	char tx_buf[6];
	char rx_buf[6];
	int rc;
	struct spi_message m;
	struct spi_transfer t;

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;

	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = READ_FIRST_TRANS;
	tx_buf[1] = reg_high_addr;
	tx_buf[2] = READ_SECOND_TRANS;
	tx_buf[3] = reg_low_addr;
	tx_buf[4] = READ_THIRD_TRANS;
	tx_buf[5] = 0x00;

	t.rx_buf = rx_buf;
	t.len = 6;
	t.bits_per_word = 16;
	rc = spi_sync(spi, &m);

	if (rc)
		pr_err("write spi command failed!\n");
	else
		*read_value = rx_buf[5];

	return rc;
}

static void nt35582_disp_on(void)
{
	uint32 panel_id1 = 0, panel_id2 = 0;

	if (!nt35582_state.display_on) {

		/* GVDD setting */
		spi_write_3bytes(spi_client, 0xC0, 0x00, 0xC0);
		spi_write_3bytes(spi_client, 0xC0, 0x01, 0x00);
		spi_write_3bytes(spi_client, 0xC0, 0x02, 0xC0);
		spi_write_3bytes(spi_client, 0xC0, 0x03, 0x00);
		/* Power setting */
		spi_write_3bytes(spi_client, 0xC1, 0x00, 0x40);
		spi_write_3bytes(spi_client, 0xC2, 0x00, 0x21);
		spi_write_3bytes(spi_client, 0xC2, 0x02, 0x02);

		/* Gamma setting */
		spi_write_3bytes(spi_client, 0xE0, 0x00, 0x0E);
		spi_write_3bytes(spi_client, 0xE0, 0x01, 0x54);
		spi_write_3bytes(spi_client, 0xE0, 0x02, 0x63);
		spi_write_3bytes(spi_client, 0xE0, 0x03, 0x76);
		spi_write_3bytes(spi_client, 0xE0, 0x04, 0x1F);
		spi_write_3bytes(spi_client, 0xE0, 0x05, 0x31);
		spi_write_3bytes(spi_client, 0xE0, 0x06, 0x62);
		spi_write_3bytes(spi_client, 0xE0, 0x07, 0x78);
		spi_write_3bytes(spi_client, 0xE0, 0x08, 0x1F);
		spi_write_3bytes(spi_client, 0xE0, 0x09, 0x25);
		spi_write_3bytes(spi_client, 0xE0, 0x0A, 0xB3);
		spi_write_3bytes(spi_client, 0xE0, 0x0B, 0x17);
		spi_write_3bytes(spi_client, 0xE0, 0x0C, 0x38);
		spi_write_3bytes(spi_client, 0xE0, 0x0D, 0x5A);
		spi_write_3bytes(spi_client, 0xE0, 0x0E, 0xA2);
		spi_write_3bytes(spi_client, 0xE0, 0x0F, 0xA2);
		spi_write_3bytes(spi_client, 0xE0, 0x10, 0x24);
		spi_write_3bytes(spi_client, 0xE0, 0x11, 0x57);

		spi_write_3bytes(spi_client, 0xE1, 0x00, 0x0E);
		spi_write_3bytes(spi_client, 0xE1, 0x01, 0x54);
		spi_write_3bytes(spi_client, 0xE1, 0x02, 0x63);
		spi_write_3bytes(spi_client, 0xE1, 0x03, 0x76);
		spi_write_3bytes(spi_client, 0xE1, 0x04, 0x1F);
		spi_write_3bytes(spi_client, 0xE1, 0x05, 0x31);
		spi_write_3bytes(spi_client, 0xE1, 0x06, 0X62);
		spi_write_3bytes(spi_client, 0xE1, 0x07, 0x78);
		spi_write_3bytes(spi_client, 0xE1, 0x08, 0x1F);
		spi_write_3bytes(spi_client, 0xE1, 0x09, 0x25);
		spi_write_3bytes(spi_client, 0xE1, 0x0A, 0xB3);
		spi_write_3bytes(spi_client, 0xE1, 0x0B, 0x17);
		spi_write_3bytes(spi_client, 0xE1, 0x0C, 0x38);
		spi_write_3bytes(spi_client, 0xE1, 0x0D, 0x5A);
		spi_write_3bytes(spi_client, 0xE1, 0x0E, 0xA2);
		spi_write_3bytes(spi_client, 0xE1, 0x0F, 0xA2);
		spi_write_3bytes(spi_client, 0xE1, 0x10, 0x24);
		spi_write_3bytes(spi_client, 0xE1, 0x11, 0x57);

		spi_write_3bytes(spi_client, 0xE2, 0x00, 0x0E);
		spi_write_3bytes(spi_client, 0xE2, 0x01, 0x54);
		spi_write_3bytes(spi_client, 0xE2, 0x02, 0x63);
		spi_write_3bytes(spi_client, 0xE2, 0x03, 0x76);
		spi_write_3bytes(spi_client, 0xE2, 0x04, 0x1F);
		spi_write_3bytes(spi_client, 0xE2, 0x05, 0x31);
		spi_write_3bytes(spi_client, 0xE2, 0x06, 0x62);
		spi_write_3bytes(spi_client, 0xE2, 0x07, 0x78);
		spi_write_3bytes(spi_client, 0xE2, 0x08, 0x1F);
		spi_write_3bytes(spi_client, 0xE2, 0x09, 0x25);
		spi_write_3bytes(spi_client, 0xE2, 0x0A, 0xB3);
		spi_write_3bytes(spi_client, 0xE2, 0x0B, 0x17);
		spi_write_3bytes(spi_client, 0xE2, 0x0C, 0x38);
		spi_write_3bytes(spi_client, 0xE2, 0x0D, 0x5A);
		spi_write_3bytes(spi_client, 0xE2, 0x0E, 0xA2);
		spi_write_3bytes(spi_client, 0xE2, 0x0F, 0xA2);
		spi_write_3bytes(spi_client, 0xE2, 0x10, 0x24);
		spi_write_3bytes(spi_client, 0xE2, 0x11, 0x57);

		spi_write_3bytes(spi_client, 0xE3, 0x00, 0x0E);
		spi_write_3bytes(spi_client, 0xE3, 0x01, 0x54);
		spi_write_3bytes(spi_client, 0xE3, 0x02, 0x63);
		spi_write_3bytes(spi_client, 0xE3, 0x03, 0x76);
		spi_write_3bytes(spi_client, 0xE3, 0x04, 0x1F);
		spi_write_3bytes(spi_client, 0xE3, 0x05, 0x31);
		spi_write_3bytes(spi_client, 0xE3, 0x06, 0x62);
		spi_write_3bytes(spi_client, 0xE3, 0x07, 0x78);
		spi_write_3bytes(spi_client, 0xE3, 0x08, 0x1F);
		spi_write_3bytes(spi_client, 0xE3, 0x09, 0x25);
		spi_write_3bytes(spi_client, 0xE3, 0x0A, 0xB3);
		spi_write_3bytes(spi_client, 0xE3, 0x0B, 0x17);
		spi_write_3bytes(spi_client, 0xE3, 0x0C, 0x38);
		spi_write_3bytes(spi_client, 0xE3, 0x0D, 0x5A);
		spi_write_3bytes(spi_client, 0xE3, 0x0E, 0xA2);
		spi_write_3bytes(spi_client, 0xE3, 0x0F, 0xA2);
		spi_write_3bytes(spi_client, 0xE3, 0x10, 0x24);
		spi_write_3bytes(spi_client, 0xE3, 0x11, 0x57);

		spi_write_3bytes(spi_client, 0xE4, 0x00, 0x48);
		spi_write_3bytes(spi_client, 0xE4, 0x01, 0x6B);
		spi_write_3bytes(spi_client, 0xE4, 0x02, 0x84);
		spi_write_3bytes(spi_client, 0xE4, 0x03, 0x9B);
		spi_write_3bytes(spi_client, 0xE4, 0x04, 0x1F);
		spi_write_3bytes(spi_client, 0xE4, 0x05, 0x31);
		spi_write_3bytes(spi_client, 0xE4, 0x06, 0x62);
		spi_write_3bytes(spi_client, 0xE4, 0x07, 0x78);
		spi_write_3bytes(spi_client, 0xE4, 0x08, 0x1F);
		spi_write_3bytes(spi_client, 0xE4, 0x09, 0x25);
		spi_write_3bytes(spi_client, 0xE4, 0x0A, 0xB3);
		spi_write_3bytes(spi_client, 0xE4, 0x0B, 0x17);
		spi_write_3bytes(spi_client, 0xE4, 0x0C, 0x38);
		spi_write_3bytes(spi_client, 0xE4, 0x0D, 0x5A);
		spi_write_3bytes(spi_client, 0xE4, 0x0E, 0xA2);
		spi_write_3bytes(spi_client, 0xE4, 0x0F, 0xA2);
		spi_write_3bytes(spi_client, 0xE4, 0x10, 0x24);
		spi_write_3bytes(spi_client, 0xE4, 0x11, 0x57);

		spi_write_3bytes(spi_client, 0xE5, 0x00, 0x48);
		spi_write_3bytes(spi_client, 0xE5, 0x01, 0x6B);
		spi_write_3bytes(spi_client, 0xE5, 0x02, 0x84);
		spi_write_3bytes(spi_client, 0xE5, 0x03, 0x9B);
		spi_write_3bytes(spi_client, 0xE5, 0x04, 0x1F);
		spi_write_3bytes(spi_client, 0xE5, 0x05, 0x31);
		spi_write_3bytes(spi_client, 0xE5, 0x06, 0x62);
		spi_write_3bytes(spi_client, 0xE5, 0x07, 0x78);
		spi_write_3bytes(spi_client, 0xE5, 0x08, 0x1F);
		spi_write_3bytes(spi_client, 0xE5, 0x09, 0x25);
		spi_write_3bytes(spi_client, 0xE5, 0x0A, 0xB3);
		spi_write_3bytes(spi_client, 0xE5, 0x0B, 0x17);
		spi_write_3bytes(spi_client, 0xE5, 0x0C, 0x38);
		spi_write_3bytes(spi_client, 0xE5, 0x0D, 0x5A);
		spi_write_3bytes(spi_client, 0xE5, 0x0E, 0xA2);
		spi_write_3bytes(spi_client, 0xE5, 0x0F, 0xA2);
		spi_write_3bytes(spi_client, 0xE5, 0x10, 0x24);
		spi_write_3bytes(spi_client, 0xE5, 0x11, 0x57);

		/* Data format setting */
		spi_write_3bytes(spi_client, 0x3A, 0x00, 0x70);

		/* Reverse PCLK signal of LCM to meet Qualcomm's platform */
		spi_write_3bytes(spi_client, 0x3B, 0x00, 0x2B);

		/* Scan direstion setting */
		spi_write_3bytes(spi_client, 0x36, 0x00, 0x00);

		/* Sleep out */
		spi_write_2bytes(spi_client, 0x11, 0x00);

		msleep(120);

		/* Display on */
		spi_write_2bytes(spi_client, 0x29, 0x00);

		pr_info("%s: LCM SPI display on CMD finished...\n", __func__);

		msleep(200);

		nt35582_state.display_on = TRUE;
	}

	/* Test to read RDDID. It should be 0x0055h and 0x0082h */
	spi_read_bytes(spi_client, 0x10, 0x80, (unsigned char *)&panel_id1);
	spi_read_bytes(spi_client, 0x11, 0x80, (unsigned char *)&panel_id2);

	pr_info(KERN_INFO "nt35582_disp_on: LCM_ID=[0x%x, 0x%x]\n",
		panel_id1, panel_id2);
}

static int lcdc_nt35582_panel_on(struct platform_device *pdev)
{
	nt35582_disp_on();
	return 0;
}

static int lcdc_nt35582_panel_off(struct platform_device *pdev)
{
	nt35582_state.display_on = FALSE;
	return 0;
}

static void lcdc_nt35582_set_backlight(struct msm_fb_data_type *mfd)
{
	int bl_level;
	int i = 0, step = 0;

	bl_level = mfd->bl_level;
	if (bl_level == nt35582_state.bl_level)
		return;
	else
		nt35582_state.bl_level = bl_level;

	if (bl_level == 0) {
		gpio_set_value_cansleep(gpio_backlight_en, 0);
		return;
	}

	/* Level:0~31 mapping to step 32~1 */
	step = 32 - bl_level;
	for (i = 0; i < step; i++) {
		gpio_set_value_cansleep(gpio_backlight_en, 0);
		ndelay(5);
		gpio_set_value_cansleep(gpio_backlight_en, 1);
		ndelay(5);
	}
}

static int __devinit nt35582_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_nt35582_pdata = pdev->dev.platform_data;
		return 0;
	}

	gpio_backlight_en = *(lcdc_nt35582_pdata->gpio_num);

	msm_fb_add_device(pdev);
	return 0;
}

#ifdef CONFIG_SPI_QUP
static int __devinit lcdc_nt35582_spi_probe(struct spi_device *spi)
{
	spi_client = spi;
	spi_client->bits_per_word = 16;
	spi_client->chip_select = 0;
	spi_client->max_speed_hz = 1100000;
	spi_client->mode = SPI_MODE_0;
	spi_setup(spi_client);

	return 0;
}

static int __devexit lcdc_nt35582_spi_remove(struct spi_device *spi)
{
	spi_client = NULL;
	return 0;
}

static struct spi_driver lcdc_nt35582_spi_driver = {
	.driver = {
		.name = LCDC_NT35582_SPI_DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.probe = lcdc_nt35582_spi_probe,
	.remove = __devexit_p(lcdc_nt35582_spi_remove),
};
#endif
static struct platform_driver this_driver = {
	.probe = nt35582_probe,
	.driver = {
		.name = LCDC_NT35582_PANEL_NAME,
	},
};

static struct msm_fb_panel_data nt35582_panel_data = {
	.on = lcdc_nt35582_panel_on,
	.off = lcdc_nt35582_panel_off,
	.set_backlight = lcdc_nt35582_set_backlight,
};

static struct platform_device this_device = {
	.name = LCDC_NT35582_PANEL_NAME,
	.id = 1,
	.dev = {
		.platform_data = &nt35582_panel_data,
	}
};

static int __init lcdc_nt35582_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
	if (msm_fb_detect_client(LCDC_NT35582_PANEL_NAME)) {
		pr_err("detect failed\n");
		return 0;
	}
#endif
	ret = platform_driver_register(&this_driver);
	if (ret) {
		pr_err("Fails to platform_driver_register...\n");
		return ret;
	}

	pinfo = &nt35582_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 25600000;
	pinfo->bl_max = 31;
	pinfo->bl_min = 1;

	pinfo->lcdc.h_back_porch = 10;	/* hsw = 8 + hbp=184 */
	pinfo->lcdc.h_front_porch = 10;
	pinfo->lcdc.h_pulse_width = 2;
	pinfo->lcdc.v_back_porch = 4;	/* vsw=1 + vbp = 2 */
	pinfo->lcdc.v_front_porch = 10;
	pinfo->lcdc.v_pulse_width = 2;
	pinfo->lcdc.border_clr = 0;	/* blk */
	pinfo->lcdc.underflow_clr = 0xff;	/* blue */
	pinfo->lcdc.hsync_skew = 0;

	ret = platform_device_register(&this_device);
	if (ret) {
		pr_err("not able to register the device\n");
		goto fail_driver;
	}
#ifdef CONFIG_SPI_QUP
	ret = spi_register_driver(&lcdc_nt35582_spi_driver);

	if (ret) {
		pr_err("not able to register spi\n");
		goto fail_device;
	}
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

device_initcall(lcdc_nt35582_panel_init);
