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
#include "mipi_truly_tft540960_1_e.h"

static struct msm_panel_common_pdata *mipi_truly_pdata;
static struct dsi_buf truly_tx_buf;
static struct dsi_buf truly_rx_buf;

static int mipi_truly_bl_ctrl;

#define TRULY_CMD_DELAY 0
#define MIPI_SETTING_DELAY 10
#define TRULY_SLEEP_OFF_DELAY 150
#define TRULY_DISPLAY_ON_DELAY 150

/* common setting */
static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char write_ram[2] = {0x2c, 0x00}; /* write ram */

static struct dsi_cmd_desc truly_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(enter_sleep), enter_sleep}
};


/* TFT540960_1_E CMD mode */
static char cmd0[5] = {
	0xFF, 0xAA, 0x55, 0x25,
	0x01,
};

static char cmd2[5] = {
	0xF3, 0x02, 0x03, 0x07,
	0x45,
};

static char cmd3[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x00,
};

static char cmd4[2] = {
	0xB1, 0xeC,
};

/* add 0X BD command */
static char cmd26_2[6] = {
	0xBD, 0x01, 0x48, 0x10, 0x38, 0x01 /* 59 HZ */
};

static char cmd5[5] = {
	0xB8, 0x01, 0x02, 0x02,
	0x02,
};

static char cmd6[4] = {
	0xBC, 0x05, 0x05, 0x05,
};

static char cmd7[2] = {
	0x4C, 0x11,
};

static char cmd8[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x01,
};

static char cmd9[4] = {
	0xB0, 0x05, 0x05, 0x05,
};

static char cmd10[4] = {
	0xB6, 0x44, 0x44, 0x44,
};
static char cmd11[4] = {
	0xB1, 0x05, 0x05, 0x05,
};

static char cmd12[4] = {
	0xB7, 0x34, 0x34, 0x34,
};

static char cmd13[4] = {
	0xB3, 0x10, 0x10, 0x10,
};

static char cmd14[4] = {
	0xB9, 0x34, 0x34, 0x34,
};

static char cmd15[4] = {
	0xB4, 0x0A, 0x0A, 0x0A,
};

static char cmd16[4] = {
	0xBA, 0x14, 0x14, 0x14,
};
static char cmd17[4] = {
	0xBC, 0x00, 0xA0, 0x00,
};

static char cmd18[4] = {
	0xBD, 0x00, 0xA0, 0x00,
};

static char cmd19[2] = {
	0xBE, 0x45,
};

static char cmd20[17] = {
	0xD1, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char cmd21[17] = {
	0xD2, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char cmd22[17] = {
	0xD3, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char cmd23[5] = {
	0xD4, 0x03, 0xB0, 0x03,
	0xF4,
};

static char cmd24[17] = {
	0xD5, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};
static char cmd25[17] = {
	0xD6, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char cmd26[17] = {
	0xD7, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};
static char cmd27[5] = {
	0xD8, 0x03, 0xB0, 0x03,
	0xF4,
};


static char cmd28[17] = {
	0xD9, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char cmd29[17] = {
	0xDD, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};
static char cmd30[17] = {
	0xDE, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char cmd31[5] = {
	0xDF, 0x03, 0xB0, 0x03,
	0xF4,
};

static char cmd32[17] = {
	0xE0, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char cmd33[17] = {
	0xE1, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char cmd34[17] = {
	0xE2, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char cmd35[5] = {
	0xE3, 0x03, 0xB0, 0x03,
	0xF4,
};

static char cmd36[17] = {
	0xE4, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};
static char cmd37[17] = {
	0xE5, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char cmd38[17] = {
	0xE6, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char cmd39[5] = {
	0xE7, 0x03, 0xB0, 0x03,
	0xF4,
};

static char cmd40[17] = {
	0xE8, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char cmd41[17] = {
	0xE9, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char cmd42[17] = {
	0xEA, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char cmd43[5] = {
	0xEB, 0x03, 0xB0, 0x03,
	0xF4,
};

static char cmd44[2] = {
	0x3A, 0x07,
};

static char cmd45[2] = {
	0x35, 0x00,
};


static struct dsi_cmd_desc truly_cmd_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd0), cmd0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd2), cmd2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd3), cmd3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd4), cmd4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd26_2), cmd26_2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd5), cmd5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd6), cmd6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd7), cmd7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd8), cmd8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd9), cmd9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd10), cmd10},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd11), cmd11},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd12), cmd12},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd13), cmd13},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd14), cmd14},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd15), cmd15},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd16), cmd16},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd17), cmd17},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd18), cmd18},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd19), cmd19},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd20), cmd20},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd21), cmd21},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd22), cmd22},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd23), cmd23},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd24), cmd24},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd25), cmd25},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd26), cmd26},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd27), cmd27},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd28), cmd28},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd29), cmd29},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd30), cmd30},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd31), cmd31},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd32), cmd32},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd33), cmd33},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd34), cmd34},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd35), cmd35},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd36), cmd36},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd37), cmd37},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd38), cmd38},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd39), cmd39},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd40), cmd40},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd41), cmd41},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd42), cmd42},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd43), cmd43},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd44), cmd44},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cmd45), cmd45},
	{DTYPE_DCS_WRITE, 1, 0, 0, TRULY_SLEEP_OFF_DELAY, sizeof(exit_sleep),
								exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, TRULY_CMD_DELAY, sizeof(display_on),
							display_on},
	{DTYPE_DCS_WRITE, 1, 0, 0, TRULY_CMD_DELAY, sizeof(write_ram),
							write_ram},

};

