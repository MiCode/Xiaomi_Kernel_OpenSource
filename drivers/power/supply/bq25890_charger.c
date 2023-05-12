// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * TI BQ25890 charger driver
 *
 * Copyright (C) 2015 Intel Corporation
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/qti_power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/types.h>
#include <linux/iio/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include "../../regulator/internal.h"

#include <linux/acpi.h>
#include <linux/of.h>
#include "sm5602_fg.h"
#include "bq25890.h"

#define BQ25890_MANUFACTURER		"Texas Instruments"
#define BQ25890_IRQ_PIN			"bq25890_irq"
#define BQ25890_REG_NUM			21

#define BQ25890_ID			3
#define BQ25895_ID			7
#define BQ25896_ID			0
#define SC8989X_ID			4
#define SYV690_ID			1

extern int msm_hsphy_dpdm_regulator_enable(struct regulator_dev *rdev);
extern int msm_hsphy_dpdm_regulator_disable(struct regulator_dev *rdev);
extern int get_usb_onlie_state(void);

int input_suspend_flag = 0;
static int sw_charger_chip_id = 0;
#define min_charger_voltage_1		4000
#define min_charger_voltage_2		4300

enum device_pn {
	SYV690 = 1,
	bq25890H = 3,
	SC89890H = 4,
};

enum bq2589x_vbus_type {
	BQ2589X_VBUS_NONE,
	BQ2589X_VBUS_USB_SDP,
	BQ2589X_VBUS_USB_CDP, /*CDP for bq25890, Adapter for bq25892*/
	BQ2589X_VBUS_USB_DCP,
	BQ2589X_VBUS_MAXC,
	BQ2589X_VBUS_UNKNOWN,
	BQ2589X_VBUS_NONSTAND,
	BQ2589X_VBUS_OTG,
	BQ2589X_VBUS_TYPE_NUM,
};

enum {
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
	QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB,
};

#define POWER_SUPPLY_PD_INACTIVE	QTI_POWER_SUPPLY_PD_INACTIVE
#define POWER_SUPPLY_PD_ACTIVE		QTI_POWER_SUPPLY_PD_ACTIVE
#define POWER_SUPPLY_PD_PPS_ACTIVE	QTI_POWER_SUPPLY_PD_PPS_ACTIVE
#define POWER_SUPPLY_TYPE_OTG_ENABLE		7
#define POWER_SUPPLY_TYPE_OTG_DISABLE		8

extern void power_supply_unregister(struct power_supply *psy);
extern struct power_supply *__must_check power_supply_register(struct device *parent,
		const struct power_supply_desc *desc, const struct power_supply_config *cfg);

static int typec_headphone_mode= 0;

static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5","USB_FLOAT","HVDCP_3"
};

static const char * const power_supply_usb_type_text[] = {
	"Unknown", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "PD_DRP", "PD_PPS", "BrickID", "USB_HVDCP",
	"USB_HVDCP3","USB_HVDCP3P5", "USB_FLOAT"
};


struct quick_charge adapter_cap[10] = {
	{ POWER_SUPPLY_TYPE_USB,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_WIRELESS,     QUICK_CHARGE_FAST },
	{0, 0},
};

struct bq25890_device *g_bq;

struct bq25890_iio_prop_channels {
	const char *datasheet_name;
	int channel_no;
	enum iio_chan_type type;
	long info_mask;
};

#define BQ25890_CHAN(_dname, _chno, _type, _mask)			\
	{								\
		.datasheet_name = _dname,				\
		.channel_no = _chno,					\
		.type = _type,						\
		.info_mask = _mask,					\
	},

#define BQ25890_CHAN_INDEX(_dname, _chno)					\
	BQ25890_CHAN(_dname, _chno, IIO_INDEX,			\
		  BIT(IIO_CHAN_INFO_RAW))

#define BQ25890_CHAN_PD_CURRENT(_dname, _chno)					\
	BQ25890_CHAN(_dname, _chno, IIO_CURRENT,			\
		  BIT(IIO_CHAN_INFO_PROCESSED))

#define BQ25890_CHAN_PD_VOLTAGE(_dname, _chno)				\
	BQ25890_CHAN(_dname, _chno, IIO_VOLTAGE,				\
		  BIT(IIO_CHAN_INFO_PROCESSED))

enum iio_type {
	BQ25890,
};

enum bq25890_iio_channels { 
	TYPEC_CC_ORIENTATION = 0,
	TYPEC_MODE,
	APDO_MAX_VOLT,
	APDO_MAX_CURR,
};

static const char * const bq25890_iio_chan[] = {
	[TYPEC_CC_ORIENTATION] = "typec_cc_orientation",
	[TYPEC_MODE] = "typec_mode",
	[APDO_MAX_VOLT] = "apdo_max_volt",
	[APDO_MAX_CURR] = "apdo_max_curr",
};

static const struct bq25890_iio_prop_channels bq25890_chans[] = {
	BQ25890_CHAN_INDEX("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	BQ25890_CHAN_INDEX("typec_mode", PSY_IIO_TYPEC_MODE)
	BQ25890_CHAN_PD_VOLTAGE("apdo_max_volt", PSY_IIO_APDO_MAX_VOLT)
	BQ25890_CHAN_PD_CURRENT("apdo_max_curr", PSY_IIO_APDO_MAX_CURR)
	BQ25890_CHAN_INDEX("25890_ship_mode", PSY_IIO_SET_SHIP_MODE)
};

static const struct regmap_range bq25890_readonly_reg_ranges[] = {
	regmap_reg_range(0x0b, 0x0c),
	regmap_reg_range(0x0e, 0x13),
};

static const struct regmap_access_table bq25890_writeable_regs = {
	.no_ranges = bq25890_readonly_reg_ranges,
	.n_no_ranges = ARRAY_SIZE(bq25890_readonly_reg_ranges),
};

static const struct regmap_range bq25890_volatile_reg_ranges[] = {
	regmap_reg_range(0x00, 0x00),
	regmap_reg_range(0x09, 0x09),
	regmap_reg_range(0x0b, 0x0c),
	regmap_reg_range(0x0e, 0x14),
};

static const struct regmap_access_table bq25890_volatile_regs = {
	.yes_ranges = bq25890_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(bq25890_volatile_reg_ranges),
};

static const struct regmap_config bq25890_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0x14,
	//.cache_type = REGCACHE_RBTREE,

	.wr_table = &bq25890_writeable_regs,
	.volatile_table = &bq25890_volatile_regs,
};

static const struct reg_field bq25890_reg_fields[] = {
	/* REG00 */
	[F_EN_HIZ]		= REG_FIELD(0x00, 7, 7),
	[F_EN_ILIM]		= REG_FIELD(0x00, 6, 6),
	[F_IILIM]		= REG_FIELD(0x00, 0, 5),
	/* REG01 */
	[F_BHOT]		= REG_FIELD(0x01, 6, 7),
	[F_BCOLD]		= REG_FIELD(0x01, 5, 5),
	[F_VINDPM_OFS]		= REG_FIELD(0x01, 0, 4),
	/* REG02 */
	[F_CONV_START]		= REG_FIELD(0x02, 7, 7),
	[F_CONV_RATE]		= REG_FIELD(0x02, 6, 6),
	[F_BOOSTF]		= REG_FIELD(0x02, 5, 5),
	[F_ICO_EN]		= REG_FIELD(0x02, 4, 4),
	[F_HVDCP_EN]		= REG_FIELD(0x02, 3, 3),  // reserved on BQ25896
	[F_MAXC_EN]		= REG_FIELD(0x02, 2, 2),  // reserved on BQ25896
	[F_FORCE_DPM]		= REG_FIELD(0x02, 1, 1),
	[F_AUTO_DPDM_EN]	= REG_FIELD(0x02, 0, 0),
	/* REG03 */
	[F_BAT_LOAD_EN]		= REG_FIELD(0x03, 7, 7),
	[F_WD_RST]		= REG_FIELD(0x03, 6, 6),
	[F_OTG_CFG]		= REG_FIELD(0x03, 5, 5),
	[F_CHG_CFG]		= REG_FIELD(0x03, 4, 4),
	[F_SYSVMIN]		= REG_FIELD(0x03, 1, 3),
	/* MIN_VBAT_SEL on BQ25896 */
	/* REG04 */
	[F_PUMPX_EN]		= REG_FIELD(0x04, 7, 7),
	[F_ICHG]		= REG_FIELD(0x04, 0, 6),
	/* REG05 */
	[F_IPRECHG]		= REG_FIELD(0x05, 4, 7),
	[F_ITERM]		= REG_FIELD(0x05, 0, 3),
	/* REG06 */
	[F_VREG]		= REG_FIELD(0x06, 2, 7),
	[F_BATLOWV]		= REG_FIELD(0x06, 1, 1),
	[F_VRECHG]		= REG_FIELD(0x06, 0, 0),
	/* REG07 */
	[F_TERM_EN]		= REG_FIELD(0x07, 7, 7),
	[F_STAT_DIS]		= REG_FIELD(0x07, 6, 6),
	[F_WD]			= REG_FIELD(0x07, 4, 5),
	[F_TMR_EN]		= REG_FIELD(0x07, 3, 3),
	[F_CHG_TMR]		= REG_FIELD(0x07, 1, 2),
	[F_JEITA_ISET]		= REG_FIELD(0x07, 0, 0), // reserved on BQ25895
	/* REG08 */
	[F_BATCMP]		= REG_FIELD(0x08, 5, 7),
	[F_VCLAMP]		= REG_FIELD(0x08, 2, 4),
	[F_TREG]		= REG_FIELD(0x08, 0, 1),
	/* REG09 */
	[F_FORCE_ICO]		= REG_FIELD(0x09, 7, 7),
	[F_TMR2X_EN]		= REG_FIELD(0x09, 6, 6),
	[F_BATFET_DIS]		= REG_FIELD(0x09, 5, 5),
	[F_JEITA_VSET]		= REG_FIELD(0x09, 4, 4), // reserved on BQ25895
	[F_BATFET_DLY]		= REG_FIELD(0x09, 3, 3),
	[F_BATFET_RST_EN]	= REG_FIELD(0x09, 2, 2),
	[F_PUMPX_UP]		= REG_FIELD(0x09, 1, 1),
	[F_PUMPX_DN]		= REG_FIELD(0x09, 0, 0),
	/* REG0A */
	[F_BOOSTV]		= REG_FIELD(0x0A, 4, 7),
	/* PFM_OTG_DIS 3 on BQ25896 */
	[F_BOOSTI]		= REG_FIELD(0x0A, 0, 2), // reserved on BQ25895
	/* REG0B */
	[F_VBUS_STAT]		= REG_FIELD(0x0B, 5, 7),
	[F_CHG_STAT]		= REG_FIELD(0x0B, 3, 4),
	[F_PG_STAT]		= REG_FIELD(0x0B, 2, 2),
	[F_SDP_STAT]		= REG_FIELD(0x0B, 1, 1), // reserved on BQ25896
	[F_VSYS_STAT]		= REG_FIELD(0x0B, 0, 0),
	/* REG0C */
	[F_WD_FAULT]		= REG_FIELD(0x0C, 7, 7),
	[F_BOOST_FAULT]		= REG_FIELD(0x0C, 6, 6),
	[F_CHG_FAULT]		= REG_FIELD(0x0C, 4, 5),
	[F_BAT_FAULT]		= REG_FIELD(0x0C, 3, 3),
	[F_NTC_FAULT]		= REG_FIELD(0x0C, 0, 2),
	/* REG0D */
	[F_FORCE_VINDPM]	= REG_FIELD(0x0D, 7, 7),
	[F_VINDPM]		= REG_FIELD(0x0D, 0, 6),
	/* REG0E */
	[F_THERM_STAT]		= REG_FIELD(0x0E, 7, 7),
	[F_BATV]		= REG_FIELD(0x0E, 0, 6),
	/* REG0F */
	[F_SYSV]		= REG_FIELD(0x0F, 0, 6),
	/* REG10 */
	[F_TSPCT]		= REG_FIELD(0x10, 0, 6),
	/* REG11 */
	[F_VBUS_GD]		= REG_FIELD(0x11, 7, 7),
	[F_VBUSV]		= REG_FIELD(0x11, 0, 6),
	/* REG12 */
	[F_ICHGR]		= REG_FIELD(0x12, 0, 6),
	/* REG13 */
	[F_VDPM_STAT]		= REG_FIELD(0x13, 7, 7),
	[F_IDPM_STAT]		= REG_FIELD(0x13, 6, 6),
	[F_IDPM_LIM]		= REG_FIELD(0x13, 0, 5),
	/* REG14 */
	[F_REG_RST]		= REG_FIELD(0x14, 7, 7),
	[F_ICO_OPTIMIZED]	= REG_FIELD(0x14, 6, 6),
	[F_PN]			= REG_FIELD(0x14, 3, 5),
	[F_TS_PROFILE]		= REG_FIELD(0x14, 2, 2),
	[F_DEV_REV]		= REG_FIELD(0x14, 0, 1)
};

