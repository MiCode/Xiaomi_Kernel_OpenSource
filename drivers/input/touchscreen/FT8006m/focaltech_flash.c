/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
* File Name: focaltech_flash.c
*
* Author:    fupeipei
*
* Created:    2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"
#include "focaltech_flash.h"

/*****************************************************************************
* Static variables
*****************************************************************************/
struct ft_chip_t ft8006m_chip_types;
u8 ft8006m_fw_upgrade_status = FW_NO_UPGRADE;
/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
/* Upgrade FW/PRAMBOOT/LCD CFG */
u8 FT8006M_CTPM_FW[] = {
#include FTS_UPGRADE_FW_APP
};

u8 FT8006M_CTPM_FW_WHITE[] = {
#include FTS_UPGRADE_FW_ALL_WHITE
};

u8 FT8006M_CTPM_FW_BLACK[] = {
#include FTS_UPGRADE_FW_ALL_BLACK
};
#if (FTS_GET_VENDOR_ID_NUM >= 2)
u8 FT8006M_FT8006M_CTPM_FW2[] = {
#include FTS_UPGRADE_FW2_APP
};
#endif

#if (FTS_GET_VENDOR_ID_NUM >= 3)
u8 FT8006M_FT8006M_CTPM_FW3[] = {
#include FTS_UPGRADE_FW3_APP
};
#endif

u8 ft8006m_aucFW_PRAM_BOOT[] = {
#ifdef FTS_UPGRADE_PRAMBOOT
#include FTS_UPGRADE_PRAMBOOT
#endif
};

struct fts_upgrade_fun  ft8006m_updatefun_curr;
struct workqueue_struct *ft8006m_touch_wq;
struct work_struct ft8006m_fw_update_work;
u8 *ft8006m_g_fw_file;
int ft8006m_g_fw_len;
/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/************************************************************************
* Name: ft8006m_ctpm_upgrade_delay
* Brief: 0
* Input: 0
* Output: 0
* Return: 0
***********************************************************************/
void ft8006m_ctpm_upgrade_delay(u32 i)
{
	do {
		i--;
	} while (i > 0);
}

/************************************************************************
* Name: ft8006m_ctpm_i2c_hid2std
* Brief:  HID to I2C
* Input: i2c info
* Output: no
* Return: fail =0
***********************************************************************/
int ft8006m_ctpm_i2c_hid2std(struct i2c_client *client)
{
#if (FTS_CHIP_IDC)
	return 0;
#else
	u8 buf[5] = {0};
	int bRet = 0;

	buf[0] = 0xeb;
	buf[1] = 0xaa;
	buf[2] = 0x09;
	bRet = ft8006m_i2c_write(client, buf, 3);
	msleep(10);
	buf[0] = buf[1] = buf[2] = 0;
	ft8006m_i2c_read(client, buf, 0, buf, 3);

	if ((0xeb == buf[0]) && (0xaa == buf[1]) && (0x08 == buf[2])) {
		FTS_DEBUG("hidi2c change to stdi2c successful!!");
		bRet = 1;
	} else {
		FTS_ERROR("hidi2c change to stdi2c error!!");
		bRet = 0;
	}

	return bRet;
#endif
}

/************************************************************************
* Name: fts_get_ft8006m_chip_types
* Brief: get correct chip information
* Input:
* Output:
* Return:
***********************************************************************/
void fts_get_ft8006m_chip_types(void)
{
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	int ic_type = 0;

	if (sizeof(ctype) != sizeof(struct ft_chip_t)) /* only one array */
		ic_type = IC_SERIALS - 1;

	ft8006m_chip_types = ctype[ic_type];

	FTS_INFO("CHIP TYPE ID = 0x%02x%02x", ft8006m_chip_types.chip_idh, ft8006m_chip_types.chip_idl);
}

