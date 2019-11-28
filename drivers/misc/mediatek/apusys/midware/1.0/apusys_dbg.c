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
#include "scheduler.h"

struct dentry *apusys_dbg_root;
struct dentry *apusys_dbg_user;
struct dentry *apusys_dbg_devinfo;
struct dentry *apusys_dbg_device;
struct dentry *apusys_dbg_mem;
struct dentry *apusys_dbg_trace;
struct dentry *apusys_dbg_test;
struct dentry *apusys_dbg_log;
struct dentry *apusys_dbg_boost;

u32 g_log_level;
u32 g_dbg_multi;
u8 cfg_apusys_trace;

EXPORT_SYMBOL(cfg_apusys_trace);

enum {
	APUSYS_DBG_TEST_SUSPEND,
	APUSYS_DBG_TEST_LOCKDEV,
	APUSYS_DBG_TEST_UNLOCKDEV,
	APUSYS_DBG_TEST_MULTITEST,
	APUSYS_DBG_TEST_MAX,
};

int dbg_get_multitest(void)
{
	return g_dbg_multi;
}

//----------------------------------------------
// user table dump
static int apusys_dbg_dump_user(struct seq_file *s, void *unused)
{
	apusys_user_dump(s);
	return 0;
}

static int apusys_dbg_open_user(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_dump_user, inode->i_private);
}

static const struct file_operations apusys_dbg_fops_user = {
	.open = apusys_dbg_open_user,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	//.write = seq_write,
};

//----------------------------------------------
// device table dump
static int apusys_dbg_dump_devinfo(struct seq_file *s, void *unused)
{
	res_mgt_dump(s);
	return 0;
}

static int apusys_dbg_open_devinfo(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_dump_devinfo, inode->i_private);
}

static const struct file_operations apusys_dbg_fops_devinfo = {
	.open = apusys_dbg_open_devinfo,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	//.write = seq_write,
};

//----------------------------------------------
// mem dump
static int apusys_dbg_dump_mem(struct seq_file *s, void *unused)
{
	LOG_CON(s, "not support yet.\n");
	return 0;
}

static int apusys_dbg_open_mem(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_dump_mem, inode->i_private);
}

static const struct file_operations apusys_dbg_fops_mem = {
	.open = apusys_dbg_open_mem,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	//.write = seq_write,
};

//----------------------------------------------
static int apusys_dbg_test_dump(struct seq_file *s, void *unused)
{
	LOG_CON(s, "%d\n", g_dbg_multi);
	return 0;
}

static int apusys_dbg_open_test(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_test_dump, inode->i_private);
}

static void _apusys_dbg_test(int test, int *arg, int count)
{
	int mdla_num = 0;

	switch (test) {
	case APUSYS_DBG_TEST_SUSPEND:
		if (count < 1)
			LOG_WARN("suspend test need 1 arg\n");

		if (arg[0])
			apusys_sched_pause();
		else
			apusys_sched_restart();
		break;
	case APUSYS_DBG_TEST_LOCKDEV:
		if (count < 2)
			LOG_WARN("lock dev test need 2 args\n");
		LOG_WARN("todo\n");
		break;
	case APUSYS_DBG_TEST_UNLOCKDEV:
		if (count < 2)
			LOG_WARN("lock dev test need 2 args\n");
		LOG_WARN("todo\n");
		break;
	case APUSYS_DBG_TEST_MULTITEST:
		mdla_num = res_get_device_num(APUSYS_DEVICE_MDLA);
		if (arg[0] > mdla_num) {
			LOG_WARN("multicore not support: too much(%d/%d)\n",
				arg[0], mdla_num);
		} else {
			g_dbg_multi = arg[0];
			LOG_INFO("setup multi test %d\n", arg[0]);
		}
		break;
	default:
		LOG_WARN("no test(%d/%d/%d)\n", test, arg[0], count);
		break;
	}
}

static ssize_t apusys_dbg_write_test(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, test;
	const int max_arg = 2;
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
	if (strcmp(token, "suspend") == 0)
		test = APUSYS_DBG_TEST_SUSPEND;
	else if (strcmp(token, "lockdev") == 0)
		test = APUSYS_DBG_TEST_LOCKDEV;
	else if (strcmp(token, "unlockdev") == 0)
		test = APUSYS_DBG_TEST_UNLOCKDEV;
	else if (strcmp(token, "multicore") == 0)
		test = APUSYS_DBG_TEST_MULTITEST;
	else {
		ret = -EINVAL;
		LOG_ERR("no test(%s)\n", token);
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

	/* call test */
	_apusys_dbg_test(test, args, i);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations apusys_dbg_fops_test = {
	.open = apusys_dbg_open_test,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = apusys_dbg_write_test,
};

//----------------------------------------------
int apusys_dbg_init(void)
{
	int ret = 0;

	LOG_INFO("+\n");

	g_log_level = 0;
	g_dbg_multi = 1;

	/* create debug root */
	apusys_dbg_root = debugfs_create_dir(APUSYS_DBG_DIR, NULL);
	ret = IS_ERR_OR_NULL(apusys_dbg_root);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		goto out;
	}

	/* create device table info */
	apusys_dbg_devinfo = debugfs_create_file("devinfo", 0444,
		apusys_dbg_root, NULL, &apusys_dbg_fops_devinfo);
	ret = IS_ERR_OR_NULL(apusys_dbg_devinfo);
	if (ret) {
		LOG_ERR("failed to create debug node(devinfo).\n");
		goto out;
	}

	/* create device queue info */
	apusys_dbg_device = debugfs_create_dir("device", apusys_dbg_root);
	ret = IS_ERR_OR_NULL(apusys_dbg_device);
	if (ret) {
		LOG_ERR("failed to create queue dir(device).\n");
		goto out;
	}

	/* create user info */
	apusys_dbg_user = debugfs_create_file("user", 0444,
		apusys_dbg_root, NULL, &apusys_dbg_fops_user);
	ret = IS_ERR_OR_NULL(apusys_dbg_user);
	if (ret) {
		LOG_ERR("failed to create debug node(user).\n");
		goto out;
	}

	/* create user info */
	apusys_dbg_mem = debugfs_create_file("mem", 0444,
		apusys_dbg_root, NULL, &apusys_dbg_fops_mem);
	ret = IS_ERR_OR_NULL(apusys_dbg_mem);
	if (ret) {
		LOG_ERR("failed to create debug node(mem).\n");
		goto out;
	}

	/* create feature option info */
	apusys_dbg_test = debugfs_create_file("test", 0644,
		apusys_dbg_root, NULL, &apusys_dbg_fops_test);
	ret = IS_ERR_OR_NULL(apusys_dbg_test);
	if (ret) {
		LOG_ERR("failed to create debug node(test).\n");
		goto out;
	}

	/* create log level */
	apusys_dbg_log = debugfs_create_u32("klog", 0644,
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

	apusys_dump_init();
out:

	LOG_INFO("-\n");
	return ret;
}

int apusys_dbg_destroy(void)
{
	apusys_dump_exit();
	debugfs_remove_recursive(apusys_dbg_root);
	return 0;
}
