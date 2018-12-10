/*
 * drivers/block/vs_block_server.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * block vservice server driver
 *
 */
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/module.h>

#include <vservices/types.h>
#include <vservices/buffer.h>
#include <vservices/protocol/block/types.h>
#include <vservices/protocol/block/common.h>
#include <vservices/protocol/block/server.h>
#include <vservices/protocol/block/client.h>
#include <vservices/service.h>
#include <vservices/wait.h>

#define VS_BLOCK_BLKDEV_DEFAULT_MODE FMODE_READ
/* Must match Linux bio sector_size (512 bytes) */
#define VS_BLOCK_BLK_DEF_SECTOR_SIZE 512
/* XXX should lookup block device physical_block_size */
#define VS_BLOCK_BLK_DEF_MIN_SECTORS 8

/*
 * Metadata for a request. Note that the bio must be embedded at the end of
 * this structure, because it is allocated from a bioset.
 */
struct block_server_request {
	struct block_server	*server;
	u32			tagid;
	u32			size;
	int			op_err;
	struct list_head	list;
	struct vs_pbuf		pbuf;
	struct vs_mbuf		*mbuf;
	bool			bounced;
	bool			submitted;

	struct bio		bio;
};

struct block_server {
	struct vs_server_block_state	server;
	struct vs_service_device	*service;

	struct block_device		*bdev;
	struct bio_set			*bioset;

	unsigned int			sector_size;
	bool				started;

	/* Bounced writes are deferred to keep memcpy off service queue */
	struct list_head		bounce_req_queue;
	struct work_struct		bounce_req_work;
	spinlock_t			bounce_req_lock;

	/* Count of outstanding requests submitted to block layer */
	atomic_t			submitted_req_count;
	wait_queue_head_t		submitted_req_wq;

	/* Completions are deferred because end_io may be in atomic context */
	struct list_head		completed_req_queue;
	struct work_struct		completed_req_work;
	spinlock_t			completed_req_lock;
};

#define state_to_block_server(state) \
	container_of(state, struct block_server, server)

#define dev_to_block_server(dev) \
	state_to_block_server(dev_get_drvdata(dev))

static inline vservice_block_block_io_error_t
block_server_linux_to_vs_error(int err)
{
	/*
	 * This list is not exhaustive. For all other errors, we return
	 * unsupported_command.
	 */
	switch (err) {
	case -ECOMM:
	case -EIO:
	case -ENOMEM:
		return VSERVICE_BLOCK_MEDIA_FAILURE;
	case -ETIME:
	case -ETIMEDOUT:
		return VSERVICE_BLOCK_MEDIA_TIMEOUT;
	case -EILSEQ:
		return VSERVICE_BLOCK_INVALID_INDEX;
	default:
		if (err)
			return VSERVICE_BLOCK_UNSUPPORTED_COMMAND;
		return 0;
	}

	return 0;
}

static inline u32 vs_req_num_sectors(struct block_server *server,
		struct block_server_request *req)
{
	return req->size / server->sector_size;
}

static inline u64 vs_req_sector_index(struct block_server_request *req)
{
	return req->bio.bi_iter.bi_sector;
}

static void vs_block_server_closed(struct vs_server_block_state *state)
{
	struct block_server *server = state_to_block_server(state);
	struct block_server_request *req;

	/*
	 * Fail all requests that haven't been sent to the block layer yet.
	 */
	spin_lock(&server->bounce_req_lock);
	while (!list_empty(&server->bounce_req_queue)) {
		req = list_first_entry(&server->bounce_req_queue,
				struct block_server_request, list);
		list_del(&req->list);
		spin_unlock(&server->bounce_req_lock);
		bio_io_error(&req->bio);
		spin_lock(&server->bounce_req_lock);
	}
	spin_unlock(&server->bounce_req_lock);

	/*
	 * Wait until all outstanding requests to the block layer are
	 * complete.
	 */
	wait_event(server->submitted_req_wq,
			!atomic_read(&server->submitted_req_count));

	/*
	 * Discard all the completed requests.
	 */
	spin_lock_irq(&server->completed_req_lock);
	while (!list_empty(&server->completed_req_queue)) {
		req = list_first_entry(&server->completed_req_queue,
				struct block_server_request, list);
		list_del(&req->list);
		if (req->mbuf) {
			spin_unlock_irq(&server->completed_req_lock);
			if (bio_data_dir(&req->bio) == WRITE)
				vs_server_block_io_free_req_write(state,
						&req->pbuf, req->mbuf);
			else
				vs_server_block_io_free_ack_read(state,
						&req->pbuf, req->mbuf);
			spin_lock_irq(&server->completed_req_lock);
		}
		bio_put(&req->bio);
	}
	spin_unlock_irq(&server->completed_req_lock);
}

