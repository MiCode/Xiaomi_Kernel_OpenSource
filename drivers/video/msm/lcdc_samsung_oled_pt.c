/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

#define DEBUG
/* #define SYSFS_DEBUG_CMD */

#ifdef CONFIG_SPI_QUP
#define LCDC_SAMSUNG_SPI_DEVICE_NAME	"lcdc_samsung_ams367pe02"
static struct spi_device *lcdc_spi_client;
#else
static int spi_cs;
static int spi_sclk;
static int spi_mosi;
#endif

struct samsung_state_type {
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
	int brightness;
};

struct samsung_spi_data {
	u8 addr;
	u8 len;
	u8 data[22];
};

static struct samsung_spi_data panel_sequence[] = {
	{ .addr = 0xf8, .len = 14, .data = { 0x01, 0x27, 0x27, 0x07, 0x07,
	 0x54, 0x9f, 0x63, 0x86, 0x1a, 0x33, 0x0d, 0x00, 0x00 } },
};
static struct samsung_spi_data display_sequence[] = {
	{ .addr = 0xf2, .len = 5, .data = { 0x02, 0x03, 0x1c, 0x10, 0x10 } },
	{ .addr = 0xf7, .len = 3, .data = { 0x00, 0x00, 0x30 } },
};

/* lum=300 cd/m2 */
static struct samsung_spi_data gamma_sequence_300[] = {
	{ .addr = 0xfa, .len = 22, .data = { 0x02, 0x18, 0x08, 0x24, 0x7d, 0x77,
	 0x5b, 0xbe, 0xc1, 0xb1, 0xb3, 0xb7, 0xa6, 0xc3, 0xc5, 0xb9, 0x00, 0xb3,
	 0x00, 0xaf, 0x00, 0xe8 } },
	{ .addr = 0xFA, .len = 1, .data = { 0x03 } },
};
/* lum = 180 cd/m2*/
static struct samsung_spi_data gamma_sequence_180[] = {
	{ .addr = 0xfa, .len = 22, .data = { 0x02, 0x18, 0x08, 0x24, 0x83, 0x78,
	 0x60, 0xc5, 0xc6, 0xb8, 0xba, 0xbe, 0xad, 0xcb, 0xcd, 0xc2, 0x00, 0x92,
	 0x00, 0x8e, 0x00, 0xbc } },
	{ .addr = 0xFA, .len = 1, .data = { 0x03 } },
};
/* lum = 80 cd/m2*/
static struct samsung_spi_data gamma_sequence_80[] = {
	{ .addr = 0xfa, .len = 22, .data = { 0x02, 0x18, 0x08, 0x24, 0x94, 0x73,
	 0x6c, 0xcb, 0xca, 0xbe, 0xc4, 0xc7, 0xb8, 0xd3, 0xd5, 0xcb, 0x00, 0x6d,
	 0x00, 0x69, 0x00, 0x8b } },
	{ .addr = 0xFA, .len = 1, .data = { 0x03 } },
};

static struct samsung_spi_data etc_sequence[] = {
	{ .addr = 0xF6, .len = 3, .data = { 0x00, 0x8e, 0x07 } },
	{ .addr = 0xB3, .len = 1, .data = { 0x0C } },
};

static struct samsung_state_type samsung_state = { .brightness = 180 };
static struct msm_panel_common_pdata *lcdc_samsung_pdata;

