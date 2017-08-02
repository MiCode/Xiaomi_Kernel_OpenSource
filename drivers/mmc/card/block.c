/*
 * Block driver for media (i.e., flash cards)
 *
 * Copyright 2002 Hewlett-Packard Company
 * Copyright 2005-2008 Pierre Ossman
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * HEWLETT-PACKARD COMPANY MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Many thanks to Alessandro Rubini and Jonathan Corbet!
 *
 * Author:  Andrew Christian
 *          28 May 2002
 */
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/bitops.h>
#include <linux/string_helpers.h>
#include <linux/delay.h>
#include <linux/capability.h>
#include <linux/compat.h>
#include <linux/pm_runtime.h>
#include <linux/ioprio.h>

#include <trace/events/mmc.h>

#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>

#include <asm/uaccess.h>

#include "queue.h"

MODULE_ALIAS("mmc:block");
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "mmcblk."

#define INAND_CMD38_ARG_EXT_CSD  113
#define INAND_CMD38_ARG_ERASE    0x00
#define INAND_CMD38_ARG_TRIM     0x01
#define INAND_CMD38_ARG_SECERASE 0x80
#define INAND_CMD38_ARG_SECTRIM1 0x81
#define INAND_CMD38_ARG_SECTRIM2 0x88
#define MMC_BLK_TIMEOUT_MS  (30 * 1000)        /* 30 sec timeout */
#define MMC_SANITIZE_REQ_TIMEOUT 240000
#define MMC_EXTRACT_INDEX_FROM_ARG(x) ((x & 0x00FF0000) >> 16)
#define MMC_CMDQ_STOP_TIMEOUT_MS 100

#define mmc_req_rel_wr(req)	((req->cmd_flags & REQ_FUA) && \
				  (rq_data_dir(req) == WRITE))
#define PACKED_CMD_VER	0x01
#define PACKED_CMD_WR	0x02
#define PACKED_TRIGGER_MAX_ELEMENTS	5000

#define MMC_BLK_MAX_RETRIES 5 /* max # of retries before aborting a command */
#define MMC_BLK_UPDATE_STOP_REASON(stats, reason)			\
	do {								\
		if (stats->enabled)					\
			stats->pack_stop_reason[reason]++;		\
	} while (0)

#define MAX_RETRIES 5
#define PCKD_TRGR_INIT_MEAN_POTEN	17
#define PCKD_TRGR_POTEN_LOWER_BOUND	5
#define PCKD_TRGR_URGENT_PENALTY	2
#define PCKD_TRGR_LOWER_BOUND		5
#define PCKD_TRGR_PRECISION_MULTIPLIER	100

static struct mmc_cmdq_req *mmc_cmdq_prep_dcmd(
		struct mmc_queue_req *mqrq, struct mmc_queue *mq);
static DEFINE_MUTEX(block_mutex);

/*
 * The defaults come from config options but can be overriden by module
 * or bootarg options.
 */
static int perdev_minors = CONFIG_MMC_BLOCK_MINORS;

/*
 * We've only got one major, so number of mmcblk devices is
 * limited to (1 << 20) / number of minors per device.  It is also
 * currently limited by the size of the static bitmaps below.
 */
static int max_devices;

#define MAX_DEVICES 256

/* TODO: Replace these with struct ida */
static DECLARE_BITMAP(dev_use, MAX_DEVICES);
static DECLARE_BITMAP(name_use, MAX_DEVICES);

/*
 * There is one mmc_blk_data per slot.
 */
struct mmc_blk_data {
	spinlock_t	lock;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int	flags;
#define MMC_BLK_CMD23	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR	(1 << 1)	/* MMC Reliable write support */
#define MMC_BLK_PACKED_CMD	(1 << 2)	/* MMC packed command support */
#define MMC_BLK_CMD_QUEUE	(1 << 3) /* MMC command queue support */

	unsigned int	usage;
	unsigned int	read_only;
	unsigned int	part_type;
	unsigned int	name_idx;
	unsigned int	reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)
#define MMC_BLK_FLUSH		BIT(4)
#define MMC_BLK_PARTSWITCH	BIT(5)

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with dev_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int	part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	struct device_attribute num_wr_reqs_to_start_packing;
	struct device_attribute no_pack_for_random;
	int	area_type;
};

static DEFINE_MUTEX(open_lock);

enum {
	MMC_PACKED_NR_IDX = -1,
	MMC_PACKED_NR_ZERO,
	MMC_PACKED_NR_SINGLE,
};

module_param(perdev_minors, int, 0444);
MODULE_PARM_DESC(perdev_minors, "Minors numbers to allocate per device");

static inline int mmc_blk_part_switch(struct mmc_card *card,
				      struct mmc_blk_data *md);
static int get_card_status(struct mmc_card *card, u32 *status, int retries);
static int mmc_blk_cmdq_switch(struct mmc_card *card,
			       struct mmc_blk_data *md, bool enable);

static inline void mmc_blk_clear_packed(struct mmc_queue_req *mqrq)
{
	struct mmc_packed *packed = mqrq->packed;

	BUG_ON(!packed);

	mqrq->cmd_type = MMC_PACKED_NONE;
	packed->nr_entries = MMC_PACKED_NR_ZERO;
	packed->idx_failure = MMC_PACKED_NR_IDX;
	packed->retries = 0;
	packed->blocks = 0;
}

static struct mmc_blk_data *mmc_blk_get(struct gendisk *disk)
{
	struct mmc_blk_data *md;

	mutex_lock(&open_lock);
	md = disk->private_data;
	if (md && md->usage == 0)
		md = NULL;
	if (md)
		md->usage++;
	mutex_unlock(&open_lock);

	return md;
}

static inline int mmc_get_devidx(struct gendisk *disk)
{
	int devidx = disk->first_minor / perdev_minors;
	return devidx;
}

static void mmc_blk_put(struct mmc_blk_data *md)
{
	mutex_lock(&open_lock);
	md->usage--;
	if (md->usage == 0) {
		int devidx = mmc_get_devidx(md->disk);
		blk_cleanup_queue(md->queue.queue);

		__clear_bit(devidx, dev_use);

		put_disk(md->disk);
		kfree(md);
	}
	mutex_unlock(&open_lock);
}

static ssize_t power_ro_lock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	struct mmc_card *card;
	int locked = 0;

	if (!md)
		return -EINVAL;

	card = md->queue.card;
	if (card->ext_csd.boot_ro_lock & EXT_CSD_BOOT_WP_B_PERM_WP_EN)
		locked = 2;
	else if (card->ext_csd.boot_ro_lock & EXT_CSD_BOOT_WP_B_PWR_WP_EN)
		locked = 1;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", locked);

	mmc_blk_put(md);

	return ret;
}

static ssize_t power_ro_lock_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct mmc_blk_data *md, *part_md;
	struct mmc_card *card;
	unsigned long set;

	if (kstrtoul(buf, 0, &set))
		return -EINVAL;

	if (set != 1)
		return count;

	md = mmc_blk_get(dev_to_disk(dev));
	if (!md)
		return -EINVAL;
	card = md->queue.card;

	mmc_get_card(card);

	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BOOT_WP,
				card->ext_csd.boot_ro_lock |
				EXT_CSD_BOOT_WP_B_PWR_WP_EN,
				card->ext_csd.part_time);
	if (ret)
		pr_err("%s: Locking boot partition ro until next power on failed: %d\n", md->disk->disk_name, ret);
	else
		card->ext_csd.boot_ro_lock |= EXT_CSD_BOOT_WP_B_PWR_WP_EN;

	mmc_put_card(card);

	if (!ret) {
		pr_info("%s: Locking boot partition ro until next power on\n",
			md->disk->disk_name);
		set_disk_ro(md->disk, 1);

		list_for_each_entry(part_md, &md->part, part)
			if (part_md->area_type == MMC_BLK_DATA_AREA_BOOT) {
				pr_info("%s: Locking boot partition ro until next power on\n", part_md->disk->disk_name);
				set_disk_ro(part_md->disk, 1);
			}
	}

	mmc_blk_put(md);
	return count;
}

static ssize_t force_ro_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int ret;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));

	if (!md)
		return -EINVAL;

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		       get_disk_ro(dev_to_disk(dev)) ^
		       md->read_only);
	mmc_blk_put(md);
	return ret;
}

static ssize_t force_ro_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	char *end;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	unsigned long set = simple_strtoul(buf, &end, 0);

	if (!md)
		return -EINVAL;

	if (end == buf) {
		ret = -EINVAL;
		goto out;
	}

	set_disk_ro(dev_to_disk(dev), set || md->read_only);
	ret = count;
out:
	mmc_blk_put(md);
	return ret;
}

#ifdef CONFIG_MMC_SIMULATE_MAX_SPEED

static int max_read_speed, max_write_speed, cache_size = 4;

module_param(max_read_speed, int, S_IRUSR | S_IRGRP);
MODULE_PARM_DESC(max_read_speed, "maximum KB/s read speed 0=off");
module_param(max_write_speed, int, S_IRUSR | S_IRGRP);
MODULE_PARM_DESC(max_write_speed, "maximum KB/s write speed 0=off");
module_param(cache_size, int, S_IRUSR | S_IRGRP);
MODULE_PARM_DESC(cache_size, "MB high speed memory or SLC cache");

/*
 * helper macros and expectations:
 *  size    - unsigned long number of bytes
 *  jiffies - unsigned long HZ timestamp difference
 *  speed   - unsigned KB/s transfer rate
 */
#define size_and_speed_to_jiffies(size, speed) \
		((size) * HZ / (speed) / 1024UL)
#define jiffies_and_speed_to_size(jiffies, speed) \
		(((speed) * (jiffies) * 1024UL) / HZ)
#define jiffies_and_size_to_speed(jiffies, size) \
		((size) * HZ / (jiffies) / 1024UL)

/* Limits to report warning */
/* jiffies_and_size_to_speed(10*HZ, queue_max_hw_sectors(q) * 512UL) ~ 25 */
#define MIN_SPEED(q) 250 /* 10 times faster than a floppy disk */
#define MAX_SPEED(q) jiffies_and_size_to_speed(1, queue_max_sectors(q) * 512UL)

#define speed_valid(speed) ((speed) > 0)

static const char off[] = "off\n";

static int max_speed_show(int speed, char *buf)
{
	if (speed)
		return scnprintf(buf, PAGE_SIZE, "%uKB/s\n", speed);
	else
		return scnprintf(buf, PAGE_SIZE, off);
}

static int max_speed_store(const char *buf, struct request_queue *q)
{
	unsigned int limit, set = 0;

	if (!strncasecmp(off, buf, sizeof(off) - 2))
		return set;
	if (kstrtouint(buf, 0, &set) || (set > INT_MAX))
		return -EINVAL;
	if (set == 0)
		return set;
	limit = MAX_SPEED(q);
	if (set > limit)
		pr_warn("max speed %u ineffective above %u\n", set, limit);
	limit = MIN_SPEED(q);
	if (set < limit)
		pr_warn("max speed %u painful below %u\n", set, limit);
	return set;
}

static ssize_t max_write_speed_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	int ret = max_speed_show(atomic_read(&md->queue.max_write_speed), buf);

	mmc_blk_put(md);
	return ret;
}

static ssize_t max_write_speed_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	int set = max_speed_store(buf, md->queue.queue);

	if (set < 0) {
		mmc_blk_put(md);
		return set;
	}

	atomic_set(&md->queue.max_write_speed, set);
	mmc_blk_put(md);
	return count;
}

static const DEVICE_ATTR(max_write_speed, S_IRUGO | S_IWUSR,
	max_write_speed_show, max_write_speed_store);

static ssize_t max_read_speed_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	int ret = max_speed_show(atomic_read(&md->queue.max_read_speed), buf);

	mmc_blk_put(md);
	return ret;
}

static ssize_t max_read_speed_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	int set = max_speed_store(buf, md->queue.queue);

	if (set < 0) {
		mmc_blk_put(md);
		return set;
	}

	atomic_set(&md->queue.max_read_speed, set);
	mmc_blk_put(md);
	return count;
}

static const DEVICE_ATTR(max_read_speed, S_IRUGO | S_IWUSR,
	max_read_speed_show, max_read_speed_store);

static ssize_t cache_size_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	struct mmc_queue *mq = &md->queue;
	int cache_size = atomic_read(&mq->cache_size);
	int ret;

	if (!cache_size)
		ret = scnprintf(buf, PAGE_SIZE, off);
	else {
		int speed = atomic_read(&mq->max_write_speed);

		if (!speed_valid(speed))
			ret = scnprintf(buf, PAGE_SIZE, "%uMB\n", cache_size);
		else { /* We accept race between cache_jiffies and cache_used */
			unsigned long size = jiffies_and_speed_to_size(
				jiffies - mq->cache_jiffies, speed);
			long used = atomic_long_read(&mq->cache_used);

			if (size >= used)
				size = 0;
			else
				size = (used - size) * 100 / cache_size
					/ 1024UL / 1024UL;

			ret = scnprintf(buf, PAGE_SIZE, "%uMB %lu%% used\n",
				cache_size, size);
		}
	}

	mmc_blk_put(md);
	return ret;
}

static ssize_t cache_size_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mmc_blk_data *md;
	unsigned int set = 0;

	if (strncasecmp(off, buf, sizeof(off) - 2)
	 && (kstrtouint(buf, 0, &set) || (set > INT_MAX)))
		return -EINVAL;

	md = mmc_blk_get(dev_to_disk(dev));
	atomic_set(&md->queue.cache_size, set);
	mmc_blk_put(md);
	return count;
}

static const DEVICE_ATTR(cache_size, S_IRUGO | S_IWUSR,
	cache_size_show, cache_size_store);

/* correct for write-back */
static long mmc_blk_cache_used(struct mmc_queue *mq, unsigned long waitfor)
{
	long used = 0;
	int speed = atomic_read(&mq->max_write_speed);

	if (speed_valid(speed)) {
		unsigned long size = jiffies_and_speed_to_size(
					waitfor - mq->cache_jiffies, speed);
		used = atomic_long_read(&mq->cache_used);

		if (size >= used)
			used = 0;
		else
			used -= size;
	}

	atomic_long_set(&mq->cache_used, used);
	mq->cache_jiffies = waitfor;

	return used;
}

static void mmc_blk_simulate_delay(
	struct mmc_queue *mq,
	struct request *req,
	unsigned long waitfor)
{
	int max_speed;

	if (!req)
		return;

	max_speed = (rq_data_dir(req) == READ)
		? atomic_read(&mq->max_read_speed)
		: atomic_read(&mq->max_write_speed);
	if (speed_valid(max_speed)) {
		unsigned long bytes = blk_rq_bytes(req);

		if (rq_data_dir(req) != READ) {
			int cache_size = atomic_read(&mq->cache_size);

			if (cache_size) {
				unsigned long size = cache_size * 1024L * 1024L;
				long used = mmc_blk_cache_used(mq, waitfor);

				used += bytes;
				atomic_long_set(&mq->cache_used, used);
				bytes = 0;
				if (used > size)
					bytes = used - size;
			}
		}
		waitfor += size_and_speed_to_jiffies(bytes, max_speed);
		if (time_is_after_jiffies(waitfor)) {
			long msecs = jiffies_to_msecs(waitfor - jiffies);

			if (likely(msecs > 0))
				msleep(msecs);
		}
	}
}

#else

#define mmc_blk_simulate_delay(mq, req, waitfor)

#endif
static ssize_t
num_wr_reqs_to_start_packing_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	int num_wr_reqs_to_start_packing;
	int ret;

	if (!md)
		return -EINVAL;
	num_wr_reqs_to_start_packing = md->queue.num_wr_reqs_to_start_packing;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", num_wr_reqs_to_start_packing);

	mmc_blk_put(md);
	return ret;
}

static ssize_t
num_wr_reqs_to_start_packing_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int value;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	struct mmc_card *card;
	int ret = count;

	if (!md)
		return -EINVAL;

	card = md->queue.card;
	if (!card) {
		ret = -EINVAL;
		goto exit;
	}

	sscanf(buf, "%d", &value);

	if (value >= 0) {
		md->queue.num_wr_reqs_to_start_packing =
		    min_t(int, value, (int)card->ext_csd.max_packed_writes);

		pr_debug("%s: trigger to pack: new value = %d",
			mmc_hostname(card->host),
			md->queue.num_wr_reqs_to_start_packing);
	} else {
		pr_err("%s: value %d is not valid. old value remains = %d",
			mmc_hostname(card->host), value,
			md->queue.num_wr_reqs_to_start_packing);
		ret = -EINVAL;
	}

exit:
	mmc_blk_put(md);
	return ret;
}

static ssize_t
no_pack_for_random_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	int ret;

	if (!md)
		return -EINVAL;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", md->queue.no_pack_for_random);

	mmc_blk_put(md);
	return ret;
}

static ssize_t
no_pack_for_random_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int value;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	struct mmc_card *card;
	int ret = count;

	if (!md)
		return -EINVAL;

	card = md->queue.card;
	if (!card) {
		ret = -EINVAL;
		goto exit;
	}

	sscanf(buf, "%d", &value);

	if (value < 0) {
		pr_err("%s: value %d is not valid. old value remains = %d",
			mmc_hostname(card->host), value,
			md->queue.no_pack_for_random);
		ret = -EINVAL;
		goto exit;
	}

	md->queue.no_pack_for_random = (value > 0) ?  true : false;

	pr_debug("%s: no_pack_for_random: new value = %d",
		mmc_hostname(card->host),
		md->queue.no_pack_for_random);

exit:
	mmc_blk_put(md);
	return ret;
}

static int mmc_blk_open(struct block_device *bdev, fmode_t mode)
{
	struct mmc_blk_data *md = mmc_blk_get(bdev->bd_disk);
	int ret = -ENXIO;

	mutex_lock(&block_mutex);
	if (md) {
		if (md->usage == 2)
			check_disk_change(bdev);
		ret = 0;

		if ((mode & FMODE_WRITE) && md->read_only) {
			mmc_blk_put(md);
			ret = -EROFS;
		}
	}
	mutex_unlock(&block_mutex);

	return ret;
}

static void mmc_blk_release(struct gendisk *disk, fmode_t mode)
{
	struct mmc_blk_data *md = disk->private_data;

	mutex_lock(&block_mutex);
	mmc_blk_put(md);
	mutex_unlock(&block_mutex);
}

static int
mmc_blk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->cylinders = get_capacity(bdev->bd_disk) / (4 * 16);
	geo->heads = 4;
	geo->sectors = 16;
	return 0;
}

struct mmc_blk_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
};

static struct mmc_blk_ioc_data *mmc_blk_ioctl_copy_from_user(
	struct mmc_ioc_cmd __user *user)
{
	struct mmc_blk_ioc_data *idata;
	int err;

