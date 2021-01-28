/*
 * Copyright (C) 2015-2016 Intel Corp. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <linux/rpmb.h>
#include "rpmb-cdev.h"

static DEFINE_IDA(rpmb_ida);

/**
 * rpmb_dev_get - increase rpmb device ref counter
 *
 * @rdev: rpmb device
 */
struct rpmb_dev *rpmb_dev_get(struct rpmb_dev *rdev)
{
	return get_device(&rdev->dev) ? rdev : NULL;
}
EXPORT_SYMBOL_GPL(rpmb_dev_get);

/**
 * rpmb_dev_put - decrease rpmb device ref counter
 *
 * @rdev: rpmb device
 */
void rpmb_dev_put(struct rpmb_dev *rdev)
{
	put_device(&rdev->dev);
}
EXPORT_SYMBOL_GPL(rpmb_dev_put);

static int rpmb_request_verify(struct rpmb_dev *rdev, struct rpmb_data *rpmbd)
{
	u16 req_type, block_count, addr;
	u32 wc;
	struct rpmb_cmd *in_cmd = &rpmbd->icmd;
	struct rpmb_cmd *out_cmd = &rpmbd->ocmd;

	if (!in_cmd->frames || !in_cmd->nframes ||
	    !out_cmd->frames || !out_cmd->nframes)
		return -EINVAL;

	req_type = be16_to_cpu(in_cmd->frames[0].req_resp);
	block_count = be16_to_cpu(in_cmd->frames[0].block_count);
	addr = be16_to_cpu(in_cmd->frames[0].addr);
	wc = be32_to_cpu(in_cmd->frames[0].write_counter);

	if (rpmbd->req_type != req_type) {
		dev_notice(&rdev->dev, "rpmb req type doesn't match 0x%04X = 0x%04X\n",
			req_type, rpmbd->req_type);
		return -EINVAL;
	}

	switch (req_type) {
	case RPMB_PROGRAM_KEY:
		dev_dbg(&rdev->dev, "rpmb program key = 0x%1x blk = %d\n",
			req_type, block_count);
		break;
	case RPMB_GET_WRITE_COUNTER:
		dev_dbg(&rdev->dev, "rpmb get write counter = 0x%1x blk = %d\n",
			req_type, block_count);

		break;
	case RPMB_WRITE_DATA:
		dev_dbg(&rdev->dev, "rpmb write data = 0x%1x blk = %d addr = %d, wc = %d (0x%x)\n",
			req_type, block_count, addr, wc, wc);

		if (rdev->ops->reliable_wr_cnt &&
		    block_count > rdev->ops->reliable_wr_cnt) {
			dev_notice(&rdev->dev, "rpmb write data: block count %u > reliable wr count %u\n",
				block_count, rdev->ops->reliable_wr_cnt);
			return -EINVAL;
		}

		if (block_count > in_cmd->nframes) {
			dev_notice(&rdev->dev, "rpmb write data: block count %u > in frame count %u\n",
				block_count, in_cmd->nframes);
			return -EINVAL;
		}
		break;
	case RPMB_READ_DATA:
		dev_dbg(&rdev->dev, "rpmb read data = 0x%1x blk = %d addr = %d\n",
			req_type, block_count, addr);

		if (block_count > out_cmd->nframes) {
			dev_notice(&rdev->dev, "rpmb read data: block count %u > out frame count %u\n",
				block_count, out_cmd->nframes);
			return -EINVAL;
		}
		break;
	case RPMB_RESULT_READ:
		/* Internal command not supported */
		dev_notice(&rdev->dev, "NOTSUPPORTED rpmb resut read = 0x%1x blk = %d\n",
			req_type, block_count);
		return -EOPNOTSUPP;

	default:
		dev_notice(&rdev->dev, "Error rpmb invalid command = 0x%1x blk = %d\n",
			req_type, block_count);
		return -EINVAL;
	}

	return 0;
}

/**
 * rpmb_cmd_fixup - fixup rpmb command
 *
 * @rdev: rpmb device
 * @cmds: rpmb command list
 * @ncmds: number of commands
 *
 */
static void rpmb_cmd_fixup(struct rpmb_dev *rdev,
			   struct rpmb_cmd *cmds, u32 ncmds)
{
	int i;

	if (rdev->ops->type != RPMB_TYPE_EMMC)
		return;

	/* Fixup RPMB_READ_DATA specific to eMMC
	 * The block count of the RPMB read operation is not indicated
	 * in the original RPMB Data Read Request packet.
	 * This is different then implementation for other protocol
	 * standards.
	 */
	for (i = 0; i < ncmds; i++)
		if (cmds->frames->req_resp == cpu_to_be16(RPMB_READ_DATA)) {
			dev_dbg(&rdev->dev, "Fixing up READ_DATA frame to block_count=0\n");
			cmds->frames->block_count = 0;
		}
}

