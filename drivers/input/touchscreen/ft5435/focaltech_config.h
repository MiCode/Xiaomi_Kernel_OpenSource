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
#ifndef _LINUX_FOCLATECH_CONFIG_H_
#define _LINUX_FOCLATECH_CONFIG_H_
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
#define FTS_CHIP_TYPE   _FT5435
#define FTS_DEBUG_EN                            1
#define FTS_MT_PROTOCOL_B_EN                    1
#define FTS_REPORT_PRESSURE_EN                  0
#define FTS_FORCE_TOUCH_EN                      0
#define FTS_GESTURE_EN                          1
#define FTS_ESDCHECK_EN                         1
#define FTS_TEST_EN                             0
#define FTS_GLOVE_EN                            0
#define FTS_COVER_EN                            0
#define FTS_CHARGER_EN                          0
#define FTS_PSENSOR_EN                          0
#define FTS_SYSFS_NODE_EN                       1
#define FTS_APK_NODE_EN                         1
#define FTS_POWER_SOURCE_CUST_EN                1
#define FTS_AUTO_UPGRADE_EN                     1
#define FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN         0
#define FTS_AUTO_CLB_EN                         0
#define FTS_GET_VENDOR_ID_NUM                   0
#define FTS_VENDOR_1_ID                         0x00
#define FTS_VENDOR_2_ID                         0x00
#define FTS_VENDOR_3_ID                         0x00
#define FTS_UPGRADE_FW_APP                      "include/firmware/LQ_D6_FT5435_Boen_Debug_V0f_D01_20170831_app.i"
#define FTS_UPGRADE_FW2_APP                     "include/firmware/LQ_D6_FT5435_Boen_Debug_V0f_D01_20170831_app.i"
#define FTS_UPGRADE_FW3_APP                     "include/firmware/LQ_D6_FT5435_Boen_Debug_V0f_D01_20170831_app.i"
#define FTS_UPGRADE_STRESS_TEST                 0
#define FTS_UPGRADE_TEST_NUMBER                 1000
#endif /* _LINUX_FOCLATECH_CONFIG_H_ */
