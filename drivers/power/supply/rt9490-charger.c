// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/bits.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/phy/phy.h>

#include "charger_class.h"
#include "mtk_charger.h"

static bool dbg_log_en = true;
module_param(dbg_log_en, bool, 0644);

#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_info(dev, "%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

#define RT9490_REG_SYS_MIN	0x00
#define RT9490_REG_VCHG_CTRL	0x01
#define RT9490_REG_ICHG_CTRL	0x03
#define RT9490_REG_MIVR_CTRL	0x05
#define RT9490_REG_AICR_CTRL	0x06
#define RT9490_REG_PRE_CHG	0x08
#define RT9490_REG_EOC_CTRL	0x09
#define RT9490_REG_RECHG	0x0A
#define RT9490_REG_VOTG		0x0B
#define RT9490_REG_IOTG		0x0D
#define RT9490_REG_SFTMR_CTRL	0x0E
#define RT9490_REG_CHG_CTRL0	0x0F
#define RT9490_REG_CHG_CTRL1	0x10
#define RT9490_REG_CHG_CTRL2	0x11
#define RT9490_REG_CHG_CTRL3	0x12
#define RT9490_REG_CHG_CTRL4	0x13
#define RT9490_REG_CHG_CTRL5	0x14
#define RT9490_REG_THREG_CTRL	0x16
#define RT9490_REG_JEITA_CTRL1	0x18
#define RT9490_REG_AICC_CTRL	0x19
#define RT9490_REG_CHG_STAT0	0x1B
#define RT9490_REG_CHG_STAT1	0x1C
#define RT9490_REG_CHG_STAT2	0x1D
#define RT9490_REG_CHG_STAT4	0x1F
#define RT9490_REG_FAULT_STAT1	0x21
#define RT9490_REG_PUMP_EXP	0x49
#define RT9490_REG_ADD_CTRL0	0x4A

#define RT9490_OTGLBP_MASK	BIT(4)
#define RT9490_OTGOVP_MASK	BIT(5)
#define RT9490_OTGUVP_MASK	BIT(4)

#define RT9490_ICHG_MINUA	50000
#define RT9490_ICHG_MAXUA	5000000
#define RT9490_CV_MAXUV		1880000
#define RT9490_OTG_MINUV	2800000
#define RT9490_OTG_STEPUV	10000
#define RT9490_OTG_MAXUV	22000000
#define RT9490_OTG_N_VOLTAGES	1920
#define RT9490_AICR_WAITUS	4000
#define RT9490_VMIVR_MAXUV	22000000

#define NORMAL_CHARGING_CURR_UA	500000
#define FAST_CHARGING_CURR_UA	1500000
enum {
	RT9490_STAT_NOT_CHARGING = 0,
	RT9490_STAT_TRICKLE_CHARGE,
	RT9490_STAT_PRE_CHARGE,
	RT9490_STAT_FAST_CHARGE_CC,
	RT9490_STAT_FAST_CHARGE_CV,
	RT9490_STAT_IEOC,
	RT9490_STAT_BACKGROUND_CHARGE,
	RT9490_STAT_CHARGE_DONE,
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

enum rt9490_fields {
	F_VSYSMIN = 0,
	F_MIVR,
	F_VPRECHG,
	F_IPRECHG,
	F_RSTRG,
	F_IEOC,
	F_TRECHG,
	F_VRECHG,
	F_IOTG,
	F_EN_FST_TMR,
	F_EN_CHG,
	F_EN_AICC,
	F_FORCE_AICC,
	F_EN_HZ,
	F_EN_TE,
	F_VACOVP,
	F_WDRST,
	F_WATCHDOG,
	F_EN_FUSBDET,
	F_EN_BC12,
	F_SDRV_CTRL,
	F_SDRV_DLY,
	F_IBAT_REG,
	F_EN_AICR,
	F_EN_ILIM,
	F_THREG,
	F_TOTP,
	F_JEITA_DIS,
	F_MIVR_STAT,
	F_VAC1_PG,
	F_VBUS_PG,
	F_CHG_STAT,
	F_VBUS_STAT,
	F_AICC_STAT,
	F_PE_EN,
	F_PE_SEL,
	F_PE10_INC,
	F_PE20_CODE,
	F_AUTO_AICR,
	F_TD_EOC,
	F_EOC_RST,
	F_AUTO_MIVR,
	F_MAX_FIELDS
};

enum {
	RT9490_RANGE_VSYSMIN = 0,
	RT9490_RANGE_CV,
	RT9490_RANGE_ICHG,
	RT9490_RANGE_MIVR,
	RT9490_RANGE_AICR,
	RT9490_RANGE_IPRECHG,
	RT9490_RANGE_IEOC,
	RT9490_RANGE_IOTG,
	RT9490_RANGE_PE20_CODE,
	RT9490_MAX_RANGES
};

enum {
	RT9490_CABLE_NO_INPUT = 0,
	RT9490_CABLE_USB_SDP,
	RT9490_CABLE_USB_CDP,
	RT9490_CABLE_USB_DCP,
	RT9490_CABLE_UNKNOWN_DCP,
	RT9490_CABLE_NSTD,
	RT9490_CABLE_APPLE_TA,
	RT9490_CABLE_IN_OTG,
	RT9490_CABLE_BAD_ADAPTER,
	RT9490_CABLE_POWER_BY_VBUS = 11
};

enum rt9490_adc_chan {
	RT9490_ADC_TDIE,
	RT9490_ADC_TS,
	RT9490_ADC_VSYS,
	RT9490_ADC_VBAT,
	RT9490_ADC_VBUS,
	RT9490_ADC_IBAT,
	RT9490_ADC_IBUS,
	RT9490_ADC_VAC1,
	RT9490_ADC_VAC2,
	RT9490_ADC_DM,
	RT9490_ADC_DP,
	RT9490_ADC_MAX
};

enum rt9490_charging_stat {
	RT9490_CHG_STAT_READY = 0,
	RT9490_CHG_STAT_TRICKLE,
	RT9490_CHG_STAT_PRECHG,
	RT9490_CHG_STAT_FASTCHG_CC,
	RT9490_CHG_STAT_FASTCHG_CV,
	RT9490_CHG_STAT_IEOC,
	RT9490_CHG_STAT_BG_CHG,
	RT9490_CHG_STAT_CHG_DONE,
	RT9490_CHG_STAT_MAX,
};

enum rt9490_attach_trigger {
	ATTACH_TRIG_IGNORE,
	ATTACH_TRIG_PWR_RDY,
	ATTACH_TRIG_TYPEC,
};

enum rt9490_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

struct rt9490_chg_data {
	struct device *dev;
	struct regmap *regmap;
	struct regmap_field *rm_field[F_MAX_FIELDS];
	struct power_supply *psy;
	struct iio_channel *adc_chans[RT9490_ADC_MAX];
	const char *chg_name;
	struct charger_device *chgdev;
	struct mutex lock;
	struct mutex pe_lock;
	struct power_supply_desc chg_desc;
	enum power_supply_usb_type usb_type;
	unsigned int vbus_ready;
	struct completion aicc_done;
	atomic_t aicc_once;
	atomic_t attach;
	enum rt9490_attach_trigger attach_trig;
	u32 boot_mode;
	u32 boot_type;
	bool bc12_dn;
	struct workqueue_struct *wq;
	struct work_struct bc12_work;
	struct gpio_desc *ceb_gpio;
};

static const char *const rt9490_attach_trig_names[] = {
	"ignore", "pwr_rdy", "typec",
};

static const char *const rt9490_port_stat_names[] = {
	[RT9490_CABLE_NO_INPUT]		= "No Input",
	[RT9490_CABLE_USB_SDP]		= "SDP",
	[RT9490_CABLE_USB_CDP]		= "CDP",
	[RT9490_CABLE_USB_DCP]		= "DCP",
	[RT9490_CABLE_UNKNOWN_DCP]	= "Unknown DCP",
	[RT9490_CABLE_NSTD]		= "NSTD",
	[RT9490_CABLE_APPLE_TA]		= "Apple TA",
	[RT9490_CABLE_IN_OTG]		= "In OTG",
	[RT9490_CABLE_BAD_ADAPTER]	= "Bad Adapter",
	[RT9490_CABLE_POWER_BY_VBUS]	= "Power By Vbus",
};

static struct reg_field rt9490_reg_fields[] = {
	[F_VSYSMIN]	= REG_FIELD(RT9490_REG_SYS_MIN, 0, 5),
	[F_MIVR]	= REG_FIELD(RT9490_REG_MIVR_CTRL, 0, 7),
	[F_VPRECHG]	= REG_FIELD(RT9490_REG_PRE_CHG, 6, 7),
	[F_IPRECHG]	= REG_FIELD(RT9490_REG_PRE_CHG, 0, 5),
	[F_RSTRG]	= REG_FIELD(RT9490_REG_EOC_CTRL, 6, 6),
	[F_IEOC]	= REG_FIELD(RT9490_REG_EOC_CTRL, 0, 4),
	[F_TRECHG]	= REG_FIELD(RT9490_REG_RECHG, 4, 5),
	[F_VRECHG]	= REG_FIELD(RT9490_REG_RECHG, 0, 3),
	[F_IOTG]	= REG_FIELD(RT9490_REG_IOTG, 0, 6),
	[F_EN_FST_TMR]	= REG_FIELD(RT9490_REG_SFTMR_CTRL, 3, 3),
	[F_EN_CHG]	= REG_FIELD(RT9490_REG_CHG_CTRL0, 5, 5),
	[F_EN_AICC]	= REG_FIELD(RT9490_REG_CHG_CTRL0, 4, 4),
	[F_FORCE_AICC]	= REG_FIELD(RT9490_REG_CHG_CTRL0, 3, 3),
	[F_EN_HZ]	= REG_FIELD(RT9490_REG_CHG_CTRL0, 2, 2),
	[F_EN_TE]	= REG_FIELD(RT9490_REG_CHG_CTRL0, 1, 1),
	[F_VACOVP]	= REG_FIELD(RT9490_REG_CHG_CTRL1, 4, 5),
	[F_WDRST]	= REG_FIELD(RT9490_REG_CHG_CTRL1, 3, 3),
	[F_WATCHDOG]	= REG_FIELD(RT9490_REG_CHG_CTRL1, 0, 2),
	[F_EN_FUSBDET]	= REG_FIELD(RT9490_REG_CHG_CTRL2, 7, 7),
	[F_EN_BC12]	= REG_FIELD(RT9490_REG_CHG_CTRL2, 6, 6),
	[F_SDRV_CTRL]	= REG_FIELD(RT9490_REG_CHG_CTRL2, 1, 2),
	[F_SDRV_DLY]	= REG_FIELD(RT9490_REG_CHG_CTRL2, 0, 0),
	[F_IBAT_REG]	= REG_FIELD(RT9490_REG_CHG_CTRL5, 3, 4),
	[F_EN_AICR]	= REG_FIELD(RT9490_REG_CHG_CTRL5, 2, 2),
	[F_EN_ILIM]	= REG_FIELD(RT9490_REG_CHG_CTRL5, 1, 1),
	[F_THREG]	= REG_FIELD(RT9490_REG_THREG_CTRL, 6, 7),
	[F_TOTP]	= REG_FIELD(RT9490_REG_THREG_CTRL, 4, 5),
	[F_JEITA_DIS]	= REG_FIELD(RT9490_REG_JEITA_CTRL1, 0, 0),
	[F_MIVR_STAT]	= REG_FIELD(RT9490_REG_CHG_STAT0, 6, 6),
	[F_VAC1_PG]	= REG_FIELD(RT9490_REG_CHG_STAT0, 1, 1),
	[F_VBUS_PG]	= REG_FIELD(RT9490_REG_CHG_STAT0, 0, 0),
	[F_CHG_STAT]	= REG_FIELD(RT9490_REG_CHG_STAT1, 5, 7),
	[F_VBUS_STAT]	= REG_FIELD(RT9490_REG_CHG_STAT1, 1, 4),
	[F_AICC_STAT]	= REG_FIELD(RT9490_REG_CHG_STAT2, 6, 7),
	[F_PE_EN]	= REG_FIELD(RT9490_REG_PUMP_EXP, 7, 7),
	[F_PE_SEL]	= REG_FIELD(RT9490_REG_PUMP_EXP, 6, 6),
	[F_PE10_INC]	= REG_FIELD(RT9490_REG_PUMP_EXP, 5, 5),
	[F_PE20_CODE]	= REG_FIELD(RT9490_REG_PUMP_EXP, 0, 4),
	[F_AUTO_AICR]	= REG_FIELD(RT9490_REG_ADD_CTRL0, 5, 5),
	[F_TD_EOC]	= REG_FIELD(RT9490_REG_ADD_CTRL0, 4, 4),
	[F_EOC_RST]	= REG_FIELD(RT9490_REG_ADD_CTRL0, 3, 3),
	[F_AUTO_MIVR]	= REG_FIELD(RT9490_REG_ADD_CTRL0, 2, 2),
};

#define RT9490_LINEAR_RANGE(_idx, _min, _min_sel, _max_sel, _step) \
	[_idx] = REGULATOR_LINEAR_RANGE(_min, _min_sel, _max_sel, _step)

/* All converted to microvolt or microamp */
static const struct linear_range rt9490_ranges[RT9490_MAX_RANGES] = {
	RT9490_LINEAR_RANGE(RT9490_RANGE_VSYSMIN, 2500000, 0, 54, 250000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_CV, 3000000, 300, 1880, 10000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_ICHG, 50000, 5, 500, 10000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_MIVR, 3600000, 36, 220, 100000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_AICR, 100000, 10, 330, 10000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_IPRECHG, 40000, 1, 50, 40000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_IEOC, 40000, 1, 25, 40000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_IOTG, 120000, 3, 83, 40000),
	RT9490_LINEAR_RANGE(RT9490_RANGE_PE20_CODE, 5500000, 0, 29, 500000),
};

static const enum power_supply_property rt9490_charger_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_TYPE,
};

