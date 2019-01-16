/*
 * kernel/power/tuxonice_extent.h
 *
 * Copyright (C) 2003-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * It contains declarations related to extents. Extents are
 * TuxOnIce's method of storing some of the metadata for the image.
 * See tuxonice_extent.c for more info.
 *
 */

#include "tuxonice_modules.h"

#ifndef EXTENT_H
#define EXTENT_H

struct hibernate_extent {
	unsigned long start, end;
	struct hibernate_extent *next;
};

struct hibernate_extent_chain {
	unsigned long size;	/* size of the chain ie sum (max-min+1) */
	int num_extents;
	struct hibernate_extent *first, *last_touched;
	struct hibernate_extent *current_extent;
	unsigned long current_offset;
};

/* Simplify iterating through all the values in an extent chain */
#define toi_extent_for_each(extent_chain, extentpointer, value) \
if ((extent_chain)->first) \
	for ((extentpointer) = (extent_chain)->first, (value) = \
			(extentpointer)->start; \
	     ((extentpointer) && ((extentpointer)->next || (value) <= \
				 (extentpointer)->end)); \
	     (((value) == (extentpointer)->end) ? \
		((extentpointer) = (extentpointer)->next, (value) = \
		 ((extentpointer) ? (extentpointer)->start : 0)) : \
			(value)++))

#endif
