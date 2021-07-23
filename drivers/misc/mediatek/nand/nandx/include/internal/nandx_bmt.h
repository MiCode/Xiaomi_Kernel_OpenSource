/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NANDX_BMT_H__
#define __NANDX_BMT_H__

#include "nandx_util.h"
#include "nandx_info.h"

#define DATA_BAD_BLK		0xffff

#define DATA_MAX_BMT_COUNT		0x400
#define DATA_BMT_VERSION		(1)

struct data_bmt_entry {
	u16 bad_index;		/* bad block index */
	u8 flag;		/* mapping block index in the replace pool */
};

struct data_bmt_struct {
	struct data_bmt_entry entry[DATA_MAX_BMT_COUNT];
	unsigned int version;
	unsigned int bad_count;	/* bad block count */
	unsigned int start_block;	/* data partition start block addr */
	unsigned int end_block;	/* data partition start block addr */
	unsigned int checksum;
};

enum UPDATE_REASON {
	UPDATE_ERASE_FAIL,
	UPDATE_WRITE_FAIL,
	UPDATE_UNMAPPED_BLOCK,
	UPDATE_REMOVE_ENTRY,
	UPDATE_INIT_WRITE,
	UPDATE_REASON_COUNT,
	INIT_BAD_BLK,
	FTL_MARK_BAD = 64,
};

int nandx_bmt_init(struct nandx_chip_info *dev_info, u32 block_num,
		   bool rebuild);
void nandx_bmt_reset(void);
void nandx_bmt_exit(void);
int nandx_bmt_remark(u32 *blocks, int count, enum UPDATE_REASON reason);
int nandx_bmt_block_isbad(u32 block);
int nandx_bmt_block_markbad(u32 block);
u32 nandx_bmt_update(u32 bad_block, enum UPDATE_REASON reason);
void nandx_bmt_update_oob(u32 block, u8 *oob);
u32 nandx_bmt_get_mapped_block(u32 block);
int nandx_bmt_get_data_bmt(struct data_bmt_struct *data_bmt);
int nandx_bmt_init_data_bmt(u32 start_block, u32 end_block);

#endif				/* #ifndef __NANDX_BMT_H__ */
