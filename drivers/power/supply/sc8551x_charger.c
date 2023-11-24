// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
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
#define CONFIG_MTK_CLASS
#ifdef CONFIG_MTK_CLASS
#include "charger_class.h"
#endif /*CONFIG_MTK_CLASS*/

#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
#include "dvchg_class.h"
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/

#define SC8551X_DRV_VERSION              "1.0.0_G"

enum {
    SC8551X_STANDALONG = 0,
    SC8551X_MASTER,
    SC8551X_SLAVE,
};

static const char* sc8551x_psy_name[] = {
    [SC8551X_STANDALONG] = "sc-cp-standalone",
    [SC8551X_MASTER] = "sc-cp-master",
    [SC8551X_SLAVE] = "sc-cp-slave",
};

static const char* sc8551x_irq_name[] = {
    [SC8551X_STANDALONG] = "sc8551x-standalone-irq",
    [SC8551X_MASTER] = "sc8551x-master-irq",
    [SC8551X_SLAVE] = "sc8551x-slave-irq",
};

static int sc8551x_mode_data[] = {
    [SC8551X_STANDALONG] = SC8551X_STANDALONG,
    [SC8551X_MASTER] = SC8551X_MASTER,
    [SC8551X_SLAVE] = SC8551X_SLAVE,
};

enum {
    ADC_IBUS,
    ADC_VBUS,
    ADC_VAC,
    ADC_VOUT,
    ADC_VBAT,
    ADC_IBAT,
    ADC_TSBUS,
    ADC_TSBAT,
    ADC_TDIE,
    ADC_MAX_NUM,
}ADC_CH;

static const u32 sc8551x_adc_accuracy_tbl[ADC_MAX_NUM] = {
	150000,	/* IBUS */
	35000,	/* VBUS */
	35000,	/* VAC */
	20000,	/* VOUT */
	20000,	/* VBAT */
	200000,	/* IBAT */
    0,	/* TSBUS */
    0,	/* TSBAT */
	4,	/* TDIE */
};

static const int sc8551x_adc_m[] = 
    {15625, 375, 5, 125, 125, 3125, 9766, 9766, 5};

static const int sc8551x_adc_l[] = 
    {10000, 100, 1, 100, 100, 1000, 100000, 100000, 10};

enum sc8551x_notify {
    SC8551X_NOTIFY_OTHER = 0,
	SC8551X_NOTIFY_IBUSUCPF,
	SC8551X_NOTIFY_VBUSOVPALM,
	SC8551X_NOTIFY_VBATOVPALM,
	SC8551X_NOTIFY_IBUSOCP,
	SC8551X_NOTIFY_VBUSOVP,
	SC8551X_NOTIFY_IBATOCP,
	SC8551X_NOTIFY_VBATOVP,
	SC8551X_NOTIFY_VOUTOVP,
	SC8551X_NOTIFY_VDROVP,
};

