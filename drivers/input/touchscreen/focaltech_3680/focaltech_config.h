/* SPDX-License-Identifier: GPL-2.0
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * Copyright (C) 2022 XiaoMi, Inc.
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
#define _FT8716             0x87160805
#define _FT8736             0x87360806
#define _FT8607             0x86070809
#define _FT8006U            0x8006D80B
#define _FT8006S            0x8006A80B
#define _FT8613             0x8613080C
#define _FT8719             0x8719080D
#define _FT8739             0x8739080E
#define _FT8615             0x8615080F
#define _FT8201             0x82010810
#define _FT8201AA           0x8201A810
#define _FT8006P            0x86220811
#define _FT8613S            0x8613C814
#define _FT8756             0x87560815
#define _FT8656             0x86560815
#define _FT8756M            0x8756A815
#define _FT8302             0x83020816
#define _FT8009             0x80090817
#define _FT8006S_AA         0x86320819
#define _FT7250             0x7250081A
#define _FT7120             0x7120081B
#define _FT8720             0x8720081C
#define _FT8726             0x8726081C
#define _FT8720H            0x8720E81C
#define _FT8720M            0x8720F81C
#define _FT8016             0x8016081D
#define _FT2388             0x2388081E
#define _FT8006S_AB         0x8642081F
#define _FT8722             0x87220820
#define _FT8201AB           0x8201B821
#define _FT8203             0x82030821
#define _FT8201M            0x8201C821
#define _FT8006S_AN         0x8006B822
#define _FT7131             0x71310823
#define _FT7250AB           0x7250B824
#define _FT7130             0x71300825
#define _FT8205             0x82050826



#define _FT5426             0x54260402
#define _FT5436             0x54360402
#define _FT5526             0x55260402
#define _FT5446             0x54460402
#define _FT5346             0x53460402
#define _FT7661             0x76610402
#define _FT7511             0x75110402
#define _FT7421             0x74210402
#define _FT7681             0x76810402
#define _FT3417             0x34170402
#define _FT3517             0x35170402
#define _FT3327             0x33270402
#define _FT3427             0x34270402
#define _FT7311             0x73110402
#define _FT5526_V00         0x5526C402

#define _FT5726             0x57260401
#define _FT5826S            0x5826C401
#define _FT7811             0x78110401
#define _FT3617             0x36170401
#define _FT3717             0x37170401

#define _FT6236U            0x6236D003
#define _FT6336G            0x6336A003
#define _FT6336U            0x6336D003
#define _FT6436U            0x6436D003
#define _FT6436T            0x6436E003

#define _FT3267             0x32670004
#define _FT3367             0x33670004

#define _FT5446_Q03         0x5446C482
#define _FT5446_P03         0x5446A481
#define _FT5446_N03         0x5446A489
#define _FT5426_003         0x5426D482
#define _FT5526_003         0x5526D482

#define _FT3518             0x35180481
#define _FT3518U            0x3518D481
#define _FT3558             0x35580481
#define _FT3528             0x35280481
#define _FT5536             0x55360481
#define _FT3418             0x34180481

#define _FT5446U            0x5446D083
#define _FT5456U            0x5456D083
#define _FT3417U            0x3417D083
#define _FT5426U            0x5426D083
#define _FT3428             0x34280083

#define _FT7302             0x73020084
#define _FT7202             0x72020084
#define _FT3308             0x33080084
#define _FT6446             0x64460084

#define _FT6346U            0x6346D085
#define _FT6346G            0x6346A085
#define _FT3067             0x30670085
#define _FT3068             0x30680085
#define _FT3168             0x31680085
#define _FT3268             0x32680085
#define _FT6146             0x61460085
#define _FT3168G            0x3168A085

#define _FT5726_003         0x5726D486
#define _FT5726_V03         0x5726C486
#define _FT3617_003         0x3617D486

#define _FT3618             0x36180487
#define _FT5646             0x56460487
#define _FT3A58             0x3A580487
#define _FT3B58             0x3B580487
#define _FT3D58             0x3D580487
#define _FT3D59             0x3D590487
#define _FT5936             0x59360487
#define _FT5A36             0x5A360487
#define _FT5B36             0x5B360487
#define _FT5D36             0x5D360487
#define _FT5946             0x59460487
#define _FT5A46             0x5A460487
#define _FT5B46             0x5B460487
#define _FT5D46             0x5D460487

#define _FT3658U            0x3658D488
#define _FT3658G            0x3658A488

#define _FT3519             0x35190489
#define _FT3519T            0x3519E489
#define _FT3419             0x34190489
#define _FT5536U_003        0x5536D489
#define _FT5426G            0x5426A489
#define _FT3437_N03         0x34370489

#define _FT3680             0x3680008A
#define _FT368A             0x368A008A
#define _FT3681             0x3681008A
#define _FT3881             0x3881008A

#define _FT3169             0x3169008B
#define _FT3269             0x3269008B



/*************************************************/