/* TFT540960_1_E VIDEO mode */
static char video0[5] = {
	0xFF, 0xAA, 0x55, 0x25,
	0x01,
};

static char video2[5] = {
	0xF3, 0x02, 0x03, 0x07,
	0x15,
};

static char video3[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x00,
};

static char video4[2] = {
	0xB1, 0xFC,
};

static char video5[5] = {
	0xB8, 0x01, 0x02, 0x02,
	0x02,
};

static char video6[4] = {
	0xBC, 0x05, 0x05, 0x05,
};

static char video7[2] = {
	0x4C, 0x11,
};

static char video8[6] = {
	0xF0, 0x55, 0xAA, 0x52,
	0x08, 0x01,
};

static char video9[4] = {
	0xB0, 0x05, 0x05, 0x05,
};

static char video10[4] = {
	0xB6, 0x44, 0x44, 0x44,
};

static char video11[4] = {
	0xB1, 0x05, 0x05, 0x05,
};

static char video12[4] = {
	0xB7, 0x34, 0x34, 0x34,
};

static char video13[4] = {
	0xB3, 0x10, 0x10, 0x10,
};

static char video14[4] = {
	0xB9, 0x34, 0x34, 0x34,
};

static char video15[4] = {
	0xB4, 0x0A, 0x0A, 0x0A,
};

static char video16[4] = {
	0xBA, 0x14, 0x14, 0x14,
};

static char video17[4] = {
	0xBC, 0x00, 0xA0, 0x00,
};

static char video18[4] = {
	0xBD, 0x00, 0xA0, 0x00,
};

static char video19[2] = {
	0xBE, 0x45,
};

static char video20[17] = {
	0xD1, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char video21[17] = {
	0xD2, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char video22[17] = {
	0xD3, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char video23[5] = {
	0xD4, 0x03, 0xB0, 0x03,
	0xF4,
};

static char video24[17] = {
	0xD5, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char video25[17] = {
	0xD6, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char video26[17] = {
	0xD7, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char video27[5] = {
	0xD8, 0x03, 0xB0, 0x03,
	0xF4,
};

static char video28[17] = {
	0xD9, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char video29[17] = {
	0xDD, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char video30[17] = {
	0xDE, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char video31[5] = {
	0xDF, 0x03, 0xB0, 0x03,
	0xF4,
};

static char video32[17] = {
	0xE0, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char video33[17] = {
	0xE1, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char video34[17] = {
	0xE2, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char video35[5] = {
	0xE3, 0x03, 0xB0, 0x03,
	0xF4,
};

static char video36[17] = {
	0xE4, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char video37[17] = {
	0xE5, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char video38[17] = {
	0xE6, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char video39[5] = {
	0xE7, 0x03, 0xB0, 0x03,
	0xF4,
};

static char video40[17] = {
	0xE8, 0x00, 0x32, 0x00,
	0x41, 0x00, 0x54, 0x00,
	0x67, 0x00, 0x7A, 0x00,
	0x98, 0x00, 0xB0, 0x00,
	0xDB,
};

static char video41[17] = {
	0xE9, 0x01, 0x01, 0x01,
	0x3F, 0x01, 0x70, 0x01,
	0xB4, 0x01, 0xEC, 0x01,
	0xED, 0x02, 0x1E, 0x02,
	0x51,
};

static char video42[17] = {
	0xEA, 0x02, 0x6C, 0x02,
	0x8D, 0x02, 0xA5, 0x02,
	0xC9, 0x02, 0xEA, 0x03,
	0x19, 0x03, 0x45, 0x03,
	0x7A,
};

static char video43[5] = {
	0xEB, 0x03, 0xB0, 0x03,
	0xF4,
};

static char video44[2] = {
	0x3A, 0x07,
};

static char video45[2] = {
	0x35, 0x00,
};

static struct dsi_cmd_desc truly_video_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video0), video0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video2), video2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video3), video3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video4), video4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video5), video5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video6), video6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video7), video7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video8), video8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video9), video9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video10), video10},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video11), video11},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video12), video12},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video13), video13},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video14), video14},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video15), video15},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video16), video16},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video17), video17},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video18), video18},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video19), video19},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video20), video20},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video21), video21},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video22), video22},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video23), video23},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video24), video24},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video25), video25},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video26), video26},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video27), video27},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video28), video28},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video29), video29},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video30), video30},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video31), video31},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video32), video32},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video33), video33},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video34), video34},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video35), video35},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video36), video36},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video37), video37},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video38), video38},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video39), video39},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video40), video40},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video41), video41},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video42), video42},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video43), video43},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video44), video44},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video45), video45},

	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_on), display_on},
};