/************************************************************************
* Name: ft8006m_ctpm_get_upgrade_array
* Brief: decide which ic
* Input: no
* Output: get ic info in fts_updateinfo_curr
* Return: no
***********************************************************************/
void ft8006m_ctpm_get_upgrade_array(void)
{

	FTS_FUNC_ENTER();

	fts_get_ft8006m_chip_types();

	ft8006m_ctpm_i2c_hid2std(ft8006m_i2c_client);

	/* Get functin pointer */
	memcpy(&ft8006m_updatefun_curr, &ft8006m_updatefun, sizeof(struct fts_upgrade_fun));

	FTS_FUNC_EXIT();
}

/************************************************************************
* Name: ft8006m_ctpm_rom_or_pram_reset
* Brief: RST CMD(07), reset to romboot(maybe->bootloader)
* Input:
* Output:
* Return:
***********************************************************************/
void ft8006m_ctpm_rom_or_pram_reset(struct i2c_client *client)
{
	u8 rst_cmd = FTS_REG_RESET_FW;

	FTS_INFO("[UPGRADE]******Reset to romboot/bootloader******");
	ft8006m_i2c_write(client, &rst_cmd, 1);
	/* The delay can't be changed */
	msleep(300);
}

/************************************************************************
* Name: ft8006m_ctpm_auto_clb
* Brief:  auto calibration
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
int ft8006m_ctpm_auto_clb(struct i2c_client *client)
{
#if FTS_AUTO_CLB_EN
	u8 uc_temp = 0x00;
	u8 i = 0;

	/*start auto CLB */
	msleep(200);

	ft8006m_i2c_write_reg(client, 0, FTS_REG_WORKMODE_FACTORY_VALUE);
	/*make sure already enter factory mode */
	msleep(100);
	/*write command to start calibration */
	ft8006m_i2c_write_reg(client, 2, 0x4);
	msleep(300);
	if ((ft8006m_chip_types.chip_idh == 0x11) || (ft8006m_chip_types.chip_idh == 0x12) || (ft8006m_chip_types.chip_idh == 0x13) || (ft8006m_chip_types.chip_idh == 0x14)) {
		for (i = 0; i < 100; i++) {
			ft8006m_i2c_read_reg(client, 0x02, &uc_temp);
			if (0x02 == uc_temp ||
				0xFF == uc_temp) {
				break;
			}
			msleep(20);
		}
	} else {
		for (i = 0; i < 100; i++) {
			ft8006m_i2c_read_reg(client, 0, &uc_temp);
			if (0x0 == ((uc_temp&0x70)>>4)) {
				break;
			}
			msleep(20);
		}
	}
	ft8006m_i2c_write_reg(client, 0, 0x40);
	msleep(200);
	ft8006m_i2c_write_reg(client, 2, 0x5);
	msleep(300);
	ft8006m_i2c_write_reg(client, 0, FTS_REG_WORKMODE_WORK_VALUE);
	msleep(300);
#endif

	return 0;
}

/************************************************************************
* Name: ft8006m_GetFirmwareSize
* Brief:  get file size
* Input: file name
* Output: no
* Return: file size
***********************************************************************/
int ft8006m_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[FILE_NAME_LENGTH];

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTXXXX_INI_FILEPATH_CONFIG, firmware_name);
	if (NULL == pfile) {
		pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_ERROR("error occured while opening file %s", filepath);
		return -EIO;
	}
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

/************************************************************************
* Name: ft8006m_ReadFirmware
* Brief:  read firmware buf for .bin file.
* Input: file name, data buf
* Output: data buf
* Return: 0
***********************************************************************/
int ft8006m_ReadFirmware(char *firmware_name, u8 *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[FILE_NAME_LENGTH];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTXXXX_INI_FILEPATH_CONFIG, firmware_name);
	if (NULL == pfile) {
		pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_ERROR("[UPGRADE] Error occured while opening file %s.\n", filepath);
		return -EIO;
	}
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);
	return 0;
}

