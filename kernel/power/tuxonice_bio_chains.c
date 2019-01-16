/*
 * kernel/power/tuxonice_bio_devinfo.c
 *
 * Copyright (C) 2009-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * Distributed under GPLv2.
 *
 */

#include <linux/mm_types.h>
#include "tuxonice_bio.h"
#include "tuxonice_bio_internal.h"
#include "tuxonice_alloc.h"
#include "tuxonice_ui.h"
#include "tuxonice.h"
#include "tuxonice_io.h"

static struct toi_bdev_info *prio_chain_head;
static int num_chains;

/* Pointer to current entry being loaded/saved. */
struct toi_extent_iterate_state toi_writer_posn;

#define metadata_size (sizeof(struct toi_bdev_info) - \
		offsetof(struct toi_bdev_info, uuid))

/*
 * After section 0 (header) comes 2 => next_section[0] = 2
 */
static int next_section[3] = { 2, 3, 1 };

/**
 * dump_block_chains - print the contents of the bdev info array.
 **/
void dump_block_chains(void)
{
	int i = 0;
	int j;
	struct toi_bdev_info *cur_chain = prio_chain_head;

	while (cur_chain) {
		struct hibernate_extent *this = cur_chain->blocks.first;

		printk(KERN_DEBUG "Chain %d (prio %d):", i, cur_chain->prio);

		while (this) {
			printk(KERN_CONT " [%lu-%lu]%s", this->start,
			       this->end, this->next ? "," : "");
			this = this->next;
		}

		printk("\n");
		cur_chain = cur_chain->next;
		i++;
	}

	printk(KERN_DEBUG "Saved states:\n");
	for (i = 0; i < 4; i++) {
		printk(KERN_DEBUG "Slot %d: Chain %d.\n", i, toi_writer_posn.saved_chain_number[i]);

		cur_chain = prio_chain_head;
		j = 0;
		while (cur_chain) {
			printk(KERN_DEBUG " Chain %d: Extent %d. Offset %lu.\n",
			       j, cur_chain->saved_state[i].extent_num,
			       cur_chain->saved_state[i].offset);
			cur_chain = cur_chain->next;
			j++;
		}
		printk(KERN_CONT "\n");
	}
}

/**
 *
 **/
static void toi_extent_chain_next(void)
{
	struct toi_bdev_info *this = toi_writer_posn.current_chain;

	if (!this->blocks.current_extent)
		return;

	if (this->blocks.current_offset == this->blocks.current_extent->end) {
		if (this->blocks.current_extent->next) {
			this->blocks.current_extent = this->blocks.current_extent->next;
			this->blocks.current_offset = this->blocks.current_extent->start;
		} else {
			this->blocks.current_extent = NULL;
			this->blocks.current_offset = 0;
		}
	} else
		this->blocks.current_offset++;
}

/**
 *
 */

static struct toi_bdev_info *__find_next_chain_same_prio(void)
{
	struct toi_bdev_info *start_chain = toi_writer_posn.current_chain;
	struct toi_bdev_info *this = start_chain;
	int orig_prio = this->prio;

	do {
		this = this->next;

		if (!this)
			this = prio_chain_head;

		/* Back on original chain? Use it again. */
		if (this == start_chain)
			return start_chain;

	} while (!this->blocks.current_extent || this->prio != orig_prio);

	return this;
}

static void find_next_chain(void)
{
	struct toi_bdev_info *this;

	this = __find_next_chain_same_prio();

	/*
	 * If we didn't get another chain of the same priority that we
	 * can use, look for the next priority.
	 */
	while (this && !this->blocks.current_extent)
		this = this->next;

	toi_writer_posn.current_chain = this;
}

/**
 * toi_extent_state_next - go to the next extent
 * @blocks: The number of values to progress.
 * @stripe_mode: Whether to spread usage across all chains.
 *
 * Given a state, progress to the next valid entry. We may begin in an
 * invalid state, as we do when invoked after extent_state_goto_start below.
 *
 * When using compression and expected_compression > 0, we let the image size
 * be larger than storage, so we can validly run out of data to return.
 **/
