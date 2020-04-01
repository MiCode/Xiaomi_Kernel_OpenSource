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
#include <linux/spinlock.h>
#include <linux/uuid.h>

#include <linux/neuron.h>
#include <linux/neuron_block.h>

#define DRIVER_NAME "neuron-application-block-server"
#define BLOCK_NAME "neuron_block"

struct bio_priv {
	struct neuron_application *app_dev;
	uint32_t id;
	struct sk_buff *skb;
};

struct block_server_dev {
	struct request_queue *queue;
	struct gendisk *gd;
	char *bdev_name;
	struct block_device *bdev;
	uint32_t sector_size;
	struct bio_list bio_list;
	spinlock_t list_lock;
};

static const struct of_device_id app_block_server_match[] = {
	{
		.compatible = "qcom,neuron-block-server",
	},
	{},
};
MODULE_DEVICE_TABLE(of, app_block_server_match);

static int get_blk_dev(struct neuron_application *app_dev);

static ssize_t blk_name_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE - 1, "%s\n", blk_dev->bdev_name);
}

static ssize_t blk_name_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(dev);
	struct neuron_application *app_dev = to_neuron_application(dev);
	char *input;
	char *cp;

	if (blk_dev->bdev_name) {
		pr_err("No permission to write\n");
		return count;
	}

	if (count >= (PAGE_SIZE - 1))
		return -EINVAL;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	cp = strnchr(input, count, '\n');
	if (cp)
		*cp = '\0';

	if (strlen(input)) {
		blk_dev->bdev_name = input;
	} else {
		kfree(input);
		 blk_dev->bdev_name = NULL;
	}

	if (get_blk_dev(app_dev)) {
		pr_err("Error getting block device %s\n", blk_dev->bdev_name);
		blk_dev->bdev_name = NULL;
	}

	return count;
}
static DEVICE_ATTR_RW(blk_name);

static enum neuron_block_resp_status block_to_neuron_status(struct bio *bio)
{
	enum neuron_block_resp_status status;

	switch (bio->bi_status) {

	case BLK_STS_OK:
		status = BLOCK_RESP_SUCCESS;
		break;
	case BLK_STS_TIMEOUT:
		status = BLOCK_RESP_TIMEOUT;
		break;
	case BLK_STS_RESOURCE:
		status = BLOCK_RESP_NOMEM;
		break;
	case BLK_STS_NOTSUPP:
		status = BLOCK_RESP_OPNOTSUPP;
		break;
	case BLK_STS_IOERR:
	default:
		status = BLOCK_RESP_IOERROR;
		break;
	}

	return status;
}

static int app_blk_server_get_response(struct neuron_application *app_dev,
				       uint32_t *id,
				       enum neuron_block_resp_status *status,
				       struct sk_buff **skb)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct bio_priv *bio_priv;
	struct bio *bio;

	spin_lock_irq(&blk_dev->list_lock);
	if (bio_list_empty(&blk_dev->bio_list)) {
		pr_debug("Get response EAGAIN\n");
		spin_unlock_irq(&blk_dev->list_lock);
		return -EAGAIN;
	}

	bio = bio_list_pop(&blk_dev->bio_list);
	spin_unlock_irq(&blk_dev->list_lock);

	*status = block_to_neuron_status(bio);

	bio_priv = bio->bi_private;
	*id = bio_priv->id;

	if (bio_priv->skb == NULL) {
		*skb = NULL;
	} else if (bio_op(bio) != REQ_OP_READ) {
		consume_skb(bio_priv->skb);
		*skb = NULL;
	} else {
		*skb = bio_priv->skb;
	}

	bio_put(bio);
	kfree(bio_priv);

	return 0;
}

static void app_blk_server_request_done(struct bio *bio)
{
	struct bio_priv *bio_priv = bio->bi_private;
	struct neuron_application *app_dev = bio_priv->app_dev;
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	unsigned long flags;

	spin_lock_irqsave(&blk_dev->list_lock, flags);
	bio_list_add(&blk_dev->bio_list, bio);
	spin_unlock_irqrestore(&blk_dev->list_lock, flags);

	neuron_app_wakeup(app_dev, NEURON_BLOCK_SERVER_EVENT_RESPONSE);
}

