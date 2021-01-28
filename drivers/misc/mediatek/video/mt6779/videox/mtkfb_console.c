// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/font.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

#include "mtkfb_console.h"
#include "ddp_hal.h"

/* ------------------------------------------------------------------------- */

#define MFC_WIDTH           (ctxt->fb_width)
#define MFC_HEIGHT          (ctxt->fb_height)
#define MFC_BPP             (ctxt->fb_bpp)
#define MFC_PITCH           (MFC_WIDTH * MFC_BPP)

#define MFC_FG_COLOR        (ctxt->fg_color)
#define MFC_BG_COLOR        (ctxt->bg_color)

#define MFC_FONT            font_vga_8x16
#define MFC_FONT_WIDTH      (MFC_FONT.width)
#define MFC_FONT_HEIGHT     (MFC_FONT.height)
#define MFC_FONT_DATA       (MFC_FONT.data)

#define MFC_ROW_SIZE        (MFC_FONT_HEIGHT * MFC_PITCH * ctxt->scale)
#define MFC_ROW_FIRST       ((BYTE *)(ctxt->fb_addr))
#define MFC_ROW_SECOND      (MFC_ROW_FIRST + MFC_ROW_SIZE)
#define MFC_ROW_LAST        (MFC_ROW_FIRST + MFC_SIZE - MFC_ROW_SIZE)
#define MFC_SIZE            (MFC_ROW_SIZE * ctxt->rows)
#define MFC_SCROLL_SIZE     (MFC_SIZE - MFC_ROW_SIZE)

#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))

/* ------------------------------------------------------------------------- */
UINT32 MFC_Get_Cursor_Offset(MFC_HANDLE handle)
{
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)handle;

	UINT32 offset = ctxt->cursor_col * MFC_FONT_WIDTH * MFC_BPP +
			ctxt->cursor_row * MFC_FONT_HEIGHT * MFC_PITCH;

	offset *= ctxt->scale;
	return offset;
}

static void _mfc_draw_row(struct MFC_CONTEXT *ctxt, BYTE *dest, BYTE raw_color)
{
	UINT32 pixel_row, i;
	uint16_t color;
	int cols, index;

	for (pixel_row = 0; pixel_row < ctxt->scale; pixel_row++) {
		for (cols = 7; cols >= 0; cols--) {
			if (raw_color >> cols & 1)
				color = MFC_FG_COLOR;
			else
				color = MFC_BG_COLOR;

			for (i = 0; i < ctxt->scale; i++) {
				index = i + (7 - cols) * ctxt->scale;
				((uint16_t *)dest)[index] = color;
			}
		}
		dest += MFC_PITCH;
	}
}

static void _mfc_draw_char(struct MFC_CONTEXT *ctxt, UINT32 x, UINT32 y, char c)
{
	BYTE ch = *((BYTE *)&c);
	const BYTE *cdat;
	BYTE *dest;
	INT32 rows, cols, offset;

	int font_draw_table16[4];

	if (x > (MFC_WIDTH - MFC_FONT_WIDTH)) {
		pr_info("draw width too large,x=%d\n", x);
		return;
	}
	if (y > (MFC_HEIGHT - MFC_FONT_HEIGHT)) {
		pr_info("draw hight too large,y=%d\n", y);
		return;
	}

	offset = y * MFC_PITCH + x * MFC_BPP;
	offset *= ctxt->scale;
	dest = (MFC_ROW_FIRST + offset);

	switch (MFC_BPP) {
	case 2:
		font_draw_table16[0] = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR,
							     MFC_BG_COLOR);
		font_draw_table16[1] = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR,
							     MFC_FG_COLOR);
		font_draw_table16[2] = MAKE_TWO_RGB565_COLOR(MFC_FG_COLOR,
							     MFC_BG_COLOR);
		font_draw_table16[3] = MAKE_TWO_RGB565_COLOR(MFC_FG_COLOR,
							     MFC_FG_COLOR);

		cdat = (const BYTE *)MFC_FONT_DATA + ch * MFC_FONT_HEIGHT;

		for (rows = MFC_FONT_HEIGHT; rows--;) {
			BYTE bits = *cdat++;

			if (ctxt->scale >= 2) {
				_mfc_draw_row(ctxt, dest, bits);
				dest += (MFC_PITCH * ctxt->scale);
			} else {
				((UINT32 *)dest)[0] =
					font_draw_table16[bits >> 6];
				((UINT32 *)dest)[1] =
					font_draw_table16[bits >> 4 & 3];
				((UINT32 *)dest)[2] =
					font_draw_table16[bits >> 2 & 3];
				((UINT32 *)dest)[3] =
					font_draw_table16[bits & 3];
				dest += MFC_PITCH;
			}
		}
		break;
	case 3:
		cdat = (const BYTE *)MFC_FONT_DATA + ch * MFC_FONT_HEIGHT;
		for (rows = MFC_FONT_HEIGHT; rows--; dest += MFC_PITCH) {
			BYTE bits = *cdat++;
			BYTE *tmp = dest;

			for (cols = 0; cols < 8; ++cols) {
				UINT32 color = ((bits >> (7 - cols)) & 0x1) ?
						MFC_FG_COLOR : MFC_BG_COLOR;
				((BYTE *)tmp)[0] = color & 0xff;
				((BYTE *)tmp)[1] = (color >> 8) & 0xff;
				((BYTE *)tmp)[2] = (color >> 16) & 0xff;
				tmp += 3;
			}
		}
		break;
	case 4:
		cdat = (const BYTE *)MFC_FONT_DATA + ch * MFC_FONT_HEIGHT;
		for (rows = MFC_FONT_HEIGHT; rows--; dest += MFC_PITCH) {
			BYTE bits = *cdat++;
			BYTE *tmp = dest;

			for (cols = 0; cols < 8; ++cols) {
				UINT32 color = ((bits >> (7 - cols)) & 0x1) ?
						MFC_FG_COLOR : MFC_BG_COLOR;
				((BYTE *)tmp)[1] = color & 0xff;
				((BYTE *)tmp)[2] = (color >> 8) & 0xff;
				((BYTE *)tmp)[3] = (color >> 16) & 0xff;
				((BYTE *)tmp)[0] = (color >> 16) & 0xff;
				tmp += 4;
			}
		}
		break;
	default:
		pr_info("draw char fail,MFC_BPP=%d\n", MFC_BPP);
		break;
	}
}

