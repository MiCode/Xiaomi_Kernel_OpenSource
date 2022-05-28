// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
	pr_info("aov trigger aee dump+\n");

	pr_info("aov trigger AEE dump-\n");

	return 0;
}

int proc_open(struct inode *inode, struct file *file)
{
	struct mtk_aov *aov_dev = aov_core_get_device();

	const char *name;

	try_module_get(THIS_MODULE);

	name = file->f_path.dentry->d_name.name;
	if (!strcmp(name, "daemon")) {
		file->private_data = &aov_dev->aee_info.data[0];
	} else if (!strcmp(name, "kernel")) {
		file->private_data = &aov_dev->aee_info.data[1];
	} else if (!strcmp(name, "stream")) {
		file->private_data = &aov_dev->aee_info.data[2];
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

int aov_aee_init(struct mtk_aov *aov_dev)
{
	struct aov_aee *aee_info = &aov_dev->aee_info;

	//aed_set_extra_func(aov_notify_aee);

	aee_info->entry = proc_mkdir("mtk_img_debug", NULL);
	if (aee_info->entry) {
		aee_info->data[0].size = aov_AEE_MAX_BUFFER_SIZE;
		aee_info->data[0].count = 0;
		aee_info->data[1].size = aov_AEE_MAX_BUFFER_SIZE;
		aee_info->data[1].count = 0;
		aee_info->data[2].size = aov_AEE_MAX_BUFFER_SIZE;
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

int aov_aee_uninit(struct mtk_aov *aov_dev)
{
	struct aov_aee *aee_info = &aov_dev->aee_info;

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
