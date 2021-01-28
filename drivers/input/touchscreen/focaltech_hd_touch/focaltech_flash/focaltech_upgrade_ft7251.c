// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 */

/*****************************************************************************
 *
 * File Name: focaltech_upgrade_ft7251.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-08-15
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
u8 pb_file_ft7251[] = {
#include "../include/pramboot/FT7251_Pramboot_V1.0_20180612.i"
};

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/

static int fts_ft7251_upgrade_mode(enum FW_FLASH_MODE mode, u8 *buf, u32 len)
{
	int ret = 0;
	u32 start_addr = 0;
	u8 cmd[4] = {0};
	u32 delay = 0;
	int ecc_in_host = 0;
	int ecc_in_tp = 0;

	if ((buf == NULL) || (len < FTS_MIN_LEN)) {
		FTS_ERROR("buffer/len(%x) is invalid", len);
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
	if (upgrade_func_ft7251.appoff_handle_in_ic)
		start_addr = 0; /* offset handle in pramboot */
	else
		start_addr = upgrade_func_ft7251.appoff;

	if (mode == FLASH_MODE_PARAM)
		cmd[1] = FLASH_MODE_PARAM_VALUE;

	FTS_INFO("flash mode:0x%02x, start addr=0x%04x", cmd[1], start_addr);
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
	return -EIO;
}

/************************************************************************
 * Name: fts_ft7251_upgrade
 * Brief:
 * Input:
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft7251_upgrade(u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u32 app_len = 0;

	FTS_INFO("fw app upgrade...");
	if (buf == NULL) {
		FTS_ERROR("fw buf is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("fw buffer len(%x) fail", len);
		return -EINVAL;
	}

	app_len = len - upgrade_func_ft7251.appoff;
	tmpbuf = buf + upgrade_func_ft7251.appoff;
	ret = fts_ft7251_upgrade_mode(FLASH_MODE_APP, tmpbuf, app_len);
	if (ret < 0) {
		FTS_INFO("fw upgrade fail,reset to normal boot");
		if (fts_fwupg_reset_in_boot() < 0)
			FTS_ERROR("reset to normal boot fail");

		return ret;
	}

	return 0;
}

struct upgrade_func upgrade_func_ft7251 = {
	.ctype = {0x12, 0x13},
	.fwveroff = 0x210E,
	.fwcfgoff = 0x1000,
	.appoff = 0x2000,
	.new_return_value_from_ic = true,
	.appoff_handle_in_ic = true,
	.pramboot_supported = true,
	.pramboot = pb_file_ft7251,
	.pb_length = sizeof(pb_file_ft7251),
	.pram_ecc_check_mode = ECC_CHECK_MODE_CRC16,
	.hid_supported = false,
	.upgrade = fts_ft7251_upgrade,
};
