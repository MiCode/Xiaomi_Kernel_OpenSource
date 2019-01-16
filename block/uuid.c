#include <linux/blkdev.h>
#include <linux/ctype.h>
#include <linux/fs_uuid.h>
#include <linux/slab.h>
#include <linux/export.h>

static int debug_enabled;

#define PRINTK(fmt, args...) do {					\
	if (debug_enabled)						\
		printk(KERN_DEBUG fmt, ## args);			\
	} while (0)

#define PRINT_HEX_DUMP(v1, v2, v3, v4, v5, v6, v7, v8)			\
	do {								\
		if (debug_enabled)					\
			print_hex_dump(v1, v2, v3, v4, v5, v6, v7, v8);	\
	} while (0)

/*
 * Simple UUID translation
 */

struct uuid_info {
	const char *key;
	const char *name;
	long bkoff;
	unsigned sboff;
	unsigned sig_len;
	const char *magic;
	int uuid_offset;
	int last_mount_offset;
	int last_mount_size;
};

/*
 * Based on libuuid's blkid_magic array. Note that I don't
 * have uuid offsets for all of these yet - mssing ones are 0x0.
 * Further information welcome.
 *
 * Rearranged by page of fs signature for optimisation.
 */
static struct uuid_info uuid_list[] = {
	{NULL, "oracleasm", 0, 32, 8, "ORCLDISK", 0x0, 0, 0},
	{"ntfs", "ntfs", 0, 3, 8, "NTFS    ", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0x52, 5, "MSWIN", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0x52, 8, "FAT32   ", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0x36, 5, "MSDOS", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0x36, 8, "FAT16   ", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0x36, 8, "FAT12   ", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0, 1, "\353", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0, 1, "\351", 0x0, 0, 0},
	{"vfat", "vfat", 0, 0x1fe, 2, "\125\252", 0x0, 0, 0},
	{"xfs", "xfs", 0, 0, 4, "XFSB", 0x20, 0, 0},
	{"romfs", "romfs", 0, 0, 8, "-rom1fs-", 0x0, 0, 0},
	{"bfs", "bfs", 0, 0, 4, "\316\372\173\033", 0, 0, 0},
	{"cramfs", "cramfs", 0, 0, 4, "E=\315\050", 0x0, 0, 0},
	{"qnx4", "qnx4", 0, 4, 6, "QNX4FS", 0, 0, 0},
	{NULL, "crypt_LUKS", 0, 0, 6, "LUKS\xba\xbe", 0x0, 0, 0},
	{"squashfs", "squashfs", 0, 0, 4, "sqsh", 0, 0, 0},
	{"squashfs", "squashfs", 0, 0, 4, "hsqs", 0, 0, 0},
	{"ocfs", "ocfs", 0, 8, 9, "OracleCFS", 0x0, 0, 0},
	{"lvm2pv", "lvm2pv", 0, 0x018, 8, "LVM2 001", 0x0, 0, 0},
	{"sysv", "sysv", 0, 0x3f8, 4, "\020~\030\375", 0, 0, 0},
	{"ext", "ext", 1, 0x38, 2, "\123\357", 0x468, 0x42c, 4},
	{"minix", "minix", 1, 0x10, 2, "\177\023", 0, 0, 0},
	{"minix", "minix", 1, 0x10, 2, "\217\023", 0, 0, 0},
	{"minix", "minix", 1, 0x10, 2, "\150\044", 0, 0, 0},
	{"minix", "minix", 1, 0x10, 2, "\170\044", 0, 0, 0},
	{"lvm2pv", "lvm2pv", 1, 0x018, 8, "LVM2 001", 0x0, 0, 0},
	{"vxfs", "vxfs", 1, 0, 4, "\365\374\001\245", 0, 0, 0},
	{"hfsplus", "hfsplus", 1, 0, 2, "BD", 0x0, 0, 0},
	{"hfsplus", "hfsplus", 1, 0, 2, "H+", 0x0, 0, 0},
	{"hfsplus", "hfsplus", 1, 0, 2, "HX", 0x0, 0, 0},
	{"hfs", "hfs", 1, 0, 2, "BD", 0x0, 0, 0},
	{"ocfs2", "ocfs2", 1, 0, 6, "OCFSV2", 0x0, 0, 0},
	{"lvm2pv", "lvm2pv", 0, 0x218, 8, "LVM2 001", 0x0, 0, 0},
	{"lvm2pv", "lvm2pv", 1, 0x218, 8, "LVM2 001", 0x0, 0, 0},
	{"ocfs2", "ocfs2", 2, 0, 6, "OCFSV2", 0x0, 0, 0},
	{"swap", "swap", 0, 0xff6, 10, "SWAP-SPACE", 0x40c, 0, 0},
	{"swap", "swap", 0, 0xff6, 10, "SWAPSPACE2", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0xff6, 9, "S1SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0xff6, 9, "S2SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0xff6, 9, "ULSUSPEND", 0x40c, 0, 0},
	{"ocfs2", "ocfs2", 4, 0, 6, "OCFSV2", 0x0, 0, 0},
	{"ocfs2", "ocfs2", 8, 0, 6, "OCFSV2", 0x0, 0, 0},
	{"hpfs", "hpfs", 8, 0, 4, "I\350\225\371", 0, 0, 0},
	{"reiserfs", "reiserfs", 8, 0x34, 8, "ReIsErFs", 0x10054, 0, 0},
	{"reiserfs", "reiserfs", 8, 20, 8, "ReIsErFs", 0x10054, 0, 0},
	{"zfs", "zfs", 8, 0, 8, "\0\0\x02\xf5\xb0\x07\xb1\x0c", 0x0, 0, 0},
	{"zfs", "zfs", 8, 0, 8, "\x0c\xb1\x07\xb0\xf5\x02\0\0", 0x0, 0, 0},
	{"ufs", "ufs", 8, 0x55c, 4, "T\031\001\000", 0, 0, 0},
	{"swap", "swap", 0, 0x1ff6, 10, "SWAP-SPACE", 0x40c, 0, 0},
	{"swap", "swap", 0, 0x1ff6, 10, "SWAPSPACE2", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x1ff6, 9, "S1SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x1ff6, 9, "S2SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x1ff6, 9, "ULSUSPEND", 0x40c, 0, 0},
	{"reiserfs", "reiserfs", 64, 0x34, 9, "ReIsEr2Fs", 0x10054, 0, 0},
	{"reiserfs", "reiserfs", 64, 0x34, 9, "ReIsEr3Fs", 0x10054, 0, 0},
	{"reiserfs", "reiserfs", 64, 0x34, 8, "ReIsErFs", 0x10054, 0, 0},
	{"reiser4", "reiser4", 64, 0, 7, "ReIsEr4", 0x100544, 0, 0},
	{"gfs2", "gfs2", 64, 0, 4, "\x01\x16\x19\x70", 0x0, 0, 0},
	{"gfs", "gfs", 64, 0, 4, "\x01\x16\x19\x70", 0x0, 0, 0},
	{"btrfs", "btrfs", 64, 0x40, 8, "_BHRfS_M", 0x0, 0, 0},
	{"swap", "swap", 0, 0x3ff6, 10, "SWAP-SPACE", 0x40c, 0, 0},
	{"swap", "swap", 0, 0x3ff6, 10, "SWAPSPACE2", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x3ff6, 9, "S1SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x3ff6, 9, "S2SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x3ff6, 9, "ULSUSPEND", 0x40c, 0, 0},
	{"udf", "udf", 32, 1, 5, "BEA01", 0x0, 0, 0},
	{"udf", "udf", 32, 1, 5, "BOOT2", 0x0, 0, 0},
	{"udf", "udf", 32, 1, 5, "CD001", 0x0, 0, 0},
	{"udf", "udf", 32, 1, 5, "CDW02", 0x0, 0, 0},
	{"udf", "udf", 32, 1, 5, "NSR02", 0x0, 0, 0},
	{"udf", "udf", 32, 1, 5, "NSR03", 0x0, 0, 0},
	{"udf", "udf", 32, 1, 5, "TEA01", 0x0, 0, 0},
	{"iso9660", "iso9660", 32, 1, 5, "CD001", 0x0, 0, 0},
	{"iso9660", "iso9660", 32, 9, 5, "CDROM", 0x0, 0, 0},
	{"jfs", "jfs", 32, 0, 4, "JFS1", 0x88, 0, 0},
	{"swap", "swap", 0, 0x7ff6, 10, "SWAP-SPACE", 0x40c, 0, 0},
	{"swap", "swap", 0, 0x7ff6, 10, "SWAPSPACE2", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x7ff6, 9, "S1SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x7ff6, 9, "S2SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0x7ff6, 9, "ULSUSPEND", 0x40c, 0, 0},
	{"swap", "swap", 0, 0xfff6, 10, "SWAP-SPACE", 0x40c, 0, 0},
	{"swap", "swap", 0, 0xfff6, 10, "SWAPSPACE2", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0xfff6, 9, "S1SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0xfff6, 9, "S2SUSPEND", 0x40c, 0, 0},
	{"swap", "swsuspend", 0, 0xfff6, 9, "ULSUSPEND", 0x40c, 0, 0},
	{"zfs", "zfs", 264, 0, 8, "\0\0\x02\xf5\xb0\x07\xb1\x0c", 0x0, 0, 0},
	{"zfs", "zfs", 264, 0, 8, "\x0c\xb1\x07\xb0\xf5\x02\0\0", 0x0, 0, 0},
	{NULL, NULL, 0, 0, 0, NULL, 0x0, 0, 0}
};

static int null_uuid(const char *uuid)
{
	int i;

	for (i = 0; i < 16 && !uuid[i]; i++);

	return (i == 16);
}


static void uuid_end_bio(struct bio *bio, int err)
{
	struct page *page = bio->bi_io_vec[0].bv_page;

	if (!test_bit(BIO_UPTODATE, &bio->bi_flags))
		SetPageError(page);

	unlock_page(page);
	bio_put(bio);
}


/**
 * submit - submit BIO request
 * @dev: The block device we're using.
 * @page_num: The page we're reading.
 *
 * Based on Patrick Mochell's pmdisk code from long ago: "Straight from the
 * textbook - allocate and initialize the bio. If we're writing, make sure
 * the page is marked as dirty. Then submit it and carry on."
 **/
static struct page *read_bdev_page(struct block_device *dev, int page_num)
{
	struct bio *bio = NULL;
	struct page *page = alloc_page(GFP_NOFS | __GFP_HIGHMEM);

