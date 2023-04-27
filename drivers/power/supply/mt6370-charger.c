// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Mediatek Inc.
 *
 * Author: ChiaEn Wu <chiaen_wu@richtek.com>
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/linear_range.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/workqueue.h>

#include "charger_class.h"
#include "mtk_charger.h"

static bool dbg_log_en = true;
module_param(dbg_log_en, bool, 0644);
#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_info(dev, "%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define LINEAR_RANGE_IDX(_min, _min_sel, _max_sel, _step)	\
	{							\
		.min = _min,					\
		.min_sel = _min_sel,				\
		.max_sel = _max_sel,				\
		.step = _step,					\
	}

/* #define MT6370_APPLE_SAMSUNG_TA_SUPPORT */

/* Define this macro if DCD timeout is supported */
#define CONFIG_MT6370_DCDTOUT_SUPPORT

#define MT6370_REG_DEV_INFO		0x100
#define MT6370_REG_CORECTRL2		0x102
#define MT6370_REG_RSTPASCODE1		0x103
#define MT6370_REG_RSTPASCODE2		0x104
#define MT6370_REG_HIDDENPASCODE1	0x107
#define MT6370_REG_HIDDENPASCODE2	0x108
#define MT6370_REG_HIDDENPASCODE3	0x109
#define MT6370_REG_HIDDENPASCODE4	0x10A
#define MT6370_REG_OSCCTRL		0x110
#define MT6370_REG_CHG_CTRL1		0x111
#define MT6370_REG_CHG_CTRL2		0x112
#define MT6370_REG_CHG_CTRL3		0x113
#define MT6370_REG_CHG_CTRL4		0x114
#define MT6370_REG_CHG_CTRL5		0x115
#define MT6370_REG_CHG_CTRL6		0x116
#define MT6370_REG_CHG_CTRL7		0x117
#define MT6370_REG_CHG_CTRL8		0x118
#define MT6370_REG_CHG_CTRL9		0x119
#define MT6370_REG_CHG_CTRL10		0x11A
#define MT6370_REG_CHG_CTRL11		0x11B
#define MT6370_REG_CHG_CTRL12		0x11C
#define MT6370_REG_CHG_CTRL13		0x11D
#define MT6370_REG_CHG_CTRL14		0x11E
#define MT6370_REG_CHG_CTRL15		0x11F
#define MT6370_REG_CHG_CTRL16		0x120
#define MT6370_REG_CHGADC		0x121
#define MT6370_REG_DEVICE_TYPE		0x122
#define MT6370_REG_QCCTRL1		0x123
#define MT6370_REG_QCCTRL2		0x124
#define MT6370_REG_QC3P0CTRL1		0x125
#define MT6370_REG_QC3P0CTRL2		0x126
#define MT6370_REG_USB_STATUS1		0x127
#define MT6370_REG_QCSTATUS1		0x128
#define MT6370_REG_QCSTATUS2		0x129
#define MT6370_REG_CHGPUMP		0x12A
#define MT6370_REG_CHG_CTRL17		0x12B
#define MT6370_REG_CHG_CTRL18		0x12C
#define MT6370_REG_CHGDIRCHG1		0x12D
#define MT6370_REG_CHGDIRCHG2		0x12E
#define MT6370_REG_CHGDIRCHG3		0x12F
#define MT6370_REG_CHGHIDDENCTRL0	0x130
#define MT6370_REG_CHGHIDDENCTRL1	0x131
#define MT6370_REG_LG_CONTROL		0x133
#define MT6370_REG_CHGHIDDENCTRL6	0x135
#define MT6370_REG_CHGHIDDENCTRL7	0x136
#define MT6370_REG_CHGHIDDENCTRL8	0x137
#define MT6370_REG_CHGHIDDENCTRL9	0x138
#define MT6370_REG_CHGHIDDENCTRL15	0x13E
#define MT6370_REG_CHGHIDDENCTRL22	0x145
#define MT6370_REG_CHG_STAT		0x14A
#define MT6370_REG_CHGNTC		0x14B
#define MT6370_REG_ADCDATAH		0x14C
#define MT6370_REG_ADCDATAL		0x14D
#define MT6370_REG_ADCBATDATAH		0x152
#define MT6370_REG_CHG_CTRL19		0x160
#define MT6370_REG_VDDASUPPLY		0x162
#define MT6370_REG_FLED_EN		0x17E
#define MT6370_REG_CHG_STAT1		0X1D0
#define MT6370_REG_CHG_STAT2		0x1D1
#define MT6370_REG_CHG_STAT3		0x1D2
#define MT6370_REG_CHG_STAT4		0x1D3
#define MT6370_REG_CHG_STAT5		0x1D4
#define MT6370_REG_CHG_STAT6		0x1D5
#define MT6370_REG_QCSTAT		0x1D6
#define MT6370_REG_DICHGSTAT		0x1D7
#define MT6370_REG_OVPCTRL_STAT		0x1D8

/* ========== CHG_CTRL1 0x11 ============ */
#define MT6370_SHIFT_OPA_MODE		0
#define MT6370_SHIFT_HZ_EN		2
#define MT6370_SHIFT_FORCE_SLEEP	3

#define MT6370_MASK_OPA_MODE		(1 << MT6370_SHIFT_OPA_MODE)
#define MT6370_MASK_HZ_EN		(1 << MT6370_SHIFT_HZ_EN)
#define MT6370_MASK_FORCE_SLEEP		(1 << MT6370_SHIFT_FORCE_SLEEP)
/* ========== CHG_CTRL2 0x12 ============ */
#define MT6370_SHIFT_CHG_EN		0
#define MT6370_SHIFT_CFO_EN		1
#define MT6370_SHIFT_IINLMTSEL		2
#define MT6370_SHIFT_TE_EN		4
#define MT6370_SHIFT_BYPASS_MODE	5

#define MT6370_MASK_CHG_EN		(1 << MT6370_SHIFT_CHG_EN)
#define MT6370_MASK_CFO_EN		(1 << MT6370_SHIFT_CFO_EN)
#define MT6370_MASK_IINLMTSEL		0x0C
#define MT6370_MASK_TE_EN		(1 << MT6370_SHIFT_TE_EN)
#define MT6370_MASK_BYPASS_MODE		(1 << MT6370_SHIFT_BYPASS_MODE)
/* ========== CHG_CTRL3 0x13 ============ */
#define MT6370_SHIFT_AICR		2
#define MT6370_SHIFT_AICR_EN		1
#define MT6370_SHIFT_ILIM_EN		0

#define MT6370_MASK_AICR		0xFC
#define MT6370_MASK_AICR_EN		(1 << MT6370_SHIFT_AICR_EN)
#define MT6370_MASK_ILIM_EN		(1 << MT6370_SHIFT_ILIM_EN)

/* ========== CHG_CTRL6 0x16 ============ */
#define MT6370_SHIFT_MIVR		1
#define MT6370_SHIFT_MIVR_EN		0

#define MT6370_MASK_MIVR		0xFE
#define MT6370_MASK_MIVR_EN		(1 << MT6370_SHIFT_MIVR_EN)

/* ========== CHG_CTRL7 0x17 ============ */
#define MT6370_SHIFT_ICHG		2
#define MT6370_MASK_ICHG		0xFC
/* ========== CHG_CTRL9 0x19 ============ */
#define MT6370_SHIFT_IEOC		4
#define MT6370_MASK_IEOC		0xF0
/* ========== CHG_CTRL10 0x1A ============ */
#define MT6370_SHIFT_BOOST_OC		0

#define MT6370_MASK_BOOST_OC		0x07
/* ========== CHG_CTRL12 0x1C ============ */
#define MT6370_SHIFT_TMR_EN		1
#define MT6370_SHIFT_WT_FC		5

#define MT6370_MASK_TMR_EN		(1 << MT6370_SHIFT_TMR_EN)
#define MT6370_MASK_WT_FC		0xE0
/* ========== CHG_CTRL13 0x1D ============ */
#define MT6370_SHIFT_WDT_EN		7

#define MT6370_MASK_WDT_EN		(1 << MT6370_SHIFT_WDT_EN)
/* ========== CHG_CTRL14 0x1E ============ */
#define MT6370_SHIFT_AICL_MEAS		7
#define MT6370_SHIFT_AICL_VTH		0

#define MT6370_MASK_AICL_MEAS		(1 << MT6370_SHIFT_AICL_MEAS)
#define MT6370_MASK_AICL_VTH		0x07
/* ========== CHG_CTRL16 0x20 ============ */
#define MT6370_SHIFT_JEITA_EN		4

#define MT6370_MASK_JEITA_EN		(1 << MT6370_SHIFT_JEITA_EN)
/* ========== CHG_DEVICETYPE 0x22 ============ */
#define MT6370_SHIFT_USBCHGEN		7
#define MT6370_SHFT_DCDTOUTEN		6
#define MT6370_SHIFT_DCPSTD		2
#define MT6370_SHIFT_CDP		1
#define MT6370_SHIFT_SDP		0

#define MT6370_MASK_USBCHGEN		(1 << MT6370_SHIFT_USBCHGEN)
#define MT6370_MASK_DCDTOUTEN		(1 << MT6370_SHFT_DCDTOUTEN)
#define MT6370_MASK_DCPSTD		(1 << MT6370_SHIFT_DCPSTD)
#define MT6370_MASK_CDP		(1 << MT6370_SHIFT_CDP)
#define MT6370_MASK_SDP		(1 << MT6370_SHIFT_SDP)

/* ========== QCCTRL2 0x24 ============ */
#define MT6370_SHIFT_EN_DCP		1

#define MT6370_MASK_EN_DCP		(1 << MT6370_SHIFT_EN_DCP)

/* ========== USBSTATUS1 0x27 ============ */
#define MT6370_SHIFT_USB_STATUS		4
#define MT6370_SHIFT_DCDT		2

#define MT6370_MASK_FAST_UNKNOWN_TA_DECT	(0x80)
#define MT6370_MASK_USB_STATUS		0x70
/* ========== QCSTATUS1 0x28 ============= */
#define MT6370_SHIFT_VLGC_DISABLE	(7)
#define MT6370_MASK_VLGC_DISABLE	(1 << MT6370_SHIFT_VLGC_DISABLE)
/* ========== CHG_CTRL17 0x2B ============ */
#define MT6370_SHIFT_PUMPX_EN		7
#define MT6370_SHIFT_PUMPX_20_10	6
#define MT6370_SHIFT_PUMPX_UP_DN	5
#define MT6370_SHIFT_PUMPX_DEC		0

#define MT6370_MASK_PUMPX_EN		(1 << MT6370_SHIFT_PUMPX_EN)
#define MT6370_MASK_PUMPX_20_10		(1 << MT6370_SHIFT_PUMPX_20_10)
#define MT6370_MASK_PUMPX_UP_DN		(1 << MT6370_SHIFT_PUMPX_UP_DN)
#define MT6370_MASK_PUMPX_DEC		0x1F
/* ========== CHG_CTRL18 0x2C ============ */
#define MT6370_SHIFT_IRCMP_RES		3
#define MT6370_SHIFT_IRCMP_VCLAMP	0

#define MT6370_MASK_IRCMP_RES		0x38
#define MT6370_MASK_IRCMP_VCLAMP	0x07
/* ========== CHG_DIRCHG1 0x2E ============ */
#define MT6370_SHIFT_DC_WDT		4

#define MT6370_MASK_DC_WDT		0x70

/* ========== CHG_STAT 0x4A ============ */
#define MT6370_SHIFT_ADC_STAT		0
#define MT6370_SHIFT_CHG_STAT		6

#define MT6370_MASK_ADC_STAT		(1 << MT6370_SHIFT_ADC_STAT)
#define MT6370_MASK_CHG_STAT		0xC0
/* ============ VDDA SUPPLY 0x62 ============ */
#define MT6370_SHIFT_LBPHYS_SEL		(7)
#define MT6370_SHIFT_LBP_DT		(5)
#define MT6370_MASK_LBP			(0xE0)
/* ========== CHG_IRQ5 0xC4 ============ */
#define MT6370_SHIFT_CHG_IEOCI		7
#define MT6370_SHIFT_CHG_TERMI		6
#define MT6370_SHIFT_CHG_RECHGI		5
#define MT6370_SHIFT_CHG_SSFINISHI	4
#define MT6370_SHIFT_CHG_WDTMRI		3
#define MT6370_SHIFT_CHGDET_DONEI	2
#define MT6370_SHIFT_CHG_ICHGMEASI	1
#define MT6370_SHIFT_CHG_AICLMEASI	0

#define MT6370_MASK_CHG_IEOCI		(1 << MT6370_SHIFT_CHG_IEOCI)
#define MT6370_MASK_CHG_TERMI		(1 << MT6370_SHIFT_CHG_TERMI)
#define MT6370_MASK_CHG_RECHGI		(1 << MT6370_SHIFT_CHG_RECHGI)
#define MT6370_MASK_CHG_SSFINISHI	(1 << MT6370_SHIFT_CHG_SSFINISHI)
#define MT6370_MASK_CHG_WDTMRI		(1 << MT6370_SHIFT_CHG_WDTMRI)
#define MT6370_MASK_CHGDET_DONEI	(1 << MT6370_SHIFT_CHGDET_DONEI)
#define MT6370_MASK_CHG_ICHGMEASI	(1 << MT6370_SHIFT_CHG_ICHGMEASI)
#define MT6370_MASK_CHG_AICLMEASI	(1 << MT6370_SHIFT_CHG_AICLMEASI)
/* ========== CHG_STAT1 0xD0 ============ */
#define MT6370_SHIFT_PWR_RDY		7
#define MT6370_SHIFT_CHG_MIVR		6
#define MT6370_SHIFT_CHG_AICR		5
#define MT6370_SHIFT_CHG_TREG		4
#define MT6370_SHIFT_DIRCHG_ON		0

#define MT6370_MASK_PWR_RDY		(1 << MT6370_SHIFT_PWR_RDY)
#define MT6370_MASK_CHG_MIVR		(1 << MT6370_SHIFT_CHG_MIVR)
#define MT6370_MASK_CHG_AICR		(1 << MT6370_SHIFT_CHG_AICR)
#define MT6370_MASK_CHG_TREG		(1 << MT6370_SHIFT_CHG_TREG)
#define MT6370_MASK_DIRCHG_ON		(1 << MT6370_SHIFT_DIRCHG_ON)


/* ========== CHG_STAT2 0xD1 ============ */
#define MT6370_SHIFT_CHG_VBUSOV_STAT	7

#define MT6370_MASK_CHG_VBUSOV_STAT	(1 << MT6370_SHIFT_CHG_VBUSOV_STAT)
/* ========== CHG_STAT4 0xD3 ============ */
#define MT6370_SHIFT_CHG_TMRI_STAT	3

#define MT6370_MASK_CHG_TMRI_STAT	(1 << MT6370_SHIFT_CHG_TMRI_STAT)
/* ========== CHG_STAT5 0xD4 ============ */
#define MT6370_SHIFT_CHG_IEOCI_STAT	7

#define MT6370_MASK_CHG_IEOCI_STAT	(1 << MT6370_SHIFT_CHG_IEOCI_STAT)
/* ========== DPDM_STAT 0xD6 ============ */
#define MT6370_SHIFT_DCDTI_STAT		7

#define MT6370_MASK_DCDTI_STAT		(1 << MT6370_SHIFT_DCDTI_STAT)

#define MT6370_VOBST_MASK		GENMASK(7, 2)
#define MT6370_OTG_OC_MASK		GENMASK(2, 0)
#define MT6370_OTG_PIN_EN_MASK		BIT(1)
#define MT6370_OPA_MODE_MASK		BIT(0)
#define MT6370_BATOVP_LVL_MASK		GENMASK(6, 5)


#define MT6370_MIVR_IBUS_TH_100_mA	100
#define MT6370_AICL_VTH_MAX		4800000
#define MT6370_MIVR_MAX			13400000

#define RT5081_VENDOR_ID		(0x80)
#define MT6370_VENDOR_ID	(0xE0)
#define MT6370_CHG_PROPERTIES_SIZE	12
#define PHY_MODE_BC11_SET		1
#define PHY_MODE_BC11_CLR		2

enum mt6370_chg_reg_field {
	/* MT6370_REG_CHG_CTRL2 */
	F_IINLMTSEL, F_CFO_EN, F_CHG_EN,
	/* MT6370_REG_CHG_CTRL3 */
	F_IAICR, F_AICR_EN, F_ILIM_EN,
	/* MT6370_REG_CHG_CTRL4 */
	F_VOREG,
	/* MT6370_REG_CHG_CTRL6 */
	F_VMIVR,
	/* MT6370_REG_CHG_CTRL7 */
	F_ICHG,
	/* MT6370_REG_CHG_CTRL8 */
	F_IPREC,
	/* MT6370_REG_CHG_CTRL9 */
	F_IEOC,
	/* MT6370_REG_DEVICE_TYPE */
	F_USBCHGEN,
	/* MT6370_REG_USB_STATUS1 */
	F_USB_STAT, F_CHGDET,
	/* MT6370_REG_CHG_STAT */
	F_CHG_STAT, F_BOOST_STAT, F_VBAT_LVL,
	/* MT6370_REG_FLED_EN */
	F_FL_STROBE,
	/* MT6370_REG_CHG_STAT1 */
	F_CHG_MIVR_STAT,
	/* MT6370_REG_OVPCTRL_STAT */
	F_UVP_D_STAT,
	F_MAX
};

