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

#include "apusys_drv.h"
#include "mdw_cmn.h"
#include "mdw_tag.h"
#include "apusys_dbg.h"
#include "memory_mgt.h"
#include "resource_mgt.h"
#include "midware_trace.h"
#include "apusys_user.h"
#include "scheduler.h"
#include "memory_dump.h"

#include "aee.h"

struct dentry *apusys_dbg_root;
struct dentry *apusys_dbg_user;
struct dentry *apusys_dbg_devinfo;
struct dentry *apusys_dbg_device;
struct dentry *apusys_dbg_mem;
struct dentry *apusys_dbg_trace;
struct dentry *apusys_dbg_test;
struct dentry *apusys_dbg_log;
struct dentry *apusys_dbg_boost;

struct dentry *apusys_dbg_debug_root;
struct dentry *apusys_dbg_debug_log;


u32 g_mdw_klog;
u32 g_dbg_prop[DBG_PROP_MAX];

u8 cfg_apusys_trace;

EXPORT_SYMBOL(cfg_apusys_trace);

enum {
	APUSYS_DBG_TEST_SUSPEND,
	APUSYS_DBG_TEST_LOCKDEV,
	APUSYS_DBG_TEST_UNLOCKDEV,
	APUSYS_DBG_TEST_MULTITEST,
	APUSYS_DBG_TEST_TCM_DEFAULT,
	APUSYS_DBG_TEST_QUERY_MEM,
	APUSYS_DBG_TEST_AEE,
	APUSYS_DBG_TEST_MAX,
};

int dbg_get_prop(int idx)
{
	if (idx >= DBG_PROP_MAX)
		return -EINVAL;

	return g_dbg_prop[idx];
}

//----------------------------------------------
// loh dump
static int apusys_dbg_dump_log(struct seq_file *s, void *unused)
{
	mdw_tags_show(s);
	return 0;
}

static int apusys_dbg_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_dump_log, inode->i_private);
}

