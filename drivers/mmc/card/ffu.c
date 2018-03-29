/*
 * *  ffu.c
 *
 *  Copyright 2007-2008 Pierre Ossman
 *
 *  Modified by SanDisk Corp., Copyright (c) 2013 SanDisk Corp.
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
#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/mmc/ffu.h>

#include "../core/core.h"

#include <asm/uaccess.h>

#define  FFU_BUS_FREQ	25000000

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

/* This strcut is cloned from mmc/card/block.c without modification*/
struct mmc_blk_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
};

/*
 * Turn the cache ON/OFF.
 * Turning the cache OFF shall trigger flushing of the data
 * to the non-volatile storage.
 * This function should be called with host claimed
 */
int mmc_ffu_cache_ctrl(struct mmc_host *host, u8 enable)
{
	struct mmc_card *card = host->card;
	unsigned int timeout;
	int err = 0;

#ifdef CONFIG_MTK_EMMC_CACHE
	if (card->quirks & MMC_QUIRK_DISABLE_CACHE)
		return err;
#endif

	if (card && mmc_card_mmc(card) &&
			(card->ext_csd.cache_size > 0)) {
		enable = !!enable;

		if (card->ext_csd.cache_ctrl ^ enable) {
			timeout = enable ? card->ext_csd.generic_cmd6_time : 0;
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_CACHE_CTRL, enable, timeout);
			if (err)
				pr_err("%s: cache %s error %d\n",
						mmc_hostname(card->host),
						enable ? "on" : "off",
						err);
			else
				card->ext_csd.cache_ctrl = enable;
		}
	}

	return err;
}

/* This function is cloned from mmc_blk_ioctl_copy_from_user() and only change
   MMC_IOC_MAX_BYTES as MMC_FFU_IOC_MAX_BYTES */
struct mmc_blk_ioc_data *mmc_ffu_ioctl_copy_from_user(
	struct mmc_ioc_cmd __user *user)
{
	struct mmc_blk_ioc_data *idata;
	int err;

	idata = kzalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(&idata->ic, user, sizeof(idata->ic))) {
		err = -EFAULT;
		goto idata_err;
	}

	idata->buf_bytes = (u64) idata->ic.blksz * idata->ic.blocks;
	if (idata->buf_bytes > MMC_FFU_IOC_MAX_BYTES) {
		err = -EOVERFLOW;
		goto idata_err;
	}

	if (!idata->buf_bytes)
		return idata;

	idata->buf = kzalloc(idata->buf_bytes, GFP_KERNEL);
	if (!idata->buf) {
		err = -ENOMEM;
		goto idata_err;
	}

	if (copy_from_user(idata->buf, (void __user *)(unsigned long)
					idata->ic.data_ptr, idata->buf_bytes)) {
		err = -EFAULT;
		goto copy_err;
	}

	return idata;

copy_err:
	kfree(idata->buf);
idata_err:
	kfree(idata);
out:
	return ERR_PTR(err);
}

static void mmc_ffu_prepare_mrq(struct mmc_card *card,
	struct mmc_request *mrq, struct scatterlist *sg, unsigned int sg_len,
	u32 arg, unsigned int blocks, unsigned int blksz, int write)
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
		return -1;

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
	struct mmc_command cmd = {0};

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
				pr_warn("%s: Warning: Host did not wait for busy state to end.\n",
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
	struct scatterlist *sg, unsigned int sg_len, u32 arg,
	unsigned int blocks, unsigned int blksz, int write)
{
	struct mmc_request mrq = {0};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;
	mmc_ffu_prepare_mrq(card, &mrq, sg, sg_len, arg, blocks, blksz,
		write);
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

	kfree(mem);
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
	unsigned long max_sz, unsigned int max_segs, unsigned int max_seg_sz)
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

	mem->arr = kcalloc(max_segs, sizeof(struct mmc_ffu_pages), GFP_KERNEL);
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

	area->sg = kmalloc_array(area->max_segs, sizeof(struct scatterlist),
		GFP_KERNEL);
	if (!area->sg) {
		ret = -ENOMEM;
		goto out_free;
	}

	ret = mmc_ffu_map_sg(area->mem, size, area->sg,
			area->max_segs, area->max_seg_sz, &area->sg_len, 1);

	if (ret != 0)
		goto out_free;

	return 0;