	idata = kzalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(&idata->ic, user, sizeof(idata->ic))) {
		err = -EFAULT;
		goto idata_err;
	}

	idata->buf_bytes = (u64) idata->ic.blksz * idata->ic.blocks;
	if (idata->buf_bytes > MMC_IOC_MAX_BYTES) {
		err = -EOVERFLOW;
		goto idata_err;
	}

	if (!idata->buf_bytes)
		return idata;

	idata->buf = kzalloc(idata->buf_bytes, GFP_KERNEL);
	if (!idata->buf) {
		err = -ENOMEM;
		goto idata_err;
	}

	if (copy_from_user(idata->buf, (void __user *)(unsigned long)
					idata->ic.data_ptr, idata->buf_bytes)) {
		err = -EFAULT;
		goto copy_err;
	}

	return idata;

copy_err:
	kfree(idata->buf);
idata_err:
	kfree(idata);
out:
	return ERR_PTR(err);
}

static int mmc_blk_ioctl_copy_to_user(struct mmc_ioc_cmd __user *ic_ptr,
				      struct mmc_blk_ioc_data *idata)
{
	struct mmc_ioc_cmd *ic = &idata->ic;

	if (copy_to_user(&(ic_ptr->response), ic->response,
			 sizeof(ic->response)))
		return -EFAULT;

	if (!idata->ic.write_flag) {
		if (copy_to_user((void __user *)(unsigned long)ic->data_ptr,
				 idata->buf, idata->buf_bytes))
			return -EFAULT;
	}

	return 0;
}

static int ioctl_rpmb_card_status_poll(struct mmc_card *card, u32 *status,
				       u32 retries_max)
{
	int err;
	u32 retry_count = 0;

	if (!status || !retries_max)
		return -EINVAL;

	do {
		err = get_card_status(card, status, 5);
		if (err)
			break;

		if (!R1_STATUS(*status) &&
				(R1_CURRENT_STATE(*status) != R1_STATE_PRG))
			break; /* RPMB programming operation complete */

		/*
		 * Rechedule to give the MMC device a chance to continue
		 * processing the previous command without being polled too
		 * frequently.
		 */
		usleep_range(1000, 5000);
	} while (++retry_count < retries_max);

	if (retry_count == retries_max)
		err = -EPERM;

	return err;
}

static int ioctl_do_sanitize(struct mmc_card *card)
{
	int err;

	if (!mmc_can_sanitize(card) &&
			(card->host->caps2 & MMC_CAP2_SANITIZE)) {
		pr_warn("%s: %s - SANITIZE is not supported\n",
				mmc_hostname(card->host), __func__);
		err = -EOPNOTSUPP;
		goto out;
	}

	pr_debug("%s: %s - SANITIZE IN PROGRESS...\n",
		mmc_hostname(card->host), __func__);

	trace_mmc_blk_erase_start(EXT_CSD_SANITIZE_START, 0, 0);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_SANITIZE_START, 1,
					MMC_SANITIZE_REQ_TIMEOUT);
	trace_mmc_blk_erase_end(EXT_CSD_SANITIZE_START, 0, 0);

	if (err)
		pr_err("%s: %s - EXT_CSD_SANITIZE_START failed. err=%d\n",
		       mmc_hostname(card->host), __func__, err);

	pr_debug("%s: %s - SANITIZE COMPLETED\n", mmc_hostname(card->host),
					     __func__);
out:
	return err;
}

static int __mmc_blk_ioctl_cmd(struct mmc_card *card, struct mmc_blk_data *md,
			       struct mmc_blk_ioc_data *idata)
{
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct mmc_request mrq = {NULL};
	struct scatterlist sg;
	int err;

	if (!card || !md || !idata)
		return -EINVAL;

	cmd.opcode = idata->ic.opcode;
	cmd.arg = idata->ic.arg;
	cmd.flags = idata->ic.flags;

	if (idata->ic.postsleep_max_us < idata->ic.postsleep_min_us) {
		pr_err("%s: min value: %u must not be greater than max value: %u\n",
			__func__, idata->ic.postsleep_min_us,
			idata->ic.postsleep_max_us);
		WARN_ON(1);
		return -EPERM;
	}

	if (idata->buf_bytes) {
		data.sg = &sg;
		data.sg_len = 1;
		data.blksz = idata->ic.blksz;
		data.blocks = idata->ic.blocks;

		sg_init_one(data.sg, idata->buf, idata->buf_bytes);

		if (idata->ic.write_flag)
			data.flags = MMC_DATA_WRITE;
		else
			data.flags = MMC_DATA_READ;

		/* data.flags must already be set before doing this. */
		mmc_set_data_timeout(&data, card);

		/* Allow overriding the timeout_ns for empirical tuning. */
		if (idata->ic.data_timeout_ns)
			data.timeout_ns = idata->ic.data_timeout_ns;

		if ((cmd.flags & MMC_RSP_R1B) == MMC_RSP_R1B) {
			/*
			 * Pretend this is a data transfer and rely on the
			 * host driver to compute timeout.  When all host
			 * drivers support cmd.cmd_timeout for R1B, this
			 * can be changed to:
			 *
			 *     mrq.data = NULL;
			 *     cmd.cmd_timeout = idata->ic.cmd_timeout_ms;
			 */
			data.timeout_ns = idata->ic.cmd_timeout_ms * 1000000;
		}

		mrq.data = &data;
	}

	mrq.cmd = &cmd;

	if (mmc_card_doing_bkops(card)) {
		err = mmc_stop_bkops(card);
		if (err) {
			dev_err(mmc_dev(card->host),
				"%s: stop_bkops failed %d\n", __func__, err);
			return err;
		}
	}

	err = mmc_blk_part_switch(card, md);
	if (err)
		return err;

	if (idata->ic.is_acmd) {
		err = mmc_app_cmd(card->host, card);
		if (err)
			return err;
	}

	if ((MMC_EXTRACT_INDEX_FROM_ARG(cmd.arg) == EXT_CSD_SANITIZE_START) &&
	    (cmd.opcode == MMC_SWITCH)) {
		err = ioctl_do_sanitize(card);

		if (err)
			pr_err("%s: ioctl_do_sanitize() failed. err = %d",
			       __func__, err);

		return err;
	}

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error) {
		dev_err(mmc_dev(card->host), "%s: cmd error %d\n",
						__func__, cmd.error);
		return cmd.error;
	}
	if (data.error) {
		dev_err(mmc_dev(card->host), "%s: data error %d\n",
						__func__, data.error);
		return data.error;
	}

	/*
	 * According to the SD specs, some commands require a delay after
	 * issuing the command.
	 */
	if (idata->ic.postsleep_min_us)
		usleep_range(idata->ic.postsleep_min_us, idata->ic.postsleep_max_us);

	memcpy(&(idata->ic.response), cmd.resp, sizeof(cmd.resp));

	return err;
}

struct mmc_blk_ioc_rpmb_data {
	struct mmc_blk_ioc_data *data[MMC_IOC_MAX_RPMB_CMD];
};

static struct mmc_blk_ioc_rpmb_data *mmc_blk_ioctl_rpmb_copy_from_user(
	struct mmc_ioc_rpmb __user *user)
{
	struct mmc_blk_ioc_rpmb_data *idata;
	int err, i;

	idata = kzalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	for (i = 0; i < MMC_IOC_MAX_RPMB_CMD; i++) {
		idata->data[i] = mmc_blk_ioctl_copy_from_user(&(user->cmds[i]));
		if (IS_ERR(idata->data[i])) {
			err = PTR_ERR(idata->data[i]);
			goto copy_err;
		}
	}

	return idata;

copy_err:
	while (--i >= 0) {
		kfree(idata->data[i]->buf);
		kfree(idata->data[i]);
	}
	kfree(idata);
out:
	return ERR_PTR(err);
}

static int mmc_blk_ioctl_rpmb_cmd(struct block_device *bdev,
	struct mmc_ioc_rpmb __user *ic_ptr)
{
	struct mmc_blk_ioc_rpmb_data *idata;
	struct mmc_blk_data *md;
	struct mmc_card *card;
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct mmc_request mrq = {NULL};
	struct scatterlist sg;
	int err = 0, i = 0;
	u32 status = 0;

	/* The caller must have CAP_SYS_RAWIO */
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	md = mmc_blk_get(bdev->bd_disk);
	/* make sure this is a rpmb partition */
	if ((!md) || (!(md->area_type & MMC_BLK_DATA_AREA_RPMB))) {
		err = -EINVAL;
		return err;
	}

	idata = mmc_blk_ioctl_rpmb_copy_from_user(ic_ptr);
	if (IS_ERR(idata)) {
		err = PTR_ERR(idata);
		goto cmd_done;
	}

	card = md->queue.card;
	if (IS_ERR(card)) {
		err = PTR_ERR(card);
		goto idata_free;
	}

	mmc_get_card(card);

	if (mmc_card_doing_bkops(card)) {
		if (mmc_card_cmdq(card)) {
			err = mmc_cmdq_halt(card->host, true);
			if (err)
				goto cmd_rel_host;
		}
		err = mmc_stop_bkops(card);
		if (err) {
			dev_err(mmc_dev(card->host),
				"%s: stop_bkops failed %d\n", __func__, err);
			goto cmd_rel_host;
		}
		if (mmc_card_cmdq(card)) {
			err = mmc_cmdq_halt(card->host, false);
			if (err)
				goto cmd_rel_host;
		}
	}

	err = mmc_blk_part_switch(card, md);
	if (err)
		goto cmd_rel_host;

	for (i = 0; i < MMC_IOC_MAX_RPMB_CMD; i++) {
		struct mmc_blk_ioc_data *curr_data;
		struct mmc_ioc_cmd *curr_cmd;

		curr_data = idata->data[i];
		curr_cmd = &curr_data->ic;
		if (!curr_cmd->opcode)
			break;

		cmd.opcode = curr_cmd->opcode;
		cmd.arg = curr_cmd->arg;
		cmd.flags = curr_cmd->flags;

		if (curr_data->buf_bytes) {
			data.sg = &sg;
			data.sg_len = 1;
			data.blksz = curr_cmd->blksz;
			data.blocks = curr_cmd->blocks;

			sg_init_one(data.sg, curr_data->buf,
					curr_data->buf_bytes);

			if (curr_cmd->write_flag)
				data.flags = MMC_DATA_WRITE;
			else
				data.flags = MMC_DATA_READ;

			/* data.flags must already be set before doing this. */
			mmc_set_data_timeout(&data, card);

			/*
			 * Allow overriding the timeout_ns for empirical tuning.
			 */
			if (curr_cmd->data_timeout_ns)
				data.timeout_ns = curr_cmd->data_timeout_ns;

			mrq.data = &data;
		}

		mrq.cmd = &cmd;

		err = mmc_set_blockcount(card, data.blocks,
				curr_cmd->write_flag & (1 << 31));
		if (err)
			goto cmd_rel_host;

		mmc_wait_for_req(card->host, &mrq);

		if (cmd.error) {
			dev_err(mmc_dev(card->host), "%s: cmd error %d\n",
					__func__, cmd.error);
			err = cmd.error;
			goto cmd_rel_host;
		}
		if (data.error) {
			dev_err(mmc_dev(card->host), "%s: data error %d\n",
					__func__, data.error);
			err = data.error;
			goto cmd_rel_host;
		}

		if (copy_to_user(&(ic_ptr->cmds[i].response), cmd.resp,
					sizeof(cmd.resp))) {
			err = -EFAULT;
			goto cmd_rel_host;
		}

		if (!curr_cmd->write_flag) {
			if (copy_to_user((void __user *)(unsigned long)
						curr_cmd->data_ptr,
						curr_data->buf,
						curr_data->buf_bytes)) {
				err = -EFAULT;
				goto cmd_rel_host;
			}
		}

		/*
		 * Ensure RPMB command has completed by polling CMD13
		 * "Send Status".
		 */
		err = ioctl_rpmb_card_status_poll(card, &status, 5);
		if (err)
			dev_err(mmc_dev(card->host),
					"%s: Card Status=0x%08X, error %d\n",
					__func__, status, err);
	}

cmd_rel_host:
	mmc_put_card(card);

idata_free:
	for (i = 0; i < MMC_IOC_MAX_RPMB_CMD; i++) {
		kfree(idata->data[i]->buf);
		kfree(idata->data[i]);
	}
	kfree(idata);

cmd_done:
	mmc_blk_put(md);
	if (card->cmdq_init)
		wake_up(&card->host->cmdq_ctx.wait);
	return err;
}

static int mmc_blk_ioctl_cmd(struct block_device *bdev,
			     struct mmc_ioc_cmd __user *ic_ptr)
{
	struct mmc_blk_ioc_data *idata;
	struct mmc_blk_data *md;
	struct mmc_card *card;
	int err = 0, ioc_err = 0;

	/*
	 * The caller must have CAP_SYS_RAWIO, and must be calling this on the
	 * whole block device, not on a partition.  This prevents overspray
	 * between sibling partitions.
	 */
	if ((!capable(CAP_SYS_RAWIO)) || (bdev != bdev->bd_contains))
		return -EPERM;

	idata = mmc_blk_ioctl_copy_from_user(ic_ptr);
	if (IS_ERR_OR_NULL(idata))
		return PTR_ERR(idata);
	md = mmc_blk_get(bdev->bd_disk);
	if (!md) {
		err = -EINVAL;
		goto cmd_err;
	}

	card = md->queue.card;
	if (IS_ERR_OR_NULL(card)) {
		err = PTR_ERR(card);
		goto cmd_done;
	}

	mmc_get_card(card);

	if (mmc_card_cmdq(card)) {
		err = mmc_cmdq_halt_on_empty_queue(card->host);
		if (err) {
			pr_err("%s: halt failed while doing %s err (%d)\n",
					mmc_hostname(card->host),
					__func__, err);
			mmc_put_card(card);
			goto cmd_done;
		}
	}

	ioc_err = __mmc_blk_ioctl_cmd(card, md, idata);

	mmc_put_card(card);

	err = mmc_blk_ioctl_copy_to_user(ic_ptr, idata);

	if (mmc_card_cmdq(card)) {
		if (mmc_cmdq_halt(card->host, false))
			pr_err("%s: %s: cmdq unhalt failed\n",
			       mmc_hostname(card->host), __func__);
	}

cmd_done:
	mmc_blk_put(md);
cmd_err:
	kfree(idata->buf);
	kfree(idata);
	return ioc_err ? ioc_err : err;
}

static int mmc_blk_ioctl_multi_cmd(struct block_device *bdev,
				   struct mmc_ioc_multi_cmd __user *user)
{
	struct mmc_blk_ioc_data **idata = NULL;
	struct mmc_ioc_cmd __user *cmds = user->cmds;
	struct mmc_card *card;
	struct mmc_blk_data *md;
	int i, err = 0, ioc_err = 0;
	__u64 num_of_cmds;

	/*
	 * The caller must have CAP_SYS_RAWIO, and must be calling this on the
	 * whole block device, not on a partition.  This prevents overspray
	 * between sibling partitions.
	 */
	if ((!capable(CAP_SYS_RAWIO)) || (bdev != bdev->bd_contains))
		return -EPERM;

	if (copy_from_user(&num_of_cmds, &user->num_of_cmds,
			   sizeof(num_of_cmds)))
		return -EFAULT;

	if (num_of_cmds > MMC_IOC_MAX_CMDS)
		return -EINVAL;

	idata = kcalloc(num_of_cmds, sizeof(*idata), GFP_KERNEL);
	if (!idata)
		return -ENOMEM;

	for (i = 0; i < num_of_cmds; i++) {
		idata[i] = mmc_blk_ioctl_copy_from_user(&cmds[i]);
		if (IS_ERR(idata[i])) {
			err = PTR_ERR(idata[i]);
			num_of_cmds = i;
			goto cmd_err;
		}
	}

	md = mmc_blk_get(bdev->bd_disk);
	if (!md)
		goto cmd_err;

	card = md->queue.card;
	if (IS_ERR(card)) {
		err = PTR_ERR(card);
		goto cmd_done;
	}

	mmc_get_card(card);

	for (i = 0; i < num_of_cmds && !ioc_err; i++)
		ioc_err = __mmc_blk_ioctl_cmd(card, md, idata[i]);

	mmc_put_card(card);

	/* copy to user if data and response */
	for (i = 0; i < num_of_cmds && !err; i++)
		err = mmc_blk_ioctl_copy_to_user(&cmds[i], idata[i]);

cmd_done:
	mmc_blk_put(md);
cmd_err:
	for (i = 0; i < num_of_cmds; i++) {
		kfree(idata[i]->buf);
		kfree(idata[i]);
	}
	kfree(idata);
	return ioc_err ? ioc_err : err;
}

static int mmc_blk_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MMC_IOC_CMD:
		return mmc_blk_ioctl_cmd(bdev,
				(struct mmc_ioc_cmd __user *)arg);
	case MMC_IOC_RPMB_CMD:
		return mmc_blk_ioctl_rpmb_cmd(bdev,
				(struct mmc_ioc_rpmb __user *)arg);
	case MMC_IOC_MULTI_CMD:
		return mmc_blk_ioctl_multi_cmd(bdev,
				(struct mmc_ioc_multi_cmd __user *)arg);
	default:
		return -EINVAL;
	}
}

#ifdef CONFIG_COMPAT
static int mmc_blk_compat_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	return mmc_blk_ioctl(bdev, mode, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct block_device_operations mmc_bdops = {
	.open			= mmc_blk_open,
	.release		= mmc_blk_release,
	.getgeo			= mmc_blk_getgeo,
	.owner			= THIS_MODULE,
	.ioctl			= mmc_blk_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= mmc_blk_compat_ioctl,
#endif
};

static int mmc_blk_cmdq_switch(struct mmc_card *card,
			       struct mmc_blk_data *md, bool enable)
{
	int ret = 0;
	bool cmdq_mode = !!mmc_card_cmdq(card);
	struct mmc_host *host = card->host;
	struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;

	if (!(card->host->caps2 & MMC_CAP2_CMD_QUEUE) ||
	    !card->ext_csd.cmdq_support ||
	    (enable && !(md->flags & MMC_BLK_CMD_QUEUE)) ||
	    (cmdq_mode == enable))
		return 0;

	if (enable) {
		ret = mmc_set_blocklen(card, MMC_CARD_CMDQ_BLK_SIZE);
		if (ret) {
			pr_err("%s: failed (%d) to set block-size to %d\n",
			       __func__, ret, MMC_CARD_CMDQ_BLK_SIZE);
			goto out;
		}

	} else {
		if (!test_bit(CMDQ_STATE_HALT, &ctx->curr_state)) {
			ret = mmc_cmdq_halt(host, true);
			if (ret) {
				pr_err("%s: halt: failed: %d\n",
					mmc_hostname(host), ret);
				goto out;
			}
		}
	}

	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_CMDQ, enable,
			 card->ext_csd.generic_cmd6_time);
	if (ret) {
		pr_err("%s: cmdq mode %sable failed %d\n",
		       md->disk->disk_name, enable ? "en" : "dis", ret);
		goto out;
	}

	if (enable)
		mmc_card_set_cmdq(card);
	else
		mmc_card_clr_cmdq(card);
out:
	return ret;
}

