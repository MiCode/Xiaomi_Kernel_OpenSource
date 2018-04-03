/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
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
 * File Name: focaltech_upgrade_ft8716.c
 *
 * Author:    fupeipei
 *
 * Created:    2016-08-15
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

#if ((FTS_CHIP_TYPE == _FT8716) || (FTS_CHIP_TYPE == _FTE716))
#include "../focaltech_flash.h"
#include "focaltech_upgrade_common.h"

/*****************************************************************************
 * Static variables
 *****************************************************************************/
#define APP_FILE_MAX_SIZE           (64 * 1024)
#define APP_FILE_MIN_SIZE           (8)
#define APP_FILE_VER_MAPPING        (0x10E)
#define APP_FILE_VENDORID_MAPPING   (0x10C)
#define APP_FILE_CHIPID_MAPPING     (0x11E)
#define CONFIG_START_ADDR           (0x0000)
#define CONFIG_VENDOR_ID_OFFSET     (0x04)
#define CONFIG_PROJECT_ID_OFFSET    (0x20)
#define CONFIG_VENDOR_ID_ADDR       (CONFIG_START_ADDR+CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR      (CONFIG_START_ADDR+CONFIG_PROJECT_ID_OFFSET)

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/
static int fts_ctpm_get_i_file(struct i2c_client *client, int fw_valid);
static int fts_ctpm_get_app_i_file_ver(void);
static int fts_ctpm_get_app_bin_file_ver(struct i2c_client *client,
				char *firmware_name);
static int fts_ctpm_write_pram(struct i2c_client *client,
			u8 *pbt_buf, u32 dw_length);
static int fts_ctpm_fw_upgrade_with_app_i_file(struct i2c_client *client);
static int fts_ctpm_fw_upgrade_with_app_bin_file(struct i2c_client *client,
			char *firmware_name);

struct fts_upgrade_fun fts_updatefun = {
	.get_i_file = fts_ctpm_get_i_file,
	.get_app_bin_file_ver = fts_ctpm_get_app_bin_file_ver,
	.get_app_i_file_ver = fts_ctpm_get_app_i_file_ver,
	.upgrade_with_app_i_file = fts_ctpm_fw_upgrade_with_app_i_file,
	.upgrade_with_app_bin_file = fts_ctpm_fw_upgrade_with_app_bin_file,
	.upgrade_with_lcd_cfg_i_file = NULL,
	.upgrade_with_lcd_cfg_bin_file = NULL,
};

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/
#if (FTS_GET_VENDOR_ID_NUM != 0)
/************************************************************************
 * Name: fts_ctpm_get_vendor_id_flash
 * Brief:
 * Input:
 * Output:
 * Return:
 ***********************************************************************/
static int fts_ctpm_get_vendor_id_flash(struct i2c_client *client,
				u8 *vendor_id)
{
	u8 rw_buf[10];
	int i_ret;
	int fw_len;
	bool inpram = false;

	FTS_FUNC_ENTER();

	/*write pramboot*/
	fw_len = fts_getsize(PRAMBOOT_SIZE);
	FTS_DEBUG("[UPGRADE]: pramboot size : %d!!", fw_len);
	i_ret = fts_ctpm_write_pram(client, aucFW_PRAM_BOOT, fw_len);
	if (i_ret != 0) {
		FTS_ERROR("[UPGRADE]: write pram failed!!");
		return -EIO;
	}

	/*check run in pramboot or not!
	 *if not rum in pramboot, can not upgrade
	 */
	inpram = fts_ctpm_check_run_state(client, FTS_RUN_IN_PRAM);
	if (!inpram) {
		FTS_ERROR("[UPGRADE]: not run in pram, upgrade fail!!");
		return -EIO;
	}

	/*read vendor id*/
	rw_buf[0] = 0x03;
	rw_buf[1] = 0x00;
	rw_buf[2] = (u8)(CONFIG_VENDOR_ID_ADDR >> 8);
	rw_buf[3] = (u8)(CONFIG_VENDOR_ID_ADDR);
	i_ret = fts_i2c_write(client, rw_buf, 4);
	/* must wait, otherwise read vendor id wrong */
	usleep_range(10000, 20000);
	i_ret = fts_i2c_read(client, NULL, 0, vendor_id, 1);
	if (i_ret < 0)
		return -EIO;
	FTS_DEBUG("Vendor ID from Flash:%x", *vendor_id);
	return 0;
}
#endif

/************************************************************************
 * Name: fts_ctpm_get_i_file
 * Brief: get .i file
 * Input:
 * Output:
 * Return: 0   - ok
 *		 <0 - fail
 ***********************************************************************/
