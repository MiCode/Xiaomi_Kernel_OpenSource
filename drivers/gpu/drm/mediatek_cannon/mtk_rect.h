/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _MTK_RECT_H_
#define _MTK_RECT_H_

struct mtk_rect {
	int x;
	int y;
	int width;
	int height;
};

void mtk_rect_make(struct mtk_rect *in, int left, int top, int width,
		   int height);
void mtk_rect_set(struct mtk_rect *in, int left, int top, int right,
		  int bottom);
void mtk_rect_join(const struct mtk_rect *in1, const struct mtk_rect *in2,
		   struct mtk_rect *out);

#endif
