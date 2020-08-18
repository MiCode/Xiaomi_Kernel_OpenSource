/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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
* File Name: focaltech_upgrade_ft8719.c
*
* Author: Focaltech Driver Team
*
* Created: 2017-11-22
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "../focaltech_core.h"
#include "../focaltech_flash.h"

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
u8 pb_file_ft8719[] = {
#include "../include/pramboot/FT8719_Pramboot_V0.5_20171221.i"
};

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/************************************************************************
 * fts_ft8719_upgrade_mode -
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8719_upgrade_mode(struct i2c_client *client, enum FW_FLASH_MODE mode, u8 *buf, u32 len)
{
	int ret = 0;
	u32 start_addr = 0;
	u8 cmd[4] = { 0 };
	u32 delay = 0;
	int ecc_in_host = 0;
	int ecc_in_tp = 0;

	if ((NULL == buf) || (len < FTS_MIN_LEN)) {
		MI_TOUCH_LOGE(1, "buffer/len(%x) is invalid", len);
		return -EINVAL;
	}

	/* enter into upgrade environment */
	ret = fts_fwupg_enter_into_boot(client);
	if (ret < 0) {
		MI_TOUCH_LOGE(1, "enter into pramboot/bootloader fail,ret=%d", ret);
		goto fw_reset;
	}

	cmd[0] = FTS_CMD_FLASH_MODE;
	cmd[1] = FLASH_MODE_UPGRADE_VALUE;
	start_addr = upgrade_func_ft8719.appoff;
	MI_TOUCH_LOGI(1, "flash mode:0x%02x, start addr=0x%04x", cmd[1], start_addr);
	ret = fts_i2c_write(client, cmd, 2);
	if (ret < 0) {
		MI_TOUCH_LOGE(1, "upgrade mode(09) cmd write fail");
		goto fw_reset;
	}

	delay = FTS_ERASE_SECTOR_DELAY * (len / FTS_MAX_LEN_SECTOR);
	ret = fts_fwupg_erase(client, delay);
	if (ret < 0) {
		MI_TOUCH_LOGE(1, "erase cmd write fail");
		goto fw_reset;
	}

	/* write app */
	ecc_in_host = fts_flash_write_buf(client, start_addr, buf, len, 1);
	if (ecc_in_host < 0) {
		MI_TOUCH_LOGE(1, "lcd initial code write fail");
		goto fw_reset;
	}

	/* ecc */
	ecc_in_tp = fts_fwupg_ecc_cal(client, start_addr, len);
	if (ecc_in_tp < 0) {
		MI_TOUCH_LOGE(1, "ecc read fail");
		goto fw_reset;
	}

	MI_TOUCH_LOGI(1, "ecc in tp:%x, host:%x", ecc_in_tp, ecc_in_host);
	if (ecc_in_tp != ecc_in_host) {
		MI_TOUCH_LOGE(1, "ecc check fail");
		goto fw_reset;
	}

	MI_TOUCH_LOGI(1, "upgrade success, reset to normal boot");
	ret = fts_fwupg_reset_in_boot(client);
	if (ret < 0) {
		MI_TOUCH_LOGE(1, "reset to normal boot fail");
	}

	msleep(400);
	return 0;

fw_reset:
	return -EIO;
}

/************************************************************************
* Name: fts_ft8719_upgrade
* Brief:
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_ft8719_upgrade(struct i2c_client *client, u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u32 app_len = 0;

	MI_TOUCH_LOGI(1, "fw app upgrade...");
	if (NULL == buf) {
		MI_TOUCH_LOGE(1, "fw buf is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		MI_TOUCH_LOGE(1, "fw buffer len(%x) fail", len);
		return -EINVAL;
	}

	app_len = len;
	tmpbuf = buf;
	ret = fts_ft8719_upgrade_mode(client, FLASH_MODE_APP, tmpbuf, app_len);
	if (ret < 0) {
		MI_TOUCH_LOGI(1, "fw upgrade fail,reset to normal boot");
		if (fts_fwupg_reset_in_boot(client) < 0) {
			MI_TOUCH_LOGE(1, "reset to normal boot fail");
		}
		return ret;
	}

	return 0;
}

struct upgrade_func upgrade_func_ft8719 = {
	.ctype = {0x0D},
	.newmode = true,
	.fwveroff = 0x010E,
	.fwcfgoff = 0x1F80,
	.appoff = 0x2000,
	.pramboot_supported = true,
	.pramboot = pb_file_ft8719,
	.pb_length = sizeof(pb_file_ft8719),
	.hid_supported = false,
	.upgrade = fts_ft8719_upgrade
};