enum mt6370_irq {
	MT6370_IRQ_ATTACH_I = 0,
	MT6370_IRQ_UVP_D_EVT,
	MT6370_IRQ_MIVR,
	MT6370_IRQ_TREG,
	MT6370_IRQ_VBUSOV,
	MT6370_IRQ_TMRI,
	MT6370_IRQ_AICLMEASI,
	MT6370_IRQ_WDTMRI,
	MT6370_IRQ_RECHGI,
	MT6370_IRQ_IEOCI,
	MT6370_IRQ_DCDTI,
	MT6370_IRQ_MAX
};

enum mt6370_charger_irqidx {
	MT6370_CHG_IRQIDX_CHGIRQ1 = 0,
	MT6370_CHG_IRQIDX_CHGIRQ2,
	MT6370_CHG_IRQIDX_CHGIRQ3,
	MT6370_CHG_IRQIDX_CHGIRQ4,
	MT6370_CHG_IRQIDX_CHGIRQ5,
	MT6370_CHG_IRQIDX_CHGIRQ6,
	MT6370_CHG_IRQIDX_QCIRQ,
	MT6370_CHG_IRQIDX_DICHGIRQ7,
	MT6370_CHG_IRQIDX_OVPCTRLIRQ,
	MT6370_CHG_IRQIDX_MAX,
};

static enum power_supply_property mt6370_chg_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
};


struct mt6370_priv {
	struct device *dev;
	struct iio_channel *iio_adcs;
	struct mutex attach_lock;
	struct mutex ichg_access_lock;
	struct mutex aicr_access_lock;
	struct mutex irq_access_lock;
	struct mutex pe_access_lock;
	struct mutex hidden_mode_lock;
	struct mutex pp_lock;
	struct mutex tchg_lock;
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	struct regmap *regmap;
	struct regmap_field *rmap_fields[F_MAX];
	struct regulator_dev *rdev;
	struct workqueue_struct *wq;
	struct work_struct bc12_work;
	struct delayed_work mivr_dwork;
	struct charger_device *chgdev;
	struct charger_properties chg_props;
	struct charger_properties ls_props;
	unsigned int irq_nums[MT6370_IRQ_MAX];
	atomic_t attach;
	int psy_usb_type;
	bool pwr_rdy;
	union power_supply_propval *old_propval;
	int tchg;
	int aicr_limit;
	bool ieoc_wkard;
	bool pp_en;
	bool bc12_dn;
	bool dcd_timeout;
	u32 ichg;
	u32 mivr;
	u32 ieoc;
	u32 ichg_dis_chg;
	u32 zcv;
	u32 hidden_mode_cnt;
	u32 chip_vid;
	u8 irq_flag[MT6370_CHG_IRQIDX_MAX];
	wait_queue_head_t wait_queue;
	atomic_t bc12_cnt;
};

struct mt6370_chg_platform_data {
	u32 ichg;
	u32 aicr;
	u32 mivr;
	u32 cv;
	u32 ieoc;
	u32 safety_timer;
	u32 ircmp_resistor;
	u32 ircmp_vclamp;
	u32 dc_wdt;
	u32 lbp_hys_sel;
	u32 lbp_dt;
	u32 boot_mode;
	u32 boot_type;
	bool en_te;
	bool en_wdt;
	bool en_otg_wdt;
	bool en_polling;
	bool disable_vlgc;
	bool fast_unknown_ta_dect;
	bool post_aicl;
	const char *chgdev_name;
	const char *ls_dev_name;
};

/* These default values will be used if there's no property in dts */
static struct mt6370_chg_platform_data mt6370_default_chg_desc = {
	.ichg = 2000000,		/* uA */
	.aicr = 500000,			/* uA */
	.mivr = 4400000,		/* uV */
	.cv = 4350000,			/* uA */
	.ieoc = 250000,			/* uA */
	.safety_timer = 12,		/* hour */
#ifdef CONFIG_MTK_BIF_SUPPORT
	.ircmp_resistor = 0,		/* uohm */
	.ircmp_vclamp = 0,		/* uV */
#else
	.ircmp_resistor = 25000,	/* uohm */
	.ircmp_vclamp = 32000,		/* uV */
#endif
	.dc_wdt = 4000000,		/* us */
	.en_te = true,
	.en_wdt = true,
	.en_polling = false,
	.post_aicl = true,
	.chgdev_name = "primary_chg",
	.ls_dev_name = "primary_load_switch",
};
enum mt6370_usb_status {
	MT6370_USB_STAT_NO_VBUS = 0,
	MT6370_USB_STAT_VBUS_FLOW_IS_UNDER_GOING,
	MT6370_USB_STAT_SDP,
	MT6370_USB_STAT_SDP_NSTD,
	MT6370_USB_STAT_DCP,
	MT6370_USB_STAT_CDP,
	MT6370_USB_STAT_MAX
};

enum mt6370_charging_status {
	MT6370_CHG_STATUS_READY = 0,
	MT6370_CHG_STATUS_PROGRESS,
	MT6370_CHG_STATUS_DONE,
	MT6370_CHG_STATUS_FAULT,
	MT6370_CHG_STATUS_MAX,
};

struct mt6370_chg_field {
	const char *name;
	const struct linear_range *range;
	struct reg_field field;
};

enum {
	MT6370_CHAN_VBUSDIV5 = 0,
	MT6370_CHAN_VBUSDIV2,
	MT6370_CHAN_VSYS,
	MT6370_CHAN_VBAT,
	MT6370_CHAN_TS_BAT,
	MT6370_CHAN_IBUS,
	MT6370_CHAN_IBAT,
	MT6370_CHAN_CHG_VDDP,
	MT6370_CHAN_TEMP_JC,
	MT6370_CHAN_MAX
};
enum {
	MT6370_RANGE_F_IAICR = 0,
	MT6370_RANGE_F_VOREG,
	MT6370_RANGE_F_VMIVR,
	MT6370_RANGE_F_ICHG,
	MT6370_RANGE_F_IPREC,
	MT6370_RANGE_F_IEOC,
	MT6370_RANGE_F_AICL_VTH,
	MT6370_RANGE_F_PUMPX,
	MT6370_RANGE_F_WT_FC,
	MT6370_RANGE_F_IR_CMP,
	MT6370_RANGE_F_VCLAMP,
	MT6370_RANGE_F_MAX
};

static const unsigned int mt6370_reg_en_hidden_mode[] = {
	MT6370_REG_HIDDENPASCODE1,
	MT6370_REG_HIDDENPASCODE2,
	MT6370_REG_HIDDENPASCODE3,
	MT6370_REG_HIDDENPASCODE4,
};

static const unsigned char mt6370_val_en_hidden_mode[] = {
	0x96, 0x69, 0xC3, 0x3C,
};

static const u32 mt6370_dc_wdt[] = {
	0, 125000, 250000, 500000, 1000000, 2000000, 4000000, 8000000,
}; /* us */

static const struct linear_range mt6370_chg_ranges[MT6370_RANGE_F_MAX] = {
	[MT6370_RANGE_F_IAICR] = LINEAR_RANGE_IDX(100000, 0x0, 0x3F, 50000),
	[MT6370_RANGE_F_VOREG] = LINEAR_RANGE_IDX(3900000, 0x0, 0x51, 10000),
	[MT6370_RANGE_F_VMIVR] = LINEAR_RANGE_IDX(3900000, 0x0, 0x5F, 100000),
	[MT6370_RANGE_F_ICHG]  = LINEAR_RANGE_IDX(500000, 0x04, 0x31, 100000),
	[MT6370_RANGE_F_IPREC] = LINEAR_RANGE_IDX(100000, 0x0, 0x0F, 50000),
	[MT6370_RANGE_F_IEOC]  = LINEAR_RANGE_IDX(100000, 0x0, 0x0F, 50000),
	[MT6370_RANGE_F_AICL_VTH]  = LINEAR_RANGE_IDX(4100000, 0x0, 0x07, 100000),
	[MT6370_RANGE_F_PUMPX]  = LINEAR_RANGE_IDX(5500000, 0x0, 0x07, 500000),
	[MT6370_RANGE_F_WT_FC]  = LINEAR_RANGE_IDX(4, 0x0, 0x07, 2),
	[MT6370_RANGE_F_IR_CMP]  = LINEAR_RANGE_IDX(0, 0x0, 0x07, 25000),
	[MT6370_RANGE_F_VCLAMP]  = LINEAR_RANGE_IDX(0, 0x0, 0x07, 32000),
};

#define MT6370_CHG_FIELD(_fd, _reg, _lsb, _msb)				\
[_fd] = {								\
	.name = #_fd,							\
	.range = NULL,							\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
}

#define MT6370_CHG_FIELD_RANGE(_fd, _reg, _lsb, _msb)			\
[_fd] = {								\
	.name = #_fd,							\
	.range = &mt6370_chg_ranges[MT6370_RANGE_##_fd],		\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
}

static const struct mt6370_chg_field mt6370_chg_fields[F_MAX] = {
	MT6370_CHG_FIELD(F_IINLMTSEL, MT6370_REG_CHG_CTRL2, 2, 3),
	MT6370_CHG_FIELD(F_CFO_EN, MT6370_REG_CHG_CTRL2, 1, 1),
	MT6370_CHG_FIELD(F_CHG_EN, MT6370_REG_CHG_CTRL2, 0, 0),
	MT6370_CHG_FIELD_RANGE(F_IAICR, MT6370_REG_CHG_CTRL3, 2, 7),
	MT6370_CHG_FIELD(F_AICR_EN, MT6370_REG_CHG_CTRL3, 1, 1),
	MT6370_CHG_FIELD(F_ILIM_EN, MT6370_REG_CHG_CTRL3, 0, 0),
	MT6370_CHG_FIELD_RANGE(F_VOREG, MT6370_REG_CHG_CTRL4, 1, 7),
	MT6370_CHG_FIELD_RANGE(F_VMIVR, MT6370_REG_CHG_CTRL6, 1, 7),
	MT6370_CHG_FIELD_RANGE(F_ICHG, MT6370_REG_CHG_CTRL7, 2, 7),
	MT6370_CHG_FIELD_RANGE(F_IPREC, MT6370_REG_CHG_CTRL8, 0, 3),
	MT6370_CHG_FIELD_RANGE(F_IEOC, MT6370_REG_CHG_CTRL9, 4, 7),
	MT6370_CHG_FIELD(F_USBCHGEN, MT6370_REG_DEVICE_TYPE, 7, 7),
	MT6370_CHG_FIELD(F_USB_STAT, MT6370_REG_USB_STATUS1, 4, 6),
	MT6370_CHG_FIELD(F_CHGDET, MT6370_REG_USB_STATUS1, 3, 3),
	MT6370_CHG_FIELD(F_CHG_STAT, MT6370_REG_CHG_STAT, 6, 7),
	MT6370_CHG_FIELD(F_BOOST_STAT, MT6370_REG_CHG_STAT, 3, 3),
	MT6370_CHG_FIELD(F_VBAT_LVL, MT6370_REG_CHG_STAT, 5, 5),
	MT6370_CHG_FIELD(F_FL_STROBE, MT6370_REG_FLED_EN, 2, 2),
	MT6370_CHG_FIELD(F_CHG_MIVR_STAT, MT6370_REG_CHG_STAT1, 6, 6),
	MT6370_CHG_FIELD(F_UVP_D_STAT, MT6370_REG_OVPCTRL_STAT, 4, 4),
};

static const u32 mt6370_otg_oc_threshold[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000,
}; /* uA */

static u8 mt6370_find_closest_reg_value_via_table(const u32 *value_table,
	u32 table_size, u32 target_value)
{
	u32 i = 0;

	/* Smaller than minimum supported value, use minimum one */
	if (target_value < value_table[0])
		return 0;

	for (i = 0; i < table_size - 1; i++) {
		if (target_value >= value_table[i] &&
		    target_value < value_table[i + 1])
			return i;
	}

	/* Greater than maximum supported value, use maximum one */
	return table_size - 1;
}

static inline int mt6370_set_bits(struct mt6370_priv *priv, u32 reg, u8 mask)
{
	return regmap_update_bits(priv->regmap, reg, mask, mask);
}

static inline int mt6370_clr_bits(struct mt6370_priv *priv, u32 reg, u8 mask)
{
	return regmap_update_bits(priv->regmap, reg, mask, 0);
}

static inline void mt6370_chg_irq_set_flag(
	struct mt6370_priv *priv, u8 *irq, u8 mask)
{
	mutex_lock(&priv->irq_access_lock);
	*irq |= mask;
	mutex_unlock(&priv->irq_access_lock);
}
static inline void mt6370_chg_irq_clr_flag(
		struct mt6370_priv *priv, u8 *irq, u8 mask)
{
	mutex_lock(&priv->irq_access_lock);
	*irq &= ~mask;
	mutex_unlock(&priv->irq_access_lock);
}

static inline int mt6370_reg_test_bit(
	struct mt6370_priv *priv, u32 cmd, u8 shift, bool *is_one)
{
	int ret = 0;
	u8 data = 0;
	u32 val;

	ret = regmap_read(priv->regmap, cmd, &val);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = val & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return ret;
}

static inline int mt6370_chg_field_get(struct mt6370_priv *priv,
				       enum mt6370_chg_reg_field fd,
				       unsigned int *val)
{
	int ret;
	unsigned int reg_val;
	u32 idx = fd;

	ret = regmap_field_read(priv->rmap_fields[idx], &reg_val);
	if (ret)
		return ret;

	if (mt6370_chg_fields[idx].range)
		return linear_range_get_value(mt6370_chg_fields[idx].range,
					       reg_val, val);

	*val = reg_val;
	return 0;
}

static void mt6370_linear_range_get_selector_within(const struct linear_range *r,
						    unsigned int val,
						    unsigned int *selector)
{
	if (r->min > val) {
		*selector = r->min_sel;
		return;
	}

	if (linear_range_get_max_value(r) < val) {
		*selector = r->max_sel;
		return;
	}

	if (r->step == 0)
		*selector = r->min_sel;
	else
		*selector = (val - r->min) / r->step + r->min_sel;
}

static int mt6370_linear_range_get_selector_high(const struct linear_range *r,
						 unsigned int val, unsigned int *selector,
						 bool *found)
{
	*found = false;

	if (linear_range_get_max_value(r) < val)
		return -EINVAL;

	if (r->min > val) {
		*selector = r->min_sel;
		return 0;
	}

	*found = true;

	if (r->step == 0)
		*selector = r->max_sel;
	else
		*selector = DIV_ROUND_UP(val - r->min, r->step) + r->min_sel;

	return 0;
}
static inline int mt6370_chg_field_set(struct mt6370_priv *priv,
				       enum mt6370_chg_reg_field fd,
				       unsigned int val)
{
	int ret;
	bool f;
	const struct linear_range *r;
	u32 idx = fd;

	if (mt6370_chg_fields[idx].range) {
		r = mt6370_chg_fields[idx].range;

		if (fd == F_VMIVR) {
			ret = mt6370_linear_range_get_selector_high(r, val, &val, &f);
			if (ret)
				val = r->max_sel;
		} else {
			mt6370_linear_range_get_selector_within(r, val, &val);
		}
	}

	return regmap_field_write(priv->rmap_fields[idx], val);
}

enum {
	MT6370_CHG_STAT_READY = 0,
	MT6370_CHG_STAT_CHARGE_IN_PROGRESS,
	MT6370_CHG_STAT_DONE,
	MT6370_CHG_STAT_FAULT,
	MT6370_CHG_STAT_MAX
};

enum {
	ATTACH_TYPE_NONE = 0,
	ATTACH_TYPE_PWR_RDY,
	ATTACH_TYPE_TYPEC,
	ATTACH_TYPE_PD,
	ATTACH_TYPE_PD_SDP,
	ATTACH_TYPE_PD_DCP,
	ATTACH_TYPE_PD_NONSTD,
	ATTACH_STAT_ATTACH_MAX
};

enum mt6370_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

static int mt6370_chg_otg_of_parse_cb(struct device_node *of,
				      const struct regulator_desc *rdesc,
				      struct regulator_config *rcfg)
{
	struct mt6370_priv *priv = rcfg->driver_data;

	rcfg->ena_gpiod = fwnode_gpiod_get_index(of_fwnode_handle(of),
						 "enable", 0, GPIOD_OUT_LOW |
						 GPIOD_FLAGS_BIT_NONEXCLUSIVE,
						 rdesc->name);
	if (IS_ERR(rcfg->ena_gpiod)) {
		rcfg->ena_gpiod = NULL;
		return 0;
	}

	return regmap_update_bits(priv->regmap, MT6370_REG_CHG_CTRL1,
				  MT6370_OTG_PIN_EN_MASK,
				  MT6370_OTG_PIN_EN_MASK);
}

static bool is_usb_rdy(struct device *dev)
{
	bool ready = true;
	struct device_node *node;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		mt_dbg(dev, "usb ready = %d\n", ready);
	} else
		dev_warn(dev, "usb node missing or invalid\n");
	return ready;
}

