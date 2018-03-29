/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __DUMCHAR_H__
#define __DUMCHAR_H__


#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/mtd/mtd.h>
#include <linux/semaphore.h>
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
#include "partition_define_tlc.h"
#else
#include "partition_define_mlc.h"
#endif

extern void env_init(loff_t env_part_addr, int mtd_number);
extern struct mtd_info *__mtd_next_device(int i);
extern int init_pmt(void);
/* #include <linux/mmc/sd_misc.h> */
/*
 * Macros to help debugging
 */
#define DUMCHAR_DEBUG
#undef DUMCHAR_DEBUG             /* undef it, just in case */

#ifdef DUMCHAR_DEBUG
#define DDEBUG(fmt, args...) pr_debug("dumchar_debug: " fmt, ## args)
#else
#define DDEBUG(fmt, args...)
#endif


#define DUMCHAR_MAJOR        0	/* dynamic major by default */
#define MAX_SD_BUFFER		(512)
#define ALIE_LEN		512


/* #define PrintBuff 1 */

struct dumchar_dev {
	char *dumname;		/* nvram boot userdata... */
	char actname[64];	/* full act name /dev/mt6573_sd0 /dev/mtd/mtd1 */
	struct semaphore sem;	/* Mutual exclusion */
	dev_type type;		/* nand device or emmc device? */
	unsigned long long size;	/* partition size */
	struct cdev cdev;
	Region region;		/* for emmc */
	unsigned long long start_address;	/* for emmc */
	unsigned int mtd_index;	/* for nand */
};

struct Region_Info {
	Region region;
	unsigned long long size_Byte;
};

struct file_obj {
	struct file *act_filp;
	int index;		/* index in dumchar_dev arry */
};

#define REGION_NUM						8
#define EXT_CSD_BOOT_SIZE_MULT          226	/* R */
#define EXT_CSD_RPMB_SIZE_MULT          168	/* R */

#define	MSDC_RAW_DEVICE					"/dev/misc-sd"
extern u64 mtd_partition_start_address(struct mtd_info *mtd);


#define mtd_for_each_device(mtd)			\
	for ((mtd) = __mtd_next_device(0);		\
	     (mtd) != NULL;				\
	     (mtd) = __mtd_next_device(mtd->index + 1))

#endif /*__DUMCHAR_H__ */