static ssize_t
vs_block_server_readonly_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct block_server *server = dev_to_block_server(dev);
	int err;
	unsigned long val;

	vs_service_state_lock(server->service);
	if (server->started) {
		err = -EBUSY;
		goto unlock;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		goto unlock;

	if (bdev_read_only(server->bdev) && !val) {
		dev_info(dev,
				"Cannot set %s to read/write: read-only device\n",
				server->service->name);
		err = -EINVAL;
		goto unlock;
	}

	server->server.readonly = val;
	err = count;

unlock:
	vs_service_state_unlock(server->service);

	return err;
}

static ssize_t
vs_block_server_readonly_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct block_server *server = dev_to_block_server(dev);
	int cnt;

	vs_service_state_lock(server->service);
	cnt = scnprintf(buf, PAGE_SIZE, "%d\n", server->server.readonly);
	vs_service_state_unlock(server->service);

	return cnt;
}

static ssize_t
vs_block_server_start_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct block_server *server = dev_to_block_server(dev);
	int err;
	unsigned long val;

	vs_service_state_lock(server->service);

	err = kstrtoul(buf, 0, &val);
	if (err)
		goto unlock;

	if (!val && server->started) {
		err = -EBUSY;
		goto unlock;
	}

	if (val && !server->started) {
		server->started = true;

		if (server->server.state.base.statenum ==
				VSERVICE_BASE_STATE_CLOSED__OPEN)
			vs_server_block_open_complete(&server->server,
					VS_SERVER_RESP_SUCCESS);
	}

	err = count;
unlock:
	vs_service_state_unlock(server->service);

	return err;
}

static ssize_t
vs_block_server_start_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct block_server *server = dev_to_block_server(dev);
	int cnt;

	vs_service_state_lock(server->service);
	cnt = scnprintf(buf, PAGE_SIZE, "%d\n", server->started);
	vs_service_state_unlock(server->service);

	return cnt;
}

static DEVICE_ATTR(start, S_IWUSR | S_IRUSR, vs_block_server_start_show,
	vs_block_server_start_store);
static DEVICE_ATTR(readonly, S_IWUSR | S_IRUSR, vs_block_server_readonly_show,
	vs_block_server_readonly_store);

static struct attribute *vs_block_server_dev_attrs[] = {
	&dev_attr_start.attr,
	&dev_attr_readonly.attr,
	NULL,
};

static const struct attribute_group vs_block_server_attr_group = {
	.attrs = vs_block_server_dev_attrs
};

/*
 * Invoked by vs_server_block_handle_req_open() after receiving open
 * requests to perform server specific initialisations
 *
 * The "delayed start" feature can be enforced here
 */
static vs_server_response_type_t
vs_block_server_open(struct vs_server_block_state * _state)
{
	struct block_server *server = state_to_block_server(_state);

	return (server->started) ? VS_SERVER_RESP_SUCCESS :
				   VS_SERVER_RESP_EXPLICIT_COMPLETE;
}

