/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include "mipi_renesas.h"
#include <mach/socinfo.h>

#define RENESAS_CMD_DELAY 0 /* 50 */
#define RENESAS_SLEEP_OFF_DELAY 50
static struct msm_panel_common_pdata *mipi_renesas_pdata;

static struct dsi_buf renesas_tx_buf;
static struct dsi_buf renesas_rx_buf;

static int mipi_renesas_lcd_init(void);

static char config_sleep_out[2] = {0x11, 0x00};
static char config_CMD_MODE[2] = {0x40, 0x01};
static char config_WRTXHT[7] = {0x92, 0x16, 0x08, 0x08, 0x00, 0x01, 0xe0};
static char config_WRTXVT[7] = {0x8b, 0x02, 0x02, 0x02, 0x00, 0x03, 0x60};
static char config_PLL2NR[2] = {0xa0, 0x24};
static char config_PLL2NF1[2] = {0xa2, 0xd0};
static char config_PLL2NF2[2] = {0xa4, 0x00};
static char config_PLL2BWADJ1[2] = {0xa6, 0xd0};
static char config_PLL2BWADJ2[2] = {0xa8, 0x00};
static char config_PLL2CTL[2] = {0xaa, 0x00};
static char config_DBICBR[2] = {0x48, 0x03};
static char config_DBICTYPE[2] = {0x49, 0x00};
static char config_DBICSET1[2] = {0x4a, 0x1c};
static char config_DBICADD[2] = {0x4b, 0x00};
static char config_DBICCTL[2] = {0x4e, 0x01};
/* static char config_COLMOD_565[2] = {0x3a, 0x05}; */
/* static char config_COLMOD_666PACK[2] = {0x3a, 0x06}; */
static char config_COLMOD_888[2] = {0x3a, 0x07};
static char config_MADCTL[2] = {0x36, 0x00};
static char config_DBIOC[2] = {0x82, 0x40};
static char config_CASET[7] = {0x2a, 0x00, 0x00, 0x00, 0x00, 0x01, 0xdf };
static char config_PASET[7] = {0x2b, 0x00, 0x00, 0x00, 0x00, 0x03, 0x5f };
static char config_TXON[2] = {0x81, 0x00};
static char config_BLSET_TM[2] = {0xff, 0x6c};
static char config_DSIRXCTL[2] = {0x41, 0x01};
static char config_TEON[2] = {0x35, 0x00};
static char config_TEOFF[1] = {0x34};

static char config_AGCPSCTL_TM[2] = {0x56, 0x08};

static char config_DBICADD70[2] = {0x4b, 0x70};
static char config_DBICSET_15[2] = {0x4a, 0x15};
static char config_DBICADD72[2] = {0x4b, 0x72};

static char config_Power_Ctrl_2a_cmd[3] = {0x4c, 0x40, 0x10};
static char config_Auto_Sequencer_Setting_a_cmd[3] = {0x4c, 0x00, 0x00};
static char Driver_Output_Ctrl_indx[3] = {0x4c, 0x00, 0x01};
static char Driver_Output_Ctrl_cmd[3] = {0x4c, 0x03, 0x10};
static char config_LCD_drive_AC_Ctrl_indx[3] = {0x4c, 0x00, 0x02};
static char config_LCD_drive_AC_Ctrl_cmd[3] = {0x4c, 0x01, 0x00};
static char config_Entry_Mode_indx[3] = {0x4c, 0x00, 0x03};
static char config_Entry_Mode_cmd[3] = {0x4c, 0x00, 0x00};
static char config_Display_Ctrl_1_indx[3] = {0x4c, 0x00, 0x07};
static char config_Display_Ctrl_1_cmd[3] = {0x4c, 0x00, 0x00};
static char config_Display_Ctrl_2_indx[3] = {0x4c, 0x00, 0x08};
static char config_Display_Ctrl_2_cmd[3] = {0x4c, 0x00, 0x04};
static char config_Display_Ctrl_3_indx[3] = {0x4c, 0x00, 0x09};
static char config_Display_Ctrl_3_cmd[3] = {0x4c, 0x00, 0x0c};
static char config_Display_IF_Ctrl_1_indx[3] = {0x4c, 0x00, 0x0c};
static char config_Display_IF_Ctrl_1_cmd[3] = {0x4c, 0x40, 0x10};
static char config_Display_IF_Ctrl_2_indx[3] = {0x4c, 0x00, 0x0e};
static char config_Display_IF_Ctrl_2_cmd[3] = {0x4c, 0x00, 0x00};