static const struct file_operations apusys_dbg_fops_log = {
	.open = apusys_dbg_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

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
	.release = single_release,
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
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
// mem dump
static int apusys_dbg_dump_mem(struct seq_file *s, void *unused)
{
	//apusys_user_print_log();
	apusys_user_show_log(s);
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
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
static int apusys_dbg_test_dump(struct seq_file *s, void *unused)
{
	mdw_con_info(s, "-------------------------------------------------\n");
	mdw_con_info(s, " multi policy(%d):\n", g_dbg_prop[DBG_PROP_MULTICORE]);
	mdw_con_info(s, " tcm_default(0x%x):\n",
		g_dbg_prop[DBG_PROP_TCM_DEFAULT]);
	mdw_con_info(s, "    set indicate default tcm size if user don't set\n");
	mdw_con_info(s, "    1MB: 1048546\n");
	mdw_con_info(s, " query_mem(%d):\n", g_dbg_prop[DBG_PROP_QUERY_MEM]);
	mdw_con_info(s, "    0: disable, can't query kva/iova from code\n");
	mdw_con_info(s, "    1: enable, can query kva/iova from code\n");
	mdw_con_info(s, "-------------------------------------------------\n");

	return 0;
}

static int apusys_dbg_open_test(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dbg_test_dump, inode->i_private);
}

static void apusys_dbg_test_func(int test, int *arg, int count)
{
	int mdla_num = 0;
	unsigned int vlm_start = 0, vlm_size = 0;

	switch (test) {
	case APUSYS_DBG_TEST_SUSPEND:
		if (count < 1)
			mdw_drv_warn("suspend test need 1 arg\n");

		if (arg[0])
			apusys_sched_pause();
		else
			apusys_sched_restart();
		break;

	case APUSYS_DBG_TEST_LOCKDEV:
		if (count < 2)
			mdw_drv_warn("lock dev test need 2 args\n");
		mdw_drv_warn("todo\n");
		break;

	case APUSYS_DBG_TEST_UNLOCKDEV:
		if (count < 2)
			mdw_drv_warn("lock dev test need 2 args\n");
		mdw_drv_warn("todo\n");
		break;

	case APUSYS_DBG_TEST_MULTITEST:
		mdla_num = res_get_device_num(APUSYS_DEVICE_MDLA);
		if (arg[0] > mdla_num) {
			mdw_drv_warn("multicore not support: too much(%d/%d)\n",
				arg[0], mdla_num);
		} else {
			g_dbg_prop[DBG_PROP_MULTICORE] = arg[0];
			mdw_drv_debug("setup multi test %d\n", arg[0]);
		}
		break;

	case APUSYS_DBG_TEST_TCM_DEFAULT:
		if (apusys_mem_get_vlm(&vlm_start, &vlm_size)) {
			mdw_drv_err("get vlm fail\n");
			break;
		}

		vlm_size = vlm_size < (unsigned int)arg[0] ?
			vlm_size : (unsigned int)arg[0];
		mdw_drv_debug("tcm default%u/%d)\n", vlm_size, arg[0]);

		g_dbg_prop[DBG_PROP_TCM_DEFAULT] = vlm_size;
		break;

	case APUSYS_DBG_TEST_QUERY_MEM:
		mdw_drv_debug("query mem(%d)\n", arg[0]);
		g_dbg_prop[DBG_PROP_QUERY_MEM] = arg[0];
		break;

	case APUSYS_DBG_TEST_AEE:
#ifdef CONFIG_MTK_AEE_FEATURE
		if (arg[0] == 1) {
			aee_kernel_warning("VPU", "\nCRDISPATCH_KEY:VPU\n",
				"apusys midware test");
		}
#else
		mdw_drv_info("not support aee\n");
#endif
		break;

	default:
		mdw_drv_warn("no test(%d/%d/%d)\n", test, arg[0], count);
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
		mdw_drv_err("copy_from_user failed, ret=%d\n", ret);
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
	else if (strcmp(token, "tcm_default") == 0)
		test = APUSYS_DBG_TEST_TCM_DEFAULT;
	else if (strcmp(token, "query_mem") == 0)
		test = APUSYS_DBG_TEST_QUERY_MEM;
	else if (strcmp(token, "aee") == 0)
		test = APUSYS_DBG_TEST_AEE;
	else {
		ret = -EINVAL;
		mdw_drv_err("no test(%s)\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			mdw_drv_err("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	/* call test */
	apusys_dbg_test_func(test, args, i);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations apusys_dbg_fops_test = {
	.open = apusys_dbg_open_test,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apusys_dbg_write_test,
};

//----------------------------------------------
int apusys_dbg_init(void)
{
	int ret = 0;

	mdw_flw_debug("+\n");

	g_mdw_klog = 0;
	memset(g_dbg_prop, 0, sizeof(g_dbg_prop));
	//g_dbg_prop[DBG_PROP_MULTICORE] = 1;

	/* create debug root */
	apusys_dbg_root = debugfs_create_dir(APUSYS_DBG_DIR, NULL);
	ret = IS_ERR_OR_NULL(apusys_dbg_root);
	if (ret)
		goto out;

	/* create device table info */
	apusys_dbg_devinfo = debugfs_create_file("devinfo", 0444,
		apusys_dbg_root, NULL, &apusys_dbg_fops_devinfo);
	ret = IS_ERR_OR_NULL(apusys_dbg_devinfo);
	if (ret)
		goto out;

	/* create device queue info */
	apusys_dbg_device = debugfs_create_dir("device", apusys_dbg_root);
	ret = IS_ERR_OR_NULL(apusys_dbg_device);
	if (ret)
		goto out;

	/* create user info */
	apusys_dbg_user = debugfs_create_file("user", 0444,
		apusys_dbg_root, NULL, &apusys_dbg_fops_user);
	ret = IS_ERR_OR_NULL(apusys_dbg_user);
	if (ret)
		goto out;

	/* create user info */
	apusys_dbg_mem = debugfs_create_file("mem", 0444,
		apusys_dbg_root, NULL, &apusys_dbg_fops_mem);
	ret = IS_ERR_OR_NULL(apusys_dbg_mem);
	if (ret) {
		mdw_drv_err("failed to create debug node(mem).\n");
		goto out;
	}

	/* create feature option info */
	apusys_dbg_test = debugfs_create_file("test", 0644,
		apusys_dbg_root, NULL, &apusys_dbg_fops_test);
	ret = IS_ERR_OR_NULL(apusys_dbg_test);
	if (ret)
		goto out;

	/* create log level */
	apusys_dbg_log = debugfs_create_u32("klog", 0644,
		apusys_dbg_root, &g_mdw_klog);

	/* create trace enable */
	apusys_dbg_trace = debugfs_create_u8("trace_en", 0644,
		apusys_dbg_root, &cfg_apusys_trace);
	cfg_apusys_trace = 0;

	ret = IS_ERR_OR_NULL(apusys_dbg_trace);
	if (ret)
		goto out;

	/* tmp log dump node */
	apusys_dbg_debug_root =  debugfs_create_dir("apusys_debug", NULL);
	ret = IS_ERR_OR_NULL(apusys_dbg_debug_root);
	if (ret)
		goto out;

	apusys_dbg_debug_log = debugfs_create_file("log", 0444,
		apusys_dbg_debug_root, NULL, &apusys_dbg_fops_log);
	ret = IS_ERR_OR_NULL(apusys_dbg_debug_log);
	if (ret)
		goto out;

	apusys_dump_init();
out:

	mdw_flw_debug("-\n");
	return ret;
}

int apusys_dbg_destroy(void)
{
	apusys_dump_exit();
	debugfs_remove_recursive(apusys_dbg_root);
	return 0;
}
