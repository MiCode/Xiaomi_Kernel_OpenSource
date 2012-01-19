/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#include "mipi_toshiba.h"

static struct pwm_device *bl_lpm;
static struct mipi_dsi_panel_platform_data *mipi_toshiba_pdata;

#define TM_GET_PID(id) (((id) & 0xff00)>>8)

static struct dsi_buf toshiba_tx_buf;
static struct dsi_buf toshiba_rx_buf;
static int mipi_toshiba_lcd_init(void);

#ifdef TOSHIBA_CMDS_UNUSED
static char one_lane[3] = {0xEF, 0x60, 0x62};
static char dmode_wqvga[2] = {0xB3, 0x01};
static char intern_wr_clk1_wqvga[3] = {0xef, 0x2f, 0x22};
static char intern_wr_clk2_wqvga[3] = {0xef, 0x6e, 0x33};
static char hor_addr_2A_wqvga[5] = {0x2A, 0x00, 0x00, 0x00, 0xef};
static char hor_addr_2B_wqvga[5] = {0x2B, 0x00, 0x00, 0x01, 0xaa};
static char if_sel_cmd[2] = {0x53, 0x00};
#endif

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};

static char mcap_off[2] = {0xb2, 0x00};
static char ena_test_reg[3] = {0xEF, 0x01, 0x01};
static char two_lane[3] = {0xEF, 0x60, 0x63};
static char non_burst_sync_pulse[3] = {0xef, 0x61, 0x09};
static char dmode_wvga[2] = {0xB3, 0x00};
static char intern_wr_clk1_wvga[3] = {0xef, 0x2f, 0xcc};
static char intern_wr_clk2_wvga[3] = {0xef, 0x6e, 0xdd};
static char hor_addr_2A_wvga[5] = {0x2A, 0x00, 0x00, 0x01, 0xdf};
static char hor_addr_2B_wvga[5] = {0x2B, 0x00, 0x00, 0x03, 0x55};
static char if_sel_video[2] = {0x53, 0x01};

static struct dsi_cmd_desc toshiba_wvga_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mcap_off), mcap_off},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ena_test_reg), ena_test_reg},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(two_lane), two_lane},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(non_burst_sync_pulse),
					non_burst_sync_pulse},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(dmode_wvga), dmode_wvga},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(intern_wr_clk1_wvga),
					intern_wr_clk1_wvga},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(intern_wr_clk2_wvga),
					intern_wr_clk2_wvga},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hor_addr_2A_wvga),
					hor_addr_2A_wvga},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hor_addr_2B_wvga),
					hor_addr_2B_wvga},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(if_sel_video), if_sel_video},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on}
};

static char mcap_start[2] = {0xb0, 0x04};
static char num_out_pixelform[3] = {0xb3, 0x00, 0x87};
static char dsi_ctrl[3] = {0xb6, 0x30, 0x83};
static char panel_driving[7] = {0xc0, 0x01, 0x00, 0x85, 0x00, 0x00, 0x00};
static char dispV_timing[5] = {0xc1, 0x00, 0x10, 0x00, 0x01};
static char dispCtrl[3] = {0xc3, 0x00, 0x19};
static char test_mode_c4[2] = {0xc4, 0x03};
static char dispH_timing[15] = {
	/* TYPE_DCS_LWRITE */
	0xc5, 0x00, 0x01, 0x05,
	0x04, 0x5e, 0x00, 0x00,
	0x00, 0x00, 0x0b, 0x17,
	0x05, 0x00, 0x00
};
static char test_mode_c6[2] = {0xc6, 0x00};
static char gamma_setA[13] = {
	0xc8, 0x0a, 0x15, 0x18,
	0x1b, 0x1c, 0x0d, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00
};
static char gamma_setB[13] = {
	0xc9, 0x0d, 0x1d, 0x1f,
	0x1f, 0x1f, 0x10, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00
};
static char gamma_setC[13] = {
	0xca, 0x1e, 0x1f, 0x1e,
	0x1d, 0x1d, 0x10, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00
};
static char powerSet_ChrgPmp[5] = {0xd0, 0x02, 0x00, 0xa3, 0xb8};
static char testMode_d1[6] = {0xd1, 0x10, 0x14, 0x53, 0x64, 0x00};
static char powerSet_SrcAmp[3] = {0xd2, 0xb3, 0x00};
static char powerInt_PS[3] = {0xd3, 0x33, 0x03};
static char vreg[2] = {0xd5, 0x00};
static char test_mode_d6[2] = {0xd6, 0x01};
static char timingCtrl_d7[9] = {
	0xd7, 0x09, 0x00, 0x84,
	0x81, 0x61, 0xbc, 0xb5,
	0x05
};
static char timingCtrl_d8[7] = {
	0xd8, 0x04, 0x25, 0x90,
	0x4c, 0x92, 0x00
};
static char timingCtrl_d9[4] = {0xd9, 0x5b, 0x7f, 0x05};
static char white_balance[6] = {0xcb, 0x00, 0x00, 0x00, 0x1c, 0x00};
static char vcs_settings[2] = {0xdd, 0x53};
static char vcom_dc_settings[2] = {0xde, 0x43};
static char testMode_e3[5] = {0xe3, 0x00, 0x00, 0x00, 0x00};
static char testMode_e4[6] = {0xe4, 0x00, 0x00, 0x22, 0xaa, 0x00};
static char testMode_e5[2] = {0xe5, 0x00};
static char testMode_fa[4] = {0xfa, 0x00, 0x00, 0x00};
static char testMode_fd[5] = {0xfd, 0x00, 0x00, 0x00, 0x00};
static char testMode_fe[5] = {0xfe, 0x00, 0x00, 0x00, 0x00};
static char mcap_end[2] = {0xb0, 0x03};
static char set_add_mode[2] = {0x36, 0x0};
static char set_pixel_format[2] = {0x3a, 0x70};