/*
 * Most of the val -> idx conversions can be computed, given the minimum,
 * maximum and the step between values. For the rest of conversions, we use
 * lookup tables.
 */
enum bq25890_table_ids {
	/* range tables */
	TBL_ICHG,
	TBL_ITERM,
	TBL_VREG,
	TBL_BOOSTV,
	TBL_SYSVMIN,

	/* lookup tables */
	TBL_TREG,
	TBL_BOOSTI,
};

/* Thermal Regulation Threshold lookup table, in degrees Celsius */
static const u32 bq25890_treg_tbl[] = { 60, 80, 100, 120 };

#define BQ25890_TREG_TBL_SIZE		ARRAY_SIZE(bq25890_treg_tbl)

/* Boost mode current limit lookup table, in uA */
static const u32 bq25890_boosti_tbl[] = {
	500000, 700000, 1100000, 1300000, 1600000, 1800000, 2100000, 2400000
};

#define BQ25890_BOOSTI_TBL_SIZE		ARRAY_SIZE(bq25890_boosti_tbl)

struct bq25890_range {
	u32 min;
	u32 max;
	u32 step;
};

struct bq25890_lookup {
	const u32 *tbl;
	u32 size;
};

static const union {
	struct bq25890_range  rt;
	struct bq25890_lookup lt;
} bq25890_tables[] = {
	/* range tables */
	[TBL_ICHG] =	{ .rt = {0,	  5056000, 64000} },	 /* uA */
	[TBL_ITERM] =	{ .rt = {64000,   1024000, 64000} },	 /* uA */
	[TBL_VREG] =	{ .rt = {3840000, 4608000, 16000} },	 /* uV */
	[TBL_BOOSTV] =	{ .rt = {4550000, 5510000, 64000} },	 /* uV */
	[TBL_SYSVMIN] = { .rt = {3000000, 3700000, 100000} },	 /* uV */

	/* lookup tables */
	[TBL_TREG] =	{ .lt = {bq25890_treg_tbl, BQ25890_TREG_TBL_SIZE} },
	[TBL_BOOSTI] =	{ .lt = {bq25890_boosti_tbl, BQ25890_BOOSTI_TBL_SIZE} }
},
sc8989x_tables[] = {
	/* range tables */
	[TBL_ICHG] =	{ .rt = {0,	  5056000, 60000} },	 /* uA */
	[TBL_ITERM] =	{ .rt = {30000,   960000,  60000} },	 /* uA */
	[TBL_VREG] =	{ .rt = {3840000, 4856000, 16000} },	 /* uV */
	[TBL_BOOSTV] =	{ .rt = {3900000, 5400000, 100000} },	 /* uV */
	[TBL_SYSVMIN] = { .rt = {3000000, 3700000, 100000} },	 /* uV */

	/* lookup tables */
	[TBL_TREG] =	{ .lt = {bq25890_treg_tbl, BQ25890_TREG_TBL_SIZE} },
	[TBL_BOOSTI] =	{ .lt = {bq25890_boosti_tbl, BQ25890_BOOSTI_TBL_SIZE} }
};

static int get_board_temp(struct bq25890_device *bq,
				 int *val);

extern struct sm_fg_chip *sm2;

#ifdef SOC_DECIMAL_2_POINT
extern int fg_get_soc_decimal_rate(void);

extern int fg_get_soc_decimal(void);
#endif

static bool is_mtbf_mode;

static int bq25890_field_read(struct bq25890_device *bq,
			      enum bq25890_fields field_id)
{
	int ret;
	int val;

	ret = regmap_field_read(bq->rmap_fields[field_id], &val);
	if (ret < 0)
		return ret;

	return val;
}

static int bq25890_field_write(struct bq25890_device *bq,
			       enum bq25890_fields field_id, u8 val)
{
	return regmap_field_write(bq->rmap_fields[field_id], val);
}

static u8 bq25890_find_idx(struct bq25890_device *bq,u32 value, enum bq25890_table_ids id)
{
	u8 idx;
  
	if (id >= TBL_TREG) {
		const u32 *tbl;
		u32 tbl_size;

		if (bq->chip_id == SC8989X_ID) {
			tbl = sc8989x_tables[id].lt.tbl;
			tbl_size = sc8989x_tables[id].lt.size;
		} else {
			tbl = bq25890_tables[id].lt.tbl;
			tbl_size = bq25890_tables[id].lt.size;
		}

		for (idx = 1; idx < tbl_size && tbl[idx] <= value; idx++)
			;
	} else {
		const struct bq25890_range *rtbl;
		u8 rtbl_size;

		if (bq->chip_id == SC8989X_ID) {
			rtbl = &sc8989x_tables[id].rt;
		} else {
			rtbl = &bq25890_tables[id].rt;
		}

		rtbl_size = (rtbl->max - rtbl->min) / rtbl->step + 1;

		for (idx = 1;
		     idx < rtbl_size && (idx * rtbl->step + rtbl->min <= value);
		     idx++)
			;
	}

	return idx - 1;
}

static u32 bq25890_find_val(u8 idx, enum bq25890_table_ids id)
{
	const struct bq25890_range *rtbl;

	/* lookup table? */
	if (id >= TBL_TREG)
		return bq25890_tables[id].lt.tbl[idx];

	/* range table */
	rtbl = &bq25890_tables[id].rt;

	return (rtbl->min + idx * rtbl->step);
}

enum bq25890_status {
	STATUS_NOT_CHARGING,
	STATUS_PRE_CHARGING,
	STATUS_FAST_CHARGING,
	STATUS_TERMINATION_DONE,
};

enum bq25890_chrg_fault {
	CHRG_FAULT_NORMAL,
	CHRG_FAULT_INPUT,
	CHRG_FAULT_THERMAL_SHUTDOWN,
	CHRG_FAULT_TIMER_EXPIRED,
};

