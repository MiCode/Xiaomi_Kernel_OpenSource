/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef _DISP_RECT_H_
#define _DISP_RECT_H_

struct disp_rect;

int rect_isEmpty(const struct disp_rect *in);
void rect_set(struct disp_rect *in, int left, int top, int right, int bottom);

void rect_initial(struct disp_rect *in);
void rect_make(struct disp_rect *in, int left, int top, int width, int height);

int rect_intersect(const struct disp_rect *src, const struct disp_rect *dst,
		   struct disp_rect *out);

void rect_join_coord(const int x, const int y,
		     const int width, const int height,
		     const struct disp_rect *in2, struct disp_rect *out);

void rect_join(const struct disp_rect *in1, const struct disp_rect *in2,
	       struct disp_rect *out);

unsigned long shift_address(const struct disp_rect *ori,
			    const struct disp_rect *dst, const int bpp,
			    const int pitch, unsigned long addr);

int rect_equal(const struct disp_rect *one, const struct disp_rect *two);

#endif
