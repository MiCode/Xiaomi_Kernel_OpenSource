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
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <asm/memblock.h>
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
#define IOCTL_EMI_MEMORY_INIT        1
#define IOCTL_MNL_NVRAM_FILE_TO_MEM  2
#define IOCTL_MNL_NVRAM_MEM_TO_FILE  3
#define IOCTL_ADC_CAPTURE_ADDR_GET   4

#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6768)
#define GPS_EMI_MPU_REGION           29
#define GPS_EMI_BASE_ADDR_OFFSET     (2*SZ_1M + SZ_1M/2 + 0x1000)
#define GPS_EMI_MPU_SIZE             (SZ_1M + SZ_1M/2 - 0x2000)
#define EMI_MPU_PROTECTION_IS_READY  1
#if EMI_MPU_PROTECTION_IS_READY
#include <mt_emi_api.h>
#endif
#endif
#if defined(CONFIG_MACH_MT6779) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
#define GPS_EMI_MPU_REGION           29
#define GPS_EMI_BASE_ADDR_OFFSET     (3*SZ_1M + 0x10000)
#define GPS_EMI_MPU_SIZE             (0xF0000)
#define EMI_MPU_PROTECTION_IS_READY  1
#if EMI_MPU_PROTECTION_IS_READY
#include <mt_emi_api.h>
#endif
#endif
#if defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6775) || defined(CONFIG_MACH_MT6758)
#define GPS_EMI_MPU_REGION           30
#define GPS_EMI_BASE_ADDR_OFFSET     (SZ_1M)
#define GPS_EMI_MPU_SIZE             (SZ_1M)
#define EMI_MPU_PROTECTION_IS_READY  1
#if EMI_MPU_PROTECTION_IS_READY
#include <mt_emi_api.h>
#endif
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6893)
#define GPS_EMI_NEW_API
#define GPS_EMI_MPU_REGION           29
#define GPS_EMI_BASE_ADDR_OFFSET     (3*SZ_1M + 0x10000)
#define GPS_EMI_MPU_SIZE             (0xF0000)
#define GPS_DL_EMI_MPU_DOMAIN_AP      0
#define GPS_DL_EMI_MPU_DOMAIN_CONN    2
#define EMI_MPU_PROTECTION_IS_READY  1
#if EMI_MPU_PROTECTION_IS_READY
#include <memory/mediatek/emi.h>
#endif
#endif
#if defined(CONFIG_MACH_MT6833)
#define GPS_EMI_NEW_API
#define GPS_EMI_MPU_REGION           29
#define GPS_EMI_BASE_ADDR_OFFSET     (4*SZ_1M)
#define GPS_EMI_MPU_SIZE             (0xFFFFF)
#define GPS_DL_EMI_MPU_DOMAIN_AP      0
#define GPS_DL_EMI_MPU_DOMAIN_CONN    2
#define EMI_MPU_PROTECTION_IS_READY  0
#if EMI_MPU_PROTECTION_IS_READY
#include <memory/mediatek/emi.h>
#endif
#endif
#if defined(CONFIG_MACH_MT6779) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
#define GPS_EMI_NEW_API
#define GPS_EMI_MPU_REGION           29
#define GPS_EMI_BASE_ADDR_OFFSET     (3*SZ_1M + 0x10000)
#define GPS_EMI_MPU_SIZE             (0xF0000)
#define GPS_DL_EMI_MPU_DOMAIN_AP      0
#define GPS_DL_EMI_MPU_DOMAIN_CONN    2
#define EMI_MPU_PROTECTION_IS_READY  1
#if EMI_MPU_PROTECTION_IS_READY
#include <memory/mediatek/emi.h>
#endif
#endif

#define GPS_ADC_CAPTURE_BUFF_SIZE   0x50000
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
/* typedef unsigned char   UINT8, *PUINT8, **PPUINT8; */

/******************************************************************************
 * local variables
******************************************************************************/
phys_addr_t gGpsEmiPhyBase;
void __iomem *pGpsEmibaseaddr;
struct gps_emi_dev *devobj;

static struct semaphore fw_dl_mtx;

