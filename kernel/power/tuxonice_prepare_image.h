/*
 * kernel/power/tuxonice_prepare_image.h
 *
 * Copyright (C) 2003-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 */

#include <asm/sections.h>

extern int toi_prepare_image(void);
extern void toi_recalculate_image_contents(int storage_available);
extern unsigned long real_nr_free_pages(unsigned long zone_idx_mask);
extern long image_size_limit;
extern void toi_free_extra_pagedir_memory(void);
extern unsigned long extra_pd1_pages_allowance;
extern void free_attention_list(void);

#define MIN_FREE_RAM 100
#define MIN_EXTRA_PAGES_ALLOWANCE 500

#define all_zones_mask ((unsigned long) ((1 << MAX_NR_ZONES) - 1))
#ifdef CONFIG_HIGHMEM
#define real_nr_free_high_pages() (real_nr_free_pages(1 << ZONE_HIGHMEM))
#define real_nr_free_low_pages() (real_nr_free_pages(all_zones_mask - \
						(1 << ZONE_HIGHMEM)))
#else
#define real_nr_free_high_pages() (0)
#define real_nr_free_low_pages() (real_nr_free_pages(all_zones_mask))

/* For eat_memory function */
#define ZONE_HIGHMEM (MAX_NR_ZONES + 1)
#endif

unsigned long get_header_storage_needed(void);
unsigned long any_to_free(int use_image_size_limit);
int try_allocate_extra_memory(void);
