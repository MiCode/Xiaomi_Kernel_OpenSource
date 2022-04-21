// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ShuFan Lee <shufan_lee@richtek.com>
 */

#include <linux/completion.h>
#include <linux/iio/consumer.h>
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/workqueue.h>

#include "charger_class.h"
#include "mtk_charger.h"

static bool dbg_log_en;
module_param(dbg_log_en, bool, 0644);
#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_info(dev, "%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

#define M_TO_U(val)	((val) * 1000)
#define U_TO_M(val)	((val) / 1000)

#define MT6375_MANUFACTURER	"Mediatek"

#define MT6375_REG_CORE_CTRL2	0x106
#define MT6375_REG_TM_PAS_CODE1	0x107
#define MT6375_REG_CHG_BATPRO	0x117
#define MT6375_REG_CHG_TOP1	0x120
#define MT6375_REG_CHG_TOP2	0x121
#define MT6375_REG_CHG_AICR	0x122
#define MT6375_REG_CHG_MIVR	0x123
#define MT6375_REG_CHG_VCHG	0x125
#define MT6375_REG_CHG_ICHG	0x126
#define MT6375_REG_CHG_TMR	0x127
#define MT6375_REG_CHG_EOC	0x128
#define MT6375_REG_CHG_VSYS	0x129
#define MT6375_REG_CHG_WDT	0x12A
#define MT6375_REG_CHG_PUMPX	0x12B
#define MT6375_REG_CHG_AICC1	0x12C
#define MT6375_REG_CHG_AICC2	0x12D
#define MT6375_REG_OTG_LBP	0x130
#define MT6375_REG_OTG_V	0x131
#define MT6375_REG_OTG_C	0x132
#define MT6375_REG_BAT_COMP	0x133
#define MT6375_REG_CHG_STAT	0x134
#define MT6375_REG_CHG_DUMY0	0x135
#define MT6375_REG_CHG_HD_TOP1	0x13B
#define MT6375_REG_CHG_HD_BUCK5	0x140
#define MT6375_REG_BC12_FUNC	0x150
#define MT6375_REG_BC12_STAT	0x151
#define MT6375_REG_DPDM_CTRL1	0x153
#define MT6375_REG_DPDM_CTRL2	0x154
#define MT6375_REG_DPDM_CTRL4	0x156
#define MT6375_REG_VBAT_MON_RPT	0x19C
#define MT6375_REG_BATEND_CODE	0x19E
#define MT6375_REG_ADC_CONFG1	0x1A4
#define MT6375_REG_ADC_ZCV_RPT	0x1CA
#define MT6375_REG_CHG_STAT0	0x1E0
#define MT6375_REG_CHG_STAT1	0x1E1

#define MT6375_MSK_BATFET_DIS	0x40
#define MT6375_MSK_BLEED_DIS_EN	BIT(7)
#define MT6375_MSK_OTG_EN	0x04
#define MT6375_MSK_OTG_CV	0x3F
#define MT6375_MSK_OTG_CC	0x07
#define MT6375_MSK_CLK_FREQ	0xC0
#define MT6375_MSK_COMP_CLAMP	0x03
#define MT6375_MSK_BUCK_RAMPOFT	0xC0

#define ADC_CONV_TIME_US	2200
#define ADC_VBAT_SCALE		1250
#define ADC_TO_VBAT_RAW(vbat)	((vbat) * 1000 / ADC_VBAT_SCALE)
#define ADC_FROM_VBAT_RAW(raw)	((raw) * ADC_VBAT_SCALE / 1000)

#define NORMAL_CHARGING_CURR_UA	500000
#define FAST_CHARGING_CURR_UA	1500000
#define RECHG_THRESHOLD		100

enum mt6375_chg_reg_field {
	/* MT6375_REG_CORE_CTRL2 */
	F_SHIP_RST_DIS,
	/* MT6375_REG_CHG_BATPRO */
	F_BATINT, F_BATPROTECT_EN,
	/* MT6375_REG_CHG_TOP1 */
	F_CHG_EN, F_BUCK_EN, F_HZ, F_BATFET_DISDLY, F_BATFET_DIS, F_PP_PG_FLAG,
	/* MT6375_REG_CHG_TOP2 */
	F_VBUS_OV,
	/* MT6375_REG_CHG_AICR */
	F_ILIM_EN, F_IAICR,
	/* MT6375_REG_CHG_MIVR */
	F_VMIVR,
	/* MT6375_REG_CHG_VCHG */
	F_CV, F_VREC,
	/* MT6375_REG_CHG_ICHG */
	F_CC,
	/* MT6375_REG_CHG_TMR */
	F_CHG_TMR, F_CHG_TMR_EN,
	/* MT6375_REG_CHG_EOC */
	F_EOC_RST, F_TE, F_IEOC,
	/* MT6375_REG_CHG_VSYS */
	F_BLEED_DIS_EN,
	/* MT6375_REG_CHG_WDT */
	F_WDT, F_WDT_RST, F_WDT_EN,
	/* MT6375_REG_CHG_PUMPX */
	F_PE20_CODE, F_PE10_INC, F_PE_SEL, F_PE_EN,
	/* MT6375_REG_CHG_AICC1 */
	F_AICC_VTH, F_AICC_EN,
	/* MT6375_REG_CHG_AICC2 */
	F_AICC_RPT, F_AICC_ONESHOT,
	/* MT6375_REG_OTG_LBP */
	F_OTG_LBP,
	/* MT6375_REG_OTG_C */
	F_OTG_CC,
	/* MT6375_REG_BAT_COMP */
	F_IRCMP_V, F_IRCMP_R,
	/* MT6375_REG_CHG_STAT */
	F_IC_STAT,
	/* MT6375_REG_CHG_HD_TOP1 */
	F_FORCE_VBUS_SINK,
	/* MT6375_REG_BC12_FUNC */
	F_DCDT_SEL, F_BC12_EN,
	/* MT6375_REG_BC12_STAT */
	F_PORT_STAT,
	/* MT6375_REG_DPDM_CTRL1 */
	F_DM_DET_EN, F_DP_DET_EN, F_DPDM_SW_VCP_EN, F_MANUAL_MODE,
	/* MT6375_REG_DPDM_CTRL2 */
	F_DP_LDO_VSEL, F_DP_LDO_EN,
	/* MT6375_REG_DPDM_CTRL4 */
	F_DP_PULL_RSEL, F_DP_PULL_REN,
	/* MT6375_REG_ADC_CONFG1 */
	F_VBAT_MON_EN,
	/* MT6375_REG_CHG_STAT0 */
	F_ST_PWR_RDY,
	/* MT6375_REG_CHG_STAT1 */
	F_ST_MIVR,
	F_MAX,
};

enum {
	CHG_STAT_SLEEP,
	CHG_STAT_VBUS_RDY,
	CHG_STAT_TRICKLE,
	CHG_STAT_PRE,
	CHG_STAT_FAST,
	CHG_STAT_EOC,
	CHG_STAT_BKGND,
	CHG_STAT_DONE,
	CHG_STAT_FAULT,
	CHG_STAT_OTG = 15,
	CHG_STAT_MAX,
};

enum {
	PORT_STAT_NOINFO,
	PORT_STAT_APPLE_10W = 8,
	PORT_STAT_SAMSUNG,
	PORT_STAT_APPLE_5W,
	PORT_STAT_APPLE_12W,
	PORT_STAT_UNKNOWN_TA,
	PORT_STAT_SDP,
	PORT_STAT_CDP,
	PORT_STAT_DCP,
};

enum mt6375_adc_chan {
	ADC_CHAN_CHGVINDIV5,
	ADC_CHAN_VSYS,
	ADC_CHAN_VBAT,
	ADC_CHAN_IBUS,
	ADC_CHAN_IBAT,
	ADC_CHAN_TEMP_JC,
	ADC_CHAN_USBDP,
	ADC_CHAN_USBDM,
	ADC_CHAN_MAX,
};

/* map with mtk_chg_type_det.c */
enum attach_type {
	ATTACH_TYPE_NONE,
	ATTACH_TYPE_PWR_RDY,
	ATTACH_TYPE_TYPEC,
	ATTACH_TYPE_PD,
	ATTACH_TYPE_PD_SDP,
	ATTACH_TYPE_PD_DCP,
	ATTACH_TYPE_PD_NONSTD,
};

enum mt6375_attach_trigger {
	ATTACH_TRIG_IGNORE,
	ATTACH_TRIG_PWR_RDY,
	ATTACH_TRIG_TYPEC,
};

enum mt6375_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

enum mt6375_chg_dtprop_type {
	DTPROP_U32,
	DTPROP_BOOL,
};

struct mt6375_chg_data {
	struct device *dev;
	struct regmap *rmap;
	struct regmap_field *rmap_fields[F_MAX];
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	struct regulator_dev *rdev;
	struct regulator_desc rdesc;
	struct iio_channel *iio_adcs;
	struct mutex attach_lock;
	struct mutex pe_lock;
	struct mutex cv_lock;
	struct mutex hm_lock;
	struct workqueue_struct *wq;
	struct work_struct bc12_work;
	struct completion pe_done;
	struct completion aicc_done;
	struct charger_device *chgdev;

	enum power_supply_usb_type psy_usb_type;
	bool pwr_rdy;
	atomic_t attach;
	bool bc12_dn;
	bool batprotect_en;
	u32 hm_use_cnt;
	u32 zcv;
	u32 cv;
	atomic_t eoc_cnt;
	atomic_t tchg;
	int vbat0_flag;
};

struct mt6375_chg_platform_data {
	u32 aicr;
	u32 mivr;
	u32 ichg;
	u32 ieoc;
	u32 cv;
	u32 wdt;
	u32 otg_lbp;
	u32 ircmp_v;
	u32 ircmp_r;
	u32 vbus_ov;
	u32 vrec;
	u32 chg_tmr;
	u32 dcdt_sel;
	u32 bc12_sel;
	u32 boot_mode;
	u32 boot_type;
	enum mt6375_attach_trigger attach_trig;
	const char *chg_name;
	bool chg_tmr_en;
	bool wdt_en;
	bool te_en;
	bool usb_killer_detect;
};

struct mt6375_chg_range {
	u32 min;
	u32 max;
	u16 step;
	u8 offset;
	const u32 *table;
	u16 num_table;
	bool round_up;
};

struct mt6375_chg_field {
	const char *name;
	const struct mt6375_chg_range *range;
	struct reg_field field;
};

static const char *const mt6375_port_stat_names[] = {
	[PORT_STAT_NOINFO] = "No Info",
	[PORT_STAT_APPLE_10W] = "Apple 10W",
	[PORT_STAT_SAMSUNG] = "Samsung",
	[PORT_STAT_APPLE_5W] = "Apple 5W",
	[PORT_STAT_APPLE_12W] = "Apple 12W",
	[PORT_STAT_UNKNOWN_TA] = "Unknown TA",
	[PORT_STAT_SDP] = "SDP",
	[PORT_STAT_CDP] = "CDP",
	[PORT_STAT_DCP] = "DCP",
};

static const char *const mt6375_attach_trig_names[] = {
	"ignore", "pwr_rdy", "typec",
};

static const u32 mt6375_chg_vbus_ov[] = {
	5800, 6500, 11000, 14500,
};

static const u32 mt6375_chg_wdt[] = {
	8000, 40000, 80000, 160000,
};