static int mt6370_chg_set_usbsw(struct mt6370_priv *priv,
				enum mt6370_usbsw usbsw)
{
	struct phy *phy;
	int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
					       PHY_MODE_BC11_CLR;

	mt_dbg(priv->dev, "usbsw=%d\n", usbsw);
	phy = phy_get(priv->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(priv->dev, "failed to get usb2-phy\n");
		return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_err(priv->dev, "failed to set phy ext mode\n");
	phy_put(priv->dev, phy);

	return ret;
}

#ifndef CONFIG_MT6370_DCDTOUT_SUPPORT
static int __maybe_unused mt6370_enable_dcd_tout(
			      struct mt6370_priv *priv, bool en)
{
	dev_info(priv->dev, "%s en = %d\n", __func__, en);
	return (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_DEVICE_TYPE,
		 MT6370_MASK_DCDTOUTEN);
}

static int __maybe_unused mt6370_is_dcd_tout_enable(
			     struct mt6370_priv *priv, bool *en)
{
	int ret;
	u32 val;

	ret = regmap_read(priv->regmap, MT6370_REG_DEVICE_TYPE, &val);
	if (ret < 0) {
		*en = false;
		return ret;
	}
	*en = (val & MT6370_MASK_DCDTOUTEN ? true : false);
	return 0;
}
#endif

static int mt6370_chg_enable_bc12(struct mt6370_priv *priv, bool en)
{
	int i, ret, attach;
	static const int max_wait_cnt = 250;
#ifndef CONFIG_MT6370_DCDTOUT_SUPPORT
	bool dcd_en = false;
#endif /* CONFIG_MT6370_DCDTOUT_SUPPORT */

	mt_dbg(priv->dev, "en=%d\n", en);
	if (en) {
#ifndef CONFIG_MT6370_DCDTOUT_SUPPORT
		ret = mt6370_is_dcd_tout_enable(priv, &dcd_en);
		if (!dcd_en)
			msleep(180);
#endif /* CONFIG_MT6370_DCDTOUT_SUPPORT */
		/* CDP port specific process */
		dev_info(priv->dev, "check CDP block\n");
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy(priv->dev))
				break;
			attach = atomic_read(&priv->attach);
			if (attach == ATTACH_TYPE_TYPEC)
				msleep(100);
			else {
				dev_notice(priv->dev, "%s: change attach:%d, disable bc12\n",
					   __func__, attach);
				en = false;
				break;
			}
		}
		if (i == max_wait_cnt)
			dev_notice(priv->dev, "CDP timeout\n", __func__);
		else
			dev_info(priv->dev, "CDP free\n", __func__);
	}
	ret = mt6370_chg_set_usbsw(priv, en ? USBSW_CHG : USBSW_USB);
	if (ret) {
		dev_err(priv->dev, "failed to set usbw\n");
		return ret;
	}
	return mt6370_chg_field_set(priv, F_USBCHGEN, en);
}

static inline int mt6370_toggle_chgdet_flow(struct mt6370_priv *priv)
{
	int ret = 0;
	u8 data = 0;

	/* read data */
	ret = regmap_bulk_read(priv->regmap, MT6370_REG_DEVICE_TYPE, &data, 1);
	if (ret < 0) {
		dev_err(priv->dev, "%s: read usbd fail\n", __func__);
		goto out;
	}

	/* usbd off */
	data &= ~MT6370_MASK_USBCHGEN;
	ret = regmap_bulk_write(priv->regmap,
				MT6370_REG_DEVICE_TYPE, &data, 1);
	if (ret < 0) {
		dev_err(priv->dev, "%s: usbd off fail\n", __func__);
		goto out;
	}

	udelay(40);

	/* usbd on */
	data |= MT6370_MASK_USBCHGEN;
	ret = regmap_bulk_write(priv->regmap,
				MT6370_REG_DEVICE_TYPE, &data, 1);
	if (ret < 0)
		dev_err(priv->dev, "%s: usbd on fail\n", __func__);
out:

	return ret;
}
static int mt6370_bc12_workaround(struct mt6370_priv *priv)
{
	int ret = 0;

	dev_info(priv->dev, "%s\n", __func__);


	ret = mt6370_toggle_chgdet_flow(priv);
	if (ret < 0)
		dev_err(priv->dev, "%s: fail\n", __func__);

	mdelay(10);

	ret = mt6370_toggle_chgdet_flow(priv);
	if (ret < 0)
		dev_err(priv->dev, "%s: fail\n", __func__);


	return ret;

}
static inline bool mt6370_is_meta_mode(struct mt6370_priv *priv)
{
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(priv->dev);

	return (pdata->boot_mode == 1 ||
		pdata->boot_mode == 5);
}

static void mt6370_power_supply_changed(struct mt6370_priv *priv)
{
	int ret = 0, i = 0;
	union power_supply_propval propval[MT6370_CHG_PROPERTIES_SIZE];

	mt_dbg(priv->dev, "%s:\n", __func__);
	memset(propval, 0, sizeof(propval));

	for (i = 0; i < ARRAY_SIZE(propval); i++) {
		ret = power_supply_get_property(priv->psy,
						mt6370_chg_properties[i],
						&propval[i]);
		if (ret < 0)
			dev_notice(priv->dev,
				   "%s: get prop fail(%d), i = %d\n",
				   __func__, ret, i);
	}

	if (memcmp(priv->old_propval, propval, sizeof(propval))) {
		mt_dbg(priv->dev, "%s: set success\n", __func__);
		memcpy(priv->old_propval, propval, sizeof(propval));
		power_supply_changed(priv->psy);
	}
}

static void mt6370_chg_bc12_work_func(struct work_struct *work)
{
	struct mt6370_priv *priv = container_of(work, struct mt6370_priv,
						bc12_work);
	int ret;
	bool bc12_en = false, rpt_psy = true, bc12_ctrl = true;
	unsigned int attach, usb_stat = 0;
	u8 chip_vid = priv->chip_vid;

	mutex_lock(&priv->attach_lock);
	mt_dbg(priv->dev, "attach = %d, bc12_dn = %d\n", atomic_read(&priv->attach), priv->bc12_dn);
	attach = atomic_read(&priv->attach);

	if (attach) {
		atomic_inc(&priv->bc12_cnt);
		if (mt6370_is_meta_mode(priv)) {
		/* Skip charger type detection to speed up meta boot.*/
			dev_notice(priv->dev, "force Standard USB Host in meta\n");
			priv->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			bc12_ctrl = false;
			goto bc12_work_func_out;
		}

		if (attach && priv->dcd_timeout) {
			mt_dbg(priv->dev, " dcd timeout%s, %d\n", __func__, __LINE__);
			priv->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			priv->dcd_timeout = false;
			goto bc12_work_func_out;
		}
	} else {
		atomic_set(&priv->bc12_cnt, 0);
	}

	switch (attach) {
	case ATTACH_TYPE_NONE:
		priv->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto bc12_work_func_out;
	case ATTACH_TYPE_TYPEC:
		if (!priv->bc12_dn) {
			bc12_en = true;
			rpt_psy = false;
			goto bc12_work_func_out;
		}
		ret = mt6370_chg_field_get(priv, F_USB_STAT, &usb_stat);
		if (ret < 0) {
			dev_err(priv->dev, "failed to get port stat\n");
			rpt_psy = false;
			goto bc12_work_func_out;
		}
		break;
	case ATTACH_TYPE_PD_SDP:
		usb_stat = MT6370_USB_STAT_SDP;
		break;
	case ATTACH_TYPE_PD_DCP:
		/* not to enable bc12 */
		priv->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		goto bc12_work_func_out;
	case ATTACH_TYPE_PD_NONSTD:
		usb_stat = MT6370_USB_STAT_SDP_NSTD;
		break;
	default:
		dev_err(priv->dev, "Invalid attach state\n");
		break;
	}


	switch (usb_stat) {
	case MT6370_USB_STAT_SDP:
		priv->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case MT6370_USB_STAT_SDP_NSTD:
		priv->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case MT6370_USB_STAT_DCP:
		priv->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case MT6370_USB_STAT_CDP:
		priv->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case MT6370_USB_STAT_NO_VBUS:
	case MT6370_USB_STAT_VBUS_FLOW_IS_UNDER_GOING:
		priv->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto bc12_work_func_out;
	default:
		dev_info(priv->dev, "Unknown port stat %d\n", usb_stat);
		bc12_ctrl = false;
		rpt_psy = false;
		goto bc12_work_func_out;
	}
	/* BC12 workaround (NONSTD -> STD) */
	if (atomic_read(&priv->bc12_cnt) < 3 &&
		priv->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP &&
		(chip_vid == RT5081_VENDOR_ID ||
		 chip_vid == MT6370_VENDOR_ID)) {
		ret = mt6370_bc12_workaround(priv);
		/* Workaround success, wait for next event */
		if (ret >= 0) {
			dev_info(priv->dev, "bc12 workaround success\n");
			bc12_ctrl = false;
			rpt_psy = false;
		}
		goto bc12_work_func_out;
	}

#ifdef MT6370_APPLE_SAMSUNG_TA_SUPPORT
	ret = mt6370_detect_apple_samsung_ta(priv);
	if (ret < 0)
		dev_err(priv->dev, "%s: detect a/ss ta fail(%d)\n",
			__func__, ret);
#endif
	mt_dbg(priv->dev, "%s, usb_stat = %d, psy_desc.type = %d, psy_usb_type = %d\n",
	       __func__, usb_stat, priv->psy_desc.type, priv->psy_usb_type);
bc12_work_func_out:
	mutex_unlock(&priv->attach_lock);
	if (bc12_ctrl && (mt6370_chg_enable_bc12(priv, bc12_en) < 0))
		dev_err(priv->dev, "failed to set bc12 = %d\n", bc12_en);
	if (rpt_psy)
		mt6370_power_supply_changed(priv);
}

static int mt6370_chg_toggle_cfo(struct mt6370_priv *priv)
{
	int ret;
	unsigned int fl_strobe;

	/* check if flash led in strobe mode */
	ret = mt6370_chg_field_get(priv, F_FL_STROBE, &fl_strobe);
	if (ret) {
		dev_err(priv->dev, "Failed to get FL_STROBE_EN\n");
		return ret;
	}

	if (fl_strobe) {
		dev_err(priv->dev, "Flash led is still in strobe mode\n");
		return ret;
	}

	/* cfo off */
	ret = mt6370_chg_field_set(priv, F_CFO_EN, 0);
	if (ret) {
		dev_err(priv->dev, "Failed to disable CFO_EN\n");
		return ret;
	}

	/* cfo on */
	ret = mt6370_chg_field_set(priv, F_CFO_EN, 1);
	if (ret)
		dev_err(priv->dev, "Failed to enable CFO_EN\n");

	return ret;
}

static int mt6370_chg_read_adc_chan(struct mt6370_priv *priv, unsigned int chan,
				    int *val)
{
	int ret;

	if (chan >= MT6370_CHAN_MAX)
		return -EINVAL;

	ret = iio_read_channel_processed(&priv->iio_adcs[chan], val);
	if (ret)
		dev_err(priv->dev, "Failed to read ADC\n");

	return ret;
}

static void mt6370_chg_mivr_dwork_func(struct work_struct *work)
{
	struct mt6370_priv *priv = container_of(work, struct mt6370_priv,
						mivr_dwork.work);
	int ret;
	unsigned int mivr_stat, ibus;

	ret = mt6370_chg_field_get(priv, F_CHG_MIVR_STAT, &mivr_stat);
	if (ret) {
		dev_err(priv->dev, "Failed to get mivr state\n");
		goto mivr_handler_out;
	}

	if (!mivr_stat)
		goto mivr_handler_out;

	ret = mt6370_chg_read_adc_chan(priv, MT6370_CHAN_IBUS, &ibus);
	if (ret) {
		dev_err(priv->dev, "Failed to get ibus\n");
		goto mivr_handler_out;
	}

	if (ibus < MT6370_MIVR_IBUS_TH_100_mA) {
		ret = mt6370_chg_toggle_cfo(priv);
		if (ret)
			dev_err(priv->dev, "Failed to toggle cfo\n");
	}

mivr_handler_out:
	enable_irq(priv->irq_nums[MT6370_IRQ_MIVR]);
	pm_relax(priv->dev);
}

static void __maybe_unused mt6370_chg_pwr_rdy_check(struct mt6370_priv *priv)
{
	int ret;
	unsigned int pwr_rdy, otg_en;
	union power_supply_propval val;

	/* Check in OTG mode or not */
	ret = mt6370_chg_field_get(priv, F_BOOST_STAT, &otg_en);
	if (ret) {
		dev_err(priv->dev, "Failed to get OTG state\n");
		return;
	}

	if (otg_en)
		return;

	ret = mt6370_chg_field_get(priv, F_UVP_D_STAT, &pwr_rdy);
	if (ret) {
		dev_err(priv->dev, "Failed to get pwr_rdy state reg\n");
		return;
	}

	if (pwr_rdy)
		val.intval = ATTACH_TYPE_NONE;
	else
		val.intval = ATTACH_TYPE_TYPEC;

	ret = power_supply_set_property(priv->psy, POWER_SUPPLY_PROP_ONLINE,
					&val);
	if (ret)
		dev_err(priv->dev, "Failed to start attach/detach flow\n");
}

static int mt6370_chg_get_online(struct mt6370_priv *priv,
				 union power_supply_propval *val)
{
	mutex_lock(&priv->attach_lock);
	val->intval = atomic_read(&priv->attach);
	mutex_unlock(&priv->attach_lock);

	return 0;
}

static int mt6370_chg_is_enabled(struct mt6370_priv *priv, bool *en)
{
	int ret = 0;
	u32 val = 0;

	ret = mt6370_chg_field_get(priv, F_CHG_EN, &val);
	if (ret < 0)
		return ret;
	*en = val;
	return 0;
}

static int mt6370_chg_get_status(struct mt6370_priv *priv,
				 union power_supply_propval *val)
{
	int ret;
	unsigned int chg_stat;
	bool __maybe_unused chg_en = false;

/*mt6357 do charger type detection*/
#if IS_ENABLED(CONFIG_MT6357_DO_CTD)
	struct power_supply *chg_psy = NULL;

	chg_psy = devm_power_supply_get_by_phandle(priv->dev,
						"charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pr_info("get chg_psy fail %s\n", __func__);
		return -1;
	}

	ret = power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_ONLINE, val);
	if (ret) {
		dev_err(priv->dev, "Failed to get online status\n");
		return ret;
	}

	if (!val->intval) {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	}

#else
/*mt6370 do charger type detection*/
	union power_supply_propval online;

	ret = power_supply_get_property(priv->psy, POWER_SUPPLY_PROP_ONLINE, &online);
	if (ret) {
		dev_err(priv->dev, "Failed to get online status\n");
		return ret;
	}

	if (!online.intval) {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	}
#endif

	ret = mt6370_chg_field_get(priv, F_CHG_STAT, &chg_stat);
	if (ret)
		return ret;

	switch (chg_stat) {
	case MT6370_CHG_STAT_READY:
		ret = mt6370_chg_is_enabled(priv, &chg_en);
		if (ret)
			return ret;
		if (chg_en)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return ret;
	case MT6370_CHG_STAT_FAULT:
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return ret;
	case MT6370_CHG_STAT_CHARGE_IN_PROGRESS:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return ret;
	case MT6370_CHG_STAT_DONE:
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return ret;
	default:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		return ret;
	}
}

static int mt6370_chg_get_charge_type(struct mt6370_priv *priv,
				      union power_supply_propval *val)
{
	int type, ret;
	unsigned int chg_stat, vbat_lvl;

	ret = mt6370_chg_field_get(priv, F_CHG_STAT, &chg_stat);
	if (ret)
		return ret;

	ret = mt6370_chg_field_get(priv, F_VBAT_LVL, &vbat_lvl);
	if (ret)
		return ret;