enum sc8551x_error_stata {
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
    { .reg = 0x05, .len = 1, .bit = {
                {.mask = BIT(6), .name = "vac ovp flag", .notify = SC8551X_NOTIFY_OTHER},
                },
    },
    { .reg = 0x08, .len = 2, .bit = {
                {.mask = BIT(4), .name = "ibus ucp fall flag", .notify = SC8551X_NOTIFY_IBUSUCPF},
                {.mask = BIT(6), .name = "ibus ucp rise flag", .notify = SC8551X_NOTIFY_OTHER},
                },
    },
    { .reg = 0x0a, .len = 4, .bit = {
                {.mask = BIT(0), .name = "pin diag fail flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(1), .name = "conv ocp flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(3), .name = "ss timeout flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(7), .name = "tshut flag", .notify = SC8551X_NOTIFY_OTHER},
                },
    },
    { .reg = 0x0b, .len = 1, .bit = {
                {.mask = BIT(3), .name = "wd timeout flag", .notify = SC8551X_NOTIFY_OTHER},
                },
    },
    { .reg = 0x0e, .len = 3, .bit = {
                {.mask = BIT(0), .name = "adc done flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(1), .name = "vbat insert flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(2), .name = "adapter insert flag", .notify = SC8551X_NOTIFY_OTHER},
                },
    },
    { .reg = 0x11, .len = 4, .bit = {
                {.mask = BIT(4), .name = "ibus ocp flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(5), .name = "vbus ovp flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(6), .name = "ibat ocp flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(7), .name = "vbat ovp flag", .notify = SC8551X_NOTIFY_OTHER},
                },
    },
    { .reg = 0x2f, .len = 2, .bit = {
                {.mask = BIT(2), .name = "pmid2out ovp flag", .notify = SC8551X_NOTIFY_OTHER},
                {.mask = BIT(3), .name = "pmid2out uvp flag", .notify = SC8551X_NOTIFY_OTHER},
                },
    },
};
    

/************************************************************************/
#define SC8551X_DEVICE_ID                0x51

#define SC8551X_REG16                    0x16

#define SC8551X_REGMAX                   0x36

enum sc8551x_reg_range {
    SC8551X_VBAT_OVP,
    SC8551X_IBAT_OCP,
    SC8551X_VBUS_OVP,
    SC8551X_IBUS_OCP,
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

#define SC8551X_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define SC8551X_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }

/*N17 code for HQHW-4609 by miaozhichao at 2023/7/18 start*/
static const struct reg_range sc8551x_reg_range[] = {
    [SC8551X_VBAT_OVP]      = SC8551X_CHG_RANGE(3500, 5075, 25, 3500, true),
    [SC8551X_IBAT_OCP]      = SC8551X_CHG_RANGE(2000, 10000, 100, 2000, true),
    [SC8551X_VBUS_OVP]      = SC8551X_CHG_RANGE(6000, 12350, 50, 6000, true),
    [SC8551X_IBUS_OCP]      = SC8551X_CHG_RANGE(1000, 4750, 250, 1000, true),
};
/*N17 code for HQHW-4609 by miaozhichao at 2023/7/18 end*/

enum sc8551x_fields {
    VBAT_OVP_DIS, VBAT_OVP, /*reg00*/
    IBAT_OCP_DIS, IBAT_OCP, /*reg02*/
    VAC_OVP, /*reg05*/
    VBUS_PD_EN, VBUS_OVP, /*reg06*/
    IBUS_OCP_DIS, IBUS_OCP, /*reg08*/
    VBUS_ERRORLO_STAT, VBUS_ERRORHI_STAT, CP_SWITCHING_STAT, /*reg0A*/
    REG_RST, FSW_SET, WD_TIMEOUT_DIS, WD_TIMEOUT, /*reg0B*/
    CHG_EN, MS, /*reg0C*/
    DEVICE_ID, /*reg13*/
    ADC_EN, ADC_RATE, /*reg14*/
    SS_TIMEOUT, VOUT_OVP_DIS, SET_IBAT_SNS_RES, /*reg2B*/
    IBUS_UCP_FALL_DG, /*reg2E*/
    MODE, /*reg31*/
    VBUS_IN_RANGE_DIS, /*reg35*/
    F_MAX_FIELDS,
};


//REGISTER
static const struct reg_field sc8551x_reg_fields[] = {
    /*reg00*/
    [VBAT_OVP_DIS] = REG_FIELD(0x00, 7, 7),
    [VBAT_OVP] = REG_FIELD(0x00, 0, 5),
    /*reg02*/
    [IBAT_OCP_DIS] = REG_FIELD(0x02, 7, 7),
    [IBAT_OCP] = REG_FIELD(0x02, 0, 6),
    /*reg05*/
    [VAC_OVP] = REG_FIELD(0x05, 0, 2),
    /*reg06*/
    [VBUS_PD_EN] = REG_FIELD(0x06, 7, 7),
    [VBUS_OVP] = REG_FIELD(0x06, 0, 6),
    /*reg08*/
    [IBUS_OCP_DIS] = REG_FIELD(0x08, 7, 7),
    [IBUS_OCP] = REG_FIELD(0x08, 0, 3),
    /*reg0A*/
    [VBUS_ERRORLO_STAT] = REG_FIELD(0x0A, 5, 5),
    [VBUS_ERRORHI_STAT] = REG_FIELD(0x0A, 4, 4),
    [CP_SWITCHING_STAT] = REG_FIELD(0x0A, 2, 2),
    /*reg0B*/
    [REG_RST] = REG_FIELD(0x0B, 7, 7),
    [FSW_SET] = REG_FIELD(0x0B, 4, 6),
    [WD_TIMEOUT_DIS] = REG_FIELD(0x0B, 2, 2),
    [WD_TIMEOUT] = REG_FIELD(0x0B, 0, 1),
    /*reg0C*/
    [CHG_EN] = REG_FIELD(0x0C, 7, 7),
    [MS] = REG_FIELD(0x0C, 5, 6),
    /*reg13*/
    [DEVICE_ID] = REG_FIELD(0x13, 0, 7),
    /*reg14*/
    [ADC_EN] = REG_FIELD(0x14, 7, 7),
    [ADC_RATE] = REG_FIELD(0x14, 6, 6),
    /*reg2B*/
    [SS_TIMEOUT] = REG_FIELD(0x2B, 5, 7),
    [VOUT_OVP_DIS] = REG_FIELD(0x2B, 3, 3),
    [SET_IBAT_SNS_RES] = REG_FIELD(0x2B, 1, 1),
    /*reg2E*/
    [IBUS_UCP_FALL_DG] = REG_FIELD(0x2E, 3, 3),
    /*reg31*/
    [MODE] = REG_FIELD(0x31, 0, 0),
    /*reg35*/
    [VBUS_IN_RANGE_DIS] = REG_FIELD(0x35, 6, 6),
};

static const struct regmap_config sc8551x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    
    .max_register = SC8551X_REGMAX,
};

/************************************************************************/

struct sc8551x_cfg_e {
    int vbat_ovp_dis;
    int vbat_ovp;
    int ibat_ocp_dis;
    int ibat_ocp;
    int vac_ovp;
    int vbus_ovp;
    int ibus_ocp_dis;
    int ibus_ocp;
    int fsw_set;
    int wd_timeout_dis;
    int wd_timeout;
    int ss_timeout;
    int vout_ovp_dis;
    int ibat_sns_r;
    int ibus_ucp_fall_dg;
};

struct sc8551x_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    struct sc8551x_cfg_e cfg;
    int irq_gpio;
    int irq;

    int mode;

    bool charge_enabled;
    bool present;
    int vbus_volt;
    int ibus_curr;
    int vbat_volt;
    int ibat_curr;
    int die_temp;

#ifdef CONFIG_MTK_CLASS
    struct charger_device *chg_dev;
#endif /*CONFIG_MTK_CLASS*/

#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
    struct dvchg_dev *charger_pump;
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/

    const char *chg_dev_name;

    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *psy;
};




/********************COMMON API***********************/
__maybe_unused static u8 val2reg(enum sc8551x_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range= &sc8551x_reg_range[id];

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

__maybe_unused static u32 reg2val(enum sc8551x_reg_range id, u8 reg)
{
    const struct reg_range *range= &sc8551x_reg_range[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] :
                  range->offset + range->step * reg;
}
/*********************************************************/
static int sc8551x_field_read(struct sc8551x_chip *sc,
                enum sc8551x_fields field_id, int *val)
{
    int ret;

    ret = regmap_field_read(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "sc8551x read field %d fail: %d\n", field_id, ret);
    }
    
    return ret;
}

static int sc8551x_field_write(struct sc8551x_chip *sc,
                enum sc8551x_fields field_id, int val)
{
    int ret;
    
    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "sc8551x read field %d fail: %d\n", field_id, ret);
    }
    
    return ret;
}

