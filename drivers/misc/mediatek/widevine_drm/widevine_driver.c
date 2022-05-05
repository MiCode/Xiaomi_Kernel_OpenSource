// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "widevine_driver.h"
#include "mtk_heap.h"

static dev_t wv_devt;
static struct cdev *wv_cdev;
static struct class *wv_class;
static struct device *wv_device;

int wv_dbg_level;
module_param(wv_dbg_level, int, 0644);

/******open file function********/
int wv_open(struct inode *inode, struct file *filep)
{
	WV_LOG(1, "filep->f_op = %p\n", filep->f_op);
	return 0;
}

/**close file***/
int wv_release(struct inode *inode, struct file *filep)
{
	WV_LOG(1, "filep->f_op = %p\n", filep->f_op);
	return 0;
}

/*******IO control*********/
static long wv_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	uint32_t sec_handle = 0;
	struct WV_FD_TO_SEC_HANDLE in_out_param;
	struct dma_buf *dmabuf = NULL;

	WV_LOG(1, "enter ioctl\n");
	switch (cmd) {
	case WV_IOC_GET_SEC_HANDLE:
		WV_LOG(1, "WV_IOC_GET_SEC_HANDLE\n");
		if (copy_from_user(&in_out_param, (void *)arg,
					sizeof(struct WV_FD_TO_SEC_HANDLE))) {
			WV_LOG(0, "Copy from user error\n");
			ret = -EFAULT;
			goto err_quit;
		}
		WV_LOG(1, "share_fd = %d\n", in_out_param.share_fd);
		// get dma buf
		dmabuf = dma_buf_get(in_out_param.share_fd);
		if (IS_ERR_OR_NULL(dmabuf)) {
			WV_LOG(0, "dma_buf_get fail: %ld\n", PTR_ERR(dmabuf));
			ret = -EFAULT;
			goto err_quit;
		}
		// get secure handle
		sec_handle = dmabuf_to_secure_handle(dmabuf);
		if (!sec_handle) {
			WV_LOG(0, "get secure handle failed\n");
			ret = -EFAULT;
			goto err_quit;
		}
		in_out_param.sec_handle = sec_handle;
		if (copy_to_user((void *)arg, &in_out_param, sizeof(struct WV_FD_TO_SEC_HANDLE))) {
			WV_LOG(0, "Copy to user error\n");
			ret = -EFAULT;
			goto err_quit;
		}
		break;
	default:
		WV_LOG(0, "unknown ioctl cmd\n");
		ret = -EINVAL;
		break;
	}

err_quit:
	if (!IS_ERR_OR_NULL(dmabuf))
		dma_buf_put(dmabuf);
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_wv_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	WV_LOG(1, "use compatible ioctl\n");
	return wv_cdev->ops->unlocked_ioctl(filep, cmd, arg);
}
#endif

/*****file operation structure********/
static const struct file_operations wv_fops = {
	.owner = THIS_MODULE,
	.open  = wv_open,
	.release = wv_release,
	.unlocked_ioctl = wv_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_wv_ioctl,
#endif
};

static int wv_probe(struct platform_device *pdev)
{
	int ret = 0;

	WV_LOG(0, "start probe\n");
	// create char device
	ret = alloc_chrdev_region(&wv_devt, 0, 1, WV_DEVNAME);
	if (ret < 0) {
		WV_LOG(0, "alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}

	// Allocate driver
	wv_cdev = cdev_alloc();
	if (wv_cdev == NULL) {
		WV_LOG(0, "cdev_alloc failed\n");
		ret = -ENOMEM;
		goto out;
	}

	// Attach file operation
	cdev_init(wv_cdev, &wv_fops);
	wv_cdev->owner = THIS_MODULE;
	wv_cdev->ops = &wv_fops;

	// Add to system
	ret = cdev_add(wv_cdev, wv_devt, 1);
	if (ret < 0) {
		WV_LOG(0, "cdev_add failed, %d\n", ret);
		goto out;
	}

	wv_class = class_create(THIS_MODULE, WV_DEVNAME);
	if (!wv_class) {
		ret = -1;
		WV_LOG(0, "class_create error\n");
		goto out;
	}

	wv_device = device_create(wv_class, NULL, wv_devt, NULL, WV_DEVNAME);
	if (!wv_device) {
		ret = -1;
		WV_LOG(0, "device_create error\n");
		goto out;
	}

	WV_LOG(0, "probe done\n");
	return 0;

out:
	// Release char driver
	if (wv_cdev != NULL) {
		cdev_del(wv_cdev);
		wv_cdev = NULL;
	}
	unregister_chrdev_region(wv_devt, 1);
	return ret;
}

static int wv_remove(struct platform_device *pdev)
{
	WV_LOG(0, "remove device\n");

	device_destroy(wv_class, wv_devt);
	class_destroy(wv_class);
	// Release char driver
	if (wv_cdev != NULL) {
		cdev_del(wv_cdev);
		wv_cdev = NULL;
	}
	unregister_chrdev_region(wv_devt, 1);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id wv_of_match[] = {
	{.compatible = "mediatek,drm_wv",},
	{}
};
#endif

MODULE_DEVICE_TABLE(of, wv_of_match);

static struct platform_driver mtk_wv_driver = {
	.probe = wv_probe,
	.remove = wv_remove,
	.driver = {
		.name = WV_DEVNAME,
		.owner  = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = wv_of_match,
#endif
	},
};

static int __init mtk_wv_init(void)
{
	WV_LOG(0, "Register the widevine driver\n");

	if (platform_driver_register(&mtk_wv_driver)) {
		WV_LOG(0, "failed to register widevine device\n");
		return -ENODEV;
	}

	return 0;
}

/*uninstall device*/
static void __exit mtk_wv_exit(void)
{
	platform_driver_unregister(&mtk_wv_driver);
	WV_LOG(0, "driver exit successful\n");
}

module_init(mtk_wv_init);
module_exit(mtk_wv_exit);
MODULE_AUTHOR("Shan Zhang <shan.zhang@mediatek.com>");
MODULE_DESCRIPTION("Widevine Drm Driver");
MODULE_LICENSE("GPL");