static const u32 mt6375_chg_otg_cc[] = {
	500, 700, 1100, 1300, 1800, 2100, 2400,
};

/* for regulator usage */
static const u32 mt6375_chg_otg_cc_micro[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000,
};

static const u32 mt6375_chg_dpdm_ldo_vsel[] = {
	600, 650, 700, 750, 1800, 2800, 3300,
};

#define MT6375_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
	.min = _min, \
	.max = _max, \
	.step = _step, \
	.offset = _offset, \
	.round_up = _ru, \
}

#define MT6375_CHG_RANGE_T(_table, _ru) \
	{ .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }

static const struct mt6375_chg_range mt6375_chg_ranges[F_MAX] = {
	[F_BATINT] = MT6375_CHG_RANGE(3900, 4710, 10, 0, false),
	[F_VBUS_OV] = MT6375_CHG_RANGE_T(mt6375_chg_vbus_ov, false),
	[F_IAICR] = MT6375_CHG_RANGE(100, 3225, 25, 2, false),
	[F_VMIVR] = MT6375_CHG_RANGE(3900, 13400, 100, 0, true),
	[F_CV] = MT6375_CHG_RANGE(3900, 4710, 10, 0, false),
	[F_VREC] = MT6375_CHG_RANGE(100, 200, 100, 0, false),
	[F_CC] = MT6375_CHG_RANGE(300, 3150, 50, 6, false),
	[F_CHG_TMR] = MT6375_CHG_RANGE(5, 20, 5, 0, false),
	[F_IEOC] = MT6375_CHG_RANGE(100, 800, 50, 1, false),
	[F_WDT] = MT6375_CHG_RANGE_T(mt6375_chg_wdt, false),
	[F_PE20_CODE] = MT6375_CHG_RANGE(5500, 20000, 500, 0, false),
	[F_AICC_VTH] = MT6375_CHG_RANGE(3900, 13400, 100, 0, true),
	[F_AICC_RPT] = MT6375_CHG_RANGE(100, 3225, 25, 2, false),
	[F_OTG_LBP] = MT6375_CHG_RANGE(2700, 3800, 100, 4, false),
	[F_OTG_CC] = MT6375_CHG_RANGE_T(mt6375_chg_otg_cc, true),
	[F_IRCMP_V] = MT6375_CHG_RANGE(0, 224, 32, 0, false),
	[F_IRCMP_R] = MT6375_CHG_RANGE(0, 116900, 16700, 0, false),
	[F_DCDT_SEL] = MT6375_CHG_RANGE(0, 600, 300, 0, false),
	[F_DP_LDO_VSEL] = MT6375_CHG_RANGE_T(mt6375_chg_dpdm_ldo_vsel, false),
};

#define MT6375_CHG_FIELD_RANGE(_fd, _reg, _lsb, _msb, _range) \
	[_fd] = { \
		.name = #_fd, \
		.range = _range ? &mt6375_chg_ranges[_fd] : NULL, \
		.field = REG_FIELD(_reg, _lsb, _msb) \
	}

#define MT6375_CHG_FIELD(_fd, _reg, _lsb, _msb) \
	MT6375_CHG_FIELD_RANGE(_fd, _reg, _lsb, _msb, (_msb > _lsb))

static const struct mt6375_chg_field mt6375_chg_fields[F_MAX] = {
	MT6375_CHG_FIELD(F_SHIP_RST_DIS, MT6375_REG_CORE_CTRL2, 0, 0),
	MT6375_CHG_FIELD(F_BATINT, MT6375_REG_CHG_BATPRO, 0, 6),
	MT6375_CHG_FIELD(F_BATPROTECT_EN, MT6375_REG_CHG_BATPRO, 7, 7),
	MT6375_CHG_FIELD(F_CHG_EN, MT6375_REG_CHG_TOP1, 0, 0),
	MT6375_CHG_FIELD(F_BUCK_EN, MT6375_REG_CHG_TOP1, 1, 1),
	MT6375_CHG_FIELD(F_HZ, MT6375_REG_CHG_TOP1, 3, 3),
	MT6375_CHG_FIELD(F_BATFET_DISDLY, MT6375_REG_CHG_TOP1, 5, 5),
	MT6375_CHG_FIELD(F_BATFET_DIS, MT6375_REG_CHG_TOP1, 6, 6),
	MT6375_CHG_FIELD(F_PP_PG_FLAG, MT6375_REG_CHG_TOP1, 7, 7),
	MT6375_CHG_FIELD(F_VBUS_OV, MT6375_REG_CHG_TOP2, 0, 1),
	MT6375_CHG_FIELD(F_IAICR, MT6375_REG_CHG_AICR, 0, 6),
	MT6375_CHG_FIELD(F_ILIM_EN, MT6375_REG_CHG_AICR, 7, 7),
	MT6375_CHG_FIELD(F_VMIVR, MT6375_REG_CHG_MIVR, 0, 6),
	MT6375_CHG_FIELD(F_CV, MT6375_REG_CHG_VCHG, 0, 6),
	MT6375_CHG_FIELD_RANGE(F_VREC, MT6375_REG_CHG_VCHG, 7, 7, true),
	MT6375_CHG_FIELD(F_CC, MT6375_REG_CHG_ICHG, 0, 5),
	MT6375_CHG_FIELD(F_CHG_TMR, MT6375_REG_CHG_TMR, 4, 5),
	MT6375_CHG_FIELD(F_CHG_TMR_EN, MT6375_REG_CHG_TMR, 7, 7),
	MT6375_CHG_FIELD(F_EOC_RST, MT6375_REG_CHG_EOC, 0, 0),
	MT6375_CHG_FIELD(F_TE, MT6375_REG_CHG_EOC, 1, 1),
	MT6375_CHG_FIELD(F_IEOC, MT6375_REG_CHG_EOC, 4, 7),
	MT6375_CHG_FIELD(F_BLEED_DIS_EN, MT6375_REG_CHG_VSYS, 7, 7),
	MT6375_CHG_FIELD(F_WDT, MT6375_REG_CHG_WDT, 0, 1),
	MT6375_CHG_FIELD(F_WDT_RST, MT6375_REG_CHG_WDT, 2, 2),
	MT6375_CHG_FIELD(F_WDT_EN, MT6375_REG_CHG_WDT, 3, 3),
	MT6375_CHG_FIELD(F_PE20_CODE, MT6375_REG_CHG_PUMPX, 0, 4),
	MT6375_CHG_FIELD(F_PE10_INC, MT6375_REG_CHG_PUMPX, 5, 5),
	MT6375_CHG_FIELD(F_PE_SEL, MT6375_REG_CHG_PUMPX, 6, 6),
	MT6375_CHG_FIELD(F_PE_EN, MT6375_REG_CHG_PUMPX, 7, 7),
	MT6375_CHG_FIELD(F_AICC_VTH, MT6375_REG_CHG_AICC1, 0, 6),
	MT6375_CHG_FIELD(F_AICC_EN, MT6375_REG_CHG_AICC1, 7, 7),
	MT6375_CHG_FIELD(F_AICC_RPT, MT6375_REG_CHG_AICC2, 0, 6),
	MT6375_CHG_FIELD(F_AICC_ONESHOT, MT6375_REG_CHG_AICC2, 7, 7),
	MT6375_CHG_FIELD(F_OTG_CC, MT6375_REG_OTG_C, 0, 2),
	MT6375_CHG_FIELD(F_OTG_LBP, MT6375_REG_OTG_LBP, 0, 3),
	MT6375_CHG_FIELD(F_IRCMP_V, MT6375_REG_BAT_COMP, 0, 2),
	MT6375_CHG_FIELD(F_IRCMP_R, MT6375_REG_BAT_COMP, 4, 6),
	MT6375_CHG_FIELD_RANGE(F_IC_STAT, MT6375_REG_CHG_STAT, 0, 3, false),
	MT6375_CHG_FIELD(F_FORCE_VBUS_SINK, MT6375_REG_CHG_HD_TOP1, 6, 6),
	MT6375_CHG_FIELD(F_DCDT_SEL, MT6375_REG_BC12_FUNC, 4, 5),
	MT6375_CHG_FIELD(F_BC12_EN, MT6375_REG_BC12_FUNC, 7, 7),
	MT6375_CHG_FIELD_RANGE(F_PORT_STAT, MT6375_REG_BC12_STAT, 0, 3, false),
	MT6375_CHG_FIELD(F_DM_DET_EN, MT6375_REG_DPDM_CTRL1, 0, 0),
	MT6375_CHG_FIELD(F_DP_DET_EN, MT6375_REG_DPDM_CTRL1, 1, 1),
	MT6375_CHG_FIELD(F_DPDM_SW_VCP_EN, MT6375_REG_DPDM_CTRL1, 5, 5),
	MT6375_CHG_FIELD(F_MANUAL_MODE, MT6375_REG_DPDM_CTRL1, 7, 7),
	MT6375_CHG_FIELD(F_DP_LDO_VSEL, MT6375_REG_DPDM_CTRL2, 4, 6),
	MT6375_CHG_FIELD(F_DP_LDO_EN, MT6375_REG_DPDM_CTRL2, 7, 7),
	MT6375_CHG_FIELD(F_DP_PULL_RSEL, MT6375_REG_DPDM_CTRL4, 6, 6),
	MT6375_CHG_FIELD(F_DP_PULL_REN, MT6375_REG_DPDM_CTRL4, 7, 7),
	MT6375_CHG_FIELD(F_VBAT_MON_EN, MT6375_REG_ADC_CONFG1, 5, 5),
	MT6375_CHG_FIELD(F_ST_PWR_RDY, MT6375_REG_CHG_STAT0, 0, 0),
	MT6375_CHG_FIELD(F_ST_MIVR, MT6375_REG_CHG_STAT1, 7, 7),
};

static inline int mt6375_chg_field_set(struct mt6375_chg_data *ddata,
				       enum mt6375_chg_reg_field fd, u32 val);
static int mt6375_enable_hm(struct mt6375_chg_data *ddata, bool en)
{
	int ret = 0;

	mutex_lock(&ddata->hm_lock);
	if (en) {
		if (ddata->hm_use_cnt == 0) {
			ret = regmap_write(ddata->rmap, MT6375_REG_TM_PAS_CODE1,
					   0x69);
			if (ret < 0)
				goto out;
		}
		ddata->hm_use_cnt++;
	} else {
		if (ddata->hm_use_cnt == 1) {
			ret = regmap_write(ddata->rmap, MT6375_REG_TM_PAS_CODE1,
					   0x00);
			if (ret < 0)
				goto out;
		}
		if (ddata->hm_use_cnt > 0)
			ddata->hm_use_cnt--;
	}
out:
	mutex_unlock(&ddata->hm_lock);
	return ret;
}

