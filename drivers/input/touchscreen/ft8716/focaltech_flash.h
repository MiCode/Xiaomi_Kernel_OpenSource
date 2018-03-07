/************************************************************************
* Copyright (C) 2012-2016, Focaltech Systems (R)£¬All Rights Reserved.
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
#define FTS_MAX_TRIES                                5
#define FTS_RETRY_DLY                                20
#define FTS_MAX_WR_BUF                               10
#define FTS_MAX_RD_BUF                               2
#define FTS_FW_PKT_META_LEN                          6
#define FTS_FW_PKT_DLY_MS                            20
#define FTS_FW_LAST_PKT                              0x6ffa
#define FTS_EARSE_DLY_MS                             100
#define FTS_55_AA_DLY_NS                             5000
#define FTS_CAL_START                                0x04
#define FTS_CAL_FIN                                  0x00
#define FTS_CAL_STORE                                0x05
#define FTS_CAL_RETRY                                100
#define FTS_REG_CAL                                  0x00
#define FTS_CAL_MASK                                 0x70
#define FTS_BLOADER_SIZE_OFF                         12
#define FTS_BLOADER_NEW_SIZE                         30
#define FTS_DATA_LEN_OFF_OLD_FW                      8
#define FTS_DATA_LEN_OFF_NEW_FW                      14
#define FTS_FINISHING_PKT_LEN_OLD_FW                 6
#define FTS_FINISHING_PKT_LEN_NEW_FW                 12
#define FTS_MAGIC_BLOADER_Z7                         0x7bfa
#define FTS_MAGIC_BLOADER_LZ4                        0x6ffa
#define FTS_MAGIC_BLOADER_GZF_30                     0x7ff4
#define FTS_MAGIC_BLOADER_GZF                        0x7bf4
#define FTS_REG_ECC                                  0xCC
#define FTS_RST_CMD_REG2                             0xBC
#define FTS_READ_ID_REG                              0x90
#define FTS_ERASE_APP_REG                            0x61
#define FTS_ERASE_PARAMS_CMD                         0x63
#define FTS_FW_WRITE_CMD                             0xBF
#define FTS_REG_RESET_FW                             0x07
#define FTS_RST_CMD_REG1                             0xFC
#define LEN_FLASH_ECC_MAX                            0xFFFE

#define BL_VERSION_LZ4                               0
#define BL_VERSION_Z7                                1
#define BL_VERSION_GZF                               2

#define FTS_PACKET_LENGTH                            128
#define FTS_SETTING_BUF_LEN                          128

#define FTS_UPGRADE_LOOP                             30
#define FTS_MAX_POINTS_2                             2
#define FTS_MAX_POINTS_5                             5
#define FTS_MAX_POINTS_10                            10
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
	PRAMBOOT_SIZE,
	LCD_CFG_SIZE
};

/* pramboot for 8716 */
#define FTS_PRAMBOOT_8716   "include/pramboot/FT8716_Pramboot_V0.5_20160723.i"
#define FTS_UPGRADE_PRAMBOOT    FTS_PRAMBOOT_8716


#define FTS_PRAMBOOT_E716	"include/pramboot/FT8716_Pramboot_V0.5_20160723.i"
#define FTS_UPGRADE_PRAMBOOT    FTS_PRAMBOOT_8716




/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
/* IC info */

struct fts_upgrade_fun {
	int (*get_app_bin_file_ver)(char *);
	int (*get_app_i_file_ver)(void);
	int (*upgrade_with_app_i_file)(struct i2c_client *);
	int (*upgrade_with_app_bin_file)(struct i2c_client *, char *);
	int (*upgrade_with_lcd_cfg_i_file)(struct i2c_client *);
	int (*upgrade_with_lcd_cfg_bin_file)(struct i2c_client *, char *);
};
extern struct fts_upgrade_fun fts_updatefun;

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern unsigned char CTPM_FW[];
extern unsigned char aucFW_PRAM_BOOT[];
extern unsigned char CTPM_LCD_CFG[];

extern struct fts_upgrade_fun  fts_updatefun_curr;
extern struct ft_chip_t chip_types;

#if FTS_AUTO_UPGRADE_EN
extern struct workqueue_struct *touch_wq;
extern struct work_struct fw_update_work;
#endif

void fts_ctpm_upgrade_init(void);
void fts_ctpm_upgrade_exit(void);
void fts_ctpm_upgrade_delay(u32 i);
void fts_ctpm_get_upgrade_array(void);
int fts_ctpm_auto_upgrade(struct i2c_client *client);
int fts_fw_upgrade(struct device *dev, bool force);
int fts_ctpm_auto_clb_sharp(struct i2c_client *client);

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
u32 fts_getsize(u8 fw_type);
int fts_GetFirmwareSize(char *firmware_name);
int fts_ctpm_i2c_hid2std(struct i2c_client *client);
int fts_ReadFirmware(char *firmware_name, unsigned char *firmware_buf);
void fts_ctpm_rom_or_pram_reset(struct i2c_client *client);
enum FW_STATUS fts_ctpm_get_pram_or_rom_id(struct i2c_client *client);
#endif


