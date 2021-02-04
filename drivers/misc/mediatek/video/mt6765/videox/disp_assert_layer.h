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

#ifndef __DISP_ASSERT_LAYER_H__
#define __DISP_ASSERT_LAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mtkfb_console.h"
enum DAL_STATUS {
	DAL_STATUS_OK = 0,

	DAL_STATUS_NOT_READY = -1,
	DAL_STATUS_INVALID_ARGUMENT = -2,
	DAL_STATUS_LOCK_FAIL = -3,
	DAL_STATUS_LCD_IN_SUSPEND = -4,
	DAL_STATUS_FATAL_ERROR = -10,
};


enum DAL_COLOR {
	DAL_COLOR_BLACK = 0x000000,
	DAL_COLOR_WHITE = 0xFFFFFF,
	DAL_COLOR_RED = 0xFF0000,
	DAL_COLOR_GREEN = 0x00FF00,
	DAL_COLOR_BLUE = 0x0000FF,
	DAL_COLOR_TURQUOISE = (DAL_COLOR_GREEN | DAL_COLOR_BLUE),
	DAL_COLOR_YELLOW = (DAL_COLOR_RED | DAL_COLOR_GREEN),
	DAL_COLOR_PINK = (DAL_COLOR_RED | DAL_COLOR_BLUE),
	DAL_COLOR_STEEL_BLUE = 0x4682B4,
	DAL_COLOR_OLIVE_GREEN = 0xCAFF70,
	DAL_COLOR_DARK_CYAN = 0x008B8B,
	DAL_COLOR_MAROON = 0x8B1C62,
	DAL_COLOR_CORNSILK = 0x8B8878,
	DAL_COLOR_OPAQUE = 0x555555,
};

struct Layer_draw_info {
	int layer_num;
	int dx[12];
	int dy[12];
	int top_l_x[12];
	int top_l_y[12];
	int bot_r_x[12];
	int bot_r_y[12];
	int frame_width[12];
};

enum SHOW_LAYER_COLOR {
	SHOW_LAYER_COLOR_BLACK = 0xFF000000,
	SHOW_LAYER_COLOR_WHITE = 0xFFFFFFFF,
	SHOW_LAYER_COLOR_RED = 0xFFFF0000,
	SHOW_LAYER_COLOR_GREEN = 0xFF00FF00,
	SHOW_LAYER_COLOR_BLUE = 0xFF0000FF,
	SHOW_LAYER_COLOR_TURQUOISE =
		(SHOW_LAYER_COLOR_GREEN | SHOW_LAYER_COLOR_BLUE),
	SHOW_LAYER_COLOR_YELLOW =
		(SHOW_LAYER_COLOR_RED | SHOW_LAYER_COLOR_GREEN),
	SHOW_LAYER_COLOR_PINK =
		(SHOW_LAYER_COLOR_RED | SHOW_LAYER_COLOR_BLUE),
	SHOW_LAYER_COLOR_STEEL_BLUE = 0xFF4682B4,
	SHOW_LAYER_COLOR_OLIVE_GREEN = 0xFFCAFF70,
	SHOW_LAYER_COLOR_DARK_CYAN = 0xFF008B8B,
	SHOW_LAYER_COLOR_MAROON = 0xFF8B1C62,
	SHOW_LAYER_COLOR_CORNSILK = 0xFF8B8878,
	SHOW_LAYER_COLOR_OPAQUE = 0xFFFFFFFF,
};
/* Display Assertion Layer API */

unsigned int DAL_GetLayerSize(void);
enum DAL_STATUS DAL_SetScreenColor(enum DAL_COLOR color);
enum DAL_STATUS DAL_Init(unsigned long layerVA, unsigned long layerPA);
enum DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor);
enum DAL_STATUS DAL_Clean(void);
enum DAL_STATUS DAL_Printf(const char *fmt, ...);
enum DAL_STATUS DAL_OnDispPowerOn(void);
enum DAL_STATUS DAL_LowMemoryOn(void);
enum DAL_STATUS DAL_LowMemoryOff(void);
int is_DAL_Enabled(void);
int show_layers_draw_wdma(struct Layer_draw_info *draw_info);
extern enum MFC_STATUS DAL_CHECK_MFC_RET(enum MFC_STATUS expr);
extern MFC_HANDLE show_mfc_handle;
extern enum DAL_COLOR color_wdma[24];
extern void *show_layers_va;



#ifdef __cplusplus
}
#endif
#endif				/* __DISP_ASSERT_LAYER_H__ */
