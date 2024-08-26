// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
*/

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/regmap.h>

#include "../charger_class/hq_cp_class.h"

#define SC8541_DRV_VERSION              "1.0.1_G"

enum {
	SC8541_STANDALONG = 0,
	SC8541_MASTER,
	SC8541_SLAVE,
};

static const char* sc8541_psy_name[] = {
	[SC8541_STANDALONG] = "sc-cp-standalone",
	[SC8541_MASTER] = "sc-cp-master",
	[SC8541_SLAVE] = "sc-cp-slave",
};

static const char* sc8541_irq_name[] = {
	[SC8541_STANDALONG] = "sc8541-standalone-irq",
	[SC8541_MASTER] = "sc8541-master-irq",
	[SC8541_SLAVE] = "sc8541-slave-irq",
};

static int sc8541_mode_data[] = {
	[SC8541_STANDALONG] = SC8541_STANDALONG,
	[SC8541_MASTER] = SC8541_MASTER,
	[SC8541_SLAVE] = SC8541_SLAVE,
};

enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC1,
	ADC_VAC2,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TSBUS,
	ADC_TSBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
}SC_8541_ADC_CH;

static const u32 sc8541_adc_accuracy_tbl[ADC_MAX_NUM] = {
	150000,	/* IBUS */
	35000,	/* VBUS */
	35000,	/* VAC1 */
	35000,	/* VAC2 */
	20000,	/* VOUT */
	20000,	/* VBAT */
	200000,	/* IBAT */
	0,	/* TSBUS */
	0,	/* TSBAT */
	4,	/* TDIE */
};

static const int sc8541_adc_m[] =
	{25, 375, 5, 5, 125, 125, 3125, 9766, 9766, 5};

static const int sc8541_adc_l[] =
	{10, 100, 1, 1, 100, 100, 1000, 100000, 100000, 10};

enum sc8541_notify {
	SC8541_NOTIFY_OTHER = 0,
	SC8541_NOTIFY_IBUSUCPF,
	SC8541_NOTIFY_VBUSOVPALM,
	SC8541_NOTIFY_VBATOVPALM,
	SC8541_NOTIFY_IBUSOCP,
	SC8541_NOTIFY_VBUSOVP,
	SC8541_NOTIFY_IBATOCP,
	SC8541_NOTIFY_VBATOVP,
	SC8541_NOTIFY_VOUTOVP,
	SC8541_NOTIFY_VDROVP,
};

enum sc8541_error_stata {
	ERROR_VBUS_HIGH = 0,
	ERROR_VBUS_LOW,
	ERROR_VBUS_OVP,
	ERROR_IBUS_OCP,
	ERROR_VBAT_OVP,
	ERROR_IBAT_OCP,
};

struct flag_bit {
	int notify;
	int mask;
	char *name;
};

struct intr_flag {
	int reg;
	int len;
	struct flag_bit bit[8];
};