	switch (chg_stat) {
	case MT6370_CHG_STAT_CHARGE_IN_PROGRESS:
		if (vbat_lvl)
			type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case MT6370_CHG_STAT_READY:
	case MT6370_CHG_STAT_DONE:
	case MT6370_CHG_STAT_FAULT:
	default:
		type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	val->intval = type;

	return 0;
}

static int mt6370_chg_set_online(struct mt6370_priv *priv,
				 const union power_supply_propval *val)
{
	bool pwr_rdy = !!val->intval;
	int attach = val->intval;

	mt_dbg(priv->dev, "%s, attach = %d\n", __func__, attach);
	mutex_lock(&priv->attach_lock);
	if (pwr_rdy == !!atomic_read(&priv->attach)) {
		dev_err(priv->dev, "pwr_rdy is same(%d)\n", pwr_rdy);
		mutex_unlock(&priv->attach_lock);
		return 0;
	}

	if (attach == ATTACH_TYPE_NONE)
		priv->bc12_dn = false;

	if (!priv->bc12_dn)
		atomic_set(&priv->attach, attach);
	mutex_unlock(&priv->attach_lock);

	if (attach > ATTACH_TYPE_PD && priv->bc12_dn)
		return 0;

	if (!queue_work(priv->wq, &priv->bc12_work))
		dev_err(priv->dev, "bc12 work has already queued\n");

	return 0;
}

static int mt6370_enable_hidden_mode(struct mt6370_priv *priv, bool en)
{
	int ret = 0;

	mutex_lock(&priv->hidden_mode_lock);

	if (en) {
		if (priv->hidden_mode_cnt == 0) {
			ret = regmap_bulk_write(priv->regmap,
						mt6370_reg_en_hidden_mode[0],
						mt6370_val_en_hidden_mode,
						ARRAY_SIZE(mt6370_val_en_hidden_mode));
			if (ret < 0)
				goto err;
		}
		priv->hidden_mode_cnt++;
	} else {
		if (priv->hidden_mode_cnt == 1) /* last one */
			ret = regmap_write(priv->regmap,
					   mt6370_reg_en_hidden_mode[0], 0x00);
		priv->hidden_mode_cnt--;
		if (ret < 0)
			goto err;
	}
	mt_dbg(priv->dev, "%s: en = %d\n", __func__, en);
	goto out;

err:
	dev_info(priv->dev, "%s: en = %d fail(%d)\n", __func__, en, ret);
out:
	mutex_unlock(&priv->hidden_mode_lock);
	return ret;
}

static int __mt6370_get_cv(struct mt6370_priv *priv, u32 *cv)
{
	int ret = 0;
	union power_supply_propval val;

	ret = power_supply_get_property(priv->psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	if (ret < 0)
		return ret;

	*cv = val.intval;

	return ret;
}

static int mt6370_get_cv(struct charger_device *chgdev, u32 *cv)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);

	return __mt6370_get_cv(priv, cv);
}

static int __mt6370_set_cv(struct mt6370_priv *priv, u32 uV)
{
	int ret = 0, reg_val = 0;
	u32 ori_cv;

	/* Get the original cv to check if this step of setting cv is necessary */
	ret = __mt6370_get_cv(priv, &ori_cv);
	if (ret)
		return ret;

	if (ori_cv == uV)
		return 0;

	/* Enable hidden mode */
	ret = mt6370_enable_hidden_mode(priv, true);
	if (ret)
		return ret;

	/* Store BATOVP Level info */
	ret = regmap_read(priv->regmap, MT6370_REG_CHGHIDDENCTRL22, &reg_val);
	if (ret)
		goto out;

	/* Disable BATOVP */
	ret = regmap_write(priv->regmap, MT6370_REG_CHGHIDDENCTRL22,
			   reg_val | MT6370_BATOVP_LVL_MASK);
	if (ret)
		goto out;

	/* Set CV */
	ret = mt6370_chg_field_set(priv, F_VOREG, uV);
	if (ret)
		goto out;

	/* Delay 5ms */
	mdelay(5);

	/* Enable BATOVP and restore BATOVP level */
	ret = regmap_write(priv->regmap, MT6370_REG_CHGHIDDENCTRL22, reg_val);
out:
	/* Disable hidden mode */
	return mt6370_enable_hidden_mode(priv, false);
}

static int mt6370_set_cv(struct charger_device *chgdev, u32 uV)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);

	return __mt6370_set_cv(priv, uV);
}

static int mt6370_chg_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct mt6370_priv *priv = power_supply_get_drvdata(psy);

	if (!priv)
		return -ENODEV;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return mt6370_chg_get_online(priv, val);
	case POWER_SUPPLY_PROP_STATUS:
		return mt6370_chg_get_status(priv, val);
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		return mt6370_chg_get_charge_type(priv, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return mt6370_chg_field_get(priv, F_ICHG, &val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = linear_range_get_max_value(&mt6370_chg_ranges[MT6370_RANGE_F_ICHG]);
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return mt6370_chg_field_get(priv, F_VOREG, &val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = linear_range_get_max_value(&mt6370_chg_ranges[MT6370_RANGE_F_VOREG]);
		return 0;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return mt6370_chg_field_get(priv, F_IAICR, &val->intval);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		return mt6370_chg_field_get(priv, F_VMIVR, &val->intval);
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		return mt6370_chg_field_get(priv, F_IPREC, &val->intval);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return mt6370_chg_field_get(priv, F_IEOC, &val->intval);
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = priv->psy_usb_type;
		return 0;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = priv->psy_desc.type;
		return 0;
	default:
		return -EINVAL;
	}
}

static int mt6370_chg_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	struct mt6370_priv *priv = power_supply_get_drvdata(psy);

	if (!priv)
		return -ENODEV;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return mt6370_chg_set_online(priv, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return mt6370_chg_field_set(priv, F_ICHG, val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return __mt6370_set_cv(priv, val->intval);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return mt6370_chg_field_set(priv, F_IAICR, val->intval);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		return mt6370_chg_field_set(priv, F_VMIVR, val->intval);
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		return mt6370_chg_field_set(priv, F_IPREC, val->intval);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return mt6370_chg_field_set(priv, F_IEOC, val->intval);
	default:
		return -EINVAL;
	}
}

static int mt6370_chg_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_usb_type mt6370_chg_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};

static const struct power_supply_desc mt6370_chg_psy_desc = {
	.name = "mt6370-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = mt6370_chg_properties,
	.num_properties = ARRAY_SIZE(mt6370_chg_properties),
	.get_property = mt6370_chg_get_property,
	.set_property = mt6370_chg_set_property,
	.property_is_writeable = mt6370_chg_property_is_writeable,
	.usb_types = mt6370_chg_usb_types,
	.num_usb_types = ARRAY_SIZE(mt6370_chg_usb_types),
};

static const struct regulator_ops mt6370_chg_otg_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_current_limit = regulator_set_current_limit_regmap,
	.get_current_limit = regulator_get_current_limit_regmap,
};

static const u32 mt6370_chg_otg_oc_ma[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000,
};

static const struct regulator_desc mt6370_chg_otg_rdesc = {
	.of_match = "usb-otg-vbus-regulator",
	.of_parse_cb = mt6370_chg_otg_of_parse_cb,
	.name = "mt6370-usb-otg-vbus",
	.ops = &mt6370_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.min_uV = 4425000,
	.uV_step = 25000,
	.n_voltages = 57,
	.vsel_reg = MT6370_REG_CHG_CTRL5,
	.vsel_mask = MT6370_VOBST_MASK,
	.enable_reg = MT6370_REG_CHG_CTRL1,
	.enable_mask = MT6370_OPA_MODE_MASK,
	.curr_table = mt6370_chg_otg_oc_ma,
	.n_current_limits = ARRAY_SIZE(mt6370_chg_otg_oc_ma),
	.csel_reg = MT6370_REG_CHG_CTRL10,
	.csel_mask = MT6370_OTG_OC_MASK,
};

static int mt6370_chg_init_rmap_fields(struct mt6370_priv *priv)
{
	int i;
	const struct mt6370_chg_field *fds = mt6370_chg_fields;

	for (i = 0; i < F_MAX; i++) {
		priv->rmap_fields[i] = devm_regmap_field_alloc(priv->dev,
							       priv->regmap,
							       fds[i].field);
		if (IS_ERR(priv->rmap_fields[i]))
			return dev_err_probe(priv->dev,
					     PTR_ERR(priv->rmap_fields[i]),
					     "Failed to allocate regmapfield[%s]\n",
					     fds[i].name);
	}

	return 0;
}

static int mt6370_set_fast_charge_timer(
	struct mt6370_priv *priv, u32 hour)
{
	int ret = 0;
	u32 reg_fct = 0;
	const struct linear_range *r = &mt6370_chg_ranges[MT6370_RANGE_F_WT_FC];


	mt6370_linear_range_get_selector_within(r, hour, &reg_fct);

	dev_info(priv->dev, "%s: timer = %d (0x%02X)\n", __func__, hour, reg_fct);

	ret = regmap_update_bits(priv->regmap,
				 MT6370_REG_CHG_CTRL12,
				 MT6370_MASK_WT_FC,
				 reg_fct << MT6370_SHIFT_WT_FC);

	return ret;
}

static int mt6370_set_dc_wdt(struct mt6370_priv *priv, u32 us)
{
	int ret = 0;
	u8 reg_wdt = 0;

	reg_wdt = mt6370_find_closest_reg_value_via_table(mt6370_dc_wdt,
							  ARRAY_SIZE(mt6370_dc_wdt),
							  us);

	dev_info(priv->dev, "%s: wdt = %dms(0x%02X)\n", __func__, us / 1000, reg_wdt);

	ret = regmap_update_bits(priv->regmap,
				 MT6370_REG_CHGDIRCHG2,
				 MT6370_MASK_DC_WDT,
				 reg_wdt << MT6370_SHIFT_DC_WDT);

	return ret;
}

static int mt6370_enable_jeita(struct mt6370_priv *priv, bool en)
{
	int ret = 0;

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL16, MT6370_MASK_JEITA_EN);

	return ret;
}

static int mt6370_enable_hz(struct mt6370_priv *priv, bool en)
{
	int ret = 0;

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL1, MT6370_MASK_HZ_EN);

	return ret;
}

static int mt6370_set_ircmp_resistor(struct mt6370_priv *priv,
	u32 uohm)
{
	int ret = 0;
	u8 reg_resistor = 0;
	const struct linear_range *r = &mt6370_chg_ranges[MT6370_RANGE_F_IR_CMP];

	mt6370_linear_range_get_selector_within(r, uohm, &uohm);

	dev_info(priv->dev, "%s: resistor = %d (0x%02X)\n", __func__, uohm, reg_resistor);

	ret = regmap_update_bits(priv->regmap,
				 MT6370_REG_CHG_CTRL18,
				 MT6370_MASK_IRCMP_RES,
				 uohm << MT6370_SHIFT_IRCMP_RES);

	return ret;
}

static int mt6370_set_ircmp_vclamp(struct mt6370_priv *priv, u32 uV)
{
	int ret = 0;
	u32 reg_vclamp = 0;
	const struct linear_range *r = &mt6370_chg_ranges[MT6370_RANGE_F_VCLAMP];

	mt6370_linear_range_get_selector_within(r, uV, &reg_vclamp);

	dev_info(priv->dev, "%s: vclamp = %d (0x%02X)\n", __func__, uV, reg_vclamp);

	ret = regmap_update_bits(priv->regmap,
				 MT6370_REG_CHG_CTRL18,
				 MT6370_MASK_IRCMP_VCLAMP,
				 reg_vclamp << MT6370_SHIFT_IRCMP_VCLAMP);

	return ret;
}

static int mt6370_set_otglbp(
	struct mt6370_priv *priv, u32 lbp_hys_sel, u32 lbp_dt)
{
	u8 reg_data = (lbp_hys_sel << MT6370_SHIFT_LBPHYS_SEL)
			| (lbp_dt << MT6370_SHIFT_LBP_DT);

	dev_info(priv->dev, "%s: otglbp(%d), dt(%d)\n", __func__, lbp_hys_sel, lbp_dt);

	return regmap_update_bits(priv->regmap, MT6370_REG_VDDASUPPLY,
				  MT6370_MASK_LBP, reg_data);
}

static int mt6370_disable_vlgc(struct mt6370_priv *priv, bool dis)
{
	return (dis ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_QCSTATUS1, MT6370_MASK_VLGC_DISABLE);
}

static int mt6370_enable_fast_unknown_ta_dect(
	struct mt6370_priv *priv, bool en)
{
	return (en ? mt6370_clr_bits : mt6370_set_bits)
		(priv, MT6370_REG_USB_STATUS1, MT6370_MASK_FAST_UNKNOWN_TA_DECT);
}


#define MT6370_CHG_DT_PROP_DECL(_name, _type, _field)	\
{							\
	.name = "mediatek,chg-" #_name,			\
	.type = MT6370_PARSE_TYPE_##_type,		\
	.fd = _field,					\
}

static int mt6370_chg_init_otg_regulator(struct mt6370_priv *priv)
{
	struct regulator_config rcfg = {
		.dev = priv->dev,
		.regmap = priv->regmap,
		.driver_data = priv,
	};

	priv->rdev = devm_regulator_register(priv->dev, &mt6370_chg_otg_rdesc,
					     &rcfg);

	return PTR_ERR_OR_ZERO(priv->rdev);
}
static char *mt6370_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static int mt6370_chg_init_psy(struct mt6370_priv *priv)
{
	struct power_supply_config cfg = {
		.drv_data = priv,
		.of_node = dev_of_node(priv->dev),
		.supplied_to = mt6370_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(mt6370_psy_supplied_to),
	};

	memcpy(&priv->psy_desc, &mt6370_chg_psy_desc, sizeof(priv->psy_desc));
	priv->psy = devm_power_supply_register(priv->dev, &priv->psy_desc, &cfg);

	return PTR_ERR_OR_ZERO(priv->psy);
}

static void mt6370_chg_destroy_attach_lock(void *data)
{
	struct mutex *attach_lock = data;

	mutex_destroy(attach_lock);
}

static void mt6370_chg_destroy_tchg_lock(void *data)
{
	struct mutex *tchg_lock = data;

	mutex_destroy(tchg_lock);
}

static void mt6370_chg_destroy_irq_access_lock(void *data)
{
	struct mutex *irq_access_lock = data;

	mutex_destroy(irq_access_lock);
}
static void mt6370_chg_destroy_pp_lock(void *data)
{
	struct mutex *pp_lock = data;

	mutex_destroy(pp_lock);
}

static void mt6370_chg_destroy_hidden_mode_lock(void *data)
{
	struct mutex *hidden_mode_lock = data;

	mutex_destroy(hidden_mode_lock);
}

static void mt6370_chg_destroy_pe_access_lock(void *data)
{
	struct mutex *pe_access_lock = data;

	mutex_destroy(pe_access_lock);
}

static void mt6370_chg_destroy_aicr_access_lock(void *data)
{
	struct mutex *aicr_access_lock = data;

	mutex_destroy(aicr_access_lock);
}

static void mt6370_chg_destroy_ichg_access_lock(void *data)
{
	struct mutex *ichg_access_lock = data;

	mutex_destroy(ichg_access_lock);
}


static void mt6370_chg_destroy_wq(void *data)
{
	struct workqueue_struct *wq = data;

	flush_workqueue(wq);
	destroy_workqueue(wq);
}

static void mt6370_chg_cancel_mivr_dwork(void *data)
{
	struct delayed_work *mivr_dwork = data;

	cancel_delayed_work_sync(mivr_dwork);
}

static void mt6370_chg_dev_unregister(void *data)
{
	struct charger_device *chgdev = data;

	charger_device_unregister(chgdev);
}

static irqreturn_t mt6370_attach_i_handler(int irq, void *data)
{
	struct mt6370_priv *priv = data;
	unsigned int otg_en;
	int ret, attach;

	mt_dbg(priv->dev, "%s\n", __func__);


	/* Check in OTG mode or not */
	ret = mt6370_chg_field_get(priv, F_BOOST_STAT, &otg_en);
	if (ret) {
		dev_err(priv->dev, "Failed to get OTG state\n");
		return IRQ_NONE;
	}

	if (otg_en)
		return IRQ_HANDLED;

	mutex_lock(&priv->attach_lock);
	priv->bc12_dn = true;
	attach = atomic_read(&priv->attach);
	mutex_unlock(&priv->attach_lock);

	if (attach < ATTACH_TYPE_PD && !queue_work(priv->wq, &priv->bc12_work))
		dev_err(priv->dev, "bc12 work has already queued\n");

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_uvp_d_evt_handler(int irq, void *data)
{

	return IRQ_HANDLED;
}

static int mt6370_enable_wdt(struct mt6370_priv *priv, bool en)
{
	int ret = 0;

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL13, MT6370_MASK_WDT_EN);

	return ret;
}

static inline int mt6370_ichg_workaround(struct mt6370_priv *priv, u32 uA)
{
	int ret = 0;

	/* Vsys short protection */
	mt6370_enable_hidden_mode(priv, true);

	if (priv->ichg >= 900000 && uA < 900000)
		ret = regmap_update_bits(priv->regmap,
					 MT6370_REG_CHGHIDDENCTRL7, 0x60, 0x00);
	else if (uA >= 900000 && priv->ichg < 900000)
		ret = regmap_update_bits(priv->regmap,
					 MT6370_REG_CHGHIDDENCTRL7, 0x60, 0x40);

	mt6370_enable_hidden_mode(priv, false);
	return ret;
}

static int __mt6370_get_ichg(struct mt6370_priv *priv, u32 *ichg)
{
	int ret = 0;
	union power_supply_propval val;

	ret = power_supply_get_property(priv->psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
					&val);
	if (ret < 0)
		return ret;

	*ichg = val.intval;
	return ret;
}

