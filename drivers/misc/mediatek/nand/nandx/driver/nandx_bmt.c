/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include "nandx_util.h"
#include "nandx_errno.h"
#include "nandx_info.h"
#include "nandx_core.h"
#include "nandx_ops.h"
#include "nandx_bmt.h"

#define BMT_VERSION		(1)	/* initial version */
#define MAIN_BMT_SIZE		(0x200)
#define SIGNATURE_SIZE		(3)
#define MAIN_SIGNATURE_OFFSET	(0)
#define OOB_SIGNATURE_OFFSET	(1)
#define OOB_INDEX_OFFSET	(29)
#define OOB_INDEX_SIZE		(2)
#define FAKE_INDEX		(0xAAAA)

struct bmt_header {
	char signature[SIGNATURE_SIZE];
	u8 version;
	u8 bad_count;		/* bad block count in pool */
	u8 mapped_count;	/* mapped block count in pool */
	u8 checksum;
	u8 reseverd[13];
};

struct bmt_entry {
	u16 bad_index;		/* bad block index */
	u16 mapped_index;	/* mapping block index in the replace pool */
};

struct bmt_data_info {
	struct bmt_header header;
	struct bmt_entry table[MAIN_BMT_SIZE];
	struct data_bmt_struct data_bmt;
};

struct bmt_handler {
	struct nandx_chip_info *dev_info;
	u32 start_block;
	u32 block_count;
	u32 current_block;
	u8 *data;
	u8 *oob;
	struct bmt_data_info data_info;
};

static const char MAIN_SIGNATURE[] = "BMT";
static const char OOB_SIGNATURE[] = "bmt";

static struct bmt_handler *nandx_bmt;

static bool bmt_read_page(struct bmt_handler *bmt, u32 page)
{
	int ret;
	struct nandx_ops ops;

	nandx_get_device(FL_READING);

	ops.row = page;
	ops.col = 0;
	ops.len = bmt->dev_info->page_size;
	ops.data = bmt->data;
	ops.oob = bmt->oob;
	ret = nandx_core_read(&ops, 1, MODE_SLC);
	if (ret == -ENANDFLIPS)
		ret = 0;

	nandx_release_device();
	return ret < 0 ? false : true;
}

static bool bmt_write_page(struct bmt_handler *bmt, u32 page)
{
	int ret;
	struct nandx_ops ops;

	nandx_get_device(FL_WRITING);

	ops.row = page;
	ops.col = 0;
	ops.len = bmt->dev_info->page_size;
	ops.data = bmt->data;
	ops.oob = bmt->oob;
	ret = nandx_core_write(&ops, 1, MODE_SLC);

	nandx_release_device();
	return ret < 0 ? false : true;
}

static bool bmt_erase_block(struct bmt_handler *bmt, u32 block)
{
	int ret;
	u32 row;

	nandx_get_device(FL_ERASING);

	row = block * (bmt->dev_info->block_size / bmt->dev_info->page_size);
	ret = nandx_core_erase(&row, 1, MODE_SLC);

	nandx_release_device();
	return ret < 0 ? false : true;
}

static bool bmt_page_is_empty(struct bmt_handler *bmt, u8 *page)
{
	int i;

	for (i = 0; i < bmt->dev_info->page_size; i++) {
		if (page[i] != 0xff)
			return false;
	}

	return true;
}

static bool bmt_block_isbad(struct bmt_handler *bmt, u32 block)
{
	bool ret;
	u32 row;
	bool empty_page = true;

	row = block * (bmt->dev_info->block_size / bmt->dev_info->page_size);

	memset(bmt->data, 0, bmt->dev_info->page_size);
	memset(bmt->oob, 0, bmt->dev_info->oob_size);

	ret = bmt_read_page(bmt, row);
	if (ret)
		empty_page = bmt_page_is_empty(bmt, bmt->data);

	if (!empty_page && bmt->oob[0] == 0xff)
		return false;

	pr_info("Check bmt block vendor mark %u\n", block);

	nandx_get_device(FL_READING);
	ret = nandx_core_is_bad(row);
	nandx_release_device();

	return ret;
}

