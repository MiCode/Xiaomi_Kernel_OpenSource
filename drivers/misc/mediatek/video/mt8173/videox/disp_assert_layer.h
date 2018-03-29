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
#include <linux/types.h>
#ifdef __cplusplus
extern "C" {
#endif

	extern bool is_early_suspended;
	extern struct semaphore sem_early_suspend;
	extern struct mutex OverlaySettingMutex;
	extern atomic_t OverlaySettingDirtyFlag;
	extern atomic_t OverlaySettingApplied;

	typedef enum {
		DAL_STATUS_OK = 0,

		DAL_STATUS_NOT_READY = -1,
		DAL_STATUS_INVALID_ARGUMENT = -2,
		DAL_STATUS_LOCK_FAIL = -3,
		DAL_STATUS_LCD_IN_SUSPEND = -4,
		DAL_STATUS_FATAL_ERROR = -10,
	} DAL_STATUS;


	typedef enum {
		DAL_COLOR_BLACK = 0x000000,
		DAL_COLOR_WHITE = 0xFFFFFF,
		DAL_COLOR_RED = 0xFF0000,
		DAL_COLOR_GREEN = 0x00FF00,
		DAL_COLOR_BLUE = 0x0000FF,
		DAL_COLOR_TURQUOISE = (DAL_COLOR_GREEN | DAL_COLOR_BLUE),
		DAL_COLOR_YELLOW = (DAL_COLOR_RED | DAL_COLOR_GREEN),
		DAL_COLOR_PINK = (DAL_COLOR_RED | DAL_COLOR_BLUE),
	} DAL_COLOR;


/* Display Assertion Layer API */

	unsigned int DAL_GetLayerSize(void);
	int is_DAL_Enabled(void);
	DAL_STATUS DAL_Init(unsigned long layerVA, unsigned long layerPA);
	DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor);
	DAL_STATUS DAL_Clean(void);
	DAL_STATUS DAL_Printf(const char *fmt, ...);
	DAL_STATUS DAL_OnDispPowerOn(void);
	DAL_STATUS DAL_LowMemoryOn(void);
	DAL_STATUS DAL_LowMemoryOff(void);
	DAL_STATUS DAL_SetScreenColor(DAL_COLOR color);
#ifdef __cplusplus
}
#endif
#endif				/* __DISP_ASSERT_LAYER_H__ */