static int mt6370_get_ieoc(struct mt6370_priv *priv, u32 *ieoc)
{
	int ret = 0;
	union power_supply_propval val;


	ret = power_supply_get_property(priv->psy,
					POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
					&val);
	if (ret < 0)
		return ret;

	*ieoc = val.intval;

	return ret;

}

static int __mt6370_set_ieoc(struct mt6370_priv *priv, u32 ieoc)
{
	int ret = 0;
	union power_supply_propval val;

	/* IEOC workaround */
	if (priv->ieoc_wkard)
		ieoc += 100000; /* 100mA */

	val.intval = ieoc;
	ret = power_supply_set_property(priv->psy,
					POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
					&val);

	if (ret < 0)
		dev_err(priv->dev, "%s: set ieoc fail\n", __func__);

	/* Store IEOC */
	ret = mt6370_get_ieoc(priv, &priv->ieoc);

	return ret;
}

static int __mt6370_set_ichg(struct mt6370_priv *priv, u32 uA)
{
	int ret = 0;
	u8 chip_vid = priv->chip_vid;
	union power_supply_propval val;

	uA = (uA < 500000) ? 500000 : uA;

	if (chip_vid == RT5081_VENDOR_ID || chip_vid == MT6370_VENDOR_ID) {
		ret = mt6370_ichg_workaround(priv, uA);
		if (ret < 0)
			dev_info(priv->dev, "%s: workaround fail\n", __func__);
	}

	val.intval = uA;
	ret = power_supply_set_property(priv->psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
					&val);
	if (ret < 0)
		return ret;

	/* Store Ichg setting */
	__mt6370_get_ichg(priv, &priv->ichg);

	if (chip_vid != RT5081_VENDOR_ID && chip_vid != MT6370_VENDOR_ID)
		goto bypass_ieoc_workaround;
	/* Workaround to make IEOC accurate */
	if (uA < 900000 && !priv->ieoc_wkard) { /* 900mA */
		ret = __mt6370_set_ieoc(priv, priv->ieoc + 100000);
		priv->ieoc_wkard = true;
	} else if (uA >= 900000 && priv->ieoc_wkard) {
		priv->ieoc_wkard = false;
		ret = __mt6370_set_ieoc(priv, priv->ieoc - 100000);
	}

bypass_ieoc_workaround:
	return ret;
}

static int mt6370_get_ichg(struct charger_device *chgdev, u32 *ichg)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);

	return __mt6370_get_ichg(priv, ichg);
}

static int mt6370_set_ichg(struct charger_device *chgdev, u32 uA)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	mutex_lock(&priv->ichg_access_lock);
	ret = __mt6370_set_ichg(priv, uA);
	mutex_unlock(&priv->ichg_access_lock);

	return ret;
}

static int __mt6370_get_mivr(struct mt6370_priv *priv, u32 *mivr)
{
	int ret = 0;
	union power_supply_propval val;

	ret = power_supply_get_property(priv->psy,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
					&val);
	if (ret < 0)
		return ret;

	*mivr = val.intval;

	return ret;
}

static int mt6370_get_mivr(struct charger_device *chgdev, u32 *mivr)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = __mt6370_get_mivr(priv, mivr);

	return ret;
}

static int __mt6370_set_mivr(struct mt6370_priv *priv, u32 uV)
{
	int ret = 0;
	union power_supply_propval val;

	val.intval = uV;
	ret = power_supply_set_property(priv->psy,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
					&val);
	return ret;
}
static int mt6370_set_mivr(struct charger_device *chgdev, u32 uV)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	mutex_lock(&priv->pp_lock);

	if (!priv->pp_en) {
		dev_err(priv->dev, "%s: power path is disabled\n", __func__);
		goto out;
	}

	ret = __mt6370_set_mivr(priv, uV);
out:
	if (ret >= 0)
		priv->mivr = uV;
	mutex_unlock(&priv->pp_lock);
	return ret;
}

static int mt6370_set_ieoc(struct charger_device *chgdev, u32 uA)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	mutex_lock(&priv->ichg_access_lock);
	ret = __mt6370_set_ieoc(priv, uA);
	mutex_unlock(&priv->ichg_access_lock);

	return ret;
}

static int mt6370_get_aicr(struct charger_device *chgdev, u32 *aicr)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);
	union power_supply_propval val;

	ret = power_supply_get_property(priv->psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);
	if (ret < 0)
		return ret;

	*aicr = val.intval;

	return ret;
}

static int __mt6370_set_aicr(struct mt6370_priv *priv, u32 uA)
{
	int ret = 0;
	union power_supply_propval val;

	val.intval = uA;
	ret = power_supply_set_property(priv->psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);

	return ret;
}

static int mt6370_set_aicr(struct charger_device *chgdev, u32 uA)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	mutex_lock(&priv->aicr_access_lock);
	ret = __mt6370_set_aicr(priv, uA);
	mutex_unlock(&priv->aicr_access_lock);

	return ret;
}

static int mt6370_enable_charging(struct charger_device *chgdev, bool en)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);
	int ret = 0;
	u32 ichg_ramp_t = 0;

	mt_dbg(priv->dev, "%s: en = %d\n", __func__, en);

	/* Workaround for avoiding vsys overshoot when charge disable */
	mutex_lock(&priv->ichg_access_lock);
	if (!en) {
		if (priv->ichg <= 500000)
			goto out;
		priv->ichg_dis_chg = priv->ichg;
		ichg_ramp_t = (priv->ichg - 500000) / 50000 * 2;
		ret = mt6370_chg_field_set(priv, F_ICHG, 500000);
		if (ret < 0) {
			dev_notice(priv->dev,
				   "%s: set ichg fail\n", __func__);
			goto out;
		}
		mdelay(ichg_ramp_t);
	} else {
		if (priv->ichg == priv->ichg_dis_chg) {
			ret = __mt6370_set_ichg(priv, priv->ichg);
			if (ret < 0)
				dev_notice(priv->dev,
					   "%s: set ichg fail\n", __func__);
		}
	}
out:
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL2, MT6370_MASK_CHG_EN);
	if (ret < 0)
		dev_notice(priv->dev, "%s: fail, en = %d\n", __func__, en);
	mutex_unlock(&priv->ichg_access_lock);
	mt6370_power_supply_changed(priv);
	return ret;
}

static int mt6370_plug_in(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(priv->dev);

	dev_info(priv->dev, "%s\n", __func__);

	/* Enable WDT */
	if (pdata->en_wdt) {
		ret = mt6370_enable_wdt(priv, true);
		if (ret < 0)
			dev_err(priv->dev, "%s: en wdt failed\n", __func__);
	}

	/* Enable charger */
	ret = mt6370_enable_charging(chgdev, true);
	if (ret < 0) {
		dev_err(priv->dev, "%s: en chg failed\n", __func__);
		return ret;
	}

	return ret;
}

static int mt6370_plug_out(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	dev_info(priv->dev, "%s\n", __func__);

	/* Reset AICR limit */
	priv->aicr_limit = -1;

	/* Enable charger */
	ret = mt6370_enable_charging(chgdev, true);
	if (ret < 0) {
		dev_notice(priv->dev, "%s: en chg failed\n", __func__);
		return ret;
	}

	/* Disable WDT */
	ret = mt6370_enable_wdt(priv, false);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable wdt failed\n", __func__);


	return ret;
}

static int mt6370_get_charging_status(struct mt6370_priv *priv,
	enum mt6370_charging_status *chg_stat)
{
	int ret = 0;
	u32 val;

	ret = regmap_read(priv->regmap, MT6370_REG_CHG_STAT, &val);
	if (ret < 0)
		return ret;

	*chg_stat = (val & MT6370_MASK_CHG_STAT) >> MT6370_SHIFT_CHG_STAT;

	return ret;
}

static int mt6370_kick_wdt(struct charger_device *chgdev)
{
	/* Any I2C communication can kick watchdog timer */
	int ret = 0;
	enum mt6370_charging_status chg_status;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = mt6370_get_charging_status(priv, &chg_status);

	return ret;
}

static int mt6370_get_mivr_state(struct charger_device *chgdev, bool *in_loop)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);
	u32 val;

	ret = regmap_read(priv->regmap, MT6370_REG_CHG_STAT1, &val);
	if (ret < 0)
		return ret;
	*in_loop = (ret & MT6370_MASK_MIVR) >> MT6370_SHIFT_MIVR;
	return 0;
}

static int mt6370_is_charging_done(struct charger_device *chgdev, bool *done)
{
	int ret = 0;
	unsigned int chg_stat = MT6370_CHG_STATUS_READY;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = mt6370_get_charging_status(priv, &chg_stat);
	if (ret < 0)
		return ret;

	/* Return is charging done or not */
	switch (chg_stat) {
	case MT6370_CHG_STATUS_READY:
	case MT6370_CHG_STATUS_PROGRESS:
	case MT6370_CHG_STATUS_FAULT:
		*done = false;
		break;
	case MT6370_CHG_STATUS_DONE:
		*done = true;
		break;
	default:
		*done = false;
		break;
	}

	return 0;
}

static int mt6370_get_zcv(struct charger_device *chgdev, u32 *uV)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);

	dev_info(priv->dev, "%s: zcv = %dmV\n", __func__,
		 priv->zcv / 1000);
	*uV = priv->zcv;

	return 0;
}

static int mt6370_set_aicl_vth(struct mt6370_priv *priv, u32 aicl_vth)
{
	int ret = 0;
	u32 reg_aicl_vth;
	const struct linear_range *r = &mt6370_chg_ranges[MT6370_RANGE_F_AICL_VTH];

	mt6370_linear_range_get_selector_within(r, aicl_vth, &reg_aicl_vth);
	dev_info(priv->dev, "%s: vth = %d (0x%02X)\n", __func__, aicl_vth,
		reg_aicl_vth);
	ret = regmap_update_bits(
		priv->regmap,
		MT6370_REG_CHG_CTRL14,
		MT6370_MASK_AICL_VTH,
		reg_aicl_vth << MT6370_SHIFT_AICL_VTH
	);
	return ret;
}

static inline int mt6370_post_aicl_measure(struct charger_device *chgdev,
					   u32 start, u32 stop, u32 step,
					   u32 *measure)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);
	int cur, ret, val;

	mt_dbg(priv->dev,
	       "%s: post_aicc = (%d, %d, %d)\n", __func__, start, stop, step);
	for (cur = start; cur < stop; cur += step) {
		/* set_aicr to cur */
		ret = __mt6370_set_aicr(priv, cur + step);
		if (ret < 0)
			return ret;
		usleep_range(150, 200);
		ret = regmap_read(priv->regmap, MT6370_REG_CHG_STAT1, &val);
		if (ret < 0)
			return ret;
		/* read mivr stat */
		if (val & MT6370_MASK_CHG_MIVR)
			break;
	}
	if (cur > stop)
		cur = stop;
	*measure = cur;
	return 0;
}

static int __mt6370_run_aicl(struct mt6370_priv *priv)
{
	int ret = 0;
	u32 mivr = 0, aicl_vth = 0, aicr = 0;
	bool mivr_stat = false;
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(priv->dev);

	mt_dbg(priv->dev, "%s\n", __func__);

	ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_STAT1,
		MT6370_SHIFT_MIVR, &mivr_stat);
	if (ret < 0) {
		dev_err(priv->dev, "%s: read mivr stat failed\n", __func__);
		goto out;
	}

	if (!mivr_stat) {
		mt_dbg(priv->dev, "%s: mivr stat not act\n", __func__);
		goto out;
	}

	ret = __mt6370_get_mivr(priv, &mivr);
	if (ret < 0)
		goto out;

	/* Check if there's a suitable AICL_VTH */
	aicl_vth = mivr + 200000;
	if (aicl_vth > MT6370_AICL_VTH_MAX) {
		dev_info(priv->dev, "%s: no suitable VTH, vth = %d\n",
			 __func__, aicl_vth);
		ret = -EINVAL;
		goto out;
	}

	ret = mt6370_set_aicl_vth(priv, aicl_vth);
	if (ret < 0)
		goto out;

	/* Clear AICL measurement IRQ */
	mt6370_chg_irq_clr_flag(priv,
				&priv->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ5],
				MT6370_MASK_CHG_AICLMEASI);

	mutex_lock(&priv->pe_access_lock);
	mutex_lock(&priv->aicr_access_lock);

	ret = mt6370_set_bits(priv, MT6370_REG_CHG_CTRL14, MT6370_MASK_AICL_MEAS);
	if (ret < 0)
		goto unlock_out;

	ret = wait_event_interruptible_timeout(priv->wait_queue,
					       priv->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ5] &
					       MT6370_MASK_CHG_AICLMEASI,
					       msecs_to_jiffies(2500));
	if (ret <= 0) {
		dev_err(priv->dev, "%s: wait AICL time out, ret = %d\n",
			__func__, ret);
		ret = -EIO;
		goto unlock_out;
	}

	ret = mt6370_get_aicr(priv->chgdev, &aicr);
	if (ret < 0)
		goto unlock_out;

	if (pdata->post_aicl == false)
		goto skip_post_aicl;

	dev_info(priv->dev, "%s: aicc pre val = %d\n", __func__, aicr);
	/* always start/end aicc_val/aicc_val+200mA */
	ret = mt6370_post_aicl_measure(priv->chgdev, aicr,
				       aicr + 200000, 50000, &aicr);
	if (ret < 0)
		goto unlock_out;
	dev_info(priv->dev, "%s: aicc post val = %d\n", __func__, aicr);

skip_post_aicl:
	priv->aicr_limit = aicr;
	dev_info(priv->dev, "%s: OK, aicr upper bound = %dmA\n", __func__,
		 aicr / 1000);

unlock_out:
	mutex_unlock(&priv->aicr_access_lock);
	mutex_unlock(&priv->pe_access_lock);
out:
	return ret;
}

static int mt6370_run_aicl(struct charger_device *chgdev, u32 *uA)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = __mt6370_run_aicl(priv);
	if (ret >= 0)
		*uA = priv->aicr_limit;

	return ret;
}

static int __mt6370_enable_te(struct mt6370_priv *priv, bool en)
{
	int ret = 0;

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL2, MT6370_MASK_TE_EN);

	return ret;
}

static int mt6370_enable_te(struct charger_device *chgdev, bool en)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);

	return __mt6370_enable_te(priv, en);
}

static int mt6370_reset_eoc_state(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	dev_info(priv->dev, "%s\n", __func__);

	mt6370_enable_hidden_mode(priv, true);

	ret = mt6370_set_bits(priv, MT6370_REG_CHGHIDDENCTRL0, 0x80);
	if (ret < 0) {
		dev_err(priv->dev, "%s: set failed, ret = %d\n",
			__func__, ret);
		goto err;
	}

	udelay(100);
	ret = mt6370_clr_bits(priv, MT6370_REG_CHGHIDDENCTRL0, 0x80);
	if (ret < 0) {
		dev_err(priv->dev, "%s: clear failed, ret = %d\n",
			__func__, ret);
		goto err;
	}

err:
	mt6370_enable_hidden_mode(priv, false);

	return ret;
}

static int mt6370_get_adc(struct charger_device *chgdev,
		enum adc_channel chan, int *val)
{
	int ret;
	unsigned int adc_chan;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		adc_chan = MT6370_CHAN_VBUSDIV5;
		break;
	case ADC_CHANNEL_VSYS:
		adc_chan = MT6370_CHAN_VSYS;
		break;
	case ADC_CHANNEL_VBAT:
		adc_chan = MT6370_CHAN_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		adc_chan = MT6370_CHAN_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		adc_chan = MT6370_CHAN_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		adc_chan = MT6370_CHAN_TEMP_JC;
		break;
	default:
		return -EINVAL;
	}
	ret = mt6370_chg_read_adc_chan(priv, adc_chan, val);
	if (ret < 0) {
		dev_err(priv->dev, "failed to read adc\n");
		return ret;
	}
	return 0;
}

static int mt6370_safety_check(struct charger_device *chgdev, u32 polling_ieoc)
{
	int ret = 0;
	int adc_ibat = 0;
	static int counter;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = mt6370_get_adc(chgdev, ADC_CHANNEL_IBAT, &adc_ibat);
	if (ret < 0) {
		dev_info(priv->dev, "%s: get adc failed\n", __func__);
		return ret;
	}

	if (adc_ibat <= polling_ieoc)
		counter++;
	else
		counter = 0;

	/* If IBAT is less than polling_ieoc for 3 times, trigger EOC event */
	if (counter == 3) {
		dev_info(priv->dev, "%s: polling_ieoc = %d, ibat = %d\n",
			 __func__, polling_ieoc, adc_ibat);
		charger_dev_notify(priv->chgdev, CHARGER_DEV_NOTIFY_EOC);
		counter = 0;
	}

	return ret;
}

static int mt6370_get_min_ichg(struct charger_device *chgdev, u32 *uA)
{
	*uA = 500000;
	return 0;
}

static int mt6370_get_min_aicr(struct charger_device *chgdev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int __mt6370_enable_safety_timer(struct mt6370_priv *priv, bool en)
{
	int ret = 0;

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL12, MT6370_MASK_TMR_EN);

	return ret;
}

