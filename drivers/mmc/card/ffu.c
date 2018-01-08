/*
 * *  ffu.c
 *
 *  Copyright 2007-2008 Pierre Ossman
 *
 *  Modified by SanDisk Corp., Copyright (c) 2013 SanDisk Corp.
 *  Copyright (C) 2017 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program includes bug.h, card.h, host.h, mmc.h, scatterlist.h,
 * slab.h, ffu.h & swap.h header files
 * The original, unmodified version of this program - the mmc_test.c
 * file - is obtained under the GPL v2.0 license that is available via
 * http://www.gnu.org/licenses/,
 * or http://www.opensource.org/licenses/gpl-2.0.php
*/

#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/mmc/ffu.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include "samsung_64GB_emmc_p05_B614.h"
#include "queue.h"
#include "../core/core.h"

struct mmc_blk_data {
	spinlock_t lock;
	struct gendisk *disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int flags;
#define MMC_BLK_CMD23	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR	(1 << 1)	/* MMC Reliable write support */
#define MMC_BLK_PACKED_CMD	(1 << 2)	/* MMC packed command support */
#define MMC_BLK_CMD_QUEUE	(1 << 3)	/* MMC command queue support */

	unsigned int usage;
	unsigned int read_only;
	unsigned int part_type;
	unsigned int name_idx;
	unsigned int reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)
#define MMC_BLK_FLUSH		BIT(4)

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with mmc_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	struct device_attribute num_wr_reqs_to_start_packing;
	struct device_attribute bkops_check_threshold;
	struct device_attribute no_pack_for_random;
	int area_type;
};

extern int mmc_blk_cmdq_switch(struct mmc_card *card,
			struct mmc_blk_data *md, bool enable);

#define SAMSUNG_CID_MANFID 0x15
#define SAMSUNG_64GB_CID_B614_PROD_NAME "RH64MB"

/**
 * struct mmc_ffu_pages - pages allocated by 'alloc_pages()'.
 *  <at> page: first page in the allocation
 *  <at> order: order of the number of pages allocated
 */

struct mmc_ffu_pages {
	struct page *page;
	unsigned int order;
};

/**
 * struct mmc_ffu_mem - allocated memory.
 *  <at> arr: array of allocations
 *  <at> cnt: number of allocations
 */
struct mmc_ffu_mem {
	struct mmc_ffu_pages *arr;
	unsigned int cnt;
};

struct mmc_ffu_area {
	unsigned long max_sz;
	unsigned int max_tfr;
	unsigned int max_segs;
	unsigned int max_seg_sz;
	unsigned int blocks;
	unsigned int sg_len;
	struct mmc_ffu_mem *mem;
	struct scatterlist *sg;
};

struct fw_update_info {
	unsigned int manfid;
	char prod_name[8];
	u8 old_fw_ver;
	u8 new_fw_ver;
	u8 *update_arry;
	u32 update_arry_size;
};

static void mmc_ffu_prepare_mrq(struct mmc_card *card,
				struct mmc_request *mrq, struct scatterlist *sg,
				unsigned int sg_len, u32 arg,
				unsigned int blocks, unsigned int blksz,
				int write)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

	if (blocks > 1) {
		mrq->cmd->opcode = write ?
		    MMC_WRITE_MULTIPLE_BLOCK : MMC_READ_MULTIPLE_BLOCK;
	} else {
		mrq->cmd->opcode = write ? MMC_WRITE_BLOCK :
		    MMC_READ_SINGLE_BLOCK;
	}

	mrq->cmd->arg = arg;
	if (!mmc_card_blockaddr(card))
		mrq->cmd->arg <<= 9;

	mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	if (blocks == 1) {
		mrq->stop = NULL;
	} else {
		mrq->stop->opcode = MMC_STOP_TRANSMISSION;
		mrq->stop->arg = 0;
		mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	}

	mrq->data->blksz = blksz;
	mrq->data->blocks = blocks;
	mrq->data->flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
	mrq->data->sg = sg;
	mrq->data->sg_len = sg_len;

	mmc_set_data_timeout(mrq->data, card);
}

/*
 * Checks that a normal transfer didn't have any errors
 */
static int mmc_ffu_check_result(struct mmc_request *mrq)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data);

	if (mrq->cmd->error != 0)
		return -EINVAL;

	if (mrq->data->error != 0)
		return -EINVAL;

	if (mrq->stop != NULL && mrq->stop->error != 0)
		return -EINVAL;

	if (mrq->data->bytes_xfered != (mrq->data->blocks * mrq->data->blksz))
		return -EINVAL;

	return 0;
}

