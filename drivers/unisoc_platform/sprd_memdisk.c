// SPDX-License-Identifier: GPL-2.0
/*
 * Memdisk drive for Unisoc.
 *
 * Copyright (C) 2022 Unisoc.
 * Author: Kun Wang <kun.wang1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kernel.h>	/* pr_notice() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>
#include <linux/ctype.h>
#include "../../../block/blk.h"

MODULE_LICENSE("Dual BSD/GPL");

static int memdisk_major;
module_param(memdisk_major, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512
#define MEMDISK_MINORS	16
struct memdisk_partition_info {
	unsigned long start;
	unsigned long size;	/* Device size in sectors */
	u8 *data;		/* The data array */
	const char *partition_name;
};
/*
 * The internal representation of our device.
 */
struct memdisk_dev {
	unsigned long size;	/* Device size in sectors */
	spinlock_t lock;	/* For mutual exclusion */
	struct gendisk *gd;	/* The gendisk structure */
	struct blk_mq_tag_set tag_set;
	struct memdisk_partition_info *memdiskp[];
};
static int memdisks_count;
static struct memdisk_dev *memdisks;

/*
 * Handle an I/O request.
 */
static void memdisk_transfer(struct memdisk_dev *dev, sector_t sector,
			     unsigned long nsect, char *buffer, int write)
{
	int i;
	struct memdisk_partition_info *memdiskp = NULL;
	unsigned long offset = sector * hardsect_size;
	unsigned long nbytes = nsect * hardsect_size;

	if ((offset + nbytes) > (dev->size * hardsect_size)) {
		pr_notice("memdisk: Beyond-end write (%ld %ld)\n",
			  offset, nbytes);
		return;
	}

	for (i = 0; i < memdisks_count; i++)
		if (sector >= memdisks->memdiskp[i]->start &&
			sector < memdisks->memdiskp[i]->start + memdisks->memdiskp[i]->size) {
			offset = (sector - memdisks->memdiskp[i]->start) * hardsect_size;
			nbytes = nsect * hardsect_size;
			memdiskp = dev->memdiskp[i];
			break;
		}

	if (i == memdisks_count)
		return;

	if (write)
		memcpy(memdiskp->data + offset, buffer, nbytes);
	else
		memcpy(buffer, memdiskp->data + offset, nbytes);
}

static blk_status_t memdisk_queue_rq(struct blk_mq_hw_ctx *hctx,
					const struct blk_mq_queue_data *bd)
{
	struct request *req = bd->rq;
	struct request_queue *q = hctx->queue;
	struct memdisk_dev *dev = q->queuedata;
	struct bio_vec bvec;
	struct req_iterator iter;
	sector_t sector = blk_rq_pos(req);

	blk_mq_start_request(req);
	spin_lock_irq(&memdisks->lock);

	if (blk_rq_is_passthrough(req)) {
		pr_notice("Skip non-CMD request/n");
		blk_mq_end_request(req, BLK_STS_IOERR);
		spin_unlock_irq(&memdisks->lock);
		return BLK_STS_IOERR;
	}

	rq_for_each_segment(bvec, req, iter) {
		char *buffer = kmap_atomic(bvec.bv_page) + bvec.bv_offset;
		unsigned int nsecs = bvec.bv_len >> SECTOR_SHIFT;

		memdisk_transfer(dev, sector, nsecs,
				buffer, rq_data_dir(req));
		sector += nsecs;
		kunmap_atomic(buffer);
	}

	blk_mq_end_request(req, BLK_STS_OK);
	spin_unlock_irq(&memdisks->lock);
	return BLK_STS_OK;
}

static const struct blk_mq_ops memdisk_mq_ops = {
	.queue_rq = memdisk_queue_rq,
};


