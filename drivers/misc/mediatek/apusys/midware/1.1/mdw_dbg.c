/*
 * Copyright (C) 2020 MediaTek Inc.
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

#include "mdw_cmn.h"
#include "mdw_tag.h"
#include "mdw_rsc.h"
#include "mdw_dbg.h"
#include "apusys_dbg.h"
#include "mdw_mem.h"
#include "mdw_trace.h"
#include "mdw_usr.h"
#include "mdw_sched.h"

#include "aee.h"

struct dentry *mdw_dbg_root;
struct dentry *mdw_dbg_user;
struct dentry *mdw_dbg_devinfo;
struct dentry *mdw_dbg_device;
struct dentry *mdw_dbg_mem;
struct dentry *mdw_dbg_trace;
struct dentry *mdw_dbg_test;
struct dentry *mdw_dbg_log;
struct dentry *mdw_dbg_boost;

struct dentry *mdw_dbg_debug_root;
struct dentry *mdw_dbg_debug_log;

u32 g_mdw_klog;
u32 g_dbg_prop[MDW_DBG_PROP_MAX];

u8 cfg_apusys_trace;

enum {
	APUSYS_DBG_TEST_SUSPEND,
	APUSYS_DBG_TEST_LOCKDEV,
	APUSYS_DBG_TEST_UNLOCKDEV,
	APUSYS_DBG_TEST_MULTITEST,
	APUSYS_DBG_TEST_TCM_DEFAULT,
	APUSYS_DBG_TEST_QUERY_MEM,
	APUSYS_DBG_TEST_AEE,
	APUSYS_DBG_CMD_TIMEOUT_AEE,
	APUSYS_DBG_TEST_MAX,
};

int mdw_dbg_get_prop(int idx)
{
	if (idx >= MDW_DBG_PROP_MAX)
		return -EINVAL;

	return g_dbg_prop[idx];
}

void mdw_dbg_aee(char *name)
{
#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning("VPU", "\nCRDISPATCH_KEY:APUSYS_MIDDLEWARE\n",
		name);
#else
	mdw_drv_info("not support aee\n");
#endif
}

//----------------------------------------------
// tags log dump
static int mdw_dbg_dump_log(struct seq_file *s, void *unused)
{
	mdw_tag_show(s);
	return 0;
}

static int mdw_dbg_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, mdw_dbg_dump_log, inode->i_private);
}

static const struct file_operations mdw_dbg_fops_log = {
	.open = mdw_dbg_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//----------------------------------------------
// user table dump
static int mdw_dbg_dump_usr(struct seq_file *s, void *unused)
{
	mdw_usr_dump(s);
	return 0;
}

static int mdw_dbg_open_usr(struct inode *inode, struct file *file)
{
	return single_open(file, mdw_dbg_dump_usr, inode->i_private);
}

static const struct file_operations mdw_dbg_fops_user = {
	.open = mdw_dbg_open_usr,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//----------------------------------------------
// device table dump
static int mdw_dbg_dump_devinfo(struct seq_file *s, void *unused)
{
	mdw_rsc_dump(s);
	return 0;
}

static int mdw_dbg_open_devinfo(struct inode *inode, struct file *file)
{
	return single_open(file, mdw_dbg_dump_devinfo, inode->i_private);
}

static const struct file_operations mdw_dbg_fops_devinfo = {
	.open = mdw_dbg_open_devinfo,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
// mem dump
static int mdw_dbg_dump_mem(struct seq_file *s, void *unused)
{
	//mdw_user_print_log();
	//TODO Change to Tag
	mdw_usr_aee_mem(s);
	return 0;
}

static int mdw_dbg_open_mem(struct inode *inode, struct file *file)
{
	return single_open(file, mdw_dbg_dump_mem, inode->i_private);
}

static const struct file_operations mdw_dbg_fops_mem = {
	.open = mdw_dbg_open_mem,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
static int mdw_dbg_test_dump(struct seq_file *s, void *unused)
{
	mdw_con_info(s, "-------------------------------------------------\n");
	mdw_con_info(s, " multicore(%d):\n",
		g_dbg_prop[MDW_DBG_PROP_MULTICORE]);
	mdw_con_info(s, "    0: scheduler decide\n");
	mdw_con_info(s, "    1: force single\n");
	mdw_con_info(s, "    2: force multi\n");
	mdw_con_info(s, " tcm_default(0x%x):\n",
		g_dbg_prop[MDW_DBG_PROP_TCM_DEFAULT]);
	mdw_con_info(s, "    set default tcm size if user doesn't set\n");
	mdw_con_info(s, "    1MB: 1048546\n");
	mdw_con_info(s, " query_mem(%d):\n",
		g_dbg_prop[MDW_DBG_PROP_QUERY_MEM]);
	mdw_con_info(s, "    0: disable, can't query kva/iova from code\n");
	mdw_con_info(s, "    1: enable, can query kva/iova from code\n");
	mdw_con_info(s, " lockdev <device_type> <index>\n");
	mdw_con_info(s, "    device_type/index: check /d/apusys_midware/devinfo\n");
	mdw_con_info(s, " unlockdev <device_type> <index>\n");
	mdw_con_info(s, "    device_type/index: check /d/apusys_midware/devinfo\n");
	mdw_con_info(s, " aee_test:\n");
	mdw_con_info(s, "    1: trigger aee to dump information\n");
	mdw_con_info(s, " aee_cmd_timeout(%d):\n",
		g_dbg_prop[MDW_DBG_PROP_CMD_TIMEOUT_AEE]);
	mdw_con_info(s, "    0: disable\n");
	mdw_con_info(s, "    1: enable, trigger aee when cmd timeout\n");
	mdw_con_info(s, "-------------------------------------------------\n");

	return 0;
}

static int mdw_dbg_open_test(struct inode *inode, struct file *file)
{
	return single_open(file, mdw_dbg_test_dump, inode->i_private);
}

static void mdw_dbg_test_func(int test, int *arg, int count)
{
	unsigned int vlm_start = 0, vlm_size = 0;
	int type = 0, idx = 0;
	struct mdw_dev_info *d = NULL;

	switch (test) {
	case APUSYS_DBG_TEST_SUSPEND:
		if (count != 1) {
			mdw_drv_warn("suspend test need 1 arg\n");
			break;
		}
		if (arg[0])
			mdw_sched_pause();
		else
			mdw_sched_restart();
		break;

	case APUSYS_DBG_TEST_LOCKDEV:
		if (count != 2) {
			mdw_drv_warn("lock test need 2 arg\n");
			break;
		}
		type = arg[0];
		idx = arg[1];
		d = mdw_rsc_get_dinfo(type, idx);
		if (!d)
			return;

		mdw_drv_warn("lock dev(%d-%d), ret(%d)\n",
			type, idx, d->lock(d));
		break;

	case APUSYS_DBG_TEST_UNLOCKDEV:
		if (count != 2) {
			mdw_drv_warn("unlock test need 2 args\n");
			break;
		}
		type = arg[0];
		idx = arg[1];
		d = mdw_rsc_get_dinfo(type, idx);
		if (!d)
			return;

		mdw_drv_warn("lock dev(%d-%d), ret(%d)\n",
			type, idx, d->unlock(d));
		break;

	case APUSYS_DBG_TEST_MULTITEST:
		if (count != 1) {
			mdw_drv_warn("multi test need 1 arg\n");
			break;
		}
		if (arg[0] > 2) {
			mdw_drv_warn("multicore not support(%d)\n", arg[0]);
		} else {
			g_dbg_prop[MDW_DBG_PROP_MULTICORE] = arg[0];
			mdw_drv_debug("setup multi test %d\n", arg[0]);
		}
		break;

	case APUSYS_DBG_TEST_TCM_DEFAULT:
		if (count != 1) {
			mdw_drv_warn("default tcm test need 1 arg\n");
			break;
		}
		mdw_mem_get_vlm(&vlm_start, &vlm_size);

		g_dbg_prop[MDW_DBG_PROP_TCM_DEFAULT] = vlm_size <
			(unsigned int)arg[0] ? vlm_size : (unsigned int)arg[0];

		mdw_drv_warn("tcm default(%u/%d)\n", vlm_size, arg[0]);
		break;

	case APUSYS_DBG_TEST_QUERY_MEM:
		if (count != 1) {
			mdw_drv_warn("query mem test need 1 arg\n");
			break;
		}
		mdw_drv_warn("query mem(%d)\n", arg[0]);
		g_dbg_prop[MDW_DBG_PROP_QUERY_MEM] = arg[0];
		break;

	case APUSYS_DBG_TEST_AEE:
		mdw_dbg_aee("apusys midware test");
		break;

	case APUSYS_DBG_CMD_TIMEOUT_AEE:
		if (count != 1) {
			mdw_drv_warn("timeout aee enable need 1 arg\n");
			break;
		}
		mdw_drv_warn("timeout aee(%d)\n", arg[0]);
		g_dbg_prop[MDW_DBG_PROP_CMD_TIMEOUT_AEE] = arg[0];
		break;


	default:
		mdw_drv_warn("no test(%d/%d/%d)\n", test, arg[0], count);
		break;
	}
}

static ssize_t mdw_dbg_write_test(struct file *flip,
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
	else if (strcmp(token, "aee_test") == 0)
		test = APUSYS_DBG_TEST_AEE;
	else if (strcmp(token, "aee_cmd_timeout") == 0)
		test = APUSYS_DBG_CMD_TIMEOUT_AEE;
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
	mdw_dbg_test_func(test, args, i);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations mdw_dbg_fops_test = {
	.open = mdw_dbg_open_test,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mdw_dbg_write_test,
};

//----------------------------------------------
int mdw_dbg_init(void)
{
	int ret = 0;

	mdw_flw_debug("+\n");

	g_mdw_klog = 0;
	memset(g_dbg_prop, 0, sizeof(g_dbg_prop));

	/* create debug root */
	mdw_dbg_root = debugfs_create_dir(APUSYS_DBG_DIR, NULL);

	/* create device table info */
	mdw_dbg_devinfo = debugfs_create_file("devinfo", 0444,
		mdw_dbg_root, NULL, &mdw_dbg_fops_devinfo);

	/* create device queue info */
	mdw_dbg_device = debugfs_create_dir("device", mdw_dbg_root);

	/* create user info */
	mdw_dbg_user = debugfs_create_file("user", 0444,
		mdw_dbg_root, NULL, &mdw_dbg_fops_user);

	/* create user info */
	mdw_dbg_mem = debugfs_create_file("mem", 0444,
		mdw_dbg_root, NULL, &mdw_dbg_fops_mem);

	/* create feature option info */
	mdw_dbg_test = debugfs_create_file("test", 0644,
		mdw_dbg_root, NULL, &mdw_dbg_fops_test);

	/* create log level */
	mdw_dbg_log = debugfs_create_u32("klog", 0644,
		mdw_dbg_root, &g_mdw_klog);

	/* create trace enable */
	cfg_apusys_trace = 0;
	mdw_dbg_trace = debugfs_create_u8("trace_en", 0644,
		mdw_dbg_root, &cfg_apusys_trace);

	/* tmp log dump node */
	mdw_dbg_debug_root =  debugfs_create_dir("apusys_debug", NULL);
	mdw_dbg_debug_log = debugfs_create_file("log", 0444,
		mdw_dbg_debug_root, NULL, &mdw_dbg_fops_log);

	apusys_dump_init();

	mdw_flw_debug("-\n");
	return ret;
}

int mdw_dbg_exit(void)
{
	apusys_dump_exit();
	debugfs_remove_recursive(mdw_dbg_root);
	return 0;
}