static const enum power_supply_usb_type rt9490_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
};

static char *rt9490_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

static int rt9490_get_be16_selector_to_value(struct rt9490_chg_data *data,
					     unsigned int reg,
					     unsigned int range_idx,
					     unsigned int *val)
{
	__be16 be16_sel;
	unsigned int sel;
	int ret;

	ret = regmap_raw_read(data->regmap, reg, &be16_sel, sizeof(be16_sel));
	if (ret)
		return ret;

	sel = be16_to_cpu(be16_sel);

	return linear_range_get_value(rt9490_ranges + range_idx, sel, val);
}

static int rt9490_set_value_to_be16_selector(struct rt9490_chg_data *data,
					     unsigned int reg,
					     unsigned int range_idx,
					     unsigned int val)
{
	unsigned int sel = 0;
	__be16 be16_sel;

	linear_range_get_selector_within(rt9490_ranges + range_idx, val, &sel);

	be16_sel = cpu_to_be16(sel);

	return regmap_raw_write(data->regmap, reg, &be16_sel, sizeof(be16_sel));
}

static int rt9490_charger_get_status(struct rt9490_chg_data *data,
				     union power_supply_propval *val)
{
	unsigned int status;
	int ret;

	ret = regmap_field_read(data->rm_field[F_CHG_STAT], &status);
	if (ret)
		return ret;

	switch (status) {
	case RT9490_STAT_NOT_CHARGING:
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case RT9490_STAT_CHARGE_DONE:
		val->intval = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
	}

	return 0;
}

static int rt9490_charger_get_charge_type(struct rt9490_chg_data *data,
					  union power_supply_propval *val)
{
	unsigned int status;
	int ret;

	ret = regmap_field_read(data->rm_field[F_CHG_STAT], &status);
	if (ret)
		return ret;

	switch (status) {
	case RT9490_STAT_NOT_CHARGING:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case RT9490_STAT_TRICKLE_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	default:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
	}

	return 0;
}

static int rt9490_charger_get_online(struct rt9490_chg_data *data,
				     union power_supply_propval *val)
{
	mutex_lock(&data->lock);
	val->intval = atomic_read(&data->attach);
	mutex_unlock(&data->lock);

	return 0;
}

static int rt9490_charger_get_adc(struct rt9490_chg_data *data, u8 chan_idx,
				  union power_supply_propval *val)
{
	int adc_val, ret;

	if (chan_idx >= RT9490_ADC_MAX)
		return -EINVAL;

	ret = iio_read_channel_processed(data->adc_chans[chan_idx], &adc_val);
	if (ret)
		return ret;

	val->intval = adc_val;
	return 0;
}

static int rt9490_charger_get_ichg(struct rt9490_chg_data *data,
				   union power_supply_propval *val)
{
	unsigned int value;
	int ret;

	ret = rt9490_get_be16_selector_to_value(data, RT9490_REG_ICHG_CTRL,
						RT9490_RANGE_ICHG, &value);
	if (ret)
		return ret;

	val->intval = value;
	return 0;
}

static int rt9490_charger_get_max_ichg(struct rt9490_chg_data *data,
				       union power_supply_propval *val)
{
	val->intval = RT9490_ICHG_MAXUA;
	return 0;
}

static int rt9490_charger_get_cv(struct rt9490_chg_data *data,
				 union power_supply_propval *val)
{
	unsigned int value;
	int ret;

	ret = rt9490_get_be16_selector_to_value(data, RT9490_REG_VCHG_CTRL,
						RT9490_RANGE_CV, &value);
	if (ret)
		return ret;

	val->intval = value;
	return 0;
}

