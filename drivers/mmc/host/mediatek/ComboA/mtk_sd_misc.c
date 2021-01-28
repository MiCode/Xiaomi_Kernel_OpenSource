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

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "mtk_sd.h"
#include <mmc/core/core.h>
#include <mmc/core/card.h>
#include <mmc/core/mmc_ops.h>
#include <mt-plat/mtk_sd_misc.h>
#include "msdc_io.h"
#include "dbg.h"

#define PARTITION_NAME_LENGTH   (64)
#define DRV_NAME_MISC           "misc-sd"

#define DEBUG_MMC_IOCTL
#ifdef DEBUG_MMC_IOCTL
#define MMC_IOCTL_DECLARE_INT32(var) int var
#define MMC_IOCTL_PR_DBG(fmt, args...)   pr_debug(fmt, ##args)
#else
#define MMC_IOCTL_DECLARE_INT32(...)
#define MMC_IOCTL_PR_DBG(fmt, args...)
#endif

/*
 * For simple_sd_ioctl
 */
#define FORCE_IN_DMA            (0x11)
#define FORCE_IN_PIO            (0x10)
#define FORCE_NOTHING           (0x0)

static u32 *sg_msdc_multi_buffer;
#define SG_MSDC_MULTI_BUFFER_SIZE (64 * 1024)

static int simple_sd_open(struct inode *inode, struct file *file)
{
	return 0;
}

int msdc_reinit(struct msdc_host *host)
{
	struct mmc_host *mmc;
	struct mmc_card *card;
	int ret = -1;

	if (!host || !host->mmc || !host->mmc->card) {
		pr_notice("Invalid msdc_host, mmc_host, or card");
		return -1;
	}

	mmc = host->mmc;
	card = mmc->card;

	if (host->hw->host_function != MSDC_SD)
		goto skip_reinit2;

	if (host->block_bad_card)
		ERR_MSG("Need block this bad SD card from re-initialization");

	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)
	 || (host->block_bad_card != 0))
		goto skip_reinit1;
	mmc_get_card(card);
	mmc->ios.timing = MMC_TIMING_LEGACY;
	msdc_ops_set_ios(mmc, &mmc->ios);
	/* FIX ME, check if bus_ops->reset() shall be un-commented */
	/* power reset sdcard */
	/* ret = mmc->bus_ops->reset(mmc); */
	mmc_put_card(card);

	ERR_MSG("Reinit %s", ret == 0 ? "success" : "fail");

skip_reinit1:
	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE) && (host->mmc->card)
		&& mmc_card_present(host->mmc->card)
		&& (!mmc_card_removed(host->mmc->card))
		&& (host->block_bad_card == 0))
		ret = 0;
skip_reinit2:
	return ret;
}

static int simple_sd_ioctl_get_cid(struct msdc_ioctl *msdc_ctl)
{
	struct msdc_host *host_ctl;

	if (!msdc_ctl)
		return -EINVAL;

	host_ctl = mtk_msdc_host[msdc_ctl->host_num];

	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice("host_ctl or mmc or card is NULL\n");
		return -EINVAL;
	}

	MMC_IOCTL_PR_DBG("user want the cid in msdc slot%d\n",
		msdc_ctl->host_num);

	if (copy_to_user(msdc_ctl->buffer, &host_ctl->mmc->card->raw_cid, 16))
		return -EFAULT;

	MMC_IOCTL_PR_DBG("cid:0x%x,0x%x,0x%x,0x%x\n",
		host_ctl->mmc->card->raw_cid[0],
		host_ctl->mmc->card->raw_cid[1],
		host_ctl->mmc->card->raw_cid[2],
		host_ctl->mmc->card->raw_cid[3]);

	return 0;

}

static int simple_sd_ioctl_set_bootpart(struct msdc_ioctl *msdc_ctl)
{
	u8 *l_buf = NULL;
	struct msdc_host *host_ctl;
	struct mmc_host *mmc;
	int ret = 0;
	int bootpart = 0;

	host_ctl = mtk_msdc_host[msdc_ctl->host_num];

	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice("host_ctl or mmc or card is NULL\n");
		return -EINVAL;
	}
	mmc = host_ctl->mmc;

	if (msdc_ctl->buffer == NULL)
		return -EINVAL;

	mmc_get_card(mmc->card);

	MMC_IOCTL_PR_DBG("user want set boot partition in msdc slot%d\n",
		msdc_ctl->host_num);

	ret = mmc_get_ext_csd(mmc->card, &l_buf);
	if (ret) {
		pr_debug("mmc_get_ext_csd error, set boot partition\n");
		goto end;
	}

	if (copy_from_user(&bootpart, msdc_ctl->buffer, 1)) {
		ret = -EFAULT;
		goto end;
	}

	if ((bootpart != EMMC_BOOT1_EN)
	 && (bootpart != EMMC_BOOT2_EN)
	 && (bootpart != EMMC_BOOT_USER)) {
		pr_debug("set boot partition error, not support %d\n",
			bootpart);
		ret = -EFAULT;
		goto end;
	}

	if (((l_buf[EXT_CSD_PART_CFG] & 0x38) >> 3) != bootpart) {
		/* active boot partition */
		l_buf[EXT_CSD_PART_CFG] &= ~0x38;
		l_buf[EXT_CSD_PART_CFG] |= (bootpart << 3);
		pr_debug("mmc_switch set %x\n", l_buf[EXT_CSD_PART_CFG]);
		ret = mmc_switch(mmc->card, 0, EXT_CSD_PART_CFG,
			l_buf[EXT_CSD_PART_CFG], 1000);
		if (ret) {
			pr_debug("mmc_switch error, set boot partition\n");
		} else {
			mmc->card->ext_csd.part_config =
				l_buf[EXT_CSD_PART_CFG];
		}
	}

