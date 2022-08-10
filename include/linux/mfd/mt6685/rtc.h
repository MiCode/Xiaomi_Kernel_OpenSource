/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MT6685_RTC_H__
#define __MT6685_RTC_H__

#include <linux/mfd/mt6685/registers.h>

/*features*/
#define SUPPORT_EOSC_CALI
#define SUPPORT_PWR_OFF_ALARM

#define HWID                    MT6685_HWCID_L

/* we map HW YEA 0 (2000) to 1968 not 1970 because 2000 is the leap year */
#define RTC_MIN_YEAR            1968
#define RTC_BASE_YEAR           1900
#define RTC_NUM_YEARS           128
#define RTC_MIN_YEAR_OFFSET     (RTC_MIN_YEAR - RTC_BASE_YEAR)

#define RTC_TC_SEC                      0x12
#define RTC_TC_MIN                      0x14
#define RTC_TC_HOU                      0x16
#define RTC_TC_DOM                      0x18
#define RTC_TC_DOW                      0x1A
#define RTC_TC_MTH                      0x1C
#define RTC_TC_YEA                      0x1E
/* Min, Hour, Dom... register offset to RTC_TC_SEC */
#define RTC_OFFSET_SEC                  0
#define RTC_OFFSET_MIN                  1
#define RTC_OFFSET_HOUR                 2
#define RTC_OFFSET_DOM                  3
#define RTC_OFFSET_DOW                  4
#define RTC_OFFSET_MTH                  5
#define RTC_OFFSET_YEAR                 6
#define RTC_OFFSET_COUNT                7

#define RTC_DSN_ID                      0x580

#define DCXO_ANA_ID                     0x200

#define RTC_BBPU                        0x8
#define RTC_WRTGR_MT6685                0x42
#define RTC_IRQ_STA                     0xa
#define RTC_IRQ_EN                      0xc
#define RTC_AL_MASK                     0x10

#define RTC_AL_SEC                      0x20
#define RTC_AL_MIN                      0x22
#define RTC_AL_HOU                      0x24
#define RTC_AL_HOU_H                    0x25
#define RTC_AL_DOM                      0x26
#define RTC_AL_DOW                      0x28
#define RTC_AL_DOW_H                    0x29
#define RTC_AL_MTH                      0x2a
#define RTC_AL_MTH_H                    0x2b
#define RTC_AL_YEA                      0x2c
#define RTC_AL_YEA_H                    0x2d
#define RTC_OSC32CON                    0x2e
#define RTC_POWERKEY1                   0x30
#define RTC_POWERKEY2                   0x32
#define RTC_PDN1                        0x34
#define RTC_PDN1_H                      0x35
#define RTC_PDN2                        0x36
#define RTC_SPAR0                       0x38
#define RTC_SPAR1                       0x3a
#define RTC_PROT                        0x3c
#define RTC_WRTGR                       0x42
#define RTC_CON                         0x44
#define RTC_INT_CNT_L                   0x48
#define RTC_SPAR_MACRO                  0x58

#define RTC_TC_SEC_MASK                 0x3f
#define RTC_TC_MIN_MASK                 0x3f
#define RTC_TC_HOU_MASK                 0x1f
#define RTC_TC_DOM_MASK                 0x1f
#define RTC_TC_DOW_MASK                 0x7
#define RTC_TC_MTH_MASK                 0xf
#define RTC_TC_YEA_MASK                 0x7f

#define RTC_AL_SEC_MASK                 0x3f
#define RTC_AL_MIN_MASK                 0x3f
#define RTC_AL_HOU_MASK                 0x1f
#define RTC_AL_DOM_MASK                 0x1f
#define RTC_AL_DOW_MASK                 0x7
#define RTC_AL_MTH_MASK                 0xf
#define RTC_AL_YEA_MASK                 0x7f

#define RTC_PWRON_YEA                   RTC_PDN2
#define RTC_PWRON_MTH                   RTC_PDN2
#define RTC_PWRON_SEC                   RTC_SPAR0
#define RTC_PWRON_MIN                   RTC_SPAR1
#define RTC_PWRON_HOU                   RTC_SPAR1
#define RTC_PWRON_DOM                   RTC_SPAR1