static int add_bio_pages_from_frags(struct sk_buff *skb, struct bio *bio)
{
	unsigned int bytes;
	struct page *page;
	uint32_t offset;
	int result;
	int i;

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		bytes = skb_frag_size(frag);
		page = skb_frag_page(frag);
		offset = skb_frag_off(frag);

		result = bio_add_page(bio, page, bytes, offset);
		if (result < bytes) {
			pr_err("Error adding page to bio. result:%d\n", result);
			kfree_skb(skb);
			bio_io_error(bio);
			return -EIO;
		}
	}

	return 0;
}

static int app_blk_server_prepare_bio(struct bio **bio,
				      struct neuron_application *app_dev,
				      unsigned int op,
				      uint32_t req_id,
				      uint64_t start,
				      uint32_t nr_iovecs,
				      uint16_t flags,
				      struct sk_buff *skb)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct bio_priv *bio_priv;
	bool flush_flag;
	bool commit_flag;
	bool sync_flag;
	unsigned int op_flags;

	bio_priv = kzalloc(sizeof(struct bio_priv), GFP_KERNEL);
	if (!bio_priv)
		return -ENOMEM;

	*bio = bio_alloc(GFP_KERNEL, nr_iovecs);
	if (!*bio)
		return -ENOMEM;

	bio_priv->app_dev = app_dev;
	bio_priv->id = req_id;
	bio_priv->skb = skb;
	(*bio)->bi_private = bio_priv;

	bio_set_dev(*bio, blk_dev->bdev);
	(*bio)->bi_iter.bi_sector = start;
	(*bio)->bi_end_io = app_blk_server_request_done;

	flush_flag = (flags & NEURON_BLOCK_REQ_PREFLUSH);
	commit_flag = (flags & NEURON_BLOCK_REQ_FUA);
	sync_flag = (flags & NEURON_BLOCK_REQ_SYNC);

	op_flags = (flush_flag ? REQ_PREFLUSH : 0) |
		 (commit_flag ? REQ_FUA : 0) |
		 (sync_flag ? REQ_SYNC : 0);

	bio_set_op_attrs(*bio, op, op_flags);

	if (skb) {
		struct sk_buff *iter;

		add_bio_pages_from_frags(skb, *bio);

		skb_walk_frags(skb, iter)
			add_bio_pages_from_frags(iter, *bio);
	}
	//blk_recount_segments(bdev_get_queue(blk_dev->bdev), *bio);

	return 0;
}

static int app_blk_server_do_read(struct neuron_application *app_dev,
				  uint32_t req_id,
				  uint64_t start,
				  uint32_t num,
				  uint16_t flags)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct bio *bio;
	int size;
	uint32_t nr_iovecs;
	int ret;
	struct sk_buff *skb = NULL;

	size = num * blk_dev->sector_size;
	if (num)
		nr_iovecs = 1 + ((size - 1) >> PAGE_SHIFT);
	else
		nr_iovecs = 0;

	if (num)
		skb = neuron_alloc_pskb(size, GFP_KERNEL);

	ret = app_blk_server_prepare_bio(&bio, app_dev, REQ_OP_READ, req_id,
					 start, nr_iovecs, flags, skb);
	if (ret)
		return ret;

	generic_make_request(bio);

	return 0;
}

static int app_blk_server_do_write(struct neuron_application *app_dev,
				   uint32_t req_id,
				   uint64_t start,
				   uint32_t num,
				   uint16_t flags,
				   struct sk_buff *skb)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct bio *bio;
	int size;
	int nr_iovecs;
	int ret;

	size = num * blk_dev->sector_size;
	if (num)
		nr_iovecs = 1 + ((size - 1) >> PAGE_SHIFT);
	else
		nr_iovecs = 0;

	ret = app_blk_server_prepare_bio(&bio, app_dev, REQ_OP_WRITE, req_id,
					 start, nr_iovecs, flags, skb);
	if (ret)
		return ret;

	generic_make_request(bio);

	return 0;
}

static int app_blk_server_do_discard(struct neuron_application *app_dev,
				     uint32_t req_id,
				     uint64_t start,
				     uint32_t num,
				     uint16_t flags)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	uint32_t size;
	struct bio *bio;
	int ret;

	size = num * blk_dev->sector_size;

	ret = app_blk_server_prepare_bio(&bio, app_dev, REQ_OP_DISCARD, req_id,
					 start, 0, flags, NULL);
	if (ret)
		return ret;

	generic_make_request(bio);

	return 0;
}