end:
	msdc_ctl->result = ret;

	mmc_put_card(mmc->card);

	kfree(l_buf);
	return ret;
}

int simple_sd_ioctl_rw(struct msdc_ioctl *msdc_ctl)
{
	struct scatterlist msdc_sg;
	struct mmc_data msdc_data = { 0 };
	struct mmc_command msdc_cmd = { 0 };
	struct mmc_command msdc_stop;
	int ret = 0;
	char part_id;
	int no_single_rw;
	u32 total_size;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int is_cmdq_en = false;
#endif

#ifdef MTK_MSDC_USE_CMD23
	struct mmc_command msdc_sbc;
#endif

	struct mmc_request msdc_mrq = { 0 };
	struct msdc_host *host_ctl;
	struct mmc_host *mmc;

	if (!msdc_ctl)
		return -EINVAL;

	host_ctl = mtk_msdc_host[msdc_ctl->host_num];

	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice("host_ctl or mmc or card is NULL\n");
		return -EINVAL;
	}
	mmc = host_ctl->mmc;

	if ((msdc_ctl->total_size <= 0) ||
	    (msdc_ctl->total_size > host_ctl->mmc->max_seg_size) ||
		(msdc_ctl->total_size > SG_MSDC_MULTI_BUFFER_SIZE))
		return -EINVAL;
	total_size = msdc_ctl->total_size;

	if (msdc_ctl->total_size > 512)
		no_single_rw = 1;
	else
		no_single_rw = 0;

#ifdef MTK_MSDC_USE_CACHE
	if (msdc_ctl->iswrite && mmc_card_mmc(mmc->card)
	 && (mmc->card->ext_csd.cache_ctrl & 0x1))
		no_single_rw = 1;
#endif
	if (msdc_ctl->iswrite) {
		if (msdc_ctl->opcode != MSDC_CARD_DUNM_FUNC) {
			if (copy_from_user(sg_msdc_multi_buffer,
				msdc_ctl->buffer, total_size)) {
				dma_force[host_ctl->id] = FORCE_NOTHING;
				ret = -EFAULT;
				goto rw_end_without_release;
			}
		} else {
			/* called from other kernel module */
			memcpy(sg_msdc_multi_buffer, msdc_ctl->buffer,
				total_size);
		}
	} else {
		memset(sg_msdc_multi_buffer, 0, total_size);
	}
	mmc_get_card(mmc->card);

	MMC_IOCTL_PR_DBG("user want access %d partition\n",
		msdc_ctl->partition);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc->card->ext_csd.cmdq_en) {
		/* cmdq enabled, turn it off first */
		pr_debug("[MSDC_DBG] cmdq enabled, turn it off\n");
		ret = mmc_cmdq_disable(mmc->card);
		if (ret) {
			pr_debug("[MSDC_DBG] turn off cmdq en failed\n");
			goto rw_end;
		} else
			is_cmdq_en = true;
	}
#endif

	if (host_ctl->hw->host_function == MSDC_EMMC) {
		switch (msdc_ctl->partition) {
		case EMMC_PART_BOOT1:
			part_id = 1;
			break;
		case EMMC_PART_BOOT2:
			part_id = 2;
			break;
		default:
			/* make sure access partition is user data area */
			part_id = 0;
			break;
		}

		if (msdc_switch_part(host_ctl, part_id))
			goto rw_end;
	}

	if (no_single_rw) {
		memset(&msdc_stop, 0, sizeof(struct mmc_command));

#ifdef MTK_MSDC_USE_CMD23
		memset(&msdc_sbc, 0, sizeof(struct mmc_command));
#endif
	}

	msdc_mrq.cmd = &msdc_cmd;
	msdc_mrq.data = &msdc_data;

	if (msdc_ctl->trans_type)
		dma_force[host_ctl->id] = FORCE_IN_DMA;
	else
		dma_force[host_ctl->id] = FORCE_IN_PIO;

	if (msdc_ctl->iswrite) {
		msdc_data.flags = MMC_DATA_WRITE;
		if (no_single_rw)
			msdc_cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		else
			msdc_cmd.opcode = MMC_WRITE_BLOCK;
		msdc_data.blocks = total_size / 512;
	} else {
		msdc_data.flags = MMC_DATA_READ;
		if (no_single_rw)
			msdc_cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
		else
			msdc_cmd.opcode = MMC_READ_SINGLE_BLOCK;
		msdc_data.blocks = total_size / 512;
		memset(sg_msdc_multi_buffer, 0, total_size);
	}

#ifdef MTK_MSDC_USE_CMD23
	if (no_single_rw == 0)
		goto skip_sbc_prepare;

	if ((mmc_card_mmc(mmc->card) || (mmc_card_sd(mmc->card)
	 && mmc->card->scr.cmds & SD_SCR_CMD23_SUPPORT))
	 && !(mmc->card->quirks & MMC_QUIRK_BLK_NO_CMD23)) {
		msdc_mrq.sbc = &msdc_sbc;
		msdc_mrq.sbc->opcode = MMC_SET_BLOCK_COUNT;
#ifdef MTK_MSDC_USE_CACHE
		/* if ioctl access cacheable partition data,
		 * there is on flush mechanism in msdc driver
		 * so do reliable write .
		 */
		if (mmc_card_mmc(mmc->card)
		 && (mmc->card->ext_csd.cache_ctrl & 0x1)
		 && (msdc_cmd.opcode == MMC_WRITE_MULTIPLE_BLOCK))
			msdc_mrq.sbc->arg = msdc_data.blocks | (1 << 31);
		else
			msdc_mrq.sbc->arg = msdc_data.blocks;
#else
		msdc_mrq.sbc->arg = msdc_data.blocks;
#endif
		msdc_mrq.sbc->flags = MMC_RSP_R1 | MMC_CMD_AC;
	}