static int rt9490_charger_get_max_cv(struct rt9490_chg_data *data,
				     union power_supply_propval *val)
{
	val->intval = RT9490_CV_MAXUV;
	return 0;
}

static int rt9490_charger_get_aicr(struct rt9490_chg_data *data,
				   union power_supply_propval *val)
{
	unsigned int value;
	int ret;

	ret = rt9490_get_be16_selector_to_value(data, RT9490_REG_AICR_CTRL,
						RT9490_RANGE_AICR, &value);
	if (ret)
		return ret;

	val->intval = value;
	return 0;
}

static int rt9490_charger_get_mivr(struct rt9490_chg_data *data,
				   union power_supply_propval *val)
{
	unsigned int sel, value;
	int ret;

	ret = regmap_field_read(data->rm_field[F_MIVR], &sel);
	if (ret)
		return ret;

	ret = linear_range_get_value(rt9490_ranges + RT9490_RANGE_MIVR, sel,
				     &value);
	if (ret)
		return ret;

	val->intval = value;
	return 0;
}

static int rt9490_charger_get_usb_type(struct rt9490_chg_data *data,
				       union power_supply_propval *val)
{
	mutex_lock(&data->lock);
	val->intval = data->usb_type;
	dev_info(data->dev, "%s: usb_type=%d\n", __func__, data->usb_type);
	mutex_unlock(&data->lock);

	return 0;
}

static int rt9490_charger_get_iprechg(struct rt9490_chg_data *data,
				      union power_supply_propval *val)
{
	unsigned int sel, value;
	int ret;

	ret = regmap_field_read(data->rm_field[F_IPRECHG], &sel);
	if (ret)
		return ret;

	ret = linear_range_get_value(rt9490_ranges + RT9490_RANGE_IPRECHG, sel,
				     &value);
	if (ret)
		return ret;

	val->intval = value;
	return 0;
}

static int rt9490_charger_get_ieoc(struct rt9490_chg_data *data,
				   union power_supply_propval *val)
{
	unsigned int sel, value;
	int ret;

	ret = regmap_field_read(data->rm_field[F_IEOC], &sel);
	if (ret)
		return ret;

	ret = linear_range_get_value(rt9490_ranges + RT9490_RANGE_IEOC, sel,
				     &value);
	if (ret)
		return ret;

	val->intval = value;
	return 0;
}

static int rt9490_charger_get_manufacturer(struct rt9490_chg_data *data,
					   union power_supply_propval *val)
{
	static const char *manufacturer = "Richtek Technology Corp";

	val->strval = manufacturer;
	return 0;
}

static void rt9490_chg_attach_pre_process(struct rt9490_chg_data *data,
					  enum rt9490_attach_trigger trig,
					  int attach)
{
	bool bc12_dn;
	int ret;

	ret = regmap_field_read(data->rm_field[F_VAC1_PG], &data->vbus_ready);
	if (ret) {
		dev_info(data->dev, "%s failed to get F_VAC1_PG(%d)\n",
			__func__, ret);
		return;
	}

	dev_info(data->dev, "trig=%s,attach=%d,vbus_ready=%d\n",
		 rt9490_attach_trig_names[trig], attach, data->vbus_ready);
	dev_info(data->dev, "data_bc12_dn=%d\n", data->bc12_dn);
	/* if attach trigger is not match, ignore it */
	if (data->attach_trig != trig) {
		dev_notice(data->dev, "trig=%s ignore\n",
			   rt9490_attach_trig_names[trig]);
		return;
	}

	if (attach == ATTACH_TYPE_NONE)
		data->bc12_dn = false;

	bc12_dn = data->bc12_dn;
	if (!bc12_dn)
		atomic_set(&data->attach, attach);

	if (attach > ATTACH_TYPE_PD && bc12_dn)
		return;

	if (!queue_work(data->wq, &data->bc12_work))
		dev_notice(data->dev, "bc12 work already queued\n");
}

static int rt9490_charger_set_ichg(struct rt9490_chg_data *data,
				   const union power_supply_propval *val)
{
	return rt9490_set_value_to_be16_selector(data, RT9490_REG_ICHG_CTRL,
						 RT9490_RANGE_ICHG,
						 val->intval);
}

static int rt9490_enable_charging(struct charger_device *chgdev, bool en);
static int rt9490_charger_set_cv(struct rt9490_chg_data *data,
				 const union power_supply_propval *val)
{
	union power_supply_propval vbat_val;
	int ret;

	ret = rt9490_charger_get_adc(data, RT9490_ADC_VBAT, &vbat_val);
	if (ret)
		return ret;
	if (val->intval < vbat_val.intval) {
		dev_notice(data->dev, "cv(%d)<vbat(%d), disable charging\n",
			   val->intval, vbat_val.intval);
		ret = rt9490_enable_charging(data->chgdev, false);
			return ret;
	}
	return rt9490_set_value_to_be16_selector(data, RT9490_REG_VCHG_CTRL,
						 RT9490_RANGE_CV,
						 val->intval);
}

static int rt9490_charger_set_aicr(struct rt9490_chg_data *data,
				   const union power_supply_propval *val)
{
	return rt9490_set_value_to_be16_selector(data, RT9490_REG_AICR_CTRL,
						 RT9490_RANGE_AICR,
						 val->intval);
}

static int rt9490_charger_set_mivr(struct rt9490_chg_data *data,
				   const union power_supply_propval *val)
{
	unsigned int sel = 0;

	linear_range_get_selector_within(rt9490_ranges + RT9490_RANGE_MIVR,
					 val->intval, &sel);
	return regmap_field_write(data->rm_field[F_MIVR], sel);
}

static int rt9490_charger_set_iprechg(struct rt9490_chg_data *data,
				      const union power_supply_propval *val)
{
	unsigned int sel = 0;

	linear_range_get_selector_within(rt9490_ranges + RT9490_RANGE_IPRECHG,
					 val->intval, &sel);
	return regmap_field_write(data->rm_field[F_IPRECHG], sel);
}

static int rt9490_charger_set_ieoc(struct rt9490_chg_data *data,
				   const union power_supply_propval *val)
{
	unsigned int sel = 0;

	linear_range_get_selector_within(rt9490_ranges + RT9490_RANGE_IEOC,
					 val->intval, &sel);
	return regmap_field_write(data->rm_field[F_IEOC], sel);
}

static int rt9490_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct rt9490_chg_data *data = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(data))
		return PTR_ERR_OR_ZERO(data);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return rt9490_charger_get_status(data, val);
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		return rt9490_charger_get_charge_type(data, val);
	case POWER_SUPPLY_PROP_ONLINE:
		return rt9490_charger_get_online(data, val);
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return rt9490_charger_get_adc(data, RT9490_ADC_VBUS, val);
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return rt9490_charger_get_adc(data, RT9490_ADC_IBUS, val);
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (data->chg_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = NORMAL_CHARGING_CURR_UA;
		else if (data->chg_desc.type == POWER_SUPPLY_TYPE_USB_DCP)
			val->intval = FAST_CHARGING_CURR_UA;
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return rt9490_charger_get_ichg(data, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return rt9490_charger_get_max_ichg(data, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return rt9490_charger_get_cv(data, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		return rt9490_charger_get_max_cv(data, val);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return rt9490_charger_get_aicr(data, val);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		return rt9490_charger_get_mivr(data, val);
	case POWER_SUPPLY_PROP_USB_TYPE:
		return rt9490_charger_get_usb_type(data, val);
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		return rt9490_charger_get_iprechg(data, val);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return rt9490_charger_get_ieoc(data, val);
	case POWER_SUPPLY_PROP_MANUFACTURER:
		return rt9490_charger_get_manufacturer(data, val);
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = data->chg_desc.type;
		return 0;
	default:
		return -ENODATA;
	}
}

static int rt9490_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct rt9490_chg_data *data = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(data))
		return PTR_ERR_OR_ZERO(data);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&data->lock);
		rt9490_chg_attach_pre_process(data, ATTACH_TRIG_TYPEC,
					      val->intval);
		mutex_unlock(&data->lock);
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return rt9490_charger_set_ichg(data, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return rt9490_charger_set_cv(data, val);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return rt9490_charger_set_aicr(data, val);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		return rt9490_charger_set_mivr(data, val);
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		return rt9490_charger_set_iprechg(data, val);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return rt9490_charger_set_ieoc(data, val);
	default:
		return -EINVAL;
	}
}

/* MTK Charger Interface */
static int rt9490_psy_prop_ops(struct rt9490_chg_data *data, bool is_set,
				enum power_supply_property psp, uint32_t *value)
{
	union power_supply_propval val;
	int ret = 0;

	if (is_set) {
		val.intval = *value;
		ret = power_supply_set_property(data->psy, psp, &val);
		if (ret)
			return ret;
	} else {
		ret = power_supply_get_property(data->psy, psp, &val);
		if (ret)
			return ret;

		*value = val.intval;
	}