static int app_blk_server_do_secure_erase(struct neuron_application *app_dev,
					  uint32_t req_id,
					  uint64_t start,
					  uint32_t num,
					  uint16_t flags)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	uint32_t size;
	struct bio *bio;
	int ret;

	size = num * blk_dev->sector_size;

	ret = app_blk_server_prepare_bio(&bio, app_dev, REQ_OP_SECURE_ERASE,
					 req_id, start, 0, flags, NULL);
	if (ret)
		return ret;

	generic_make_request(bio);

	return 0;
}

static int app_blk_server_do_write_same(struct neuron_application *app_dev,
					uint32_t req_id,
					uint64_t start,
					uint32_t num,
					uint16_t flags,
					struct sk_buff *skb)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct bio *bio;
	int size;
	int ret;

	size = num * blk_dev->sector_size;

	ret = app_blk_server_prepare_bio(&bio, app_dev, REQ_OP_WRITE_SAME,
					 req_id, start, 1, flags, skb);
	if (ret)
		return ret;

	generic_make_request(bio);

	return 0;
}

static int app_blk_server_do_write_zeroes(struct neuron_application *app_dev,
					  uint32_t req_id,
					  uint64_t start,
					  uint32_t num,
					  uint16_t flags)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct bio *bio;
	int size;
	int ret;

	size = num * blk_dev->sector_size;

	ret = app_blk_server_prepare_bio(&bio, app_dev, REQ_OP_WRITE_ZEROES,
					 req_id, start, 0, flags, NULL);
	if (ret)
		return ret;
	generic_make_request(bio);

	return 0;
}

static int match_dev_name(struct device *dev, const void *name)
{
	if (!dev_name(dev))
		return 0;
	return !strcmp(dev_name(dev), (char *)name);
}

static int app_blk_server_get_bd_params(struct neuron_application *app_dev,
					const struct neuron_block_param **param)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct neuron_block_param *params;
	struct queue_limits limits;
	int err;
	const char *label_prop;
	const char *out_values;

	if (!blk_dev->bdev)
		return -EAGAIN;

	limits = bdev_get_queue(blk_dev->bdev)->limits;

	params = kzalloc(sizeof(struct neuron_block_param) +
			 sizeof(u8)*PARTITION_META_INFO_VOLNAMELTH,
			 GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	params->logical_block_size = bdev_logical_block_size(blk_dev->bdev);
	params->physical_block_size = bdev_physical_block_size(blk_dev->bdev);
	blk_dev->sector_size = bdev_logical_block_size(blk_dev->bdev);

	params->alignment_offset = blk_dev->bdev->bd_part->alignment_offset;
	params->read_only = bdev_read_only(blk_dev->bdev);
	params->num_device_sectors = blk_dev->bdev->bd_part->nr_sects;

	params->discard_max_sectors = limits.max_discard_sectors;
	params->discard_max_hw_sectors = limits.max_hw_discard_sectors;
	params->discard_granularity = limits.discard_granularity;

	params->wc_flag = test_bit(QUEUE_FLAG_WC,
				&bdev_get_queue(blk_dev->bdev)->queue_flags);
	params->fua_flag = test_bit(QUEUE_FLAG_FUA,
				&bdev_get_queue(blk_dev->bdev)->queue_flags);

	err = of_property_read_string(app_dev->dev.of_node, "label",
				      &label_prop);
	if (!err) {
		err = strscpy(params->label, label_prop,
			      PARTITION_META_INFO_VOLNAMELTH);
	} else {
		if (blk_dev->bdev->bd_disk->part0.info &&
		    strlen(blk_dev->bdev->bd_disk->part0.info->volname)) {
			memcpy(params->label,
			       blk_dev->bdev->bd_disk->part0.info->volname,
			       sizeof(u8)*PARTITION_META_INFO_VOLNAMELTH);
		} else {
			memcpy(params->label, blk_dev->bdev_name,
			       strlen(blk_dev->bdev_name));
		}
	}

	err = of_property_read_string(app_dev->dev.of_node, "uuid-string",
				      &out_values);
	if (err < 0) {
		if (blk_dev->bdev->bd_disk->part0.info) {
			const char *uuid;

			uuid = blk_dev->bdev->bd_disk->part0.info->uuid;
			err = uuid_parse(uuid, &params->uuid);
			if (err)
				pr_err("Invalid uuid.\n");
		} else
			uuid_copy(&params->uuid, &uuid_null);
	} else {
		err = uuid_parse(out_values, &params->uuid);
		if (err)
			pr_err("Invalid uuid.\n");
	}

	*param = params;

	return 0;
}