skip_sbc_prepare:
#endif

	msdc_cmd.arg = msdc_ctl->address;

	if (!mmc_card_blockaddr(mmc->card)) {
		pr_debug("this device use byte address!!\n");
		msdc_cmd.arg <<= 9;
	}
	msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	if (no_single_rw) {
		msdc_stop.opcode = MMC_STOP_TRANSMISSION;
		msdc_stop.arg = 0;
		msdc_stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

		msdc_data.stop = &msdc_stop;
	} else {
		msdc_data.stop = NULL;
	}
	msdc_data.blksz = 512;
	msdc_data.sg = &msdc_sg;
	msdc_data.sg_len = 1;

	MMC_IOCTL_PR_DBG("total size is %d\n", total_size);
	MMC_IOCTL_PR_DBG("ueser buf address is 0x%p!\n", msdc_ctl->buffer);

	sg_init_one(&msdc_sg, sg_msdc_multi_buffer, total_size);
	mmc_set_data_timeout(&msdc_data, mmc->card);
	mmc_wait_for_req(mmc, &msdc_mrq);

	if (msdc_ctl->partition)
		msdc_switch_part(host_ctl, 0);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (is_cmdq_en) {
		pr_debug("[MSDC_DBG] turn on cmdq\n");
		ret = mmc_cmdq_enable(host_ctl->mmc->card);
		if (ret)
			pr_debug("[MSDC_DBG] turn on cmdq en failed\n");
		else
			is_cmdq_en = false;
	}
#endif

	mmc_put_card(mmc->card);
	if (!msdc_ctl->iswrite) {
		if (msdc_ctl->opcode != MSDC_CARD_DUNM_FUNC) {
			if (copy_to_user(msdc_ctl->buffer, sg_msdc_multi_buffer,
				total_size)) {
				dma_force[host_ctl->id] = FORCE_NOTHING;
				ret = -EFAULT;
				goto rw_end_without_release;
			}
		} else {
			/* called from other kernel module */
			memcpy(msdc_ctl->buffer, sg_msdc_multi_buffer,
				total_size);
		}
	}
	/* clear the global buffer of R/W IOCTL */
	memset(sg_msdc_multi_buffer, 0, total_size);
	goto rw_end_without_release;

rw_end:

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (is_cmdq_en) {
		pr_debug("[MSDC_DBG] turn on cmdq\n");
		ret = mmc_cmdq_enable(mmc->card);
		if (ret)
			pr_debug("[MSDC_DBG] turn on cmdq en failed\n");
		else
			is_cmdq_en = false;
	}
#endif
	mmc_put_card(mmc->card);

rw_end_without_release:
	if (ret)
		msdc_ctl->result = ret;

	if (msdc_cmd.error)
		msdc_ctl->result = msdc_cmd.error;

	if (msdc_data.error)
		msdc_ctl->result = msdc_data.error;
	else
		msdc_ctl->result = 0;

	dma_force[host_ctl->id] = FORCE_NOTHING;
	return msdc_ctl->result;

}

#ifdef CONFIG_PWR_LOSS_MTK_TEST
static int sd_ioctl_reinit(struct msdc_ioctl *msdc_ctl)
{
	struct msdc_host *host = mtk_msdc_host[1];

	if (host != NULL)
		return msdc_reinit(host);
	else
		return -EINVAL;
}

static int sd_ioctl_cd_pin_en(struct msdc_ioctl	*msdc_ctl)
{
	struct msdc_host *host = mtk_msdc_host[1];

	if (host != NULL)
		return (host->mmc->caps & MMC_CAP_NONREMOVABLE)
			== MMC_CAP_NONREMOVABLE;
	else
		return -EINVAL;
}

static int simple_sd_ioctl_get_csd(struct msdc_ioctl *msdc_ctl)
{
	struct msdc_host *host_ctl;

	if (!msdc_ctl)
		return -EINVAL;

	host_ctl = mtk_msdc_host[msdc_ctl->host_num];

	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice("host_ctl or mmc or card is NULL\n");
		return -EINVAL;
	}

	MMC_IOCTL_PR_DBG("user want the csd in msdc slot%d\n",
		msdc_ctl->host_num);

	if (copy_to_user(msdc_ctl->buffer, &host_ctl->mmc->card->raw_csd, 16))
		return -EFAULT;

	MMC_IOCTL_PR_DBG("csd:0x%x,0x%x,0x%x,0x%x\n",
		 host_ctl->mmc->card->raw_csd[0],
		 host_ctl->mmc->card->raw_csd[1],
		 host_ctl->mmc->card->raw_csd[2],
		 host_ctl->mmc->card->raw_csd[3]);

	return 0;

}

