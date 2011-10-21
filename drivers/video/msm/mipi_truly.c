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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_truly.h"

static struct msm_panel_common_pdata *mipi_truly_pdata;
static struct dsi_buf truly_tx_buf;
static struct dsi_buf truly_rx_buf;

#define TRULY_CMD_DELAY		0
#define TRULY_SLEEP_OFF_DELAY	150
#define TRULY_DISPLAY_ON_DELAY	150
#define GPIO_TRULY_LCD_RESET	129

static int prev_bl = 17;

static char extend_cmd_enable[4] = {0xB9, 0xFF, 0x83, 0x69};
static char display_setting[16] = {
	0xB2, 0x00, 0x23, 0x62,
	0x62, 0x70, 0x00, 0xFF,
	0x00, 0x00, 0x00, 0x00,
	0x03, 0x03, 0x00, 0x01,
};
static char wave_cycle_setting[6] = {0xB4, 0x00, 0x1D, 0x5F, 0x0E, 0x06};
static char gip_setting[27] = {
	0xD5, 0x00, 0x04, 0x03,
	0x00, 0x01, 0x05, 0x1C,
	0x70, 0x01, 0x03, 0x00,
	0x00, 0x40, 0x06, 0x51,
	0x07, 0x00, 0x00, 0x41,
	0x06, 0x50, 0x07, 0x07,
	0x0F, 0x04, 0x00,
};
static char power_setting[20] = {
	0xB1, 0x01, 0x00, 0x34,
	0x06, 0x00, 0x0F, 0x0F,
	0x2A, 0x32, 0x3F, 0x3F,
	0x07, 0x3A, 0x01, 0xE6,
	0xE6, 0xE6, 0xE6, 0xE6,
};
static char vcom_setting[3] = {0xB6, 0x56, 0x56};
static char pannel_setting[2] = {0xCC, 0x02};
static char gamma_setting[35] = {
	0xE0, 0x00, 0x1D, 0x22,
	0x38, 0x3D, 0x3F, 0x2E,
	0x4A, 0x06, 0x0D, 0x0F,
	0x13, 0x15, 0x13, 0x16,
	0x10, 0x19, 0x00, 0x1D,
	0x22, 0x38, 0x3D, 0x3F,
	0x2E, 0x4A, 0x06, 0x0D,
	0x0F, 0x13, 0x15, 0x13,
	0x16, 0x10, 0x19,
};
static char mipi_setting[14] = {
	0xBA, 0x00, 0xA0, 0xC6,
	0x00, 0x0A, 0x00, 0x10,
	0x30, 0x6F, 0x02, 0x11,
	0x18, 0x40,
};
static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};

static struct dsi_cmd_desc truly_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep}
};

static struct dsi_cmd_desc truly_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(extend_cmd_enable), extend_cmd_enable},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(display_setting), display_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(wave_cycle_setting), wave_cycle_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(gip_setting), gip_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(power_setting), power_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(vcom_setting), vcom_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(pannel_setting), pannel_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(gamma_setting), gamma_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, TRULY_CMD_DELAY,
			sizeof(mipi_setting), mipi_setting},
	{DTYPE_DCS_WRITE, 1, 0, 0, TRULY_SLEEP_OFF_DELAY,
			sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, TRULY_DISPLAY_ON_DELAY,
			sizeof(display_on), display_on},
};

static int mipi_truly_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	msleep(20);
	mipi_dsi_cmds_tx(mfd, &truly_tx_buf, truly_display_on_cmds,
			ARRAY_SIZE(truly_display_on_cmds));

	return 0;
}

static int mipi_truly_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(mfd, &truly_tx_buf, truly_display_off_cmds,
			ARRAY_SIZE(truly_display_off_cmds));

	return 0;
}

#define BL_LEVEL	17
static void mipi_truly_set_backlight(struct msm_fb_data_type *mfd)
{
	int step = 0, i = 0;
	int bl_level = mfd->bl_level;

	/* real backlight level, 1 - max, 16 - min, 17 - off */
	bl_level = BL_LEVEL - bl_level;

	if (bl_level > prev_bl) {
		step = bl_level - prev_bl;
		if (bl_level == BL_LEVEL)
			step--;
	} else if (bl_level < prev_bl) {
		step = bl_level + 16 - prev_bl;
	} else {
		pr_debug("%s: no change\n", __func__);
		return;
	}

	if (bl_level == BL_LEVEL) {
		/* turn off backlight */
		mipi_truly_pdata->pmic_backlight(0);
	} else {
		if (prev_bl == BL_LEVEL) {
			/* turn on backlight */
			mipi_truly_pdata->pmic_backlight(1);
			udelay(30);
		}
		/* adjust backlight level */
		for (i = 0; i < step; i++) {
			mipi_truly_pdata->pmic_backlight(0);
			udelay(1);
			mipi_truly_pdata->pmic_backlight(1);
			udelay(1);
		}
	}
	msleep(20);
	prev_bl = bl_level;

	return;
}

static int __devinit mipi_truly_lcd_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		mipi_truly_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_truly_lcd_probe,
	.driver = {
		.name   = "mipi_truly",
	},
};

static struct msm_fb_panel_data truly_panel_data = {
	.on		= mipi_truly_lcd_on,
	.off		= mipi_truly_lcd_off,
	.set_backlight	= mipi_truly_set_backlight,
};

static int ch_used[3];

int mipi_truly_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_truly", (panel << 8)|channel);

	if (!pdev)
		return -ENOMEM;

	truly_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &truly_panel_data,
				sizeof(truly_panel_data));
	if (ret) {
		pr_err("%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);

	if (ret) {
		pr_err("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int __init mipi_truly_lcd_init(void)
{
	mipi_dsi_buf_alloc(&truly_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&truly_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}

module_init(mipi_truly_lcd_init);
