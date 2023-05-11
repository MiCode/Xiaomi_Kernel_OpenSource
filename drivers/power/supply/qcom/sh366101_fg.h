
/*
 * drivers/battery/sh366101_fg.h
 *
 * Copyright (C) 2021 SinoWealth
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef SH366101_FG_H
#define SH366101_FG_H

#undef IS_PACK_ONLY
#define IS_PACK_ONLY                0

#if !(IS_PACK_ONLY)
#define BMS_FG_VERIFY               "BMS_FG_VERIFY"
#define BMS_FC_VOTER                "BMS_FC_VOTER"
#define SM_RAW_SOC_FULL             10000
#define SM_REPORT_FULL_SOC          9800
#define SM_CHARGE_FULL_SOC          9750
#define SM_RECHARGE_SOC             9850
#endif

#define FG_INIT_MARK                0xA000
#define FG_REMOVE_IRQ               1

#define PROPERTY_NAME_SIZE          128
#define WRITE_BUF_MAX_LEN           160

#define MA_TO_UA                    1000

/* 20211029, Ethan */
#define GAUGE_LOG_MIN_TIMESPAN      5
#define HOST_DELAY                  msleep
/* #define HOST_DELAY                  mdelay */

/* CMD Table */

#undef FG_ALTMAC_BLOCK_ENABLE
#define FG_ALTMAC_BLOCK_ENABLE      1

#define INVALID_REG_ADDR            0xFF
#define RET_ERR                     -1

#define MONITOR_WORK_10S            10
#define MONITOR_WORK_5S             5
#define MONITOR_WORK_1S             1

#define MAX_BUF_LEN                 1024
#define CMD_ALTMAC                  0x3E
#define CMD_ALTBLOCK                0x40
#define CMD_ALTCHK                  0x60
#define CMD_ALTNUM                  0x61
#define CMD_SBS_DELAY               3
#define CMDMASK_MASK                0xFF000000
#define CMDMASK_SINGLE              0x01000000
#define CMDMASK_WRITE               0x80000000
#define CMDMASK_ALTMAC_R            0x08000000
#define CMDMASK_ALTMAC_W            (CMDMASK_WRITE | CMDMASK_ALTMAC_R)
#define CMD_CALIINFO                (CMDMASK_ALTMAC_R | 0xE014)
#define CMD_GAUGEINFO               (CMDMASK_ALTMAC_R | 0xE0)
#define CMD_GAUGEBLOCK1             (CMDMASK_ALTMAC_R | 0xE1)
#define CMD_GAUGEBLOCK2             (CMDMASK_ALTMAC_R | 0xE2)
#define CMD_GAUGEBLOCK3             (CMDMASK_ALTMAC_R | 0xE3)
#define CMD_GAUGEBLOCK4             (CMDMASK_ALTMAC_R | 0xE4)
#define CMD_GAUGEBLOCK5             (CMDMASK_ALTMAC_R | 0xE5)
#define CMD_GAUGEBLOCK6             (CMDMASK_ALTMAC_R | 0xE6)
#define CMD_GAUGEBLOCK7             (CMDMASK_ALTMAC_R | 0xE7)
#define CMD_GAUGEBLOCK_FG           (CMDMASK_ALTMAC_R | 0xEF)

/* Read Status Flag */
#define FG_STATUS_HIGH_TEMPERATURE   BIT(15)
#define FG_STATUS_LOW_TEMPERATURE    BIT(14)
#define FG_STATUS_TERM_SOC           BIT(10)
#define FG_STATUS_FULL_SOC           BIT(9)
#define FG_STATUS_BATT_PRESENT       BIT(3)
#define FG_STATUS_LOW_SOC2           BIT(2)
#define FG_STATUS_LOW_SOC1           BIT(1)
#define FG_OP_STATUS_CHG_DISCHG      BIT(0)

/* Buffer Convert */
#define BUF2U16_BG(p)               ((u16)(((u16)(u8)((p)[0]) << 8) | (u8)((p)[1])))
#define BUF2U16_LT(p)               ((u16)(((u16)(u8)((p)[1]) << 8) | (u8)((p)[0])))
#define BUF2U32_BG(p)               ((u32)(((u32)(u8)((p)[0]) << 24) | ((u32)(u8)((p)[1]) << 16) | ((u32)(u8)((p)[2]) << 8) | (u8)((p)[3])))
#define BUF2U32_LT(p)               ((u32)(((u32)(u8)((p)[3]) << 24) | ((u32)(u8)((p)[2]) << 16) | ((u32)(u8)((p)[1]) << 8) | (u8)((p)[0])))
#define GAUGEINFO_LEN               32
#define GAUGESTR_LEN                512
#define U64_MAXVALUE                0xFFFFFFFFFFFFFFFF
#define TEMPER_OFFSET               2731

/* Check Version */
#define CMD_UNSEALKEY               0x80008000
#define CMD_SEAL                    0x20
#define CMD_AFI_STATIC_SUM          (CMDMASK_ALTMAC_R | 0x0004)
#define CMD_AFI_CHEMID              (CMDMASK_ALTMAC_R | 0x0008)
#define CMD_FWVERSION_MAIN          (CMDMASK_ALTMAC_R | 0x00F1)
#define CMD_FWDATE1                 (CMDMASK_ALTMAC_R | 0x00F5)
#define CMD_FWDATE2                 (CMDMASK_ALTMAC_R | 0x00F6)
#define CMD_TS_VER                  (CMDMASK_ALTMAC_R | 0x00CC)
#define FW_DATE_MASK                0x00FF

#define IAP_READ_LEN                2
#define CMD_IAPSTATE_CHECK          0xA0

#define CHECK_VERSION_ERR           -1
#define CHECK_VERSION_OK            0
#define CHECK_VERSION_FW            BIT(0)
#define CHECK_VERSION_AFI           BIT(1)
#define CHECK_VERSION_TS            BIT(2)


#define IIC_ADDR_OF_2_KERNEL(addr)  ((u8)((u8)addr >> 1))

/* file_decode_process */
#define OPERATE_READ                1
#define OPERATE_WRITE               2
#define OPERATE_COMPARE             3
#define OPERATE_WAIT                4

#define ERRORTYPE_NONE              0
#define ERRORTYPE_ALLOC             1
#define ERRORTYPE_LINE              2
#define ERRORTYPE_COMM              3
#define ERRORTYPE_COMPARE           4

/* delay: b0: operate, b1: 2, b2-b3: time, big-endian */
/* other: b0: operate, b1: TWIADR, b2: reg, b3: data_length, b4...end: data */
#define INDEX_TYPE                  0
#define INDEX_ADDR                  1
#define INDEX_REG                   2
#define INDEX_LENGTH                3
#define INDEX_DATA                  4
#define INDEX_WAIT_LENGTH           1
#define INDEX_WAIT_HIGH             2
#define INDEX_WAIT_LOW              3
#define LINELEN_WAIT                4
#define LINELEN_READ                4
#define LINELEN_COMPARE             4
#define LINELEN_WRITE               4
#define FILEDECODE_STRLEN           96
#define COMPARE_RETRY_CNT           2
#define COMPARE_RETRY_WAIT          50
#define BUF_MAX_LENGTH              512

#define FILE_DECODE_RETRY           2
#define FILE_DECODE_DELAY           100

#endif /* SH366101_FG_H */

