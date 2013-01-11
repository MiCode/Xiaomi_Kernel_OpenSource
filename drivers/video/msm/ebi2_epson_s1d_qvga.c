/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/memory.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include "linux/proc_fs.h"

#include <linux/delay.h>

#include <mach/hardware.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>

#define CMD_NOP_C							0x00
#define CMD_SOFT_RESET_C					0x99
#define CMD_DISPLAY_ON_C					0xAF
#define CMD_DISPLAY_OFF_C					0xAE
#define CMD_SET_DISPLAY_C					0xCA
#define CMD_SET_DISPLAY_TIMING_C			0xA1
#define CMD_SET_DATA_C						0xBC
#define CMD_SET_START_ADDRESS_C				0x15
#define CMD_SET_END_ADDRESS_C				0x75
#define CMD_RAM_WRITE_C						0x5C
#define CMD_RAM_READ_C						0x5D
#define CMD_SET_AREA_SCROLLING_C			0xAA
#define CMD_SET_DISPLAY_START_LINE_C		0xAB
#define CMD_PARTIAL_DISPLAY_IN_C			0xA8
#define CMD_PARTIAL_DISPLAY_OUT_C			0xA9
#define CMD_SET_DISPLAY_DATA_INTERFACE_C	0x31
#define CMD_SET_DISPLAY_COLOR_MODE_C		0x8B
#define CMD_SELECT_MTP_ROM_MODE_C			0x65
#define CMD_MTP_ROM_MODE_IN_C				0x67
#define CMD_MTP_ROM_MODE_OUT_C				0x68
#define CMD_MTP_ROM_OPERATION_IN_C			0x69
#define CMD_MTP_ROM_OPERATION_OUT_C			0x70
#define CMD_GATE_LINE_SCAN_MODE_C			0x6F
#define CMD_SET_AC_OPERATION_DRIVE_C		0x8C
#define CMD_SET_ELECTRONIC_CONTROL_C		0x20
#define CMD_SET_POSITIVE_CORRECTION_CHARS_C	0x22
#define CMD_SET_NEGATIVE_CORRECTION_CHARS_C	0x25
#define CMD_SET_POWER_CONTROL_C				0x21
#define CMD_SET_PARTIAL_POWER_CONTROL_C		0x23
#define CMD_SET_8_COLOR_CONTROL_C			0x24
#define CMD_SLEEP_IN_C						0x95
#define CMD_SLEEP_OUT_C						0x94
#define CMD_VDD_OFF_C						0x97
#define CMD_VDD_ON_C						0x96
#define CMD_STOP_OSCILLATION_C				0x93
#define CMD_START_OSCILLATION_C				0x92
#define CMD_TEST_SOURCE_C					0xFD
#define CMD_TEST_FUSE_C						0xFE
#define CMD_TEST_C							0xFF
#define CMD_STATUS_READ_C					0xE8
#define CMD_REVISION_READ_C					0xE9

#define PANEL_WIDTH			240
#define PANEL_HEIGHT		320
#define ACTIVE_WIN_WIDTH	PANEL_WIDTH
#define ACTIVE_WIN_HEIGHT	PANEL_HEIGHT

#define ACTIVE_WIN_H_START	0
#define ACTIVE_WIN_V_START	0

#define DISP_CMD_OUT(cmd) outpw(DISP_CMD_PORT, (cmd << 1));
#define DISP_DATA_OUT(data) outpw(DISP_DATA_PORT, (data << 1));
#define DISP_DATA_IN() inpw(DISP_DATA_PORT);

static void *DISP_CMD_PORT;
static void *DISP_DATA_PORT;
static boolean disp_initialized;
static boolean display_on;
static struct msm_panel_common_pdata *ebi2_epson_pdata;

static void epson_s1d_disp_init(struct platform_device *pdev);
static int epson_s1d_disp_off(struct platform_device *pdev);
static int epson_s1d_disp_on(struct platform_device *pdev);
static void epson_s1d_disp_set_rect(int x, int y, int xres, int yres);

