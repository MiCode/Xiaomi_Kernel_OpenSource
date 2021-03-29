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

#ifndef __H_DDP_COLOR_FORMAT__
#define __H_DDP_COLOR_FORMAT__

#include "ddp_info.h"

int fmt_bpp(DpColorFormat fmt);
int fmt_swap(DpColorFormat fmt);
int fmt_color_space(DpColorFormat fmt);
int fmt_is_yuv422(DpColorFormat fmt);
int fmt_is_yuv420(DpColorFormat fmt);
int fmt_hw_value(DpColorFormat fmt);
char *fmt_string(DpColorFormat fmt);
DpColorFormat fmt_type(int unique, int swap);

#endif