static void _mfc_scroll_up(struct MFC_CONTEXT *ctxt)
{
	const UINT32 bg_color = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR,
						      MFC_BG_COLOR);
	UINT32 *ptr = (UINT32 *)MFC_ROW_LAST;
	int i = MFC_ROW_SIZE / sizeof(UINT32);

	memcpy(MFC_ROW_FIRST, MFC_ROW_SECOND, MFC_SCROLL_SIZE);

	while (--i >= 0)
		*ptr++ = bg_color;
}

static void _mfc_newline(struct MFC_CONTEXT *ctxt)
{
	/* Bin:add for filling the color for the blank of this column */
	while (ctxt->cursor_col < ctxt->cols) {
		_mfc_draw_char(ctxt, ctxt->cursor_col * MFC_FONT_WIDTH,
			       ctxt->cursor_row * MFC_FONT_HEIGHT, ' ');

		++ctxt->cursor_col;
	}
	++ctxt->cursor_row;
	ctxt->cursor_col = 0;

	/* check if we need to scroll the terminal */
	if (ctxt->cursor_row >= ctxt->rows) {
		/* Scroll everything up */
		_mfc_scroll_up(ctxt);

		/* decrement row number */
		--ctxt->cursor_row;
	}
}

#define check_newline()				\
do {						\
	if (ctxt->cursor_col >= ctxt->cols)	\
		_mfc_newline(ctxt);		\
} while (0)

static void _mfc_putc(struct MFC_CONTEXT *ctxt, const char c)
{
	check_newline();

	switch (c) {
	case '\n': /* next line */
		_mfc_newline(ctxt);
		break;

	case '\r': /* carriage return */
		ctxt->cursor_col = 0;
		break;

	case '\t': /* tab 8 */
		ctxt->cursor_col += 8;
		ctxt->cursor_col &= ~0x0007;
		check_newline();
		break;

	default: /* draw the char */
		_mfc_draw_char(ctxt, ctxt->cursor_col * MFC_FONT_WIDTH,
			       ctxt->cursor_row * MFC_FONT_HEIGHT, c);
		++ctxt->cursor_col;
		check_newline();
		break;
	}
}

static int MFC_GetScale(unsigned int fb_width, unsigned int fb_height,
			unsigned int fb_bpp)
{
	if (fb_bpp == 2 && fb_width * fb_height >= 1080 * 1920)
		return 2;

	return 1;
}