static int sc8551x_read_block(struct sc8551x_chip *sc,
                int reg, uint8_t *val, int len)
{
    int ret;

    ret = regmap_bulk_read(sc->regmap, reg, val, len);
    if (ret < 0) {
        dev_err(sc->dev, "sc8551x read %02x block failed %d\n", reg, ret);
    }

    return ret;
}



/*******************************************************/
__maybe_unused static int sc8551x_detect_device(struct sc8551x_chip *sc)
{
    int ret;
    int val;

    ret = sc8551x_field_read(sc, DEVICE_ID, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s fail(%d)\n", __func__, ret);
        return ret;
    }

    if (val != SC8551X_DEVICE_ID) {
        dev_err(sc->dev, "%s not find SC8551X, ID = 0x%02x\n", __func__, ret);
        return -EINVAL;
    }

    return ret;
}

__maybe_unused static int sc8551x_reg_reset(struct sc8551x_chip *sc)
{
    return sc8551x_field_write(sc, REG_RST, 1);
}

__maybe_unused static int sc8551x_dump_reg(struct sc8551x_chip *sc)
{
    int ret;
    int i;
    int val;

    for (i = 0; i <= SC8551X_REGMAX; i++) {
        ret = regmap_read(sc->regmap, i, &val);
        dev_err(sc->dev, "%s reg[0x%02x] = 0x%02x\n", 
                __func__, i, val);
    }

    return ret;
}

__maybe_unused static int sc8551x_enable_charge(struct sc8551x_chip *sc, bool en)
{
    int ret;
    dev_info(sc->dev,"%s:%d",__func__,en);

	sc8551x_dump_reg(sc);
    ret = sc8551x_field_write(sc, CHG_EN, !!en);

	msleep(200);
	sc8551x_dump_reg(sc);
    return ret;
}


__maybe_unused static int sc8551x_check_charge_enabled(struct sc8551x_chip *sc, bool *enabled)
{
    int ret, val;

    ret = sc8551x_field_read(sc, CP_SWITCHING_STAT, &val);

    *enabled = (bool)val;

    dev_info(sc->dev,"%s:%d", __func__, val);

    return ret;
}