static int bq25890_set_ship_mode(struct bq25890_device *bq, int val)
{
	int ret;

	if((val < 0) || (val > 1)) {
		return -ENOMEM;
	}

	ret = bq25890_field_write(bq, F_BATFET_DIS, val);
	if (ret < 0) {
		dev_dbg(bq->dev, "set ship mode failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int bq25890_get_ship_mode(struct bq25890_device *bq,
	int *val)
{
	int ret;

	ret = bq25890_field_read(bq, F_BATFET_DIS);
	if (ret < 0) {
		dev_dbg(bq->dev, "get ship mode failed %d\n", ret);
		return ret;
	}

	*val = ret;
	return 0;
}

static int bq25890_read(struct bq25890_device *bq, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static void bq25890_dump_register(struct bq25890_device *bq)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < BQ25890_REG_NUM; i++) {
		ret = bq25890_read(bq, i, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", i, reg_val);
			idx += len;
		}
	}

	dev_err(bq->dev, "%s: %s", __func__, buf);
}

int bq25890_charging_term_en(int val)
{
	int ret = 0;
	ret = bq25890_field_write(g_bq, F_TERM_EN, val);
	if (ret < 0) {
		dev_err(g_bq->dev, "set 25890 charging termination failed %d\n", ret);
		return ret;
	}
	dev_err(g_bq->dev, "set term_en = %d\n", val);

	return ret;
}
EXPORT_SYMBOL(bq25890_charging_term_en);

int bq25890_charger_start_charge(struct bq25890_device *bq)
{
	int ret;
	ret = bq25890_field_write(bq, F_CHG_CFG, 1);
	if (ret < 0) {
		dev_err(bq->dev, "set 25890 charge enable failed %d\n", ret);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(bq25890_charger_start_charge);

int bq25890_charger_stop_charge(struct bq25890_device *bq)
{
	int ret;
	ret = bq25890_field_write(bq, F_CHG_CFG, 0);
	if (ret < 0) {
		dev_err(bq->dev, "set 25890 charge disable failed %d\n", ret);
	}

	return ret;
}
EXPORT_SYMBOL(bq25890_charger_stop_charge);

static int bq25890_charger_set_status(struct bq25890_device *bq,
	int val)
{
	int ret = 0;
	if (!val) {
		ret = bq25890_charger_stop_charge(bq);
		if (ret)
			dev_err(bq->dev, "stop charge failed %d\n", ret);
	} else if (val) {
		ret = bq25890_charger_start_charge(bq);
		if (ret)
			dev_err(bq->dev, "start charge failed %d\n", ret);
	}

	return ret;
}

static int bq25890_charger_get_state(struct bq25890_device *bq,
				  struct bq25890_state *state)
{
	int i, ret;

	struct {
		enum bq25890_fields id;
		u8 *data;
	} state_fields[] = {
		{F_CHG_STAT,	&state->chrg_status},
		{F_PG_STAT,	&state->online},
		{F_VSYS_STAT,	&state->vsys_status},
		{F_BOOST_FAULT, &state->boost_fault},
		{F_BAT_FAULT,	&state->bat_fault},
		{F_CHG_FAULT,	&state->chrg_fault},
		{F_VBUS_STAT,	&state->vbus_status}
	},
	sc8989x_fields[] = {
		{F_CHG_STAT,	&state->chrg_status},
		{F_VBUS_GD,	&state->online},
		{F_VSYS_STAT,	&state->vsys_status},
		{F_BOOST_FAULT, &state->boost_fault},
		{F_BAT_FAULT,	&state->bat_fault},
		{F_CHG_FAULT,	&state->chrg_fault},
		{F_VBUS_STAT,	&state->vbus_status}
	};


	for (i = 0; i < ARRAY_SIZE(state_fields); i++) {
		if (bq->chip_id == SC8989X_ID) {
			ret = bq25890_field_read(bq, sc8989x_fields[i].id);
		} else {
			ret = bq25890_field_read(bq, state_fields[i].id);
		}
		if (ret < 0)
			return ret;

		*state_fields[i].data = ret;
	}

	return 0;
}

extern int get_pps_enable_status(void);
static int bq25890_power_supply_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	int ret;
	int is_pps_on = 0;
	int online = 0;
	struct bq25890_device *bq = power_supply_get_drvdata(psy);
	struct bq25890_state state;

	mutex_lock(&bq->lock);
	state = bq->state;
	mutex_unlock(&bq->lock);

	is_pps_on = get_pps_enable_status();
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq25890_charger_get_state(bq, &state);
		if (ret < 0)
			return ret;

		online = get_usb_onlie_state();

		if (!online)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (state.chrg_status == STATUS_TERMINATION_DONE &&
			is_pps_on == 0)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			if(bq->old_online != online){
				bq->update_cont = 20;
				cancel_delayed_work(&bq->xm_prop_change_work);
				schedule_delayed_work(&bq->xm_prop_change_work, msecs_to_jiffies(10));
			}
		}

		bq->old_online = online;

		pr_info("old_online = %d, online = %d, state.chrg_status = %d, status = %d, is_pps_on = %d\n", bq->old_online, online, state.chrg_status, val->intval, is_pps_on);

		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ25890_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		if (bq->chip_id == BQ25890_ID)
			val->strval = "BQ25890";
		else if (bq->chip_id == BQ25895_ID)
			val->strval = "BQ25895";
		else if (bq->chip_id == BQ25896_ID)
			val->strval = "BQ25896";
		else if (bq->chip_id == SC8989X_ID)
			val->strval = "SC8989X";
		else if (bq->chip_id == SYV690_ID)
			val->strval = "SYV690";
		else
			val->strval = "UNKNOWN";

		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq25890_charger_get_state(bq, &state);
		if (ret < 0)
			return ret;
		val->intval = state.online;
		bq->online = state.online;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (!state.chrg_fault && !state.bat_fault && !state.boost_fault)
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		else if (state.bat_fault)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else if (state.chrg_fault == CHRG_FAULT_TIMER_EXPIRED)
			val->intval = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
		else if (state.chrg_fault == CHRG_FAULT_THERMAL_SHUTDOWN)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq25890_field_read(bq, F_ICHGR); /* read measured value */
		if (ret < 0)
			return ret;

		/* converted_val = ADC_val * 50mA (table 10.3.19) */
		val->intval = ret * 50000;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq25890_field_read(bq, F_IDPM_LIM); /* read measured value */
		if (ret < 0)
			return ret;

		/* converted_val = ADC_val * 50mA +100mA(table 10.3.19) */
		val->intval = ret * 50000+100000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = bq25890_find_val(bq->init_data.ichg, TBL_ICHG);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq25890_field_read(bq, F_VBUSV); /* read measured value */
		if (ret < 0)
			return ret;

		/* converted_val = 2.6V + ADC_val * 100mV (table 10.3.15) */
		val->intval = 2600000 + ret * 100000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = bq25890_find_val(bq->init_data.vreg, TBL_VREG);
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		val->intval = bq25890_find_val(bq->init_data.iterm, TBL_ITERM);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq25890_field_read(bq, F_SYSV); /* read measured value */
		if (ret < 0)
			return ret;

		/* converted_val = 2.304V + ADC_val * 20mV (table 10.3.15) */
		val->intval = 2304000 + ret * 20000;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int bq25890_power_supply_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct bq25890_device *bq = power_supply_get_drvdata(psy);
	struct bq25890_state state;
	int ret;

	mutex_lock(&bq->lock);
	state = bq->state;
	mutex_unlock(&bq->lock);

	switch (psp) {
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
			if (bq->chip_id == SYV690 || bq->chip_id == bq25890H) {
				ret = bq25890_field_write(bq, F_ICHG,(val->intval)/64000); /* read measured value */
				if (ret < 0)
					return ret;
			}
			if (bq->chip_id == SC89890H) {
				ret = bq25890_field_write(bq, F_ICHG,(val->intval)/60000); /* read measured value */
				if (ret < 0)
					return ret;
			}
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
			/* converted_val = 2.304V + ADC_val * 20mV (table 10.3.15) */
			ret = bq25890_field_write(bq, F_VREG, (val->intval - 3840000) / 16000);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
			ret = bq25890_field_write(bq, F_VREG, (val->intval - 3840000) / 16000);
			if (ret < 0)
				return ret;
			break;
		case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			if (bq->chip_id == SYV690 || bq->chip_id == bq25890H) {
				ret = bq25890_field_write(bq, F_ITERM, (val->intval - 64000) / 64000);
				if (ret < 0)
					return ret;
			}
			if (bq->chip_id == SC89890H) {
				ret = bq25890_field_write(bq, F_ITERM, (val->intval - 30000) / 60000);
				if (ret < 0)
					return ret;
			}
			
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			ret = bq25890_field_write(bq, F_IILIM, (val->intval-100000) / 50000);
			if (ret < 0) {
				bq25890_field_write(bq, F_IILIM, (val->intval-100000) / 50000);
				pr_err("[%s]line=%d: dbg_info 25890 set input current limit failed\n", __FUNCTION__, __LINE__);
				return ret;
			}
			break;
		case POWER_SUPPLY_PROP_STATUS:
			ret = bq25890_charger_set_status(bq, val->intval);
			if (ret < 0)
				return ret;
			break;
		default:
			return -EINVAL;
	}

	return 0;

}

static int bq25890_power_supply_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
		return 1;
	default:
		break;
	}

	return 0;
}

static int bq25890_get_chip_state(struct bq25890_device *bq,
				  struct bq25890_state *state)
{
	int i, ret;

	struct {
		enum bq25890_fields id;
		u8 *data;
	} state_fields[] = {
		{F_CHG_STAT,	&state->chrg_status},
		{F_PG_STAT,	&state->online},
		{F_VSYS_STAT,	&state->vsys_status},
		{F_BOOST_FAULT, &state->boost_fault},
		{F_BAT_FAULT,	&state->bat_fault},
		{F_CHG_FAULT,	&state->chrg_fault},
		{F_VBUS_STAT,	&state->vbus_status}
	},
	sc8989x_fields[] = {
		{F_CHG_STAT,	&state->chrg_status},
		{F_VBUS_GD,	&state->online},
		{F_VSYS_STAT,	&state->vsys_status},
		{F_BOOST_FAULT, &state->boost_fault},
		{F_BAT_FAULT,	&state->bat_fault},
		{F_CHG_FAULT,	&state->chrg_fault},
		{F_VBUS_STAT,	&state->vbus_status}
	};


	for (i = 0; i < ARRAY_SIZE(state_fields); i++) {
		if (bq->chip_id == SC8989X_ID) {
			ret = bq25890_field_read(bq, sc8989x_fields[i].id);
		} else {
			ret = bq25890_field_read(bq, state_fields[i].id);
		}
		if (ret < 0)
			return ret;

		*state_fields[i].data = ret;
	}

	if (state->online == 1) {
		ret = bq25890_field_write(bq, F_CONV_RATE, 1);
		if (ret < 0)
			pr_err("Config F_CONV_RATE = 1 failed %d\n", ret);
	} else {
		ret = bq25890_field_write(bq, F_CONV_RATE, 0);
		if (ret < 0)
			pr_err("Config F_CONV_RATE = 0 failed %d\n", ret);
	}

	pr_info("S:CHG/PG/VSYS=%d/%d/%d, F:CHG/BOOST/BAT=%d/%d/%d,vbus=%d\n",
		state->chrg_status, state->online, state->vsys_status,
		state->chrg_fault, state->boost_fault, state->bat_fault, state->vbus_status);

	return 0;
}
#ifdef CONFIG_HQ_QGKI
int bq25890_detect_charger_status(struct bq25890_device *bq)
{

	bq->charger_status = bq25890_field_read(bq, F_CHG_STAT);
	if (bq->charger_status < 0)
		return bq->charger_status;

	return 0;
}
EXPORT_SYMBOL(bq25890_detect_charger_status);

int bq25890_detect_status(struct bq25890_device *bq)
{

	bq->charger_val = bq25890_field_read(bq, F_VBUS_STAT);
	if (bq->charger_val < 0)
		return bq->charger_val;

	return 0;
}
EXPORT_SYMBOL(bq25890_detect_status);

int bq25890_detect_charger_vbus_good_status(struct bq25890_device *bq)
{
	bq->vbus_good_status = bq25890_field_read(bq, F_VBUS_GD);
	if (bq->vbus_good_status < 0)
		return bq->vbus_good_status;

	return 0;
}
EXPORT_SYMBOL(bq25890_detect_charger_vbus_good_status);
#endif
static bool bq25890_state_changed(struct bq25890_device *bq,
				  struct bq25890_state *new_state)
{
	struct bq25890_state old_state;

	mutex_lock(&bq->lock);
	old_state = bq->state;
	mutex_unlock(&bq->lock);