	return 0;
}

static int rt9490_plug_in(struct charger_device *chgdev)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);

	/* Set WDT 40s */
	return regmap_field_write(data->rm_field[F_WATCHDOG], 5);
}

static int rt9490_plug_out(struct charger_device *chgdev)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	uint32_t value = 0;
	int ret;

	dev_info(data->dev, "%s: ++\n", __func__);
	atomic_set(&data->aicc_once, 0);
	ret = regmap_field_write(data->rm_field[F_EN_AICC], 0);
	if (ret) {
		dev_info(data->dev, "Failed to disable aicc\n");
		return ret;
	}
	regmap_field_write(data->rm_field[F_WATCHDOG], 0);
	if (ret) {
		dev_info(data->dev, "Failed to disable watchdog\n");
		return ret;
	}
	return rt9490_psy_prop_ops(data, true, POWER_SUPPLY_PROP_ONLINE,
				   &value);
}

static int rt9490_is_charging_enable(struct charger_device *chgdev, bool *en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	unsigned int val;
	int ret;

	dev_info(data->dev, "%s: ++\n", __func__);
	ret = regmap_field_read(data->rm_field[F_EN_CHG], &val);
	if (ret)
		return ret;

	*en = val;
	return 0;
}

static int rt9490_init_chip(struct charger_device *chg_dev)
{
	return 0;
}

static int rt9490_kick_wdt(struct charger_device *chgdev)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	/* Trigger watchdog time reset */
	return regmap_field_write(data->rm_field[F_WDRST], 1);
}

#define DUMP_REG_BUF_SIZE	1024
static int rt9490_dump_registers(struct charger_device *chgdev)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	int ret, i;
	u32 val;
	char buf[DUMP_REG_BUF_SIZE] = "\0";
	static const struct {
		const char *name;
		const char *unit;
		enum power_supply_property psp;
	} settings[] = {
		{ .psp = POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
		  .name = "ICHG", .unit = "mA" },
		{ .psp = POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
		  .name = "AICR", .unit = "mA" },
		{ .psp = POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
		  .name = "MIVR", .unit = "mV" },
		{ .psp = POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
		  .name = "IEOC", .unit = "mA" },
		{ .psp = POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
		  .name = "CV", .unit = "mV" },
	};
	static const struct {
		const char *name;
		const char *unit;
		enum rt9490_adc_chan chan;
	} adcs[] = {
		{ .chan = RT9490_ADC_VBUS, .name = "VBUS", .unit = "mV" },
		{ .chan = RT9490_ADC_IBUS, .name = "IBUS", .unit = "mA" },
		{ .chan = RT9490_ADC_VBAT, .name = "VBAT", .unit = "mV" },
		{ .chan = RT9490_ADC_IBAT, .name = "IBAT", .unit = "mA" },
		{ .chan = RT9490_ADC_VSYS, .name = "VSYS", .unit = "mV" },
	};
	static const struct {
		const u8 reg;
		const char *name;
	} regs[] = {
		{ .reg = RT9490_REG_CHG_CTRL0, .name = "CHG_CTRL0" },
		{ .reg = RT9490_REG_CHG_CTRL1, .name = "CHG_CTRL1" },
		{ .reg = RT9490_REG_CHG_STAT0, .name = "CHG_STAT0" },
		{ .reg = RT9490_REG_CHG_STAT1, .name = "CHG_STAT1" },
	};

	for (i = 0; i < ARRAY_SIZE(settings); i++) {
		ret = rt9490_psy_prop_ops(data, false, settings[i].psp, &val);
		if (ret) {
			dev_err(data->dev, "failed to get %s\n",
				settings[i].name);
			return ret;
		}
		val /= 1000;
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
		ret = iio_read_channel_processed(data->adc_chans[adcs[i].chan],
						 &val);
		if (ret) {
			dev_err(data->dev, "failed to read adc %s\n",
				adcs[i].name);
			return ret;
		}

		if (i == ARRAY_SIZE(adcs) - 1)
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = %d%s\n", adcs[i].name, val,
				  adcs[i].unit);
		else
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = %d%s, ", adcs[i].name, val,
				  adcs[i].unit);
	}

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_read(data->regmap, regs[i].reg, &val);
		if (ret) {
			dev_err(data->dev, "failed to get %s\n", regs[i].name);
			return ret;
		}
		if (i == ARRAY_SIZE(regs) - 1)
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = 0x%02X\n", regs[i].name, val);
		else
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = 0x%02X, ", regs[i].name, val);
	}

	dev_info(data->dev, "%s %s", __func__, buf);

	ret = rt9490_kick_wdt(chgdev);
	if (ret)
		dev_info(data->dev, "Failed to kick watchdog\n");

	return ret;
}

static int rt9490_enable_charging(struct charger_device *chgdev, bool en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: en = %d\n", __func__, en);
	if (data->ceb_gpio)
		gpiod_set_value(data->ceb_gpio, !en);
	mdelay(1);
	return regmap_field_write(data->rm_field[F_EN_CHG], en);
}

static int rt9490_get_ichg(struct charger_device *chgdev, uint32_t *ichg)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	return rt9490_psy_prop_ops(data, false,
				   POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
				   ichg);
}

static int rt9490_set_ichg(struct charger_device *chgdev, uint32_t ichg)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ichg = %d\n", __func__, ichg);
	return rt9490_psy_prop_ops(data, true,
				   POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
				   &ichg);
}

static int rt9490_get_aicr(struct charger_device *chgdev, uint32_t *aicr)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	return rt9490_psy_prop_ops(data, false,
				   POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, aicr);
}

static int rt9490_set_aicr(struct charger_device *chgdev, uint32_t aicr)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: aicr = %d\n", __func__, aicr);
	return rt9490_psy_prop_ops(data, true,
				   POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
				   &aicr);
}

static int rt9490_get_min_aicr(struct charger_device *chgdev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int rt9490_get_cv(struct charger_device *chgdev, uint32_t *cv)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	return rt9490_psy_prop_ops(data, false,
				   POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
				   cv);
}

static int rt9490_set_cv(struct charger_device *chgdev, uint32_t cv)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: cv = %d\n", __func__, cv);
	return rt9490_psy_prop_ops(data, true,
				   POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
				   &cv);
}

static int rt9490_set_mivr(struct charger_device *chgdev, uint32_t mivr)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: mivr = %d\n", __func__, mivr);
	return rt9490_psy_prop_ops(data, true,
				   POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
				   &mivr);
}

static int rt9490_get_mivr(struct charger_device *chgdev, uint32_t *mivr)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	return rt9490_psy_prop_ops(data, false,
				   POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, mivr);
}

static int rt9490_is_charging_done(struct charger_device *chgdev, bool *done)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property(data->psy,
			POWER_SUPPLY_PROP_STATUS, &val);
	if (ret) {
		dev_err(data->dev, "%s: Failed to get chg status\n", __func__);
		return ret;
	}

	*done = val.intval == POWER_SUPPLY_STATUS_FULL;
	dev_info(data->dev, "%s: done = %d\n", __func__, *done);
	return ret;
}

static int rt9490_get_min_ichg(struct charger_device *chgdev, uint32_t *uA)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	*uA = RT9490_ICHG_MINUA;
	return 0;
}

static int rt9490_set_ieoc(struct charger_device *chgdev, uint32_t ieoc)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ieoc = %d\n", __func__, ieoc);
	return rt9490_psy_prop_ops(data, true,
				   POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
				   &ieoc);
}

static int rt9490_get_ieoc(struct charger_device *chgdev, uint32_t *ieoc)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	return rt9490_psy_prop_ops(data, false,
				   POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, ieoc);
}

static int rt9490_enable_te(struct charger_device *chgdev, bool en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: en = %d\n", __func__, en);
	return regmap_field_write(data->rm_field[F_EN_TE], en);
}

static int rt9490_reset_eoc_state(struct charger_device *chgdev)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ++\n", __func__);
	return regmap_field_write(data->rm_field[F_EOC_RST], 1);
}

static int rt9490_enable_hz(struct charger_device *chgdev, bool en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: en = %d\n", __func__, en);
	return regmap_field_write(data->rm_field[F_EN_HZ], en);
}

static int rt9490_set_vac_ovp(struct charger_device *chgdev, u32 uV)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: ovp_lvl = %d", __func__, uV);
	if (uV < 7000000)
		return regmap_field_write(data->rm_field[F_VACOVP], 0x3);
	else if (uV >= 7000000 && uV < 12000000)
		return regmap_field_write(data->rm_field[F_VACOVP], 0x2);
	else if (uV >= 12000000 && uV < 22000000)
		return regmap_field_write(data->rm_field[F_VACOVP], 0x1);
	else
		return regmap_field_write(data->rm_field[F_VACOVP], 0x0);
}