static inline int mmc_blk_part_switch(struct mmc_card *card,
				      struct mmc_blk_data *md)
{
	int ret;
	struct mmc_blk_data *main_md = dev_get_drvdata(&card->dev);

	if ((main_md->part_curr == md->part_type) &&
	    (card->part_curr == md->part_type))
		return 0;

	if (mmc_card_mmc(card)) {
		u8 part_config = card->ext_csd.part_config;

		if (md->part_type) {
			/* disable CQ mode for non-user data partitions */
			ret = mmc_blk_cmdq_switch(card, md, false);
			if (ret)
				return ret;
		}

		part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		part_config |= md->part_type;

		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_PART_CONFIG, part_config,
				 card->ext_csd.part_time);

		if (ret) {
			pr_err("%s: mmc_blk_part_switch failure, %d -> %d\n",
				mmc_hostname(card->host), main_md->part_curr,
					md->part_type);
			return ret;
		}

		card->ext_csd.part_config = part_config;
		card->part_curr = md->part_type;
	}

	main_md->part_curr = md->part_type;
	return 0;
}

static u32 mmc_sd_num_wr_blocks(struct mmc_card *card)
{
	int err;
	u32 result;
	__be32 *blocks;

	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};

	struct scatterlist sg;

	cmd.opcode = MMC_APP_CMD;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err)
		return (u32)-1;
	if (!mmc_host_is_spi(card->host) && !(cmd.resp[0] & R1_APP_CMD))
		return (u32)-1;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = SD_APP_SEND_NUM_WR_BLKS;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = 4;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;
	mmc_set_data_timeout(&data, card);

	mrq.cmd = &cmd;
	mrq.data = &data;

	blocks = kmalloc(4, GFP_KERNEL);
	if (!blocks)
		return (u32)-1;

	sg_init_one(&sg, blocks, 4);

	mmc_wait_for_req(card->host, &mrq);

	result = ntohl(*blocks);
	kfree(blocks);

	if (cmd.error || data.error)
		result = (u32)-1;

	return result;
}

static int get_card_status(struct mmc_card *card, u32 *status, int retries)
{
	struct mmc_command cmd = {0};
	int err;

	cmd.opcode = MMC_SEND_STATUS;
	if (!mmc_host_is_spi(card->host))
		cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
	err = mmc_wait_for_cmd(card->host, &cmd, retries);
	if (err == 0)
		*status = cmd.resp[0];
	return err;
}

static int card_busy_detect(struct mmc_card *card, unsigned int timeout_ms,
		bool hw_busy_detect, struct request *req, int *gen_err)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
	int err = 0;
	u32 status;

	do {
		err = get_card_status(card, &status, 5);
		if (err) {
			pr_err("%s: error %d requesting status\n",
			       req->rq_disk->disk_name, err);
			return err;
		}

		if (status & R1_ERROR) {
			pr_err("%s: %s: error sending status cmd, status %#x\n",
				req->rq_disk->disk_name, __func__, status);
			*gen_err = 1;
		}

		/* We may rely on the host hw to handle busy detection.*/
		if ((card->host->caps & MMC_CAP_WAIT_WHILE_BUSY) &&
			hw_busy_detect)
			break;

		/*
		 * Timeout if the device never becomes ready for data and never
		 * leaves the program state.
		 */
		if (time_after(jiffies, timeout)) {
			pr_err("%s: Card stuck in programming state! %s %s\n",
				mmc_hostname(card->host),
				req->rq_disk->disk_name, __func__);
			return -ETIMEDOUT;
		}

		/*
		 * Some cards mishandle the status bits,
		 * so make sure to check both the busy
		 * indication and the card state.
		 */
	} while (!(status & R1_READY_FOR_DATA) ||
		 (R1_CURRENT_STATE(status) == R1_STATE_PRG));

	return err;
}

static int send_stop(struct mmc_card *card, unsigned int timeout_ms,
		struct request *req, int *gen_err, u32 *stop_status)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	int err;
	bool use_r1b_resp = rq_data_dir(req) == WRITE;

	/*
	 * Normally we use R1B responses for WRITE, but in cases where the host
	 * has specified a max_busy_timeout we need to validate it. A failure
	 * means we need to prevent the host from doing hw busy detection, which
	 * is done by converting to a R1 response instead.
	 */
	if (host->max_busy_timeout && (timeout_ms > host->max_busy_timeout))
		use_r1b_resp = false;

	cmd.opcode = MMC_STOP_TRANSMISSION;
	if (use_r1b_resp) {
		cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
		cmd.busy_timeout = timeout_ms;
	} else {
		cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
	}

	err = mmc_wait_for_cmd(host, &cmd, 5);
	if (err)
		return err;

	*stop_status = cmd.resp[0];

	/* No need to check card status in case of READ. */
	if (rq_data_dir(req) == READ)
		return 0;

	if (!mmc_host_is_spi(host) &&
		(*stop_status & R1_ERROR)) {
		pr_err("%s: %s: general error sending stop command, resp %#x\n",
			req->rq_disk->disk_name, __func__, *stop_status);
		*gen_err = 1;
	}

	return card_busy_detect(card, timeout_ms, use_r1b_resp, req, gen_err);
}

#define ERR_NOMEDIUM	3
#define ERR_RETRY	2
#define ERR_ABORT	1
#define ERR_CONTINUE	0

static int mmc_blk_cmd_error(struct request *req, const char *name, int error,
	bool status_valid, u32 status)
{
	switch (error) {
	case -EILSEQ:
		/* response crc error, retry the r/w cmd */
		pr_err_ratelimited(
			"%s: response CRC error sending %s command, card status %#x\n",
			req->rq_disk->disk_name,
			name, status);
		return ERR_RETRY;

	case -ETIMEDOUT:
		pr_err_ratelimited(
			"%s: timed out sending %s command, card status %#x\n",
			req->rq_disk->disk_name, name, status);

		/* If the status cmd initially failed, retry the r/w cmd */
		if (!status_valid) {
			pr_err_ratelimited("%s: status not valid, retrying timeout\n",
				req->rq_disk->disk_name);
			return ERR_RETRY;
		}
		/*
		 * If it was a r/w cmd crc error, or illegal command
		 * (eg, issued in wrong state) then retry - we should
		 * have corrected the state problem above.
		 */
		if (status & (R1_COM_CRC_ERROR | R1_ILLEGAL_COMMAND)) {
			pr_err_ratelimited(
				"%s: command error, retrying timeout\n",
				req->rq_disk->disk_name);
			return ERR_RETRY;
		}

		/* Otherwise abort the command */
		pr_err_ratelimited(
			"%s: not retrying timeout\n",
			req->rq_disk->disk_name);
		return ERR_ABORT;

	default:
		/* We don't understand the error code the driver gave us */
		pr_err_ratelimited(
			"%s: unknown error %d sending read/write command, card status %#x\n",
		       req->rq_disk->disk_name, error, status);
		return ERR_ABORT;
	}
}

/*
 * Initial r/w and stop cmd error recovery.
 * We don't know whether the card received the r/w cmd or not, so try to
 * restore things back to a sane state.  Essentially, we do this as follows:
 * - Obtain card status.  If the first attempt to obtain card status fails,
 *   the status word will reflect the failed status cmd, not the failed
 *   r/w cmd.  If we fail to obtain card status, it suggests we can no
 *   longer communicate with the card.
 * - Check the card state.  If the card received the cmd but there was a
 *   transient problem with the response, it might still be in a data transfer
 *   mode.  Try to send it a stop command.  If this fails, we can't recover.
 * - If the r/w cmd failed due to a response CRC error, it was probably
 *   transient, so retry the cmd.
 * - If the r/w cmd timed out, but we didn't get the r/w cmd status, retry.
 * - If the r/w cmd timed out, and the r/w cmd failed due to CRC error or
 *   illegal cmd, retry.
 * Otherwise we don't understand what happened, so abort.
 */
static int mmc_blk_cmd_recovery(struct mmc_card *card, struct request *req,
	struct mmc_blk_request *brq, int *ecc_err, int *gen_err)
{
	bool prev_cmd_status_valid = true;
	u32 status, stop_status = 0;
	int err, retry;

	if (mmc_card_removed(card))
		return ERR_NOMEDIUM;

	/*
	 * Try to get card status which indicates both the card state
	 * and why there was no response.  If the first attempt fails,
	 * we can't be sure the returned status is for the r/w command.
	 */
	for (retry = 2; retry >= 0; retry--) {
		err = get_card_status(card, &status, 0);
		if (!err)
			break;

		/* Re-tune if needed */
		mmc_retune_recheck(card->host);

		prev_cmd_status_valid = false;
		pr_err("%s: error %d sending status command, %sing\n",
		       req->rq_disk->disk_name, err, retry ? "retry" : "abort");
	}

	/* We couldn't get a response from the card.  Give up. */
	if (err) {
		if (card->err_in_sdr104)
			return ERR_RETRY;
		/* Check if the card is removed */
		if (mmc_detect_card_removed(card->host))
			return ERR_NOMEDIUM;
		return ERR_ABORT;
	}

	/* Flag ECC errors */
	if ((status & R1_CARD_ECC_FAILED) ||
	    (brq->stop.resp[0] & R1_CARD_ECC_FAILED) ||
	    (brq->cmd.resp[0] & R1_CARD_ECC_FAILED))
		*ecc_err = 1;

	/* Flag General errors */
	if (!mmc_host_is_spi(card->host) && rq_data_dir(req) != READ)
		if ((status & R1_ERROR) ||
			(brq->stop.resp[0] & R1_ERROR)) {
			pr_err("%s: %s: general error sending stop or status command, stop cmd response %#x, card status %#x\n",
			       req->rq_disk->disk_name, __func__,
			       brq->stop.resp[0], status);
			*gen_err = 1;
		}

	/*
	 * Check the current card state.  If it is in some data transfer
	 * mode, tell it to stop (and hopefully transition back to TRAN.)
	 */
	if (R1_CURRENT_STATE(status) == R1_STATE_DATA ||
	    R1_CURRENT_STATE(status) == R1_STATE_RCV) {
		err = send_stop(card,
			DIV_ROUND_UP(brq->data.timeout_ns, 1000000),
			req, gen_err, &stop_status);
		if (err) {
			pr_err("%s: error %d sending stop command\n",
			       req->rq_disk->disk_name, err);
			/*
			 * If the stop cmd also timed out, the card is probably
			 * not present, so abort. Other errors are bad news too.
			 */
			return ERR_ABORT;
		}

		if (stop_status & R1_CARD_ECC_FAILED)
			*ecc_err = 1;
	}

	/* Check for set block count errors */
	if (brq->sbc.error)
		return mmc_blk_cmd_error(req, "SET_BLOCK_COUNT", brq->sbc.error,
				prev_cmd_status_valid, status);

	/* Check for r/w command errors */
	if (brq->cmd.error)
		return mmc_blk_cmd_error(req, "r/w cmd", brq->cmd.error,
				prev_cmd_status_valid, status);

	/* Data errors */
	if (!brq->stop.error)
		return ERR_CONTINUE;

	/* Now for stop errors.  These aren't fatal to the transfer. */
	pr_info("%s: error %d sending stop command, original cmd response %#x, card status %#x\n",
	       req->rq_disk->disk_name, brq->stop.error,
	       brq->cmd.resp[0], status);

	/*
	 * Subsitute in our own stop status as this will give the error
	 * state which happened during the execution of the r/w command.
	 */
	if (stop_status) {
		brq->stop.resp[0] = stop_status;
		brq->stop.error = 0;
	}
	return ERR_CONTINUE;
}

static int mmc_blk_reset(struct mmc_blk_data *md, struct mmc_host *host,
			 int type)
{
	int err;

	if (md->reset_done & type)
		return -EEXIST;

	md->reset_done |= type;
	err = mmc_hw_reset(host);
	if (err && err != -EOPNOTSUPP) {
		/* We failed to reset so we need to abort the request */
		pr_err("%s: %s: failed to reset %d\n", mmc_hostname(host),
				__func__, err);
		return -ENODEV;
	}

	/* Ensure we switch back to the correct partition */
	if (host->card) {
		struct mmc_blk_data *main_md =
			dev_get_drvdata(&host->card->dev);
		int part_err;

		main_md->part_curr = main_md->part_type;
		part_err = mmc_blk_part_switch(host->card, md);
		if (part_err) {
			/*
			 * We have failed to get back into the correct
			 * partition, so we need to abort the whole request.
			 */
			return -ENODEV;
		}
	}
	return err;
}

static inline void mmc_blk_reset_success(struct mmc_blk_data *md, int type)
{
	md->reset_done &= ~type;
}

int mmc_access_rpmb(struct mmc_queue *mq)
{
	struct mmc_blk_data *md = mq->data;
	/*
	 * If this is a RPMB partition access, return ture
	 */
	if (md && md->part_type == EXT_CSD_PART_CONFIG_ACC_RPMB)
		return true;

	return false;
}

static struct mmc_cmdq_req *mmc_blk_cmdq_prep_discard_req(struct mmc_queue *mq,
						struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_host *host = card->host;
	struct mmc_cmdq_context_info *ctx_info = &host->cmdq_ctx;
	struct mmc_cmdq_req *cmdq_req;
	struct mmc_queue_req *active_mqrq;

	BUG_ON(req->tag > card->ext_csd.cmdq_depth);
	BUG_ON(test_and_set_bit(req->tag, &host->cmdq_ctx.active_reqs));

	set_bit(CMDQ_STATE_DCMD_ACTIVE, &ctx_info->curr_state);

	active_mqrq = &mq->mqrq_cmdq[req->tag];
	active_mqrq->req = req;

	cmdq_req = mmc_cmdq_prep_dcmd(active_mqrq, mq);
	cmdq_req->cmdq_req_flags |= QBR;
	cmdq_req->mrq.cmd = &cmdq_req->cmd;
	cmdq_req->tag = req->tag;
	return cmdq_req;
}

static int mmc_blk_cmdq_issue_discard_rq(struct mmc_queue *mq,
					struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_cmdq_req *cmdq_req = NULL;
	unsigned int from, nr, arg;
	int err = 0;

	if (!mmc_can_erase(card)) {
		err = -EOPNOTSUPP;
		blk_end_request(req, err, blk_rq_bytes(req));
		goto out;
	}

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);

	if (mmc_can_discard(card))
		arg = MMC_DISCARD_ARG;
	else if (mmc_can_trim(card))
		arg = MMC_TRIM_ARG;
	else
		arg = MMC_ERASE_ARG;

	cmdq_req = mmc_blk_cmdq_prep_discard_req(mq, req);
	if (card->quirks & MMC_QUIRK_INAND_CMD38) {
		__mmc_switch_cmdq_mode(cmdq_req->mrq.cmd,
				EXT_CSD_CMD_SET_NORMAL,
				INAND_CMD38_ARG_EXT_CSD,
				arg == MMC_TRIM_ARG ?
				INAND_CMD38_ARG_TRIM :
				INAND_CMD38_ARG_ERASE,
				0, true, false);
		err = mmc_cmdq_wait_for_dcmd(card->host, cmdq_req);
		if (err)
			goto clear_dcmd;
	}
	err = mmc_cmdq_erase(cmdq_req, card, from, nr, arg);
clear_dcmd:
	mmc_host_clk_hold(card->host);
	blk_complete_request(req);
out:
	return err ? 1 : 0;
}

static int mmc_blk_issue_discard_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	unsigned int from, nr, arg;
	int err = 0, type = MMC_BLK_DISCARD;

	if (!mmc_can_erase(card)) {
		err = -EOPNOTSUPP;
		goto out;
	}

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);

	if (mmc_can_discard(card))
		arg = MMC_DISCARD_ARG;
	else if (mmc_can_trim(card))
		arg = MMC_TRIM_ARG;
	else
		arg = MMC_ERASE_ARG;
retry:
	if (card->quirks & MMC_QUIRK_INAND_CMD38) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 INAND_CMD38_ARG_EXT_CSD,
				 arg == MMC_TRIM_ARG ?
				 INAND_CMD38_ARG_TRIM :
				 INAND_CMD38_ARG_ERASE,
				 0);
		if (err)
			goto out;
	}
	err = mmc_erase(card, from, nr, arg);
out:
	if (err == -EIO && !mmc_blk_reset(md, card->host, type))
		goto retry;
	if (!err)
		mmc_blk_reset_success(md, type);
	blk_end_request(req, err, blk_rq_bytes(req));

	return err ? 0 : 1;
}

static int mmc_blk_cmdq_issue_secdiscard_rq(struct mmc_queue *mq,
				       struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_cmdq_req *cmdq_req = NULL;
	unsigned int from, nr, arg;
	int err = 0;

	if (!(mmc_can_secure_erase_trim(card))) {
		err = -EOPNOTSUPP;
		blk_end_request(req, err, blk_rq_bytes(req));
		goto out;
	}

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);

	if (mmc_can_trim(card) && !mmc_erase_group_aligned(card, from, nr))
		arg = MMC_SECURE_TRIM1_ARG;
	else
		arg = MMC_SECURE_ERASE_ARG;

	cmdq_req = mmc_blk_cmdq_prep_discard_req(mq, req);
	if (card->quirks & MMC_QUIRK_INAND_CMD38) {
		__mmc_switch_cmdq_mode(cmdq_req->mrq.cmd,
				EXT_CSD_CMD_SET_NORMAL,
				INAND_CMD38_ARG_EXT_CSD,
				arg == MMC_SECURE_TRIM1_ARG ?
				INAND_CMD38_ARG_SECTRIM1 :
				INAND_CMD38_ARG_SECERASE,
				0, true, false);
		err = mmc_cmdq_wait_for_dcmd(card->host, cmdq_req);
		if (err)
			goto clear_dcmd;
	}

	err = mmc_cmdq_erase(cmdq_req, card, from, nr, arg);
	if (err)
		goto clear_dcmd;

	if (arg == MMC_SECURE_TRIM1_ARG) {
		if (card->quirks & MMC_QUIRK_INAND_CMD38) {
			__mmc_switch_cmdq_mode(cmdq_req->mrq.cmd,
					EXT_CSD_CMD_SET_NORMAL,
					INAND_CMD38_ARG_EXT_CSD,
					INAND_CMD38_ARG_SECTRIM2,
					0, true, false);
			err = mmc_cmdq_wait_for_dcmd(card->host, cmdq_req);
			if (err)
				goto clear_dcmd;
		}

		err = mmc_cmdq_erase(cmdq_req, card, from, nr,
				MMC_SECURE_TRIM2_ARG);
	}
clear_dcmd:
	mmc_host_clk_hold(card->host);
	blk_complete_request(req);
out:
	return err ? 1 : 0;
}