	return (old_state.chrg_status != new_state->chrg_status ||
		old_state.chrg_fault != new_state->chrg_fault	||
		old_state.online != new_state->online		||
		old_state.bat_fault != new_state->bat_fault	||
		old_state.boost_fault != new_state->boost_fault ||
		old_state.vsys_status != new_state->vsys_status ||
		old_state.vbus_status != new_state->vbus_status);
}

static int request_dpdm(struct bq25890_device *bq, bool enable)
{
	int rc = 0;
	/* fetch the DPDM regulator */
	if (!bq->dpdm_reg && of_get_property(bq->dev->of_node,
				"dpdm-supply", NULL)) {
		bq->dpdm_reg = devm_regulator_get(bq->dev, "dpdm");
		if (IS_ERR(bq->dpdm_reg)) {
			rc = PTR_ERR(bq->dpdm_reg);
			dev_err(bq->dev, "Couldn't get dpdm regulator rc=%d\n",
					rc);
			bq->dpdm_reg = NULL;
			return rc;
		}
	}
	mutex_lock(&bq->dpdm_lock);
	if (enable) {
		if (bq->dpdm_reg && !bq->dpdm_enabled) {
			pr_err("--->southchip ap dpdm disable\n");
			rc = msm_hsphy_dpdm_regulator_enable(bq->dpdm_reg->rdev);
			if (rc < 0)
				dev_err(bq->dev,
					"Couldn't enable dpdm regulator rc=%d\n",
					rc);
			else
				bq->dpdm_enabled = true;
		}
	} else {
		if (bq->dpdm_reg && bq->dpdm_enabled) {
			pr_err("--->southchip ap dpdm enable\n");
			rc = msm_hsphy_dpdm_regulator_disable(bq->dpdm_reg->rdev);
			if (rc < 0)
				dev_err(bq->dev,
					"Couldn't disable dpdm regulator rc=%d\n",
					rc);
			else
				bq->dpdm_enabled = false;
		}
	}
	mutex_unlock(&bq->dpdm_lock);
	return rc;
}

static int bq25890_hw_init(struct bq25890_device *bq);
static void bq25890_handle_state_change(struct bq25890_device *bq,
					struct bq25890_state *new_state)
{
	int ret;
	struct bq25890_state old_state;

	mutex_lock(&bq->lock);
	old_state = bq->state;
	mutex_unlock(&bq->lock);

	if (!new_state->online) {			     /* power removed */
		/* disable ADC */
		pr_err("--->southchip, adapter remove\n");
		ret = bq25890_field_write(bq, F_CONV_RATE, 0);
		if (ret < 0)
			goto error;
          	if (bq->chip_id == SC8989X_ID) {
			cancel_delayed_work_sync(&bq->detect_vbat_set_vindpm_work);
                }
		cancel_delayed_work_sync(&bq->detect_float_work);
		request_dpdm(bq,0); // sily open ap dp dm
	} else if (!old_state.online) {			    /* power inserted */
		pr_err("--->southchip, adapter insert\n");
		bq->detect_force_dpdm_count = 0;
		if (bq->chip_id == SC8989X_ID || bq->chip_id == BQ25890_ID) {
			bq25890_field_write(bq, F_IILIM, 0);
			msleep(100);
			request_dpdm(bq,1); //close ap dp dm
			pr_err("southchip force dpdm, 02\n");
			regmap_write(bq->rmap, 0x02, 0x02);
			//bq25890_field_write(bq, F_FORCE_DPM, 1); //force run bc1.2
			//msleep(500);
		} else {
			request_dpdm(bq,1); //sily close ap dp dm
			pr_err("SILY force dpdm\n");
			bq25890_field_write(bq, F_FORCE_DPM, 1); //force run bc1.2
			msleep(500);
		}
		/* enable ADC, to have control of charge current/voltage */
		ret = bq25890_field_write(bq, F_CONV_RATE, 1);
		if (ret < 0)
			goto error;
	}


	if (old_state.vbus_status == 0 && new_state->vbus_status != 0) {
		pr_err("southchip bc1.2 done, open ap dpdm\n");
		if (bq->chip_id == SC8989X_ID) {
			pr_info("set Vindpm to 4800mV\n");
			bq25890_field_write(bq, F_FORCE_VINDPM, 1);
			bq25890_field_write(bq, F_VINDPM, 0x16);//Vindpm 4.8V
		
			schedule_delayed_work(&bq->detect_vbat_set_vindpm_work, msecs_to_jiffies(2000));
                }
		if (new_state->vbus_status == 5 && bq->detect_force_dpdm_count < 1) {		// float
			schedule_delayed_work(&bq->detect_float_work, msecs_to_jiffies(1000));
		}
		request_dpdm(bq,0); //open ap dp dm
	}


	return;

error:
	pr_err("Error communicating with the chip.\n");
}

static irqreturn_t bq25890_irq_handler_thread(int irq, void *private)
{
	struct bq25890_device *bq = private;
	int ret;
	struct bq25890_state state;

	pr_info("Enter bq25890_irq_handler_thread.\n");
	bq25890_dump_register(bq);
	ret = bq25890_get_chip_state(bq, &state);
	if (ret < 0)
		goto handled;

	if (!bq25890_state_changed(bq, &state))
		goto handled;

	bq25890_handle_state_change(bq, &state);

	mutex_lock(&bq->lock);
	bq->state = state;
	mutex_unlock(&bq->lock);

	if (bq->usb) {
		power_supply_changed(bq->usb);
		if (IS_ERR(bq->usb)) {
			pr_err("Cannot get bq->usb,err_line=%d\n", __LINE__);
			PTR_ERR(bq->usb);
		}
	}
	if (bq->charger) {
		power_supply_changed(bq->charger);
		if (IS_ERR(bq->charger)) {
			pr_err("Cannot get bq->charger,err_line=%d\n", __LINE__);
			PTR_ERR(bq->charger);
		}
	}
	dev_err(bq->dev, "power_supply_changed usb bq->charger\n");

handled:
	return IRQ_HANDLED;
}

static int bq25890_chip_reset(struct bq25890_device *bq)
{
	int ret;
	int rst_check_counter = 10;

	ret = bq25890_field_write(bq, F_REG_RST, 1);
	if (ret < 0)
		return ret;

	do {
		ret = bq25890_field_read(bq, F_REG_RST);
		if (ret < 0)
			return ret;

		usleep_range(5, 10);
	} while (ret == 1 && --rst_check_counter);

	if (!rst_check_counter)
		return -ETIMEDOUT;

	return 0;
}

int main_chgic_reset(void)
{
	int ret;

	ret = bq25890_chip_reset(g_bq);
	if (ret < 0) {
		pr_err("Reset failed %d\n", ret);
		return ret;
	}
	ret = bq25890_field_write(g_bq, F_CONV_RATE, 0);
	if (ret < 0)
		pr_err("bq25890 conv rate failed\n");
	return 0;
}
EXPORT_SYMBOL(main_chgic_reset);

static int bq25890_hw_init(struct bq25890_device *bq)
{
	int ret;
	int i;
	struct bq25890_state state;

	const struct {
		enum bq25890_fields id;
		u32 value;
	} init_data[] = {
		{F_ICHG,	 bq->init_data.ichg},
		{F_VREG,	 bq->init_data.vreg},
		{F_ITERM,	 bq->init_data.iterm},
		{F_IPRECHG,	 bq->init_data.iprechg},
		{F_SYSVMIN,	 bq->init_data.sysvmin},
		{F_BOOSTV,	 bq->init_data.boostv},
		{F_BOOSTI,	 bq->init_data.boosti},
		{F_BOOSTF,	 bq->init_data.boostf},
		{F_EN_ILIM,	 bq->init_data.ilim_en},
		{F_TREG,	 bq->init_data.treg}
	};

	// ret = bq25890_chip_reset(bq);
	// if (ret < 0) {
	// 	pr_err("Reset failed %d\n", ret);
	// 	return ret;
	// }

	/* disable watchdog */
	ret = bq25890_field_write(bq, F_WD, 0);
	if (ret < 0) {
		pr_err("Disabling watchdog failed %d\n", ret);
		return ret;
	}

	/* disable HVDCP */
	ret = bq25890_field_write(bq, F_HVDCP_EN, 0);
	if (ret < 0) {
		pr_err("Disabling hvdcp failed %d\n", ret);
		return ret;
	}

	/*close auto dpdm */
	ret = bq25890_field_write(bq, F_AUTO_DPDM_EN, 0);//close auto dpdm
	if (ret < 0) {
		pr_err("Disabling F_AUTO_DPDM_EN failed %d\n", ret);
		return ret;
	}

	/* disable ICO */
	ret = bq25890_field_write(bq, F_ICO_EN, 0);
	if (ret < 0) {
		pr_err("Disabling ico failed %d\n", ret);
		return ret;
	}

	/* vrechg 200mv */
	ret = bq25890_field_write(bq, F_VRECHG, 1);
	if (ret < 0) {
		pr_err("vrechg 200mv failed %d\n", ret);
		return ret;
	}

	/* run relative setting */
	ret = bq25890_field_write(bq, F_FORCE_VINDPM, 0);
	if (ret < 0) {
		pr_err("run relative setting failed %d\n", ret);
        }

	/* charging termination */
	ret = bq25890_field_write(bq, F_TERM_EN, 1);
	if (ret < 0) {
		pr_err("charging termination failed %d\n", ret);
		return ret;
	}

	/* initialize currents/voltages and other parameters */
	for (i = 0; i < ARRAY_SIZE(init_data); i++) {
		ret = bq25890_field_write(bq, init_data[i].id,
					  init_data[i].value);
		if (ret < 0) {
			pr_err("Writing init data failed %d\n", ret);
			return ret;
		}
	}

	if (bq->chip_id == SC8989X_ID)
		ret = bq25890_field_write(bq, F_BOOSTV, 0xD);
 	else
		ret = bq25890_field_write(bq, F_BOOSTV, 0xA);

	if (ret < 0) {
		pr_err("boostv  failed %d\n", ret);
		return ret;
	}

	/* Configure ADC for continuous conversions. This does not enable it. */
	ret = bq25890_field_write(bq, F_CONV_RATE, 1);
	if (ret < 0) {
		pr_err("Config ADC failed %d\n", ret);
		return ret;
	}

	ret = bq25890_get_chip_state(bq, &state);
	if (ret < 0) {
		pr_err("Get state failed %d\n", ret);
		return ret;
	}

	mutex_lock(&bq->lock);
	bq->state = state;
	mutex_unlock(&bq->lock);
	bq25890_dump_register(bq);
	return 0;
}