#ifndef CONFIG_SPI_QUP
static void samsung_spi_write_byte(boolean dc, u8 data)
{
	uint32 bit;
	int bnum;

	gpio_set_value(spi_sclk, 0);
	gpio_set_value(spi_mosi, dc ? 1 : 0);
	udelay(1);			/* at least 20 ns */
	gpio_set_value(spi_sclk, 1);	/* clk high */
	udelay(1);			/* at least 20 ns */

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

static void samsung_spi_read_bytes(u8 cmd, u8 *data, int num)
{
	int bnum;

	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(2);

	/* command byte first */
	samsung_spi_write_byte(0, cmd);
	udelay(2);

	gpio_direction_input(spi_mosi);

	if (num > 1) {
		/* extra dummy clock */
		gpio_set_value(spi_sclk, 0);
		udelay(1);
		gpio_set_value(spi_sclk, 1);
		udelay(1);
	}

	/* followed by data bytes */
	bnum = num * 8;	/* number of bits */
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

#ifdef DEBUG
static const char *byte_to_binary(const u8 *buf, int len)
{
	static char b[32*8+1];
	char *p = b;
	int i, z;

	for (i = 0; i < len; ++i) {
		u8 val = *buf++;
		for (z = 1 << 7; z > 0; z >>= 1)
			*p++ = (val & z) ? '1' : '0';
	}
	*p = 0;

	return b;
}
#endif

#define BIT_OFFSET	(bit_size % 8)
#define ADD_BIT(val) do { \
		tx_buf[bit_size / 8] |= \
			(u8)((val ? 1 : 0) << (7 - BIT_OFFSET)); \
		++bit_size; \
	} while (0)

#define ADD_BYTE(data) do { \
		tx_buf[bit_size / 8] |= (u8)(data >> BIT_OFFSET); \
		bit_size += 8; \
		if (BIT_OFFSET != 0) \
			tx_buf[bit_size / 8] |= (u8)(data << (8 - BIT_OFFSET));\
	} while (0)

