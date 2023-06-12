// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2015, 2021, The Linux Foundation. All rights reserved.
 */

#include "qpic_display.h"
#include "qpic_display_panel.h"

/**
 * ILI9341 commands
 */
#define OP_ILI9341_INTERFACE_CONTROL	0xf6
#define OP_ILI9341_TEARING_EFFECT_LINE_ON	0x35

static int ili9341_on(struct qpic_display_data *qpic_display)
{
	u8 param[4];

	qpic_display->qpic_transfer(qpic_display, OP_SOFT_RESET, NULL, 0);
	/* wait for 120 ms after reset as panel spec suggests */
	msleep(120);
	qpic_display->qpic_transfer(qpic_display, OP_SET_DISPLAY_OFF, NULL, 0);
	/* wait for 20 ms after disply off */
	msleep(20);

	/* set memory access control */
	param[0] = 0x48;
	qpic_display->qpic_transfer(qpic_display, OP_SET_ADDRESS_MODE, param, 1);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	param[0] = 0x66;
	qpic_display->qpic_transfer(qpic_display, OP_SET_PIXEL_FORMAT, param, 1);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* set interface */
	param[0] = 1;
	param[1] = 0;
	param[2] = 0;
	qpic_display->qpic_transfer(qpic_display, OP_ILI9341_INTERFACE_CONTROL, param, 3);
	/* wait for 20 ms after command sent */
	msleep(20);

	/* exit sleep mode */
	qpic_display->qpic_transfer(qpic_display, OP_EXIT_SLEEP_MODE, NULL, 0);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* normal mode */
	qpic_display->qpic_transfer(qpic_display, OP_ENTER_NORMAL_MODE, NULL, 0);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* display on */
	qpic_display->qpic_transfer(qpic_display, OP_SET_DISPLAY_ON, NULL, 0);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	param[0] = 0;
	qpic_display->qpic_transfer(qpic_display, OP_ILI9341_TEARING_EFFECT_LINE_ON, param, 1);

	return 0;
}

static void ili9341_off(struct qpic_display_data *qpic_display)
{
	qpic_display->qpic_transfer(qpic_display, OP_SET_DISPLAY_OFF, NULL, 0);
	/* wait for 20 ms after display off */
	msleep(20);
}

static struct qpic_panel_config ili_qvga_panel = {
	.xres = 320,
	.yres = 480,
	.bpp = 16,
};

void get_ili_qvga_panel_config(struct qpic_display_data *qpic_display)
{
	qpic_display->panel_config = &ili_qvga_panel;
	qpic_display->panel_on = ili9341_on;
	qpic_display->panel_off = ili9341_off;
}