static int  bq25890_get_chg_type(struct bq25890_device *bq)
{
	u8 val;
	int type, real_type;

	val = bq25890_field_read(bq,F_VBUS_STAT);
	type = (int)val;

	if (type == BQ2589X_VBUS_USB_SDP)
		real_type = POWER_SUPPLY_TYPE_USB;
	else if (type == BQ2589X_VBUS_USB_CDP)
		real_type = POWER_SUPPLY_TYPE_USB_CDP;
	else if (type == BQ2589X_VBUS_USB_DCP)
		real_type = POWER_SUPPLY_TYPE_USB_DCP;
	else if (type == BQ2589X_VBUS_NONSTAND || type == BQ2589X_VBUS_UNKNOWN)
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	else if (type == BQ2589X_VBUS_MAXC)
		real_type = POWER_SUPPLY_TYPE_USB_HVDCP;
	else
		real_type = POWER_SUPPLY_TYPE_UNKNOWN;

	bq->charge_type = val;
	return real_type;
}

static int get_usb_real_type(struct bq25890_device *bq)
{
	int  real_type;
	if(!bq)
		return -EINVAL;
	real_type = bq25890_get_chg_type(bq);
	if(bq->pdactive)
		real_type = POWER_SUPPLY_TYPE_USB_PD;
	if(bq->otg_enable)
		real_type = 0;
	bq->real_type = real_type;
	return real_type;
}

extern int get_usbpd_verifed_state(void);
int get_quick_charge_type(struct bq25890_device *bq)
{
	int i = 0;
	int pd_auth = 0;

	if (!bq)
		return 0;

	bq->real_type = get_usb_real_type(bq);
	if(bq->real_type == POWER_SUPPLY_TYPE_USB_PD){
		pd_auth = get_usbpd_verifed_state();
		if(1 == pd_auth)
			return QUICK_CHARGE_TURBE;
		else
			return QUICK_CHARGE_FAST;
	} else {
		while (adapter_cap[i].adap_type != 0) {
			if (bq->real_type == adapter_cap[i].adap_type) {
				return adapter_cap[i].adap_cap;
			}
			i++;
		}
	}
	pr_info("get_quick_charge_type is not supported in usb\n");
	return 0;
}


int get_apdo_max(struct bq25890_device *bq)
{
	if (!bq)
		return 0;

	bq->real_type = get_usb_real_type(bq);
	if(bq->real_type == POWER_SUPPLY_TYPE_USB_PD){
		return ((bq->apdo_max_volt * bq->apdo_max_curr)/1000000);
	} else
		pr_err("get_apdo_max is not supported in usb\n");
	return 0;
}

static int bq25890_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	int ret = 0;
	struct bq25890_device *bq = power_supply_get_drvdata(psy);
	struct bq25890_state state;
	union power_supply_propval battemp_val;

	mutex_lock(&bq->lock);
	state = bq->state;
	mutex_unlock(&bq->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = get_usb_onlie_state();
		pr_info("usb online = %d\n",val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = get_usb_real_type(bq);
		if (val->intval  == POWER_SUPPLY_TYPE_USB || val->intval  == POWER_SUPPLY_TYPE_USB_CDP)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = bq25890_charger_get_state(bq, &state);
		if (ret < 0)
			return ret;
		if ((bq->pdactive) && (state.online)) {
			val->intval = POWER_SUPPLY_USB_TYPE_PD;
			pr_info("usb type is pd ,vbus_state=%x\n", state.vbus_status);
			break;
		}
		switch(state.vbus_status) {
			case 0x00:
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
				break;
			case 0x01:
				val->intval = POWER_SUPPLY_USB_TYPE_SDP;
				break;
			case 0x02:
				val->intval = POWER_SUPPLY_USB_TYPE_CDP;
				break;
			case 0x03:
				val->intval = POWER_SUPPLY_USB_TYPE_DCP;
				break;
			case 0x04:
				val->intval = POWER_SUPPLY_USB_TYPE_DCP;
				break;
			case 0x05:
			case 0x06:
				val->intval = POWER_SUPPLY_USB_TYPE_C;
				break;
			default:
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
				break;
		}
		pr_info("Get usb type %d,vbus_state=%x\n", val->intval, state.vbus_status);
		break;
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = get_quick_charge_type(bq);
		pr_info("get_quick_charge_type %d\n", val->intval);
		bq->batpsy = power_supply_get_by_name("battery");
		if(bq->batpsy != NULL) {
			ret = power_supply_get_property(bq->batpsy,POWER_SUPPLY_PROP_TEMP,&battemp_val);
			if (battemp_val.intval >= 580)
				val->intval = QUICK_CHARGE_NORMAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return 0;
}

static int bq25890_usb_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int ret;
	struct bq25890_device *bq = power_supply_get_drvdata(psy);
	switch (psp) {
		case POWER_SUPPLY_PROP_USB_TYPE:
			switch(val->intval)
			{
				case POWER_SUPPLY_TYPE_OTG_ENABLE:
					ret = bq25890_field_write(bq, F_CHG_CFG, FALSE);
					ret = bq25890_field_write(bq, F_OTG_CFG, TRUE);
					pr_info("enable otg mode\n");
					break;
				case POWER_SUPPLY_TYPE_OTG_DISABLE:
					ret = bq25890_field_write(bq, F_OTG_CFG, FALSE);
					ret = bq25890_field_write(bq, F_CHG_CFG, TRUE);
					pr_info("disable otg mode\n");
					break;
				case POWER_SUPPLY_PD_ACTIVE:
				case POWER_SUPPLY_PD_PPS_ACTIVE:
					bq->pdactive = 1;
					pr_info("bq->pdactive = %d\n",val->intval);
					break;
				case POWER_SUPPLY_PD_INACTIVE:
					bq->pdactive = 0;
					pr_info("bq->pdactive = %d\n",val->intval);
					break;
                          default:break;
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return 0;
}

static int bq25890_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_USB_TYPE:
		return 1;
	default:
		break;
	}
	return 0;
}

static enum power_supply_usb_type bq25890_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,		/* Standard Downstream Port */
	POWER_SUPPLY_USB_TYPE_DCP,		/* Dedicated Charging Port */
	POWER_SUPPLY_USB_TYPE_CDP,		/* Charging Downstream Port */
	POWER_SUPPLY_USB_TYPE_ACA,		/* Accessory Charger Adapters */
	POWER_SUPPLY_USB_TYPE_C,		/* Type C Port */
	POWER_SUPPLY_USB_TYPE_PD,		/* Power Delivery Port */
	POWER_SUPPLY_USB_TYPE_PD_DRP,		/* PD Dual Role Port */
};

static enum power_supply_property bq25890_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
};

/* usb_data initialization */

 static const struct power_supply_desc bq25890_usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = bq25890_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(bq25890_psy_usb_types),
	.properties = bq25890_usb_props,
	.num_properties = ARRAY_SIZE(bq25890_usb_props),
	.get_property = bq25890_usb_get_property,
	.set_property = bq25890_usb_set_property,
	.property_is_writeable = bq25890_usb_prop_is_writeable,
};

static enum power_supply_property bq25890_power_supply_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static char *bq25890_charger_supplied_to[] = {
	"main-battery",
};

static const struct power_supply_desc bq25890_power_supply_desc = {
	.name = "bq25890_charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = bq25890_power_supply_props,
	.num_properties = ARRAY_SIZE(bq25890_power_supply_props),
	.get_property = bq25890_power_supply_get_property,
	.set_property = bq25890_power_supply_set_property,
	.property_is_writeable = bq25890_power_supply_prop_is_writeable,
};

static int bq25890_power_supply_init(struct bq25890_device *bq)
{
	struct power_supply_config psy_cfg = { .drv_data = bq, };

	psy_cfg.supplied_to = bq25890_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(bq25890_charger_supplied_to);

	bq->charger = power_supply_register(bq->dev, &bq25890_power_supply_desc,
					    &psy_cfg);

	return PTR_ERR_OR_ZERO(bq->charger);
}

static int bq25890_usb_power_supply_init(struct bq25890_device *bq)
{
	struct power_supply_config psy_cfg = { .drv_data = bq, };

	psy_cfg.supplied_to = bq25890_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(bq25890_charger_supplied_to);

	bq->usb = power_supply_register(bq->dev, &bq25890_usb_psy_desc,
					    &psy_cfg);

	return PTR_ERR_OR_ZERO(bq->charger);
}

static void bq25890_usb_work(struct work_struct *data)
{
	int ret;
	struct bq25890_device *bq =
			container_of(data, struct bq25890_device, usb_work);

	pr_info("Enter bq25890 bq25890_usb_work.\n");
	switch (bq->usb_event) {
	case USB_EVENT_ID:
		/* Enable boost mode */
		ret = bq25890_field_write(bq, F_OTG_CFG, 1);
		if (ret < 0)
			goto error;
		break;

	case USB_EVENT_NONE:
		/* Disable boost mode */
		ret = bq25890_field_write(bq, F_OTG_CFG, 0);
		if (ret < 0)
			goto error;

		if (bq->charger) {
			power_supply_changed(bq->charger);
			if (IS_ERR(bq->charger)) {
				pr_err("Cannot get bq->charger,err_line=%d\n", __LINE__);
				PTR_ERR(bq->charger);
			}
		}
		break;
	}

	return;

error:
	pr_err("Error switching to boost/charger mode.\n");
}

static int bq25890_usb_notifier(struct notifier_block *nb, unsigned long val,
				void *priv)
{
	struct bq25890_device *bq =
			container_of(nb, struct bq25890_device, usb_nb);

	pr_err("Enter bq25890 bq25890_usb_notifier.\n");
	bq->usb_event = val;
	queue_work(system_power_efficient_wq, &bq->usb_work);

	return NOTIFY_OK;
}