static int mt6375_set_boost_param(struct mt6375_chg_data *ddata, bool bst)
{
	int i, ret;
	u8 val;

	static const u16 regs[] = {
		MT6375_REG_CHG_TOP2,
		MT6375_REG_CHG_DUMY0,
		MT6375_REG_CHG_HD_BUCK5,
		MT6375_REG_CHG_VSYS,
	};
	static const u8 msks[] = {
		MT6375_MSK_CLK_FREQ,
		MT6375_MSK_COMP_CLAMP,
		MT6375_MSK_BUCK_RAMPOFT,
		MT6375_MSK_BLEED_DIS_EN,
	};
	static const u8 buck[] = {
		0x01, 0x00, 0x01, 0x01,
	};
	static const u8 boost[] = {
		0x00, 0x03, 0x03, 0x00,
	};

	ret = mt6375_enable_hm(ddata, true);
	if (ret < 0)
		return ret;
	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		val = bst ? boost[i] : buck[i];
		val <<= ffs(msks[i]) - 1;
		ret = regmap_update_bits(ddata->rmap, regs[i], msks[i], val);
		if (ret < 0) {
			dev_err(ddata->dev,
				"failed to set reg0x%02X=0x%02X, msk0x%02X\n",
				regs[i], val, msks[i]);
			goto recover;
		}
	}
	goto out;
recover:
	/*
	 * we do not guarantee the recovery is OK
	 * keep the error code from above
	 */
	for (; i >= 0; i--) {
		val = bst ? buck[i] : boost[i];
		val <<= ffs(msks[i]) - 1;
		if (regmap_update_bits(ddata->rmap, regs[i], msks[i],
				       val) < 0) {
			dev_err(ddata->dev,
				"failed to set reg0x%02X=0x%02X, msk0x%02X\n",
				regs[i], val, msks[i]);
		}
	}
out:
	mt6375_enable_hm(ddata, false);
	return ret;
}

static bool mt6375_chg_is_usb_killer(struct mt6375_chg_data *ddata)
{
	int i, ret, vdp, vdm;
	bool killer = false;
	static const u32 vdiff = 200;
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(ddata->dev);
	static const struct {
		enum mt6375_chg_reg_field fd;
		u32 val;
	} settings[] = {
		{ F_MANUAL_MODE, 1 },
		{ F_DPDM_SW_VCP_EN, 1 },
		{ F_DP_DET_EN, 1 },
		{ F_DM_DET_EN, 1 },
		{ F_DP_LDO_VSEL, 1800 },
		{ F_DP_LDO_EN, 1 },
		{ F_DP_PULL_RSEL, 0 },
		{ F_DP_PULL_REN, 1 },
	};

	if (!pdata->usb_killer_detect) {
		mt_dbg(ddata->dev, "disabled\n");
		return false;
	}

	/* turn on usb dp 1.8V */
	for (i = 0; i < ARRAY_SIZE(settings); i++) {
		ret = mt6375_chg_field_set(ddata, settings[i].fd,
					   settings[i].val);
		if (ret < 0)
			goto recover;
	}
	--i;

	/* check usb dpdm */
	ret = iio_read_channel_processed(&ddata->iio_adcs[ADC_CHAN_USBDP],
					 &vdp);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to read usbdp voltage\n");
		goto recover;
	}
	ret = iio_read_channel_processed(&ddata->iio_adcs[ADC_CHAN_USBDM],
					 &vdm);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to read usbdm voltage\n");
		goto recover;
	}
	vdp = U_TO_M(vdp);
	vdm = U_TO_M(vdm);
	mt_dbg(ddata->dev, "dp=%dmV, dm=%dmV, vdiff=%dmV\n", vdp, vdm,
	       abs(vdp - vdm));
	if (abs(vdp - vdm) < vdiff) {
		mt_dbg(ddata->dev, "suspect usb killer\n");
		killer = true;
	}
recover:
	/* we do not guarantee the recovery is OK */
	for (; i >= 0; i--)
		mt6375_chg_field_set(ddata, settings[i].fd, 0);
	return killer;
}

static int mt6375_chg_regulator_enable(struct regulator_dev *rdev)
{
	int ret;
	struct mt6375_chg_data *ddata = rdev->reg_data;

	if (mt6375_chg_is_usb_killer(ddata))
		return -EIO;
	ret = mt6375_set_boost_param(ddata, true);
	if (ret < 0)
		return ret;
	ret = regulator_enable_regmap(rdev);
	if (ret < 0) {
		mt6375_set_boost_param(ddata, false);
		return ret;
	}
	return 0;
}

static int mt6375_chg_regulator_disable(struct regulator_dev *rdev)
{
	int ret;
	struct mt6375_chg_data *ddata = rdev->reg_data;

	ret = mt6375_set_boost_param(ddata, false);
	if (ret < 0)
		return ret;
	ret = regulator_disable_regmap(rdev);
	if (ret < 0) {
		mt6375_set_boost_param(ddata, true);
		return ret;
	}
	return 0;
}

static const struct regulator_ops mt6375_chg_otg_rops = {
	.enable = mt6375_chg_regulator_enable,
	.disable = mt6375_chg_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_current_limit = regulator_set_current_limit_regmap,
	.get_current_limit = regulator_get_current_limit_regmap,
};

static const struct regulator_desc mt6375_chg_otg_rdesc = {
	.of_match = "mt6375,otg-vbus",
	.ops = &mt6375_chg_otg_rops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.min_uV = 4850000,
	.uV_step = 25000,
	.n_voltages = 47,
	.linear_min_sel = 20,
	.curr_table = mt6375_chg_otg_cc_micro,
	.n_current_limits = ARRAY_SIZE(mt6375_chg_otg_cc_micro),
	.csel_reg = MT6375_REG_OTG_C,
	.csel_mask = MT6375_MSK_OTG_CC,
	.vsel_reg = MT6375_REG_OTG_V,
	.vsel_mask = MT6375_MSK_OTG_CV,
	.enable_reg = MT6375_REG_CHG_TOP1,
	.enable_mask = MT6375_MSK_OTG_EN,
};

static const struct mt6375_chg_platform_data mt6375_chg_pdata_def = {
	.aicr = 3225,
	.mivr = 4400,
	.ichg = 2000,
	.ieoc = 150,
	.cv = 4200,
	.wdt = 40000,
	.vbus_ov = 14500,
	.vrec = 100,
	.ircmp_v = 0,
	.ircmp_r = 0,	/* uOhm */
	.chg_tmr = 10,	/* hr */
	.dcdt_sel = 600,
	.wdt_en = false,
	.te_en = true,
	.chg_tmr_en = true,
	.chg_name = "primary_chg",
	.usb_killer_detect = false,
};

static inline u8 mt6375_chg_val_toreg(const struct mt6375_chg_range *range,
				      u32 val)
{
	int i;
	u8 reg;

	if (!range)
		return val;

	if (range->table) {
		if (val <= range->table[0])
			return 0;
		for (i = 1; i < range->num_table - 1; i++) {
			if (val == range->table[i])
				return i;
			if (val > range->table[i] &&
			     val < range->table[i + 1])
				return range->round_up ? i + 1 : i;
		}
		return range->num_table - 1;
	}
	if (val <= range->min)
		reg = 0;
	else if (val >= range->max)
		reg = (range->max - range->min) / range->step;
	else if (range->round_up)
		reg = DIV_ROUND_UP(val - range->min, range->step);
	else
		reg = (val - range->min) / range->step;
	return reg + range->offset;
}

static inline u32 mt6375_chg_reg_toval(const struct mt6375_chg_range *range,
				       u8 reg)
{
	if (!range)
		return reg;
	return range->table ? range->table[reg] :
			      range->min + range->step * (reg - range->offset);
}

static inline int mt6375_chg_field_get(struct mt6375_chg_data *ddata,
				       enum mt6375_chg_reg_field fd, u32 *val)
{
	int ret;
	u32 regval;

	ret = regmap_field_read(ddata->rmap_fields[fd], &regval);
	if (ret < 0)
		return ret;
	*val = mt6375_chg_reg_toval(mt6375_chg_fields[fd].range, regval);
	mt_dbg(ddata->dev, "%s, reg=0x%02X, val=%d\n",
	       mt6375_chg_fields[fd].name, regval, *val);
	return 0;
}

static inline int mt6375_chg_field_set(struct mt6375_chg_data *ddata,
				       enum mt6375_chg_reg_field fd, u32 val)
{
	mt_dbg(ddata->dev, "%s, val=%d\n", mt6375_chg_fields[fd].name,
	       val);
	val = mt6375_chg_val_toreg(mt6375_chg_fields[fd].range, val);
	return regmap_field_write(ddata->rmap_fields[fd], val);
}

static int mt6375_chg_enable_charging(struct mt6375_chg_data *ddata, bool en)
{
	int ret;

	mutex_lock(&ddata->cv_lock);
	ret = mt6375_chg_field_set(ddata, F_CHG_EN, en);
	mutex_unlock(&ddata->cv_lock);
	return ret;
}

static int mt6375_chg_is_enabled(struct mt6375_chg_data *ddata, bool *en)
{
	int ret = 0;
	u32 val = 0;

	ret = mt6375_chg_field_get(ddata, F_CHG_EN, &val);
	if (ret < 0)
		return ret;
	*en = val;
	return 0;
}

static int mt6375_chg_is_charge_done(struct mt6375_chg_data *ddata, bool *done)
{
	int ret;
	union power_supply_propval val;

	ret = power_supply_get_property(ddata->psy, POWER_SUPPLY_PROP_STATUS,
					&val);
	if (ret < 0)
		return ret;
	*done = (val.intval == POWER_SUPPLY_STATUS_FULL);
	return 0;
}

static int mt6375_chg_set_cv(struct mt6375_chg_data *ddata, u32 mV)
{
	int ret = 0;
	bool done = false, enabled = false;

	mutex_lock(&ddata->cv_lock);
	if (ddata->batprotect_en) {
		dev_notice(ddata->dev,
			   "batprotect enabled, should not set cv\n");
		goto out;
	}
	if (mV <= ddata->cv || mV >= ddata->cv + RECHG_THRESHOLD)
		goto out_cv;
	ret = mt6375_chg_is_charge_done(ddata, &done);
	if (ret < 0 || !done)
		goto out_cv;
	ret = mt6375_chg_is_enabled(ddata, &enabled);
	if (ret < 0 || !enabled)
		goto out_cv;
	if (mt6375_chg_field_set(ddata, F_CHG_EN, false) < 0)
		dev_notice(ddata->dev, "failed to disable charging\n");
out_cv:
	ret = mt6375_chg_field_set(ddata, F_CV, mV);
	if (!ret)
		ddata->cv = mV;
	if (done && enabled)
		mt6375_chg_field_set(ddata, F_CHG_EN, true);
out:
	mutex_unlock(&ddata->cv_lock);
	return ret;
}

static int mt6375_get_chg_status(struct mt6375_chg_data *ddata)
{
	int ret = 0, attach;
	u32 stat;
	bool chg_en = false;

	attach = atomic_read(&ddata->attach);
	if (!attach)
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	ret = mt6375_chg_is_enabled(ddata, &chg_en);
	if (ret < 0)
		return ret;
	ret = mt6375_chg_field_get(ddata, F_IC_STAT, &stat);
	if (ret < 0)
		return ret;
	switch (stat) {
	case CHG_STAT_OTG:
		return POWER_SUPPLY_STATUS_DISCHARGING;
	case CHG_STAT_SLEEP:
	case CHG_STAT_VBUS_RDY:
	case CHG_STAT_TRICKLE:
	case CHG_STAT_PRE:
	case CHG_STAT_FAST:
	case CHG_STAT_EOC:
	case CHG_STAT_BKGND:
		if (chg_en)
			return POWER_SUPPLY_STATUS_CHARGING;
		else
			return POWER_SUPPLY_STATUS_NOT_CHARGING;
	case CHG_STAT_DONE:
		return POWER_SUPPLY_STATUS_FULL;
	case CHG_STAT_FAULT:
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	default:
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}
}

