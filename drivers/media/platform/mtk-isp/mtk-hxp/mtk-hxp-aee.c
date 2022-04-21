// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/module.h>

#include <aee.h>

#include "mtk-hxp-drv.h"
#include "mtk-hxp-aee.h"
#include "mtk-hxp-core.h"

int hxp_notify_aee(void)
{
	//unsigned long flag;
	//struct msg *msg;
	//char dummy;

	pr_info("HCP trigger AEE dump+\n");

	//msg = msg_pool_get(hxp_mtkdev);
	//msg->user_obj.id = HXP_IMGSYS_AEE_DUMP_ID;
	//msg->user_obj.len = 0;
	//msg->user_obj.info.send.hcp = HXP_IMGSYS_AEE_DUMP_ID;
	//msg->user_obj.info.send.req = 0;
	//msg->user_obj.info.send.ack = 0;

	//spin_lock_irqsave(&hxp_mtkdev->msglock, flag);
	//msg->user_obj.info.send.seq = atomic_inc_return(&hxp_mtkdev->seq);

	//list_add_tail(&msg->entry, &hxp_mtkdev->chans[MODULE_AOV]);
	//spin_unlock_irqrestore(&hxp_mtkdev->msglock, flag);
	//wake_up(&hxp_mtkdev->poll_wq[MODULE_IMG]);

	//hxp_core_send_cmd(hxp_mtkdev, HXP_IMGSYS_AEE_DUMP_ID, &dummy, 1, 0, 1);

	//module_notify(hxp_mtkdev, &msg->user_obj);

	pr_info("HCP trigger AEE dump-\n");

	return 0;
}

int proc_open(struct inode *inode, struct file *file)
{
	struct mtk_hxp *hxp_dev = hxp_core_get_device();

	const char *name;

	try_module_get(THIS_MODULE);

	name = file->f_path.dentry->d_name.name;
	if (!strcmp(name, "daemon")) {
		file->private_data = &hxp_dev->aee_info.data[0];
	} else if (!strcmp(name, "kernel")) {
		file->private_data = &hxp_dev->aee_info.data[1];
	} else if (!strcmp(name, "stream")) {
		file->private_data = &hxp_dev->aee_info.data[2];
	} else {
		module_put(THIS_MODULE);
		return -EPERM;
	}

	if (file->private_data == NULL) {
		pr_info("failed to allocate proc file(%s) buffer", name);
		return -ENOMEM;
	}

	pr_info("%s: %s", __func__, name);

	return 0;
}

static ssize_t proc_read(struct file *file, char __user *buf, size_t lbuf,
	loff_t *ppos)
{
	struct proc_info *info = (struct proc_info *)file->private_data;
	int nbytes, maxbytes, bytes_to_do;

	maxbytes = info->count - *ppos;
	bytes_to_do = (maxbytes > lbuf) ? lbuf : maxbytes;
	if (bytes_to_do == 0)
		pr_info("Reached end of the device on a read");

	nbytes = bytes_to_do - copy_to_user(buf, info->buffer + *ppos, bytes_to_do);
	*ppos += nbytes;

	pr_info("\n Leaving the   READ function, nbytes=%d, pos=%d\n",
		nbytes, (int)*ppos);

	return nbytes;
}

static ssize_t proc_write(struct file *file, const char __user *buf,
	size_t lbuf, loff_t *ppos)
{
	struct proc_info *info = (struct proc_info *)file->private_data;
	int nbytes, maxbytes, bytes_to_do;

	maxbytes = info->size - *ppos;
	bytes_to_do = (maxbytes > lbuf) ? lbuf : maxbytes;
	if (bytes_to_do == 0)
		pr_info("Reached end of the device on a write");

	nbytes =
			bytes_to_do - copy_from_user(info->buffer + *ppos, buf, bytes_to_do);

	*ppos += nbytes;
	if (*ppos > info->count)
		info->count = *ppos;

	pr_info("\n Leaving the WRITE function, nbytes=%d, pos=%d\n",
		nbytes, (int)*ppos);

	return nbytes;
}

int proc_close(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static const struct proc_ops aee_ops = {
	.proc_open = proc_open,
	.proc_read  = proc_read,
	.proc_write = proc_write,
	.proc_release = proc_close
};

int hxp_aee_init(struct mtk_hxp *hxp_dev)
{
	struct hxp_aee *aee_info = &hxp_dev->aee_info;

	//aed_set_extra_func(hxp_notify_aee);

	aee_info->entry = proc_mkdir("mtk_img_debug", NULL);
	if (aee_info->entry) {
		aee_info->data[0].size = HXP_AEE_MAX_BUFFER_SIZE;
		aee_info->data[0].count = 0;
		aee_info->data[1].size = HXP_AEE_MAX_BUFFER_SIZE;
		aee_info->data[1].count = 0;
		aee_info->data[2].size = HXP_AEE_MAX_BUFFER_SIZE;
		aee_info->data[2].count = 0;

		aee_info->daemon = proc_create(
			"daemon", 0644, aee_info->entry, &aee_ops);
		aee_info->stream = proc_create(
			"stream", 0644, aee_info->entry, &aee_ops);
		aee_info->kernel = proc_create(
			"kernel", 0644, aee_info->entry, &aee_ops);
	} else {
		pr_info("%s: failed to create imgsys debug node\n", __func__);
	}

	return 0;
}

int hxp_aee_uninit(struct mtk_hxp *hxp_dev)
{
	struct hxp_aee *aee_info = &hxp_dev->aee_info;

	if (aee_info->kernel)
		proc_remove(aee_info->kernel);

	if (aee_info->daemon)
		proc_remove(aee_info->daemon);

	if (aee_info->stream)
		proc_remove(aee_info->stream);

	if (aee_info->entry)
		proc_remove(aee_info->entry);

	return 0;
}
