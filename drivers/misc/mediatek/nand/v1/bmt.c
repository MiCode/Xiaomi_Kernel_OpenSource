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

#include <mach/mtk_nand.h>
#include "mtk_nand_util.h"
#include "bmt.h"
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/div64.h>

static const char MAIN_SIGNATURE[] = "BMT";
static const char OOB_SIGNATURE[] = "bmt";
#define SIGNATURE_SIZE	(3)

#define MAX_DAT_SIZE		0x4000
#define MAX_OOB_SIZE		0x800

static struct mtd_info *mtd_bmt;
static struct nand_chip *nand_chip_bmt;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
#define BLOCK_SIZE_BMT		(gn_devinfo.blocksize * 1024)
#define PAGE_PER_SIZE_BMT	((gn_devinfo.blocksize * 1024) / gn_devinfo.pagesize)
#define PAGE_ADDR(block)	((block) * PAGE_PER_SIZE_BMT)
#else
#define BLOCK_SIZE_BMT          (1 << nand_chip_bmt->phys_erase_shift)
#define PAGE_PER_SIZE_BMT       (1 << (nand_chip_bmt->phys_erase_shift-nand_chip_bmt->page_shift))
#define OFFSET(block)       (((u64)block) * BLOCK_SIZE_BMT)
#define PAGE_ADDR(block)    ((block) * PAGE_PER_SIZE_BMT)
#endif
#define PAGE_SIZE_BMT		(1 << nand_chip_bmt->page_shift)

struct phys_bmt_header {
	char signature[3];
	u8 version;
	u8 bad_count;		/* bad block count in pool */
	u8 mapped_count;	/* mapped block count in pool */
	u8 checksum;
	u8 reseverd[13];
};

struct phys_bmt_struct {
	struct phys_bmt_header header;
	bmt_entry table[MAX_BMT_SIZE];
};

struct bmt_oob_data {
	char signature[3];
};

/*********************************************************************
* Flash is splited into 2 parts, system part is for normal system	*
* system usage, size is system_block_count, another is replace pool *
*	+-------------------------------------------------+			*
*	|	system_block_count	| bmt_block_count |			*
*	+-------------------------------------------------+			*
*********************************************************************/
static u32 total_block_count;	/* block number in flash */
u32 system_block_count;
static int bmt_block_count;	/* bmt table size */
/* static int bmt_count;			block used in bmt */
static int page_per_block;	/* page per count */

static u32 bmt_block_index;	/* bmt block index */
static bmt_struct bmt;		/* dynamic created global bmt table */


static u8 dat_buf[MAX_DAT_SIZE];
static u8 oob_buf[MAX_OOB_SIZE];
static bool pool_erased;

/***************************************************************
*
* Interface adaptor for preloader/uboot/kernel
*	These interfaces operate on physical address, read/write
*	physical data.
*
***************************************************************/
int nand_read_page_bmt(u32 page, u8 *dat, u8 *oob)
{
	return mtk_nand_exec_read_page(mtd_bmt, page, PAGE_SIZE_BMT, dat, oob);
}

bool nand_block_bad_bmt(u64 offset)
{
	return mtk_nand_block_bad_hw(mtd_bmt, offset);
}

bool nand_erase_bmt(u64 offset)
{
	int status;

	if (offset < 0x20000)
		MSG(INIT, "erase offset: 0x%llx\n", offset);


	status = mtk_nand_erase_hw(mtd_bmt, (u32)(offset >> nand_chip_bmt->page_shift));
	/* as nand_chip structure doesn't have a erase function defined */
	if (status & NAND_STATUS_FAIL)
		return false;
	else
		return true;
}

int mark_block_bad_bmt(u64 offset)
{
	return mtk_nand_block_markbad_hw(mtd_bmt, offset);	/* mark_block_bad_hw(offset); */
}

bool nand_write_page_bmt(u32 page, u8 *dat, u8 *oob)
{
	if (mtk_nand_exec_write_page(mtd_bmt, page, PAGE_SIZE_BMT, dat, oob))
		return false;
	else
		return true;
}