static int mmc_ffu_busy(struct mmc_command *cmd)
{
	return !(cmd->resp[0] & R1_READY_FOR_DATA) ||
	    (R1_CURRENT_STATE(cmd->resp[0]) == R1_STATE_PRG);
}

static int mmc_ffu_wait_busy(struct mmc_card *card)
{
	int ret, busy = 0;
	struct mmc_command cmd = { 0 };

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	do {
		ret = mmc_wait_for_cmd(card->host, &cmd, 0);
		if (ret)
			break;

		if (!busy && mmc_ffu_busy(&cmd)) {
			busy = 1;
			if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY) {
				pr_warn("%s: Warning: Host did not "
					"wait for busy state to end.\n",
					mmc_hostname(card->host));
			}
		}

	} while (mmc_ffu_busy(&cmd));

	return ret;
}

/*
 * transfer with certain parameters
 */
static int mmc_ffu_simple_transfer(struct mmc_card *card,
				   struct scatterlist *sg, unsigned int sg_len,
				   u32 arg, unsigned int blocks,
				   unsigned int blksz, int write)
{
	struct mmc_request mrq = { 0 };
	struct mmc_command cmd = { 0 };
	struct mmc_command stop = { 0 };
	struct mmc_data data = { 0 };

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;
	mmc_ffu_prepare_mrq(card, &mrq, sg, sg_len, arg, blocks, blksz, write);
	mmc_wait_for_req(card->host, &mrq);

	mmc_ffu_wait_busy(card);

	return mmc_ffu_check_result(&mrq);
}

/*
 * Map memory into a scatterlist.
 */
static int mmc_ffu_map_sg(struct mmc_ffu_mem *mem, unsigned long size,
			  struct scatterlist *sglist, unsigned int max_segs,
			  unsigned int max_seg_sz, unsigned int *sg_len,
			  int min_sg_len)
{
	struct scatterlist *sg = NULL;
	unsigned int i;
	unsigned long sz = size;

	sg_init_table(sglist, max_segs);
	if (min_sg_len > max_segs)
		min_sg_len = max_segs;

	*sg_len = 0;
	do {
		for (i = 0; i < mem->cnt; i++) {
			unsigned long len = PAGE_SIZE << mem->arr[i].order;

			if (min_sg_len && (size / min_sg_len < len))
				len = ALIGN(size / min_sg_len, CARD_BLOCK_SIZE);
			if (len > sz)
				len = sz;
			if (len > max_seg_sz)
				len = max_seg_sz;
			if (sg)
				sg = sg_next(sg);
			else
				sg = sglist;
			if (!sg)
				return -EINVAL;
			sg_set_page(sg, mem->arr[i].page, len, 0);
			sz -= len;
			*sg_len += 1;
			if (!sz)
				break;
		}
	} while (sz);

	if (sg)
		sg_mark_end(sg);

	return 0;
}

static void mmc_ffu_free_mem(struct mmc_ffu_mem *mem)
{
	if (!mem)
		return;
	while (mem->cnt--)
		__free_pages(mem->arr[mem->cnt].page, mem->arr[mem->cnt].order);
	kfree(mem->arr);
}

/*
 * Cleanup struct mmc_ffu_area.
 */
static int mmc_ffu_area_cleanup(struct mmc_ffu_area *area)
{
	kfree(area->sg);
	mmc_ffu_free_mem(area->mem);

	return 0;
}

/*
 * Allocate a lot of memory, preferably max_sz but at least min_sz. In case
 * there isn't much memory do not exceed 1/16th total low mem pages. Also do
 * not exceed a maximum number of segments and try not to make segments much
 * bigger than maximum segment size.
 */
