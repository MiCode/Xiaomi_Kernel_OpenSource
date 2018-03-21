/************************************************************************
* Copyright (C) 2010-2017, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_flash.h
*
*    Author: fupeipei
*
*   Created: 2016-08-07
*
*  Abstract:
*
************************************************************************/
#ifndef __LINUX_FOCALTECH_FLASH_H__
#define __LINUX_FOCALTECH_FLASH_H__

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_flash/focaltech_upgrade_common.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_REG_ECC                                  0xCC
#define FTS_RST_CMD_REG2                             0xBC
#define FTS_READ_ID_REG                              0x90
#define FTS_ERASE_APP_REG                            0x61
#define FTS_ERASE_PARAMS_CMD                         0x63
#define FTS_FW_WRITE_CMD                             0xBF
#define FTS_REG_RESET_FW                             0x07
#define FTS_RST_CMD_REG1                             0xFC
#define LEN_FLASH_ECC_MAX                            0xFFFE

#define FTS_PACKET_LENGTH                            128
#define FTS_SETTING_BUF_LEN                          128

#define FTS_UPGRADE_LOOP                             30
#define AUTO_CLB_NEED                                1
#define AUTO_CLB_NONEED                              0
#define FTS_UPGRADE_AA                               0xAA
#define FTS_UPGRADE_55                               0x55
#define FTXXXX_INI_FILEPATH_CONFIG                   "/sdcard/"

enum FW_STATUS {
	FTS_RUN_IN_ERROR,
	FTS_RUN_IN_APP,
	FTS_RUN_IN_ROM,
	FTS_RUN_IN_PRAM,
	FTS_RUN_IN_BOOTLOADER
};

enum FILE_SIZE_TYPE {
	FW_SIZE,
	FW2_SIZE,
	FW3_SIZE,
	PRAMBOOT_SIZE,
};

/* pramboot for 8716 */
#define FTS_PRAMBOOT_8716   "include/pramboot/FT8716_Pramboot_V0.5_20160723.i"
#define FTS_UPGRADE_PRAMBOOT    FTS_PRAMBOOT_8716


#define FTS_PRAMBOOT_E716   "include/pramboot/FT8716_Pramboot_V0.5_20160723.i"
#define FTS_UPGRADE_PRAMBOOT    FTS_PRAMBOOT_8716




/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
/* IC info */

struct fts_upgrade_fun {
	int (*get_i_file)(struct i2c_client *, int);
	int (*get_app_bin_file_ver)(char *);
	int (*get_app_i_file_ver)(void);
	int (*upgrade_with_app_i_file)(struct i2c_client *);
	int (*upgrade_with_app_bin_file)(struct i2c_client *, char *);
	int (*get_hlic_ver)(void);
	int (*upgrade_with_lcd_cfg_i_file)(struct i2c_client *);
	int (*upgrade_with_lcd_cfg_bin_file)(struct i2c_client *, char *);
};
extern struct fts_upgrade_fun ft8006m_updatefun;

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern u8 FT8006M_CTPM_FW[];
extern u8 FT8006M_CTPM_FW_WHITE[];
extern u8 FT8006M_CTPM_FW_BLACK[];
extern u8 FT8006M_FT8006M_CTPM_FW2[];
extern u8 FT8006M_FT8006M_CTPM_FW3[];
extern u8 ft8006m_aucFW_PRAM_BOOT[];
extern u8 FT8006M_CTPM_LCD_CFG[];
extern u8 *ft8006m_g_fw_file;
extern int ft8006m_g_fw_len;
extern struct fts_upgrade_fun  ft8006m_updatefun_curr;
extern struct ft_chip_t ft8006m_chip_types;

#if FTS_AUTO_UPGRADE_EN
extern struct workqueue_struct *ft8006m_touch_wq;
extern struct work_struct ft8006m_fw_update_work;
#endif

void ft8006m_ctpm_upgrade_init(void);
void ft8006m_ctpm_upgrade_exit(void);
void ft8006m_ctpm_upgrade_delay(u32 i);
void ft8006m_ctpm_get_upgrade_array(void);
int ft8006m_ctpm_auto_upgrade(struct i2c_client *client);
int ft8006m_fw_upgrade(struct device *dev, bool force);
int ft8006m_ctpm_auto_clb(struct i2c_client *client);

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
u32 ft8006m_getsize(u8 fw_type);
int ft8006m_GetFirmwareSize(char *firmware_name);
int ft8006m_ctpm_i2c_hid2std(struct i2c_client *client);
int ft8006m_ReadFirmware(char *firmware_name, u8 *firmware_buf);
void ft8006m_ctpm_rom_or_pram_reset(struct i2c_client *client);
enum FW_STATUS ft8006m_ctpm_get_pram_or_rom_id(struct i2c_client *client);
#endif