static int mmc_blk_issue_secdiscard_rq(struct mmc_queue *mq,
				       struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	unsigned int from, nr, arg;
	int err = 0, type = MMC_BLK_SECDISCARD;

	if (!(mmc_can_secure_erase_trim(card))) {
		err = -EOPNOTSUPP;
		goto out;
	}

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);

	if (mmc_can_trim(card) && !mmc_erase_group_aligned(card, from, nr))
		arg = MMC_SECURE_TRIM1_ARG;
	else
		arg = MMC_SECURE_ERASE_ARG;

retry:
	if (card->quirks & MMC_QUIRK_INAND_CMD38) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 INAND_CMD38_ARG_EXT_CSD,
				 arg == MMC_SECURE_TRIM1_ARG ?
				 INAND_CMD38_ARG_SECTRIM1 :
				 INAND_CMD38_ARG_SECERASE,
				 0);
		if (err)
			goto out_retry;
	}

	err = mmc_erase(card, from, nr, arg);
	if (err == -EIO)
		goto out_retry;
	if (err)
		goto out;

	if (arg == MMC_SECURE_TRIM1_ARG) {
		if (card->quirks & MMC_QUIRK_INAND_CMD38) {
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					 INAND_CMD38_ARG_EXT_CSD,
					 INAND_CMD38_ARG_SECTRIM2,
					 0);
			if (err)
				goto out_retry;
		}

		err = mmc_erase(card, from, nr, MMC_SECURE_TRIM2_ARG);
		if (err == -EIO)
			goto out_retry;
		if (err)
			goto out;
	}

out_retry:
	if (err && !mmc_blk_reset(md, card->host, type))
		goto retry;
	if (!err)
		mmc_blk_reset_success(md, type);
out:
	blk_end_request(req, err, blk_rq_bytes(req));

	return err ? 0 : 1;
}

static int mmc_blk_issue_flush(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	int ret = 0;

	if (!req)
		return 0;

	if (req->cmd_flags & REQ_BARRIER) {
		/*
		 * If eMMC cache flush policy is set to 1, then the device
		 * shall flush the requests in First-In-First-Out (FIFO) order.
		 * In this case, as per spec, the host must not send any cache
		 * barrier requests as they are redundant and add unnecessary
		 * overhead to both device and host.
		 */
		if (card->ext_csd.cache_flush_policy & 1)
			goto end_req;

		/*
		 * In case barrier is not supported or enabled in the device,
		 * use flush as a fallback option.
		 */
		ret = mmc_cache_barrier(card);
		if (ret)
			ret = mmc_flush_cache(card);
	 } else if (req->cmd_flags & REQ_FLUSH) {
		ret = mmc_flush_cache(card);
	 }
	if (ret == -ENODEV) {
		pr_err("%s: %s: restart mmc card",
				req->rq_disk->disk_name, __func__);
		if (mmc_blk_reset(md, card->host, MMC_BLK_FLUSH))
			pr_err("%s: %s: fail to restart mmc",
				req->rq_disk->disk_name, __func__);
		else
			mmc_blk_reset_success(md, MMC_BLK_FLUSH);
	}

	if (ret) {
		pr_err("%s: %s: notify flush error to upper layers",
				req->rq_disk->disk_name, __func__);
		ret = -EIO;
	}

end_req:
#ifdef CONFIG_MMC_SIMULATE_MAX_SPEED
	else if (atomic_read(&mq->cache_size)) {
		long used = mmc_blk_cache_used(mq, jiffies);

		if (used) {
			int speed = atomic_read(&mq->max_write_speed);

			if (speed_valid(speed)) {
				unsigned long msecs = jiffies_to_msecs(
					size_and_speed_to_jiffies(
						used, speed));
				if (msecs)
					msleep(msecs);
			}
		}
	}
#endif
	blk_end_request_all(req, ret);

	return ret ? 0 : 1;
}

/*
 * Reformat current write as a reliable write, supporting
 * both legacy and the enhanced reliable write MMC cards.
 * In each transfer we'll handle only as much as a single
 * reliable write can handle, thus finish the request in
 * partial completions.
 */
static inline void mmc_apply_rel_rw(struct mmc_blk_request *brq,
				    struct mmc_card *card,
				    struct request *req)
{
	if (!(card->ext_csd.rel_param & EXT_CSD_WR_REL_PARAM_EN)) {
		/* Legacy mode imposes restrictions on transfers. */
		if (!IS_ALIGNED(brq->cmd.arg, card->ext_csd.rel_sectors))
			brq->data.blocks = 1;

		if (brq->data.blocks > card->ext_csd.rel_sectors)
			brq->data.blocks = card->ext_csd.rel_sectors;
		else if (brq->data.blocks < card->ext_csd.rel_sectors)
			brq->data.blocks = 1;
	}
}

#define CMD_ERRORS							\
	(R1_OUT_OF_RANGE |	/* Command argument out of range */	\
	 R1_ADDRESS_ERROR |	/* Misaligned address */		\
	 R1_BLOCK_LEN_ERROR |	/* Transferred block length incorrect */\
	 R1_WP_VIOLATION |	/* Tried to write to protected block */	\
	 R1_CC_ERROR |		/* Card controller error */		\
	 R1_ERROR)		/* General/unknown error */

static int mmc_blk_err_check(struct mmc_card *card,
			     struct mmc_async_req *areq)
{
	struct mmc_queue_req *mq_mrq = container_of(areq, struct mmc_queue_req,
						    mmc_active);
	struct mmc_blk_request *brq = &mq_mrq->brq;
	struct request *req = mq_mrq->req;
	int need_retune = card->host->need_retune;
	int ecc_err = 0, gen_err = 0;

	if (card->host->sdr104_wa && mmc_card_sd(card) &&
	    (card->host->ios.timing == MMC_TIMING_UHS_SDR104) &&
	    !card->sdr104_blocked &&
	    (brq->data.error == -EILSEQ ||
	     brq->data.error == -EIO ||
	     brq->data.error == -ETIMEDOUT ||
	     brq->cmd.error == -EILSEQ ||
	     brq->cmd.error == -EIO ||
	     brq->cmd.error == -ETIMEDOUT ||
	     brq->sbc.error))
		card->err_in_sdr104 = true;

	/*
	 * sbc.error indicates a problem with the set block count
	 * command.  No data will have been transferred.
	 *
	 * cmd.error indicates a problem with the r/w command.  No
	 * data will have been transferred.
	 *
	 * stop.error indicates a problem with the stop command.  Data
	 * may have been transferred, or may still be transferring.
	 */
	if (brq->sbc.error || brq->cmd.error || brq->stop.error ||
	    brq->data.error) {
		switch (mmc_blk_cmd_recovery(card, req, brq, &ecc_err, &gen_err)) {
		case ERR_RETRY:
			return MMC_BLK_RETRY;
		case ERR_ABORT:
			return MMC_BLK_ABORT;
		case ERR_NOMEDIUM:
			return MMC_BLK_NOMEDIUM;
		case ERR_CONTINUE:
			break;
		}
	}

	/*
	 * Check for errors relating to the execution of the
	 * initial command - such as address errors.  No data
	 * has been transferred.
	 */
	if (brq->cmd.resp[0] & CMD_ERRORS) {
		pr_err("%s: r/w command failed, status = %#x\n",
		       req->rq_disk->disk_name, brq->cmd.resp[0]);
		return MMC_BLK_ABORT;
	}

	/*
	 * Everything else is either success, or a data error of some
	 * kind.  If it was a write, we may have transitioned to
	 * program mode, which we have to wait for it to complete.
	 */
	if (!mmc_host_is_spi(card->host) && rq_data_dir(req) != READ) {
		int err;

		/* Check stop command response */
		if (brq->stop.resp[0] & R1_ERROR) {
			pr_err("%s: %s: general error sending stop command, stop cmd response %#x\n",
			       req->rq_disk->disk_name, __func__,
			       brq->stop.resp[0]);
			gen_err = 1;
		}

		err = card_busy_detect(card, MMC_BLK_TIMEOUT_MS, false, req,
					&gen_err);
		if (err)
			return MMC_BLK_CMD_ERR;
	}

	/* if general error occurs, retry the write operation. */
	if (gen_err) {
		pr_warn("%s: retrying write for general error\n",
				req->rq_disk->disk_name);
		return MMC_BLK_RETRY;
	}

	if (brq->data.error) {
		if (need_retune && !brq->retune_retry_done) {
			pr_info("%s: retrying because a re-tune was needed\n",
				req->rq_disk->disk_name);
			brq->retune_retry_done = 1;
			return MMC_BLK_RETRY;
		}
		pr_err("%s: error %d transferring data, sector %u, nr %u, cmd response %#x, card status %#x\n",
		       req->rq_disk->disk_name, brq->data.error,
		       (unsigned)blk_rq_pos(req),
		       (unsigned)blk_rq_sectors(req),
		       brq->cmd.resp[0], brq->stop.resp[0]);

		if (rq_data_dir(req) == READ) {
			if (ecc_err)
				return MMC_BLK_ECC_ERR;
			return MMC_BLK_DATA_ERR;
		} else {
			return MMC_BLK_CMD_ERR;
		}
	}

	if (!brq->data.bytes_xfered)
		return MMC_BLK_RETRY;

	if (mmc_packed_cmd(mq_mrq->cmd_type)) {
		if (unlikely(brq->data.blocks << 9 != brq->data.bytes_xfered))
			return MMC_BLK_PARTIAL;
		else
			return MMC_BLK_SUCCESS;
	}

	if (blk_rq_bytes(req) != brq->data.bytes_xfered)
		return MMC_BLK_PARTIAL;

	return MMC_BLK_SUCCESS;
}

static int mmc_blk_packed_err_check(struct mmc_card *card,
				    struct mmc_async_req *areq)
{
	struct mmc_queue_req *mq_rq = container_of(areq, struct mmc_queue_req,
			mmc_active);
	struct request *req = mq_rq->req;
	struct mmc_packed *packed = mq_rq->packed;
	int err, check, status;
	u8 *ext_csd;

	BUG_ON(!packed);

	packed->retries--;
	check = mmc_blk_err_check(card, areq);
	err = get_card_status(card, &status, 0);
	if (err) {
		pr_err("%s: error %d sending status command\n",
		       req->rq_disk->disk_name, err);
		return MMC_BLK_ABORT;
	}

	if (status & R1_EXCEPTION_EVENT) {
		err = mmc_get_ext_csd(card, &ext_csd);
		if (err) {
			pr_err("%s: error %d sending ext_csd\n",
			       req->rq_disk->disk_name, err);
			return MMC_BLK_ABORT;
		}

		if ((ext_csd[EXT_CSD_EXP_EVENTS_STATUS] &
		     EXT_CSD_PACKED_FAILURE) &&
		    (ext_csd[EXT_CSD_PACKED_CMD_STATUS] &
		     EXT_CSD_PACKED_GENERIC_ERROR)) {
			if (ext_csd[EXT_CSD_PACKED_CMD_STATUS] &
			    EXT_CSD_PACKED_INDEXED_ERROR) {
				packed->idx_failure =
				  ext_csd[EXT_CSD_PACKED_FAILURE_INDEX] - 1;
				check = MMC_BLK_PARTIAL;
			}
			pr_err("%s: packed cmd failed, nr %u, sectors %u, "
			       "failure index: %d\n",
			       req->rq_disk->disk_name, packed->nr_entries,
			       packed->blocks, packed->idx_failure);
		}
		kfree(ext_csd);
	}

	return check;
}

static void mmc_blk_rw_rq_prep(struct mmc_queue_req *mqrq,
			       struct mmc_card *card,
			       int disable_multi,
			       struct mmc_queue *mq)
{
	u32 readcmd, writecmd;
	struct mmc_blk_request *brq = &mqrq->brq;
	struct request *req = mqrq->req;
	struct mmc_blk_data *md = mq->data;
	bool do_data_tag;

	/*
	 * Reliable writes are used to implement Forced Unit Access and
	 * are supported only on MMCs.
	 */
	bool do_rel_wr = (req->cmd_flags & REQ_FUA) &&
		(rq_data_dir(req) == WRITE) &&
		(md->flags & MMC_BLK_REL_WR);

	memset(brq, 0, sizeof(struct mmc_blk_request));
	brq->mrq.cmd = &brq->cmd;
	brq->mrq.data = &brq->data;

	brq->cmd.arg = blk_rq_pos(req);
	if (!mmc_card_blockaddr(card))
		brq->cmd.arg <<= 9;
	brq->cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	brq->data.blksz = 512;
	brq->stop.opcode = MMC_STOP_TRANSMISSION;
	brq->stop.arg = 0;
	brq->data.blocks = blk_rq_sectors(req);

	brq->data.fault_injected = false;
	/*
	 * The block layer doesn't support all sector count
	 * restrictions, so we need to be prepared for too big
	 * requests.
	 */
	if (brq->data.blocks > card->host->max_blk_count)
		brq->data.blocks = card->host->max_blk_count;

	if (brq->data.blocks > 1) {
		/*
		 * After a read error, we redo the request one sector
		 * at a time in order to accurately determine which
		 * sectors can be read successfully.
		 */
		if (disable_multi)
			brq->data.blocks = 1;

		/*
		 * Some controllers have HW issues while operating
		 * in multiple I/O mode
		 */
		if (card->host->ops->multi_io_quirk)
			brq->data.blocks = card->host->ops->multi_io_quirk(card,
						(rq_data_dir(req) == READ) ?
						MMC_DATA_READ : MMC_DATA_WRITE,
						brq->data.blocks);
	}

	if (brq->data.blocks > 1 || do_rel_wr) {
		/* SPI multiblock writes terminate using a special
		 * token, not a STOP_TRANSMISSION request.
		 */
		if (!mmc_host_is_spi(card->host) ||
		    rq_data_dir(req) == READ)
			brq->mrq.stop = &brq->stop;
		readcmd = MMC_READ_MULTIPLE_BLOCK;
		writecmd = MMC_WRITE_MULTIPLE_BLOCK;
	} else {
		brq->mrq.stop = NULL;
		readcmd = MMC_READ_SINGLE_BLOCK;
		writecmd = MMC_WRITE_BLOCK;
	}
	if (rq_data_dir(req) == READ) {
		brq->cmd.opcode = readcmd;
		brq->data.flags |= MMC_DATA_READ;
		if (brq->mrq.stop)
			brq->stop.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 |
					MMC_CMD_AC;
	} else {
		brq->cmd.opcode = writecmd;
		brq->data.flags |= MMC_DATA_WRITE;
		if (brq->mrq.stop)
			brq->stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B |
					MMC_CMD_AC;
	}

	if (do_rel_wr)
		mmc_apply_rel_rw(brq, card, req);

	/*
	 * Data tag is used only during writing meta data to speed
	 * up write and any subsequent read of this meta data
	 */
	do_data_tag = (card->ext_csd.data_tag_unit_size) &&
		(req->cmd_flags & REQ_META) &&
		(rq_data_dir(req) == WRITE) &&
		((brq->data.blocks * brq->data.blksz) >=
		 card->ext_csd.data_tag_unit_size);

	/*
	 * Pre-defined multi-block transfers are preferable to
	 * open ended-ones (and necessary for reliable writes).
	 * However, it is not sufficient to just send CMD23,
	 * and avoid the final CMD12, as on an error condition
	 * CMD12 (stop) needs to be sent anyway. This, coupled
	 * with Auto-CMD23 enhancements provided by some
	 * hosts, means that the complexity of dealing
	 * with this is best left to the host. If CMD23 is
	 * supported by card and host, we'll fill sbc in and let
	 * the host deal with handling it correctly. This means
	 * that for hosts that don't expose MMC_CAP_CMD23, no
	 * change of behavior will be observed.
	 *
	 * N.B: Some MMC cards experience perf degradation.
	 * We'll avoid using CMD23-bounded multiblock writes for
	 * these, while retaining features like reliable writes.
	 */
	if ((md->flags & MMC_BLK_CMD23) && mmc_op_multi(brq->cmd.opcode) &&
	    (do_rel_wr || !(card->quirks & MMC_QUIRK_BLK_NO_CMD23) ||
	     do_data_tag)) {
		brq->sbc.opcode = MMC_SET_BLOCK_COUNT;
		brq->sbc.arg = brq->data.blocks |
			(do_rel_wr ? (1 << 31) : 0) |
			(do_data_tag ? (1 << 29) : 0);
		brq->sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;
		brq->mrq.sbc = &brq->sbc;
	}

	mmc_set_data_timeout(&brq->data, card);

	brq->data.sg = mqrq->sg;
	brq->data.sg_len = mmc_queue_map_sg(mq, mqrq);

	/*
	 * Adjust the sg list so it is the same size as the
	 * request.
	 */
	if (brq->data.blocks != blk_rq_sectors(req)) {
		int i, data_size = brq->data.blocks << 9;
		struct scatterlist *sg;

		for_each_sg(brq->data.sg, sg, brq->data.sg_len, i) {
			data_size -= sg->length;
			if (data_size <= 0) {
				sg->length += data_size;
				i++;
				break;
			}
		}
		brq->data.sg_len = i;
	}

	mqrq->mmc_active.mrq = &brq->mrq;
	mqrq->mmc_active.mrq->req = mqrq->req;
	mqrq->mmc_active.err_check = mmc_blk_err_check;

	mmc_queue_bounce_pre(mqrq);
}

static inline u8 mmc_calc_packed_hdr_segs(struct request_queue *q,
					  struct mmc_card *card)
{
	unsigned int hdr_sz = mmc_large_sector(card) ? 4096 : 512;
	unsigned int max_seg_sz = queue_max_segment_size(q);
	unsigned int len, nr_segs = 0;

	do {
		len = min(hdr_sz, max_seg_sz);
		hdr_sz -= len;
		nr_segs++;
	} while (hdr_sz);

	return nr_segs;
}

/**
 * mmc_blk_disable_wr_packing() - disables packing mode
 * @mq:	MMC queue.
 *
 */
void mmc_blk_disable_wr_packing(struct mmc_queue *mq)
{
	if (mq) {
		mq->wr_packing_enabled = false;
		mq->num_of_potential_packed_wr_reqs = 0;
	}
}
EXPORT_SYMBOL(mmc_blk_disable_wr_packing);

static int get_packed_trigger(int potential, struct mmc_card *card,
			      struct request *req, int curr_trigger)
{
	static int num_mean_elements = 1;
	static unsigned long mean_potential = PCKD_TRGR_INIT_MEAN_POTEN;
	unsigned int trigger = curr_trigger;
	unsigned int pckd_trgr_upper_bound = card->ext_csd.max_packed_writes;

	/* scale down the upper bound to 75% */
	pckd_trgr_upper_bound = (pckd_trgr_upper_bound * 3) / 4;

	/*
	 * since the most common calls for this function are with small
	 * potential write values and since we don't want these calls to affect
	 * the packed trigger, set a lower bound and ignore calls with
	 * potential lower than that bound
	 */
	if (potential <= PCKD_TRGR_POTEN_LOWER_BOUND)
		return trigger;

