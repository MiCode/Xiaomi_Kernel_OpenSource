/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2018, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: global configurations
*
* Version: v1.0
*
************************************************************************/
#ifndef _LINUX_FOCLATECH_CONFIG_H_
#define _LINUX_FOCLATECH_CONFIG_H_

/**************************************************/
/****** G: A, I: B, S: C, U: D  ******************/
/****** chip type defines, do not modify *********/
#define _FT5446             0x54460402

/*************************************************/

/*
 * choose your ic chip type of focaltech
 */
#define FTS_CHIP_TYPE   _FT5446

/******************* Enables *********************/
/*********** 1 to enable, 0 to disable ***********/

/*
 * show debug log info
 * enable it for debug, disable it for release
 */
#define FTS_DEBUG_EN                            1

/*
 * Linux MultiTouch Protocol
 * 1: Protocol B(default), 0: Protocol A
 */
#define FTS_MT_PROTOCOL_B_EN                    1

/*
 * Tp_self test enable
 * 1: enable, 0:disable(default)
 */
 #define FTS_TP_SELFTEST                     1

/*
 * Pinctrl enable
 * default: disable
 */
#define FTS_PINCTRL_EN                          0

/*
 * Customer power enable
 * enable it when customer need control TP power
 * default: disable
 */
#define FTS_POWER_SOURCE_CUST_EN                1

/****************************************************/

/********************** Upgrade ****************************/
/*
 * Check vendor_id number
 * 0:No check vendor_id (default)
 * 1/2/3: Check vendor_id for vendor compatibility
 */
#define FTS_GET_VENDOR_ID_NUM                   0

/*
 * vendor_id(s) for vendor(s) to be compatible with.
 * a confirmation of vendor_id(s) is recommended.
 * FTS_VENDOR_ID = PANEL_ID << 8 + VENDOR_ID
 * FTS_GET_VENDOR_ID_NUM == 0/1, no check vendor id, you may ignore them
 * FTS_GET_VENDOR_ID_NUM > 1, compatible with FTS_VENDOR_ID
 * FTS_GET_VENDOR_ID_NUM >= 2, compatible with FTS_VENDOR_ID2
 * FTS_GET_VENDOR_ID_NUM >= 3, compatible with FTS_VENDOR_ID3
 */
#define FTS_VENDOR_ID                          0x0000
#define FTS_VENDOR_ID2                         0x6d
#define FTS_VENDOR_ID3                         0x0000

/*********************************************************/

#endif /* _LINUX_FOCLATECH_CONFIG_H_ */