static int simple_sd_ioctl_get_bootpart(struct msdc_ioctl *msdc_ctl)
{
	u8 *l_buf = NULL;
	struct msdc_host *host_ctl;
	struct mmc_host *mmc;
	int ret = 0;
	int bootpart = 0;
	unsigned int user_buffer;

	host_ctl = mtk_msdc_host[msdc_ctl->host_num];
	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice("host_ctl or mmc or card is NULL\n");
		return -EINVAL;
	}
	mmc = host_ctl->mmc;

	if (get_user(user_buffer, msdc_ctl->buffer))
		return -EINVAL;

	mmc_get_card(mmc->card);

	MMC_IOCTL_PR_DBG("user want get boot partition info in msdc slot%d\n",
		msdc_ctl->host_num);

	ret = mmc_get_ext_csd(mmc->card, &l_buf);
	if (ret) {
		pr_debug("mmc_get_ext_csd error, get boot part\n");
		goto end;
	}
	bootpart = (l_buf[EXT_CSD_PART_CFG] & 0x38) >> 3;

	MMC_IOCTL_PR_DBG("bootpart Byte[EXT_CSD_PART_CFG] =%x, booten=%x\n",
		l_buf[EXT_CSD_PART_CFG], bootpart);

	if (msdc_ctl->opcode != MSDC_CARD_DUNM_FUNC) {
		if (copy_to_user(msdc_ctl->buffer, &bootpart, 1)) {
			ret = -EFAULT;
			goto end;
		}
	} else {
		/* called from other kernel module */
		memcpy(msdc_ctl->buffer, &bootpart, 1);
	}

end:
	msdc_ctl->result = ret;

	mmc_put_card(mmc->card);

	kfree(l_buf);

	return ret;
}

static int simple_sd_ioctl_get_partition_size(struct msdc_ioctl *msdc_ctl)
{
	int ret = 0;
	struct msdc_host *host_ctl;
	unsigned long long partitionsize = 0;
	struct mmc_host *mmc;

	host_ctl = mtk_msdc_host[msdc_ctl->host_num];

	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice("host_ctl or mmc or card is NULL\n");
		return -EINVAL;
	}

	mmc = host_ctl->mmc;

	mmc_get_card(mmc->card);

	MMC_IOCTL_PR_DBG("get size of partition=%d\n", msdc_ctl->partition);

	switch (msdc_ctl->partition) {
	case EMMC_PART_BOOT1:
		partitionsize = msdc_get_other_capacity(host_ctl, "boot0");
		break;
	case EMMC_PART_BOOT2:
		partitionsize = msdc_get_other_capacity(host_ctl, "boot1");
		break;
	case EMMC_PART_RPMB:
		partitionsize = msdc_get_other_capacity(host_ctl, "rpmb");
		break;
	case EMMC_PART_USER:
		partitionsize = msdc_get_user_capacity(host_ctl);
		break;
	default:
		pr_debug("not support partition =%d\n", msdc_ctl->partition);
		partitionsize = 0;
		break;
	}

	MMC_IOCTL_PR_DBG("bootpart partitionsize =%llx\n", partitionsize);

	if (copy_to_user(msdc_ctl->buffer, &partitionsize, 8))
		ret = -EFAULT;

	msdc_ctl->result = ret;

	mmc_put_card(mmc->card);

	return ret;
}

static int simple_sd_ioctl_set_driving(struct msdc_ioctl *msdc_ctl)
{
	void __iomem *base;
	struct msdc_host *host;

	/* cannot access ioctl except of Engineer Mode */
	if (strcmp(current->comm, "em_svr"))
		return -EINVAL;

	host = mtk_msdc_host[msdc_ctl->host_num];
	if (host == NULL)
		return -EINVAL;

	base = host->base;

	msdc_clk_enable(host);

	MMC_IOCTL_PR_DBG("set: clk driving is 0x%x\n",
		msdc_ctl->clk_pu_driving);
	MMC_IOCTL_PR_DBG("set: cmd driving is 0x%x\n",
		msdc_ctl->cmd_pu_driving);
	MMC_IOCTL_PR_DBG("set: dat driving is 0x%x\n",
		msdc_ctl->dat_pu_driving);
	MMC_IOCTL_PR_DBG("set: rst driving is 0x%x\n",
		msdc_ctl->rst_pu_driving);
	MMC_IOCTL_PR_DBG("set: ds driving is 0x%x\n",
		msdc_ctl->ds_pu_driving);

	host->hw->driving_applied->clk_drv = msdc_ctl->clk_pu_driving;
	host->hw->driving_applied->cmd_drv = msdc_ctl->cmd_pu_driving;
	host->hw->driving_applied->dat_drv = msdc_ctl->dat_pu_driving;
	host->hw->driving_applied->rst_drv = msdc_ctl->rst_pu_driving;
	host->hw->driving_applied->ds_drv = msdc_ctl->ds_pu_driving;
	msdc_set_driving(host, host->hw->driving_applied);

	msdc_clk_disable(host);

	return 0;
}

