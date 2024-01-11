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
#include <linux/version.h>
#include <linux/phy/phy.h>
#include "../../misc/mediatek/typec/tcpc/inc/tcpm.h"
#include "mtk_battery.h"

#define CONFIG_MTK_CLASS

#ifdef CONFIG_MTK_CLASS
#include "charger_class.h"
#include "mtk_charger.h"
#ifdef CONFIG_MTK_CHARGER_V4P19
#include "charger_type.h"
#include "mtk_charger_intf.h"
#endif /*CONFIG_MTK_CHARGER_V4P19*/
#endif /*CONFIG_MTK_CLASS */

#define SC8989X_DRV_VERSION         "1.0.0_G"
#define DELAY_CHARGE_UI_MS 1000 // must > 100

enum sc8960x_part_no {
    SC8950_PN_NUM   = 0x02,
    BQ25891H_PN_NUM = 0x03,
    SC89890H_PN_NUM = 0x04,
    SC89895_PN_NUM  = 0x04,
    SC89890W_PN_NUM = 0x07,
};

#define SC8989X_VINDPM_MAX           15300      //mV

#define SC8989X_REG7D                0x7D
#define SC8989X_KEY1                 0x48
#define SC8989X_KEY2                 0x54
#define SC8989X_KEY3                 0x53
#define SC8989X_KEY4                 0x38

#define SC8989X_ADC_EN               0x87
#define SC8989X_DPDM3                0x88
#define SC8989X_PRIVATE              0xF9

#define SC8989X_ITERM_STEP           60         //mA
#define SC8989X_ITERM_OFFSET         30         //mA

#define SC8989X_ITC_STEP             60         //mA
#define SC8989X_ITC_OFFSET           60         //mA

#define SC8989X_VOTG_STEP            100        //mV
#define SC8989X_VOTG_OFFSET          3900;      //mV

enum sc8989x_vbus_stat {
    VBUS_STAT_NO_INPUT = 0,
    VBUS_STAT_SDP,
    VBUS_STAT_CDP,
    VBUS_STAT_DCP,
    VBUS_STAT_HVDCP,
    VBUS_STAT_UNKOWN,
    VBUS_STAT_NONSTAND,
    VBUS_STAT_OTG,
    VBUS_STAT_PD,
};

enum sc8989x_chg_stat {
    CHG_STAT_NOT_CHARGE = 0,
    CHG_STAT_PRE_CHARGE,
    CHG_STAT_FAST_CHARGE,
    CHG_STAT_CHARGE_DONE,
};

enum sc8989x_adc_channel {
    SC8989X_ADC_VBAT,
    SC8989X_ADC_VSYS,
    SC8989X_ADC_VBUS,
    SC8989X_ADC_ICC,
    SC8989X_ADC_IBUS,

};

enum vindpm_track {
    SC8989X_TRACK_DIS,
    SC8989X_TRACK_200,
    SC8989X_TRACK_250,
    SC8989X_TRACK_300,
};

enum {
    SC8989X_VBUSSTAT_NOINPUT = 0,
    SC8989X_VBUSSTAT_SDP,
    SC8989X_VBUSSTAT_CDP,
    SC8989X_VBUSSTAT_DCP,
    SC8989X_VBUSSTAT_HVDCP,
    SC8989X_VBUSSTAT_FLOAT,
    SC8989X_VBUSSTAT_NON_STD,
    SC8989X_VBUSSTAT_OTG,
};

enum sc8989x_usbsw {
	USBSW_CHG = 0,
 	USBSW_USB,
};

int real_type = -1;
EXPORT_SYMBOL(real_type);
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2
enum sc8989x_fields {
    EN_HIZ, EN_ILIM, IINDPM,
    DP_DRIVE, DM_DRIVE, VINDPM_OS,
    CONV_START, CONV_RATE, BOOST_FRE, ICO_EN, HVDCP_EN, FORCE_DPDM,
        AUTO_DPDM_EN,
    FORCE_DSEL, WD_RST, OTG_CFG, CHG_CFG, VSYS_MIN, VBATMIN_SEL,
    EN_PUMPX, ICC,
    ITC, ITERM,
    CV, VBAT_LOW, VRECHG,
    EN_ITERM, STAT_DIS, TWD, EN_TIMER, TCHG, JEITA_ISET,
    BAT_COMP, VCLAMP, TJREG,
    FORCE_ICO, TMR2X_EN, BATFET_DIS, JETTA_VSET_WARM, BATFET_DLY, BATFET_RST_EN,
        PUMPX_UP, PUMPX_DN,
    V_OTG, PFM_OTG_DIS, IBOOST_LIM,
    VBUS_STAT, CHG_STAT, PG_STAT, VSYS_STAT,
    FORCE_VINDPM, VINDPM,
    ADC_VBAT,
    ADC_VSYS,
    VBUS_GD,ADC_VBUS,
    ADC_ICC,
    VINDPM_STAT, IINDPM_STAT,
    REG_RST, ICO_STAT, PN, NTC_PROFILE, DEV_VERSION,
    VBAT_REG_LSB,
    ADC_IBUS,
    DP3P3V_DM0V_EN,
    VINDPM_TRACK,
    F_IIC_OPTION,

    F_MAX_FIELDS,
};

enum sc8989x_reg_range {
    SC8989X_IINDPM,
    SC8989X_ICHG,
    SC8989X_IBOOST,
    SC8989X_VBAT_REG,
    SC8989X_VINDPM,
    SC8989X_ITERM,
    SC8989X_VBAT,
    SC8989X_VSYS,
    SC8989X_VBUS,
    SC8989X_ICC,
    SC8989X_IBUS,
    SC8989X_VOTG,
    SC8989X_ITC,

    //bq25891h reg
    BQ2589X_ICHG,
    BQ2589X_ITERM,
    BQ2589X_VOTG,
    BQ2589X_ITC,

};

struct power_supply_desc *p_usb_psy_desc;
EXPORT_SYMBOL(p_usb_psy_desc);

struct reg_range {
    u32 min;
    u32 max;
    u32 step;
    u32 offset;
    const u32 *table;
    u16 num_table;
    bool round_up;
};

struct sc8989x_cfg_e {
    const char *chg_name;
    int ico_en;
    int hvdcp_en;
    int auto_dpdm_en;
    int vsys_min;
    int vbatmin_sel;
    int itrick;
    int iterm;
    int vbat_cv;
    int vbat_low;
    int vrechg;
    int en_term;
    int stat_dis;
    int wd_time;
    int en_timer;
    int charge_timer;
    int bat_comp;
    int vclamp;
    int votg;
    int iboost;
    int force_vindpm;
    int vindpm;
};

/* These default values will be applied if there's no property in dts */
static struct sc8989x_cfg_e sc8989x_default_cfg = {
    .chg_name = "primary_chg",
    .ico_en = 0,
    .hvdcp_en = 1,
    .auto_dpdm_en = 1,
    .vsys_min = 5,
    .vbatmin_sel = 0,
    .itrick = 3,
    .iterm = 3,
    .vbat_cv = 38,
    .vbat_low = 1,
    .vrechg = 0,
    .en_term = 1,
    .stat_dis = 0,
    .wd_time = 0,
    .en_timer = 1,
    .charge_timer = 2,
    .bat_comp = 0,
    .vclamp = 0,
    .votg = 12,
    .iboost = 3,
    .force_vindpm = 0,
    .vindpm = 20,
};

struct sc8989x_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

#ifdef CONFIG_MTK_CLASS
    struct charger_device *chg_dev;
    struct charger_properties chg_props;
#ifdef CONFIG_MTK_CHARGER_V4P19
    enum charger_type adp_type;
    struct power_supply *chg_psy;
    struct delayed_work psy_dwork;
#endif /*CONFIG_MTK_CHARGER_V4P19*/
#endif /*CONFIG_MTK_CLASS */

    int chg_type;
    int psy_usb_type;
    struct tcpc_device *tcpc;
    int irq_gpio;
    int irq;

    struct delayed_work force_detect_dwork;
    struct mutex force_detect_lock;
    int force_detect_count;
    bool pr_swap;

    int power_good;
    int vbus_good;
    uint8_t dev_id;

    struct power_supply_desc psy_desc;
    struct sc8989x_cfg_e *cfg;
    struct power_supply *psy;
    struct power_supply *chg_psy;
    struct mutex i2c_rw_lock;

    struct delayed_work chg_ui_rush_dwork;
    ktime_t usb_plug_in_time;

    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    struct delayed_work icl_step_dwork;
    struct delayed_work chg_enable_dwork;
    int icl_goal;
    int bc12_done_flag;
    /* End for HTH-342463 c2c charging disconnect 20230926 */
};

static const u32 sc8989x_iboost[] = {
    500, 750, 1200, 1400, 1650, 1875, 2150, 2450,
};

#define SC8989X_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define SC8989X_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }

static const struct reg_range sc8989x_reg_range_ary[] = {
    [SC8989X_IINDPM] = SC8989X_CHG_RANGE(100, 3250, 50, 100, false),
    [SC8989X_ICHG] = SC8989X_CHG_RANGE(0, 5040, 60, 0, false),
    [SC8989X_ITERM] = SC8989X_CHG_RANGE(30, 930, 60, 30, false),
    [SC8989X_VBAT_REG] = SC8989X_CHG_RANGE(3840, 4848, 16, 3840, false),
    [SC8989X_VINDPM] = SC8989X_CHG_RANGE(3900, 15300, 100, 2600, false),
    [SC8989X_IBOOST] = SC8989X_CHG_RANGE_T(sc8989x_iboost, false),
    [SC8989X_VBAT] = SC8989X_CHG_RANGE(2304, 4848, 20, 2304, false),
    [SC8989X_VSYS] = SC8989X_CHG_RANGE(2304, 4848, 20, 2304, false),
    [SC8989X_VBUS] = SC8989X_CHG_RANGE(2600, 15300, 100, 2600, false),
    [SC8989X_ICC] = SC8989X_CHG_RANGE(0, 6350, 50, 0, false),
    [SC8989X_IBUS] = SC8989X_CHG_RANGE(0, 6350, 50, 0, false),
    [SC8989X_VOTG] = SC8989X_CHG_RANGE(3900, 5400, 100, 3900, false),
    [SC8989X_ITC] = SC8989X_CHG_RANGE(60, 960, 60, 60, false),

    //BQ2589X REG
    [BQ2589X_ICHG] = SC8989X_CHG_RANGE(0, 5056, 64, 0, false),
    [BQ2589X_ITERM] = SC8989X_CHG_RANGE(64, 1024, 64, 64, false),
    [BQ2589X_VOTG] = SC8989X_CHG_RANGE(4550, 5510, 64, 4550, false),
    [BQ2589X_ITC] = SC8989X_CHG_RANGE(64, 1024, 64, 64, false),
};