/************************************************************************
* Name: ft8006m_getsize
* Brief: Get different file's size
* Input:
* Output:
* Return: file's size
***********************************************************************/
u32 ft8006m_getsize(u8 fw_type)
{
	int fw_len = 0;

	if (fw_type == FW_SIZE)
		fw_len = sizeof(FT8006M_CTPM_FW_WHITE);
#if (FTS_GET_VENDOR_ID_NUM >= 2)
	else if (fw_type == FW2_SIZE)
		fw_len = sizeof(FT8006M_FT8006M_CTPM_FW2);
#endif
#if (FTS_GET_VENDOR_ID_NUM >= 3)
	else if (fw_type == FW3_SIZE)
		fw_len = sizeof(FT8006M_FT8006M_CTPM_FW3);
#endif
#if FTS_CHIP_IDC
	else if (fw_type == PRAMBOOT_SIZE)
		fw_len = sizeof(ft8006m_aucFW_PRAM_BOOT);
#endif

	return fw_len;
}

/************************************************************************
* Name: ft8006m_ctpm_get_pram_or_rom_id
* Brief: 0
* Input: 0
* Output: 0
* Return: 0
***********************************************************************/
enum FW_STATUS ft8006m_ctpm_get_pram_or_rom_id(struct i2c_client *client)
{
	u8 buf[4];
	u8 reg_val[2] = {0};
	enum FW_STATUS inRomBoot = FTS_RUN_IN_ERROR;

	ft8006m_ctpm_i2c_hid2std(client);

	/*Enter upgrade mode*/
	/*send 0x55 in time windows*/
	buf[0] = FTS_UPGRADE_55;
	buf[1] = FTS_UPGRADE_AA;
	ft8006m_i2c_write(client, buf, 2);

	msleep(20);

	buf[0] = 0x90;
	buf[1] = buf[2] = buf[3] = 0x00;
	ft8006m_i2c_read(client, buf, 4, reg_val, 2);

	FTS_DEBUG("[UPGRADE] Read ROM/PRAM/Bootloader id:0x%02x%02x", reg_val[0], reg_val[1]);
	if ((reg_val[0] == 0x00) || (reg_val[0] == 0xFF)) {
		inRomBoot = FTS_RUN_IN_ERROR;
	} else if (reg_val[0] == ft8006m_chip_types.pramboot_idh && reg_val[1] == ft8006m_chip_types.pramboot_idl) {
		inRomBoot = FTS_RUN_IN_PRAM;
	} else if (reg_val[0] == ft8006m_chip_types.rom_idh && reg_val[1] == ft8006m_chip_types.rom_idl) {
		inRomBoot = FTS_RUN_IN_ROM;
	} else if (reg_val[0] == ft8006m_chip_types.bootloader_idh && reg_val[1] == ft8006m_chip_types.bootloader_idl) {
		inRomBoot = FTS_RUN_IN_BOOTLOADER;
	}

	return inRomBoot;
}

/************************************************************************
* Name: fts_ctpm_get_app_file
* Brief: get app file by Vendor ID
* Input:
* Output:
* Return: <0: vendor id not correct,not upgrade
***********************************************************************/
int ft8006m_ctpm_get_i_file(struct i2c_client *client, int fw_valid)
{
	int ret;

	if (ft8006m_updatefun_curr.get_i_file)
		ret = ft8006m_updatefun_curr.get_i_file(client, fw_valid);
	else
		ret = -EIO;

	return ret;
}

/************************************************************************
* Name: ft8006m_ctpm_get_app_ver
* Brief:  get app file version
* Input:
* Output:
* Return: fw version
***********************************************************************/
int ft8006m_ctpm_get_app_ver(void)
{
	int i_ret = 0;

	if (ft8006m_updatefun_curr.get_app_i_file_ver)
		i_ret = ft8006m_updatefun_curr.get_app_i_file_ver();

	return i_ret;
}

/************************************************************************
* Name: ft8006m_ctpm_fw_upgrade
* Brief:  fw upgrade entry funciotn
* Input:
* Output:
* Return: 0  - upgrade successfully
*         <0 - upgrade failed
***********************************************************************/
int ft8006m_ctpm_fw_upgrade(struct i2c_client *client)
{
	int i_ret = 0;

	if (ft8006m_updatefun_curr.upgrade_with_app_i_file)
		i_ret = ft8006m_updatefun_curr.upgrade_with_app_i_file(client);

	return i_ret;
}