int get_capa_from_sm5602(struct bq25890_device *bq)
{
	int ret = 0;
	union power_supply_propval val = {0, };

	if (bq->batpsy == NULL)
		bq->batpsy = power_supply_get_by_name("battery");

	if (bq->batpsy) {
		ret = power_supply_get_property(bq->batpsy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		if (ret < 0) {
			pr_err("get capacity from sm5602 failed, ret = %d\n", ret);
			return ret ;
		}
	}
	return val.intval;
}


static void bq25890_dumpic_work(struct work_struct *work)
{
	int mtbf_soc = 0;
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq25890_device *bq =
			container_of(dwork, struct bq25890_device, dumpic_work);
	bq25890_dump_register(bq);

	if (is_mtbf_mode) {
		mtbf_soc = get_capa_from_sm5602(bq);
		if (mtbf_soc < 45) {
			bq25890_charger_start_charge(bq);
		}
		if (mtbf_soc > 95) {
			bq25890_charger_stop_charge(bq);
		}
	}
	schedule_delayed_work(&bq->dumpic_work, msecs_to_jiffies(10000));
}

static void bq25890_detect_vbat_set_vindpm_work(struct work_struct *work)
{
	struct bq25890_device *bq =
			container_of(work, struct bq25890_device, detect_vbat_set_vindpm_work.work);
	int vbat;

	vbat = bq25890_field_read(bq, F_BATV);

	vbat = 2304 + vbat * 20;
	pr_info("bq25890_detect_vbat_set_vindpm_work:vbat:%dmV\n",vbat);
	if (vbat <= min_charger_voltage_1) {
		// <=4v,set vindpm 4.4
		pr_info("vbat less than 4000 mv set Vindpm to 4400mV\n");
		bq25890_field_write(bq, F_VINDPM, 0x12);//Vindpm 4.4V
	} else if (vbat <= min_charger_voltage_2) {
		// 4v<vbat<=4.3v,vindpm 4.6
		pr_info(" set Vindpm to 4600mv\n");
		bq25890_field_write(bq, F_VINDPM, 0x14);
	} else {
		// vbat>4.3v,vindpm 4.8
		pr_info(" set Vindpm to 4800mv\n");
		bq25890_field_write(bq, F_VINDPM, 0x16);
	}
	schedule_delayed_work(&bq->detect_vbat_set_vindpm_work, msecs_to_jiffies(10000));

}

static void bq25890_detect_float_work(struct work_struct *work)
{
	struct bq25890_device *bq =
			container_of(work, struct bq25890_device, detect_float_work.work);

	request_dpdm(bq,1); //close ap dp dm
	pr_err("southchip force dpdm\n");
	bq25890_field_write(bq, F_FORCE_DPM, 1); //force run bc1.2
	bq->detect_force_dpdm_count ++;
	mutex_lock(&bq->lock);
	bq->state.vbus_status = 0;
	mutex_unlock(&bq->lock);
}

static int bq25890_irq_probe(struct bq25890_device *bq)
{
	struct gpio_desc *irq;

	irq = devm_gpiod_get(bq->dev, BQ25890_IRQ_PIN, GPIOD_IN);
	if (IS_ERR(irq)) {
		pr_err("Could not probe irq pin.\n");
		return PTR_ERR(irq);
	}

	return gpiod_to_irq(irq);
}

static int bq25890_fw_read_u32_props(struct bq25890_device *bq)
{
	int ret;
	u32 property;
	int i;
	struct bq25890_init_data *init = &bq->init_data;
	struct {
		char *name;
		bool optional;
		enum bq25890_table_ids tbl_id;
		u8 *conv_data; /* holds converted value from given property */
	} props[] = {
		/* required properties */
		{"ti,charge-current", false, TBL_ICHG, &init->ichg},
		{"ti,battery-regulation-voltage", false, TBL_VREG, &init->vreg},
		{"ti,termination-current", false, TBL_ITERM, &init->iterm},
		{"ti,precharge-current", false, TBL_ITERM, &init->iprechg},
		{"ti,minimum-sys-voltage", false, TBL_SYSVMIN, &init->sysvmin},
		{"ti,boost-voltage", false, TBL_BOOSTV, &init->boostv},
		{"ti,boost-max-current", false, TBL_BOOSTI, &init->boosti},

		/* optional properties */
		{"ti,thermal-regulation-threshold", true, TBL_TREG, &init->treg}
	};

	/* initialize data for optional properties */
	init->treg = 3; /* 120 degrees Celsius */

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = device_property_read_u32(bq->dev, props[i].name,
					       &property);
		if (ret < 0) {
			if (props[i].optional)
				continue;

			pr_err("Unable to read property %d %s\n", ret,
				props[i].name);

			return ret;
		}

		*props[i].conv_data = bq25890_find_idx(bq,property,
						       props[i].tbl_id);
	}

	return 0;
}

static int bq25890_fw_probe(struct bq25890_device *bq)
{
	int ret;
	struct bq25890_init_data *init = &bq->init_data;

	ret = bq25890_fw_read_u32_props(bq);
	if (ret < 0)
		return ret;

	init->ilim_en = device_property_read_bool(bq->dev, "ti,use-ilim-pin");
	init->boostf = device_property_read_bool(bq->dev, "ti,boost-low-freq");

	return 0;
}


int get_input_suspend_flag(void)
{
    return input_suspend_flag;
}
EXPORT_SYMBOL(get_input_suspend_flag);

/* add hq_test_input_suspend node start */
static ssize_t hq_test_input_suspend_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bq->hq_test_input_suspend = val;
	if(bq->hq_test_input_suspend)
		input_suspend_flag = 1;//disable slave chg
	else
		input_suspend_flag = 0;//enable slave chg

	bq25890_field_write(g_bq, F_EN_HIZ, bq->hq_test_input_suspend);
	pr_info("bq->hq_test_input_suspend = %d\n", bq->hq_test_input_suspend);

	return count;
}

static ssize_t hq_test_input_suspend_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	return scnprintf(buf, PAGE_SIZE, "%d\n", bq->hq_test_input_suspend);
}
static CLASS_ATTR_RW(hq_test_input_suspend);
/* end */

/* add input_suspend node start */
static ssize_t input_suspend_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bq->input_suspend = val;
	if(bq->input_suspend) {
		//bq25890_field_write(bq, F_FORCE_VINDPM, 1);
		//bq25890_field_write(bq, F_VINDPM, 127);
		bq25890_field_write(bq, F_CHG_CFG, 0);
		input_suspend_flag = 1;//disable slave chg
	} else {
		//bq25890_field_write(bq, F_FORCE_VINDPM, 0);
		bq25890_field_write(bq, F_CHG_CFG, 1);
		input_suspend_flag = 0;//enable slave chg
	}
	pr_info("bq->input_suspend = %d\n", bq->input_suspend);

	return count;
}

static ssize_t input_suspend_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	return scnprintf(buf, PAGE_SIZE, "%d\n", bq->input_suspend);
}
static CLASS_ATTR_RW(input_suspend);
/* end */

/* add batt_id node start */
static ssize_t batt_id_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	return scnprintf(buf, PAGE_SIZE, "%d\n", bq->fake_battery_id);
}
static CLASS_ATTR_RO(batt_id);
/* end */

/* add otg_enable node start */
static ssize_t otg_enable_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);

	bq->otg_enable = bq25890_field_read(bq, F_OTG_CFG);
	if (bq->otg_enable < 0)
		return bq->otg_enable;
	pr_info("get otg enable = %d\n", bq->otg_enable);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bq->otg_enable);
}
static CLASS_ATTR_RO(otg_enable);
/* end */

/* add board_temp node start */
static ssize_t board_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val;

	get_board_temp(bq, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(board_temp);
/* end */

/* add typec_cc_orientation node start */
static ssize_t typec_cc_orientation_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);

	int  cc_orientation;
	bq->otg_enable = bq25890_field_read(bq, F_OTG_CFG);

	if (bq->online || bq->otg_enable) {
		cc_orientation = bq->typec_cc_orientation;
	} else {
		cc_orientation = 0;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", cc_orientation);
}
static CLASS_ATTR_RO(typec_cc_orientation);


static ssize_t apdo_max_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", get_apdo_max(bq));
}
static CLASS_ATTR_RO(apdo_max);
/* end */
/* add set_ship_mode node start */
static ssize_t set_ship_mode_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if((val == 1) || (val == 0)) {
		bq25890_set_ship_mode(bq, val);
		pr_err("set shipmode success,val = %d\n", val);
	} else {
		pr_err("%d is not a valid value,set_ship_mode failed\n", val);
	}
	return count;
}

/* add set_mtbf_current node start */
static ssize_t mtbf_current_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val, ret;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if(val != 0) {
		is_mtbf_mode = 1;
		pr_err("set mtbf current,val = %d\n", val);
		/* disable chg timer */
		ret = bq25890_field_write(bq, F_TMR_EN, 0);
		if (ret < 0)
			pr_err("Disable chg timer failed %d\n", ret);
	} else {
		is_mtbf_mode = 0;
		pr_err("val == 0, not need to set iilim and ichg\n");
		/* enable chg timer */
		ret = bq25890_field_write(bq, F_TMR_EN, 1);
		if (ret < 0)
			pr_err("enable chg timer failed %d\n", ret);
	}
	return count;
}
/*end*/

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i;

	if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
	    usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB) {
		for (i = 0; i < ARRAY_SIZE(qc_power_supply_usb_type_text);
		     i++) {
			if (i == (usb_type - QTI_POWER_SUPPLY_USB_TYPE_HVDCP))
				return qc_power_supply_usb_type_text[i];
		}
		return "Unknown";
	}

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

static ssize_t typec_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int type;

	type = get_usb_real_type(bq);
	if (bq->typec_mode == 0 && type != 0)
		type = 1;
	else
		type = bq->typec_mode;
	pr_err(" %s vaule = %d\n", __func__, type);
	return scnprintf(buf, PAGE_SIZE, "%s\n",  usb_typec_mode_text[type]);
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t set_ship_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val;

	bq25890_get_ship_mode(bq, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(set_ship_mode);
/* end */

/* add soc_decimal node start */
static ssize_t soc_decimal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	union power_supply_propval val = {.intval = 0};

#ifdef SOC_DECIMAL_2_POINT
	val.intval = fg_get_soc_decimal();
#endif
	pr_err("soc_decimal_show,val = %d\n", val.intval);
	if(val.intval < 0)
		val.intval = 0;
	return scnprintf(buf, PAGE_SIZE, "%d", val.intval);
}
static CLASS_ATTR_RO(soc_decimal);
/* end */

/* add soc_decimal_rate node start */
static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	union power_supply_propval val = {.intval = 0};

#ifdef SOC_DECIMAL_2_POINT
	val.intval = fg_get_soc_decimal_rate();
#endif
	pr_err("soc_decimal_rate_show,val = %d\n", val.intval);
	if(val.intval > 100 || val.intval < 0)
		val.intval = 0;
	return scnprintf(buf, PAGE_SIZE, "%d", val.intval);
}
static CLASS_ATTR_RO(soc_decimal_rate);
/* end */

/* add soc_decimal_rate node start */
static ssize_t real_type_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val,real_type;

	real_type = get_usb_real_type(bq);
	val = real_type - 3;
	pr_info("real type = %s\n", get_usb_type_name(val));
	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(real_type);