#define RTC_PWRON_SEC_SHIFT             0x0
#define RTC_PWRON_MIN_SHIFT             0x0
#define RTC_PWRON_HOU_SHIFT             0x6
#define RTC_PWRON_DOM_SHIFT             0xb
#define RTC_PWRON_MTH_SHIFT             0x0
#define RTC_PWRON_YEA_SHIFT             0x8

#define RTC_PWRON_SEC_MASK              (RTC_AL_SEC_MASK << RTC_PWRON_SEC_SHIFT)
#define RTC_PWRON_MIN_MASK              (RTC_AL_MIN_MASK << RTC_PWRON_MIN_SHIFT)
#define RTC_PWRON_HOU_MASK              (RTC_AL_HOU_MASK << RTC_PWRON_HOU_SHIFT)
#define RTC_PWRON_DOM_MASK              (RTC_AL_DOM_MASK << RTC_PWRON_DOM_SHIFT)
#define RTC_PWRON_MTH_MASK              (RTC_AL_MTH_MASK << RTC_PWRON_MTH_SHIFT)
#define RTC_PWRON_YEA_MASK              (RTC_AL_YEA_MASK << RTC_PWRON_YEA_SHIFT)

#define RTC_BBPU_KEY                    0x4300
#define RTC_BBPU_CBUSY                  BIT(6)
#define RTC_BBPU_RELOAD                 BIT(5)
#define RTC_BBPU_AUTO                   BIT(3)
#define RTC_BBPU_CLR                    BIT(1)
#define RTC_BBPU_PWREN                  BIT(0)
#define RTC_BBPU_AL_STA                 BIT(7)
#define RTC_BBPU_RESET_AL               BIT(3)
#define RTC_BBPU_RESET_SPAR             BIT(2)

#define RTC_AL_MASK_DOW                 BIT(4)

#define RTC_IRQ_EN_LP                   BIT(3)
#define RTC_IRQ_EN_ONESHOT              BIT(2)
#define RTC_IRQ_EN_AL                   BIT(0)
#define RTC_IRQ_EN_ONESHOT_AL           (RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_IRQ_STA_LP                  BIT(3)
#define RTC_IRQ_STA_AL                  BIT(0)

#define RTC_PDN1_PWRON_TIME             BIT(7)
#define RTC_PDN2_PWRON_LOGO             BIT(15)
#define RTC_PDN2_PWRON_ALARM            BIT(4)

#define RTC_RG_FG2                      0x54
#define RTC_RG_FG3                      0x56

#define TOP_RTC_EOSC32_CK_PDN           MT6685_SCK_TOP_CKPDN_CON0_L
#define TOP_RTC_EOSC32_CK_PDN_MASK      (MT6685_RG_RTC_EOSC32_CK_PDN_MASK \
						<< MT6685_RG_RTC_EOSC32_CK_PDN_SHIFT)

#define EOSC_CALI_TD					RTC_AL_DOW_H
#define EOSC_CALI_TD_MASK               MT6685_RG_EOSC_CALI_TD_MASK

#define TOP_DIG_WPK                     MT6685_TOP_DIG_WPK
#define DIG_WPK_KEY_MASK        (MT6685_DIG_WPK_KEY_MASK << MT6685_DIG_WPK_KEY_SHIFT)

#define TOP_DIG_WPK_H                   MT6685_TOP_DIG_WPK_H
#define DIG_WPK_KEY_H_MASK      (MT6685_DIG_WPK_KEY_H_MASK << MT6685_DIG_WPK_KEY_H_SHIFT)

#define RG_RTC_MCLK_PDN                 MT6685_SCK_TOP_CKPDN_CON0_L
#define RG_RTC_MCLK_PDN_STA_MASK        MT6685_RG_RTC_MCLK_PDN_MASK
#define RG_RTC_MCLK_PDN_STA_SHIFT       MT6685_RG_RTC_MCLK_PDN_SHIFT

#define RG_RTC_MCLK_PDN_SET             MT6685_SCK_TOP_CKPDN_CON0_L_SET
#define RG_RTC_MCLK_PDN_CLR             MT6685_SCK_TOP_CKPDN_CON0_L_CLR
#define RG_RTC_MCLK_PDN_MASK    (MT6685_RG_RTC_MCLK_PDN_MASK << MT6685_RG_RTC_MCLK_PDN_SHIFT)