__maybe_unused static int sc8551x_get_status(struct sc8551x_chip *sc, uint32_t *status)
{
    int ret, val;
    *status = 0;

    ret = sc8551x_field_read(sc, VBUS_ERRORHI_STAT, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s fail to read VBUS_ERRORHI_STAT(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0)
        *status |= BIT(ERROR_VBUS_HIGH);

    ret = sc8551x_field_read(sc, VBUS_ERRORLO_STAT, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s fail to read VBUS_ERRORLO_STAT(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0)
        *status |= BIT(ERROR_VBUS_LOW);


    return ret;

}

__maybe_unused static int sc8551x_enable_adc(struct sc8551x_chip *sc, bool en)
{
    dev_info(sc->dev,"%s:%d", __func__, en);
    return sc8551x_field_write(sc, ADC_EN, !!en);
}

__maybe_unused static int sc8551x_set_adc_scanrate(struct sc8551x_chip *sc, bool oneshot)
{
    dev_info(sc->dev,"%s:%d",__func__,oneshot);
    return sc8551x_field_write(sc, ADC_RATE, !!oneshot);
}

static int sc8551x_get_adc_data(struct sc8551x_chip *sc, 
            int channel, int *result)
{
    uint8_t val[2] = {0};
    int ret;
    int tmp_ret = 0;
    struct power_supply *bat_psy = NULL;
    union power_supply_propval val_new;
    
    if(channel >= ADC_MAX_NUM) 
        return -EINVAL;

    //sc8551x_enable_adc(sc, true);
    //msleep(50);

    ret = sc8551x_read_block(sc, SC8551X_REG16 + (channel << 1), val, 2);

    if (ret < 0) {
        return ret;
    }

    *result = (val[1] | (val[0] << 8)) * 
                sc8551x_adc_m[channel] / sc8551x_adc_l[channel];
    
    if (channel == ADC_IBAT) {
	bat_psy = power_supply_get_by_name("battery");
    	if (IS_ERR_OR_NULL(bat_psy)) {
		dev_info(sc->dev,"%s: failed to get battery psy\n", __func__);
		return 0;
    	} else {
		tmp_ret = power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val_new);
		*result = val_new.intval /1000;
	}
    }
    dev_err(sc->dev,"%s %d %d", __func__, channel, *result);

    //sc8551x_enable_adc(sc, false);

    return ret;
}

__maybe_unused static int sc8551x_set_busovp_th(struct sc8551x_chip *sc, int threshold)
{
    int reg_val = val2reg(SC8551X_VBUS_OVP, threshold);

    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return sc8551x_field_write(sc, VBUS_OVP, reg_val);
}

__maybe_unused static int sc8551x_set_busocp_th(struct sc8551x_chip *sc, int threshold)
{
    int reg_val = val2reg(SC8551X_IBUS_OCP, threshold);
    
    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return sc8551x_field_write(sc, IBUS_OCP, reg_val);
}

__maybe_unused static int sc8551x_set_batovp_th(struct sc8551x_chip *sc, int threshold)
{
    int reg_val = val2reg(SC8551X_VBAT_OVP, threshold);
    
    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return sc8551x_field_write(sc, VBAT_OVP, reg_val);
}

__maybe_unused static int sc8551x_set_batocp_th(struct sc8551x_chip *sc, int threshold)
{
    int reg_val = val2reg(SC8551X_IBAT_OCP, threshold);
    
    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return sc8551x_field_write(sc, IBAT_OCP, reg_val);
}

__maybe_unused static int sc8551x_is_vbuslowerr(struct sc8551x_chip *sc, bool *err)
{
    int ret;
    int val;

    ret = sc8551x_field_read(sc, VBUS_ERRORLO_STAT, &val);
    if(ret < 0) {
        return ret;
    }

    dev_info(sc->dev,"%s:%d",__func__,val);

    *err = (bool)val;

    return ret;
}

__maybe_unused static int sc8551x_init_device(struct sc8551x_chip *sc)
{
    int ret = 0;
    int i;
    struct {
        enum sc8551x_fields field_id;
        int conv_data;
    } props[] = {
        {VBAT_OVP_DIS, sc->cfg.vbat_ovp_dis},
        {VBAT_OVP, sc->cfg.vbat_ovp},
        {IBAT_OCP_DIS, sc->cfg.ibat_ocp_dis},
        {IBAT_OCP, sc->cfg.ibat_ocp},
        {VAC_OVP, sc->cfg.vac_ovp},
        {VBUS_OVP, sc->cfg.vbus_ovp},
        {IBUS_OCP_DIS, sc->cfg.ibus_ocp_dis},
        {IBUS_OCP, sc->cfg.ibus_ocp},
        {FSW_SET, sc->cfg.fsw_set},
        {WD_TIMEOUT_DIS, sc->cfg.wd_timeout_dis},
        {WD_TIMEOUT, sc->cfg.wd_timeout},
        {SS_TIMEOUT, sc->cfg.ss_timeout},
        {VOUT_OVP_DIS, sc->cfg.vout_ovp_dis},
        {SET_IBAT_SNS_RES, sc->cfg.ibat_sns_r},
        {IBUS_UCP_FALL_DG, sc->cfg.ibus_ucp_fall_dg},
    };

    ret = sc8551x_reg_reset(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s Failed to reset registers(%d)\n", __func__, ret);
    }
    msleep(10);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = sc8551x_field_write(sc, props[i].field_id, props[i].conv_data);
    }

    if (sc->mode == SC8551X_SLAVE) {
        ret = sc8551x_field_write(sc, VBUS_IN_RANGE_DIS, 1);
    }

    sc8551x_enable_adc(sc,true);

    return ret;
}


/*********************mtk charger interface start**********************************/
#ifdef CONFIG_MTK_CLASS
static inline int to_sc8551x_adc(enum adc_channel chan)
{
	switch (chan) {
	case ADC_CHANNEL_VBUS:
		return ADC_VBUS;
	case ADC_CHANNEL_VBAT:
		return ADC_VBAT;
		//return ADC_VOUT;// real battery arrive need change native !!!!
	case ADC_CHANNEL_IBUS:
		return ADC_IBUS;
	case ADC_CHANNEL_IBAT:
		return ADC_IBAT;
	case ADC_CHANNEL_TEMP_JC:
		return ADC_TDIE;
	case ADC_CHANNEL_VOUT:
		return ADC_VOUT;
	default:
		break;
	}
	return ADC_MAX_NUM;
}


static int mtk_sc8551x_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
    struct sc8551x_chip *sc = charger_get_data(chg_dev);
    int ret;

    ret = sc8551x_check_charge_enabled(sc, en);

    return ret;
}


static int mtk_sc8551x_enable_chg(struct charger_device *chg_dev, bool en)
{
    struct sc8551x_chip *sc = charger_get_data(chg_dev);
    int ret;

    ret = sc8551x_enable_charge(sc,en);

    return ret;
}


static int mtk_sc8551x_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
    struct sc8551x_chip *sc = charger_get_data(chg_dev);
    int mv;
    mv = uV / 1000;

    return sc8551x_set_busovp_th(sc, mv);
}

static int mtk_sc8551x_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
    struct sc8551x_chip *sc = charger_get_data(chg_dev);
    int ma;
    ma = uA / 1000;

    return sc8551x_set_busocp_th(sc, ma);
}

static int mtk_sc8551x_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{   
    struct sc8551x_chip *sc = charger_get_data(chg_dev);
    int ret;

    ret = sc8551x_set_batovp_th(sc, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

static int mtk_sc8551x_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{   
    struct sc8551x_chip *sc = charger_get_data(chg_dev);
    int ret;

    ret = sc8551x_set_batocp_th(sc, uA/1000);
    if (ret < 0)
        return ret;

    return ret;
}


static int mtk_sc8551x_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
			  int *min, int *max)
{
    struct sc8551x_chip *sc = charger_get_data(chg_dev);

    sc8551x_get_adc_data(sc, to_sc8551x_adc(chan), max);

    if(chan != ADC_CHANNEL_TEMP_JC) 
        *max = *max * 1000;
    
    if (min != max)
		*min = *max;

	dev_err(sc->dev, "%s %d %d\n", __func__, chan ,*max);
    return 0;
}

static int mtk_sc8551x_get_adc_accuracy(struct charger_device *chg_dev,
				   enum adc_channel chan, int *min, int *max)
{
    *min = *max = sc8551x_adc_accuracy_tbl[to_sc8551x_adc(chan)];
    return 0;   
}

static int mtk_sc8551x_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
    struct sc8551x_chip *sc = charger_get_data(chg_dev);

    return sc8551x_is_vbuslowerr(sc,err);
}

