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
* File Name: focaltech_upgrade_ft6336GU.c
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

#if ((IC_SERIALS == 0x03) || (IC_SERIALS == 0x04))
#include "../focaltech_flash.h"
#include "focaltech_upgrade_common.h"

/*****************************************************************************
* Static variables
*****************************************************************************/
#define APP_FILE_MAX_SIZE           (60 * 1024)
#define APP_FILE_MIN_SIZE           (8)
#define APP_FILE_VER_MAPPING        (0x10A)
#define APP_FILE_VENDORID_MAPPING   (0x10C)
#define CONFIG_START_ADDR           (0x7B0)
#define CONFIG_VENDOR_ID_OFFSET     (0x4)
#define CONFIG_PROJECT_ID_OFFSET    (0x20)
#define CONFIG_VENDOR_ID_ADDR       (CONFIG_START_ADDR+CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR      (CONFIG_START_ADDR+CONFIG_PROJECT_ID_OFFSET)

#define APP_LEN         0x00
#define APP_LEN_NE      0x02
#define APP_P1_ECC      0x04
#define APP_P1_ECC_NE   0x05
#define APP_P2_ECC      0x06
#define APP_P2_ECC_NE   0x07

#define APP1_START          0x00
#define APP1_LEN            0x100
#define APP_VERIF_ADDR      (APP1_START + APP1_LEN)
#define APP_VERIF_LEN       0x20
#define APP1_ECC_ADDR       (APP_VERIF_ADDR + APP_P1_ECC)
#define APP2_START          (APP_VERIF_ADDR + APP_VERIF_LEN)
#define APP2_ECC_ADDR       (APP_VERIF_ADDR + APP_P2_ECC)
/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
static int fts_ft6x36gu_get_i_file(struct i2c_client *client, int fw_valid);
static int fts_ft6x36gu_get_app_i_file_ver(void);
static int fts_ft6x36gu_get_app_bin_file_ver(struct i2c_client *client,
				char *firmware_name);
static int fts_ft6x36gu_upgrade_with_app_i_file(struct i2c_client *client);
static int fts_ft6x36gu_upgrade_with_app_bin_file(struct i2c_client *client,
					char *firmware_name);

struct fts_upgrade_fun fts_updatefun = {

	.get_i_file = fts_ft6x36gu_get_i_file,
	.get_app_bin_file_ver = fts_ft6x36gu_get_app_bin_file_ver,
	.get_app_i_file_ver = fts_ft6x36gu_get_app_i_file_ver,
	.upgrade_with_app_i_file = fts_ft6x36gu_upgrade_with_app_i_file,
	.upgrade_with_app_bin_file = fts_ft6x36gu_upgrade_with_app_bin_file,
	.upgrade_with_lcd_cfg_i_file = NULL,
	.upgrade_with_lcd_cfg_bin_file = NULL,
};

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
#if (FTS_GET_VENDOR_ID_NUM != 0)
/************************************************************************
* Name: fts_ft6x36gu_get_vendor_id_flash
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
static int fts_ft6x36gu_get_vendor_id_flash(struct i2c_client *client,
					u8 *vendor_id)
{
	u8 reg_val[2] = {0};
	u32 i = 0;
	u8 rw_buf[10];
	int i_ret;

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		rw_buf[0] = FTS_UPGRADE_55;
		rw_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write(client, rw_buf, 2);
		if (i_ret < 0) {
			FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa!!");
			continue;
		}

		/*check run in bootloader or not*/
		usleep_range(1000, 2000);
		rw_buf[0] = FTS_READ_ID_REG;
		rw_buf[1] = rw_buf[2] = rw_buf[3] = 0x00;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, rw_buf, 4, reg_val, 2);

		FTS_DEBUG("[UPGRADE]: ID1 = 0x%x,ID2 = 0x%x!!",
					reg_val[0], reg_val[1]);
		if ((reg_val[0] == chip_types.bootloader_idh)
			&& (reg_val[1] == chip_types.bootloader_idl)) {
			FTS_DEBUG("[UPGRADE]: read bootloader id ok!! ");
			break;
		}
		FTS_ERROR("[UPGRADE]: read bootloader id fail!!");
	}

	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;

	/*read vendor id*/
	rw_buf[0] = 0x03;
	rw_buf[1] = 0x00;
	rw_buf[2] = (u8)(CONFIG_VENDOR_ID_ADDR >> 8);
	rw_buf[3] = (u8)(CONFIG_VENDOR_ID_ADDR);
	i_ret = fts_i2c_write(client, rw_buf, 4);
	usleep_range(10000, 20000); /*must wait, otherwise read vendor id fail*/
	i_ret = fts_i2c_read(client, NULL, 0, vendor_id, 1);
	if (i_ret < 0)
		return -EIO;
	FTS_DEBUG("Vendor ID from Flash:%x", *vendor_id);
	return 0;
}
#endif