	if (!page) {
		printk(KERN_ERR "Failed to allocate a page for reading data " "in UUID checks.");
		return NULL;
	}

	bio = bio_alloc(GFP_NOFS, 1);
	bio->bi_bdev = dev;
	bio->bi_sector = page_num << 3;
	bio->bi_end_io = uuid_end_bio;
	bio->bi_flags |= (1 << BIO_TOI);

	PRINTK("Submitting bio on device %lx, page %d using bio %p and page %p.\n",
	       (unsigned long)dev->bd_dev, page_num, bio, page);

	if (bio_add_page(bio, page, PAGE_SIZE, 0) < PAGE_SIZE) {
		printk(KERN_DEBUG "ERROR: adding page to bio at %d\n", page_num);
		bio_put(bio);
		__free_page(page);
		printk(KERN_DEBUG "read_bdev_page freed page %p (in error " "path).\n", page);
		return NULL;
	}

	lock_page(page);
	submit_bio(READ | REQ_SYNC, bio);

	wait_on_page_locked(page);
	if (PageError(page)) {
		__free_page(page);
		page = NULL;
	}
	return page;
}

int bdev_matches_key(struct block_device *bdev, const char *key)
{
	unsigned char *data = NULL;
	struct page *data_page = NULL;

	int dev_offset, pg_num, pg_off, i;
	int last_pg_num = -1;
	int result = 0;
	char buf[50];

	if (null_uuid(key)) {
		PRINTK("Refusing to find a NULL key.\n");
		return 0;
	}

	if (!bdev->bd_disk) {
		bdevname(bdev, buf);
		PRINTK("bdev %s has no bd_disk.\n", buf);
		return 0;
	}

	if (!bdev->bd_disk->queue) {
		bdevname(bdev, buf);
		PRINTK("bdev %s has no queue.\n", buf);
		return 0;
	}

	for (i = 0; uuid_list[i].name; i++) {
		struct uuid_info *dat = &uuid_list[i];

		if (!dat->key || strcmp(dat->key, key))
			continue;

		dev_offset = (dat->bkoff << 10) + dat->sboff;
		pg_num = dev_offset >> 12;
		pg_off = dev_offset & 0xfff;

		if ((((pg_num + 1) << 3) - 1) > bdev->bd_part->nr_sects >> 1)
			continue;

		if (pg_num != last_pg_num) {
			if (data_page) {
				kunmap(data_page);
				__free_page(data_page);
			}
			data_page = read_bdev_page(bdev, pg_num);
			if (!data_page)
				continue;
			data = kmap(data_page);
		}

		last_pg_num = pg_num;

		if (strncmp(&data[pg_off], dat->magic, dat->sig_len))
			continue;

		result = 1;
		break;
	}

	if (data_page) {
		kunmap(data_page);
		__free_page(data_page);
	}

	return result;
}

/*
 * part_matches_fs_info - Does the given partition match the details given?
 *
 * Returns a score saying how good the match is.
 * 0 = no UUID match.
 * 1 = UUID but last mount time differs.
 * 2 = UUID, last mount time but not dev_t
 * 3 = perfect match
 *
 * This lets us cope elegantly with probing resulting in dev_ts changing
 * from boot to boot, and with the case where a user copies a partition
 * (UUID is non unique), and we need to check the last mount time of the
 * correct partition.
 */
int part_matches_fs_info(struct hd_struct *part, struct fs_info *seek)
{
	struct block_device *bdev;
	struct fs_info *got;
	int result = 0;
	char buf[50];

	if (null_uuid((char *)&seek->uuid)) {
		PRINTK("Refusing to find a NULL uuid.\n");
		return 0;
	}

	bdev = bdget(part_devt(part));

	PRINTK("part_matches fs info considering %x.\n", part_devt(part));

	if (blkdev_get(bdev, FMODE_READ, 0)) {
		PRINTK("blkdev_get failed.\n");
		return 0;
	}

	if (!bdev->bd_disk) {
		bdevname(bdev, buf);
		PRINTK("bdev %s has no bd_disk.\n", buf);
		goto out;
	}

	if (!bdev->bd_disk->queue) {
		bdevname(bdev, buf);
		PRINTK("bdev %s has no queue.\n", buf);
		goto out;
	}

	got = fs_info_from_block_dev(bdev);

	if (got && !memcmp(got->uuid, seek->uuid, 16)) {
		PRINTK(" Have matching UUID.\n");
		PRINTK(" Got: LMS %d, LM %p.\n", got->last_mount_size, got->last_mount);
		PRINTK(" Seek: LMS %d, LM %p.\n", seek->last_mount_size, seek->last_mount);
		result = 1;

		if (got->last_mount_size == seek->last_mount_size &&
		    got->last_mount && seek->last_mount &&
		    !memcmp(got->last_mount, seek->last_mount, got->last_mount_size)) {
			result = 2;

			PRINTK(" Matching last mount time.\n");

			if (part_devt(part) == seek->dev_t) {
				result = 3;
				PRINTK(" Matching dev_t.\n");
			} else
				PRINTK("Dev_ts differ (%x vs %x).\n", part_devt(part), seek->dev_t);
		}
	}

	PRINTK(" Score for %x is %d.\n", part_devt(part), result);
	free_fs_info(got);
 out:
	blkdev_put(bdev, FMODE_READ);
	return result;
}

void free_fs_info(struct fs_info *fs_info)
{
	if (!fs_info || IS_ERR(fs_info))
		return;

	if (fs_info->last_mount)
		kfree(fs_info->last_mount);

	kfree(fs_info);
}
EXPORT_SYMBOL_GPL(free_fs_info);

struct fs_info *fs_info_from_block_dev(struct block_device *bdev)
{
	unsigned char *data = NULL;
	struct page *data_page = NULL;