static int fts_ctpm_get_i_file(struct i2c_client *client, int fw_valid)
{
	int ret = -EIO;

#if (FTS_GET_VENDOR_ID_NUM != 0)
	u8 vendor_id = 0;

	if (fw_valid) {
		ret = fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &vendor_id);
	} else {
		ret = fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &vendor_id);
		if ((ret < 0) || (vendor_id == 0xEF) || (vendor_id == 0xED)) {
			/* 8736 can't read vendor id from A8 command */
			ret = fts_ctpm_get_vendor_id_flash(client, &vendor_id);
		}
	}

	if (ret < 0) {
		FTS_ERROR("Get upgrade file fail because of Vendor ID wrong");
		return ret;
	}
	FTS_INFO("[UPGRADE] vendor id tp=%x", vendor_id);
	FTS_INFO("[UPGRADE] vendor id driver:%x, FTS_VENDOR_ID:%02x %02x %02x",
		 vendor_id, FTS_VENDOR_1_ID, FTS_VENDOR_2_ID, FTS_VENDOR_3_ID);

	ret = 0;
	switch (vendor_id) {
#if (FTS_GET_VENDOR_ID_NUM >= 1)
	case FTS_VENDOR_1_ID:
		g_fw_file = CTPM_FW;
		g_fw_len = fts_getsize(FW_SIZE);
		FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW, SIZE:%x", g_fw_len);
		break;
#endif
#if (FTS_GET_VENDOR_ID_NUM >= 2)
	case FTS_VENDOR_2_ID:
		g_fw_file = CTPM_FW2;
		g_fw_len = fts_getsize(FW2_SIZE);
		FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW2, SIZE:%x", g_fw_len);
		break;
#endif
#if (FTS_GET_VENDOR_ID_NUM >= 3)
	case FTS_VENDOR_3_ID:
		g_fw_file = CTPM_FW3;
		g_fw_len = fts_getsize(FW3_SIZE);
		FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW3, SIZE:%x", g_fw_len);
		break;
#endif
	default:
		FTS_ERROR("[UPGRADE]Vendor ID check fail, get fw file fail");
		ret = -EIO;
		break;
	}
#endif

	return ret;
}

/************************************************************************
 * Name: fts_ctpm_get_app_bin_file_ver
 * Brief:  get .i file version
 * Input: no
 * Output: no
 * Return: fw version
 ***********************************************************************/
static int fts_ctpm_get_app_bin_file_ver(struct i2c_client *client,
				char *firmware_name)
{
	const struct firmware *fw = NULL;
	int fw_ver = 0;
	int ret;

	FTS_FUNC_ENTER();

	ret = request_firmware(&fw, firmware_name, &client->dev);
	if (ret) {
		FTS_ERROR("[UPGRADE]: failed to get fw %s\n", firmware_name);
		return ret;
	}

	if (fw->size < APP_FILE_MIN_SIZE || fw->size > APP_FILE_MAX_SIZE)
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fw->size);
	else
		fw_ver = fw->data[APP_FILE_VER_MAPPING];

	release_firmware(fw);
	FTS_FUNC_EXIT();

	return fw_ver;
}

/************************************************************************
 * Name: fts_ctpm_get_app_i_file_ver
 * Brief:  get .i file version
 * Input: no
 * Output: no
 * Return: fw version
 ***********************************************************************/
static int fts_ctpm_get_app_i_file_ver(void)
{
	int fwsize = g_fw_len;

	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
		return 0;
	}

	return g_fw_file[APP_FILE_VER_MAPPING];
}

/************************************************************************
 * Name: fts_ctpm_write_pram
 * Brief: fw upgrade
 * Input: i2c info, file buf, file len
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_ctpm_write_pram(struct i2c_client *client,
			u8 *pbt_buf, u32 dw_length)
{
	int i_ret;
	bool inrom = false;

	FTS_FUNC_ENTER();

	/*check the length of the pramboot*/
	if (dw_length > APP_FILE_MAX_SIZE || dw_length < APP_FILE_MIN_SIZE) {
		FTS_ERROR("[UPGRADE] pramboot length(%d) fail", dw_length);
		return -EIO;
	}

	/*send comond to FW, reset and start write pramboot*/
	i_ret = fts_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: send upgrade cmd to FW error!!");
		return i_ret;
	}

	/*check run in rom or not! if run in rom, will write pramboot*/
	inrom = fts_ctpm_check_run_state(client, FTS_RUN_IN_ROM);
	if (!inrom) {
		FTS_ERROR("[UPGRADE]: not run in rom, write pramboot fail!!");
		return -EIO;
	}

	/*write pramboot to pram*/
	i_ret = fts_ctpm_write_pramboot_for_idc(client,
					dw_length, aucFW_PRAM_BOOT);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: write pramboot fail!!");
		return i_ret;
	}

	/*read out checksum*/
	i_ret = fts_ctpm_pramboot_ecc(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: write pramboot ecc error!!");
		return i_ret;
	}

	/*start pram*/
	fts_ctpm_start_pramboot(client);

	FTS_FUNC_EXIT();

	return 0;
}