static struct intr_flag cp_intr_flag[] = {
	{ .reg = 0x18, .len = 7, .bit = {
				{.mask = BIT(0), .name = "vbus ovp alm flag", .notify = SC8541_NOTIFY_VBUSOVPALM},
				{.mask = BIT(1), .name = "vbus ovp flag", .notify = SC8541_NOTIFY_VBUSOVP},
				{.mask = BIT(3), .name = "ibat ocp alm flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(4), .name = "ibat ocp flag", .notify = SC8541_NOTIFY_IBATOCP},
				{.mask = BIT(5), .name = "vout ovp flag", .notify = SC8541_NOTIFY_VOUTOVP},
				{.mask = BIT(6), .name = "vbat ovp alm flag", .notify = SC8541_NOTIFY_VBATOVPALM},
				{.mask = BIT(7), .name = "vbat ovp flag", .notify = SC8541_NOTIFY_VBATOVP},
				},
	},
	{ .reg = 0x19, .len = 3, .bit = {
				{.mask = BIT(2), .name = "pin diag fall flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(5), .name = "ibus ucp fall flag", .notify = SC8541_NOTIFY_IBUSUCPF},
				{.mask = BIT(7), .name = "ibus ocp flag", .notify = SC8541_NOTIFY_IBUSOCP},
				},
	},
	{ .reg = 0x1a, .len = 8, .bit = {
				{.mask = BIT(0), .name = "acrb2 config flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(1), .name = "acrb1 config flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(2), .name = "vbus present flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(3), .name = "vac2 insert flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(4), .name = "vac1 insert flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(5), .name = "vat insert flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(6), .name = "vac2 ovp flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(7), .name = "vac1 ovp flag", .notify = SC8541_NOTIFY_OTHER},
				},
	},
	{ .reg = 0x1b, .len = 8, .bit = {
				{.mask = BIT(0), .name = "wd timeout flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(1), .name = "tdie alm flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(2), .name = "tshut flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(3), .name = "tsbat flt flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(4), .name = "tsbus flt flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(5), .name = "tsbus tsbat alm flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(6), .name = "ss timeout flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(7), .name = "adc done flag", .notify = SC8541_NOTIFY_OTHER},
				},
	},
	{ .reg = 0x1c, .len = 3, .bit = {
				{.mask = BIT(3), .name = "vbus errorlo flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(4), .name = "vbus errorhi flag", .notify = SC8541_NOTIFY_OTHER},
				{.mask = BIT(6), .name = "cp switching flag", .notify = SC8541_NOTIFY_OTHER},
				},
	},
};

/************************************************************************/
#define SC8541_DEVICE_ID                0x41

#define SC8541_VBAT_OVP_MIN             3840
#define SC8541_VBAT_OVP_MAX             5110
#define SC8541_VBAT_OVP_STEP            10

#define SC8541_IBAT_OCP_MIN             0
#define SC8541_IBAT_OCP_MAX             12700
#define SC8541_IBAT_OCP_STEP            10

#define SC8541_VBUS_OVP_MIN             7000
#define SC8541_VBUS_OVP_MAX             13350
#define SC8541_VBUS_OVP_STEP            50

#define SC8541_IBUS_OCP_MIN             1000
#define SC8541_IBUS_OCP_MAX             8000
#define SC8541_IBUS_OCP_STEP            250

#define SC8541_REG18                    0x18
#define SC8541_REG25                    0x25
#define SC8541_REGMAX                   0x42

enum sc8541_reg_range {
	SC8541_VBAT_OVP,
	SC8541_IBAT_OCP,
	SC8541_VBUS_OVP,
	SC8541_IBUS_OCP,
	SC8541_VBUS_OVP_ALM,
	SC8541_VBAT_OVP_ALM,
};

struct reg_range {
	u32 min;
	u32 max;
	u32 step;
	u32 offset;
	const u32 *table;
	u16 num_table;
	bool round_up;
};

#define SC8541_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
	.min = _min, \
	.max = _max, \
	.step = _step, \
	.offset = _offset, \
	.round_up = _ru, \
}

#define SC8541_CHG_RANGE_T(_table, _ru) \
	{ .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }


static const struct reg_range sc8541_reg_range[] = {
	[SC8541_VBAT_OVP]      = SC8541_CHG_RANGE(3840, 5110, 10, 3840, false),
	[SC8541_IBAT_OCP]      = SC8541_CHG_RANGE(0, 12700, 100, 0, false),
	[SC8541_VBUS_OVP]      = SC8541_CHG_RANGE(7000, 13350, 50, 7000, false),
	[SC8541_IBUS_OCP]      = SC8541_CHG_RANGE(1000, 8000, 250, 1000, false),
	[SC8541_VBUS_OVP_ALM]  = SC8541_CHG_RANGE(7000, 13350, 50, 7000, false),
	[SC8541_VBAT_OVP_ALM]  = SC8541_CHG_RANGE(60, 960, 60, 60, false),
};


enum sc8541_fields {
	VBAT_OVP_DIS, VBAT_OVP,
	VBAT_OVP_ALM_DIS, VBAT_OVP_ALM,
	IBAT_OCP_DIS, IBAT_OCP,
	IBAT_OCP_ALM_DIS, IBAT_OCP_ALM,
	IBUS_UCP_DIS, VBUS_IN_RANGE_DIS,
	VBUS_PD_EN, VBUS_OVP,
	VBUS_OVP_ALM_DIS, VBUS_OVP_ALM,
	IBUS_OCP_DIS, IBUS_OCP,
	TSHUT_DIS, TSBUS_FLT_DIS, TSBAT_FLT_DIS,
	TDIE_ALM,
	TSBUS_FLT,
	TSBAT_FLT,
	VAC1_OVP, VAC2_OVP, VAC1_PD_EN, VAC2_PD_EN,
	REG_RST, OTG_EN, CHG_EN, MODE, ACDRV1_STAT, ACDRV2_STAT,
	FSW_SET, WD_TIMEOUT, WD_TIMEOUT_DIS,
	SET_IBAT_SNS_RES, SS_TIMEOUT, IBUS_UCP_FALL_DG,
	VOUT_OVP_DIS, VOUT_OVP, MS,
	CP_SWITCHING_STAT,VBUS_ERRORHI_STAT,VBUS_ERRORLO_STAT,
	DEVICE_ID,
	ADC_EN, ADC_RATE,
	ACDRV_MANUAL_EN, ACDRV1_EN, ACDRV2_EN,
	PMID2OUT_UVP, PMID2OUT_OVP, PMID2OUT_UVP_FLAG, PMID2OUT_OVP_FLAG,
	F_MAX_FIELDS,
};

//REGISTER
static const struct reg_field sc8541_reg_fields[] = {
	/*reg00*/
	[VBAT_OVP_DIS] = REG_FIELD(0x00, 7, 7),
	[VBAT_OVP] = REG_FIELD(0x00, 0, 6),
	/*reg01*/
	[VBAT_OVP_ALM_DIS] = REG_FIELD(0x01, 7, 7),
	[VBAT_OVP_ALM] = REG_FIELD(0x01, 0, 6),
	/*reg02*/
	[IBAT_OCP_DIS] = REG_FIELD(0x02, 7, 7),
	[IBAT_OCP] = REG_FIELD(0x02, 0, 6),
	/*reg03*/
	[IBAT_OCP_ALM_DIS] = REG_FIELD(0x03, 7, 7),
	[IBAT_OCP_ALM] = REG_FIELD(0x03, 0, 6),
	/*reg05*/
	[IBUS_UCP_DIS] = REG_FIELD(0x05, 7, 7),
	[VBUS_IN_RANGE_DIS] = REG_FIELD(0x05, 2, 2),
	/*reg06*/
	[VBUS_PD_EN] = REG_FIELD(0x06, 7, 7),
	[VBUS_OVP] = REG_FIELD(0x06, 0, 6),
	/*reg07*/
	[VBUS_OVP_ALM_DIS] = REG_FIELD(0x07, 7, 7),
	[VBUS_OVP_ALM] = REG_FIELD(0x07, 0, 6),
	/*reg08*/
	[IBUS_OCP_DIS] = REG_FIELD(0x08, 7, 7),
	[IBUS_OCP] = REG_FIELD(0x08, 0, 4),
	/*reg0A*/
	[TSHUT_DIS] = REG_FIELD(0x0A, 7, 7),
	[TSBUS_FLT_DIS] = REG_FIELD(0x0A, 3, 3),
	[TSBAT_FLT_DIS] = REG_FIELD(0x0A, 2, 2),
	/*reg0B*/
	[TDIE_ALM] = REG_FIELD(0x0B, 0, 7),
	/*reg0C*/
	[TSBUS_FLT] = REG_FIELD(0x0C, 0, 7),
	/*reg0D*/
	[TSBAT_FLT] = REG_FIELD(0x0D, 0, 7),
	/*reg0E*/
	[VAC1_OVP] = REG_FIELD(0x0E, 5, 7),
	[VAC2_OVP] = REG_FIELD(0x0E, 2, 4),
	[VAC1_PD_EN] = REG_FIELD(0x0E, 1, 1),
	[VAC2_PD_EN] = REG_FIELD(0x0E, 0, 0),
	/*reg0F*/
	[REG_RST] = REG_FIELD(0x0F, 7, 7),
	[OTG_EN] = REG_FIELD(0x0F, 5, 5),
	[CHG_EN] = REG_FIELD(0x0F, 4, 4),
	[MODE] = REG_FIELD(0x0F, 3, 3),
	[ACDRV1_STAT] = REG_FIELD(0x0F, 1, 1),
	[ACDRV2_STAT] = REG_FIELD(0x0F, 0, 0),
	/*reg10*/
	[FSW_SET] = REG_FIELD(0x10, 5, 7),
	[WD_TIMEOUT] = REG_FIELD(0x10, 3, 4),
	[WD_TIMEOUT_DIS] = REG_FIELD(0x10, 2, 2),
	/*reg11*/
	[SET_IBAT_SNS_RES] = REG_FIELD(0x11, 7, 7),
	[SS_TIMEOUT] = REG_FIELD(0x11, 4, 6),
	[IBUS_UCP_FALL_DG] = REG_FIELD(0x11, 2, 3),
	/*reg12*/
	[VOUT_OVP_DIS] = REG_FIELD(0x12, 7, 7),
	[VOUT_OVP] = REG_FIELD(0x12, 5, 6),
	[MS] = REG_FIELD(0x12, 0, 1),
	/*reg17*/
	[CP_SWITCHING_STAT] = REG_FIELD(0x17, 6, 6),
	[VBUS_ERRORHI_STAT] = REG_FIELD(0x17, 4, 4),
	[VBUS_ERRORLO_STAT] = REG_FIELD(0x17, 3, 3),
	/*reg22*/
	[DEVICE_ID] = REG_FIELD(0x22, 0, 7),
	/*reg23*/
	[ADC_EN] = REG_FIELD(0x23, 7, 7),
	[ADC_RATE] = REG_FIELD(0x23, 6, 6),
	/*reg40*/
	[ACDRV_MANUAL_EN] = REG_FIELD(0x40, 6, 6),
	[ACDRV1_EN] = REG_FIELD(0x40, 5, 5),
	[ACDRV2_EN] = REG_FIELD(0x40, 4, 4),
	/*reg42*/
	[PMID2OUT_UVP] = REG_FIELD(0x42, 5, 7),
	[PMID2OUT_OVP] = REG_FIELD(0x42, 2, 4),
	[PMID2OUT_UVP_FLAG] = REG_FIELD(0x42, 1, 1),
	[PMID2OUT_OVP_FLAG] = REG_FIELD(0x42, 0, 0),
};

static const struct regmap_config sc8541_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SC8541_REGMAX,
};

/************************************************************************/

struct sc8541_cfg_e {
	int vbat_ovp_dis;
	int vbat_ovp;
	int vbat_ovp_alm_dis;
	int vbat_ovp_alm;
	int ibat_ocp_dis;
	int ibat_ocp;
	int ibat_ocp_alm_dis;
	int ibat_ocp_alm;
	int ibus_ucp_dis;
	int vbus_in_range_dis;
	int vbus_pd_en;
	int vbus_ovp;
	int vbus_ovp_alm_dis;
	int vbus_ovp_alm;
	int ibus_ocp_dis;
	int ibus_ocp;
	int tshut_dis;
	int tsbus_flt_dis;
	int tsbat_flt_dis;
	int tdie_alm;
	int tsbus_flt;
	int tsbat_flt;
	int vac1_ovp;
	int vac2_ovp;
	int vac1_pd_en;
	int vac2_pd_en;
	int fsw_set;
	int wd_timeout;
	int wd_timeout_dis;
	int ibat_sns_r;
	int ss_timeout;
	int ibus_ucp_fall_dg;
	int vout_ovp_dis;
	int vout_ovp;
	int pmid2out_uvp;
	int pmid2out_ovp;
};

struct sc8541_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];