static unsigned long toi_extent_state_next(int blocks, int current_stream)
{
	int i;

	if (!toi_writer_posn.current_chain)
		return -ENOSPC;

	/* Assume chains always have lengths that are multiples of @blocks */
	for (i = 0; i < blocks; i++)
		toi_extent_chain_next();

	/* The header stream is not striped */
	if (current_stream || !toi_writer_posn.current_chain->blocks.current_extent)
		find_next_chain();

	return toi_writer_posn.current_chain ? 0 : -ENOSPC;
}

static void toi_insert_chain_in_prio_list(struct toi_bdev_info *this)
{
	struct toi_bdev_info **prev_ptr;
	struct toi_bdev_info *cur;

	/* Loop through the existing chain, finding where to insert it */
	prev_ptr = &prio_chain_head;
	cur = prio_chain_head;

	while (cur && cur->prio >= this->prio) {
		prev_ptr = &cur->next;
		cur = cur->next;
	}

	this->next = *prev_ptr;
	*prev_ptr = this;

	this = prio_chain_head;
	while (this)
		this = this->next;
	num_chains++;
}

/**
 * toi_extent_state_goto_start - reinitialize an extent chain iterator
 * @state:	Iterator to reinitialize
 **/
void toi_extent_state_goto_start(void)
{
	struct toi_bdev_info *this = prio_chain_head;

	while (this) {
		toi_message(TOI_BIO, TOI_VERBOSE, 0,
			    "Setting current extent to %p.", this->blocks.first);
		this->blocks.current_extent = this->blocks.first;
		if (this->blocks.current_extent) {
			toi_message(TOI_BIO, TOI_VERBOSE, 0,
				    "Setting current offset to %lu.",
				    this->blocks.current_extent->start);
			this->blocks.current_offset = this->blocks.current_extent->start;
		}

		this = this->next;
	}

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Setting current chain to %p.", prio_chain_head);
	toi_writer_posn.current_chain = prio_chain_head;
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Leaving extent state goto start.");
}

/**
 * toi_extent_state_save - save state of the iterator
 * @state:		Current state of the chain
 * @saved_state:	Iterator to populate
 *
 * Given a state and a struct hibernate_extent_state_store, save the current
 * position in a format that can be used with relocated chains (at
 * resume time).
 **/
void toi_extent_state_save(int slot)
{
	struct toi_bdev_info *cur_chain = prio_chain_head;
	struct hibernate_extent *extent;
	struct hibernate_extent_saved_state *chain_state;
	int i = 0;

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "toi_extent_state_save, slot %d.", slot);

	if (!toi_writer_posn.current_chain) {
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "No current chain => " "chain_num = -1.");
		toi_writer_posn.saved_chain_number[slot] = -1;
		return;
	}

	while (cur_chain) {
		i++;
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "Saving chain %d (%p) "
			    "state, slot %d.", i, cur_chain, slot);

		chain_state = &cur_chain->saved_state[slot];

		chain_state->offset = cur_chain->blocks.current_offset;

		if (toi_writer_posn.current_chain == cur_chain) {
			toi_writer_posn.saved_chain_number[slot] = i;
			toi_message(TOI_BIO, TOI_VERBOSE, 0, "This is the chain "
				    "we were on => chain_num is %d.", i);
		}

		if (!cur_chain->blocks.current_extent) {
			chain_state->extent_num = 0;
			toi_message(TOI_BIO, TOI_VERBOSE, 0, "No current extent "
				    "for this chain => extent_num %d is 0.", i);
			cur_chain = cur_chain->next;
			continue;
		}

		extent = cur_chain->blocks.first;
		chain_state->extent_num = 1;

		while (extent != cur_chain->blocks.current_extent) {
			chain_state->extent_num++;
			extent = extent->next;
		}

		toi_message(TOI_BIO, TOI_VERBOSE, 0, "extent num %d is %d.", i,
			    chain_state->extent_num);

		cur_chain = cur_chain->next;
	}
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Completed saving extent state slot %d.", slot);
}

/**
 * toi_extent_state_restore - restore the position saved by extent_state_save
 * @state:		State to populate
 * @saved_state:	Iterator saved to restore
 **/
