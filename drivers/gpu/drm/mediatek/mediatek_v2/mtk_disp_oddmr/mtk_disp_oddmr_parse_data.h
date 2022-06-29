/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_ODDMR_PARSE_DATA_H__
#define __MTK_DISP_ODDMR_PARSE_DATA_H__
//#define APP_DEBUG
#ifdef APP_DEBUG
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
struct mtk_oddmr_panelid {
	uint32_t len;
	uint8_t data[16];
};
#define OD_TABLE_MAX 2
#define DMR_TABLE_MAX 2
#define DMR_GAIN_MAX 15
#define OD_GAIN_MAX 15
#else
#include <linux/types.h>
#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>
#include "mtk_disp_oddmr.h"
#endif
/***************** file parsing ******************/
struct mtk_oddmr_pq_pair {
	uint32_t addr;
	uint32_t value;
};

struct mtk_oddmr_pq_param {
	uint32_t counts;
	struct mtk_oddmr_pq_pair *param;
};

struct mtk_oddmr_table_raw {
	/* size unit 1 byte */
	uint32_t size;
	uint8_t *value;
};

struct mtk_oddmr_table_gain {
	uint32_t item;
	uint32_t value;
};

/***************** od param ******************/
/* od alloc table pq gain_table */
/* od table */
struct mtk_oddmr_od_table_basic_info {
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t dbv;
	uint32_t min_fps;
	uint32_t max_fps;
	uint32_t min_dbv;
	uint32_t max_dbv;
	uint32_t reserved;
};
struct mtk_oddmr_od_table {
	struct mtk_oddmr_od_table_basic_info table_basic_info;
	uint8_t *gain_table_raw;
	struct mtk_oddmr_pq_param pq_od;
	uint32_t fps_cnt;
	struct mtk_oddmr_table_gain fps_table[OD_GAIN_MAX];
	uint32_t bl_cnt;
	struct mtk_oddmr_table_gain bl_table[OD_GAIN_MAX];
	struct mtk_oddmr_table_raw raw_table;
};
struct mtk_oddmr_od_basic_param {
	struct mtk_oddmr_panelid panelid;
	/* 0:AP 1:ddic */
	uint32_t resolution_switch_mode;
	uint32_t panel_width;
	uint32_t panel_height;
	uint32_t table_cnt;
	uint32_t od_mode;
	/* 0:no_dither 1:12to11 2:12to10 */
	uint32_t dither_sel;
	uint32_t dither_ctl;
	/* bit(0) hscaling, bit(1) vscaling */
	uint32_t scaling_mode;
	uint32_t od_hsk_2;
	uint32_t od_hsk_3;
	uint32_t od_hsk_4;
	uint32_t reserved;
};
/* od basic info */
struct mtk_oddmr_od_basic_info {
	struct mtk_oddmr_od_basic_param basic_param;
	struct mtk_oddmr_pq_param basic_pq;
};
struct mtk_oddmr_od_param {
	struct mtk_oddmr_od_basic_info od_basic_info;
	struct mtk_oddmr_od_table *od_tables;
	uint32_t valid_table;
	int valid_table_cnt;
};

/***************** dmr param ******************/
/* dmr alloc table pq */
struct mtk_oddmr_dmr_table_basic_info {
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t dbv;
	uint32_t min_fps;
	uint32_t max_fps;
	uint32_t min_dbv;
	uint32_t max_dbv;
	uint32_t reserved;
};
struct mtk_oddmr_dmr_fps_gain {
	uint32_t fps;
	uint32_t beta;
	uint32_t gain;
	uint32_t offset;
};
struct mtk_oddmr_dmr_table {
	struct mtk_oddmr_dmr_table_basic_info table_basic_info;
	uint32_t fps_cnt;
	struct mtk_oddmr_dmr_fps_gain fps_table[DMR_GAIN_MAX];
	uint32_t bl_cnt;
	struct mtk_oddmr_table_gain bl_table[DMR_GAIN_MAX];
	struct mtk_oddmr_pq_param pq_common;
	struct mtk_oddmr_pq_param pq_single_pipe;
	struct mtk_oddmr_pq_param pq_left_pipe;
	struct mtk_oddmr_pq_param pq_right_pipe;
	struct mtk_oddmr_table_raw raw_table_single;
	struct mtk_oddmr_table_raw raw_table_left;
	struct mtk_oddmr_table_raw raw_table_right;
};

