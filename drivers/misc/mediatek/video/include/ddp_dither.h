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

#ifndef __DDP_DITHER_H__
#define __DDP_DITHER_H__

typedef enum {
	DISP_DITHER0,
	DISP_DITHER1
} disp_dither_id_t;

void disp_dither_init(disp_dither_id_t id, int width, int height,
	unsigned int dither_bpp, void *cmdq);

void dither_test(const char *cmd, char *debug_output);

#endif

