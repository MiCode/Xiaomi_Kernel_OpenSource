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
#define IS_ADC_HIGHFREQ             0

#undef ENABLE_CHGBLOCK
#define ENABLE_CHGBLOCK             0
#undef ENABLE_TERMBLOCK
#define ENABLE_TERMBLOCK            0

#if !(IS_PACK_ONLY)
#define BMS_FG_VERIFY               "BMS_FG_VERIFY"
#define BMS_FC_VOTER                "BMS_FC_VOTER"
#define SM_RAW_SOC_FULL             10000
#define SM_REPORT_FULL_SOC          9800
#define SM_CHARGE_FULL_SOC          9750
#define SM_RECHARGE_SOC             9850
#endif

#define FG_INIT_MARK                0xA000
#define FG_REMOVE_IRQ               0

#define PROPERTY_NAME_SIZE          128
#define WRITE_BUF_MAX_LEN           160

#define MA_TO_UA                    1000

#if IS_ADC_HIGHFREQ
#define GAUGE_REFRESH_SPAN          5
#else
#define GAUGE_REFRESH_SPAN          1
#endif
#define GAUGE_LOG_MIN_TIMESPAN      10
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
#define CMD_CADCINFO                (CMDMASK_ALTMAC_R | 0xCF) /* 20211111, Ethan */
#define CMD_GAUGEINFO               (CMDMASK_ALTMAC_R | 0xE0)
#define CMD_GAUGEBLOCK1             (CMDMASK_ALTMAC_R | 0xE1)
#define CMD_GAUGEBLOCK2             (CMDMASK_ALTMAC_R | 0xE2)
#define CMD_GAUGEBLOCK3             (CMDMASK_ALTMAC_R | 0xE3)
#define CMD_GAUGEBLOCK4             (CMDMASK_ALTMAC_R | 0xE4)
#define CMD_GAUGEBLOCK5             (CMDMASK_ALTMAC_R | 0xE5)
#define CMD_GAUGEBLOCK6             (CMDMASK_ALTMAC_R | 0xE6)
#define CMD_GAUGEBLOCK7             (CMDMASK_ALTMAC_R | 0xE7)
#define CMD_GAUGEBLOCK_FG           (CMDMASK_ALTMAC_R | 0xEF)
#define CMD_CONTROLSTATUS           (CMDMASK_ALTMAC_R | 0x0000) /* 20211123, Ethan */
#define CMD_RUNFLAG                 0x06 /* 20211123, Ethan */
#define DELTA_VOLT                  (200 * MA_TO_UA) /* 20211208, Ethan */

/* 20211126, Ethan */
#define CMD_OEMFLAG                 (CMDMASK_ALTMAC_R | 0xC1)
#define CMD_MASK_OEM_LIFETIMEEN     0x8000
#define CMD_MASK_OEM_GAUGEEN        0x4000
#define CMD_MASK_OEM_CALI           0x0008 /* 20211228, Ethan */
#define CMD_LIFETIMEADC             (CMDMASK_ALTMAC_R | 0x6B)
#define LEN_LIFETIMEADC             12

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

/* 20211122, Ethan. Gauge Enable */
#define CMD_ENABLE_GAUGE            0x0021
#define CMD_ENABLE_LIFETIME         0x0067 /* 20211126, Ethan */
#define DELAY_ENABLE_GAUGE          1000 /* Write E2rom & Gauge Init */
/* 20220106, Ethan */
#define CMD_RESET                   0x0041
#define DELAY_RESET                 1000
#define TEMPER_MIN_RESET            -450
#define VOLT_MIN_RESET 				(4000 * MA_TO_UA)  /* 20220110, Ethan */
#define SOC_MIN_RESET 				10
#define DELAY_WRITE_E2ROM           750

#define IAP_READ_LEN                2
#define CMD_IAPSTATE_CHECK          0xA0

#define CHECK_VERSION_ERR           -1
#define CHECK_VERSION_OK            0
#define CHECK_VERSION_FW            BIT(0)
#define CHECK_VERSION_AFI           BIT(1)
#define CHECK_VERSION_TS            BIT(2)
#define CHECK_VERSION_WHOLE_CHIP    (CHECK_VERSION_FW | CHECK_VERSION_AFI | CHECK_VERSION_TS)

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

/* 20211112, Ethan. Charge Block */
/* #if ENABLE_CHGBLOCK */
#define MASK_CHARGESTATUS_DEGRADE   0x07
#define CMD_CHARGESTATUS            (CMDMASK_ALTMAC_R | 0x55)
#define CMD_CHARGEVOLTAGES          (CMDMASK_ALTMAC_W | 0xB0)
#define LEN_CHARGESTATUS            7
#define LEN_CHARGEVOLTAGES          16
/* #endif            */

/* 20211112, Ethan. Terminate Voltage */
/* #if ENABLE_TERMBLOCK */
#define CMD_TERMINATEVOLT           (CMDMASK_ALTMAC_W | 0xB1)
#define LEN_TERMINATEVOLT           3
/* #endif */

/* 20211113, Ethan. User Buffer */
#define CMD_USERBUFFER              (CMDMASK_ALTMAC_W | 0x70)
#define LEN_USERBUFFER              32
#define CMDMASK_LOCK1_TRIGGERED		0x0004
static inline struct power_supply *get_power_supply_by_name(char *name)
{
	if (!name)
		return (struct power_supply *)NULL;
	else
		return power_supply_get_by_name(name);
}

#define psy_do_property(name, function, property, value) \
({	\
	struct power_supply *psy;	\
	int ret = 0;	\
	psy = get_power_supply_by_name((name));	\
	if (!psy) {	\
		pr_err("%s: Fail to "#function" psy (%s)\n",	\
			__func__, (name));	\
		value.intval = 0;	\
		ret = -ENOENT;	\
	} else {	\
		if (psy->desc->function##_property != NULL) { \
			ret = psy->desc->function##_property(psy, \
				(enum power_supply_property) (property), &(value)); \
			if (ret < 0) {	\
				pr_err("%s: Fail to %s "#function" (%d=>%d)\n", \
						__func__, name, (property), ret);	\
				value.intval = 0;	\
			}	\
		} else {	\
			ret = -ENOSYS;	\
		}	\
		power_supply_put(psy);		\
	}					\
	ret;	\
})

#endif /* SH366101_FG_H */
