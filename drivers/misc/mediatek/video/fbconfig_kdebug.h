/*
 * File: drivers/video/omap_new/debug.c
 *
 * Debug support for the omapfb driver
 *
 * Copyright (C) 2004 Nokia Corporation
 * Author: Imre Deak <imre.deak@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __FBCONFIG_KDEBUG_H
#define __FBCONFIG_KDEBUG_H
void ConfigPara_Init(void);
void ConfigPara_Deinit(void);
int fb_config_execute_cmd(void);
int fbconfig_get_esd_check_exec(void);
BOOL get_fbconfig_start_lcm_config(void);


/* *****************debug for fbconfig tool in kernel part************* */
#define MAX_INSTRUCTION 35

typedef enum {
	RECORD_CMD = 0,
	RECORD_MS = 1,
	RECORD_PIN_SET = 2,
} RECORD_TYPE;

typedef struct CONFIG_RECORD {
	struct CONFIG_RECORD *next;
	RECORD_TYPE type;	/* msleep;cmd;setpin;resetpin. */
	int ins_num;
	int ins_array[MAX_INSTRUCTION];
} CONFIG_RECORD;


typedef enum {
	HS_PRPR = 0,
	HS_ZERO = 1,
	HS_TRAIL = 2,
	TA_GO = 3,
	TA_SURE = 4,
	TA_GET = 5,
	DA_HS_EXIT = 6,
	CLK_ZERO = 7,
	CLK_TRAIL = 8,
	CONT_DET = 9,
	CLK_HS_PRPR = 10,
	CLK_HS_POST = 11,
	CLK_HS_EXIT = 12,
	HPW = 13,
	HFP = 14,
	HBP = 15,
	VPW = 16,
	VFP = 17,
	VBP = 18,
	LPX = 19,
	SSC_EN = 0xFE,
	MAX = 0XFF,
} MIPI_SETTING_TYPE;

typedef struct MIPI_TIMING {
	MIPI_SETTING_TYPE type;
	unsigned int value;
} MIPI_TIMING;

typedef struct FBCONFIG_LAYER_INFO {
	int layer_enable[4];	/* layer id :0 1 2 3 */
	unsigned int layer_size[3];
} FBCONFIG_LAYER_INFO;

typedef struct ESD_PARA {
	int addr;
	int type;
	int para_num;
	char *esd_ret_buffer;
} ESD_PARA;

typedef struct LAYER_H_SIZE {
	int layer_size;
	int height;
	int fmt;
} LAYER_H_SIZE;

typedef struct MIPI_CLK_V2 {
	unsigned char div1;
	unsigned char div2;
	unsigned short fbk_div;
} MIPI_CLK_V2;

typedef struct LCM_TYPE_FB {
	int clock;
	int lcm_type;
} LCM_TYPE_FB;

typedef struct LCM_REG_READ {
	int check_addr;
	int check_para_num;
	int check_type;
	char *check_buffer;
} LCM_REG_READ;

typedef struct {
	void (*set_cmd_mode) (void);
	int (*set_mipi_clk) (unsigned int clk);
	void (*set_dsi_post) (void);
	void (*set_lane_num) (unsigned int lane_num);
	void (*set_mipi_timing) (MIPI_TIMING timing);
	void (*set_te_enable) (char enable);
	void (*set_continuous_clock) (int enable);
	int (*set_spread_frequency) (unsigned int clk);
	int (*set_get_misc) (const char *name, void *parameter);

} FBCONFIG_DISP_IF;

FBCONFIG_DISP_IF *disphal_fbconfig_get_def_if(void);

#endif				/* __FBCONFIG_KDEBUG_H */