void toi_extent_state_restore(int slot)
{
	int i = 0;
	struct toi_bdev_info *cur_chain = prio_chain_head;
	struct hibernate_extent_saved_state *chain_state;

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "toi_extent_state_restore - slot %d.", slot);

	if (toi_writer_posn.saved_chain_number[slot] == -1) {
		toi_writer_posn.current_chain = NULL;
		return;
	}

	while (cur_chain) {
		int posn;
		int j;
		i++;
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "Restoring chain %d (%p) "
			    "state, slot %d.", i, cur_chain, slot);

		chain_state = &cur_chain->saved_state[slot];

		posn = chain_state->extent_num;

		cur_chain->blocks.current_extent = cur_chain->blocks.first;
		cur_chain->blocks.current_offset = chain_state->offset;

		if (i == toi_writer_posn.saved_chain_number[slot]) {
			toi_writer_posn.current_chain = cur_chain;
			toi_message(TOI_BIO, TOI_VERBOSE, 0, "Found current chain.");
		}

		for (j = 0; j < 4; j++)
			if (i == toi_writer_posn.saved_chain_number[j]) {
				toi_writer_posn.saved_chain_ptr[j] = cur_chain;
				toi_message(TOI_BIO, TOI_VERBOSE, 0,
					    "Found saved chain ptr %d (%p) (offset"
					    " %d).", j, cur_chain,
					    cur_chain->saved_state[j].offset);
			}

		if (posn) {
			while (--posn)
				cur_chain->blocks.current_extent =
				    cur_chain->blocks.current_extent->next;
		} else
			cur_chain->blocks.current_extent = NULL;

		cur_chain = cur_chain->next;
	}
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Done.");
	if (test_action_state(TOI_LOGALL))
		dump_block_chains();
}

/*
 * Storage needed
 *
 * Returns amount of space in the image header required
 * for the chain data. This ignores the links between
 * pages, which we factor in when allocating the space.
 */
int toi_bio_devinfo_storage_needed(void)
{
	int result = sizeof(num_chains);
	struct toi_bdev_info *chain = prio_chain_head;

	while (chain) {
		result += metadata_size;

		/* Chain size */
		result += sizeof(int);

		/* Extents */
		result += (2 * sizeof(unsigned long) * chain->blocks.num_extents);

		chain = chain->next;
	}

	result += 4 * sizeof(int);
	return result;
}

static unsigned long chain_pages_used(struct toi_bdev_info *chain)
{
	struct hibernate_extent *this = chain->blocks.first;
	struct hibernate_extent_saved_state *state = &chain->saved_state[3];
	unsigned long size = 0;
	int extent_idx = 1;

	if (!state->extent_num) {
		if (!this)
			return 0;
		else
			return chain->blocks.size;
	}

	while (extent_idx < state->extent_num) {
		size += (this->end - this->start + 1);
		this = this->next;
		extent_idx++;
	}

	/* We didn't use the one we're sitting on, so don't count it */
	return size + state->offset - this->start;
}

/**
 * toi_serialise_extent_chain - write a chain in the image
 * @chain:	Chain to write.
 **/
static int toi_serialise_extent_chain(struct toi_bdev_info *chain)
{
	struct hibernate_extent *this;
	int ret;
	int i = 1;

	chain->pages_used = chain_pages_used(chain);

	if (test_action_state(TOI_LOGALL))
		dump_block_chains();
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Serialising chain (dev_t %lx).", chain->dev_t);
	/* Device info -  dev_t, prio, bmap_shift, blocks per page, positions */
	ret = toiActiveAllocator->rw_header_chunk(WRITE, &toi_blockwriter_ops,
						  (char *)&chain->uuid, metadata_size);
	if (ret)
		return ret;

	/* Num extents */
	ret = toiActiveAllocator->rw_header_chunk(WRITE, &toi_blockwriter_ops,
						  (char *)&chain->blocks.num_extents, sizeof(int));
	if (ret)
		return ret;

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "%d extents.", chain->blocks.num_extents);

	this = chain->blocks.first;
	while (this) {
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "Extent %d.", i);
		ret = toiActiveAllocator->rw_header_chunk(WRITE,
							  &toi_blockwriter_ops,
							  (char *)this, 2 * sizeof(this->start));
		if (ret)
			return ret;
		this = this->next;
		i++;
	}

	return ret;
}

int toi_serialise_extent_chains(void)
{
	struct toi_bdev_info *this = prio_chain_head;
	int result;

	/* Write the number of chains */
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Write number of chains (%d)", num_chains);
	result = toiActiveAllocator->rw_header_chunk(WRITE,
						     &toi_blockwriter_ops, (char *)&num_chains,
						     sizeof(int));
	if (result)
		return result;

	/* Then the chains themselves */
	while (this) {
		result = toi_serialise_extent_chain(this);
		if (result)
			return result;
		this = this->next;
	}

	/*
	 * Finally, the chain we should be on at the start of each
	 * section.
	 */
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Saved chain numbers.");
	result = toiActiveAllocator->rw_header_chunk(WRITE,
						     &toi_blockwriter_ops,
						     (char *)&toi_writer_posn.saved_chain_number[0],
						     4 * sizeof(int));

	return result;
}

