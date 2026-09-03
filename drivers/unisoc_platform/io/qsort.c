/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Unisoc Inc.
 */

#include "iomonitor.h"

static inline char *sprd_med(char *, char *, char *, int (*)(const void *, const void *));
static inline void sprd_doswap(char *, char *, int, int);

/*
 * Qsort routine from Bentley & McIlroy's "Engineering x Sort Function".
 */
#define sprd_swap_func(TYPE, pmi, pmj, n) {	\
	long i = (n) / sizeof(TYPE);		\
	TYPE *pi = (TYPE *) (pmi);		\
	TYPE *pj = (TYPE *) (pmj);		\
	do {					\
		TYPE t = *pi;		\
		*pi++ = *pj;			\
		*pj++ = t;			\
	} while (--i > 0);			\
}

#define SPRD_SWAPINIT(x, width) (swaptype = ((uintptr_t)x) % sizeof(long) || \
	width % sizeof(long) ? 2 : width == sizeof(long) ? 0 : 1)

static inline void
sprd_doswap(char *x, char *y, int n, int swaptype) {
	if (swaptype <= 1)
		sprd_swap_func(long, x, y, n)
	else
		sprd_swap_func(char, x, y, n)
}

#define sprd_swap(x, y)					\
	do {						\
		if (swaptype == 0) {			\
			long t = *(long *)(x);		\
			*(long *)(x) = *(long *)(y);	\
			*(long *)(y) = t;		\
		} else					\
			sprd_doswap(x, y, width, swaptype);	\
	} while (0)

#define vecswap(x, y, n)				\
	do {						\
		if ((n) > 0)				\
			sprd_doswap(x, y, n, swaptype);	\
	} while (0)

static inline char *
sprd_med(char *x, char *y, char *c, int (*cmp)(const void *, const void *))
{
	return cmp(x, y) < 0 ?
		(cmp(y, c) < 0 ? y : (cmp(x, c) < 0 ? c : x))
		: (cmp(y, c) > 0 ? y : (cmp(x, c) < 0 ? x : c));
}

void sprd_qsort(void *aa, size_t n, size_t width, int (*cmp)(const void *, const void *))
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	int d, r, swaptype, swap_cnt;
	char *x = aa;

loop:
	SPRD_SWAPINIT(x, width);
	swap_cnt = 0;
	if (n < 7) {
		for (pm = (char *)x + width; pm < (char *) x + n * width; pm += width)
			for (pl = pm; pl > (char *) x && cmp(pl - width, pl) > 0;
			     pl -= width)
				sprd_swap(pl, pl - width);
		return;
	}
	pm = (char *)x + (n / 2) * width;
	if (n > 7) {
		pl = (char *)x;
		pn = (char *)x + (n - 1) * width;
		if (n > 40) {
			d = (n / 8) * width;
			pl = sprd_med(pl, pl + d, pl + 2 * d, cmp);
			pm = sprd_med(pm - d, pm, pm + d, cmp);
			pn = sprd_med(pn - 2 * d, pn - d, pn, cmp);
		}
		pm = sprd_med(pl, pm, pn, cmp);
	}
	sprd_swap(x, pm);
	pa = pb = (char *)x + width;

	pc = pd = (char *)x + (n - 1) * width;
	for (;;) {
		while (pb <= pc && (r = cmp(pb, x)) <= 0) {
			if (r == 0) {
				swap_cnt = 1;
				sprd_swap(pa, pb);
				pa += width;
			}
			pb += width;
		}
		while (pb <= pc && (r = cmp(pc, x)) >= 0) {
			if (r == 0) {
				swap_cnt = 1;
				sprd_swap(pc, pd);
				pd -= width;
			}
			pc -= width;
		}
		if (pb > pc)
			break;
		sprd_swap(pb, pc);
		swap_cnt = 1;
		pb += width;
		pc -= width;
	}
	if (swap_cnt == 0) {  /* Switch to insertion sort */
		for (pm = (char *) x + width; pm < (char *) x + n * width; pm += width)
			for (pl = pm; pl > (char *) x && cmp(pl - width, pl) > 0;
			     pl -= width)
				sprd_swap(pl, pl - width);
		return;
	}

	pn = (char *)x + n * width;
	r = min(pa - (char *)x, pb - pa);
	vecswap(x, pb - r, r);
	r = min(pd - pc, pn - pd - (int)width);
	vecswap(pb, pn - r, r);
	r = pb - pa;
	if (r > (int)width)
		sprd_qsort(x, r / width, width, cmp);
	r = pd - pc;
	if (r > (int)width) {
		/* Iterate rather than recurse to save stack space */
		x = pn - r;
		n = r / width;
		goto loop;
	}
	/* sprd_qsort(pn - r, r / width, width, cmp); */
}