static int mipi_truly_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi  = &mfd->panel_info.mipi;
	pr_info("%s: mode = %d\n", __func__, mipi->mode);
	msleep(120);

	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_dsi_cmds_tx(mfd, &truly_tx_buf,
			truly_video_display_on_cmds,
			ARRAY_SIZE(truly_video_display_on_cmds));
	} else if (mipi->mode == DSI_CMD_MODE) {
		mipi_dsi_cmds_tx(mfd, &truly_tx_buf,
			truly_cmd_display_on_cmds,
			ARRAY_SIZE(truly_cmd_display_on_cmds));
	}

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

static ssize_t mipi_truly_wta_bl_ctrl(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int err;

	err =  kstrtoint(buf, 0, &mipi_truly_bl_ctrl);
	if (err)
		return ret;

	pr_info("%s: bl ctrl set to %d\n", __func__, mipi_truly_bl_ctrl);

	return ret;
}

static DEVICE_ATTR(bl_ctrl, S_IWUSR, NULL, mipi_truly_wta_bl_ctrl);

static struct attribute *mipi_truly_fs_attrs[] = {
	&dev_attr_bl_ctrl.attr,
	NULL,
};

static struct attribute_group mipi_truly_fs_attr_group = {
	.attrs = mipi_truly_fs_attrs,
};

static int mipi_truly_create_sysfs(struct platform_device *pdev)
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
		&mipi_truly_fs_attr_group);
	if (rc) {
		pr_err("%s: sysfs group creation failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}


static int __devinit mipi_truly_lcd_probe(struct platform_device *pdev)
{
	struct platform_device *pthisdev = NULL;
	int rc = 0;

	if (pdev->id == 0) {
		mipi_truly_pdata = pdev->dev.platform_data;
		if (mipi_truly_pdata->bl_lock)
			spin_lock_init(&mipi_truly_pdata->bl_spinlock);
		return rc;
	}

	pthisdev = msm_fb_add_device(pdev);
	mipi_truly_create_sysfs(pthisdev);

	return rc;
}

static struct platform_driver this_driver = {
	.probe  = mipi_truly_lcd_probe,
	.driver = {
		.name   = "mipi_truly_tft540960_1_e",
	},
};

static int old_bl_level;

static void mipi_truly_set_backlight(struct msm_fb_data_type *mfd)
{
	int bl_level;
	unsigned long flags;
	bl_level = mfd->bl_level;

	if (mipi_truly_pdata->bl_lock) {
		if (!mipi_truly_bl_ctrl) {
			/* Level received is of range 1 to bl_max,
			   We need to convert the levels to 1
			   to 31 */
			bl_level = (2 * bl_level * 31 + mfd->panel_info.bl_max)
					/(2 * mfd->panel_info.bl_max);
			if (bl_level == old_bl_level)
				return;

			if (bl_level == 0)
				mipi_truly_pdata->backlight(0, 1);

			if (old_bl_level == 0)
				mipi_truly_pdata->backlight(50, 1);

			spin_lock_irqsave(&mipi_truly_pdata->bl_spinlock,
						flags);
			mipi_truly_pdata->backlight(bl_level, 0);
			spin_unlock_irqrestore(&mipi_truly_pdata->bl_spinlock,
						flags);
			old_bl_level = bl_level;
		} else {
			mipi_truly_pdata->backlight(bl_level, 1);
		}
	} else {
		mipi_truly_pdata->backlight(bl_level, mipi_truly_bl_ctrl);
	}
}

static struct msm_fb_panel_data truly_panel_data = {
	.on	= mipi_truly_lcd_on,
	.off = mipi_truly_lcd_off,
	.set_backlight = mipi_truly_set_backlight,
};

static int ch_used[3];

static int mipi_truly_tft540960_1_e_lcd_init(void)
{
	mipi_dsi_buf_alloc(&truly_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&truly_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
int mipi_truly_tft540960_1_e_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_truly_tft540960_1_e_lcd_init();
	if (ret) {
		pr_err("%s: platform_device_register failed!\n", __func__);
		return ret;
	}

	pdev = platform_device_alloc("mipi_truly_tft540960_1_e",
						(panel << 8)|channel);
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
