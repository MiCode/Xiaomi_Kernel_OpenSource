/*
 * Copyright (c) 2014 Ezequiel Garcia
 * Copyright (c) 2011 Free Electrons
 *
 * Driver parameter handling strongly based on drivers/mtd/ubi/build.c
 *   Copyright (c) International Business Machines Corp., 2006
 *   Copyright (c) Nokia Corporation, 2007
 *   Authors: Artem Bityutskiy, Frank Haverkamp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 */

/*
 * Read-only block devices on top of UBI volumes
 *
 * A simple implementation to allow a block device to be layered on top of a
 * UBI volume. The implementation is provided by creating a static 1-to-1
 * mapping between the block device and the UBI volume.
 *
 * The addressed byte is obtained from the addressed block sector, which is
 * mapped linearly into the corresponding LEB:
 *
 *   LEB number = addressed byte / LEB size
 *
 * This feature is compiled in the UBI core, and adds a 'block' parameter
 * to allow early creation of block devices on top of UBI volumes. Runtime
 * block creation/removal for UBI volumes is provided through two UBI ioctls:
 * UBI_IOCVOLCRBLK and UBI_IOCVOLRMBLK.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mtd/ubi.h>
#include <linux/workqueue.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <asm/div64.h>

#include "ubi/ubi-media.h"
#include "ubi/ubi.h"
#include "mt_ftl.h"

/* Maximum number of supported devices */
#define MT_FTL_BLK_MAX_DEVICES 32

/* Maximum length of the 'block=' parameter */
#define MT_FTL_BLK_PARAM_LEN 63

/* Maximum number of comma-separated items in the 'block=' parameter */
#define MT_FTL_BLK_PARAM_COUNT 2

struct mt_ftl_blk_param {
	int ubi_num;
	int vol_id;
	char name[MT_FTL_BLK_PARAM_LEN+1];
};

/* Numbers of elements set in the @mt_ftl_blk_param array */
static int mt_ftl_blk_devs __initdata;

/* MTD devices specification parameters */
static struct mt_ftl_blk_param mt_ftl_blk_param[MT_FTL_BLK_MAX_DEVICES] __initdata;

/* Linked list of all mt_ftl_blk instances */
static LIST_HEAD(mt_ftl_blk_devices);
static DEFINE_MUTEX(devices_mutex);
static int mt_ftl_blk_major;

static int __init mt_ftl_blk_set_param(const char *val,
				     const struct kernel_param *kp)
{
	int i, ret;
	size_t len;
	struct mt_ftl_blk_param *param;
	char buf[MT_FTL_BLK_PARAM_LEN];
	char *pbuf = &buf[0];
	char *tokens[MT_FTL_BLK_PARAM_COUNT];

	if (!val)
		return -EINVAL;

	len = strnlen(val, MT_FTL_BLK_PARAM_LEN);
	if (len == 0) {
		ubi_warn("block: empty 'block=' parameter - ignored\n");
		return 0;
	}

	if (len == MT_FTL_BLK_PARAM_LEN) {
		ubi_err("block: parameter \"%s\" is too long, max. is %d\n",
			val, MT_FTL_BLK_PARAM_LEN);
		return -EINVAL;
	}

	strcpy(buf, val);

	/* Get rid of the final newline */
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	for (i = 0; i < MT_FTL_BLK_PARAM_COUNT; i++)
		tokens[i] = strsep(&pbuf, ",");

	param = &mt_ftl_blk_param[mt_ftl_blk_devs];
	if (tokens[1]) {
		/* Two parameters: can be 'ubi, vol_id' or 'ubi, vol_name' */
		ret = kstrtoint(tokens[0], 10, &param->ubi_num);
		if (ret < 0)
			return -EINVAL;

		/* Second param can be a number or a name */
		ret = kstrtoint(tokens[1], 10, &param->vol_id);
		if (ret < 0) {
			param->vol_id = -1;
			strcpy(param->name, tokens[1]);
		}

	} else {
		/* One parameter: must be device path */
		strcpy(param->name, tokens[0]);
		param->ubi_num = -1;
		param->vol_id = -1;
	}

	mt_ftl_blk_devs++;

	return 0;
}