//REGISTER
static const struct reg_field sc8989x_reg_fields[] = {
    /*reg00 */
    [EN_HIZ] = REG_FIELD(0x00, 7, 7),
    [EN_ILIM] = REG_FIELD(0x00, 6, 6),
    [IINDPM] = REG_FIELD(0x00, 0, 5),
    /*reg01 */
    [DP_DRIVE] = REG_FIELD(0x01, 5, 7),
    [DM_DRIVE] = REG_FIELD(0x01, 2, 4),
    [VINDPM_OS] = REG_FIELD(0x01, 0, 0),
    /*reg02 */
    [CONV_START] = REG_FIELD(0x02, 7, 7),
    [CONV_RATE] = REG_FIELD(0x02, 6, 6),
    [BOOST_FRE] = REG_FIELD(0x02, 5, 5),
    [ICO_EN] = REG_FIELD(0x02, 4, 4),
    [HVDCP_EN] = REG_FIELD(0x02, 3, 3),
    [FORCE_DPDM] = REG_FIELD(0x02, 1, 1),
    [AUTO_DPDM_EN] = REG_FIELD(0x02, 0, 0),
    /*reg03 */
    [FORCE_DSEL] = REG_FIELD(0x03, 7, 7),
    [WD_RST] = REG_FIELD(0x03, 6, 6),
    [OTG_CFG] = REG_FIELD(0x03, 5, 5),
    [CHG_CFG] = REG_FIELD(0x03, 4, 4),
    [VSYS_MIN] = REG_FIELD(0x03, 1, 3),
    [VBATMIN_SEL] = REG_FIELD(0x03, 0, 0),
    /*reg04 */
    [EN_PUMPX] = REG_FIELD(0x04, 7, 7),
    [ICC] = REG_FIELD(0x04, 0, 6),
    /*reg05 */
    [ITC] = REG_FIELD(0x05, 4, 7),
    [ITERM] = REG_FIELD(0x05, 0, 3),
    /*reg06 */
    [CV] = REG_FIELD(0x06, 2, 7),
    [VBAT_LOW] = REG_FIELD(0x06, 1, 1),
    [VRECHG] = REG_FIELD(0x06, 0, 0),
    /*reg07 */
    [EN_ITERM] = REG_FIELD(0x07, 7, 7),
    [STAT_DIS] = REG_FIELD(0x07, 6, 6),
    [TWD] = REG_FIELD(0x07, 4, 5),
    [EN_TIMER] = REG_FIELD(0x07, 3, 3),
    [TCHG] = REG_FIELD(0x07, 1, 2),
    [JEITA_ISET] = REG_FIELD(0x07, 0, 0),
    /*reg08 */
    [BAT_COMP] = REG_FIELD(0x08, 5, 7),
    [VCLAMP] = REG_FIELD(0x08, 2, 4),
    [TJREG] = REG_FIELD(0x08, 0, 1),
    /*reg09 */
    [FORCE_ICO] = REG_FIELD(0x09, 7, 7),
    [TMR2X_EN] = REG_FIELD(0x09, 6, 6),
    [BATFET_DIS] = REG_FIELD(0x09, 5, 5),
    [JETTA_VSET_WARM] = REG_FIELD(0x09, 4, 4),
    [BATFET_DLY] = REG_FIELD(0x09, 3, 3),
    [BATFET_RST_EN] = REG_FIELD(0x09, 2, 2),
    [PUMPX_UP] = REG_FIELD(0x09, 1, 1),
    [PUMPX_DN] = REG_FIELD(0x09, 0, 0),
    /*reg0A */
    [V_OTG] = REG_FIELD(0x0A, 4, 7),
    [PFM_OTG_DIS] = REG_FIELD(0x0A, 3, 3),
    [IBOOST_LIM] = REG_FIELD(0x0A, 0, 2),
    /*reg0B */
    [VBUS_STAT] = REG_FIELD(0x0B, 5, 7),
    [CHG_STAT] = REG_FIELD(0x0B, 3, 4),
    [PG_STAT] = REG_FIELD(0x0B, 2, 2),
    [VSYS_STAT] = REG_FIELD(0x0B, 0, 0),
    /*reg0D */
    [FORCE_VINDPM] = REG_FIELD(0x0D, 7, 7),
    [VINDPM] = REG_FIELD(0x0D, 0, 6),
    /*reg0E */
    [ADC_VBAT] = REG_FIELD(0x0E, 0, 6),
    /*reg0F */
    [ADC_VSYS] = REG_FIELD(0x0F, 0, 6),
    /*reg11 */
    [VBUS_GD] = REG_FIELD(0x11, 7, 7),
    [ADC_VBUS] = REG_FIELD(0x11, 0, 6),
    /*reg12 */
    [ADC_ICC] = REG_FIELD(0x12, 0, 6),
    /*reg13 */
    [VINDPM_STAT] = REG_FIELD(0x13, 7, 7),
    [IINDPM_STAT] = REG_FIELD(0x13, 6, 6),
    /*reg14 */
    [REG_RST] = REG_FIELD(0x14, 7, 7),
    [ICO_STAT] = REG_FIELD(0x14, 6, 6),
    [PN] = REG_FIELD(0x14, 3, 5),
    [NTC_PROFILE] = REG_FIELD(0x14, 2, 2),
    [DEV_VERSION] = REG_FIELD(0x14, 0, 1),
    /*reg40 */    
    [VBAT_REG_LSB] = REG_FIELD(0x40, 6, 6),
    /*reg86 */    
    [ADC_IBUS] = REG_FIELD(0x86, 1, 7),
    /*reg83 */
    [DP3P3V_DM0V_EN] = REG_FIELD(0x83, 5, 5),
    [VINDPM_TRACK] = REG_FIELD(0x85, 1, 2),
    /*reg82 */
    [F_IIC_OPTION] = REG_FIELD(0x82, 0, 1),
};

static const struct regmap_config sc8989x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    .max_register = 0xFF,
};

/********************COMMON API***********************/
static u8 val2reg(enum sc8989x_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range = &sc8989x_reg_range_ary[id];

    if (!range)
        return val;

    if (range->table) {
        if (val <= range->table[0])
            return 0;
        for (i = 1; i < range->num_table - 1; i++) {
            if (val == range->table[i])
                return i;
            if (val > range->table[i] && val < range->table[i + 1])
                return range->round_up ? i + 1 : i;
        }
        return range->num_table - 1;
    }
    if (val <= range->min)
        reg = (range->min - range->offset) / range->step;
    else if (val >= range->max)
        reg = (range->max - range->offset) / range->step;
    else if (range->round_up)
        reg = (val - range->offset) / range->step + 1;
    else
        reg = (val - range->offset) / range->step;
    return reg;
}

static u32 reg2val(enum sc8989x_reg_range id, u8 reg)
{
    const struct reg_range *range = &sc8989x_reg_range_ary[id];
    if (!range)
        return reg;

    return range->table ? range->table[reg] : range->offset + range->step * reg;
}

/*********************I2C API*********************/
static int sc8989x_field_read(struct sc8989x_chip *sc,
                            enum sc8989x_fields field_id, int *val)
{
    int ret;
    int count = 3;

    mutex_lock(&sc->i2c_rw_lock);
    while (count--) {
        ret = regmap_field_read(sc->rmap_fields[field_id], val);
        if (ret < 0) {
            dev_err(sc->dev, "sc8989x read field %d fail: %d\n", field_id, ret);
        } else {
            mutex_unlock(&sc->i2c_rw_lock);
            return ret;
        }
        udelay(200);
    }
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

static int sc8989x_field_write(struct sc8989x_chip *sc,
                            enum sc8989x_fields field_id, int val)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "sc8989x read field %d fail: %d\n", field_id, ret);
    }
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

/*********************CHIP API*********************/
int Charger_Detect_Init(struct sc8989x_chip *sc)
{
    struct phy *phy = NULL;
    int ret = 0;

    pr_info("%s: start\n",__func__);

    if (IS_ERR_OR_NULL(sc)) {
        pr_err("sc is NULL\n");
        return false;
	}

    phy = phy_get(sc->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		pr_err("failed to get usb2-phy\n");
		return false;
	}

    ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, PHY_MODE_BC11_SET);
    if (ret) {
        dev_err(sc->dev, "%s:phy_set_mode_ext Fail\n", __func__);
        return false;
    }

    dev_err(sc->dev, "%s: successful\n", __func__);
    return true;
}
EXPORT_SYMBOL(Charger_Detect_Init);

int Charger_Detect_Release(struct sc8989x_chip *sc)
{
    struct phy *phy = NULL;
    int ret = 0;

    pr_info("%s: start\n",__func__);

    if (IS_ERR_OR_NULL(sc)) {
        pr_err("sc is NULL\n");
        return false;
	}

    phy = phy_get(sc->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
        pr_err("failed to get usb2-phy\n");
        return false;
	}

    ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, PHY_MODE_BC11_CLR);
    if (ret) {
        dev_err(sc->dev, "%s: phy_set_mode_ext Fail\n", __func__);
        return false;
    }

    dev_err(sc->dev, "%s: successful\n", __func__);
    return true;
}
EXPORT_SYMBOL(Charger_Detect_Release);

static int sc8989x_set_key(struct sc8989x_chip *sc)
{
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY1);
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY2);
    regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY3);
    return regmap_write(sc->regmap, SC8989X_REG7D, SC8989X_KEY4);
}


static int sc8989x_set_iic_option(struct sc8989x_chip *sc) {
    int ret;
    int val;

    ret = sc8989x_field_read(sc, F_IIC_OPTION, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }    
    sc8989x_field_write(sc, F_IIC_OPTION, 0x00);

    return sc8989x_set_key(sc);
}

static int sc8989x_set_wa(struct sc8989x_chip *sc)
{
    int ret;
    int val;

    if (IS_ERR_OR_NULL(sc)) {
        return -EINVAL;
    }
    if (sc->dev_id == BQ25891H_PN_NUM) {
        dev_err(sc->dev, "%s: BQ25891H needn't set\n", __func__);
        return true;
    }

    ret = regmap_read(sc->regmap, SC8989X_DPDM3, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }

    regmap_write(sc->regmap, SC8989X_DPDM3, SC8989X_PRIVATE);

    return sc8989x_set_key(sc);
}

__maybe_unused static int sc8989x_set_vbat_lsb(struct sc8989x_chip *sc, bool en)
{
    int ret;
    int val;

    if (IS_ERR_OR_NULL(sc)) {
        return -EINVAL;
    }

    if (sc->dev_id == BQ25891H_PN_NUM) {
        dev_err(sc->dev, "%s: BQ25891H needn't set vbat lsb\n", __func__);
        return true;
    }

    ret = sc8989x_field_read(sc, VBAT_REG_LSB, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }

    sc8989x_field_write(sc, VBAT_REG_LSB, en);

    return sc8989x_set_key(sc);
}

__maybe_unused static int sc8989x_set_vindpm_track(struct sc8989x_chip *sc,
                                     enum vindpm_track track)
{
    int ret;
    int val;

    ret = sc8989x_field_read(sc, VINDPM_TRACK, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }
    sc8989x_field_write(sc, VINDPM_TRACK, track);

    return sc8989x_set_key(sc);
}

__maybe_unused static int sc8989x_adc_ibus_en(struct sc8989x_chip *sc, bool en)
{
    int ret;
    int val;

    ret = regmap_read(sc->regmap, SC8989X_ADC_EN, &val);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }
    ret = regmap_read(sc->regmap, SC8989X_ADC_EN, &val);
    val = en ? val | BIT(2) : val & ~BIT(2);

    regmap_write(sc->regmap, SC8989X_ADC_EN, val);

    return sc8989x_set_key(sc);
}