/************************************************************************
 * Name: fts_ctpm_write_app
 * Brief:  fw upgrade
 * Input: i2c info, file buf, file len
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_ctpm_write_app(struct i2c_client *client,
			u8 *pbt_buf, u32 dw_length)
{
	u32 temp;
	int i_ret;
	bool inpram = false;

	FTS_FUNC_ENTER();

	/*check run in pramboot or not!
	 *if not rum in pramboot, can not upgrade
	 */
	inpram = fts_ctpm_check_run_state(client, FTS_RUN_IN_PRAM);
	if (!inpram) {
		FTS_ERROR("[UPGRADE]: not run in pram, upgrade fail!!");
		return -EIO;
	}

	/*upgrade init*/
	i_ret = fts_ctpm_upgrade_idc_init(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: upgrade init error, upgrade fail!!");
		return i_ret;
	}

	/*erase the app erea in flash*/
	i_ret = fts_ctpm_erase_flash(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: erase flash error!!");
		return i_ret;
	}

	/*start to write app*/
	i_ret = fts_ctpm_write_app_for_idc(client, dw_length, pbt_buf);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: write app error!!");
		return i_ret;
	}

	/*read check sum*/
	temp = 0x1000;
	i_ret = fts_ctpm_upgrade_ecc(client, temp, dw_length);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: ecc error!!");
		return i_ret;
	}

	/*upgrade success, reset the FW*/
	fts_ctpm_rom_or_pram_reset(client);

	FTS_FUNC_EXIT();

	return 0;
}

/************************************************************************
 * Name: fts_ctpm_fw_upgrade_use_buf
 * Brief: fw upgrade
 * Input: i2c info, file buf, file len
 * Output: no
 * Return: fail <0
 *		 success =0
 ***********************************************************************/
static int fts_ctpm_fw_upgrade_use_buf(struct i2c_client *client,
				u8 *pbt_buf, u32 fwsize)
{
	int i_ret = 0;
	int fw_len;

	FTS_FUNC_ENTER();

	/*write pramboot*/
	fw_len = fts_getsize(PRAMBOOT_SIZE);
	FTS_DEBUG("[UPGRADE]: pramboot size : %d!!", fw_len);
	i_ret = fts_ctpm_write_pram(client, aucFW_PRAM_BOOT, fw_len);
	if (i_ret != 0) {
		FTS_ERROR("[UPGRADE]: write pram failed!!");
		return -EIO;
	}

	/*write app*/
	i_ret =  fts_ctpm_write_app(client, pbt_buf, fwsize);

	FTS_FUNC_EXIT();

	return i_ret;
}

/************************************************************************
 * Name: fts_ctpm_fw_upgrade_with_app_i_file
 * Brief:  upgrade with *.i file
 * Input: i2c info
 * Output:
 * Return: fail < 0
 ***********************************************************************/
static int fts_ctpm_fw_upgrade_with_app_i_file(struct i2c_client *client)
{
	int i_ret = 0;
	u32 fw_len;
	u8 *fw_buf;

	FTS_INFO("[UPGRADE]**********start upgrade with app.i**********");

	fw_len = g_fw_len;
	fw_buf = g_fw_file;
	if (fw_len < APP_FILE_MIN_SIZE || fw_len > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fw_len);
		return -EIO;
	}

	i_ret = fts_ctpm_fw_upgrade_use_buf(client, fw_buf, fw_len);
	if (i_ret != 0)
		FTS_ERROR("[UPGRADE] upgrade app.i failed");
	else
		FTS_INFO("[UPGRADE]: upgrade app.i succeed");

	return i_ret;
}

/************************************************************************
 * Name: fts_ctpm_fw_upgrade_with_app_bin_file
 * Brief: upgrade with *.bin file
 * Input: i2c info, file name
 * Output: no
 * Return: success =0
 ***********************************************************************/
static int fts_ctpm_fw_upgrade_with_app_bin_file(struct i2c_client *client,
					char *firmware_name)
{
	const struct firmware *fw = NULL;
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	bool ecc_ok = false;
	int fwsize = 0;

	FTS_INFO("[UPGRADE]**********start upgrade with app.bin**********");

	i_ret = request_firmware(&fw, firmware_name, &client->dev);
	if (i_ret) {
		FTS_ERROR("[UPGRADE]: failed to get fw %s\n", firmware_name);
		return i_ret;
	}

	if (fw->size < APP_FILE_MIN_SIZE || fw->size > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: app.bin length(%x) error, upgrade fail",
							fwsize);
		goto ERROR_BIN;
	}

	/*check the app.bin invalid or not*/
	pbt_buf = (u8 *)fw->data;
	ecc_ok = fts_check_app_bin_valid_idc(pbt_buf);

	if (ecc_ok) {
		FTS_INFO("[UPGRADE] app.bin ecc ok");
		i_ret = fts_ctpm_fw_upgrade_use_buf(client, pbt_buf, fw->size);
		if (i_ret != 0) {
			FTS_ERROR("[UPGRADE]: upgrade app.bin failed");
			goto ERROR_BIN;
		} else {
			FTS_INFO("[UPGRADE]: upgrade app.bin succeed");
		}
	} else {
		FTS_ERROR("[UPGRADE] app.bin ecc failed");
		goto ERROR_BIN;
	}

ERROR_BIN:
	release_firmware(fw);
	return i_ret;
}
#endif  /* #if (FTS_CHIP_TYPE == _FT8716) */