/*
 * choose your ic chip type of focaltech
 */
#define FTS_CHIP_TYPE   _FT3680

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
 * Report Pressure in multitouch
 * 1:enable(default),0:disable
 */
#define FTS_REPORT_PRESSURE_EN                  0

/*
 * Stylus PEN enable
 * 1:enable(default),0:disable
 */
#define FTS_PEN_EN                              0

/*
 * Gesture function enable
 * default: disable
 */
#define FTS_GESTURE_EN                          0

/*
 * ESD check & protection
 * default: disable
 */
#define FTS_ESDCHECK_EN                         0

/*
 * Host test enable
 * 1: enable, 0:disable(default)
 */
#define FTS_TEST_EN                             1

/*
 * Pinctrl enable
 * default: disable
 */
#define FTS_PINCTRL_EN                          1

/*
 * Customer power enable
 * enable it when customer need control TP power
 * default: disable
 */
#define FTS_POWER_SOURCE_CUST_EN                1

/****************************************************/

/********************** Upgrade ****************************/
/*
 * auto upgrade
 */
#define FTS_AUTO_UPGRADE_EN                     1

/*
 * auto upgrade for lcd cfg
 */
#define FTS_AUTO_LIC_UPGRADE_EN                 0

/*
 * Numbers of modules support
 */
#define FTS_GET_MODULE_NUM                      0

/*
 * module_id: mean vendor_id generally, also maybe gpio or lcm_id...
 * If means vendor_id, the FTS_MODULE_ID = PANEL_ID << 8 + VENDOR_ID
 * FTS_GET_MODULE_NUM == 0/1, no check module id, you may ignore them
 * FTS_GET_MODULE_NUM >= 2, compatible with FTS_MODULE2_ID
 * FTS_GET_MODULE_NUM >= 3, compatible with FTS_MODULE3_ID
 */
#define FTS_MODULE_ID                           0x0000
#define FTS_MODULE2_ID                          0x0000
#define FTS_MODULE3_ID                          0x0000

/*
 * Need set the following when get firmware via firmware_request()
 * For example: if module'vendor is tianma,
 * #define FTS_MODULE_NAME                        "tianma"
 * then file_name will be "focaltech_ts_fw_tianma"
 * You should rename fw to "focaltech_ts_fw_tianma", and push it into
 * etc/firmware or by customers
 */
#define FTS_MODULE_NAME                         ""
#define FTS_MODULE2_NAME                        ""
#define FTS_MODULE3_NAME                        ""

/*
 * FW.i file for auto upgrade, you must replace it with your own
 * define your own fw_file, the sample one to be replaced is invalid
 * NOTE: if FTS_GET_MODULE_NUM > 1, it's the fw corresponding with FTS_VENDOR_ID
 */
#define FTS_UPGRADE_FW_FILE                     "include/firmware/fw_sample.i"

/*
 * if FTS_GET_MODULE_NUM >= 2, fw corrsponding with FTS_VENDOR_ID2
 * define your own fw_file, the sample one is invalid
 */
#define FTS_UPGRADE_FW2_FILE                    "include/firmware/fw_sample.i"

/*
 * if FTS_GET_MODULE_NUM >= 3, fw corrsponding with FTS_VENDOR_ID3
 * define your own fw_file, the sample one is invalid
 */
#define FTS_UPGRADE_FW3_FILE                    "include/firmware/fw_sample.i"

/*********************************************************/

#endif /* _LINUX_FOCLATECH_CONFIG_H_ */
