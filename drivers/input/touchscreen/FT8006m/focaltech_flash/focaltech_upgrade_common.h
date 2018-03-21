/************************************************************************
* Copyright (C) 2010-2017, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: focaltech_upgrade_common.h
*
*    Author: fupeipei
*
*   Created: 2016-08-16
*
*  Abstract:
*
************************************************************************/
#ifndef __LINUX_FOCALTECH_UPGRADE_COMMON_H__
#define __LINUX_FOCALTECH_UPGRADE_COMMON_H__

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "../focaltech_flash.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
int ft8006m_ctpm_erase_flash(struct i2c_client *client);
int ft8006m_ctpm_pramboot_ecc(struct i2c_client *client);
bool ft8006m_ctpm_check_run_state(struct i2c_client *client, int state);
void ft8006m_ctpm_start_pramboot(struct i2c_client *client);
int ft8006m_ctpm_start_fw_upgrade(struct i2c_client *client);
bool ft8006m_ctpm_check_in_pramboot(struct i2c_client *client);
int ft8006m_ctpm_upgrade_idc_init(struct i2c_client *client);
int ft8006m_ctpm_write_app_for_idc(struct i2c_client *client, u32 length, u8 *readbuf);
int ft8006m_ctpm_upgrade_ecc(struct i2c_client *client, u32 startaddr, u32 length);
int ft8006m_ctpm_write_pramboot_for_idc(struct i2c_client *client, u32 length, u8 *readbuf);
int ft8006m_writeflash(struct i2c_client *client, u32 writeaddr, u32 length, u8 *readbuf, u32 cnt);
bool ft8006m_check_app_bin_valid_idc(u8 *pbt_buf);

int ft8006m_ctpm_get_app_ver(void);
int ft8006m_ctpm_fw_upgrade(struct i2c_client *client);
int ft8006m_ctpm_lcd_cfg_upgrade(struct i2c_client *client);

#endif