__maybe_unused static int sc8989x_get_adc_ibus(struct sc8989x_chip *sc, int *val)
{
    int ret;
    int reg = 0;

    ret = sc8989x_field_read(sc, ADC_IBUS, &reg);
    if (ret < 0) {
        sc8989x_set_key(sc);
    }
    ret = sc8989x_field_read(sc, ADC_IBUS, &reg);

    *val = reg2val(SC8989X_IBUS, (u8)reg);

    return 0;
}

__maybe_unused static int __sc8989x_get_adc(struct sc8989x_chip *sc, enum sc8989x_adc_channel chan,
                            int *val) {
    int reg_val, ret = 0;
    enum sc8989x_fields field_id;
    enum sc8989x_reg_range range_id;
    //stat adc conversion  default: one shot
    //sc8989x_field_write(sc, CONV_START, true);
    msleep(20);
    switch (chan) {
    case SC8989X_ADC_VBAT:
        field_id = ADC_VBAT;
        range_id = SC8989X_VBAT;
        break;
    case SC8989X_ADC_VSYS:
        field_id = ADC_VSYS;
        range_id = SC8989X_VSYS;
        break;
    case SC8989X_ADC_VBUS:
        field_id = ADC_VBUS;
        range_id = SC8989X_VBUS;
        break;
    case SC8989X_ADC_ICC:
        field_id = ADC_ICC;
        range_id = SC8989X_ICC;
        break;
    case SC8989X_ADC_IBUS:
        sc8989x_get_adc_ibus(sc, val);
        dev_info(sc->dev, "get_adc channel: %d val: %d", chan, *val);
        return 0;
    default:
        goto err;
    }

    ret = sc8989x_field_read(sc, field_id, &reg_val);
    if (ret < 0)
        goto err;

    *val = reg2val(range_id, reg_val);
    dev_info(sc->dev, "get_adc channel: %d val: %d", chan, *val);

    return 0;
err:
    dev_err(sc->dev, "get_adc fail channel: %d", chan);
    return -EINVAL;
}

__maybe_unused static int sc8989x_chg_set_usbsw(struct sc8989x_chip *sc,
		 enum sc8989x_usbsw usbsw)
{
	struct phy *phy;
  	int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
  					       PHY_MODE_BC11_CLR;

    pr_info("usbsw=%d\n", usbsw);

  	phy = phy_get(sc->dev, "usb2-phy");
  	if (IS_ERR_OR_NULL(phy)) {
        pr_err("failed to get usb2-phy\n");
  		return -ENODEV;
  	}

  	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
  	if (ret)
 		dev_err(sc->dev, "failed to set phy ext mode\n");

  	phy_put(sc->dev, phy);

  	return ret;
}

static int sc8989x_get_iindpm(struct sc8989x_chip *sc, int *curr_ma)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, IINDPM, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read iindpm failed(%d)\n", ret);
        return ret;
    }

    *curr_ma = reg2val(SC8989X_IINDPM, reg_val);

    return ret;
}

/* Begin for HTH-342463 c2c charging disconnect 20230926 */
static int sc8989x_write_iindpm(struct sc8989x_chip *sc, int curr_ma)
{
    int reg_val = val2reg(SC8989X_IINDPM, curr_ma);
    sc8989x_field_write(sc, EN_ILIM, 0);

    return sc8989x_field_write(sc, IINDPM, reg_val);
}

static int sc8989x_set_iindpm(struct sc8989x_chip *sc, int curr_ma)
{
    int ret = 0;
    int temp_ma = 0;

    ret = sc8989x_get_iindpm(sc,&temp_ma);
    if (ret) {
        dev_err(sc->dev, "read iindpm failed(%d)\n", ret);
    }

    if(temp_ma == curr_ma) {
        dev_err(sc->dev, "temp_ma = curr_ma: %d, needn't get\n", curr_ma);
        return 0;
    }

    if (sc->dev_id == SC89890H_PN_NUM) {
        if (!sc->vbus_good) {
            cancel_delayed_work(&sc->icl_step_dwork);
	    ret = sc8989x_write_iindpm(sc, curr_ma);
	    dev_info(sc->dev, "sc vbus absent, set icl to %dmA", curr_ma);
	} else {
            dev_info(sc->dev, "sc new icl:%dmA, last icl:%dmA", curr_ma, sc->icl_goal);
	    sc->icl_goal = curr_ma;
	    if (!sc->bc12_done_flag) {		// bc1.2 ongoing
                dev_info(sc->dev, "sc bc1.2 ongoing, set iindpm skip");
	    } else if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP) {	//sdp
	        schedule_delayed_work(&sc->icl_step_dwork, msecs_to_jiffies(100)); // delay 100ms
		dev_info(sc->dev, "sc sdp set new icl:%dmA", curr_ma);
	    } else {
	        ret = sc8989x_write_iindpm(sc, curr_ma);
		dev_info(sc->dev, "sc write icl:%dmA", curr_ma);
	    }
	}
    } else {
        ret = sc8989x_write_iindpm(sc, curr_ma);
        dev_info(sc->dev, "ti write icl to %dmA", curr_ma);
    }

    return ret;
}
/* End for HTH-342463 c2c charging disconnect 20230926 */

static int sc8989x_set_dpdm_hiz(struct sc8989x_chip *sc)
{
    sc8989x_field_write(sc, DP_DRIVE, 0);
    return sc8989x_field_write(sc, DM_DRIVE, 0);
}

__maybe_unused static int sc8989x_reset_wdt(struct sc8989x_chip *sc)
{
    return sc8989x_field_write(sc, WD_RST, 1);
}

static int sc8989x_set_chg_enable(struct sc8989x_chip *sc, bool enable)
{
    int ret,reg_val;
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    int old_en;

    ret = sc8989x_field_read(sc, CHG_CFG, &old_en);
    if (ret) {
        dev_err(sc->dev, "read CHG_CFG failed(%d)\n", ret);
        return ret;
    }
    old_en = !!old_en;
    if ((enable && old_en) || (!enable && !old_en)) {
        dev_info(sc->dev, "charge already %s",
            old_en ? "enabled" : "disabled");
        return 0;
    }
    /* End for HTH-342463 c2c charging disconnect 20230926 */

    dev_err(sc->dev, "sc8989x_set_chg_enable (%d)\n", enable);
    ret = sc8989x_field_read(sc, OTG_CFG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read OTG_CFG failed(%d)\n", ret);
        return ret;
    }

    if(reg_val)
	    sc8989x_field_write(sc, OTG_CFG, !reg_val);

    reg_val = enable ? 1 : 0;
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    if (sc->dev_id == SC89890H_PN_NUM) {
        if (!reg_val) { // disable charge
	    dev_info(sc->dev, "disable charge now");
            ret = sc8989x_field_write(sc, CHG_CFG, 0);
            cancel_delayed_work(&sc->chg_enable_dwork);
            cancel_delayed_work(&sc->icl_step_dwork);
	} else {        // enable charge
	    if (!sc->vbus_good) {
	        cancel_delayed_work(&sc->chg_enable_dwork);
                cancel_delayed_work(&sc->icl_step_dwork);
                dev_info(sc->dev, "vbus absent, enable charge skip");
	    } else {
	        if (!sc->bc12_done_flag) // bc1.2 ongoing
                    dev_info(sc->dev, "sc bc1.2 ongoing, enable charge skip");
                else if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP) { // sdp detected
		    sc8989x_write_iindpm(sc, 100);
                    schedule_delayed_work(&sc->chg_enable_dwork, msecs_to_jiffies(100));
                    schedule_delayed_work(&sc->icl_step_dwork, msecs_to_jiffies(200));
                    dev_info(sc->dev, "sc set icl to 100mA before charge enable");
		} else {
		    ret = sc8989x_field_write(sc, CHG_CFG, 1);
                    dev_info(sc->dev, "sc charge enable now");
		}
	    }
	}
    } else {
        ret = sc8989x_field_write(sc, CHG_CFG, reg_val);
        dev_info(sc->dev, "ti charge %s now", reg_val ? "enable" : "disable");
    }
    return ret;
    /* End for HTH-342463 c2c charging disconnect 20230926 */
}

__maybe_unused static int sc8989x_check_chg_enabled(struct sc8989x_chip *sc, bool *enable)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, CHG_CFG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge enable failed(%d)\n", ret);
        return ret;
    }

    *enable = ! !reg_val;

    return ret;
}

__maybe_unused static int sc8989x_set_otg_enable(struct sc8989x_chip *sc, bool enable)
{
    int ret,reg_val;
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    int old_en;

    ret = sc8989x_field_read(sc, OTG_CFG, &old_en);
    if (ret) {
        dev_err(sc->dev, "read CHG_CFG failed(%d)\n", ret);
    }

    old_en = !!old_en;
    if ((enable && old_en) || (!enable && !old_en)) {
        dev_info(sc->dev, "otg already %s",
            old_en ? "enabled" : "disabled");
        return 0;
    }
    /* End for HTH-342463 c2c charging disconnect 20230926 */

    dev_err(sc->dev, "sc8989x_set_otg_enable (%d)\n", enable);
    ret = sc8989x_field_read(sc, CHG_CFG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read CHG_CFG failed(%d)\n", ret);
    }

    if(reg_val) {
        ret = sc8989x_field_write(sc, CHG_CFG, !reg_val);
        if (ret) {
            dev_err(sc->dev, "write CHG_CFG failed(%d)\n", ret);
        }
    }

    reg_val = enable ? 1 : 0;
    ret = sc8989x_field_write(sc, OTG_CFG, reg_val);
    if (ret) {
        dev_err(sc->dev, "write OTG_CFG failed(%d)\n", ret);
    }

    if(!reg_val) {
        ret = sc8989x_field_write(sc, CHG_CFG, !reg_val);
        if (ret) {
            dev_err(sc->dev, "write CHG_CFG failed(%d)\n", ret);
        }
    }

    return ret;
}

__maybe_unused static int sc8989x_set_iboost(struct sc8989x_chip *sc, int curr_ma)
{
    int reg_val = val2reg(SC8989X_IBOOST, curr_ma);

    return sc8989x_field_write(sc, IBOOST_LIM, reg_val);
}

static int sc8989x_set_votg(struct sc8989x_chip *sc, int curr_mv)
{
    int reg_val = 0;

    if (sc->dev_id == BQ25891H_PN_NUM) {
        reg_val = val2reg(BQ2589X_VOTG, curr_mv);
    } else {
        reg_val = val2reg(SC8989X_VOTG, curr_mv);
    }

    return sc8989x_field_write(sc, V_OTG, reg_val);
}

__maybe_unused static int sc8989x_get_votg(struct sc8989x_chip *sc, int *curr_mv)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, V_OTG, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read VOTG failed(%d)\n", ret);
        return ret;
    }

    if (sc->dev_id == BQ25891H_PN_NUM) {
        *curr_mv = reg2val(BQ2589X_VOTG, reg_val);
    } else {
        *curr_mv = reg2val(SC8989X_VOTG, reg_val);
    }

    return ret;
}

