/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __FBCONFIG_KDEBUG_H
#define __FBCONFIG_KDEBUG_H

#include <linux/types.h>
#include "ddp_ovl.h"

void PanelMaster_Init(void);
void PanelMaster_Deinit(void);
int fb_config_execute_cmd(void);
int fbconfig_get_esd_check_exec(void);
extern int m4u_query_mva_info(unsigned int domain_idx, unsigned int mva,
			      unsigned int size, unsigned int *real_mva,
			      unsigned int *real_size);
#ifndef TOTAL_OVL_LAYER_NUM
#define TOTAL_OVL_LAYER_NUM OVL_LAYER_NUM
#endif

#define MAX_INSTRUCTION 35
#define NUM_OF_DSI 1

enum RECORD_TYPE {
	RECORD_CMD = 0,
	RECORD_MS = 1,
	RECORD_PIN_SET = 2,
};

enum DSI_INDEX {
	PM_DSI0 = 0,
	PM_DSI1 = 1,
	PM_DSI_DUAL = 2,
	PM_DSI_MAX = 0XFF,
};


struct CONFIG_RECORD {
	enum RECORD_TYPE type;	/* msleep;cmd;setpin;resetpin. */
	int ins_num;
	int ins_array[MAX_INSTRUCTION];
};

struct CONFIG_RECORD_LIST {
	struct CONFIG_RECORD record;
	struct list_head list;
};

enum MIPI_SETTING_TYPE {
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
};

struct MIPI_TIMING {
	enum MIPI_SETTING_TYPE type;
	unsigned int value;
};

struct SETTING_VALUE {
	enum DSI_INDEX dsi_index;
	unsigned int value[NUM_OF_DSI];
};

struct PM_LAYER_EN {
	int layer_en[TOTAL_OVL_LAYER_NUM];
};

struct PM_LAYER_INFO {
	int index;
	int height;
	int width;
	int fmt;
	unsigned int layer_size;
};

struct ESD_PARA {
	int addr;
	int type;
	int para_num;
	char *esd_ret_buffer;
};
#if 0
struct LAYER_H_SIZE {
	int layer_size;
	int height;
	int fmt;
};
#endif
struct MIPI_CLK_V2 {
	unsigned char div1;
	unsigned char div2;
	unsigned short fbk_div;
};

struct LCM_TYPE_FB {
	int clock;
	int lcm_type;
};

struct DSI_RET {
	int dsi[NUM_OF_DSI];	/* for there are totally 2 dsi. */
};


struct LCM_REG_READ {
	int check_addr;
	int check_para_num;
	int check_type;
	char *check_buffer;
};

struct FBCONFIG_DISP_IF {
	void (*set_cmd_mode)(void);
	int (*set_mipi_clk)(unsigned int clk);
	void (*set_dsi_post)(void);
	void (*set_lane_num)(unsigned int lane_num);
	void (*set_mipi_timing)(struct MIPI_TIMING timing);
	void (*set_te_enable)(char enable);
	void (*set_continuous_clock)(int enable);
	int (*set_spread_frequency)(unsigned int clk);
	int (*set_get_misc)(const char *name, void *parameter);

};

struct misc_property {
	unsigned int dual_port:1;
	unsigned int overall_layer_num:5;
	unsigned int reserved:26;
};

void Panel_Master_DDIC_config(void);
int fbconfig_get_esd_check(enum DSI_INDEX dsi_id, uint32_t cmd,
						uint8_t *buffer, uint32_t num);

#include <linux/uaccess.h>
#include <linux/compat.h>

#ifdef CONFIG_COMPAT

struct compat_lcm_type_fb {
	compat_int_t clock;
	compat_int_t lcm_type;
};

struct compat_config_record {
	compat_int_t type;	/* msleep;cmd;setpin;resetpin. */
	compat_int_t ins_num;
	compat_int_t ins_array[MAX_INSTRUCTION];
};

struct compat_dsi_ret {
	compat_int_t dsi[NUM_OF_DSI];	/* for there are totally 2 dsi. */
};

struct compat_mipi_timing {
	compat_int_t type;
	compat_uint_t value;
};

struct compat_pm_layer_en {
	compat_int_t layer_en[TOTAL_OVL_LAYER_NUM];
};

struct compat_pm_layer_info {
	compat_int_t index;
	compat_int_t height;
	compat_int_t width;
	compat_int_t fmt;
	compat_uint_t layer_size;
};

struct compat_esd_para {
	compat_int_t addr;
	compat_int_t type;
	compat_int_t para_num;
	compat_uint_t esd_ret_buffer;
};

#endif
/* end CONFIG_COMPAT */

#endif
/* __FBCONFIG_KDEBUG_H */
