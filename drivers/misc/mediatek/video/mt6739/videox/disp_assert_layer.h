/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __DISP_ASSERT_LAYER_H__
#define __DISP_ASSERT_LAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
#endif /* __DISP_ASSERT_LAYER_H__ */