static int rt9490_enable_safety_timer(struct charger_device *chgdev, bool en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "%s: en = %d\n", __func__, en);
	return regmap_field_write(data->rm_field[F_EN_FST_TMR], en);
}

static int rt9490_is_safety_timer_enable(
		struct charger_device *chgdev, bool *en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	unsigned int val;
	int ret;

	dev_info(data->dev, "%s: ++\n", __func__);
	ret = regmap_field_read(data->rm_field[F_EN_FST_TMR], &val);
	if (ret)
		return ret;

	*en = val;
	return 0;
}

static int rt9490_enable_power_path(struct charger_device *chgdev, bool en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	u32 mivr = (en ? 4500000 : RT9490_VMIVR_MAXUV);

	dev_info(data->dev, "%s: en = %d\n", __func__, en);
	return rt9490_set_mivr(chgdev, mivr);
}

static int rt9490_is_power_path_enable(struct charger_device *chgdev, bool *en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	unsigned int val;
	int ret;

	dev_info(data->dev, "%s: ++\n", __func__);
	ret = regmap_field_read(data->rm_field[F_EN_HZ], &val);
	if (ret)
		return ret;

	*en = !val;
	return 0;
}

static int rt9490_enable_otg(struct charger_device *chgdev, bool en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	struct regulator *regulator;
	int ret;

	dev_info(data->dev, "%s: en = %d\n", __func__, en);
	regulator = devm_regulator_get(data->dev, "rt9490-otg-vbus");
	if (IS_ERR(regulator)) {
		dev_err(data->dev, "failed to get otg regulator\n");
		return PTR_ERR(regulator);
	}
	ret = en ? regulator_enable(regulator) : regulator_disable(regulator);
	devm_regulator_put(regulator);
	return ret;
}

static int rt9490_set_otg_cc(struct charger_device *chgdev, u32 uA)
{
	int ret;
	struct regulator *regulator;
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	dev_info(data->dev, "uA = %d\n", uA);
	regulator = devm_regulator_get(data->dev, "rt9490-otg-vbus");
	if (IS_ERR(regulator)) {
		dev_err(data->dev, "failed to get otg regulator\n");
		return PTR_ERR(regulator);
	}
	ret = regulator_set_current_limit(regulator, uA, uA);
	devm_regulator_put(regulator);
	return ret;
}

static int rt9490_run_pe(struct rt9490_chg_data *data, bool pe20)
{
	int ret = 0;
	unsigned long timeout = pe20 ? 1400 : 2800;
	uint32_t value;
	unsigned int pumpx_en;

	value = 800000;
	ret = rt9490_psy_prop_ops(data, true,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &value);
	if (ret)
		return ret;

	value = 2000000;
	ret = rt9490_psy_prop_ops(data, true,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &value);
	if (ret)
		return ret;

	ret = regmap_field_write(data->rm_field[F_EN_CHG], 1);
	if (ret)
		return ret;

	ret = regmap_field_write(data->rm_field[F_PE_SEL], pe20);
	if (ret)
		return ret;

	ret = regmap_field_write(data->rm_field[F_PE_EN], 1);
	if (ret)
		return ret;

	msleep(timeout);

	/* Each round wait 50ms, timeout 1000ms */
	ret = regmap_field_read_poll_timeout(data->rm_field[F_PE_EN], pumpx_en,
					     !pumpx_en, 50000, 1000000);
	if (ret)
		dev_err(data->dev, "Failed to wait pe (%d)\n", ret);

	return ret;
}

static int rt9490_set_pep_current_pattern(struct charger_device *chgdev,
					  bool inc)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	int ret;

	dev_info(data->dev, "%s: inc = %d\n", __func__, inc);
	mutex_lock(&data->pe_lock);
	ret = regmap_field_write(data->rm_field[F_PE10_INC], inc);
	if (ret) {
		dev_err(data->dev, "Failed to set pe10 up/down\n");
		goto out;
	}
	ret = rt9490_run_pe(data, false);
out:
	mutex_unlock(&data->pe_lock);
	return ret;
}

static int rt9490_set_pep20_efficiency_table(struct charger_device *chgdev)
{
	return 0;
}

static int rt9490_set_pep20_current_pattern(struct charger_device *chgdev,
					    uint32_t uV)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	unsigned int sel = 0;
	int ret;

	dev_info(data->dev, "%s: pe20 = %d\n", __func__, uV);
	mutex_lock(&data->pe_lock);

	linear_range_get_selector_within(rt9490_ranges + RT9490_RANGE_PE20_CODE,
					 uV, &sel);
	ret = regmap_field_write(data->rm_field[F_PE20_CODE], sel);
	if (ret) {
		dev_err(data->dev, "Failed to set pe20 code\n");
		goto out;
	}

	ret = rt9490_run_pe(data, true);
out:
	mutex_unlock(&data->pe_lock);
	return ret;
}

static int rt9490_set_pep20_reset(struct charger_device *chgdev)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	uint32_t value;
	int ret;

	dev_info(data->dev, "++\n");
	mutex_lock(&data->pe_lock);

	value = 4600000;
	ret = rt9490_psy_prop_ops(data, true,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT, &value);
	if (ret)
		goto out;

	value = 100000;
	ret = rt9490_psy_prop_ops(data, true,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &value);
	if (ret)
		goto out;

	msleep(250);

	value = 500000;
	ret = rt9490_psy_prop_ops(data, true,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &value);
out:
	mutex_unlock(&data->pe_lock);
	return ret;
}

static int rt9490_get_tchg(struct charger_device *chgdev, int *tmin, int *tmax)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	int adc_val, ret;

	ret = iio_read_channel_processed(data->adc_chans[RT9490_ADC_TDIE],
								&adc_val);
	if (ret)
		return ret;

	dev_info(data->dev, "tchg = %d\n", adc_val);
	*tmin = *tmax = adc_val;
	return ret;
}

static int rt9490_get_adc(struct charger_device *chgdev, enum adc_channel chan,
			  int *min, int *max)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	enum rt9490_adc_chan adc_chan;
	int adc_val, ret;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		adc_chan = RT9490_ADC_VBUS;
		break;
	case ADC_CHANNEL_VSYS:
		adc_chan = RT9490_ADC_VSYS;
		break;
	case ADC_CHANNEL_VBAT:
		adc_chan = RT9490_ADC_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		adc_chan = RT9490_ADC_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		adc_chan = RT9490_ADC_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		adc_chan = RT9490_ADC_TDIE;
		break;
	default:
		return -EINVAL;
	}

	ret = iio_read_channel_processed(data->adc_chans[adc_chan], &adc_val);
	if (ret) {
		dev_err(data->dev, "Failed to read adc\n");
		return ret;
	}

	*max = *min = adc_val * 1000;
	return 0;
}

static int rt9490_get_vbus(struct charger_device *chgdev, uint32_t *vbus)
{
	return rt9490_get_adc(chgdev, ADC_CHANNEL_VBUS, vbus, vbus);
}

static int rt9490_get_ibat(struct charger_device *chgdev, uint32_t *ibat)
{
	return rt9490_get_adc(chgdev, ADC_CHANNEL_IBAT, ibat, ibat);
}

static int rt9490_get_ibus(struct charger_device *chgdev, uint32_t *ibus)
{
	return rt9490_get_adc(chgdev, ADC_CHANNEL_IBUS, ibus, ibus);
}

static const struct charger_properties rt9490_chg_props = {
	.alias_name = "rt9490_chg",
};

static int rt9490_do_event(struct charger_device *chgdev,
			   uint32_t event, uint32_t args)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(data->psy);
		break;
	default:
		break;
	}
	return 0;
}

static int rt9490_get_mivr_state(struct charger_device *chgdev, bool *active)
{
	int ret;
	unsigned int val;
	struct rt9490_chg_data *data = charger_get_data(chgdev);

	ret = regmap_field_read(data->rm_field[F_MIVR_STAT], &val);
	if (ret)
		return ret;
	*active = val;
	return 0;
}

static int rt9490_charger_get_aicc(struct rt9490_chg_data *data, u32 *val)
{
	unsigned int value;
	int ret;

	ret = rt9490_get_be16_selector_to_value(data, RT9490_REG_AICC_CTRL,
						RT9490_RANGE_AICR, &value);
	if (ret)
		return ret;

	*val = value;
	return 0;
}