int toi_register_storage_chain(struct toi_bdev_info *new)
{
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Inserting chain %p into list.", new);
	toi_insert_chain_in_prio_list(new);
	return 0;
}

static void free_bdev_info(struct toi_bdev_info *chain)
{
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Free chain %p.", chain);

	toi_message(TOI_BIO, TOI_VERBOSE, 0, " - Block extents.");
	toi_put_extent_chain(&chain->blocks);

	/*
	 * The allocator may need to do more than just free the chains
	 * (swap_free, for example). Don't call from boot kernel.
	 */
	toi_message(TOI_BIO, TOI_VERBOSE, 0, " - Allocator extents.");
	if (chain->allocator)
		chain->allocator->bio_allocator_ops->free_storage(chain);

	/*
	 * Dropping out of reading atomic copy? Need to undo
	 * toi_open_by_devnum.
	 */
	toi_message(TOI_BIO, TOI_VERBOSE, 0, " - Bdev.");
	if (chain->bdev && !IS_ERR(chain->bdev) &&
	    chain->bdev != resume_block_device &&
	    chain->bdev != header_block_device && test_toi_state(TOI_TRYING_TO_RESUME))
		toi_close_bdev(chain->bdev);

	/* Poison */
	toi_message(TOI_BIO, TOI_VERBOSE, 0, " - Struct.");
	toi_kfree(39, chain, sizeof(*chain));

	if (prio_chain_head == chain)
		prio_chain_head = NULL;

	num_chains--;
}

void free_all_bdev_info(void)
{
	struct toi_bdev_info *this = prio_chain_head;

	while (this) {
		struct toi_bdev_info *next = this->next;
		free_bdev_info(this);
		this = next;
	}

	memset((char *)&toi_writer_posn, 0, sizeof(toi_writer_posn));
	prio_chain_head = NULL;
}

static void set_up_start_position(void)
{
	toi_writer_posn.current_chain = prio_chain_head;
	go_next_page(0, 0);
}

/**
 * toi_load_extent_chain - read back a chain saved in the image
 * @chain:	Chain to load
 *
 * The linked list of extents is reconstructed from the disk. chain will point
 * to the first entry.
 **/
