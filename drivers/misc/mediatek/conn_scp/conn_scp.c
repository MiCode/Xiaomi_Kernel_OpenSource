// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <conap_scp.h>
#include <linux/poll.h>

#include "conap_scp_priv.h"
#ifdef AOLTEST_SUPPORT
#include "aoltest_core.h"
#endif

#define CONN_SCP_DEVNAME            "connscp"

struct conn_scp_dev {
	struct class *cls;
	struct device *dev;
	dev_t devno;
	struct cdev chdev;
};

struct conn_scp_dev *g_conn_scp_dev;

const struct file_operations g_conn_scp_dev_fops = {
};

int conn_scp_init(void)
{
	int ret = 0;
	int err = 0;

	g_conn_scp_dev = kzalloc(sizeof(*g_conn_scp_dev), GFP_KERNEL);
	if (g_conn_scp_dev == NULL) {
		err = -ENOMEM;
		ret = -ENOMEM;
		goto err_out;
	}

	pr_info("Registering conn_scp chardev\n");
	ret = alloc_chrdev_region(&g_conn_scp_dev->devno, 0, 1, CONN_SCP_DEVNAME);
	if (ret) {
		pr_info("alloc_chrdev_region fail: %d\n", ret);
		err = -ENOMEM;
		goto err_out;
	} else {
		pr_info("major: %d, minor: %d\n",
				MAJOR(g_conn_scp_dev->devno), MINOR(g_conn_scp_dev->devno));
	}
	cdev_init(&g_conn_scp_dev->chdev, &g_conn_scp_dev_fops);

	g_conn_scp_dev->chdev.owner = THIS_MODULE;
	err = cdev_add(&g_conn_scp_dev->chdev, g_conn_scp_dev->devno, 1);
	if (err) {
		pr_info("cdev_add fail: %d\n", err);
		goto err_out;
	}
	g_conn_scp_dev->cls = class_create(THIS_MODULE, "conn_scp");
	if (IS_ERR(g_conn_scp_dev->cls)) {
		pr_info("Unable to create class, err = %d\n", (int)PTR_ERR(g_conn_scp_dev->cls));
		goto err_out;
	}

	g_conn_scp_dev->dev = device_create(g_conn_scp_dev->cls,
		NULL, g_conn_scp_dev->devno, g_conn_scp_dev, "conn_scp");

	if (IS_ERR(g_conn_scp_dev->dev)) {
		pr_err("device create fail, error code(%ld)\n",
								PTR_ERR(g_conn_scp_dev->dev));
		goto err_out;
	}
	pr_info("CONN SCP device init Done\n");

	/*****************************************/
	conap_scp_init();

#ifdef AOLTEST_SUPPORT
	aoltest_core_init();
#endif

	return 0;

err_out:
	if (g_conn_scp_dev != NULL) {
		if (err == 0)
			cdev_del(&g_conn_scp_dev->chdev);
		if (ret == 0)
			unregister_chrdev_region(g_conn_scp_dev->devno, 1);
		kfree(g_conn_scp_dev);
		g_conn_scp_dev = NULL;
	}
	return -1;

}

void conn_scp_exit(void)
{

	pr_info("Unregistering conn_scp test chardev\n");
	conap_scp_deinit();

#ifdef AOLTEST_SUPPORT
	aoltest_core_deinit();
#endif

	cdev_del(&g_conn_scp_dev->chdev);
	unregister_chrdev_region(g_conn_scp_dev->devno, 1);
	device_destroy(g_conn_scp_dev->cls, g_conn_scp_dev->devno);
	class_destroy(g_conn_scp_dev->cls);
	kfree(g_conn_scp_dev);
	g_conn_scp_dev = NULL;
	pr_info("Done\n");


}


module_init(conn_scp_init);
module_exit(conn_scp_exit);
MODULE_AUTHOR("Willy Yu <Willy.Yu@mediatek.com>");
MODULE_DESCRIPTION("Conn SCP Bridge dev");
MODULE_LICENSE("GPL");

