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
 * File Name: focaltech_upgrade_ft8006.c
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

#if (FTS_CHIP_TYPE == _FT8006)
#include "../focaltech_flash.h"
#include "focaltech_upgrade_common.h"

/*****************************************************************************
 * Static variables
 *****************************************************************************/
#define APP_FILE_MAX_SIZE           (93 * 1024)
#define APP_FILE_MIN_SIZE           (8)
#define APP_FILE_VER_MAPPING        (0x10E)
#define APP_FILE_VENDORID_MAPPING   (0x10C)
#define APP_FILE_CHIPID_MAPPING     (0x11E)
#define CONFIG_START_ADDR           (0xF80)
#define CONFIG_START_ADDR_LEN       (0x80)
#define CONFIG_VENDOR_ID_OFFSET     (0x04)
#define CONFIG_PROJECT_ID_OFFSET    (0x20)
#define CONFIG_VENDOR_ID_ADDR       (CONFIG_START_ADDR+CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR      (CONFIG_START_ADDR+CONFIG_PROJECT_ID_OFFSET)
#define LCD_CFG_MAX_SIZE            (4 * 1024)
#define LCD_CFG_MIN_SIZE            (8)

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/
static int fts_ctpm_get_i_file(struct i2c_client *client, int fw_valid);
static int fts_ctpm_get_app_i_file_ver(void);
static int fts_ctpm_get_app_bin_file_ver(char *firmware_name);
static int fts_ctpm_fw_upgrade_with_app_i_file(struct i2c_client *client);
static int fts_ctpm_fw_upgrade_with_app_bin_file(struct i2c_client *client,
					char *firmware_name);
static int fts_ctpm_fw_upgrade_with_lcd_cfg_i_file(struct i2c_client *client);
static int fts_ctpm_fw_upgrade_with_lcd_cfg_bin_file(struct i2c_client *client,
					char *firmware_name);

struct fts_upgrade_fun fts_updatefun = {

	.get_i_file = fts_ctpm_get_i_file,
	.get_app_bin_file_ver = fts_ctpm_get_app_bin_file_ver,
	.get_app_i_file_ver = fts_ctpm_get_app_i_file_ver,
	.upgrade_with_app_i_file = fts_ctpm_fw_upgrade_with_app_i_file,
	.upgrade_with_app_bin_file = fts_ctpm_fw_upgrade_with_app_bin_file,
	.upgrade_with_lcd_cfg_i_file = fts_ctpm_fw_upgrade_with_lcd_cfg_i_file,
	.upgrade_with_lcd_cfg_bin_file =
				fts_ctpm_fw_upgrade_with_lcd_cfg_bin_file,
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
	bool inbootloader = false;
	u8 rw_buf[10];
	int i_ret;

	fts_ctpm_i2c_hid2std(client);

