/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NANDX_PMT_H__
#define __NANDX_PMT_H__

#include "nandx_util.h"
#include "nandx_info.h"

#define MAX_PARTITION_NAME_LEN	64

#define REGION_LOW_PAGE		0x004C4F57
#define REGION_FULL_PAGE	0x46554C4C
#define REGION_SLC_MODE		0x00534C43
#define REGION_TLC_MODE		0x00544C43
#define PT_SIG			0x50547633	/* "PTv3" */
#define MPT_SIG			0x4D505433	/* "MPT3" */
#define PT_SIG_SIZE		4
#define PT_TLCRATIO_SIZE	4
#define is_valid_pt(buf)	(!memcmp(buf, "3vTP", 4))
#define is_valid_mpt(buf)	(!memcmp(buf, "3TPM", 4))

#define PMT_POOL_SIZE		(2)
#undef PART_MAX_COUNT
#define PART_MAX_COUNT		(40)

struct extension {
	unsigned int type;
	unsigned int attr;
};

struct pt_resident {
	char name[MAX_PARTITION_NAME_LEN];	/* partition name */
	unsigned long long size;	/* partition size */
	union {
		unsigned long long part_id;
		struct extension ext;
	};
	unsigned long long offset;	/* partition start */
	unsigned long long mask_flags;	/* partition flags */

};

/**
 * struct pmt_handler - partition table handler
 * @info: struct nandx_chip_info which contains nand device info
 * @pmt: partition table buffer
 * @block_bitmap: block type bitmap buffer for mntl
 * @start_blk: start block of partition region
 * @pmt_page: the latest pmt page number in pmt block
 * @part_num: partition number of current pmt
 * @sys_slc_ratio: slc ratio of system partition
 * @usr_slc_ratio: slc ratio of user partition
 */
struct pmt_handler {
	struct nandx_chip_info *info;
	struct pt_resident *pmt;
	u32 *block_bitmap;
	u32 start_blk;
	u32 pmt_page;
	u32 part_num;
	u32 sys_slc_ratio;
	u32 usr_slc_ratio;
};

int nandx_pmt_init(struct nandx_chip_info *info, u32 start_blk);
void nandx_pmt_exit(void);
int nandx_pmt_update(void);
struct pt_resident *nandx_pmt_get_partition(u64 addr);
u64 nandx_pmt_get_start_address(struct pt_resident *pt);
bool nandx_pmt_is_raw_partition(struct pt_resident *pt);
struct pmt_handler *nandx_get_pmt_handler(void);
int nandx_pmt_addr_to_row(u64 addr, u32 *block, u32 *page);
bool nandx_pmt_blk_is_slc(u64 addr);
#endif