	struct sc8541_cfg_e cfg;
	int irq_gpio;
	int irq;

	int mode;

	bool charge_enabled;
	int usb_present;
	int vbus_volt;
	int ibus_curr;
	int vbat_volt;
	int ibat_curr;
	int die_temp;

	#ifdef CONFIG_MTK_CLASS
	struct charger_device *chg_dev;
	#endif /*CONFIG_MTK_CLASS*/

	#ifdef CONFIG_HUAQIN_CP_POLICY_MODULE
	struct chargerpump_dev *master_cp_chg;
	struct chargerpump_dev *slave_cp_chg;
	#endif

	const char *chg_dev_name;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
};

/********************COMMON API***********************/
__maybe_unused static u8 val2reg(enum sc8541_reg_range id, u32 val)
{
	int i;
	u8 reg;
	const struct reg_range *range= &sc8541_reg_range[id];

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
		reg = (range->max - range->offset) / range->step;
	else if (range->round_up)
		reg = (val - range->offset) / range->step + 1;
	else
		reg = (val - range->offset) / range->step;
	return reg;
}

__maybe_unused static u32 reg2val(enum sc8541_reg_range id, u8 reg)
{
	const struct reg_range *range= &sc8541_reg_range[id];
	if (!range)
		return reg;
	return range->table ? range->table[reg] :
					range->offset + range->step * reg;
}
/*********************************************************/
static int sc8541_field_read(struct sc8541_chip *sc,
				enum sc8541_fields field_id, int *val)
{
	int ret;

	ret = regmap_field_read(sc->rmap_fields[field_id], val);
	if (ret < 0) {
		dev_err(sc->dev, "sc8541 read field %d fail: %d\n", field_id, ret);
	}

	return ret;
}

static int sc8541_field_write(struct sc8541_chip *sc,
				enum sc8541_fields field_id, int val)
{
	int ret;

	ret = regmap_field_write(sc->rmap_fields[field_id], val);
	if (ret < 0) {
		dev_err(sc->dev, "sc8541 read field %d fail: %d\n", field_id, ret);
	}

