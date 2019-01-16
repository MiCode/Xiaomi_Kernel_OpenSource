/*
 * kernel/power/tuxonice_file.c
 *
 * Copyright (C) 2005-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * Distributed under GPLv2.
 *
 * This file encapsulates functions for usage of a simple file as a
 * backing store. It is based upon the swapallocator, and shares the
 * same basic working. Here, though, we have nothing to do with
 * swapspace, and only one device to worry about.
 *
 * The user can just
 *
 * echo TuxOnIce > /path/to/my_file
 *
 * dd if=/dev/zero bs=1M count=<file_size_desired> >> /path/to/my_file
 *
 * and
 *
 * echo /path/to/my_file > /sys/power/tuxonice/file/target
 *
 * then put what they find in /sys/power/tuxonice/resume
 * as their resume= parameter in lilo.conf (and rerun lilo if using it).
 *
 * Having done this, they're ready to hibernate and resume.
 *
 * TODO:
 * - File resizing.
 */

#include <linux/blkdev.h>
#include <linux/mount.h>
#include <linux/fs.h>
#include <linux/fs_uuid.h>

#include "tuxonice.h"
#include "tuxonice_modules.h"
#include "tuxonice_bio.h"
#include "tuxonice_alloc.h"
#include "tuxonice_builtin.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_ui.h"
#include "tuxonice_io.h"

#define target_is_normal_file() (S_ISREG(target_inode->i_mode))

static struct toi_module_ops toi_fileops;

static struct file *target_file;
static struct block_device *toi_file_target_bdev;
static unsigned long pages_available, pages_allocated;
static char toi_file_target[256];
static struct inode *target_inode;
static int file_target_priority;
static int used_devt;
static int target_claim;
static dev_t toi_file_dev_t;
static int sig_page_index;

/* For test_toi_file_target */
static struct toi_bdev_info *file_chain;

static int has_contiguous_blocks(struct toi_bdev_info *dev_info, int page_num)
{
	int j;
	sector_t last = 0;

	for (j = 0; j < dev_info->blocks_per_page; j++) {
		sector_t this = bmap(target_inode,
				     page_num * dev_info->blocks_per_page + j);

		if (!this || (last && (last + 1) != this))
			break;

		last = this;
	}

	return j == dev_info->blocks_per_page;
}

static unsigned long get_usable_pages(struct toi_bdev_info *dev_info)
{
	unsigned long result = 0;
	struct block_device *bdev = dev_info->bdev;
	int i;

	switch (target_inode->i_mode & S_IFMT) {
	case S_IFSOCK:
	case S_IFCHR:
	case S_IFIFO:		/* Socket, Char, Fifo */
		return -1;
	case S_IFREG:		/* Regular file: current size - holes + free
				   space on part */
		for (i = 0; i < (target_inode->i_size >> PAGE_SHIFT); i++) {
			if (has_contiguous_blocks(dev_info, i))
				result++;
		}
		break;
	case S_IFBLK:		/* Block device */
		if (!bdev->bd_disk) {
			toi_message(TOI_IO, TOI_VERBOSE, 0, "bdev->bd_disk null.");
			return 0;
		}

		result = (bdev->bd_part ?
			  bdev->bd_part->nr_sects :
			  get_capacity(bdev->bd_disk)) >> (PAGE_SHIFT - 9);
	}


	return result;
}