static void epson_s1d_disp_set_rect(int x, int y, int xres, int yres)
{
	int right, bottom;

	if (!disp_initialized)
		return;

	right = x + xres - 1;
	bottom = y + yres - 1;

	x += ACTIVE_WIN_H_START;
	y += ACTIVE_WIN_V_START;
	right += ACTIVE_WIN_H_START;
	bottom += ACTIVE_WIN_V_START;

	if ((PANEL_WIDTH  > x) &&
		(PANEL_HEIGHT > y) &&
		(PANEL_WIDTH > right) &&
		(PANEL_HEIGHT > bottom)) {
		DISP_CMD_OUT(CMD_SET_START_ADDRESS_C);
		DISP_DATA_OUT((uint8)x);
		DISP_DATA_OUT((uint8)(y>>8));
		DISP_DATA_OUT((uint8)y);

		DISP_CMD_OUT(CMD_SET_END_ADDRESS_C);
		DISP_DATA_OUT((uint8)right);
		DISP_DATA_OUT((uint8)(bottom>>8));
		DISP_DATA_OUT((uint8)bottom);
		DISP_CMD_OUT(CMD_RAM_WRITE_C);
	}
}

static void epson_s1d_disp_init(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	if (disp_initialized)
		return;

	mfd = platform_get_drvdata(pdev);

	DISP_CMD_PORT = mfd->cmd_port;
	DISP_DATA_PORT = mfd->data_port;

	disp_initialized = TRUE;
}

static int epson_s1d_disp_off(struct platform_device *pdev)
{
	if (!disp_initialized)
		epson_s1d_disp_init(pdev);

	if (display_on) {
		DISP_CMD_OUT(CMD_SOFT_RESET_C);
		DISP_CMD_OUT(CMD_VDD_OFF_C);
		display_on = FALSE;
	}

	return 0;
}