	return ret;
}

static int sc8541_read_block(struct sc8541_chip *sc,
				int reg, uint8_t *val, int len)
{
	int ret;

	ret = regmap_bulk_read(sc->regmap, reg, val, len);
	if (ret < 0) {
		dev_err(sc->dev, "sc8541 read %02x block failed %d\n", reg, ret);
	}

	return ret;
}

/*******************************************************/
__maybe_unused static int sc8541_detect_device(struct sc8541_chip *sc)
{
	int ret;
	int val;

	ret = sc8541_field_read(sc, DEVICE_ID, &val);
	if (ret < 0) {
		dev_err(sc->dev, "%s fail(%d)\n", __func__, ret);
		return ret;
	}

	if (val != SC8541_DEVICE_ID) {
		dev_err(sc->dev, "%s not find SC8541, ID = 0x%02x\n", __func__, ret);
		return -EINVAL;
	}

	return ret;
}

__maybe_unused static int sc8541_reg_reset(struct sc8541_chip *sc)
{
	return sc8541_field_write(sc, REG_RST, 1);
}

__maybe_unused static int sc8541_dump_reg(struct sc8541_chip *sc)
{
	int ret;
	int i;
	int val;

	for (i = 0; i <= SC8541_REGMAX; i++) {
		ret = regmap_read(sc->regmap, i, &val);
		dev_err(sc->dev, "%s reg[0x%02x] = 0x%02x\n", 
				__func__, i, val);
	}

	return ret;
}

__maybe_unused static int sc8541_enable_charge(struct sc8541_chip *sc, bool en)
{
	int ret;
	dev_info(sc->dev,"%s:%d",__func__,en);

	ret = sc8541_field_write(sc, CHG_EN, !!en);

	return ret;
}


__maybe_unused static int sc8541_check_charge_enabled(struct sc8541_chip *sc, bool *enabled)
{
	int ret, val;

	ret = sc8541_field_read(sc, CP_SWITCHING_STAT, &val);

	*enabled = (bool)val;

	dev_info(sc->dev,"%s:%d", __func__, val);

	return ret;
}

__maybe_unused static int sc8541_get_status(struct sc8541_chip *sc, uint32_t *status)
{
	int ret, val;
	*status = 0;

	ret = sc8541_field_read(sc, VBUS_ERRORHI_STAT, &val);
	if (ret < 0) {
		dev_err(sc->dev, "%s fail to read VBUS_ERRORHI_STAT(%d)\n", __func__, ret);
		return ret;
	}
	if (val != 0)
		*status |= BIT(ERROR_VBUS_HIGH);

	ret = sc8541_field_read(sc, VBUS_ERRORLO_STAT, &val);
	if (ret < 0) {
		dev_err(sc->dev, "%s fail to read VBUS_ERRORLO_STAT(%d)\n", __func__, ret);
		return ret;
	}
	if (val != 0)
		*status |= BIT(ERROR_VBUS_LOW);

	return ret;
}

__maybe_unused static int sc8541_enable_adc(struct sc8541_chip *sc, bool en)
{
	dev_info(sc->dev,"%s:%d", __func__, en);
	return sc8541_field_write(sc, ADC_EN, !!en);
}

__maybe_unused static int sc8541_set_adc_scanrate(struct sc8541_chip *sc, bool oneshot)
{
	dev_info(sc->dev,"%s:%d",__func__,oneshot);
	return sc8541_field_write(sc, ADC_RATE, !!oneshot);
}

static int sc8541_get_adc_data(struct sc8541_chip *sc,
			int channel, int *result)
{
	uint8_t val[2] = {0};
	int ret;

	if(channel >= ADC_MAX_NUM)
		return -EINVAL;

	msleep(50);

	ret = sc8541_read_block(sc, SC8541_REG25 + (channel << 1), val, 2);
	if (ret < 0) {
		return ret;
	}

	*result = (val[1] | (val[0] << 8)) *
				sc8541_adc_m[channel] / sc8541_adc_l[channel];

	dev_info(sc->dev,"%s %d %d", __func__, channel, *result);

	return ret;
}

__maybe_unused static int sc8541_set_busovp_th(struct sc8541_chip *sc, int threshold)
{
	int reg_val = val2reg(SC8541_VBUS_OVP, threshold);

	dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

	return sc8541_field_write(sc, VBUS_OVP, reg_val);
}

__maybe_unused static int sc8541_set_busocp_th(struct sc8541_chip *sc, int threshold)
{
	int reg_val = val2reg(SC8541_IBUS_OCP, threshold);

	dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

	return sc8541_field_write(sc, IBUS_OCP, reg_val);
}

__maybe_unused static int sc8541_set_batovp_th(struct sc8541_chip *sc, int threshold)
{
	int reg_val = val2reg(SC8541_VBAT_OVP, threshold);

	dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

	return sc8541_field_write(sc, VBAT_OVP, reg_val);
}

__maybe_unused static int sc8541_set_batocp_th(struct sc8541_chip *sc, int threshold)
{
	int reg_val = val2reg(SC8541_IBAT_OCP, threshold);

	dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

	return sc8541_field_write(sc, IBAT_OCP, reg_val);
}

__maybe_unused static int sc8541_set_vbusovp_alarm(struct sc8541_chip *sc, int threshold)
{
	int reg_val = val2reg(SC8541_VBUS_OVP_ALM, threshold);

	dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

	return sc8541_field_write(sc, VBUS_OVP_ALM, reg_val);
}

__maybe_unused static int sc8541_set_vbatovp_alarm(struct sc8541_chip *sc, int threshold)
{
	int reg_val = val2reg(SC8541_VBAT_OVP_ALM, threshold);

	dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

	return sc8541_field_write(sc, VBAT_OVP_ALM, reg_val);
}

__maybe_unused static int sc8541_disable_vbatovp_alarm(struct sc8541_chip *sc, bool en)
{
	int ret;
	dev_info(sc->dev,"%s:%d",__func__,en);

	ret = sc8541_field_write(sc, VBAT_OVP_ALM_DIS, !!en);

	return ret;
}

__maybe_unused static int sc8541_disable_vbusovp_alarm(struct sc8541_chip *sc, bool en)
{
	int ret;
	dev_info(sc->dev,"%s:%d",__func__,en);

	ret = sc8541_field_write(sc, VBUS_OVP_ALM_DIS, !!en);

	return ret;
}


__maybe_unused static int sc8541_is_vbuslowerr(struct sc8541_chip *sc, bool *err)
{
	int ret;
	int val;

	ret = sc8541_field_read(sc, VBUS_ERRORLO_STAT, &val);
	if(ret < 0) {
		return ret;
	}

	dev_info(sc->dev,"%s:%d",__func__,val);

	*err = (bool)val;

	return ret;
}

__maybe_unused static int sc8541_init_device(struct sc8541_chip *sc)
{
	int ret = 0;
	int i;
	struct {
		enum sc8541_fields field_id;
		int conv_data;
	} props[] = {
		{VBAT_OVP_DIS, sc->cfg.vbat_ovp_dis},
		{VBAT_OVP, sc->cfg.vbat_ovp},
		{VBAT_OVP_ALM_DIS, sc->cfg.vbat_ovp_alm_dis},
		{VBAT_OVP_ALM, sc->cfg.vbat_ovp_alm},
		{IBAT_OCP_DIS, sc->cfg.ibat_ocp_dis},
		{IBAT_OCP, sc->cfg.ibat_ocp},
		{IBAT_OCP_ALM_DIS, sc->cfg.ibat_ocp_alm_dis},
		{IBAT_OCP_ALM, sc->cfg.ibat_ocp_alm},
		{IBUS_UCP_DIS, sc->cfg.ibus_ucp_dis},
		{VBUS_IN_RANGE_DIS, sc->cfg.vbus_in_range_dis},
		{VBUS_PD_EN, sc->cfg.vbus_pd_en},
		{VBUS_OVP, sc->cfg.vbus_ovp},
		{VBUS_OVP_ALM_DIS, sc->cfg.vbus_ovp_alm_dis},
		{VBUS_OVP_ALM, sc->cfg.vbus_ovp_alm},
		{IBUS_OCP_DIS, sc->cfg.ibus_ocp_dis},
		{IBUS_OCP, sc->cfg.ibus_ocp},
		{TSHUT_DIS, sc->cfg.tshut_dis},
		{TSBUS_FLT_DIS, sc->cfg.tsbus_flt_dis},
		{TSBAT_FLT_DIS, sc->cfg.tsbat_flt_dis},
		{TDIE_ALM, sc->cfg.tdie_alm},
		{TSBUS_FLT, sc->cfg.tsbus_flt},
		{TSBAT_FLT, sc->cfg.tsbat_flt},
		{VAC1_OVP, sc->cfg.vac1_ovp},
		{VAC2_OVP, sc->cfg.vac2_ovp},
		{VAC1_PD_EN, sc->cfg.vac1_pd_en},
		{VAC2_PD_EN, sc->cfg.vac2_pd_en},
		{FSW_SET, sc->cfg.fsw_set},
		{WD_TIMEOUT, sc->cfg.wd_timeout},
		{WD_TIMEOUT_DIS, sc->cfg.wd_timeout_dis},
		{SET_IBAT_SNS_RES, sc->cfg.ibat_sns_r},
		{SS_TIMEOUT, sc->cfg.ss_timeout},
		{IBUS_UCP_FALL_DG, sc->cfg.ibus_ucp_fall_dg},
		{VOUT_OVP_DIS, sc->cfg.vout_ovp_dis},
		{VOUT_OVP, sc->cfg.vout_ovp},
		{PMID2OUT_UVP, sc->cfg.pmid2out_uvp},
		{PMID2OUT_OVP, sc->cfg.pmid2out_ovp},
	};

	ret = sc8541_reg_reset(sc);
	if (ret < 0) {
		dev_err(sc->dev, "%s Failed to reset registers(%d)\n", __func__, ret);
	}
	msleep(10);

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = sc8541_field_write(sc, props[i].field_id, props[i].conv_data);
	}