/***************************************************************
*
* static internal function
*
***************************************************************/
static void dump_bmt_info(bmt_struct *bmt)
{
	int i;

	MSG(INIT, "BMT v%d. total %d mapping:\n", bmt->version, bmt->mapped_count);
	for (i = 0; i < bmt->mapped_count; i++)
		MSG(INIT, "\t0x%x -> 0x%x\n", bmt->table[i].bad_index, bmt->table[i].mapped_index);

}

static bool match_bmt_signature(u8 *dat, u8 *oob)
{

	if (memcmp(dat + MAIN_SIGNATURE_OFFSET, MAIN_SIGNATURE, SIGNATURE_SIZE))
		return false;

	if (memcmp(oob + OOB_SIGNATURE_OFFSET, OOB_SIGNATURE, SIGNATURE_SIZE))
		MSG(INIT, "main signature match, oob signature doesn't match, but ignore\n");

	return true;
}

static u8 cal_bmt_checksum(struct phys_bmt_struct *phys_table, int bmt_size)
{
	int i;
	u8 checksum = 0;
	u8 *dat = (u8 *) phys_table;

	checksum += phys_table->header.version;
	checksum += phys_table->header.mapped_count;

	dat += sizeof(struct phys_bmt_header);
	for (i = 0; i < bmt_size * sizeof(bmt_entry); i++)
		checksum += dat[i];

	return checksum;
}


static int is_block_mapped(int index)
{
	int i;

	for (i = 0; i < bmt.mapped_count; i++) {
		if (index == bmt.table[i].mapped_index)
			return i;
	}
	return -1;
}

static bool is_page_used(u8 *dat, u8 *oob)
{
	if (2048 == PAGE_SIZE_BMT)
		return ((oob[13] != 0xFF) || (oob[14] != 0xFF));

	return ((oob[OOB_INDEX_OFFSET] != 0xFF) || (oob[OOB_INDEX_OFFSET + 1] != 0xFF));
}

static bool valid_bmt_data(struct phys_bmt_struct *phys_table)
{
	int i;
	u8 checksum = cal_bmt_checksum(phys_table, bmt_block_count);

	/* checksum correct? */
	if (phys_table->header.checksum != checksum) {
		MSG(INIT, "BMT Data checksum error: %x %x\n", phys_table->header.checksum,
			checksum);
		return false;
	}

	MSG(INIT, "BMT Checksum is: 0x%x\n", phys_table->header.checksum);

	/* block index correct? */
	for (i = 0; i < phys_table->header.mapped_count; i++) {
		if (phys_table->table[i].bad_index >= total_block_count
			|| phys_table->table[i].mapped_index >= total_block_count
			|| phys_table->table[i].mapped_index < system_block_count) {
			MSG(INIT, "index error: bad_index: %d, mapped_index: %d\n",
				phys_table->table[i].bad_index, phys_table->table[i].mapped_index);
			return false;
		}
	}

	/* pass check, valid bmt. */
	MSG(INIT, "Valid BMT, version v%d\n", phys_table->header.version);
	return true;
}

static void fill_nand_bmt_buffer(bmt_struct *bmt, u8 *dat, u8 *oob)
{
	struct phys_bmt_struct *phys_bmt;

	phys_bmt = kmalloc(sizeof(struct phys_bmt_struct), GFP_KERNEL);

	dump_bmt_info(bmt);

	/* fill struct phys_bmt_struct structure with bmt_struct */
	memset(phys_bmt, 0xFF, sizeof(struct phys_bmt_struct));

	memcpy(phys_bmt->header.signature, MAIN_SIGNATURE, SIGNATURE_SIZE);
	phys_bmt->header.version = BMT_VERSION;
	/* phys_bmt.header.bad_count = bmt->bad_count; */
	phys_bmt->header.mapped_count = bmt->mapped_count;
	memcpy(phys_bmt->table, bmt->table, sizeof(bmt_entry) * bmt_block_count);

	phys_bmt->header.checksum = cal_bmt_checksum(phys_bmt, bmt_block_count);

	memcpy(dat + MAIN_SIGNATURE_OFFSET, phys_bmt, sizeof(struct phys_bmt_struct));
	memcpy(oob + OOB_SIGNATURE_OFFSET, OOB_SIGNATURE, SIGNATURE_SIZE);
	kfree(phys_bmt);
}

