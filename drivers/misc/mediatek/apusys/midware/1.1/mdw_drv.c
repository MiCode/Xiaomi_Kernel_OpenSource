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

#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>

#include "apusys_power.h"

#include "apusys_drv.h"
#include "mdw_dbg.h"
#include "mdw_cmn.h"
#include "mdw_cmd.h"
#include "mdw_mem.h"
#include "mdw_usr.h"
#include "mdw_rsc.h"
#include "mdw_sched.h"
#include "mdw_tag.h"
#include "mdw_sysfs.h"

/* define */
#define APUSYS_DEV_NAME "apusys"
//#define MDW_LOAD_FW_SUPPORT

/* global variable */
static dev_t mdw_devt;
static struct cdev *mdw_cdev;
static struct class *mdw_class;
struct device *mdw_device;

/* function declaration */
static int mdw_open(struct inode *, struct file *);
static int mdw_release(struct inode *, struct file *);
static long mdw_ioctl(struct file *, unsigned int, unsigned long);
static long mdw_compat_ioctl(struct file *, unsigned int, unsigned long);

static const struct file_operations mdw_fops = {
	.open = mdw_open,
	.unlocked_ioctl = mdw_ioctl,
	.compat_ioctl = mdw_compat_ioctl,
	.release = mdw_release,
};

static int mdw_open(struct inode *inode, struct file *filp)
{
	struct mdw_usr *u;

	u = mdw_usr_create();
	if (!u)
		return -ENOMEM;

	filp->private_data = u;

	return 0;
}

static int mdw_release(struct inode *inode, struct file *filp)
{
	struct mdw_usr *u;

	u = filp->private_data;
	mdw_usr_put(u);

	return 0;
}