enum MFC_STATUS MFC_Open(MFC_HANDLE *handle, void *fb_addr,
			 unsigned int fb_width, unsigned int fb_height,
			 unsigned int fb_bpp, unsigned int fg_color,
			 unsigned int bg_color)
{
	struct MFC_CONTEXT *ctxt = NULL;

	if (!handle || !fb_addr)
		return MFC_STATUS_INVALID_ARGUMENT;

	/* if (fb_bpp != 2) */
	/* return MFC_STATUS_NOT_IMPLEMENTED; */

	ctxt = kzalloc(sizeof(struct MFC_CONTEXT), GFP_KERNEL);
	if (!ctxt)
		return MFC_STATUS_OUT_OF_MEMORY;

	ctxt->scale = MFC_GetScale(fb_width, fb_height, fb_bpp);
	if (ctxt->scale == 0)
		ctxt->scale = 1;

	sema_init(&ctxt->sem, 1);
	ctxt->fb_addr = fb_addr;
	ctxt->fb_width = fb_width;
	ctxt->fb_height = fb_height;
	ctxt->fb_bpp = fb_bpp;
	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;
	ctxt->rows = fb_height / (MFC_FONT_HEIGHT * ctxt->scale);
	ctxt->cols = fb_width / (MFC_FONT_WIDTH * ctxt->scale);
	ctxt->font_width = MFC_FONT_WIDTH;
	ctxt->font_height = MFC_FONT_HEIGHT;

	*handle = ctxt;

	return MFC_STATUS_OK;
}

enum MFC_STATUS MFC_Open_Ex(MFC_HANDLE *handle, void *fb_addr,
			    unsigned int fb_width, unsigned int fb_height,
			    unsigned int fb_pitch, unsigned int fb_bpp,
			    unsigned int fg_color, unsigned int bg_color)
{
	struct MFC_CONTEXT *ctxt = NULL;

	if (!handle || !fb_addr)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (fb_bpp != 2)
		return MFC_STATUS_NOT_IMPLEMENTED; /* only support RGB565 */

	ctxt = kzalloc(sizeof(struct MFC_CONTEXT), GFP_KERNEL);
	if (!ctxt)
		return MFC_STATUS_OUT_OF_MEMORY;

	ctxt->scale = MFC_GetScale(fb_width, fb_height, fb_bpp);
	if (ctxt->scale == 0)
		ctxt->scale = 1;

	sema_init(&ctxt->sem, 1);
	ctxt->fb_addr = fb_addr;
	ctxt->fb_width = fb_pitch;
	ctxt->fb_height = fb_height;
	ctxt->fb_bpp = fb_bpp;
	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;
	ctxt->rows = fb_height / (MFC_FONT_HEIGHT * ctxt->scale);
	ctxt->cols = fb_width / (MFC_FONT_WIDTH * ctxt->scale);
	ctxt->font_width = MFC_FONT_WIDTH;
	ctxt->font_height = MFC_FONT_HEIGHT;

	*handle = ctxt;

	return MFC_STATUS_OK;
}

enum MFC_STATUS MFC_Close(MFC_HANDLE handle)
{
	if (!handle)
		return MFC_STATUS_INVALID_ARGUMENT;

	kfree(handle);

	return MFC_STATUS_OK;
}

enum MFC_STATUS MFC_SetColor(MFC_HANDLE handle, unsigned int fg_color,
			     unsigned int bg_color)
{
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)handle;

	if (!ctxt)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (down_interruptible(&ctxt->sem)) {
		pr_err("[MFC] ERROR: Can't get semaphore in %s()\n", __func__);
		return MFC_STATUS_LOCK_FAIL;
	}

	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;

	up(&ctxt->sem);

	return MFC_STATUS_OK;
}

enum MFC_STATUS MFC_ResetCursor(MFC_HANDLE handle)
{
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)handle;

	if (!ctxt)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (down_interruptible(&ctxt->sem)) {
		pr_err("[MFC] ERROR: Can't get semaphore in %s()\n", __func__);
		return MFC_STATUS_LOCK_FAIL;
	}

	ctxt->cursor_row = ctxt->cursor_col = 0;

	up(&ctxt->sem);

	return MFC_STATUS_OK;
}

enum MFC_STATUS MFC_Print(MFC_HANDLE handle, const char *str)
{
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)handle;
	int count = 0;

	if (!ctxt || !str)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (down_interruptible(&ctxt->sem)) {
		pr_err("[MFC] ERROR: Can't get semaphore in %s()\n", __func__);
		return MFC_STATUS_LOCK_FAIL;
	}

	count = strlen(str);

	while (count--)
		_mfc_putc(ctxt, *str++);

	up(&ctxt->sem);

	return MFC_STATUS_OK;
}