out_free:
	mmc_ffu_area_cleanup(area);
	return ret;
}

static int mmc_ffu_write(struct mmc_card *card, u8 *src, u32 arg,
	int size)
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
		size / CARD_BLOCK_SIZE, CARD_BLOCK_SIZE, 1);

	pr_err("FFU write result %d\n", rc);

exit:
	mmc_ffu_area_cleanup(&mem);
	return rc;
}

static int mmc_ffu_restart(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;

	card->host->ios.timing = MMC_TIMING_LEGACY;
	mmc_set_clock(card->host, 300000);
	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_1);

	card->state |= MMC_STATE_FFUED;
	mmc_power_off(host);
	mmc_power_up(host, card->ocr);
	err = mmc_reinit_oldcard(host);
	pr_err("mmc_init_card ret %d\n", err);
	if (!err)
		card->state &= ~MMC_STATE_FFUED;

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

#define CID_MANFID_SANDISK	0x2
#define CID_MANFID_TOSHIBA	0x11
#define CID_MANFID_MICRON	0x13
#define CID_MANFID_SAMSUNG	0x15
#define CID_MANFID_SANDISK_NEW	0x45
#define CID_MANFID_KSI		0x70
#define CID_MANFID_HYNIX	0x90


static int mmc_ffu_reduce_speed(struct mmc_card *card)
{
	int err;
	u8 bus_width = EXT_CSD_BUS_WIDTH_1;
	u8 timing = MMC_TIMING_LEGACY, hs_timing = 0;
	u32 clock;

	/* Reduce to safe and lower clock speed */
	if (card->host->ios.clock > FFU_BUS_FREQ)
		clock = FFU_BUS_FREQ;
	else
		clock = card->host->ios.clock;

	/* Some device does not allow FFU in 8 bit mode,
	   so switch to 4bit mode */

	if (card->host->ios.timing == MMC_TIMING_MMC_HS400 ||
	    card->host->ios.timing == MMC_TIMING_MMC_HS200 ||
	    card->host->ios.timing == MMC_TIMING_MMC_DDR52) {
		timing = MMC_TIMING_MMC_HS;
		bus_width = EXT_CSD_BUS_WIDTH_4;
		hs_timing = 1;
	} else if (card->host->ios.timing == MMC_TIMING_MMC_HS) {
		if (!(card->host->caps &
		      (MMC_CAP_8_BIT_DATA | MMC_CAP_4_BIT_DATA))) {
			bus_width = EXT_CSD_BUS_WIDTH_1;
		} else {
			bus_width = EXT_CSD_BUS_WIDTH_4;
		}
		hs_timing = 1;
	} else if (card->host->ios.timing == MMC_TIMING_LEGACY) {
		if (!(card->host->caps &
		      (MMC_CAP_8_BIT_DATA | MMC_CAP_4_BIT_DATA))) {
			bus_width = EXT_CSD_BUS_WIDTH_1;
		} else {
			bus_width = EXT_CSD_BUS_WIDTH_4;
		}
		hs_timing = 0;
	}

	if (hs_timing == 1) {
		pr_err("FFU switch to HS\n");
		/* After changing timing, platform dependent HW may fail to
		   correctly latch response of CMD13 for checking card status.
		   Therefore __mmc_switch(..., true, false, false) is invoked
		   to avoid using CMD13 for checking card status */
		err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_HS_TIMING, hs_timing,
			card->ext_csd.generic_cmd6_time,
			true, false, false);
		if (err) {
			pr_err("FFU: %s: error %d switch to high-speed\n",
				mmc_hostname(card->host), err);
			goto exit;
		}

		mmc_set_timing(card->host, timing);
	}

	mmc_set_clock(card->host, clock);

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
		EXT_CSD_BUS_WIDTH, bus_width,
		card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("FFU: %s: error %d change bus width to 4 bit\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	if (bus_width == EXT_CSD_BUS_WIDTH_1)
		mmc_set_bus_width(card->host, MMC_BUS_WIDTH_1);
	else
		mmc_set_bus_width(card->host, MMC_BUS_WIDTH_4);

exit:
	return err;
}

