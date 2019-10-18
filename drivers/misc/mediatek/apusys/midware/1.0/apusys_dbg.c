/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>

#include "apusys_cmn.h"
#include "apusys_dbg.h"
#include "resource_mgt.h"
#include "midware_trace.h"
#include "apusys_user.h"

struct dentry *apusys_dbg_root;
struct dentry *apusys_dbg_user;
struct dentry *apusys_dbg_device;
struct dentry *apusys_dbg_queue;
struct dentry *apusys_dbg_trace;
struct dentry *apusys_dbg_fo;
struct dentry *apusys_dbg_log;

u8 g_log_level = APUSYS_LOG_INFO;
u8 cfg_apusys_trace;
EXPORT_SYMBOL(cfg_apusys_trace);


uint8_t apusys_fo_list[APUSYS_FO_MAX];

//----------------------------------------------
inline int get_fo_from_list(int idx)
{
	if (idx >= APUSYS_FO_MAX)
		return -EINVAL;

	return (int)apusys_fo_list[idx];
}

//----------------------------------------------
// user table dump
static int apusys_dbg_dump_user(struct seq_file *s, void *unused)
{
	apusys_user_dump(s);
	return 0;
}

static int apusys_dbg_user_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_dump_user, inode->i_private);
}

static const struct file_operations apusys_dbg_fops_user = {
	.open = apusys_dbg_user_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	//.write = seq_write,
};

//----------------------------------------------
// device table dump
static int apusys_dbg_dump_dev(struct seq_file *s, void *unused)
{
	res_mgt_dump(s);
	return 0;
}

static int apusys_dbg_device_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_dump_dev, inode->i_private);
}

static const struct file_operations apusys_dbg_fops_device = {
	.open = apusys_dbg_device_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	//.write = seq_write,
};

//----------------------------------------------
// queue table dump
static int apusys_dbg_dump_queue(struct seq_file *s, void *unused)
{
	LOG_CON(s, "hello~\n");
	return 0;
}

static int apusys_dbg_queue_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_dump_queue, inode->i_private);
}

static ssize_t apusys_dbg_queue_read(struct file *flip,
		char __user *buffer,
		size_t count, loff_t *f_pos)
{
	struct seq_file *s = (struct seq_file *)flip->private_data;
	int num = 0, ret = 0, dev_type = 0;

	if (s == NULL)
		return -EINVAL;

	if (count < sizeof(num)) {
		LOG_ERR("size too small(%lu/%lu)\n", count, sizeof(num));
		return -EINVAL;
	}

	dev_type = *(int *)s->private;
	num = res_get_queue_len(dev_type);
	if (num < 0)
		return -ENODEV;

	LOG_DEBUG("queue(%d) length = %d\n", dev_type, num);

	ret = simple_read_from_buffer(buffer, count,
		f_pos, &num, sizeof(num));

	LOG_CON(s, "queue(%d) length = %d\n", dev_type, num);

	return ret;
}

static const struct file_operations apusys_dbg_fops_queue = {
	.open = apusys_dbg_queue_open,
	.read = apusys_dbg_queue_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * input dev_type is device table's type,
 * allocated by resource mgt
 */
int apusys_dbg_create_queue(int *dev_type)
{
	int ret = 0, type = 0;
	struct dentry *dbg_dev_q = NULL;
	char dev_name[32];

	/* check argument */
	if (dev_type == NULL)
		return -EINVAL;

	/* check queue dir */
	ret = IS_ERR_OR_NULL(apusys_dbg_queue);
	if (ret) {
		LOG_ERR("failed to get queue dir.\n");
		return -EINVAL;
	}

	memset(dev_name, 0, sizeof(dev_name));
	type = *(int *)dev_type;
	snprintf(dev_name, sizeof(dev_name)-1, "%d", type);
	LOG_INFO("private: %d/%s/%p\n", type, dev_name, dev_type);

	/* create with dev type */
	dbg_dev_q = debugfs_create_file(dev_name, 0644,
		apusys_dbg_queue, dev_type, &apusys_dbg_fops_queue);
	ret = IS_ERR_OR_NULL(dbg_dev_q);
	if (ret)
		LOG_ERR("create q len node(%d) fail(%d)\n", *dev_type, ret);

	return ret;
}

//----------------------------------------------
static int apusys_dbg_fo_dump(struct seq_file *s, void *unused)
{
	LOG_CON(s, "|---------------------------------|\n");
	LOG_CON(s, "| apusys fo list                  |\n");
	LOG_CON(s, "|---------------------------------|\n");
	LOG_CON(s, "| multicore  = %-3d                |\n",
		apusys_fo_list[APUSYS_FO_MULTICORE]);
	LOG_CON(s, "| scheduler  = %-3d                |\n",
		apusys_fo_list[APUSYS_FO_SCHED]);
	LOG_CON(s, "| preemption = %-3d                |\n",
		apusys_fo_list[APUSYS_FO_PREEMPTION]);
	LOG_CON(s, "| timerecord = %-3d                |\n",
		apusys_fo_list[APUSYS_FO_TIMERECORD]);
	LOG_CON(s, "|---------------------------------|\n");

	return 0;
}

static int apusys_dbg_fo_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_fo_dump, inode->i_private);
}

