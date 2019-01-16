/*
 * kernel/power/tuxonice_bio.h
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * Distributed under GPLv2.
 *
 * This file contains declarations for functions exported from
 * tuxonice_bio.c, which contains low level io functions.
 */

#include <linux/buffer_head.h>
#include "tuxonice_extent.h"

void toi_put_extent_chain(struct hibernate_extent_chain *chain);
int toi_add_to_extent_chain(struct hibernate_extent_chain *chain,
			    unsigned long start, unsigned long end);

struct hibernate_extent_saved_state {
	int extent_num;
	struct hibernate_extent *extent_ptr;
	unsigned long offset;
};

struct toi_bdev_info {
	struct toi_bdev_info *next;
	struct hibernate_extent_chain blocks;
	struct block_device *bdev;
	struct toi_module_ops *allocator;
	int allocator_index;
	struct hibernate_extent_chain allocations;
	char name[266];		/* "swap on " or "file " + up to 256 chars */

	/* Saved in header */
	char uuid[17];
	dev_t dev_t;
	int prio;
	int bmap_shift;
	int blocks_per_page;
	unsigned long pages_used;
	struct hibernate_extent_saved_state saved_state[4];
};

struct toi_extent_iterate_state {
	struct toi_bdev_info *current_chain;
	int num_chains;
	int saved_chain_number[4];
	struct toi_bdev_info *saved_chain_ptr[4];
};

/*
 * Our exported interface so the swapwriter and filewriter don't
 * need these functions duplicated.
 */
struct toi_bio_ops {
	int (*bdev_page_io) (int rw, struct block_device *bdev, long pos, struct page *page);
	int (*register_storage) (struct toi_bdev_info *new);
	void (*free_storage) (void);
};

struct toi_allocator_ops {
	unsigned long (*toi_swap_storage_available) (void);
};

extern struct toi_bio_ops toi_bio_ops;

extern char *toi_writer_buffer;
extern int toi_writer_buffer_posn;

struct toi_bio_allocator_ops {
	int (*register_storage) (void);
	unsigned long (*storage_available) (void);
	int (*allocate_storage) (struct toi_bdev_info *, unsigned long);
	int (*bmap) (struct toi_bdev_info *);
	void (*free_storage) (struct toi_bdev_info *);
};