static int
vs_block_server_complete_req_read(struct block_server_request *req)
{
	struct block_server *server = req->server;
	struct vs_server_block_state *state = &server->server;
	int err = -EIO;

	if (req->op_err) {
		err = req->op_err;
		dev_dbg(&server->service->dev,
				"read nack, err %d sector 0x%llx num 0x%x\n",
				err, vs_req_sector_index(req),
				vs_req_num_sectors(server, req));

		if (req->mbuf)
			vs_server_block_io_free_ack_read(state, &req->pbuf,
					req->mbuf);

		err = vs_server_block_io_send_nack_read(state, req->tagid,
				block_server_linux_to_vs_error(err),
				GFP_KERNEL);
	} else {
		if (req->bounced && !req->mbuf) {
			req->mbuf = vs_server_block_io_alloc_ack_read(
					&server->server, &req->pbuf,
					GFP_KERNEL);
			if (IS_ERR(req->mbuf)) {
				err = PTR_ERR(req->mbuf);
				req->mbuf = NULL;
			}
		}

		if (req->bounced && req->mbuf) {
			int i;
			struct bio_vec *bv;
			void *data = req->pbuf.data;

			if (vs_pbuf_resize(&req->pbuf, req->size) < 0) {
				bio_io_error(&req->bio);
				return 0;
			}

			bio_for_each_segment_all(bv, &req->bio, i) {
				memcpy(data, page_address(bv->bv_page) +
						bv->bv_offset, bv->bv_len);
				data += bv->bv_len;
				__free_page(bv->bv_page);
			}
			req->bounced = false;
		}

		if (req->mbuf) {
			dev_vdbg(&server->service->dev,
					"read ack, sector 0x%llx num 0x%x\n",
					vs_req_sector_index(req),
					vs_req_num_sectors(server, req));

			err = vs_server_block_io_send_ack_read(state,
					req->tagid, req->pbuf, req->mbuf);

			if (err && (err != -ENOBUFS)) {
				vs_server_block_io_free_ack_read(state,
						&req->pbuf, req->mbuf);
				req->mbuf = NULL;
			}
		} else {
			WARN_ON(!err || !req->bounced);
		}
	}

	if (err && (err != -ENOBUFS))
		dev_dbg(&server->service->dev,
				"error %d sending read reply\n", err);
	else if (err == -ENOBUFS)
		dev_vdbg(&server->service->dev, "out of quota, will retry\n");

	return err;
}

static int
vs_block_server_complete_req_write(struct block_server_request *req)
{
	struct block_server *server = req->server;
	struct vs_server_block_state *state = &server->server;
	int err;

	WARN_ON(req->mbuf);

	if (req->op_err) {
		dev_dbg(&server->service->dev,
				"write nack, err %d sector 0x%llx num 0x%x\n",
				req->op_err, vs_req_sector_index(req),
				vs_req_num_sectors(server, req));

		err = vs_server_block_io_send_nack_write(state, req->tagid,
				block_server_linux_to_vs_error(req->op_err),
				GFP_KERNEL);
	} else {
		dev_vdbg(&server->service->dev,
				"write ack, sector 0x%llx num 0x%x\n",
				vs_req_sector_index(req),
				vs_req_num_sectors(server, req));

		err = vs_server_block_io_send_ack_write(state, req->tagid,
				GFP_KERNEL);
	}

	if (err && (err != -ENOBUFS))
		dev_dbg(&server->service->dev,
				"error %d sending write reply\n", err);
	else if (err == -ENOBUFS)
		dev_vdbg(&server->service->dev, "out of quota, will retry\n");

	return err;
}

static int vs_block_server_complete_req(struct block_server *server,
		struct block_server_request *req)
{
	int err;

	req->bio.bi_iter.bi_idx = 0;
	if (!vs_state_lock_safe(&server->server))
		return -ENOLINK;

	if (bio_data_dir(&req->bio) == WRITE)
		err = vs_block_server_complete_req_write(req);
	else
		err = vs_block_server_complete_req_read(req);

	vs_state_unlock(&server->server);

	if (err == -ENOBUFS)
		dev_vdbg(&server->service->dev, "bio %pK response out of quota, will retry\n", &req->bio);

	return err;
}

static void vs_block_server_complete_requests_work(struct work_struct *work)
{
	struct block_server *server = container_of(work, struct block_server,
			completed_req_work);
	struct block_server_request *req;

	vs_service_send_batch_start(server->service, false);

	/*
	 * Send ack/nack responses for each completed request. If a request
	 * cannot be sent because we are over-quota then this function will
	 * return with a non-empty list, and the tx_ready handler will
	 * reschedule us when we are back under quota. In all other cases
	 * this function will return with an empty list.
	 */
	spin_lock_irq(&server->completed_req_lock);
	while (!list_empty(&server->completed_req_queue)) {
		int err;
		req = list_first_entry(&server->completed_req_queue,
				struct block_server_request, list);
		dev_vdbg(&server->service->dev, "complete bio %pK\n", &req->bio);
		list_del(&req->list);
		spin_unlock_irq(&server->completed_req_lock);

		err = vs_block_server_complete_req(server, req);
		if (err == -ENOBUFS) {
			dev_vdbg(&server->service->dev, "defer bio %pK\n", &req->bio);
			/*
			 * Couldn't send the completion; re-queue the request
			 * and exit. We'll start again when more quota becomes
			 * available.
			 */
			spin_lock_irq(&server->completed_req_lock);
			list_add_tail(&req->list,
					&server->completed_req_queue);
			break;
		}

		dev_vdbg(&server->service->dev, "free bio %pK err %d\n", &req->bio, err);
		bio_put(&req->bio);

		spin_lock_irq(&server->completed_req_lock);
	}
	spin_unlock_irq(&server->completed_req_lock);

	vs_service_send_batch_end(server->service, true);
}

