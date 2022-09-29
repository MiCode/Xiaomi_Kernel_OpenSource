// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * Author: ChenHung Yang <chenhung.yang@mediatek.com>
 */

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/module.h>

#include <aee.h>

#include "mtk-aov-drv.h"
#include "mtk-aov-aee.h"
#include "mtk-aov-core.h"

int aov_notify_aee(void)
{
	pr_info("%s+\n", __func__);
	pr_info("%s-\n", __func__);

	return 0;
}

int proc_open(struct inode *inode, struct file *file)
{
	struct mtk_aov *aov_dev = aov_core_get_device();

	const char *name;

	(void)try_module_get(THIS_MODULE);

	name = file->f_path.dentry->d_name.name;
	if (!strcmp(name, "kernel")) {
		file->private_data = &aov_dev->aee_info.buffer;
	} else {
		module_put(THIS_MODULE);
		return -EPERM;
	}

	pr_debug("%s: %s", __func__, name);

	return 0;
}

static ssize_t proc_read(struct file *file, char __user *buf, size_t length,
	loff_t *ppos)
{
	struct proc_info *info = (struct proc_info *)file->private_data;
	int bytes, remain;

	remain = info->count - *ppos;
	remain = (remain > length) ? length : remain;
	if (remain <= 0) {
		pr_debug("reach aov kernel node end");
		info->count = 0;
		return 0;
	}

	bytes = remain - copy_to_user(buf, info->buffer + *ppos, remain);
	*ppos += bytes;

	pr_debug("read aov kernel node bytes=%d, pos=%d\n", bytes, (int)*ppos);

	return bytes;
}

int proc_close(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static const struct proc_ops aee_ops = {
	.proc_open = proc_open,
	.proc_read  = proc_read,
	.proc_write = NULL,
	.proc_release = proc_close
};

int aov_aee_init(struct mtk_aov *aov_dev)
{
	struct aov_aee *aee_info = &aov_dev->aee_info;
	struct aee_record *record = &aee_info->record;

	// aed_set_extra_func(aov_notify_aee);

	aee_info->entry = proc_mkdir("mtk_aov_debug", NULL);
	if (aee_info->entry) {
		aee_info->buffer.count = 0;

		aee_info->kernel = proc_create(
			"kernel", 0444, aee_info->entry, &aee_ops);
	} else {
		pr_info("%s: failed to create aov debug node\n", __func__);
	}

	spin_lock_init(&record->lock);
	record->head = 0;
	record->tail = 0;

	return 0;
}

int aov_aee_record(struct mtk_aov *aov_dev,
	int op_seq, enum aov_op op_code)
{
	struct aov_aee *aee_info = &aov_dev->aee_info;
	struct aee_record *record = &aee_info->record;
	unsigned long flag;
	struct op_data *data;

	dev_dbg(aov_dev->dev, "%s+\n", __func__);

	spin_lock_irqsave(&record->lock, flag);
	data = &record->data[record->tail++];
	if (record->tail >= AOV_AEE_MAX_RECORD_COUNT)
		record->tail = 0;

	if (record->head == record->tail) {
		record->head++;
		if (record->head >= AOV_AEE_MAX_RECORD_COUNT)
			record->head = 0;
	}
	spin_unlock_irqrestore(&record->lock, flag);

	data->op_time = ktime_get_boottime_ns();
	data->op_seq  = op_seq;
	data->op_code = op_code;

	dev_dbg(aov_dev->dev, "%s: record time(%lld), seq(%d), cmd(%d)\n",
		__func__, data->op_time, data->op_seq, data->op_code);

	dev_dbg(aov_dev->dev, "%s-\n", __func__);

	return 0;
}

int aov_aee_flush(struct mtk_aov *aov_dev)
{
	struct aov_aee *aee_info = &aov_dev->aee_info;
	struct aee_record *record = &aee_info->record;
	unsigned long flag;
	struct proc_info *node;
	int count;
	int index;
	int offset;
	int remain;
	int length;

	dev_info(aov_dev->dev, "%s+\n", __func__);

	spin_lock_irqsave(&record->lock, flag);
	index = record->head;
	count = record->tail - record->head;
	if (count < 0)
		count += AOV_AEE_MAX_RECORD_COUNT;
	spin_unlock_irqrestore(&record->lock, flag);

	dev_info(aov_dev->dev, "%s: flush index(%d), count(%d)\n",
		__func__, index, count);

	// flush to node buffer
	node = &aee_info->buffer;
	offset = node->count;
	for (; count > 0; index++, count--) {
		if (index >= AOV_AEE_MAX_RECORD_COUNT)
			index = 0;

		remain = AOV_AEE_MAX_BUFFER_SIZE - offset;

		dev_dbg(aov_dev->dev, "%s: flush offset(%d), remain(%d)\n",
			__func__, offset, remain);

		if ((index >= 0) && (index < AOV_AEE_MAX_RECORD_COUNT) &&
				(offset >= 0) && (offset < AOV_AEE_MAX_BUFFER_SIZE) &&
				(remain > 0)) {
			dev_dbg(aov_dev->dev, "%s: flush time(%lld), seq(%d), cmd(%d)\n",
			__func__, record->data[index].op_time,
				record->data[index].op_seq, record->data[index].op_code);

			length = snprintf(&(node->buffer[offset]), remain,
				"time(%lld), seq(%d), cmd(%d)\n",
				record->data[index].op_time, record->data[index].op_seq,
				record->data[index].op_code);
			if (length < 0) {
				dev_info(aov_dev->dev, "%s: failed to call snprintf(%d)",
					__func__, length);
				break;
			}
			offset += length;
		} else {
			break;
		}
	}

	node->count = ((offset > AOV_AEE_MAX_BUFFER_SIZE) ?
		AOV_AEE_MAX_BUFFER_SIZE : offset);

	dev_info(aov_dev->dev, "%s-\n", __func__);

	return 0;
}

int aov_aee_uninit(struct mtk_aov *aov_dev)
{
	struct aov_aee *aee_info = &aov_dev->aee_info;

	if (aee_info->kernel)
		proc_remove(aee_info->kernel);

	if (aee_info->entry)
		proc_remove(aee_info->entry);

	return 0;
}
