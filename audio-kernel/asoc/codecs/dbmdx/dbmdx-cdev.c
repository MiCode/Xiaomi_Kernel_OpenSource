/*
 * DSPG DBMDX codec driver character device interface
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/delay.h>

#include "dbmdx-interface.h"

static struct dbmdx_private *dbmdx_p;

static atomic_t cdev_opened = ATOMIC_INIT(0);

/* Access to the audio buffer is controlled through "audio_owner". Either the
 * character device or the ALSA-capture device can be opened.
 */
static int dbmdx_record_open(struct inode *inode, struct file *file)
{
	file->private_data = dbmdx_p;

	if (!atomic_add_unless(&cdev_opened, 1, 1))
		return -EBUSY;

	return 0;
}

static int dbmdx_record_release(struct inode *inode, struct file *file)
{
	dbmdx_p->lock(dbmdx_p);
	dbmdx_p->va_flags.buffering = 0;
	dbmdx_p->unlock(dbmdx_p);

	flush_work(&dbmdx_p->sv_work);

	atomic_dec(&cdev_opened);

	return 0;
}

/*
 * read out of the kfifo as much as was requested or if requested more
 * as much as is in the FIFO
 */
static ssize_t dbmdx_record_read(struct file *file, char __user *buf,
				 size_t count_want, loff_t *f_pos)
{
	struct dbmdx_private *p = (struct dbmdx_private *)file->private_data;
	size_t not_copied;
	ssize_t to_copy = count_want;
	int avail;
	unsigned int copied;
	int ret;

	dev_dbg(p->dbmdx_dev, "%s: count_want:%zu f_pos:%lld\n",
			__func__, count_want, *f_pos);

	avail = kfifo_len(&p->detection_samples_kfifo);

	if (avail == 0)
		return 0;

	if (count_want > avail)
		to_copy = avail;

	ret = kfifo_to_user(&p->detection_samples_kfifo,
		buf, to_copy, &copied);
	if (ret)
		return -EIO;

	not_copied = count_want - copied;
	*f_pos = *f_pos + (count_want - not_copied);

	return count_want - not_copied;
}

static const struct file_operations dbmdx_cdev_fops = {
	.owner   = THIS_MODULE,
	.open    = dbmdx_record_open,
	.release = dbmdx_record_release,
	.read    = dbmdx_record_read,
};

/*
 * read out of the kfifo as much as was requested and block until all
 * data is available or a timeout occurs
 */
static ssize_t dbmdx_record_read_blocking(struct file *file, char __user *buf,
		size_t count_want, loff_t *f_pos)
{
	struct dbmdx_private *p = (struct dbmdx_private *)file->private_data;

	size_t not_copied;
	ssize_t to_copy = count_want;
	int avail;
	unsigned int copied, total_copied = 0;
	int ret;
	unsigned long timeout = jiffies + msecs_to_jiffies(500);

	dev_dbg(p->dbmdx_dev, "%s: count_want:%zu f_pos:%lld\n",
			__func__, count_want, *f_pos);

	avail = kfifo_len(&p->detection_samples_kfifo);

	while ((total_copied < count_want) &&
			time_before(jiffies, timeout) && avail) {
		to_copy = avail;
		if (count_want - total_copied < avail)
			to_copy = count_want - total_copied;

		ret = kfifo_to_user(&p->detection_samples_kfifo,
			buf + total_copied, to_copy, &copied);
		if (ret)
			return -EIO;

		total_copied += copied;

		avail = kfifo_len(&p->detection_samples_kfifo);
		if (avail == 0 && p->va_flags.buffering)
			usleep_range(100000, 110000);
	}

	if (avail && (total_copied < count_want))
		dev_err(p->dev, "dbmdx: timeout during reading\n");

	not_copied = count_want - total_copied;
	*f_pos = *f_pos + (count_want - not_copied);

	return count_want - not_copied;
}

static const struct file_operations dbmdx_cdev_block_fops = {
	.owner   = THIS_MODULE,
	.open    = dbmdx_record_open,
	.release = dbmdx_record_release,
	.read    = dbmdx_record_read_blocking,
};

int dbmdx_register_cdev(struct dbmdx_private *p)
{
	int ret;

	dbmdx_p = p;

	ret = alloc_chrdev_region(&p->record_chrdev, 0, 2, "dbmdx");
	if (ret) {
		dev_err(p->dbmdx_dev, "failed to allocate character device\n");
		return -EINVAL;
	}

	cdev_init(&p->record_cdev, &dbmdx_cdev_fops);
	cdev_init(&p->record_cdev_block, &dbmdx_cdev_block_fops);

	p->record_cdev.owner = THIS_MODULE;
	p->record_cdev_block.owner = THIS_MODULE;

	ret = cdev_add(&p->record_cdev, p->record_chrdev, 1);
	if (ret) {
		dev_err(p->dbmdx_dev, "failed to add character device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		return -EINVAL;
	}

	ret = cdev_add(&p->record_cdev_block, p->record_chrdev + 1, 1);
	if (ret) {
		dev_err(p->dbmdx_dev,
			"failed to add blocking character device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		return -EINVAL;
	}

	p->record_dev = device_create(p->ns_class, &platform_bus,
				      MKDEV(MAJOR(p->record_chrdev), 0),
				      p, "dbmdx-%d", 0);
	if (IS_ERR(p->record_dev)) {
		dev_err(p->dev, "could not create device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		cdev_del(&p->record_cdev);
		return -EINVAL;
	}

	p->record_dev_block = device_create(p->ns_class, &platform_bus,
					    MKDEV(MAJOR(p->record_chrdev), 1),
					    p, "dbmdx-%d", 1);
	if (IS_ERR(p->record_dev_block)) {
		dev_err(p->dev, "could not create device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		cdev_del(&p->record_cdev_block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(dbmdx_register_cdev);

void dbmdx_deregister_cdev(struct dbmdx_private *p)
{
	if (p->record_dev) {
		device_unregister(p->record_dev);
		cdev_del(&p->record_cdev);
	}
	if (p->record_dev_block) {
		device_unregister(p->record_dev_block);
		cdev_del(&p->record_cdev_block);
	}
	unregister_chrdev_region(p->record_chrdev, 2);

	dbmdx_p = NULL;
}
EXPORT_SYMBOL(dbmdx_deregister_cdev);