/************************************************************************
* Name: fts_ft6x36gu_get_i_file
* Brief: get .i file
* Input:
* Output:
* Return: 0   - ok
*		 <0 - fail
***********************************************************************/
static int fts_ft6x36gu_get_i_file(struct i2c_client *client, int fw_valid)
{
	int ret = -EIO;

#if (FTS_GET_VENDOR_ID_NUM != 0)
	u8 vendor_id = 0;

	if (fw_valid)
		ret = fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &vendor_id);
	else
		ret = fts_ft6x36gu_get_vendor_id_flash(client, &vendor_id);

	FTS_DEBUG("[UPGRADE] tp_vendor_id=%x", vendor_id);
	if (ret < 0) {
		FTS_ERROR("Get upgrade file fail because of Vendor ID wrong");
		return ret;
	}

	FTS_INFO("[UPGRADE]tp vendor id:%x, FTS_VENDOR_ID:%02x %02x %02x",
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
* Name: fts_ft6x36gu_get_app_bin_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ft6x36gu_get_app_bin_file_ver(struct i2c_client *client,
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
* Name: fts_ft6x36gu_get_app_i_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ft6x36gu_get_app_i_file_ver(void)
{
	int fwsize = g_fw_len;

	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
		return 0;
	}

	return g_fw_file[APP_FILE_VER_MAPPING];
}

/*****************************************************************************
* Name: fts_check_app_bin_valid
* Brief:
* Input:
* Output:
* Return:
*****************************************************************************/
static bool fts_check_app_bin_valid(u8 *buf)
{
	int i;
	u16 len;
	u16 len_neg;
	u16 ecc2_len = 0;
	u8 cal_ecc = 0;
	u8 cal_ecc2 = 0;

	FTS_INFO("[UPGRADE] Check APP.BIN ECC");

	/* 1. start code byte */
	if (buf[0] != 0x02) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- the first byte(%x) error",
						buf[0]);
		return false;
	}

	/* 2. len */
	len = ((u16)buf[APP_VERIF_ADDR + APP_LEN] << 8)
		+ buf[APP_VERIF_ADDR + APP_LEN + 1];
	len_neg = (u16)(buf[APP_VERIF_ADDR + APP_LEN_NE] << 8)
		+ buf[APP_VERIF_ADDR + APP_LEN_NE + 1];
	if ((len ^ len_neg) != 0xFFFF) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- LENGTH(%04x) XOR error",
						len);
		return false;
	}

	/* 3. ecc */
	if (((buf[APP1_ECC_ADDR] ^ buf[APP1_ECC_ADDR+1]) != 0xFF)
		|| ((buf[APP2_ECC_ADDR] ^ buf[APP2_ECC_ADDR+1]) != 0xFF)) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC(%x %x) XOR error",
				buf[APP1_ECC_ADDR], buf[APP2_ECC_ADDR]);
		return false;
	}

	/* APP1 */
	for (i = 0; i < APP1_LEN; i++)
		cal_ecc ^= buf[APP1_START+i];
	/* APP2 */
	ecc2_len = ((u16)buf[APP_VERIF_ADDR+0x10] << 8)
			+ buf[APP_VERIF_ADDR+0x11];
	ecc2_len = len - APP1_LEN - APP_VERIF_LEN - ecc2_len;
	for (i = 0; i < ecc2_len; i++)
		cal_ecc2 ^= buf[APP2_START+i];

	if ((cal_ecc != buf[APP1_ECC_ADDR])
		|| (cal_ecc2 != buf[APP2_ECC_ADDR])) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC(%x %x) calc error",
					cal_ecc, cal_ecc2);
		return false;
	}

	return true;
}