static struct mmc_ffu_mem *mmc_ffu_alloc_mem(unsigned long min_sz,
				unsigned long max_sz,
				unsigned int max_segs,
				unsigned int max_seg_sz)
{
	unsigned long max_page_cnt = DIV_ROUND_UP(max_sz, PAGE_SIZE);
	unsigned long min_page_cnt = DIV_ROUND_UP(min_sz, PAGE_SIZE);
	unsigned long max_seg_page_cnt = DIV_ROUND_UP(max_seg_sz, PAGE_SIZE);
	unsigned long page_cnt = 0;
	unsigned long limit = nr_free_buffer_pages() >> 4;
	struct mmc_ffu_mem *mem;

	if (max_page_cnt > limit)
		max_page_cnt = limit;
	if (min_page_cnt > max_page_cnt)
		min_page_cnt = max_page_cnt;

	if (max_seg_page_cnt > max_page_cnt)
		max_seg_page_cnt = max_page_cnt;

	if (max_segs > max_page_cnt)
		max_segs = max_page_cnt;

	mem = kzalloc(sizeof(struct mmc_ffu_mem), GFP_KERNEL);
	if (!mem)
		return NULL;

	mem->arr = kzalloc(sizeof(struct mmc_ffu_pages) * max_segs, GFP_KERNEL);
	if (!mem->arr)
		goto out_free;

	while (max_page_cnt) {
		struct page *page;
		unsigned int order;
		gfp_t flags = GFP_KERNEL | GFP_DMA | __GFP_NOWARN |
				__GFP_NORETRY;

		order = get_order(max_seg_page_cnt << PAGE_SHIFT);
		while (1) {
			page = alloc_pages(flags, order);
			if (page || !order)
				break;
			order -= 1;
		}
		if (!page) {
			if (page_cnt < min_page_cnt)
				goto out_free;
			break;
		}
		mem->arr[mem->cnt].page = page;
		mem->arr[mem->cnt].order = order;
		mem->cnt += 1;
		if (max_page_cnt <= (1UL << order))
			break;
		max_page_cnt -= 1UL << order;
		page_cnt += 1UL << order;
		if (mem->cnt >= max_segs) {
			if (page_cnt < min_page_cnt)
				goto out_free;
			break;
		}
	}

	return mem;

out_free:
	mmc_ffu_free_mem(mem);
	return NULL;
}

/*
 * Initialize an area for data transfers.
 * Copy the data to the allocated pages.
 */
static int mmc_ffu_area_init(struct mmc_ffu_area *area, struct mmc_card *card,
				u8 *data, unsigned int size)
{
	int ret, i, length;

	area->max_segs = card->host->max_segs;
	area->max_seg_sz = card->host->max_seg_size & ~(CARD_BLOCK_SIZE - 1);
	area->max_tfr = size;

	if (area->max_tfr >> 9 > card->host->max_blk_count)
		area->max_tfr = card->host->max_blk_count << 9;
	if (area->max_tfr > card->host->max_req_size)
		area->max_tfr = card->host->max_req_size;
	if (area->max_tfr / area->max_seg_sz > area->max_segs)
		area->max_tfr = area->max_segs * area->max_seg_sz;

	/*
	 * Try to allocate enough memory for a max. sized transfer. Less is OK
	 * because the same memory can be mapped into the scatterlist more than
	 * once. Also, take into account the limits imposed on scatterlist
	 * segments by the host driver.
	 */
	area->mem = mmc_ffu_alloc_mem(1, area->max_tfr, area->max_segs,
				area->max_seg_sz);
	if (!area->mem)
		return -ENOMEM;

	/* copy data to page */
	length = 0;
	for (i = 0; i < area->mem->cnt; i++) {
		memcpy(page_address(area->mem->arr[i].page), data + length,
		       min(size - length, area->max_seg_sz));
		length += area->max_seg_sz;
	}

	area->sg = kmalloc(sizeof(struct scatterlist) * area->max_segs,
				GFP_KERNEL);
	if (!area->sg) {
		ret = -ENOMEM;
		goto out_free;
	}

	ret = mmc_ffu_map_sg(area->mem, size, area->sg,
			area->max_segs, area->max_seg_sz, &area->sg_len,
			1);

	if (ret != 0)
		goto out_free;

	return 0;

out_free:
	mmc_ffu_area_cleanup(area);
	return ret;
}

static int mmc_ffu_write(struct mmc_card *card, u8 *src, u32 arg, int size)
{
	int rc;
	struct mmc_ffu_area mem;

	mem.sg = NULL;
	mem.mem = NULL;

	if (!src) {
		pr_err("FFU: %s: data buffer is NULL\n",
				mmc_hostname(card->host));
		return -EINVAL;
	}
	rc = mmc_ffu_area_init(&mem, card, src, size);
	if (rc != 0)
		goto exit;

	rc = mmc_ffu_simple_transfer(card, mem.sg, mem.sg_len, arg,
			size / CARD_BLOCK_SIZE, CARD_BLOCK_SIZE,
			1);

exit:
	mmc_ffu_area_cleanup(&mem);
	return rc;
}

/* Flush all scheduled work from the MMC work queue.
 * and initialize the MMC device */
