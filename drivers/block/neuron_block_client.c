// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/idr.h>

#include <linux/neuron.h>
#include <linux/neuron_block.h>

#define DRIVER_NAME "neuron-application-block-client"
#define DISK_NAME "nd_"
#define BLOCK_NAME "neuron_block"

#define bio_sector(bio) bio->bi_iter.bi_sector
#define bio_flags(bio) bio->bi_opf

static int block_client_major_nr;
static struct ida ida;

struct block_client_dev {
	struct device *dev;
	struct request_queue *queue;
	struct gendisk *gd;
	bool read_only;
	uint32_t sector_size;
	struct bio_list bio_list;
	spinlock_t list_lock;
	int id;
	struct kref kref;
	struct work_struct add_disk_work;
};

static const struct of_device_id app_block_client_match[] = {
	{
		.compatible = "qcom,neuron-block-client",
	},
	{},
};
MODULE_DEVICE_TABLE(of, app_block_client_match);

static blk_qc_t block_client_make_request(struct request_queue *q,
					  struct bio *bio)
{
	struct block_client_dev *blk_dev = bio->bi_disk->private_data;
	struct neuron_application *app_dev =
					to_neuron_application(blk_dev->dev);
	unsigned long flags;

	blk_queue_split(q, &bio);

	if ((bio_op(bio) == REQ_OP_WRITE) && (blk_dev->read_only)) {
		pr_err("Permission denied! Read-only block device\n");
		bio_io_error(bio);
		return BLK_QC_T_NONE;
	}

	spin_lock_irqsave(&blk_dev->list_lock, flags);
	bio_list_add(&blk_dev->bio_list, bio);
	spin_unlock_irqrestore(&blk_dev->list_lock, flags);

	neuron_app_wakeup(app_dev, NEURON_BLOCK_CLIENT_EVENT_REQUEST);

	return BLK_QC_T_NONE;
}

static int block_client_open(struct block_device *bdev, fmode_t mode)
{
	struct block_client_dev *blk_dev = bdev->bd_disk->private_data;

	if ((blk_dev->read_only) && (mode & FMODE_WRITE)) {
		pr_err("Read/write disk should be read-only\n");
		return -EROFS;
	}

	return 0;
}

static const struct block_device_operations block_client_fops = {
	.owner = THIS_MODULE,
	.open = block_client_open,
};

static enum neuron_block_req_type block_to_neuron_type(struct bio *bio)
{
	enum neuron_block_req_type req_type;

	switch (bio_op(bio)) {

	case REQ_OP_READ:
		pr_debug("READ REQUEST\n");
		req_type = NEURON_BLOCK_REQUEST_READ;
		break;
	case REQ_OP_WRITE:
		pr_debug("WRITE REQUEST\n");
		req_type = NEURON_BLOCK_REQUEST_WRITE;
		break;
	case REQ_OP_DISCARD:
		pr_debug("DISCARD REQUEST\n");
		req_type = NEURON_BLOCK_REQUEST_DISCARD;
		break;
	case REQ_OP_SECURE_ERASE:
		pr_debug("SECURE ERASE REQUEST\n");
		req_type = NEURON_BLOCK_REQUEST_SECURE_ERASE;
		break;
	case REQ_OP_WRITE_SAME:
		pr_debug("WRITE_SAME REQUEST\n");
		req_type = NEURON_BLOCK_REQUEST_WRITE_SAME;
		break;
	case REQ_OP_WRITE_ZEROES:
		pr_debug("WRITE_ZEROES REQUEST\n");
		req_type = NEURON_BLOCK_REQUEST_WRITE_ZEROES;
		break;
	case REQ_OP_FLUSH:
		pr_debug("REQ_OP_FLUSH REQUEST\n");
		req_type = NEURON_BLOCK_REQUEST_READ;
		break;
	default:
		pr_err("Request operation not found.\n");
		req_type = -EOPNOTSUPP;
		break;
	}

