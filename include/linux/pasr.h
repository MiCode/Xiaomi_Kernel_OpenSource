/*
 * Copyright (C) ST-Ericsson SA 2011
 * Author: Maxime Coquelin <maxime.coquelin@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 */
#ifndef _LINUX_PASR_H
#define _LINUX_PASR_H

#include <linux/mm.h>
#include <linux/spinlock.h>

#ifdef CONFIG_PASR

#define PASR_MAX_SECTION_NR_PER_DIE	8
#define PASR_MAX_DIE_NR			4

extern u64 section_size;
extern unsigned int section_bit;

/**
 * struct pasr_section - Represent either a DDR Bank or Segment depending on
 * the DDR configuration (Bank-Row-Column or Row-Bank-Coloumn)
 *
 * @start: Start address of the segment.
 * @pair: Pointer on another segment in case of dependency (e.g. interleaving).
 *	Masking of the dependant segments have to be done accordingly.
 * @free_size: Represents the free memory size in the segment.
 * @lock: Protect the free_size counter
 * @die: Pointer to the Die the segment is part of.
 */
struct pasr_section {
	phys_addr_t start;
	struct pasr_section *pair;
	u64 free_size;
	spinlock_t *lock;
	struct pasr_die *die;
};

/**
 * struct pasr_die - Represent a DDR die
 *
 * @start: Start address of the die.
 * @end: End address of the die.
 * @idx: Index of the die.
 * @nr_sections: Number of Bank or Segment in the die.
 * @section: Table of the die's segments.
 * @mem_reg: Represents the PASR mask of the die. It is either MR16 or MR17,
 *	depending on the addressing configuration (RBC or BRC).
 * @apply_mask: Callback registred by the platform's PASR driver to apply the
 *	calculated PASR mask.
 * @cookie: Private data for the platform's PASR driver.
 */
struct pasr_die {
	phys_addr_t start;
	phys_addr_t end;
	int idx;
	int nr_sections;
	struct pasr_section section[PASR_MAX_SECTION_NR_PER_DIE];
	u16 mem_reg; /* Either MR16 or MR17 */

	void (*apply_mask)(u16 *mem_reg, void *cookie);
	void *cookie;
};

/**
 * struct pasr_map - Represent the DDR physical map
 *
 * @nr_dies: Number of DDR dies.
 * @die: Table of the dies.
 */
struct pasr_map {
	int nr_dies;
	struct pasr_die die[PASR_MAX_DIE_NR];
};

#define for_each_pasr_section(i, j, map, s)			\
	for (i = 0; i < map.nr_dies; i++)			\
		for (s = &map.die[i].section[0], j = 0;		\
			j < map.die[i].nr_sections;		\
			j++, s = &map.die[i].section[j])

/**
 * pasr_register_mask_function()
 *
 * @die_addr: Physical base address of the die.
 * @function: Callback function for applying the DDR PASR mask.
 * @cookie: Private data called with the callback function.
 *
 * This function is to be called by the platform specific PASR driver in
 * charge of application of the PASR masks.
 */
int pasr_register_mask_function(phys_addr_t die_addr,
		void *function, void *cookie);

/**
 * pasr_put()
 *
 * @paddr: Physical address of the freed memory chunk.
 * @size: Size of the freed memory chunk.
 *
 * This function is to be placed in the allocators when memory chunks are
 * inserted in the free memory pool.
 * This function has only to be called for unused memory, otherwise retention
 * cannot be guaranteed.
 */
void pasr_put(phys_addr_t paddr, u64 size);

/**
 * pasr_get()
 *
 * @paddr: Physical address of the allocated memory chunk.
 * @size: Size of the allocated memory chunk.
 *
 * This function is to be placed in the allocators when memory chunks are
 * removed from the free memory pool.
 * If pasr_put() is used by the allocator, using this function is mandatory to
 * guarantee retention.
 */
void pasr_get(phys_addr_t paddr, u64 size);


static inline void pasr_kput(struct page *page, int order)
{
	if (order == MAX_ORDER - 1)
		pasr_put(page_to_phys(page), PAGE_SIZE << (MAX_ORDER - 1));
}

static inline void pasr_kget(struct page *page, int order)
{
	if (order == MAX_ORDER - 1)
		pasr_get(page_to_phys(page), PAGE_SIZE << (MAX_ORDER - 1));
}

int __init early_pasr_setup(void);
int __init late_pasr_setup(void);
int __init pasr_init_core(struct pasr_map *);

#else
static inline int pasr_register_mask_function(phys_addr_t die_addr,
		void *function, void *cookie)
{
	return 0;
}

#define pasr_kput(page, order) do {} while (0)
#define pasr_kget(page, order) do {} while (0)

#define pasr_put(paddr, size) do {} while (0)
#define pasr_get(paddr, size) do {} while (0)
#endif /* CONFIG_PASR */

#endif /* _LINUX_PASR_H */