/* end */

/* add mtbf_current_rate node start */
static ssize_t mtbf_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct bq25890_device *bq = container_of(c, struct bq25890_device, usb_debug_class);
	int val_iilim, val_ichg;

	val_iilim = bq25890_field_read(bq, F_IILIM);
	val_ichg = bq25890_field_read(bq, F_ICHG);

	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n", val_iilim, val_ichg, is_mtbf_mode);
}
static CLASS_ATTR_RW(mtbf_current);

bool is_mtbf_mode_func(void)
{
	return is_mtbf_mode;
}
EXPORT_SYMBOL_GPL(is_mtbf_mode_func);
/* end */

static struct attribute *usb_debug_class_attrs[] = {
	&class_attr_batt_id.attr,
	&class_attr_input_suspend.attr,
	&class_attr_otg_enable.attr,
	&class_attr_board_temp.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_apdo_max.attr,
	&class_attr_set_ship_mode.attr,
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_typec_mode.attr,
	&class_attr_real_type.attr,
	&class_attr_mtbf_current.attr,
	&class_attr_hq_test_input_suspend.attr,
	NULL,
};
ATTRIBUTE_GROUPS(usb_debug_class);

static ssize_t dev_real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val,real_type;
	real_type = get_usb_real_type(g_bq);
	val = real_type - 3;
	pr_info("real type = %s\n", get_usb_type_name(val));
	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static DEVICE_ATTR(real_type, 0664, dev_real_type_show, NULL);

static ssize_t dev_input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",g_bq->input_suspend);
}

static ssize_t dev_input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_bq->input_suspend = val;
	if(g_bq->input_suspend) {
		//bq25890_field_write(g_bq, F_FORCE_VINDPM, 1);
		//bq25890_field_write(g_bq, F_VINDPM, 127);
		bq25890_field_write(g_bq, F_CHG_CFG, 0);
		input_suspend_flag = 1;//disable slave chg
	} else {
		//bq25890_field_write(g_bq, F_FORCE_VINDPM, 0);
		bq25890_field_write(g_bq, F_CHG_CFG, 1);
		input_suspend_flag = 0;//enable slave chg
	}
	bq25890_field_write(g_bq, F_EN_HIZ, g_bq->input_suspend);
	pr_info("bq->input_suspend = %d\n", g_bq->input_suspend);

	return count;
}
static DEVICE_ATTR(input_suspend, 0664, dev_input_suspend_show, dev_input_suspend_store);

static ssize_t dev_hq_test_input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",g_bq->hq_test_input_suspend);
}

static ssize_t dev_hq_test_input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_bq->hq_test_input_suspend = val;
	if(g_bq->hq_test_input_suspend)
		input_suspend_flag = 1;//disable slave chg
	else
		input_suspend_flag = 0;//enable slave chg

	bq25890_field_write(g_bq, F_EN_HIZ, g_bq->hq_test_input_suspend);
	pr_info("bq->hq_test_input_suspend = %d\n", g_bq->hq_test_input_suspend);

	return count;
}
static DEVICE_ATTR(hq_test_input_suspend, 0664, dev_hq_test_input_suspend_show, dev_hq_test_input_suspend_store);

static struct attribute *battery_attributes[] = {
	&dev_attr_real_type.attr,
	&dev_attr_input_suspend.attr,
	&dev_attr_hq_test_input_suspend.attr,
	NULL,
};

static const struct attribute_group battery_attr_group = {
	.attrs = battery_attributes,
};

static const struct attribute_group *battery_attr_groups[] = {
	&battery_attr_group,
	NULL,
};

static int usb_debug_class_init(struct bq25890_device *bq)
{
	int ret;
	int rc = -EINVAL;

	bq->usb_debug_class.name = "qcom-battery";
	bq->usb_debug_class.class_groups = usb_debug_class_groups;
	ret = class_register(&bq->usb_debug_class);
	if (ret < 0)
		pr_err("Failed to create usb_debug_class,ret = %d\n", ret);

	bq->batt_device.class = &bq->usb_debug_class;
	dev_set_name(&bq->batt_device, "odm_battery");
	bq->batt_device.parent = bq->dev;
	bq->batt_device.groups = battery_attr_groups;
	rc = device_register(&bq->batt_device);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
	}

	return ret;
}
/* end */

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uvent(struct work_struct *work)
{
	int count;
	struct bq25890_device *bq = container_of(work, struct bq25890_device, xm_prop_change_work.work);

	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SOC_DECIMAL=\n",	//length=31+8
		"POWER_SUPPLY_SOC_DECIMAL_RATE=\n",	//length=31+8
	};
	static char *envp[] = {
		uevent_string[0],
		uevent_string[1],

		NULL,

	};
	char *prop_buf = NULL;

	count = bq->update_cont;
	if(bq->update_cont < 0)
		return;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return;

	soc_decimal_show(& (bq->usb_debug_class), NULL, prop_buf);
	strncpy( uevent_string[0]+25, prop_buf,MAX_UEVENT_LENGTH-25);

	soc_decimal_rate_show( &(bq->usb_debug_class), NULL, prop_buf);
	strncpy( uevent_string[1]+30, prop_buf,MAX_UEVENT_LENGTH-30);

	pr_err("uevent test : %s\n %s count %d\n",
			envp[0], envp[1], count);

	/*add our prop end*/

	kobject_uevent_env(&bq->dev->kobj, KOBJ_CHANGE, envp);

	free_page((unsigned long)prop_buf);
	bq->update_cont = count - 1;

	schedule_delayed_work(&bq->xm_prop_change_work, msecs_to_jiffies(100));

	return;
}


static int get_board_temp(struct bq25890_device *bq,
				 int *val)
{
	int ret, temp;

	if (bq->board_therm_channel) {
		ret = iio_read_channel_processed(bq->board_therm_channel,
				&temp);
		if (ret < 0) {
			pr_err("Error in reading temp channel, ret=%d\n", ret);
			return ret;
		}
		*val = temp / 100;
		pr_info("get_board_temp(%d)\n", *val);
	} else {
		return -ENODATA;
	}

	return ret;
}

/* BSP.AUDIO - 2022.07.26 - modify for typec*/
int get_usb_typec_mode(void)
{
	return typec_headphone_mode;
}
EXPORT_SYMBOL(get_usb_typec_mode);
/* end modify*/

static int bq25890_iio_set_prop(struct bq25890_device *bq,
	int channel, int val)
{
	//int ret = 0;

	switch (channel) {
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		bq->typec_cc_orientation = val;
		break;
	case PSY_IIO_TYPEC_MODE:
		bq->typec_mode = val;
		typec_headphone_mode = val;
		break;
	case PSY_IIO_APDO_MAX_VOLT:
		bq->apdo_max_volt = val;
		pr_info("set apdo_max_volt(%d)\n", bq->apdo_max_volt);
		break;
	case PSY_IIO_APDO_MAX_CURR:
		bq->apdo_max_curr = val;
		pr_info("set apdo_max_curr(%d)\n",bq->apdo_max_curr );
		break;
	case PSY_IIO_SET_SHIP_MODE:
		bq25890_set_ship_mode(bq, val);
		pr_err("[%s]line=%d: set ship mode success\n", __FUNCTION__, __LINE__);
		break;
	default:
		pr_info("bq25890 master set prop %d is not supported\n", channel);
		return -EINVAL;
	}

	return 0;
}

static int bq25890_iio_get_prop(struct bq25890_device *bq,
	int channel, int *val)
{
	int ret = 0;

	switch (channel) {
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		*val = bq->typec_cc_orientation;
		break;
	case PSY_IIO_BOARD_TEMP:
		ret = get_board_temp(bq, val);
		if (ret)
			return -EINVAL;
		break;
	case PSY_IIO_SET_SHIP_MODE:
		ret = bq25890_get_ship_mode(bq, val);
		if (ret < 0)
			return ret;
		break;
	default:
		pr_info("bq25890 master get prop %d is not supported\n", channel);
		return -EINVAL;
	}

	return IIO_VAL_INT;
}

static int bq25890_write_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, int val, int val2,
			 long mask)
{
	struct bq25890_device *iio_chip = iio_priv(indio_dev);
	int channel;
	channel = chan->channel;

	return bq25890_iio_set_prop(iio_chip, channel, val);
}

static int bq25890_read_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask)
{
	struct bq25890_device *iio_chip = iio_priv(indio_dev);
	int channel;

	channel = chan->channel;

	return bq25890_iio_get_prop(iio_chip, channel, val);
}

static int bq25890_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct bq25890_device *iio_chip = iio_priv(indio_dev);
	int i;
	struct iio_chan_spec *iio_chan = iio_chip->bq25890_iio_chan_ids;

	for (i = 0; i < iio_chip->nchannels; i++) {
		pr_info("bq25890_of_xlate iio_chan->channel: %d iiospec->args[0]: %d\n", iio_chan->channel, iiospec->args[0]);
		if (iio_chan->channel == iiospec->args[0])
			return i;
		iio_chan++;
	}

	return -EINVAL;
}

static const struct iio_info bq25890_iio_info = {
	.read_raw = bq25890_read_raw,
	.write_raw = bq25890_write_raw,
	.of_xlate = bq25890_of_xlate,
};

static int bq25890_iio_probe_init(struct bq25890_device *bq,
	struct iio_dev *indio_dev, const struct bq25890_iio_prop_channels *bq25890_chans,
	const struct iio_info *bq25890_iio_info)
{
	int i;
	struct iio_chan_spec *iio_chan;

	bq->bq25890_iio_chan_ids = devm_kcalloc(bq->dev, bq->nchannels,
		sizeof(*bq->bq25890_iio_chan_ids), GFP_KERNEL);
	if (!bq->bq25890_iio_chan_ids)
		return -ENOMEM;

	for (i = 0; i < bq->nchannels; i++) {
		iio_chan = &bq->bq25890_iio_chan_ids[i];

		iio_chan->channel = bq25890_chans[i].channel_no;
		iio_chan->datasheet_name =
			bq25890_chans[i].datasheet_name;
		iio_chan->extend_name = bq25890_chans[i].datasheet_name;
		iio_chan->info_mask_separate =
			bq25890_chans[i].info_mask;
		iio_chan->type = bq25890_chans[i].type;
		iio_chan->address = i;
	}

	indio_dev->info = bq25890_iio_info;

	return 0;
}

int get_iio_channel(struct bq25890_device *bq, const char *propname,
                                        struct iio_channel **chan)
{
        int ret = 0;