	return req_type;
}

static struct sk_buff *bio_to_skb(struct bio *bio)
{
	struct sk_buff *head_skb = NULL;
	struct sk_buff *tail_skb = NULL;
	struct bio_vec bvec;
	struct bvec_iter iter;
	int err;

	head_skb = alloc_skb(0, GFP_KERNEL);
	tail_skb = head_skb;

	bio_for_each_segment(bvec, bio, iter) {

		if (skb_shinfo(tail_skb)->nr_frags == MAX_SKB_FRAGS) {
			struct sk_buff *next_skb = NULL;

			if (head_skb != tail_skb) {
				head_skb->len += tail_skb->len;
				head_skb->data_len += tail_skb->data_len;
				head_skb->truesize += tail_skb->truesize;
			}

			next_skb = alloc_skb(0, GFP_KERNEL);

			if (!skb_shinfo(head_skb)->frag_list) {
				skb_shinfo(head_skb)->frag_list = next_skb;
			} else {
				tail_skb->next = next_skb;
				next_skb->prev = tail_skb;
			}
			tail_skb = next_skb;
		}

		err = skb_append_pagefrags(tail_skb, bvec.bv_page,
					   bvec.bv_offset,
					   bvec.bv_len);
		if (err) {
			pr_debug("Error appending page to skb.\n");
			return ERR_PTR(err);
		}
		tail_skb->len += bvec.bv_len;
		tail_skb->data_len += bvec.bv_len;
		tail_skb->truesize += PAGE_SIZE;
	}


	if (head_skb != tail_skb) {
		head_skb->len += tail_skb->len;
		head_skb->data_len += tail_skb->data_len;
		head_skb->truesize += tail_skb->truesize;
	}

	return head_skb;
}

static int app_block_client_get_request(struct neuron_application *app_dev,
					void **opaque_id,
					enum neuron_block_req_type *req_type,
					uint16_t *flags,
					uint64_t *start_sector,
					uint32_t *sectors,
					struct sk_buff **out_skb)
{
	struct block_client_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct bio *bio;
	bool flush_flag;
	bool commit_flag;
	bool sync_flag;

	spin_lock_irq(&blk_dev->list_lock);
	if (!bio_list_size(&blk_dev->bio_list)) {
		spin_unlock_irq(&blk_dev->list_lock);
		return -EAGAIN;
	}

	bio = bio_list_pop(&blk_dev->bio_list);
	spin_unlock_irq(&blk_dev->list_lock);

	*opaque_id = bio;
	*start_sector = bio->bi_iter.bi_sector;
	*sectors = bio_sectors(bio);
	flush_flag = (bio_flags(bio) & REQ_PREFLUSH);

	commit_flag = (bio_flags(bio) & REQ_FUA);
	sync_flag = (bio_flags(bio) & REQ_SYNC);

	*flags = (flush_flag ? (1 << __NEURON_BLOCK_REQ_PREFLUSH) : 0) |
		 (commit_flag ? (1 << __NEURON_BLOCK_REQ_FUA) : 0) |
		 (sync_flag ? (1 << __NEURON_BLOCK_REQ_SYNC) : 0);

	*req_type = block_to_neuron_type(bio);
	if (*req_type < 0)
		goto failed_req;

	if ((*req_type == NEURON_BLOCK_REQUEST_DISCARD) ||
			(*req_type == NEURON_BLOCK_REQUEST_SECURE_ERASE) ||
			(*req_type == NEURON_BLOCK_REQUEST_WRITE_ZEROES) ||
			(*sectors == 0)) {
		*out_skb = NULL;
	} else {
		*out_skb = bio_to_skb(bio);
		if (IS_ERR(*out_skb))
			goto failed_req;
	}

	pr_debug("vcnt: %d\n", bio->bi_vcnt);
	pr_debug("flags: %d\n", *flags);
	pr_debug("start_sector: %lld\n", (long long)*start_sector);
	pr_debug("sectors: %d\n", *sectors);