	i_ret = fts_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: send upgrade cmd to FW error!!");
		return i_ret;
	}

	/*Enter upgrade mode*/
	fts_ctpm_i2c_hid2std(client);
	usleep_range(10000, 20000);

	inbootloader = fts_ctpm_check_run_state(client, FTS_RUN_IN_BOOTLOADER);
	if (!inbootloader) {
		FTS_ERROR("[UPGRADE]: not run in bootloader, upgrade fail!!");
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
 * Name: fts_ft5x46_get_i_file
 * Brief: get .i file
 * Input:
 * Output:
 * Return: 0   - ok
 *		 <0 - fail
 ***********************************************************************/
static int fts_ctpm_get_i_file(struct i2c_client *client, int fw_valid)
{
	int ret = 0;

#if (FTS_GET_VENDOR_ID_NUM != 0)
	u8 vendor_id = 0;

	if (fw_valid)
		ret = fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &vendor_id);
	else
		ret = fts_ctpm_get_vendor_id_flash(client, &vendor_id);
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
#else
	/* (FTS_GET_VENDOR_ID_NUM == 0) */
	g_fw_file = CTPM_FW;
	g_fw_len = fts_getsize(FW_SIZE);
	FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW, SIZE:%x", g_fw_len);
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
static int fts_ctpm_get_app_bin_file_ver(char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int fwsize = 0;
	int fw_ver = 0;

	FTS_FUNC_ENTER();

	fwsize = fts_GetFirmwareSize(firmware_name);
	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
		return -EIO;
	}

	pbt_buf = kmalloc(fwsize + 1, GFP_KERNEL);
	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		FTS_ERROR("[UPGRADE]: request_firmware failed!!");
		kfree(pbt_buf);
		return -EIO;
	}

	fw_ver = pbt_buf[APP_FILE_VER_MAPPING];

	kfree(pbt_buf);
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
 * Name: fts_ctpm_fw_upgrade_use_buf
 * Brief: fw upgrade
 * Input: i2c info, file buf, file len
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_ctpm_fw_upgrade_use_buf(struct i2c_client *client,
			u8 *pbt_buf, u32 dw_length)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j = 0;
	u32 temp;
	u32 length;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 upgrade_ecc;
	int i_ret = 0;
	bool inbootloader = false;

	fts_ctpm_i2c_hid2std(client);

	i_ret = fts_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: send upgrade cmd to FW error!!");
		return i_ret;
	}

	/*Enter upgrade mode*/
	fts_ctpm_i2c_hid2std(client);
	usleep_range(10000, 20000);

	inbootloader = fts_ctpm_check_run_state(client, FTS_RUN_IN_BOOTLOADER);
	if (!inbootloader) {
		FTS_ERROR("[UPGRADE]: not run in bootloader, upgrade fail!!");
		return -EIO;
	}

	/*send upgrade type to reg 0x09: 0x0B: upgrade; 0x0A: download*/
	auc_i2c_write_buf[0] = 0x09;
	auc_i2c_write_buf[1] = 0x0B;
	fts_i2c_write(client, auc_i2c_write_buf, 2);

	/*
	 * All.bin <= 128K
	 * APP.bin <= 94K
	 * LCD_CFG <= 4K
	 */
	auc_i2c_write_buf[0] = 0xB0;
	auc_i2c_write_buf[1] = (u8) ((dw_length >> 16) & 0xFF);
	auc_i2c_write_buf[2] = (u8) ((dw_length >> 8) & 0xFF);
	auc_i2c_write_buf[3] = (u8) (dw_length & 0xFF);
	fts_i2c_write(client, auc_i2c_write_buf, 4);


	/*erase the app erea in flash*/
	i_ret = fts_ctpm_erase_flash(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: erase flash error!!");
		return i_ret;
	}

	/*write FW to ctpm flash*/
	upgrade_ecc = 0;
	FTS_DEBUG("[UPGRADE]: write FW to ctpm flash!!");
	temp = 0;
	packet_number = (dw_length) / FTS_PACKET_LENGTH;
	packet_buf[0] = FTS_FW_WRITE_CMD;

	for (j = 0; j < packet_number; j++) {
		temp = 0x5000 + j * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
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
		/* usleep_range(1000, 2000); */

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((j + 0x1000 + (0x5000/FTS_PACKET_LENGTH))
					== (((reg_val[0]) << 8) | reg_val[1]))
				break;

			if (i > 15) {
				usleep_range(1000, 2000);
				FTS_DEBUG("[UPGRADE]: write flash:host :%x!!",
					(j + 0x1000
					 + (0x5000/FTS_PACKET_LENGTH)));
				FTS_DEBUG("[UPGRADE]: write flash:status :%x!!",
					(((reg_val[0]) << 8) | reg_val[1]));
			}
			/* usleep_range(1000, 2000); */
			fts_ctpm_upgrade_delay(10000);
		}
	}

	if ((dw_length) % FTS_PACKET_LENGTH > 0) {
		temp = 0x5000 + packet_number * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
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
		/* usleep_range(1000, 2000); */

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((0x1000 + ((0x5000 + packet_number
				* FTS_PACKET_LENGTH)/((dw_length)
				% FTS_PACKET_LENGTH)))
				== (((reg_val[0]) << 8) | reg_val[1]))
				break;

			if (i > 15) {
				usleep_range(1000, 2000);
				FTS_DEBUG("[UPGRADE]: write flash:host :%x!!",
					(j + 0x1000
					 + (0x5000/FTS_PACKET_LENGTH)));
				FTS_DEBUG("[UPGRADE]: write flash:status :%x!!",
					(((reg_val[0]) << 8) | reg_val[1]));
			}
			/* usleep_range(1000, 2000); */
			fts_ctpm_upgrade_delay(10000);
		}
	}

	msleep(50);

	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	FTS_DEBUG("[UPGRADE]: read out checksum!!");
	auc_i2c_write_buf[0] = 0x64;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(300);

	temp = 0x5000;
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = (64*1024-1);
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = fts_i2c_write(client, auc_i2c_write_buf, 6);
	msleep(dw_length/256);

	temp = (0x5000+(64*1024-1));
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = (dw_length-(64*1024-1));
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = fts_i2c_write(client, auc_i2c_write_buf, 6);
	msleep(dw_length/256);

	for (i = 0; i < 100; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

		if (0xF0 == reg_val[0] && 0x55 == reg_val[1]) {
			FTS_DEBUG("[UPGRADE]: reg_val[0]=%02x reg_val[0]=%02x!",
					reg_val[0], reg_val[1]);
			break;
		}
		usleep_range(1000, 2000);

	}
	auc_i2c_write_buf[0] = 0x66;
	fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != upgrade_ecc) {
		FTS_ERROR("[UPGRADE]: ecc error! FW=%02x upgrade_ecc=%02x!!",
				reg_val[0], upgrade_ecc);
		return -EIO;
	}
	FTS_DEBUG("[UPGRADE]: checksum %x %x!!", reg_val[0], upgrade_ecc);

	FTS_DEBUG("[UPGRADE]: reset the new FW!!");
	auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1000);

	fts_ctpm_i2c_hid2std(client);

	return 0;
}