/**
 * rpmb_cmd_seq - send RPMB command sequence
 *
 * @rdev: rpmb device
 * @cmds: rpmb command list
 * @ncmds: number of commands
 *
 * Return: 0 on success
 *         -EINVAL on wrong parameters
 *         -EOPNOTSUPP if device doesn't support the requested operation
 *         < 0 if the operation fails
 */
int rpmb_cmd_seq(struct rpmb_dev *rdev, struct rpmb_cmd *cmds, u32 ncmds)
{
	int err;

	if (!rdev || !cmds || !ncmds)
		return -EINVAL;

	mutex_lock(&rdev->lock);
	err = -EOPNOTSUPP;
	if (rdev->ops && rdev->ops->cmd_seq) {
		rpmb_cmd_fixup(rdev, cmds, ncmds);
		err = rdev->ops->cmd_seq(rdev->dev.parent, cmds, ncmds);
	}
	mutex_unlock(&rdev->lock);
	return err;
}
EXPORT_SYMBOL_GPL(rpmb_cmd_seq);

static void rpmb_cmd_set(struct rpmb_cmd *cmd, u32 flags,
			 struct rpmb_frame *frames, u32 nframes)
{
	cmd->flags = flags;
	cmd->frames = frames;
	cmd->nframes = nframes;
}
#ifdef RPMB_DEBUG
static void rpmb_dump_frame(u8 *data_frame)
{
	pr_notice("mac, frame[196] = 0x%x\n", data_frame[196]);
	pr_notice("mac, frame[197] = 0x%x\n", data_frame[197]);
	pr_notice("mac, frame[198] = 0x%x\n", data_frame[198]);
	pr_notice("data,frame[228] = 0x%x\n", data_frame[228]);
	pr_notice("data,frame[229] = 0x%x\n", data_frame[229]);
	pr_notice("nonce, frame[484] = 0x%x\n", data_frame[484]);
	pr_notice("nonce, frame[485] = 0x%x\n", data_frame[485]);
	pr_notice("nonce, frame[486] = 0x%x\n", data_frame[486]);
	pr_notice("nonce, frame[487] = 0x%x\n", data_frame[487]);
	pr_notice("wc, frame[500] = 0x%x\n", data_frame[500]);
	pr_notice("wc, frame[501] = 0x%x\n", data_frame[501]);
	pr_notice("wc, frame[502] = 0x%x\n", data_frame[502]);
	pr_notice("wc, frame[503] = 0x%x\n", data_frame[503]);
	pr_notice("addr, frame[504] = 0x%x\n", data_frame[504]);
	pr_notice("addr, frame[505] = 0x%x\n", data_frame[505]);
	pr_notice("blkcnt,frame[506] = 0x%x\n", data_frame[506]);
	pr_notice("blkcnt,frame[507] = 0x%x\n", data_frame[507]);
	pr_notice("result, frame[508] = 0x%x\n", data_frame[508]);
	pr_notice("result, frame[509] = 0x%x\n", data_frame[509]);
	pr_notice("type, frame[510] = 0x%x\n", data_frame[510]);
	pr_notice("type, frame[511] = 0x%x\n", data_frame[511]);
}
#endif
/**
 * rpmb_cmd_req - send rpmb request command
 *
 * @rdev: rpmb device
 * @rpmbd: rpmb request data
 *
 * Return: 0 on success
 *         -EINVAL on wrong parameters
 *         -EOPNOTSUPP if device doesn't support the requested operation
 *         < 0 if the operation fails
 */
