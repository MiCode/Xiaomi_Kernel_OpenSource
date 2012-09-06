/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include "mipi_NT35510.h"

static struct msm_panel_common_pdata *mipi_nt35510_pdata;
static struct dsi_buf nt35510_tx_buf;
static struct dsi_buf nt35510_rx_buf;

static int mipi_nt35510_bl_ctrl;

#define NT35510_SLEEP_OFF_DELAY 150
#define NT35510_DISPLAY_ON_DELAY 150

/* common setting */
static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char write_ram[2] = {0x2c, 0x00}; /* write ram */

static struct dsi_cmd_desc nt35510_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(enter_sleep), enter_sleep}
};

static char cmd0[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x01,
};
static char cmd1[4] = {
	0xBC, 0x00, 0xA0, 0x00,
};
static char cmd2[4] = {
	0xBD, 0x00, 0xA0, 0x00,
};
static char cmd3[3] = {
	0xBE, 0x00, 0x79,
};
static char cmd4[53] = {
	0xD1, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char cmd5[53] = {
	0xD2, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char cmd6[53] = {
	0xD3, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char cmd7[53] = {
	0xD4, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char cmd8[53] = {
	0xD5, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char cmd9[53] = {
	0xD6, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char cmd10[4] = {
	0xB0, 0x0A, 0x0A, 0x0A,
};
static char cmd11[4] = {
	0xB1, 0x0A, 0x0A, 0x0A,
};
static char cmd12[4] = {
	0xBA, 0x24, 0x24, 0x24,
};
static char cmd13[4] = {
	0xB9, 0x24, 0x24, 0x24,
};
static char cmd14[4] = {
	0xB8, 0x24, 0x24, 0x24,
};
static char cmd15[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x00,
};
static char cmd16[2] = {
	0xB3, 0x00,
};
static char cmd17[2] = {
	0xB4, 0x10,
};
static char cmd18[2] = {
	0xB6, 0x02,
};
static char cmd19[3] = {
	0xB1, 0xEC, 0x00,
};
static char cmd19_rotate[3] = {
	0xB1, 0xEC, 0x06,
};
static char cmd20[4] = {
	0xBC, 0x05, 0x05, 0x05,
};
static char cmd21[3] = {
	0xB7, 0x20, 0x20,
};
static char cmd22[5] = {
	0xB8, 0x01, 0x03, 0x03,
	0x03,
};
static char cmd23[19] = {
	0xC8, 0x01, 0x00, 0x78,
	0x50, 0x78, 0x50, 0x78,
	0x50, 0x78, 0x50, 0xC8,
	0x3C, 0x3C, 0xC8, 0xC8,
	0x3C, 0x3C, 0xC8,
};
static char cmd24[6] = {
	0xBD, 0x01, 0x84, 0x07,
	0x31, 0x00,
};
static char cmd25[6] = {
	0xBE, 0x01, 0x84, 0x07,
	0x31, 0x00,
};
static char cmd26[6] = {
	0xBF, 0x01, 0x84, 0x07,
	0x31, 0x00,
};
static char cmd27[2] = {
	0x35, 0x00,
};
static char config_MADCTL[2] = {0x36, 0x00};
static struct dsi_cmd_desc nt35510_cmd_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd0), cmd0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd1), cmd1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd2), cmd2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd3), cmd3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd4), cmd4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd5), cmd5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd6), cmd6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd7), cmd7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd8), cmd8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd9), cmd9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd10), cmd10},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd11), cmd11},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd12), cmd12},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd13), cmd13},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd14), cmd14},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd15), cmd15},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd16), cmd16},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd17), cmd17},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd18), cmd18},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd19), cmd19},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd20), cmd20},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd21), cmd21},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd22), cmd22},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd23), cmd23},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd24), cmd24},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd25), cmd25},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd26), cmd26},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(cmd27), cmd27},

	{DTYPE_DCS_WRITE, 1, 0, 0, 150,	sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,	sizeof(display_on), display_on},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 150,
		sizeof(config_MADCTL), config_MADCTL},

	{DTYPE_DCS_WRITE, 1, 0, 0, 10,	sizeof(write_ram), write_ram},
};

static struct dsi_cmd_desc nt35510_cmd_display_on_cmds_rotate[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 50,
		sizeof(cmd19_rotate), cmd19_rotate},
};