/************************************************************************
 * Name: fts_ctpm_fw_upgrade_use_buf
 * Brief: fw upgrade
 * Input: i2c info, file buf, file len
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_ctpm_lcd_cfg_upgrade_use_buf(struct i2c_client *client,
				u8 *pbt_buf, u32 dw_length)
{
	u8 reg_val[4] = {0};
	u8 cfg_backup[CONFIG_START_ADDR_LEN+1] = { 0 };
	u32 i = 0;
	u32 packet_number;
	u32 j = 0;
	u32 temp;
	u32 length;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 upgrade_ecc;
	int i_ret;

	fts_ctpm_i2c_hid2std(client);

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/*write 0xaa to register FTS_RST_CMD_REG1 */
		fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_AA);
		usleep_range(10000, 20000);

		/*write 0x55 to register FTS_RST_CMD_REG1*/
		fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_55);
		msleep(200);

		/*Enter upgrade mode*/
		fts_ctpm_i2c_hid2std(client);

		usleep_range(10000, 20000);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write(client, auc_i2c_write_buf, 2);
		if (i_ret < 0) {
			FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa!!");
			continue;
		}

		/*check run in bootloader or not*/
		usleep_range(1000, 2000);
		auc_i2c_write_buf[0] = FTS_READ_ID_REG;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2]
					= auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
		FTS_DEBUG("[UPGRADE]:ID1 = 0x%x,ID2 = 0x%x!!",
					reg_val[0], reg_val[1]);

		if (reg_val[0] == chip_types.bootloader_idh
			&& reg_val[1] == chip_types.bootloader_idl) {
			FTS_DEBUG("[UPGRADE]: read bootloader id ok!!");
			break;
		}
		FTS_DEBUG("[UPGRADE]: read bootloader id fail!!");
	}

	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;

	/* Backup FW configuratin area */
	reg_val[0] = 0x03;
	reg_val[1] = (u8)((CONFIG_START_ADDR-1) >> 16);
	reg_val[2] = (u8)((CONFIG_START_ADDR-1) >> 8);
	reg_val[3] = (u8)((CONFIG_START_ADDR-1));
	i_ret = fts_i2c_read(client, reg_val, 4,
			cfg_backup, CONFIG_START_ADDR_LEN+1);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] Read Config area error, don't upgrade");
		return -EIO;
	}

	/*send upgrade type to reg 0x09: 0x0B: upgrade; 0x0A: download*/
	auc_i2c_write_buf[0] = 0x09;
	auc_i2c_write_buf[1] = 0x0C;
	fts_i2c_write(client, auc_i2c_write_buf, 2);

	/*Step 4:erase app and panel paramenter area*/
	FTS_DEBUG("[UPGRADE]: erase app and panel paramenter area!!");
	auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1000);

	for (i = 0; i < 15; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
		if (0xF0 == reg_val[0] && 0xAA == reg_val[1])
			break;
		msleep(50);
	}
	FTS_DEBUG("[UPGRADE]: erase app area reg_val = %x %x!!",
					reg_val[0], reg_val[1]);

	auc_i2c_write_buf[0] = 0xB0;
	auc_i2c_write_buf[1] = 0;
	auc_i2c_write_buf[2] = (u8) ((dw_length >> 8) & 0xFF);
	auc_i2c_write_buf[3] = (u8) (dw_length & 0xFF);
	fts_i2c_write(client, auc_i2c_write_buf, 4);

	/*write FW to ctpm flash*/
	upgrade_ecc = 0;
	FTS_DEBUG("[UPGRADE]: write FW to ctpm flash!!");
	temp = 0;
	packet_number = (dw_length) / FTS_PACKET_LENGTH;
	packet_buf[0] = FTS_FW_WRITE_CMD;
	packet_buf[1] = 0;
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
		/* usleep_range(1000, 2000); */

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
				break;

			if (i > 15) {
				usleep_range(1000, 2000);
				FTS_DEBUG("[UPGRADE]: write flash:host : %x!",
					(j + 0x1000 +
					 (0x5000/FTS_PACKET_LENGTH)),
					(((reg_val[0]) << 8) | reg_val[1]));
				FTS_DEBUG("[UPGRADE]: write flash:status : %x!",
					(((reg_val[0]) << 8) | reg_val[1]));
			}
			/* usleep_range(1000, 2000); */
			fts_ctpm_upgrade_delay(10000);
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
		/* usleep_range(1000, 2000); */

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((0x1000 + ((packet_number * FTS_PACKET_LENGTH)
				/((dw_length) % FTS_PACKET_LENGTH)))
				== (((reg_val[0]) << 8) | reg_val[1]))
				break;

			if (i > 15) {
				usleep_range(1000, 2000);
				FTS_DEBUG("[UPGRADE]: write flash:host : %x!",
					(j + 0x1000 +
					 (0x5000/FTS_PACKET_LENGTH)),
					(((reg_val[0]) << 8) | reg_val[1]));
				FTS_DEBUG("[UPGRADE]: write flash:status : %x!",
					(((reg_val[0]) << 8) | reg_val[1]));
			}
			/* usleep_range(1000, 2000); */
			fts_ctpm_upgrade_delay(10000);
		}
	}

	/* Write Back FW configuratin area */
	packet_buf[0] = FTS_FW_WRITE_CMD;
	packet_buf[1] = (u8)(CONFIG_START_ADDR >> 16);
	packet_buf[2] = (u8)(CONFIG_START_ADDR >> 8);
	packet_buf[3] = (u8)(CONFIG_START_ADDR);
	packet_buf[4] = (u8)(CONFIG_START_ADDR_LEN >> 8);
	packet_buf[5] = (u8)(CONFIG_START_ADDR_LEN);
	memcpy(&packet_buf[6], &cfg_backup[1], CONFIG_START_ADDR_LEN);
	i_ret = fts_i2c_write(client, packet_buf, CONFIG_START_ADDR_LEN + 6);
	if (i_ret < 0)
		FTS_ERROR("[UPGRADE] Write Configuration area error");

	msleep(50);

	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	FTS_DEBUG("[UPGRADE]: read out checksum!!");
	auc_i2c_write_buf[0] = 0x64;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(300);

	temp = 0x00;
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = 0;
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = dw_length;
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = fts_i2c_write(client, auc_i2c_write_buf, 6);
	msleep(dw_length/256);

	for (i = 0; i < 100; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

		if (0xF0 == reg_val[0] && 0x55 == reg_val[1]) {
			FTS_DEBUG("[UPGRADE]:reg_val[0]=%02x reg_val[0]=%02x!",
						reg_val[0], reg_val[1]);
			break;
		}
		usleep_range(1000, 2000);

	}
	auc_i2c_write_buf[0] = 0x66;
	fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != upgrade_ecc) {
		FTS_ERROR("[UPGRADE]: ecc error! FW=%02x upgrade_ecc=%02x!!",
					reg_val[0], upgrade_ecc);
		return -EIO;
	}
	FTS_DEBUG("[UPGRADE]: checksum %x %x!!", reg_val[0], upgrade_ecc);

	FTS_DEBUG("[UPGRADE]: reset the new FW!!");
	auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1000);

	fts_ctpm_i2c_hid2std(client);

	return 0;
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
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	bool ecc_ok = false;
	int fwsize = 0;

	FTS_INFO("[UPGRADE]**********start upgrade with app.bin**********");

	fwsize = fts_GetFirmwareSize(firmware_name);
	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: app.bin length(%x) error, upgrade fail",
					fwsize);
		return -EIO;
	}

	pbt_buf = kmalloc(fwsize + 1, GFP_KERNEL);
	if (pbt_buf == NULL) {
		FTS_ERROR(" malloc pbt_buf failed ");
		goto ERROR_BIN;
	}

	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		FTS_ERROR("[UPGRADE]: request_firmware failed!!");
		goto ERROR_BIN;
	}

	/*check the app.bin invalid or not*/
	ecc_ok = fts_check_app_bin_valid_idc(pbt_buf);

	if (ecc_ok) {
		FTS_INFO("[UPGRADE] app.bin ecc ok");
		i_ret = fts_ctpm_fw_upgrade_use_buf(client, pbt_buf, fwsize);
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

	kfree(pbt_buf);
	return i_ret;
ERROR_BIN:
	kfree(pbt_buf);
	return -EIO;
}