static bool bmt_mark_block_bad(struct bmt_handler *bmt, u32 block)
{
	int ret, i, num;
	struct nandx_ops *ops;
	u32 row;

	nandx_get_device(FL_WRITING);

	num = bmt->dev_info->wl_page_num * bmt->dev_info->wl_page_num;
	row = block * (bmt->dev_info->block_size / bmt->dev_info->page_size);

	ops = mem_alloc(num, sizeof(struct nandx_ops));
	if (!ops) {
		nandx_release_device();
		return -ENOMEM;
	}

	memset(ops, 0, num * sizeof(struct nandx_ops));
	memset(bmt->data, 0, bmt->dev_info->page_size);
	memset(bmt->oob, 0, bmt->dev_info->oob_size);

	for (i = 0; i < num; i++) {
		ops[i].row = row + i;
		ops[i].col = 0;
		ops[i].len = bmt->dev_info->page_size;
		ops[i].data = bmt->data;
		ops[i].oob = bmt->oob;
	}

	ret = nandx_core_write(ops, num, MODE_SLC);
	mem_free(ops);

	nandx_release_device();

	return ret < 0 ? false : true;
}

int nandx_bmt_block_isbad(u32 block)
{
	if (bmt_block_isbad(nandx_bmt, block))
		return -ENANDBAD;

	return 0;
}

int nandx_bmt_block_markbad(u32 block)
{
	if (bmt_mark_block_bad(nandx_bmt, block))
		return -ENANDWRITE;

	return 0;
}

static void bmt_dump_info(struct bmt_data_info *data_info)
{
	int i;

	pr_info("BMT v%d. total %d mapping:\n",
		data_info->header.version, data_info->header.mapped_count);
	for (i = 0; i < data_info->header.mapped_count; i++) {
		pr_info("block[%d] map to block[%d]\n",
			data_info->table[i].bad_index,
			data_info->table[i].mapped_index);
	}
}

static bool bmt_match_signature(struct bmt_handler *bmt)
{

	return memcmp(bmt->data + MAIN_SIGNATURE_OFFSET, MAIN_SIGNATURE,
		      SIGNATURE_SIZE) ? false : true;
}

static u8 bmt_calculate_checksum(struct bmt_handler *bmt)
{
	struct bmt_data_info *data_info = &bmt->data_info;
	u32 i, size = bmt->block_count * sizeof(struct bmt_entry);
	u8 checksum = 0;
	u8 *data = (u8 *)data_info->table;

	checksum += data_info->header.version;
	checksum += data_info->header.mapped_count;

	for (i = 0; i < size; i++)
		checksum += data[i];

	return checksum;
}

static bool bmt_block_is_mapped(struct bmt_data_info *data_info, u32 index)
{
	int i;

	for (i = 0; i < data_info->header.mapped_count; i++) {
		/* bmt block mapped to another */
		if (index == data_info->table[i].mapped_index)
			return true;
	}

	return false;
}

static bool bmt_data_is_valid(struct bmt_handler *bmt)
{
	u8 checksum;
	struct bmt_data_info *data_info = &bmt->data_info;

	checksum = bmt_calculate_checksum(bmt);

	pr_debug("BMT Checksum is: 0x%x\n", data_info->header.checksum);
	/* checksum correct? */
	if (data_info->header.checksum != checksum) {
		pr_err("BMT Data checksum error: %x %x\n",
		       data_info->header.checksum, checksum);
		return false;
	}

	/* pass check, valid bmt. */
	pr_debug("Valid BMT, version v%d\n", data_info->header.version);

	return true;
}

static void bmt_fill_buffer(struct bmt_handler *bmt)
{
	struct bmt_data_info *data_info = &bmt->data_info;

	bmt_dump_info(&bmt->data_info);

	memcpy(data_info->header.signature, MAIN_SIGNATURE, SIGNATURE_SIZE);
	data_info->header.version = BMT_VERSION;

	data_info->header.checksum = bmt_calculate_checksum(bmt);

	memcpy(bmt->data + MAIN_SIGNATURE_OFFSET, data_info,
	       sizeof(struct bmt_data_info));
	memcpy(bmt->oob + OOB_SIGNATURE_OFFSET, OOB_SIGNATURE,
	       SIGNATURE_SIZE);
}

