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


#ifndef _PMT_H
#define _PMT_H
#include <mach/mtk_nand.h>
#include "mtk_nand_util.h"
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
#include "partition_define_tlc.h"
#else
#include "partition_define_mlc.h"
#endif

#define MAX_PARTITION_NAME_LEN 64

int new_part_tab(u8 *buf, struct mtd_info *mtd);
int update_part_tab(struct mtd_info *mtd);
static int read_pmt(void __user *arg);
typedef u32(*GetLowPageNumber) (u32 pageNo);
extern GetLowPageNumber functArray[];
extern bool g_bInitDone;
extern struct mtk_nand_host *host;
extern flashdev_info gn_devinfo;
extern bool g_bInitDone;

#define REGION_LOW_PAGE 0x004C4F57
#define REGION_FULL_PAGE 0x46554C4C
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
#define REGION_SLC_MODE 0x00534C43
#define REGION_TLC_MODE 0x00544C43
#endif

typedef struct {
	unsigned char name[MAX_PARTITION_NAME_LEN];	/* partition name */
	unsigned long long size;	/* partition size */
	unsigned long long part_id;                          /* partition region */
	unsigned long long offset;	/* partition start */
	unsigned long long mask_flags;	/* partition flags */
} pt_resident;


#define DM_ERR_OK 0
#define DM_ERR_NO_VALID_TABLE 9
#define DM_ERR_NO_SPACE_FOUND 10
#define ERR_NO_EXIST  1

#define PT_SIG      0x50547633	/*"PTv3" */
#define MPT_SIG    0x4D505433	/*"MPT3" */
#define PT_SIG_SIZE 8
#define is_valid_pt(buf) (!memcmp(buf, "3vTP", 4))
#define is_valid_mpt(buf) (!memcmp(buf, "3TPM", 4))
#define RETRY_TIMES 5

extern u32 slc_ratio;
extern u32 sys_slc_ratio;
extern u32 usr_slc_ratio;

typedef u32 (*GetLowPageNumber)(u32 pageNo);
extern GetLowPageNumber functArray[];

extern u32 system_block_count;

typedef struct _DM_PARTITION_INFO {
	char part_name[MAX_PARTITION_NAME_LEN];	/* the name of partition */
	unsigned long long start_addr;	/* the start address of partition */
	unsigned long long part_len;	/* the length of partition */
	unsigned char part_visibility;	/* part_visibility is 0: this partition is hidden and CANNOT download */
	/* part_visibility is 1: this partition is visible and can download */
	unsigned char dl_selected;	/* dl_selected is 0: this partition is NOT selected to download */
	/* dl_selected is 1: this partition is selected to download */
} DM_PARTITION_INFO;

typedef struct {
	unsigned int pattern;
	unsigned int part_num;	/* The actual number of partitions */
	DM_PARTITION_INFO part_info[PART_MAX_COUNT];
} DM_PARTITION_INFO_PACKET;

typedef struct {
	int sequencenumber:8;
	int tool_or_sd_update:8;
	int mirror_pt_dl:4;
	int mirror_pt_has_space:4;
	int pt_changed:4;
	int pt_has_space:4;
} pt_info;

#endif