/************************************************************************
 * Name: fts_ctpm_fw_upgrade_with_lcd_cfg_i_file
 * Brief:  upgrade with *.i file
 * Input: i2c info
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_ctpm_fw_upgrade_with_lcd_cfg_i_file(struct i2c_client *client)
{
	int i_ret = 0;
	int lcd_cfg_size = 0;

	FTS_DEBUG("[UPGRADE]**********upgrade with lcd_cfg.i**********");

	lcd_cfg_size = fts_getsize(LCD_CFG_SIZE);
	if (lcd_cfg_size < LCD_CFG_MIN_SIZE
		|| lcd_cfg_size > LCD_CFG_MAX_SIZE) {
		FTS_ERROR("[UPGRADE] lcd_cfg.i length(%x) error", lcd_cfg_size);
		return -EIO;
	}

	/*FW upgrade*/
	i_ret = fts_ctpm_lcd_cfg_upgrade_use_buf(client,
					CTPM_LCD_CFG, lcd_cfg_size);
	if (i_ret != 0)
		FTS_ERROR("[UPGRADE] lcd_cfg.i upgrade fail, ret=%d", i_ret);
	else
		FTS_INFO("[UPGRADE] lcd_cfg.i upgrade succeed");

	return i_ret;
}

/************************************************************************
 * Name: fts_ctpm_fw_upgrade_with_lcd_cfg_bin_file
 * Brief:  upgrade with *.bin file
 * Input: i2c info, file name
 * Output: no
 * Return: success =0
 ***********************************************************************/