/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), i
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int memdisk_getgeo(struct block_device *bd, struct hd_geometry *geo)
{
	long size;
	struct memdisk_dev *dev = bd->bd_disk->private_data;

	pr_notice("%s finished\n", __func__);
	size = dev->size * (hardsect_size / KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 4;

	return 0;
}

/*
 * The device operations structure.
 */
static const struct block_device_operations memdisk_ops = {
	.owner = THIS_MODULE,
	.getgeo = memdisk_getgeo
};

#if IS_ENABLED(CONFIG_SPRD_MEMDISK_PARTITION)
static void set_nr_sectors(struct block_device *bdev, sector_t sectors)
{
	spin_lock(&bdev->bd_size_lock);
	i_size_write(bdev->bd_inode, (loff_t)sectors << SECTOR_SHIFT);
	spin_unlock(&bdev->bd_size_lock);
}

static ssize_t whole_disk_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR_RO(whole_disk);

static struct block_device *sprd_add_partition(struct gendisk *disk, int partno,
				sector_t start, sector_t len, int flags,
				struct partition_meta_info *info)
{
	dev_t devt = MKDEV(0, 0);
	struct device *ddev = disk_to_dev(disk);
	struct device *pdev;
	struct block_device *bdev;
	const char *dname;
	int err;

	lockdep_assert_held(&disk->open_mutex);
	if (partno >= disk_max_parts(disk))
		return ERR_PTR(-EINVAL);
	/*
	 * Partitions are not supported on zoned block devices that are used as
	 * such.
	 */
	switch (disk->queue->limits.zoned) {
	case BLK_ZONED_HM:
		pr_warn("%s: partitions not supported on host managed zoned block device\n",
			disk->disk_name);
		return ERR_PTR(-ENXIO);
	case BLK_ZONED_HA:
		pr_info("%s: disabling host aware zoned block device support due to partitions\n",
			disk->disk_name);
		blk_queue_set_zoned(disk, BLK_ZONED_NONE);
		break;
	case BLK_ZONED_NONE:
		break;
	}
	if (xa_load(&disk->part_tbl, partno))
		return ERR_PTR(-EBUSY);
	/* ensure we always have a reference to the whole disk */
	get_device(disk_to_dev(disk));
	err = -ENOMEM;
	bdev = bdev_alloc(disk, partno);
	if (!bdev)
		goto out_put_disk;
	bdev->bd_start_sect = start;
	set_nr_sectors(bdev, len);
	pdev = &bdev->bd_device;
	dname = dev_name(ddev);
	if (isdigit(dname[strlen(dname) - 1]))
		dev_set_name(pdev, "%sp%d", dname, partno);
	else
		dev_set_name(pdev, "%s%d", dname, partno);
	device_initialize(pdev);
	pdev->class = &block_class;
	pdev->type = &part_type;
	pdev->parent = ddev;
	/* in consecutive minor range? */
	if (bdev->bd_partno < disk->minors) {
		devt = MKDEV(disk->major, disk->first_minor + bdev->bd_partno);
	} else {
		err = blk_alloc_ext_minor();
		if (err < 0)
			goto out_put;
		devt = MKDEV(BLOCK_EXT_MAJOR, err);
	}
	pdev->devt = devt;
	if (info) {
		err = -ENOMEM;
		bdev->bd_meta_info = kmemdup(info, sizeof(*info), GFP_KERNEL);
		if (!bdev->bd_meta_info)
			goto out_put;
	}
	/* delay uevent until 'holders' subdir is created */
	dev_set_uevent_suppress(pdev, 1);
	err = device_add(pdev);
	if (err)
		goto out_put;
	err = -ENOMEM;
	bdev->bd_holder_dir = kobject_create_and_add("holders", &pdev->kobj);
	if (!bdev->bd_holder_dir)
		goto out_del;
	dev_set_uevent_suppress(pdev, 0);
if (flags & ADDPART_FLAG_WHOLEDISK) {
	err = device_create_file(pdev, &dev_attr_whole_disk);
		if (err)
			goto out_del;
	}
	/* everything is up and running, commence */
	err = xa_insert(&disk->part_tbl, partno, bdev, GFP_KERNEL);
	if (err)
		goto out_del;
	bdev_add(bdev, devt);
	/* suppress uevent if the disk suppresses it */
	if (!dev_get_uevent_suppress(ddev))
		kobject_uevent(&pdev->kobj, KOBJ_ADD);
	return bdev;
out_del:
	kobject_put(bdev->bd_holder_dir);
	device_del(pdev);
out_put:
	put_device(pdev);
	return ERR_PTR(err);
out_put_disk:
	put_disk(disk);
	return ERR_PTR(err);
}
#endif

static void memdisk_setup_device(void)
{
	int i, ret;
	struct request_queue *queue;
#if IS_ENABLED(CONFIG_SPRD_MEMDISK_PARTITION)
	int temp;
	sector_t len;
	struct partition_meta_info info;
	struct block_device *part;
	sector_t start = 0;
#endif


	spin_lock_init(&memdisks->lock);
	ret = blk_mq_alloc_sq_tag_set(&memdisks->tag_set, &memdisk_mq_ops, 1,
						BLK_MQ_F_SHOULD_MERGE);
	if (ret) {
		pr_err("%s blk_mq_alloc_sq_tag_set failed\n", __func__);
		return;
	}

	memdisks->gd = blk_mq_alloc_disk(&memdisks->tag_set, memdisks);
	if (IS_ERR(memdisks->gd)) {
		pr_err("%s alloc_disk failure\n", __func__);
		return;
	}

	queue = memdisks->gd->queue;

	blk_queue_logical_block_size(queue, hardsect_size);
	memdisks->gd->major = memdisk_major;
	memdisks->gd->first_minor = 0;
	memdisks->gd->fops = &memdisk_ops;
	memdisks->gd->minors = MEMDISK_MINORS;
	memdisks->gd->private_data = memdisks;

#if IS_ENABLED(CONFIG_SPRD_MEMDISK_PARTITION)
	sprintf(memdisks->gd->disk_name,  "memdisk0");
#else
	sprintf(memdisks->gd->disk_name,  "memdisk0p1");
#endif
	for (i = 0; i < memdisks_count; i++)
		memdisks->size += memdisks->memdiskp[i]->size;

	set_capacity(memdisks->gd,
		     (sector_t)(memdisks->size * (hardsect_size / KERNEL_SECTOR_SIZE)));
	add_disk(memdisks->gd);

#if IS_ENABLED(CONFIG_SPRD_MEMDISK_PARTITION)
	for (i = 0; i < memdisks_count; i++) {
		sprintf(info.volname, "%s", memdisks->memdiskp[i]->partition_name);
		sprintf(info.uuid, "memdisk0.p%d", i);
		temp = hardsect_size / KERNEL_SECTOR_SIZE;
		len = (sector_t)(memdisks->memdiskp[i]->size * temp);
		start = memdisks->memdiskp[i]->start * (hardsect_size / KERNEL_SECTOR_SIZE);
		part = sprd_add_partition(memdisks->gd, i+1, start, len, ADDPART_FLAG_NONE, &info);
		if (IS_ERR(part)) {
			pr_err(" %s: p%d could not be added: %ld\n",
			       memdisks->gd->disk_name, i+1, -PTR_ERR(part));
			continue;
		}
	}
#endif

	pr_notice("%s: i=%d success.\n", __func__, i);
}

static void *memdisk_ram_vmap(phys_addr_t start, size_t size,
				 unsigned int memtype)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	if (memtype)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = pgprot_writecombine(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);

	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	vaddr = vm_map_ram(pages, page_count, -1);
	kfree(pages);

	return vaddr;
}