static int bmt_load_data(struct bmt_handler *bmt)
{
	u32 row;
	struct nandx_chip_info *info = bmt->dev_info;
	u32 bmt_index = bmt->start_block + bmt->block_count - 1;

	for (; bmt_index >= bmt->start_block; bmt_index--) {
		if (bmt_block_isbad(bmt, bmt_index)) {
			pr_err("Skip bad block: %d\n", bmt_index);
			continue;
		}

		row = bmt_index * (info->block_size / info->page_size);
		if (!bmt_read_page(bmt, row)) {
			pr_err("read block %d error\n", bmt_index);
			continue;
		}

		memcpy(&bmt->data_info, bmt->data + MAIN_SIGNATURE_OFFSET,
		       sizeof(struct bmt_data_info));

		if (!bmt_match_signature(bmt)) {
			pr_err("signature not match at %u\n", bmt_index);
			continue;
		}

		pr_info("Match bmt signature @ block 0x%x\n", bmt_index);

		if (!bmt_data_is_valid(bmt)) {
			pr_err("BMT data not correct %d\n", bmt_index);
			continue;
		}

		bmt_dump_info(&bmt->data_info);
		return bmt_index;
	}

	pr_err("Bmt block not found!\n");

	return 0;
}

static int find_available_block(struct bmt_handler *bmt, bool start_from_end)
{
	u32 i, block;
	int direct, retry;

	pr_debug("Try to find available block\n");

	block = start_from_end ? (bmt->dev_info->block_num - 1) :
	    bmt->start_block;
	direct = start_from_end ? -1 : 1;
	retry = start_from_end ? 1 : 0;

	for (i = 0; i < bmt->block_count; i++, block += direct) {
		if (block == bmt->current_block) {
			pr_debug("Skip bmt block 0x%x\n", block);
			continue;
		}

		if (bmt_block_isbad(bmt, block)) {
			pr_debug("Skip bad block 0x%x\n", block);
			continue;
		}

		if (bmt_block_is_mapped(&bmt->data_info, block)) {
			pr_debug("Skip mapped block 0x%x\n", block);
			continue;
		}

		pr_debug("Find block 0x%x available\n", block);

		if (retry) {
			/* fix block is good, but erase fail */
			if (bmt_erase_block(bmt, block)) {
				pr_info
				    ("Reserved 0x%x for DL_INFO, find next\n",
				     block);
				retry -= 1;
			}
			pr_info("Erase block %u fail, retry\n", block);
			continue;
		}

		return block;
	}

	return 0;
}

static u16 get_bad_index_from_oob(struct bmt_handler *bmt, u8 *oob)
{
	u16 index;
	u32 offset = OOB_INDEX_OFFSET;

	if (KB(2) == bmt->dev_info->page_size)
		offset = 13;

	memcpy(&index, oob + offset, OOB_INDEX_SIZE);

	return index;
}

static void set_bad_index_to_oob(struct bmt_handler *bmt, u8 *oob, u16 index)
{
	u32 offset = OOB_INDEX_OFFSET;

	if (KB(2) == bmt->dev_info->page_size)
		offset = 13;

	memcpy(oob + offset, &index, sizeof(index));
}

static int bmt_write_to_flash(struct bmt_handler *bmt)
{
	bool ret;
	u32 page, page_size, block_size;

	page_size = bmt->dev_info->page_size;
	block_size = bmt->dev_info->block_size;

	pr_debug("Try to write BMT\n");

	while (1) {
		if (bmt->current_block == 0) {
			bmt->current_block = find_available_block(bmt, true);
			if (!bmt->current_block) {
				pr_err("no valid block\n");
				return -ENOSPC;
			}
		}

		pr_info("Find BMT block: 0x%x\n", bmt->current_block);

		/* write bmt to flash */
		ret = bmt_erase_block(bmt, bmt->current_block);
		if (ret) {
			/* fill NAND BMT buffer */
			memset(bmt->oob, 0xFF, bmt->dev_info->oob_size);
			bmt_fill_buffer(bmt);
			page = bmt->current_block * (block_size / page_size);
			ret = bmt_write_page(bmt, page);
			if (ret)
				break;
			pr_err("write failed 0x%x\n", bmt->current_block);
		}

		pr_err("erase fail, mark bad 0x%x\n", bmt->current_block);
		bmt_mark_block_bad(bmt, bmt->current_block);

		bmt->current_block = 0;
	}

	pr_debug("Write BMT data to block 0x%x success\n",
		 bmt->current_block);

	return 0;
}