static int fts_ctpm_fw_upgrade_with_lcd_cfg_bin_file(struct i2c_client *client,
						char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	bool ecc_ok = false;
	int lcd_cfg_size = fts_GetFirmwareSize(firmware_name);

	FTS_DEBUG("[UPGRADE]**********upgrade with lcd_cfg.bin**********");

	if (lcd_cfg_size < LCD_CFG_MIN_SIZE
			|| lcd_cfg_size > LCD_CFG_MAX_SIZE) {
		FTS_ERROR("[UPGRADE] lcd_cfg.bin length(%x) error",
				lcd_cfg_size);
		return -EIO;
	}

	pbt_buf = kmalloc(lcd_cfg_size + 1, GFP_KERNEL);
	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		FTS_ERROR("[UPGRADE]: request_firmware failed!!");
		goto ERROR_LCD_CFG_BIN;
	}

	/*check the app.bin invalid or not*/
	ecc_ok = 1;

	if (ecc_ok) {
		FTS_INFO("[UPGRADE]: lcd_cfg.bin ecc ok!!");
		i_ret = fts_ctpm_lcd_cfg_upgrade_use_buf(client,
						pbt_buf, lcd_cfg_size);
		if (i_ret != 0) {
			FTS_ERROR("[UPGRADE]: lcd_cfg.bin upgrade failed!!");
			goto ERROR_LCD_CFG_BIN;
		} else {
			FTS_INFO("[UPGRADE]: lcd_cfg.bin upgrade succeed!!");
		}
	} else {
		FTS_ERROR("[UPGRADE]: lcd_cfg.bin ecc failed!!");

	}

	kfree(pbt_buf);
	return i_ret;

ERROR_LCD_CFG_BIN:
	kfree(pbt_buf);
	return -EIO;
}
#endif  /* #if (FTS_CHIP_TYPE == _FT8006) */