static char config_Panel_IF_Ctrl_1_indx[3] = {0x4c, 0x00, 0x20};
static char config_Panel_IF_Ctrl_1_cmd[3] = {0x4c, 0x01, 0x3f};
static char config_Panel_IF_Ctrl_3_indx[3] = {0x4c, 0x00, 0x22};
static char config_Panel_IF_Ctrl_3_cmd[3] = {0x4c, 0x76, 0x00};
static char config_Panel_IF_Ctrl_4_indx[3] = {0x4c, 0x00, 0x23};
static char config_Panel_IF_Ctrl_4_cmd[3] = {0x4c, 0x1c, 0x0a};
static char config_Panel_IF_Ctrl_5_indx[3] = {0x4c, 0x00, 0x24};
static char config_Panel_IF_Ctrl_5_cmd[3] = {0x4c, 0x1c, 0x2c};
static char config_Panel_IF_Ctrl_6_indx[3] = {0x4c, 0x00, 0x25};
static char config_Panel_IF_Ctrl_6_cmd[3] = {0x4c, 0x1c, 0x4e};
static char config_Panel_IF_Ctrl_8_indx[3] = {0x4c, 0x00, 0x27};
static char config_Panel_IF_Ctrl_8_cmd[3] = {0x4c, 0x00, 0x00};
static char config_Panel_IF_Ctrl_9_indx[3] = {0x4c, 0x00, 0x28};
static char config_Panel_IF_Ctrl_9_cmd[3] = {0x4c, 0x76, 0x0c};


