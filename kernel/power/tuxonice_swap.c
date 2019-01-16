/*
 * kernel/power/tuxonice_swap.c
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * Distributed under GPLv2.
 *
 * This file encapsulates functions for usage of swap space as a
 * backing store.
 */

#include <linux/suspend.h>
#include <linux/blkdev.h>
#include <linux/swapops.h>
#include <linux/swap.h>
#include <linux/syscalls.h>
#include <linux/fs_uuid.h>

#include "tuxonice.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_modules.h"
#include "tuxonice_io.h"
#include "tuxonice_ui.h"
#include "tuxonice_extent.h"
#include "tuxonice_bio.h"
#include "tuxonice_alloc.h"
#include "tuxonice_builtin.h"

static struct toi_module_ops toi_swapops;

/* For swapfile automatically swapon/off'd. */
static char swapfilename[255] = "";
static int toi_swapon_status;

/* Swap Pages */
static unsigned long swap_allocated;

static struct sysinfo swapinfo;

static int is_ram_backed(struct swap_info_struct *si)
{
	if (!strncmp(si->bdev->bd_disk->disk_name, "ram", 3) ||
	    !strncmp(si->bdev->bd_disk->disk_name, "zram", 4))
		return 1;

	return 0;
}

/**
 * enable_swapfile: Swapon the user specified swapfile prior to hibernating.
 *
 * Activate the given swapfile if it wasn't already enabled. Remember whether
 * we really did swapon it for swapoffing later.
 */
static void enable_swapfile(void)
{
	int activateswapresult = -EINVAL;

	hib_log("swapfilename = '%s'\n", swapfilename);

	if (swapfilename[0]) {
		/* Attempt to swap on with maximum priority */
		activateswapresult = sys_swapon(swapfilename, 0xFFFF);
		if (activateswapresult && activateswapresult != -EBUSY)
			printk(KERN_ERR "TuxOnIce: The swapfile/partition "
			       "specified by /sys/power/tuxonice/swap/swapfile"
			       " (%s) could not be turned on (error %d). "
			       "Attempting to continue.\n", swapfilename, activateswapresult);
		if (!activateswapresult)
			toi_swapon_status = 1;
	}
}

/**
 * disable_swapfile: Swapoff any file swaponed at the start of the cycle.
 *
 * If we did successfully swapon a file at the start of the cycle, swapoff
 * it now (finishing up).
 */
static void disable_swapfile(void)
{
	if (!toi_swapon_status)
		return;

	hib_log("swapfilename = '%s'\n", swapfilename);
	sys_swapoff(swapfilename);
	toi_swapon_status = 0;
}

static int add_blocks_to_extent_chain(struct toi_bdev_info *chain,
				      unsigned long start, unsigned long end)
{
	if (test_action_state(TOI_TEST_BIO))
		toi_message(TOI_IO, TOI_VERBOSE, 0, "Adding extent %lu-%lu to "
			    "chain %p.", start << chain->bmap_shift,
			    end << chain->bmap_shift, chain);

	return toi_add_to_extent_chain(&chain->blocks, start, end);
}


static int get_main_pool_phys_params(struct toi_bdev_info *chain)
{
	struct hibernate_extent *extentpointer = NULL;
	unsigned long address, extent_min = 0, extent_max = 0;
	int empty = 1;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "get main pool phys params for "
		    "chain %d.", chain->allocator_index);

	if (!chain->allocations.first)
		return 0;

	if (chain->blocks.first)
		toi_put_extent_chain(&chain->blocks);

	toi_extent_for_each(&chain->allocations, extentpointer, address) {
		swp_entry_t swap_address = (swp_entry_t) { address };
		struct block_device *bdev;
		sector_t new_sector = map_swap_entry(swap_address, &bdev);

		if (empty) {
			empty = 0;
			extent_min = extent_max = new_sector;
			continue;
		}

		if (new_sector == extent_max + 1) {
			extent_max++;
			continue;
		}

		if (add_blocks_to_extent_chain(chain, extent_min, extent_max)) {
			printk(KERN_ERR "Out of memory while making block " "chains.\n");
			return -ENOMEM;
		}

		extent_min = new_sector;
		extent_max = new_sector;
	}

	if (!empty && add_blocks_to_extent_chain(chain, extent_min, extent_max)) {
		printk(KERN_ERR "Out of memory while making block chains.\n");
		return -ENOMEM;
	}

	return 0;
}

/*
 * Like si_swapinfo, except that we don't include ram backed swap (compcache!)
 * and don't need to use the spinlocks (userspace is stopped when this
 * function is called).
 */