static int mmc_ffu_restart(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;

	mmc_cache_ctrl(host, 0);
	/*in msm8956 we use mmc_power_save_host and mmc_power_restore_host to reset*/
	err = mmc_hw_reset(host);
	if (err) {
		pr_warn("%s: hw_reset failed%d\n", __func__, err);
		return err;
	}
	pr_debug("mmc_ffu_restart done\n");
	return err;
}

/* Host set the EXT_CSD */
static int mmc_host_set_ffu(struct mmc_card *card, u32 ffu_enable)
{
	int err;

	/* check if card is eMMC 5.0 or higher */
	if (card->ext_csd.rev < 7)
		return -EINVAL;

	if (FFU_ENABLED(ffu_enable)) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_FW_CONFIG, MMC_FFU_ENABLE,
				 card->ext_csd.generic_cmd6_time);
		if (err) {
			pr_err("%s: switch to FFU failed with error %d\n",
			       mmc_hostname(card->host), err);
			return err;
		}
	}

	return 0;
}

#define CID_MANFID_SAMSUNG             0x15

int mmc_ffu_download(struct mmc_card *card, struct mmc_command *cmd,
			u8 *data, int buf_bytes)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;
	int ret;
	/* Read the EXT_CSD */
	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("FFU: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* Check if FFU is supported by card */
	if (!FFU_SUPPORTED_MODE(ext_csd[EXT_CSD_SUPPORTED_MODE])) {
		err = -EINVAL;
		pr_err("FFU: %s: error %d FFU is not supported\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	err = mmc_host_set_ffu(card, ext_csd[EXT_CSD_FW_CONFIG]);
	if (err) {
		pr_err("FFU: %s: error %d FFU is not supported\n",
				mmc_hostname(card->host), err);
		err = -EINVAL;
		goto exit;
	}

	/* set device to FFU mode */
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
			 MMC_FFU_MODE_SET, card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("FFU: %s: error %d FFU is not supported\n",
				mmc_hostname(card->host), err);
		goto exit_normal;
	}

	/* set CMD ARG */
	cmd->arg = ext_csd[EXT_CSD_FFU_ARG] |
			ext_csd[EXT_CSD_FFU_ARG + 1] << 8 |
			ext_csd[EXT_CSD_FFU_ARG + 2] << 16 |
			ext_csd[EXT_CSD_FFU_ARG + 3] << 24;

	/* If arg is zero, should be set to a special value for samsung eMMC
	 */
	if (card->cid.manfid == CID_MANFID_SAMSUNG && cmd->arg == 0x0) {
		cmd->arg = 0xc7810000;
	}

	err = mmc_ffu_write(card, data, cmd->arg, buf_bytes);
	if (err) {
		pr_err(" mmc_ffu_write:  %s  %d\n", __FILE__, __LINE__);
	}

exit_normal:
	/* host switch back to work in normal MMC Read/Write commands */
	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_MODE_CONFIG, MMC_FFU_MODE_NORMAL,
			 card->ext_csd.generic_cmd6_time);
	if (ret)
		err = ret;

exit:
	return err;
}

EXPORT_SYMBOL(mmc_ffu_download);