/* return valid index if found BMT, else return 0 */
static int load_bmt_data(int start, int pool_size)
{
	int bmt_index = start + pool_size - 1;	/* find from the end */
	struct phys_bmt_struct *phys_table;

	phys_table = kmalloc(sizeof(struct phys_bmt_struct), GFP_KERNEL);

	MSG(INIT, "[%s]: begin to search BMT from block 0x%x\n", __func__, bmt_index);

	for (bmt_index = start + pool_size - 1; bmt_index >= start; bmt_index--) {
		if (nand_block_bad_bmt(OFFSET(bmt_index))) {
			MSG(INIT, "Skip bad block: %d\n", bmt_index);
			continue;
		}

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.tlcControl.normaltlc))
			gn_devinfo.tlcControl.slcopmodeEn = true;
#endif
		pr_warn("[load_bmt_data] datbuf 0x%lx oobbuf 0x%lx\n", (unsigned long)dat_buf, (unsigned long)oob_buf);
		if (!nand_read_page_bmt(PAGE_ADDR(bmt_index), dat_buf, oob_buf)) {
			pr_debug("Error found when read block %d\n", bmt_index);
			continue;
		}

		if (!match_bmt_signature(dat_buf, oob_buf))
			continue;

		pr_debug("Match bmt signature @ block: %d, %d\n", bmt_index, PAGE_ADDR(bmt_index));

		memcpy(phys_table, dat_buf + MAIN_SIGNATURE_OFFSET, sizeof(struct phys_bmt_struct));

		if (!valid_bmt_data(phys_table)) {
			MSG(INIT, "BMT data is not correct %d\n", bmt_index);
			continue;
		} else {
			bmt.mapped_count = phys_table->header.mapped_count;
			bmt.version = phys_table->header.version;
			/* bmt.bad_count = phys_table.header.bad_count; */
			memcpy(bmt.table, phys_table->table, bmt.mapped_count * sizeof(bmt_entry));

			MSG(INIT, "bmt found at block: %d, mapped block: %d\n", bmt_index,
				bmt.mapped_count);
			kfree(phys_table);
			return bmt_index;
		}
	}

	MSG(INIT, "bmt block not found!\n");
	kfree(phys_table);
	return 0;
}

/*************************************************************************
* Find an available block and erase.									*
* start_from_end: if true, find available block from end of flash.	*
*				else, find from the beginning of the pool			*
* need_erase: if true, all unmapped blocks in the pool will be erased	*
*************************************************************************/
static int find_available_block(bool start_from_end)
{
	int i;			/* , j; */
	int block = system_block_count;
	int direction;
	/* int avail_index = 0; */
	MSG(INIT, "Try to find_available_block, pool_erase: %d\n", pool_erased);


#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.tlcControl.normaltlc))
		gn_devinfo.tlcControl.slcopmodeEn = true;
#endif


	if (start_from_end) {
		block = total_block_count - 1;
		direction = -1;
	} else {
		block = system_block_count;
		direction = 1;
	}

	for (i = 0; i < bmt_block_count; i++, block += direction) {
		if (block == bmt_block_index) {
			MSG(INIT, "Skip bmt block 0x%x\n", block);
			continue;
		}

		if (nand_block_bad_bmt(OFFSET(block))) {
			MSG(INIT, "Skip bad block 0x%x\n", block);
			continue;
		}

		if (is_block_mapped(block) >= 0) {
			MSG(INIT, "Skip mapped block 0x%x\n", block);
			continue;
		}

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (!nand_erase_bmt(((u64)block) * (gn_devinfo.blocksize * 1024))) {
			MSG(INIT, "Erase block 0x%x failed\n", block);
			mark_block_bad_bmt(((u64)block) * (gn_devinfo.blocksize * 1024));
			continue;
		}
#endif

		MSG(INIT, "Find block 0x%x available\n", block);
		return block;
	}

	return 0;
}