static int sc8989x_get_ichg(struct sc8989x_chip *sc, int *curr_ma)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, ICC, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read ICC failed(%d)\n", ret);
        return ret;
    }

    if (sc->dev_id == BQ25891H_PN_NUM) {
        *curr_ma = reg2val(BQ2589X_ICHG, reg_val);
    } else {
        *curr_ma = reg2val(SC8989X_ICHG, reg_val);
    }

    return ret;
}

static int sc8989x_set_ichg(struct sc8989x_chip *sc, int curr_ma)
{
    int reg_val = 0, ret = 0;
    int temp_ma = 0;

    ret = sc8989x_get_ichg(sc,&temp_ma);
    if (ret) {
	    dev_err(sc->dev, "read ICC failed(%d)\n", ret);
    }

    if (temp_ma == curr_ma) {
        dev_err(sc->dev, "temp_ma = curr_ma: %d, needn't get\n", curr_ma);
        return 0;
    }

    if (sc->dev_id == BQ25891H_PN_NUM) {
        reg_val = val2reg(BQ2589X_ICHG, curr_ma);
    } else {
        reg_val = val2reg(SC8989X_ICHG, curr_ma);
    }

    return sc8989x_field_write(sc, ICC, reg_val);
}


static int sc8989x_set_term_curr(struct sc8989x_chip *sc, int curr_ma)
{
    int reg_val = 0;

    if (sc->dev_id == BQ25891H_PN_NUM) {
        reg_val = val2reg(BQ2589X_ITERM, curr_ma);
    } else {
        reg_val = val2reg(SC8989X_ITERM, curr_ma);
    }

    return sc8989x_field_write(sc, ITERM, reg_val);
}

__maybe_unused static int sc8989x_get_term_curr(struct sc8989x_chip *sc, int *curr_ma)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, ITERM, &reg_val);
    if (ret)
        return ret;

    if (sc->dev_id == BQ25891H_PN_NUM) {
        *curr_ma = reg2val(BQ2589X_ITERM, reg_val);
    } else {
        *curr_ma = reg2val(SC8989X_ITERM, reg_val);
    }

    return ret;
}

static int sc8989x_set_trick_curr(struct sc8989x_chip *sc, int curr_ma)
{
    int reg_val = 0;

    if (sc->dev_id == BQ25891H_PN_NUM) {
        reg_val = val2reg(BQ2589X_ITC, curr_ma);
    } else {
        reg_val = val2reg(SC8989X_ITC, curr_ma);
    }

    return sc8989x_field_write(sc, ITC, reg_val);
}

__maybe_unused static int sc8989x_get_trick_curr(struct sc8989x_chip *sc, int *curr_ma)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, ITC, &reg_val);
    if (ret)
        return ret;

    if (sc->dev_id == BQ25891H_PN_NUM) {
        *curr_ma = reg2val(BQ2589X_ITC, reg_val);
    } else {
        *curr_ma = reg2val(SC8989X_ITC, reg_val);
    }

    return ret;
}


__maybe_unused static int sc8989x_set_safet_timer(struct sc8989x_chip *sc, bool enable)
{
    int reg_val = enable ? 1 : 0;

    return sc8989x_field_write(sc, EN_TIMER, reg_val);
}

__maybe_unused static int sc8989x_check_safet_timer(struct sc8989x_chip *sc, bool *enabled)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, EN_TIMER, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read ICEN_TIMERC failed(%d)\n", ret);
        return ret;
    }

    *enabled = reg_val ? true : false;

    return ret;
}

static int sc8989x_set_vbat(struct sc8989x_chip *sc, int volt_mv)
{
    int reg_val = val2reg(SC8989X_VBAT_REG, volt_mv);

    return sc8989x_field_write(sc, CV, reg_val);
}

static int sc8989x_get_vbat(struct sc8989x_chip *sc, int *volt_mv)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, CV, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read vbat reg failed(%d)\n", ret);
        return ret;
    }

    *volt_mv = reg2val(SC8989X_VBAT_REG, reg_val);

    return ret;
}

static int sc8989x_set_vindpm(struct sc8989x_chip *sc, int volt_mv)
{
    int reg_val = val2reg(SC8989X_VINDPM, volt_mv);

    sc8989x_field_write(sc, FORCE_VINDPM, 1);

    return sc8989x_field_write(sc, VINDPM, reg_val);
}

static int sc8989x_get_vindpm(struct sc8989x_chip *sc, int *volt_mv)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, VINDPM, &reg_val);
    if (ret)
        return ret;

    *volt_mv = reg2val(SC8989X_VINDPM, reg_val);

    return ret;
}

__maybe_unused static int sc8989x_set_hiz(struct sc8989x_chip *sc, bool enable)
{
    int reg_val = enable ? 1 : 0;

    sc8989x_set_iindpm(sc, 500);
    sc8989x_set_ichg(sc, 500);

    return sc8989x_field_write(sc, EN_HIZ, reg_val);
}

static int sc8989x_reset_dpdm(struct sc8989x_chip *sc) {
    int ret;
    int val;

    dev_err(sc->dev, "sc8989x_reset_dpdm\n");
    ret = regmap_read(sc->regmap, 0x02, &val);
    if (ret < 0) {
        return ret;
    }
    if(val & 0x02){
        val &= 0xfd;
        ret = regmap_write(sc->regmap, 0x02, val);
    }

    return ret;
}

static int sc8989x_force_dpdm(struct sc8989x_chip *sc)
{
    int ret;
    int val;

    dev_err(sc->dev, "%s: start\n",__func__);
    ret = Charger_Detect_Init(sc);
    if (!ret) {
        dev_err(sc->dev, "%s: Charger_Detect_Init fail\n",__func__);
        return ret;
    }

    ret = regmap_read(sc->regmap, 0x02, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s: regmap_read error\n",__func__);
        return ret;
    }
    val |= 0x02;
    ret = regmap_write(sc->regmap, 0x02, val);

    sc->power_good = 0;
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    sc->bc12_done_flag = 0; // bc1.2 onging
    /* End for HTH-342463 c2c charging disconnect 20230926 */

    dev_err(sc->dev, "%s: successful\n",__func__);

    return ret;
}

static int sc8989x_get_charge_stat(struct sc8989x_chip *sc)
{
    int ret;
    int chg_stat, vbus_stat;

    ret = sc8989x_field_read(sc, CHG_STAT, &chg_stat);
    if (ret)
        return ret;

    ret = sc8989x_field_read(sc, VBUS_STAT, &vbus_stat);
    if (ret)
        return ret;

    if (vbus_stat == VBUS_STAT_OTG) {
        return POWER_SUPPLY_STATUS_DISCHARGING;
    } else {
        switch (chg_stat) {
        case CHG_STAT_NOT_CHARGE:
            return POWER_SUPPLY_STATUS_NOT_CHARGING;
        case CHG_STAT_PRE_CHARGE:
        case CHG_STAT_FAST_CHARGE:
            return POWER_SUPPLY_STATUS_CHARGING;
        case CHG_STAT_CHARGE_DONE:
            return POWER_SUPPLY_STATUS_FULL;
        }
    }

    return POWER_SUPPLY_STATUS_UNKNOWN;
}

__maybe_unused static int sc8989x_check_charge_done(struct sc8989x_chip *sc, bool *chg_done)
{
    int ret, reg_val;

    ret = sc8989x_field_read(sc, CHG_STAT, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read charge stat failed(%d)\n", ret);
        return ret;
    }

    *chg_done = (reg_val == CHG_STAT_CHARGE_DONE) ? true : false;
    return ret;
}

static bool sc8989x_detect_device(struct sc8989x_chip *sc)
{
    int ret;
    int val;

    ret = sc8989x_field_read(sc, PN, &val);
    if (ret < 0 || !(val == SC89890H_PN_NUM || val == SC89895_PN_NUM ||
        val == SC8950_PN_NUM || val == SC89890W_PN_NUM ||val == SC8950_PN_NUM
        || val == BQ25891H_PN_NUM)) {
        dev_err(sc->dev, "not find sc8989x, part_no = %d\n", val);
        return false;
    }

    sc->dev_id = val;
    dev_err(sc->dev, "sc->dev_id: %d\n",sc->dev_id);

    return true;
}

static int sc8989x_dump_register(struct sc8989x_chip *sc)
{
    int ret;
    int i;
    int val;

    for (i = 0; i <= 0x14; i++) {
        ret = regmap_read(sc->regmap, i, &val);
        if (ret < 0) {
            return ret;
        }
        dev_info(sc->dev, "%s reg[0x%02x] = 0x%02x\n", __func__, i, val);
    }

    return 0;
}

#ifdef CONFIG_MTK_CLASS
/********************MTK OPS***********************/
static int sc8989x_plug_in(struct charger_device *chg_dev)
{
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);

    /* Enable charging */
    ret = sc8989x_set_chg_enable(sc, true);
    if (ret) {
        dev_err(sc->dev, "Failed to enable charging:%d\n", ret);
    }

    return ret;
}

static int sc8989x_plug_out(struct charger_device *chg_dev)
{
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8989x_set_chg_enable(sc, false);
    if (ret) {
        dev_err(sc->dev, "Failed to disable charging:%d\n", ret);
    }
    return ret;
}

static int sc8989x_enable(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_set_chg_enable(sc, en);

    dev_info(sc->dev, "%s charger %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sc8989x_is_enabled(struct charger_device *chg_dev, bool * enabled)
{
    int ret;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_check_chg_enabled(sc, enabled);
    dev_info(sc->dev, "charger is %s\n",
            *enabled ? "charging" : "not charging");

    return ret;
}

static int sc8989x_get_charging_current(struct charger_device *chg_dev, u32 *curr)
{
    int ret = 0;
    int curr_ma;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    ret = sc8989x_get_ichg(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

static int sc8989x_set_charging_current(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: charge curr = %duA\n", __func__, curr);

    return sc8989x_set_ichg(sc, curr / 1000);
}

static int sc8989x_get_input_current(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr_ma;
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8989x_get_iindpm(sc, &curr_ma);
    if (!ret) {
        *curr = curr_ma * 1000;
    }

    return ret;
}

static int sc8989x_set_input_current(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    if(sc->dev_id == SC89890H_PN_NUM && sc->bc12_done_flag == 0 ) {
        dev_info(sc->dev, "%s %duA, bc12 is doing, skip\n", __func__, curr);
        return 0;
    }
    /* End for HTH-342463 c2c charging disconnect 20230926 */
    dev_info(sc->dev, "%s: iindpm curr = %duA\n", __func__, curr);

    return sc8989x_set_iindpm(sc, curr / 1000);
}

static int sc8989x_get_constant_voltage(struct charger_device *chg_dev, u32 *volt)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int volt_mv;
    int ret;

    dev_info(sc->dev, "%s\n", __func__);

    ret = sc8989x_get_vbat(sc, &volt_mv);
    if (!ret) {
        *volt = volt_mv * 1000;
    }

    return ret;
}

static int sc8989x_set_constant_voltage(struct charger_device *chg_dev, u32 volt)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: charge volt = %duV\n", __func__, volt);

    return sc8989x_set_vbat(sc, volt / 1000);
}

static int sc8989x_kick_wdt(struct charger_device *chg_dev)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);
    return sc8989x_reset_wdt(sc);
}