#define SPAR_PROT_STAT_MASK             MT6685_RTC_SPAR_PROT_STAT_MASK
#define SPAR_PROT_STAT_SHIFT            MT6685_RTC_SPAR_PROT_STAT_SHIFT

/*SCK_TOP rtc interrupt*/
#define SCK_TOP_INT_CON0                MT6685_SCK_TOP_INT_CON0
#define EN_RTC_INTERRUPT                MT6685_RG_INT_EN_RTC_MASK
#define SCK_TOP_INT_STATUS0             MT6685_SCK_TOP_INT_STATUS0

#define TOP2_ELR1                       MT6685_TOP2_ELR1

#define MTK_RTC_POLL_DELAY_US            10
#define MTK_RTC_POLL_TIMEOUT             (jiffies_to_usecs(HZ))

#define RTC_POFF_ALM_SET                 _IOW('p', 0x15, struct rtc_time) /* Set alarm time  */

#define SPARE_REG_WIDTH                  1

enum mtk_rtc_spare_enum {
	SPARE_FG2,
	SPARE_FG3,
	SPARE_SPAR0,
#ifdef SUPPORT_PWR_OFF_ALARM
	SPARE_KPOC,
#endif
	SPARE_RG_MAX,
};

enum rtc_irq_sta {
	RTC_NONE,
	RTC_ALSTA,
	RTC_TCSTA,
	RTC_LPSTA,
};

enum rtc_reg_set {
	RTC_REG,
	RTC_MASK,
	RTC_SHIFT
};

enum unlock_version {
	UNLOCK_MT6685_SERIES,
};

#ifdef SUPPORT_EOSC_CALI

#define EOSC_SOL_1      0x5
#define EOSC_SOL_2      0x7

enum rtc_eosc_cali_td {
	EOSC_CALI_TD_01_SEC = 0x3,
	EOSC_CALI_TD_02_SEC,
	EOSC_CALI_TD_04_SEC,
	EOSC_CALI_TD_08_SEC,
	EOSC_CALI_TD_16_SEC,
};

#endif

#ifdef SUPPORT_PWR_OFF_ALARM
struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

enum boot_mode_t {

	NORMAL_BOOT = 0,
	META_BOOT = 1,
	RECOVERY_BOOT = 2,
	SW_REBOOT = 3,
	FACTORY_BOOT = 4,
	ADVMETA_BOOT = 5,
	ATE_FACTORY_BOOT = 6,
	ALARM_BOOT = 7,
	KERNEL_POWER_OFF_CHARGING_BOOT = 8,
	LOW_POWER_OFF_CHARGING_BOOT = 9,
	DONGLE_BOOT = 10,
	UNKNOWN_BOOT
};
#endif

struct mtk_rtc_data {
	u32         wrtgr;
	u32			unlock_version;
};

static const char *rtc_time_reg_name[RTC_OFFSET_COUNT] = {
	[0] = "SEC",
	[1] = "MIN",
	[2] = "HOUR",
	[3] = "DOM",
	[4] = "DOW",
	[5] = "MTH",
	[6] = "YEAR",
};

enum rtc_time_mask {
	SEC_MASK,
	MIN_MASK,
	HOU_MASK,
	DOM_MASK,
	DOW_MASK,
	MTH_MASK,
	YEA_MASK,
	MASK_COUNT

};

static char rtc_time_mask[MASK_COUNT] = {
	[SEC_MASK] = 0x3f,
	[MIN_MASK] = 0x3f,
	[HOU_MASK] = 0x1f,
	[DOM_MASK] = 0x1f,
	[DOW_MASK] = 0x7,
	[MTH_MASK] = 0xf,
	[YEA_MASK] = 0x7f
};

struct mt6685_rtc {
	struct rtc_device       *rtc_dev;

	/* Protect register access from multiple tasks */
	struct mutex            lock;
	struct mutex            clk_lock;
	struct regmap           *regmap;
	struct regmap           *regmap_spar;
	int                     irq;
	u32                     addr_base;
	const struct mtk_rtc_data *data;
#ifdef SUPPORT_EOSC_CALI
	bool                    cali_is_supported;
#endif
#ifdef SUPPORT_PWR_OFF_ALARM
	struct work_struct work;
	struct completion comp;
#if IS_ENABLED(CONFIG_PM)
struct notifier_block pm_nb;
#endif
#endif
};

#endif /* __MT6685_RTC_H__ */
