/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
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

void rect_join_coord(const int x, const int y, const int width,
		     const int height, const struct disp_rect *in2,
		     struct disp_rect *out);

void rect_join(const struct disp_rect *in1, const struct disp_rect *in2,
	       struct disp_rect *out);

unsigned long shift_address(const struct disp_rect *ori,
			    const struct disp_rect *dst, const int bpp,
			    const int pitch, unsigned long addr);

int rect_equal(const struct disp_rect *one, const struct disp_rect *two);

#endif
