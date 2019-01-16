/*
 * kernel/power/tuxonice_pageflags.c
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Routines for serialising and relocating pageflags in which we
 * store our image metadata.
 */

#include <linux/list.h>
#include <linux/module.h>
#include "tuxonice_pageflags.h"
#include "power.h"

int toi_pageflags_space_needed(void)
{
	int total = 0;
	struct bm_block *bb;

	total = sizeof(unsigned int);

	list_for_each_entry(bb, &pageset1_map->blocks, hook)
	    total += 2 * sizeof(unsigned long) + PAGE_SIZE;

	return total;
}
EXPORT_SYMBOL_GPL(toi_pageflags_space_needed);
