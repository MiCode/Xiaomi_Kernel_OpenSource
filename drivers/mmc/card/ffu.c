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
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/ffu.h>

#include "../core/core.h"
#include "../core/mmc_ops.h"

#define  FFU_BUS_FREQ	25000000
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

	if (card && mmc_card_mmc(card) &&
			(card->ext_csd.cache_size > 0)) {
		enable = !!enable;

		if (card->ext_csd.cache_ctrl ^ enable) {
			timeout = enable ? card->ext_csd.generic_cmd6_time : 0;
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_CACHE_CTRL, enable, timeout);
			if (err)
				pr_notice("%s: cache %s error %d\n",
						mmc_hostname(card->host),
						enable ? "on" : "off",
						err);
			else
				card->ext_csd.cache_ctrl = enable;
		}
	}

	return err;
}

/*
 * Checks that a normal transfer didn't have any errors
 */
static int mmc_ffu_check_result(struct mmc_request *mrq)
{
	if (!mrq || !mrq->cmd || !mrq->data)
		return -EINVAL;

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
				pr_notice("%s: Warning: Host did not wait for busy state to end.\n",
					mmc_hostname(card->host));
			}
		}

	} while (mmc_ffu_busy(&cmd));

	return ret;
}

static int mmc_ffu_restart(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;

	card->host->ios.timing = MMC_TIMING_LEGACY;
	mmc_set_clock(card->host, 300000);
	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_1);

	card->state |= MMC_STATE_FFUED;

	err = mmc_reinit_oldcard(host);
	pr_notice("mmc_init_card ret %d\n", err);
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
			pr_notice("%s: switch to FFU failed with error %d\n",
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

int mmc_ffu_install(struct mmc_card *card, u8 *ext_csd)
{
	u8 *ext_csd_new = NULL;
	int err;
	u32 ffu_data_len;
	u32 timeout;
	u8 set = 1;
	u8 retry = 10;

#if !defined(FFU_DUMMY_TEST)
	if (!FFU_FEATURES(ext_csd[EXT_CSD_FFU_FEATURES])) {

		/* host switch back to work in normal MMC Read/Write commands */
		if ((card->cid.manfid == CID_MANFID_HYNIX) &&
			(card->cid.prv == 0x03)) {
			set = 0;
		}

		pr_notice("FFU exit FFU mode\n");
		err = mmc_switch(card, set,
			EXT_CSD_MODE_CONFIG, MMC_FFU_MODE_NORMAL,
			card->ext_csd.generic_cmd6_time);
		if (err) {
			pr_notice("FFU: %s: error %d exit FFU mode\n",
				mmc_hostname(card->host), err);
			mmc_ffu_restart(card);
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
			mmc_ffu_restart(card);
			return err;
		}

		timeout = ext_csd[EXT_CSD_OPERATION_CODE_TIMEOUT];
		if (timeout == 0 || timeout > 0x17) {
			timeout = 0x17;
			pr_notice("FFU: %s: operation code timeout is out of range. Using maximum timeout.\n",
				mmc_hostname(card->host));
		}

		/* timeout is at millisecond resolution */
		timeout = (100 * (1 << timeout) / 1000) + 1;

		/* set ext_csd to install mode */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_MODE_OPERATION_CODES,
			MMC_FFU_INSTALL_SET, timeout);

		if (err) {
			pr_notice("FFU: %s: error %d setting install mode\n",
				mmc_hostname(card->host), err);
			mmc_ffu_restart(card);
			goto exit;
		}

	}
#endif

	err = mmc_ffu_restart(card);
	if (err) {
		pr_notice("FFU: %s: error %d restart\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

#ifdef CONFIG_MTK_EMMC_HW_CQ
	if (card)
		(void)mmc_blk_cmdq_switch(card, 0);
#endif
	/* read ext_csd */
	while (retry--) {
		err = mmc_get_ext_csd(card, &ext_csd_new);
		if (err)
			pr_notice("FFU: %s: sending ext_csd retry times %d\n",
				mmc_hostname(card->host), retry);
		else
			break;
	}
#ifdef CONFIG_MTK_EMMC_HW_CQ
	if (card)
		(void)mmc_blk_cmdq_switch(card, 1);
#endif
	if (err) {
		pr_notice("FFU: %s: sending ext_csd error %d\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* return status */
	err = ext_csd_new[EXT_CSD_FFU_STATUS];
	if (!err) {
		pr_notice("FFU: %s: succeed FFU\n",
			mmc_hostname(card->host));
	} else if (err) {
		pr_notice("FFU: %s: error %d FFU install:\n",
			mmc_hostname(card->host), err);
		err = -EINVAL;
	}

exit:
	kfree(ext_csd_new);
	return err;
}

void mmc_wait_for_ffu_req(struct mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_card *card = host->card;
	struct mmc_command *cmd = mrq->cmd;
#ifndef FFU_DUMMY_TEST
	struct mmc_command stop = {0};
#endif
	u8 *ext_csd = NULL;
	int err;

	if (!mmc_card_mmc(card)) {
		mrq->cmd->error = -EINVAL;
		return;
	} else if (cmd->opcode == MMC_FFU_INSTALL_OP) {
		/*
		 * Always return success since we installed when
		 * MMC_FFU_DOWNLOAD_OP
		 */
		return;
	}
	/* Read EXT_CSD */
	err = mmc_get_ext_csd(card, &ext_csd);
	if (err) {
		pr_notice("FFU: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* Check if FFU is supported by card */
	if (!FFU_SUPPORTED_MODE(ext_csd[EXT_CSD_SUPPORTED_MODE])) {
		err = -EINVAL;
		pr_notice("FFU: %s: error %d FFU is not supported\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	err = mmc_host_set_ffu(card, ext_csd[EXT_CSD_FW_CONFIG]);
	if (err) {
		pr_notice("FFU: %s: error %d FFU fails to enable\n",
				mmc_hostname(card->host), err);
		err = -EINVAL;
		goto exit;
	}

	pr_notice("eMMC cache originally %s -> %s\n",
		((card->ext_csd.cache_ctrl) ? "on" : "off"),
		((card->ext_csd.cache_ctrl) ? "turn off" : "keep"));
	if (card->ext_csd.cache_ctrl) {
		mmc_flush_cache(card);
		mmc_ffu_cache_ctrl(card->host, 0);
	}

	/* set device to FFU mode */
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
		EXT_CSD_MODE_CONFIG,
		MMC_FFU_MODE_SET, card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_notice("FFU: %s: error %d when switch to FFU\n",
			mmc_hostname(card->host), err);
		err = -EINVAL;
		goto exit;
	}

#ifndef FFU_DUMMY_TEST
	/* set CMD ARG */
	cmd->arg = ext_csd[EXT_CSD_FFU_ARG] |
		ext_csd[EXT_CSD_FFU_ARG + 1] << 8 |
		ext_csd[EXT_CSD_FFU_ARG + 2] << 16 |
		ext_csd[EXT_CSD_FFU_ARG + 3] << 24;

	/* If arg is zero, should be set to a special value for samsung
	 */
	if (card->cid.manfid == CID_MANFID_SAMSUNG && cmd->arg == 0x0)
		cmd->arg = 0xc7810000;

	pr_notice("FFU perform write\n");

	//FFU_ARG, DATA_SECTOR_SIZE
	mrq->cmd->opcode = MMC_WRITE_MULTIPLE_BLOCK;
	mrq->stop = &stop;
	mrq->stop->opcode = MMC_STOP_TRANSMISSION;
	mrq->stop->arg = 0;
	mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;

	mmc_wait_for_req(card->host, mrq);
	mmc_ffu_wait_busy(card);

	err = mmc_ffu_check_result(mrq);
	if (err) {
		pr_notice("FFU: %s: error %d write fail\n",
				mmc_hostname(card->host), err);
		goto exit;
	}

#endif
	err = mmc_ffu_install(card, ext_csd);

exit:
	mrq->cmd->error = err;
	kfree(ext_csd);
	return;
}
EXPORT_SYMBOL(mmc_wait_for_ffu_req);
