/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#ifndef __HQ_SYSFS_MISC_H__
#define __HQ_SYSFS_MISC_H__

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <linux/atomic.h>
#include <linux/ctype.h>

#include <linux/hqsysfs.h>



#define HUAQIN_MISC_NAME	"misc"


enum misc_id{
	MISC_EMMC_SIZE = 0,
	MISC_RAM_SIZE,
	MISC_BOOT_MODE,
	MISC_OTP_SN,
	MISC_NONE
};

struct emmc_info{
	unsigned int cid[4] ;
	const char *emmc_name;
};

struct misc_info{
	enum misc_id m_id;
	struct attribute attr;
};

struct cam_info{
	char *cam_drv_name;
	char *cam_vendro_name;
};

#define __MISC(_id, _misc_name) {				\
	.m_id = _id,				\
	.attr = {.name = __stringify(_misc_name),				\
	.mode = VERIFY_OCTAL_PERMISSIONS(S_IWUSR|S_IRUGO) },		\
}


#define MISC_INFO(_id, _misc_name) \
struct misc_info misc_info_##_misc_name = __MISC(_id, _misc_name)



#define __EMMC(cid_0, cid_1, cid_2, cid_3, _emmc_name) {				\
	.cid[0] = cid_0,											\
	.cid[1] = cid_1,											\
	.cid[2] = cid_2,											\
	.cid[3] = cid_3,											\
	.emmc_name	= __stringify(_emmc_name),						\
}


#define EMMC_INFO(cid_0, cid_1, cid_2, cid_3, _emmc_name) \
struct emmc_info emmc_info_##_emmc_name = __EMMC(cid_0, cid_1, cid_2, cid_3, _emmc_name)



#define CAM_MAP_INFO(_drv, _vendor)  \
struct cam_info cam_info_##_drv = { \
	.cam_drv_name = __stringify(_drv),                           \
	.cam_vendro_name = __stringify(_vendor),                           \
}

char *get_emmc_name(void);
char *map_cam_drv_to_vendor(char *drv);


#endif
