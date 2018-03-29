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




#define KB  (1024ULL)
#define MB  (1024 * KB)
#define GB  (1024 * MB)

#define PART_PRELOADER "PRELOADER"
#define PART_MBR "MBR"
#define PART_EBR1 "EBR1"
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
#define PART_LOGO "LOGO"
#define PART_EBR2 "EBR2"
#define PART_EXPDB "EXPDB"
#define PART_TEE1 "TEE1"
#define PART_TEE2 "TEE2"
#define PART_KB "KB"
#define PART_DKB "DKB"
#define PART_ANDROID "ANDROID"
#define PART_CACHE "CACHE"
#define PART_USRDATA "USRDATA"
#define PART_FAT "FAT"
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

#define PART_SIZE_PRELOADER			(256*KB)
#define PART_SIZE_MBR			(512*KB)
#define PART_SIZE_EBR1			(512*KB)
#define PART_SIZE_PRO_INFO			(3072*KB)
#define PART_SIZE_NVRAM			(5120*KB)
#define PART_SIZE_PROTECT_F			(10240*KB)
#define PART_SIZE_PROTECT_S			(10240*KB)
#define PART_SIZE_SECCFG			(128*KB)
#define PART_OFFSET_SECCFG			(0x1d00000)
#define PART_SIZE_UBOOT			(384*KB)
#define PART_SIZE_BOOTIMG			(16384*KB)
#define PART_SIZE_RECOVERY			(16384*KB)
#define PART_SIZE_SEC_RO			(6144*KB)
#define PART_OFFSET_SEC_RO			(0x3d80000)
#define PART_SIZE_MISC			(512*KB)
#define PART_SIZE_LOGO			(3072*KB)
#define PART_SIZE_EBR2			(512*KB)
#define PART_SIZE_EXPDB			(10240*KB)
#define PART_SIZE_TEE1			(5120*KB)
#define PART_SIZE_TEE2			(5120*KB)
#define PART_SIZE_KB			(1024*KB)
#define PART_SIZE_DKB			(1024*KB)
#define PART_SIZE_ANDROID			(1048576*KB)
#define PART_SIZE_CACHE			(129024*KB)
#define PART_SIZE_USRDATA			(2097152*KB)
#define PART_SIZE_FAT			(0*KB)
#define PART_SIZE_BMTPOOL			(0)	/* (0xa8) */

#ifdef CONFIG_MTK_EMMC_SUPPORT
#define PART_NUM			25
#else
extern int get_part_num_nand(void);

#define PART_NUM			get_part_num_nand()
#endif

#ifndef RAND_START_ADDR
#define RAND_START_ADDR   1024
#endif

#define PART_MAX_COUNT			 40

#define MBR_START_ADDRESS_BYTE			(20480*KB)

#define WRITE_SIZE_Byte		512
typedef enum {
	EMMC = 1,
	NAND = 2,
} dev_type;

#ifdef CONFIG_MTK_EMMC_SUPPORT
typedef enum {
	EMMC_PART_UNKNOWN =
	    0, EMMC_PART_BOOT1, EMMC_PART_BOOT2, EMMC_PART_RPMB, EMMC_PART_GP1, EMMC_PART_GP2,
	    EMMC_PART_GP3, EMMC_PART_GP4, EMMC_PART_USER, EMMC_PART_END
} Region;
#else
typedef enum {
	NAND_PART_UNKNOWN = 0, NAND_PART_USER
} Region;
#endif
struct excel_info {
	char *name;
	unsigned long long size;
	unsigned long long start_address;
	dev_type type;
	unsigned int partition_idx;
	Region region;
};

#ifdef CONFIG_MTK_EMMC_SUPPORT
/*MBR or EBR struct*/
#define SLOT_PER_MBR 4
#define MBR_COUNT 8

struct MBR_EBR_struct {
	char part_name[8];
	int part_index[SLOT_PER_MBR];
};

extern struct MBR_EBR_struct MBR_EBR_px[MBR_COUNT];
#endif
extern struct excel_info PartInfo[PART_MAX_COUNT];


#endif