static int epson_s1d_disp_on(struct platform_device *pdev)
{
	int i;
	if (!disp_initialized)
		epson_s1d_disp_init(pdev);

	if (!display_on) {
		/* Enable Vdd regulator */
		DISP_CMD_OUT(CMD_VDD_ON_C);
		msleep(20);

		/* Soft Reset before configuring display */
		DISP_CMD_OUT(CMD_SOFT_RESET_C);
		msleep(20);

		/* Set display attributes */

		/* GATESCAN */
		DISP_CMD_OUT(CMD_GATE_LINE_SCAN_MODE_C);
		DISP_DATA_OUT(0x0);

		/* DISSET */
		DISP_CMD_OUT(CMD_SET_DISPLAY_C);
		DISP_DATA_OUT(0x31);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT((uint8)((PANEL_HEIGHT - 1)>>8));
		DISP_DATA_OUT((uint8)(PANEL_HEIGHT - 1));
		DISP_DATA_OUT(0x03);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x08);

		/* VOLSET */
		DISP_CMD_OUT(
				   CMD_SET_ELECTRONIC_CONTROL_C);
		DISP_DATA_OUT(0x10);
		DISP_DATA_OUT(0x80);
		DISP_DATA_OUT(0x11);
		DISP_DATA_OUT(0x1B);
		DISP_DATA_OUT(0x02);
		DISP_DATA_OUT(0x0D);
		DISP_DATA_OUT(0x00);

		/* PWRCTL */
		DISP_CMD_OUT(CMD_SET_POWER_CONTROL_C);
		DISP_DATA_OUT(0x01);
		DISP_DATA_OUT(0x24);
		DISP_DATA_OUT(0x0F);
		DISP_DATA_OUT(0xFE);
		DISP_DATA_OUT(0x33);
		DISP_DATA_OUT(0x31);
		DISP_DATA_OUT(0xFF);
		DISP_DATA_OUT(0x03);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x77);
		DISP_DATA_OUT(0x33);
		DISP_DATA_OUT(0x11);
		DISP_DATA_OUT(0x44);
		DISP_DATA_OUT(0x00);

		/* PPWRCTL */
		DISP_CMD_OUT(CMD_SET_PARTIAL_POWER_CONTROL_C);
		DISP_DATA_OUT(0x33);
		DISP_DATA_OUT(0xFF);
		DISP_DATA_OUT(0x03);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x44);
		DISP_DATA_OUT(0x00);

		/* SPLOUT */
		DISP_CMD_OUT(CMD_SLEEP_OUT_C);
		msleep(100);

		/* DATSET */
		DISP_CMD_OUT(CMD_SET_DATA_C);
		DISP_DATA_OUT(0x00);

		/* DISTMEMSET */
		DISP_CMD_OUT(CMD_SET_DISPLAY_TIMING_C);
		DISP_DATA_OUT(0x01);
		DISP_DATA_OUT(0x2E);
		DISP_DATA_OUT(0x0A);
		DISP_DATA_OUT(0x2C);
		DISP_DATA_OUT(0x23);
		DISP_DATA_OUT(0x2F);
		DISP_DATA_OUT(0x00);

		/* GAMSETP */
		DISP_CMD_OUT(CMD_SET_POSITIVE_CORRECTION_CHARS_C);
		DISP_DATA_OUT(0x37);
		DISP_DATA_OUT(0xFF);
		DISP_DATA_OUT(0x7F);
		DISP_DATA_OUT(0x15);
		DISP_DATA_OUT(0x37);
		DISP_DATA_OUT(0x05);

		/* GAMSETN */
		DISP_CMD_OUT(CMD_SET_NEGATIVE_CORRECTION_CHARS_C);
		DISP_DATA_OUT(0x37);
		DISP_DATA_OUT(0xFF);
		DISP_DATA_OUT(0x7F);
		DISP_DATA_OUT(0x15);
		DISP_DATA_OUT(0x37);
		DISP_DATA_OUT(0x05);

		/* ACDRIVE */
		DISP_CMD_OUT(CMD_SET_AC_OPERATION_DRIVE_C);
		DISP_DATA_OUT(0x00);

		/* TEST */
		DISP_CMD_OUT(CMD_TEST_C);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x01);

		/* COLMOD */
		DISP_CMD_OUT(CMD_SET_DISPLAY_COLOR_MODE_C);
		DISP_DATA_OUT(0x00);

		/* STADDSET */
		DISP_CMD_OUT(CMD_SET_START_ADDRESS_C);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x00);

		/* EDADDSET */
		DISP_CMD_OUT(CMD_SET_END_ADDRESS_C);
		DISP_DATA_OUT(0xEF);
		DISP_DATA_OUT(0x01);
		DISP_DATA_OUT(0x3F);

		/* Set Display Start Line */
		DISP_CMD_OUT(CMD_SET_DISPLAY_START_LINE_C);
		DISP_DATA_OUT(0x00);

		/* Set Display Data Interface */
		DISP_CMD_OUT(CMD_SET_DISPLAY_DATA_INTERFACE_C);
		DISP_DATA_OUT(0x00);
		DISP_DATA_OUT(0x04);

		epson_s1d_disp_set_rect(0,
						 0,
						 ACTIVE_WIN_WIDTH,
						 ACTIVE_WIN_HEIGHT);

		for (i = 0; i < (ACTIVE_WIN_WIDTH * ACTIVE_WIN_HEIGHT); i++)
			outpdw(DISP_DATA_PORT, 0);

		/* DISON */
		DISP_CMD_OUT(CMD_DISPLAY_ON_C);
		msleep(60);

		display_on = TRUE;
	}

	return 0;
}

static int epson_s1d_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		ebi2_epson_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = epson_s1d_probe,
	.driver = {
		.name   = "ebi2_epson_s1d_qvga",
	},
};

static struct msm_fb_panel_data epson_s1d_panel_data = {
	.on = epson_s1d_disp_on,
	.off = epson_s1d_disp_off,
	.set_rect = epson_s1d_disp_set_rect,
};

static struct platform_device this_device = {
	.name   = "ebi2_epson_s1d_qvga",
	.id	= 1,
	.dev	= {
		.platform_data = &epson_s1d_panel_data,
	}
};

static int __init epson_s1d_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	ret = platform_driver_register(&this_driver);
	if (!ret) {
		pinfo = &epson_s1d_panel_data.panel_info;
		pinfo->xres = PANEL_WIDTH;
		pinfo->yres = PANEL_HEIGHT;
		MSM_FB_SINGLE_MODE_PANEL(pinfo);
		pinfo->type = EBI2_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->wait_cycle = 0x048423E8;
		pinfo->bpp = 18;
		pinfo->fb_num = 2;
		pinfo->lcd.vsync_enable = FALSE;

		ret = platform_device_register(&this_device);
		if (ret)
			platform_driver_unregister(&this_driver);
	}

	return ret;
}

module_init(epson_s1d_init);