static int mt6370_enable_safety_timer(struct charger_device *chgdev, bool en)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = __mt6370_enable_safety_timer(priv, en);

	return ret;
}

static int mt6370_is_safety_timer_enable(struct charger_device *chgdev,
	bool *en)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_CTRL12,
				  MT6370_SHIFT_TMR_EN, en);

	return ret;
}


static int mt6370_enable_power_path(struct charger_device *chgdev, bool en)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	mutex_lock(&priv->pp_lock);

	dev_info(priv->dev, "%s: en = %d, pp_en = %d\n",
		 __func__, en, priv->pp_en);
	if (en == priv->pp_en)
		goto out;

	ret = (en ? mt6370_clr_bits : mt6370_set_bits)
		(priv, MT6370_REG_CHG_CTRL1, MT6370_MASK_FORCE_SLEEP);
	/*
	 * enable power path -> unmask mivr irq
	 * mask mivr irq -> disable power path
	 */

	if (!en)
		disable_irq_nosync(priv->irq_nums[MT6370_IRQ_MIVR]);
	ret = __mt6370_set_mivr(priv, en ? priv->mivr : MT6370_MIVR_MAX);
	if (en)
		enable_irq(priv->irq_nums[MT6370_IRQ_MIVR]);
	priv->pp_en = en;
out:
	mutex_unlock(&priv->pp_lock);
	return ret;
}

static int mt6370_is_power_path_enable(struct charger_device *chgdev, bool *en)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);

	mutex_lock(&priv->pp_lock);
	*en = priv->pp_en;
	mutex_unlock(&priv->pp_lock);

	return 0;
}

static int mt6370_enable_discharge(struct charger_device *chgdev, bool en)
{
	int ret = 0, i = 0;
	const u32 check_dischg_max = 3;
	bool is_dischg = true;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);

	ret = mt6370_enable_hidden_mode(priv, true);
	if (ret < 0)
		goto out;

	/* Set bit2 of reg[0x31] to 1/0 to enable/disable discharging */
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHGHIDDENCTRL1, 0x04);
	if (ret < 0) {
		dev_err(priv->dev, "%s: en = %d failed, ret = %d\n",
			__func__, en, ret);
		return ret;
	}

	if (!en) {
		for (i = 0; i < check_dischg_max; i++) {
			ret = mt6370_reg_test_bit(priv,
						  MT6370_REG_CHGHIDDENCTRL1, 2,
						  &is_dischg);
			if (!is_dischg)
				break;
			ret = mt6370_clr_bits(priv, MT6370_REG_CHGHIDDENCTRL1, 0x04);
		}
		if (i == check_dischg_max)
			dev_err(priv->dev,
				"%s: disable discharg failed, ret = %d\n",
				__func__, ret);
	}

out:
	mt6370_enable_hidden_mode(priv, false);
	return ret;
}

static int mt6370_enable_otg(struct charger_device *chgdev, bool en)
{
	int ret = 0;
	bool en_otg = false;
	struct mt6370_priv *priv = charger_get_data(chgdev);
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(priv->dev);
	u8 hidden_val = en ? 0x00 : 0x0F;
	u8 lg_slew_rate = en ? 0x7C : 0x73;
	u32 val;

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);

	mt6370_enable_hidden_mode(priv, true);

	/*
	 * Woraround :
	 * slow Low side mos Gate driver slew rate for decline VBUS noise
	 * reg[0x33] = 0x7C after entering OTG mode
	 * reg[0x33] = 0x73 after leaving OTG mode
	 */
	ret = regmap_write(priv->regmap, MT6370_REG_LG_CONTROL, lg_slew_rate);
	if (ret < 0) {
		dev_err(priv->dev,
			"%s: recover Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
		goto out;
	}

	ret = regmap_read(priv->regmap, MT6370_REG_LG_CONTROL, &val);
	if (ret < 0)
		dev_info(priv->dev, "%s: read reg0x33 failed\n", __func__);
	else
		dev_info(priv->dev, "%s: reg0x33 = 0x%02X\n", __func__, ret);

	/* Turn off USB charger detection/Enable WDT */
	if (en) {
		if (pdata->en_otg_wdt) {
			ret = mt6370_enable_wdt(priv, true);
			if (ret < 0)
				dev_err(priv->dev, "%s: en wdt fail\n",
					__func__);
		}
	}

	/* Switch OPA mode to boost mode */
	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL1, MT6370_MASK_OPA_MODE);

	msleep(20);

	if (en) {
		ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_CTRL1,
					  MT6370_SHIFT_OPA_MODE, &en_otg);
		if (ret < 0 || !en_otg) {
			dev_err(priv->dev, "%s: fail(%d)\n", __func__, ret);
			goto err_en_otg;
		}
	}

	/*
	 * Woraround reg[0x35] = 0x00 after entering OTG mode
	 * reg[0x35] = 0x0F after leaving OTG mode
	 */
	ret = regmap_write(priv->regmap,
		MT6370_REG_CHGHIDDENCTRL6, hidden_val);
	if (ret < 0)
		dev_err(priv->dev, "%s: workaroud failed, ret = %d\n",
			__func__, ret);

	/* Disable WDT */
	if (!en) {
		ret = mt6370_enable_wdt(priv, false);
		if (ret < 0)
			dev_err(priv->dev, "%s: disable wdt failed\n",
				__func__);
	}
	goto out;

err_en_otg:
	/* Disable OTG */
	mt6370_clr_bits(priv, MT6370_REG_CHG_CTRL1, MT6370_MASK_OPA_MODE);

	/* Disable WDT */
	ret = mt6370_enable_wdt(priv, false);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable wdt failed\n", __func__);

	/* Recover Low side mos Gate slew rate */
	ret = regmap_write(priv->regmap, MT6370_REG_LG_CONTROL, 0x73);
	if (ret < 0)
		dev_err(priv->dev,
			"%s: recover Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
	ret = -EIO;
out:
	mt6370_enable_hidden_mode(priv, false);
	return ret;
}


static int mt6370_set_otg_current_limit(struct charger_device *chgdev, u32 uA)
{
	int ret = 0;
	u8 reg_ilimit = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	/* Set higher OC threshold */
	for (reg_ilimit = 0;
	     reg_ilimit < ARRAY_SIZE(mt6370_otg_oc_threshold) - 1; reg_ilimit++)
		if (uA <= mt6370_otg_oc_threshold[reg_ilimit])
			break;

	dev_info(priv->dev, "%s: ilimit = %d (0x%02X)\n", __func__, uA,
		reg_ilimit);

	ret = regmap_update_bits(priv->regmap, MT6370_REG_CHG_CTRL10,
				 MT6370_MASK_BOOST_OC,
				 reg_ilimit << MT6370_SHIFT_BOOST_OC);

	return ret;
}

static int mt6370_enable_pump_express(struct mt6370_priv *priv, bool en)
{
	int ret = 0, i = 0;
	const int max_wait_times = 5;
	bool pumpx_en = false;

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);

	ret = mt6370_set_aicr(priv->chgdev, 800000);
	if (ret < 0)
		return ret;

	ret = mt6370_set_ichg(priv->chgdev, 2000000);
	if (ret < 0)
		return ret;

	ret = mt6370_enable_charging(priv->chgdev, true);
	if (ret < 0)
		return ret;

	mt6370_enable_hidden_mode(priv, true);

	ret = mt6370_clr_bits(priv, MT6370_REG_CHGHIDDENCTRL9, 0x80);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable psk mode fail\n", __func__);

	ret = (en ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL17, MT6370_MASK_PUMPX_EN);
	if (ret < 0)
		goto out;

	for (i = 0; i < max_wait_times; i++) {
		msleep(2500);
		ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_CTRL17,
					  MT6370_SHIFT_PUMPX_EN, &pumpx_en);
		if (!pumpx_en && ret >= 0)
			break;
	}
	if (i == max_wait_times) {
		dev_err(priv->dev, "%s: wait failed, ret = %d\n", __func__, ret);
		ret = -EIO;
		goto out;
	}
	ret = 0;
out:
	mt6370_set_bits(priv, MT6370_REG_CHGHIDDENCTRL9, 0x80);
	mt6370_enable_hidden_mode(priv, false);
	return ret;
}

static int mt6370_set_pep_current_pattern(struct charger_device *chgdev,
	bool is_increase)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	dev_info(priv->dev, "%s: pe1.0 pump_up = %d\n", __func__, is_increase);

	mutex_lock(&priv->pe_access_lock);

	/* Set to PE1.0 */
	ret = mt6370_clr_bits(priv, MT6370_REG_CHG_CTRL17, MT6370_MASK_PUMPX_20_10);

	/* Set Pump Up/Down */
	ret = (is_increase ? mt6370_set_bits : mt6370_clr_bits)
		(priv, MT6370_REG_CHG_CTRL17, MT6370_MASK_PUMPX_UP_DN);
	if (ret < 0)
		goto out;

	/* Enable PumpX */
	ret = mt6370_enable_pump_express(priv, true);

out:
	mutex_unlock(&priv->pe_access_lock);
	return ret;
}

static int mt6370_set_pep20_efficiency_table(struct charger_device *chgdev)
{
	return 0;
}

static int mt6370_set_pep20_current_pattern(struct charger_device *chgdev,
	u32 uV)
{
	int ret = 0;
	u32 reg_volt = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);
	const struct linear_range *r = &mt6370_chg_ranges[MT6370_RANGE_F_PUMPX];

	dev_info(priv->dev, "%s: pep2.0  = %d\n", __func__, uV);

	mutex_lock(&priv->pe_access_lock);
	/* Set to PEP2.0 */
	ret = mt6370_set_bits(priv, MT6370_REG_CHG_CTRL17, MT6370_MASK_PUMPX_20_10);
	if (ret < 0)
		goto out;

	mt6370_linear_range_get_selector_within(r, uV, &reg_volt);
	/* Set Voltage */
	ret = regmap_update_bits(priv->regmap, MT6370_REG_CHG_CTRL17,
				 MT6370_MASK_PUMPX_DEC,
				 reg_volt << MT6370_SHIFT_PUMPX_DEC);
	if (ret < 0)
		goto out;

	/* Enable PumpX */
	ret = mt6370_enable_pump_express(priv, true);
	ret = (ret >= 0) ? 0 : ret;

out:
	mutex_unlock(&priv->pe_access_lock);
	return ret;
}

static int mt6370_set_pep20_reset(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	mutex_lock(&priv->pe_access_lock);
	/* disable skip mode */
	mt6370_enable_hidden_mode(priv, true);

	ret = mt6370_clr_bits(priv, MT6370_REG_CHGHIDDENCTRL9, 0x80);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable psk mode fail\n", __func__);

	/* Select IINLMTSEL to use AICR */
	ret = mt6370_chg_field_set(priv, F_IINLMTSEL, 2);
	if (ret < 0)
		goto out;

	ret = mt6370_set_aicr(chgdev, 100000);
	if (ret < 0)
		goto out;

	msleep(250);

	ret = mt6370_set_aicr(chgdev, 700000);

out:
	mt6370_set_bits(priv, MT6370_REG_CHGHIDDENCTRL9, 0x80);
	mt6370_enable_hidden_mode(priv, false);
	mutex_unlock(&priv->pe_access_lock);
	return ret;
}

static int mt6370_enable_cable_drop_comp(struct charger_device *chgdev,
	bool en)
{
	int ret = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	dev_info(priv->dev, "%s: en = %d\n", __func__, en);

	mutex_lock(&priv->pe_access_lock);
	/* Set to PEP2.0 */
	ret = mt6370_set_bits(priv, MT6370_REG_CHG_CTRL17, MT6370_MASK_PUMPX_20_10);
	if (ret < 0)
		goto out;

	/* Set Voltage */
	ret = regmap_update_bits(priv->regmap, MT6370_REG_CHG_CTRL17,
				 MT6370_MASK_PUMPX_DEC,
				 0x1F << MT6370_SHIFT_PUMPX_DEC);
	if (ret < 0)
		goto out;

	/* Enable PumpX */
	ret = mt6370_enable_pump_express(priv, true);

out:
	mutex_unlock(&priv->pe_access_lock);
	return ret;
}

static int mt6370_get_tchg(struct charger_device *chgdev, int *tchg_min,
	int *tchg_max)
{
	int ret = 0, adc_temp = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);
	u32 retry_cnt = 3;

	/* Get value from ADC */
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_TEMP_JC, &adc_temp);
	if (ret < 0)
		return ret;

	/* Convert milli degree Celsius to degree Celsius */
	adc_temp /= 1000;

	/* Check unusual temperature */
	while (adc_temp >= 120 && retry_cnt > 0) {
		dev_err(priv->dev, "%s: [WARNING] t = %d\n", __func__, adc_temp);
		mt6370_get_adc(priv->chgdev, ADC_CHANNEL_VBAT, &adc_temp);
		ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_TEMP_JC, &adc_temp);
		adc_temp /= 1000;
		retry_cnt--;
	}
	if (ret < 0)
		return ret;

	mutex_lock(&priv->tchg_lock);
	if (adc_temp >= 120)
		adc_temp = priv->tchg;
	else
		priv->tchg = adc_temp;
	mutex_unlock(&priv->tchg_lock);

	*tchg_min = adc_temp;
	*tchg_max = adc_temp;

	dev_info(priv->dev, "%s: tchg = %d\n", __func__, adc_temp);

	return ret;
}

static int mt6370_get_ibus(struct charger_device *chgdev, u32 *ibus)
{
	int ret = 0, adc_ibus = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	/* Get value from ADC */
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_IBUS, &adc_ibus);
	if (ret < 0)
		return ret;

	*ibus = adc_ibus * 1000;

	dev_info(priv->dev, "%s: ibus = %dmA\n", __func__, adc_ibus);
	return ret;
}

static int mt6370_get_vbus(struct charger_device *chgdev, u32 *vbus)
{
	int ret = 0, adc_vbus = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	/* Get value from ADC */
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_VBUS, &adc_vbus);
	if (ret < 0)
		return ret;

	*vbus = adc_vbus * 1000;

	mt_dbg(priv->dev, "%s: vbus = %dmV\n", __func__, adc_vbus);
	return ret;
}

static int mt6370_get_ibat(struct charger_device *chgdev, u32 *ibat)
{
	int ret = 0, adc_ibat = 0;
	struct mt6370_priv *priv = charger_get_data(chgdev);

	/* Get value from ADC */
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_IBAT, &adc_ibat);
	if (ret < 0)
		return ret;

	*ibat = adc_ibat * 1000;

	dev_info(priv->dev, "%s: ibat = %dmA\n", __func__, adc_ibat);
	return ret;
}

static int mt6370_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);

	if (!priv->psy) {
		dev_notice(priv->dev, "%s: cannot get psy\n", __func__);
		return -ENODEV;
	}

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		mt6370_power_supply_changed(priv);
		break;
	default:
		break;
	}
	return 0;
}

static const unsigned int mt6370_chg_reg_addr[] = {
	MT6370_REG_CHG_CTRL1,
	MT6370_REG_CHG_CTRL2,
	MT6370_REG_CHG_CTRL3,
	MT6370_REG_CHG_CTRL4,
	MT6370_REG_CHG_CTRL5,
	MT6370_REG_CHG_CTRL6,
	MT6370_REG_CHG_CTRL7,
	MT6370_REG_CHG_CTRL8,
	MT6370_REG_CHG_CTRL9,
	MT6370_REG_CHG_CTRL10,
	MT6370_REG_CHG_CTRL11,
	MT6370_REG_CHG_CTRL12,
	MT6370_REG_CHG_CTRL13,
	MT6370_REG_CHG_CTRL14,
	MT6370_REG_CHG_CTRL15,
	MT6370_REG_CHG_CTRL16,
	MT6370_REG_CHGADC,
	MT6370_REG_DEVICE_TYPE,
	MT6370_REG_QCCTRL1,
	MT6370_REG_QCCTRL2,
	MT6370_REG_QC3P0CTRL1,
	MT6370_REG_QC3P0CTRL2,
	MT6370_REG_USB_STATUS1,
	MT6370_REG_QCSTATUS1,
	MT6370_REG_QCSTATUS2,
	MT6370_REG_CHGPUMP,
	MT6370_REG_CHG_CTRL17,
	MT6370_REG_CHG_CTRL18,
	MT6370_REG_CHGDIRCHG1,
	MT6370_REG_CHGDIRCHG2,
	MT6370_REG_CHGDIRCHG3,
	MT6370_REG_CHG_STAT,
	MT6370_REG_CHGNTC,
	MT6370_REG_ADCDATAH,
	MT6370_REG_ADCDATAL,
	MT6370_REG_CHG_CTRL19,
	MT6370_REG_CHG_STAT1,
	MT6370_REG_CHG_STAT2,
	MT6370_REG_CHG_STAT3,
	MT6370_REG_CHG_STAT4,
	MT6370_REG_CHG_STAT5,
	MT6370_REG_CHG_STAT6,
	MT6370_REG_QCSTAT,
	MT6370_REG_DICHGSTAT,
	MT6370_REG_OVPCTRL_STAT,
};