/************************************************************************
* Name: ft8006m_ctpm_fw_upgrade
* Brief:  fw upgrade entry funciotn
* Input:
* Output:
* Return: 0  - upgrade successfully
*         <0 - upgrade failed
***********************************************************************/
int ft8006m_ctpm_lcd_cfg_upgrade(struct i2c_client *client)
{
	int i_ret = 0;

	if (ft8006m_updatefun_curr.upgrade_with_lcd_cfg_i_file)
		i_ret = ft8006m_updatefun_curr.upgrade_with_lcd_cfg_i_file(client);

	return i_ret;
}

bool fts_check_fw_valid(struct i2c_client *client)
{
	int i = 0;
	u8 chip_id = 0;

	for (i = 0; i < 3; i++) {
		ft8006m_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id);
		if (chip_id == ft8006m_chip_types.chip_idh)
			return true;
	}

	return false;
}

#if (!(FTS_UPGRADE_STRESS_TEST))
/************************************************************************
* Name: fts_ctpm_check_fw_status
* Brief: Check App is valid or not
* Input:
* Output:
* Return: -EIO - I2C communication error
*         FTS_RUN_IN_APP - APP valid
*         0 - APP invalid
***********************************************************************/
static int fts_ctpm_check_fw_status(struct i2c_client *client)
{
	u8 chip_id1 = 0;
	u8 chip_id2 = 0;
	int fw_status = FTS_RUN_IN_ERROR;
	int i = 0;
	int ret = 0;
	int i2c_noack_retry = 0;

	for (i = 0; i < 5; i++) {
		ret = ft8006m_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id1);
		if (ret < 0) {
			i2c_noack_retry++;
			continue;
		}
		ret = ft8006m_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id2);
		if (ret < 0) {
			i2c_noack_retry++;
			continue;
		}

		if ((chip_id1 == ft8006m_chip_types.chip_idh)
#if FTS_CHIP_IDC
			&& (chip_id2 == ft8006m_chip_types.chip_idl)
#endif
		   ) {
			fw_status = FTS_RUN_IN_APP;
			break;
		}
	}

	FTS_DEBUG("[UPGRADE]: chip_id = %02x%02x, ft8006m_chip_types.chip_idh = %02x%02x",
			 chip_id1, chip_id2, ft8006m_chip_types.chip_idh, ft8006m_chip_types.chip_idl);

	/* I2C No ACK 5 times, then return -EIO */
	if (i2c_noack_retry >= 5)
		return -EIO;

	/* I2C communication ok, but not get correct ID, need check pram/rom/bootloader */
	if (i >= 5) {
		fw_status = ft8006m_ctpm_get_pram_or_rom_id(client);
	}

	return fw_status;
}

/************************************************************************
* Name: fts_ctpm_check_fw_ver
* Brief: Check vendor id is valid or not
* Input:
* Output:
* Return: 1 - vendor id valid
*         0 - vendor id invalid
***********************************************************************/
static int fts_ctpm_check_fw_ver(struct i2c_client *client)
{
	u8 uc_tp_fm_ver;
	u8 uc_host_fm_ver = 0;

	ft8006m_i2c_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
	uc_host_fm_ver = ft8006m_ctpm_get_app_ver();

	FTS_DEBUG("[UPGRADE]: uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x!!", uc_tp_fm_ver, uc_host_fm_ver);
	if (uc_tp_fm_ver < uc_host_fm_ver) {
		return 1;
	} else {
		return 0;
	}
}