static void mt6375_chg_attach_pre_process(struct mt6375_chg_data *ddata,
					  enum mt6375_attach_trigger trig,
					  int attach)
{
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(ddata->dev);
	bool bc12_dn;

	mt_dbg(ddata->dev, "trig=%s,attach=%d\n",
	       mt6375_attach_trig_names[trig], attach);
	/* if attach trigger is not match, ignore it */
	if (pdata->attach_trig != trig) {
		mt_dbg(ddata->dev, "trig=%s ignored\n",
		       mt6375_attach_trig_names[trig]);
		return;
	}

	mutex_lock(&ddata->attach_lock);
	if (attach == ATTACH_TYPE_NONE)
		ddata->bc12_dn = false;

	bc12_dn = ddata->bc12_dn;
	if (!bc12_dn)
		atomic_set(&ddata->attach, attach);
	mutex_unlock(&ddata->attach_lock);

	if (attach > ATTACH_TYPE_PD && bc12_dn)
		return;

	if (!queue_work(ddata->wq, &ddata->bc12_work))
		dev_notice(ddata->dev, "%s bc12 work already queued\n", __func__);
}

static void mt6375_chg_pwr_rdy_process(struct mt6375_chg_data *ddata)
{
	int ret;
	u32 val;

	ret = mt6375_chg_field_get(ddata, F_ST_PWR_RDY, &val);
	if (ret < 0 || ddata->pwr_rdy == val)
		return;
	ddata->pwr_rdy = val;
	mt_dbg(ddata->dev, "pwr_rdy=%d\n", val);
	ret = mt6375_chg_field_set(ddata, F_BLEED_DIS_EN, !ddata->pwr_rdy);
	if (ret < 0)
		dev_err(ddata->dev, "failed to set bleed discharge = %d\n",
			!ddata->pwr_rdy);
	mt6375_chg_attach_pre_process(ddata, ATTACH_TRIG_PWR_RDY,
				val ? ATTACH_TYPE_PWR_RDY : ATTACH_TYPE_NONE);
}

static int mt6375_chg_set_usbsw(struct mt6375_chg_data *ddata,
				enum mt6375_usbsw usbsw)
{
	struct phy *phy;
	int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
					       PHY_MODE_BC11_CLR;

	mt_dbg(ddata->dev, "usbsw=%d\n", usbsw);
	phy = phy_get(ddata->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(ddata->dev, "failed to get usb2-phy\n");
		return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_err(ddata->dev, "failed to set phy ext mode\n");
	phy_put(ddata->dev, phy);
	return ret;
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

static int mt6375_chg_enable_bc12(struct mt6375_chg_data *ddata, bool en)
{
	int i, ret, attach;
	static const int max_wait_cnt = 250;

	mt_dbg(ddata->dev, "en=%d\n", en);
	if (en) {
		/* CDP port specific process */
		dev_info(ddata->dev, "check CDP block\n");
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy(ddata->dev))
				break;
			attach = atomic_read(&ddata->attach);
			if (attach == ATTACH_TYPE_TYPEC)
				msleep(100);
			else {
				dev_notice(ddata->dev, "%s: change attach:%d, disable bc12\n",
					   __func__, attach);
				en = false;
				break;
			}
		}
		if (i == max_wait_cnt)
			dev_notice(ddata->dev, "CDP timeout\n", __func__);
		else
			dev_info(ddata->dev, "CDP free\n", __func__);
	}
	ret = mt6375_chg_set_usbsw(ddata, en ? USBSW_CHG : USBSW_USB);
	if (ret)
		return ret;
	return mt6375_chg_field_set(ddata, F_BC12_EN, en);
}

static void mt6375_chg_bc12_work_func(struct work_struct *work)
{
	struct mt6375_chg_data *ddata = container_of(work,
						     struct mt6375_chg_data,
						     bc12_work);
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(ddata->dev);
	bool bc12_ctrl = true, bc12_en = false, rpt_psy = true;
	int ret, attach;
	u32 val = 0;

	mutex_lock(&ddata->attach_lock);
	attach = atomic_read(&ddata->attach);
	mt_dbg(ddata->dev, "attach=%d\n", attach);

	if (attach > ATTACH_TYPE_NONE && pdata->boot_mode == 5) {
		/* skip bc12 to speed up ADVMETA_BOOT */
		dev_notice(ddata->dev, "force SDP in meta mode\n");
		ddata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		ddata->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		goto out;
	}

	switch (attach) {
	case ATTACH_TYPE_NONE:
		ddata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		ddata->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	case ATTACH_TYPE_TYPEC:
		if (!ddata->bc12_dn) {
			bc12_en = true;
			rpt_psy = false;
			goto out;
		}
		ret = mt6375_chg_field_get(ddata, F_PORT_STAT, &val);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to get port stat\n");
			rpt_psy = false;
			goto out;
		}
		break;
	case ATTACH_TYPE_PD_SDP:
		val = PORT_STAT_SDP;
		break;
	case ATTACH_TYPE_PD_DCP:
		/* not to enable bc12 */
		ddata->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		ddata->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		goto out;
	case ATTACH_TYPE_PD_NONSTD:
		val = PORT_STAT_UNKNOWN_TA;
		break;
	default:
		dev_info(ddata->dev,
			 "%s: using tradtional bc12 flow!\n", __func__);
		break;
	}

	switch (val) {
	case PORT_STAT_NOINFO:
		bc12_ctrl = false;
		rpt_psy = false;
		dev_info(ddata->dev, "%s no info\n", __func__);
		goto out;
	case PORT_STAT_APPLE_5W:
	case PORT_STAT_APPLE_10W:
	case PORT_STAT_APPLE_12W:
	case PORT_STAT_SAMSUNG:
	case PORT_STAT_DCP:
		ddata->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		ddata->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		bc12_en = true;
		break;
	case PORT_STAT_SDP:
		ddata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		ddata->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case PORT_STAT_CDP:
		ddata->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		ddata->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case PORT_STAT_UNKNOWN_TA:
		ddata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		ddata->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	default:
		bc12_ctrl = false;
		rpt_psy = false;
		dev_info(ddata->dev, "Unknown port stat %d\n", val);
		goto out;
	}
	mt_dbg(ddata->dev, "port stat = %s\n", mt6375_port_stat_names[val]);
out:
	mutex_unlock(&ddata->attach_lock);
	if (bc12_ctrl && (mt6375_chg_enable_bc12(ddata, bc12_en) < 0))
		dev_err(ddata->dev, "failed to set bc12 = %d\n", bc12_en);
	if (rpt_psy)
		power_supply_changed(ddata->psy);
}

static enum power_supply_usb_type mt6375_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};

static enum power_supply_property mt6375_chg_psy_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
};

static int mt6375_chg_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return 1;
	default:
		return 0;
	}
	return 0;
}