static int toi_file_register_storage(void)
{
	struct toi_bdev_info *devinfo;
	int result = 0;
	struct fs_info *fs_info;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "toi_file_register_storage.");
	if (!strlen(toi_file_target)) {
		toi_message(TOI_IO, TOI_VERBOSE, 0, "Register file storage: "
			    "No target filename set.");
		return 0;
	}

	target_file = filp_open(toi_file_target, O_RDONLY | O_LARGEFILE, 0);
	toi_message(TOI_IO, TOI_VERBOSE, 0, "filp_open %s returned %p.",
		    toi_file_target, target_file);

	if (IS_ERR(target_file) || !target_file) {
		target_file = NULL;
		toi_file_dev_t = name_to_dev_t(toi_file_target);
		if (!toi_file_dev_t) {
			struct kstat stat;
			int error = vfs_stat(toi_file_target, &stat);
			printk(KERN_INFO "Open file %s returned %p and "
			       "name_to_devt failed.\n", toi_file_target, target_file);
			if (error) {
				printk(KERN_INFO "Stating the file also failed."
				       " Nothing more we can do.\n");
				return 0;
			} else
				toi_file_dev_t = stat.rdev;
		}

		toi_file_target_bdev = toi_open_by_devnum(toi_file_dev_t);
		if (IS_ERR(toi_file_target_bdev)) {
			printk(KERN_INFO "Got a dev_num (%lx) but failed to "
			       "open it.\n", (unsigned long)toi_file_dev_t);
			toi_file_target_bdev = NULL;
			return 0;
		}
		used_devt = 1;
		target_inode = toi_file_target_bdev->bd_inode;
	} else
		target_inode = target_file->f_mapping->host;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "Succeeded in opening the target.");
	if (S_ISLNK(target_inode->i_mode) || S_ISDIR(target_inode->i_mode) ||
	    S_ISSOCK(target_inode->i_mode) || S_ISFIFO(target_inode->i_mode)) {
		printk(KERN_INFO "File support works with regular files,"
		       " character files and block devices.\n");
		/* Cleanup routine will undo the above */
		return 0;
	}

	if (!used_devt) {
		if (S_ISBLK(target_inode->i_mode)) {
			toi_file_target_bdev = I_BDEV(target_inode);
			if (!blkdev_get(toi_file_target_bdev, FMODE_WRITE | FMODE_READ, NULL))
				target_claim = 1;
		} else
			toi_file_target_bdev = target_inode->i_sb->s_bdev;
		if (!toi_file_target_bdev) {
			printk(KERN_INFO "%s is not a valid file allocator "
			       "target.\n", toi_file_target);
			return 0;
		}
		toi_file_dev_t = toi_file_target_bdev->bd_dev;
	}

	devinfo = toi_kzalloc(39, sizeof(struct toi_bdev_info), GFP_ATOMIC);
	if (!devinfo) {
		printk("Failed to allocate a toi_bdev_info struct for the file allocator.\n");
		return -ENOMEM;
	}

	devinfo->bdev = toi_file_target_bdev;
	devinfo->allocator = &toi_fileops;
	devinfo->allocator_index = 0;

	fs_info = fs_info_from_block_dev(toi_file_target_bdev);
	if (fs_info && !IS_ERR(fs_info)) {
		memcpy(devinfo->uuid, &fs_info->uuid, 16);
		free_fs_info(fs_info);
	} else
		result = (int)PTR_ERR(fs_info);

	/* Unlike swap code, only complain if fs_info_from_block_dev returned
	 * -ENOMEM. The 'file' might be a full partition, so might validly not
	 * have an identifiable type, UUID etc.
	 */
	if (result)
		printk(KERN_DEBUG "Failed to get fs_info for file device (%d).\n", result);
	devinfo->dev_t = toi_file_dev_t;
	devinfo->prio = file_target_priority;
	devinfo->bmap_shift = target_inode->i_blkbits - 9;
	devinfo->blocks_per_page = (1 << (PAGE_SHIFT - target_inode->i_blkbits));
	sprintf(devinfo->name, "file %s", toi_file_target);
	file_chain = devinfo;
	toi_message(TOI_IO, TOI_VERBOSE, 0, "Dev_t is %lx. Prio is %d. Bmap "
		    "shift is %d. Blocks per page %d.",
		    devinfo->dev_t, devinfo->prio, devinfo->bmap_shift, devinfo->blocks_per_page);

	/* Keep one aside for the signature */
	pages_available = get_usable_pages(devinfo) - 1;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "Registering file storage, %lu "
		    "pages.", pages_available);

	toi_bio_ops.register_storage(devinfo);
	return 0;
}