static char config_gam_adjust_00_indx[3] = {0x4c, 0x03, 0x00};
static char config_gam_adjust_00_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_01_indx[3] = {0x4c, 0x03, 0x01};
static char config_gam_adjust_01_cmd[3] = {0x4c, 0x05, 0x02};
static char config_gam_adjust_02_indx[3] = {0x4c, 0x03, 0x02};
static char config_gam_adjust_02_cmd[3] = {0x4c, 0x07, 0x05};
static char config_gam_adjust_03_indx[3] = {0x4c, 0x03, 0x03};
static char config_gam_adjust_03_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_04_indx[3] = {0x4c, 0x03, 0x04};
static char config_gam_adjust_04_cmd[3] = {0x4c, 0x02, 0x00};
static char config_gam_adjust_05_indx[3] = {0x4c, 0x03, 0x05};
static char config_gam_adjust_05_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_06_indx[3] = {0x4c, 0x03, 0x06};
static char config_gam_adjust_06_cmd[3] = {0x4c, 0x10, 0x10};
static char config_gam_adjust_07_indx[3] = {0x4c, 0x03, 0x07};
static char config_gam_adjust_07_cmd[3] = {0x4c, 0x02, 0x02};
static char config_gam_adjust_08_indx[3] = {0x4c, 0x03, 0x08};
static char config_gam_adjust_08_cmd[3] = {0x4c, 0x07, 0x04};
static char config_gam_adjust_09_indx[3] = {0x4c, 0x03, 0x09};
static char config_gam_adjust_09_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_0A_indx[3] = {0x4c, 0x03, 0x0a};
static char config_gam_adjust_0A_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_0B_indx[3] = {0x4c, 0x03, 0x0b};
static char config_gam_adjust_0B_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_0C_indx[3] = {0x4c, 0x03, 0x0c};
static char config_gam_adjust_0C_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_0D_indx[3] = {0x4c, 0x03, 0x0d};
static char config_gam_adjust_0D_cmd[3] = {0x4c, 0x10, 0x10};
static char config_gam_adjust_10_indx[3] = {0x4c, 0x03, 0x10};
static char config_gam_adjust_10_cmd[3] = {0x4c, 0x01, 0x04};
static char config_gam_adjust_11_indx[3] = {0x4c, 0x03, 0x11};
static char config_gam_adjust_11_cmd[3] = {0x4c, 0x05, 0x03};
static char config_gam_adjust_12_indx[3] = {0x4c, 0x03, 0x12};
static char config_gam_adjust_12_cmd[3] = {0x4c, 0x03, 0x04};
static char config_gam_adjust_15_indx[3] = {0x4c, 0x03, 0x15};
static char config_gam_adjust_15_cmd[3] = {0x4c, 0x03, 0x04};
static char config_gam_adjust_16_indx[3] = {0x4c, 0x03, 0x16};
static char config_gam_adjust_16_cmd[3] = {0x4c, 0x03, 0x1c};
static char config_gam_adjust_17_indx[3] = {0x4c, 0x03, 0x17};
static char config_gam_adjust_17_cmd[3] = {0x4c, 0x02, 0x04};
static char config_gam_adjust_18_indx[3] = {0x4c, 0x03, 0x18};
static char config_gam_adjust_18_cmd[3] = {0x4c, 0x04, 0x02};
static char config_gam_adjust_19_indx[3] = {0x4c, 0x03, 0x19};
static char config_gam_adjust_19_cmd[3] = {0x4c, 0x03, 0x05};
static char config_gam_adjust_1C_indx[3] = {0x4c, 0x03, 0x1c};
static char config_gam_adjust_1C_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_1D_indx[3] = {0x4c, 0x03, 0x1D};
static char config_gam_adjust_1D_cmd[3] = {0x4c, 0x02, 0x1f};
static char config_gam_adjust_20_indx[3] = {0x4c, 0x03, 0x20};
static char config_gam_adjust_20_cmd[3] = {0x4c, 0x05, 0x07};
static char config_gam_adjust_21_indx[3] = {0x4c, 0x03, 0x21};
static char config_gam_adjust_21_cmd[3] = {0x4c, 0x06, 0x04};
static char config_gam_adjust_22_indx[3] = {0x4c, 0x03, 0x22};
static char config_gam_adjust_22_cmd[3] = {0x4c, 0x04, 0x05};
static char config_gam_adjust_27_indx[3] = {0x4c, 0x03, 0x27};
static char config_gam_adjust_27_cmd[3] = {0x4c, 0x02, 0x03};
static char config_gam_adjust_28_indx[3] = {0x4c, 0x03, 0x28};
static char config_gam_adjust_28_cmd[3] = {0x4c, 0x03, 0x00};
static char config_gam_adjust_29_indx[3] = {0x4c, 0x03, 0x29};
static char config_gam_adjust_29_cmd[3] = {0x4c, 0x00, 0x02};

static char config_Power_Ctrl_1_indx[3] = {0x4c, 0x01, 0x00};
static char config_Power_Ctrl_1b_cmd[3] = {0x4c, 0x36, 0x3c};
static char config_Power_Ctrl_2_indx[3] = {0x4c, 0x01, 0x01};
static char config_Power_Ctrl_2b_cmd[3] = {0x4c, 0x40, 0x03};
static char config_Power_Ctrl_3_indx[3] = {0x4c, 0x01, 0x02};
static char config_Power_Ctrl_3a_cmd[3] = {0x4c, 0x00, 0x01};
static char config_Power_Ctrl_4_indx[3] = {0x4c, 0x01, 0x03};
static char config_Power_Ctrl_4a_cmd[3] = {0x4c, 0x3c, 0x58};
static char config_Power_Ctrl_6_indx[3] = {0x4c, 0x01, 0x0c};
static char config_Power_Ctrl_6a_cmd[3] = {0x4c, 0x01, 0x35};

static char config_Auto_Sequencer_Setting_b_cmd[3] = {0x4c, 0x00, 0x02};