struct mtk_oddmr_dmr_basic_param {
	struct mtk_oddmr_panelid panelid;
	/* 0:AP 1:ddic */
	uint32_t resolution_switch_mode;
	uint32_t panel_width;
	uint32_t panel_height;
	uint32_t is_second_dmr;
	uint32_t table_cnt;
	uint32_t dmr_table_mode;
	/* 0:no_dither 1:12to11 2:12to10 */
	uint32_t dither_sel;
	uint32_t dither_ctl;
	uint32_t reserved;
};
/* dmr basic info */
struct mtk_oddmr_dmr_basic_info {
	struct mtk_oddmr_dmr_basic_param basic_param;
	struct mtk_oddmr_pq_param basic_pq;
};

struct mtk_oddmr_dmr_param {
	struct mtk_oddmr_dmr_basic_info dmr_basic_info;
	struct mtk_oddmr_dmr_table *dmr_tables;
	uint32_t valid_table;
	int valid_table_cnt;
};

enum MTK_ODDMR_PARAM_DATA_TYPE {
	ODDMR_DMR_BASIC_INFO = 0x01,
	ODDMR_DMR_TABLE = 0x02,
	ODDMR_OD_BASIC_INFO = 0x03,
	ODDMR_OD_TABLE = 0x04,
};
#define ODDMR_SECTION_WHOLE 0
#define ODDMR_SECTION_END 0xFEFE
enum MTK_ODDMR_DMR_BASIC_SUB_ID {
	DMR_BASIC_WHOLE = ODDMR_SECTION_WHOLE,
	DMR_BASIC_PARAM = 0x0100,
	DMR_BASIC_PQ = 0x0200,
	DMR_BASIC_END = ODDMR_SECTION_END,
};
enum MTK_ODDMR_DMR_TABLE_SUB_ID {
	DMR_TABLE_WHOLE = ODDMR_SECTION_WHOLE,
	DMR_TABLE_BASIC_INFO = 0x0100,
	DMR_TABLE_FPS_GAIN_TABLE = 0x0200,
	DMR_TABLE_BL_GAIN_TABLE = 0x0300,
	DMR_TABLE_PQ_COMMON = 0x0400,
	DMR_TABLE_PQ_SINGLE = 0x0500,
	DMR_TABLE_DATA_SINGLE = 0x0600,
	DMR_TABLE_PQ_LEFT = 0x0700,
	DMR_TABLE_DATA_LEFT = 0x0800,
	DMR_TABLE_PQ_RIGHT = 0x0900,
	DMR_TABLE_DATA_RIGHT = 0x0A00,
	DMR_TABLE_END = ODDMR_SECTION_END,
};
enum MTK_ODDMR_OD_BASIC_SUB_ID {
	OD_BASIC_WHOLE = ODDMR_SECTION_WHOLE,
	OD_BASIC_PARAM = 0x0100,
	OD_BASIC_PQ = 0x0200,
	OD_BASIC_END = ODDMR_SECTION_END,
};

enum MTK_ODDMR_OD_TABLE_SUB_ID {
	OD_TABLE_WHOLE = ODDMR_SECTION_WHOLE,
	OD_TABLE_BASIC_INFO = 0x0100,
	OD_TABLE_GAIN_TABLE = 0x0200,
	OD_TABLE_PQ_OD = 0x0300,
	OD_TABLE_FPS_GAIN_TABLE = 0x0400,
	OD_TABLE_DBV_GAIN_TABLE = 0x0500,
	OD_TABLE_DATA = 0x0600,
	OD_TABLE_END = ODDMR_SECTION_END,
};
int mtk_oddmr_load_param(struct mtk_disp_oddmr *priv, struct mtk_drm_oddmr_param *param);
extern struct mtk_oddmr_dmr_param g_dmr_param;
extern struct mtk_oddmr_od_param g_od_param;
extern int is_dmr_basic_info_loaded;
extern int is_od_basic_info_loaded;
#endif