static unsigned long toi_file_storage_available(void)
{
	return pages_available;
}

static int toi_file_allocate_storage(struct toi_bdev_info *chain, unsigned long request)
{
	unsigned long available = pages_available - pages_allocated;
	unsigned long to_add = min(available, request);

	toi_message(TOI_IO, TOI_VERBOSE, 0, "Pages available is %lu. Allocated "
		    "is %lu. Allocating %lu pages from file.",
		    pages_available, pages_allocated, to_add);
	pages_allocated += to_add;

	return to_add;
}

/**
 * __populate_block_list - add an extent to the chain
 * @min:	Start of the extent (first physical block = sector)
 * @max:	End of the extent (last physical block = sector)
 *
 * If TOI_TEST_BIO is set, print a debug message, outputting the min and max
 * fs block numbers.
 **/
static int __populate_block_list(struct toi_bdev_info *chain, int min, int max)
{
	if (test_action_state(TOI_TEST_BIO))
		toi_message(TOI_IO, TOI_VERBOSE, 0, "Adding extent %d-%d.",
			    min << chain->bmap_shift, ((max + 1) << chain->bmap_shift) - 1);

	return toi_add_to_extent_chain(&chain->blocks, min, max);
}

static int get_main_pool_phys_params(struct toi_bdev_info *chain)
{
	int i, extent_min = -1, extent_max = -1, result = 0, have_sig_page = 0;
	unsigned long pages_mapped = 0;

	toi_message(TOI_IO, TOI_VERBOSE, 0, "Getting file allocator blocks.");

	if (chain->blocks.first)
		toi_put_extent_chain(&chain->blocks);

	if (!target_is_normal_file()) {
		result = (pages_available > 0) ?
		    __populate_block_list(chain, chain->blocks_per_page,
					  (pages_allocated + 1) * chain->blocks_per_page - 1) : 0;
		return result;
	}

	/*
	 * FIXME: We are assuming the first page is contiguous. Is that
	 * assumption always right?
	 */

	for (i = 0; i < (target_inode->i_size >> PAGE_SHIFT); i++) {
		sector_t new_sector;

		if (!has_contiguous_blocks(chain, i))
			continue;

		if (!have_sig_page) {
			have_sig_page = 1;
			sig_page_index = i;
			continue;
		}

		pages_mapped++;

		/* Ignore first page - it has the header */
		if (pages_mapped == 1)
			continue;

		new_sector = bmap(target_inode, (i * chain->blocks_per_page));

		/*
		 * I'd love to be able to fill in holes and resize
		 * files, but not yet...
		 */

		if (new_sector == extent_max + 1)
			extent_max += chain->blocks_per_page;
		else {
			if (extent_min > -1) {
				result = __populate_block_list(chain, extent_min, extent_max);
				if (result)
					return result;
			}

			extent_min = new_sector;
			extent_max = extent_min + chain->blocks_per_page - 1;
		}

		if (pages_mapped == pages_allocated)
			break;
	}

	if (extent_min > -1) {
		result = __populate_block_list(chain, extent_min, extent_max);
		if (result)
			return result;
	}

	return 0;
}

static void toi_file_free_storage(struct toi_bdev_info *chain)
{
	pages_allocated = 0;
	file_chain = NULL;
}

/**
 * toi_file_print_debug_stats - print debug info
 * @buffer:	Buffer to data to populate
 * @size:	Size of the buffer
 **/
static int toi_file_print_debug_stats(char *buffer, int size)
{
	int len = scnprintf(buffer, size, "- File Allocator active.\n");

	len += scnprintf(buffer + len, size - len, "  Storage available for "
			 "image: %lu pages.\n", pages_available);

	return len;
}