static int rt9490_run_aicc(struct charger_device *chgdev, u32 *uA)
{
	int ret;
	bool active = false;
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	long ret_comp;
	unsigned int aicc_stat = 0;
	u32 val = 0;

	ret = rt9490_get_mivr_state(chgdev, &active);
	if (ret)
		return ret;
	if (!active) {
		dev_notice(data->dev, "mivr loop is not active\n");
		return 0;
	}

	mutex_lock(&data->pe_lock);
	if (!atomic_read(&data->aicc_once)) {
		atomic_set(&data->aicc_once, 1);
		ret = regmap_field_write(data->rm_field[F_EN_AICC], 1);
		if (ret)
			goto out;
	} else {
		ret = regmap_field_write(data->rm_field[F_FORCE_AICC], 1);
		if (ret)
			goto out;
	}
	reinit_completion(&data->aicc_done);

	ret_comp = wait_for_completion_interruptible_timeout(&data->aicc_done,
		   msecs_to_jiffies(7000));
	if (ret_comp == 0)
		ret = -ETIMEDOUT;
	else if (ret_comp < 0)
		ret = -EINTR;
	else
		ret = 0;
	if (ret < 0) {
		dev_err(data->dev, "failed to wait aicc(%d)\n", ret);
		goto out;
	}

	ret = regmap_field_read(data->rm_field[F_AICC_STAT], &aicc_stat);
	if (ret)
		goto out;

	/* maximum input current detected */
	if (aicc_stat == 2) {
		ret = rt9490_charger_get_aicc(data, &val);
		if (ret)
			goto out;
		*uA = val;
	}
	dev_info(data->dev, "%s IAICC = 0x%04X\n", __func__, val);

out:
	mutex_unlock(&data->pe_lock);
	return ret;
}

static int rt9490_enable_chg_type_det(struct charger_device *chgdev, bool en)
{
	struct rt9490_chg_data *data = charger_get_data(chgdev);
	int attach = en ? ATTACH_TYPE_TYPEC : ATTACH_TYPE_NONE;

	dev_info(data->dev, "%s en=d\n", __func__, en);
	mutex_lock(&data->lock);
	rt9490_chg_attach_pre_process(data, ATTACH_TRIG_TYPEC, attach);
	mutex_unlock(&data->lock);
	return 0;
}

static const struct charger_ops rt9490_chg_ops = {
	/* Normal charging */
	.plug_in = rt9490_plug_in,
	.plug_out = rt9490_plug_out,
	.dump_registers = rt9490_dump_registers,
	.enable = rt9490_enable_charging,
	.is_enabled = rt9490_is_charging_enable,
	.init_chip = rt9490_init_chip,
	.get_charging_current = rt9490_get_ichg,
	.set_charging_current = rt9490_set_ichg,
	.get_input_current = rt9490_get_aicr,
	.set_input_current = rt9490_set_aicr,
	.get_min_input_current = rt9490_get_min_aicr,
	.get_constant_voltage = rt9490_get_cv,
	.set_constant_voltage = rt9490_set_cv,
	.kick_wdt = rt9490_kick_wdt,
	.set_mivr = rt9490_set_mivr,
	.get_mivr = rt9490_get_mivr,
	.get_mivr_state = rt9490_get_mivr_state,
	.is_charging_done = rt9490_is_charging_done,
	.get_min_charging_current = rt9490_get_min_ichg,
	.set_eoc_current = rt9490_set_ieoc,
	.get_eoc_current = rt9490_get_ieoc,
	.enable_termination = rt9490_enable_te,
	.run_aicl = rt9490_run_aicc,
	.reset_eoc_state = rt9490_reset_eoc_state,
	.enable_hz = rt9490_enable_hz,
	.set_vac_ovp = rt9490_set_vac_ovp,

	/* Safety timer */
	.enable_safety_timer = rt9490_enable_safety_timer,
	.is_safety_timer_enabled = rt9490_is_safety_timer_enable,

	/* Power path */
	.enable_powerpath = rt9490_enable_power_path,
	.is_powerpath_enabled = rt9490_is_power_path_enable,

	/* OTG */
	.enable_otg = rt9490_enable_otg,
	.set_boost_current_limit = rt9490_set_otg_cc,

	/* PE+/PE+20 */
	.send_ta_current_pattern = rt9490_set_pep_current_pattern,
	.set_pe20_efficiency_table = rt9490_set_pep20_efficiency_table,
	.send_ta20_current_pattern = rt9490_set_pep20_current_pattern,
	.reset_ta = rt9490_set_pep20_reset,

	/* ADC */
	.get_tchg_adc = rt9490_get_tchg,
	.get_adc = rt9490_get_adc,
	.get_vbus_adc = rt9490_get_vbus,
	.get_ibat_adc = rt9490_get_ibat,
	.get_ibus_adc = rt9490_get_ibus,

	/* charger type detection */
	.enable_chg_type_det = rt9490_enable_chg_type_det,

	/* Event */
	.event = rt9490_do_event,
};

static int rt9490_otg_set_voltage_sel(struct regulator_dev *rdev,
				      unsigned int selector)
{
	struct rt9490_chg_data *data = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	__be16 be16_sel = cpu_to_be16(selector);

	return regmap_raw_write(data->regmap, desc->vsel_reg, &be16_sel,
				sizeof(be16_sel));
}

static int rt9490_otg_get_voltage_sel(struct regulator_dev *rdev)
{
	struct rt9490_chg_data *data = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	__be16 be16_sel;
	int ret;

	ret = regmap_raw_read(data->regmap, desc->vsel_reg, &be16_sel,
			      sizeof(be16_sel));
	if (ret)
		return ret;

	return be16_to_cpu(be16_sel);
}

static int rt9490_otg_set_current_limit(struct regulator_dev *rdev, int min_uA,
					int max_uA)
{
	struct rt9490_chg_data *data = rdev_get_drvdata(rdev);
	unsigned int sel = 0;

	linear_range_get_selector_within(rt9490_ranges + RT9490_RANGE_IOTG,
					 max_uA, &sel);
	return regmap_field_write(data->rm_field[F_IOTG], sel);
}

static int rt9490_otg_get_current_limit(struct regulator_dev *rdev)
{
	struct rt9490_chg_data *data = rdev_get_drvdata(rdev);
	unsigned int sel, cval;
	int ret;

	ret = regmap_field_read(data->rm_field[F_IOTG], &sel);
	if (ret)
		return ret;

	ret = linear_range_get_value(rt9490_ranges + RT9490_RANGE_IOTG, sel,
				     &cval);
	if (ret)
		return ret;

	return cval;
}

static int rt9490_otg_get_error_flags(struct regulator_dev *rdev,
				      unsigned int *flags)
{
	struct rt9490_chg_data *data = rdev_get_drvdata(rdev);
	unsigned int events = 0, lbp_status, fault_status;
	int ret;

	ret = regmap_read(data->regmap, RT9490_REG_CHG_STAT4, &lbp_status);
	if (ret)
		return ret;

	ret = regmap_read(data->regmap, RT9490_REG_FAULT_STAT1, &fault_status);
	if (ret)
		return ret;

	if (lbp_status & RT9490_OTGLBP_MASK)
		events |= REGULATOR_ERROR_FAIL;

	if (fault_status & RT9490_OTGUVP_MASK)
		events |= REGULATOR_ERROR_UNDER_VOLTAGE;

	if (fault_status & RT9490_OTGOVP_MASK)
		events |= REGULATOR_ERROR_REGULATION_OUT;

	*flags = events;
	return 0;
}

static const struct regulator_ops rt9490_otg_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = rt9490_otg_set_voltage_sel,
	.get_voltage_sel = rt9490_otg_get_voltage_sel,
	.set_current_limit = rt9490_otg_set_current_limit,
	.get_current_limit = rt9490_otg_get_current_limit,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_error_flags = rt9490_otg_get_error_flags,
};

static const struct regulator_desc rt9490_otg_desc = {
	.name = "otg-vbus",
	.of_match = of_match_ptr("otg_vbus"),
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.min_uV = RT9490_OTG_MINUV,
	.uV_step = RT9490_OTG_STEPUV,
	.n_voltages = RT9490_OTG_N_VOLTAGES,
	.ops = &rt9490_otg_regulator_ops,
	.vsel_reg = RT9490_REG_VOTG,
	.vsel_mask = GENMASK(10, 0),
	.enable_reg = RT9490_REG_CHG_CTRL3,
	.enable_mask = BIT(6),
};