		ret = of_property_match_string(bq->dev->of_node,
                                        "io-channel-names", propname);
		if (ret < 0) {
			dev_err(bq->dev, "Unable to read property %d %s\n", ret,
				"io-channel-names");

			return ret;
		}

        *chan = iio_channel_get(bq->dev, propname);
        if (IS_ERR(*chan)) {
                ret = PTR_ERR(*chan);
                if (ret != -EPROBE_DEFER)
                        pr_err("%s channel unavailable, %d\n", propname, ret);
                *chan = NULL;
        }

        return ret;
}

int get_sw_charger_chip_id(void)
{
	return sw_charger_chip_id;
}
EXPORT_SYMBOL(get_sw_charger_chip_id);

static int bq25890_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	struct device *dev = &client->dev;
	struct bq25890_device *bq;
	int ret;
	int i;
	struct iio_dev *indio_dev;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(dev, sizeof(*bq));
	if (!indio_dev)
		return -ENOMEM;

	bq = iio_priv(indio_dev);

	bq->client = client;
	bq->dev = dev;

	mutex_init(&bq->lock);

	bq->rmap = devm_regmap_init_i2c(client, &bq25890_regmap_config);
	if (IS_ERR(bq->rmap)) {
		pr_err("failed to allocate register map\n");
		return PTR_ERR(bq->rmap);
	}

	for (i = 0; i < ARRAY_SIZE(bq25890_reg_fields); i++) {
		const struct reg_field *reg_fields = bq25890_reg_fields;

		bq->rmap_fields[i] = devm_regmap_field_alloc(dev, bq->rmap,
							     reg_fields[i]);
		if (IS_ERR(bq->rmap_fields[i])) {
			pr_err("cannot allocate regmap field\n");
			return PTR_ERR(bq->rmap_fields[i]);
		}
	}

	i2c_set_clientdata(client, bq);

	bq->chip_id = bq25890_field_read(bq, F_PN);
	if (bq->chip_id < 0) {
		pr_err("Cannot read chip ID.\n");
		return bq->chip_id;
	}
	sw_charger_chip_id = bq->chip_id;
	pr_err("bq25890-------Chip with ID=%d\n", bq->chip_id);

#if 0
	if ((bq->chip_id != BQ25890_ID) && (bq->chip_id != BQ25895_ID)
			&& (bq->chip_id != BQ25896_ID)) {
		dev_err(dev, "Chip with ID=%d, not supported!\n", bq->chip_id);
		return -ENODEV;
	}
#endif

	if (!dev->platform_data) {
		ret = bq25890_fw_probe(bq);
		if (ret < 0) {
			pr_err("Cannot read device properties.\n");
			return ret;
		}
	} else {
		return -ENODEV;
	}

	ret = bq25890_hw_init(bq);
	if (ret < 0) {
		pr_err("Cannot initialize the chip.\n");
		return ret;
	}
	pr_err("bq25890 hw init success\n");

	if (client->irq <= 0)
		client->irq = bq25890_irq_probe(bq);

	if (client->irq < 0) {
		pr_err("No irq resource found.\n");
		return client->irq;
	}

	INIT_DELAYED_WORK(&bq->dumpic_work, bq25890_dumpic_work);
	schedule_delayed_work(&bq->dumpic_work, msecs_to_jiffies(10000));

	INIT_DELAYED_WORK(&bq->detect_vbat_set_vindpm_work, bq25890_detect_vbat_set_vindpm_work);
	INIT_DELAYED_WORK(&bq->detect_float_work, bq25890_detect_float_work);
	/* OTG reporting */
	bq->usb_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
	if (!IS_ERR_OR_NULL(bq->usb_phy)) {
		INIT_WORK(&bq->usb_work, bq25890_usb_work);
		bq->usb_nb.notifier_call = bq25890_usb_notifier;
		usb_register_notifier(bq->usb_phy, &bq->usb_nb);
		pr_err("bq25890 usb_phy found.\n");
	}

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					bq25890_irq_handler_thread,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					BQ25890_IRQ_PIN, bq);
	if (ret) {
		pr_err("irq request failed!");
		goto usb_fail;
	}

	bq->nchannels = ARRAY_SIZE(bq25890_chans);
	ret = bq25890_iio_probe_init(bq, indio_dev, bq25890_chans, &bq25890_iio_info);

	ret = bq25890_usb_power_supply_init(bq);
	if (ret < 0) {
		dev_err(dev, "Failed to register usb power supply\n");
		goto irq_fail;
	}
	dev_err(dev, "bq25890 probe usb init success\n");

	ret = bq25890_power_supply_init(bq);
	if (ret < 0) {
		pr_err("Failed to register power supply\n");
		goto psy_fail;
	}

	indio_dev->dev.parent = &client->dev;
	indio_dev->dev.of_node = client->dev.of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = bq->bq25890_iio_chan_ids;
	indio_dev->num_channels = bq->nchannels;
	//indio_dev->name = "bq25890_iio";

	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret) {
		dev_err(dev, "iio device register failed ret=%d\n", ret);
		goto psy_fail;
	}

	ret = get_iio_channel(bq, "quiet_therm", &bq->board_therm_channel);
	if (ret < 0) {
		dev_err(dev, "get_iio_channel quiet_therm fail");
		//goto psy_fail;
	}

	// INIT_DELAYED_WORK(&bq->board_therm_work, board_therm);
	// schedule_delayed_work(&bq->board_therm_work, msecs_to_jiffies(10000));
	INIT_DELAYED_WORK(&bq->xm_prop_change_work, generate_xm_charge_uvent);
	schedule_delayed_work(&bq->xm_prop_change_work, msecs_to_jiffies(30000));

	dev_err(dev, "bq25890 probe power_supply init success\n");

	bq->fake_battery_id = 1; //fake batt_id for P0.1
	ret = usb_debug_class_init(bq);
	if (ret < 0) {
		dev_err(dev, "Failed to register usb debug\n");
		goto init_fail;
	}

	bq25890_irq_handler_thread(client->irq, (void *) bq);

	g_bq = bq;

	return 0;


init_fail:
	if (bq && bq->charger)
		power_supply_unregister(bq->charger);
psy_fail:
	if ((bq && bq->usb))
		power_supply_unregister(bq->usb);
irq_fail:
	devm_free_irq(dev, client->irq,bq);
usb_fail:
	if (!IS_ERR_OR_NULL(bq->usb_phy))
		usb_unregister_notifier(bq->usb_phy, &bq->usb_nb);

	return ret;
}

static int bq25890_remove(struct i2c_client *client)
{
	struct bq25890_device *bq = i2c_get_clientdata(client);

	power_supply_unregister(bq->charger);

	if (!IS_ERR_OR_NULL(bq->usb_phy))
		usb_unregister_notifier(bq->usb_phy, &bq->usb_nb);

	/* reset all registers to default values */
	bq25890_chip_reset(bq);

	return 0;
}

static void bq25890_shutdown(struct i2c_client *client)
{
	int ret = 0;
	struct bq25890_device *bq = i2c_get_clientdata(client);

	pr_err("bq25890 shutdown success\n");
	/* reset all registers to default values */
	//bq25890_chip_reset(bq);

	/* disable HVDCP */
	ret = bq25890_field_write(bq, F_HVDCP_EN, 0);
	if (ret < 0)
		pr_err("Disabling hvdcp failed %d\n", ret);

	/* set vreg 4448mv */
	bq25890_field_write(bq, F_VREG, 38);

	/* run relative setting */
	ret = bq25890_field_write(bq, F_FORCE_VINDPM, 0);
	if (ret < 0)
		pr_err("run relative setting failed %d\n", ret);

	/* close adc */
	ret = bq25890_field_write(bq, F_CONV_RATE, 0);
	if (ret < 0)
		pr_err("bq25890 conv rate failed\n");

	/*disable otg*/
	ret = bq25890_field_write(bq, F_OTG_CFG, FALSE);
	ret = bq25890_field_write(bq, F_CHG_CFG, TRUE);

	ret = bq25890_field_write(bq, F_AUTO_DPDM_EN, 1);
	if (ret < 0)
		pr_err("bq25890 enable auto dpdm failed\n");

	bq25890_dump_register(bq);
}

#ifdef CONFIG_PM_SLEEP
static int bq25890_suspend(struct device *dev)
{
	struct bq25890_device *bq = dev_get_drvdata(dev);

	/*
	 * If charger is removed, while in suspend, make sure ADC is diabled
	 * since it consumes slightly more power.
	 */
	return bq25890_field_write(bq, F_CONV_START, 0);
}

static int bq25890_resume(struct device *dev)
{
	int ret;
	struct bq25890_state state;
	struct bq25890_device *bq = dev_get_drvdata(dev);

	ret = bq25890_get_chip_state(bq, &state);
	if (ret < 0)
		return ret;

	mutex_lock(&bq->lock);
	bq->state = state;
	mutex_unlock(&bq->lock);

	/* Re-enable ADC only if charger is plugged in. */
	if (state.online) {
		ret = bq25890_field_write(bq, F_CONV_START, 1);
		if (ret < 0)
			return ret;
	}

	/* signal userspace, maybe state changed while suspended */
	if (bq->charger) {
		power_supply_changed(bq->charger);
		if (IS_ERR(bq->charger)) {
			pr_err("Cannot get bq->charger,err_line=%d\n", __LINE__);
			PTR_ERR(bq->charger);
		}
	}

	return 0;
}
#endif

static const struct dev_pm_ops bq25890_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(bq25890_suspend, bq25890_resume)
};

static const struct i2c_device_id bq25890_i2c_ids[] = {
	{ "bq25890", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq25890_i2c_ids);

static const struct of_device_id bq25890_of_match[] = {
	{ .compatible = "ti,bq25890_chg", },
	{ },
};
MODULE_DEVICE_TABLE(of, bq25890_of_match);

static const struct acpi_device_id bq25890_acpi_match[] = {
	{"BQ258900", 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, bq25890_acpi_match);

static struct i2c_driver bq25890_driver = {
	.driver = {
		.name = "bq25890-charger",
		.of_match_table = of_match_ptr(bq25890_of_match),
		.acpi_match_table = ACPI_PTR(bq25890_acpi_match),
		.pm = &bq25890_pm,
	},
	.probe = bq25890_probe,
	.remove = bq25890_remove,
	.shutdown = bq25890_shutdown,
	.id_table = bq25890_i2c_ids,
};
module_i2c_driver(bq25890_driver);

MODULE_AUTHOR("Laurentiu Palcu <laurentiu.palcu@intel.com>");
MODULE_DESCRIPTION("bq25890 charger driver");
MODULE_LICENSE("GPL");
