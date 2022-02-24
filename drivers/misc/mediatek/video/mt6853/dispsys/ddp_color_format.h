// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
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
DpColorFormat  fmt_type(int unique, int swap);

#endif