static int vs_block_server_tx_ready(struct vs_server_block_state *state)
{
	struct block_server *server = state_to_block_server(state);

	schedule_work(&server->completed_req_work);

	return 0;
}

static bool vs_block_can_map_pbuf(struct request_queue *q,
		struct vs_pbuf *pbuf, size_t size)
{
	/* The pbuf must satisfy the driver's alignment requirements. */
	if (!blk_rq_aligned(q, (unsigned long)pbuf->data, size))
		return false;

	/*
	 * bios can only contain pages. Sometime the pbuf is in an IO region
	 * that has no struct page (e.g. a channel primary buffer), in which
	 * case we can't map it into a bio.
	 */
	/* FIXME: Redmine issue #930 - philip. */
	if (!pfn_valid(__pa(pbuf->data) >> PAGE_SHIFT))
		return false;

	return true;
}

static int vs_block_bio_map_pbuf(struct bio *bio, struct vs_pbuf *pbuf)
{
	int offset = offset_in_page((unsigned long)pbuf->data);
	void *ptr = pbuf->data;
	int size = pbuf->size;

	while (size > 0) {
		unsigned bytes = min_t(unsigned, PAGE_SIZE - offset, size);

		if (bio_add_page(bio, virt_to_page(ptr), bytes,
					offset) < bytes)
			return -EIO;

		ptr += bytes;
		size -= bytes;
		offset = 0;
	}

	return 0;
}

/* Read request handling */
static void vs_block_server_read_done(struct bio *bio)
{
	unsigned long flags;
	int err = blk_status_to_errno(bio->bi_status);
	struct block_server_request *req = container_of(bio,
			struct block_server_request, bio);
	struct block_server *server = req->server;
	req->op_err = err;

	spin_lock_irqsave(&server->completed_req_lock, flags);
	if (req->mbuf)
		list_add(&req->list, &server->completed_req_queue);
	else
		list_add_tail(&req->list, &server->completed_req_queue);
	spin_unlock_irqrestore(&server->completed_req_lock, flags);

	if (req->submitted && atomic_dec_and_test(&server->submitted_req_count))
		wake_up_all(&server->submitted_req_wq);

	schedule_work(&server->completed_req_work);
}

/*
 * TODO: this may need to split and chain the bio if it exceeds the physical
 * segment limit of the device. Not clear whose responsibility that is; queue
 * might do it for us (if there is one)
 */
#define vs_block_make_request(bio) generic_make_request(bio)

static int vs_block_submit_read(struct block_server *server,
		struct block_server_request *req, gfp_t gfp)
{
	struct request_queue *q = bdev_get_queue(server->bdev);
	struct bio *bio = &req->bio;
	int size = req->size;
	int err = 0;

	if (req->mbuf && vs_block_can_map_pbuf(q, &req->pbuf, size)) {
		/*
		 * The mbuf is valid and the driver can directly access the
		 * pbuf, so we don't need a bounce buffer. Map the pbuf
		 * directly into the bio.
		*/
		if (vs_pbuf_resize(&req->pbuf, size) < 0)
			err = -EIO;
		if (!err)
			err = vs_block_bio_map_pbuf(bio, &req->pbuf);
	} else {
		/* We need a bounce buffer. First set up the bvecs. */
		bio->bi_iter.bi_size = size;

		while (size > 0) {
			struct bio_vec *bvec = &bio->bi_io_vec[bio->bi_vcnt];

			BUG_ON(bio->bi_vcnt >= bio->bi_max_vecs);

			bvec->bv_page = NULL; /* Allocated below */
			bvec->bv_len = min_t(unsigned, PAGE_SIZE, size);
			bvec->bv_offset = 0;

			bio->bi_vcnt++;
			size -= bvec->bv_len;
		}

		err = bio_alloc_pages(bio, gfp);
		if (!err) {
			blk_recount_segments(q, bio);
			req->bounced = true;
		}
	}

