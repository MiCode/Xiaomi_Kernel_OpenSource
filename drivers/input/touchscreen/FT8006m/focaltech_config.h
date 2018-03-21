/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
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
/************************************************************************
*
* File Name: focaltech_config.h
*
*    Author: Focaltech Driver Team
*
*   Created: 2016-08-08
*
*  Abstract: global configurations
*
*   Version: v1.0
*
************************************************************************/
#ifndef _LINUX_FOCLATECH_CONFIG_H_
#define _LINUX_FOCLATECH_CONFIG_H_

/**************************************************/
/****** G: A, I: B, S: C, U: D  ******************/
/****** chip type defines, do not modify *********/
#define _FT8716     0x87160805
#define _FT8736     0x87360806
#define _FT8006     0x80060807
#define _FT8606     0x86060808
#define _FT8607     0x86070809
#define _FTE716     0xE716080a

#define _FT5416     0x54160002
#define _FT5426     0x54260002
#define _FT5435     0x54350002
#define _FT5436     0x54360002
#define _FT5526     0x55260002
#define _FT5526I    0x5526B002
#define _FT5446     0x54460002
#define _FT5346     0x53460002
#define _FT5446I    0x5446B002
#define _FT5346I    0x5346B002
#define _FT7661     0x76610002
#define _FT7511     0x75110002
#define _FT7421     0x74210002
#define _FT7681     0x76810002
#define _FT3C47U    0x3C47D002
#define _FT3417     0x34170002
#define _FT3517     0x35170002
#define _FT3327     0x33270002
#define _FT3427     0x34270002

#define _FT5626     0x56260001
#define _FT5726     0x57260001
#define _FT5826B    0x5826B001
#define _FT5826S    0x5826C001
#define _FT7811     0x78110001
#define _FT3D47     0x3D470001
#define _FT3617     0x36170001
#define _FT3717     0x37170001
#define _FT3817B    0x3817B001

#define _FT6236U    0x6236D003
#define _FT6336G    0x6336A003
#define _FT6336U    0x6336D003
#define _FT6436U    0x6436D003

#define _FT3267     0x32670004
#define _FT3367     0x33670004



/*************************************************/

/*
 * choose your ic chip type of focaltech
 */
#define FTS_CHIP_TYPE   _FT8006

/******************* Enables *********************/
/*********** 1 to enable, 0 to disable ***********/

/*
 * show debug log info
 * enable it for debug, disable it for release
 */
#define FTS_DEBUG_EN                            1

#define FTS_LOCK_DOWN_INFO			1

#define FTS_CAT_RAWDATA			1

/*
 * Linux MultiTouch Protocol
 * 1: Protocol B(default), 0: Protocol A
 */

#define FTS_MT_PROTOCOL_B_EN                    0


/*
 * Report Pressure in multitouch
 * 1:enable(default),0:disable
*/
#define FTS_REPORT_PRESSURE_EN                  1

/*
 * Force touch support
 * different pressure for multitouch
 * 1: true pressure for force touch
 * 0: constant pressure(default)
 */
#define FTS_FORCE_TOUCH_EN                      0

/*
 * Gesture function enable
 * default: disable
 */
#define FTS_GESTURE_EN                          1

/*
 * ESD check & protection
 * default: disable
 */
#define FTS_ESDCHECK_EN                         1

/*
 * Production test enable
 * 1: enable, 0:disable(default)
 */
#define FTS_TEST_EN                             1

/*
 * Glove mode enable
 * 1: enable, 0:disable(default)
 */
#define FTS_GLOVE_EN                            0
/*
 * cover enable
 * 1: enable, 0:disable(default)
 */
#define FTS_COVER_EN                            0
/*
 * Charger enable
 * 1: enable, 0:disable(default)
 */
#define FTS_CHARGER_EN                          0

/*
 * Proximity sensor
 * default: disable
 */
#define FTS_PSENSOR_EN                          0

/*
 * Nodes for tools, please keep enable
 */
#define FTS_SYSFS_NODE_EN                       1
#define FTS_APK_NODE_EN                         1

/*
 * Customer power enable
 * enable it when customer need control TP power
 * default: disable
 */
#define FTS_POWER_SOURCE_CUST_EN                1

/****************************************************/

/********************** Upgrade ****************************/
/*
 * auto upgrade, please keep enable
 */
#ifdef WT_COMPILE_FACTORY_VERSION
	#define FTS_AUTO_UPGRADE_EN                     0
#else
#define FTS_AUTO_UPGRADE_EN                     1
#endif

/*
 * auto upgrade for lcd cfg
 * default: 0
 */
#ifdef WT_COMPILE_FACTORY_VERSION
	#define FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN         0
#else
#define FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN         1
#endif

/* auto cb check
 * default: disable
 */
#define FTS_AUTO_CLB_EN                         0

/*
 * Check vendor_id number
 * 0:No check vendor_id (default)
 * 1/2/3: Check vendor_id for vendor compatibility
 */
#define FTS_GET_VENDOR_ID_NUM                   0

/*
 * vendor_id(s) for vendor(s) to be compatible with.
 * a confirmation of vendor_id(s) is recommended.
 * FTS_GET_VENDOR_ID_NUM == 0, no check vendor id, you may ignore them
 * FTS_GET_VENDOR_ID_NUM >= 1, compatible with FTS_VENDOR_1_ID
 * FTS_GET_VENDOR_ID_NUM >= 2, compatible with FTS_VENDOR_2_ID
 * FTS_GET_VENDOR_ID_NUM == 3, compatible with FTS_VENDOR_3_ID
 */
#define FTS_VENDOR_1_ID                         0x00
#define FTS_VENDOR_2_ID                         0x00
#define FTS_VENDOR_3_ID                         0x00

/*
 * FW_APP.i file for auto upgrade, you must replace it with your own
 * define your own fw_app, the sample one to be replaced is invalid
 * NOTE: if FTS_GET_VENDOR_ID_NUM >= 1, it's the fw corresponding with FTS_VENDOR_1_ID
 */
#define FTS_UPGRADE_FW_APP                      "include/firmware/FT8006m_app_sample.i"

#define FTS_UPGRADE_FW_ALL_WHITE                      "include/firmware/white_all.i"

#define FTS_UPGRADE_FW_ALL_BLACK                      "include/firmware/black_all.i"


/*
 * if FTS_GET_VENDOR_ID_NUM >= 2, fw corrsponding with FTS_VENDOR_2_ID
 * define your own fw_app, the sample one is invalid
 */
#define FTS_UPGRADE_FW2_APP                     "include/firmware/FT8006m_app_sample.i"

/*
 * if FTS_GET_VENDOR_ID_NUM == 3, fw corrsponding with FTS_VENDOR_3_ID
 * define your own fw_app, the sample one is invalid
 */
#define FTS_UPGRADE_FW3_APP                     "include/firmware/FT8006m_app_sample.i"

/*
 * lcd_cfg.i file for lcd cfg upgrade
 * define your own lcd_cfg.i, the sample one is invalid
 */



/*
 * upgrade stress test for debug
 * enable it for upgrade debug if needed
 * default: disable
 */
#define FTS_UPGRADE_STRESS_TEST                 0
/* stress test times, default: 1000 */
#define FTS_UPGRADE_TEST_NUMBER                 1000

/*********************************************************/

#endif /* _LINUX_FOCLATECH_CONFIG_H_ */