static struct dsi_cmd_desc toshiba_wsvga_display_on_cmds[] = {
	{DTYPE_GEN_WRITE2, 1, 0, 0, 10, sizeof(mcap_start), mcap_start},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 10, sizeof(num_out_pixelform),
		num_out_pixelform},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 10, sizeof(dsi_ctrl), dsi_ctrl},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_driving), panel_driving},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(dispV_timing), dispV_timing},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(dispCtrl), dispCtrl},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(test_mode_c4), test_mode_c4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(dispH_timing), dispH_timing},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(test_mode_c6), test_mode_c6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setA), gamma_setA},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setB), gamma_setB},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setC), gamma_setC},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(powerSet_ChrgPmp),
		powerSet_ChrgPmp},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(testMode_d1), testMode_d1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(powerSet_SrcAmp),
		powerSet_SrcAmp},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(powerInt_PS), powerInt_PS},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(vreg), vreg},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(test_mode_d6), test_mode_d6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(timingCtrl_d7), timingCtrl_d7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(timingCtrl_d8), timingCtrl_d8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(timingCtrl_d9), timingCtrl_d9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(white_balance), white_balance},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(vcs_settings), vcs_settings},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(vcom_dc_settings),
		vcom_dc_settings},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(testMode_e3), testMode_e3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(testMode_e4), testMode_e4},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(testMode_e5), testMode_e5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(testMode_fa), testMode_fa},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(testMode_fd), testMode_fd},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(testMode_fe), testMode_fe},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mcap_end), mcap_end},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_add_mode), set_add_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_pixel_format),
		set_pixel_format},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_on), display_on}
};

static struct dsi_cmd_desc toshiba_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep}
};

static int mipi_toshiba_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (TM_GET_PID(mfd->panel.id) == MIPI_DSI_PANEL_WVGA_PT)
		mipi_dsi_cmds_tx(mfd, &toshiba_tx_buf,
			toshiba_wvga_display_on_cmds,
			ARRAY_SIZE(toshiba_wvga_display_on_cmds));
	else if (TM_GET_PID(mfd->panel.id) == MIPI_DSI_PANEL_WSVGA_PT ||
		TM_GET_PID(mfd->panel.id) == MIPI_DSI_PANEL_WUXGA)
		mipi_dsi_cmds_tx(mfd, &toshiba_tx_buf,
			toshiba_wsvga_display_on_cmds,
			ARRAY_SIZE(toshiba_wsvga_display_on_cmds));
	else
		return -EINVAL;

	return 0;
}

static int mipi_toshiba_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(mfd, &toshiba_tx_buf, toshiba_display_off_cmds,
			ARRAY_SIZE(toshiba_display_off_cmds));

	return 0;
}

void mipi_bklight_pwm_cfg(void)
{
	if (mipi_toshiba_pdata && mipi_toshiba_pdata->dsi_pwm_cfg)
		mipi_toshiba_pdata->dsi_pwm_cfg();
}

static void mipi_toshiba_set_backlight(struct msm_fb_data_type *mfd)
{
	int ret;
	static int bklight_pwm_cfg;

	if (bklight_pwm_cfg == 0) {
		mipi_bklight_pwm_cfg();
		bklight_pwm_cfg++;
	}

	if (bl_lpm) {
		ret = pwm_config(bl_lpm, MIPI_TOSHIBA_PWM_DUTY_LEVEL *
			mfd->bl_level, MIPI_TOSHIBA_PWM_PERIOD_USEC);
		if (ret) {
			pr_err("pwm_config on lpm failed %d\n", ret);
			return;
		}
		if (mfd->bl_level) {
			ret = pwm_enable(bl_lpm);
			if (ret)
				pr_err("pwm enable/disable on lpm failed"
					"for bl %d\n",	mfd->bl_level);
		} else {
			pwm_disable(bl_lpm);
		}
	}
}

static int __devinit mipi_toshiba_lcd_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		mipi_toshiba_pdata = pdev->dev.platform_data;
		return 0;
	}

	if (mipi_toshiba_pdata == NULL) {
		pr_err("%s.invalid platform data.\n", __func__);
		return -ENODEV;
	}

	if (mipi_toshiba_pdata != NULL)
		bl_lpm = pwm_request(mipi_toshiba_pdata->gpio[0],
			"backlight");

	if (bl_lpm == NULL || IS_ERR(bl_lpm)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_lpm = NULL;
	}
	pr_debug("bl_lpm = %p lpm = %d\n", bl_lpm,
		mipi_toshiba_pdata->gpio[0]);

	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_toshiba_lcd_probe,
	.driver = {
		.name   = "mipi_toshiba",
	},
};

static struct msm_fb_panel_data toshiba_panel_data = {
	.on		= mipi_toshiba_lcd_on,
	.off		= mipi_toshiba_lcd_off,
	.set_backlight  = mipi_toshiba_set_backlight,
};

static int ch_used[3];

int mipi_toshiba_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_toshiba_lcd_init();
	if (ret) {
		pr_err("mipi_toshiba_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_toshiba", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	toshiba_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &toshiba_panel_data,
		sizeof(toshiba_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_toshiba_lcd_init(void)
{
	mipi_dsi_buf_alloc(&toshiba_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&toshiba_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