static char video0[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x01,
};
static char video1[4] = {
	0xBC, 0x00, 0xA0, 0x00,
};
static char video2[4] = {
	0xBD, 0x00, 0xA0, 0x00,
};
static char video3[3] = {
	0xBE, 0x00, 0x79,
};
static char video4[53] = {
	0xD1, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char video5[53] = {
	0xD2, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char video6[53] = {
	0xD3, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char video7[53] = {
	0xD4, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char video8[53] = {
	0xD5, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char video9[53] = {
	0xD6, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x32, 0x00,
	0x4F, 0x00, 0x65, 0x00,
	0x8B, 0x00, 0xA8, 0x00,
	0xD5, 0x00, 0xF7, 0x01,
	0x2B, 0x01, 0x54, 0x01,
	0x8E, 0x01, 0xBB, 0x01,
	0xBC, 0x01, 0xE3, 0x02,
	0x08, 0x02, 0x1C, 0x02,
	0x39, 0x02, 0x4F, 0x02,
	0x76, 0x02, 0xA3, 0x02,
	0xE3, 0x03, 0x12, 0x03,
	0x4C, 0x03, 0x66, 0x03,
	0x9A,
};
static char video10[4] = {
	0xB0, 0x0A, 0x0A, 0x0A,
};
static char video11[4] = {
	0xB1, 0x0A, 0x0A, 0x0A,
};
static char video12[4] = {
	0xBA, 0x24, 0x24, 0x24,
};
static char video13[4] = {
	0xB9, 0x24, 0x24, 0x24,
};
static char video14[4] = {
	0xB8, 0x24, 0x24, 0x24,
};
static char video15[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x00,
};
static char video16[2] = {
	0xB3, 0x00,
};
static char video17[2] = {
	0xB4, 0x10,
};
static char video18[2] = {
	0xB6, 0x02,
};
static char video19[3] = {
	0xB1, 0xFC, 0x00,
};
static char video20[4] = {
	0xBC, 0x05, 0x05, 0x05,
};
static char video21[3] = {
	0xB7, 0x20, 0x20,
};
static char video22[5] = {
	0xB8, 0x01, 0x03, 0x03,
	0x03,
};
static char video23[19] = {
	0xC8, 0x01, 0x00, 0x78,
	0x50, 0x78, 0x50, 0x78,
	0x50, 0x78, 0x50, 0xC8,
	0x3C, 0x3C, 0xC8, 0xC8,
	0x3C, 0x3C, 0xC8,
};
static char video24[6] = {
	0xBD, 0x01, 0x84, 0x07,
	0x31, 0x00,
};
static char video25[6] = {
	0xBE, 0x01, 0x84, 0x07,
	0x31, 0x00,
};
static char video26[6] = {
	0xBF, 0x01, 0x84, 0x07,
	0x31, 0x00,
};
static char video27[2] = {
	0x35, 0x00,
};
static char config_video_MADCTL[2] = {0x36, 0xC0};
static struct dsi_cmd_desc nt35510_video_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video0), video0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video1), video1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video2), video2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video3), video3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video4), video4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video5), video5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video6), video6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video7), video7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video8), video8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video9), video9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video10), video10},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video11), video11},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video12), video12},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video13), video13},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video14), video14},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video15), video15},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video16), video16},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video17), video17},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video18), video18},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video19), video19},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video20), video20},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video21), video21},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video22), video22},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video23), video23},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video24), video24},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video25), video25},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video26), video26},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 50, sizeof(video27), video27},
	{DTYPE_DCS_WRITE, 1, 0, 0, NT35510_SLEEP_OFF_DELAY, sizeof(exit_sleep),
			exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, NT35510_DISPLAY_ON_DELAY, sizeof(display_on),
			display_on},
};

static struct dsi_cmd_desc nt35510_video_display_on_cmds_rotate[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 150,
		sizeof(config_video_MADCTL), config_video_MADCTL},
};
static int mipi_nt35510_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	static int rotate;
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi  = &mfd->panel_info.mipi;

	if (!mfd->cont_splash_done) {
		mfd->cont_splash_done = 1;
		return 0;
	}

	if (mipi_nt35510_pdata && mipi_nt35510_pdata->rotate_panel)
		rotate = mipi_nt35510_pdata->rotate_panel();

	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_dsi_cmds_tx(&nt35510_tx_buf,
			nt35510_video_display_on_cmds,
			ARRAY_SIZE(nt35510_video_display_on_cmds));

		if (rotate) {
			mipi_dsi_cmds_tx(&nt35510_tx_buf,
				nt35510_video_display_on_cmds_rotate,
			ARRAY_SIZE(nt35510_video_display_on_cmds_rotate));
		}
	} else if (mipi->mode == DSI_CMD_MODE) {
		mipi_dsi_cmds_tx(&nt35510_tx_buf,
			nt35510_cmd_display_on_cmds,
			ARRAY_SIZE(nt35510_cmd_display_on_cmds));

		if (rotate) {
			mipi_dsi_cmds_tx(&nt35510_tx_buf,
				nt35510_cmd_display_on_cmds_rotate,
			ARRAY_SIZE(nt35510_cmd_display_on_cmds_rotate));
		}
	}

	return 0;
}