static int mtk_sc8551x_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    return 0;
}

static int mtk_sc8551x_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
    return 0;
}

static int mtk_sc8551x_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    return 0;
}   

static int mtk_sc8551x_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
    return 0;
}

static int mtk_sc8551x_init_chip(struct charger_device *chg_dev)
{
    struct sc8551x_chip *sc = charger_get_data(chg_dev);

    return sc8551x_init_device(sc);
}

/*N17 code for HQ-329269 by xm tianye9 at 2023/09/15 start*/
static int sc8551x_set_workmode(struct sc8551x_chip *sc, int mode)
{
	int ret = 0;

	ret = sc8551x_field_write(sc, MODE, mode);
	if (!ret)
		dev_info(sc->dev, "%s =%d\n", __func__, mode);

	return ret;
}

 static int sc8551x_get_workmode(struct sc8551x_chip *sc, int *mode)
 {
	int ret = 0, val = 0;

	ret = sc8551x_field_read(sc, MODE, &val);
	if (!ret){
		*mode = val;
		dev_info(sc->dev, "%s =%d\n", __func__, val);
	}
	return ret;
 }

static int sc8551x_ops_set_chg_work_mode(struct charger_device *charger_pump, int mode)
{
	int ret = 0;
	struct sc8551x_chip *sc = charger_get_data(charger_pump);

	dev_info(sc->dev, "N17:%s = %d", __func__, mode);
	ret = sc8551x_set_workmode(sc, mode);

	return ret;
}

static int sc8551x_ops_get_chg_work_mode(struct charger_device *charger_pump, int *mode)
{
	int ret = 0;
	struct sc8551x_chip *sc = charger_get_data(charger_pump);

	ret = sc8551x_get_workmode(sc, mode);
	dev_info(sc->dev, "N17:%s = %d", __func__, *mode);

	return ret;
}
/*N17 code for HQ-329269 by xm tianye9 at 2023/09/15 end*/

/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 start*/
static int sc8551x_get_cp_device_id(void)
{
	return SC8551X_CP;
}
/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 end*/

static const struct charger_ops sc8551x_chg_ops = {
	 .enable = mtk_sc8551x_enable_chg,
	 .is_enabled = mtk_sc8551x_is_chg_enabled,
	 .get_adc = mtk_sc8551x_get_adc,
     .get_adc_accuracy = mtk_sc8551x_get_adc_accuracy,
	 .set_vbusovp = mtk_sc8551x_set_vbusovp,
	 .set_ibusocp = mtk_sc8551x_set_ibusocp,
	 .set_vbatovp = mtk_sc8551x_set_vbatovp,
	 .set_ibatocp = mtk_sc8551x_set_ibatocp,
	 .init_chip = mtk_sc8551x_init_chip,
     .is_vbuslowerr = mtk_sc8551x_is_vbuslowerr,
	 .set_vbatovp_alarm = mtk_sc8551x_set_vbatovp_alarm,
	 .reset_vbatovp_alarm = mtk_sc8551x_reset_vbatovp_alarm,
	 .set_vbusovp_alarm = mtk_sc8551x_set_vbusovp_alarm,
	 .reset_vbusovp_alarm = mtk_sc8551x_reset_vbusovp_alarm,
/*N17 code for HQ-329269 by xm tianye9 at 2023/09/15 start*/
	 .get_cp_work_mode = sc8551x_ops_get_chg_work_mode,
	 .set_cp_work_mode = sc8551x_ops_set_chg_work_mode,
/*N17 code for HQ-329269 by xm tianye9 at 2023/09/15 end*/
/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 start*/
	 .get_cp_device = sc8551x_get_cp_device_id,
/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 end*/
};

static const struct charger_properties sc8551x_chg_props = {
	.alias_name = "sc8551x_chg",
};

#endif /*CONFIG_MTK_CLASS*/
/********************mtk charger interface end*************************************************/

#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
static inline int sc_to_sc8551x_adc(enum sc_adc_channel chan)
{
	switch (chan) {
	case SC_ADC_VBUS:
		return ADC_VBUS;
	case SC_ADC_VBAT:
		return ADC_VBAT;
	case SC_ADC_IBUS:
		return ADC_IBUS;
	case SC_ADC_IBAT:
		return ADC_IBAT;
	case SC_ADC_TDIE:
		return ADC_TDIE;
	default:
		break;
	}
	return ADC_MAX_NUM;
}


static int sc_sc8551x_set_enable(struct dvchg_dev *charger_pump, bool enable)
{
    struct sc8551x_chip *sc = dvchg_get_private(charger_pump);
    int ret;

    ret = sc8551x_enable_charge(sc,enable);

    return ret;
}

static int sc_sc8551x_get_is_enable(struct dvchg_dev *charger_pump, bool *enable)
{
    struct sc8551x_chip *sc = dvchg_get_private(charger_pump);
    int ret;

    ret = sc8551x_check_charge_enabled(sc, enable);

    return ret;
}