static int mt6375_chg_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct mt6375_chg_data *ddata = power_supply_get_drvdata(psy);
	int ret = 0;
	u16 data;
	u32 _val = 0;

	mt_dbg(ddata->dev, "psp=%d\n", psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = MT6375_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = atomic_read(&ddata->attach);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = mt6375_get_chg_status(ddata);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		mutex_lock(&ddata->pe_lock);
		ret = mt6375_chg_field_get(ddata, F_CC, &val->intval);
		mutex_unlock(&ddata->pe_lock);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		mutex_lock(&ddata->cv_lock);
		ret = mt6375_chg_field_get(ddata, F_CV, &val->intval);
		mutex_unlock(&ddata->cv_lock);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		mutex_lock(&ddata->pe_lock);
		ret = mt6375_chg_field_get(ddata, F_IAICR, &val->intval);
		mutex_unlock(&ddata->pe_lock);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		mutex_lock(&ddata->pe_lock);
		ret = mt6375_chg_field_get(ddata, F_VMIVR, &val->intval);
		mutex_unlock(&ddata->pe_lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = mt6375_chg_field_get(ddata, F_IEOC, &val->intval);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		mutex_lock(&ddata->attach_lock);
		val->intval = ddata->psy_usb_type;
		mutex_unlock(&ddata->attach_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (ddata->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = NORMAL_CHARGING_CURR_UA;
		else if (ddata->psy_desc.type == POWER_SUPPLY_TYPE_USB_DCP)
			val->intval = FAST_CHARGING_CURR_UA;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (ddata->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = ddata->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		mutex_lock(&ddata->cv_lock);
		ret = mt6375_chg_field_get(ddata, F_VBAT_MON_EN, &_val);
		if (_val) {
			ret = -EBUSY;
			dev_notice(ddata->dev, "vbat_mon is enabled\n");
			mutex_unlock(&ddata->cv_lock);
			break;
		}
		ret = mt6375_chg_field_set(ddata, F_VBAT_MON_EN, 1);
		if (ret < 0) {
			dev_notice(ddata->dev, "failed to enable vbat monitor\n");
			mutex_unlock(&ddata->cv_lock);
			break;
		}
		usleep_range(ADC_CONV_TIME_US * 2, ADC_CONV_TIME_US * 3);
		ret = regmap_bulk_read(ddata->rmap, MT6375_REG_VBAT_MON_RPT, &data, 2);
		if (ret < 0)
			dev_notice(ddata->dev, "failed to get vbat monitor report\n");
		else
			val->intval = ADC_FROM_VBAT_RAW(be16_to_cpu(data));
		if (mt6375_chg_field_set(ddata, F_VBAT_MON_EN, 0) < 0)
			dev_notice(ddata->dev, "failed to disable vbat monitor\n");
		mutex_unlock(&ddata->cv_lock);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		val->intval = ddata->vbat0_flag;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mt6375_chg_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	int ret = 0;
	struct mt6375_chg_data *ddata = power_supply_get_drvdata(psy);

	mt_dbg(ddata->dev, "psp=%d\n", psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mt6375_chg_attach_pre_process(ddata, ATTACH_TRIG_TYPEC,
					      val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = mt6375_chg_enable_charging(ddata, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		mutex_lock(&ddata->pe_lock);
		ret = mt6375_chg_field_set(ddata, F_CC, val->intval);
		mutex_unlock(&ddata->pe_lock);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = mt6375_chg_set_cv(ddata, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		mutex_lock(&ddata->pe_lock);
		ret = mt6375_chg_field_set(ddata, F_IAICR, val->intval);
		mutex_unlock(&ddata->pe_lock);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		mutex_lock(&ddata->pe_lock);
		ret = mt6375_chg_field_set(ddata, F_VMIVR, val->intval);
		mutex_unlock(&ddata->pe_lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = mt6375_chg_field_set(ddata, F_IEOC, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		ddata->vbat0_flag = val->intval;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static char *mt6375_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static const struct power_supply_desc mt6375_psy_desc = {
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = mt6375_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(mt6375_chg_psy_usb_types),
	.properties = mt6375_chg_psy_properties,
	.num_properties = ARRAY_SIZE(mt6375_chg_psy_properties),
	.property_is_writeable = mt6375_chg_property_is_writeable,
	.get_property = mt6375_chg_get_property,
	.set_property = mt6375_chg_set_property,
};

/* The following functions are for mtk charger device */

static int mt6375_enable_charging(struct charger_device *chgdev, bool en)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	return mt6375_chg_enable_charging(ddata, en);
}

static int mt6375_is_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	return mt6375_chg_is_enabled(ddata, en);
}

static int mt6375_set_ichg(struct charger_device *chgdev, u32 uA)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = U_TO_M(uA) };

	mt_dbg(ddata->dev, "ichg=%d\n", uA);
	return power_supply_set_property(ddata->psy,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
}

static int mt6375_get_ichg(struct charger_device *chgdev, u32 *uA)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val;

	ret = power_supply_get_property(ddata->psy,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
	if (ret < 0)
		return ret;
	*uA = M_TO_U(val.intval);
	return 0;
}

static int mt6375_get_min_ichg(struct charger_device *chgdev, u32 *uA)
{
	*uA = M_TO_U(mt6375_chg_fields[F_CC].range->min);
	return 0;
}

static int mt6375_set_cv(struct charger_device *chgdev, u32 uV)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mt_dbg(ddata->dev, "cv=%d\n", uV);
	return mt6375_chg_set_cv(ddata, U_TO_M(uV));
}

static int mt6375_get_cv(struct charger_device *chgdev, u32 *uV)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val;

	ret = power_supply_get_property(ddata->psy,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
	if (ret < 0)
		return ret;
	*uV = M_TO_U(val.intval);
	return 0;
}

static int mt6375_set_aicr(struct charger_device *chgdev, u32 uA)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = U_TO_M(uA) };

	mt_dbg(ddata->dev, "aicr=%d\n", uA);
	return power_supply_set_property(ddata->psy,
		POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
}

static int mt6375_get_aicr(struct charger_device *chgdev, u32 *uA)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val;

	ret = power_supply_get_property(ddata->psy,
		POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	if (ret < 0)
		return ret;
	*uA = M_TO_U(val.intval);
	return 0;
}

static int mt6375_get_min_aicr(struct charger_device *chgdev, u32 *uA)
{
	*uA = M_TO_U(mt6375_chg_fields[F_IAICR].range->min);
	return 0;
}

static int mt6375_set_mivr(struct charger_device *chgdev, u32 uV)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = U_TO_M(uV) };

	mt_dbg(ddata->dev, "mivr=%d\n", uV);
	return power_supply_set_property(ddata->psy,
		POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, &val);
}

static int mt6375_get_mivr(struct charger_device *chgdev, u32 *uV)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val;

	ret = power_supply_get_property(ddata->psy,
		POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, &val);
	if (ret < 0)
		return ret;
	*uV = M_TO_U(val.intval);
	return 0;
}

static int mt6375_get_mivr_state(struct charger_device *chgdev, bool *active)
{
	int ret;
	u32 val;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	*active = false;
	ret = mt6375_chg_field_get(ddata, F_ST_MIVR, &val);
	if (ret < 0)
		return ret;
	*active = val;
	return 0;
}

static int mt6375_get_adc(struct charger_device *chgdev, enum adc_channel chan,
			  int *min, int *max)
{
	int ret;
	enum mt6375_adc_chan adc_chan;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		adc_chan = ADC_CHAN_CHGVINDIV5;
		break;
	case ADC_CHANNEL_VSYS:
		adc_chan = ADC_CHAN_VSYS;
		break;
	case ADC_CHANNEL_VBAT:
		adc_chan = ADC_CHAN_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		adc_chan = ADC_CHAN_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		adc_chan = ADC_CHAN_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		adc_chan = ADC_CHAN_TEMP_JC;
		break;
	default:
		return -EINVAL;
	}
	ret = iio_read_channel_processed(&ddata->iio_adcs[adc_chan], min);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to read adc\n");
		return ret;
	}
	*max = *min;
	return 0;
}

static int mt6375_get_vbus(struct charger_device *chgdev, u32 *vbus)
{
	return mt6375_get_adc(chgdev, ADC_CHANNEL_VBUS, vbus, vbus);
}

static int mt6375_get_ibus(struct charger_device *chgdev, u32 *ibus)
{
	return mt6375_get_adc(chgdev, ADC_CHANNEL_IBUS, ibus, ibus);
}

static int mt6375_get_ibat(struct charger_device *chgdev, u32 *ibat)
{
	return mt6375_get_adc(chgdev, ADC_CHANNEL_IBAT, ibat, ibat);
}

#define ABNORMAL_TEMP_JC	120
static int mt6375_get_tchg(struct charger_device *chgdev,
			   int *tchg_min, int *tchg_max)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	int temp_jc = 0, ret = 0, retry_cnt = 3;

	/* temp abnormal workaround */
	do {
		ret = mt6375_get_adc(chgdev, ADC_CHANNEL_TEMP_JC, &temp_jc,
				     &temp_jc);
	} while ((ret < 0 || temp_jc >= ABNORMAL_TEMP_JC) && (--retry_cnt) > 0);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to get temp jc\n");
		return ret;
	}
	/* use last result if temp_jc is abnormal */
	if (temp_jc >= ABNORMAL_TEMP_JC)
		temp_jc = atomic_read(&ddata->tchg);
	else
		atomic_set(&ddata->tchg, temp_jc);
	*tchg_min = *tchg_max = temp_jc;
	mt_dbg(ddata->dev, "tchg=%d\n", temp_jc);
	return 0;
}

static int mt6375_get_zcv(struct charger_device *chgdev, u32 *uV)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	*uV = M_TO_U(ddata->zcv);
	return 0;
}

static int mt6375_set_ieoc(struct charger_device *chgdev, u32 uA)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = U_TO_M(uA) };

	mt_dbg(ddata->dev, "ieoc=%d\n", uA);
	return power_supply_set_property(ddata->psy,
		POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &val);
}

static int mt6375_reset_eoc_state(struct charger_device *chgdev)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	return mt6375_chg_field_set(ddata, F_EOC_RST, 1);
}

static int mt6375_sw_check_eoc(struct charger_device *chgdev, u32 uA)
{
	int ret, ibat;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	ret = mt6375_get_ibat(chgdev, &ibat);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to get ibat\n");
		return ret;
	}

	if (ibat <= uA) {
		/* if it happens 3 times, trigger EOC event */
		if (atomic_read(&ddata->eoc_cnt) == 2) {
			atomic_set(&ddata->eoc_cnt, 0);
			mt_dbg(ddata->dev, "ieoc=%d, ibat=%d\n", uA, ibat);
			charger_dev_notify(ddata->chgdev,
					   CHARGER_DEV_NOTIFY_EOC);
		} else
			atomic_inc(&ddata->eoc_cnt);
	} else
		atomic_set(&ddata->eoc_cnt, 0);
	return 0;
}

static int mt6375_is_charge_done(struct charger_device *chgdev, bool *done)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	return mt6375_chg_is_charge_done(ddata, done);
}

static int mt6375_enable_te(struct charger_device *chgdev, bool en)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mt_dbg(ddata->dev, "en=%d\n", en);
	return mt6375_chg_field_set(ddata, F_TE, en);
}

static int mt6375_enable_buck(struct charger_device *chgdev, bool en)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mt_dbg(ddata->dev, "en=%d\n", en);
	return mt6375_chg_field_set(ddata, F_BUCK_EN, en);
}

static int mt6375_is_buck_enabled(struct charger_device *chgdev, bool *en)
{
	int ret;
	u32 val;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	ret = mt6375_chg_field_get(ddata, F_BUCK_EN, &val);
	if (ret < 0)
		return ret;
	*en = val;
	return 0;
}

static int mt6375_enable_chg_timer(struct charger_device *chgdev, bool en)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mt_dbg(ddata->dev, "en=%d\n", en);
	return mt6375_chg_field_set(ddata, F_CHG_TMR_EN, en);
}

static int mt6375_is_chg_timer_enabled(struct charger_device *chgdev, bool *en)
{
	int ret;
	u32 val;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	ret = mt6375_chg_field_get(ddata, F_CHG_TMR_EN, &val);
	if (ret < 0)
		return ret;
	*en = val;
	return 0;
}

static int mt6375_enable_hz(struct charger_device *chgdev, bool en)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	return mt6375_chg_field_set(ddata, F_HZ, en ? 1 : 0);
}

static int mt6375_kick_wdt(struct charger_device *chgdev)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	return mt6375_chg_field_set(ddata, F_WDT_RST, 1);
}

static int mt6375_run_aicc(struct charger_device *chgdev, u32 *uA)
{
	int ret;
	bool active;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	long ret_comp;

	ret = mt6375_get_mivr_state(chgdev, &active);
	if (ret < 0)
		return ret;
	if (!active) {
		mt_dbg(ddata->dev, "mivr loop is not active\n");
		return 0;
	}

	mutex_lock(&ddata->pe_lock);
	ret = mt6375_chg_field_set(ddata, F_AICC_EN, 1);
	if (ret < 0)
		goto out;
	reinit_completion(&ddata->aicc_done);
	/* worst case = 128steps * 52msec = 6656ms */
	ret_comp = wait_for_completion_interruptible_timeout(&ddata->aicc_done,
		msecs_to_jiffies(7000));
	if (ret_comp == 0)
		ret = -ETIMEDOUT;
	else if (ret_comp < 0)
		ret = -EINTR;
	else
		ret = 0;
	if (ret < 0) {
		dev_err(ddata->dev, "failed to wait aicc (%d)\n", ret);
		goto out;
	}
	ret = mt6375_chg_field_get(ddata, F_AICC_RPT, uA);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to get aicc report\n");
		goto out;
	}
	*uA = M_TO_U(*uA);
out:
	mt6375_chg_field_set(ddata, F_AICC_EN, 0);
	mutex_unlock(&ddata->pe_lock);
	return ret;
}

static int mt6375_run_pe(struct mt6375_chg_data *ddata, bool pe20)
{
	int ret;
	unsigned long timeout = pe20 ? 1400 : 2800;
	long ret_comp;

	ret = mt6375_chg_field_set(ddata, F_IAICR, 800);
	if (ret < 0)
		return ret;
	ret = mt6375_chg_field_set(ddata, F_CC, 2000);
	if (ret < 0)
		return ret;
	ret = mt6375_chg_field_set(ddata, F_CHG_EN, 1);
	if (ret < 0)
		return ret;
	ret = mt6375_chg_field_set(ddata, F_PE_SEL, pe20);
	if (ret < 0)
		return ret;
	ret = mt6375_chg_field_set(ddata, F_PE_EN, 1);
	if (ret < 0)
		return ret;
	reinit_completion(&ddata->pe_done);
	ret_comp = wait_for_completion_interruptible_timeout(&ddata->pe_done,
		msecs_to_jiffies(timeout));
	if (ret_comp == 0)
		ret = -ETIMEDOUT;
	else if (ret_comp < 0)
		ret = -EINTR;
	else
		ret = 0;
	if (ret < 0)
		dev_err(ddata->dev, "failed to wait pe (%d)\n", ret);
	return ret;
}

static int mt6375_set_pe_current_pattern(struct charger_device *chgdev,
					 bool inc)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mutex_lock(&ddata->pe_lock);
	mt_dbg(ddata->dev, "inc=%d\n", inc);
	ret = mt6375_chg_field_set(ddata, F_PE10_INC, inc);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set pe10 up/down\n");
		goto out;
	}
	ret = mt6375_run_pe(ddata, false);
out:
	mutex_unlock(&ddata->pe_lock);
	return ret;
}

static int mt6375_set_pe20_efficiency_table(struct charger_device *chgdev)
{
	return 0;
}