static int simple_sd_ioctl_get_driving(struct msdc_ioctl *msdc_ctl)
{
	void __iomem *base;
	struct msdc_host *host;
	struct msdc_hw_driving driving = {0};

	host = mtk_msdc_host[msdc_ctl->host_num];
	if (host == NULL)
		return -EINVAL;

	base = host->base;

	msdc_clk_enable(host);
	msdc_get_driving(host, &driving);

	msdc_ctl->clk_pu_driving = driving.clk_drv;
	msdc_ctl->cmd_pu_driving = driving.cmd_drv;
	msdc_ctl->dat_pu_driving = driving.dat_drv;

	if (host->id == 0) {
		msdc_ctl->rst_pu_driving = driving.rst_drv;
		msdc_ctl->ds_pu_driving = driving.ds_drv;
	} else {
		msdc_ctl->rst_pu_driving = 0;
		msdc_ctl->ds_pu_driving = 0;
	}

	MMC_IOCTL_PR_DBG("read: clk driving is 0x%x\n",
		msdc_ctl->clk_pu_driving);
	MMC_IOCTL_PR_DBG("read: cmd driving is 0x%x\n",
		msdc_ctl->cmd_pu_driving);
	MMC_IOCTL_PR_DBG("read: dat driving is 0x%x\n",
		msdc_ctl->dat_pu_driving);
	MMC_IOCTL_PR_DBG("read: rst driving is 0x%x\n",
		msdc_ctl->rst_pu_driving);
	MMC_IOCTL_PR_DBG("read: ds driving is 0x%x\n",
		msdc_ctl->ds_pu_driving);
	msdc_clk_disable(host);

	return 0;
}

/*  to ensure format operate is clean the emmc device fully(partition erase) */
#define MBR_PART_NUM            6
#define __MMC_ERASE_ARG         0x00000000
#define __MMC_TRIM_ARG          0x00000001
#define __MMC_DISCARD_ARG       0x00000003

/* call mmc block layer interface for userspace to do erase operate */
static int simple_mmc_erase_func(unsigned int start, unsigned int size)
{
	struct msdc_host *host;
	struct mmc_host *mmc;
	unsigned int arg;

	host = mtk_msdc_host[0];
	if (!host || !host->mmc || !host->mmc->card) {
		pr_notice("host or mmc or card is NULL\n");
		return -EINVAL;
	}
	mmc = host->mmc;

	mmc_get_card(mmc->card);

	if (mmc_can_discard(mmc->card)) {
		arg = __MMC_DISCARD_ARG;
	} else if (mmc_can_trim(mmc->card)) {
		arg = __MMC_TRIM_ARG;
	} else if (mmc_can_erase(mmc->card)) {
		arg = __MMC_ERASE_ARG;
	} else {
		pr_notice("[%s]: emmc card can't support trim / discard / erase\n",
			__func__);
		goto end;
	}

	msdc_switch_part(host, 0);

	pr_debug("[%s]: start=0x%x, size=%d, arg=0x%x, can_trim=(0x%x), EXT_CSD_SEC_GB_CL_EN=0x%lx\n",
		__func__, start, size, arg,
		mmc->card->ext_csd.sec_feature_support,
		EXT_CSD_SEC_GB_CL_EN);
	mmc_erase(mmc->card, start, size, arg);

	MMC_IOCTL_PR_DBG("[%s]: erase done....arg=0x%x\n", __func__, arg);

end:
	mmc_put_card(mmc->card);

	return 0;
}

#ifdef CONFIG_PWR_LOSS_MTK_TEST
/* These definitiona and functions are coded by reference to
 * mmc_blk_issue_discard_rq()@block.c
 */
#define INAND_CMD38_ARG_EXT_CSD  113
#define INAND_CMD38_ARG_ERASE    0x00
#define INAND_CMD38_ARG_TRIM     0x01
#define INAND_CMD38_ARG_SECERASE 0x80
#define INAND_CMD38_ARG_SECTRIM1 0x81
#define INAND_CMD38_ARG_SECTRIM2 0x88
static int simple_sd_ioctl_erase_selected_area(struct msdc_ioctl *msdc_ctl)
{
	struct msdc_host *host_ctl;
	struct mmc_host *mmc;
	unsigned int from, nr, arg;
	int err = 0;

	host_ctl = mtk_msdc_host[msdc_ctl->host_num];
	if (!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card) {
		pr_notice("host_ctl or mmc or card is NULL\n");
		return -EINVAL;
	}

	mmc = host_ctl->mmc;

	mmc_get_card(mmc->card);

	msdc_switch_part(host_ctl, 0);

	if (!mmc_can_erase(mmc->card)) {
		err = -EOPNOTSUPP;
		goto out;
	}

	from = msdc_ctl->address;
	nr = msdc_ctl->total_size;

	if (mmc_can_discard(mmc->card))
		arg = MMC_DISCARD_ARG;
	else if (mmc_can_trim(mmc->card))
		arg = MMC_TRIM_ARG;
	else
		arg = MMC_ERASE_ARG;

	MMC_IOCTL_PR_DBG("Erase range %x~%x\n", from, from + nr - 1);

	if (mmc_card_mmc(mmc->card)) {
		if (mmc->card->quirks & MMC_QUIRK_INAND_CMD38) {
			err = mmc_switch(mmc->card, EXT_CSD_CMD_SET_NORMAL,
				 INAND_CMD38_ARG_EXT_CSD,
				 arg == MMC_TRIM_ARG ?
				 INAND_CMD38_ARG_TRIM :
				 INAND_CMD38_ARG_ERASE,
				 0);
			if (err)
				goto out;
		}
	}

	err = mmc_erase(mmc->card, from, nr, arg);
out:

	mmc_put_card(mmc->card);

	msdc_ctl->result = err;

	return msdc_ctl->result;

}
#endif

static int simple_mmc_erase_partition(unsigned char *name)
{
	struct hd_struct part = {0};

	/* just support erase cache & data partition now */
	if (name &&
	    (strncmp(name, "usrdata", 7) == 0 ||
	     strncmp(name, "cache", 5) == 0)) {
		/* find erase start address and erase size,
		 * just support high capacity emmc card now
		 */

		if (msdc_get_part_info(name, &part)) {
			pr_debug("erase %s, start sector: 0x%x, size: 0x%x\n",
				name, (u32)part.start_sect, (u32)part.nr_sects);
			simple_mmc_erase_func(part.start_sect, part.nr_sects);
		}
	}

	return 0;

}

