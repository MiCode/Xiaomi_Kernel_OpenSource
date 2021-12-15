/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_DRM_ASSERT_EXT_H
#define _MTK_DRM_ASSERT_EXT_H

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

int DAL_SetScreenColor(enum DAL_COLOR color);
int DAL_SetColor(unsigned int fgColor, unsigned int bgColor);
int DAL_Clean(void);
int DAL_Printf(const char *fmt, ...);

#endif