	/*
	 * this is to prevent integer overflow in the following calculation:
	 * once every PACKED_TRIGGER_MAX_ELEMENTS reset the algorithm
	 */
	if (num_mean_elements > PACKED_TRIGGER_MAX_ELEMENTS) {
		num_mean_elements = 1;
		mean_potential = PCKD_TRGR_INIT_MEAN_POTEN;
	}

	/*
	 * get next mean value based on previous mean value and current
	 * potential packed writes. Calculation is as follows:
	 * mean_pot[i+1] =
	 *	((mean_pot[i] * num_mean_elem) + potential)/(num_mean_elem + 1)
	 */
	mean_potential *= num_mean_elements;
	/*
	 * add num_mean_elements so that the division of two integers doesn't
	 * lower mean_potential too much
	 */
	if (potential > mean_potential)
		mean_potential += num_mean_elements;
	mean_potential += potential;
	/* this is for gaining more precision when dividing two integers */
	mean_potential *= PCKD_TRGR_PRECISION_MULTIPLIER;
	/* this completes the mean calculation */
	mean_potential /= ++num_mean_elements;
	mean_potential /= PCKD_TRGR_PRECISION_MULTIPLIER;

	/*
	 * if current potential packed writes is greater than the mean potential
	 * then the heuristic is that the following workload will contain many
	 * write requests, therefore we lower the packed trigger. In the
	 * opposite case we want to increase the trigger in order to get less
	 * packing events.
	 */
	if (potential >= mean_potential)
		trigger = (trigger <= PCKD_TRGR_LOWER_BOUND) ?
				PCKD_TRGR_LOWER_BOUND : trigger - 1;
	else
		trigger = (trigger >= pckd_trgr_upper_bound) ?
				pckd_trgr_upper_bound : trigger + 1;

	/*
	 * an urgent read request indicates a packed list being interrupted
	 * by this read, therefore we aim for less packing, hence the trigger
	 * gets increased
	 */
	if (req && (req->cmd_flags & REQ_URGENT) && (rq_data_dir(req) == READ))
		trigger += PCKD_TRGR_URGENT_PENALTY;

	return trigger;
}

static void mmc_blk_write_packing_control(struct mmc_queue *mq,
					  struct request *req)
{
	struct mmc_host *host = mq->card->host;
	int data_dir;

	if (!(host->caps2 & MMC_CAP2_PACKED_WR))
		return;

	/* Support for the write packing on eMMC 4.5 or later */
	if (mq->card->ext_csd.rev <= 5)
		return;

	/*
	 * In case the packing control is not supported by the host, it should
	 * not have an effect on the write packing. Therefore we have to enable
	 * the write packing
	 */
	if (!(host->caps2 & MMC_CAP2_PACKED_WR_CONTROL)) {
		mq->wr_packing_enabled = true;
		return;
	}

	if (!req || (req && (req->cmd_flags & REQ_FLUSH))) {
		if (mq->num_of_potential_packed_wr_reqs >
				mq->num_wr_reqs_to_start_packing)
			mq->wr_packing_enabled = true;
		mq->num_wr_reqs_to_start_packing =
			get_packed_trigger(mq->num_of_potential_packed_wr_reqs,
					   mq->card, req,
					   mq->num_wr_reqs_to_start_packing);
		mq->num_of_potential_packed_wr_reqs = 0;
		return;
	}

	data_dir = rq_data_dir(req);

	if (data_dir == READ) {
		mmc_blk_disable_wr_packing(mq);
		mq->num_wr_reqs_to_start_packing =
			get_packed_trigger(mq->num_of_potential_packed_wr_reqs,
					   mq->card, req,
					   mq->num_wr_reqs_to_start_packing);
		mq->num_of_potential_packed_wr_reqs = 0;
		mq->wr_packing_enabled = false;
		return;
	} else if (data_dir == WRITE) {
		mq->num_of_potential_packed_wr_reqs++;
	}

	if (mq->num_of_potential_packed_wr_reqs >
			mq->num_wr_reqs_to_start_packing)
		mq->wr_packing_enabled = true;
}

struct mmc_wr_pack_stats *mmc_blk_get_packed_statistics(struct mmc_card *card)
{
	if (!card)
		return NULL;

	return &card->wr_pack_stats;
}
EXPORT_SYMBOL(mmc_blk_get_packed_statistics);

void mmc_blk_init_packed_statistics(struct mmc_card *card)
{
	int max_num_of_packed_reqs = 0;

	if (!card || !card->wr_pack_stats.packing_events)
		return;

	max_num_of_packed_reqs = card->ext_csd.max_packed_writes;

	spin_lock(&card->wr_pack_stats.lock);
	memset(card->wr_pack_stats.packing_events, 0,
		(max_num_of_packed_reqs + 1) *
	       sizeof(*card->wr_pack_stats.packing_events));
	memset(&card->wr_pack_stats.pack_stop_reason, 0,
		sizeof(card->wr_pack_stats.pack_stop_reason));
	card->wr_pack_stats.enabled = true;
	spin_unlock(&card->wr_pack_stats.lock);
}
EXPORT_SYMBOL(mmc_blk_init_packed_statistics);

static u8 mmc_blk_prep_packed_list(struct mmc_queue *mq, struct request *req)
{
	struct request_queue *q = mq->queue;
	struct mmc_card *card = mq->card;
	struct request *cur = req, *next = NULL;
	struct mmc_blk_data *md = mq->data;
	struct mmc_queue_req *mqrq = mq->mqrq_cur;
	bool en_rel_wr = card->ext_csd.rel_param & EXT_CSD_WR_REL_PARAM_EN;
	unsigned int req_sectors = 0, phys_segments = 0;
	unsigned int max_blk_count, max_phys_segs;
	bool put_back = true;
	u8 max_packed_rw = 0;
	u8 reqs = 0;
	struct mmc_wr_pack_stats *stats = &card->wr_pack_stats;

	if (!(md->flags & MMC_BLK_PACKED_CMD))
		goto no_packed;

	if (!mq->wr_packing_enabled)
		goto no_packed;

	if ((rq_data_dir(cur) == WRITE) &&
	    mmc_host_packed_wr(card->host))
		max_packed_rw = card->ext_csd.max_packed_writes;

	if (max_packed_rw == 0)
		goto no_packed;

	if (mmc_req_rel_wr(cur) &&
	    (md->flags & MMC_BLK_REL_WR) && !en_rel_wr)
		goto no_packed;

	if (mmc_large_sector(card) &&
	    !IS_ALIGNED(blk_rq_sectors(cur), 8))
		goto no_packed;

	if (cur->cmd_flags & REQ_FUA)
		goto no_packed;

	mmc_blk_clear_packed(mqrq);

	max_blk_count = min(card->host->max_blk_count,
			    card->host->max_req_size >> 9);
	if (unlikely(max_blk_count > 0xffff))
		max_blk_count = 0xffff;

	max_phys_segs = queue_max_segments(q);
	req_sectors += blk_rq_sectors(cur);
	phys_segments += cur->nr_phys_segments;

	if (rq_data_dir(cur) == WRITE) {
		req_sectors += mmc_large_sector(card) ? 8 : 1;
		phys_segments += mmc_calc_packed_hdr_segs(q, card);
	}

	spin_lock(&stats->lock);
	do {
		if (reqs >= max_packed_rw - 1) {
			put_back = false;
			break;
		}

		spin_lock_irq(q->queue_lock);
		next = blk_fetch_request(q);
		spin_unlock_irq(q->queue_lock);
		if (!next) {
			MMC_BLK_UPDATE_STOP_REASON(stats, EMPTY_QUEUE);
			put_back = false;
			break;
		}

		if (mmc_large_sector(card) &&
		    !IS_ALIGNED(blk_rq_sectors(next), 8)) {
			MMC_BLK_UPDATE_STOP_REASON(stats, LARGE_SEC_ALIGN);
			break;
		}

		if (next->cmd_flags & REQ_DISCARD ||
		    next->cmd_flags & REQ_FLUSH) {
			MMC_BLK_UPDATE_STOP_REASON(stats, FLUSH_OR_DISCARD);
			break;
		}

		if (next->cmd_flags & REQ_FUA) {
			MMC_BLK_UPDATE_STOP_REASON(stats, FUA);
			break;
		}

		if (rq_data_dir(cur) != rq_data_dir(next)) {
			MMC_BLK_UPDATE_STOP_REASON(stats, WRONG_DATA_DIR);
			break;
		}

		if (mmc_req_rel_wr(next) &&
		    (md->flags & MMC_BLK_REL_WR) && !en_rel_wr) {
			MMC_BLK_UPDATE_STOP_REASON(stats, REL_WRITE);
			break;
		}

		req_sectors += blk_rq_sectors(next);
		if (req_sectors > max_blk_count) {
			if (stats->enabled)
				stats->pack_stop_reason[EXCEEDS_SECTORS]++;
			break;
		}

		phys_segments +=  next->nr_phys_segments;
		if (phys_segments > max_phys_segs) {
			MMC_BLK_UPDATE_STOP_REASON(stats, EXCEEDS_SEGMENTS);
			break;
		}

		if (mq->no_pack_for_random) {
			if ((blk_rq_pos(cur) + blk_rq_sectors(cur)) !=
			    blk_rq_pos(next)) {
				MMC_BLK_UPDATE_STOP_REASON(stats, RANDOM);
				put_back = 1;
				break;
			}
		}

		if (rq_data_dir(next) == WRITE)
			mq->num_of_potential_packed_wr_reqs++;
		list_add_tail(&next->queuelist, &mqrq->packed->list);
		cur = next;
		reqs++;
	} while (1);

	if (put_back) {
		spin_lock_irq(q->queue_lock);
		blk_requeue_request(q, next);
		spin_unlock_irq(q->queue_lock);
	}

	if (stats->enabled) {
		if (reqs + 1 <= card->ext_csd.max_packed_writes)
			stats->packing_events[reqs + 1]++;
		if (reqs + 1 == max_packed_rw)
			MMC_BLK_UPDATE_STOP_REASON(stats, THRESHOLD);
	}

	spin_unlock(&stats->lock);

	if (reqs > 0) {
		list_add(&req->queuelist, &mqrq->packed->list);
		mqrq->packed->nr_entries = ++reqs;
		mqrq->packed->retries = reqs;
		return reqs;
	}

no_packed:
	mqrq->cmd_type = MMC_PACKED_NONE;
	return 0;
}

static void mmc_blk_packed_hdr_wrq_prep(struct mmc_queue_req *mqrq,
					struct mmc_card *card,
					struct mmc_queue *mq)
{
	struct mmc_blk_request *brq = &mqrq->brq;
	struct request *req = mqrq->req;
	struct request *prq;
	struct mmc_blk_data *md = mq->data;
	struct mmc_packed *packed = mqrq->packed;
	bool do_rel_wr, do_data_tag;
	u32 *packed_cmd_hdr;
	u8 hdr_blocks;
	u8 i = 1;

	BUG_ON(!packed);

	mqrq->cmd_type = MMC_PACKED_WRITE;
	packed->blocks = 0;
	packed->idx_failure = MMC_PACKED_NR_IDX;

	packed_cmd_hdr = packed->cmd_hdr;
	memset(packed_cmd_hdr, 0, sizeof(packed->cmd_hdr));
	packed_cmd_hdr[0] = cpu_to_le32((packed->nr_entries << 16) |
		(PACKED_CMD_WR << 8) | PACKED_CMD_VER);
	hdr_blocks = mmc_large_sector(card) ? 8 : 1;

	/*
	 * Argument for each entry of packed group
	 */
	list_for_each_entry(prq, &packed->list, queuelist) {
		do_rel_wr = mmc_req_rel_wr(prq) && (md->flags & MMC_BLK_REL_WR);
		do_data_tag = (card->ext_csd.data_tag_unit_size) &&
			(prq->cmd_flags & REQ_META) &&
			(rq_data_dir(prq) == WRITE) &&
			((brq->data.blocks * brq->data.blksz) >=
			 card->ext_csd.data_tag_unit_size);
		/* Argument of CMD23 */
		packed_cmd_hdr[(i * 2)] = cpu_to_le32(
			(do_rel_wr ? MMC_CMD23_ARG_REL_WR : 0) |
			(do_data_tag ? MMC_CMD23_ARG_TAG_REQ : 0) |
			blk_rq_sectors(prq));
		/* Argument of CMD18 or CMD25 */
		packed_cmd_hdr[((i * 2)) + 1] = cpu_to_le32(
			mmc_card_blockaddr(card) ?
			blk_rq_pos(prq) : blk_rq_pos(prq) << 9);
		packed->blocks += blk_rq_sectors(prq);
		i++;
	}

	memset(brq, 0, sizeof(struct mmc_blk_request));
	brq->mrq.cmd = &brq->cmd;
	brq->mrq.data = &brq->data;
	brq->mrq.sbc = &brq->sbc;
	brq->mrq.stop = &brq->stop;

	brq->sbc.opcode = MMC_SET_BLOCK_COUNT;
	brq->sbc.arg = MMC_CMD23_ARG_PACKED | (packed->blocks + hdr_blocks);
	brq->sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;

	brq->cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
	brq->cmd.arg = blk_rq_pos(req);
	if (!mmc_card_blockaddr(card))
		brq->cmd.arg <<= 9;
	brq->cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	brq->data.blksz = 512;
	brq->data.blocks = packed->blocks + hdr_blocks;
	brq->data.flags |= MMC_DATA_WRITE;
	brq->data.fault_injected = false;

	brq->stop.opcode = MMC_STOP_TRANSMISSION;
	brq->stop.arg = 0;
	brq->stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mmc_set_data_timeout(&brq->data, card);

	brq->data.sg = mqrq->sg;
	brq->data.sg_len = mmc_queue_map_sg(mq, mqrq);

	mqrq->mmc_active.mrq = &brq->mrq;

	/*
	 * This is intended for packed commands tests usage - in case these
	 * functions are not in use the respective pointers are NULL
	 */
	if (mq->err_check_fn)
		mqrq->mmc_active.err_check = mq->err_check_fn;
	else
		mqrq->mmc_active.err_check = mmc_blk_packed_err_check;

	if (mq->packed_test_fn)
		mq->packed_test_fn(mq->queue, mqrq);

	mmc_queue_bounce_pre(mqrq);
}

static int mmc_blk_cmd_err(struct mmc_blk_data *md, struct mmc_card *card,
			   struct mmc_blk_request *brq, struct request *req,
			   int ret)
{
	struct mmc_queue_req *mq_rq;
	mq_rq = container_of(brq, struct mmc_queue_req, brq);

	/*
	 * If this is an SD card and we're writing, we can first
	 * mark the known good sectors as ok.
	 *
	 * If the card is not SD, we can still ok written sectors
	 * as reported by the controller (which might be less than
	 * the real number of written sectors, but never more).
	 */
	if (mmc_card_sd(card)) {
		u32 blocks;
		if (!brq->data.fault_injected) {
			blocks = mmc_sd_num_wr_blocks(card);
			if (blocks != (u32)-1)
				ret = blk_end_request(req, 0, blocks << 9);
		} else
			ret = blk_end_request(req, 0, brq->data.bytes_xfered);
	} else {
		if (!mmc_packed_cmd(mq_rq->cmd_type))
			ret = blk_end_request(req, 0, brq->data.bytes_xfered);
	}
	return ret;
}

static int mmc_blk_end_packed_req(struct mmc_queue_req *mq_rq)
{
	struct request *prq;
	struct mmc_packed *packed = mq_rq->packed;
	int idx = packed->idx_failure, i = 0;
	int ret = 0;

	BUG_ON(!packed);

	while (!list_empty(&packed->list)) {
		prq = list_entry_rq(packed->list.next);
		if (idx == i) {
			/* retry from error index */
			packed->nr_entries -= idx;
			mq_rq->req = prq;
			ret = 1;

			if (packed->nr_entries == MMC_PACKED_NR_SINGLE) {
				list_del_init(&prq->queuelist);
				mmc_blk_clear_packed(mq_rq);
			}
			return ret;
		}
		list_del_init(&prq->queuelist);
		blk_end_request(prq, 0, blk_rq_bytes(prq));
		i++;
	}

	mmc_blk_clear_packed(mq_rq);
	return ret;
}

static void mmc_blk_abort_packed_req(struct mmc_queue_req *mq_rq)
{
	struct request *prq;
	struct mmc_packed *packed = mq_rq->packed;

	BUG_ON(!packed);

	while (!list_empty(&packed->list)) {
		prq = list_entry_rq(packed->list.next);
		list_del_init(&prq->queuelist);
		blk_end_request(prq, -EIO, blk_rq_bytes(prq));
	}

	mmc_blk_clear_packed(mq_rq);
}

static void mmc_blk_revert_packed_req(struct mmc_queue *mq,
				      struct mmc_queue_req *mq_rq)
{
	struct request *prq;
	struct request_queue *q = mq->queue;
	struct mmc_packed *packed = mq_rq->packed;

	BUG_ON(!packed);

	while (!list_empty(&packed->list)) {
		prq = list_entry_rq(packed->list.prev);
		if (prq->queuelist.prev != &packed->list) {
			list_del_init(&prq->queuelist);
			spin_lock_irq(q->queue_lock);
			blk_requeue_request(mq->queue, prq);
			spin_unlock_irq(q->queue_lock);
		} else {
			list_del_init(&prq->queuelist);
		}
	}

	mmc_blk_clear_packed(mq_rq);
}

static int mmc_blk_cmdq_start_req(struct mmc_host *host,
				  struct mmc_cmdq_req *cmdq_req)
{
	struct mmc_request *mrq = &cmdq_req->mrq;

	mrq->done = mmc_blk_cmdq_req_done;
	return mmc_cmdq_start_req(host, cmdq_req);
}

/* prepare for non-data commands */
static struct mmc_cmdq_req *mmc_cmdq_prep_dcmd(
		struct mmc_queue_req *mqrq, struct mmc_queue *mq)
{
	struct request *req = mqrq->req;
	struct mmc_cmdq_req *cmdq_req = &mqrq->cmdq_req;

	memset(&mqrq->cmdq_req, 0, sizeof(struct mmc_cmdq_req));

	cmdq_req->mrq.data = NULL;
	cmdq_req->cmd_flags = req->cmd_flags;
	cmdq_req->mrq.req = mqrq->req;
	req->special = mqrq;
	cmdq_req->cmdq_req_flags |= DCMD;
	cmdq_req->mrq.cmdq_req = cmdq_req;

	return &mqrq->cmdq_req;
}


#define IS_RT_CLASS_REQ(x)     \
	(IOPRIO_PRIO_CLASS(req_get_ioprio(x)) == IOPRIO_CLASS_RT)

static struct mmc_cmdq_req *mmc_blk_cmdq_rw_prep(
		struct mmc_queue_req *mqrq, struct mmc_queue *mq)
{
	struct mmc_card *card = mq->card;
	struct request *req = mqrq->req;
	struct mmc_blk_data *md = mq->data;
	bool do_rel_wr = mmc_req_rel_wr(req) && (md->flags & MMC_BLK_REL_WR);
	bool do_data_tag;
	bool read_dir = (rq_data_dir(req) == READ);
	bool prio = IS_RT_CLASS_REQ(req);
	struct mmc_cmdq_req *cmdq_rq = &mqrq->cmdq_req;

