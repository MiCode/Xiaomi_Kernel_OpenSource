/*
 * kernel/power/tuxonice_pageflags.h
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 */

#ifndef KERNEL_POWER_TUXONICE_PAGEFLAGS_H
#define KERNEL_POWER_TUXONICE_PAGEFLAGS_H

extern struct memory_bitmap *pageset1_map;
extern struct memory_bitmap *pageset1_copy_map;
extern struct memory_bitmap *pageset2_map;
extern struct memory_bitmap *page_resave_map;
extern struct memory_bitmap *io_map;
extern struct memory_bitmap *nosave_map;
extern struct memory_bitmap *free_map;

#define PagePageset1(page) \
	(memory_bm_test_bit(pageset1_map, page_to_pfn(page)))
#define SetPagePageset1(page) \
	(memory_bm_set_bit(pageset1_map, page_to_pfn(page)))
#define ClearPagePageset1(page) \
	(memory_bm_clear_bit(pageset1_map, page_to_pfn(page)))

#define PagePageset1Copy(page) \
	(memory_bm_test_bit(pageset1_copy_map, page_to_pfn(page)))
#define SetPagePageset1Copy(page) \
	(memory_bm_set_bit(pageset1_copy_map, page_to_pfn(page)))
#define ClearPagePageset1Copy(page) \
	(memory_bm_clear_bit(pageset1_copy_map, page_to_pfn(page)))

#define PagePageset2(page) \
	(memory_bm_test_bit(pageset2_map, page_to_pfn(page)))
#define SetPagePageset2(page) \
	(memory_bm_set_bit(pageset2_map, page_to_pfn(page)))
#define ClearPagePageset2(page) \
	(memory_bm_clear_bit(pageset2_map, page_to_pfn(page)))

#define PageWasRW(page) \
	(memory_bm_test_bit(pageset2_map, page_to_pfn(page)))
#define SetPageWasRW(page) \
	(memory_bm_set_bit(pageset2_map, page_to_pfn(page)))
#define ClearPageWasRW(page) \
	(memory_bm_clear_bit(pageset2_map, page_to_pfn(page)))

#define PageResave(page) (page_resave_map ? \
	memory_bm_test_bit(page_resave_map, page_to_pfn(page)) : 0)
#define SetPageResave(page) \
	(memory_bm_set_bit(page_resave_map, page_to_pfn(page)))
#define ClearPageResave(page) \
	(memory_bm_clear_bit(page_resave_map, page_to_pfn(page)))

#define PageNosave(page) (nosave_map ? \
		memory_bm_test_bit(nosave_map, page_to_pfn(page)) : 0)
#define SetPageNosave(page) \
	(memory_bm_set_bit(nosave_map, page_to_pfn(page)))
#define ClearPageNosave(page) \
	(memory_bm_clear_bit(nosave_map, page_to_pfn(page)))

#define PageNosaveFree(page) (free_map ? \
		memory_bm_test_bit(free_map, page_to_pfn(page)) : 0)
#define SetPageNosaveFree(page) \
	(memory_bm_set_bit(free_map, page_to_pfn(page)))
#define ClearPageNosaveFree(page) \
	(memory_bm_clear_bit(free_map, page_to_pfn(page)))

extern void save_pageflags(struct memory_bitmap *pagemap);
extern int load_pageflags(struct memory_bitmap *pagemap);
extern int toi_pageflags_space_needed(void);
#endif
