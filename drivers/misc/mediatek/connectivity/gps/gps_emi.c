/*
 * Implementation of the GPS EMI driver.
 *
 * Copyright (C) 2014 Mediatek
 * Authors:
 * Heiping <Heiping.Lei@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*******************************************************************************
* Dependency
*******************************************************************************/
#ifdef CONFIG_MTK_GPS_EMI
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
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <asm/memblock.h>
#include <mach/emi_mpu.h>
#include "gps.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

/******************************************************************************
 * Definition
******************************************************************************/
/* device name and major number */
#define GPSEMI_DEVNAME            "gps_emi"
#define IOCTL_MNL_IMAGE_FILE_TO_MEM  1
#define IOCTL_MNL_NVRAM_FILE_TO_MEM  2
#define IOCTL_MNL_NVRAM_MEM_TO_FILE  3

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define GPS_DBG_NONE(fmt, arg...)    do {} while (0)
#define GPS_DBG pr_err
#define GPS_TRC GPS_DBG_NONE
#define GPS_VER pr_err
#define GPS_ERR pr_err
/*******************************************************************************
* structure & enumeration
*******************************************************************************/
/*---------------------------------------------------------------------------*/
struct gps_emi_dev {
	struct class *cls;
	struct device *dev;
	dev_t devno;
	struct cdev chdev;
};
typedef unsigned char   UINT8, *PUINT8, **PPUINT8;

/******************************************************************************
 * local variables
******************************************************************************/
phys_addr_t gGpsEmiPhyBase;
UINT8 __iomem *pGpsEmibaseaddr = NULL;
struct gps_emi_dev *devobj = NULL;
#define EMI_MPU_PROTECTION_IS_READY 1

void mtk_wcn_consys_gps_memory_reserve(void)
{
#if 0
#ifdef MTK_WCN_ARM64
	gGpsEmiPhyBase = arm64_memblock_steal(SZ_1M, SZ_1M);
#else
	gGpsEmiPhyBase = arm_memblock_steal(SZ_1M, SZ_1M);
#endif
#else
	gGpsEmiPhyBase = gConEmiPhyBase + SZ_1M;

#endif
	if (gGpsEmiPhyBase)
		GPS_DBG("memblock done: 0x%zx\n", (size_t)gGpsEmiPhyBase);
	else
		GPS_DBG("memblock fail\n");
}
INT32 mtk_wcn_consys_gps_emi_init(void)
{
	INT32 iRet = -1;

	mtk_wcn_consys_gps_memory_reserve();
	if (gGpsEmiPhyBase) {
    #if CONSYS_EMI_MPU_SETTING
		/*set MPU for EMI share Memory*/
		GPS_DBG("setting MPU for EMI share memory\n");
    #if EMI_MPU_PROTECTION_IS_READY
		emi_mpu_set_region_protection(gGpsEmiPhyBase,
			gGpsEmiPhyBase + SZ_1M - 1,
			20,
			SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
				FORBIDDEN, NO_PROTECTION, FORBIDDEN, NO_PROTECTION));
    #endif

    #endif
		GPS_DBG("get consys start phy address(0x%zx)\n", (size_t)gGpsEmiPhyBase);
    #if 0
		/*consys to ap emi remapping register:10001310, cal remapping address*/
		addrPhy = (gGpsEmiPhyBase & 0xFFF00000) >> 20;

		/*enable consys to ap emi remapping bit12*/
		addrPhy -= 0x400;/*Gavin ??*/
		addrPhy = addrPhy | 0x1000;

		CONSYS_REG_WRITE(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET,
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET) | addrPhy);

		GPS_DBG("GPS_EMI_MAPPING dump(0x%08x)\n",
			CONSYS_REG_READ(conn_reg.topckgen_base + CONSYS_EMI_MAPPING_OFFSET));
    #endif

		pGpsEmibaseaddr = ioremap_nocache(gGpsEmiPhyBase, SZ_1M);
		if (pGpsEmibaseaddr != NULL) {
			UINT8 *pFullPatchName = "/system/etc/firmware/MNL.bin";
		    osal_firmware *pPatch = NULL;

		    GPS_DBG("EMI mapping OK(0x%p)\n", pGpsEmibaseaddr);
		    memset_io(pGpsEmibaseaddr, 0, SZ_1M);
		    if ((NULL != pFullPatchName)
				&& (0 == wmt_dev_patch_get(pFullPatchName, &pPatch, 0/*BCNT_PATCH_BUF_HEADROOM*/))) {
				if (pPatch != NULL) {
						/*get full name patch success*/
						GPS_DBG("get full patch name(%s) buf(0x%p) size(%ld)\n",
							pFullPatchName, (pPatch)->data, (pPatch)->size);
						GPS_DBG("AF get patch, pPatch(0x%p)\n", pPatch);
				}
		    }
		    if (NULL != pPatch) {
				if ((pPatch)->size <= SZ_1M) {
					memcpy(pGpsEmibaseaddr, (pPatch)->data, (pPatch)->size);
					iRet = 1;
				}
			}
		} else{
		    GPS_DBG("EMI mapping fail\n");
		}
	} else {
		GPS_DBG("gps emi memory address gGpsEmiPhyBase invalid\n");
	}
	return iRet;
}