static unsigned short get_bad_index_from_oob(u8 *oob_buf)
{
	unsigned short index;
	pr_err("OOB_INDEX_OFFSET %d\n", OOB_INDEX_OFFSET);
	if (2048 == PAGE_SIZE_BMT)
		memcpy(&index, oob_buf + 13, OOB_INDEX_SIZE);
	else
		memcpy(&index, oob_buf + OOB_INDEX_OFFSET, OOB_INDEX_SIZE);

	return index;
}

void set_bad_index_to_oob(u8 *oob, u16 index)
{
	if (2048 == PAGE_SIZE_BMT)
		memcpy(oob + 13, &index, sizeof(index));
	else
		memcpy(oob + OOB_INDEX_OFFSET, &index, sizeof(index));
}

static int migrate_from_bad(u64 offset, u8 *write_dat, u8 *write_oob)
{
	int page;
	u64 temp;
	u32 error_block;
	u32 error_page = (u32)(offset >> nand_chip_bmt->page_shift) % page_per_block;
	u32 orig_block;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	u32 idx;
	bool tlc_mode_block = false;
	int bRet;
#endif
	int to_index;
	u32 tick = 1;

	temp = offset;
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	error_block = (u32) temp;
	orig_block = error_block;

	memcpy(oob_buf, write_oob, MAX_OOB_SIZE);

	to_index = find_available_block(false);

	if (!to_index) {
		MSG(INIT, "Cannot find an available block for BMT\n");
		return 0;
	}

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.tlcControl.normaltlc)) {
		if (error_block >= system_block_count) {
			for (idx = 0; idx < bmt_block_count; idx++) {
				if (bmt.table[idx].mapped_index == error_block) {
					orig_block = bmt.table[idx].bad_index;
					break;
				}
			}
		}
		temp = (u64)orig_block & 0xFFFFFFFF;
		tlc_mode_block = mtk_block_istlc(temp * (gn_devinfo.blocksize * 1024));
		if (!tlc_mode_block) {
			gn_devinfo.tlcControl.slcopmodeEn = true; /*slc mode*/
			tick = 3;
		} else
			gn_devinfo.tlcControl.slcopmodeEn = false;
	}
#endif

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.tlcControl.normaltlc) && tlc_mode_block) {
		if (error_block >= system_block_count)
			set_bad_index_to_oob(oob_buf, orig_block);
		else
			set_bad_index_to_oob(oob_buf, error_block);
		memcpy(nand_chip_bmt->oob_poi, oob_buf, mtd_bmt->oobsize);
		nand_erase_bmt(((u64)to_index) * (gn_devinfo.blocksize * 1024));
		bRet = mtk_nand_write_tlc_block_hw(mtd_bmt, nand_chip_bmt,
			write_dat, to_index, 0, (gn_devinfo.blocksize * 1024));
		if (bRet != 0) {
			MSG(INIT, "Write to page 0x%x fail\n", PAGE_ADDR(to_index) + error_page);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			mark_block_bad_bmt(((u64)to_index) * (gn_devinfo.blocksize * 1024));
#else
			mark_block_bad_bmt(OFFSET(to_index));
#endif
			return migrate_from_bad(offset, write_dat, write_oob);
		}
	} else