int toi_load_extent_chain(int index, int *num_loaded)
{
	struct toi_bdev_info *chain = toi_kzalloc(39,
						  sizeof(struct toi_bdev_info), GFP_ATOMIC);
	struct hibernate_extent *this, *last = NULL;
	int i, ret;

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Loading extent chain %d.", index);
	/* Get dev_t, prio, bmap_shift, blocks per page, positions */
	ret = toiActiveAllocator->rw_header_chunk_noreadahead(READ, NULL,
							      (char *)&chain->uuid, metadata_size);

	if (ret) {
		printk(KERN_ERR "Failed to read the size of extent chain.\n");
		toi_kfree(39, chain, sizeof(*chain));
		return 1;
	}

	toi_bkd.pages_used[index] = chain->pages_used;

	ret = toiActiveAllocator->rw_header_chunk_noreadahead(READ, NULL,
							      (char *)&chain->blocks.num_extents,
							      sizeof(int));
	if (ret) {
		printk(KERN_ERR "Failed to read the size of extent chain.\n");
		toi_kfree(39, chain, sizeof(*chain));
		return 1;
	}

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "%d extents.", chain->blocks.num_extents);

	for (i = 0; i < chain->blocks.num_extents; i++) {
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "Extent %d.", i + 1);

		this = toi_kzalloc(2, sizeof(struct hibernate_extent), TOI_ATOMIC_GFP);
		if (!this) {
			printk(KERN_INFO "Failed to allocate a new extent.\n");
			free_bdev_info(chain);
			return -ENOMEM;
		}
		this->next = NULL;
		/* Get the next page */
		ret = toiActiveAllocator->rw_header_chunk_noreadahead(READ,
								      NULL, (char *)this,
								      2 * sizeof(this->start));
		if (ret) {
			printk(KERN_INFO "Failed to read an extent.\n");
			toi_kfree(2, this, sizeof(struct hibernate_extent));
			free_bdev_info(chain);
			return 1;
		}

		if (last)
			last->next = this;
		else {
			char b1[32], b2[32], b3[32];
			/*
			 * Open the bdev
			 */
			toi_message(TOI_BIO, TOI_VERBOSE, 0,
				    "Chain dev_t is %s. Resume dev t is %s. Header"
				    " bdev_t is %s.\n",
				    format_dev_t(b1, chain->dev_t),
				    format_dev_t(b2, resume_dev_t),
				    format_dev_t(b3, toi_sig_data->header_dev_t));

			if (chain->dev_t == resume_dev_t)
				chain->bdev = resume_block_device;
			else if (chain->dev_t == toi_sig_data->header_dev_t)
				chain->bdev = header_block_device;
			else {
				chain->bdev = toi_open_bdev(chain->uuid, chain->dev_t, 1);
				if (IS_ERR(chain->bdev)) {
					free_bdev_info(chain);
					return -ENODEV;
				}
			}

			toi_message(TOI_BIO, TOI_VERBOSE, 0, "Chain bmap shift "
				    "is %d and blocks per page is %d.",
				    chain->bmap_shift, chain->blocks_per_page);

			chain->blocks.first = this;

			/*
			 * Couldn't do this earlier, but can't do
			 * goto_start now - we may have already used blocks
			 * in the first chain.
			 */
			chain->blocks.current_extent = this;
			chain->blocks.current_offset = this->start;

			/*
			 * Can't wait until we've read the whole chain
			 * before we insert it in the list. We might need
			 * this chain to read the next page in the header
			 */
			toi_insert_chain_in_prio_list(chain);
		}

		/*
		 * We have to wait until 2 extents are loaded before setting up
		 * properly because if the first extent has only one page, we
		 * will need to put the position on the second extent. Sounds
		 * obvious, but it wasn't!
		 */
		(*num_loaded)++;
		if ((*num_loaded) == 2)
			set_up_start_position();
		last = this;
	}

	/*
	 * Shouldn't get empty chains, but it's not impossible. Link them in so
	 * they get freed properly later.
	 */
	if (!chain->blocks.num_extents)
		toi_insert_chain_in_prio_list(chain);

	if (!chain->blocks.current_extent) {
		chain->blocks.current_extent = chain->blocks.first;
		if (chain->blocks.current_extent)
			chain->blocks.current_offset = chain->blocks.current_extent->start;
	}
	return 0;
}

int toi_load_extent_chains(void)
{
	int result;
	int to_load;
	int i;
	int extents_loaded = 0;

	result = toiActiveAllocator->rw_header_chunk_noreadahead(READ, NULL,
								 (char *)&to_load, sizeof(int));
	if (result)
		return result;
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "%d chains to read.", to_load);

	for (i = 0; i < to_load; i++) {
		toi_message(TOI_BIO, TOI_VERBOSE, 0, " >> Loading chain %d/%d.", i, to_load);
		result = toi_load_extent_chain(i, &extents_loaded);
		if (result)
			return result;
	}

	/* If we never got to a second extent, we still need to do this. */
	if (extents_loaded == 1)
		set_up_start_position();

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Save chain numbers.");
	result = toiActiveAllocator->rw_header_chunk_noreadahead(READ,
								 &toi_blockwriter_ops,
								 (char *)&toi_writer_posn.
								 saved_chain_number[0],
								 4 * sizeof(int));

	return result;
}

static int toi_end_of_stream(int writing, int section_barrier)
{
	struct toi_bdev_info *cur_chain = toi_writer_posn.current_chain;
	int compare_to = next_section[current_stream];
	struct toi_bdev_info *compare_chain = toi_writer_posn.saved_chain_ptr[compare_to];
	int compare_offset = compare_chain ? compare_chain->saved_state[compare_to].offset : 0;

	if (!section_barrier)
		return 0;

	if (!cur_chain)
		return 1;

	if (cur_chain == compare_chain && cur_chain->blocks.current_offset == compare_offset) {
		if (writing) {
			if (!current_stream) {
				debug_broken_header();
				return 1;
			}
		} else {
			more_readahead = 0;
			toi_message(TOI_BIO, TOI_VERBOSE, 0,
				    "Reached the end of stream %d "
				    "(not an error).", current_stream);
			return 1;
		}
	}

	return 0;
}

