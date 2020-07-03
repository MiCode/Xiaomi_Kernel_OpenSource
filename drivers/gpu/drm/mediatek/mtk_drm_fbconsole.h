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

#ifndef __MTK_FB_CONSOLE_H__
#define __MTK_FB_CONSOLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define UINT8 unsigned char
#define UINT32 unsigned int
#define INT32 int
#define BYTE unsigned char

#define MFC_CHECK_RET(expr)                                                    \
	do {                                                                   \
		enum MFC_STATUS ret = (expr);                                  \
		ASSERT(!(ret == MFC_STATUS_OK));                               \
	} while (0)

enum MFC_STATUS {
	MFC_STATUS_OK = 0,

	MFC_STATUS_INVALID_ARGUMENT = -1,
	MFC_STATUS_NOT_IMPLEMENTED = -2,
	MFC_STATUS_OUT_OF_MEMORY = -3,
	MFC_STATUS_LOCK_FAIL = -4,
	MFC_STATUS_FATAL_ERROR = -5,
};

#define MFC_HANDLE void *

struct MFC_CONTEXT {
	struct semaphore sem;

	UINT8 *fb_addr;
	UINT32 fb_width;
	UINT32 fb_height;
	UINT32 fb_bpp;
	UINT32 fg_color;
	UINT32 bg_color;
	UINT32 screen_color;
	UINT32 rows;
	UINT32 cols;
	UINT32 cursor_row;
	UINT32 cursor_col;
	UINT32 font_width;
	UINT32 font_height;
	UINT32 scale;
	/*Avoid Kmemleak scan*/
	struct file *filp;
};

/* MTK Framebuffer Console API */
enum MFC_STATUS MFC_Open(MFC_HANDLE *handle, void *fb_addr,
			 unsigned int fb_width, unsigned int fb_height,
			 unsigned int fb_bpp, unsigned int fg_color,
			 unsigned int bg_color, struct file *filp);

enum MFC_STATUS MFC_Open_Ex(MFC_HANDLE *handle, void *fb_addr,
			    unsigned int fb_width, unsigned int fb_height,
			    unsigned int fb_pitch, unsigned int fb_bpp,
			    unsigned int fg_color, unsigned int bg_color,
			    struct file *filp);

enum MFC_STATUS MFC_Close(MFC_HANDLE handle);

enum MFC_STATUS MFC_SetColor(MFC_HANDLE handle, unsigned int fg_color,
			     unsigned int bg_color);

enum MFC_STATUS MFC_ResetCursor(MFC_HANDLE handle);

enum MFC_STATUS MFC_Print(MFC_HANDLE handle, const char *str);

enum MFC_STATUS MFC_LowMemory_Printf(MFC_HANDLE handle, const char *str,
				     UINT32 fg_color, UINT32 bg_color);

enum MFC_STATUS MFC_SetMem(MFC_HANDLE handle, const char *str, UINT32 color);
UINT32 MFC_Get_Cursor_Offset(MFC_HANDLE handle);

/* -------- screen logger -------- */
struct screen_logger {
	struct list_head list;
	char *obj;
	char *message;
};

enum message_mode { MESSAGE_REPLACE = 0, MESSAGE_APPEND = 1 };

void screen_logger_init(void);
void screen_logger_add_message(char *obj, enum message_mode mode,
			       char *message);
void screen_logger_remove_message(const char *obj);
void screen_logger_print(MFC_HANDLE handle);
void screen_logger_empty(void);
void screen_logger_test_case(MFC_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __MTK_FB_CONSOLE_H__ */