static int get_blk_dev(struct neuron_application *app_dev)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);
	struct device *dev;

	dev = class_find_device(&block_class, NULL,
				(const void *)blk_dev->bdev_name,
				match_dev_name);
	if (!dev) {
		pr_err("Device with name %s not found.\n", blk_dev->bdev_name);
		return -ENODEV;
	}

	blk_dev->bdev = blkdev_get_by_dev(dev->devt,
					  FMODE_READ|FMODE_WRITE, NULL);
	if (IS_ERR(blk_dev->bdev)) {
		pr_err("Getting block device %s by device number failed.\n",
		       blk_dev->bdev_name);
		return PTR_ERR(blk_dev->bdev);
	}

	neuron_app_wakeup(app_dev, NEURON_BLOCK_SERVER_EVENT_BD_PARAMS);

	return 0;
}

static int app_block_server_probe(struct neuron_application *app_dev)
{
	struct block_server_dev *blk_dev;
	int err;
	int ret;
	const char *name_prop;

	blk_dev = kzalloc(sizeof(*blk_dev), GFP_KERNEL);
	if (!blk_dev)
		return -ENOMEM;

	bio_list_init(&blk_dev->bio_list);
	spin_lock_init(&blk_dev->list_lock);

	dev_set_drvdata(&app_dev->dev, blk_dev);

	ret = device_create_file(&app_dev->dev, &dev_attr_blk_name);
	if (ret) {
		pr_err("Sysfs creation failed with error: %d\n", ret);
		goto fail;
	}

	err = of_property_read_string(app_dev->dev.of_node, "device-name",
				      &name_prop);
	if (!err) {

		blk_dev->bdev_name = kzalloc(strlen(name_prop) + 1,
					     GFP_KERNEL);
		if (!blk_dev->bdev_name) {
			ret = -ENOMEM;
			goto fail;
		}

		err = strscpy(blk_dev->bdev_name, name_prop,
			      strlen(name_prop) + 1);
	} else {
		pr_err("No device name found in device tree.\n");
		return 0;
	}

	if (get_blk_dev(app_dev)) {
		ret = -EPROBE_DEFER;
		goto fail;
	}

	return 0;
fail:
	device_remove_file(&app_dev->dev, &dev_attr_blk_name);
	dev_set_drvdata(&app_dev->dev, NULL);
	kfree(blk_dev);
	return ret;
}

static void app_block_server_remove(struct neuron_application *app_dev)
{
	struct block_server_dev *blk_dev = dev_get_drvdata(&app_dev->dev);

	device_remove_file(&app_dev->dev, &dev_attr_blk_name);
	dev_set_drvdata(&app_dev->dev, NULL);
	kfree(blk_dev);
}

static struct neuron_block_app_server_driver app_block_server_drv = {
	.base = {
		.driver = {
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
			.of_match_table = app_block_server_match,
		},
		.protocol_driver = &protocol_server_block_driver,
		.probe  = app_block_server_probe,
		.remove = app_block_server_remove,
	},
	.get_bd_params = app_blk_server_get_bd_params,
	.get_response = app_blk_server_get_response,
	.do_read = app_blk_server_do_read,
	.do_write = app_blk_server_do_write,
	.do_discard = app_blk_server_do_discard,
	.do_secure_erase = app_blk_server_do_secure_erase,
	.do_write_same = app_blk_server_do_write_same,
	.do_write_zeroes = app_blk_server_do_write_zeroes,
};

static int __init block_server_init(void)
{
	int ret = 0;

	ret = neuron_register_app_driver(&app_block_server_drv.base);
	if (ret < 0) {
		pr_err("Failed to register driver\n");
		return ret;
	}

	return 0;
}

static void block_server_exit(void)
{
	neuron_unregister_app_driver(&app_block_server_drv.base);
}

module_init(block_server_init);
module_exit(block_server_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron block server module");
