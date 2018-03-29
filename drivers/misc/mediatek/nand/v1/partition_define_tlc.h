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


#ifndef __PARTITION_DEFINE_H__
#define __PARTITION_DEFINE_H__




#define KB  (1024)
#define MB  (1024 * KB)
#define GB  (1024 * MB)

#define PART_PRELOADER "PRELOADER"
#define PART_PRO_INFO "PRO_INFO"
#define PART_NVRAM "NVRAM"
#define PART_PROTECT_F "PROTECT_F"
#define PART_PROTECT_S "PROTECT_S"
#define PART_SECCFG "SECCFG"
#define PART_UBOOT "UBOOT"
#define PART_BOOTIMG "BOOTIMG"
#define PART_RECOVERY "RECOVERY"
#define PART_SEC_RO "SEC_RO"
#define PART_MISC "MISC"
#define PART_FRP "FRP"
#define PART_LOGO "LOGO"
#define PART_EXPDB "EXPDB"
#define PART_FAT "FAT"
#define PART_ANDROID "ANDROID"
#define PART_CACHE "CACHE"
#define PART_NVDATA "NVDATA"
#define PART_USRDATA "USRDATA"
#define PART_BMTPOOL "BMTPOOL"
/*preloader re-name*/
#define PART_SECURE "SECURE"
#define PART_SECSTATIC "SECSTATIC"
#define PART_ANDSYSIMG "ANDSYSIMG"
#define PART_USER "USER"
/*Uboot re-name*/
#define PART_APANIC "APANIC"

#define PART_FLAG_NONE              0
#define PART_FLAG_LEFT             0x1
#define PART_FLAG_END              0x2
#define PART_MAGIC              0x58881688

#if defined(CONFIG_MTK_MLC_NAND_SUPPORT)
#define PART_SIZE_BMTPOOL			(168*1024*1024)
#else
#define PART_SIZE_BMTPOOL			(14*1024*1024)
#endif

#if 0
#ifdef CONFIG_MTK_EMMC_SUPPORT
#define PART_SIZE_SECCFG			(128*KB)
#define PART_OFFSET_SECCFG			(0x2900000)
#define PART_SIZE_SEC_RO			(256*KB)
#define PART_OFFSET_SEC_RO			(0x3780000)
#else
#define PART_SIZE_SECCFG			(256*KB)
#define PART_OFFSET_SECCFG			(0xb00000)
#define PART_SIZE_SEC_RO			(256*KB)
#define PART_OFFSET_SEC_RO			(0x19c0000)
#endif
#else
#define PART_SIZE_SECCFG			0
#define PART_OFFSET_SECCFG			0
#define PART_SIZE_SEC_RO			0
#define PART_OFFSET_SEC_RO			0
#endif

#ifndef RAND_START_ADDR
#define RAND_START_ADDR   0
#endif


#define PART_MAX_COUNT			 40


#define WRITE_SIZE_Byte		(16*1024)

typedef enum {
	EMMC = 1,
	NAND = 2,
} dev_type;

typedef enum {
	USER = 0,
	BOOT_1,
	BOOT_2,
	RPMB,
	GP_1,
	GP_2,
	GP_3,
	GP_4,
} Region;


struct excel_info {
	char *name;
	unsigned long long size;
	unsigned long long start_address;
	dev_type type;
	unsigned int partition_idx;
	Region region;
};

extern struct excel_info PartInfo[PART_MAX_COUNT];


#else
extern int get_part_num_nand(void);

#define PART_NUM			get_part_num_nand()

#endif