/************************************************************************
* Name: fts_ft6x36gu_upgrade_use_buf
* Brief: fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_ft6x36gu_upgrade_use_buf(struct i2c_client *client,
				u8 *pbt_buf, u32 dw_length)
{
	u8 reg_val[2] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 length;
	u32 fw_length;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 upgrade_ecc;

	FTS_FUNC_ENTER();

	/*if the first byte of app is not 0x02,
	 *the app is invaild, can not upgrade*/
	if (pbt_buf[0] != 0x02) {
		FTS_ERROR("[UPGRADE]: app first byte != 0x02. cannot upgrade!");
		return -EINVAL;
	}

	/*check app length*/
	if (dw_length > 0x11f) {
		fw_length = ((u32)pbt_buf[0x100]<<8) + pbt_buf[0x101];
		if (dw_length < fw_length) {
			FTS_ERROR("[UPGRADE]: Fw length error!!");
			return -EINVAL;
		}
	} else {
		FTS_ERROR("[UPGRADE]: Fw length error!!");
		return -EINVAL;
	}

	/*send upgrade commond*/
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/*send 0xAA and 0x55 to fw(0xFC reg), and start upgrade*/
		fts_i2c_write_reg(client, FTS_RST_CMD_REG2, FTS_UPGRADE_AA);
		usleep_range(10000, 20000);
		fts_i2c_write_reg(client, FTS_RST_CMD_REG2, FTS_UPGRADE_55);
		usleep_range(10000, 20000);

		/*upgrade init in ROM*/
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		fts_i2c_write(client, auc_i2c_write_buf, 1);
		auc_i2c_write_buf[0] = FTS_UPGRADE_AA;
		fts_i2c_write(client, auc_i2c_write_buf, 1);
		usleep_range(10000, 20000);

		/*check run in ROM now*/
		auc_i2c_write_buf[0] = FTS_READ_ID_REG;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2]
					= auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);

		if (reg_val[0] == chip_types.bootloader_idh
			&& reg_val[1] == chip_types.bootloader_idl)
			break;
	}

	if (i >= FTS_UPGRADE_LOOP) {
		FTS_ERROR("[UPGRADE]: get bootload id error !!");
		return -EIO;
	}

	/*erase app in flash*/
	FTS_INFO("[UPGRADE]: erase app!!");
	auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(2000);

	for (i = 0; i < 200; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		auc_i2c_write_buf[1] = 0x00;
		auc_i2c_write_buf[2] = 0x00;
		auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
		if (0xb0 == reg_val[0] && 0x02 == reg_val[1]) {
			FTS_INFO("[UPGRADE]: erase app finished!!");
			break;
		}
		msleep(50);
	}

	/*write app to flash*/
	upgrade_ecc = 0;
	FTS_INFO("[UPGRADE]: write app to flash!!");

	dw_length = fw_length;
	packet_number = (dw_length) / FTS_PACKET_LENGTH;
	packet_buf[0] = FTS_FW_WRITE_CMD;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) {
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		length = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (length >> 8);
		packet_buf[5] = (u8) length;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			upgrade_ecc ^= packet_buf[6 + i];
		}

		fts_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			auc_i2c_write_buf[1] = 0x00;
			auc_i2c_write_buf[2] = 0x00;
			auc_i2c_write_buf[3] = 0x00;
			reg_val[0] = 0x00;
			reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
			if (0xb0 == (reg_val[0] & 0xf0) && (0x03 + (j % 0x0ffd))
				== (((reg_val[0] & 0x0f) << 8) | reg_val[1]))
				break;
			/* usleep_range(1000, 2000); */
			fts_ctpm_upgrade_delay(1000);
		}
	}

	if ((dw_length) % FTS_PACKET_LENGTH > 0) {
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_length) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number
						* FTS_PACKET_LENGTH + i];
			upgrade_ecc ^= packet_buf[6 + i];
		}

		fts_i2c_write(client, packet_buf, temp + 6);

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			auc_i2c_write_buf[1] = 0x00;
			auc_i2c_write_buf[2] = 0x00;
			auc_i2c_write_buf[3] = 0x00;
			reg_val[0] = 0x00;
			reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
			if (0xb0 == (reg_val[0] & 0xf0) && (0x03 + (j % 0x0ffd))
				== (((reg_val[0] & 0x0f) << 8) | reg_val[1]))
				break;
			/* usleep_range(1000, 2000); */
			fts_ctpm_upgrade_delay(1000);
		}
	}

	/*read out checksum*/
	FTS_INFO("[UPGRADE]: read out checksum!!");
	auc_i2c_write_buf[0] = FTS_REG_ECC;
	fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	/*check sum error, upgrade fail*/
	if (reg_val[0] != upgrade_ecc) {
		FTS_ERROR("[UPGRADE]: ecc error : FW=%02x upgrade_ecc=%02x!!",
				reg_val[0], upgrade_ecc);
		return -EIO;
	}
	FTS_INFO("[UPGRADE]: ecc ok!!");

	/*upgrade success, reset the new FW*/
	FTS_INFO("[UPGRADE]: reset the new FW!!");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(300);

	FTS_FUNC_EXIT();

	return 0;
}

/************************************************************************
* Name: fts_ft6x36gu_upgrade_with_app_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output:
* Return: fail < 0
***********************************************************************/
static int fts_ft6x36gu_upgrade_with_app_i_file(struct i2c_client *client)
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

	i_ret = fts_ft6x36gu_upgrade_use_buf(client, fw_buf, fw_len);
	if (i_ret != 0)
		FTS_ERROR("[UPGRADE] upgrade app.i failed");
	else
		FTS_INFO("[UPGRADE]: upgrade app.i succeed");

	return i_ret;
}

/************************************************************************
* Name: fts_ft6x36gu_upgrade_with_app_bin_file
* Brief: upgrade with *.bin file
* Input: i2c info, file name
* Output: no
* Return: success =0
***********************************************************************/
static int fts_ft6x36gu_upgrade_with_app_bin_file(struct i2c_client *client,
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
	ecc_ok = fts_check_app_bin_valid(pbt_buf);

	if (ecc_ok) {
		FTS_INFO("[UPGRADE] app.bin ecc ok");
		i_ret = fts_ft6x36gu_upgrade_use_buf(client, pbt_buf, fw->size);
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
#endif  /* FT6x36GU */