static ssize_t apusys_dbg_fo_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, fo;
	const int max_arg = 1;
	unsigned int args[max_arg];

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';
	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "multicore") == 0)
		fo = APUSYS_FO_MULTICORE;
	else if (strcmp(token, "scheduler") == 0)
		fo = APUSYS_FO_SCHED;
	else if (strcmp(token, "preemption") == 0)
		fo = APUSYS_FO_PREEMPTION;
	else if (strcmp(token, "timerecord") == 0)
		fo = APUSYS_FO_TIMERECORD;
	else {
		ret = -EINVAL;
		LOG_ERR("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	apusys_fo_list[fo] = args[0];

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations apusys_dbg_fops_fo = {
	.open = apusys_dbg_fo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = apusys_dbg_fo_write,
};

//----------------------------------------------
int apusys_dbg_init(void)
{
	int ret = 0;


	/* create debug root */
	apusys_dbg_root = debugfs_create_dir(APUSYS_DBG_DIR, NULL);
	ret = IS_ERR_OR_NULL(apusys_dbg_root);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		goto out;
	}

	/* create device table info */
	apusys_dbg_device = debugfs_create_file("device", 0644,
		apusys_dbg_root, NULL, &apusys_dbg_fops_device);
	ret = IS_ERR_OR_NULL(apusys_dbg_device);
	if (ret) {
		LOG_ERR("failed to create debug node(device).\n");
		goto out;
	}

	/* create device queue info */
	apusys_dbg_queue = debugfs_create_dir("queue", apusys_dbg_root);
	ret = IS_ERR_OR_NULL(apusys_dbg_queue);
	if (ret) {
		LOG_ERR("failed to create queue dir.\n");
		goto out;
	}

	/* create user info */
	apusys_dbg_user = debugfs_create_file("user", 0644,
		apusys_dbg_root, NULL, &apusys_dbg_fops_user);
	ret = IS_ERR_OR_NULL(apusys_dbg_user);
	if (ret) {
		LOG_ERR("failed to create debug node(user).\n");
		goto out;
	}

	/* create feature option info */
	apusys_dbg_fo = debugfs_create_file("fo", 0644,
		apusys_dbg_root, NULL, &apusys_dbg_fops_fo);
	ret = IS_ERR_OR_NULL(apusys_dbg_fo);
	if (ret) {
		LOG_ERR("failed to create debug node(fo).\n");
		goto out;
	}

	/* create log level */
	apusys_dbg_log = debugfs_create_u8("log_level", 0644,
		apusys_dbg_root, &g_log_level);

	/* create trace enable */
	apusys_dbg_trace = debugfs_create_u8("trace_en", 0644,
		apusys_dbg_root, &cfg_apusys_trace);
	cfg_apusys_trace = 0;

	ret = IS_ERR_OR_NULL(apusys_dbg_trace);
	if (ret) {
		LOG_ERR("failed to create debug node(trace_en).\n");
		goto out;
	}

	/* init feature option */
	apusys_fo_list[APUSYS_FO_MULTICORE] = 0;
	apusys_fo_list[APUSYS_FO_SCHED] = 1;
	apusys_fo_list[APUSYS_FO_PREEMPTION] = 0;
	apusys_fo_list[APUSYS_FO_TIMERECORD] = 0;

out:
	return ret;
}

int apusys_dbg_destroy(void)
{
	debugfs_remove_recursive(apusys_dbg_root);

	return 0;
}