#endif
	{
	{			/* migrate error page first */
		MSG(INIT, "Write error page: 0x%x\n", error_page);
		if (!write_dat) {
			nand_read_page_bmt(PAGE_ADDR(error_block) + error_page, dat_buf, NULL);
			write_dat = dat_buf;
		}
		/* memcpy(oob_buf, write_oob, MAX_OOB_SIZE); */

		if (error_block < system_block_count)
			set_bad_index_to_oob(oob_buf, error_block);
			/* if error_block is already a mapped block,
			original mapping index is in OOB. */

		if (!nand_write_page_bmt(PAGE_ADDR(to_index) + error_page, write_dat, oob_buf)) {
			MSG(INIT, "Write to page 0x%x fail\n", PAGE_ADDR(to_index) + error_page);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
				mark_block_bad_bmt(((u64)to_index) * (gn_devinfo.blocksize * 1024));
#else
			mark_block_bad_bmt(OFFSET(to_index));
#endif
			return migrate_from_bad(offset, write_dat, write_oob);
		}
	}

		for (page = 0; page < page_per_block; (page += tick)) {
			if (page != error_page) {
				nand_read_page_bmt(PAGE_ADDR(error_block) + page, dat_buf, oob_buf);
				if (is_page_used(dat_buf, oob_buf)) {
					if (error_block < system_block_count)
						set_bad_index_to_oob(oob_buf, error_block);
					MSG(INIT, "\tmigrate page 0x%x to page 0x%x\n",
					PAGE_ADDR(error_block) + page, PAGE_ADDR(to_index) + page);
				if (!nand_write_page_bmt
					(PAGE_ADDR(to_index) + page, dat_buf, oob_buf)) {
					MSG(INIT, "Write to page 0x%x fail\n",
						PAGE_ADDR(to_index) + page);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
					mark_block_bad_bmt(((u64)to_index) * (gn_devinfo.blocksize * 1024));
#else
					mark_block_bad_bmt(OFFSET(to_index));
#endif
					return migrate_from_bad(offset, write_dat, write_oob);
				}
			}
		}
	}
	}

	MSG(INIT, "Migrate from 0x%x to 0x%x done!\n", error_block, to_index);

	return to_index;
}

static bool write_bmt_to_flash(u8 *dat, u8 *oob)
{
	bool need_erase = true;

	MSG(INIT, "Try to write BMT\n");

	if (bmt_block_index == 0) {
		need_erase = false;
		bmt_block_index = find_available_block(true);
		if (!bmt_block_index) {
			MSG(INIT, "Cannot find an available block for BMT\n");
			return false;
		}
	}

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
	&& (gn_devinfo.tlcControl.normaltlc))
		gn_devinfo.tlcControl.slcopmodeEn = true; /*change to slc mode*/
#endif

	MSG(INIT, "Find BMT block: 0x%x\n", bmt_block_index);

	/* write bmt to flash */
	if (need_erase) {
		if (!nand_erase_bmt(((u64)bmt_block_index) * (gn_devinfo.blocksize * 1024))) {
			MSG(INIT, "BMT block erase fail, mark bad: 0x%x\n", bmt_block_index);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			mark_block_bad_bmt(((u64)bmt_block_index) * (gn_devinfo.blocksize * 1024));
#else
			mark_block_bad_bmt(OFFSET(bmt_block_index));
#endif

			bmt_block_index = 0;
			return write_bmt_to_flash(dat, oob);	/* recursive call */
		}
	}

	if (!nand_write_page_bmt(PAGE_ADDR(bmt_block_index), dat, oob)) {
		MSG(INIT, "Write BMT data fail, need to write again\n");
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		mark_block_bad_bmt(((u64)bmt_block_index) * (gn_devinfo.blocksize * 1024));
#else
		mark_block_bad_bmt(OFFSET(bmt_block_index));
#endif
		/* bmt.bad_count++; */

		bmt_block_index = 0;
		return write_bmt_to_flash(dat, oob);	/* recursive call */
	}

	MSG(INIT, "Write BMT data to block 0x%x success\n", bmt_block_index);
	return true;
}