	int dev_offset, pg_num, pg_off;
	int uuid_pg_num, uuid_pg_off, i;
	unsigned char *uuid_data = NULL;
	struct page *uuid_data_page = NULL;

	int last_pg_num = -1, last_uuid_pg_num = 0;
	char buf[50];
	struct fs_info *fs_info = NULL;

	bdevname(bdev, buf);

	PRINTK("uuid_from_block_dev looking for partition type of %s.\n", buf);

	for (i = 0; uuid_list[i].name; i++) {
		struct uuid_info *dat = &uuid_list[i];
		dev_offset = (dat->bkoff << 10) + dat->sboff;
		pg_num = dev_offset >> 12;
		pg_off = dev_offset & 0xfff;
		uuid_pg_num = dat->uuid_offset >> 12;
		uuid_pg_off = dat->uuid_offset & 0xfff;

		if ((((pg_num + 1) << 3) - 1) > bdev->bd_part->nr_sects >> 1)
			continue;

		/* Ignore partition types with no UUID offset */
		if (!dat->uuid_offset)
			continue;

		if (pg_num != last_pg_num) {
			if (data_page) {
				kunmap(data_page);
				__free_page(data_page);
			}
			data_page = read_bdev_page(bdev, pg_num);
			if (!data_page)
				continue;
			data = kmap(data_page);
		}

		last_pg_num = pg_num;

		if (strncmp(&data[pg_off], dat->magic, dat->sig_len))
			continue;

		PRINTK("This partition looks like %s.\n", dat->name);

		fs_info = kzalloc(sizeof(struct fs_info), GFP_KERNEL);

		if (!fs_info) {
			PRINTK("Failed to allocate fs_info struct.");
			fs_info = ERR_PTR(-ENOMEM);
			break;
		}

		/* UUID can't be off the end of the disk */
		if ((uuid_pg_num > bdev->bd_part->nr_sects >> 3) || !dat->uuid_offset)
			goto no_uuid;

		if (!uuid_data || uuid_pg_num != last_uuid_pg_num) {
			/* No need to reread the page from above */
			if (uuid_pg_num == pg_num && uuid_data)
				memcpy(uuid_data, data, PAGE_SIZE);
			else {
				if (uuid_data_page) {
					kunmap(uuid_data_page);
					__free_page(uuid_data_page);
				}
				uuid_data_page = read_bdev_page(bdev, uuid_pg_num);
				if (!uuid_data_page)
					continue;
				uuid_data = kmap(uuid_data_page);
			}
		}

		last_uuid_pg_num = uuid_pg_num;
		memcpy(&fs_info->uuid, &uuid_data[uuid_pg_off], 16);
		fs_info->dev_t = bdev->bd_dev;

 no_uuid:
		PRINT_HEX_DUMP(KERN_EMERG, "fs_info_from_block_dev "
			       "returning uuid ", DUMP_PREFIX_NONE, 16, 1, fs_info->uuid, 16, 0);

		if (dat->last_mount_size) {
			int pg = dat->last_mount_offset >> 12, sz;
			int off = dat->last_mount_offset & 0xfff;
			struct page *last_mount = read_bdev_page(bdev, pg);
			unsigned char *last_mount_data;
			char *ptr;

			if (!last_mount) {
				fs_info = ERR_PTR(-ENOMEM);
				break;
			}
			last_mount_data = kmap(last_mount);
			sz = dat->last_mount_size;
			ptr = kmalloc(sz, GFP_KERNEL);

			if (!ptr) {
				printk(KERN_EMERG "fs_info_from_block_dev "
				       "failed to get memory for last mount " "timestamp.");
				free_fs_info(fs_info);
				fs_info = ERR_PTR(-ENOMEM);
			} else {
				fs_info->last_mount = ptr;
				fs_info->last_mount_size = sz;
				memcpy(ptr, &last_mount_data[off], sz);
			}

			kunmap(last_mount);
			__free_page(last_mount);
		}
		break;
	}

	if (data_page) {
		kunmap(data_page);
		__free_page(data_page);
	}

	if (uuid_data_page) {
		kunmap(uuid_data_page);
		__free_page(uuid_data_page);
	}

	return fs_info;
}
EXPORT_SYMBOL_GPL(fs_info_from_block_dev);

static int __init uuid_debug_setup(char *str)
{
	int value;

	if (sscanf(str, "=%d", &value))
		debug_enabled = value;

	return 1;
}

__setup("uuid_debug", uuid_debug_setup);