/**
 * go_next_page - skip blocks to the start of the next page
 * @writing: Whether we're reading or writing the image.
 *
 * Go forward one page.
 **/
int go_next_page(int writing, int section_barrier)
{
	struct toi_bdev_info *cur_chain = toi_writer_posn.current_chain;
	int max = cur_chain ? cur_chain->blocks_per_page : 1;

	/* Nope. Go foward a page - or maybe two. Don't stripe the header,
	 * so that bad fragmentation doesn't put the extent data containing
	 * the location of the second page out of the first header page.
	 */
	if (toi_extent_state_next(max, current_stream)) {
		/* Don't complain if readahead falls off the end */
		if (writing && section_barrier) {
			toi_message(TOI_BIO, TOI_VERBOSE, 0, "Extent state eof. "
				    "Expected compression ratio too optimistic?");
			if (test_action_state(TOI_LOGALL))
				dump_block_chains();
		}
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "Ran out of extents to "
			    "read/write. (Not necessarily a fatal error.");
		return -ENOSPC;
	}

	return 0;
}

int devices_of_same_priority(struct toi_bdev_info *this)
{
	struct toi_bdev_info *check = prio_chain_head;
	int i = 0;

	while (check) {
		if (check->prio == this->prio)
			i++;
		check = check->next;
	}

	return i;
}

/**
 * toi_bio_rw_page - do i/o on the next disk page in the image
 * @writing: Whether reading or writing.
 * @page: Page to do i/o on.
 * @is_readahead: Whether we're doing readahead
 * @free_group: The group used in allocating the page
 *
 * Submit a page for reading or writing, possibly readahead.
 * Pass the group used in allocating the page as well, as it should
 * be freed on completion of the bio if we're writing the page.
 **/
int toi_bio_rw_page(int writing, struct page *page, int is_readahead, int free_group)
{
	int result = toi_end_of_stream(writing, 1);
	struct toi_bdev_info *dev_info = toi_writer_posn.current_chain;

	if (result) {
		if (writing)
			abort_hibernate(TOI_INSUFFICIENT_STORAGE,
					"Insufficient storage for your image.");
		else
			toi_message(TOI_BIO, TOI_VERBOSE, 0, "Seeking to "
				    "read/write another page when stream has " "ended.");
		return -ENOSPC;
	}

	toi_message(TOI_BIO, TOI_VERBOSE, 0,
		    "%s %lx:%ld",
		    writing ? "Write" : "Read", dev_info->dev_t, dev_info->blocks.current_offset);

	result = toi_do_io(writing, dev_info->bdev,
			   dev_info->blocks.current_offset << dev_info->bmap_shift,
			   page, is_readahead, 0, free_group);

	/* Ignore the result here - will check end of stream if come in again */
	go_next_page(writing, 1);

	if (result)
		printk(KERN_ERR "toi_do_io returned %d.\n", result);
	return result;
}

dev_t get_header_dev_t(void)
{
	return prio_chain_head->dev_t;
}

struct block_device *get_header_bdev(void)
{
	return prio_chain_head->bdev;
}

unsigned long get_headerblock(void)
{
	return prio_chain_head->blocks.first->start << prio_chain_head->bmap_shift;
}

int get_main_pool_phys_params(void)
{
	struct toi_bdev_info *this = prio_chain_head;
	int result;

	while (this) {
		result = this->allocator->bio_allocator_ops->bmap(this);
		if (result)
			return result;
		this = this->next;
	}

	return 0;
}

static int apply_header_reservation(void)
{
	int i;

	if (!header_pages_reserved) {
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "No header pages reserved at the moment.");
		return 0;
	}

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Applying header reservation.");

	/* Apply header space reservation */
	toi_extent_state_goto_start();

	for (i = 0; i < header_pages_reserved; i++)
		if (go_next_page(1, 0))
			return -ENOSPC;

	/* The end of header pages will be the start of pageset 2 */
	toi_extent_state_save(2);

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Finished applying header reservation.");
	return 0;
}