	if (sc->mode == SC8541_SLAVE) {
		ret = sc8541_field_write(sc, VBUS_IN_RANGE_DIS, 1);
		if (ret < 0) {
			dev_err(sc->dev, "%s Failed to set vbus in range(%d)\n", __func__, ret);
		}
	}

	sc8541_enable_adc(sc, false);
	sc8541_set_busovp_th(sc, 13000);

	sc8541_dump_reg(sc);

	return ret;
}

static inline int sc_to_sc8541_adc(enum sc_adc_channel chan)
{
	switch (chan) {
	case ADC_GET_VBUS:
		return ADC_VBUS;
	case ADC_GET_VBAT:
		return ADC_VBAT;
	case ADC_GET_IBUS:
		return ADC_IBUS;
	case ADC_GET_IBAT:
		return ADC_IBAT;
	case ADC_GET_TDIE:
		return ADC_TDIE;
	default:
		break;
	}
	return ADC_MAX_NUM;
}

static int sc_sc8541_set_enable(struct chargerpump_dev *charger_pump, bool enable)
{
	struct sc8541_chip *sc = chargerpump_get_private(charger_pump);
	int ret;

	ret = sc8541_enable_charge(sc,enable);

	return ret;
}

static int sc_sc8541_get_is_enable(struct chargerpump_dev *charger_pump, bool *enable)
{
	struct sc8541_chip *sc = chargerpump_get_private(charger_pump);
	int ret;

	ret = sc8541_check_charge_enabled(sc, enable);

	return ret;
}

static int sc_sc8541_get_status(struct chargerpump_dev *charger_pump, uint32_t *status)
{
	struct sc8541_chip *sc = chargerpump_get_private(charger_pump);
	int ret = 0;

	ret = sc8541_get_status(sc, status);

	return ret;
}

static int sc_sc8541_get_adc_value(struct chargerpump_dev *charger_pump, enum sc_adc_channel ch, int *value)
{
	struct sc8541_chip *sc = chargerpump_get_private(charger_pump);
	int ret = 0;

	ret = sc8541_get_adc_data(sc, sc_to_sc8541_adc(ch), value);

	return ret;
}