static char config_Panel_IF_Ctrl_10_indx[3] = {0x4c, 0x00, 0x29};
static char config_Panel_IF_Ctrl_10a_cmd[3] = {0x4c, 0x03, 0xbf};
static char config_Auto_Sequencer_Setting_indx[3] = {0x4c, 0x01, 0x06};
static char config_Auto_Sequencer_Setting_c_cmd[3] = {0x4c, 0x00, 0x03};
static char config_Power_Ctrl_2c_cmd[3] = {0x4c, 0x40, 0x10};

static char config_VIDEO[2] = {0x40, 0x00};

static char config_Panel_IF_Ctrl_10_indx_off[3] = {0x4C, 0x00, 0x29};

static char config_Panel_IF_Ctrl_10b_cmd_off[3] = {0x4C, 0x00, 0x02};

static char config_Power_Ctrl_1a_cmd[3] = {0x4C, 0x30, 0x00};

static struct dsi_cmd_desc renesas_sleep_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(config_sleep_out), config_sleep_out }
};

static struct dsi_cmd_desc renesas_display_off_cmds[] = {
	/* Choosing Command Mode */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CMD_MODE), config_CMD_MODE },

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_b_cmd),
			config_Auto_Sequencer_Setting_b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY * 2,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	/* After waiting >= 5 frames, turn OFF RGB signals
	This is done by on DSI/MDP (depends on Vid/Cmd Mode.  */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_a_cmd),
			config_Auto_Sequencer_Setting_a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10_indx_off),
			config_Panel_IF_Ctrl_10_indx_off},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10b_cmd_off),
				config_Panel_IF_Ctrl_10b_cmd_off},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1_indx),
				config_Power_Ctrl_1_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1a_cmd),
				config_Power_Ctrl_1a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_TEOFF), config_TEOFF},
};

