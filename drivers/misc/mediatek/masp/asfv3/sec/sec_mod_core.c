// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#include "sec_hal.h"
#include "sec_boot_lib.h"
#include "sec_version.h"
#include "sec_ioctl.h"

#define MOD                         "MASP"

#define CI_BLK_SIZE                 16
#define CI_BLK_ALIGN(len) (((len)+CI_BLK_SIZE-1) & ~(CI_BLK_SIZE-1))

/**************************************************************************
 *  GLOBAL VARIABLES
 **************************************************************************/
/* if sec is not enabled, this param will not be updated */
uint lks = 2;
module_param(lks, uint, 0444);	/* r--r--r-- */
MODULE_PARM_DESC(lks, "A device lks parameter under sysfs (0=NL, 1=L, 2=NA)");


/**************************************************************************
 *  SEC DRIVER EXIT
 **************************************************************************/
void sec_core_exit(void)
{
	pr_debug("[%s] version '%s%s', exit.\n", MOD, BUILD_TIME, BUILD_BRANCH);
}

/**************************************************************************
 *  SEC DRIVER IOCTL
 **************************************************************************/
long sec_core_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int ret = 0;
	unsigned int rid[4] = {0};

	/* ---------------------------------- */
	/* IOCTL                              */
	/* ---------------------------------- */

	if (_IOC_TYPE(cmd) != SEC_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > SEC_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok((void __user *)arg,
				             _IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok((void __user *)arg,
				             _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {

	/* ---------------------------------- */
	/* get random id                      */
	/* ---------------------------------- */
	case SEC_GET_RANDOM_ID:
		pr_debug("[%s] CMD - SEC_GET_RANDOM_ID\n", MOD);
		sec_get_random_id(&rid[0]);
		ret =
			osal_copy_to_user((void __user *)arg, (void *)&rid[0],
					  sizeof(unsigned int) * 4);
		break;

	/* ---------------------------------- */
	/* init boot info                     */
	/* ---------------------------------- */
	case SEC_BOOT_INIT:
		pr_debug("[%s] CMD - SEC_BOOT_INIT\n", MOD);
		ret = masp_boot_init();
		ret = osal_copy_to_user((void __user *)arg,
					(void *)&ret,
					sizeof(int));
		break;

	/* ---------------------------------- */
	/* check if secure usbdl is enbaled   */
	/* ---------------------------------- */
	case SEC_USBDL_IS_ENABLED:
		pr_debug("[%s] CMD - SEC_USBDL_IS_ENABLED\n", MOD);
		ret = sec_usbdl_enabled();
		ret = osal_copy_to_user((void __user *)arg,
					(void *)&ret,
					sizeof(int));
		break;

	/* ---------------------------------- */
	/* check if secure boot is enbaled    */
	/* ---------------------------------- */
	case SEC_BOOT_IS_ENABLED:
		pr_debug("[%s] CMD - SEC_BOOT_IS_ENABLED\n", MOD);
		ret = sec_boot_enabled();
		ret = osal_copy_to_user((void __user *)arg,
					(void *)&ret,
					sizeof(int));
		break;

	}

	return 0;
}