	return 0;

failed_req:
	bio_io_error(bio);
	return -EAGAIN;
}

static blk_status_t neuron_to_block_status(enum neuron_block_resp_status status)
{
	blk_status_t ret;

	switch (status) {
	case BLOCK_RESP_SUCCESS:
		ret = BLK_STS_OK;
		break;
	case BLOCK_RESP_TIMEOUT:
		ret = BLK_STS_TIMEOUT;
		break;
	case BLOCK_RESP_NOMEM:
		ret = BLK_STS_RESOURCE;
		break;
	case BLOCK_RESP_OPNOTSUPP:
		ret = BLK_STS_NOTSUPP;
		break;
	case BLOCK_RESP_IOERROR:
	default:
		ret = BLK_STS_IOERR;
		break;
	}

	return ret;
}

static int app_block_client_do_response(struct neuron_application *app_dev,
					void *opaque_id,
					enum neuron_block_resp_status status)
{
	struct bio *bio = opaque_id;

	bio->bi_status = neuron_to_block_status(status);
	bio_endio(bio);

	if (status)
		pr_err("Request completed with errors.\n");

	return 0;
}

static char *bin_to_uuid(char *dst, const void *src, size_t count)
{
	const unsigned char *_src = src;

	while (count--) {
		dst = hex_byte_pack(dst, *_src++);
		if (count == 12 || count == 10 || count == 8 || count == 6) {
			*dst = '-';
			dst++;
		}
	}

	return dst;
}

static void clean_blk_dev_obj(struct kref *kref)
{
	struct block_client_dev *blk_dev =
			container_of(kref, struct block_client_dev,
				     kref);

	ida_simple_remove(&ida, blk_dev->id);
	ida_destroy(&ida);
	del_gendisk(blk_dev->gd);
	if (blk_dev->queue)
		blk_cleanup_queue(blk_dev->queue);
	put_disk(blk_dev->gd);
	kfree(blk_dev);
}

static void app_block_client_add_disk_work(struct work_struct *work)
{
	struct block_client_dev *blk_dev =
				container_of(work, struct block_client_dev,
					     add_disk_work);

	device_add_disk(blk_dev->dev, blk_dev->gd, NULL);
	kref_put(&blk_dev->kref, clean_blk_dev_obj);
}

static int app_block_client_do_set_bd_params(struct neuron_application *app_dev,
					     struct neuron_block_param *param)
{
	struct block_client_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	int ret = 0;

	blk_dev->queue = blk_alloc_queue(GFP_KERNEL);
	if (!blk_dev->queue) {
		pr_err("Queue allocation failed.\n");
		ret = -ENOMEM;
		goto fail_init_queue;
	}

	blk_queue_make_request(blk_dev->queue, block_client_make_request);

	blk_queue_logical_block_size(blk_dev->queue, param->logical_block_size);
	blk_queue_physical_block_size(blk_dev->queue,
				      param->physical_block_size);
	blk_dev->sector_size = param->logical_block_size;
	blk_queue_alignment_offset(blk_dev->queue, param->alignment_offset);

	blk_dev->queue->limits.max_discard_sectors =
						param->discard_max_hw_sectors;
	blk_dev->queue->limits.max_hw_discard_sectors =
						param->discard_max_sectors;
	blk_dev->queue->limits.discard_granularity = param->discard_granularity;

	blk_dev->queue->queuedata = blk_dev;
	blk_dev->read_only = param->read_only;

	blk_queue_write_cache(blk_dev->queue, param->wc_flag, param->fua_flag);

	if (param->discard_max_hw_sectors > 0)
		blk_queue_flag_set(QUEUE_FLAG_DISCARD, blk_dev->queue);

	/* paravirt device. non-rotational device (SSD). */
	blk_queue_flag_set(QUEUE_FLAG_VIRT, blk_dev->queue);