static int sc8989x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s: vindpm volt = %d\n", __func__, volt);

    return sc8989x_set_vindpm(sc, volt / 1000);
}

static int sc8989x_get_ivl(struct charger_device *chg_dev, u32 *uV)
{
    int ret = 0;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    unsigned int reg_val;

    ret = sc8989x_get_vindpm(sc, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read vindpm failed(%d)\n", ret);
	return ret;
    }
    *uV = reg_val * 1000;
    dev_info(sc->dev, "%s: vindpm volt = %d\n", __func__, *uV);

    return ret;
}

static int sc8989x_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
    int ret;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    unsigned int reg_val;


    ret = sc8989x_field_read(sc, VINDPM_STAT, &reg_val);
    if (ret) {
        dev_err(sc->dev, "read vindpm stat failed(%d)\n", ret);
	return ret;
    } else {
	*in_loop = reg_val;
        dev_info(sc->dev, "%s: vindpm_stat(%d)\n", __func__, *in_loop);
    }

    return 0;
}

static int sc8989x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    ret = sc8989x_check_charge_done(sc, done);

    dev_info(sc->dev, "%s: charge %s done\n", __func__, *done ? "is" : "not");
    return ret;
}

static int sc8989x_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    *uA = 60 * 1000;
    dev_err(sc->dev, "%s\n", __func__);
    return 0;
}

static int sc8989x_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    *uA = 60 * 1000;
    dev_err(sc->dev, "%s\n", __func__);
    return 0;
}

__maybe_unused static int sc8989x_set_qc_9V(struct charger_device *chg_dev, bool en)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int reg_val_dp = 0;
    int reg_val_dm = 0;
    int ret = -1;

    if(IS_ERR_OR_NULL(sc)) {
        pr_err("%s sc is NULL\n", __func__);
        goto err;
    }

    ret = Charger_Detect_Init(sc);
    if (!ret) {
        dev_err(sc->dev, "%s: Charger_Detect_Init fail\n",__func__);
        goto err;
    }

    if(en) {
        dev_err(sc->dev, "sc8989x_set_qc_9V");
        ret = sc8989x_field_write(sc, DP_DRIVE, 0x06);
        ret |= sc8989x_field_write(sc, DM_DRIVE, 0x02);
    } else {
        dev_err(sc->dev, "sc8989x_set_qc_5V");
        ret = sc8989x_field_write(sc, DP_DRIVE, 0x02);
        ret |= sc8989x_field_write(sc, DM_DRIVE, 0x0);
    }

    ret = Charger_Detect_Init(sc);
    if (!ret) {
        dev_err(sc->dev, "%s: Charger_Detect_Init fail\n",__func__);
        goto err;
    }

    ret = sc8989x_field_read(sc, DP_DRIVE, &reg_val_dp);
    if (ret) {
        dev_err(sc->dev, "read dp_drive failed(%d)\n", ret);
        goto err;
    } else {
        dev_info(sc->dev, "%s: reg_val_dp(%d)\n", __func__, reg_val_dp);
    }

    ret = sc8989x_field_read(sc, DM_DRIVE, &reg_val_dm);
    if (ret) {
        dev_err(sc->dev, "read dm_drive failed(%d)\n", ret);
        goto err;
    } else {
        dev_info(sc->dev, "%s: reg_val_dm(%d)\n", __func__, reg_val_dm);
    }

    return ret;
err:
    pr_err("%s FAIL\n", __func__);
    return ret;
}

static int sc8989x_dump_registers(struct charger_device *chg_dev) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s\n", __func__);

    return sc8989x_dump_register(sc);
}

static int sc8989x_send_ta_current_pattern(struct charger_device *chg_dev, bool is_increase)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    int i;

    dev_info(sc->dev, "%s: %s\n", __func__, is_increase ?
            "pumpx up" : "pumpx dn");
    //pumpx start
    ret = sc8989x_set_iindpm(sc, 100);
    if (ret)
        return ret;
    msleep(10);

    for (i = 0; i < 6; i++) {
        if (i < 3) {
            sc8989x_set_iindpm(sc, 800);
            is_increase ? msleep(100) : msleep(300);
        } else {
            sc8989x_set_iindpm(sc, 800);
            is_increase ? msleep(300) : msleep(100);
        }
        sc8989x_set_iindpm(sc, 100);
        msleep(100);
    }

    //pumpx stop
    sc8989x_set_iindpm(sc, 800);
    msleep(500);
    //pumpx wdt, max 240ms
    sc8989x_set_iindpm(sc, 100);
    msleep(100);

    return sc8989x_set_iindpm(sc, 1500);
}

static int sc8989x_send_ta20_current_pattern(struct charger_device *chg_dev, u32 uV)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    int i;

    if (uV < 5500000) {
        uV = 5500000;
    } else if (uV > 15000000) {
        uV = 15000000;
    }

    val = (uV - 5500000) / 500000;

    dev_info(sc->dev, "%s ta20 vol=%duV, val=%d\n", __func__, uV, val);

    sc8989x_set_iindpm(sc, 100);
    msleep(150);
    for (i = 4; i >= 0; i--) {
        sc8989x_set_iindpm(sc, 800);
        (val & (1 << i)) ? msleep(100) : msleep(50);
        sc8989x_set_iindpm(sc, 100);
        (val & (1 << i)) ? msleep(50) : msleep(100);
    }
    sc8989x_set_iindpm(sc, 800);
    msleep(150);
    sc8989x_set_iindpm(sc, 100);
    msleep(240);

    return sc8989x_set_iindpm(sc, 800);
}

static int sc8989x_set_ta20_reset(struct charger_device *chg_dev)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int curr;
    int ret;

    ret = sc8989x_get_iindpm(sc, &curr);

    ret = sc8989x_set_iindpm(sc, 100);
    msleep(300);
    return sc8989x_set_iindpm(sc, curr);
}

static int sc8989x_get_adc(struct charger_device *chg_dev,
                        enum adc_channel chan, int *min, int *max)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    enum sc8989x_adc_channel sc_chan;
    switch (chan) {
    case ADC_CHANNEL_VBAT:
        sc_chan = SC8989X_ADC_VBAT;
        break;
    case ADC_CHANNEL_VSYS:
        sc_chan = SC8989X_ADC_VSYS;
        break;
    case ADC_CHANNEL_VBUS:
        sc_chan = SC8989X_ADC_VBUS;
        break;
    case ADC_CHANNEL_IBUS:
        sc_chan = SC8989X_ADC_IBUS;
        break;
    default:
        return -95;
    }

    __sc8989x_get_adc(sc, sc_chan, min);
    *min = *min * 1000;
    *max = *min;
    return 0;
}

static int sc8989x_set_otg(struct charger_device *chg_dev, bool enable)
{
    int ret;
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    if(enable) {
    	ret = sc8989x_chg_set_usbsw(sc, USBSW_USB);
    	if(ret < 0) {
            dev_err(sc->dev, "%s: failed to set USB(%d)\n",__func__,ret);
    	}
    } else {
	ret = sc8989x_chg_set_usbsw(sc, USBSW_CHG);
        if(ret < 0) {
            dev_err(sc->dev, "%s: failed to set USB(%d)\n",__func__,ret);
        }
    }
    ret = sc8989x_set_otg_enable(sc, enable);
    /* ret |= sc8989x_set_chg_enable(sc, !enable); */

    dev_info(sc->dev, "%s OTG %s\n", enable ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int sc8989x_set_safety_timer(struct charger_device *chg_dev, bool enable)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s  %s\n", __func__, enable ? "enable" : "disable");

    return sc8989x_set_safet_timer(sc, enable);
}

static int sc8989x_is_safety_timer_enabled(struct charger_device *chg_dev, bool *enabled)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_check_safet_timer(sc, enabled);
}

static int sc8989x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s otg curr = %d\n", __func__, curr);
    return sc8989x_set_iboost(sc, curr / 1000);
}

static int sc8989x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    dev_info(sc->dev, "%s\n", __func__);

#ifdef CONFIG_MTK_CHARGER_V4P19
    switch (event) {
    case EVENT_EOC:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
    break;
    case EVENT_RECHARGE:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
    break;
    default:
    break;
    }
#else
    switch (event) {
    case EVENT_FULL:
    case EVENT_RECHARGE:
    case EVENT_DISCHARGE:
        power_supply_changed(sc->psy);
        break;
    default:
        break;
    }
#endif /*CONFIG_MTK_CHARGER_V4P19*/

    return 0;
}

static int sc8989x_enable_hz(struct charger_device *chg_dev, bool enable)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s %s\n", __func__, enable ? "enable" : "disable");

    return sc8989x_set_hiz(sc, enable);
}

static int sc8989x_enable_powerpath(struct charger_device *chg_dev, bool en)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    dev_info(sc->dev, "%s en = %d\n", __func__, en);
    if( IS_ERR_OR_NULL(sc)) {
        return false;
    }

    return sc8989x_set_chg_enable(sc, en);
}

static int sc8989x_is_powerpath_enabled(struct charger_device *chg_dev, bool *en) {
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    if( IS_ERR_OR_NULL(sc)) {
        return false;
    }

    return sc8989x_check_chg_enabled(sc, en);
}

__maybe_unused
static int sc8989x_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return __sc8989x_get_adc(sc, SC8989X_ADC_VBUS, vbus);
}

__maybe_unused static int sc8989x_get_ibus(struct charger_device *chg_dev, u32 *ibus)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    u32 vbus = 0;

    if (IS_ERR_OR_NULL(sc)) {
        return -EINVAL;
    }

    if (sc->dev_id == BQ25891H_PN_NUM) {
        ret = __sc8989x_get_adc(sc, SC8989X_ADC_VBUS, &vbus);
        ret |= __sc8989x_get_adc(sc, SC8989X_ADC_ICC, ibus);
	if (vbus > 6500)
            *ibus = (*ibus) / 2;
        dev_info(sc->dev, "ibus of bq25891h is %d\n", *ibus);
        return ret;
    }

    return __sc8989x_get_adc(sc, SC8989X_ADC_IBUS, ibus);
}

__maybe_unused static int sc8989x_get_ibat(struct charger_device *chg_dev, u32 *ibat)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);

    return __sc8989x_get_adc(sc, SC8989X_ADC_ICC, ibat);
}

static int sc8989x_set_recharge(struct charger_device *chg_dev,bool en)
{
    struct sc8989x_chip *sc = dev_get_drvdata(&chg_dev->dev);
    sc8989x_field_write(sc, VRECHG, en);

    return 0;
}

static struct charger_ops sc8989x_chg_ops = {
    /* Normal charging */
    .plug_in = sc8989x_plug_in,
    .plug_out = sc8989x_plug_out,
    .enable = sc8989x_enable,
    .is_enabled = sc8989x_is_enabled,
    .get_charging_current = sc8989x_get_charging_current,
    .set_charging_current = sc8989x_set_charging_current,
    .get_input_current = sc8989x_get_input_current,
    .set_input_current = sc8989x_set_input_current,
    .get_constant_voltage = sc8989x_get_constant_voltage,
    .set_constant_voltage = sc8989x_set_constant_voltage,
    .kick_wdt = sc8989x_kick_wdt,
    .set_mivr = sc8989x_set_ivl,
    .get_mivr = sc8989x_get_ivl,
    .get_mivr_state = sc8989x_get_mivr_state,
    .is_charging_done = sc8989x_is_charging_done,
    .get_min_input_current = sc8989x_get_min_aicr,
    .get_min_charging_current = sc8989x_get_min_ichg,
    .dump_registers = sc8989x_dump_registers,

