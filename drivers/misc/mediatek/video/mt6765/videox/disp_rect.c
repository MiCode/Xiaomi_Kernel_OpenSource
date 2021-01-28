// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "disp_drv_log.h"
#include "disp_rect.h"
#include "ddp_info.h"

int rect_isEmpty(const struct disp_rect *in)
{
	return in->width == 0 || in->height == 0;
}

void rect_set(struct disp_rect *in, int left, int top, int right, int bottom)
{
	in->x = left;
	in->y = top;
	in->width = right - left + 1;
	in->height = bottom - top + 1;
}

void rect_initial(struct disp_rect *in)
{
	in->x = 0;
	in->y = 0;
	in->width = 0;
	in->height = 0;
}

void rect_make(struct disp_rect *in, int left, int top, int width, int height)
{
	in->x = left;
	in->y = top;
	in->width = width;
	in->height = height;
}

int rect_intersect(const struct disp_rect *src, const struct disp_rect *dst,
		struct disp_rect *out)
{
	int left = src->x;
	int top = src->y;
	int right = src->x + src->width - 1;
	int bottom = src->y + src->height - 1;
	int fLeft = dst->x;
	int fTop = dst->y;
	int fRight = dst->x + dst->width - 1;
	int fBottom = dst->y + dst->height - 1;

	if (left < right && top < bottom && !rect_isEmpty(dst) &&
			fLeft <= right && left <= fRight &&
			fTop <= bottom && top <= fBottom) {
		if (fLeft < left)
			fLeft = left;
		if (fTop < top)
			fTop = top;
		if (fRight > right)
			fRight = right;
		if (fBottom > bottom)
			fBottom = bottom;
		rect_set(out, fLeft, fTop, fRight, fBottom);
		DISPDBG(
			"rect_intersect (%d,%d,%d,%d) & (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",
			src->x, src->y, src->width, src->height,
			dst->x, dst->y, dst->width,	dst->height,
			out->x, out->y, out->width, out->height);
		return 1;
	}
	/*make out empty*/
	rect_initial(out);
	return 0;

}

void rect_join_coord(const int x, const int y, const int width,
	const int height, const struct disp_rect *in2, struct disp_rect *out)
{
	struct disp_rect rect_coord = {0, 0, 0, 0};

	rect_make(&rect_coord, x, y, width, height);
	rect_join(&rect_coord, in2, out);
}

void rect_join(const struct disp_rect *in1, const struct disp_rect *in2,
		struct disp_rect *out)
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

	/* do nothing if the params are empty*/
	if (left > right || top >  bottom) {
		rect_set(out, fLeft, fTop, fRight, fBottom);
	} else {
		/* if we are empty, just assign*/
		if (fLeft >  fRight || fTop > fBottom) {
			rect_set(out, left, top, right, bottom);
		} else {
			if (left < fLeft)
				fLeft = left;
			if (top < fTop)
				fTop = top;
			if (right > fRight)
				fRight = right;
			if (bottom > fBottom)
				fBottom = bottom;
			rect_set(out, fLeft, fTop, fRight, fBottom);
		}
	}
	DISPDBG("rect_join (%d,%d,%d,%d) & (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",
		in1->x, in1->y, in1->width, in1->height, in2_x, in2_y,
		in2_w, in2_h, in2->x, in2->y, in2->width, in2->height);
}

int rect_equal(const struct disp_rect *one, const struct disp_rect *two)
{
	return (one->x == two->x) && (one->y == two->y) &&
		(one->width == two->width) && (one->height == two->height);
}

unsigned long shift_address(const struct disp_rect *ori,
	const struct disp_rect *dst, const int bpp,
	const int pitch, unsigned long addr)
{
	int x_shift = dst->x - ori->x;
	int y_shfit = dst->y - ori->y;

	return addr + y_shfit*pitch + x_shift*bpp;
}