static int mt6375_set_pe20_current_pattern(struct charger_device *chgdev,
					   u32 uV)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mutex_lock(&ddata->pe_lock);
	mt_dbg(ddata->dev, "pe20=%d\n", uV);
	ret = mt6375_chg_field_set(ddata, F_PE20_CODE, U_TO_M(uV));
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set pe20 code\n", __func__);
		goto out;
	}
	ret = mt6375_run_pe(ddata, true);
out:
	mutex_unlock(&ddata->pe_lock);
	return ret;
}

static int mt6375_enable_pe_cable_drop_comp(struct charger_device *chgdev,
					    bool en)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	if (en)
		return 0;
	mutex_lock(&ddata->pe_lock);
	mt_dbg(ddata->dev, "en=%d\n", en);
	ret = mt6375_chg_field_set(ddata, F_PE20_CODE, 0x1F);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set cable drop comp code\n");
		goto out;
	}
	ret = mt6375_run_pe(ddata, true);
out:
	mutex_unlock(&ddata->pe_lock);
	return ret;
}

static int mt6375_reset_pe_ta(struct charger_device *chgdev)
{
	int ret;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mutex_lock(&ddata->pe_lock);
	mt_dbg(ddata->dev, "++\n");
	ret = mt6375_chg_field_set(ddata, F_VMIVR, 4600);
	if (ret < 0)
		goto out;
	ret = mt6375_chg_field_set(ddata, F_IAICR, 100);
	if (ret < 0)
		goto out;
	msleep(250);
	ret = mt6375_chg_field_set(ddata, F_IAICR, 500);
out:
	mutex_unlock(&ddata->pe_lock);
	return ret;
}

static int mt6375_set_otg_cc(struct charger_device *chgdev, u32 uA)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mt_dbg(ddata->dev, "otg_cc=%d\n", uA);
	return mt6375_chg_field_set(ddata, F_OTG_CC, U_TO_M(uA));
}

static int mt6375_enable_otg(struct charger_device *chgdev, bool en)
{
	int ret;
	struct regulator *regulator;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mt_dbg(ddata->dev, "en=%d\n", en);
	regulator = devm_regulator_get(ddata->dev, "usb-otg-vbus");
	if (IS_ERR(regulator)) {
		dev_err(ddata->dev, "failed to get otg regulator\n");
		return PTR_ERR(regulator);
	}
	ret = en ? regulator_enable(regulator) : regulator_disable(regulator);
	devm_regulator_put(regulator);
	return ret;
}

static int mt6375_enable_discharge(struct charger_device *chgdev, bool en)
{
	int i, ret;
	u32 val;
	const int dischg_retry_cnt = 3;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mt_dbg(ddata->dev, "en=%d\n", en);
	ret = mt6375_enable_hm(ddata, true);
	if (ret < 0)
		return ret;

	ret = mt6375_chg_field_set(ddata, F_FORCE_VBUS_SINK, en);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set dischg %d\n", __func__, en);
		goto out;
	}

	/* for disable, make sure it is disabled */
	if (!en) {
		for (i = 0; i < dischg_retry_cnt; i++) {
			ret = mt6375_chg_field_get(ddata, F_FORCE_VBUS_SINK,
						   &val);
			if (ret < 0)
				continue;
			if (!val)
				break;
			mt6375_chg_field_set(ddata, F_FORCE_VBUS_SINK, 0);
		}
		if (i == dischg_retry_cnt) {
			dev_err(ddata->dev, "failed to disable dischg\n");
			ret = -EINVAL;
		}
	}
out:
	mt6375_enable_hm(ddata, false);
	return ret;
}

static int mt6375_enable_chg_type_det(struct charger_device *chgdev, bool en)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	int attach = en ? ATTACH_TYPE_TYPEC : ATTACH_TYPE_NONE;

	mt_dbg(ddata->dev, "en=%d\n", en);
	mt6375_chg_attach_pre_process(ddata, ATTACH_TRIG_TYPEC, attach);
	return 0;
}

#define DUMP_REG_BUF_SIZE	1024
static int mt6375_dump_registers(struct charger_device *chgdev)
{
	int i, ret;
	u32 val;
	u16 data;
	char buf[DUMP_REG_BUF_SIZE] = "\0";
	static struct {
		const char *name;
		const char *unit;
		enum mt6375_chg_reg_field fd;
	} settings[] = {
		{ .fd = F_CC, .name = "CC", .unit = "mA" },
		{ .fd = F_IAICR, .name = "AICR", .unit = "mA" },
		{ .fd = F_VMIVR, .name = "MIVR", .unit = "mV" },
		{ .fd = F_IEOC, .name = "IEOC", .unit = "mA" },
		{ .fd = F_CV, .name = "CV", .unit = "mV" },
	};
	static struct {
		const char *name;
		const char *unit;
		enum mt6375_adc_chan chan;
	} adcs[] = {
		{ .chan = ADC_CHAN_CHGVINDIV5, .name = "VBUS", .unit = "mV" },
		{ .chan = ADC_CHAN_IBUS, .name = "IBUS", .unit = "mA" },
		{ .chan = ADC_CHAN_VBAT, .name = "VBAT", .unit = "mV" },
		{ .chan = ADC_CHAN_IBAT, .name = "IBAT", .unit = "mA" },
		{ .chan = ADC_CHAN_VSYS, .name = "VSYS", .unit = "mV" },
	};
	static struct {
		const u16 reg;
		const char *name;
	} regs[] = {
		{ .reg = MT6375_REG_CHG_STAT, .name = "CHG_STAT" },
		{ .reg = MT6375_REG_CHG_STAT0, .name = "CHG_STAT0" },
		{ .reg = MT6375_REG_CHG_STAT1, .name = "CHG_STAT1" },
		{ .reg = MT6375_REG_CHG_TOP1, .name = "CHG_TOP1" },
		{ .reg = MT6375_REG_CHG_TOP2, .name = "CHG_TOP2" },
		{ .reg = MT6375_REG_CHG_EOC, .name = "CHG_EOC" },
	};
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	for (i = 0; i < ARRAY_SIZE(settings); i++) {
		ret = mt6375_chg_field_get(ddata, settings[i].fd, &val);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to get %s\n",
				settings[i].name);
			return ret;
		}
		if (i == ARRAY_SIZE(settings) - 1)
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = %d%s\n", settings[i].name, val,
				  settings[i].unit);
		else
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = %d%s, ", settings[i].name, val,
				  settings[i].unit);
	}

	for (i = 0; i < ARRAY_SIZE(adcs); i++) {
		ret = iio_read_channel_processed(&ddata->iio_adcs[adcs[i].chan],
						 &val);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to read adc %s\n",
				adcs[i].name);
			return ret;
		}
		val = U_TO_M(val);
		if (i == ARRAY_SIZE(adcs) - 1)
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = %d%s\n", adcs[i].name, val,
				  adcs[i].unit);
		else
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = %d%s, ", adcs[i].name, val,
				  adcs[i].unit);
	}

	/* No need to lock, it is only for information dump */
	if (ddata->batprotect_en) {
		ret = regmap_bulk_read(ddata->rmap, MT6375_REG_VBAT_MON_RPT,
				       &data, 2);
		if (ret < 0)
			dev_err(ddata->dev,
				"failed to get vbat monitor report\n");
		else {
			val = ADC_FROM_VBAT_RAW(be16_to_cpu(data));
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "VBATCELL = %dmV\n", val);
		}
	}

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_read(ddata->rmap, regs[i].reg, &val);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to read %s\n",
				regs[i].name);
			return ret;
		}
		if (i == ARRAY_SIZE(regs) - 1)
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = 0x%02X\n", regs[i].name, val);
		else
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = 0x%02X, ", regs[i].name, val);
	}
	dev_info(ddata->dev, "%s %s", __func__, buf);
	return 0;
}

static int mt6375_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(ddata->psy);
		break;
	default:
		break;
	}
	return 0;
}

static int mt6375_plug_in(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(ddata->dev);

	mt_dbg(ddata->dev, "++\n");
	if (pdata->wdt_en) {
		ret = mt6375_chg_field_set(ddata, F_WDT_EN, 1);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to enable WDT\n");
			return ret;
		}
	}
	return ret;
}

static int mt6375_plug_out(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(ddata->dev);

	mt_dbg(ddata->dev, "++\n");
	if (pdata->wdt_en) {
		ret = mt6375_chg_field_set(ddata, F_WDT_EN, 0);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to disable WDT\n");
			return ret;
		}
	}
	return 0;
}

static int mt6375_enable_6pin_battery_charging(struct charger_device *chgdev,
					       bool en)
{
	int ret = 0;
	u16 data, batend_code;
	u32 vbat, cv;
	struct mt6375_chg_data *ddata = charger_get_data(chgdev);

	mutex_lock(&ddata->cv_lock);
	if (ddata->batprotect_en == en)
		goto out;

	mt_dbg(ddata->dev, "en=%d\n", en);
	if (!en)
		goto dis_pro;

	ret = mt6375_chg_field_set(ddata, F_VBAT_MON_EN, 1);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to enable vbat monitor\n");
		goto out;
	}
	usleep_range(ADC_CONV_TIME_US * 2, ADC_CONV_TIME_US * 3);
	ret = regmap_bulk_read(ddata->rmap, MT6375_REG_VBAT_MON_RPT, &data, 2);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to get vbat monitor report\n");
		goto dis_mon;
	}
	vbat = ADC_FROM_VBAT_RAW(be16_to_cpu(data));
	ret = mt6375_chg_field_get(ddata, F_CV, &cv);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to get cv\n");
		goto dis_mon;
	}
	mt_dbg(ddata->dev, "vbat = %dmV, cv = %dmV\n", vbat, cv);
	if (vbat >= cv) {
		dev_warn(ddata->dev, "vbat(%d) >= cv(%d), should not start\n",
			 vbat, cv);
		goto dis_mon;
	}
	ret = mt6375_chg_field_set(ddata, F_BATINT, cv);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set batint\n");
		goto dis_mon;
	}
	batend_code = ADC_TO_VBAT_RAW(cv);
	batend_code = cpu_to_be16(batend_code);
	mt_dbg(ddata->dev, "batend code = 0x%04X\n", batend_code);
	ret = regmap_bulk_write(ddata->rmap, MT6375_REG_BATEND_CODE,
				&batend_code, 2);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set batend code\n");
		goto dis_mon;
	}
	ret = mt6375_chg_field_set(ddata, F_BATPROTECT_EN, 1);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to enable bat protect\n");
		goto dis_mon;
	}
	ret = mt6375_chg_field_set(ddata, F_CV,
				   mt6375_chg_fields[F_CV].range->max);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set maximum cv\n");
		goto dis_pro;
	}
	ddata->batprotect_en = true;
	mt_dbg(ddata->dev, "successfully\n");
	goto out;
dis_pro:
	if (mt6375_chg_field_set(ddata, F_BATPROTECT_EN, 0) < 0)
		dev_notice(ddata->dev, "failed to disable bat protect\n");
	if (mt6375_chg_field_set(ddata, F_CV, ddata->cv) < 0)
		dev_notice(ddata->dev, "failed to set cv\n");
dis_mon:
	if (mt6375_chg_field_set(ddata, F_VBAT_MON_EN, 0) < 0)
		dev_notice(ddata->dev, "failed to disable vbat monitor\n");
	ddata->batprotect_en = false;
out:
	mutex_unlock(&ddata->cv_lock);
	return ret;
}