static int mdw_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = NULL;

	mdw_drv_info("+\n");

	if (!apusys_power_check()) {
		mdw_drv_err("apusys disable\n");
		return -ENODEV;
	}

	mdw_device = &pdev->dev;

	/* get major */
	ret = alloc_chrdev_region(&mdw_devt, 0, 1, APUSYS_DEV_NAME);
	if (ret < 0) {
		mdw_drv_err("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}

	/* Allocate driver */
	mdw_cdev = cdev_alloc();
	if (mdw_cdev == NULL) {
		mdw_drv_err("cdev_alloc failed\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Attatch file operation. */
	cdev_init(mdw_cdev, &mdw_fops);
	mdw_cdev->owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(mdw_cdev, mdw_devt, 1);
	if (ret < 0) {
		mdw_drv_err("attatch file operation failed, %d\n", ret);
		goto out;
	}

	/* Create class register */
	mdw_class = class_create(THIS_MODULE, "apusysdrv");
	if (IS_ERR(mdw_class)) {
		ret = PTR_ERR(mdw_class);
		mdw_drv_err("unable to create class, err = %d\n", ret);
		goto out;
	}

	dev = device_create(mdw_class, NULL, mdw_devt,
		NULL, APUSYS_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		mdw_drv_err("failed to create device: /dev/%s, err = %d",
			APUSYS_DEV_NAME, ret);
		goto out;
	}

	mdw_dbg_init();
	mdw_sysfs_init(mdw_device);
	mdw_tag_init();
	mdw_mem_init();
	mdw_rsc_init();
	mdw_usr_init();
	mdw_drv_info("-\n");

	return 0;

out:
	/* Release device */
	if (dev != NULL)
		device_destroy(mdw_class, mdw_devt);

	/* Release class */
	if (mdw_class != NULL)
		class_destroy(mdw_class);

	/* Release char driver */
	if (mdw_cdev != NULL) {
		cdev_del(mdw_cdev);
		mdw_cdev = NULL;
	}
	unregister_chrdev_region(mdw_devt, 1);

	return ret;
}

static int mdw_remove(struct platform_device *pdev)
{
	mdw_drv_info("+\n");

	mdw_usr_exit();
	mdw_rsc_exit();
	mdw_mem_exit();
	mdw_tag_exit();
	mdw_sysfs_exit();
	mdw_dbg_exit();

	/* Release device */
	device_destroy(mdw_class, mdw_devt);

	/* Release class */
	if (mdw_class != NULL)
		class_destroy(mdw_class);

	/* Release char driver */
	if (mdw_cdev != NULL) {
		cdev_del(mdw_cdev);
		mdw_cdev = NULL;
	}
	unregister_chrdev_region(mdw_devt, 1);

	mdw_drv_info("-\n");
	return 0;
}

static int mdw_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return mdw_sched_pause();
}

static int mdw_resume(struct platform_device *pdev)
{
	mdw_sched_restart();
	return 0;
}

static int mdw_handshake(struct apusys_ioctl_hs *hs)
{
	int ret = 0;

	hs->magic_num = mdw_cmd_get_magic();
	hs->cmd_version = mdw_cmd_get_ver();
	switch (hs->type) {
	case APUSYS_HANDSHAKE_BEGIN:
		hs->begin.mem_support = mdw_mem_get_support();
		hs->begin.dev_support = mdw_rsc_get_dev_bmp();
		hs->begin.dev_type_max = 64;
		if (hs->begin.mem_support & (1UL << APUSYS_MEM_VLM)) {
			mdw_mem_get_vlm(&hs->begin.vlm_start,
				&hs->begin.vlm_size);
		} else {
			hs->begin.vlm_start = 0;
			hs->begin.vlm_size = 0;
		}

		mdw_drv_debug("support dev(0x%llx)mem(0x%x/0x%x/%u)\n",
			hs->begin.dev_support,
			hs->begin.mem_support,
			hs->begin.vlm_start,
			hs->begin.vlm_size);
		break;

	case APUSYS_HANDSHAKE_QUERY_DEV:
		hs->dev.num = mdw_rsc_get_dev_num(hs->dev.type);
		if (hs->dev.num <= 0)
			ret = -ENODEV;
		mdw_drv_debug("dev(%u) num(%u)\n", hs->dev.type, hs->dev.num);
		break;

	default:
		mdw_drv_debug("wrong handshake type(%d)\n",
			hs->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long mdw_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct apusys_mem um;
	struct apusys_ioctl_cmd ucmd;
	struct apusys_ioctl_hs hs;
	struct mdw_usr *u;
	struct apusys_ioctl_power upwr;
	struct apusys_ioctl_ucmd uc;
	struct apusys_ioctl_sec us;
#ifdef MDW_LOAD_FW_SUPPORT
	struct apusys_ioctl_fw f;
#endif

	u = (struct mdw_usr *)filp->private_data;

	switch (cmd) {
	case APUSYS_IOCTL_HANDSHAKE:
		/* handshaking, pass kernel apusys information */
		if (copy_from_user(&hs, (void *)arg,
			sizeof(struct apusys_ioctl_hs))) {
			mdw_drv_err("copy handshake struct fail\n");
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_handshake(&hs);
		if (ret)
			goto out;

		if (copy_to_user((void *)arg, &hs,
			sizeof(struct apusys_ioctl_hs))) {
			mdw_drv_err("handshake with user fail\n");
			ret = -EINVAL;
		}
		break;

	case APUSYS_IOCTL_MEM_ALLOC:
		mdw_drv_warn("not support mem alloc\n");
		ret = -EINVAL;
		break;

	case APUSYS_IOCTL_MEM_IMPORT:
		if (copy_from_user(&um, (void *)arg,
			sizeof(struct apusys_mem))) {
			mdw_drv_err("copy mem struct fail\n");
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_usr_mem_import(&um, u);
		if (ret)
			goto out;

		if (copy_to_user((void *)arg, &um,
			sizeof(struct apusys_mem))) {
			mdw_drv_err("copy mem struct to u fail\n");
			ret = -EINVAL;
		}
		break;

	case APUSYS_IOCTL_MEM_MAP:
		if (copy_from_user(&um, (void *)arg,
			sizeof(struct apusys_mem))) {
			mdw_drv_err("copy mem struct fail\n");
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_usr_mem_map(&um, u);
		if (ret)
			goto out;

		if (copy_to_user((void *)arg, &um,
			sizeof(struct apusys_mem))) {
			mdw_drv_err("copy mem struct to u fail\n");
			ret = -EINVAL;
		}
		break;

	case APUSYS_IOCTL_MEM_FREE:
	case APUSYS_IOCTL_MEM_UNIMPORT:
	case APUSYS_IOCTL_MEM_UNMAP:
		if (copy_from_user(&um, (void *)arg,
			sizeof(struct apusys_mem))) {
			mdw_drv_err("copy mem struct fail\n");
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_usr_mem_free(&um, u);
		break;

	case APUSYS_IOCTL_RUN_CMD_SYNC:
		if (copy_from_user(&ucmd, (void *)arg,
			sizeof(struct apusys_ioctl_cmd))) {
			mdw_drv_err("copy cmd struct fail\n");
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_usr_run_cmd_sync(u, &ucmd);
		break;

	case APUSYS_IOCTL_RUN_CMD_ASYNC:
		if (copy_from_user(&ucmd, (void *)arg,
			sizeof(struct apusys_ioctl_cmd))) {
			mdw_drv_err("copy cmd struct fail\n");
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_usr_run_cmd_async(u, &ucmd);

		if (copy_to_user((void *)arg, &ucmd,
			sizeof(struct apusys_ioctl_cmd))) {
			mdw_drv_err("copy cmd struct to u fail\n");
			ret = -EINVAL;
		}
		break;

	case APUSYS_IOCTL_WAIT_CMD:
		if (copy_from_user(&ucmd, (void *)arg,
			sizeof(struct apusys_ioctl_cmd))) {
			mdw_drv_err("copy cmd struct fail\n");
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_usr_wait_cmd(u, &ucmd);
		break;

	case APUSYS_IOCTL_SET_POWER:
		ret = copy_from_user(&upwr, (void *)arg,
			sizeof(struct apusys_ioctl_power));
		if (ret) {
			mdw_drv_err("copy power struct fail\n");
			goto out;
		}

		ret = mdw_usr_set_pwr(&upwr);
		break;

	case APUSYS_IOCTL_DEVICE_ALLOC:
		ret = -EINVAL;
		break;

	case APUSYS_IOCTL_DEVICE_FREE:
		ret = -EINVAL;
		break;

	case APUSYS_IOCTL_FW_LOAD:
#ifdef MDW_LOAD_FW_SUPPORT
		ret = copy_from_user(&f, (void *)arg,
			sizeof(struct apusys_ioctl_fw));
		if (ret) {
			mdw_drv_err("copy fw struct fail\n");
			goto out;
		}

		ret = mdw_usr_fw(&f, APUSYS_FIRMWARE_LOAD);
#else
		ret = -EINVAL;
		mdw_drv_warn("not support fw load\n");
#endif
		break;

	case APUSYS_IOCTL_FW_UNLOAD:
#ifdef MDW_LOAD_FW_SUPPORT
		ret = copy_from_user(&f, (void *)arg,
			sizeof(struct apusys_ioctl_fw));
		if (ret) {
			mdw_drv_err("copy fw struct fail\n");
			goto out;
		}

		ret = mdw_usr_fw(&f, APUSYS_FIRMWARE_UNLOAD);
#else
		ret = -EINVAL;
		mdw_drv_warn("not suppot fw unload\n");
#endif
		break;

	case APUSYS_IOCTL_USER_CMD:
		ret = copy_from_user(&uc, (void *)arg,
			sizeof(struct apusys_ioctl_ucmd));
		if (ret) {
			mdw_drv_err("copy ucmd struct fail\n");
			goto out;
		}

		ret = mdw_usr_ucmd(&uc);
		break;

	case APUSYS_IOCTL_SEC_DEVICE_LOCK:
		ret = copy_from_user(&us, (void *)arg,
			sizeof(struct apusys_ioctl_sec));
		if (ret) {
			mdw_drv_err("copy sec struct fail\n");
			goto out;
		}

		ret = mdw_usr_dev_sec_alloc(us.dev_type, u);
		break;

	case APUSYS_IOCTL_SEC_DEVICE_UNLOCK:
		ret = copy_from_user(&us, (void *)arg,
			sizeof(struct apusys_ioctl_sec));
		if (ret) {
			mdw_drv_err("copy sec struct fail\n");
			goto out;
		}

		if (us.dev_type >= APUSYS_DEVICE_MAX || us.dev_type < 0) {
			mdw_drv_err("invalid sec type(%d)\n", us.dev_type);
			ret = -EINVAL;
			goto out;
		}

		ret = mdw_usr_dev_sec_free(us.dev_type, u);
		break;

	default:
		ret = -EINVAL;
		break;
	}

out:
	mdw_flw_debug("cmd(%u)ret(%d)\n", _IOC_NR(cmd), ret);
	return ret;
}

static long mdw_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case APUSYS_IOCTL_HANDSHAKE:
	case APUSYS_IOCTL_MEM_ALLOC:
	case APUSYS_IOCTL_MEM_FREE:
	case APUSYS_IOCTL_MEM_IMPORT:
	case APUSYS_IOCTL_MEM_UNIMPORT:
	case APUSYS_IOCTL_MEM_CTL:
	case APUSYS_IOCTL_RUN_CMD_SYNC:
	case APUSYS_IOCTL_RUN_CMD_ASYNC:
	case APUSYS_IOCTL_WAIT_CMD:
	case APUSYS_IOCTL_SET_POWER:
	case APUSYS_IOCTL_DEVICE_ALLOC:
	case APUSYS_IOCTL_DEVICE_FREE:
	case APUSYS_IOCTL_FW_LOAD:
	case APUSYS_IOCTL_FW_UNLOAD:
	case APUSYS_IOCTL_USER_CMD:
	case APUSYS_IOCTL_SEC_DEVICE_LOCK:
	case APUSYS_IOCTL_SEC_DEVICE_UNLOCK:
	case APUSYS_IOCTL_MEM_MAP:
	case APUSYS_IOCTL_MEM_UNMAP:
	{
		return flip->f_op->unlocked_ioctl(flip, cmd,
					(unsigned long)compat_ptr(arg));
	}
	default:
		return -ENOIOCTLCMD;
		/*return vpu_ioctl(flip, cmd, arg);*/
	}

	return 0;
}

#ifdef CONFIG_PM
static int mdw_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);
	return mdw_suspend(pdev, PMSG_SUSPEND);
}

static int mdw_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);
	return mdw_resume(pdev);
}

static int mdw_pm_restore_noirq(struct device *device)
{
	return 0;
}

static const struct dev_pm_ops mdw_pm_ops = {
	.suspend = mdw_pm_suspend,
	.resume = mdw_pm_resume,
	.freeze = mdw_pm_suspend,
	.thaw = mdw_pm_resume,
	.poweroff = mdw_pm_suspend,
	.restore = mdw_pm_resume,
	.restore_noirq = mdw_pm_restore_noirq,
};
#endif

static struct platform_driver mdw_drv = {
	.probe = mdw_probe,
	.remove = mdw_remove,
	.suspend = mdw_suspend,
	.resume  = mdw_resume,
	.driver = {
		.name = APUSYS_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &mdw_pm_ops,
#endif
	},
};

static void mdw_dev_release(struct device *dev)
{
}

static struct platform_device mdw_dev = {
	.name = APUSYS_DEV_NAME,
	.id = -1,
	.dev = {
			.release = mdw_dev_release,
		},
};

static int __init mdw_init(void)
{
	if (!apusys_power_check()) {
		mdw_drv_err("apusys disable\n");
		return -ENODEV;
	}

	if (platform_driver_register(&mdw_drv)) {
		mdw_drv_err("failed to register apusys midware driver");
		return -ENODEV;
	}

	if (platform_device_register(&mdw_dev)) {
		mdw_drv_err("failed to register apusys midware device");
		platform_driver_unregister(&mdw_drv);
		return -ENODEV;
	}

	return 0;
}

static void __exit mdw_exit(void)
{
	platform_driver_unregister(&mdw_drv);
	platform_device_unregister(&mdw_dev);
}

module_init(mdw_init);
module_exit(mdw_exit);
MODULE_DESCRIPTION("MTK APUSys Middleware Driver");
MODULE_AUTHOR("SPT1");
MODULE_LICENSE("GPL");