static struct kernel_param_ops mt_ftl_blk_param_ops = {
	.set    = mt_ftl_blk_set_param,
};
module_param_cb(block, &mt_ftl_blk_param_ops, NULL, 0);
MODULE_PARM_DESC(block, "Attach block devices to UBI volumes. Parameter format: block=<path|dev,num|dev,name>.\n"
			"Multiple \"block\" parameters may be specified.\n"
			"UBI volumes may be specified by their number, name, or path to the device node.\n"
			"Examples\n"
			"Using the UBI volume path:\n"
			"ubi.block=/dev/ubi0_0\n"
			"Using the UBI device, and the volume name:\n"
			"ubi.block=0,rootfs\n"
			"Using both UBI device number and UBI volume number:\n"
			"ubi.block=0,0\n");

static struct mt_ftl_blk *find_dev_nolock(int ubi_num, int vol_id)
{
	struct mt_ftl_blk *dev;

	list_for_each_entry(dev, &mt_ftl_blk_devices, list)
		if (dev->ubi_num == ubi_num && dev->vol_id == vol_id)
			return dev;
	return NULL;
}

static int mt_ftl_blk_flush(struct mt_ftl_blk *dev, bool sync)
{
	int ret = 0;

	/*if (dev->cache_state != STATE_DIRTY)
		return 0;*/

	/*
	 * TODO: mtdblock sets STATE_EMPTY, arguing that it prevents the
	 * underlying media to get changed without notice.
	 * I'm not fully convinced, so I just put STATE_CLEAN.
	 */
	/* dev->cache_state = STATE_CLEAN; */

	/* Atomically change leb with buffer contents */
	/*ret = ubi_leb_change(dev->desc, dev->cache_leb_num,
			     dev->cache, dev->leb_size);*/
	/*ret = mt_ftl_commit(dev->desc);
	if (ret) {
		dev_err(disk_to_dev(dev->gd), "mt_ftl_commit error %d\n", ret);
		return ret;
	}*/

	/* Sync ubi device when device is released and on block flush ioctl */
	if (sync)
		ret = ubi_sync(dev->ubi_num);

	return ret;
}

static int do_mt_ftl_blk_request(struct mt_ftl_blk *dev, struct request *req)
{
	int len, ret;
	sector_t sec;

	if (req->cmd_type != REQ_TYPE_FS)
		return -EIO;

	if (blk_rq_pos(req) + blk_rq_cur_sectors(req) >
	    get_capacity(req->rq_disk))
		return -EIO;

	sec = blk_rq_pos(req);
	len = blk_rq_cur_bytes(req);

	if ((req->bio->bi_rw & WRITE_SYNC) == WRITE_SYNC)
		dev->sync = 1;

	if (unlikely(req->cmd_flags & REQ_DISCARD)) {
		mt_ftl_err(dev, "REQ_DISCARD to mt_ftl");
		return mt_ftl_discard(dev, sec, len);
	}

	/*
	 * Let's prevent the device from being removed while we're doing I/O
	 * work. Notice that this means we serialize all the I/O operations,
	 * but it's probably of no impact given the NAND core serializes
	 * flash access anyway.
	 */
	/* mt_ftl_err(dev, "mt_ftl_blk : %s pos %d len %d\n",
	   (rq_data_dir(req)==READ)?"READ":"WRITE", (int)(sec << 9), len); */
	switch (rq_data_dir(req)) {
	case READ:
		ret = mt_ftl_read(dev, bio_data(req->bio), sec, len);
		break;
	case WRITE:
		ret = mt_ftl_write(dev, bio_data(req->bio), sec, len);
		break;
	default:
		mt_ftl_err(dev, "unknown request\n");
		return -EIO;
	}
	return ret;
}


static void mt_ftl_blk_do_work(struct work_struct *work)
{
	struct mt_ftl_blk *dev =
		container_of(work, struct mt_ftl_blk, work);
	struct request_queue *rq = dev->rq;
	struct request *req = NULL;

	spin_lock_irq(rq->queue_lock);
	req = blk_fetch_request(rq);
	while (req) {
		int res;

		spin_unlock_irq(rq->queue_lock);

		mutex_lock(&dev->dev_mutex);
		res = do_mt_ftl_blk_request(dev, req);
		if (res < 0) {
			ubi_err("[Bean]request error\n");
			break;
		}

		mutex_unlock(&dev->dev_mutex);

		spin_lock_irq(rq->queue_lock);

		if (!__blk_end_request_cur(req, res))
			req = blk_fetch_request(rq);
	}

	if (req)
		__blk_end_request_all(req, -EIO);

	spin_unlock_irq(rq->queue_lock);
}