	blk_dev->gd = alloc_disk(DISK_MAX_PARTS);
	if (!blk_dev->gd) {
		pr_err("Gendisk allocation failed.\n");
		return -ENOMEM;
	}

	set_disk_ro(blk_dev->gd, blk_dev->read_only);
	blk_dev->gd->major = block_client_major_nr;

	blk_dev->id = ida_simple_get(&ida, 0, 0, GFP_KERNEL);
	if (blk_dev->id < 0) {
		pr_err("Error get a new id.\n");
		return blk_dev->id;
	}

	blk_dev->gd->first_minor =  blk_dev->id * DISK_MAX_PARTS;
	blk_dev->gd->fops = &block_client_fops;
	blk_dev->gd->queue = blk_dev->queue;
	blk_dev->gd->flags |= GENHD_FL_EXT_DEVT; /* allow extended devt */
	blk_dev->gd->private_data = blk_dev;
	snprintf(blk_dev->gd->disk_name, PAGE_SIZE - 1, "%s%d", DISK_NAME,
		blk_dev->id);
	set_capacity(blk_dev->gd, param->num_device_sectors);

	blk_dev->gd->part0.info =
			kzalloc(sizeof(struct partition_meta_info),
				GFP_KERNEL);
	if (!blk_dev->gd->part0.info)
		return -ENOMEM;

	bin_to_uuid(blk_dev->gd->part0.info->uuid,
		    param->uuid.b,
		    sizeof(param->uuid.b));

	memcpy(blk_dev->gd->part0.info->volname, param->label,
	       sizeof(u8)*PARTITION_META_INFO_VOLNAMELTH);

	INIT_WORK(&blk_dev->add_disk_work, app_block_client_add_disk_work);
	kref_init(&blk_dev->kref);

	kref_get(&blk_dev->kref);
	schedule_work(&blk_dev->add_disk_work);

	kfree(param);

	return 0;

fail_init_queue:
	put_disk(blk_dev->gd);
	kfree(param);
	return ret;
}

static int app_block_client_probe(struct neuron_application *app_dev)
{
	struct block_client_dev *blk_dev;

	blk_dev = kzalloc(sizeof(*blk_dev), GFP_KERNEL);
	if (!blk_dev)
		return -ENOMEM;

	blk_dev->dev = &app_dev->dev;
	bio_list_init(&blk_dev->bio_list);
	spin_lock_init(&blk_dev->list_lock);
	ida_init(&ida);

	dev_set_drvdata(&app_dev->dev, blk_dev);

	return 0;
}

static void app_block_client_remove(struct neuron_application *app_dev)
{
	struct block_client_dev *blk_dev = dev_get_drvdata(&app_dev->dev);

	flush_work(&blk_dev->add_disk_work);
	kref_put(&blk_dev->kref, clean_blk_dev_obj);
}

static struct neuron_block_app_client_driver app_block_client_drv = {
	.base = {
		.driver = {
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
			.of_match_table = app_block_client_match,
		},
		.protocol_driver = &protocol_client_block_driver,
		.probe  = app_block_client_probe,
		.remove = app_block_client_remove,
	},
	.get_request = app_block_client_get_request,
	.do_response = app_block_client_do_response,
	.do_set_bd_params = app_block_client_do_set_bd_params,
};

static int __init block_client_init(void)
{
	int ret = 0;

	ret = neuron_register_app_driver(&app_block_client_drv.base);
	if (ret < 0) {
		pr_err("Failed to register driver\n");
		return ret;
	}

	block_client_major_nr = register_blkdev(0, BLOCK_NAME);
	if (block_client_major_nr < 0) {
		pr_err("Major number registration failed.\n");
		return block_client_major_nr;
	}

	return 0;
}

static void block_client_exit(void)
{
	unregister_blkdev(block_client_major_nr, BLOCK_NAME);
	neuron_unregister_app_driver(&app_block_client_drv.base);
}

module_init(block_client_init);
module_exit(block_client_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron block client module");