	memset(&mqrq->cmdq_req, 0, sizeof(struct mmc_cmdq_req));

	cmdq_rq->tag = req->tag;
	if (read_dir) {
		cmdq_rq->cmdq_req_flags |= DIR;
		cmdq_rq->data.flags = MMC_DATA_READ;
	} else {
		cmdq_rq->data.flags = MMC_DATA_WRITE;
	}
	if (prio)
		cmdq_rq->cmdq_req_flags |= PRIO;

	if (do_rel_wr)
		cmdq_rq->cmdq_req_flags |= REL_WR;

	cmdq_rq->data.blocks = blk_rq_sectors(req);
	cmdq_rq->blk_addr = blk_rq_pos(req);
	cmdq_rq->data.blksz = MMC_CARD_CMDQ_BLK_SIZE;

	mmc_set_data_timeout(&cmdq_rq->data, card);

	do_data_tag = (card->ext_csd.data_tag_unit_size) &&
		(req->cmd_flags & REQ_META) &&
		(rq_data_dir(req) == WRITE) &&
		((cmdq_rq->data.blocks * cmdq_rq->data.blksz) >=
		 card->ext_csd.data_tag_unit_size);
	if (do_data_tag)
		cmdq_rq->cmdq_req_flags |= DAT_TAG;
	cmdq_rq->data.sg = mqrq->sg;
	cmdq_rq->data.sg_len = mmc_queue_map_sg(mq, mqrq);

	/*
	 * Adjust the sg list so it is the same size as the
	 * request.
	 */
	if (cmdq_rq->data.blocks > card->host->max_blk_count)
		cmdq_rq->data.blocks = card->host->max_blk_count;

	if (cmdq_rq->data.blocks != blk_rq_sectors(req)) {
		int i, data_size = cmdq_rq->data.blocks << 9;
		struct scatterlist *sg;

		for_each_sg(cmdq_rq->data.sg, sg, cmdq_rq->data.sg_len, i) {
			data_size -= sg->length;
			if (data_size <= 0) {
				sg->length += data_size;
				i++;
				break;
			}
		}
		cmdq_rq->data.sg_len = i;
	}

	mqrq->cmdq_req.cmd_flags = req->cmd_flags;
	mqrq->cmdq_req.mrq.req = mqrq->req;
	mqrq->cmdq_req.mrq.cmdq_req = &mqrq->cmdq_req;
	mqrq->cmdq_req.mrq.data = &mqrq->cmdq_req.data;
	mqrq->req->special = mqrq;

	pr_debug("%s: %s: mrq: 0x%p req: 0x%p mqrq: 0x%p bytes to xf: %d mmc_cmdq_req: 0x%p card-addr: 0x%08x dir(r-1/w-0): %d\n",
		 mmc_hostname(card->host), __func__, &mqrq->cmdq_req.mrq,
		 mqrq->req, mqrq, (cmdq_rq->data.blocks * cmdq_rq->data.blksz),
		 cmdq_rq, cmdq_rq->blk_addr,
		 (cmdq_rq->cmdq_req_flags & DIR) ? 1 : 0);

	return &mqrq->cmdq_req;
}

static int mmc_blk_cmdq_issue_rw_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_queue_req *active_mqrq;
	struct mmc_card *card = mq->card;
	struct mmc_host *host = card->host;
	struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;
	struct mmc_cmdq_req *mc_rq;
	u8 active_small_sector_read = 0;
	int ret = 0;

	mmc_deferred_scaling(host);
	mmc_cmdq_clk_scaling_start_busy(host, true);

	BUG_ON((req->tag < 0) || (req->tag > card->ext_csd.cmdq_depth));
	BUG_ON(test_and_set_bit(req->tag, &host->cmdq_ctx.data_active_reqs));
	BUG_ON(test_and_set_bit(req->tag, &host->cmdq_ctx.active_reqs));

	active_mqrq = &mq->mqrq_cmdq[req->tag];
	active_mqrq->req = req;

	mc_rq = mmc_blk_cmdq_rw_prep(active_mqrq, mq);

	if (card->quirks & MMC_QUIRK_CMDQ_EMPTY_BEFORE_DCMD) {
		unsigned int sectors = blk_rq_sectors(req);

		if (((sectors > 0) && (sectors < 8))
		    && (rq_data_dir(req) == READ))
			active_small_sector_read = 1;
	}
	ret = mmc_blk_cmdq_start_req(card->host, mc_rq);
	if (!ret && active_small_sector_read)
		host->cmdq_ctx.active_small_sector_read_reqs++;
	/*
	 * When in SVS2 on low load scenario and there are lots of requests
	 * queued for CMDQ we need to wait till the queue is empty to scale
	 * back up to Nominal even if there is a sudden increase in load.
	 * This impacts performance where lots of IO get executed in SVS2
	 * frequency since the queue is full. As SVS2 is a low load use case
	 * we can serialize the requests and not queue them in parallel
	 * without impacting other use cases. This makes sure the queue gets
	 * empty faster and we will be able to scale up to Nominal frequency
	 * when needed.
	 */
	if (!ret && (host->clk_scaling.state == MMC_LOAD_LOW))
		wait_event_interruptible(ctx->queue_empty_wq,
					(!ctx->active_reqs));

	return ret;
}

/*
 * Issues a flush (dcmd) request
 */
int mmc_blk_cmdq_issue_flush_rq(struct mmc_queue *mq, struct request *req)
{
	int err;
	struct mmc_queue_req *active_mqrq;
	struct mmc_card *card = mq->card;
	struct mmc_host *host;
	struct mmc_cmdq_req *cmdq_req;
	struct mmc_cmdq_context_info *ctx_info;

	BUG_ON(!card);
	host = card->host;
	BUG_ON(!host);
	BUG_ON(req->tag > card->ext_csd.cmdq_depth);
	BUG_ON(test_and_set_bit(req->tag, &host->cmdq_ctx.active_reqs));

	ctx_info = &host->cmdq_ctx;

	set_bit(CMDQ_STATE_DCMD_ACTIVE, &ctx_info->curr_state);

	active_mqrq = &mq->mqrq_cmdq[req->tag];
	active_mqrq->req = req;

	cmdq_req = mmc_cmdq_prep_dcmd(active_mqrq, mq);
	cmdq_req->cmdq_req_flags |= QBR;
	cmdq_req->mrq.cmd = &cmdq_req->cmd;
	cmdq_req->tag = req->tag;

	err = mmc_cmdq_prepare_flush(cmdq_req->mrq.cmd);
	if (err) {
		pr_err("%s: failed (%d) preparing flush req\n",
		       mmc_hostname(host), err);
		return err;
	}
	err = mmc_blk_cmdq_start_req(card->host, cmdq_req);
	return err;
}
EXPORT_SYMBOL(mmc_blk_cmdq_issue_flush_rq);

static void mmc_blk_cmdq_reset(struct mmc_host *host, bool clear_all)
{
	int err = 0;

	if (mmc_cmdq_halt(host, true)) {
		pr_err("%s: halt failed\n", mmc_hostname(host));
		goto reset;
	}

	if (clear_all)
		mmc_cmdq_discard_queue(host, 0);
reset:
	mmc_host_clk_hold(host);
	host->cmdq_ops->disable(host, true);
	mmc_host_clk_release(host);
	err = mmc_cmdq_hw_reset(host);
	if (err && err != -EOPNOTSUPP) {
		pr_err("%s: failed to cmdq_hw_reset err = %d\n",
				mmc_hostname(host), err);
		mmc_host_clk_hold(host);
		host->cmdq_ops->enable(host);
		mmc_host_clk_release(host);
		mmc_cmdq_halt(host, false);
		goto out;
	}
	/*
	 * CMDQ HW reset would have already made CQE
	 * in unhalted state, but reflect the same
	 * in software state of cmdq_ctx.
	 */
	mmc_host_clr_halt(host);
out:
	return;
}

/**
 * is_cmdq_dcmd_req - Checks if tag belongs to DCMD request.
 * @q:		request_queue pointer.
 * @tag:	tag number of request to check.
 *
 * This function checks if the request with tag number "tag"
 * is a DCMD request or not based on cmdq_req_flags set.
 *
 * returns true if DCMD req, otherwise false.
 */
static bool is_cmdq_dcmd_req(struct request_queue *q, int tag)
{
	struct request *req;
	struct mmc_queue_req *mq_rq;
	struct mmc_cmdq_req *cmdq_req;

	req = blk_queue_find_tag(q, tag);
	if (WARN_ON(!req))
		goto out;
	mq_rq = req->special;
	if (WARN_ON(!mq_rq))
		goto out;
	cmdq_req = &(mq_rq->cmdq_req);
	return (cmdq_req->cmdq_req_flags & DCMD);
out:
	return -ENOENT;
}

/**
 * mmc_blk_cmdq_reset_all - Reset everything for CMDQ block request.
 * @host:	mmc_host pointer.
 * @err:	error for which reset is performed.
 *
 * This function implements reset_all functionality for
 * cmdq. It resets the controller, power cycle the card,
 * and invalidate all busy tags(requeue all request back to
 * elevator).
 */
static void mmc_blk_cmdq_reset_all(struct mmc_host *host, int err)
{
	struct mmc_request *mrq = host->err_mrq;
	struct mmc_card *card = host->card;
	struct mmc_cmdq_context_info *ctx_info = &host->cmdq_ctx;
	struct request_queue *q;
	int itag = 0;
	int ret = 0;

	if (WARN_ON(!mrq))
		return;

	q = mrq->req->q;
	WARN_ON(!test_bit(CMDQ_STATE_ERR, &ctx_info->curr_state));

	#ifdef CONFIG_MMC_CLKGATE
	pr_debug("%s: %s: active_reqs = %lu, clk_requests = %d\n",
			mmc_hostname(host), __func__,
			ctx_info->active_reqs, host->clk_requests);
	#endif

	mmc_blk_cmdq_reset(host, false);

	for_each_set_bit(itag, &ctx_info->active_reqs,
			host->num_cq_slots) {
		ret = is_cmdq_dcmd_req(q, itag);
		if (WARN_ON(ret == -ENOENT))
			continue;
		if (!ret) {
			WARN_ON(!test_and_clear_bit(itag,
				 &ctx_info->data_active_reqs));
			mmc_cmdq_post_req(host, itag, err);
		} else {
			clear_bit(CMDQ_STATE_DCMD_ACTIVE,
					&ctx_info->curr_state);
		}
		WARN_ON(!test_and_clear_bit(itag,
					&ctx_info->active_reqs));
		mmc_host_clk_release(host);
		mmc_put_card(card);
	}

	spin_lock_irq(q->queue_lock);
	blk_queue_invalidate_tags(q);
	spin_unlock_irq(q->queue_lock);
}

static void mmc_blk_cmdq_shutdown(struct mmc_queue *mq)
{
	int err;
	struct mmc_card *card = mq->card;
	struct mmc_host *host = card->host;

	mmc_get_card(card);
	mmc_host_clk_hold(host);
	err = mmc_cmdq_halt(host, true);
	if (err) {
		pr_err("%s: halt: failed: %d\n", __func__, err);
		goto out;
	}

	/* disable CQ mode in card */
	if (mmc_card_cmdq(card)) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_CMDQ, 0,
				 card->ext_csd.generic_cmd6_time);
		if (err) {
			pr_err("%s: failed to switch card to legacy mode: %d\n",
			       __func__, err);
			goto out;
		}
		mmc_card_clr_cmdq(card);
	}
	host->cmdq_ops->disable(host, false);
	host->card->cmdq_init = false;
out:
	mmc_host_clk_release(host);
	mmc_put_card(card);
}

static enum blk_eh_timer_return mmc_blk_cmdq_req_timed_out(struct request *req)
{
	struct mmc_queue *mq = req->q->queuedata;
	struct mmc_host *host = mq->card->host;
	struct mmc_queue_req *mq_rq = req->special;
	struct mmc_request *mrq;
	struct mmc_cmdq_req *cmdq_req;
	struct mmc_cmdq_context_info *ctx_info = &host->cmdq_ctx;

	BUG_ON(!host);

	/*
	 * The mmc_queue_req will be present only if the request
	 * is issued to the LLD. The request could be fetched from
	 * block layer queue but could be waiting to be issued
	 * (for e.g. clock scaling is waiting for an empty cmdq queue)
	 * Reset the timer in such cases to give LLD more time
	 */
	if (!mq_rq) {
		pr_warn("%s: restart timer for tag: %d\n", __func__, req->tag);
		return BLK_EH_RESET_TIMER;
	}

	mrq = &mq_rq->cmdq_req.mrq;
	cmdq_req = &mq_rq->cmdq_req;

	BUG_ON(!mrq || !cmdq_req);

	if (cmdq_req->cmdq_req_flags & DCMD)
		mrq->cmd->error = -ETIMEDOUT;
	else
		mrq->data->error = -ETIMEDOUT;

	if (mrq->cmd && mrq->cmd->error) {
		if (!(mrq->req->cmd_flags & REQ_FLUSH)) {
			/*
			 * Notify completion for non flush commands like
			 * discard that wait for DCMD finish.
			 */
			set_bit(CMDQ_STATE_REQ_TIMED_OUT,
					&ctx_info->curr_state);
			complete(&mrq->completion);
			return BLK_EH_NOT_HANDLED;
		}
	}

	if (test_bit(CMDQ_STATE_REQ_TIMED_OUT, &ctx_info->curr_state) ||
		test_bit(CMDQ_STATE_ERR, &ctx_info->curr_state))
		return BLK_EH_NOT_HANDLED;

	set_bit(CMDQ_STATE_REQ_TIMED_OUT, &ctx_info->curr_state);
	return BLK_EH_HANDLED;
}

/*
 * mmc_blk_cmdq_err: error handling of cmdq error requests.
 * Function should be called in context of error out request
 * which has claim_host and rpm acquired.
 * This may be called with CQ engine halted. Make sure to
 * unhalt it after error recovery.
 *
 * TODO: Currently cmdq error handler does reset_all in case
 * of any erorr. Need to optimize error handling.
 */
static void mmc_blk_cmdq_err(struct mmc_queue *mq)
{
	struct mmc_host *host = mq->card->host;
	struct mmc_request *mrq = host->err_mrq;
	struct mmc_cmdq_context_info *ctx_info = &host->cmdq_ctx;
	struct request_queue *q;
	int err, ret;
	u32 status = 0;

	mmc_host_clk_hold(host);
	host->cmdq_ops->dumpstate(host);
	mmc_host_clk_release(host);

	if (WARN_ON(!mrq))
		return;

	q = mrq->req->q;
	err = mmc_cmdq_halt(host, true);
	if (err) {
		pr_err("halt: failed: %d\n", err);
		goto reset;
	}

	/* RED error - Fatal: requires reset */
	if (mrq->cmdq_req->resp_err) {
		err = mrq->cmdq_req->resp_err;
		goto reset;
	}

	/*
	 * TIMEOUT errrors can happen because of execution error
	 * in the last command. So send cmd 13 to get device status
	 */
	if ((mrq->cmd && (mrq->cmd->error == -ETIMEDOUT)) ||
			(mrq->data && (mrq->data->error == -ETIMEDOUT))) {
		if (mmc_host_halt(host) || mmc_host_cq_disable(host)) {
			ret = get_card_status(host->card, &status, 0);
			if (ret)
				pr_err("%s: CMD13 failed with err %d\n",
						mmc_hostname(host), ret);
		}
		pr_err("%s: Timeout error detected with device status 0x%08x\n",
			mmc_hostname(host), status);
	}

	/*
	 * In case of software request time-out, we schedule err work only for
	 * the first error out request and handles all other request in flight
	 * here.
	 */
	if (test_bit(CMDQ_STATE_REQ_TIMED_OUT, &ctx_info->curr_state)) {
		err = -ETIMEDOUT;
	} else if (mrq->data && mrq->data->error) {
		err = mrq->data->error;
	} else if (mrq->cmd && mrq->cmd->error) {
		/* DCMD commands */
		err = mrq->cmd->error;
	}

reset:
	mmc_blk_cmdq_reset_all(host, err);
	if (mrq->cmdq_req->resp_err)
		mrq->cmdq_req->resp_err = false;
	mmc_cmdq_halt(host, false);

	host->err_mrq = NULL;
	clear_bit(CMDQ_STATE_REQ_TIMED_OUT, &ctx_info->curr_state);
	WARN_ON(!test_and_clear_bit(CMDQ_STATE_ERR, &ctx_info->curr_state));
	wake_up(&ctx_info->wait);
}

/* invoked by block layer in softirq context */
void mmc_blk_cmdq_complete_rq(struct request *rq)
{
	struct mmc_queue_req *mq_rq = rq->special;
	struct mmc_request *mrq = &mq_rq->cmdq_req.mrq;
	struct mmc_host *host = mrq->host;
	struct mmc_cmdq_context_info *ctx_info = &host->cmdq_ctx;
	struct mmc_cmdq_req *cmdq_req = &mq_rq->cmdq_req;
	struct mmc_queue *mq = (struct mmc_queue *)rq->q->queuedata;
	int err = 0;
	bool is_dcmd = false;

	if (mrq->cmd && mrq->cmd->error)
		err = mrq->cmd->error;
	else if (mrq->data && mrq->data->error)
		err = mrq->data->error;

	if ((err || cmdq_req->resp_err) && !cmdq_req->skip_err_handling) {
		pr_err("%s: %s: txfr error(%d)/resp_err(%d)\n",
				mmc_hostname(mrq->host), __func__, err,
				cmdq_req->resp_err);
		if (test_bit(CMDQ_STATE_ERR, &ctx_info->curr_state)) {
			pr_err("%s: CQ in error state, ending current req: %d\n",
				__func__, err);
		} else {
			set_bit(CMDQ_STATE_ERR, &ctx_info->curr_state);
			BUG_ON(host->err_mrq != NULL);
			host->err_mrq = mrq;
			schedule_work(&mq->cmdq_err_work);
		}
		goto out;
	}
	/*
	 * In case of error CMDQ is expected to be either in halted
	 * or disable state so cannot receive any completion of
	 * other requests.
	 */
	BUG_ON(test_bit(CMDQ_STATE_ERR, &ctx_info->curr_state));

	/* clear pending request */
	BUG_ON(!test_and_clear_bit(cmdq_req->tag,
				   &ctx_info->active_reqs));
	if (cmdq_req->cmdq_req_flags & DCMD)
		is_dcmd = true;
	else
		BUG_ON(!test_and_clear_bit(cmdq_req->tag,
					 &ctx_info->data_active_reqs));
	if (!is_dcmd)
		mmc_cmdq_post_req(host, cmdq_req->tag, err);
	if (cmdq_req->cmdq_req_flags & DCMD) {
		clear_bit(CMDQ_STATE_DCMD_ACTIVE, &ctx_info->curr_state);
		blk_end_request_all(rq, err);
		goto out;
	}
	/*
	 * In case of error, cmdq_req->data.bytes_xfered is set to 0.
	 * If we call blk_end_request() with nr_bytes as 0 then the request
	 * never gets completed. So in case of error, to complete a request
	 * with error we should use blk_end_request_all().
	 */
	if (err && cmdq_req->skip_err_handling) {
		cmdq_req->skip_err_handling = false;
		blk_end_request_all(rq, err);
		goto out;
	}

	blk_end_request(rq, err, cmdq_req->data.bytes_xfered);

out:

	mmc_cmdq_clk_scaling_stop_busy(host, true, is_dcmd);
	if (!test_bit(CMDQ_STATE_ERR, &ctx_info->curr_state)) {
		mmc_host_clk_release(host);
		wake_up(&ctx_info->wait);
		mmc_put_card(host->card);
	}

	if (!ctx_info->active_reqs)
		wake_up_interruptible(&host->cmdq_ctx.queue_empty_wq);

	if (blk_queue_stopped(mq->queue) && !ctx_info->active_reqs)
		complete(&mq->cmdq_shutdown_complete);

	return;
}