void si_swapinfo_no_compcache(void)
{
	unsigned int i;

	si_swapinfo(&swapinfo);
	swapinfo.freeswap = 0;
	swapinfo.totalswap = 0;

	for (i = 0; i < MAX_SWAPFILES; i++) {
		struct swap_info_struct *si = get_swap_info_struct(i);
		if (si && (si->flags & SWP_WRITEOK) && !is_ram_backed(si)) {
			swapinfo.totalswap += si->inuse_pages;
			swapinfo.freeswap += si->pages - si->inuse_pages;
		}
	}
}

/*
 * We can't just remember the value from allocation time, because other
 * processes might have allocated swap in the mean time.
 */
static unsigned long toi_swap_storage_available(void)
{
	toi_message(TOI_IO, TOI_VERBOSE, 0, "In toi_swap_storage_available.");
	si_swapinfo_no_compcache();
	return swapinfo.freeswap + swap_allocated;
}

static int toi_swap_initialise(int starting_cycle)
{
	if (!starting_cycle)
		return 0;

	enable_swapfile();
	return 0;
}

static void toi_swap_cleanup(int ending_cycle)
{
	if (!ending_cycle)
		return;

	disable_swapfile();
}

static void toi_swap_free_storage(struct toi_bdev_info *chain)
{
	/* Free swap entries */
	struct hibernate_extent *extentpointer;
	unsigned long extentvalue;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "Freeing storage for chain %p.", chain);

	swap_allocated -= chain->allocations.size;
	toi_extent_for_each(&chain->allocations, extentpointer, extentvalue)
	    swap_free((swp_entry_t) {
		      extentvalue});

	toi_put_extent_chain(&chain->allocations);
}

static void free_swap_range(unsigned long min, unsigned long max)
{
	int j;

	for (j = min; j <= max; j++)
		swap_free((swp_entry_t) {
			  j}
	);
	swap_allocated -= (max - min + 1);
}

/*
 * Allocation of a single swap type. Swap priorities are handled at the higher
 * level.
 */
static int toi_swap_allocate_storage(struct toi_bdev_info *chain, unsigned long request)
{
	unsigned long gotten = 0;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "  Swap allocate storage: Asked to"
		    " allocate %lu pages from device %d.", request, chain->allocator_index);

	while (gotten < request) {
		swp_entry_t start, end;
		get_swap_range_of_type(chain->allocator_index, &start, &end, request - gotten + 1);
		if (start.val) {
			int added = end.val - start.val + 1;
			if (toi_add_to_extent_chain(&chain->allocations, start.val, end.val)) {
				printk(KERN_INFO "Failed to allocate extent for "
				       "%lu-%lu.\n", start.val, end.val);
				free_swap_range(start.val, end.val);
				break;
			}
			gotten += added;
			swap_allocated += added;
		} else
			break;
	}

	toi_message(TOI_IO, TOI_VERBOSE, 0, "  Allocated %lu pages.", gotten);
	return gotten;
}

static int toi_swap_register_storage(void)
{
	int i, result = 0;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "toi_swap_register_storage.");
	for (i = 0; i < MAX_SWAPFILES; i++) {
		struct swap_info_struct *si = get_swap_info_struct(i);
		struct toi_bdev_info *devinfo;
		unsigned char *p;
		unsigned char buf[256];
		struct fs_info *fs_info;

		if (!si || !(si->flags & SWP_WRITEOK) || is_ram_backed(si))
			continue;

		devinfo = toi_kzalloc(39, sizeof(struct toi_bdev_info), GFP_ATOMIC);
		if (!devinfo) {
			printk("Failed to allocate devinfo struct for swap " "device %d.\n", i);
			return -ENOMEM;
		}

		devinfo->bdev = si->bdev;
		devinfo->allocator = &toi_swapops;
		devinfo->allocator_index = i;

		fs_info = fs_info_from_block_dev(si->bdev);
		if (fs_info && !IS_ERR(fs_info)) {
			memcpy(devinfo->uuid, &fs_info->uuid, 16);
			free_fs_info(fs_info);
		} else
			result = (int)PTR_ERR(fs_info);

		if (!fs_info)
			printk("fs_info from block dev returned %d.\n", result);
		devinfo->dev_t = si->bdev->bd_dev;
		devinfo->prio = si->prio;
		devinfo->bmap_shift = 3;
		devinfo->blocks_per_page = 1;

		p = d_path(&si->swap_file->f_path, buf, sizeof(buf));
		sprintf(devinfo->name, "swap on %s", p);

		toi_message(TOI_IO, TOI_VERBOSE, 0, "Registering swap storage:"
			    " Device %d (%lx), prio %d.", i,
			    (unsigned long)devinfo->dev_t, devinfo->prio);
		toi_bio_ops.register_storage(devinfo);
	}

	return 0;
}