	if (err) {
		bio->bi_status = err;
		bio_endio(bio);
	} else {
		dev_vdbg(&server->service->dev,
				"submit read req sector %#llx count %#x\n",
				vs_req_sector_index(req),
				vs_req_num_sectors(server, req));
		req->submitted = true;
		atomic_inc(&server->submitted_req_count);
		vs_block_make_request(bio);
	}

	return 0;
}

static int vs_block_server_io_req_read(struct vs_server_block_state *state,
		u32 tagid, u64 sector_index, u32 num_sects, bool nodelay,
		bool flush)
{
	struct block_server *server = state_to_block_server(state);
	struct bio *bio;
	struct block_server_request *req;
	unsigned size = num_sects * server->sector_size;
	unsigned op_flags = 0;

	/*
	 * This nr_pages calculation assumes that the pbuf data is offset from
	 * the start of the size-aligned message buffer by more than 0 but
	 * less than one sector, which is always true for the current message
	 * layout generated by mill when we assume 512-byte sectors.
	 */
	unsigned nr_pages = 1 + (size >> PAGE_SHIFT);

	bio = bio_alloc_bioset(GFP_KERNEL, nr_pages, server->bioset);
	if (!bio)
		return -ENOMEM;
	dev_vdbg(&server->service->dev, "alloc r bio %pK\n", bio);
	req = container_of(bio, struct block_server_request, bio);

	req->server = server;
	req->tagid = tagid;
	req->op_err = 0;
	req->mbuf = NULL;
	req->size = size;
	req->bounced = false;
	req->submitted = false;

	if (flush) {
		op_flags |= REQ_PREFLUSH;
	}
	if (nodelay) {
		op_flags |= REQ_SYNC;
	}

	bio->bi_iter.bi_sector = (sector_t)sector_index;
	bio_set_dev(bio, server->bdev);
	bio_set_op_attrs(bio, REQ_OP_READ, op_flags);
	bio->bi_end_io = vs_block_server_read_done;

	req->mbuf = vs_server_block_io_alloc_ack_read(state, &req->pbuf,
			GFP_KERNEL);
	if (IS_ERR(req->mbuf) && (PTR_ERR(req->mbuf) == -ENOBUFS)) {
		/* Fall back to a bounce buffer */
		req->mbuf = NULL;
	} else if (IS_ERR(req->mbuf)) {
		bio->bi_status = PTR_ERR(req->mbuf);
		bio_endio(bio);
		return 0;
	}

	return vs_block_submit_read(server, req, GFP_KERNEL);
}

/* Write request handling */
static int vs_block_submit_bounced_write(struct block_server *server,
		struct block_server_request *req, gfp_t gfp)
{
	struct bio *bio = &req->bio;
	void *data = req->pbuf.data;
	struct bio_vec *bv;
	int i;

	if (bio_alloc_pages(bio, gfp | __GFP_NOWARN) < 0)
		return -ENOMEM;
	blk_recount_segments(bdev_get_queue(server->bdev), bio);
	req->bounced = true;

	/* Copy all the data into the bounce buffer */
	bio_for_each_segment_all(bv, bio, i) {
		memcpy(page_address(bv->bv_page) + bv->bv_offset, data,
				bv->bv_len);
		data += bv->bv_len;
	}

	vs_server_block_io_free_req_write(&server->server, &req->pbuf,
			req->mbuf);
	req->mbuf = NULL;

	dev_vdbg(&server->service->dev,
			"submit bounced write req sector %#llx count %#x\n",
			vs_req_sector_index(req),
			vs_req_num_sectors(server, req));
	req->submitted = true;
	atomic_inc(&server->submitted_req_count);
	vs_block_make_request(bio);

	return 0;
}