static int __init memdisk_init(void)
{
	int i = 0;
	int ret = 0;
	int initialized_node_num = 0;
	const char *name;
	struct resource res = { 0 };
	struct device_node *np = NULL;
	struct device_node *memnp = NULL;
	struct device_node *child = NULL;
	struct memdisk_partition_info *memdiskp = NULL;

	pr_notice("%s initiate\n", __func__);
	memdisk_major = register_blkdev(memdisk_major, "memdisk");
	if (memdisk_major <= 0) {
		pr_notice("%s memdisk unable to get major number\n", __func__);
		return -EBUSY;
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,memdisk");
	if (!np)
		return -ENODEV;

	for_each_child_of_node(np, child)
		memdisks_count++;

	memdisks = kzalloc(sizeof(struct memdisk_dev) +
		memdisks_count * sizeof(void *), GFP_KERNEL);
	if (!memdisks)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		memdiskp = kzalloc(sizeof(struct memdisk_partition_info),
			GFP_KERNEL);
		if (!memdiskp) {
			ret = -ENOMEM;
			goto err_1;
		}

		memnp = of_parse_phandle(child, "memory-region", 0);
		if (!memnp) {
			ret = -ENODEV;
			goto err_2;
		}

		ret = of_address_to_resource(memnp, 0, &res);
		if (ret != 0) {
			pr_notice("of_address_to_resource failed!\n");
			ret = -EINVAL;
			goto err_2;
		}


		ret = of_property_read_string(child, "label", &name);
		if (ret) {
			pr_notice("para lable failed!\n");
			ret = -EINVAL;
			goto err_2;
		}

#ifdef CONFIG_64BIT
		pr_notice("memdisk %d res start 0x%llx,end 0x%llx\n", i,
			  res.start, res.end);
#else
		pr_notice("memdisk %d res start 0x%x,end 0x%x\n", i,
			  res.start, res.end);
#endif
		memdiskp->data =
		    memdisk_ram_vmap(res.start, resource_size(&res), 0);
		if (!memdiskp->data) {
			pr_notice("sprd memdisk%d map error!\n", i);
			ret = -ENOMEM;
			goto err_2;
		}

		memdiskp->partition_name = name;
		memdiskp->size = resource_size(&res) / hardsect_size;
		memdiskp->start = (i == 0 ? 0 : memdisks->memdiskp[i-1]->start +
				memdisks->memdiskp[i-1]->size);
		memdisks->memdiskp[i] = memdiskp;

		i++;
		initialized_node_num++;
	}

	memdisk_setup_device();
	of_node_put(np);
	of_node_put(memnp);
	pr_notice("%s complete\n", __func__);

	return 0;

err_2:
	kfree(memdiskp);

err_1:
	for (i = 0; i < memdisks_count; i++)
		if (memdisks->memdiskp[i]->data)
			vm_unmap_ram(memdisks->memdiskp[i]->data,
				memdisks->memdiskp[i]->size * hardsect_size);

	for (i = 0; i < initialized_node_num; i++)
		kfree(memdisks->memdiskp[i]);

	kfree(memdisks);

	return ret;
}

static void __exit memdisk_exit(void)
{
	int i;

	for (i = 0; i < memdisks_count; i++)
		if (memdisks->memdiskp[i]->data)
			vm_unmap_ram(memdisks->memdiskp[i]->data,
				memdisks->memdiskp[i]->size * hardsect_size);

	if (memdisks->gd) {
		del_gendisk(memdisks->gd);
		put_disk(memdisks->gd);
	}
	unregister_blkdev(memdisk_major, "memdisk");

	for (i = 0; i < memdisks_count; i++)
		kfree(memdisks->memdiskp[i]);

	kfree(memdisks);
}

module_init(memdisk_init);
module_exit(memdisk_exit);