    /* Safety timer */
    .enable_safety_timer = sc8989x_set_safety_timer,
    .is_safety_timer_enabled = sc8989x_is_safety_timer_enabled,

    /* Power path */
    .enable_powerpath = sc8989x_enable_powerpath,
    .is_powerpath_enabled = sc8989x_is_powerpath_enabled,

    /* OTG */
    .enable_otg = sc8989x_set_otg,
    .set_boost_current_limit = sc8989x_set_boost_ilmt,
    .enable_discharge = NULL,

    /* PE+/PE+20 */
    .send_ta_current_pattern = sc8989x_send_ta_current_pattern,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = sc8989x_send_ta20_current_pattern,
    .reset_ta = sc8989x_set_ta20_reset,
    .enable_cable_drop_comp = NULL,

    /* ADC */
    .get_adc = sc8989x_get_adc,
    .get_vbus_adc = sc8989x_get_vbus,
    .get_ibus_adc = sc8989x_get_ibus,
    .get_ibat_adc = sc8989x_get_ibat,

    .event = sc8989x_do_event,
    .enable_hz = sc8989x_enable_hz,
    .set_recharge = sc8989x_set_recharge,
    .set_hvdcp_adapter_voltage_9v = sc8989x_set_qc_9V,
};

static const struct charger_properties sc8989x_chg_props = {
    .alias_name = "sc8989x_chg",
};

#ifdef CONFIG_MTK_CHARGER_V4P19
static void sc8989x_inform_psy_dwork_handler(struct work_struct *work) 
{
    int ret = 0;
    union power_supply_propval propval;
    struct sc8989x_chip *sc = container_of(work,
                                        struct sc8989x_chip,
                                        psy_dwork.work);
    if (!sc->chg_psy) {
        sc->chg_psy = power_supply_get_by_name("charger");
        if (!sc->chg_psy) {
            pr_err("%s get power supply fail\n", __func__);
            mod_delayed_work(system_wq, &sc->psy_dwork, 
                    msecs_to_jiffies(2000));
            return ;
        }
    }

    if (sc->adp_type != CHARGER_UNKNOWN)
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = power_supply_set_property(sc->chg_psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    propval.intval = sc->adp_type;

    ret = power_supply_set_property(sc->chg_psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

}
#endif /*CONFIG_MTK_CHARGER_V4P19*/

static void sc8989x_set_chg_type(struct sc8989x_chip *sc, int type)
{
#ifdef CONFIG_MTK_CHARGER_V4P19
    switch (type) {
        case VBUS_STAT_NO_INPUT:
            sc->adp_type = CHARGER_UNKNOWN;
            break;
        case VBUS_STAT_SDP:
        case VBUS_STAT_CDP:
        case VBUS_STAT_UNKOWN:
            sc->adp_type = STANDARD_HOST;
            Charger_Detect_Release(sc);
            break;
        case VBUS_STAT_DCP:
        case VBUS_STAT_HVDCP:
            sc->adp_type = STANDARD_CHARGER;
            break;
        case VBUS_STAT_NONSTAND:
            sc->adp_type = NONSTANDARD_CHARGER;
            break;
        default:
            sc->adp_type = CHARGER_UNKNOWN;
            break;
    }

    schedule_delayed_work(&sc->psy_dwork, 0);
#endif /*CONFIG_MTK_CHARGER_V4P19*/
}

#endif /*CONFIG_MTK_CLASS */

/* Begin for HTH-342463 c2c charging disconnect 20230926 */
static void sc8989x_chg_enable_dwork_handler(struct work_struct *work)
{
    struct sc8989x_chip *sc = container_of(
        work, struct sc8989x_chip, chg_enable_dwork.work);

    if (sc->vbus_good) {
        sc8989x_field_write(sc, CHG_CFG, 1);
        dev_info(sc->dev, "charge enable now");
    } else
        dev_info(sc->dev, "vbus not good, not enable charge");
}

static void sc8989x_icl_step_dwork_handler(struct work_struct *work)
{
    int iindpm_regval, iindpm_nowval;
    struct sc8989x_chip *sc = container_of(
        work, struct sc8989x_chip, icl_step_dwork.work);

    if (!sc->vbus_good || sc->psy_usb_type != POWER_SUPPLY_USB_TYPE_SDP) {
        dev_info(sc->dev, "vbus drop out");
        return;
    }

    sc8989x_field_read(sc, IINDPM, &iindpm_regval);
    iindpm_nowval = val2reg(SC8989X_IINDPM, sc->icl_goal);
    if (iindpm_nowval == iindpm_regval)
        dev_info(sc->dev, "icl reached %dmA", sc->icl_goal);
    else if (iindpm_nowval > iindpm_regval) {
        iindpm_nowval = iindpm_regval + 1;
        sc8989x_field_write(sc, IINDPM, iindpm_nowval);
        schedule_delayed_work(&sc->icl_step_dwork, msecs_to_jiffies(100));
        dev_info(sc->dev, "icl increase to %dmA", iindpm_nowval);
    } else {
        sc8989x_field_write(sc, IINDPM, iindpm_nowval);
        dev_info(sc->dev, "icl reduce to %dmA", iindpm_nowval);
    }
}
/* Begin for HTH-342463 c2c charging disconnect 20230926 */

static void lc_chg_ui_rush_dwork_handler(struct work_struct *work)
{
    int delta_time = 0;
    struct sc8989x_chip *sc = container_of(work,
                                        struct sc8989x_chip,
                                        chg_ui_rush_dwork.work);

    /* In case continue to quick plug */
    delta_time = ktime_ms_delta(ktime_get(), sc->usb_plug_in_time);
    if (delta_time > (DELAY_CHARGE_UI_MS - 100)) {
	if (sc->vbus_good && sc->chg_type == POWER_SUPPLY_TYPE_UNKNOWN) {
            sc->bc12_done_flag = 1;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            sc->chg_type = POWER_SUPPLY_TYPE_USB;
	    real_type = VBUS_STAT_UNKOWN;
	    dev_info(sc->dev,
                    "set fake charger type,real_type:%d, chg_type:%d\n",
                    real_type, sc->chg_type);
	}
	if(!IS_ERR_OR_NULL(p_usb_psy_desc))
            p_usb_psy_desc->type = sc->chg_type;
	power_supply_changed(sc->psy);
	dev_info(sc->dev, "%s charge ui rush start\n", __func__);
    }

    return;
}

/**********************interrupt*********************/
static void sc8989x_force_detection_dwork_handler(struct work_struct *work) {
    int ret;
    struct sc8989x_chip *sc = container_of(work,
                                        struct sc8989x_chip,
                                        force_detect_dwork.work);

    if (!sc->vbus_good) {
        dev_err(sc->dev, "%s: vbus not good\n", __func__);
        return;
    }

    ret = sc8989x_force_dpdm(sc);
    if (ret) {
        dev_err(sc->dev, "%s: force dpdm failed(%d)\n", __func__, ret);

        return;
    }

    sc->force_detect_count++;
    dev_err(sc->dev, "%s: force_detect_count: %d\n", __func__, sc->force_detect_count);

    return;
}

static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
    if (tcpm_inquire_typec_attach_state(tcpc) != TYPEC_ATTACHED_SNK)
        return -EINVAL;
    return 0;
}

static int sc8989x_get_charger_type(struct sc8989x_chip *sc) {
    int ret;
    int reg_val = 0;
    static int old_reg_val = VBUS_STAT_NO_INPUT;
    int temp;

    if (sc->pr_swap) {
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        return 0;
    }
    
    ret = sc8989x_field_read(sc, PG_STAT, &reg_val);
    /*if(!ret && reg_val == 0) {
        return ret;
    }*/

    ret = sc8989x_field_read(sc, VBUS_STAT, &reg_val);
    if (ret < 0) {
        return ret;
    }

    //Begin HTH-322252, reread the bus_stat to avoid reading the wrong VBUS_STAT
    if(old_reg_val != VBUS_STAT_NO_INPUT && reg_val != VBUS_STAT_NO_INPUT  && old_reg_val != reg_val) {
        msleep(50);
        temp = reg_val;
        ret = sc8989x_field_read(sc, VBUS_STAT, &reg_val);
        if (ret) {
            dev_err(sc->dev, "%s Read VBUS_STAT fail\n", __func__);
            reg_val = temp;
        }
        dev_info(sc->dev, "%s %d -> %d \n", __func__, temp, reg_val);
    }
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    if(reg_val != VBUS_STAT_NO_INPUT && reg_val != VBUS_STAT_OTG) {
        sc->bc12_done_flag = 1;
    }
    /* End for HTH-342463 c2c charging disconnect 20230926 */

    old_reg_val = reg_val;
    if (real_type == VBUS_STAT_PD && reg_val == VBUS_STAT_UNKOWN) {
        Charger_Detect_Release(sc);
        dev_err(sc->dev, "%s real_type is pd, not do bc12\n", __func__);
        return ret;
    }
    real_type = reg_val;
    dev_err(sc->dev, "%s:real_type=%d, reg_val=%d\n",__func__,real_type,reg_val);

    switch (reg_val) {
    case VBUS_STAT_NO_INPUT:
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
            sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        break;
    case VBUS_STAT_SDP:
        Charger_Detect_Release(sc);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        if(sc->force_detect_count == 1){
            sc->bc12_done_flag = 0;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
            sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
            schedule_delayed_work(&sc->force_detect_dwork,
                                msecs_to_jiffies(1000));
            cancel_delayed_work(&sc->chg_ui_rush_dwork);
        }
	break;
    case VBUS_STAT_CDP:
        Charger_Detect_Release(sc);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
        if(sc->force_detect_count == 1){
            sc->bc12_done_flag = 0;
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
            sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
            schedule_delayed_work(&sc->force_detect_dwork,
                                msecs_to_jiffies(1000));
            cancel_delayed_work(&sc->chg_ui_rush_dwork);
        }
        break;
    case VBUS_STAT_DCP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    case VBUS_STAT_HVDCP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_ACA;/* instead of HVDCP */
        sc->chg_type = POWER_SUPPLY_TYPE_USB_ACA;/*  instead of HVDCP */
        /*sc8989x_set_vindpm(sc, 8300);
        sc8989x_set_qc_9V(sc,true);*/
        break;
    case VBUS_STAT_UNKOWN:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        if (sc->force_detect_count < 16) {
            dev_err(sc->dev, "%s: force_detect_count: %d\n", __func__, 
                        sc->force_detect_count);
            schedule_delayed_work(&sc->force_detect_dwork,
                        msecs_to_jiffies(500));
        }
        Charger_Detect_Release(sc);
        break;
    case VBUS_STAT_NONSTAND:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        Charger_Detect_Release(sc);
        break;
    default:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        Charger_Detect_Release(sc);
        break;
    }

#ifdef CONFIG_MTK_CLASS
    sc8989x_set_chg_type(sc, reg_val);
#endif /*CONFIG_MTK_CLASS */

    dev_info(sc->dev, "%s vbus stat: 0x%02x, psy_usb_type: %d, chg_type: %d, real_type : %d\n",
            __func__, reg_val, sc->psy_usb_type, sc->chg_type, real_type);

    return ret;
}

static int adc_conversion_enable(struct sc8989x_chip *sc, int enable)
{
    int ret = 0;
    if (IS_ERR_OR_NULL(sc)) {
       goto err;
    }
    ret = sc8989x_field_write(sc, CONV_START, enable);
    if (ret < 0) {
        dev_err(sc->dev, "%s: set CONV_START fail\n", __func__);
        goto err;
    }
    ret = sc8989x_field_write(sc, CONV_RATE, enable);
    if (ret < 0) {
        dev_err(sc->dev, "%s: set CONV_START fail\n", __func__);
        goto err;
    }
    return ret;
err:
    return -EINVAL;
}

static void sc8989x_inserted_irq(struct sc8989x_chip *sc)
{
    int ret;
    dev_err(sc->dev, "%s: adapter/usb inserted\n", __func__);
    if (sc->pr_swap){
        dev_err(sc->dev, "%s: pr_swap is true, return !\n", __func__);
        return;
    }

    ret = sc8989x_chg_set_usbsw(sc, USBSW_CHG);
    if(ret < 0) {
            dev_err(sc->dev, "%s: failed to set bc12(%d)\n",__func__,ret);
          /*return;*/
    }

    if (sc->dev_id != BQ25891H_PN_NUM) {
        sc8989x_set_vindpm_track(sc, SC8989X_TRACK_300);
    }

    sc8989x_set_vindpm(sc, 4600);
    sc->force_detect_count = 0;
    sc8989x_field_write(sc, EN_ILIM, false);

    dev_err(sc->dev, "%s: enter force_detect_dwork, force_detect_count: %d\n",
                __func__,
                sc->force_detect_count);

    //reset dpdm before force_dp_dm
    ret = sc8989x_reset_dpdm(sc);
    if (ret) {
        dev_err(sc->dev, "%s: reset dpdm failed(%d)\n", __func__, ret);
    }

    schedule_delayed_work(&sc->force_detect_dwork,
                        msecs_to_jiffies(100));
}

static void sc8989x_removed_irq(struct sc8989x_chip *sc)
{
    int ret;
    dev_info(sc->dev, "%s: adapter/usb removed\n", __func__);
    if (sc->pr_swap)
        sc->pr_swap = false;

    dev_err(sc->dev, "%s: cancel_delayed_work: force_detect_dwork\n", __func__);
    cancel_delayed_work(&sc->force_detect_dwork);
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    sc->bc12_done_flag = 0;
    cancel_delayed_work(&sc->chg_enable_dwork);
    cancel_delayed_work(&sc->icl_step_dwork);
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */

    if (sc->dev_id != BQ25891H_PN_NUM) {
        sc8989x_set_iindpm(sc, 250);
    } else {
        sc8989x_set_iindpm(sc, 100);
    }

    sc8989x_set_dpdm_hiz(sc);
    ret = sc8989x_chg_set_usbsw(sc, USBSW_CHG);
    if(ret < 0) {
            dev_err(sc->dev, "%s: failed to set USB(%d)\n",__func__,ret);
    }

}

static irqreturn_t sc8989x_irq_handler(int irq, void *data)
{
    int ret;
    int reg_val;
    bool prev_vbus_gd;
    struct sc8989x_chip *sc = (struct sc8989x_chip *)data;

    dev_info(sc->dev, "%s: sc8989x_irq_handler\n", __func__);

    ret = sc8989x_field_read(sc, VBUS_GD, &reg_val);
    if (ret) {
        dev_err(sc->dev, "%s: read VBUS_GD fail\n", __func__);
        return IRQ_HANDLED;
    }
    prev_vbus_gd = sc->vbus_good;
    sc->vbus_good = ! !reg_val;

    if (!prev_vbus_gd && sc->vbus_good) {
        dev_err(sc->dev, "%s: enter sc8989x_inserted_irq\n", __func__);
        adc_conversion_enable(sc, true);
        sc8989x_inserted_irq(sc);
	sc->usb_plug_in_time = ktime_get();
	schedule_delayed_work(&sc->chg_ui_rush_dwork,
			msecs_to_jiffies(DELAY_CHARGE_UI_MS));
    } else if (prev_vbus_gd && !sc->vbus_good) {
        dev_err(sc->dev, "%s: enter sc8989x_removed_irq\n", __func__);
        sc8989x_removed_irq(sc);
        adc_conversion_enable(sc, false);
        msleep(10);
    }

    sc8989x_get_charger_type(sc);
    if(!IS_ERR_OR_NULL(p_usb_psy_desc))
        p_usb_psy_desc->type = sc->chg_type;
    power_supply_changed(sc->psy);

    sc8989x_dump_register(sc);
    return IRQ_HANDLED;
}

/**********************system*********************/
static int sc8989x_parse_dt(struct sc8989x_chip *sc)
{
    struct device_node *np = sc->dev->of_node;
    int i;
    int ret = 0;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,sc8989x,ico-en", &(sc->cfg->ico_en)},
        {"sc,sc8989x,hvdcp-en", &(sc->cfg->hvdcp_en)},
        {"sc,sc8989x,auto-dpdm-en", &(sc->cfg->auto_dpdm_en)},
        {"sc,sc8989x,vsys-min", &(sc->cfg->vsys_min)},
        {"sc,sc8989x,vbatmin-sel", &(sc->cfg->vbatmin_sel)},
        {"sc,sc8989x,itrick", &(sc->cfg->itrick)},
        {"sc,sc8989x,iterm", &(sc->cfg->iterm)},
        {"sc,sc8989x,vbat-cv", &(sc->cfg->vbat_cv)},
        {"sc,sc8989x,vbat-low", &(sc->cfg->vbat_low)},
        {"sc,sc8989x,vrechg", &(sc->cfg->vrechg)},
        {"sc,sc8989x,en-term", &(sc->cfg->en_term)},
        {"sc,sc8989x,stat-dis", &(sc->cfg->stat_dis)},
        {"sc,sc8989x,wd-time", &(sc->cfg->wd_time)},
        {"sc,sc8989x,en-timer", &(sc->cfg->en_timer)},
        {"sc,sc8989x,charge-timer", &(sc->cfg->charge_timer)},
        {"sc,sc8989x,bat-comp", &(sc->cfg->bat_comp)},
        {"sc,sc8989x,vclamp", &(sc->cfg->vclamp)},
        {"sc,sc8989x,votg", &(sc->cfg->votg)},
        {"sc,sc8989x,iboost", &(sc->cfg->iboost)},
        {"sc,sc8989x,vindpm", &(sc->cfg->vindpm)},};

    sc->irq_gpio = of_get_named_gpio(np, "sc,intr-gpio", 0);
    if (sc->irq_gpio < 0)
        dev_err(sc->dev, "%s sc,intr-gpio is not available\n", __func__);

    if (of_property_read_string(np, "charger_name", &sc->cfg->chg_name) < 0)
        dev_err(sc->dev, "%s no charger name\n", __func__);

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name, props[i].conv_data);
        if (ret < 0) {
            dev_err(sc->dev, "%s not find\n", props[i].name);
            continue;
        }
    }

    return 0;
}