static void vs_block_server_write_bounce_work(struct work_struct *work)
{
	struct block_server *server = container_of(work, struct block_server,
			bounce_req_work);
	struct block_server_request *req;

	spin_lock(&server->bounce_req_lock);
	while (!list_empty(&server->bounce_req_queue)) {
		req = list_first_entry(&server->bounce_req_queue,
				struct block_server_request, list);
		dev_vdbg(&server->service->dev, "write bio %pK\n", &req->bio);
		list_del(&req->list);
		spin_unlock(&server->bounce_req_lock);

		if (vs_block_submit_bounced_write(server, req,
					GFP_KERNEL) == -ENOMEM) {
			spin_lock(&server->bounce_req_lock);
			list_add(&req->list, &server->bounce_req_queue);
			spin_unlock(&server->bounce_req_lock);
			schedule_work(work);
			return;
		}

		spin_lock(&server->bounce_req_lock);
	}
	spin_unlock(&server->bounce_req_lock);
}

static void vs_block_server_write_done(struct bio *bio)
{
	unsigned long flags;
	int err = blk_status_to_errno(bio->bi_status);
	struct block_server_request *req = container_of(bio,
			struct block_server_request, bio);
	struct block_server *server = req->server;

	if (req->bounced) {
		int i;
		struct bio_vec *bv;
		bio_for_each_segment_all(bv, bio, i)
			__free_page(bv->bv_page);
	} else if (req->mbuf) {
		vs_server_block_io_free_req_write(&server->server, &req->pbuf,
				req->mbuf);
		req->mbuf = NULL;
	}

	if (req->submitted && atomic_dec_and_test(&server->submitted_req_count))
		wake_up_all(&server->submitted_req_wq);

	req->op_err = err;

	spin_lock_irqsave(&server->completed_req_lock, flags);
	list_add_tail(&req->list, &server->completed_req_queue);
	spin_unlock_irqrestore(&server->completed_req_lock, flags);

	schedule_work(&server->completed_req_work);
}