static int samsung_serigo(struct samsung_spi_data data)
{
#ifdef CONFIG_SPI_QUP
	char                tx_buf[32];
	int                 bit_size = 0, i, rc;
	struct spi_message  m;
	struct spi_transfer t;

	if (!lcdc_spi_client) {
		pr_err("%s lcdc_spi_client is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	spi_setup(lcdc_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ADD_BIT(FALSE);
	ADD_BYTE(data.addr);
	for (i = 0; i < data.len; ++i) {
		ADD_BIT(TRUE);
		ADD_BYTE(data.data[i]);
	}

	/* add padding bits so we round to next byte */
	t.len = (bit_size+7) / 8;
	if (t.len <= 4)
		t.bits_per_word = bit_size;

	rc = spi_sync(lcdc_spi_client, &m);
#ifdef DEBUG
	pr_info("%s: addr=0x%02x, #args=%d[%d] [%s], rc=%d\n",
		__func__, data.addr, t.len, t.bits_per_word,
		byte_to_binary(tx_buf, t.len), rc);
#endif
	return rc;
#else
	int i;

	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(2);

	samsung_spi_write_byte(FALSE, data.addr);
	udelay(2);

	for (i = 0; i < data.len; ++i) {
		samsung_spi_write_byte(TRUE, data.data[i]);
		udelay(2);
	}

	/* Chip Select - high */
	gpio_set_value(spi_cs, 1);
#ifdef DEBUG
	pr_info("%s: cmd=0x%02x, #args=%d\n", __func__, data.addr, data.len);
#endif
	return 0;
#endif
}

static int samsung_write_cmd(u8 cmd)
{
#ifdef CONFIG_SPI_QUP
	char                tx_buf[2];
	int                 bit_size = 0, rc;
	struct spi_message  m;
	struct spi_transfer t;

	if (!lcdc_spi_client) {
		pr_err("%s lcdc_spi_client is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	spi_setup(lcdc_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ADD_BIT(FALSE);
	ADD_BYTE(cmd);

	t.len = 2;
	t.bits_per_word = 9;

	rc = spi_sync(lcdc_spi_client, &m);
#ifdef DEBUG
	pr_info("%s: addr=0x%02x, #args=%d[%d] [%s], rc=%d\n",
		__func__, cmd, t.len, t.bits_per_word,
		byte_to_binary(tx_buf, t.len), rc);
#endif
	return rc;
#else
	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(2);

	samsung_spi_write_byte(FALSE, cmd);

	/* Chip Select - high */
	udelay(2);
	gpio_set_value(spi_cs, 1);
#ifdef DEBUG
	pr_info("%s: cmd=0x%02x\n", __func__, cmd);
#endif
	return 0;
#endif
}

static int samsung_serigo_list(struct samsung_spi_data *data, int count)
{
	int i, rc;
	for (i = 0; i < count; ++i, ++data) {
		rc = samsung_serigo(*data);
		if (rc)
			return rc;
		msleep(10);
	}
	return 0;
}

#ifndef CONFIG_SPI_QUP
static void samsung_spi_init(void)
{
	spi_sclk = *(lcdc_samsung_pdata->gpio_num);
	spi_cs   = *(lcdc_samsung_pdata->gpio_num + 1);
	spi_mosi = *(lcdc_samsung_pdata->gpio_num + 2);

	/* Set the output so that we don't disturb the slave device */
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_mosi, 0);

	/* Set the Chip Select deasserted (active low) */
	gpio_set_value(spi_cs, 1);
}
#endif

static void samsung_disp_powerup(void)
{
	if (!samsung_state.disp_powered_up && !samsung_state.display_on)
		samsung_state.disp_powered_up = TRUE;
}

static struct work_struct disp_on_delayed_work;
static void samsung_disp_on_delayed_work(struct work_struct *work_ptr)
{
	/* 0x01: Software Reset */
	samsung_write_cmd(0x01);
	msleep(120);

	msleep(300);
	samsung_serigo_list(panel_sequence,
		sizeof(panel_sequence)/sizeof(*panel_sequence));
	samsung_serigo_list(display_sequence,
		sizeof(display_sequence)/sizeof(*display_sequence));

	switch (samsung_state.brightness) {
	case 300:
		samsung_serigo_list(gamma_sequence_300,
			sizeof(gamma_sequence_300)/sizeof(*gamma_sequence_300));
		break;
	case 180:
	default:
		samsung_serigo_list(gamma_sequence_180,
			sizeof(gamma_sequence_180)/sizeof(*gamma_sequence_180));
		break;
	case 80:
		samsung_serigo_list(gamma_sequence_80,
			sizeof(gamma_sequence_80)/sizeof(*gamma_sequence_80));
		break;
	}

	samsung_serigo_list(etc_sequence,
		sizeof(etc_sequence)/sizeof(*etc_sequence));

	/* 0x11: Sleep Out */
	samsung_write_cmd(0x11);
	msleep(120);
	/* 0x13: Normal Mode On */
	samsung_write_cmd(0x13);

#ifndef CONFIG_SPI_QUP
	{
		u8 data;

		msleep(120);
		/* 0x0A: Read Display Power Mode */
		samsung_spi_read_bytes(0x0A, &data, 1);
		pr_info("%s: power=[%s]\n", __func__,
			byte_to_binary(&data, 1));

		msleep(120);
		/* 0x0C: Read Display Pixel Format */
		samsung_spi_read_bytes(0x0C, &data, 1);
		pr_info("%s: pixel-format=[%s]\n", __func__,
			byte_to_binary(&data, 1));
	}
#endif
	msleep(120);
	/* 0x29: Display On */
	samsung_write_cmd(0x29);
}

static void samsung_disp_on(void)
{
	if (samsung_state.disp_powered_up && !samsung_state.display_on) {
		INIT_WORK(&disp_on_delayed_work, samsung_disp_on_delayed_work);
		schedule_work(&disp_on_delayed_work);

		samsung_state.display_on = TRUE;
	}
}

static int lcdc_samsung_panel_on(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	if (!samsung_state.disp_initialized) {
#ifndef CONFIG_SPI_QUP
		lcdc_samsung_pdata->panel_config_gpio(1);
		samsung_spi_init();
#endif
		samsung_disp_powerup();
		samsung_disp_on();
		samsung_state.disp_initialized = TRUE;
	}
	return 0;
}

static int lcdc_samsung_panel_off(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	if (samsung_state.disp_powered_up && samsung_state.display_on) {
		/* 0x10: Sleep In */
		samsung_write_cmd(0x10);
		msleep(120);

		samsung_state.display_on = FALSE;
		samsung_state.disp_initialized = FALSE;
	}
	return 0;
}

#ifdef SYSFS_DEBUG_CMD
static ssize_t samsung_rda_cmd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "n/a\n");
	pr_info("%s: 'n/a'\n", __func__);
	return ret;
}

static ssize_t samsung_wta_cmd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	uint32 cmd;

	sscanf(buf, "%x", &cmd);
	samsung_write_cmd((u8)cmd);

	return ret;
}