static int mt6370_is_charging_enable(struct mt6370_priv *priv, bool *en)
{
	int ret = 0;

	ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_CTRL2,
				  MT6370_SHIFT_CHG_EN, en);

	return ret;
}

/* Charging status name */
static const char *mt6370_chg_status_name[MT6370_CHG_STATUS_MAX] = {
	"ready", "progress", "done", "fault",
};
static int mt6370_dump_register(struct charger_device *chgdev)
{
	int i = 0, ret = 0;
	u32 ichg = 0, aicr = 0, mivr = 0, ieoc = 0, cv = 0;
	u32 val, chg_stat = 0;
	bool chg_en = 0;
	int adc_vsys = 0, adc_vbat = 0, adc_ibat = 0, adc_ibus = 0;
	int adc_vbus = 0;
	enum mt6370_charging_status chg_status = MT6370_CHG_STATUS_READY;
	u8  chg_ctrl[2] = {0};
	struct mt6370_priv *priv = charger_get_data(chgdev);

	ret = mt6370_get_ichg(chgdev, &ichg);
	ret = mt6370_get_aicr(chgdev, &aicr);
	ret = mt6370_get_charging_status(priv, &chg_status);
	ret = mt6370_get_ieoc(priv, &ieoc);
	ret = mt6370_get_mivr(chgdev, &mivr);
	ret = mt6370_get_cv(chgdev, &cv);
	ret = mt6370_is_charging_enable(priv, &chg_en);
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_VSYS, &adc_vsys);
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_VBAT, &adc_vbat);
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_IBAT, &adc_ibat);
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_IBUS, &adc_ibus);
	ret = mt6370_get_adc(priv->chgdev, ADC_CHANNEL_VBUS, &adc_vbus);

	ret = regmap_read(priv->regmap, MT6370_REG_CHG_STAT1, &chg_stat);
	ret = regmap_bulk_read(priv->regmap, MT6370_REG_CHG_CTRL1, &chg_ctrl, 2);

	if (chg_stat == MT6370_CHG_STATUS_FAULT) {
		for (i = 0; i < ARRAY_SIZE(mt6370_chg_reg_addr); i++) {
			ret = regmap_read(priv->regmap,
					  mt6370_chg_reg_addr[i], &val);
			if (ret < 0)
				return ret;

			mt_dbg(priv->dev, "%s: reg[0x%02X] = 0x%02X\n",
				__func__, mt6370_chg_reg_addr[i], val);
		}
	}

	dev_info(priv->dev,
		 "%s: ICHG = %dmA, AICR = %dmA, MIVR = %dmV, IEOC = %dmA, CV = %dmV\n",
		 __func__, ichg / 1000, aicr / 1000, mivr / 1000, ieoc / 1000, cv / 1000);

	dev_info(priv->dev,
		 "%s: VSYS = %dmV, VBAT = %dmV, IBAT = %dmA, IBUS = %dmA, VBUS = %dmV\n",
		 __func__, adc_vsys, adc_vbat, adc_ibat, adc_ibus, adc_vbus);

	dev_info(priv->dev, "%s: CHG_EN = %d, CHG_STATUS = %s, CHG_STAT = 0x%02X\n",
		 __func__, chg_en, mt6370_chg_status_name[chg_status], chg_stat);

	dev_info(priv->dev, "%s: CHG_CTRL1 = 0x%02X, CHG_CTRL2 = 0x%02X\n",
		 __func__, chg_ctrl[0], chg_ctrl[1]);

	ret = 0;
	return ret;
}

static int mt6370_enable_chg_type_det(struct charger_device *chgdev, bool en)
{
	struct mt6370_priv *priv = charger_get_data(chgdev);
	union power_supply_propval val;

	mt_dbg(priv->dev, "en=%d\n", en);

	val.intval =  en ? ATTACH_TYPE_TYPEC : ATTACH_TYPE_NONE;
	mt6370_chg_set_online(priv, &val);
	return 0;
}
static const struct charger_properties mt6370_chg_props = {
	.alias_name = "mt6370_chg",
};

static struct charger_ops mt6370_chg_ops = {
	/* Normal charging */
	.plug_out = mt6370_plug_out,
	.plug_in = mt6370_plug_in,
	.dump_registers = mt6370_dump_register,
	.enable = mt6370_enable_charging,
	.get_charging_current = mt6370_get_ichg,
	.set_charging_current = mt6370_set_ichg,
	.get_input_current = mt6370_get_aicr,
	.set_input_current = mt6370_set_aicr,
	.get_constant_voltage = mt6370_get_cv,
	.set_constant_voltage = mt6370_set_cv,
	.kick_wdt = mt6370_kick_wdt,
	.set_mivr = mt6370_set_mivr,
	.get_mivr = mt6370_get_mivr,
	.get_mivr_state = mt6370_get_mivr_state,
	.is_charging_done = mt6370_is_charging_done,
	.get_zcv = mt6370_get_zcv,
	.run_aicl = mt6370_run_aicl,
	.set_eoc_current = mt6370_set_ieoc,
	.enable_termination = mt6370_enable_te,
	.reset_eoc_state = mt6370_reset_eoc_state,
	.safety_check = mt6370_safety_check,
	.get_min_charging_current = mt6370_get_min_ichg,
	.get_min_input_current = mt6370_get_min_aicr,

	/* Safety timer */
	.enable_safety_timer = mt6370_enable_safety_timer,
	.is_safety_timer_enabled = mt6370_is_safety_timer_enable,

	/* Power path */
	.enable_powerpath = mt6370_enable_power_path,
	.is_powerpath_enabled = mt6370_is_power_path_enable,

	/* Charger type detection */
	.enable_chg_type_det = mt6370_enable_chg_type_det,

	/* OTG */
	.enable_otg = mt6370_enable_otg,
	.set_boost_current_limit = mt6370_set_otg_current_limit,
	.enable_discharge = mt6370_enable_discharge,

	/* PE+/PE+20 */
	.send_ta_current_pattern = mt6370_set_pep_current_pattern,
	.set_pe20_efficiency_table = mt6370_set_pep20_efficiency_table,
	.send_ta20_current_pattern = mt6370_set_pep20_current_pattern,
	.reset_ta = mt6370_set_pep20_reset,
	.enable_cable_drop_comp = mt6370_enable_cable_drop_comp,

	/* ADC */
	.get_tchg_adc = mt6370_get_tchg,
	.get_ibus_adc = mt6370_get_ibus,
	.get_vbus_adc = mt6370_get_vbus,
	.get_ibat_adc = mt6370_get_ibat,