/*
 * workspace_size
 *
 * Description:
 * Returns the number of bytes of RAM needed for this
 * code to do its work. (Used when calculating whether
 * we have enough memory to be able to hibernate & resume).
 *
 */
static int toi_swap_memory_needed(void)
{
	return 1;
}

/*
 * Print debug info
 *
 * Description:
 */
static int toi_swap_print_debug_stats(char *buffer, int size)
{
	int len = 0;

	len = scnprintf(buffer, size, "- Swap Allocator enabled.\n");
	if (swapfilename[0])
		len += scnprintf(buffer + len, size - len,
				 "  Attempting to automatically swapon: %s.\n", swapfilename);

	si_swapinfo_no_compcache();

	len += scnprintf(buffer + len, size - len,
			 "  Swap available for image: %lu+%lu pages.\n",
			 swapinfo.freeswap, swap_allocated);

	return len;
}

static int header_locations_read_sysfs(const char *page, int count)
{
	int i, printedpartitionsmessage = 0, len = 0, haveswap = 0;
	struct inode *swapf = NULL;
	int zone;
	char *path_page = (char *)toi_get_free_page(10, GFP_KERNEL);
	char *path, *output = (char *)page;
	int path_len;

	if (!page)
		return 0;

	for (i = 0; i < MAX_SWAPFILES; i++) {
		struct swap_info_struct *si = get_swap_info_struct(i);

		if (!si || !(si->flags & SWP_WRITEOK))
			continue;

		if (S_ISBLK(si->swap_file->f_mapping->host->i_mode)) {
			haveswap = 1;
			if (!printedpartitionsmessage) {
				len += sprintf(output + len,
					       "For swap partitions, simply use the "
					       "format: resume=swap:/dev/hda1.\n");
				printedpartitionsmessage = 1;
			}
		} else {
			path_len = 0;

			path = d_path(&si->swap_file->f_path, path_page, PAGE_SIZE);
			path_len = snprintf(path_page, PAGE_SIZE, "%s", path);

			haveswap = 1;
			swapf = si->swap_file->f_mapping->host;
			zone = bmap(swapf, 0);
			if (!zone) {
				len += sprintf(output + len,
					       "Swapfile %s has been corrupted. Reuse"
					       " mkswap on it and try again.\n", path_page);
			} else {
				char name_buffer[BDEVNAME_SIZE];
				len += sprintf(output + len,
					       "For swapfile `%s`,"
					       " use resume=swap:/dev/%s:0x%x.\n",
					       path_page,
					       bdevname(si->bdev, name_buffer),
					       zone << (swapf->i_blkbits - 9));
			}
		}
	}

	if (!haveswap)
		len = sprintf(output, "You need to turn on swap partitions "
			      "before examining this file.\n");

	toi_free_page(10, (unsigned long)path_page);
	return len;
}

static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_STRING("swapfilename", SYSFS_RW, swapfilename, 255, 0, NULL),
	SYSFS_CUSTOM("headerlocations", SYSFS_READONLY,
		     header_locations_read_sysfs, NULL, 0, NULL),
	SYSFS_INT("enabled", SYSFS_RW, &toi_swapops.enabled, 0, 1, 0,
		  attempt_to_parse_resume_device2),
};

static struct toi_bio_allocator_ops toi_bio_swapops = {
	.register_storage = toi_swap_register_storage,
	.storage_available = toi_swap_storage_available,
	.allocate_storage = toi_swap_allocate_storage,
	.bmap = get_main_pool_phys_params,
	.free_storage = toi_swap_free_storage,
};

static struct toi_module_ops toi_swapops = {
	.type = BIO_ALLOCATOR_MODULE,
	.name = "swap storage",
	.directory = "swap",
	.module = THIS_MODULE,
	.memory_needed = toi_swap_memory_needed,
	.print_debug_info = toi_swap_print_debug_stats,
	.initialise = toi_swap_initialise,
	.cleanup = toi_swap_cleanup,
	.bio_allocator_ops = &toi_bio_swapops,

	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

/* ---- Registration ---- */
static __init int toi_swap_load(void)
{
	return toi_register_module(&toi_swapops);
}

#ifdef MODULE
static __exit void toi_swap_unload(void)
{
	toi_unregister_module(&toi_swapops);
}
module_init(toi_swap_load);
module_exit(toi_swap_unload);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nigel Cunningham");
MODULE_DESCRIPTION("TuxOnIce SwapAllocator");
#else
late_initcall(toi_swap_load);
#endif