/*******************************************************************
* Reconstruct bmt, called when found bmt info doesn't match bad
* block info in flash.
*
* Return NULL for failure
*******************************************************************/
bmt_struct *reconstruct_bmt(bmt_struct *bmt)
{
	int i;
	int index = system_block_count;
	unsigned short bad_index;
	int mapped;
	int ret;

	/* init everything in BMT struct */
	bmt->version = BMT_VERSION;
	bmt->bad_count = 0;
	bmt->mapped_count = 0;

	memset(bmt->table, 0, bmt_block_count * sizeof(bmt_entry));

	for (i = 0; i < bmt_block_count; i++, index++) {
		if (nand_block_bad_bmt(OFFSET(index))) {
			MSG(INIT, "Skip bad block: 0x%x\n", index);
			/* bmt->bad_count++; */
			continue;
		}

		MSG(INIT, "read page: 0x%x\n", PAGE_ADDR(index));
		ret = nand_read_page_bmt(PAGE_ADDR(index), dat_buf, oob_buf);
		if (ret != 1) {
			MSG(INIT, "Error when read block %d, try tlc mode\n", bmt_block_index);
			gn_devinfo.tlcControl.slcopmodeEn = FALSE;
			ret = nand_read_page_bmt(PAGE_ADDR(index), dat_buf, oob_buf);
			if (ret != 1) {
				gn_devinfo.tlcControl.slcopmodeEn = TRUE;
				MSG(INIT, "Error when read block %d in try tlc mode continue\n", bmt_block_index);
				continue;
			}
			gn_devinfo.tlcControl.slcopmodeEn = TRUE;
		}
		bad_index = get_bad_index_from_oob(oob_buf);
		if (bad_index >= system_block_count) {
			MSG(INIT, "get bad index: 0x%x\n", bad_index);
			if (bad_index != 0xFFFF)
				MSG(INIT, "Invalid bad index found in block 0x%x, bad index 0x%x\n",
					index, bad_index);
			continue;
		}

		MSG(INIT, "Block 0x%x is mapped to bad block: 0x%x\n", index, bad_index);

		if (!nand_block_bad_bmt(OFFSET(bad_index))) {
			MSG(INIT, "\tbut block 0x%x is not marked as bad, invalid mapping\n",
				bad_index);
			continue;	/* no need to erase here, it will be erased later when trying to write BMT */
		}
		mapped = is_block_mapped(bad_index);
		if (mapped >= 0) {
			MSG(INIT,
				"bad block 0x%x is mapped to 0x%x, should be caused by power lost, replace with one\n",
				bmt->table[mapped].bad_index, bmt->table[mapped].mapped_index);
			bmt->table[mapped].mapped_index = index;	/* use new one instead. */
		} else {
			/* add mapping to BMT */
			bmt->table[bmt->mapped_count].bad_index = bad_index;
			bmt->table[bmt->mapped_count].mapped_index = index;
			bmt->mapped_count++;
		}

		MSG(INIT, "Add mapping: 0x%x -> 0x%x to BMT\n", bad_index, index);

	}

	MSG(INIT, "Scan replace pool done, mapped block: %d\n", bmt->mapped_count);
	/* dump_bmt_info(bmt); */

	/* fill NAND BMT buffer */
	memset(oob_buf, 0xFF, sizeof(oob_buf));
	fill_nand_bmt_buffer(bmt, dat_buf, oob_buf);
#if 0
	/* write BMT back */
	if (!write_bmt_to_flash(dat_buf, oob_buf))
		MSG(INIT, "TRAGEDY: cannot find a place to write BMT!!!!\n");
#endif
	return bmt;
}