static void mt_ftl_blk_request(struct request_queue *rq)
{
	struct mt_ftl_blk *dev;
	struct request *req;

	dev = rq->queuedata;

	if (!dev)
		while ((req = blk_fetch_request(rq)) != NULL)
			__blk_end_request_all(req, -ENODEV);
	else
		queue_work(dev->wq, &dev->work);
}

static int mt_ftl_blk_open(struct block_device *bdev, fmode_t mode)
{
	struct mt_ftl_blk *dev = bdev->bd_disk->private_data;
	int ret;

	mutex_lock(&dev->dev_mutex);
	if (dev->refcnt > 0) {
		/*
		 * The volume is already open, just increase the reference
		 * counter.
		 */
		goto out_done;
	}

	/*
	 * We want users to be aware they should only mount us as read-only.
	 * It's just a paranoid check, as write requests will get rejected
	 * in any case.
	 */
	if (mode & FMODE_WRITE)
		dev->desc = ubi_open_volume(dev->ubi_num, dev->vol_id, UBI_READWRITE);
	else
		dev->desc = ubi_open_volume(dev->ubi_num, dev->vol_id, UBI_READONLY);

	if (IS_ERR(dev->desc)) {
		ubi_err("%s failed to open ubi volume %d_%d",
			dev->gd->disk_name, dev->ubi_num, dev->vol_id);
		ret = PTR_ERR(dev->desc);
		dev->desc = NULL;
		goto out_unlock;
	}

	/* Allocate cache buffer, mtdblock uses vmalloc and we do too */
	dev->cache_leb_num = -1;
	dev->cache = vmalloc(dev->leb_size);
	if (!dev->cache)
		ret = -ENOMEM;

out_done:
	dev->refcnt++;
	mutex_unlock(&dev->dev_mutex);
	return 0;

out_unlock:
	mutex_unlock(&dev->dev_mutex);
	return ret;
}

static void mt_ftl_blk_release(struct gendisk *gd, fmode_t mode)
{
	struct mt_ftl_blk *dev = gd->private_data;

	mutex_lock(&dev->dev_mutex);
	dev->refcnt--;
	if (dev->refcnt == 0) {
		mt_ftl_blk_flush(dev, true);

		vfree(dev->cache);
		dev->cache_leb_num = -1;
		/* dev->cache_state = STATE_EMPTY; */
		ubi_close_volume(dev->desc);
		dev->desc = NULL;
	}
	mutex_unlock(&dev->dev_mutex);
}

static int mt_ftl_blk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	/* Some tools might require this information */
	geo->heads = 1;
	geo->cylinders = 1;
	geo->sectors = get_capacity(bdev->bd_disk);
	geo->start = 0;
	return 0;
}

static const struct block_device_operations mt_ftl_blk_ops = {
	.owner = THIS_MODULE,
	.open = mt_ftl_blk_open,
	.release = mt_ftl_blk_release,
	.getgeo	= mt_ftl_blk_getgeo,
};