static int rt9490_do_charger_init(struct rt9490_chg_data *data)
{
	unsigned int vbus_ready, cv;
	union power_supply_propval val;
	int ret;
	u32 tmp;
	struct device_node *bc12_np, *boot_np, *np = data->dev->of_node;
	const struct {
		u32 size;
		u32 tag;
		u32 boot_mode;
		u32 boot_type;
	} *tag;

	ret = device_property_read_u32(data->dev, "richtek,cv-microvolt", &cv);
	if (!ret) {
		val.intval = cv;
		ret = rt9490_charger_set_cv(data, &val);
		if (ret)
			return ret;
	}

	/* mediatek chgdev name */
	ret = device_property_read_string(data->dev, "chg_name",
						&data->chg_name);
	if (ret) {
		dev_notice(data->dev, "failed to get chg_name\n");
		data->chg_name = "primary_chg";
	}

	/*
	 * mediatek bc12_sel
	 * 0 means bc12 owner is THIS_MODULE,
	 * if it is not 0, always ignore
	 */
	bc12_np = of_parse_phandle(np, "bc12_sel", 0);
	if (!bc12_np) {
		dev_err(data->dev, "failed to get bc12_sel phandle\n");
		return -ENODEV;
	}
	if (of_property_read_u32(bc12_np, "bc12_sel", &tmp) < 0) {
		dev_err(data->dev, "property bc12_sel not found\n");
		return -EINVAL;
	}
	if (tmp != 0)
		data->attach_trig = ATTACH_TRIG_IGNORE;
	else if (IS_ENABLED(CONFIG_TCPC_CLASS))
		data->attach_trig = ATTACH_TRIG_TYPEC;
	else
		data->attach_trig = ATTACH_TRIG_PWR_RDY;

	ret = regmap_field_write(data->rm_field[F_WATCHDOG], 0);
	if (ret)
		return ret;

	ret = regmap_field_write(data->rm_field[F_AUTO_AICR], 0);
	if (ret)
		return ret;

	/* mediatek boot mode */
	boot_np = of_parse_phandle(np, "boot_mode", 0);
	if (!boot_np) {
		dev_err(data->dev, "failed to get bootmode phandle\n");
		return -ENODEV;
	}
	tag = of_get_property(boot_np, "atag,boot", NULL);
	if (!tag) {
		dev_err(data->dev, "failed to get atag,boot\n");
		return -EINVAL;
	}
	dev_info(data->dev, "sz:0x%x tag:0x%x mode:0x%x type:0x%x\n",
		 tag->size, tag->tag, tag->boot_mode, tag->boot_type);
	data->boot_mode = tag->boot_mode;
	data->boot_type = tag->boot_type;
	/* set aicr = 200mA in 1:META_BOOT 5:ADVMETA_BOOT */
	if (data->boot_mode == 1 || data->boot_mode == 5)
		val.intval = 200000;
	else
	/* 500mA is the minimum input current to fit all types of charger */
		val.intval = 500000;
	ret = rt9490_charger_set_aicr(data, &val);
	if (ret)
		return ret;

	/* IC default 3000mA, per 10mA need 16uS, need 4000uS for rampping*/
	usleep_range(RT9490_AICR_WAITUS, RT9490_AICR_WAITUS + 1000);

	/* After AICR internal rampping time to disable ILIM */
	ret = regmap_field_write(data->rm_field[F_EN_ILIM], 0);
	if (ret)
		return ret;

	ret = regmap_field_write(data->rm_field[F_VACOVP], 0);
	if (ret)
		return ret;

	ret = regmap_field_read(data->rm_field[F_VAC1_PG], &vbus_ready);
	if (ret)
		return ret;

	ret = regmap_field_write(data->rm_field[F_EN_BC12], 0);
	if (ret)
		return ret;

	/* F_TD_EOC = 0: for E2 sample */
	ret = regmap_field_write(data->rm_field[F_TD_EOC], 0);
	if (ret)
		return ret;

	if (data->vbus_ready == vbus_ready)
		return 0;

	return ret;
}

static int rt9490_get_all_adcs(struct rt9490_chg_data *data)
{
	const char *chan_names[RT9490_ADC_MAX] = {
			"TDIE", "TS", "VSYS", "VBAT", "VBUS", "IBAT",
			"IBUS", "VAC1", "VAC2", "DM", "DP" };
	int i;

	for (i = 0; i < ARRAY_SIZE(chan_names); i++) {
		data->adc_chans[i] =
				 devm_iio_channel_get(data->dev, chan_names[i]);
		if (IS_ERR(data->adc_chans[i])) {
			dev_err(data->dev, "Failed to %s adc\n", chan_names[i]);
			return PTR_ERR(data->adc_chans[i]);
		}
	}

	return 0;
}

static int rt9490_chg_set_usbsw(struct rt9490_chg_data *data,
				enum rt9490_usbsw usbsw)
{
	struct phy *phy;
	int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
					       PHY_MODE_BC11_CLR;

	dev_info(data->dev, "usbsw=%d\n", usbsw);
	phy = phy_get(data->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(data->dev, "Failed to get usb2-phy");
		return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_err(data->dev, "Failed to set phy ext mode\n");
	phy_put(data->dev, phy);
	return ret;
}

static bool is_usb_rdy(struct device *dev)
{
	bool rdy = true;
	struct device_node *node;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		rdy = !of_property_read_bool(node, "cdp-block");
		dev_info(dev, "usb ready = %d\n", rdy);
	} else
		dev_warn(dev, "usb node missing or invalid\n");
	return rdy;
}

static int rt9490_chg_enable_bc12(struct rt9490_chg_data *data, bool en)
{
	int i, ret, attach;
	static const int max_wait_cnt = 250;

	dev_info(data->dev, "%s en=%d\n", __func__, en);
	if (en) {
		/* CDP port specific process */
		dev_info(data->dev, "check CDP block\n");
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy(data->dev))
				break;
			attach = atomic_read(&data->attach);
			if (attach == ATTACH_TYPE_TYPEC)
				msleep(100);
			else {
				dev_notice(data->dev, "Change attach:%d, disable bc12\n",
					   attach);
				en = false;
				break;
			}
		}
		if (i == max_wait_cnt)
			dev_notice(data->dev, "CDP timeout\n");
		else
			dev_info(data->dev, "CDP free\n");
	}
	ret = rt9490_chg_set_usbsw(data, en ? USBSW_CHG : USBSW_USB);
	if (ret)
		return ret;
	return regmap_field_write(data->rm_field[F_EN_BC12], en);
}

static void rt9490_chg_bc12_work_func(struct work_struct *work)
{
	int ret, attach;
	bool bc12_ctrl = true, bc12_en = false, rpt_psy = true;
	struct rt9490_chg_data *data = container_of(work,
						    struct rt9490_chg_data,
						    bc12_work);
	u32 result = 0;

	mutex_lock(&data->lock);
	attach = atomic_read(&data->attach);
	if (attach > ATTACH_TYPE_NONE && data->boot_mode == 5) {
		/* skip bc12 to speed up ADVMETA_BOOT */
		dev_notice(data->dev, "Force SDP in meta mode\n");
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB;
		data->usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		data->bc12_dn = false;
		goto out;
	}

	switch (attach) {
	case ATTACH_TYPE_NONE:
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB;
		data->usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	case ATTACH_TYPE_TYPEC:
		dev_info(data->dev, "data_bc12_dn=%d\n", data->bc12_dn);
		if (!data->bc12_dn) {
			bc12_en = true;
			rpt_psy = false;
			goto out;
		}
		ret = regmap_field_read(data->rm_field[F_VBUS_STAT], &result);
		if (ret) {
			dev_err(data->dev, "Failed to get vbus stat\n");
			rpt_psy = false;
			goto out;
		}
		break;
	case ATTACH_TYPE_PD_SDP:
		result = RT9490_CABLE_USB_SDP;
		break;
	case ATTACH_TYPE_PD_DCP:
		/* not to enable bc12 */
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		data->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		goto out;
	case ATTACH_TYPE_PD_NONSTD:
		result = RT9490_CABLE_NSTD;
		break;
	default:
		dev_info(data->dev, "Using traditional bc12 flow!\n");
		break;
	}

	switch (result) {
	case RT9490_CABLE_USB_SDP:
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB;
		data->usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case RT9490_CABLE_NSTD:
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB;
		data->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case RT9490_CABLE_USB_CDP:
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		data->usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case RT9490_CABLE_USB_DCP:
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		data->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		bc12_en = true;
		break;
	case RT9490_CABLE_UNKNOWN_DCP:
		/* HVDCP case, please implement it */
		data->chg_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		data->usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		bc12_en = true;
		break;
	case RT9490_CABLE_APPLE_TA:
		data->chg_desc.type = POWER_SUPPLY_TYPE_APPLE_BRICK_ID;
		data->usb_type = POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID;
		break;
	default:
		data->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		data->usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		bc12_ctrl = false;
		rpt_psy = false;
		dev_info(data->dev, "Unknown port stat %d\n", result);
		goto out;
	}
	dev_info(data->dev, "port stat = %s\n", rt9490_port_stat_names[result]);
out:
	mutex_unlock(&data->lock);
	if (bc12_ctrl && (rt9490_chg_enable_bc12(data, bc12_en) < 0))
		dev_err(data->dev, "Failed to set bc12 = %d\n", bc12_en);
	if (rpt_psy)
		power_supply_changed(data->psy);
}

static irqreturn_t rt9490_vbus_ready_handler(int irqno, void *priv)
{
	struct rt9490_chg_data *data = priv;
	int ret;

	dev_info(data->dev, "%s ++\n", __func__);
	mutex_lock(&data->lock);

	ret = regmap_field_read(data->rm_field[F_VAC1_PG],
				&data->vbus_ready);
	if (ret) {
		mutex_unlock(&data->lock);
		return IRQ_NONE;
	}

	rt9490_chg_attach_pre_process(data, ATTACH_TRIG_PWR_RDY,
				      data->vbus_ready ?
				      ATTACH_TYPE_PWR_RDY : ATTACH_TYPE_NONE);
	mutex_unlock(&data->lock);

	return IRQ_HANDLED;
}