	/* Event */
	.event = mt6370_do_event,
};
static irqreturn_t mt6370_mivr_handler(int irq, void *data)
{
	struct mt6370_priv *priv = data;

	pm_stay_awake(priv->dev);
	disable_irq_nosync(priv->irq_nums[MT6370_IRQ_MIVR]);
	schedule_delayed_work(&priv->mivr_dwork, msecs_to_jiffies(200));

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_treg_handler(int irq, void *data)
{
	int ret = 0;
	bool treg_stat = false;
	struct mt6370_priv *priv = data;

	dev_err(priv->dev, "%s\n", __func__);

	/* Read treg status */
	ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_STAT1,
				  MT6370_SHIFT_CHG_TREG, &treg_stat);
	if (ret < 0)
		dev_err(priv->dev, "%s: read treg stat failed\n", __func__);
	else
		dev_err(priv->dev, "%s: treg stat = %d\n", __func__, treg_stat);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_vbusov_handler(int irq, void *data)
{
	int ret = 0;
	bool vbusov_stat = false;
	struct mt6370_priv *priv = data;
	struct chgdev_notify *noti = &(priv->chgdev->noti);

	dev_err(priv->dev, "%s\n", __func__);
	ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_STAT2,
				  MT6370_SHIFT_CHG_VBUSOV_STAT, &vbusov_stat);
	if (ret < 0)
		return IRQ_HANDLED;

	noti->vbusov_stat = vbusov_stat;
	dev_info(priv->dev, "%s: stat = %d\n", __func__, vbusov_stat);

	charger_dev_notify(priv->chgdev, CHARGER_DEV_NOTIFY_VBUS_OVP);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_tmri_handler(int irq, void *data)
{
	int ret = 0;
	bool tmr_stat = false;
	struct mt6370_priv *priv = data;

	dev_err(priv->dev, "%s\n", __func__);
	ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_STAT4,
				  MT6370_SHIFT_CHG_TMRI_STAT, &tmr_stat);
	if (ret < 0)
		return IRQ_HANDLED;

	dev_info(priv->dev, "%s: stat = %d\n", __func__, tmr_stat);
	if (!tmr_stat)
		return IRQ_HANDLED;

	charger_dev_notify(priv->chgdev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_aiclmeasi_handler(int irq, void *data)
{
	struct mt6370_priv *priv = data;

	dev_info(priv->dev, "%s\n", __func__);
	mt6370_chg_irq_set_flag(priv, &priv->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ5],
				MT6370_MASK_CHG_AICLMEASI);

	wake_up_interruptible(&priv->wait_queue);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_wdtmri_handler(int irq, void *data)
{
	int ret = 0;
	struct mt6370_priv *priv = data;

	dev_notice(priv->dev, "%s\n", __func__);
	ret = mt6370_kick_wdt(priv->chgdev);
	if (ret < 0)
		dev_err(priv->dev, "%s: kick wdt failed\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_rechgi_handler(int irq, void *data)
{
	struct mt6370_priv *priv = data;

	dev_info(priv->dev, "%s\n", __func__);
	charger_dev_notify(priv->chgdev, CHARGER_DEV_NOTIFY_RECHG);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_ieoci_handler(int irq, void *data)
{
	int ret = 0;
	bool ieoc_stat = false;
	struct mt6370_priv *priv = data;

	dev_info(priv->dev, "%s\n", __func__);
	ret = mt6370_reg_test_bit(priv, MT6370_REG_CHG_STAT5,
				  MT6370_SHIFT_CHG_IEOCI_STAT, &ieoc_stat);
	if (ret < 0)
		return IRQ_HANDLED;

	dev_info(priv->dev, "%s: stat = %d\n", __func__, ieoc_stat);
	if (!ieoc_stat)
		return IRQ_HANDLED;

	charger_dev_notify(priv->chgdev, CHARGER_DEV_NOTIFY_EOC);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_dcdti_handler(int irq, void *data)
{
	struct mt6370_priv *priv = data;
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(priv->dev);
	int ret = 0;
	bool dcdt = false;

	dev_info(priv->dev, "%s\n", __func__);
	if (pdata->fast_unknown_ta_dect) {
		ret = mt6370_reg_test_bit(priv, MT6370_REG_USB_STATUS1,
					  MT6370_SHIFT_DCDT, &dcdt);
		if (ret < 0 || !dcdt)
			return IRQ_HANDLED;
		dev_info(priv->dev, "%s: unknown TA Detected\n", __func__);
		priv->dcd_timeout = true;
		queue_work(priv->wq, &priv->bc12_work);
	}
	return IRQ_HANDLED;
}

static int mt6370_chg_init_chgdev(struct mt6370_priv *priv)
{
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(priv->dev);

	mt_dbg(priv->dev, "%s\n", __func__);
	priv->chgdev = charger_device_register(pdata->chgdev_name, priv->dev,
					       priv, &mt6370_chg_ops,
					       &mt6370_chg_props);
	return IS_ERR(priv->chgdev) ? PTR_ERR(priv->chgdev) : 0;
}

#define MT6370_CHG_IRQ(_name)						\
{									\
	.name = #_name,							\
	.handler = mt6370_##_name##_handler,				\
}

static int mt6370_chg_init_irq(struct mt6370_priv *priv)
{
	int i, ret;
	const struct {
		char *name;
		irq_handler_t handler;
	} mt6370_chg_irqs[] = {
		MT6370_CHG_IRQ(attach_i),
		MT6370_CHG_IRQ(uvp_d_evt),
		MT6370_CHG_IRQ(mivr),
		MT6370_CHG_IRQ(treg),
		MT6370_CHG_IRQ(vbusov),
		MT6370_CHG_IRQ(tmri),
		MT6370_CHG_IRQ(aiclmeasi),
		MT6370_CHG_IRQ(wdtmri),
		MT6370_CHG_IRQ(rechgi),
		MT6370_CHG_IRQ(ieoci),
		MT6370_CHG_IRQ(dcdti),
	};

	for (i = 0; i < ARRAY_SIZE(mt6370_chg_irqs); i++) {
		ret = platform_get_irq_byname(to_platform_device(priv->dev),
					      mt6370_chg_irqs[i].name);
		if (ret < 0)
			return dev_err_probe(priv->dev, ret,
					     "Failed to get irq %s\n",
					     mt6370_chg_irqs[i].name);

		priv->irq_nums[i] = ret;
		ret = devm_request_threaded_irq(priv->dev, ret, NULL,
						mt6370_chg_irqs[i].handler,
						IRQF_TRIGGER_LOW,
						dev_name(priv->dev), priv);
		if (ret)
			return dev_err_probe(priv->dev, ret,
					     "Failed to request irq %s\n",
					     mt6370_chg_irqs[i].name);
	}

	return 0;
}

#ifdef MT6370_APPLE_SAMSUNG_TA_SUPPORT
static int mt6370_detect_apple_samsung_ta(struct mt6370_priv *priv)
{
	int ret = 0;
	bool dcd_timeout = false;
	bool dp_0_9v = false, dp_1_5v = false, dp_2_3v = false, dm_2_3v = false;

	/* Only SDP/CDP/DCP could possibly be A/SS TA */
	if (priv->psy_usb_type != POWER_SUPPLY_USB_TYPE_SDP &&
	    priv->psy_usb_type != POWER_SUPPLY_USB_TYPE_CDP &&
	    priv->psy_usb_type != POWER_SUPPLY_USB_TYPE_DCP)
		return -EINVAL;

	if (priv->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP ||
	    priv->psy_usb_type == POWER_SUPPLY_USB_TYPE_CDP) {
		ret = mt6370_reg_test_bit(priv, MT6370_REG_QCSTAT,
					  MT6370_SHIFT_DCDTI_STAT,
					  &dcd_timeout);
		if (ret < 0) {
			dev_err(priv->dev, "%s: read dcd timeout failed\n",
				__func__);
			return ret;
		}

		if (!dcd_timeout) {
			dev_info(priv->dev, "%s: dcd is not timeout\n",
				 __func__);
			return 0;
		}
	}

	/* Check DP > 0.9V */
	ret = regmap_update_bits(priv->regmap,
				 MT6370_REG_QCSTATUS2,
				 0x0F,
				 0x03);

	ret = mt6370_reg_test_bit(priv->regmap, MT6370_REG_QCSTATUS2,
				  4, &dp_0_9v);
	if (ret < 0)
		return ret;

	if (!dp_0_9v) {
		dev_info(priv->dev, "%s: DP < 0.9V\n", __func__);
		return ret;
	}

	ret = mt6370_reg_test_bit(priv->regmap, MT6370_REG_QCSTATUS2, 5, &dp_1_5v);
	if (ret < 0)
		return ret;

	/* SS charger */
	if (!dp_1_5v) {
		dev_info(priv->dev, "%s: 0.9V < DP < 1.5V\n", __func__);
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		return ret;
	}

	/* Check DP > 2.3 V */
	ret = mt6370_reg_update_bits(priv->regmap, MT6370_REG_QCSTATUS2,
				     0x0F, 0x0B);
	ret = mt6370_reg_test_bit(priv->regmap, MT6370_REG_QCSTATUS2, 5, &dp_2_3v);
	if (ret < 0)
		return ret;

	/* Check DM > 2.3V */
	ret = mt6370_reg_update_bits(priv->regmap, MT6370_REG_QCSTATUS2,
				     0x0F, 0x0F
	);
	ret = mt6370_reg_test_bit(priv->regmap, MT6370_REG_QCSTATUS2, 5, &dm_2_3v);
	if (ret < 0)
		return ret;

	/* Apple charger */
	if (!dp_2_3v && !dm_2_3v) {
		dev_info(priv->dev, "%s: 1.5V < DP < 2.3V && DM < 2.3V\n", __func__);
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
	} else if (!dp_2_3v && dm_2_3v) {
		dev_info(priv->dev, "%s: 1.5V < DP < 2.3V && 2.3V < DM\n", __func__);
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
	} else if (dp_2_3v && !dm_2_3v) {
		dev_info(priv->dev, "%s: 2.3V < DP && DM < 2.3V\n", __func__);
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
	} else {
		dev_info(priv->dev, "%s: 2.3V < DP && 2.3V < DM\n", __func__);
		priv->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
	}

	return 0;
}
#endif

static int mt6370_chg_sw_workaround(struct mt6370_priv *priv)
{
	int ret = 0;
	u8 zcv_data[2] = {0};

	dev_info(priv->dev, "%s\n", __func__);

	mt6370_enable_hidden_mode(priv, true);

	/* Read ZCV data */
	ret = regmap_bulk_read(priv->regmap, MT6370_REG_ADCBATDATAH, &zcv_data, 2);
	if (ret < 0)
		dev_err(priv->dev, "%s: read zcv data failed\n", __func__);
	else {
		priv->zcv = 5000 * (zcv_data[0] * 256 + zcv_data[1]);

		dev_info(priv->dev, "%s: zcv = (0x%02X, 0x%02X, %dmV)\n",
			 __func__, zcv_data[0], zcv_data[1],
			 priv->zcv / 1000);
	}

	/* Trigger any ADC before disabling ZCV */
	ret = regmap_write(priv->regmap, MT6370_REG_CHGADC, 0x11);
	if (ret < 0)
		dev_err(priv->dev, "%s: trigger ADC failed\n", __func__);

	/* Disable ZCV */
	ret = mt6370_set_bits(priv, MT6370_REG_OSCCTRL, 0x04);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable ZCV failed\n", __func__);

	/* Disable TS auto sensing */
	ret = mt6370_clr_bits(priv, MT6370_REG_CHGHIDDENCTRL15, 0x01);

	/* Disable SEN_DCP for charging mode */
	ret = mt6370_clr_bits(priv, MT6370_REG_QCCTRL2, MT6370_MASK_EN_DCP);

	mt6370_enable_hidden_mode(priv, false);

	return ret;
}

static int mt6370_chg_get_pdata(struct device *dev)
{

	struct device_node *boot_np, *np = dev->of_node;
	struct mt6370_priv *priv = dev_get_drvdata(dev);
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(dev);
	const struct {
		u32 size;
		u32 tag;
		u32 boot_mode;
		u32 boot_type;
	} *tag;

	mt_dbg(dev, "%s\n", __func__);
	if (np) {
		pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &mt6370_default_chg_desc, sizeof(*pdata));
		/* Alias name is in charger properties but not in desc */
		if (of_property_read_string(np, "chg_alias_name",
			&(priv->chg_props.alias_name)) < 0) {
			dev_err(dev, "%s: no chg alias name\n", __func__);
			priv->chg_props.alias_name = "mt6370_chg";
		}

		if (of_property_read_string(np, "ls_alias_name",
			&(priv->ls_props.alias_name)) < 0) {
			dev_err(dev, "%s: no ls alias name\n", __func__);
			priv->ls_props.alias_name = "mt6370_ls";
		}

		if (of_property_read_string(np, "charger_name",
			&(pdata->chgdev_name)) < 0)
			dev_err(dev, "%s: no charger name\n", __func__);

		if (of_property_read_string(np, "load_switch_name",
			&(pdata->ls_dev_name)) < 0)
			dev_err(dev, "%s: no load switch name\n", __func__);

		if (of_property_read_u32(np, "ichg", &pdata->ichg) < 0)
			dev_err(dev, "%s: no ichg\n", __func__);

		if (of_property_read_u32(np, "aicr", &pdata->aicr) < 0)
			dev_err(dev, "%s: no aicr\n", __func__);

		if (of_property_read_u32(np, "mivr", &pdata->mivr) < 0)
			dev_err(dev, "%s: no mivr\n", __func__);

		if (of_property_read_u32(np, "cv", &pdata->cv) < 0)
			dev_err(dev, "%s: no cv\n", __func__);

		if (of_property_read_u32(np, "ieoc", &pdata->ieoc) < 0)
			dev_err(dev, "%s: no ieoc\n", __func__);

		if (of_property_read_u32(np, "safety_timer",
			&pdata->safety_timer) < 0)
			dev_err(dev, "%s: no safety timer\n", __func__);

		if (of_property_read_u32(np, "dc_wdt", &pdata->dc_wdt) < 0)
			dev_err(dev, "%s: no dc wdt\n", __func__);

		if (of_property_read_u32(np, "ircmp_resistor",
			&pdata->ircmp_resistor) < 0)
			dev_err(dev, "%s: no ircmp resistor\n", __func__);

		if (of_property_read_u32(np, "ircmp_vclamp",
			&pdata->ircmp_vclamp) < 0)
			dev_err(dev, "%s: no ircmp vclamp\n", __func__);

		if (of_property_read_u32(np, "lbp_hys_sel", &pdata->lbp_hys_sel) < 0)
			dev_err(dev, "%s: no lbp_hys_sel\n", __func__);

		if (of_property_read_u32(np, "lbp_dt", &pdata->lbp_dt) < 0)
			dev_err(dev, "%s: no lbp_dt\n", __func__);

		/* mediatek boot mode */
		boot_np = of_parse_phandle(np, "boot_mode", 0);
		if (!boot_np) {
			dev_err(dev, "failed to get bootmode phandle\n");
			return -ENODEV;
		}
		tag = of_get_property(boot_np, "atag,boot", NULL);
		if (!tag) {
			dev_err(dev, "failed to get atag,boot\n");
			return -EINVAL;
		}
		dev_info(dev, "sz:0x%x tag:0x%x mode:0x%x type:0x%x\n",
			 tag->size, tag->tag, tag->boot_mode, tag->boot_type);
		pdata->boot_mode = tag->boot_mode;
		pdata->boot_type = tag->boot_type;
		pdata->en_te = of_property_read_bool(np, "enable_te");
		pdata->en_wdt = of_property_read_bool(np, "enable_wdt");
		pdata->en_otg_wdt = of_property_read_bool(np, "enable_otg_wdt");
		pdata->en_polling = of_property_read_bool(np, "enable_polling");
		pdata->disable_vlgc = of_property_read_bool(np, "disable_vlgc");
		pdata->fast_unknown_ta_dect =
			of_property_read_bool(np, "fast_unknown_ta_dect");
		pdata->post_aicl = of_property_read_bool(np, "post_aicl");

		dev->platform_data = pdata;
	}
	return pdata ? 0 : -ENODEV;
}

static int mt6370_init_mutex(struct device *dev, struct mt6370_priv *priv)
{
	int ret;

	mutex_init(&priv->attach_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_attach_lock,
				       &priv->attach_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init attach lock\n");

	mutex_init(&priv->ichg_access_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_ichg_access_lock,
				       &priv->ichg_access_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init ichg_access lock\n");

	mutex_init(&priv->aicr_access_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_aicr_access_lock,
				       &priv->aicr_access_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init aicr_access lock\n");

	mutex_init(&priv->pe_access_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_pe_access_lock,
				       &priv->pe_access_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init pe_access lock\n");

	mutex_init(&priv->hidden_mode_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_hidden_mode_lock,
				       &priv->hidden_mode_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init hidden_mode lock\n");

	mutex_init(&priv->pp_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_pp_lock,
				       &priv->pp_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init pp lock\n");

	mutex_init(&priv->tchg_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_tchg_lock,
				       &priv->tchg_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init tchg lock\n");

	mutex_init(&priv->irq_access_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_irq_access_lock,
				       &priv->irq_access_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init irq_access lock\n");


	return 0;
}

static ssize_t shipping_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct mt6370_priv *priv = dev_get_drvdata(dev);
	int32_t tmp = 0;
	int ret = 0;

	if (kstrtoint(buf, 10, &tmp) < 0) {
		dev_notice(dev, "parsing number fail\n");
		return -EINVAL;
	}
	if (tmp != 5526789)
		return -EINVAL;
	ret = regmap_write(priv->regmap, MT6370_REG_RSTPASCODE1, 0xA9);
	if (ret < 0) {
		dev_notice(dev, "set passcode1 fail\n");
		return ret;
	}
	ret = regmap_write(priv->regmap, MT6370_REG_RSTPASCODE2, 0x96);
	if (ret < 0) {
		dev_notice(dev, "set passcode2 fail\n");
		return ret;
	}
	/* reset all chg/fled/ldo/rgb/bl/db reg and logic */
	ret = regmap_write(priv->regmap, MT6370_REG_CORECTRL2, 0x7F);
	if (ret < 0) {
		dev_notice(dev, "set reset bits fail\n");
		return ret;
	}
	/* disable chg auto sensing */
	mt6370_enable_hidden_mode(priv, true);
	ret = mt6370_clr_bits(priv, MT6370_REG_CHGHIDDENCTRL15, 0x01);
	if (ret < 0) {
		dev_notice(dev, "set auto sensing disable\n");
		return ret;
	}
	mt6370_enable_hidden_mode(priv, false);
	mdelay(50);
	/* enter shipping mode */
	ret = mt6370_set_bits(priv, MT6370_REG_CHG_CTRL2, 0x80);
	if (ret < 0) {
		dev_notice(dev, "enter shipping mode\n");
		return ret;
	}
	return count;
}

static const DEVICE_ATTR_WO(shipping_mode);
static int mt6370_chg_init_setting(struct mt6370_priv *priv)
{
	int ret;
	struct mt6370_chg_platform_data *pdata = dev_get_platdata(priv->dev);

	dev_info(priv->dev, "%s\n", __func__);
	/* Disable usb_chg_en */
	ret = mt6370_chg_field_set(priv, F_USBCHGEN, 0);
	if (ret) {
		dev_err(priv->dev, "Failed to disable usb_chg_en\n");
		return ret;
	}

	/* Change input current limit selection to using IAICR results */
	ret = mt6370_chg_field_set(priv, F_IINLMTSEL, 2);
	if (ret) {
		dev_err(priv->dev, "Failed to set IINLMTSEL\n");
		return ret;
	}

	ret = mt6370_chg_field_set(priv, F_ILIM_EN, 0);
	if (ret) {
		dev_err(priv->dev, "Failed to set ILIM\n");
		return ret;
	}
	ret = regmap_read(priv->regmap, MT6370_REG_DEV_INFO, &priv->chip_vid);
	if (ret) {
		dev_err(priv->dev, "Failed to get device ID\n");
		return ret;
	}

	ret = __mt6370_set_ichg(priv, pdata->ichg);
	if (ret < 0)
		dev_err(priv->dev, "%s: set ichg failed\n", __func__);

	if (mt6370_is_meta_mode(priv)) {
		ret = __mt6370_set_aicr(priv, 200000);
		dev_info(priv->dev, "%s: set aicr to 200mA in meta mode\n",
			__func__);
	} else {
		ret = __mt6370_set_aicr(priv, pdata->aicr);
	}
	if (ret < 0)
		dev_err(priv->dev, "%s: set aicr failed\n", __func__);

	ret = __mt6370_set_mivr(priv, pdata->mivr);
	if (ret < 0)
		dev_err(priv->dev, "%s: set mivr failed\n", __func__);
	priv->mivr = pdata->mivr;

	ret = __mt6370_set_cv(priv, pdata->cv);
	if (ret < 0)
		dev_err(priv->dev, "%s: set voreg failed\n", __func__);

	ret = __mt6370_set_ieoc(priv, pdata->ieoc);
	if (ret < 0)
		dev_err(priv->dev, "%s: set ieoc failed\n", __func__);

	ret = __mt6370_enable_te(priv, pdata->en_te);
	if (ret < 0)
		dev_err(priv->dev, "%s: set te failed\n", __func__);

	ret = mt6370_set_fast_charge_timer(priv, pdata->safety_timer);
	if (ret < 0)
		dev_err(priv->dev, "%s: set fast timer failed\n", __func__);

	ret = mt6370_set_dc_wdt(priv, pdata->dc_wdt);
	if (ret < 0)
		dev_err(priv->dev, "%s: set dc watch dog timer failed\n",
			__func__);

	ret = __mt6370_enable_safety_timer(priv, true);
	if (ret < 0)
		dev_err(priv->dev, "%s: enable charger timer failed\n",
			__func__);

	/* Initially disable WDT to prevent 1mA power consumption */
	ret = mt6370_enable_wdt(priv, false);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable watchdog timer failed\n",
			__func__);

	/* Disable JEITA */
	ret = mt6370_enable_jeita(priv, false);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable jeita failed\n", __func__);

	/* Disable HZ */
	ret = mt6370_enable_hz(priv, false);
	if (ret < 0)
		dev_err(priv->dev, "%s: disable hz failed\n", __func__);

	ret = mt6370_set_ircmp_resistor(priv, pdata->ircmp_resistor);
	if (ret < 0)
		dev_err(priv->dev,
			"%s: set IR compensation resistor failed\n", __func__);

	ret = mt6370_set_ircmp_vclamp(priv, pdata->ircmp_vclamp);
	if (ret < 0)
		dev_err(priv->dev,
			"%s: set IR compensation vclamp failed\n", __func__);

	ret = mt6370_set_otglbp(
		priv, pdata->lbp_hys_sel, pdata->lbp_dt);
	if (ret < 0)
		dev_err(priv->dev, "%s: set otg lbp fail\n", __func__);

	ret = mt6370_disable_vlgc(priv, pdata->disable_vlgc);
	if (ret < 0)
		dev_err(priv->dev, "%s: set vlgc fail\n", __func__);

	ret = mt6370_enable_fast_unknown_ta_dect(priv, pdata->fast_unknown_ta_dect);
	if (ret < 0) {
		dev_err(priv->dev,
			"%s: set fast unknown ta dect fail\n", __func__);
	}
	return 0;
}
static int mt6370_chg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt6370_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;

	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap)
		return dev_err_probe(dev, -ENODEV, "Failed to get regmap\n");

	ret = mt6370_chg_init_rmap_fields(priv);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init regmap fields\n");

	platform_set_drvdata(pdev, priv);

	priv->iio_adcs = devm_iio_channel_get_all(priv->dev);
	if (IS_ERR(priv->iio_adcs))
		return dev_err_probe(dev, PTR_ERR(priv->iio_adcs),
				     "Failed to get iio adc\n");

	ret = mt6370_chg_init_otg_regulator(priv);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init OTG regulator\n");

	ret = mt6370_chg_init_psy(priv);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init psy\n");

	ret = mt6370_chg_get_pdata(dev);
	if (ret < 0) {
		dev_err(dev, "failed to get platform data\n");
		return ret;
	}
	ret = mt6370_init_mutex(dev, priv);
	if (ret < 0) {
		dev_err(dev, "failed to init mutex\n");
		return ret;
	}

	/* parsing priv data */
	priv->dev = &pdev->dev;
	priv->aicr_limit = -1;
	priv->hidden_mode_cnt = 0;
	priv->ieoc_wkard = false;
	priv->ieoc = 250000; /* register default value 250mA */
	priv->ichg = 2000000;
	priv->ichg_dis_chg = 2000000;
	priv->pp_en = true;
	atomic_set(&priv->bc12_cnt, 0);
	atomic_set(&priv->attach, ATTACH_TYPE_NONE);
	priv->old_propval =
		devm_kcalloc(priv->dev, mt6370_chg_psy_desc.num_properties,
			     sizeof(*priv->old_propval), GFP_KERNEL);
	if (!priv->old_propval)
		return -ENOMEM;

	init_waitqueue_head(&priv->wait_queue);
	priv->wq = create_singlethread_workqueue(dev_name(priv->dev));
	if (IS_ERR(priv->wq))
		return dev_err_probe(dev, PTR_ERR(priv->wq),
				     "Failed to create workqueue\n");

	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_wq, priv->wq);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init wq\n");

	INIT_WORK(&priv->bc12_work, mt6370_chg_bc12_work_func);
	INIT_DELAYED_WORK(&priv->mivr_dwork, mt6370_chg_mivr_dwork_func);
	ret = devm_add_action_or_reset(dev, mt6370_chg_cancel_mivr_dwork,
				       &priv->mivr_dwork);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init mivr dwork\n");

	ret = mt6370_chg_init_setting(priv);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to init mt6370 charger setting\n");

	ret = mt6370_chg_sw_workaround(priv);
	if (ret < 0)
		return dev_err_probe(dev, ret, "%s: software workaround failed\n");

	ret = mt6370_chg_init_chgdev(priv);
	if (ret < 0) {
		dev_err(dev, "failed to init chgdev\n");
		return ret;
	}
	ret = devm_add_action_or_reset(dev, mt6370_chg_dev_unregister,
				       &priv->chgdev);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init chgdev\n");

	ret = mt6370_chg_init_irq(priv);
	if (ret)
		return ret;

	ret = device_create_file(priv->dev, &dev_attr_shipping_mode);
	if (ret < 0) {
		dev_notice(&pdev->dev, "create shipping attr fail\n");
		return ret;
	}
	dev_info(dev, "%s probe successfully\n", __func__);
	return 0;
}

static int mt6375_chg_remove(struct platform_device *pdev)
{
	struct mt6375_priv *priv = platform_get_drvdata(pdev);

	mt_dbg(&pdev->dev, "%s\n", __func__);
	if (priv)
		device_remove_file(&pdev->dev, &dev_attr_shipping_mode);

	return 0;
}
static const struct of_device_id mt6370_chg_of_match[] = {
	{ .compatible = "mediatek,mt6370-charger", },
	{}
};
MODULE_DEVICE_TABLE(of, mt6370_chg_of_match);

static struct platform_driver mt6370_chg_driver = {
	.probe = mt6370_chg_probe,
	.driver = {
		.name = "mt6370-charger",
		.of_match_table = mt6370_chg_of_match,
	},
	.remove = mt6375_chg_remove,
};
module_platform_driver(mt6370_chg_driver);

MODULE_AUTHOR("ChiaEn Wu <chiaen_wu@richtek.com>");
MODULE_DESCRIPTION("MediaTek MT6370 Charger Driver");
MODULE_LICENSE("GPL v2");