/*
 * Complete reqs from block layer softirq context
 * Invoked in irq context
 */
void mmc_blk_cmdq_req_done(struct mmc_request *mrq)
{
	struct request *req = mrq->req;

	blk_complete_request(req);
}
EXPORT_SYMBOL(mmc_blk_cmdq_req_done);

static int mmc_blk_issue_rw_rq(struct mmc_queue *mq, struct request *rqc)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_blk_request *brq = &mq->mqrq_cur->brq;
	int ret = 1, disable_multi = 0, retry = 0, type, retune_retry_done = 0;
	enum mmc_blk_status status;
	struct mmc_queue_req *mq_rq;
	struct request *req = rqc;
	struct mmc_async_req *areq;
	const u8 packed_nr = 2;
	u8 reqs = 0;
	bool reset = false;
#ifdef CONFIG_MMC_SIMULATE_MAX_SPEED
	unsigned long waitfor = jiffies;
#endif

	if (!rqc && !mq->mqrq_prev->req)
		return 0;

	if (rqc)
		reqs = mmc_blk_prep_packed_list(mq, rqc);

	do {
		if (rqc) {
			/*
			 * When 4KB native sector is enabled, only 8 blocks
			 * multiple read or write is allowed
			 */
			if ((brq->data.blocks & 0x07) &&
			    (card->ext_csd.data_sector_size == 4096)) {
				pr_err("%s: Transfer size is not 4KB sector size aligned\n",
					req->rq_disk->disk_name);
				mq_rq = mq->mqrq_cur;
				goto cmd_abort;
			}

			if (reqs >= packed_nr)
				mmc_blk_packed_hdr_wrq_prep(mq->mqrq_cur,
							    card, mq);
			else
				mmc_blk_rw_rq_prep(mq->mqrq_cur, card, 0, mq);
			areq = &mq->mqrq_cur->mmc_active;
		} else
			areq = NULL;
		areq = mmc_start_req(card->host, areq, (int *) &status);
		if (!areq) {
			if (status == MMC_BLK_NEW_REQUEST)
				set_bit(MMC_QUEUE_NEW_REQUEST, &mq->flags);
			return 0;
		}

		mq_rq = container_of(areq, struct mmc_queue_req, mmc_active);
		brq = &mq_rq->brq;
		req = mq_rq->req;
		type = rq_data_dir(req) == READ ? MMC_BLK_READ : MMC_BLK_WRITE;
		mmc_queue_bounce_post(mq_rq);

		if (card->err_in_sdr104) {
			/*
			 * Data CRC/timeout errors will manifest as CMD/DATA
			 * ERR. But we'd like to retry these too.
			 * Moreover, no harm done if this fails too for multiple
			 * times, we anyway reduce the bus-speed and retry the
			 * same request.
			 * If that fails too, we don't override this status.
			 */
			if (status == MMC_BLK_ABORT ||
			    status == MMC_BLK_CMD_ERR ||
			    status == MMC_BLK_DATA_ERR ||
			    status == MMC_BLK_RETRY)
				/* reset on all of these errors and retry */
				reset = true;

			status = MMC_BLK_RETRY;
			card->err_in_sdr104 = false;
		}

		switch (status) {
		case MMC_BLK_SUCCESS:
		case MMC_BLK_PARTIAL:
			/*
			 * A block was successfully transferred.
			 */
			mmc_blk_reset_success(md, type);

			mmc_blk_simulate_delay(mq, rqc, waitfor);

			if (mmc_packed_cmd(mq_rq->cmd_type)) {
				ret = mmc_blk_end_packed_req(mq_rq);
				break;
			} else {
				ret = blk_end_request(req, 0,
						brq->data.bytes_xfered);
			}

			/*
			 * If the blk_end_request function returns non-zero even
			 * though all data has been transferred and no errors
			 * were returned by the host controller, it's a bug.
			 */
			if (status == MMC_BLK_SUCCESS && ret) {
				pr_err("%s BUG rq_tot %d d_xfer %d\n",
				       __func__, blk_rq_bytes(req),
				       brq->data.bytes_xfered);
				rqc = NULL;
				goto cmd_abort;
			}
			break;
		case MMC_BLK_CMD_ERR:
			ret = mmc_blk_cmd_err(md, card, brq, req, ret);
			if (mmc_blk_reset(md, card->host, type))
				goto cmd_abort;
			if (!ret)
				goto start_new_req;
			break;
		case MMC_BLK_RETRY:
			retune_retry_done = brq->retune_retry_done;
			if (retry++ < MMC_BLK_MAX_RETRIES) {
				break;
			} else if (reset) {
				reset = false;
				/*
				 * If we exhaust all the retries due to
				 * CRC/timeout errors in SDR140 mode with UHS SD
				 * cards, re-configure the card in SDR50
				 * bus-speed mode.
				 * All subsequent re-init of this card will be
				 * in SDR50 mode, unless it is removed and
				 * re-inserted. When new UHS SD cards are
				 * inserted, it may start at SDR104 mode if
				 * supported by the card.
				 */
				pr_err("%s: blocked SDR104, lower the bus-speed (SDR50 / DDR50)\n",
					req->rq_disk->disk_name);
				mmc_host_clear_sdr104(card->host);
				mmc_suspend_clk_scaling(card->host);
				mmc_blk_reset(md, card->host, type);
				/* SDR104 mode is blocked from now on */
				card->sdr104_blocked = true;
				/* retry 5 times again */
				retry = 0;
				break;
			}
			/* Fall through */
		case MMC_BLK_ABORT:
			if (!mmc_blk_reset(md, card->host, type) &&
				(retry++ < (MMC_BLK_MAX_RETRIES + 1)))
				break;
			goto cmd_abort;
		case MMC_BLK_DATA_ERR: {
			int err;

			err = mmc_blk_reset(md, card->host, type);
			if (!err)
				break;
			goto cmd_abort;
		}
		case MMC_BLK_ECC_ERR:
			if (brq->data.blocks > 1) {
				/* Redo read one sector at a time */
				pr_warn("%s: retrying using single block read\n",
					req->rq_disk->disk_name);
				disable_multi = 1;
				break;
			}
			/*
			 * After an error, we redo I/O one sector at a
			 * time, so we only reach here after trying to
			 * read a single sector.
			 */
			ret = blk_end_request(req, -EIO,
						brq->data.blksz);
			if (!ret)
				goto start_new_req;
			break;
		case MMC_BLK_NOMEDIUM:
			goto cmd_abort;
		default:
			pr_err("%s: Unhandled return value (%d)",
					req->rq_disk->disk_name, status);
			goto cmd_abort;
		}

		if (ret) {
			if (mmc_packed_cmd(mq_rq->cmd_type)) {
				if (!mq_rq->packed->retries)
					goto cmd_abort;
				mmc_blk_packed_hdr_wrq_prep(mq_rq, card, mq);
				mmc_start_req(card->host,
					      &mq_rq->mmc_active, NULL);
			} else {

				/*
				 * In case of a incomplete request
				 * prepare it again and resend.
				 */
				mmc_blk_rw_rq_prep(mq_rq, card,
						disable_multi, mq);
				mmc_start_req(card->host,
						&mq_rq->mmc_active, NULL);
			}
			mq_rq->brq.retune_retry_done = retune_retry_done;
		}
	} while (ret);

	return 1;

 cmd_abort:
	if (mmc_packed_cmd(mq_rq->cmd_type)) {
		mmc_blk_abort_packed_req(mq_rq);
	} else {
		if (mmc_card_removed(card))
			req->cmd_flags |= REQ_QUIET;
		while (ret)
			ret = blk_end_request(req, -EIO,
					blk_rq_cur_bytes(req));
	}

 start_new_req:
	if (rqc) {
		if (mmc_card_removed(card)) {
			rqc->cmd_flags |= REQ_QUIET;
			blk_end_request_all(rqc, -EIO);
		} else {
			/*
			 * If current request is packed, it needs to put back.
			 */
			if (mmc_packed_cmd(mq->mqrq_cur->cmd_type))
				mmc_blk_revert_packed_req(mq, mq->mqrq_cur);

			mmc_blk_rw_rq_prep(mq->mqrq_cur, card, 0, mq);
			mmc_start_req(card->host,
				      &mq->mqrq_cur->mmc_active, NULL);
		}
	}

	return 0;
}

static inline int mmc_blk_cmdq_part_switch(struct mmc_card *card,
				      struct mmc_blk_data *md)
{
	struct mmc_blk_data *main_md = mmc_get_drvdata(card);
	struct mmc_host *host = card->host;
	struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;
	u8 part_config = card->ext_csd.part_config;

	if ((main_md->part_curr == md->part_type) &&
	    (card->part_curr == md->part_type))
		return 0;

	WARN_ON(!((card->host->caps2 & MMC_CAP2_CMD_QUEUE) &&
		 card->ext_csd.cmdq_support &&
		 (md->flags & MMC_BLK_CMD_QUEUE)));

	if (!test_bit(CMDQ_STATE_HALT, &ctx->curr_state))
		WARN_ON(mmc_cmdq_halt(host, true));

	/* disable CQ mode in card */
	if (mmc_card_cmdq(card)) {
		WARN_ON(mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_CMDQ, 0,
				  card->ext_csd.generic_cmd6_time));
		mmc_card_clr_cmdq(card);
	}

	part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
	part_config |= md->part_type;

	WARN_ON(mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_PART_CONFIG, part_config,
			  card->ext_csd.part_time));

	card->ext_csd.part_config = part_config;
	card->part_curr = md->part_type;

	main_md->part_curr = md->part_type;

	WARN_ON(mmc_blk_cmdq_switch(card, md, true));
	WARN_ON(mmc_cmdq_halt(host, false));

	return 0;
}

static int mmc_blk_cmdq_issue_rq(struct mmc_queue *mq, struct request *req)
{
	int ret;
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	unsigned int cmd_flags = req ? req->cmd_flags : 0;

	mmc_get_card(card);

	if (!card->host->cmdq_ctx.active_reqs && mmc_card_doing_bkops(card)) {
		ret = mmc_cmdq_halt(card->host, true);
		if (ret)
			goto out;
		ret = mmc_stop_bkops(card);
		if (ret) {
			pr_err("%s: %s: mmc_stop_bkops failed %d\n",
					md->disk->disk_name, __func__, ret);
			goto out;
		}
		ret = mmc_cmdq_halt(card->host, false);
		if (ret)
			goto out;
	}

	ret = mmc_blk_cmdq_part_switch(card, md);
	if (ret) {
		pr_err("%s: %s: partition switch failed %d\n",
				md->disk->disk_name, __func__, ret);
		goto out;
	}

	if (req) {
		struct mmc_host *host = card->host;
		struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;

		if ((cmd_flags & (REQ_FLUSH | REQ_DISCARD)) &&
		    (card->quirks & MMC_QUIRK_CMDQ_EMPTY_BEFORE_DCMD) &&
		    ctx->active_small_sector_read_reqs) {
			ret = wait_event_interruptible(ctx->queue_empty_wq,
						      !ctx->active_reqs);
			if (ret) {
				pr_err("%s: failed while waiting for the CMDQ to be empty %s err (%d)\n",
					mmc_hostname(host),
					__func__, ret);
				BUG_ON(1);
			}
			/* clear the counter now */
			ctx->active_small_sector_read_reqs = 0;
			/*
			 * If there were small sector (less than 8 sectors) read
			 * operations in progress then we have to wait for the
			 * outstanding requests to finish and should also have
			 * atleast 6 microseconds delay before queuing the DCMD
			 * request.
			 */
			udelay(MMC_QUIRK_CMDQ_DELAY_BEFORE_DCMD);
		}

		if (cmd_flags & REQ_DISCARD) {
			if (cmd_flags & REQ_SECURE &&
			   !(card->quirks & MMC_QUIRK_SEC_ERASE_TRIM_BROKEN))
				ret = mmc_blk_cmdq_issue_secdiscard_rq(mq, req);
			else
				ret = mmc_blk_cmdq_issue_discard_rq(mq, req);
		} else if (cmd_flags & REQ_FLUSH) {
			ret = mmc_blk_cmdq_issue_flush_rq(mq, req);
		} else {
			ret = mmc_blk_cmdq_issue_rw_rq(mq, req);
		}
	}

	return ret;

out:
	if (req)
		blk_end_request_all(req, ret);
	mmc_put_card(card);

	return ret;
}

static int mmc_blk_issue_rq(struct mmc_queue *mq, struct request *req)
{
	int ret;
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_host *host = card->host;
	unsigned long flags;
	unsigned int cmd_flags = req ? req->cmd_flags : 0;
	int err;

	if (req && !mq->mqrq_prev->req) {
		/* claim host only for the first request */
		mmc_get_card(card);

		if (mmc_card_doing_bkops(host->card)) {
			ret = mmc_stop_bkops(host->card);
			if (ret)
				goto out;
		}
	}

	ret = mmc_blk_part_switch(card, md);

	if (ret) {
		err = mmc_blk_reset(md, card->host, MMC_BLK_PARTSWITCH);
		if (!err) {
			pr_err("%s: mmc_blk_reset(MMC_BLK_PARTSWITCH) succeeded.\n",
					mmc_hostname(host));
			mmc_blk_reset_success(md, MMC_BLK_PARTSWITCH);
		} else
			pr_err("%s: mmc_blk_reset(MMC_BLK_PARTSWITCH) failed.\n",
				mmc_hostname(host));

		if (req) {
			blk_end_request_all(req, -EIO);
		}
		ret = 0;
		goto out;
	}

	mmc_blk_write_packing_control(mq, req);

	clear_bit(MMC_QUEUE_NEW_REQUEST, &mq->flags);
	if (cmd_flags & REQ_DISCARD) {
		/* complete ongoing async transfer before issuing discard */
		if (card->host->areq)
			mmc_blk_issue_rw_rq(mq, NULL);
		if (cmd_flags & REQ_SECURE &&
			!(card->quirks & MMC_QUIRK_SEC_ERASE_TRIM_BROKEN))
			ret = mmc_blk_issue_secdiscard_rq(mq, req);
		else
			ret = mmc_blk_issue_discard_rq(mq, req);
	} else if (cmd_flags & (REQ_FLUSH | REQ_BARRIER)) {
		/* complete ongoing async transfer before issuing flush */
		if (card->host->areq)
			mmc_blk_issue_rw_rq(mq, NULL);
		ret = mmc_blk_issue_flush(mq, req);
	} else {
		if (!req && host->areq) {
			spin_lock_irqsave(&host->context_info.lock, flags);
			host->context_info.is_waiting_last_req = true;
			spin_unlock_irqrestore(&host->context_info.lock, flags);
		}
		ret = mmc_blk_issue_rw_rq(mq, req);
	}

out:
	if ((!req && !(test_bit(MMC_QUEUE_NEW_REQUEST, &mq->flags))) ||
	     (cmd_flags & MMC_REQ_SPECIAL_MASK))
		/*
		 * Release host when there are no more requests
		 * and after special request(discard, flush) is done.
		 * In case sepecial request, there is no reentry to
		 * the 'mmc_blk_issue_rq' with 'mqrq_prev->req'.
		 */
		mmc_put_card(card);
	return ret;
}

static inline int mmc_blk_readonly(struct mmc_card *card)
{
	return mmc_card_readonly(card) ||
	       !(card->csd.cmdclass & CCC_BLOCK_WRITE);
}

static struct mmc_blk_data *mmc_blk_alloc_req(struct mmc_card *card,
					      struct device *parent,
					      sector_t size,
					      bool default_ro,
					      const char *subname,
					      int area_type)
{
	struct mmc_blk_data *md;
	int devidx, ret;

	devidx = find_first_zero_bit(dev_use, max_devices);
	if (devidx >= max_devices)
		return ERR_PTR(-ENOSPC);
	__set_bit(devidx, dev_use);

	md = kzalloc(sizeof(struct mmc_blk_data), GFP_KERNEL);
	if (!md) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * !subname implies we are creating main mmc_blk_data that will be
	 * associated with mmc_card with dev_set_drvdata. Due to device
	 * partitions, devidx will not coincide with a per-physical card
	 * index anymore so we keep track of a name index.
	 */
	if (!subname) {
		md->name_idx = find_first_zero_bit(name_use, max_devices);
		__set_bit(md->name_idx, name_use);
	} else
		md->name_idx = ((struct mmc_blk_data *)
				dev_to_disk(parent)->private_data)->name_idx;

	md->area_type = area_type;

	/*
	 * Set the read-only status based on the supported commands
	 * and the write protect switch.
	 */
	md->read_only = mmc_blk_readonly(card);

	md->disk = alloc_disk(perdev_minors);
	if (md->disk == NULL) {
		ret = -ENOMEM;
		goto err_kfree;
	}

	spin_lock_init(&md->lock);
	INIT_LIST_HEAD(&md->part);
	md->usage = 1;

	ret = mmc_init_queue(&md->queue, card, NULL, subname, area_type);
	if (ret)
		goto err_putdisk;

	md->queue.issue_fn = mmc_blk_issue_rq;
	md->queue.data = md;

	md->disk->major	= MMC_BLOCK_MAJOR;
	md->disk->first_minor = devidx * perdev_minors;
	md->disk->fops = &mmc_bdops;
	md->disk->private_data = md;
	md->disk->queue = md->queue.queue;
	md->disk->driverfs_dev = parent;
	set_disk_ro(md->disk, md->read_only || default_ro);
	md->disk->flags = GENHD_FL_EXT_DEVT;
	if (area_type & (MMC_BLK_DATA_AREA_RPMB | MMC_BLK_DATA_AREA_BOOT))
		md->disk->flags |= GENHD_FL_NO_PART_SCAN;

	/*
	 * As discussed on lkml, GENHD_FL_REMOVABLE should:
	 *
	 * - be set for removable media with permanent block devices
	 * - be unset for removable block devices with permanent media
	 *
	 * Since MMC block devices clearly fall under the second
	 * case, we do not set GENHD_FL_REMOVABLE.  Userspace
	 * should use the block device creation/destruction hotplug
	 * messages to tell when the card is present.
	 */

	snprintf(md->disk->disk_name, sizeof(md->disk->disk_name),
		 "mmcblk%u%s", md->name_idx, subname ? subname : "");

	if (mmc_card_mmc(card))
		blk_queue_logical_block_size(md->queue.queue,
					     card->ext_csd.data_sector_size);
	else
		blk_queue_logical_block_size(md->queue.queue, 512);

	set_capacity(md->disk, size);

	if (mmc_host_cmd23(card->host)) {
		if (mmc_card_mmc(card) ||
		    (mmc_card_sd(card) &&
		     card->scr.cmds & SD_SCR_CMD23_SUPPORT))
			md->flags |= MMC_BLK_CMD23;
	}

	if (mmc_card_mmc(card) &&
	    md->flags & MMC_BLK_CMD23 &&
	    ((card->ext_csd.rel_param & EXT_CSD_WR_REL_PARAM_EN) ||
	     card->ext_csd.rel_sectors)) {
		md->flags |= MMC_BLK_REL_WR;
		blk_queue_flush(md->queue.queue, REQ_FLUSH | REQ_FUA);
	}

	if (card->cmdq_init) {
		md->flags |= MMC_BLK_CMD_QUEUE;
		md->queue.cmdq_complete_fn = mmc_blk_cmdq_complete_rq;
		md->queue.cmdq_issue_fn = mmc_blk_cmdq_issue_rq;
		md->queue.cmdq_error_fn = mmc_blk_cmdq_err;
		md->queue.cmdq_req_timed_out = mmc_blk_cmdq_req_timed_out;
		md->queue.cmdq_shutdown = mmc_blk_cmdq_shutdown;
	}

	if (mmc_card_mmc(card) && !card->cmdq_init &&
	    (area_type == MMC_BLK_DATA_AREA_MAIN) &&
	    (md->flags & MMC_BLK_CMD23) &&
	    card->ext_csd.packed_event_en) {
		if (!mmc_packed_init(&md->queue, card))
			md->flags |= MMC_BLK_PACKED_CMD;
	}

	return md;

 err_putdisk:
	put_disk(md->disk);
 err_kfree:
	if (!subname)
		__clear_bit(md->name_idx, name_use);
	kfree(md);
 out:
	__clear_bit(devidx, dev_use);
	return ERR_PTR(ret);
}