static int sc_sc8541_set_enable_adc(struct chargerpump_dev *charger_pump, bool en)
{
	struct sc8541_chip *sc = chargerpump_get_private(charger_pump);
	int ret = 0;

	ret = sc8541_enable_adc(sc, en);

	return ret;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int sc_sc8541_set_cp_workmode(struct chargerpump_dev *charger_pump, int workmode)
{
	struct sc8541_chip *sc = chargerpump_get_private(charger_pump);
	int ret = 0;
	ret = sc8541_field_write(sc, MODE, workmode);
	dev_info(sc->dev,"%s,set cp workmode = %d\n", __func__, workmode);
	return ret;
}

static int sc_sc8541_get_cp_workmode(struct chargerpump_dev *charger_pump, int *workmode)
{
	int ret, val;
	struct sc8541_chip *sc = chargerpump_get_private(charger_pump);

	ret = sc8541_field_read(sc, MODE, &val);

	*workmode = val;

	dev_info(sc->dev,"%s:get cp workmode = %d", __func__, val);
	return ret;
}
#endif

static struct chargerpump_ops sc_sc8541_chargerpump_ops = {
	.set_enable = sc_sc8541_set_enable,
	.get_status = sc_sc8541_get_status,
	.get_is_enable = sc_sc8541_get_is_enable,
	.get_adc_value = sc_sc8541_get_adc_value,
	.set_enable_adc = sc_sc8541_set_enable_adc,
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	.set_cp_workmode = sc_sc8541_set_cp_workmode,
	.get_cp_workmode = sc_sc8541_get_cp_workmode,
#endif
};

/********************creat devices note start*************************************************/
static ssize_t sc8541_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct sc8541_chip *sc = dev_get_drvdata(dev);
	u8 addr;
	int val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8541");
	for (addr = 0x0; addr <= SC8541_REGMAX; addr++) {
		ret = regmap_read(sc->regmap, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sc8541_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	struct sc8541_chip *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= SC8541_REGMAX)
		regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, 0660, sc8541_show_registers, sc8541_store_register);

static void sc8541_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}
/********************creat devices note end*************************************************/

/*
* interrupt does nothing, just info event chagne, other module could get info
* through power supply interface
*/
static void sc8541_check_fault_status(struct sc8541_chip *sc)
{
	int ret;
	u8 flag = 0;
	int i,j;

	for (i=0;i < ARRAY_SIZE(cp_intr_flag);i++) {
		ret = sc8541_read_block(sc, cp_intr_flag[i].reg, &flag, 1);
		for (j=0; j <  cp_intr_flag[i].len; j++) {
			if (flag & cp_intr_flag[i].bit[j].mask) {
				dev_err(sc->dev,"trigger :%s\n",cp_intr_flag[i].bit[j].name);
			}
		}
	}
}

static irqreturn_t sc8541_irq_handler(int irq, void *data)
{
	struct sc8541_chip *sc = data;

	dev_err(sc->dev,"INT OCCURED\n");

	sc8541_check_fault_status(sc);

	power_supply_changed(sc->psy);

	return IRQ_HANDLED;
}

static int sc8541_register_interrupt(struct sc8541_chip *sc)
{
	int ret;

	if (gpio_is_valid(sc->irq_gpio)) {
		ret = gpio_request_one(sc->irq_gpio, GPIOF_DIR_IN,"sc8541_irq");
		if (ret) {
			dev_err(sc->dev,"failed to request sc8541_irq\n");
			return -EINVAL;
		}
		sc->irq = gpio_to_irq(sc->irq_gpio);
		if (sc->irq < 0) {
			dev_err(sc->dev,"failed to gpio_to_irq\n");
			return -EINVAL;
		}
	} else {
		dev_err(sc->dev,"irq gpio not provided\n");
		return -EINVAL;
	}

	if (sc->irq) {
		ret = devm_request_threaded_irq(&sc->client->dev, sc->irq,
				NULL, sc8541_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				sc8541_irq_name[sc->mode], sc);

		if (ret < 0) {
			dev_err(sc->dev,"request irq for irq=%d failed, ret =%d\n",
							sc->irq, ret);
			return ret;
		}
		enable_irq_wake(sc->irq);
	}

	return ret;
}
/********************interrupte end*************************************************/


/************************psy start**************************************/
static enum power_supply_property sc8541_charger_props[] = {
		POWER_SUPPLY_PROP_ONLINE,
		POWER_SUPPLY_PROP_VOLTAGE_NOW,
		POWER_SUPPLY_PROP_CURRENT_NOW,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
		POWER_SUPPLY_PROP_TEMP,
};

static int sc8541_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sc8541_chip *sc = power_supply_get_drvdata(psy);
	int result;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		sc8541_check_charge_enabled(sc, &sc->charge_enabled);
		val->intval = sc->charge_enabled;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sc8541_get_adc_data(sc, ADC_VBUS, &result);
		if (!ret)
			sc->vbus_volt = result;
		val->intval = sc->vbus_volt;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sc8541_get_adc_data(sc, ADC_IBUS, &result);
		if (!ret)
			sc->ibus_curr = result;
		val->intval = sc->ibus_curr;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8541_get_adc_data(sc, ADC_VBAT, &result);
		if (!ret)
			sc->vbat_volt = result;
		val->intval = sc->vbat_volt;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc8541_get_adc_data(sc, ADC_IBAT, &result);
		if (!ret)
			sc->ibat_curr = result;
		val->intval = sc->ibat_curr;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = sc8541_get_adc_data(sc, ADC_TDIE, &result);
		if (!ret)
			sc->die_temp = result;
		val->intval = sc->die_temp;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sc8541_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
	struct sc8541_chip *sc = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		sc8541_enable_charge(sc, val->intval);
		dev_info(sc->dev, "POWER_SUPPLY_PROP_ONLINE: %s\n",
				val->intval ? "enable" : "disable");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sc8541_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
	return 0;
}