static const struct charger_properties mt6375_chg_props = {
	.alias_name = "mt6375_chg",
};

static const struct charger_ops mt6375_chg_ops = {
	/* cable plug in/out */
	.plug_in = mt6375_plug_in,
	.plug_out = mt6375_plug_out,
	/* enable */
	.enable = mt6375_enable_charging,
	.is_enabled = mt6375_is_enabled,
	/* charging current */
	.set_charging_current = mt6375_set_ichg,
	.get_charging_current = mt6375_get_ichg,
	.get_min_charging_current = mt6375_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = mt6375_set_cv,
	.get_constant_voltage = mt6375_get_cv,
	/* input current limit */
	.set_input_current = mt6375_set_aicr,
	.get_input_current = mt6375_get_aicr,
	.get_min_input_current = mt6375_get_min_aicr,
	/* MIVR */
	.set_mivr = mt6375_set_mivr,
	.get_mivr = mt6375_get_mivr,
	.get_mivr_state = mt6375_get_mivr_state,
	/* ADC */
	.get_adc = mt6375_get_adc,
	.get_vbus_adc = mt6375_get_vbus,
	.get_ibus_adc = mt6375_get_ibus,
	.get_ibat_adc = mt6375_get_ibat,
	.get_tchg_adc = mt6375_get_tchg,
	.get_zcv = mt6375_get_zcv,
	/* charing termination */
	.set_eoc_current = mt6375_set_ieoc,
	.enable_termination = mt6375_enable_te,
	.reset_eoc_state = mt6375_reset_eoc_state,
	.safety_check = mt6375_sw_check_eoc,
	.is_charging_done = mt6375_is_charge_done,
	/* power path */
	.enable_powerpath = mt6375_enable_buck,
	.is_powerpath_enabled = mt6375_is_buck_enabled,
	/* timer */
	.enable_safety_timer = mt6375_enable_chg_timer,
	.is_safety_timer_enabled = mt6375_is_chg_timer_enabled,
	.kick_wdt = mt6375_kick_wdt,
	/* AICL */
	.run_aicl = mt6375_run_aicc,
	/* PE+/PE+20 */
	.send_ta_current_pattern = mt6375_set_pe_current_pattern,
	.set_pe20_efficiency_table = mt6375_set_pe20_efficiency_table,
	.send_ta20_current_pattern = mt6375_set_pe20_current_pattern,
	.reset_ta = mt6375_reset_pe_ta,
	.enable_cable_drop_comp = mt6375_enable_pe_cable_drop_comp,
	/* OTG */
	.set_boost_current_limit = mt6375_set_otg_cc,
	.enable_otg = mt6375_enable_otg,
	.enable_discharge = mt6375_enable_discharge,
	/* charger type detection */
	.enable_chg_type_det = mt6375_enable_chg_type_det,
	/* misc */
	.dump_registers = mt6375_dump_registers,
	.enable_hz = mt6375_enable_hz,
	/* event */
	.event = mt6375_do_event,
	/* 6pin battery */
	.enable_6pin_battery_charging = mt6375_enable_6pin_battery_charging,
};

static irqreturn_t mt6375_fl_wdt_handler(int irq, void *data)
{
	int ret;
	struct mt6375_chg_data *ddata = data;

	ret = mt6375_chg_field_set(ddata, F_WDT_RST, 1);
	if (ret < 0)
		dev_notice(ddata->dev, "failed to kick wdt\n");
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_pwr_rdy_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;

	mt_dbg(ddata->dev, "++\n");
	mt6375_chg_pwr_rdy_process(ddata);
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_detach_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;

	mt_dbg(ddata->dev, "++\n");
	mt6375_chg_pwr_rdy_process(ddata);
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_vbus_ov_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;

	mt_dbg(ddata->dev, "++\n");
	charger_dev_notify(ddata->chgdev, CHARGER_DEV_NOTIFY_VBUS_OVP);
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_chg_tout_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;

	mt_dbg(ddata->dev, "++\n");
	charger_dev_notify(ddata->chgdev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_bc12_dn_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;
	int attach;

	mt_dbg(ddata->dev, "++\n");
	mutex_lock(&ddata->attach_lock);
	ddata->bc12_dn = true;
	attach = atomic_read(&ddata->attach);
	mutex_unlock(&ddata->attach_lock);

	if (attach < ATTACH_TYPE_PD && !queue_work(ddata->wq, &ddata->bc12_work))
		dev_notice(ddata->dev, "%s bc12 work already queued\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_pe_done_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;

	mt_dbg(ddata->dev, "++\n");
	complete(&ddata->pe_done);
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_aicc_done_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;

	mt_dbg(ddata->dev, "++\n");
	complete(&ddata->aicc_done);
	return IRQ_HANDLED;
}

static irqreturn_t mt6375_fl_batpro_done_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;
	int ret;

	mt_dbg(ddata->dev, "++\n");
	ret = mt6375_enable_6pin_battery_charging(ddata->chgdev, false);
	charger_dev_notify(ddata->chgdev, CHARGER_DEV_NOTIFY_BATPRO_DONE);
	return ret < 0 ? ret : IRQ_HANDLED;
}

static irqreturn_t mt6375_adc_vbat_mon_ov_handler(int irq, void *data)
{
	struct mt6375_chg_data *ddata = data;
	u32 cv;

	mt6375_get_cv(ddata->chgdev, &cv);
	mt_dbg(ddata->dev, "cv = %dmV\n", U_TO_M(cv));
	return IRQ_HANDLED;
}

struct mt6375_chg_dtprop {
	const char *name;
	size_t offset;
	enum mt6375_chg_reg_field field;
	enum mt6375_chg_dtprop_type type;
	bool optional;
};

#define MT6375_CHG_DTPROP(_name, _field, _type, _opt) \
{ \
	.name = #_name, \
	.field = _field, \
	.type = _type, \
	.offset = offsetof(struct mt6375_chg_platform_data, _name), \
	.optional = _opt, \
}

static const struct mt6375_chg_dtprop mt6375_chg_dtprops[] = {
	MT6375_CHG_DTPROP(vbus_ov, F_VBUS_OV, DTPROP_U32, false),
	MT6375_CHG_DTPROP(chg_tmr, F_CHG_TMR, DTPROP_U32, false),
	MT6375_CHG_DTPROP(chg_tmr_en, F_CHG_TMR_EN, DTPROP_BOOL, false),
	MT6375_CHG_DTPROP(otg_lbp, F_OTG_LBP, DTPROP_U32, false),
	MT6375_CHG_DTPROP(ircmp_v, F_IRCMP_V, DTPROP_U32, false),
	MT6375_CHG_DTPROP(ircmp_r, F_IRCMP_R, DTPROP_U32, false),
	MT6375_CHG_DTPROP(wdt, F_WDT, DTPROP_U32, false),
	MT6375_CHG_DTPROP(wdt_en, F_WDT_EN, DTPROP_BOOL, false),
	MT6375_CHG_DTPROP(te_en, F_TE, DTPROP_BOOL, false),
	MT6375_CHG_DTPROP(mivr, F_VMIVR, DTPROP_U32, true),
	MT6375_CHG_DTPROP(aicr, F_IAICR, DTPROP_U32, true),
	MT6375_CHG_DTPROP(ichg, F_CC, DTPROP_U32, true),
	MT6375_CHG_DTPROP(ieoc, F_IEOC, DTPROP_U32, true),
	MT6375_CHG_DTPROP(cv, F_CV, DTPROP_U32, true),
	MT6375_CHG_DTPROP(vrec, F_VREC, DTPROP_U32, true),
	MT6375_CHG_DTPROP(dcdt_sel, F_DCDT_SEL, DTPROP_U32, true),
};

static inline u32 pdata_get_val(void *pdata, const struct mt6375_chg_dtprop *dp)
{
	if (dp->type == DTPROP_BOOL)
		return *((bool *)(pdata + dp->offset));
	return *((u32 *)(pdata + dp->offset));
}

static int mt6375_chg_apply_dt(struct mt6375_chg_data *ddata)
{
	int i, ret;
	u32 val;
	const struct mt6375_chg_dtprop *dp;

	mt_dbg(ddata->dev, "++\n");
	for (i = 0; i < ARRAY_SIZE(mt6375_chg_dtprops); i++) {
		dp = &mt6375_chg_dtprops[i];
		val = pdata_get_val(dev_get_platdata(ddata->dev), dp);
		ret = mt6375_chg_field_set(ddata, dp->field, val);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to write dtprop %s\n",
				dp->name);
			if (!dp->optional)
				return ret;
		}
	}
	return 0;
}

static void mt6375_chg_parse_dt_helper(struct device *dev, void *pdata,
				       const struct mt6375_chg_dtprop *dp)
{
	int ret;
	void *val = pdata + dp->offset;

	if (dp->type == DTPROP_BOOL)
		*((bool *)val) = device_property_read_bool(dev, dp->name);
	else {
		ret = device_property_read_u32(dev, dp->name, val);
		if (ret < 0)
			dev_info(dev, "property %s not found\n", dp->name);
	}
}

static int mt6375_chg_get_pdata(struct device *dev)
{
	int i;
	u32 val;
	const struct {
		u32 size;
		u32 tag;
		u32 boot_mode;
		u32 boot_type;
	} *tag;
	struct device_node *bc12_np, *boot_np, *np = dev->of_node;
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(dev);

	mt_dbg(dev, "%s\n", __func__);
	if (np) {
		pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &mt6375_chg_pdata_def, sizeof(*pdata));
		for (i = 0; i < ARRAY_SIZE(mt6375_chg_dtprops); i++)
			mt6375_chg_parse_dt_helper(dev, pdata,
						   &mt6375_chg_dtprops[i]);
		pdata->usb_killer_detect =
			device_property_read_bool(dev, "usb_killer_detect");

		/* mediatek chgdev name */
		if (of_property_read_string(np, "chg_name", &pdata->chg_name))
			dev_notice(dev, "failed to get chg_name\n");

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

		/*
		 * mediatek bc12_sel
		 * 0 means bc12 owner is THIS_MODULE,
		 * if it is not 0, always ignore
		 */
		bc12_np = of_parse_phandle(np, "bc12_sel", 0);
		if (!bc12_np) {
			dev_err(dev, "failed to get bc12_sel phandle\n");
			return -ENODEV;
		}
		if (of_property_read_u32(bc12_np, "bc12_sel", &val) < 0) {
			dev_err(dev, "property bc12_sel not found\n");
			return -EINVAL;
		}
		if (val != 0)
			pdata->attach_trig = ATTACH_TRIG_IGNORE;
		else if (IS_ENABLED(CONFIG_TCPC_CLASS))
			pdata->attach_trig = ATTACH_TRIG_TYPEC;
		else
			pdata->attach_trig = ATTACH_TRIG_PWR_RDY;

		dev->platform_data = pdata;
	}
	return pdata ? 0 : -ENODEV;
}

