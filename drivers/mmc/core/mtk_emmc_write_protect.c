/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

#include <mt-plat/mtk_partition.h>
#include <linux/types.h>
#include "mtk_emmc_write_protect.h"

static char *partition_nametab[] = {
	"system",
	"userdata",
};

static int partition_num = ARRAY_SIZE(partition_nametab);

static int emmc_get_partition_info(char *name, unsigned long long *start,
	unsigned long long *size)
{
	struct hd_struct *part = NULL;

	part = get_part(name);

	if (likely(part)) {
		*start = (unsigned long long)part->start_sect;
		*size = (unsigned long long)part_nr_sects_read(part);
		put_part(part);
		pr_debug("%s: start address: %llu, size =%llu\n", __func__,
			(*start)<<9, (*size)<<9);
	} else {
		pr_notice("%s: There is no %s partition info\n",
			__func__, name);
		return  -1;
	}

	return 0;
}

/* Calculate the wp_grp_size-aligned starting address and total number of
 * wp_grps need set.
 */
static inline void get_fixed_wp_params(unsigned long long *start,
unsigned long long size, unsigned int wp_grp_size, unsigned int *cnt)
{
	unsigned long long end;

	end = *start + size;

	if (*start % wp_grp_size)
		*start = *start + (wp_grp_size - *start % wp_grp_size);

/*To make sure at least one aligned WP_GRP in the partition address range*/
	if ((*start + wp_grp_size) > end) {
		pr_info("%s: partition is too small to set wp!\n", __func__);
		*cnt = 0;
	} else
		*cnt = (end - *start)/wp_grp_size;

}

static int emmc_set_usr_wp(struct mmc_card *card, unsigned char wp)
{
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};

	/* clr usr_wp */
	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_CLEAR_BITS << 24) |
		(EXT_CSD_USR_WP << 16) |
		((EXT_CSD_USR_WP_EN_PERM_WP | EXT_CSD_USR_WP_EN_PWR_WP) << 8) |
		EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &cmd;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_notice("%s: cmd error %d\n", __func__, cmd.error);
		return cmd.error;
		}

	/* set usr_wp*/
	cmd.arg = (MMC_SWITCH_MODE_SET_BITS << 24) |
		(EXT_CSD_USR_WP << 16) | (wp << 8) | EXT_CSD_CMD_SET_NORMAL;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_notice("%s: cmd error %d\n", __func__, cmd.error);
		return cmd.error;
		}

	return 0;
}

/*set both boot0 and boot1 power-on write protect*/
static int set_boot_wp(struct mmc_card *card)
{
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};

	/* clr usr_wp */
	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_CLEAR_BITS << 24) |
		(EXT_CSD_BOOT_WP << 16) |
		(0xff << 8) |
		EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &cmd;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_notice("%s: cmd error %d\n", __func__, cmd.error);
		return cmd.error;
		}

	/* set usr_wp*/
	cmd.arg = (MMC_SWITCH_MODE_SET_BITS << 24) |
		(EXT_CSD_BOOT_WP << 16) |
		(EXT_CSD_BOOT_WP_B_PWR_WP_EN << 8) |
		EXT_CSD_CMD_SET_NORMAL;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_notice("%s: cmd error %d\n", __func__, cmd.error);
		return cmd.error;
		}
	return 0;
}

#define PRELOADER_NAME  "preloader"
static int emmc_set_wp(struct mmc_card *card, char *partition_name)
{
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};
	unsigned long long start;
	unsigned long long size;
	unsigned int wp_grp_size;
	unsigned int cnt;
	unsigned int i;
	int err = 0;

	if (strcmp(partition_name, PRELOADER_NAME) == 0) {
		err = set_boot_wp(card);
		return err;
	}
	err = emmc_get_partition_info(partition_name, &start, &size);
	if (err)
		return err;

	wp_grp_size = card->wp_grp_size; //unit: 512B

    /*
     * If the partition is not aligned in wp_grp_size, discard the unaligned
     * blocks of start part and end part.
     * This means set write protection range must not over partition range.
     */
	get_fixed_wp_params(&start, size, wp_grp_size, &cnt);

	mrq.cmd = &cmd;
	cmd.opcode = MMC_SET_WRITE_PROT;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	for (i = 0; i < cnt; i++) {
		cmd.arg = start + i * wp_grp_size;
		mmc_wait_for_req(card->host, &mrq);
		if (cmd.error) {
			pr_notice("%s: cmd error %d\n", __func__, cmd.error);
			return cmd.error;
		}
	}

	return err;
}

int emmc_set_wp_by_partitions(struct mmc_card *card,
	char *partition_nametab[], int partition_num, unsigned char wp_type)
{
	int index;
	int err;

	err = emmc_set_usr_wp(card, wp_type);
	if (err)
		return err;

	for (index = 0; index < partition_num; index++) {
		err = emmc_set_wp(card, partition_nametab[index]);
		if (err) {
			pr_notice("%s: set partition %s wp is failed!\n",
				__func__, partition_nametab[index]);
			return err;
		}
	}

	return err;
}

int set_power_on_write_protect(struct mmc_card *card)
{
	int err;

	err = emmc_set_wp_by_partitions(card, partition_nametab, partition_num,
			EXT_CSD_USR_WP_EN_PWR_WP);
	return err;
}
