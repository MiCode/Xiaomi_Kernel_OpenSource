/*
 * drivers/block/vs_block_client.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * block vservice client driver
 *
 * Function vs_block_client_vs_alloc() is partially derived from
 * drivers/block/brd.c (brd_alloc())
 *
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/version.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <vservices/buffer.h>
#include <vservices/protocol/block/types.h>
#include <vservices/protocol/block/common.h>
#include <vservices/protocol/block/client.h>
#include <vservices/service.h>
#include <vservices/session.h>
#include <vservices/wait.h>

/*
 * BLK_DEF_MAX_SECTORS was replaced with the hard-coded number 1024 in 3.19,
 * and restored in 4.3
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
        (LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0))
#define BLK_DEF_MAX_SECTORS 1024
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#define bio_sector(bio) (bio)->bi_iter.bi_sector
#define bio_size(bio) (bio)->bi_iter.bi_size
#else
#define bio_sector(bio) (bio)->bi_sector
#define bio_size(bio) (bio)->bi_size
#endif

#define CLIENT_BLKDEV_NAME		"vblock"

#define PERDEV_MINORS 256

struct block_client;

struct vs_block_device {
	/*
	 * The client that created this block device. A reference is held
	 * to the client until the block device is released, so this pointer
	 * should always be valid. However, the client may since have reset;
	 * so it should only be used if, after locking it, its blkdev pointer
	 * points back to this block device.
	 */
	struct block_client		*client;

	int				id;
	struct gendisk			*disk;
	struct request_queue		*queue;

	struct kref			kref;
};

struct block_client {
	struct vs_client_block_state	client;
	struct vs_service_device	*service;

	/* Tasklet & queue for bouncing buffers out of read acks */
	struct tasklet_struct		rx_tasklet;
	struct list_head		rx_queue;
	struct spinlock			rx_queue_lock;

	/*
	 * The current virtual block device. This gets replaced when we do
	 * a reset since other parts of the kernel (e.g. vfs) may still
	 * be accessing the disk.
	 */
	struct vs_block_device		*blkdev;

	/* Shared work item for disk creation */
	struct work_struct		disk_creation_work;

	struct kref			kref;
};

#define state_to_block_client(state) \
	container_of(state, struct block_client, client)

static int block_client_major;

/* Unique identifier allocation for virtual block devices */
static DEFINE_IDA(vs_block_ida);
static DEFINE_MUTEX(vs_block_ida_lock);

static int
block_client_vs_to_linux_error(vservice_block_block_io_error_t vs_err)
{
	switch (vs_err) {
	case VSERVICE_BLOCK_INVALID_INDEX:
		return -EILSEQ;
	case VSERVICE_BLOCK_MEDIA_FAILURE:
		return -EIO;
	case VSERVICE_BLOCK_MEDIA_TIMEOUT:
		return -ETIMEDOUT;
	case VSERVICE_BLOCK_UNSUPPORTED_COMMAND:
		return -ENOTSUPP;
	case VSERVICE_BLOCK_SERVICE_RESET:
		return -ENXIO;
	default:
		WARN_ON(vs_err);
		return 0;
	}

	return 0;
}

static void vs_block_client_kfree(struct kref *kref)
{
	struct block_client *client =
		container_of(kref, struct block_client, kref);

	vs_put_service(client->service);
	kfree(client);
}

static void vs_block_client_put(struct block_client *client)
{
	kref_put(&client->kref, vs_block_client_kfree);
}

static void vs_block_device_kfree(struct kref *kref)
{
	struct vs_block_device *blkdev =
		container_of(kref, struct vs_block_device, kref);

	/* Delete the disk and clean up its queue */
	del_gendisk(blkdev->disk);
	blk_cleanup_queue(blkdev->queue);
	put_disk(blkdev->disk);

	mutex_lock(&vs_block_ida_lock);
	ida_remove(&vs_block_ida, blkdev->id);
	mutex_unlock(&vs_block_ida_lock);

	if (blkdev->client)
		vs_block_client_put(blkdev->client);

	kfree(blkdev);
}

