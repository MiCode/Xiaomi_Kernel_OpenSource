/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __WRAPPER_PMT_H__
#define __WRAPPER_PMT_H__

#include "mntl_ops.h"
#include "nandx_pmt.h"

enum DEV_TYPE {
	EMMC = 1,
	NAND = 2,
};

enum REGION {
	NAND_PART_UNKNOWN = 0,
	NAND_PART_USER,
};

struct excel_info {
	char *name;
	unsigned long long size;
	unsigned long long start_address;
	enum DEV_TYPE type;
	unsigned int partition_idx;
	enum REGION region;
};

struct DM_PARTITION_INFO {
	char part_name[MAX_PARTITION_NAME_LEN];
	unsigned long long start_addr;
	unsigned long long part_len;
	/*
	 * part_visibility 1: this partition is visible and can download
	 * part_visibility 0: this partition is hidden and CANNOT download
	 */
	unsigned char part_visibility;
	/*
	 * dl_selected 0: this partition is NOT selected to download
	 * dl_selected 1: this partition is selected to download
	 */
	unsigned char dl_selected;
};

struct DM_PARTITION_INFO_PACKET {
	unsigned int pattern;
	unsigned int part_num;
	struct DM_PARTITION_INFO part_info[PART_MAX_COUNT];
};

#ifdef CONFIG_DUM_CHAR_V2
extern struct excel_info PartInfo[];
#endif

int nandx_pmt_register(struct mtd_info *mtd);
void nandx_pmt_unregister(void);
int get_data_partition_info(struct nand_ftl_partition_info *info,
			    struct mtk_nand_chip_info *cinfo);
int mntl_update_part_tab(struct mtk_nand_chip_info *info, int num, u32 *blk);

#endif				/* __WRAPPER_PMT_H__ */
