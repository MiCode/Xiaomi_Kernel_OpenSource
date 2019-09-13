/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2012-2019, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
 *
 * File Name: focaltech_upgrade_ft6216.c
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
u8 pb_file_ft6216[] = {
#include "../include/pramboot/FT6216_Pramboot_V0.1_20180418.i"
};

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/
/************************************************************************
 * Name: fts_ft6216_upgrade
 * Brief:
 * Input:
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft6216_upgrade(u8 *buf, u32 len)
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
	start_addr = upgrade_func_ft6216.appoff;
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

struct upgrade_func upgrade_func_ft6216 = {
	.ctype = {0x84},
	.fwveroff = 0x010E,
	.fwcfgoff = 0xFFB0,
	.appoff = 0x0000,
	.pramboot_supported = true,
	.pramboot = pb_file_ft6216,
	.pb_length = sizeof(pb_file_ft6216),
	.hid_supported = false,
	.upgrade = fts_ft6216_upgrade,
};