static int sc_sc8551x_get_status(struct dvchg_dev *charger_pump, uint32_t *status)
{
    struct sc8551x_chip *sc = dvchg_get_private(charger_pump);
    int ret = 0;

    ret = sc8551x_get_status(sc, status);

    return ret;
}

static int sc_sc8551x_get_adc_value(struct dvchg_dev *charger_pump, enum sc_adc_channel ch, int *value)
{
    struct sc8551x_chip *sc = dvchg_get_private(charger_pump);
    int ret = 0;

    ret = sc8551x_get_adc_data(sc, sc_to_sc8551x_adc(ch), value);

    return ret;
}

static struct dvchg_ops sc_sc8551x_dvchg_ops = {
    .set_enable = sc_sc8551x_set_enable,
    .get_status = sc_sc8551x_get_status,
    .get_is_enable = sc_sc8551x_get_is_enable,
    .get_adc_value = sc_sc8551x_get_adc_value,
};
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/

/********************creat devices note start*************************************************/
static ssize_t sc8551x_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct sc8551x_chip *sc = dev_get_drvdata(dev);
    u8 addr;
    int val;
    u8 tmpbuf[300];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8551x");
    for (addr = 0x0; addr <= SC8551X_REGMAX; addr++) {
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

static ssize_t sc8551x_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct sc8551x_chip *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= SC8551X_REGMAX)
        regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

    return count;
}

static DEVICE_ATTR(registers, 0660, sc8551x_show_registers, sc8551x_store_register);

static void sc8551x_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}
/********************creat devices note end*************************************************/


/*
* interrupt does nothing, just info event chagne, other module could get info
* through power supply interface
*/
#ifdef CONFIG_MTK_CLASS
static inline int status_reg_to_charger(enum sc8551x_notify notify)
{
	switch (notify) {
	case SC8551X_NOTIFY_IBUSUCPF:
		return CHARGER_DEV_NOTIFY_IBUSUCP_FALL;
    case SC8551X_NOTIFY_VBUSOVPALM:
		return CHARGER_DEV_NOTIFY_VBUSOVP_ALARM;
    case SC8551X_NOTIFY_VBATOVPALM:
		return CHARGER_DEV_NOTIFY_VBATOVP_ALARM;
    case SC8551X_NOTIFY_IBUSOCP:
		return CHARGER_DEV_NOTIFY_IBUSOCP;
    case SC8551X_NOTIFY_VBUSOVP:
		return CHARGER_DEV_NOTIFY_VBUS_OVP;
    case SC8551X_NOTIFY_IBATOCP:
		return CHARGER_DEV_NOTIFY_IBATOCP;
    case SC8551X_NOTIFY_VBATOVP:
		return CHARGER_DEV_NOTIFY_BAT_OVP;
    case SC8551X_NOTIFY_VOUTOVP:
		return CHARGER_DEV_NOTIFY_VOUTOVP;
	default:
        return -EINVAL;
		break;
	}
	return -EINVAL;
}
#endif /*CONFIG_MTK_CLASS*/
static void sc8551x_check_fault_status(struct sc8551x_chip *sc)
{
    int ret;
    u8 flag = 0;
    int i,j;
#ifdef CONFIG_MTK_CLASS
    int noti;
#endif /*CONFIG_MTK_CLASS*/

    for (i=0;i < ARRAY_SIZE(cp_intr_flag);i++) {
        ret = sc8551x_read_block(sc, cp_intr_flag[i].reg, &flag, 1);
        for (j=0; j <  cp_intr_flag[i].len; j++) {
            if (flag & cp_intr_flag[i].bit[j].mask) {
                dev_err(sc->dev,"trigger :%s\n",cp_intr_flag[i].bit[j].name);
#ifdef CONFIG_MTK_CLASS
                noti = status_reg_to_charger(cp_intr_flag[i].bit[j].notify);
                if(noti >= 0) {
                    charger_dev_notify(sc->chg_dev, noti);
                }
#endif /*CONFIG_MTK_CLASS*/
            }
        }
    }
}

static irqreturn_t sc8551x_irq_handler(int irq, void *data)
{
    struct sc8551x_chip *sc = data;

    dev_err(sc->dev,"INT OCCURED\n");

    sc8551x_check_fault_status(sc);

    power_supply_changed(sc->psy);

    return IRQ_HANDLED;
}

static int sc8551x_register_interrupt(struct sc8551x_chip *sc)
{
    int ret;

    if (gpio_is_valid(sc->irq_gpio)) {
        ret = gpio_request_one(sc->irq_gpio, GPIOF_DIR_IN, "sc8551x_irq");
        if (ret) {
            dev_err(sc->dev,"failed to request sc8551x_irq\n");
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
                NULL, sc8551x_irq_handler,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                sc8551x_irq_name[sc->mode], sc);

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

static int sc8551x_set_present(struct sc8551x_chip *sc, bool present)
{
	sc->present = present;

	if (present)
		sc8551x_init_device(sc);
	else 
		sc8551x_reg_reset(sc);
	return 0;
}

/************************psy start**************************************/
static enum power_supply_property sc8551x_charger_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_TEMP,
};

static int sc8551x_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct sc8551x_chip *sc = power_supply_get_drvdata(psy);
    int result;
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        sc8551x_check_charge_enabled(sc, &sc->charge_enabled);
        val->intval = sc->charge_enabled;
        break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sc->present;
		break;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 start*/
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        ret = sc8551x_get_adc_data(sc, ADC_VBUS, &result);//VBUS
        if (!ret)
            sc->vbus_volt = result;
        val->intval = sc->vbus_volt;
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
        ret = sc8551x_get_adc_data(sc, ADC_IBUS, &result);//IBUS
        if (!ret)
            sc->ibus_curr = result;
        val->intval = sc->ibus_curr;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8551x_get_adc_data(sc, ADC_VBAT, &result);//VBAT
        if (!ret)
            sc->vbat_volt = result;
        val->intval = sc->vbat_volt;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8551x_get_adc_data(sc, ADC_IBAT, &result);//IBAT
        if (!ret)
            sc->ibat_curr = result;
        val->intval = sc->ibat_curr;
        break;
    case POWER_SUPPLY_PROP_TEMP:
        ret = sc8551x_get_adc_data(sc, ADC_TDIE, &result);//CP TEMP
        if (!ret)
            sc->die_temp = result;
        val->intval = sc->die_temp;
        break;
    default:
        return -EINVAL;
    }
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 end*/
    return 0;
}