static int mipi_nt35510_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	pr_debug("mipi_nt35510_lcd_off E\n");

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(&nt35510_tx_buf, nt35510_display_off_cmds,
			ARRAY_SIZE(nt35510_display_off_cmds));

	pr_debug("mipi_nt35510_lcd_off X\n");
	return 0;
}

static ssize_t mipi_nt35510_wta_bl_ctrl(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int err;

	err =  kstrtoint(buf, 0, &mipi_nt35510_bl_ctrl);
	if (err)
		return ret;

	pr_info("%s: bl ctrl set to %d\n", __func__, mipi_nt35510_bl_ctrl);

	return ret;
}

static DEVICE_ATTR(bl_ctrl, S_IWUSR, NULL, mipi_nt35510_wta_bl_ctrl);

static struct attribute *mipi_nt35510_fs_attrs[] = {
	&dev_attr_bl_ctrl.attr,
	NULL,
};

static struct attribute_group mipi_nt35510_fs_attr_group = {
	.attrs = mipi_nt35510_fs_attrs,
};

static int mipi_nt35510_create_sysfs(struct platform_device *pdev)
{
	int rc;
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	if (!mfd) {
		pr_err("%s: mfd not found\n", __func__);
		return -ENODEV;
	}
	if (!mfd->fbi) {
		pr_err("%s: mfd->fbi not found\n", __func__);
		return -ENODEV;
	}
	if (!mfd->fbi->dev) {
		pr_err("%s: mfd->fbi->dev not found\n", __func__);
		return -ENODEV;
	}
	rc = sysfs_create_group(&mfd->fbi->dev->kobj,
		&mipi_nt35510_fs_attr_group);
	if (rc) {
		pr_err("%s: sysfs group creation failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}

static int __devinit mipi_nt35510_lcd_probe(struct platform_device *pdev)
{
	struct platform_device *pthisdev = NULL;
	struct msm_fb_panel_data *pdata;
	pr_debug("%s\n", __func__);

	if (pdev->id == 0) {
		mipi_nt35510_pdata = pdev->dev.platform_data;
		if (mipi_nt35510_pdata->bl_lock)
			spin_lock_init(&mipi_nt35510_pdata->bl_spinlock);
		return 0;
	}

	pdata = pdev->dev.platform_data;
	if (mipi_nt35510_pdata && mipi_nt35510_pdata->rotate_panel()
			&& pdata->panel_info.type == MIPI_CMD_PANEL)
		pdata->panel_info.lcd.refx100 = 6200;

	pthisdev = msm_fb_add_device(pdev);
	mipi_nt35510_create_sysfs(pthisdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_nt35510_lcd_probe,
	.driver = {
		.name   = "mipi_NT35510",
	},
};

static int old_bl_level;

static void mipi_nt35510_set_backlight(struct msm_fb_data_type *mfd)
{
	int bl_level;
	unsigned long flags;
	bl_level = mfd->bl_level;

	if (mipi_nt35510_pdata->bl_lock) {
		if (!mipi_nt35510_bl_ctrl) {
			/* Level received is of range 1 to bl_max,
			   We need to convert the levels to 1
			   to 31 */
			bl_level = (2 * bl_level * 31 + mfd->panel_info.bl_max)
					/(2 * mfd->panel_info.bl_max);
			if (bl_level == old_bl_level)
				return;

			if (bl_level == 0)
				mipi_nt35510_pdata->backlight(0, 1);

			if (old_bl_level == 0)
				mipi_nt35510_pdata->backlight(50, 1);

			spin_lock_irqsave(&mipi_nt35510_pdata->bl_spinlock,
						flags);
			mipi_nt35510_pdata->backlight(bl_level, 0);
			spin_unlock_irqrestore(&mipi_nt35510_pdata->bl_spinlock,
						flags);
			old_bl_level = bl_level;
		} else {
			mipi_nt35510_pdata->backlight(bl_level, 1);
		}
	} else {
		mipi_nt35510_pdata->backlight(bl_level, mipi_nt35510_bl_ctrl);
	}
}

static struct msm_fb_panel_data nt35510_panel_data = {
	.on	= mipi_nt35510_lcd_on,
	.off = mipi_nt35510_lcd_off,
	.set_backlight = mipi_nt35510_set_backlight,
};

static int ch_used[3];

static int mipi_nt35510_lcd_init(void)
{
	mipi_dsi_buf_alloc(&nt35510_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&nt35510_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
int mipi_nt35510_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_nt35510_lcd_init();
	if (ret) {
		pr_err("mipi_nt35510_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_NT35510", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	nt35510_panel_data.panel_info = *pinfo;
	ret = platform_device_add_data(pdev, &nt35510_panel_data,
				sizeof(nt35510_panel_data));
	if (ret) {
		pr_debug("%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_debug("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}