/************************************************************************
* Name: fts_ctpm_check_need_upgrade
* Brief:
* Input:
* Output:
* Return: 1 - Need upgrade
*         0 - No upgrade
***********************************************************************/
static int fts_ctpm_check_need_upgrade(struct i2c_client *client)
{
	int fw_status = 0;
	int bUpgradeFlag = false;

	FTS_FUNC_ENTER();

	/* 1. veriry FW APP is valid or not */
	fw_status = fts_ctpm_check_fw_status(client);
	FTS_DEBUG("[UPGRADE]: fw_status = %d!!", fw_status);
	if (fw_status < 0) {
		/* I2C no ACK, return immediately */
		FTS_ERROR("[UPGRADE]******I2C NO ACK,exit upgrade******");
		return -EIO;
	} else if (fw_status == FTS_RUN_IN_ERROR) {
		FTS_ERROR("[UPGRADE]******IC Type Fail******");
	} else if (fw_status == FTS_RUN_IN_APP) {
		FTS_INFO("[UPGRADE]**********FW APP valid**********");

		if (ft8006m_ctpm_get_i_file(client, 1) != 0) {
			FTS_DEBUG("[UPGRADE]******Get upgrade file(fw) fail******");
			return -EIO;
		}

		if (fts_ctpm_check_fw_ver(client) == 1) {
			FTS_DEBUG("[UPGRADE]**********need upgrade fw**********");
			bUpgradeFlag = true;
		} else {
			FTS_DEBUG("[UPGRADE]**********Don't need upgrade fw**********");
			bUpgradeFlag = false;
		}
	} else {
		/* if app is invalid, reset to run ROM */
		FTS_INFO("[UPGRADE]**********FW APP invalid**********");
		ft8006m_ctpm_rom_or_pram_reset(client);
		if (ft8006m_ctpm_get_i_file(client, 0) != 0) {
			FTS_DEBUG("[UPGRADE]******Get upgrade file(flash) fail******");
			ft8006m_ctpm_rom_or_pram_reset(client);
			return -EIO;
		}
		ft8006m_ctpm_rom_or_pram_reset(client);
		bUpgradeFlag = true;
	}

	FTS_FUNC_EXIT();

	return bUpgradeFlag;
}

/************************************************************************
* Name: ft8006m_ctpm_auto_upgrade
* Brief:  auto upgrade
* Input:
* Output:
* Return: 0 - no upgrade
***********************************************************************/
int ft8006m_ctpm_auto_upgrade(struct i2c_client *client)
{
	u8 uc_tp_fm_ver;
	int i_ret = 0;
	int bUpgradeFlag = false;
	u8 uc_upgrade_times = 0;

	FTS_DEBUG("[UPGRADE]********************check upgrade need or not********************");
	bUpgradeFlag = fts_ctpm_check_need_upgrade(client);
	FTS_DEBUG("[UPGRADE]**********bUpgradeFlag = 0x%x**********", bUpgradeFlag);
	if (bUpgradeFlag <= 0) {
		FTS_DEBUG("[UPGRADE]**********No Upgrade, exit**********");

		return bUpgradeFlag;
	} else {
		/* FW Upgrade */
	ft8006m_fw_upgrade_status = FW_UPGRADING;
		do {
			uc_upgrade_times++;
			FTS_DEBUG("[UPGRADE]********************star upgrade(%d)********************", uc_upgrade_times);

			i_ret = ft8006m_ctpm_fw_upgrade(client);
			if (i_ret == 0) {
				/* upgrade success */
				ft8006m_i2c_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
				FTS_DEBUG("[UPGRADE]********************Success upgrade to new fw version 0x%x********************", uc_tp_fm_ver);

				ft8006m_ctpm_auto_clb(client);
				break;
			} else {
				/* upgrade fail, reset to run ROM BOOT..
				* if app in flash is ok, TP will work success
				*/
				FTS_ERROR("[UPGRADE]********************upgrade fail, reset now********************");
				ft8006m_ctpm_rom_or_pram_reset(client);
			}
		} while (uc_upgrade_times < 2);  /* if upgrade fail, upgrade again. then return */
	ft8006m_fw_upgrade_status = FW_UPGRADED;
	}

	return i_ret;
}
#endif

#if FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN
int fts_get_host_lic_ver(u8 *ver)
{
	int i_ret = 0;

	if (ft8006m_updatefun_curr.get_hlic_ver)
		i_ret = ft8006m_updatefun_curr.get_hlic_ver();

	if (i_ret > 0)
		*ver = i_ret;
	return i_ret;
}