static int simple_mmc_erase_partition_wrap(struct msdc_ioctl *msdc_ctl)
{
	unsigned char name[PARTITION_NAME_LENGTH];

	if (!msdc_ctl)
		return -EINVAL;

	if (msdc_ctl->total_size >= PARTITION_NAME_LENGTH)
		return -EFAULT;

	if (copy_from_user(name, (unsigned char *)msdc_ctl->buffer,
		msdc_ctl->total_size))
		return -EFAULT;
	name[msdc_ctl->total_size] = 0;

	return simple_mmc_erase_partition(name);
}
#endif

static long simple_sd_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct msdc_ioctl *msdc_ctl;
#ifdef CONFIG_PWR_LOSS_MTK_TEST
	struct msdc_host *host;
#endif
	int ret = 0;

	if ((struct msdc_ioctl *)arg == NULL) {
		switch (cmd) {
#ifdef CONFIG_PWR_LOSS_MTK_TEST
		case MSDC_REINIT_SDCARD:
			pr_info("sd ioctl re-init!!\n");
			ret = sd_ioctl_reinit((struct msdc_ioctl *)arg);
			break;

		case MSDC_CD_PIN_EN_SDCARD:
			pr_info("sd ioctl cd pin\n");
			ret = sd_ioctl_cd_pin_en((struct msdc_ioctl *)arg);
			break;

		case MSDC_SD_POWER_OFF:
			pr_info("sd ioctl power off!!!\n");
			host = mtk_msdc_host[1];
			if (host && host->mmc) {
				mmc_claim_host(host->mmc);
				mmc_power_off(host->mmc);
				mmc_release_host(host->mmc);
			}
			break;

		case MSDC_SD_POWER_ON:
			pr_info("sd ioctl power on!!!\n");
			host = mtk_msdc_host[1];
			/* FIX ME: kernel 3.18 does not provide
			 * mmc_resume_host,
			 * check if mmc_power_restore_host can be used
			 */
			/* ret = mmc_resume_host(host->mmc); */
			/* ret = mmc_power_restore_host(host->mmc); */
			break;
#endif
		default:
			pr_notice("mt_sd_ioctl:this opcode value is illegal!!\n");
			return -EINVAL;
		}
		return ret;
	}

	msdc_ctl = kmalloc(sizeof(*msdc_ctl), GFP_KERNEL);
	if (!msdc_ctl)
		return -ENOMEM;

	if (copy_from_user(msdc_ctl, (struct msdc_ioctl *)arg,
		sizeof(struct msdc_ioctl))) {
		kfree(msdc_ctl);
		return -EFAULT;
	}

	if (msdc_ctl->opcode != MSDC_ERASE_PARTITION) {
		if ((msdc_ctl->host_num < 0)
		 || (msdc_ctl->host_num >= HOST_MAX_NUM)) {
			pr_notice("invalid host num: %d\n", msdc_ctl->host_num);
			kfree(msdc_ctl);
			return -EINVAL;
		}
	}

	switch (msdc_ctl->opcode) {
	case MSDC_GET_CID:
		msdc_ctl->result = simple_sd_ioctl_get_cid(msdc_ctl);
		break;
	case MSDC_SET_BOOTPART:
		msdc_ctl->result =
			simple_sd_ioctl_set_bootpart(msdc_ctl);
		break;
#ifdef CONFIG_PWR_LOSS_MTK_TEST
	case MSDC_SINGLE_READ_WRITE:
	case MSDC_MULTIPLE_READ_WRITE:
		msdc_ctl->result = simple_sd_ioctl_rw(msdc_ctl);
		break;
	case MSDC_GET_CSD:
		msdc_ctl->result = simple_sd_ioctl_get_csd(msdc_ctl);
		break;
	case MSDC_DRIVING_SETTING:
		if (msdc_ctl->iswrite == 1) {
			msdc_ctl->result =
				simple_sd_ioctl_set_driving(msdc_ctl);
		} else {
			msdc_ctl->result =
				simple_sd_ioctl_get_driving(msdc_ctl);
		}
		break;
	case MSDC_ERASE_PARTITION:
		/* Used by ftp_emmc.c of factory and roots.cpp of recovery */
		msdc_ctl->result =
			simple_mmc_erase_partition_wrap(msdc_ctl);
		break;
#ifdef CONFIG_PWR_LOSS_MTK_TEST
	case MSDC_ERASE_SELECTED_AREA:
		msdc_ctl->result = simple_sd_ioctl_erase_selected_area(
			msdc_ctl);
		break;
#endif
	case MSDC_SD30_MODE_SWITCH:
		pr_notice("obsolete opcode!!\n");
		kfree(msdc_ctl);
		return -EINVAL;
	case MSDC_GET_BOOTPART:
		msdc_ctl->result =
			simple_sd_ioctl_get_bootpart(msdc_ctl);
		break;
	case MSDC_GET_PARTSIZE:
		msdc_ctl->result =
			simple_sd_ioctl_get_partition_size(msdc_ctl);
		break;
#endif
	default:
		pr_notice("%s:invlalid opcode!!\n", __func__);
		kfree(msdc_ctl);
		return -EINVAL;
	}

	if (copy_to_user((struct msdc_ioctl *)arg, msdc_ctl,
		sizeof(struct msdc_ioctl))) {
		kfree(msdc_ctl);
		return -EFAULT;
	}

	ret = msdc_ctl->result;
	kfree(msdc_ctl);
	return ret;
}