static int bmt_construct(struct bmt_handler *bmt)
{
	u32 index, bad_index, row, count = 0;
	struct nandx_chip_info *dev_info = bmt->dev_info;
	struct bmt_data_info *data_info = &bmt->data_info;
	u32 end_block = bmt->start_block + bmt->block_count;
	u32 pages_per_block = dev_info->block_size / dev_info->page_size;

	/* init everything in BMT struct */
	data_info->header.version = BMT_VERSION;
	data_info->header.bad_count = 0;
	data_info->header.mapped_count = 0;

	memset(data_info->table, 0,
	       bmt->block_count * sizeof(struct bmt_entry));

	/* TODO: cannot scan ftl partition for construct */
	for (index = bmt->start_block; index < end_block; index++) {
		if (bmt_block_isbad(bmt, index)) {
			pr_err("Skip bad block: 0x%x\n", index);
			continue;
		}
		row = index * pages_per_block;
		if (bmt_read_page(bmt, row)) {
			bad_index = get_bad_index_from_oob(bmt, bmt->oob);
			if (bad_index >= bmt->start_block) {
				pr_err("get bad index: 0x%x\n", bad_index);
				if (bad_index != 0xFFFF)
					pr_err("Invalid index\n");
				continue;
			}
			pr_debug("0x%x mapped to bad block: 0x%x\n",
				 index, bad_index);
			/* add mapping to BMT */
			data_info->table[count].bad_index = bad_index;
			data_info->table[count].mapped_index = index;
			count++;
		}
	}

	data_info->header.mapped_count = count;
	pr_debug("Scan replace pool done, mapped block: %d\n",
		 data_info->header.mapped_count);

	/* write BMT back */
	return bmt_write_to_flash(bmt);
}

int nandx_bmt_init(struct nandx_chip_info *dev_info, u32 block_num,
		   bool rebuild)
{
	if (block_num > MAIN_BMT_SIZE) {
		pr_err("bmt size %d over %d\n", block_num, MAIN_BMT_SIZE);
		return -ENOMEM;
	}

	nandx_bmt = mem_alloc(1, sizeof(struct bmt_handler));
	if (!nandx_bmt)
		return -ENOMEM;

	nandx_bmt->dev_info = dev_info;
	nandx_bmt->data = mem_alloc(1, dev_info->page_size);
	nandx_bmt->oob = mem_alloc(1, dev_info->oob_size);
	nandx_bmt->start_block = dev_info->block_num - block_num;
	nandx_bmt->block_count = block_num;

	pr_info("bmt start block: %d, bmt block count: %d\n",
		nandx_bmt->start_block, nandx_bmt->block_count);

	memset(nandx_bmt->data_info.table, 0,
	       block_num * sizeof(struct bmt_entry));

	nandx_bmt->current_block = bmt_load_data(nandx_bmt);
	if (!nandx_bmt->current_block) {
		if (rebuild) {
			pr_info("Not found bmt, do bmt construct...\n");
			return bmt_construct(nandx_bmt);
		}
		pr_info("Not found bmt, need construct data bmt info\n");
		return 1;
	}

	return 0;
}

void nandx_bmt_reset(void)
{
	nandx_bmt->current_block = 0;
	memset(&nandx_bmt->data_info, 0, sizeof(struct bmt_data_info));
}

void nandx_bmt_exit(void)
{
	mem_free(nandx_bmt->data);
	mem_free(nandx_bmt->oob);
	mem_free(nandx_bmt);
}

static u32 nandx_bmt_update_block(struct bmt_handler *bmt, u32 bad_block,
				  enum UPDATE_REASON reason)
{
	u32 i, map_index = 0, map_count = 0;
	struct bmt_entry *entry;
	struct bmt_data_info *data_info = &bmt->data_info;
	struct data_bmt_struct *info = &(bmt->data_info.data_bmt);

