/*
 * kernel/power/tuxonice_pagedir.h
 *
 * Copyright (C) 2006-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Declarations for routines for handling pagesets.
 */

#ifndef KERNEL_POWER_PAGEDIR_H
#define KERNEL_POWER_PAGEDIR_H

/* Pagedir
 *
 * Contains the metadata for a set of pages saved in the image.
 */

struct pagedir {
	int id;
	unsigned long size;
#ifdef CONFIG_HIGHMEM
	unsigned long size_high;
#endif
};

#ifdef CONFIG_HIGHMEM
#define get_highmem_size(pagedir) (pagedir.size_high)
#define set_highmem_size(pagedir, sz) do { pagedir.size_high = sz; } while (0)
#define inc_highmem_size(pagedir) do { pagedir.size_high++; } while (0)
#define get_lowmem_size(pagedir) (pagedir.size - pagedir.size_high)
#else
#define get_highmem_size(pagedir) (0)
#define set_highmem_size(pagedir, sz) do { } while (0)
#define inc_highmem_size(pagedir) do { } while (0)
#define get_lowmem_size(pagedir) (pagedir.size)
#endif

extern struct pagedir pagedir1, pagedir2;

extern void toi_copy_pageset1(void);

extern int toi_get_pageset1_load_addresses(void);

extern unsigned long __toi_get_nonconflicting_page(void);
struct page *___toi_get_nonconflicting_page(int can_be_highmem);

extern void toi_reset_alt_image_pageset2_pfn(void);
extern int add_boot_kernel_data_pbe(void);
#endif