static irqreturn_t rt9490_bc12_done_handler(int irqno, void *priv)
{
	struct rt9490_chg_data *data = priv;
	int attach;

	dev_info(data->dev, "%s ++\n", __func__);
	mutex_lock(&data->lock);
	data->bc12_dn = true;
	attach = atomic_read(&data->attach);
	mutex_unlock(&data->lock);
	if (attach < ATTACH_TYPE_PD && !queue_work(data->wq, &data->bc12_work))
		dev_notice(data->dev, "%s bc12 work already queued\n",
			   __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9490_wdt_handler(int irqno, void *priv)
{
	struct rt9490_chg_data *data = priv;
	int ret;

	dev_info(data->dev, "%s ++\n", __func__);
	ret = regmap_field_write(data->rm_field[F_WDRST], 1);
	if (ret)
		dev_err(data->dev, "Failed to do watchdog reset\n");

	return IRQ_HANDLED;
}

static irqreturn_t rt9490_aicc_handler(int irqno, void *priv)
{
	struct rt9490_chg_data *data = priv;

	dev_info(data->dev, "%s ++\n", __func__);
	complete(&data->aicc_done);
	return IRQ_HANDLED;
}

static const struct {
	const char *irq_name;
	irq_handler_t handler;
} rt9490_irqs[] = {
	{ "vbus-rdy",	rt9490_vbus_ready_handler },
	{ "wdt",	rt9490_wdt_handler },
	{ "aicc",	rt9490_aicc_handler },
	{ "bc12-done",	rt9490_bc12_done_handler },
};

static int rt9490_set_shipping_mode(struct rt9490_chg_data *data)
{
	int ret;

	ret = regmap_field_write(data->rm_field[F_SDRV_DLY], 1);
	if (ret) {
		dev_err(data->dev, "failed to disable ship mode delay\n");
		return ret;
	}

	return regmap_field_write(data->rm_field[F_SDRV_CTRL], 2);
}

static ssize_t shipping_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct rt9490_chg_data *data = dev_get_drvdata(dev);
	int32_t tmp = 0;
	int ret = 0;

	if (kstrtoint(buf, 10, &tmp) < 0) {
		dev_err(dev, "parsing number fail\n");
		return -EINVAL;
	}
	if (tmp != 5526789)
		return -EINVAL;
	ret = rt9490_set_shipping_mode(data);
	if (ret)
		return ret;

	return count;
}

static const DEVICE_ATTR_WO(shipping_mode);

static int rt9490_charger_probe(struct platform_device *pdev)
{
	struct rt9490_chg_data *data;
	struct power_supply_desc *desc;
	struct power_supply_config cfg = {};
	struct regulator_config reg_cfg = {};
	struct regulator_dev *rdev;
	int i, irqno, ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &pdev->dev;
	mutex_init(&data->lock);
	mutex_init(&data->pe_lock);
	atomic_set(&data->aicc_once, 0);
	atomic_set(&data->attach, 0);
	init_completion(&data->aicc_done);
	data->wq = create_singlethread_workqueue(dev_name(data->dev));
	if (!data->wq) {
		dev_err(data->dev, "failed to create workqueue\n");
		return -ENOMEM;
	}
	INIT_WORK(&data->bc12_work, rt9490_chg_bc12_work_func);
	platform_set_drvdata(pdev, data);

	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap) {
		dev_err(&pdev->dev, "Failed to get parent regmap\n");
		ret = -ENODEV;
		goto out_wq;
	}

	ret = devm_regmap_field_bulk_alloc(&pdev->dev, data->regmap,
					   data->rm_field, rt9490_reg_fields,
					   ARRAY_SIZE(rt9490_reg_fields));
	if (ret) {
		dev_err(&pdev->dev, "Failed to alloc regmap fields\n");
		goto out_wq;
	}
	ret = rt9490_get_all_adcs(data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get all adcs\n");
		goto out_wq;
	}

	ret = rt9490_do_charger_init(data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to do charger init\n");
		goto out_wq;
	}

	data->ceb_gpio = devm_gpiod_get_optional(&pdev->dev, "ceb",
						 GPIOD_OUT_LOW);
	if (IS_ERR(data->ceb_gpio)) {
		dev_info(&pdev->dev, "config ceb-gpio faiil\n");
		ret = PTR_ERR(data->ceb_gpio);
		goto out_wq;
	}

	reg_cfg.dev = &pdev->dev;
	reg_cfg.driver_data = data;
	reg_cfg.regmap = data->regmap;
	rdev = devm_regulator_register(&pdev->dev, &rt9490_otg_desc, &reg_cfg);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "Failed to register otg vbus regulator\n");
		ret = PTR_ERR(rdev);
		goto out_wq;
	}

	desc = &data->chg_desc;
	desc->name = dev_name(&pdev->dev);
	desc->type = POWER_SUPPLY_TYPE_USB;
	desc->properties = rt9490_charger_properties;
	desc->num_properties = ARRAY_SIZE(rt9490_charger_properties);
	desc->usb_types = rt9490_charger_usb_types;
	desc->num_usb_types = ARRAY_SIZE(rt9490_charger_usb_types);
	desc->get_property = rt9490_charger_get_property;
	desc->set_property = rt9490_charger_set_property;

	cfg.of_node = pdev->dev.of_node;
	cfg.drv_data = data;
	cfg.supplied_to = rt9490_charger_supplied_to;
	cfg.num_supplicants = ARRAY_SIZE(rt9490_charger_supplied_to);


	data->psy = devm_power_supply_register(&pdev->dev, desc, &cfg);
	if (IS_ERR(data->psy)) {
		dev_err(&pdev->dev, "Failed to register psy\n");
		ret = PTR_ERR(data->psy);
		goto out_wq;
	}

	data->chgdev = charger_device_register(data->chg_name, data->dev,
					       data, &rt9490_chg_ops,
					       &rt9490_chg_props);
	if (IS_ERR(data->chgdev)) {
		dev_err(&pdev->dev, "Failed to init chgdev\n");
		ret = PTR_ERR(data->chgdev);
		goto out_wq;
	}

	for (i = 0; i < ARRAY_SIZE(rt9490_irqs); i++) {
		irqno = platform_get_irq_byname(pdev, rt9490_irqs[i].irq_name);
		if (irqno < 0) {
			ret = irqno;
			goto out_get_irq;
		}

		ret = devm_request_threaded_irq(&pdev->dev, irqno, NULL,
						rt9490_irqs[i].handler, 0,
						dev_name(&pdev->dev), data);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request irq [%d]",
				irqno);
			goto out_get_irq;
		}
	}

	ret = device_create_file(data->dev, &dev_attr_shipping_mode);
	if (ret < 0) {
		dev_err(data->dev,
			"Failed to create shipping mode attribute\n");
		goto out_get_irq;
	}

	dev_info(data->dev, "%s ok\n", __func__);
	return 0;
out_get_irq:
	charger_device_unregister(data->chgdev);
out_wq:
	destroy_workqueue(data->wq);
	mutex_destroy(&data->lock);
	mutex_destroy(&data->pe_lock);
	return ret;
}

static int rt9490_charger_remove(struct platform_device *pdev)
{
	struct rt9490_chg_data *data = platform_get_drvdata(pdev);

	charger_device_unregister(data->chgdev);
	device_remove_file(data->dev, &dev_attr_shipping_mode);
	destroy_workqueue(data->wq);
	mutex_destroy(&data->lock);
	mutex_destroy(&data->pe_lock);
	return 0;
}
static void rt9490_charger_shutdown(struct platform_device *pdev)
{
	struct rt9490_chg_data *data = platform_get_drvdata(pdev);

	int ret;

	if (data->ceb_gpio)
		gpiod_set_value(data->ceb_gpio, true);
	/* Trigger the whole chip register reset */
	ret = regmap_field_write(data->rm_field[F_RSTRG], 1);
	if (ret)
		dev_info(data->dev, "Failed to reset registers\n");
}
static const struct of_device_id rt9490_charger_of_match_table[] = {
	{ .compatible = "richtek,rt9490-chg", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt9490_charger_of_match_table);

static struct platform_driver rt9490_charger_driver = {
	.driver = {
		.name = "rt9490-charger",
		.of_match_table = rt9490_charger_of_match_table,
	},
	.probe = rt9490_charger_probe,
	.remove = rt9490_charger_remove,
	.shutdown = rt9490_charger_shutdown,
};
module_platform_driver(rt9490_charger_driver);

MODULE_DESCRIPTION("Richtek RT9490 charger driver");
MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_LICENSE("GPL");