int mt_ftl_blk_create(struct ubi_volume_desc *desc)
{
	struct mt_ftl_blk *dev;
	struct gendisk *gd;
	u64 disk_capacity;
	int ret;
	struct ubi_volume_info vi;

	ubi_get_volume_info(desc, &vi);

	disk_capacity = (vi.used_bytes >> 9) + (vi.used_bytes >> 11);

	if ((sector_t)disk_capacity != disk_capacity)
		return -EFBIG;
	/* Check that the volume isn't already handled */
	mutex_lock(&devices_mutex);
	if (find_dev_nolock(vi.ubi_num, vi.vol_id)) {
		mutex_unlock(&devices_mutex);
		return -EEXIST;
	}
	mutex_unlock(&devices_mutex);

	dev = kzalloc(sizeof(struct mt_ftl_blk), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mutex_init(&dev->dev_mutex);

	dev->ubi_num = vi.ubi_num;
	dev->vol_id = vi.vol_id;
	dev->leb_size = vi.usable_leb_size;

	/* Initialize the gendisk of this mt_ftl_blk device */
	gd = alloc_disk(1);
	if (!gd) {
		ubi_err("block: alloc_disk failed");
		ret = -ENODEV;
		goto out_free_dev;
	}

	gd->fops = &mt_ftl_blk_ops;
	gd->major = mt_ftl_blk_major;
	gd->first_minor = dev->ubi_num * UBI_MAX_VOLUMES + dev->vol_id;
	gd->private_data = dev;
	sprintf(gd->disk_name, "mt_ftl_blk%d_%d", dev->ubi_num, dev->vol_id);
	set_capacity(gd, disk_capacity);
	dev->gd = gd;

	spin_lock_init(&dev->queue_lock);
	dev->rq = blk_init_queue(mt_ftl_blk_request, &dev->queue_lock);
	if (!dev->rq) {
		ubi_err("block: blk_init_queue failed");
		ret = -ENODEV;
		goto out_put_disk;
	}

	dev->rq->queuedata = dev;
	dev->gd->queue = dev->rq;

	/*
	 * Create one workqueue per volume (per registered block device).
	 * Rembember workqueues are cheap, they're not threads.
	 */
	dev->wq = alloc_workqueue("%s", 0, 0, gd->disk_name);
	if (!dev->wq) {
		ret = -ENOMEM;
		goto out_free_queue;
	}
	INIT_WORK(&dev->work, mt_ftl_blk_do_work);

	mutex_lock(&devices_mutex);
	list_add_tail(&dev->list, &mt_ftl_blk_devices);
	mutex_unlock(&devices_mutex);

	/* Must be the last step: anyone can call file ops from now on */
	add_disk(dev->gd);

	dev->param = kzalloc(sizeof(struct mt_ftl_param), GFP_KERNEL);
	if (!dev->param)
		return -ENOMEM;

	dev->desc = desc;

	ret = mt_ftl_create(dev);
	if (ret)
		return -EFAULT;

	return 0;

out_free_queue:
	blk_cleanup_queue(dev->rq);
out_put_disk:
	put_disk(dev->gd);
out_free_dev:
	kfree(dev);

	return ret;
}

static void mt_ftl_blk_cleanup(struct mt_ftl_blk *dev)
{
	del_gendisk(dev->gd);
	blk_cleanup_queue(dev->rq);
	pr_notice("UBI: %s released", dev->gd->disk_name);
	put_disk(dev->gd);
}

int mt_ftl_blk_remove(struct ubi_volume_info *vi)
{
	struct mt_ftl_blk *dev;
	int ret;

	mutex_lock(&devices_mutex);
	dev = find_dev_nolock(vi->ubi_num, vi->vol_id);
	if (!dev) {
		mutex_unlock(&devices_mutex);
		return -ENODEV;
	}

	ret = mt_ftl_remove(dev);
	if (ret)
		return -EFAULT;

	/* Found a device, let's lock it so we can check if it's busy */
	mutex_lock(&dev->dev_mutex);
	if (dev->refcnt > 0) {
		mutex_unlock(&dev->dev_mutex);
		mutex_unlock(&devices_mutex);
		return -EBUSY;
	}

	/* Remove from device list */
	list_del(&dev->list);
	mutex_unlock(&devices_mutex);

	/* Flush pending work and stop this workqueue */
	destroy_workqueue(dev->wq);

	mt_ftl_blk_cleanup(dev);
	mutex_unlock(&dev->dev_mutex);
	kfree(dev);
	return 0;
}

static int mt_ftl_blk_resize(struct ubi_volume_info *vi)
{
	struct mt_ftl_blk *dev;
	u64 disk_capacity = (vi->used_bytes >> 9) + (vi->used_bytes >> 11);

	/*
	 * Need to lock the device list until we stop using the device,
	 * otherwise the device struct might get released in
	 * 'mt_ftl_blk_remove()'.
	 */
	mutex_lock(&devices_mutex);
	dev = find_dev_nolock(vi->ubi_num, vi->vol_id);
	if (!dev) {
		mutex_unlock(&devices_mutex);
		return -ENODEV;
	}
	if ((sector_t)disk_capacity != disk_capacity) {
		mutex_unlock(&devices_mutex);
		ubi_warn("%s: the volume is too big (%d LEBs), cannot resize",
			 dev->gd->disk_name, vi->size);
		return -EFBIG;
	}

	mutex_lock(&dev->dev_mutex);

	if (get_capacity(dev->gd) != disk_capacity) {
		set_capacity(dev->gd, disk_capacity);
		pr_notice("UBI[%d]: %s resized to %lld bytes", vi->ubi_num, dev->gd->disk_name,
			vi->used_bytes);
	}
	mutex_unlock(&dev->dev_mutex);
	mutex_unlock(&devices_mutex);
	return 0;
}

static int mt_ftl_blk_notify(struct notifier_block *nb,
			 unsigned long notification_type, void *ns_ptr)
{
	struct ubi_notification *nt = ns_ptr;

	switch (notification_type) {
	case UBI_VOLUME_ADDED:
		/*
		 * We want to enforce explicit block device creation for
		 * volumes, so when a volume is added we do nothing.
		 */
		break;
	case UBI_VOLUME_REMOVED:
		mt_ftl_blk_remove(&nt->vi);
		break;
	case UBI_VOLUME_RESIZED:
		mt_ftl_blk_resize(&nt->vi);
		break;
	case UBI_VOLUME_UPDATED:
		/*
		 * If the volume is static, a content update might mean the
		 * size (i.e. used_bytes) was also changed.
		 */
		if (nt->vi.vol_type == UBI_STATIC_VOLUME)
			mt_ftl_blk_resize(&nt->vi);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block mt_ftl_blk_notifier = {
	.notifier_call = mt_ftl_blk_notify,
};

static struct ubi_volume_desc * __init
open_volume_desc(const char *name, int ubi_num, int vol_id)
{
	if (ubi_num == -1)
		/* No ubi num, name must be a vol device path */
		return ubi_open_volume_path(name, UBI_READWRITE);
	else if (vol_id == -1)
		/* No vol_id, must be vol_name */
		return ubi_open_volume_nm(ubi_num, name, UBI_READWRITE);
	else
		return ubi_open_volume(ubi_num, vol_id, UBI_READWRITE);
}

static int __init mt_ftl_blk_create_from_param(void)
{
	int i, ret;
	struct mt_ftl_blk_param *p;
	struct ubi_volume_desc *desc;
	struct ubi_volume_info vi;

	for (i = 0; i < mt_ftl_blk_devs; i++) {
		p = &mt_ftl_blk_param[i];
		desc = open_volume_desc(p->name, p->ubi_num, p->vol_id);
		if (IS_ERR(desc)) {
			ubi_err("block: can't open volume, err=%ld\n",
				PTR_ERR(desc));
			ret = PTR_ERR(desc);
			break;
		}

		ubi_get_volume_info(desc, &vi);
		ret = mt_ftl_blk_create(desc);
		if (ret) {
			ubi_err("block: can't add '%s' volume, err=%d\n",
				vi.name, ret);
			break;
		}
		ubi_close_volume(desc);
	}
	return ret;
}

static void mt_ftl_blk_remove_all(void)
{
	struct mt_ftl_blk *next;
	struct mt_ftl_blk *dev;

	list_for_each_entry_safe(dev, next, &mt_ftl_blk_devices, list) {
		/* Flush pending work and stop workqueue */
		destroy_workqueue(dev->wq);
		/* The module is being forcefully removed */
		WARN_ON(dev->desc);
		/* Remove from device list */
		list_del(&dev->list);
		mt_ftl_blk_cleanup(dev);
		kfree(dev);
	}
}

int __init mt_ftl_blk_init(void)
{
	int ret;

	mt_ftl_blk_major = register_blkdev(0, "mt_ftl_blk");
	if (mt_ftl_blk_major < 0)
		return mt_ftl_blk_major;

	/* Attach block devices from 'block=' module param */
	ret = mt_ftl_blk_create_from_param();
	if (ret)
		goto err_remove;

	/*
	 * Block devices are only created upon user requests, so we ignore
	 * existing volumes.
	 */
	ret = ubi_register_volume_notifier(&mt_ftl_blk_notifier, 1);
	if (ret)
		goto err_unreg;
	return 0;

err_unreg:
	unregister_blkdev(mt_ftl_blk_major, "mt_ftl_blk");
err_remove:
	mt_ftl_blk_remove_all();
	return ret;
}

void __exit mt_ftl_blk_exit(void)
{
	ubi_unregister_volume_notifier(&mt_ftl_blk_notifier);
	mt_ftl_blk_remove_all();
	unregister_blkdev(mt_ftl_blk_major, "mt_ftl_blk");
}

module_init(mt_ftl_blk_init);
module_exit(mt_ftl_blk_exit);