static int sc8989x_init_device(struct sc8989x_chip *sc)
{
    int ret = 0;
    int i;
    int default_cfg = 0;

    struct {
        enum sc8989x_fields field_id;
        int conv_data;
    } props[] = {
        {ICO_EN, sc->cfg->ico_en},
        {HVDCP_EN, sc->cfg->hvdcp_en},
        {AUTO_DPDM_EN, sc->cfg->auto_dpdm_en},
        {VSYS_MIN, sc->cfg->vsys_min},
        {VBATMIN_SEL, sc->cfg->vbatmin_sel},
        {ITC, sc->cfg->itrick},
        {ITERM, sc->cfg->iterm},
        {CV, sc->cfg->vbat_cv},
        {VBAT_LOW, sc->cfg->vbat_low},
        {VRECHG, sc->cfg->vrechg},
        {EN_ITERM, sc->cfg->en_term},
        {STAT_DIS, sc->cfg->stat_dis},
        {TWD, sc->cfg->wd_time},
        {EN_TIMER, sc->cfg->en_timer},
        {TCHG, sc->cfg->charge_timer},
        {BAT_COMP, sc->cfg->bat_comp},
        {VCLAMP, sc->cfg->vclamp},
        {V_OTG, sc->cfg->votg},
        {IBOOST_LIM, sc->cfg->iboost},
        {VINDPM, sc->cfg->vindpm},};

    //reg reset;
    sc8989x_field_write(sc, REG_RST, 1);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        dev_info(sc->dev, "%d--->%d\n", props[i].field_id, props[i].conv_data);
        ret = sc8989x_field_write(sc, props[i].field_id, props[i].conv_data);
    }

    if (sc->dev_id == BQ25891H_PN_NUM) {
        //set term curr
        default_cfg = (sc->cfg->iterm * SC8989X_ITERM_STEP) + SC8989X_ITERM_OFFSET;
        sc8989x_set_term_curr(sc,default_cfg);

        //set trick curr
        default_cfg =  (sc->cfg->itrick * SC8989X_ITC_STEP) + SC8989X_ITC_OFFSET;
        sc8989x_set_trick_curr(sc,default_cfg);

        //set otg volt
        default_cfg = (sc->cfg->votg * SC8989X_VOTG_STEP) + SC8989X_VOTG_OFFSET;
        sc8989x_set_votg(sc,default_cfg);
    } else {
        sc8989x_adc_ibus_en(sc, true);
        sc8989x_set_wa(sc);
    }

    sc8989x_set_iindpm(sc, 500);
    sc8989x_set_ichg(sc, 500);
    if (sc->dev_id ==  SC89890H_PN_NUM) {
        sc8989x_set_iic_option(sc);
    }
    return sc8989x_dump_register(sc);
}