void mtk_wcn_consys_gps_memory_reserve(void)
{
#if 0
#ifdef MTK_WCN_ARM64
	gGpsEmiPhyBase = arm64_memblock_steal(SZ_1M, SZ_1M);
#else
	gGpsEmiPhyBase = arm_memblock_steal(SZ_1M, SZ_1M);
#endif
#else
	#if EMI_MPU_PROTECTION_IS_READY
	gGpsEmiPhyBase = gConEmiPhyBase + GPS_EMI_BASE_ADDR_OFFSET;
	#endif
#endif
	if (gGpsEmiPhyBase)
		GPS_DBG("Con:0x%zx, Gps:0x%zx\n", (size_t)gConEmiPhyBase, (size_t)gGpsEmiPhyBase);
	else
		GPS_DBG("memblock fail\n");
}

INT32 gps_emi_mpu_set_region_protection(INT32 region)
{
#if EMI_MPU_PROTECTION_IS_READY
#if defined(GPS_EMI_NEW_API)
	struct emimpu_region_t region_info;
	int emimpu_ret1, emimpu_ret2, emimpu_ret3, emimpu_ret4, emimpu_ret5, emimpu_ret6;
	/* Set EMI MPU permission */
	GPS_DBG("emi mpu cfg: region = %d, no protection domain = %d, %d",
	    region, GPS_DL_EMI_MPU_DOMAIN_AP, GPS_DL_EMI_MPU_DOMAIN_CONN);
	emimpu_ret1 = mtk_emimpu_init_region(&region_info, region);
	emimpu_ret2 = mtk_emimpu_set_addr(&region_info, gGpsEmiPhyBase, gGpsEmiPhyBase + GPS_EMI_MPU_SIZE - 1);
	emimpu_ret3 = mtk_emimpu_set_apc(&region_info, GPS_DL_EMI_MPU_DOMAIN_AP, MTK_EMIMPU_NO_PROTECTION);
	emimpu_ret4 = mtk_emimpu_set_apc(&region_info, GPS_DL_EMI_MPU_DOMAIN_CONN, MTK_EMIMPU_NO_PROTECTION);
	emimpu_ret5 = mtk_emimpu_set_protection(&region_info);
	emimpu_ret6 = mtk_emimpu_free_region(&region_info);
	GPS_DBG("emi mpu cfg: ret = %d, %d, %d, %d, %d, %d",
	    emimpu_ret1, emimpu_ret2, emimpu_ret3, emimpu_ret4, emimpu_ret5, emimpu_ret6);
#else
	struct emi_region_info_t region_info;
	/*set MPU for EMI share Memory */
	GPS_DBG("setting MPU for EMI share memory\n");
	region_info.start = gGpsEmiPhyBase;
	region_info.end = gGpsEmiPhyBase + GPS_EMI_MPU_SIZE - 1;
	region_info.region = region;
	SET_ACCESS_PERMISSION(region_info.apc, LOCK,
	FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
	FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
	NO_PROTECTION, FORBIDDEN, NO_PROTECTION);
	emi_mpu_set_protection(&region_info);
#endif
#endif
	return 0;
}

INT32 gps_emi_patch_get(PUINT8 pPatchName, osal_firmware **ppPatch)
{
	INT32 iRet;
	osal_firmware *fw;

	iRet = -1;
	fw = NULL;
	if (!ppPatch) {
		GPS_DBG("invalid ppBufptr!\n");
		return -1;
	}
	*ppPatch = NULL;
	iRet = request_firmware((const struct firmware **)&fw, pPatchName, NULL);
	if (iRet != 0) {
		GPS_DBG("failed to open or read!(%s)\n", pPatchName);
		release_firmware(fw);
		return -1;
	}
	GPS_DBG("loader firmware %s  ok!!\n", pPatchName);
	iRet = 0;
	*ppPatch = fw;

	return iRet;
}