static struct mmc_blk_data *mmc_blk_alloc(struct mmc_card *card)
{
	sector_t size;

	if (!mmc_card_sd(card) && mmc_card_blockaddr(card)) {
		/*
		 * The EXT_CSD sector count is in number or 512 byte
		 * sectors.
		 */
		size = card->ext_csd.sectors;
	} else {
		/*
		 * The CSD capacity field is in units of read_blkbits.
		 * set_capacity takes units of 512 bytes.
		 */
		size = (typeof(sector_t))card->csd.capacity
			<< (card->csd.read_blkbits - 9);
	}

	return mmc_blk_alloc_req(card, &card->dev, size, false, NULL,
					MMC_BLK_DATA_AREA_MAIN);
}

static int mmc_blk_alloc_part(struct mmc_card *card,
			      struct mmc_blk_data *md,
			      unsigned int part_type,
			      sector_t size,
			      bool default_ro,
			      const char *subname,
			      int area_type)
{
	char cap_str[10];
	struct mmc_blk_data *part_md;

	part_md = mmc_blk_alloc_req(card, disk_to_dev(md->disk), size, default_ro,
				    subname, area_type);
	if (IS_ERR(part_md))
		return PTR_ERR(part_md);
	part_md->part_type = part_type;
	list_add(&part_md->part, &md->part);

	string_get_size((u64)get_capacity(part_md->disk), 512, STRING_UNITS_2,
			cap_str, sizeof(cap_str));
	pr_info("%s: %s %s partition %u %s\n",
	       part_md->disk->disk_name, mmc_card_id(card),
	       mmc_card_name(card), part_md->part_type, cap_str);
	return 0;
}

/* MMC Physical partitions consist of two boot partitions and
 * up to four general purpose partitions.
 * For each partition enabled in EXT_CSD a block device will be allocatedi
 * to provide access to the partition.
 */

static int mmc_blk_alloc_parts(struct mmc_card *card, struct mmc_blk_data *md)
{
	int idx, ret = 0;

	if (!mmc_card_mmc(card))
		return 0;

	for (idx = 0; idx < card->nr_parts; idx++) {
		if (card->part[idx].size) {
			ret = mmc_blk_alloc_part(card, md,
				card->part[idx].part_cfg,
				card->part[idx].size >> 9,
				card->part[idx].force_ro,
				card->part[idx].name,
				card->part[idx].area_type);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static void mmc_blk_remove_req(struct mmc_blk_data *md)
{
	struct mmc_card *card;

	if (md) {
		/*
		 * Flush remaining requests and free queues. It
		 * is freeing the queue that stops new requests
		 * from being accepted.
		 */
		card = md->queue.card;
		mmc_cleanup_queue(&md->queue);
		if (md->flags & MMC_BLK_PACKED_CMD)
			mmc_packed_clean(&md->queue);
		if (md->flags & MMC_BLK_CMD_QUEUE)
			mmc_cmdq_clean(&md->queue, card);
		device_remove_file(disk_to_dev(md->disk),
				   &md->num_wr_reqs_to_start_packing);
		if (md->disk->flags & GENHD_FL_UP) {
			device_remove_file(disk_to_dev(md->disk), &md->force_ro);
			if ((md->area_type & MMC_BLK_DATA_AREA_BOOT) &&
					card->ext_csd.boot_ro_lockable)
				device_remove_file(disk_to_dev(md->disk),
					&md->power_ro_lock);
#ifdef CONFIG_MMC_SIMULATE_MAX_SPEED
			device_remove_file(disk_to_dev(md->disk),
						&dev_attr_max_write_speed);
			device_remove_file(disk_to_dev(md->disk),
						&dev_attr_max_read_speed);
			device_remove_file(disk_to_dev(md->disk),
						&dev_attr_cache_size);
#endif

			del_gendisk(md->disk);
		}
		mmc_blk_put(md);
	}
}

static void mmc_blk_remove_parts(struct mmc_card *card,
				 struct mmc_blk_data *md)
{
	struct list_head *pos, *q;
	struct mmc_blk_data *part_md;

	__clear_bit(md->name_idx, name_use);
	list_for_each_safe(pos, q, &md->part) {
		part_md = list_entry(pos, struct mmc_blk_data, part);
		list_del(pos);
		mmc_blk_remove_req(part_md);
	}
}

static int mmc_add_disk(struct mmc_blk_data *md)
{
	int ret;
	struct mmc_card *card = md->queue.card;

	add_disk(md->disk);
	md->force_ro.show = force_ro_show;
	md->force_ro.store = force_ro_store;
	sysfs_attr_init(&md->force_ro.attr);
	md->force_ro.attr.name = "force_ro";
	md->force_ro.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(disk_to_dev(md->disk), &md->force_ro);
	if (ret)
		goto force_ro_fail;
#ifdef CONFIG_MMC_SIMULATE_MAX_SPEED
	atomic_set(&md->queue.max_write_speed, max_write_speed);
	ret = device_create_file(disk_to_dev(md->disk),
			&dev_attr_max_write_speed);
	if (ret)
		goto max_write_speed_fail;
	atomic_set(&md->queue.max_read_speed, max_read_speed);
	ret = device_create_file(disk_to_dev(md->disk),
			&dev_attr_max_read_speed);
	if (ret)
		goto max_read_speed_fail;
	atomic_set(&md->queue.cache_size, cache_size);
	atomic_long_set(&md->queue.cache_used, 0);
	md->queue.cache_jiffies = jiffies;
	ret = device_create_file(disk_to_dev(md->disk), &dev_attr_cache_size);
	if (ret)
		goto cache_size_fail;
#endif

	if ((md->area_type & MMC_BLK_DATA_AREA_BOOT) &&
	     card->ext_csd.boot_ro_lockable) {
		umode_t mode;

		if (card->ext_csd.boot_ro_lock & EXT_CSD_BOOT_WP_B_PWR_WP_DIS)
			mode = S_IRUGO;
		else
			mode = S_IRUGO | S_IWUSR;

		md->power_ro_lock.show = power_ro_lock_show;
		md->power_ro_lock.store = power_ro_lock_store;
		sysfs_attr_init(&md->power_ro_lock.attr);
		md->power_ro_lock.attr.mode = mode;
		md->power_ro_lock.attr.name =
					"ro_lock_until_next_power_on";
		ret = device_create_file(disk_to_dev(md->disk),
				&md->power_ro_lock);
		if (ret)
			goto power_ro_lock_fail;
	}

	md->num_wr_reqs_to_start_packing.show =
		num_wr_reqs_to_start_packing_show;
	md->num_wr_reqs_to_start_packing.store =
		num_wr_reqs_to_start_packing_store;
	sysfs_attr_init(&md->num_wr_reqs_to_start_packing.attr);
	md->num_wr_reqs_to_start_packing.attr.name =
		"num_wr_reqs_to_start_packing";
	md->num_wr_reqs_to_start_packing.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(disk_to_dev(md->disk),
				 &md->num_wr_reqs_to_start_packing);
	if (ret)
		goto num_wr_reqs_to_start_packing_fail;

	md->no_pack_for_random.show = no_pack_for_random_show;
	md->no_pack_for_random.store = no_pack_for_random_store;
	sysfs_attr_init(&md->no_pack_for_random.attr);
	md->no_pack_for_random.attr.name = "no_pack_for_random";
	md->no_pack_for_random.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(disk_to_dev(md->disk),
				 &md->no_pack_for_random);
	if (ret)
		goto no_pack_for_random_fails;

	return ret;

no_pack_for_random_fails:
	device_remove_file(disk_to_dev(md->disk),
			   &md->num_wr_reqs_to_start_packing);
num_wr_reqs_to_start_packing_fail:
	device_remove_file(disk_to_dev(md->disk), &md->power_ro_lock);
power_ro_lock_fail:
#ifdef CONFIG_MMC_SIMULATE_MAX_SPEED
	device_remove_file(disk_to_dev(md->disk), &dev_attr_cache_size);
cache_size_fail:
	device_remove_file(disk_to_dev(md->disk), &dev_attr_max_read_speed);
max_read_speed_fail:
	device_remove_file(disk_to_dev(md->disk), &dev_attr_max_write_speed);
max_write_speed_fail:
#endif
	device_remove_file(disk_to_dev(md->disk), &md->force_ro);
force_ro_fail:
	del_gendisk(md->disk);

	return ret;
}

#define CID_MANFID_SANDISK	0x2
#define CID_MANFID_TOSHIBA	0x11
#define CID_MANFID_MICRON	0x13
#define CID_MANFID_SAMSUNG	0x15
#define CID_MANFID_KINGSTON	0x70

#define CID_MANFID_SANDISK	0x2
#define CID_MANFID_TOSHIBA	0x11
#define CID_MANFID_MICRON	0x13
#define CID_MANFID_SAMSUNG	0x15

static const struct mmc_fixup blk_fixups[] =
{
	MMC_FIXUP("SEM02G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM04G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM08G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM16G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM32G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),

	/*
	 * Some MMC cards experience performance degradation with CMD23
	 * instead of CMD12-bounded multiblock transfers. For now we'll
	 * black list what's bad...
	 * - Certain Toshiba cards.
	 *
	 * N.B. This doesn't affect SD cards.
	 */
	MMC_FIXUP("SDMB-32", CID_MANFID_SANDISK, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("SDM032", CID_MANFID_SANDISK, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC08G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC16G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC32G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP(CID_NAME_ANY, CID_MANFID_TOSHIBA, CID_OEMID_ANY,
		  add_quirk_mmc, MMC_QUIRK_CMDQ_EMPTY_BEFORE_DCMD),

	/*
	 * Some MMC cards need longer data read timeout than indicated in CSD.
	 */
	MMC_FIXUP(CID_NAME_ANY, CID_MANFID_MICRON, 0x200, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),
	MMC_FIXUP("008GE0", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),

	/*
	 * Some Samsung MMC cards need longer data read timeout than
	 * indicated in CSD.
	 */
	MMC_FIXUP("Q7XSAB", CID_MANFID_SAMSUNG, 0x100, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),

	/*
	 * Hynix eMMC cards need longer data read timeout than
	 * indicated in CSD.
	 */
	MMC_FIXUP(CID_NAME_ANY, CID_MANFID_HYNIX, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),

	/*
	 * On these Samsung MoviNAND parts, performing secure erase or
	 * secure trim can result in unrecoverable corruption due to a
	 * firmware bug.
	 */
	MMC_FIXUP("M8G2FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MAG4FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MBG8FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MCGAFA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VAL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("KYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VZL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),

	/*
	 *  On Some Kingston eMMCs, performing trim can result in
	 *  unrecoverable data conrruption occasionally due to a firmware bug.
	 */
	MMC_FIXUP("V10008", CID_MANFID_KINGSTON, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_TRIM_BROKEN),
	MMC_FIXUP("V10016", CID_MANFID_KINGSTON, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_TRIM_BROKEN),

	/* Some INAND MCP devices advertise incorrect timeout values */
	MMC_FIXUP("SEM04G", 0x45, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_INAND_DATA_TIMEOUT),

	/*
	 * On these Samsung MoviNAND parts, performing secure erase or
	 * secure trim can result in unrecoverable corruption due to a
	 * firmware bug.
	 */
	MMC_FIXUP("M8G2FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MAG4FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MBG8FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MCGAFA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VAL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("KYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VZL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),

	END_FIXUP
};

static int mmc_blk_probe(struct mmc_card *card)
{
	struct mmc_blk_data *md, *part_md;
	char cap_str[10];

	/*
	 * Check that the card supports the command class(es) we need.
	 */
	if (!(card->csd.cmdclass & CCC_BLOCK_READ))
		return -ENODEV;

	mmc_fixup_device(card, blk_fixups);

	md = mmc_blk_alloc(card);
	if (IS_ERR(md))
		return PTR_ERR(md);

	string_get_size((u64)get_capacity(md->disk), 512, STRING_UNITS_2,
			cap_str, sizeof(cap_str));
	pr_info("%s: %s %s %s %s\n",
		md->disk->disk_name, mmc_card_id(card), mmc_card_name(card),
		cap_str, md->read_only ? "(ro)" : "");

	if (mmc_blk_alloc_parts(card, md))
		goto out;

	dev_set_drvdata(&card->dev, md);

#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
	mmc_set_bus_resume_policy(card->host, 1);
#endif

	if (mmc_add_disk(md))
		goto out;

	list_for_each_entry(part_md, &md->part, part) {
		if (mmc_add_disk(part_md))
			goto out;
	}

	pm_runtime_use_autosuspend(&card->dev);
	pm_runtime_set_autosuspend_delay(&card->dev, MMC_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(&card->dev);

	/*
	 * Don't enable runtime PM for SD-combo cards here. Leave that
	 * decision to be taken during the SDIO init sequence instead.
	 */
	if (card->type != MMC_TYPE_SD_COMBO) {
		pm_runtime_set_active(&card->dev);
		pm_runtime_enable(&card->dev);
	}

	return 0;

 out:
	mmc_blk_remove_parts(card, md);
	mmc_blk_remove_req(md);
	return 0;
}

static void mmc_blk_remove(struct mmc_card *card)
{
	struct mmc_blk_data *md = dev_get_drvdata(&card->dev);

	mmc_blk_remove_parts(card, md);
	pm_runtime_get_sync(&card->dev);
	mmc_claim_host(card->host);
	mmc_blk_part_switch(card, md);
	mmc_release_host(card->host);
	if (card->type != MMC_TYPE_SD_COMBO)
		pm_runtime_disable(&card->dev);
	pm_runtime_put_noidle(&card->dev);
	mmc_blk_remove_req(md);
	dev_set_drvdata(&card->dev, NULL);
#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
	mmc_set_bus_resume_policy(card->host, 0);
#endif
}

static int _mmc_blk_suspend(struct mmc_card *card, bool wait)
{
	struct mmc_blk_data *part_md;
	struct mmc_blk_data *md = dev_get_drvdata(&card->dev);
	int rc = 0;

	if (md) {
		rc = mmc_queue_suspend(&md->queue, wait);
		if (rc)
			goto out;
		list_for_each_entry(part_md, &md->part, part) {
			rc = mmc_queue_suspend(&part_md->queue, wait);
			if (rc)
				goto out_resume;
		}
	}
	goto out;

 out_resume:
	mmc_queue_resume(&md->queue);
	list_for_each_entry(part_md, &md->part, part) {
		mmc_queue_resume(&part_md->queue);
	}
 out:
	return rc;
}

static void mmc_blk_shutdown(struct mmc_card *card)
{
	_mmc_blk_suspend(card, 1);
}

#ifdef CONFIG_PM_SLEEP
static int mmc_blk_suspend(struct device *dev)
{
	struct mmc_card *card = mmc_dev_to_card(dev);

	return _mmc_blk_suspend(card, 0);
}

static int mmc_blk_resume(struct device *dev)
{
	struct mmc_blk_data *part_md;
	struct mmc_blk_data *md = dev_get_drvdata(dev);

	if (md) {
		/*
		 * Resume involves the card going into idle state,
		 * so current partition is always the main one.
		 */
		md->part_curr = md->part_type;
		mmc_queue_resume(&md->queue);
		list_for_each_entry(part_md, &md->part, part) {
			mmc_queue_resume(&part_md->queue);
		}
	}
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mmc_blk_pm_ops, mmc_blk_suspend, mmc_blk_resume);

static struct mmc_driver mmc_driver = {
	.drv		= {
		.name	= "mmcblk",
		.pm	= &mmc_blk_pm_ops,
	},
	.probe		= mmc_blk_probe,
	.remove		= mmc_blk_remove,
	.shutdown	= mmc_blk_shutdown,
};

static int __init mmc_blk_init(void)
{
	int res;

	if (perdev_minors != CONFIG_MMC_BLOCK_MINORS)
		pr_info("mmcblk: using %d minors per device\n", perdev_minors);

	max_devices = min(MAX_DEVICES, (1 << MINORBITS) / perdev_minors);

	res = register_blkdev(MMC_BLOCK_MAJOR, "mmc");
	if (res)
		goto out;

	res = mmc_register_driver(&mmc_driver);
	if (res)
		goto out2;

	return 0;
 out2:
	unregister_blkdev(MMC_BLOCK_MAJOR, "mmc");
 out:
	return res;
}

static void __exit mmc_blk_exit(void)
{
	mmc_unregister_driver(&mmc_driver);
	unregister_blkdev(MMC_BLOCK_MAJOR, "mmc");
}

module_init(mmc_blk_init);
module_exit(mmc_blk_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Multimedia Card (MMC) block device driver");