int rpmb_cmd_req(struct rpmb_dev *rdev, struct rpmb_data *rpmbd)
{
	struct rpmb_cmd cmd[3];
	struct rpmb_frame *res_frame;
	u32 cnt_in, cnt_out;
	u32 ncmds;
	u16 type;
	int ret;

	if (!rdev || !rpmbd) {
		pr_notice("[RPMB] %s, -EINVAL in line %d!\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	ret = rpmb_request_verify(rdev, rpmbd);
	if (ret)
		return ret;

	if (!rdev->ops || !rdev->ops->cmd_seq)
		return -EOPNOTSUPP;
#ifdef RPMB_DEBUG
	rpmb_dump_frame((u8 *)(rpmbd->icmd.frames));
#endif
	cnt_in = rpmbd->icmd.nframes;
	cnt_out = rpmbd->ocmd.nframes;
	type = rpmbd->req_type;
	switch (type) {
	case RPMB_PROGRAM_KEY:
		cnt_in = 1;
		cnt_out = 1;
		/* fall through */
	case RPMB_WRITE_DATA:
		rpmb_cmd_set(&cmd[0], RPMB_F_WRITE | RPMB_F_REL_WRITE,
			     rpmbd->icmd.frames, cnt_in);

		res_frame = rpmbd->ocmd.frames;
		memset(res_frame, 0, sizeof(*res_frame));
		res_frame->req_resp = cpu_to_be16(RPMB_RESULT_READ);
		rpmb_cmd_set(&cmd[1], RPMB_F_WRITE, res_frame, 1);

		rpmb_cmd_set(&cmd[2], 0, rpmbd->ocmd.frames, cnt_out);
		ncmds = 3;
		break;
	case RPMB_GET_WRITE_COUNTER:
		cnt_in = 1;
		cnt_out = 1;
		/* fall through */
	case RPMB_READ_DATA:
		rpmb_cmd_set(&cmd[0], RPMB_F_WRITE, rpmbd->icmd.frames, cnt_in);
		rpmb_cmd_set(&cmd[1], 0, rpmbd->ocmd.frames, cnt_out);
		ncmds = 2;
		break;
	default:
		pr_notice("[RPMB] %s, -EINVAL in line %d!\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&rdev->lock);
	ret = rdev->ops->cmd_seq(rdev->dev.parent, cmd, ncmds);
	mutex_unlock(&rdev->lock);
#ifdef RPMB_DEBUG
	rpmb_dump_frame((u8 *)(rpmbd->ocmd.frames));
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(rpmb_cmd_req);

u16 rpmb_get_rw_size(struct rpmb_dev *rdev)
{
	return rdev->ops->reliable_wr_cnt;
}
EXPORT_SYMBOL_GPL(rpmb_get_rw_size);

static void rpmb_dev_release(struct device *dev)
{
	struct rpmb_dev *rdev = to_rpmb_dev(dev);

	ida_simple_remove(&rpmb_ida, rdev->id);
	kfree(rdev);
}

struct class rpmb_class = {
	.name = "rpmb_dummy",
	.owner = THIS_MODULE,
	.dev_release = rpmb_dev_release,
};
EXPORT_SYMBOL(rpmb_class);

/**
 * rpmb_dev_find_device - return first matching rpmb device
 *
 * @data: data for the match function
 * @match: the matching function
 *
 * Return: matching rpmb device or NULL on failure
 */
struct rpmb_dev *rpmb_dev_find_device(void *data,
		     int (*match)(struct device *dev, const void *data))
{
	struct device *dev;

	dev = class_find_device(&rpmb_class, NULL, data, match);

	return dev ? to_rpmb_dev(dev) : NULL;
}
EXPORT_SYMBOL_GPL(rpmb_dev_find_device);

static int match_by_type(struct device *dev, const void *data)
{
	struct rpmb_dev *rdev = to_rpmb_dev(dev);
	enum rpmb_type *type = (enum rpmb_type *)data;

	return (*type == RPMB_TYPE_ANY || rdev->ops->type == *type);
}

/**
 * rpmb_dev_get_by_type - return first registered rpmb device
 *      with matching type.
 *      If run with RPMB_TYPE_ANY the first an probably only
 *      device is returned
 *
 * @type: rpbm underlying device type
 *
 * Return: matching rpmb device or NULL/ERR_PTR on failure
 */
struct rpmb_dev *rpmb_dev_get_by_type(enum rpmb_type type)
{
	if (type > RPMB_TYPE_MAX)
		return ERR_PTR(-EINVAL);

	return rpmb_dev_find_device(&type, match_by_type);
}
EXPORT_SYMBOL_GPL(rpmb_dev_get_by_type);

static int match_by_parent(struct device *dev, const void *data)
{
	const struct device *parent = data;

	return (parent && dev->parent == parent);
}

/**
 * rpmb_dev_find_by_device - retrieve rpmb device from the parent device
 *
 * @parent: parent device of the rpmb device
 *
 * Return: NULL if there is no rpmb device associated with the parent device
 */
struct rpmb_dev *rpmb_dev_find_by_device(struct device *parent)
{
	if (!parent)
		return NULL;

	return rpmb_dev_find_device(parent, match_by_parent);
}
EXPORT_SYMBOL_GPL(rpmb_dev_find_by_device);

static ssize_t type_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct rpmb_dev *rdev = to_rpmb_dev(dev);
	ssize_t ret;

	switch (rdev->ops->type) {
	case RPMB_TYPE_EMMC:
		ret = sprintf(buf, "EMMC\n");
		break;
	case RPMB_TYPE_UFS:
		ret = sprintf(buf, "UFS\n");
		break;
	case RPMB_TYPE_SIM:
		ret = sprintf(buf, "SIM\n");
		break;
	default:
		ret = sprintf(buf, "UNKNOWN\n");
		break;
	}

	return ret;
}
static DEVICE_ATTR_RO(type);

static ssize_t id_read(struct file *file, struct kobject *kobj,
		       struct bin_attribute *attr, char *buf,
		       loff_t off, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct rpmb_dev *rdev = to_rpmb_dev(dev);
	size_t sz = min_t(size_t, rdev->ops->dev_id_len, PAGE_SIZE);

	if (!rdev->ops->dev_id)
		return 0;

	return memory_read_from_buffer(buf, count, &off, rdev->ops->dev_id, sz);
}
static BIN_ATTR_RO(id, 0);

static ssize_t reliable_wr_cnt_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct rpmb_dev *rdev = to_rpmb_dev(dev);

	return sprintf(buf, "%u\n", rdev->ops->reliable_wr_cnt);
}
static DEVICE_ATTR_RO(reliable_wr_cnt);

static struct attribute *rpmb_attrs[] = {
	&dev_attr_type.attr,
	&dev_attr_reliable_wr_cnt.attr,
	NULL,
};

static struct bin_attribute *rpmb_bin_attributes[] = {
	&bin_attr_id,
	NULL,
};

static struct attribute_group rpmb_attr_group = {
	.attrs = rpmb_attrs,
	.bin_attrs = rpmb_bin_attributes,
};

static const struct attribute_group *rpmb_attr_groups[] = {
	&rpmb_attr_group,
	NULL
};

/**
 * rpmb_dev_unregister - unregister RPMB partition from the RPMB subsystem
 *
 * @dev: parent device of the rpmb device
 */
int rpmb_dev_unregister(struct device *dev)
{
	struct rpmb_dev *rdev;

	if (!dev)
		return -EINVAL;

	rdev = rpmb_dev_find_by_device(dev);
	if (!rdev) {
		dev_notice(dev, "no disk found %s\n", dev_name(dev->parent));
		return -ENODEV;
	}

	rpmb_dev_put(rdev);

	mutex_lock(&rdev->lock);
	rpmb_cdev_del(rdev);
	device_del(&rdev->dev);
	mutex_unlock(&rdev->lock);

	rpmb_dev_put(rdev);

	return 0;
}
EXPORT_SYMBOL_GPL(rpmb_dev_unregister);

/**
 * rpmb_dev_register - register RPMB partition with the RPMB subsystem
 *
 * @dev: storage device of the rpmb device
 * @ops: device specific operations
 */
struct rpmb_dev *rpmb_dev_register(struct device *dev,
				   const struct rpmb_ops *ops)
{
	struct rpmb_dev *rdev;
	int id;
	int ret;

	if (!dev || !ops)
		return ERR_PTR(-EINVAL);

	if (!ops->cmd_seq)
		return ERR_PTR(-EINVAL);

	if (ops->type == RPMB_TYPE_ANY || ops->type > RPMB_TYPE_MAX)
		return ERR_PTR(-EINVAL);

	rdev = kzalloc(sizeof(*rdev), GFP_KERNEL);
	if (!rdev)
		return ERR_PTR(-ENOMEM);

	id = ida_simple_get(&rpmb_ida, 0, 0, GFP_KERNEL);
	if (id < 0) {
		ret = id;
		goto exit;
	}

	mutex_init(&rdev->lock);
	rdev->ops = ops;
	rdev->id = id;

	dev_set_name(&rdev->dev, "rpmb_dummy%d", id);
	rdev->dev.class = &rpmb_class;
	rdev->dev.parent = dev;
	rdev->dev.groups = rpmb_attr_groups;

	rpmb_cdev_prepare(rdev);

	ret = device_register(&rdev->dev);
	if (ret)
		goto exit;

	rpmb_cdev_add(rdev);

	dev_dbg(&rdev->dev, "registered disk\n");

	return rdev;

exit:
	if (id >= 0)
		ida_simple_remove(&rpmb_ida, id);
	kfree(rdev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(rpmb_dev_register);

static int __init rpmb_init(void)
{
	ida_init(&rpmb_ida);
	class_register(&rpmb_class);
	return rpmb_cdev_init();
}

static void __exit rpmb_exit(void)
{
	rpmb_cdev_exit();
	class_unregister(&rpmb_class);
	ida_destroy(&rpmb_ida);
}

subsys_initcall(rpmb_init);
module_exit(rpmb_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("RPMB class");
MODULE_LICENSE("GPL");