static int sc8551x_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct sc8551x_chip *sc = power_supply_get_drvdata(psy);

    switch (prop) {
    case POWER_SUPPLY_PROP_ONLINE:
        sc8551x_enable_charge(sc, val->intval);
        dev_info(sc->dev, "POWER_SUPPLY_PROP_ONLINE: %s\n",
                val->intval ? "enable" : "disable");
        break;
	case POWER_SUPPLY_PROP_PRESENT:
		sc8551x_set_present(sc, !!val->intval);
		break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sc8551x_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
    return 0;
}

static int sc8551x_psy_register(struct sc8551x_chip *sc)
{
    sc->psy_cfg.drv_data = sc;
    sc->psy_cfg.of_node = sc->dev->of_node;

    sc->psy_desc.name = sc8551x_psy_name[sc->mode];

    sc->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    sc->psy_desc.properties = sc8551x_charger_props;
    sc->psy_desc.num_properties = ARRAY_SIZE(sc8551x_charger_props);
    sc->psy_desc.get_property = sc8551x_charger_get_property;
    sc->psy_desc.set_property = sc8551x_charger_set_property;
    sc->psy_desc.property_is_writeable = sc8551x_charger_is_writeable;


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

static int sc8551x_set_work_mode(struct sc8551x_chip *sc, int mode)
{
    sc->mode = mode;
    dev_err(sc->dev,"work mode is %s\n", sc->mode == SC8551X_STANDALONG 
        ? "standalone" : (sc->mode == SC8551X_MASTER ? "master" : "slave"));

    return 0;
}

static int sc8551x_parse_dt(struct sc8551x_chip *sc, struct device *dev)
{
    struct device_node *np = dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,sc8551x,vbat-ovp-dis", &(sc->cfg.vbat_ovp_dis)},
        {"sc,sc8551x,vbat-ovp", &(sc->cfg.vbat_ovp)},
        {"sc,sc8551x,ibat-ocp-dis", &(sc->cfg.ibat_ocp_dis)},
        {"sc,sc8551x,ibat-ocp", &(sc->cfg.ibat_ocp)},
        {"sc,sc8551x,vac-ovp", &(sc->cfg.vac_ovp)},
        {"sc,sc8551x,vbus-ovp", &(sc->cfg.vbus_ovp)},
        {"sc,sc8551x,ibus-ocp-dis", &(sc->cfg.ibus_ocp_dis)},
        {"sc,sc8551x,ibus-ocp", &(sc->cfg.ibus_ocp)},
        {"sc,sc8551x,fsw-set", &(sc->cfg.fsw_set)},
        {"sc,sc8551x,wd-timeout-dis", &(sc->cfg.wd_timeout_dis)},
        {"sc,sc8551x,wd-timeout", &(sc->cfg.wd_timeout)},
        {"sc,sc8551x,ss-timeout", &(sc->cfg.ss_timeout)},
        {"sc,sc8551x,vout-ovp-dis", &(sc->cfg.vout_ovp_dis)},
        {"sc,sc8551x,ibat-sns-r", &(sc->cfg.ibat_sns_r)},
        {"sc,sc8551x,ibus-ucp-fall-dg", &(sc->cfg.ibus_ucp_fall_dg)},
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

    sc->irq_gpio = of_get_named_gpio(np, "sc,sc8551x,irq-gpio", 0);
    if (!gpio_is_valid(sc->irq_gpio)) {
        dev_err(sc->dev,"fail to valid gpio : %d\n", sc->irq_gpio);
        return -EINVAL;
    }

    if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
		sc->chg_dev_name = "charger";
		dev_err(sc->dev, "no charger name\n");
	}

    return 0;
}
/*N17 code for HQ-301145 by miaozhichao at 2023/06/21 start*/
static struct of_device_id sc8551x_charger_match_table[] = {
    {   .compatible = "sc,sc8551x-standalone", 
        .data = &sc8551x_mode_data[SC8551X_STANDALONG], },
    {   .compatible = "sc,sc8551x-master", 
        .data = &sc8551x_mode_data[SC8551X_MASTER], },
    {   .compatible = "sc,sc8551x-slave", 
        .data = &sc8551x_mode_data[SC8551X_SLAVE], },
    {}
};
MODULE_DEVICE_TABLE(of, sc8551x_charger_match_table);
/*N17 code for HQ-301145 by miaozhichao at 2023/06/21 end*/
static int sc8551x_charger_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
    struct sc8551x_chip *sc;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    int ret, i;

    dev_err(&client->dev, "%s (%s)\n", __func__, SC8551X_DRV_VERSION);

    sc = devm_kzalloc(&client->dev, sizeof(struct sc8551x_chip), GFP_KERNEL);
    if (!sc) {
        ret = -ENOMEM;
        goto err_kzalloc;
    }

    sc->dev = &client->dev;
    sc->client = client;

    sc->regmap = devm_regmap_init_i2c(client,
                            &sc8551x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev, "Failed to initialize regmap\n");
        ret = PTR_ERR(sc->regmap);
        goto err_regmap_init;
    }

    for (i = 0; i < ARRAY_SIZE(sc8551x_reg_fields); i++) {
        const struct reg_field *reg_fields = sc8551x_reg_fields;

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

    ret = sc8551x_detect_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s detect device fail\n", __func__);
        goto err_detect_dev;
    }

    i2c_set_clientdata(client, sc);
    sc8551x_create_device_node(&(client->dev));

    match = of_match_node(sc8551x_charger_match_table, node);
    if (match == NULL) {
        dev_err(sc->dev, "device tree match not found!\n");
        goto err_match_node;
    }

    sc8551x_set_work_mode(sc, *(int *)match->data);
    if (ret) {
        dev_err(sc->dev,"Fail to set work mode!\n");
        goto err_set_mode;
    }

    ret = sc8551x_parse_dt(sc, &client->dev);
    if (ret < 0) {
        dev_err(sc->dev, "%s parse dt failed(%d)\n", __func__, ret);
        goto err_parse_dt;
    }

    ret = sc8551x_init_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s init device failed(%d)\n", __func__, ret);
        goto err_init_device;
    }

    ret = sc8551x_psy_register(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s psy register failed(%d)\n", __func__, ret);
        goto err_register_psy;
    }

    ret = sc8551x_register_interrupt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s register irq fail(%d)\n",
                    __func__, ret);
        goto err_register_irq;
    }

