// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#include "apusys_core.h"

/* define */
#define APUSYS_DEV_NAME "apusys"


/* global variable */
static struct apusys_core_info g_core_info;

static void create_dbg_root(void)
{
	g_core_info.dbg_root = debugfs_create_dir(APUSYS_DEV_NAME, NULL);

	/* check dbg root create status */
	if (IS_ERR_OR_NULL(g_core_info.dbg_root))
		pr_info("failed to create debug dir.\n");
}

static void destroy_dbg_root(void)
{
	debugfs_remove_recursive(g_core_info.dbg_root);
}

static int __init apusys_init(void)
{
	int i = 0, j = 0, ret = 0;
	int func_num = sizeof(apusys_init_func)/sizeof(int *);

	/* init apusys_dev */
	create_dbg_root();

	/* call init func */
	for (i = 0; i < func_num; i++) {
		if (apusys_init_func[i] == NULL)
			break;

		ret = apusys_init_func[i](&g_core_info);
		if (ret) {
			pr_info("init function(%d) fail(%d)", i, ret);

			/* exit device */
			for (j = i-1; j >= 0; j--)
				apusys_exit_func[j]();

			destroy_dbg_root();
			break;
		}
	}

	return ret;
}

static void __exit apusys_exit(void)
{
	int i = 0;
	int func_num = sizeof(apusys_init_func)/sizeof(int *);

	/* call release func */
	for (i = 0; i < func_num; i++) {
		if (apusys_exit_func[i] == NULL)
			break;

		apusys_exit_func[i]();
	}

	destroy_dbg_root();
}

module_init(apusys_init);
module_exit(apusys_exit);
MODULE_DESCRIPTION("MTK APUSys Driver");
MODULE_AUTHOR("SPT1");
MODULE_LICENSE("GPL");