static void toi_file_cleanup(int finishing_cycle)
{
	if (toi_file_target_bdev) {
		if (target_claim) {
			blkdev_put(toi_file_target_bdev, FMODE_WRITE | FMODE_READ);
			target_claim = 0;
		}

		if (used_devt) {
			blkdev_put(toi_file_target_bdev, FMODE_READ | FMODE_NDELAY);
			used_devt = 0;
		}
		toi_file_target_bdev = NULL;
		target_inode = NULL;
	}

	if (target_file) {
		filp_close(target_file, NULL);
		target_file = NULL;
	}

	pages_available = 0;
}

/**
 * test_toi_file_target - sysfs callback for /sys/power/tuxonince/file/target
 *
 * Test wheter the target file is valid for hibernating.
 **/
static void test_toi_file_target(void)
{
	int result = toi_file_register_storage();
	sector_t sector;
	char buf[50];
	struct fs_info *fs_info;

	if (result || !file_chain)
		return;

	/* This doesn't mean we're in business. Is any storage available? */
	if (!pages_available)
		goto out;

	toi_file_allocate_storage(file_chain, 1);
	result = get_main_pool_phys_params(file_chain);
	if (result)
		goto out;


	sector = bmap(target_inode, sig_page_index *
		      file_chain->blocks_per_page) << file_chain->bmap_shift;

	/* Use the uuid, or the dev_t if that fails */
	fs_info = fs_info_from_block_dev(toi_file_target_bdev);
	if (!fs_info || IS_ERR(fs_info)) {
		bdevname(toi_file_target_bdev, buf);
		sprintf(resume_file, "/dev/%s:%llu", buf, (unsigned long long)sector);
	} else {
		int i;
		hex_dump_to_buffer(fs_info->uuid, 16, 32, 1, buf, 50, 0);

		/* Remove the spaces */
		for (i = 1; i < 16; i++) {
			buf[2 * i] = buf[3 * i];
			buf[2 * i + 1] = buf[3 * i + 1];
		}
		buf[32] = 0;
		sprintf(resume_file, "UUID=%s:0x%llx", buf, (unsigned long long)sector);
		free_fs_info(fs_info);
	}

	toi_attempt_to_parse_resume_device(0);
 out:
	toi_file_free_storage(file_chain);
	toi_bio_ops.free_storage();
}

static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_STRING("target", SYSFS_RW, toi_file_target, 256,
		     SYSFS_NEEDS_SM_FOR_WRITE, test_toi_file_target),
	SYSFS_INT("enabled", SYSFS_RW, &toi_fileops.enabled, 0, 1, 0, NULL),
	SYSFS_INT("priority", SYSFS_RW, &file_target_priority, -4095,
		  4096, 0, NULL),
};

static struct toi_bio_allocator_ops toi_bio_fileops = {
	.register_storage = toi_file_register_storage,
	.storage_available = toi_file_storage_available,
	.allocate_storage = toi_file_allocate_storage,
	.bmap = get_main_pool_phys_params,
	.free_storage = toi_file_free_storage,
};

static struct toi_module_ops toi_fileops = {
	.type = BIO_ALLOCATOR_MODULE,
	.name = "file storage",
	.directory = "file",
	.module = THIS_MODULE,
	.print_debug_info = toi_file_print_debug_stats,
	.cleanup = toi_file_cleanup,
	.bio_allocator_ops = &toi_bio_fileops,

	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

/* ---- Registration ---- */
static __init int toi_file_load(void)
{
	return toi_register_module(&toi_fileops);
}

#ifdef MODULE
static __exit void toi_file_unload(void)
{
	toi_unregister_module(&toi_fileops);
}
module_init(toi_file_load);
module_exit(toi_file_unload);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nigel Cunningham");
MODULE_DESCRIPTION("TuxOnIce FileAllocator");
#else
late_initcall(toi_file_load);
#endif