#ifdef CONFIG_COMPAT

struct compat_simple_sd_ioctl {
	compat_int_t opcode;
	compat_int_t host_num;
	compat_int_t iswrite;
	compat_int_t trans_type;
	compat_uint_t total_size;
	compat_uint_t address;
	compat_uptr_t buffer;
	compat_int_t cmd_pu_driving;
	compat_int_t cmd_pd_driving;
	compat_int_t dat_pu_driving;
	compat_int_t dat_pd_driving;
	compat_int_t clk_pu_driving;
	compat_int_t clk_pd_driving;
	compat_int_t ds_pu_driving;
	compat_int_t ds_pd_driving;
	compat_int_t rst_pu_driving;
	compat_int_t rst_pd_driving;
	compat_int_t clock_freq;
	compat_int_t partition;
	compat_int_t hopping_bit;
	compat_int_t hopping_time;
	compat_int_t result;
	compat_int_t sd30_mode;
	compat_int_t sd30_max_current;
	compat_int_t sd30_drive;
	compat_int_t sd30_power_control;
};

static int compat_get_simple_ion_allocation(
	struct compat_simple_sd_ioctl __user *arg32,
	struct msdc_ioctl __user *arg64)
{
	compat_int_t i;
	compat_uint_t u;
	compat_uptr_t p;
	int err;

	err = get_user(i, &(arg32->opcode));
	err |= put_user(i, &(arg64->opcode));
	err |= get_user(i, &(arg32->host_num));
	err |= put_user(i, &(arg64->host_num));
	err |= get_user(i, &(arg32->iswrite));
	err |= put_user(i, &(arg64->iswrite));
	err |= get_user(i, &(arg32->trans_type));
	err |= put_user(i, &(arg64->trans_type));
	err |= get_user(u, &(arg32->total_size));
	err |= put_user(u, &(arg64->total_size));
	err |= get_user(u, &(arg32->address));
	err |= put_user(u, &(arg64->address));
	err |= get_user(p, &(arg32->buffer));
	err |= put_user(compat_ptr(p), &(arg64->buffer));
	err |= get_user(i, &(arg32->cmd_pu_driving));
	err |= put_user(i, &(arg64->cmd_pu_driving));
	err |= get_user(i, &(arg32->cmd_pd_driving));
	err |= put_user(i, &(arg64->cmd_pd_driving));
	err |= get_user(i, &(arg32->dat_pu_driving));
	err |= put_user(i, &(arg64->dat_pu_driving));
	err |= get_user(i, &(arg32->dat_pd_driving));
	err |= put_user(i, &(arg64->dat_pd_driving));
	err |= get_user(i, &(arg32->clk_pu_driving));
	err |= put_user(i, &(arg64->clk_pu_driving));
	err |= get_user(i, &(arg32->clk_pd_driving));
	err |= put_user(i, &(arg64->clk_pd_driving));
	err |= get_user(i, &(arg32->ds_pu_driving));
	err |= put_user(i, &(arg64->ds_pu_driving));
	err |= get_user(i, &(arg32->ds_pd_driving));
	err |= put_user(i, &(arg64->ds_pd_driving));
	err |= get_user(i, &(arg32->rst_pu_driving));
	err |= put_user(i, &(arg64->rst_pu_driving));
	err |= get_user(i, &(arg32->rst_pd_driving));
	err |= put_user(i, &(arg64->rst_pd_driving));
	err |= get_user(i, &(arg32->clock_freq));
	err |= put_user(i, &(arg64->clock_freq));
	err |= get_user(i, &(arg32->partition));
	err |= put_user(i, &(arg64->partition));
	err |= get_user(i, &(arg32->hopping_bit));
	err |= put_user(i, &(arg64->hopping_bit));
	err |= get_user(i, &(arg32->hopping_time));
	err |= put_user(i, &(arg64->hopping_time));
	err |= get_user(i, &(arg32->result));
	err |= put_user(i, &(arg64->result));
	err |= get_user(i, &(arg32->sd30_mode));
	err |= put_user(i, &(arg64->sd30_mode));
	err |= get_user(i, &(arg32->sd30_max_current));
	err |= put_user(i, &(arg64->sd30_max_current));
	err |= get_user(i, &(arg32->sd30_drive));
	err |= put_user(i, &(arg64->sd30_drive));
	err |= get_user(i, &(arg32->sd30_power_control));
	err |= put_user(i, &(arg64->sd30_power_control));

	return err;
}

static int compat_put_simple_ion_allocation(
	struct compat_simple_sd_ioctl __user *arg32,
	struct msdc_ioctl __user *arg64)
{
	compat_int_t i;
	compat_uint_t u;
	int err;