INT32 mtk_wcn_consys_gps_emi_init(void)
{
	INT32 iRet;

	iRet = -1;
	down(&fw_dl_mtx);
	mtk_wcn_consys_gps_memory_reserve();
	if (gGpsEmiPhyBase) {
		/*set MPU for EMI share Memory*/
		#if EMI_MPU_PROTECTION_IS_READY
		GPS_DBG("setting MPU for EMI share memory\n");
		gps_emi_mpu_set_region_protection(GPS_EMI_MPU_REGION);
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
		#if EMI_MPU_PROTECTION_IS_READY
		pGpsEmibaseaddr = ioremap_nocache(gGpsEmiPhyBase, GPS_EMI_MPU_SIZE);
		#endif
		iRet = 1;
		#if 0
		if (pGpsEmibaseaddr != NULL) {
			unsigned char *pFullPatchName = "MNL.bin";
			osal_firmware *pPatch = NULL;

			GPS_DBG("EMI mapping OK(0x%p)\n", pGpsEmibaseaddr);
			memset_io(pGpsEmibaseaddr, 0, GPS_EMI_MPU_SIZE);
			if ((pFullPatchName != NULL)
				&& (gps_emi_patch_get(pFullPatchName, &pPatch) == 0)) {
				if (pPatch != NULL) {
					/*get full name patch success*/
					GPS_DBG("get full patch name(%s) buf(0x%p) size(%ld)\n",
						pFullPatchName, (pPatch)->data, (pPatch)->size);
					GPS_DBG("AF get patch, pPatch(0x%p)\n", pPatch);
				}
			}
			if (pPatch != NULL) {
				if ((pPatch)->size <= GPS_EMI_MPU_SIZE) {
					GPS_DBG("Prepare to copy FW\n");
					memcpy(pGpsEmibaseaddr, (pPatch)->data, (pPatch)->size);
					iRet = 1;
					GPS_DBG("pGpsEmibaseaddr_1:0x%08x 0x%08x 0x%08x 0x%08x\n",
						*(unsigned int *)pGpsEmibaseaddr,
						*(((unsigned int *)pGpsEmibaseaddr)+1),
						*(((unsigned int *)pGpsEmibaseaddr)+2),
						*(((unsigned int *)pGpsEmibaseaddr)+3));
					GPS_DBG("pGpsEmibaseaddr_2:0x%08x 0x%08x 0x%08x 0x%08x\n",
						*(((unsigned int *)pGpsEmibaseaddr)+4),
						*(((unsigned int *)pGpsEmibaseaddr)+5),
						*(((unsigned int *)pGpsEmibaseaddr)+6),
						*(((unsigned int *)pGpsEmibaseaddr)+7));
				}
				release_firmware(pPatch);
				pPatch = NULL;
			}
			iounmap(pGpsEmibaseaddr);
		} else {
			GPS_DBG("EMI mapping fail\n");
		}
		#endif
	} else {
		GPS_DBG("gps emi memory address gGpsEmiPhyBase invalid\n");
	}
	up(&fw_dl_mtx);
	return iRet;
}

/*---------------------------------------------------------------------------*/
long gps_emi_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval;
	unsigned int *tmp;

	retval = 0;
	GPS_DBG("gps_emi:cmd (%d),arg(%ld)\n", cmd, arg);

	switch (cmd) {
	case IOCTL_EMI_MEMORY_INIT:
		retval = mtk_wcn_consys_gps_emi_init();
		GPS_DBG("IOCTL_EMI_MEMORY_INIT\n");
		break;

	case IOCTL_MNL_NVRAM_FILE_TO_MEM:
		GPS_DBG("IOCTL_MNL_NVRAM_FILE_TO_MEM\n");
		break;

	case IOCTL_MNL_NVRAM_MEM_TO_FILE:
		GPS_DBG("IOCTL_MNL_NVRAM_MEM_TO_FILE\n");
		break;

	case IOCTL_ADC_CAPTURE_ADDR_GET:
		tmp = (unsigned int *)&gGpsEmiPhyBase;
		GPS_DBG("gps_emi:gGpsEmiPhyBase (%x)\n", &gGpsEmiPhyBase);
		GPS_DBG("gps_emi:tmp  (%x)\n", tmp);
		if (copy_to_user((unsigned int __user *)arg, tmp, sizeof(unsigned int)))
			retval = -1;
		GPS_DBG("IOCTL_ADC_CAPTURE_ADDR_GET,(%d)\n", retval);
		break;

	default:
		GPS_DBG("unknown cmd (%d)\n", cmd);
		retval = 0;
		break;
	}
	return retval;

}