int mmc_ffu_install(struct mmc_card *card, u8 *ext_csd)
{
	int err;
	u32 ffu_data_len;
	u32 timeout;
	u8 set = 1;
	u8 retry = 10;

	if (!FFU_FEATURES(ext_csd[EXT_CSD_FFU_FEATURES])) {

		/* host switch back to work in normal MMC Read/Write commands */
		if ((card->cid.manfid == CID_MANFID_HYNIX) &&
			(card->cid.prv == 0x03)) {
			set = 0;
		}

		pr_err("FFU exit FFU mode\n");
		err = mmc_switch(card, set,
			EXT_CSD_MODE_CONFIG, MMC_FFU_MODE_NORMAL,
			card->ext_csd.generic_cmd6_time);
		if (err) {
			pr_err("FFU: %s: error %d exit FFU mode\n",
				mmc_hostname(card->host), err);
			goto exit;
		}

	}

	/* check mode operation */
	if (FFU_FEATURES(ext_csd[EXT_CSD_FFU_FEATURES])) {
		ffu_data_len = ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG] |
			       ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 1] << 8 |
			       ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 2] << 16 |
			       ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 3] << 24;

		if (!ffu_data_len) {
			err = -EPERM;
			return err;
		}

		timeout = ext_csd[EXT_CSD_OPERATION_CODE_TIMEOUT];
		if (timeout == 0 || timeout > 0x17) {
			timeout = 0x17;
			pr_warn("FFU: %s: operation code timeout is out of range. Using maximum timeout.\n",
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

	pr_err("FFU re-init eMMC at higher speed\n");
	err = mmc_ffu_restart(card);
	if (err) {
		pr_err("FFU: %s: error %d restart\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* read ext_csd */
	while (retry--) {
		err = mmc_send_ext_csd(card, ext_csd);
		if (err)
			pr_err("FFU: %s: sending ext_csd retry times %d\n",
				mmc_hostname(card->host), retry);
		else
			break;
	}
	if (err) {
		pr_err("FFU: %s: sending ext_csd error %d\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* return status */
	err = ext_csd[EXT_CSD_FFU_STATUS];
	if (!err) {
		pr_err("FFU: %s: succeed FFU\n",
			mmc_hostname(card->host));
	} else if (err) {
		pr_err("FFU: %s: error %d FFU install:\n",
			mmc_hostname(card->host), err);
		err = -EINVAL;
	}

exit:
	return err;
}

int mmc_ffu_download(struct mmc_card *card, struct mmc_command *cmd,
	u8 *data, int buf_bytes)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;

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

	pr_err("eMMC cache originally %s -> %s\n",
		((card->ext_csd.cache_ctrl) ? "on" : "off"),
		((card->ext_csd.cache_ctrl) ? "turn off" : "keep"));
	if (card->ext_csd.cache_ctrl) {
		mmc_flush_cache(card);
		mmc_ffu_cache_ctrl(card->host, 0);
	}

	mmc_ffu_reduce_speed(card);

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
		err = -EINVAL;
		goto exit;
	}

	/* set CMD ARG */
	cmd->arg = ext_csd[EXT_CSD_FFU_ARG] |
		ext_csd[EXT_CSD_FFU_ARG + 1] << 8 |
		ext_csd[EXT_CSD_FFU_ARG + 2] << 16 |
		ext_csd[EXT_CSD_FFU_ARG + 3] << 24;

	/* If arg is zero, should be set to a special value for samsung eMMC
	 */
	if (card->cid.manfid == CID_MANFID_SAMSUNG && cmd->arg == 0x0)
		cmd->arg = 0xc7810000;

	pr_err("FFU perform write\n");
	err = mmc_ffu_write(card, data, cmd->arg, buf_bytes);
	if (err && (FFU_FEATURES(ext_csd[EXT_CSD_FFU_FEATURES]))) {
		/* FIX ME, to set FFU_ABORT to MODE_OPERATION_CODES */
		;
	} else {
		err = mmc_ffu_install(card, ext_csd);
	}

exit:
	return err;
}
EXPORT_SYMBOL(mmc_ffu_download);