	if (bad_block >= info->start_block && bad_block < info->end_block) {
		if (nandx_bmt_get_mapped_block(bad_block) != DATA_BAD_BLK) {
			pr_debug("update_DATA bad block %d\n", bad_block);
			info->entry[info->bad_count].bad_index = bad_block;
			info->entry[info->bad_count].flag = reason;
			info->bad_count++;
		} else {
			pr_err("block(%d) in bad list\n", bad_block);
			return bad_block;
		}
	} else {
		map_index = find_available_block(bmt, false);
		if (!map_index) {
			pr_err("no good bmt block to map\n");
			return bad_block;
		}

		/* now let's update BMT */
		if (bad_block >= bmt->start_block) {
			/* mapped block become bad, find original bad block */
			for (i = 0; i < bmt->block_count; i++) {
				entry = &data_info->table[i];
				if (entry->mapped_index == bad_block) {
					pr_debug("block %d is bad\n",
						 entry->bad_index);
					entry->mapped_index = map_index;
					break;
				}
			}
		} else {
			map_count = data_info->header.mapped_count;
			entry = &data_info->table[map_count];
			entry->mapped_index = map_index;
			entry->bad_index = bad_block;
			data_info->header.mapped_count++;
		}
	}
	return map_index;
}

u32 nandx_bmt_update(u32 bad_block, enum UPDATE_REASON reason)
{
	u32 map_index;

	map_index = nandx_bmt_update_block(nandx_bmt, bad_block, reason);

	if (bmt_write_to_flash(nandx_bmt)) {
		NANDX_ASSERT(0);
		return bad_block;
	}

	return map_index;
}

int nandx_bmt_remark(u32 *blocks, int count, enum UPDATE_REASON reason)
{
	int i;

	for (i = 0; i < count; i++) {
		if (blocks[i] >= nandx_bmt->start_block) {
			pr_info("skip remark bmt bad %u\n", blocks[i]);
			continue;
		}

		pr_info("remark bad block %u\n", blocks[i]);
		nandx_bmt_update_block(nandx_bmt, blocks[i], reason);
	}

	return bmt_write_to_flash(nandx_bmt);
}

void nandx_bmt_update_oob(u32 block, u8 *oob)
{
	u16 index;
	u32 mapped_block;

	mapped_block = nandx_bmt_get_mapped_block(block);
	index = mapped_block == block ? FAKE_INDEX : block;

	set_bad_index_to_oob(nandx_bmt, oob, index);
}

u32 nandx_bmt_get_mapped_block(u32 block)
{
	int i;
	struct bmt_data_info *data_info = &nandx_bmt->data_info;
	struct data_bmt_struct *data_bmt_info =
	    &(nandx_bmt->data_info.data_bmt);

	if ((block >= data_bmt_info->start_block) &&
	    (block < data_bmt_info->end_block)) {
		for (i = 0; i < data_bmt_info->bad_count; i++) {
			if (data_bmt_info->entry[i].bad_index == block) {
				pr_debug
				    ("FTL bad block at 0x%x, bad_count:%d\n",
				     block, data_bmt_info->bad_count);
				return DATA_BAD_BLK;
			}
		}
	} else {
		for (i = 0; i < data_info->header.mapped_count; i++) {
			if (data_info->table[i].bad_index == block)
				return data_info->table[i].mapped_index;
		}
	}

	return block;
}

int nandx_bmt_get_data_bmt(struct data_bmt_struct *data_bmt)
{
	struct data_bmt_struct *data_bmt_info =
	    &(nandx_bmt->data_info.data_bmt);

	if (data_bmt_info->version == DATA_BMT_VERSION) {
		memcpy(data_bmt, data_bmt_info,
		       sizeof(struct data_bmt_struct));
		return 0;
	}

	return 1;
}

int nandx_bmt_init_data_bmt(u32 start_block, u32 end_block)
{
	struct data_bmt_struct *data_bmt = &nandx_bmt->data_info.data_bmt;

	data_bmt->start_block = start_block;
	data_bmt->end_block = end_block;
	data_bmt->version = DATA_BMT_VERSION;
	data_bmt->bad_count = 0;

	return 0;
}