static int mt6375_chg_init_setting(struct mt6375_chg_data *ddata)
{
	int ret;
	u32 val;
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(ddata->dev);

	mt_dbg(ddata->dev, "%s\n", __func__);
	ret = mt6375_chg_field_set(ddata, F_AICC_ONESHOT, 1);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to set aicc oneshot\n");
		return ret;
	}

	ret = mt6375_chg_field_set(ddata, F_BC12_EN, 0);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to disable bc12\n");
		return ret;
	}

	/* set aicr = 200mA in 1:META_BOOT 5:ADVMETA_BOOT */
	if (pdata->boot_mode == 1 || pdata->boot_mode == 5) {
		ret = mt6375_chg_field_set(ddata, F_IAICR, 200);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to set aicr 200mA\n");
			return ret;
		}
	}

	ret = mt6375_chg_apply_dt(ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to apply dt property\n");
		return ret;
	}

	ret = mt6375_chg_field_set(ddata, F_ILIM_EN, 0);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to disable ilim\n");
		return ret;
	}

	/*
	 * disable wdt to save 1mA power consumption
	 * it will be turned back on later
	 * if it is enabled in dt property and TA attached
	 */
	ret = mt6375_chg_field_set(ddata, F_WDT_EN, 0);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to disable WDT\n");
		return ret;
	}

	/* if get failed, just ignore it */
	ret = mt6375_chg_field_get(ddata, F_PP_PG_FLAG, &val);
	if (ret >= 0 && val)
		dev_warn(ddata->dev, "BATSYSUV occurred\n");
	return mt6375_chg_field_set(ddata, F_PP_PG_FLAG, 1);
}

static int mt6375_chg_get_iio_adc(struct mt6375_chg_data *ddata)
{
	int ret;
	u16 zcv;

	mt_dbg(ddata->dev, "%s\n", __func__);
	ddata->iio_adcs = devm_iio_channel_get_all(ddata->dev);
	if (IS_ERR(ddata->iio_adcs))
		return PTR_ERR(ddata->iio_adcs);
	ret = regmap_bulk_read(ddata->rmap, MT6375_REG_ADC_ZCV_RPT, &zcv, 2);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to get zcv\n");
		return ret;
	}
	ddata->zcv = ADC_FROM_VBAT_RAW(be16_to_cpu(zcv));
	return 0;
}

static int mt6375_chg_init_psy(struct mt6375_chg_data *ddata)
{
	struct power_supply_config cfg = {
		.drv_data = ddata,
		.of_node = ddata->dev->of_node,
		.supplied_to = mt6375_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(mt6375_psy_supplied_to),
	};

	mt_dbg(ddata->dev, "%s\n", __func__);
	memcpy(&ddata->psy_desc, &mt6375_psy_desc, sizeof(ddata->psy_desc));
	ddata->psy_desc.name = dev_name(ddata->dev);
	ddata->psy = devm_power_supply_register(ddata->dev, &ddata->psy_desc,
						&cfg);
	return IS_ERR(ddata->psy) ? PTR_ERR(ddata->psy) : 0;
}

static int mt6375_chg_init_regulator(struct mt6375_chg_data *ddata)
{
	struct regulator_config cfg = {
		.dev = ddata->dev,
		.driver_data = ddata,
		.regmap = ddata->rmap,
	};

	mt_dbg(ddata->dev, "%s\n", __func__);
	memcpy(&ddata->rdesc, &mt6375_chg_otg_rdesc, sizeof(ddata->rdesc));
	ddata->rdesc.name = dev_name(ddata->dev);
	ddata->rdev = devm_regulator_register(ddata->dev, &ddata->rdesc, &cfg);
	return IS_ERR(ddata->rdev) ? PTR_ERR(ddata->rdev) : 0;
}

static int mt6375_chg_init_chgdev(struct mt6375_chg_data *ddata)
{
	struct mt6375_chg_platform_data *pdata = dev_get_platdata(ddata->dev);

	mt_dbg(ddata->dev, "%s\n", __func__);
	ddata->chgdev = charger_device_register(pdata->chg_name, ddata->dev,
						ddata, &mt6375_chg_ops,
						&mt6375_chg_props);
	return IS_ERR(ddata->chgdev) ? PTR_ERR(ddata->chgdev) : 0;
}

#define MT6375_CHG_IRQ(_name) \
{ \
	.name = #_name, \
	.hdlr = mt6375_##_name##_handler, \
}

static int mt6375_chg_init_irq(struct mt6375_chg_data *ddata)
{
	int i, ret;
	const struct {
		char *name;
		irq_handler_t hdlr;
	} mt6375_chg_irqs[] = {
		MT6375_CHG_IRQ(fl_wdt),
		MT6375_CHG_IRQ(fl_pwr_rdy),
		MT6375_CHG_IRQ(fl_vbus_ov),
		MT6375_CHG_IRQ(fl_chg_tout),
		MT6375_CHG_IRQ(fl_detach),
		MT6375_CHG_IRQ(fl_bc12_dn),
		MT6375_CHG_IRQ(fl_pe_done),
		MT6375_CHG_IRQ(fl_aicc_done),
		MT6375_CHG_IRQ(fl_batpro_done),
		MT6375_CHG_IRQ(adc_vbat_mon_ov),
	};

	mt_dbg(ddata->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mt6375_chg_irqs); i++) {
		ret = platform_get_irq_byname(to_platform_device(ddata->dev),
					      mt6375_chg_irqs[i].name);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to get irq %s\n",
				mt6375_chg_irqs[i].name);
			return ret;
		}
		ret = devm_request_threaded_irq(ddata->dev, ret, NULL,
						mt6375_chg_irqs[i].hdlr,
						IRQF_ONESHOT,
						dev_name(ddata->dev), ddata);
		if (ret < 0) {
			dev_err(ddata->dev, "failed to request irq %s\n",
				mt6375_chg_irqs[i].name);
			return ret;
		}
	}
	return 0;
}

static int mt6375_set_shipping_mode(struct mt6375_chg_data *ddata)
{
	int ret;

	ret = mt6375_chg_field_set(ddata, F_SHIP_RST_DIS, 1);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to disable ship reset\n");
		return ret;
	}

	ret = mt6375_chg_field_set(ddata, F_BATFET_DISDLY, 0);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to disable ship mode delay\n");
		return ret;
	}

	ret = mt6375_chg_field_set(ddata, F_BUCK_EN, 0);
	if (ret < 0) {
		dev_notice(ddata->dev, "failed to disable chg buck en\n");
		return ret;
	}

	return regmap_update_bits(ddata->rmap, MT6375_REG_CHG_TOP1,
				  MT6375_MSK_BATFET_DIS, 0xFF);
}

static ssize_t shipping_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	unsigned long magic;
	struct mt6375_chg_data *ddata = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &magic);
	if (ret < 0) {
		dev_warn(dev, "parsing number fail\n");
		return ret;
	}
	if (magic != 5526789)
		return -EINVAL;
	ret = mt6375_set_shipping_mode(ddata);
	return ret < 0 ? ret : count;
}
static const DEVICE_ATTR_WO(shipping_mode);

static int mt6375_chg_probe(struct platform_device *pdev)
{
	int i, ret;
	struct mt6375_chg_data *ddata;
	struct device *dev = &pdev->dev;
	const struct mt6375_chg_field *fds = mt6375_chg_fields;

	dev_info(dev, "%s\n", __func__);
	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	ddata->rmap = dev_get_regmap(dev->parent, NULL);
	if (!ddata->rmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}

	for (i = 0; i < F_MAX; i++) {
		ddata->rmap_fields[i] = devm_regmap_field_alloc(dev,
								ddata->rmap,
								fds[i].field);
		if (IS_ERR(ddata->rmap_fields[i])) {
			dev_err(dev, "failed to allocate regmap field\n");
			return PTR_ERR(ddata->rmap_fields[i]);
		}
	}

	ret = mt6375_chg_get_pdata(dev);
	if (ret < 0) {
		dev_err(dev, "failed to get platform data\n");
		return ret;
	}

	ddata->dev = dev;
	init_completion(&ddata->pe_done);
	init_completion(&ddata->aicc_done);
	mutex_init(&ddata->attach_lock);
	mutex_init(&ddata->pe_lock);
	mutex_init(&ddata->cv_lock);
	mutex_init(&ddata->hm_lock);
	atomic_set(&ddata->attach, 0);
	atomic_set(&ddata->eoc_cnt, 0);
	ddata->wq = create_singlethread_workqueue(dev_name(dev));
	if (!ddata->wq) {
		dev_err(dev, "failed to create workqueue\n");
		ret = -ENOMEM;
		goto out;
	}
	INIT_WORK(&ddata->bc12_work, mt6375_chg_bc12_work_func);
	platform_set_drvdata(pdev, ddata);

	ret = device_create_file(dev, &dev_attr_shipping_mode);
	if (ret < 0) {
		dev_err(dev, "failed to create shipping mode attribute\n");
		goto out_wq;
	}

	ret = mt6375_chg_init_setting(ddata);
	if (ret < 0) {
		dev_err(dev, "failed to init setting\n");
		goto out_attr;
	}

	ret = mt6375_chg_get_iio_adc(ddata);
	if (ret < 0) {
		dev_err(dev, "failed to get iio adc\n");
		goto out_attr;
	}

	ret = mt6375_chg_init_psy(ddata);
	if (ret < 0) {
		dev_err(dev, "failed to init power supply\n");
		goto out_attr;
	}

	ret = mt6375_chg_init_regulator(ddata);
	if (ret < 0) {
		dev_err(dev, "failed to init regulator\n");
		goto out_attr;
	}

	ret = mt6375_chg_init_chgdev(ddata);
	if (ret < 0) {
		dev_err(dev, "failed to init chgdev\n");
		goto out_attr;
	}

	ret = mt6375_chg_init_irq(ddata);
	if (ret < 0) {
		dev_err(dev, "failed to init irq\n");
		goto out_chgdev;
	}
	mt6375_chg_pwr_rdy_process(ddata);
	mt_dbg(dev, "successfully\n");
	return 0;
out_chgdev:
	charger_device_unregister(ddata->chgdev);
out_attr:
	device_remove_file(ddata->dev, &dev_attr_shipping_mode);
out_wq:
	destroy_workqueue(ddata->wq);
out:
	mutex_destroy(&ddata->hm_lock);
	mutex_destroy(&ddata->cv_lock);
	mutex_destroy(&ddata->pe_lock);
	mutex_destroy(&ddata->attach_lock);
	return ret;
}

static int mt6375_chg_remove(struct platform_device *pdev)
{
	struct mt6375_chg_data *ddata = platform_get_drvdata(pdev);

	mt_dbg(&pdev->dev, "%s\n", __func__);
	if (ddata) {
		charger_device_unregister(ddata->chgdev);
		device_remove_file(ddata->dev, &dev_attr_shipping_mode);
		destroy_workqueue(ddata->wq);
		mutex_destroy(&ddata->hm_lock);
		mutex_destroy(&ddata->cv_lock);
		mutex_destroy(&ddata->pe_lock);
		mutex_destroy(&ddata->attach_lock);
	}
	return 0;
}

static const struct of_device_id __maybe_unused mt6375_chg_of_match[] = {
	{ .compatible = "mediatek,mt6375-chg", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6375_chg_of_match);

static struct platform_driver mt6375_chg_driver = {
	.probe = mt6375_chg_probe,
	.remove = mt6375_chg_remove,
	.driver = {
		.name = "mt6375-chg",
		.of_match_table = of_match_ptr(mt6375_chg_of_match),
	},
};
module_platform_driver(mt6375_chg_driver);

MODULE_AUTHOR("ShuFan Lee <shufan_lee@richtek.com>");
MODULE_DESCRIPTION("MT6375 Charger Driver");
MODULE_LICENSE("GPL v2");
