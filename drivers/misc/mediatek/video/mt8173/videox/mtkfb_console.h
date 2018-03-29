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


#define MFC_CHECK_RET(expr)             \
	do {	\
		MFC_STATUS ret = (expr);        \
		ASSERT(MFC_STATUS_OK == ret);   \
	} while (0)


	typedef enum {
		MFC_STATUS_OK = 0,

		MFC_STATUS_INVALID_ARGUMENT = -1,
		MFC_STATUS_NOT_IMPLEMENTED = -2,
		MFC_STATUS_OUT_OF_MEMORY = -3,
		MFC_STATUS_LOCK_FAIL = -4,
		MFC_STATUS_FATAL_ERROR = -5,
	} MFC_STATUS;


	typedef void *MFC_HANDLE;

/* --------------------------------------------------------------------------- */

	typedef struct {
		struct semaphore sem;

		uint8_t *fb_addr;
		uint32_t fb_width;
		uint32_t fb_height;
		uint32_t fb_bpp;
		uint32_t fg_color;
		uint32_t bg_color;
		uint32_t screen_color;
		uint32_t rows;
		uint32_t cols;
		uint32_t cursor_row;
		uint32_t cursor_col;
		uint32_t font_width;
		uint32_t font_height;
	} MFC_CONTEXT;

/* MTK Framebuffer Console API */

	MFC_STATUS MFC_Open(MFC_HANDLE *handle,
			    void *fb_addr,
			    unsigned int fb_width,
			    unsigned int fb_height,
			    unsigned int fb_bpp, unsigned int fg_color, unsigned int bg_color);

	MFC_STATUS MFC_Open_Ex(MFC_HANDLE *handle,
			       void *fb_addr,
			       unsigned int fb_width,
			       unsigned int fb_height,
			       unsigned int fb_pitch,
			       unsigned int fb_bpp, unsigned int fg_color, unsigned int bg_color);


	MFC_STATUS MFC_Close(MFC_HANDLE handle);

	MFC_STATUS MFC_SetColor(MFC_HANDLE handle, unsigned int fg_color, unsigned int bg_color);

	MFC_STATUS MFC_ResetCursor(MFC_HANDLE handle);

	MFC_STATUS MFC_Print(MFC_HANDLE handle, const char *str);

	MFC_STATUS MFC_LowMemory_Printf(MFC_HANDLE handle, const char *str, uint32_t fg_color,
					uint32_t bg_color);

	MFC_STATUS MFC_SetMem(MFC_HANDLE handle, const char *str, uint32_t color);
	uint32_t MFC_Get_Cursor_Offset(MFC_HANDLE handle);
#ifdef __cplusplus
}
#endif
#endif				/* __MTK_FB_CONSOLE_H__ */