int mmc_ffu_install(struct mmc_card *card, u8 new_fw_ver)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;
	u32 ffu_data_len;
	u32 timeout;

	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("FFU: %s: error %d sending ext_csd\n",
				mmc_hostname(card->host), err);
		goto exit;
	}

	/* Check if FFU is supported */
	if (!FFU_SUPPORTED_MODE(ext_csd[EXT_CSD_SUPPORTED_MODE]) ||
			FFU_CONFIG(ext_csd[EXT_CSD_FW_CONFIG])) {
		err = -EINVAL;
		pr_err("FFU: %s: error %d FFU is not supported\n",
				mmc_hostname(card->host), err);
		goto exit;
	}

	/* check mode operation */
	if (!FFU_FEATURES(ext_csd[EXT_CSD_FFU_FEATURES])) {
		/* restart the eMMC */
		err = mmc_ffu_restart(card);
		if (err) {
			pr_err("FFU: %s: error %d FFU install:\n",
					mmc_hostname(card->host), err);
		}
	} else {
		ffu_data_len = ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG] |
				ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 1] << 8 |
				ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 2] << 16 |
				ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 3] << 24;

		if (!ffu_data_len) {
			err = -EPERM;
			return err;
		}

		/* set device to FFU mode */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_MODE_CONFIG, 0x1,
				card->ext_csd.generic_cmd6_time);
		if (err) {

			pr_err("FFU: %s: error %d FFU is not supported\n",
					mmc_hostname(card->host), err);
			goto exit;
		}

		timeout = ext_csd[EXT_CSD_OPERATION_CODE_TIMEOUT];
		if (timeout == 0 || timeout > 0x17) {
			timeout = 0x17;
			pr_warn("FFU: %s: operation code timeout is out "
				"of range. Using maximum timeout.\n",
				mmc_hostname(card->host));
		}

		/* timeout is at millisecond resolution */
		timeout = (100 * (1 << timeout) / 1000) + 1;

		/* set ext_csd to install mode */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_MODE_OPERATION_CODES,
				MMC_FFU_INSTALL_SET, timeout);

		if (err) {
			pr_err("FFU: %s: error %d setting install mode\n",
					mmc_hostname(card->host), err);
			goto exit;
		}

	}

	/* read ext_csd */
	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("FFU: %s: error %d sending ext_csd\n",
				mmc_hostname(card->host), err);
		goto exit;
	}
	/* return status */
	err = ext_csd[EXT_CSD_FFU_STATUS];
	if (err) {
		pr_err("FFU: %s: error %d FFU install:\n",
				mmc_hostname(card->host), err);
		err = -EINVAL;
		goto exit;
	} else {
			if (ext_csd[EXT_CSD_FW_VERSION] == new_fw_ver) {
				pr_info("install finished ,fw_version has updated to  %d\n", (int)new_fw_ver);
		} else {
			pr_info
			("fw install status is  ok,but the version is not right,the version : %d    %s  %s  %d\n",
			ext_csd[EXT_CSD_FW_VERSION], __FILE__,
			__FUNCTION__, __LINE__);
		}
	}

exit:
	return err;
}

EXPORT_SYMBOL(mmc_ffu_install);

int update_fw(struct mmc_card *card, struct fw_update_info *fw_info)
{
	struct mmc_command cmd = { 0 };
	struct mmc_blk_data *md;
	int err = 0;

	md = mmc_get_drvdata(card);
	mmc_claim_host(card->host);
	/* disable cmdq ,or the following operation will fail.*/
	mmc_blk_cmdq_switch(card, md, false);
	cmd.opcode = MMC_FFU_DOWNLOAD_OP;
	err =
	mmc_ffu_download(card, &cmd, fw_info->update_arry,
			     fw_info->update_arry_size);
	if (err) {
		pr_err("emmc ffu_download failed %d\n", err);
		goto ffu_out;
	} else {
		pr_info
		(" %s  %s  %d  mmc_ffu_download done,start install...\n",
		__FILE__, __FUNCTION__, __LINE__);
		msleep(100);
		err = mmc_ffu_install(card, fw_info->new_fw_ver);
		if (err)
			pr_err
			(" %s  %s  %d  mmc_ffu_install failed err = %d\n",
			__FILE__, __FUNCTION__, __LINE__, err);
	}
ffu_out:
	mmc_blk_cmdq_switch(card, md, true);
	mmc_release_host(card->host);
	return err;
}

static struct fw_update_info ffu_table[] = {
	{SAMSUNG_CID_MANFID, SAMSUNG_64GB_CID_B614_PROD_NAME, 2, 5,
			ss_emmc_fw_binary_64_p05_B614, sizeof(ss_emmc_fw_binary_64_p05_B614)},
};

int mmc_ffu(struct mmc_card *card)
{
	int i;
	int err = 0;
	pr_info("mmc_ffu start,mid:%d,name:%s,fw_ver:%d\n",
		card->cid.manfid,
		card->cid.prod_name, card->ext_csd.fw_version);
	for (i = 0; i < sizeof(ffu_table) / sizeof(ffu_table[0]); i++) {
		if (card->cid.manfid == ffu_table[i].manfid &&
			!strncmp(card->cid.prod_name, ffu_table[i].prod_name,
			strlen(ffu_table[i].prod_name))) {
			if (card->ext_csd.fw_version == ffu_table[i].old_fw_ver) {
				err = update_fw(card, ffu_table + i);
			} else if (card->ext_csd.fw_version ==
				   ffu_table[i].new_fw_ver)
				pr_info
				("the emmc fw_version has updated to %d\n",
				ffu_table[i].new_fw_ver);
			else {
				pr_err("emmc fw_version misstach %d\n",
				card->ext_csd.fw_version);
			}
		}
	}
	pr_info("mmc_ffu end\n");
	return err;
}

EXPORT_SYMBOL(mmc_ffu);