enum MFC_STATUS MFC_SetMem(MFC_HANDLE handle, const char *str, UINT32 color)
{
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)handle;
	int count = 0;
	int i, j;
	UINT32 *ptr;

	if (!ctxt || !str)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (down_interruptible(&ctxt->sem)) {
		pr_err("[MFC] ERROR: Can't get semaphore in %s()\n", __func__);
		return MFC_STATUS_LOCK_FAIL;
	}

	count = strlen(str);
	count = count * MFC_FONT_WIDTH;

	for (j = 0; j < MFC_FONT_HEIGHT; j++) {
		ptr = (UINT32 *)(ctxt->fb_addr + (j + 1) * MFC_PITCH -
				 count * ctxt->fb_bpp);
		for (i = 0; i < count * ctxt->fb_bpp / sizeof(UINT32); i++)
			*ptr++ = color;
	}

	up(&ctxt->sem);

	return MFC_STATUS_OK;
}

enum MFC_STATUS MFC_LowMemory_Printf(MFC_HANDLE handle, const char *str,
				     UINT32 fg_color, UINT32 bg_color)
{
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)handle;
	int count = 0;
	unsigned int col, row, fg_color_mfc, bg_color_mfc;

	if (!ctxt || !str)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (down_interruptible(&ctxt->sem)) {
		pr_err("[MFC] ERROR: Can't get semaphore in %s()\n", __func__);
		return MFC_STATUS_LOCK_FAIL;
	}

	count = strlen(str);
	/* store cursor_col and row for printf low memory char temply */
	row = ctxt->cursor_row;
	col = ctxt->cursor_col;
	ctxt->cursor_row = 0;
	ctxt->cursor_col = ctxt->cols - count;
	fg_color_mfc = ctxt->fg_color;
	bg_color_mfc = ctxt->bg_color;

	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;
	while (count--)
		_mfc_putc(ctxt, *str++);

	/* restore cursor_col and row for printf low memory char temply */
	ctxt->cursor_row = row;
	ctxt->cursor_col = col;
	ctxt->fg_color = fg_color_mfc;
	ctxt->bg_color = bg_color_mfc;

	up(&ctxt->sem);

	return MFC_STATUS_OK;
}

/* ---------- screen logger ---------- */
static struct screen_logger logger_head;
static int screen_logger_inited;

void screen_logger_init(void)
{
	if (screen_logger_inited == 0) {
		INIT_LIST_HEAD(&logger_head.list);
		screen_logger_inited = 1;
	}
}

void screen_logger_add_message(char *obj, enum message_mode mode, char *message)
{
	struct screen_logger *p;
	int add_new = 1;
	char *new, *old;
	unsigned int len = 0;

	screen_logger_init();
	list_for_each_entry(p, &logger_head.list, list) {
		if (strcmp(p->obj, obj) != 0)
			continue;

		switch (mode) {
		case MESSAGE_REPLACE:
			old = p->message;
			new = kstrdup(message, GFP_KERNEL);
			p->message = new;
			kfree(old);
			break;
		case MESSAGE_APPEND:
			len = strlen(p->message) + strlen(message);
			new = kmalloc(sizeof(char) * (len + 1), GFP_KERNEL);
			if (unlikely(!new)) {
				pr_info("%s #%d kmalloc failed\n",
					__func__, __LINE__);
				break;
			}
			strncpy(new, p->message, strlen(p->message));
			strncat(new, message, strlen(message));
			old = p->message;
			p->message = new;
			kfree(old);
			break;
		default:
			break;
		}

		add_new = 0;
	}

	if (add_new == 1) {
		struct screen_logger *logger =
		    kmalloc(sizeof(struct screen_logger), GFP_KERNEL);

		if (logger == NULL)
			return;

		logger->obj = kstrdup(obj, GFP_KERNEL);
		logger->message = kstrdup(message, GFP_KERNEL);
		list_add_tail(&logger->list, &logger_head.list);
	}
}

void screen_logger_remove_message(char *obj)
{
	struct screen_logger *p;

	list_for_each_entry(p, &logger_head.list, list) {
		if (strcmp(p->obj, obj) == 0) {
			kfree(p->obj);
			kfree(p->message);
			/* memory leak. TODO: delete completely. */
			list_del(&p->list);
			kfree(p);
			break;
		}
	}
	p = NULL;
}

void screen_logger_print(MFC_HANDLE handle)
{
	struct screen_logger *p;

	list_for_each_entry(p, &logger_head.list, list)
		MFC_Print(handle, p->message);
}

void screen_logger_empty(void)
{
	struct screen_logger *p = NULL;

	p = list_entry(logger_head.list.prev, typeof(*p), list);
	while (p != &logger_head) {
		screen_logger_remove_message(p->obj);
		p = list_entry(logger_head.list.prev, typeof(*p), list);
	}
}

void screen_logger_test_case(MFC_HANDLE handle)
{
	screen_logger_add_message("message1", MESSAGE_REPLACE,
				  "print message1\n");
	screen_logger_add_message("message1", MESSAGE_REPLACE,
				  "print message2\n");
	screen_logger_print(handle);
	screen_logger_empty();
}