/* check if lcd init code need upgrade
* true-need  false-no need
*/
static bool fts_lic_need_upgrade(struct i2c_client *client)
{
	int ret = 0;
	u8 initcode_ver_in_tp = 0;
	u8 initcode_ver_in_host = 0;
	bool fwvalid = false;

	fwvalid = fts_check_fw_valid(client);
	if (!fwvalid) {
		FTS_INFO("fw is invalid, no upgrade lcd init code");
		return false;
	}

	ret = fts_get_host_lic_ver(&initcode_ver_in_host);
	if (ret < 0) {
		FTS_ERROR("init code in host invalid");
		return false;
	}

	ret = ft8006m_i2c_read_reg(client, 0xE4, &initcode_ver_in_tp);
	if (ret < 0) {
		FTS_ERROR("read reg0xE4 fail");
		return false;
	}

	FTS_DEBUG("tp init ver:%x, fw init ver:%x", initcode_ver_in_tp, initcode_ver_in_host);
	if (0xA5 == initcode_ver_in_tp) {
		FTS_INFO("lcd init code ver is 0xA5, don't upgade init code");
		return false;
	} else if (0xFF == initcode_ver_in_tp) {
		FTS_DEBUG("lcd init code in tp is invalid, need upgrade init code");
		return true;
	} else if (initcode_ver_in_tp <  initcode_ver_in_host)
		return true;
	else
		return false;
}

int fts_lic_upgrade(struct i2c_client *client)
{
	int ret = 0;
	bool hlic_upgrade = false;
	int upgrade_count = 0;

	hlic_upgrade = fts_lic_need_upgrade(client);
	FTS_INFO("lcd init code upgrade flag:%d", hlic_upgrade);
	if (hlic_upgrade) {
		FTS_INFO("lcd init code upgrade...");
		do {
			upgrade_count++;
			ret = ft8006m_ctpm_lcd_cfg_upgrade(client);
			if (0 == ret) {
				FTS_INFO("lcd init code upgrade succussfully");
				break;
			} else {
				ft8006m_ctpm_rom_or_pram_reset(client);
			}
		} while (upgrade_count < 2);
	}

	return ret;
}
#endif /* FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN */

#if FTS_AUTO_UPGRADE_EN
static void fts_ctpm_update_work_func(struct work_struct *work)
{
	int i_ret = 0;

	FTS_DEBUG("[UPGRADE]******************************FTS enter upgrade******************************");
	ft8006m_irq_disable();

	/* esd check */
#if FTS_ESDCHECK_EN
	ft8006m_esdcheck_switch(DISABLE);
#endif

	i_ret = ft8006m_ctpm_auto_upgrade(ft8006m_i2c_client);
	if (i_ret < 0)
		FTS_ERROR("[UPGRADE]**********TP FW upgrade failed**********");

#if FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN

	if (i_ret >= 0) {
	i_ret = fts_lic_upgrade(ft8006m_i2c_client);
	if (i_ret < 0)
	FTS_ERROR("**********lcd init code upgrade failed**********");
	}
#endif

#if FTS_ESDCHECK_EN
	ft8006m_esdcheck_switch(ENABLE);
#endif
	ft8006m_irq_enable();

	FTS_DEBUG("[UPGRADE]******************************FTS exit upgrade******************************");
}

/*****************************************************************************
*  Name: ft8006m_ctpm_upgrade_init
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void ft8006m_ctpm_upgrade_init(void)
{
	FTS_FUNC_ENTER();

	ft8006m_touch_wq = create_singlethread_workqueue("ft8006m_touch_wq");
	if (ft8006m_touch_wq) {
		INIT_WORK(&ft8006m_fw_update_work, fts_ctpm_update_work_func);
		queue_work(ft8006m_touch_wq, &ft8006m_fw_update_work);
	} else {
		FTS_ERROR("[UPGRADE]create_singlethread_workqueue failed\n");
	}

	FTS_FUNC_EXIT();
}

/*****************************************************************************
*  Name: ft8006m_ctpm_upgrade_exit
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void ft8006m_ctpm_upgrade_exit(void)
{
	FTS_FUNC_ENTER();
	destroy_workqueue(ft8006m_touch_wq);
	FTS_FUNC_EXIT();
}

#endif  /* #if FTS_AUTO_UPGRADE_EN */