/******************************************************************************/
long gps_emi_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return gps_emi_unlocked_ioctl(filp, cmd, arg);
}

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
	ssize_t ret;

	ret = 0;
	GPS_DBG("gps_emi_read begin\n");
	if (count > GPS_ADC_CAPTURE_BUFF_SIZE)
		count = GPS_ADC_CAPTURE_BUFF_SIZE;
	if (pGpsEmibaseaddr != NULL) {
		if (copy_to_user(buf, (char *)pGpsEmibaseaddr, count))
			pr_err("Copy to user failed\n");
	}
	GPS_DBG("gps_emi_read finish\n");
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
	.compat_ioctl = gps_emi_compat_ioctl,
	.open = gps_emi_open,
	.read = gps_emi_read,
	.write = gps_emi_write,
	.release = gps_emi_release,
};

/*****************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id apgps_of_ids[] = {
	{ .compatible = "mediatek,gps_emi-v1", },
	{}
};
#endif
/*****************************************************************************/
static int __init gps_emi_mod_init(void)
{
	int ret = 0;
	int err = 0;

	GPS_ERR("gps emi mod register begin");

	sema_init(&fw_dl_mtx, 1);

	devobj = kzalloc(sizeof(*devobj), GFP_KERNEL);
	if (devobj == NULL) {
		err = -ENOMEM;
		ret = -ENOMEM;
		goto err_out;
	}

	GPS_ERR("Registering chardev\n");
	ret = alloc_chrdev_region(&devobj->devno, 0, 1, GPSEMI_DEVNAME);
	if (ret) {
		GPS_ERR("alloc_chrdev_region fail: %d\n", ret);
		err = -ENOMEM;
		goto err_out;
	} else {
		GPS_ERR("major: %d, minor: %d\n", MAJOR(devobj->devno), MINOR(devobj->devno));
	}
	cdev_init(&devobj->chdev, &gps_emi_fops);
	devobj->chdev.owner = THIS_MODULE;
	err = cdev_add(&devobj->chdev, devobj->devno, 1);
	if (err) {
		GPS_ERR("cdev_add fail: %d\n", err);
		goto err_out;
	}
	devobj->cls = class_create(THIS_MODULE, "gpsemi");
	if (IS_ERR(devobj->cls)) {
		GPS_ERR("Unable to create class, err = %d\n", (int)PTR_ERR(devobj->cls));
	goto err_out;
	}
	devobj->dev = device_create(devobj->cls, NULL, devobj->devno, devobj, "gps_emi");


	GPS_ERR("GPS EMI Done\n");
	return 0;

err_out:
	if (devobj != NULL) {
		if (err == 0)
			cdev_del(&devobj->chdev);
		if (ret == 0)
			unregister_chrdev_region(devobj->devno, 1);

		kfree(devobj);
		devobj = NULL;
	}
	return -1;
}

/*****************************************************************************/
static void __exit gps_emi_mod_exit(void)
{
	if (!devobj) {
		GPS_ERR("null pointer: %p\n", devobj);
		return;
	}

	GPS_ERR("Unregistering chardev\n");
	cdev_del(&devobj->chdev);
	unregister_chrdev_region(devobj->devno, 1);
	device_destroy(devobj->cls, devobj->devno);
	class_destroy(devobj->cls);
	kfree(devobj);
	GPS_ERR("Done\n");
}

int mtk_gps_emi_init(void)
{
	GPS_ERR("gps emi mod init begin");
	return gps_emi_mod_init();
}

void mtk_gps_emi_exit(void)
{
	GPS_ERR("gps emi mod exit begin");
	return gps_emi_mod_exit();
}

/*****************************************************************************/
#if 0
module_init(gps_emi_mod_init);
module_exit(gps_emi_mod_exit);
#endif
/*****************************************************************************/
MODULE_AUTHOR("Heiping Lei <Heiping.Lei@mediatek.com>");
MODULE_DESCRIPTION("GPS EMI Driver");
MODULE_LICENSE("GPL");
#endif