static int sc8541_psy_register(struct sc8541_chip *sc)
{
	sc->psy_cfg.drv_data = sc;
	sc->psy_cfg.of_node = sc->dev->of_node;

	sc->psy_desc.name = sc8541_psy_name[sc->mode];

	sc->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
	sc->psy_desc.properties = sc8541_charger_props;
	sc->psy_desc.num_properties = ARRAY_SIZE(sc8541_charger_props);
	sc->psy_desc.get_property = sc8541_charger_get_property;
	sc->psy_desc.set_property = sc8541_charger_set_property;
	sc->psy_desc.property_is_writeable = sc8541_charger_is_writeable;


	sc->psy = devm_power_supply_register(sc->dev,
			&sc->psy_desc, &sc->psy_cfg);
	if (IS_ERR(sc->psy)) {
		dev_err(sc->dev, "%s failed to register psy\n", __func__);
		return PTR_ERR(sc->psy);
	}

	dev_info(sc->dev, "%s power supply register successfully\n", sc->psy_desc.name);

	return 0;
}


/************************psy end**************************************/

static int sc8541_set_work_mode(struct sc8541_chip *sc, int mode)
{
	sc->mode = mode;

	dev_err(sc->dev,"work mode is %s\n", sc->mode == SC8541_STANDALONG
		? "standalone" : (sc->mode == SC8541_MASTER ? "master" : "slave"));

	return 0;
}

static int sc8541_parse_dt(struct sc8541_chip *sc, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i;
	int ret;
	struct {
		char *name;
		int *conv_data;
	} props[] = {
		{"sc,sc8541,vbat-ovp-dis", &(sc->cfg.vbat_ovp_dis)},
		{"sc,sc8541,vbat-ovp", &(sc->cfg.vbat_ovp)},
		{"sc,sc8541,vbat-ovp-alm-dis", &(sc->cfg.vbat_ovp_alm_dis)},
		{"sc,sc8541,vbat-ovp-alm", &(sc->cfg.vbat_ovp_alm)},
		{"sc,sc8541,ibat-ocp-dis", &(sc->cfg.ibat_ocp_dis)},
		{"sc,sc8541,ibat-ocp", &(sc->cfg.ibat_ocp)},
		{"sc,sc8541,ibat-ocp-alm-dis", &(sc->cfg.ibat_ocp_alm_dis)},
		{"sc,sc8541,ibat-ocp-alm", &(sc->cfg.ibat_ocp_alm)},
		{"sc,sc8541,ibus-ucp-dis", &(sc->cfg.ibus_ucp_dis)},
		{"sc,sc8541,vbus-in-range-dis", &(sc->cfg.vbus_in_range_dis)},
		{"sc,sc8541,vbus-pd-en", &(sc->cfg.vbus_pd_en)},
		{"sc,sc8541,vbus-ovp", &(sc->cfg.vbus_ovp)},
		{"sc,sc8541,vbus-ovp-alm-dis", &(sc->cfg.vbus_ovp_alm_dis)},
		{"sc,sc8541,vbus-ovp-alm", &(sc->cfg.vbus_ovp_alm)},
		{"sc,sc8541,ibus-ocp-dis", &(sc->cfg.ibus_ocp_dis)},
		{"sc,sc8541,ibus-ocp", &(sc->cfg.ibus_ocp)},
		{"sc,sc8541,tshut-dis", &(sc->cfg.tshut_dis)},
		{"sc,sc8541,tsbus-flt-dis", &(sc->cfg.tsbus_flt_dis)},
		{"sc,sc8541,tsbat-flt-dis", &(sc->cfg.tsbat_flt_dis)},
		{"sc,sc8541,tdie-alm", &(sc->cfg.tdie_alm)},
		{"sc,sc8541,tsbus-flt", &(sc->cfg.tsbus_flt)},
		{"sc,sc8541,tsbat-flt", &(sc->cfg.tsbat_flt)},
		{"sc,sc8541,vac1-ovp", &(sc->cfg.vac1_ovp)},
		{"sc,sc8541,vac2-ovp", &(sc->cfg.vac2_ovp)},
		{"sc,sc8541,vac1-pd-en", &(sc->cfg.vac1_pd_en)},
		{"sc,sc8541,vac2-pd-en", &(sc->cfg.vac2_pd_en)},
		{"sc,sc8541,fsw-set", &(sc->cfg.fsw_set)},
		{"sc,sc8541,wd-timeout", &(sc->cfg.wd_timeout)},
		{"sc,sc8541,wd-timeout-dis", &(sc->cfg.wd_timeout_dis)},
		{"sc,sc8541,ibat-sns-r", &(sc->cfg.ibat_sns_r)},
		{"sc,sc8541,ss-timeout", &(sc->cfg.ss_timeout)},
		{"sc,sc8541,ibus-ucp-fall-dg", &(sc->cfg.ibus_ucp_fall_dg)},
		{"sc,sc8541,vout-ovp-dis", &(sc->cfg.vout_ovp_dis)},
		{"sc,sc8541,vout-ovp", &(sc->cfg.vout_ovp)},
		{"sc,sc8541,pmid2out-uvp", &(sc->cfg.pmid2out_uvp)},
		{"sc,sc8541,pmid2out-ovp", &(sc->cfg.pmid2out_ovp)},
	};

	/* initialize data for optional properties */
	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = of_property_read_u32(np, props[i].name,
						props[i].conv_data);
		if (ret < 0) {
			dev_err(sc->dev, "can not read %s \n", props[i].name);
			return ret;
		}
	}

	sc->irq_gpio = of_get_named_gpio(np, "sc8541,intr_gpio", 0);
	if (!gpio_is_valid(sc->irq_gpio)) {
		dev_err(sc->dev,"fail to valid gpio : %d\n", sc->irq_gpio);
		return -EINVAL;
	}

	return 0;
}

static struct of_device_id sc8541_charger_match_table[] = {
	{	.compatible = "sc,sc8541-standalone",
		.data = &sc8541_mode_data[SC8541_STANDALONG],
	},

	{	.compatible = "sc,sc8541-master",
		.data = &sc8541_mode_data[SC8541_MASTER],
	},

