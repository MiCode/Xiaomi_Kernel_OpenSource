/* drivers/input/touchscreen/gt9xx_shorttp.h
 *
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version:1.0
 *      V1.0:2012/10/20
 */


#ifndef _GT9XX_OPEN_SHORT_H_
#define _GT9XX_OPEN_SHORT_H_

#include "gt9xx.h"












































































































































































































































































































































#define MIN_DRIVER_NUM            10
#define MAX_DRIVER_NUM            42
#define MIN_SENSOR_NUM            5
#define MAX_SENSOR_NUM            30













#define GT9_DRV_HEAD    0x80
#define GT9_SEN_HEAD    0x00



#define _bRW_MISCTL__SRAM_BANK          0x4048
#define _bRW_MISCTL__MEM_CD_EN          0x4049
#define _bRW_MISCTL__CACHE_EN           0x404b
#define _bRW_MISCTL__TMR0_EN            0x40b0
#define _rRW_MISCTL__SWRST_B0_          0x4180
#define _bWO_MISCTL__CPU_SWRST_PULSE    0x4184
#define _rRW_MISCTL__BOOTCTL_B0_        0x4190
#define _rRW_MISCTL__BOOT_OPT_B0_       0x4218
#define _bRW_MISCTL__RG_OSC_CALIB       0x4268
#define _rRW_MISCTL__BOOT_CTL_          0x5094
#define _rRW_MISCTL__SHORT_BOOT_FLAG    0x5095


#define GTP_REG_DSP_SHORT               0xc000

#define GT9_REG_SEN_DRV_CNT             0x8062
#define GT9_REG_CFG_BEG                 0x8047
#define GT9_REG_KEY_VAL                 0x8093

#define GT9_REG_SEN_ORD     0x80B7
#define GT9_REG_DRV_ORD     0x80D5


struct gt9xx_short_info
{
    u8 master;
    u8 master_is_driver;
    u8 slave;
    u8 slave_is_driver;
    u16 short_code;
    u16 self_data;
    u16 impedance;
};

#define MYBIG_ENDIAN                1
#define MYLITLE_ENDIAN              0

#define	_BEYOND_MAX_LIMIT           0x0001
#define _BEYOND_MIN_LIMIT           0x0002
#define _BEYOND_KEY_MAX_LMT         0x0004
#define _BEYOND_KEY_MIN_LMT         0x0008
#define _BEYOND_UNIFORMITY_LMT      0x0010

#define	_MAX_TEST                   0x0001
#define _MIN_TEST                   0x0002
#define _KEY_MAX_TEST               0x0004
#define _KEY_MIN_TEST               0x0008
#define _UNIFORMITY_TEST            0x0010

#define _CHANNEL_PASS               0x0000

#define GTP_WAIT_RAW_MAX_TIMES      200
#define GTP_REG_RAW_DATA            0x9b60
#define GTP_REG_READ_RAW            0x8040
#define GTP_REG_RAW_READY           0x814E
#define GTP_REG_RAW_DATA_GT9F       0x87C0
#define GTP_REG_DIFF_DATA           0xA160
#define GTP_REG_REFRAW_DATA         0x9560

struct gt9xx_open_info
{
    u8 driver;
    u8 sensor;
    u16 raw_val;
    s32 beyond_type;
    u8 times;
    u8 key;
};


#endif