/*---------------------------------------------------------------------------*/
long gps_emi_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	GPS_DBG("cmd (%d),arg(%ld)\n", cmd, arg);

	switch (cmd) {
	case IOCTL_MNL_IMAGE_FILE_TO_MEM:
		retval = mtk_wcn_consys_gps_emi_init();
		GPS_DBG("IOCTL_MNL_IMAGE_FILE_TO_MEM\n");
		break;

	case IOCTL_MNL_NVRAM_FILE_TO_MEM:
		GPS_DBG("IOCTL_MNL_NVRAM_FILE_TO_MEM\n");
		break;

	case IOCTL_MNL_NVRAM_MEM_TO_FILE:
		GPS_DBG("IOCTL_MNL_NVRAM_MEM_TO_FILE\n");
		break;

	default:
		GPS_DBG("unknown cmd (%d)\n", cmd);
		retval = 0;
		break;
	}
	return retval;

}

/******************************************************************************/
/*****************************************************************************/
static int gps_emi_open(struct inode *inode, struct file *file)
{
	GPS_TRC();
	return nonseekable_open(inode, file);
}

/*****************************************************************************/


/*****************************************************************************/
static int gps_emi_release(struct inode *inode, struct file *file)
{
	GPS_TRC();

	return 0;
}

/******************************************************************************/
static ssize_t gps_emi_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;

	GPS_TRC();

	return ret;
}
/******************************************************************************/
static ssize_t gps_emi_write(struct file *file, const char __user *buf, size_t count,
		loff_t *ppos)
{
	ssize_t ret = 0;

	GPS_TRC();

	return ret;
}


/*****************************************************************************/
/* Kernel interface */
static const struct file_operations gps_emi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gps_emi_unlocked_ioctl,
	.open = gps_emi_open,
	.read = gps_emi_read,
	.write = gps_emi_write,
	.release = gps_emi_release,
};

/*****************************************************************************/
static int gps_emi_probe(struct platform_device *dev)
{
	int ret = 0, err = 0;

	devobj = kzalloc(sizeof(*devobj), GFP_KERNEL);
	if (devobj == NULL) {
		err = -ENOMEM;
		ret = -ENOMEM;
		goto err_out;
	}

	GPS_DBG("Registering chardev\n");
	ret = alloc_chrdev_region(&devobj->devno, 0, 1, GPSEMI_DEVNAME);
	if (ret) {
		GPS_ERR("alloc_chrdev_region fail: %d\n", ret);
		kfree(devobj);
		err = -ENOMEM;
		goto err_out;
	} else {
		GPS_DBG("major: %d, minor: %d\n", MAJOR(devobj->devno), MINOR(devobj->devno));
	}
	cdev_init(&devobj->chdev, &gps_emi_fops);
	devobj->chdev.owner = THIS_MODULE;
	err = cdev_add(&devobj->chdev, devobj->devno, 1);
	if (err) {
		GPS_ERR("cdev_add fail: %d\n", err);
		kfree(devobj);
		goto err_out;
	}
	devobj->cls = class_create(THIS_MODULE, "gpsemi");
	if (IS_ERR(devobj->cls)) {
		GPS_ERR("Unable to create class, err = %d\n", (int)PTR_ERR(devobj->cls));
		kfree(devobj);
		goto err_out;
	}
	devobj->dev = device_create(devobj->cls, NULL, devobj->devno, devobj, "gps_emi");

	GPS_DBG("Done\n");
	return 0;

err_out:
	if (err == 0)
		cdev_del(&devobj->chdev);
	if (ret == 0)
		unregister_chrdev_region(devobj->devno, 1);
	return -1;
}

/*****************************************************************************/
static int gps_emi_remove(struct platform_device *dev)
{
	if (!devobj) {
		GPS_ERR("null pointer: %p\n", devobj);
		return -1;
	}

	GPS_DBG("Unregistering chardev\n");
	cdev_del(&devobj->chdev);
	unregister_chrdev_region(devobj->devno, 1);
	device_destroy(devobj->cls, devobj->devno);
	class_destroy(devobj->cls);
	kfree(devobj);
	GPS_DBG("Done\n");
	return 0;
}

/*****************************************************************************/
#ifdef CONFIG_PM
/*****************************************************************************/
static int gps_emi_suspend(struct platform_device *dev, pm_message_t state)
{
	GPS_DBG("dev = %p, event = %u,", dev, state.event);
	if (state.event == PM_EVENT_SUSPEND)
		GPS_DBG("Receive PM_EVENT_SUSPEND!!\n");
	return 0;
}

/*****************************************************************************/
static int gps_emi_resume(struct platform_device *dev)
{
	GPS_DBG("");
	return 0;
}

/*****************************************************************************/
#endif        /* CONFIG_PM */
/*****************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id apgps_of_ids[] = {
	{ .compatible = "mediatek,gps_emi-v1", },
	{}
};
#endif
static struct platform_driver gps_emi_driver = {
	.probe = gps_emi_probe,
	.remove = gps_emi_remove,
#if defined(CONFIG_PM)
	.suspend = gps_emi_suspend,
	.resume = gps_emi_resume,
#endif
	.driver = {
		.name = GPSEMI_DEVNAME,
		.bus = &platform_bus_type,
#ifdef CONFIG_OF
		.of_match_table = apgps_of_ids,
#endif
	},
};

/*****************************************************************************/
static int __init gps_emi_mod_init(void)
{
	int ret = 0;

	GPS_TRC();

	ret = platform_driver_register(&gps_emi_driver);

	return ret;
}

/*****************************************************************************/
static void __exit gps_emi_mod_exit(void)
{
	GPS_TRC();
	platform_driver_unregister(&gps_emi_driver);
}

/*****************************************************************************/
module_init(gps_emi_mod_init);
module_exit(gps_emi_mod_exit);
/*****************************************************************************/
MODULE_AUTHOR("Heiping Lei <Heiping.Lei@mediatek.com>");
MODULE_DESCRIPTION("GPS EMI Driver");
MODULE_LICENSE("GPL");
#endif
