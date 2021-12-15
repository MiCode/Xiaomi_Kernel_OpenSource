/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/*****************************************************************************
 *
 * File Name: focaltech_upgrade_ft8736.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-10-25
 *
 * Abstract:
 *
 * Reference:
 *
 *****************************************************************************/
/*****************************************************************************
 * 1.Included header files
 *****************************************************************************/
#include "../focaltech_flash.h"

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/
u8 pb_file_ft8736[] = {
#include "../include/pramboot/FT8736_Pramboot_V0.5_20170526.i"
};

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/
/************************************************************************
 * Name: fts_ft8736_upgrade
 * Brief:
 * Input:
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8736_upgrade(u8 *buf, u32 len)
{
	int ret = 0;
	u32 start_addr = 0;
	u8 cmd[4] = {0};
	u32 delay = 0;
	int ecc_in_host = 0;
	int ecc_in_tp = 0;

	if (buf == NULL) {
		FTS_ERROR("fw buf is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_APP)) {
		FTS_ERROR("fw buffer len(%x) fail", len);
		return -EINVAL;
	}

	/* enter into upgrade environment */
	ret = fts_fwupg_enter_into_boot();
	if (ret < 0) {
		FTS_ERROR("enter into pramboot/bootloader fail,ret=%d", ret);
		goto fw_reset;
	}

	cmd[0] = FTS_CMD_FLASH_MODE;
	cmd[1] = FLASH_MODE_UPGRADE_VALUE;
	ret = fts_write(cmd, 2);
	if (ret < 0) {
		FTS_ERROR("upgrade mode(09) cmd write fail");
		goto fw_reset;
	}

	delay = FTS_ERASE_SECTOR_DELAY * (len / FTS_MAX_LEN_SECTOR);
	ret = fts_fwupg_erase(delay);
	if (ret < 0) {
		FTS_ERROR("erase cmd write fail");
		goto fw_reset;
	}

	/* write app */
	start_addr = upgrade_func_ft8736.appoff;
	ecc_in_host = fts_flash_write_buf(start_addr, buf, len, 1);
	if (ecc_in_host < 0) {
		FTS_ERROR("lcd initial code write fail");
		goto fw_reset;
	}

	/* ecc */
	ecc_in_tp = fts_fwupg_ecc_cal(start_addr, len);
	if (ecc_in_tp < 0) {
		FTS_ERROR("ecc read fail");
		goto fw_reset;
	}

	FTS_INFO("ecc in tp:%x, host:%x", ecc_in_tp, ecc_in_host);
	if (ecc_in_tp != ecc_in_host) {
		FTS_ERROR("ecc check fail");
		goto fw_reset;
	}

	FTS_INFO("upgrade success, reset to normal boot");
	ret = fts_fwupg_reset_in_boot();
	if (ret < 0)
		FTS_ERROR("reset to normal boot fail");


	msleep(400);
	return 0;

fw_reset:
	FTS_INFO("upgrade fail, reset to normal boot");
	ret = fts_fwupg_reset_in_boot();
	if (ret < 0)
		FTS_ERROR("reset to normal boot fail");

	return -EIO;
}

struct upgrade_func upgrade_func_ft8736 = {
	.ctype = {0x06},
	.fwveroff = 0x010E,
	.fwcfgoff = 0x0F80,
	.appoff = 0x1000,
	.pramboot_supported = true,
	.pramboot = pb_file_ft8736,
	.pb_length = sizeof(pb_file_ft8736),
	.hid_supported = false,
	.upgrade = fts_ft8736_upgrade,
};
