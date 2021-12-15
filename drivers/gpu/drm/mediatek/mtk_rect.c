/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "mtk_rect.h"
#include "mtk_log.h"

void mtk_rect_make(struct mtk_rect *in, int left, int top, int width,
		   int height)
{
	in->x = left;
	in->y = top;
	in->width = width;
	in->height = height;
}

void mtk_rect_set(struct mtk_rect *in, int left, int top, int right, int bottom)
{
	in->x = left;
	in->y = top;
	in->width = right - left + 1;
	in->height = bottom - top + 1;
}

void mtk_rect_join(const struct mtk_rect *in1, const struct mtk_rect *in2,
		   struct mtk_rect *out)
{
	int left = in1->x;
	int top = in1->y;
	int right = in1->x + in1->width - 1;
	int bottom = in1->y + in1->height - 1;
	int fLeft = in2->x;
	int fTop = in2->y;
	int fRight = in2->x + in2->width - 1;
	int fBottom = in2->y + in2->height - 1;

	int in2_x = in2->x;
	int in2_y = in2->y;
	int in2_w = in2->width;
	int in2_h = in2->height;

	/* do nothing if the params are empty */
	if (left > right || top > bottom) {
		mtk_rect_set(out, fLeft, fTop, fRight, fBottom);
	} else if (fLeft > fRight || fTop > fBottom) {
		/* if we are empty, just assign */
		mtk_rect_set(out, left, top, right, bottom);
	} else {
		if (left < fLeft)
			fLeft = left;
		if (top < fTop)
			fTop = top;
		if (right > fRight)
			fRight = right;
		if (bottom > fBottom)
			fBottom = bottom;
		mtk_rect_set(out, fLeft, fTop, fRight, fBottom);
	}
	DDPDBG("%s (%d,%d,%d,%d) & (%d,%d,%d,%d) to (%d,%d,%d,%d)\n", __func__,
	       in1->x, in1->y, in1->width, in1->height, in2_x, in2_y, in2_w,
	       in2_h, in2->x, in2->y, in2->width, in2->height);
}