	{	.compatible = "sc,sc8541-slave",
		.data = &sc8541_mode_data[SC8541_SLAVE],
	},

	{},
};

static int sc8541_charger_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
	struct sc8541_chip *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret, i;

	dev_err(&client->dev, "%s (%s)\n", __func__, SC8541_DRV_VERSION);

	sc = devm_kzalloc(&client->dev, sizeof(struct sc8541_chip), GFP_KERNEL);
	if (!sc) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	sc->dev = &client->dev;
	sc->client = client;

	sc->regmap = devm_regmap_init_i2c(client,
							&sc8541_regmap_config);
	if (IS_ERR(sc->regmap)) {
		dev_err(sc->dev, "Failed to initialize regmap\n");
		ret = PTR_ERR(sc->regmap);
		goto err_regmap_init;
	}

	for (i = 0; i < ARRAY_SIZE(sc8541_reg_fields); i++) {
		const struct reg_field *reg_fields = sc8541_reg_fields;

		sc->rmap_fields[i] =
			devm_regmap_field_alloc(sc->dev,
						sc->regmap,
						reg_fields[i]);
		if (IS_ERR(sc->rmap_fields[i])) {
			dev_err(sc->dev, "cannot allocate regmap field\n");
			ret = PTR_ERR(sc->rmap_fields[i]);
			goto err_regmap_field;
		}
	}

	ret = sc8541_detect_device(sc);
	if (ret < 0) {
		dev_err(sc->dev, "%s detect device fail\n", __func__);
		goto err_detect_dev;
	}

	i2c_set_clientdata(client, sc);
	sc8541_create_device_node(&(client->dev));

	match = of_match_node(sc8541_charger_match_table, node);
	if (match == NULL) {
		dev_err(sc->dev, "device tree match not found!\n");
		goto err_match_node;
	}

	sc8541_set_work_mode(sc, *(int *)match->data);
	if (ret) {
		dev_err(sc->dev,"Fail to set work mode!\n");
		goto err_set_mode;
	}

	ret = sc8541_parse_dt(sc, &client->dev);
	if (ret < 0) {
		dev_err(sc->dev, "%s parse dt failed(%d)\n", __func__, ret);
		goto err_parse_dt;
	}

	ret = sc8541_init_device(sc);
	if (ret < 0) {
		dev_err(sc->dev, "%s init device failed(%d)\n", __func__, ret);
		goto err_init_device;
	}

	ret = sc8541_psy_register(sc);
	if (ret < 0) {
		dev_err(sc->dev, "%s psy register failed(%d)\n", __func__, ret);
		goto err_register_psy;
	}

	ret = sc8541_register_interrupt(sc);
	if (ret < 0) {
		dev_err(sc->dev, "%s register irq fail(%d)\n",
					__func__, ret);
		goto err_register_irq;
	}

	if (sc->mode == SC8541_MASTER) {
		sc->master_cp_chg = chargerpump_register("master_cp_chg",
								sc->dev, &sc_sc8541_chargerpump_ops, sc);
		if (IS_ERR_OR_NULL(sc->master_cp_chg)) {
			ret = PTR_ERR(sc->master_cp_chg);
			dev_err(sc->dev,"Fail to register master_cp_chg!\n");
			goto err_register_sc_charger;
		}
	} else {
		sc->slave_cp_chg = chargerpump_register("slave_cp_chg",
								sc->dev, &sc_sc8541_chargerpump_ops, sc);
		if (IS_ERR_OR_NULL(sc->slave_cp_chg)) {
			ret = PTR_ERR(sc->slave_cp_chg);
			dev_err(sc->dev,"Fail to register slave_cp_chg!\n");
			goto err_register_sc_charger;
		}
	}

	dev_err(sc->dev, "sc8541[%s] probe successfully!\n", 
			sc->mode == SC8541_MASTER ? "master" : "slave");
	return 0;

err_register_psy:
err_register_irq:
err_register_sc_charger:
err_init_device:
	power_supply_unregister(sc->psy);
err_detect_dev:
err_match_node:
err_set_mode:
err_parse_dt:
err_regmap_init:
err_regmap_field:
	devm_kfree(&client->dev, sc);
err_kzalloc:
	dev_err(&client->dev,"sc8541 probe fail\n");
	return ret;
}


static int sc8541_charger_remove(struct i2c_client *client)
{
	struct sc8541_chip *sc = i2c_get_clientdata(client);

	power_supply_unregister(sc->psy);
	devm_kfree(&client->dev, sc);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc8541_suspend(struct device *dev)
{
	struct sc8541_chip *sc = dev_get_drvdata(dev);

	dev_info(sc->dev, "Suspend successfully!");
	if (device_may_wakeup(dev))
		enable_irq_wake(sc->irq);
	disable_irq(sc->irq);

	return 0;
}
static int sc8541_resume(struct device *dev)
{
	struct sc8541_chip *sc = dev_get_drvdata(dev);

	dev_info(sc->dev, "Resume successfully!");
	if (device_may_wakeup(dev))
		disable_irq_wake(sc->irq);
	enable_irq(sc->irq);

	return 0;
}

static const struct dev_pm_ops sc8541_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(sc8541_suspend, sc8541_resume)
};
#endif

static void sc8541_shutdown(struct i2c_client *client)
{
	struct sc8541_chip *sc = i2c_get_clientdata(client);
	sc8541_enable_adc(sc, 0);
	pr_err("shutdown success\n");
}

static struct i2c_driver sc8541_charger_driver = {
	.driver     = {
		.name   = "sc8541",
		.owner  = THIS_MODULE,
		.of_match_table = sc8541_charger_match_table,
		#ifdef CONFIG_PM_SLEEP
		.pm = &sc8541_pm,
		#endif
	},
	.probe      = sc8541_charger_probe,
	.remove     = sc8541_charger_remove,
	.shutdown   = sc8541_shutdown,
};

module_i2c_driver(sc8541_charger_driver);

MODULE_DESCRIPTION("SC SC8541 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@HUAQIN.com>");