static struct dsi_cmd_desc renesas_display_on_cmds[] = {
	/* Choosing Command Mode */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CMD_MODE), config_CMD_MODE },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXHT), config_WRTXHT },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXVT), config_WRTXVT },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2NR), config_PLL2NR },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2NF1), config_PLL2NF1 },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2NF2), config_PLL2NF2 },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2BWADJ1), config_PLL2BWADJ1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2BWADJ2), config_PLL2BWADJ2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2CTL), config_PLL2CTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICBR), config_DBICBR},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICTYPE), config_DBICTYPE},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET1), config_DBICSET1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD), config_DBICADD},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICCTL), config_DBICCTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_COLMOD_888), config_COLMOD_888},
	/* Choose config_COLMOD_565 or config_COLMOD_666PACK for other modes */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_MADCTL), config_MADCTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBIOC), config_DBIOC},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CASET), config_CASET},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PASET), config_PASET},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DSIRXCTL), config_DSIRXCTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_TEON), config_TEON},
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_TXON), config_TXON},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_BLSET_TM), config_BLSET_TM},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_AGCPSCTL_TM), config_AGCPSCTL_TM},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1_indx), config_Power_Ctrl_1_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1a_cmd), config_Power_Ctrl_1a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2_indx), config_Power_Ctrl_2_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2a_cmd), config_Power_Ctrl_2a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_a_cmd),
			config_Auto_Sequencer_Setting_a_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(Driver_Output_Ctrl_indx), Driver_Output_Ctrl_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(Driver_Output_Ctrl_cmd),
			Driver_Output_Ctrl_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_LCD_drive_AC_Ctrl_indx),
			config_LCD_drive_AC_Ctrl_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_LCD_drive_AC_Ctrl_cmd),
			config_LCD_drive_AC_Ctrl_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Entry_Mode_indx),
			config_Entry_Mode_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Entry_Mode_cmd),
			config_Entry_Mode_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_1_indx),
			config_Display_Ctrl_1_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_1_cmd),
			config_Display_Ctrl_1_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_2_indx),
			config_Display_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_2_cmd),
			config_Display_Ctrl_2_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_3_indx),
			config_Display_Ctrl_3_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_3_cmd),
			config_Display_Ctrl_3_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_1_indx),
			config_Display_IF_Ctrl_1_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_1_cmd),
			config_Display_IF_Ctrl_1_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_2_indx),
			config_Display_IF_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_2_cmd),
			config_Display_IF_Ctrl_2_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_1_indx),
			config_Panel_IF_Ctrl_1_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_1_cmd),
			config_Panel_IF_Ctrl_1_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_3_indx),
			config_Panel_IF_Ctrl_3_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_3_cmd),
			config_Panel_IF_Ctrl_3_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_4_indx),
			config_Panel_IF_Ctrl_4_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_4_cmd),
			config_Panel_IF_Ctrl_4_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_5_indx),
			config_Panel_IF_Ctrl_5_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_5_cmd),
			config_Panel_IF_Ctrl_5_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_6_indx),
			config_Panel_IF_Ctrl_6_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_6_cmd),
			config_Panel_IF_Ctrl_6_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_8_indx),
			config_Panel_IF_Ctrl_8_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_8_cmd),
			config_Panel_IF_Ctrl_8_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_9_indx),
			config_Panel_IF_Ctrl_9_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_9_cmd),
			config_Panel_IF_Ctrl_9_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_00_indx),
			config_gam_adjust_00_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_00_cmd),
			config_gam_adjust_00_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_01_indx),
			config_gam_adjust_01_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_01_cmd),
			config_gam_adjust_01_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_02_indx),
			config_gam_adjust_02_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_02_cmd),
			config_gam_adjust_02_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_03_indx),
			config_gam_adjust_03_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_03_cmd),
			config_gam_adjust_03_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_04_indx), config_gam_adjust_04_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_04_cmd), config_gam_adjust_04_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},


	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_05_indx), config_gam_adjust_05_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_05_cmd), config_gam_adjust_05_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_06_indx), config_gam_adjust_06_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_06_cmd), config_gam_adjust_06_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_07_indx), config_gam_adjust_07_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_07_cmd), config_gam_adjust_07_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_08_indx), config_gam_adjust_08_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_08_cmd), config_gam_adjust_08_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_09_indx), config_gam_adjust_09_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_09_cmd), config_gam_adjust_09_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0A_indx), config_gam_adjust_0A_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0A_cmd), config_gam_adjust_0A_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0B_indx), config_gam_adjust_0B_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0B_cmd), config_gam_adjust_0B_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0C_indx), config_gam_adjust_0C_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0C_cmd), config_gam_adjust_0C_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0D_indx), config_gam_adjust_0D_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0D_cmd), config_gam_adjust_0D_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_10_indx), config_gam_adjust_10_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_10_cmd), config_gam_adjust_10_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_11_indx), config_gam_adjust_11_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_11_cmd), config_gam_adjust_11_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_12_indx), config_gam_adjust_12_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_12_cmd), config_gam_adjust_12_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_15_indx), config_gam_adjust_15_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_15_cmd), config_gam_adjust_15_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_16_indx), config_gam_adjust_16_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_16_cmd), config_gam_adjust_16_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_17_indx), config_gam_adjust_17_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_17_cmd), config_gam_adjust_17_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_18_indx), config_gam_adjust_18_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_18_cmd), config_gam_adjust_18_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_19_indx), config_gam_adjust_19_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_19_cmd), config_gam_adjust_19_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1C_indx), config_gam_adjust_1C_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1C_cmd), config_gam_adjust_1C_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1D_indx), config_gam_adjust_1D_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1D_cmd), config_gam_adjust_1D_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_20_indx), config_gam_adjust_20_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_20_cmd), config_gam_adjust_20_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_21_indx), config_gam_adjust_21_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_21_cmd), config_gam_adjust_21_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},


	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_22_indx), config_gam_adjust_22_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_22_cmd), config_gam_adjust_22_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_27_indx), config_gam_adjust_27_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_27_cmd), config_gam_adjust_27_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_28_indx), config_gam_adjust_28_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_28_cmd), config_gam_adjust_28_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_29_indx), config_gam_adjust_29_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_29_cmd), config_gam_adjust_29_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1_indx), config_Power_Ctrl_1_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1b_cmd), config_Power_Ctrl_1b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2_indx), config_Power_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2b_cmd), config_Power_Ctrl_2b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_3_indx), config_Power_Ctrl_3_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_3a_cmd), config_Power_Ctrl_3a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_4_indx), config_Power_Ctrl_4_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_4a_cmd), config_Power_Ctrl_4a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_6_indx), config_Power_Ctrl_6_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_6a_cmd), config_Power_Ctrl_6a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_b_cmd),
			config_Auto_Sequencer_Setting_b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10_indx),
			config_Panel_IF_Ctrl_10_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10a_cmd),
			config_Panel_IF_Ctrl_10a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_c_cmd),
			config_Auto_Sequencer_Setting_c_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2_indx),
			config_Power_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2c_cmd),
			config_Power_Ctrl_2c_cmd},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0/* RENESAS_CMD_DELAY */,
		sizeof(config_DBICSET_15), config_DBICSET_15},

};