static int toi_bio_register_storage(void)
{
	int result = 0;
	struct toi_module_ops *this_module;

	list_for_each_entry(this_module, &toi_modules, module_list) {
		if (!this_module->enabled || this_module->type != BIO_ALLOCATOR_MODULE)
			continue;
		toi_message(TOI_BIO, TOI_VERBOSE, 0,
			    "Registering storage from %s.", this_module->name);
		result = this_module->bio_allocator_ops->register_storage();
		if (result)
			break;
	}

	return result;
}

int toi_bio_allocate_storage(unsigned long request)
{
	struct toi_bdev_info *chain = prio_chain_head;
	unsigned long to_get = request;
	unsigned long extra_pages, needed;
	int no_free = 0;

	if (!chain) {
		int result = toi_bio_register_storage();
		toi_message(TOI_BIO, TOI_VERBOSE, 0, "toi_bio_allocate_storage: "
			    "Registering storage.");
		if (result)
			return 0;
		chain = prio_chain_head;
		if (!chain) {
			printk("TuxOnIce: No storage was registered.\n");
			return 0;
		}
	}

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "toi_bio_allocate_storage: "
		    "Request is %lu pages.", request);
	extra_pages = DIV_ROUND_UP(request * (sizeof(unsigned long)
					      + sizeof(int)), PAGE_SIZE);
	needed = request + extra_pages + header_pages_reserved;
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Adding %lu extra pages and %lu "
		    "for header => %lu.", extra_pages, header_pages_reserved, needed);
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Already allocated %lu pages.", raw_pages_allocd);

	to_get = needed > raw_pages_allocd ? needed - raw_pages_allocd : 0;
	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Need to get %lu pages.", to_get);

	if (!to_get)
		return apply_header_reservation();

	while (to_get && chain) {
		int num_group = devices_of_same_priority(chain);
		int divisor = num_group - no_free;
		int i;
		unsigned long portion = DIV_ROUND_UP(to_get, divisor);
		unsigned long got = 0;
		unsigned long got_this_round = 0;
		struct toi_bdev_info *top = chain;

		toi_message(TOI_BIO, TOI_VERBOSE, 0,
			    " Start of loop. To get is %lu. Divisor is %d.", to_get, divisor);
		no_free = 0;

		/*
		 * We're aiming to spread the allocated storage as evenly
		 * as possible, but we also want to get all the storage we
		 * can off this priority.
		 */
		for (i = 0; i < num_group; i++) {
			struct toi_bio_allocator_ops *ops = chain->allocator->bio_allocator_ops;
			toi_message(TOI_BIO, TOI_VERBOSE, 0,
				    " Asking for %lu pages from chain %p.", portion, chain);
			got = ops->allocate_storage(chain, portion);
			toi_message(TOI_BIO, TOI_VERBOSE, 0,
				    " Got %lu pages from allocator %p.", got, chain);
			if (!got)
				no_free++;
			got_this_round += got;
			chain = chain->next;
		}
		toi_message(TOI_BIO, TOI_VERBOSE, 0, " Loop finished. Got a "
			    "total of %lu pages from %d allocators.",
			    got_this_round, divisor - no_free);

		raw_pages_allocd += got_this_round;
		to_get = needed > raw_pages_allocd ? needed - raw_pages_allocd : 0;

		/*
		 * If we got anything from chains of this priority and we
		 * still have storage to allocate, go over this priority
		 * again.
		 */
		if (got_this_round && to_get)
			chain = top;
		else
			no_free = 0;
	}

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Finished allocating. Calling "
		    "get_main_pool_phys_params");
	/* Now let swap allocator bmap the pages */
	get_main_pool_phys_params();

	toi_message(TOI_BIO, TOI_VERBOSE, 0, "Done. Reserving header.");
	return apply_header_reservation();
}

void toi_bio_chains_post_atomic(struct toi_boot_kernel_data *bkd)
{
	int i = 0;
	struct toi_bdev_info *cur_chain = prio_chain_head;

	while (cur_chain) {
		cur_chain->pages_used = bkd->pages_used[i];
		cur_chain = cur_chain->next;
		i++;
	}
}

int toi_bio_chains_debug_info(char *buffer, int size)
{
	/* Show what we actually used */
	struct toi_bdev_info *cur_chain = prio_chain_head;
	int len = 0;

	while (cur_chain) {
		len += scnprintf(buffer + len, size - len, "  Used %lu pages "
				 "from %s.\n", cur_chain->pages_used, cur_chain->name);
		cur_chain = cur_chain->next;
	}

	return len;
}