	err = get_user(i, &(arg64->opcode));
	err |= put_user(i, &(arg32->opcode));
	err |= get_user(i, &(arg64->host_num));
	err |= put_user(i, &(arg32->host_num));
	err |= get_user(i, &(arg64->iswrite));
	err |= put_user(i, &(arg32->iswrite));
	err |= get_user(i, &(arg64->trans_type));
	err |= put_user(i, &(arg32->trans_type));
	err |= get_user(u, &(arg64->total_size));
	err |= put_user(u, &(arg32->total_size));
	err |= get_user(u, &(arg64->address));
	err |= put_user(u, &(arg32->address));
	err |= get_user(i, &(arg64->cmd_pu_driving));
	err |= put_user(i, &(arg32->cmd_pu_driving));
	err |= get_user(i, &(arg64->cmd_pd_driving));
	err |= put_user(i, &(arg32->cmd_pd_driving));
	err |= get_user(i, &(arg64->dat_pu_driving));
	err |= put_user(i, &(arg32->dat_pu_driving));
	err |= get_user(i, &(arg64->dat_pd_driving));
	err |= put_user(i, &(arg32->dat_pd_driving));
	err |= get_user(i, &(arg64->clk_pu_driving));
	err |= put_user(i, &(arg32->clk_pu_driving));
	err |= get_user(i, &(arg64->clk_pd_driving));
	err |= put_user(i, &(arg32->clk_pd_driving));
	err |= get_user(i, &(arg64->ds_pu_driving));
	err |= put_user(i, &(arg32->ds_pu_driving));
	err |= get_user(i, &(arg64->ds_pd_driving));
	err |= put_user(i, &(arg32->ds_pd_driving));
	err |= get_user(i, &(arg64->rst_pu_driving));
	err |= put_user(i, &(arg32->rst_pu_driving));
	err |= get_user(i, &(arg64->rst_pd_driving));
	err |= put_user(i, &(arg32->rst_pd_driving));
	err |= get_user(i, &(arg64->clock_freq));
	err |= put_user(i, &(arg32->clock_freq));
	err |= get_user(i, &(arg64->partition));
	err |= put_user(i, &(arg32->partition));
	err |= get_user(i, &(arg64->hopping_bit));
	err |= put_user(i, &(arg32->hopping_bit));
	err |= get_user(i, &(arg64->hopping_time));
	err |= put_user(i, &(arg32->hopping_time));
	err |= get_user(i, &(arg64->result));
	err |= put_user(i, &(arg32->result));
	err |= get_user(i, &(arg64->sd30_mode));
	err |= put_user(i, &(arg32->sd30_mode));
	err |= get_user(i, &(arg64->sd30_max_current));
	err |= put_user(i, &(arg32->sd30_max_current));
	err |= get_user(i, &(arg64->sd30_drive));
	err |= put_user(i, &(arg32->sd30_drive));
	err |= get_user(i, &(arg64->sd30_power_control));
	err |= put_user(i, &(arg32->sd30_power_control));

	return err;
}


static long simple_sd_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct compat_simple_sd_ioctl *arg32;
	struct msdc_ioctl *arg64;
	compat_int_t k_opcode;
	int err, ret;

	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		pr_notice("f_op or unlocked ioctl is NULL.\n");
		return -ENOTTY;
	}

	if ((struct msdc_ioctl *)arg == NULL) {
		ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)arg);
		return ret;
	}

	arg32 = compat_ptr(arg);
	arg64 = compat_alloc_user_space(sizeof(*arg64));
	if (arg64 == NULL)
		return -EFAULT;

	if (!access_ok(VERIFY_WRITE, arg32, sizeof(*arg32)) ||
	     !access_ok(VERIFY_WRITE, arg64, sizeof(*arg64))) {
		return -EFAULT;
	}

	err = compat_get_simple_ion_allocation(arg32, arg64);
	if (err)
		return err;
	err = get_user(k_opcode, &(arg64->opcode));
	if (err)
		return err;

	ret = file->f_op->unlocked_ioctl(file, (unsigned int)k_opcode,
		(unsigned long)arg64);

	err = compat_put_simple_ion_allocation(arg32, arg64);

	return ret ? ret : err;
}

#endif

static const struct file_operations simple_msdc_em_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = simple_sd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = simple_sd_compat_ioctl,
#endif
	.open = simple_sd_open,
};

static struct miscdevice simple_msdc_em_dev[] = {
	{
	 .minor = MISC_DYNAMIC_MINOR,
	 .name = "misc-sd",
	 .fops = &simple_msdc_em_fops,
	}
};

static int simple_sd_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("in misc_sd_probe function\n");

	return ret;
}

static int simple_sd_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver simple_sd_driver = {
	.probe = simple_sd_probe,
	.remove = simple_sd_remove,

	.driver = {
		.name = DRV_NAME_MISC,
		.owner = THIS_MODULE,
	},
};

static int __init simple_sd_init(void)
{
	int ret;

	sg_msdc_multi_buffer = kmalloc(SG_MSDC_MULTI_BUFFER_SIZE, GFP_KERNEL);
	if (sg_msdc_multi_buffer == NULL)
		return 0;

	ret = platform_driver_register(&simple_sd_driver);
	if (ret) {
		pr_notice(DRV_NAME_MISC ": Can't register driver\n");
		return ret;
	}
	pr_debug(DRV_NAME_MISC ": MediaTek simple SD/MMC Card Driver\n");

	/*msdc0 is for emmc only, just for emmc */
	/* ret = misc_register(&simple_msdc_em_dev[host->id]); */
	ret = misc_register(&simple_msdc_em_dev[0]);
	if (ret) {
		pr_notice("register MSDC Slot[0] misc driver failed (%d)\n",
			ret);
		return ret;
	}

	return 0;
}

static void __exit simple_sd_exit(void)
{
	if (sg_msdc_multi_buffer != NULL) {
		kfree(sg_msdc_multi_buffer);
		sg_msdc_multi_buffer = NULL;
	}

	misc_deregister(&simple_msdc_em_dev[0]);

	platform_driver_unregister(&simple_sd_driver);
}

module_init(simple_sd_init);
module_exit(simple_sd_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("simple MediaTek SD/MMC Card Driver");
MODULE_AUTHOR("feifei.wang <feifei.wang@mediatek.com>");