static void vs_block_device_put(struct vs_block_device *blkdev)
{
	kref_put(&blkdev->kref, vs_block_device_kfree);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
static void
#else
static int
#endif
vs_block_client_blkdev_release(struct gendisk *disk, fmode_t mode)
{
	struct vs_block_device *blkdev = disk->private_data;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	if (WARN_ON(!blkdev))
		return;
#else
	if (WARN_ON(!blkdev))
		return -ENXIO;
#endif

	vs_block_device_put(blkdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	return 0;
#endif
}

static int vs_block_client_blkdev_open(struct block_device *bdev, fmode_t mode)
{
	struct vs_block_device *blkdev = bdev->bd_disk->private_data;
	struct block_client *client;
	int err = -ENXIO;

	if (!blkdev || !kref_get_unless_zero(&blkdev->kref))
		goto fail_get_blkdev;

	client = blkdev->client;
	if (WARN_ON(!client))
		goto fail_lock_client;

	if (!vs_state_lock_safe(&client->client)) {
		err = -ENODEV;
		goto fail_lock_client;
	}

	if (blkdev != client->blkdev) {
		/* The client has reset, this blkdev is no longer usable */
		err = -ENXIO;
		goto fail_check_client;
	}

	if ((mode & FMODE_WRITE) > 0 && client->client.readonly) {
		dev_dbg(&client->service->dev,
			"opening a readonly disk as writable\n");
		err = -EROFS;
		goto fail_check_client;
	}

	vs_state_unlock(&client->client);

	return 0;

fail_check_client:
	vs_state_unlock(&client->client);
fail_lock_client:
	vs_block_device_put(blkdev);
fail_get_blkdev:
	return err;
}

static int vs_block_client_blkdev_getgeo(struct block_device *bdev,
		struct hd_geometry *geo)
{
	/* These numbers are some default sane values for disk geometry. */
	geo->cylinders = get_capacity(bdev->bd_disk) / (4 * 16);
	geo->heads = 4;
	geo->sectors = 16;

	return 0;
}

/*
 * Indirectly determine linux block layer sector size and ensure that our
 * sector size matches.
 */
static int vs_block_client_check_sector_size(struct block_client *client,
		struct bio *bio)
{
	if (unlikely(!bio_sectors(bio))) {
		dev_err(&client->service->dev, "zero-length bio");
		return -EIO;
	}

	if (unlikely(bio_size(bio) % client->client.sector_size)) {
		dev_err(&client->service->dev,
		"bio has %zd bytes, unexpected for sector_size of %zd bytes",
		(size_t)bio_size(bio),
		(size_t)client->client.sector_size);
		return -EIO;
	}

	return 0;
}

static const struct block_device_operations block_client_ops = {
	.getgeo		= vs_block_client_blkdev_getgeo,
	.open		= vs_block_client_blkdev_open,
	.release	= vs_block_client_blkdev_release,
	.owner		= THIS_MODULE,
};

static int block_client_send_write_req(struct block_client *client,
		struct bio *bio)
{
	struct vs_client_block_state *state = &client->client;
	struct vs_mbuf *mbuf;
	struct vs_pbuf pbuf;
	struct bio_vec *bvec;
	int err;
	bool flush, nodelay, commit;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	struct bvec_iter iter;
	struct bio_vec bvec_local;
#else
	int i;
#endif

	err = vs_block_client_check_sector_size(client, bio);
	if (err < 0)
		goto fail;

	do {
		/* Wait until it's possible to send a write request */
		err = vs_wait_state_nointr(state,
				vs_client_block_io_req_write_can_send(state));
		if (err == -ECANCELED)
			err = -ENXIO;
		if (err < 0)
			goto fail;

		/* Wait for quota, while sending a write remains possible */
		mbuf = vs_wait_alloc_nointr(state,
				vs_client_block_io_req_write_can_send(state),
				vs_client_block_io_alloc_req_write(
					state, &pbuf, GFP_KERNEL));
		err = IS_ERR(mbuf) ? PTR_ERR(mbuf) : 0;

		/* Retry if sending is no longer possible */
	} while (err == -ECANCELED);

	if (err < 0)
		goto fail;

	vs_pbuf_resize(&pbuf, 0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	bvec = &bvec_local;
	bio_for_each_segment(bvec_local, bio, iter)
#else
	bio_for_each_segment(bvec, bio, i)
#endif
	{
		unsigned long flags;
		void *buf = bvec_kmap_irq(bvec, &flags);
		flush_kernel_dcache_page(bvec->bv_page);
		err = vs_pbuf_append(&pbuf, buf, bvec->bv_len);
		bvec_kunmap_irq(buf, &flags);
		if (err < 0) {
			dev_err(&client->service->dev,
				"pbuf copy failed with err %d\n", err);
			err = -EIO;
			goto fail_free_write;
		}
	}

	if (unlikely(vs_pbuf_size(&pbuf) != bio_size(bio))) {
		dev_err(&client->service->dev,
			"pbuf size is wrong: %zd, should be %zd\n",
			vs_pbuf_size(&pbuf), (size_t)bio_size(bio));
		err = -EIO;
		goto fail_free_write;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
	flush = (bio_flags(bio) & REQ_PREFLUSH);
	commit = (bio_flags(bio) & REQ_FUA);
	nodelay = (bio_flags(bio) & REQ_SYNC);
#else
	flush = (bio->bi_rw & REQ_FLUSH);
	commit = (bio->bi_rw & REQ_FUA);
	nodelay = (bio->bi_rw & REQ_SYNC);
#endif
	err = vs_client_block_io_req_write(state, bio, bio_sector(bio),
			bio_sectors(bio), nodelay, flush, commit, pbuf, mbuf);

	if (err) {
		dev_err(&client->service->dev,
				"write req failed with err %d\n", err);
		goto fail_free_write;
	}

	return 0;

fail_free_write:
	vs_client_block_io_free_req_write(state, &pbuf, mbuf);
fail:
	return err;
}

static int block_client_send_read_req(struct block_client *client,
		struct bio *bio)
{
	struct vs_client_block_state *state = &client->client;
	int err;
	bool flush, nodelay;

	err = vs_block_client_check_sector_size(client, bio);
	if (err < 0)
		return err;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
	flush = (bio_flags(bio) & REQ_PREFLUSH);
	nodelay = (bio_flags(bio) & REQ_SYNC);
#else
	flush = (bio->bi_rw & REQ_FLUSH);
	nodelay = (bio->bi_rw & REQ_SYNC);
#endif
	do {
		/* Wait until it's possible to send a read request */
		err = vs_wait_state_nointr(state,
				vs_client_block_io_req_read_can_send(state));
		if (err == -ECANCELED)
			err = -ENXIO;
		if (err < 0)
			break;

		/* Wait for quota, while sending a read remains possible */
		err = vs_wait_send_nointr(state,
			vs_client_block_io_req_read_can_send(state),
			vs_client_block_io_req_read(state, bio,
				bio_sector(bio), bio_sectors(bio),
				nodelay, flush, GFP_KERNEL));
	} while (err == -ECANCELED);

	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static blk_qc_t
#else
static void
#endif
vs_block_client_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	struct vs_block_device *blkdev = bdev->bd_disk->private_data;
	struct block_client *client;
	int err = 0;

	client = blkdev->client;
	if (!client || !kref_get_unless_zero(&client->kref)) {
		err = -ENODEV;
		goto fail_get_client;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
	blk_queue_split(q, &bio, q->bio_split);
#endif

	if (!vs_state_lock_safe(&client->client)) {
		err = -ENODEV;
		goto fail_lock_client;
	}

	if (client->blkdev != blkdev) {
		/* Client has reset, this block device is no longer usable */
		err = -EIO;
		goto fail_check_client;
	}

	if (bio_data_dir(bio) == WRITE)
		err = block_client_send_write_req(client, bio);
	else
		err = block_client_send_read_req(client, bio);

fail_check_client:
	if (err == -ENOLINK)
		err = -EIO;
	else
		vs_state_unlock(&client->client);
fail_lock_client:
	vs_block_client_put(client);
fail_get_client:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
	if (err < 0) {
		bio->bi_error = err;
		bio_endio(bio);
	}
#else
	if (err < 0)
		bio_endio(bio, err);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	return BLK_QC_T_NONE;
#endif
}

static int vs_block_client_get_blkdev_id(struct block_client *client)
{
	int id;
	int ret;

retry:
	ret = ida_pre_get(&vs_block_ida, GFP_KERNEL);
	if (ret == 0)
		return -ENOMEM;

	mutex_lock(&vs_block_ida_lock);
	ret = ida_get_new(&vs_block_ida, &id);
	mutex_unlock(&vs_block_ida_lock);

	if (ret == -EAGAIN)
		goto retry;

	return id;
}

static int vs_block_client_disk_add(struct block_client *client)
{
	struct vs_block_device *blkdev;
	unsigned int max_hw_sectors;
	int err;

	dev_dbg(&client->service->dev, "device add\n");

	blkdev = kzalloc(sizeof(*blkdev), GFP_KERNEL);
	if (!blkdev) {
		err = -ENOMEM;
		goto fail;
	}

	kref_init(&blkdev->kref);
	blkdev->id = vs_block_client_get_blkdev_id(client);
	if (blkdev->id < 0) {
		err = blkdev->id;
		goto fail_free_blkdev;
	}

	if ((blkdev->id * PERDEV_MINORS) >> MINORBITS) {
		err = -ENODEV;
		goto fail_remove_ida;
	}

	blkdev->queue = blk_alloc_queue(GFP_KERNEL);
	if (!blkdev->queue) {
		dev_err(&client->service->dev,
				"Error initializing blk queue\n");
		err = -ENOMEM;
		goto fail_remove_ida;
	}

	blk_queue_make_request(blkdev->queue, vs_block_client_make_request);
	blk_queue_bounce_limit(blkdev->queue, BLK_BOUNCE_ANY);
	blk_queue_dma_alignment(blkdev->queue, 0);

	/*
	 * Mark this as a paravirtualised device. This is just an alias
	 * of QUEUE_FLAG_NONROT, which prevents the I/O schedulers trying
	 * to wait for the disk to spin.
	 */
	queue_flag_set_unlocked(QUEUE_FLAG_VIRT, blkdev->queue);

	blkdev->queue->queuedata = blkdev;

	blkdev->client = client;
	kref_get(&client->kref);

	max_hw_sectors = min_t(sector_t, BLK_DEF_MAX_SECTORS,
			client->client.segment_size /
			client->client.sector_size);
	blk_queue_max_hw_sectors(blkdev->queue, max_hw_sectors);
	blk_queue_logical_block_size(blkdev->queue,
		client->client.sector_size);
	blk_queue_physical_block_size(blkdev->queue,
		client->client.sector_size);

	blkdev->disk = alloc_disk(PERDEV_MINORS);
	if (!blkdev->disk) {
		dev_err(&client->service->dev, "Error allocating disk\n");
		err = -ENOMEM;
		goto fail_free_blk_queue;
	}

	if (client->client.readonly) {
		dev_dbg(&client->service->dev, "set device as readonly\n");
		set_disk_ro(blkdev->disk, true);
	}

	blkdev->disk->major = block_client_major;
	blkdev->disk->first_minor = blkdev->id * PERDEV_MINORS;
	blkdev->disk->fops         = &block_client_ops;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
	blkdev->disk->driverfs_dev = &client->service->dev;
#endif
	blkdev->disk->private_data = blkdev;
	blkdev->disk->queue        = blkdev->queue;
	blkdev->disk->flags       |= GENHD_FL_EXT_DEVT;

	/*
	 * The block device name is vblock<x>, where x is a unique
	 * identifier. Userspace should rename or symlink the device
	 * appropriately, typically by processing the add uevent.
	 *
	 * If a virtual block device is reset then it may re-open with a
	 * different identifier if something still holds a reference to
	 * the old device (such as a userspace application having an open
	 * file handle).
	 */
	snprintf(blkdev->disk->disk_name, sizeof(blkdev->disk->disk_name),
			"%s%d", CLIENT_BLKDEV_NAME, blkdev->id);
	set_capacity(blkdev->disk, client->client.device_sectors *
		(client->client.sector_size >> 9));

	/*
	 * We need to hold a reference on blkdev across add_disk(), to make
	 * sure a concurrent reset does not immediately release the blkdev
	 * and call del_gendisk().
	 */
	kref_get(&blkdev->kref);

	vs_service_state_lock(client->service);
	if (!VSERVICE_BASE_STATE_IS_RUNNING(client->client.state.base)) {
		vs_service_state_unlock(client->service);
		err = -ENXIO;
		goto fail_free_blk_queue;
	}
	client->blkdev = blkdev;
	vs_service_state_unlock(client->service);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
	device_add_disk(&client->service->dev, blkdev->disk);
#else
	add_disk(blkdev->disk);
#endif
	dev_dbg(&client->service->dev, "added block disk '%s'\n",
			blkdev->disk->disk_name);

	/* Release the reference taken above. */
	vs_block_device_put(blkdev);

	return 0;

fail_free_blk_queue:
	blk_cleanup_queue(blkdev->queue);
fail_remove_ida:
	mutex_lock(&vs_block_ida_lock);
	ida_remove(&vs_block_ida, blkdev->id);
	mutex_unlock(&vs_block_ida_lock);
fail_free_blkdev:
	kfree(blkdev);
fail:
	return err;
}

static void vs_block_client_disk_creation_work(struct work_struct *work)
{
	struct block_client *client = container_of(work,
			struct block_client, disk_creation_work);
	struct vs_block_device *blkdev;
	bool running;

	vs_service_state_lock(client->service);
	blkdev = client->blkdev;
	running = VSERVICE_BASE_STATE_IS_RUNNING(client->client.state.base);

	dev_dbg(&client->service->dev,
			"disk changed: blkdev = %pK, running = %d\n",
			client->blkdev, running);
	if (!blkdev && running) {
		dev_dbg(&client->service->dev, "adding block disk\n");
		vs_service_state_unlock(client->service);
		vs_block_client_disk_add(client);
	} else {
		vs_service_state_unlock(client->service);
	}
}

static void vs_block_client_rx_tasklet(unsigned long data);

static struct vs_client_block_state *
vs_block_client_alloc(struct vs_service_device *service)
{
	struct block_client *client;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		dev_err(&service->dev, "Error allocating client struct\n");
		return NULL;
	}

	vs_get_service(service);
	client->service = service;

	INIT_LIST_HEAD(&client->rx_queue);
	spin_lock_init(&client->rx_queue_lock);
	tasklet_init(&client->rx_tasklet, vs_block_client_rx_tasklet,
			(unsigned long)client);
	tasklet_disable(&client->rx_tasklet);

	INIT_WORK(&client->disk_creation_work,
			vs_block_client_disk_creation_work);
	kref_init(&client->kref);

	dev_dbg(&service->dev, "New block client %pK\n", client);

	return &client->client;
}

static void vs_block_client_release(struct vs_client_block_state *state)
{
	struct block_client *client = state_to_block_client(state);

	flush_work(&client->disk_creation_work);

	vs_block_client_put(client);
}

/* FIXME: Jira ticket SDK-2459 - anjaniv */
static void vs_block_client_closed(struct vs_client_block_state *state)
{
	struct block_client *client = state_to_block_client(state);

	/*
	 * Stop the RX bounce tasklet and clean up its queue. We can wait for
	 * it to stop safely because it doesn't need to acquire the state
	 * lock, only the RX lock which we acquire after it is disabled.
	 */
	tasklet_disable(&client->rx_tasklet);
	spin_lock(&client->rx_queue_lock);
	while (!list_empty(&client->rx_queue)) {
		struct vs_mbuf *mbuf = list_first_entry(&client->rx_queue,
				struct vs_mbuf, queue);
		struct vs_pbuf pbuf;
		list_del(&mbuf->queue);
		vs_client_block_io_getbufs_ack_read(state, &pbuf, mbuf);
		vs_client_block_io_free_ack_read(state, &pbuf, mbuf);
	}
	spin_unlock(&client->rx_queue_lock);

	if (client->blkdev) {
		struct vs_block_device *blkdev = client->blkdev;
		char service_remove[] = "REMOVING_SERVICE=1";
		/* + 9 because "DEVNAME=" is 8 chars plus 1 for '\0' */
		char devname[sizeof(blkdev->disk->disk_name) + 9];
		char *envp[] = { service_remove, devname, NULL };

		dev_dbg(&client->service->dev, "removing block disk\n");

		/*
		 * Send a change event with DEVNAME to allow the block helper
		 * script to remove any server sessions which use either
		 * v${SERVICE_NAME} or ${DEVNAME}.  The remove event generated
		 * by the session driver doesn't include DEVNAME so the only
		 * way for userspace to map SERVICE_NAME to DEVNAME is by the
		 * symlink added when the client service was created.  If that
		 * symlink has been deleted, there's no other way to connect
		 * the two names.
		 */
		snprintf(devname, sizeof(devname), "DEVNAME=%s",
				blkdev->disk->disk_name);
		kobject_uevent_env(&client->service->dev.kobj, KOBJ_CHANGE,
				envp);

		/*
		 * We are done with the device now. The block device will only
		 * get removed once there are no more users (e.g. userspace
		 * applications).
		 */
		client->blkdev = NULL;
		vs_block_device_put(blkdev);
	}
}

static void vs_block_client_opened(struct vs_client_block_state *state)
{
	struct block_client *client = state_to_block_client(state);

#if !defined(CONFIG_LBDAF) && !defined(CONFIG_64BIT)
	if ((state->device_sectors * (state->sector_size >> 9))
			>> (sizeof(sector_t) * 8)) {
		dev_err(&client->service->dev,
				"Client doesn't support full capacity large block devices\n");
		vs_client_block_close(state);
		return;
	}
#endif

	/* Unblock the RX bounce tasklet. */
	tasklet_enable(&client->rx_tasklet);

	/*
	 * The block device allocation needs to sleep, so we defer it to a
	 * work queue.
	 */
	queue_work(client->service->work_queue, &client->disk_creation_work);
}

static int vs_block_client_ack_read(struct vs_client_block_state *state,
		void *tag, struct vs_pbuf pbuf, struct vs_mbuf *mbuf)
{
	struct block_client *client = state_to_block_client(state);
	struct bio *bio = tag;
	struct bio_vec *bvec;
	int err = 0;
	size_t bytes_read = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	struct bio_vec bvec_local;
	struct bvec_iter iter;
#else
	int i;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	bvec = &bvec_local;
	bio_for_each_segment(bvec_local, bio, iter)
#else
	bio_for_each_segment(bvec, bio, i)
#endif
	{
		unsigned long flags;
		void *buf;
		if (vs_pbuf_size(&pbuf) < bytes_read + bvec->bv_len) {
			dev_err(&client->service->dev,
					"bio read overrun: %zu into %zu byte response, but need %zd bytes\n",
					bytes_read, vs_pbuf_size(&pbuf),
					(size_t)bvec->bv_len);
			err = -EIO;
			break;
		}
		buf = bvec_kmap_irq(bvec, &flags);
		memcpy(buf, vs_pbuf_data(&pbuf) + bytes_read, bvec->bv_len);
		flush_kernel_dcache_page(bvec->bv_page);
		bvec_kunmap_irq(buf, &flags);
		bytes_read += bvec->bv_len;
	}

	vs_client_block_io_free_ack_read(state, &pbuf, mbuf);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
	if (err < 0)
		bio->bi_error = err;
	bio_endio(bio);
#else
	bio_endio(bio, err);
#endif

	return 0;
}

static void vs_block_client_rx_tasklet(unsigned long data)
{
	struct block_client *client = (struct block_client *)data;
	struct vs_mbuf *mbuf;
	struct vs_pbuf pbuf;

	spin_lock(&client->rx_queue_lock);

	/* The list shouldn't be empty. */
	if (WARN_ON(list_empty(&client->rx_queue))) {
		spin_unlock(&client->rx_queue_lock);
		return;
	}

	/* Get the next mbuf, and reschedule ourselves if there are more. */
	mbuf = list_first_entry(&client->rx_queue, struct vs_mbuf, queue);
	list_del(&mbuf->queue);
	if (!list_empty(&client->rx_queue))
		tasklet_schedule(&client->rx_tasklet);

	spin_unlock(&client->rx_queue_lock);

	/* Process the ack. */
	vs_client_block_io_getbufs_ack_read(&client->client, &pbuf, mbuf);
	vs_block_client_ack_read(&client->client, mbuf->priv, pbuf, mbuf);
}

static int vs_block_client_queue_ack_read(struct vs_client_block_state *state,
		void *tag, struct vs_pbuf pbuf, struct vs_mbuf *mbuf)
{
	struct block_client *client = state_to_block_client(state);

	spin_lock(&client->rx_queue_lock);
	list_add_tail(&mbuf->queue, &client->rx_queue);
	mbuf->priv = tag;
	spin_unlock(&client->rx_queue_lock);

	tasklet_schedule(&client->rx_tasklet);

	wake_up(&state->service->quota_wq);

	return 0;
}

static int vs_block_client_ack_write(struct vs_client_block_state *state,
		void *tag)
{
	struct bio *bio = tag;

	if (WARN_ON(!bio))
		return -EPROTO;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
	bio_endio(bio);
#else
	bio_endio(bio, 0);
#endif

	wake_up(&state->service->quota_wq);

	return 0;
}

static int vs_block_client_nack_io(struct vs_client_block_state *state,
		void *tag, vservice_block_block_io_error_t err)
{
	struct bio *bio = tag;

	if (WARN_ON(!bio))
		return -EPROTO;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
	bio->bi_error = block_client_vs_to_linux_error(err);
	bio_endio(bio);
#else
	bio_endio(bio, block_client_vs_to_linux_error(err));
#endif

	wake_up(&state->service->quota_wq);

	return 0;
}

static struct vs_client_block block_client_driver = {
	.rx_atomic		= true,
	.alloc			= vs_block_client_alloc,
	.release		= vs_block_client_release,
	.opened			= vs_block_client_opened,
	.closed			= vs_block_client_closed,
	.io = {
		.ack_read	= vs_block_client_queue_ack_read,
		.nack_read	= vs_block_client_nack_io,
		.ack_write	= vs_block_client_ack_write,
		.nack_write	= vs_block_client_nack_io,
	}
};

static int __init vs_block_client_init(void)
{
	int err;

	block_client_major = register_blkdev(0, CLIENT_BLKDEV_NAME);
	if (block_client_major < 0) {
		pr_err("Err registering blkdev\n");
		err = -ENOMEM;
		goto fail;
	}

	err = vservice_block_client_register(&block_client_driver,
			"block_client_driver");
	if (err)
		goto fail_unregister_blkdev;

	return 0;

fail_unregister_blkdev:
	unregister_blkdev(block_client_major, CLIENT_BLKDEV_NAME);
fail:
	return err;
}

static void __exit vs_block_client_exit(void)
{
	vservice_block_client_unregister(&block_client_driver);
	unregister_blkdev(block_client_major, CLIENT_BLKDEV_NAME);
}

module_init(vs_block_client_init);
module_exit(vs_block_client_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Block Client Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