/*******************************************************************
* [BMT Interface]
*
* Description:
* Init bmt from nand. Reconstruct if not found or data error
*
* Parameter:
* size: size of bmt and replace pool
*
* Return:
* NULL for failure, and a bmt struct for success
*******************************************************************/
bmt_struct *init_bmt(struct nand_chip *chip, int size)
{
	struct mtk_nand_host *host;
	u64 temp;

	if (size > 0 && size < MAX_BMT_SIZE) {
		MSG(INIT, "Init bmt table, size: %d\n", size);
		bmt_block_count = size;
	} else {
		MSG(INIT, "Invalid bmt table size: %d\n", size);
		return NULL;
	}
	nand_chip_bmt = chip;
	temp = chip->chipsize;
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	system_block_count = (u32)temp; /* (u32)(chip->chipsize / (gn_devinfo.blocksize * 1024)); */
	total_block_count = bmt_block_count + system_block_count;
	page_per_block = (gn_devinfo.blocksize * 1024) / gn_devinfo.pagesize;
	host = (struct mtk_nand_host *)chip->priv;
	mtd_bmt = &host->mtd;

	MSG(INIT, "mtd_bmt: %p, nand_chip_bmt: %p\n", mtd_bmt, nand_chip_bmt);
	MSG(INIT, "bmt count: %d, system count: %d\n", bmt_block_count, system_block_count);

	/* set this flag, and unmapped block in pool will be erased. */
	pool_erased = 0;
	memset(bmt.table, 0, size * sizeof(bmt_entry));
	bmt_block_index = load_bmt_data(system_block_count, size);
	if (bmt_block_index) {
		MSG(INIT, "Load bmt data success @ block 0x%x\n", bmt_block_index);
		dump_bmt_info(&bmt);
		return &bmt;
	}
	MSG(INIT, "Load bmt data fail, need re-construct!\n");
#if !defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if (reconstruct_bmt(&bmt))
		return &bmt;
#endif
	return NULL;
}
EXPORT_SYMBOL_GPL(init_bmt);
/*******************************************************************
* [BMT Interface]
*
* Description:
* Update BMT.
*
* Parameter:
* offset: update block/page offset.
* reason: update reason, see update_reason_t for reason.
* dat/oob: data and oob buffer for write fail.
*
* Return:
* Return true for success, and false for failure.
*******************************************************************/
bool update_bmt(u64 offset, update_reason_t reason, u8 *dat, u8 *oob)
{
	int map_index;
	int orig_bad_block = -1;
	int i;
	u64 temp;
	u32 bad_index; /* = (u32)(offset / (gn_devinfo.blocksize * 1024)); */

	temp = offset;
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	bad_index = (u32)temp;

/* return false; */

	if (reason == UPDATE_WRITE_FAIL) {
		MSG(INIT, "Write fail, need to migrate\n");
		map_index = migrate_from_bad(offset, dat, oob);
		if (!map_index) {
			MSG(INIT, "migrate fail\n");
			return false;
		}
	} else {
		map_index = find_available_block(false);
		if (!map_index) {
			MSG(INIT, "Cannot find block in pool\n");
			return false;
		}
	}

	/* now let's update BMT */
	if (bad_index >= system_block_count) {
		for (i = 0; i < bmt_block_count; i++) {
			if (bmt.table[i].mapped_index == bad_index) {
				orig_bad_block = bmt.table[i].bad_index;
				break;
			}
		}
		/* bmt.bad_count++; */
		MSG(INIT, "Mapped block becomes bad, orig bad block is 0x%x\n", orig_bad_block);

		bmt.table[i].mapped_index = map_index;
	} else {
		bmt.table[bmt.mapped_count].mapped_index = map_index;
		bmt.table[bmt.mapped_count].bad_index = bad_index;
		bmt.mapped_count++;
	}

	memset(oob_buf, 0xFF, sizeof(oob_buf));
	fill_nand_bmt_buffer(&bmt, dat_buf, oob_buf);
	if (!write_bmt_to_flash(dat_buf, oob_buf))
		return false;

	mark_block_bad_bmt(offset);

	return true;
}
EXPORT_SYMBOL_GPL(update_bmt);
/*******************************************************************
* [BMT Interface]
*
* Description:
* Given an block index, return mapped index if it's mapped, else
* return given index.
*
* Parameter:
* index: given an block index. This value cannot exceed
* system_block_count.
*
* Return NULL for failure
*******************************************************************/
u16 get_mapping_block_index(int index)
{
	int i;
/* return index; */

	if (index > system_block_count)
		return index;

	for (i = 0; i < bmt.mapped_count; i++) {
		if (bmt.table[i].bad_index == index)
			return bmt.table[i].mapped_index;
	}

	return index;
}
EXPORT_SYMBOL_GPL(get_mapping_block_index);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek");
MODULE_DESCRIPTION("Bad Block mapping management for MediaTek NAND Flash Driver");