static char config_WRTXHT2[7] = {0x92, 0x15, 0x05, 0x0F, 0x00, 0x01, 0xe0};
static char config_WRTXVT2[7] = {0x8b, 0x14, 0x01, 0x14, 0x00, 0x03, 0x60};

static struct dsi_cmd_desc renesas_hvga_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXHT2), config_WRTXHT2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXVT2), config_WRTXVT2},
};

static struct dsi_cmd_desc renesas_video_on_cmds[] = {
{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_VIDEO), config_VIDEO}
};

static struct dsi_cmd_desc renesas_cmd_on_cmds[] = {
{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CMD_MODE), config_CMD_MODE},
};

static int mipi_renesas_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	mipi  = &mfd->panel_info.mipi;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_sleep_off_cmds,
			ARRAY_SIZE(renesas_sleep_off_cmds));

	mipi_set_tx_power_mode(1);
	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_display_on_cmds,
			ARRAY_SIZE(renesas_display_on_cmds));

	if (cpu_is_msm7x25a() || cpu_is_msm7x25aa() || cpu_is_msm7x25ab()) {
		mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_hvga_on_cmds,
			ARRAY_SIZE(renesas_hvga_on_cmds));
	}

	if (mipi->mode == DSI_VIDEO_MODE)
		mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_video_on_cmds,
			ARRAY_SIZE(renesas_video_on_cmds));
	else
		mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_cmd_on_cmds,
			ARRAY_SIZE(renesas_cmd_on_cmds));
	mipi_set_tx_power_mode(0);

	return 0;
}

static int mipi_renesas_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_display_off_cmds,
			ARRAY_SIZE(renesas_display_off_cmds));

	return 0;
}

static int __devinit mipi_renesas_lcd_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		mipi_renesas_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}

static void mipi_renesas_set_backlight(struct msm_fb_data_type *mfd)
{
	int ret = -EPERM;
	int bl_level;

	bl_level = mfd->bl_level;

	if (mipi_renesas_pdata && mipi_renesas_pdata->pmic_backlight)
		ret = mipi_renesas_pdata->pmic_backlight(bl_level);
	else
		pr_err("%s(): Backlight level set failed", __func__);
}

static struct platform_driver this_driver = {
	.probe  = mipi_renesas_lcd_probe,
	.driver = {
		.name   = "mipi_renesas",
	},
};

static struct msm_fb_panel_data renesas_panel_data = {
	.on		= mipi_renesas_lcd_on,
	.off	= mipi_renesas_lcd_off,
	.set_backlight = mipi_renesas_set_backlight,
};

static int ch_used[3];

int mipi_renesas_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;
	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_renesas_lcd_init();
	if (ret) {
		pr_err("mipi_renesas_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_renesas", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	renesas_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &renesas_panel_data,
		sizeof(renesas_panel_data));
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

static int mipi_renesas_lcd_init(void)
{
	mipi_dsi_buf_alloc(&renesas_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&renesas_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