#ifdef CONFIG_MTK_CLASS
    sc->chg_dev = charger_device_register(sc->chg_dev_name,
					      &client->dev, sc,
					      &sc8551x_chg_ops,
					      &sc8551x_chg_props);
	if (IS_ERR_OR_NULL(sc->chg_dev)) {
		ret = PTR_ERR(sc->chg_dev);
		dev_err(sc->dev,"Fail to register charger!\n");
        goto err_register_mtk_charger;
	}
#endif /*CONFIG_MTK_CLASS*/

#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
    sc->charger_pump = dvchg_register("sc_dvchg",
                             sc->dev, &sc_sc8551x_dvchg_ops, sc);
    if (IS_ERR_OR_NULL(sc->charger_pump)) {
		ret = PTR_ERR(sc->charger_pump);
		dev_err(sc->dev,"Fail to register charger!\n");
        goto err_register_sc_charger;
	}
#endif /* CONFIG_SOUTHCHIP_DVCHG_CLASS */
    dev_err(sc->dev, "sc8551x[%s] probe successfully!\n", 
            sc->mode == SC8551X_MASTER ? "master" : "slave");
    return 0;

err_register_psy:
err_register_irq:
#ifdef CONFIG_MTK_CLASS
err_register_mtk_charger:
#endif /*CONFIG_MTK_CLASS*/
#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
err_register_sc_charger:
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/
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
    dev_err(&client->dev,"sc8551x probe fail\n");
    return ret;
}


static int sc8551x_charger_remove(struct i2c_client *client)
{
    struct sc8551x_chip *sc = i2c_get_clientdata(client);
/*N17 code for HQ-296089 by miaozhichao at 2023/05/11 start*/
	sc8551x_enable_adc(sc,false);
/*N17 code for HQ-296089 by miaozhichao at 2023/05/11 end*/
    power_supply_unregister(sc->psy);
    devm_kfree(&client->dev, sc);
    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc8551x_suspend(struct device *dev)
{
    struct sc8551x_chip *sc = dev_get_drvdata(dev);

    dev_info(sc->dev, "Suspend successfully!");
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irq);
    disable_irq(sc->irq);

    return 0;
}
static int sc8551x_resume(struct device *dev)
{
    struct sc8551x_chip *sc = dev_get_drvdata(dev);

    dev_info(sc->dev, "Resume successfully!");
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irq);
    enable_irq(sc->irq);

    return 0;
}
/*N17 code for HQ-296089 by miaozhichao at 2023/05/11 start*/
static void sc8551x_charger_shutdown(struct i2c_client *client)
{
	struct sc8551x_chip *sc = i2c_get_clientdata(client);
	dev_info(sc->dev, "shutdown ok!");
	sc8551x_enable_adc(sc,false);
}
/*N17 code for HQ-296089 by miaozhichao at 2023/05/11 end*/
static const struct dev_pm_ops sc8551x_pm = {
    SET_SYSTEM_SLEEP_PM_OPS(sc8551x_suspend, sc8551x_resume)
};
#endif
/*N17 code for HQ-296089 by miaozhichao at 2023/05/11 start*/
/*N17 code for HQ-301145 by miaozhichao at 2023/06/21 start*/
static struct i2c_driver sc8551x_charger_driver = {
    .driver     = {
        .name   = "sc8551x",
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(sc8551x_charger_match_table),
#ifdef CONFIG_PM_SLEEP
        .pm = &sc8551x_pm,
#endif
    },
    .probe      = sc8551x_charger_probe,
    .remove     = sc8551x_charger_remove,
    .shutdown	= sc8551x_charger_shutdown,
};
/*N17 code for HQ-301145 by miaozhichao at 2023/06/21 end*/
/*N17 code for HQ-296089 by miaozhichao at 2023/05/11 end*/
module_i2c_driver(sc8551x_charger_driver);

MODULE_DESCRIPTION("SC SC8551X Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");


