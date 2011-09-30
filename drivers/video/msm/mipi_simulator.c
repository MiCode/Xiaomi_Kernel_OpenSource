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
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_simulator.h"

static struct dsi_buf simulator_tx_buf;
static struct dsi_buf simulator_rx_buf;
static struct msm_panel_common_pdata *mipi_simulator_pdata;

static int mipi_simulator_lcd_init(void);

static char display_on[2]  = {0x00, 0x00};
static char display_off[2] = {0x00, 0x00};

static struct dsi_cmd_desc display_on_cmds[] = {
		{DTYPE_PERIPHERAL_ON, 1, 0, 0, 0, sizeof(display_on),
				display_on}
};
static struct dsi_cmd_desc display_off_cmds[] = {
		{DTYPE_PERIPHERAL_OFF, 1, 0, 0, 0, sizeof(display_off),
				display_off}
};

static int mipi_simulator_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	mipi  = &mfd->panel_info.mipi;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pr_debug("%s:%d, debug info (mode) : %d", __func__, __LINE__,
		 mipi->mode);

	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_dsi_cmds_tx(mfd, &simulator_tx_buf, display_on_cmds,
			ARRAY_SIZE(display_on_cmds));
	} else {
		pr_err("%s:%d, CMD MODE NOT SUPPORTED", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int mipi_simulator_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	mipi  = &mfd->panel_info.mipi;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pr_debug("%s:%d, debug info", __func__, __LINE__);

	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_dsi_cmds_tx(mfd, &simulator_tx_buf, display_off_cmds,
			ARRAY_SIZE(display_off_cmds));
	} else {
		pr_debug("%s:%d, DONT REACH HERE", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int __devinit mipi_simulator_lcd_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		mipi_simulator_pdata = pdev->dev.platform_data;
		return 0;
	}
	pr_debug("%s:%d, debug info", __func__, __LINE__);

	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_simulator_lcd_probe,
	.driver = {
		.name   = "mipi_simulator",
	},
};

static struct msm_fb_panel_data simulator_panel_data = {
	.on		= mipi_simulator_lcd_on,
	.off		= mipi_simulator_lcd_off,
};

static int ch_used[3];

int mipi_simulator_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pr_debug("%s:%d, debug info", __func__, __LINE__);
	ret = mipi_simulator_lcd_init();
	if (ret) {
		pr_err("mipi_simulator_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_simulator", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	simulator_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &simulator_panel_data,
		sizeof(simulator_panel_data));
	if (ret) {
		pr_err(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_simulator_lcd_init(void)
{
	mipi_dsi_buf_alloc(&simulator_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&simulator_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