static int sc8989x_register_interrupt(struct sc8989x_chip *sc)
{
    int ret = 0;

    ret = devm_gpio_request(sc->dev, sc->irq_gpio, "chr-irq");
    if (ret < 0) {
        dev_err(sc->dev, "failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    ret = gpio_direction_input(sc->irq_gpio);
    if (ret < 0) {
        dev_err(sc->dev, "failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    sc->irq = gpio_to_irq(sc->irq_gpio);
    if (ret < 0) {
        dev_err(sc->dev, "failed gpio to irq GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
                                    sc8989x_irq_handler,
                                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                    "chr_stat", sc);
    if (ret < 0) {
        dev_err(sc->dev, "request thread irq failed:%d\n", ret);
        return ret;
    } else {
        dev_err(sc->dev, "request thread irq pass:%d  sc->irq =%d\n", ret, sc->irq);
    }

    enable_irq_wake(sc->irq);

    return 0;
}

static void determine_initial_status(struct sc8989x_chip *sc) {
    sc8989x_irq_handler(sc->irq, (void *)sc);
}

//reigster
static ssize_t sc8989x_show_registers(struct device *dev,
                                    struct device_attribute *attr, char *buf) 
{
    struct sc8989x_chip *sc = dev_get_drvdata(dev);
    uint8_t addr;
    unsigned int val;
    uint8_t tmpbuf[300];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8989x");
    for (addr = 0x1; addr <= 0x14; addr++) {
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

static ssize_t sc8989x_store_register(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    struct sc8989x_chip *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int val;
    unsigned int reg;
    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= 0x14)
        regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

    return count;
}

static DEVICE_ATTR(registers, 0660, sc8989x_show_registers,
                sc8989x_store_register);

static void sc8989x_create_device_node(struct device *dev) {
    device_create_file(dev, &dev_attr_registers);
}

static enum power_supply_property sc8989x_chg_psy_properties[] = {
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
};

static enum power_supply_usb_type sc8989x_chg_psy_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
    POWER_SUPPLY_USB_TYPE_ACA,
};

static int sc8989x_chg_property_is_writeable(struct power_supply *psy,
                                            enum power_supply_property psp) {
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_ONLINE:
    case POWER_SUPPLY_PROP_ENERGY_EMPTY:
        ret = 1;
        break;

    default:
        ret = 0;
    }

    return ret;
}

static int sc8989x_chg_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val) {
    struct sc8989x_chip *sc = power_supply_get_drvdata(psy);
    int ret = 0;
    int data = 0;

    if (!sc) {
        dev_err(sc->dev, "%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    switch (psp) {
    case POWER_SUPPLY_PROP_MANUFACTURER:
        if(sc->dev_id == BQ25891H_PN_NUM)
            val->strval = "Ti";
        else
            val->strval = "SouthChip";
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        val->intval = (sc->vbus_good && (!!sc->chg_type));
        break;
    case POWER_SUPPLY_PROP_STATUS:
        ret = sc8989x_get_charge_stat(sc);
        if (ret < 0)
            break;
        val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8989x_get_ichg(sc, &data);
        if (ret)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_get_vbat(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8989x_get_iindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_get_vindpm(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8989x_get_term_curr(sc, &data);
        if (ret < 0)
            break;
        val->intval = data * 1000;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        val->intval = sc->psy_usb_type;
        break;
    case POWER_SUPPLY_PROP_CURRENT_MAX:
        if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
            val->intval = 500000;
        else
            val->intval = 1500000;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        if (sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
            val->intval = 5000000;
        else
            val->intval = 12000000;
        break;
    case POWER_SUPPLY_PROP_TYPE:
	dev_err(sc->dev, "%s:line%d: chg_type = %d\n", __func__, __LINE__,sc->chg_type);
        val->intval = sc->chg_type;
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int sc8989x_chg_set_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    const union power_supply_propval *val)
{
    struct sc8989x_chip *sc = power_supply_get_drvdata(psy);
    int ret = 0;

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        // dev_info(sc->dev, "%s  %d\n", __func__, val->intval);
        // if (val->intval == 2) {
        //     schedule_delayed_work(&sc->force_detect_dwork,
        //                         msecs_to_jiffies(300));
        // } else if (val->intval == 4) {
        //     sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
        //     sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        // }
        if (val->intval == 7) {
            sc->pr_swap = true;
        } else {
            sc->pr_swap = false;
        }
        break;
    case POWER_SUPPLY_PROP_STATUS:
        ret = sc8989x_set_chg_enable(sc, ! !val->intval);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = sc8989x_set_ichg(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_set_vbat(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8989x_set_iindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_set_vindpm(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8989x_set_term_curr(sc, val->intval / 1000);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static char *sc8989x_psy_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static const struct power_supply_desc sc8989x_psy_desc = {
    .name = "primary_chg",
    .type = POWER_SUPPLY_TYPE_USB,
    .usb_types = sc8989x_chg_psy_usb_types,
    .num_usb_types = ARRAY_SIZE(sc8989x_chg_psy_usb_types),
    .properties = sc8989x_chg_psy_properties,
    .num_properties = ARRAY_SIZE(sc8989x_chg_psy_properties),
    .property_is_writeable = sc8989x_chg_property_is_writeable,
    .get_property = sc8989x_chg_get_property,
    .set_property = sc8989x_chg_set_property,
};

static int sc8989x_psy_register(struct sc8989x_chip *sc)
{
    struct power_supply_config cfg = {
        .drv_data = sc,
        .of_node = sc->dev->of_node,
        .supplied_to = sc8989x_psy_supplied_to,
        .num_supplicants = ARRAY_SIZE(sc8989x_psy_supplied_to),
    };

    memcpy(&sc->psy_desc, &sc8989x_psy_desc, sizeof(sc->psy_desc));
    sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc, &cfg);
    return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}

static struct of_device_id sc8989x_of_device_id[] = {
    {.compatible = "southchip,sc89890h",},
    {.compatible = "southchip,sc89890w",},
    {.compatible = "southchip,sc89895",},
    {.compatible = "southchip,sc8950",},
    {},
};

MODULE_DEVICE_TABLE(of, sc8989x_of_device_id);

static int sc8989x_charger_probe(struct i2c_client *client,
                                const struct i2c_device_id *id)
{
    struct sc8989x_chip *sc;
    int ret = 0;
    int i;

    sc = devm_kzalloc(&client->dev, sizeof(struct sc8989x_chip), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->dev = &client->dev;
    sc->client = client;
    dev_err(sc->dev, "sc8989x_charger_probe start\n");

    sc->regmap = devm_regmap_init_i2c(client, &sc8989x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev, "Failed to initialize regmap\n");
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(sc8989x_reg_fields); i++) {
        const struct reg_field *reg_fields = sc8989x_reg_fields;
        sc->rmap_fields[i] =
            devm_regmap_field_alloc(sc->dev, sc->regmap, reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(sc->dev, "cannot allocate regmap field\n");
            return PTR_ERR(sc->rmap_fields[i]);
        }
    }

    i2c_set_clientdata(client, sc);
    sc8989x_create_device_node(&(client->dev));

    INIT_DELAYED_WORK(&sc->force_detect_dwork,
                        sc8989x_force_detection_dwork_handler);
    INIT_DELAYED_WORK(&sc->chg_ui_rush_dwork,
			lc_chg_ui_rush_dwork_handler);
    /* Begin for HTH-342463 c2c charging disconnect 20230926 */
    INIT_DELAYED_WORK(&sc->chg_enable_dwork, sc8989x_chg_enable_dwork_handler);
    INIT_DELAYED_WORK(&sc->icl_step_dwork, sc8989x_icl_step_dwork_handler);
    /* End for HTH-342463 c2c charging disconnect 20230926 */
#ifdef CONFIG_MTK_CHARGER_V4P19
    INIT_DELAYED_WORK(&sc->psy_dwork,
                        sc8989x_inform_psy_dwork_handler);
#endif /*CONFIG_MTK_CHARGER_V4P19*/
    if (!sc8989x_detect_device(sc)) {
        ret = -ENODEV;
        goto err_nodev;
    }

    sc->cfg = &sc8989x_default_cfg;
    ret = sc8989x_parse_dt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "parse dt fail(%d)\n", ret);
        goto err_parse_dt;
    }
    mutex_init(&sc->force_detect_lock);
    mutex_init(&sc->i2c_rw_lock);
    ret = sc8989x_init_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "init device fail(%d)\n", ret);
        goto err_init_dev;
    }

    ret = sc8989x_psy_register(sc);
    if (ret) {
        dev_err(sc->dev, "%s psy register fail(%d)\n", __func__, ret);
        goto err_psy;
    }
    ret = sc8989x_register_interrupt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s register irq fail(%d)\n", __func__, ret);
        goto err_register_irq;
    }
#ifdef CONFIG_MTK_CLASS
    /* Register charger device */
    sc->chg_dev = charger_device_register(sc->cfg->chg_name,
                                        sc->dev, sc, &sc8989x_chg_ops,
                                        &sc->chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        dev_notice(sc->dev, "%s register chg dev fail(%d)\n", __func__, ret);
        goto err_register_chg_dev;
    }
#endif                          /*CONFIG_MTK_CLASS */
    device_init_wakeup(sc->dev, 1);
    
    determine_initial_status(sc);
    sc8989x_dump_register(sc);

    dev_err(sc->dev, "sc8989x probe successfully\n!");

    return 0;

err_register_irq:
#ifdef CONFIG_MTK_CLASS
err_register_chg_dev:
#endif                          /*CONFIG_MTK_CLASS */
err_psy:
err_init_dev:
err_parse_dt:
err_nodev:
    dev_err(sc->dev, "sc8989x probe failed!\n");
    devm_kfree(&client->dev, sc);
    return -ENODEV;
}

static int sc8989x_charger_remove(struct i2c_client *client)
{
    struct sc8989x_chip *sc = i2c_get_clientdata(client);

    if (sc) {
        dev_info(sc->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CLASS
        charger_device_unregister(sc->chg_dev);
#endif                          /*CONFIG_MTK_CLASS */
        power_supply_put(sc->psy);
    }
    return 0;
}

static int get_ship_mode()
{
    struct power_supply *psy = NULL;
    struct mtk_battery *gm = NULL;

    psy = power_supply_get_by_name("battery");
    if (IS_ERR_OR_NULL(psy)) {
        pr_err("Not found power supply for battery!\n");
        return -ENODEV;
    }

    gm = (struct mtk_battery *)power_supply_get_drvdata(psy);

    pr_err("ship_mode: %d\n",gm->ship_mode);
	return gm->ship_mode;
}

static void sc8989x_charger_shutdown(struct i2c_client *client)
{
	struct sc8989x_chip *sc = i2c_get_clientdata(client);
    int ret = 0;
    bool enable = false;

    if (IS_ERR_OR_NULL(sc)) {
        return;
    }

    cancel_delayed_work(&sc->force_detect_dwork);

    enable = get_ship_mode();
    if (enable) {
		ret = sc8989x_field_write(sc, BATFET_DLY, 1);
		ret = sc8989x_field_write(sc, BATFET_DIS, 1);
	}
    return;
}

#ifdef CONFIG_PM_SLEEP
static int sc8989x_suspend(struct device *dev)
{
    struct sc8989x_chip *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irq);
    disable_irq(sc->irq);

    return 0;
}

static int sc8989x_resume(struct device *dev)
{
    struct sc8989x_chip *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);
    enable_irq(sc->irq);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irq);

    return 0;
}

static const struct dev_pm_ops sc8989x_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(sc8989x_suspend, sc8989x_resume)
};
#endif                          /* CONFIG_PM_SLEEP */

static struct i2c_driver sc8989x_charger_driver = {
    .driver = {
            .name = "sc8989x",
            .owner = THIS_MODULE,
            .of_match_table = of_match_ptr(sc8989x_of_device_id),
#ifdef CONFIG_PM_SLEEP
            .pm = &sc8989x_pm_ops,
#endif
            },
    .probe = sc8989x_charger_probe,
    .remove = sc8989x_charger_remove,
    .shutdown = sc8989x_charger_shutdown,
};

module_i2c_driver(sc8989x_charger_driver);

MODULE_DESCRIPTION("SC SC8989X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