static DEVICE_ATTR(cmd, S_IRUGO | S_IWUGO, samsung_rda_cmd, samsung_wta_cmd);
static struct attribute *fs_attrs[] = {
	&dev_attr_cmd.attr,
	NULL,
};
static struct attribute_group fs_attr_group = {
	.attrs = fs_attrs,
};
#endif

static struct msm_fb_panel_data samsung_panel_data = {
	.on = lcdc_samsung_panel_on,
	.off = lcdc_samsung_panel_off,
};

static int __devinit samsung_probe(struct platform_device *pdev)
{
	struct msm_panel_info *pinfo;
#ifdef SYSFS_DEBUG_CMD
	struct platform_device *fb_dev;
	struct msm_fb_data_type *mfd;
	int rc;
#endif

	pr_info("%s: id=%d\n", __func__, pdev->id);
	lcdc_samsung_pdata = pdev->dev.platform_data;

	pinfo = &samsung_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 25600000; /* Max 27.77MHz */
	pinfo->bl_max = 15;
	pinfo->bl_min = 1;

	/* AMS367PE02 Operation Manual, Page 7 */
	pinfo->lcdc.h_back_porch = 16-2;	/* HBP-HLW */
	pinfo->lcdc.h_front_porch = 16;
	pinfo->lcdc.h_pulse_width = 2;
	/* AMS367PE02 Operation Manual, Page 6 */
	pinfo->lcdc.v_back_porch = 3-2;		/* VBP-VLW */
	pinfo->lcdc.v_front_porch = 28;
	pinfo->lcdc.v_pulse_width = 2;

	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;
	pdev->dev.platform_data = &samsung_panel_data;

#ifndef SYSFS_DEBUG_CMD
	msm_fb_add_device(pdev);
#else
	fb_dev = msm_fb_add_device(pdev);
	mfd = platform_get_drvdata(fb_dev);
	rc = sysfs_create_group(&mfd->fbi->dev->kobj, &fs_attr_group);
	if (rc) {
		pr_err("%s: sysfs group creation failed, rc=%d\n", __func__,
			rc);
		return rc;
	}
#endif
	return 0;
}

#ifdef CONFIG_SPI_QUP
static int __devinit lcdc_samsung_spi_probe(struct spi_device *spi)
{
	pr_info("%s\n", __func__);
	lcdc_spi_client = spi;
	lcdc_spi_client->bits_per_word = 32;
	return 0;
}
static int __devexit lcdc_samsung_spi_remove(struct spi_device *spi)
{
	lcdc_spi_client = NULL;
	return 0;
}
static struct spi_driver lcdc_samsung_spi_driver = {
	.driver.name   = LCDC_SAMSUNG_SPI_DEVICE_NAME,
	.driver.owner  = THIS_MODULE,
	.probe         = lcdc_samsung_spi_probe,
	.remove        = __devexit_p(lcdc_samsung_spi_remove),
};
#endif

static struct platform_driver this_driver = {
	.probe		= samsung_probe,
	.driver.name	= "lcdc_samsung_oled",
};

static int __init lcdc_samsung_panel_init(void)
{
	int ret;

#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
	if (msm_fb_detect_client("lcdc_samsung_oled")) {
		pr_err("%s: detect failed\n", __func__);
		return 0;
	}
#endif

	ret = platform_driver_register(&this_driver);
	if (ret) {
		pr_err("%s: driver register failed, rc=%d\n", __func__, ret);
		return ret;
	}

#ifdef CONFIG_SPI_QUP
	ret = spi_register_driver(&lcdc_samsung_spi_driver);

	if (ret) {
		pr_err("%s: spi register failed: rc=%d\n", __func__, ret);
		platform_driver_unregister(&this_driver);
	} else
		pr_info("%s: SUCCESS (SPI)\n", __func__);
#else
	pr_info("%s: SUCCESS (BitBang)\n", __func__);
#endif
	return ret;
}

module_init(lcdc_samsung_panel_init);
static void __exit lcdc_samsung_panel_exit(void)
{
	pr_info("%s\n", __func__);
#ifdef CONFIG_SPI_QUP
	spi_unregister_driver(&lcdc_samsung_spi_driver);
#endif
	platform_driver_unregister(&this_driver);
}
module_exit(lcdc_samsung_panel_exit);