static int vs_block_server_io_req_write(struct vs_server_block_state *state,
		u32 tagid, u64 sector_index, u32 num_sects, bool nodelay,
		bool flush, bool commit, struct vs_pbuf pbuf, struct vs_mbuf *mbuf)
{
	struct block_server *server = state_to_block_server(state);
	struct request_queue *q = bdev_get_queue(server->bdev);
	struct bio *bio;
	struct block_server_request *req;
	unsigned long data = (unsigned long)pbuf.data;
	unsigned long start = data >> PAGE_SHIFT;
	unsigned long end = (data + pbuf.size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	int err;
	unsigned op_flags = 0;

	bio = bio_alloc_bioset(GFP_KERNEL, end - start, server->bioset);
	if (!bio)
		return -ENOMEM;
	dev_vdbg(&server->service->dev, "alloc w bio %pK\n", bio);
	req = container_of(bio, struct block_server_request, bio);

	req->server = server;
	req->tagid = tagid;
	req->op_err = 0;
	req->mbuf = mbuf;
	req->pbuf = pbuf;
	req->size = server->sector_size * num_sects;
	req->bounced = false;
	req->submitted = false;

	if (flush) {
		op_flags |= REQ_PREFLUSH;
	}
	if (commit) {
		op_flags |= REQ_FUA;
	}
	if (nodelay) {
		op_flags |= REQ_SYNC;
	}

	bio->bi_iter.bi_sector = (sector_t)sector_index;
	bio_set_dev(bio, server->bdev);
	bio_set_op_attrs(bio, REQ_OP_WRITE, op_flags);
	bio->bi_end_io = vs_block_server_write_done;

	if (pbuf.size < req->size) {
		err = -EINVAL;
		goto fail_bio;
	}
	if (WARN_ON(pbuf.size > req->size))
		pbuf.size = req->size;

	if (state->readonly) {
		err = -EROFS;
		goto fail_bio;
	}

	if (!vs_block_can_map_pbuf(q, &req->pbuf, req->pbuf.size)) {
		/* We need a bounce buffer. First set up the bvecs. */
		int size = pbuf.size;

		bio->bi_iter.bi_size = size;

		while (size > 0) {
			struct bio_vec *bvec = &bio->bi_io_vec[bio->bi_vcnt];

			BUG_ON(bio->bi_vcnt >= bio->bi_max_vecs);

			bvec->bv_page = NULL; /* Allocated later */
			bvec->bv_len = min_t(unsigned, PAGE_SIZE, size);
			bvec->bv_offset = 0;

			bio->bi_vcnt++;
			size -= bvec->bv_len;
		}

		/*
		 * Defer the rest so we don't have to hold the state lock
		 * during alloc_page & memcpy
		 */
		spin_lock(&server->bounce_req_lock);
		list_add_tail(&req->list, &server->bounce_req_queue);
		spin_unlock(&server->bounce_req_lock);
		schedule_work(&server->bounce_req_work);

		return 0;
	}

	/* No bounce needed; map the pbuf directly. */
	err = vs_block_bio_map_pbuf(bio, &pbuf);
	if (err < 0)
		goto fail_bio;

	dev_vdbg(&server->service->dev,
			"submit direct write req sector %#llx count %#x\n",
			vs_req_sector_index(req),
			vs_req_num_sectors(server, req));
	req->submitted = true;
	atomic_inc(&server->submitted_req_count);
	vs_block_make_request(bio);

	return 0;

fail_bio:
	bio->bi_status = err;
	bio_endio(bio);
	return 0;
}

static struct block_device *
vs_block_server_find_by_name(struct block_server *server)
{
	struct block_device *bdev = NULL;
	struct class_dev_iter iter;
	struct device *dev;

	class_dev_iter_init(&iter, &block_class, NULL, NULL);
	while (1) {
		dev = class_dev_iter_next(&iter);
		if (!dev)
			break;

		if (strcmp(dev_name(dev), server->service->name) == 0) {
			bdev = blkdev_get_by_dev(dev->devt,
					VS_BLOCK_BLKDEV_DEFAULT_MODE, NULL);
			if (!IS_ERR_OR_NULL(bdev))
				break;
		}
	}
	class_dev_iter_exit(&iter);

	if (!dev || IS_ERR_OR_NULL(bdev))
		return ERR_PTR(-ENODEV);

	dev_dbg(&server->service->dev, "Attached to block device %s (%d:%d)\n",
			dev_name(dev), MAJOR(dev->devt), MINOR(dev->devt));
	return bdev;
}

static struct block_device *
vs_block_server_find_by_path(struct block_server *server, const char *base_path)
{
	struct block_device *bdev;
	char *bdev_path;

	bdev_path = kasprintf(GFP_KERNEL, "%s/%s", base_path,
			server->service->name);
	if (!bdev_path)
		return ERR_PTR(-ENOMEM);

	bdev = blkdev_get_by_path(bdev_path, VS_BLOCK_BLKDEV_DEFAULT_MODE,
			NULL);
	dev_dbg(&server->service->dev, "Attached to block device %s\n",
			bdev_path);

	kfree(bdev_path);

	if (!bdev)
		return ERR_PTR(-ENODEV);
	return bdev;
}

static struct block_device *
vs_block_server_attach_block_device(struct block_server *server)
{
	const char *paths[] = {
		"/dev",
		"/dev/block",
		"/dev/mapper",
		"/dev/disk/by-partlabel",
		"/dev/disk/by-label",
		"/dev/disk/by-partuuid",
		"/dev/disk/by-uuid"
	};
	struct block_device *bdev;
	int i;

	/*
	 * Try first to look the block device up by path. This is done because
	 * the name exposed to user-space in /dev/ is not necessarily the name
	 * being used inside the kernel for the device.
	 */
	for (i = 0; i < ARRAY_SIZE(paths); i++) {
		bdev = vs_block_server_find_by_path(server, paths[i]);
		if (!IS_ERR(bdev))
			break;
	}
	if (i == ARRAY_SIZE(paths)) {
		/*
		 * Couldn't find the block device in any of the usual places.
		 * Try to match it against the kernel's device name. If the
		 * name of the service and the name of a device in the block
		 * class match then attempt to look the block device up by the
		 * dev_t (major/minor) value.
		 */
		bdev = vs_block_server_find_by_name(server);
	}
	if (IS_ERR(bdev))
		return bdev;

	// XXX get block device physical block size
	server->sector_size		= VS_BLOCK_BLK_DEF_SECTOR_SIZE;
	server->server.segment_size	= round_down(
		vs_service_max_mbuf_size(server->service) -
		sizeof(vs_message_id_t), server->sector_size);
	server->server.sector_size	= server->sector_size *
						VS_BLOCK_BLK_DEF_MIN_SECTORS;
	server->server.device_sectors	= bdev->bd_part->nr_sects /
						VS_BLOCK_BLK_DEF_MIN_SECTORS;
	if (bdev_read_only(bdev))
		server->server.readonly = true;
	server->server.flushable = true;
	server->server.committable = true;

	return bdev;
}

static struct vs_server_block_state *
vs_block_server_alloc(struct vs_service_device *service)
{
	struct block_server *server;
	int err;

	server = kzalloc(sizeof(*server), GFP_KERNEL);
	if (!server)
		return NULL;

	server->service = service;
	server->started = false;
	INIT_LIST_HEAD(&server->bounce_req_queue);
	INIT_WORK(&server->bounce_req_work, vs_block_server_write_bounce_work);
	spin_lock_init(&server->bounce_req_lock);
	atomic_set(&server->submitted_req_count, 0);
	init_waitqueue_head(&server->submitted_req_wq);
	INIT_LIST_HEAD(&server->completed_req_queue);
	INIT_WORK(&server->completed_req_work,
			vs_block_server_complete_requests_work);
	spin_lock_init(&server->completed_req_lock);

	server->bdev = vs_block_server_attach_block_device(server);
	if (IS_ERR(server->bdev)) {
		dev_err(&server->service->dev,
				"No appropriate block device was found to satisfy the service name %s - error %ld\n",
				server->service->name, PTR_ERR(server->bdev));
		goto fail_attach_device;
	}

	dev_set_drvdata(&service->dev, &server->server);

	err = sysfs_create_group(&service->dev.kobj,
				 &vs_block_server_attr_group);
	if (err) {
		dev_err(&service->dev,
			"Failed to create attribute group for service %s\n",
			service->name);
		goto fail_create_group;
	}

	/*
	 * We know the upper bound on simultaneously active bios (i.e. the
	 * smaller of the in quota, and the sum of the read and write command
	 * tag limits), so we can pre-allocate that many, and hopefully never
	 * fail to allocate one in a request handler.
	 *
	 * However, allocation may fail if the number of pages (and thus
	 * bvecs) in a request exceeds BIO_INLINE_VECS (which is hard-coded to
	 * 4 in all mainline kernels). That possibility is the only reason we
	 * can't enable rx_atomic for this driver.
	 */
	server->bioset = bioset_create(min_t(unsigned, service->recv_quota,
		VSERVICE_BLOCK_IO_READ_MAX_PENDING +
		VSERVICE_BLOCK_IO_WRITE_MAX_PENDING),
		offsetof(struct block_server_request, bio), BIOSET_NEED_BVECS);

	if (!server->bioset) {
		dev_err(&service->dev,
			"Failed to allocate bioset for service %s\n",
			service->name);
		goto fail_create_bioset;
	}

	dev_dbg(&service->dev, "New block server %pK\n", server);

	return &server->server;

fail_create_bioset:
	sysfs_remove_group(&server->service->dev.kobj,
			   &vs_block_server_attr_group);
fail_create_group:
	dev_set_drvdata(&service->dev, NULL);
	blkdev_put(server->bdev, VS_BLOCK_BLKDEV_DEFAULT_MODE);
fail_attach_device:
	kfree(server);

	return NULL;
}

static void vs_block_server_release(struct vs_server_block_state *state)
{
	struct block_server *server = state_to_block_server(state);

	cancel_work_sync(&server->bounce_req_work);
	cancel_work_sync(&server->completed_req_work);

	blkdev_put(server->bdev, VS_BLOCK_BLKDEV_DEFAULT_MODE);

	sysfs_remove_group(&server->service->dev.kobj,
			   &vs_block_server_attr_group);

	bioset_free(server->bioset);

	kfree(server);
}

static struct vs_server_block block_server_driver = {
	.alloc			= vs_block_server_alloc,
	.release		= vs_block_server_release,
	.open			= vs_block_server_open,
	.closed			= vs_block_server_closed,
	.tx_ready		= vs_block_server_tx_ready,
	.io = {
		.req_read	= vs_block_server_io_req_read,
		.req_write	= vs_block_server_io_req_write,
	},

	/* Large default quota for batching read/write commands */
	.in_quota_best		= 32,
	.out_quota_best		= 32,
};

static int __init vs_block_server_init(void)
{
	return vservice_block_server_register(&block_server_driver,
			"block_server_driver");
}

static void __exit vs_block_server_exit(void)
{
	vservice_block_server_unregister(&block_server_driver);
}

module_init(vs_block_server_init);
module_exit(vs_block_server_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Block Server Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
