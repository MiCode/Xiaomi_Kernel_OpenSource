// SPDX-License-Identifier: GPL-2.0
// BQ25980 Battery Charger Driver
// Copyright (C) 2020 Texas Instruments Incorporated - http://www.ti.com/

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/string.h>

#include "charger_class.h"
#include "mtk_charger.h"
#include "sc8561_reg.h"
#include "ln8410_reg.h"
#include "../../misc/hwid/hwid.h"

static int log_level = 1;

#define bq_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define bq_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define bq_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

enum bq_compatible_id {
    SC8561_MASTER,
	SC8561_SLAVE,
};

typedef enum  adc_ch {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VUSB,
	ADC_VWPC,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
}ADC_CH;

enum cp_type {
	UNKNOW,
	SC8561,
	LN8410,
};

static struct regmap_config sc8561_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFF,
};

struct sc8561_device {
	struct i2c_client *client;
	struct device *dev;
	struct charger_device *chg_dev;
	struct power_supply *cp_psy;
	struct power_supply_desc psy_desc;
	struct regmap *regmap;
	bool chip_ok;
	char log_tag[25];
	int work_mode;
	int chip_vendor;
	u8 adc_mode;
	unsigned int revision;
	unsigned int product_cfg;

	struct delayed_work irq_handle_work;
	int irq_gpio;
	int irq;
};

struct mtk_cp_sysfs_field_info {
	struct device_attribute attr;
	enum cp_property prop;
	int (*set)(struct sc8561_device *gm,
		struct mtk_cp_sysfs_field_info *attr, int val);
	int (*get)(struct sc8561_device *gm,
		struct mtk_cp_sysfs_field_info *attr, int *val);
};

/* SC8561_ADC */
int sc8561_enable_adc(struct sc8561_device *bq, bool enable)
{
	int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_ADC_ENABLE;
    else
        val = SC8561_ADC_DISABLE;
    val <<= SC8561_ADC_EN_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_15,
                SC8561_ADC_EN_MASK, val);
    return ret;
}

/* SC8561_CHG_EN */
int sc8561_enable_charge(struct sc8561_device *bq, bool enable)
{
	int ret = 0;
	u8 val;
    if (enable)
        val = SC8561_CHG_ENABLE;
    else
        val = SC8561_CHG_DISABLE;
    val <<= SC8561_CHG_EN_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0B,
                SC8561_CHG_EN_MASK, val);
    return ret;
}
int sc8561_check_charge_enabled(struct sc8561_device *bq, bool *enabled)
{
	int ret = 0;
	unsigned int val;
    ret = regmap_read(bq->regmap, SC8561_REG_0B, &val);
    if (!ret)
        *enabled = !!(val & SC8561_CHG_EN_MASK);
    return ret;
}
/* SC8561_BAT_OVP */
static int sc8561_enable_batovp(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_BAT_OVP_ENABLE;
    else
        val = SC8561_BAT_OVP_DISABLE;
    val <<= SC8561_BAT_OVP_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_01,
                SC8561_BAT_OVP_DIS_MASK, val);
    return ret;
}
static int sc8561_set_batovp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < SC8561_BAT_OVP_BASE)
        threshold = SC8561_BAT_OVP_BASE;
    val = (threshold - SC8561_BAT_OVP_BASE) / SC8561_BAT_OVP_LSB;
    val <<= SC8561_BAT_OVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_01,
                SC8561_BAT_OVP_MASK, val);
    return ret;
}
/* SC8561_BAT_OCP */
static int sc8561_enable_batocp(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_BAT_OCP_ENABLE;
    else
        val = SC8561_BAT_OCP_DISABLE;
    val <<= SC8561_BAT_OCP_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_02,
                SC8561_BAT_OCP_DIS_MASK, val);
    return ret;
}
static int sc8561_set_batocp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < SC8561_BAT_OCP_BASE)
        threshold = SC8561_BAT_OCP_BASE;
    val = (threshold - SC8561_BAT_OCP_BASE) / SC8561_BAT_OCP_LSB;
    val <<= SC8561_BAT_OCP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_02,
                SC8561_BAT_OCP_MASK, val);
    return ret;
}
/* SC8561_USB_OVP */
static int sc8561_set_usbovp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold == 6500)
        val = SC8561_USB_OVP_6PV5;
    else
        val = (threshold - SC8561_USB_OVP_BASE) / SC8561_USB_OVP_LSB;
    val <<= SC8561_USB_OVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_03,
                SC8561_USB_OVP_MASK, val);
    return ret;
}
/* SC8561_OVPGATE_ON_DG_SET */
static int sc8561_set_ovpgate_on_dg_set(struct sc8561_device *bq, int time)
{
    int ret = 0;
    u8 val;
	switch (time) {
		case 20:
			val = SC8561_OVPGATE_ON_DG_20MS;
			break;
		case 128:
			val = SC8561_OVPGATE_ON_DG_128MS;
			break;
		default:
			val = SC8561_OVPGATE_ON_DG_128MS;
			break;
	}

    val <<= SC8561_OVPGATE_ON_DG_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_03,
                SC8561_OVPGATE_ON_DG_MASK, val);
    return ret;
}
/* SC8561_WPC_OVP */
static int sc8561_set_wpcovp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold == 6500)
        val = SC8561_WPC_OVP_6PV5;
    else
        val = (threshold - SC8561_WPC_OVP_BASE) / SC8561_WPC_OVP_LSB;
    val <<= SC8561_WPC_OVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_04,
                SC8561_WPC_OVP_MASK, val);
    return ret;
}
/* SC8561_BUS_OVP */
static int sc8561_set_busovp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (bq->work_mode == SC8561_FORWARD_4_1_CHARGER_MODE
        || bq->work_mode == SC8561_REVERSE_1_4_CONVERTER_MODE) {
        if (threshold < 14000)
            threshold = 14000;
        else if (threshold > 22000)
            threshold = 22000;
        val = (threshold - SC8561_BUS_OVP_41MODE_BASE) / SC8561_BUS_OVP_41MODE_LSB;
    }
    else if (bq->work_mode == SC8561_FORWARD_2_1_CHARGER_MODE
        || bq->work_mode == SC8561_REVERSE_1_2_CONVERTER_MODE) {
        if (threshold < 7000)
            threshold = 7000;
        else if (threshold > 13300)
            threshold = 13300;
        val = (threshold - SC8561_BUS_OVP_21MODE_BASE) / SC8561_BUS_OVP_21MODE_LSB;
    }
    else {
        if (threshold < 3500)
            threshold = 3500;
        else if (threshold > 5500)
            threshold = 5500;
        val = (threshold - SC8561_BUS_OVP_11MODE_BASE) / SC8561_BUS_OVP_11MODE_LSB;
    }
	bq_info("sc8561_set_busovp_th= %d", threshold);
    val <<= SC8561_BUS_OVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_05,
                SC8561_BUS_OVP_MASK, val);
    return ret;
}
/* SC8561_OUT_OVP */
static int sc8561_set_outovp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < 4800)
        threshold = 4800;
    val = (threshold - SC8561_OUT_OVP_BASE) / SC8561_OUT_OVP_LSB;
    val <<= SC8561_OUT_OVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_05,
                SC8561_OUT_OVP_MASK, val);
    return ret;
}
/* SC8561_BUS_OCP */
static int sc8561_enable_busocp(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_BUS_OCP_ENABLE;
    else
        val = SC8561_BUS_OCP_DISABLE;
    val <<= SC8561_BUS_OCP_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_06,
                SC8561_BUS_OCP_DIS_MASK, val);
    return ret;
}
static int sc8561_set_busocp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < SC8561_BUS_OCP_BASE)
        threshold = SC8561_BUS_OCP_BASE;
    val = (threshold - SC8561_BUS_OCP_BASE) / SC8561_BUS_OCP_LSB;
    val <<= SC8561_BUS_OCP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_06,
                SC8561_BUS_OCP_MASK, val);
    return ret;
}
/* SC8561_BUS_UCP */
static int sc8561_enable_busucp(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_BUS_UCP_ENABLE;
    else
        val = SC8561_BUS_UCP_DISABLE;
    val <<= SC8561_BUS_UCP_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_07,
                SC8561_BUS_UCP_DIS_MASK, val);
    return ret;
}
/* SC8561_PMID2OUT_OVP */
static int sc8561_enable_pmid2outovp(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_PMID2OUT_OVP_ENABLE;
    else
        val = SC8561_PMID2OUT_OVP_DISABLE;
    val <<= SC8561_PMID2OUT_OVP_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_08,
                SC8561_PMID2OUT_OVP_DIS_MASK, val);
    return ret;
}
static int sc8561_set_pmid2outovp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < SC8561_PMID2OUT_OVP_BASE)
        threshold = SC8561_PMID2OUT_OVP_BASE;
    val = (threshold - SC8561_PMID2OUT_OVP_BASE) / SC8561_PMID2OUT_OVP_LSB;
    val <<= SC8561_PMID2OUT_OVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_08,
                SC8561_PMID2OUT_OVP_MASK, val);
    return ret;
}
/* SC8561_PMID2OUT_UVP */
static int sc8561_enable_pmid2outuvp(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_PMID2OUT_UVP_ENABLE;
    else
        val = SC8561_PMID2OUT_UVP_DISABLE;
    val <<= SC8561_PMID2OUT_UVP_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_09,
                SC8561_PMID2OUT_UVP_DIS_MASK, val);
    return ret;
}
static int sc8561_set_pmid2outuvp_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < SC8561_PMID2OUT_UVP_BASE)
        threshold = SC8561_PMID2OUT_UVP_BASE;
    val = (threshold - SC8561_PMID2OUT_UVP_BASE) / SC8561_PMID2OUT_UVP_LSB;
    val <<= SC8561_PMID2OUT_UVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_09,
                SC8561_PMID2OUT_UVP_MASK, val);
    return ret;
}
/* SC8561_BAT_OVP_ALM */
static int sc8561_enable_batovp_alarm(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_BAT_OVP_ALM_ENABLE;
    else
        val = SC8561_BAT_OVP_ALM_DISABLE;
    val <<= SC8561_BAT_OVP_ALM_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_6C,
                SC8561_BAT_OVP_ALM_DIS_MASK, val);
    return ret;
}
static int sc8561_set_batovp_alarm_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < SC8561_BAT_OVP_ALM_BASE)
        threshold = SC8561_BAT_OVP_ALM_BASE;
    val = (threshold - SC8561_BAT_OVP_ALM_BASE) / SC8561_BAT_OVP_ALM_LSB;
    val <<= SC8561_BAT_OVP_ALM_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_6C,
                SC8561_BAT_OVP_ALM_MASK, val);
    return ret;
}
/* SC8561_BUS_OCP_ALM */
static int sc8561_enable_busocp_alarm(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_BUS_OCP_ALM_ENABLE;
    else
        val = SC8561_BUS_OCP_ALM_DISABLE;
    val <<= SC8561_BUS_OCP_ALM_DIS_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_6D,
                SC8561_BUS_OCP_ALM_DIS_MASK, val);
    return ret;
}
#if 0
static int sc8561_set_busocp_alarm_th(struct sc8561_device *bq, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < SC8561_BUS_OCP_ALM_BASE)
        threshold = SC8561_BUS_OCP_ALM_BASE;
    val = (threshold - SC8561_BUS_OCP_ALM_BASE) / SC8561_BUS_OCP_ALM_LSB;
    val <<= SC8561_BUS_OCP_ALM_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_6D,
                SC8561_BUS_OCP_ALM_MASK, val);
    return ret;
}
#endif
static int sc8561_set_adc_scanrate(struct sc8561_device *bq, bool oneshot)
{
	int ret = 0;
    u8 val;
    if (oneshot)
        val = SC8561_ADC_RATE_ONESHOT;
    else
        val = SC8561_ADC_RATE_CONTINOUS;
    val <<= SC8561_ADC_RATE_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_15,
                SC8561_ADC_RATE_MASK, val);
    return ret;
}
#define SC8561_ADC_REG_BASE SC8561_REG_17
int sc8561_get_adc_data(struct sc8561_device *bq, int channel,  u32 *result)
{
    int ret = 0;
    unsigned int val_l, val_h;
    u16 val;
    if(channel >= ADC_MAX_NUM)
		return -1;
    ret = regmap_read(bq->regmap, SC8561_ADC_REG_BASE + (channel << 1), &val_h);
    ret = regmap_read(bq->regmap, SC8561_ADC_REG_BASE + (channel << 1) + 1, &val_l);
    if (ret < 0)
        return ret;
    val = (val_h << 8) | val_l;
    if(channel == ADC_IBUS)				val = val * SC8561_IBUS_ADC_LSB;
    else if(channel == ADC_VBUS)		val = val * SC8561_VBUS_ADC_LSB;
    else if(channel == ADC_VUSB)		val = val * SC8561_VUSB_ADC_LSB;
    else if(channel == ADC_VWPC)		val = val * SC8561_VWPC_ADC_LSB;
    else if(channel == ADC_VOUT)		val = val * SC8561_VOUT_ADC_LSB;
    else if(channel == ADC_VBAT)		val = val * SC8561_VBAT_ADC_LSB;
    else if(channel == ADC_IBAT)		val = val * SC8561_IBAT_ADC_LSB;
    else if(channel == ADC_TBAT)		val = val * SC8561_TSBAT_ADC_LSB;
    else if(channel == ADC_TDIE)		val = val * SC8561_TDIE_ADC_LSB;
    *result = val;
    return ret;
}
static int sc8561_set_adc_scan(struct sc8561_device *bq, int channel, bool enable)
{
	int ret = 0;
    u8 reg;
    u8 mask;
    u8 shift;
    u8 val;
    if (channel > ADC_MAX_NUM)
		return -1;
    if (channel == ADC_IBUS) {
        reg = SC8561_REG_15;
        shift = SC8561_IBUS_ADC_DIS_SHIFT;
        mask = SC8561_IBUS_ADC_DIS_MASK;
    } else {
        reg = SC8561_REG_16;
        shift = 8 - channel;
        mask = 1 << shift;
    }
    if (enable)
        val = 0 << shift;
    else
        val = 1 << shift;
    ret = regmap_update_bits(bq->regmap, reg, mask, val);
    return ret;
}
/* SC8561_SYNC_FUNCTION_EN */
int sc8561_enable_parallel_func(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_SYNC_FUNCTION_ENABLE;
    else
        val = SC8561_SYNC_FUNCTION_DISABLE;
    val <<= SC8561_SYNC_FUNCTION_EN_SHIFT;
	ret = regmap_update_bits(bq->regmap, SC8561_REG_0E,
				SC8561_SYNC_FUNCTION_EN_MASK, val);
    return ret;
}
/* SC8561_SYNC_MASTER_EN */
int sc8561_enable_config_func(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_SYNC_CONFIG_MASTER;
    else
        val = SC8561_SYNC_CONFIG_SLAVE;
    val <<= SC8561_SYNC_MASTER_EN_SHIFT;
	ret = regmap_update_bits(bq->regmap, SC8561_REG_0E,
				SC8561_SYNC_MASTER_EN_MASK, val);
    return ret;
}
/* SC8561_REG_RST */
int sc8561_set_reg_reset(struct sc8561_device *bq)
{
	int ret = 0;
	u8 val;
    val = SC8561_REG_RESET;
    val <<= SC8561_REG_RST_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0E,
                SC8561_REG_RST_MASK, val);
	return ret;
}
/* SC8561_MODE */
int sc8561_set_operation_mode(struct sc8561_device *bq, int operation_mode)
{
    int ret = 0;
    u8 val;
    switch (operation_mode) {
        case SC8561_FORWARD_4_1_CHARGER_MODE:
            val = SC8561_FORWARD_4_1_CHARGER_MODE;
        break;
        case SC8561_FORWARD_2_1_CHARGER_MODE:
            val = SC8561_FORWARD_2_1_CHARGER_MODE;
        break;
        case SC8561_FORWARD_1_1_CHARGER_MODE:
        case SC8561_FORWARD_1_1_CHARGER_MODE1:
            val = SC8561_FORWARD_1_1_CHARGER_MODE;
        break;
        case SC8561_REVERSE_1_4_CONVERTER_MODE:
            val = SC8561_REVERSE_1_4_CONVERTER_MODE;
        break;
        case SC8561_REVERSE_1_2_CONVERTER_MODE:
            val = SC8561_REVERSE_1_2_CONVERTER_MODE;
        break;
        case SC8561_REVERSE_1_1_CONVERTER_MODE:
        case SC8561_REVERSE_1_1_CONVERTER_MODE1:
            val = SC8561_REVERSE_1_1_CONVERTER_MODE;
        break;
        default:
            return -1;
        break;
    }
	bq_info("sc8561_set_operation_mode %d", operation_mode);
    bq->work_mode = val;
    val <<= SC8561_MODE_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0E,
                SC8561_MODE_MASK, val);
    return ret;
}
int sc8561_get_operation_mode(struct sc8561_device *bq, int *operation_mode)
{
	int ret = 0;
	unsigned int val;
	ret = regmap_read(bq->regmap, SC8561_REG_0E, &val);
	if (0 != ret) {
		bq_info("sc8561 get operation mode fail\n");
		return ret;
	}
	*operation_mode = (val & SC8561_MODE_MASK);
	return ret;
}
/* SC8561_ACDRV_MANUAL_EN */
static int sc8561_enable_acdrv_manual(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_ACDRV_MANUAL_MODE;
    else
        val = SC8561_ACDRV_AUTO_MODE;
    val <<= SC8561_ACDRV_MANUAL_EN_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0B,
                SC8561_ACDRV_MANUAL_EN_MASK, val);
    return ret;
}
/* SC8561_WPCGATE_EN */
int sc8561_enable_wpcgate(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_WPCGATE_ENABLE;
    else
        val = SC8561_WPCGATE_DISABLE;
    val <<= SC8561_WPCGATE_EN_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0B,
                SC8561_WPCGATE_EN_MASK, val);
    return ret;
}
/* SC8561_OVPGATE_EN */
int sc8561_enable_ovpgate(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_OVPGATE_ENABLE;
    else
        val = SC8561_OVPGATE_DISABLE;
    val <<= SC8561_OVPGATE_EN_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0B,
                SC8561_OVPGATE_EN_MASK, val);
    return ret;
}
/* SC8561_IBAT_SNS */
static int sc8561_set_sense_resistor(struct sc8561_device *bq, int r_mohm)
{
	int ret = 0;
	u8 val;
    if (r_mohm == 1)
        val = SC8561_IBAT_SNS_RES_1MHM;
    else if (r_mohm == 2)
        val = SC8561_IBAT_SNS_RES_2MHM;
    else
		return -1;
    val <<= SC8561_IBAT_SNS_RES_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0E,
                SC8561_IBAT_SNS_RES_MASK,
                val);
	return ret;
}
/* SC8561_SS_TIMEOUT */
static int sc8561_set_ss_timeout(struct sc8561_device *bq, int timeout)
{
	int ret = 0;
	u8 val;
	switch (timeout) {
		case 0:
			val = SC8561_SS_TIMEOUT_DISABLE;
			break;
		case 40:
			val = SC8561_SS_TIMEOUT_40MS;
			break;
		case 80:
			val = SC8561_SS_TIMEOUT_80MS;
			break;
		case 320:
			val = SC8561_SS_TIMEOUT_320MS;
			break;
		case 1280:
			val = SC8561_SS_TIMEOUT_1280MS;
			break;
		case 5120:
			val = SC8561_SS_TIMEOUT_5120MS;
			break;
		case 20480:
			val = SC8561_SS_TIMEOUT_20480MS;
			break;
		case 81920:
			val = SC8561_SS_TIMEOUT_81920MS;
			break;
		default:
			val = SC8561_SS_TIMEOUT_DISABLE;
			break;
	}
	val <<= SC8561_SS_TIMEOUT_SET_SHIFT;
	ret = regmap_update_bits(bq->regmap, SC8561_REG_0D,
			SC8561_SS_TIMEOUT_SET_MASK,
			val);
	return ret;
}
/* SC8561_WD_TIMEOUT_SET */
#if 0
static int sc8561_set_wdt(struct sc8561_device *bq, int ms)
{
	int ret = 0;
	u8 val;
    switch (ms) {
    case 0:
        val = SC8561_WD_TIMEOUT_DISABLE;
    case 200:
        val = SC8561_WD_TIMEOUT_0P2S;
        break;
    case 500:
        val = SC8561_WD_TIMEOUT_0P5S;
        break;
    case 1000:
        val = SC8561_WD_TIMEOUT_1S;
        break;
    case 5000:
        val = SC8561_WD_TIMEOUT_5S;
        break;
    case 30000:
        val = SC8561_WD_TIMEOUT_30S;
        break;
    case 100000:
        val = SC8561_WD_TIMEOUT_100S;
        break;
    case 255000:
        val = SC8561_WD_TIMEOUT_255S;
        break;
    default:
        val = SC8561_WD_TIMEOUT_DISABLE;
        break;
    }
    val <<= SC8561_WD_TIMEOUT_SET_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0D,
                SC8561_WD_TIMEOUT_SET_MASK, val);
    return ret;
}
#endif
/* OTHER */
static int sc8561_set_batovp_alarm_int_mask(struct sc8561_device *bq, u8 mask)
{
    int ret = 0;
    unsigned int val;
    ret = regmap_read(bq->regmap, SC8561_REG_6C, &val);
    if (ret)
        return ret;
    val |= (mask << SC8561_BAT_OVP_ALM_MASK_SHIFT);
    ret = regmap_write(bq->regmap, SC8561_REG_6C, val);
    return ret;
}
static int sc8561_set_busocp_alarm_int_mask(struct sc8561_device *bq, u8 mask)
{
    int ret = 0;
    unsigned int val;
    ret = regmap_read(bq->regmap, SC8561_REG_6D, &val);
    if (ret)
        return ret;
    val |= (mask << SC8561_BUS_OCP_ALM_MASK_SHIFT);
    ret = regmap_write(bq->regmap, SC8561_REG_6D, val);
    return ret;
}
static int sc8561_set_ucp_fall_dg(struct sc8561_device *bq, u8 date)
{
    int ret = 0;
    u8 val;
	val = date;
    val <<= SC8561_BUS_UCP_FALL_DG_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_07,
                SC8561_BUS_UCP_FALL_DG_MASK, val);
    return ret;
}
static int sc8561_set_enable_tsbat(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_TSBAT_ENABLE;
    else
        val = SC8561_TSBAT_DISABLE;
    val <<= SC8561_TSBAT_EN_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_70,
                SC8561_TSBAT_EN_MASK, val);
    return ret;
}
static int sc8561_set_sync(struct sc8561_device *bq, u8 date)
{
    int ret = 0;
    u8 val;
	val = date;
    val <<= SC8561_SYNC_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0C,
                SC8561_SYNC_MASK, val);
    return ret;
}
static int sc8561_set_acdrv_up(struct sc8561_device *bq, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = SC8561_ACDRV_UP_ENABLE;
    else
        val = SC8561_ACDRV_UP_DISABLE;
    val <<= SC8561_ACDRV_UP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_7C,
                SC8561_ACDRV_UP_MASK, val);
    return ret;
}

int sc8561_dump_important_regs(struct sc8561_device *bq)
{
	int ret = 0;
	unsigned int val;
	ret = regmap_read(bq->regmap, SC8561_REG_13, &val);
	if (!ret)
		bq_info("%s, dump converter SC state Reg [%02X] = 0x%02X",
				bq->log_tag, SC8561_REG_13, val);
	ret = regmap_read(bq->regmap, SC8561_REG_14, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_14, val);
	ret = regmap_read(bq->regmap, SC8561_REG_15, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_15, val);
	ret = regmap_read(bq->regmap, SC8561_REG_01, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_01, val);
	ret = regmap_read(bq->regmap, SC8561_REG_02, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_02, val);
	ret = regmap_read(bq->regmap, SC8561_REG_03, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_03, val);
	ret = regmap_read(bq->regmap, SC8561_REG_04, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_04, val);
	ret = regmap_read(bq->regmap, SC8561_REG_05, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_05, val);
	ret = regmap_read(bq->regmap, SC8561_REG_06, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_06, val);
	ret = regmap_read(bq->regmap, SC8561_REG_07, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_07, val);
	ret = regmap_read(bq->regmap, SC8561_REG_08, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_08, val);
	ret = regmap_read(bq->regmap, SC8561_REG_09, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_09, val);
	ret = regmap_read(bq->regmap, SC8561_REG_0A, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_0A, val);
	ret = regmap_read(bq->regmap, SC8561_REG_0B, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_0B, val);
	ret = regmap_read(bq->regmap, SC8561_REG_0C, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_0C, val);
	ret = regmap_read(bq->regmap, SC8561_REG_0D, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_0D, val);
	ret = regmap_read(bq->regmap, SC8561_REG_0E, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_0E, val);
	ret = regmap_read(bq->regmap, SC8561_REG_0F, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_0F, val);
	ret = regmap_read(bq->regmap, SC8561_REG_10, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_10, val);
	ret = regmap_read(bq->regmap, SC8561_REG_11, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_11, val);
	ret = regmap_read(bq->regmap, SC8561_REG_13, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_13, val);
	ret = regmap_read(bq->regmap, SC8561_REG_6E, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_6E, val);
	ret = regmap_read(bq->regmap, SC8561_REG_70, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_70, val);
	ret = regmap_read(bq->regmap, SC8561_REG_7C, &val);
	if (!ret)
		bq_info("dump converter SC state Reg [%02X] = 0x%02X",
				SC8561_REG_7C, val);
    return ret;
}

static int sc8561_init_int_src(struct sc8561_device *bq)
{
    int ret = 0;
    /*TODO:be careful ts bus and ts bat alarm bit mask is in
        *	fault mask register, so you need call
        *	sc8561_set_fault_int_mask for tsbus and tsbat alarm
        */
    ret = sc8561_set_batovp_alarm_int_mask(bq, SC8561_BAT_OVP_ALM_NOT_MASK);
    if (ret) {
        bq_info("failed to set alarm mask:%d\n", ret);
        return ret;
    }
    ret = sc8561_set_busocp_alarm_int_mask(bq, SC8561_BUS_OCP_ALM_NOT_MASK);
    if (ret) {
        bq_info("failed to set alarm mask:%d\n", ret);
        return ret;
    }
    return ret;
}

static int sc8561_init_protection(struct sc8561_device *cp, int forward_work_mode)
{
	int ret = 0;
	ret = sc8561_enable_batovp(cp, true);
	ret = sc8561_enable_batocp(cp, false);
	ret = sc8561_enable_busocp(cp, true);
	ret = sc8561_enable_busucp(cp, true);
	ret = sc8561_enable_pmid2outovp(cp, true);
	ret = sc8561_enable_pmid2outuvp(cp, true);
	ret = sc8561_enable_batovp_alarm(cp, true);
	ret = sc8561_enable_busocp_alarm(cp, true);
	ret = sc8561_set_batovp_th(cp, 4650);
	ret = sc8561_set_batocp_th(cp, 8000);
	ret = sc8561_set_batovp_alarm_th(cp, 4600);
	if (forward_work_mode == CP_FORWARD_4_TO_1) {
		ret = sc8561_set_busovp_th(cp, 22000);
		ret = sc8561_set_busocp_th(cp, 3750);
		ret = sc8561_set_usbovp_th(cp, 22000);
	} else if (forward_work_mode == CP_FORWARD_2_TO_1) {
		ret = sc8561_set_busovp_th(cp, 11000);
		ret = sc8561_set_busocp_th(cp, 3750);
		ret = sc8561_set_usbovp_th(cp, 14000);
	} else {
		//To do later for forward 1:1 mode
		;
	}
	//ret = sc8561_set_busovp_th(cp, cp->bus_ovp_threshold);
	//ret = sc8561_set_busocp_th(cp, cp->bus_ocp_threshold);
	//ret = sc8561_set_usbovp_th(cp, 22000);
	ret = sc8561_set_wpcovp_th(cp, 22000);
	ret = sc8561_set_outovp_th(cp, 5000);
	ret = sc8561_set_pmid2outuvp_th(cp, 100);
	ret = sc8561_set_pmid2outovp_th(cp, 600);
	return ret;
}

static int sc8561_init_adc(struct sc8561_device *cp)
{
    sc8561_set_adc_scanrate(cp, false);
    sc8561_set_adc_scan(cp, ADC_IBUS, true);
    sc8561_set_adc_scan(cp, ADC_VBUS, true);
    sc8561_set_adc_scan(cp, ADC_VUSB, true);
    sc8561_set_adc_scan(cp, ADC_VWPC, true);
    sc8561_set_adc_scan(cp, ADC_VOUT, true);
    sc8561_set_adc_scan(cp, ADC_VBAT, true);
    sc8561_set_adc_scan(cp, ADC_IBAT, true);
    sc8561_set_adc_scan(cp, ADC_TBAT, true);
    sc8561_set_adc_scan(cp, ADC_TDIE, true);
    sc8561_enable_adc(cp, false);
	return 0;
}
static int sc8561_init_device(struct sc8561_device *cp)
{
	int ret = 0;
//	ret = sc8561_set_wdt(cp, 0);
	ret = sc8561_enable_acdrv_manual(cp, true);//ac drive manual mode
	ret = sc8561_enable_wpcgate(cp, true);
	ret = sc8561_enable_ovpgate(cp, true);
	ret = sc8561_set_ss_timeout(cp, 5120);
	ret = sc8561_set_ucp_fall_dg(cp, SC8561_BUS_UCP_FALL_DG_5MS);
	ret = sc8561_set_enable_tsbat(cp, false);
	ret = sc8561_set_sync(cp, SC8561_SYNC_NO_SHIFT);
	ret = sc8561_set_acdrv_up(cp, true);
	ret = sc8561_set_sense_resistor(cp, 1);
	ret = sc8561_set_ovpgate_on_dg_set(cp, 20);
	ret = sc8561_init_protection(cp, CP_FORWARD_4_TO_1);
	ret = sc8561_init_adc(cp);
	ret = sc8561_init_int_src(cp);
	ret = sc8561_set_operation_mode(cp, SC8561_FORWARD_4_1_CHARGER_MODE);
	ret = sc8561_set_busovp_th(cp, 22000);
	return ret;
}

#define REG_NUM 78
int ln8410_dump_important_regs(struct sc8561_device *cp)
{
	int ret = 0;
	int i = 0;
	unsigned int val;
	unsigned int reg[REG_NUM] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
					0x11, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
					0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x29, 0x2D, 0x2E,
					0x30, 0x31,
					0x52, 0x54, 0x59,
					0x60, 0x61, 0x62, 0x63, 0x69,
					0x76, 0x79, 0x7B, 0x7E,
					0x80, 0x8D,
					0x98, 0x99, 0x9A, 0x9B, 0x9C,
					0xBC, 0xBD, 0xBE,
					0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE};

	for (i = 0; i < REG_NUM; i++)
	{
		ret = regmap_read(cp->regmap, reg[i], &val);
		if (!ret)
			bq_err("%s LN state Reg [%02X] = 0x%02X\n", cp->log_tag, reg[i], val);
		else
			bq_err("%s LN state Reg [%02X] read failed ret= %d\n", cp->log_tag, reg[i], ret);
	}

	return ret;
}

int ln8410_set_REGN_pull_down(struct sc8561_device *cp, bool enable)
{
	int ret = 0;
	ret = regmap_write(cp->regmap, LN8410_REG_LION_CTRL, 0xAA);
	if (enable) {
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x80, 0x80);   /* FORCE_VWPC_PD = 1 */
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x40, 0x00);   /* TM_VWPC_PD = 0 */
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_FORCE_SC_MISC, 0x04, 0x04);      /* TM_REGN_PD = 1 */
	} else {
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x80, 0x00);   /* FORCE_VWPC_PD = 0 */
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x40, 0x00);   /* TM_VWPC_PD = 0 */
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_FORCE_SC_MISC, 0x04, 0x00);      /* TM_REGN_PD = 0 */
	}
	ret = regmap_write(cp->regmap, LN8410_REG_LION_CTRL, 0x00);
	bq_err("%s REGN_PD=%d\n", cp->log_tag, enable);

	return 0;
}

static int ln8410_set_adc_mode(struct sc8561_device *cp, u8 mode)
{
	int ret = 0;

    if (mode == 0x00) {
		/* adc one-shot mode disable all channel */ 
		/* the channel will be enable when request read adc */
		ret = regmap_write(cp->regmap, LN8410_REG_14, 0x49);
		ret = regmap_write(cp->regmap, LN8410_REG_ADC_FN_DISABLE1, 0xFF);
		ret = regmap_update_bits(cp->regmap, LN8410_REG_ADC_CTRL2, 0xC0, 0xC0);/* normal mode */
		ret = regmap_update_bits(cp->regmap, LN8410_REG_LION_CFG_1, 0x11, 0x10);/* ADC_SKIP_IDLE=1, OSR=64 */
		bq_err("%s config : ADC ONESHOT mode\n", cp->log_tag);
		cp->adc_mode = mode;
    } else if ((mode == 0x01) && (cp->adc_mode != 0x01)) {
        /* adc continuous mode enable all(selected) channel */
		ret = regmap_write(cp->regmap, LN8410_REG_14, 0x88);
		ret = regmap_write(cp->regmap, LN8410_REG_ADC_FN_DISABLE1, 0x02);/* disable TSBAT */
		ret = regmap_update_bits(cp->regmap, LN8410_REG_ADC_CTRL2, 0xC0, 0xC0);/* normal mode */
		ret = regmap_update_bits(cp->regmap, LN8410_REG_LION_CFG_1, 0x11, 0x01);/* ADC_SKIP_IDLE=0, OSR=128 */
		bq_err("%s config : ADC CONTINUOUS mode\n", cp->log_tag);
		cp->adc_mode = mode;
		msleep(500); // 500MS
    } else {
		bq_err("%s config : ADC invalid mode\n", cp->log_tag);
    }

	bq_err("%s setup adc_mode=%d\n", cp->log_tag, cp->adc_mode);

	return ret;
}

/* LN8410_QB_EN */
int ln8410_enable_qb(struct sc8561_device *cp, bool enable)
{
	int ret = 0;
	u8 val;
    if (enable)
        val = LN8410_QB_ENABLE;
    else
        val = LN8410_QB_DISABLE;
    val <<= LN8410_QB_EN_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0A,
                LN8410_QB_EN_MASK, val);
    return ret;
}

/* LN8410_CHG_EN */
int ln8410_enable_charge(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;

    val = LN8410_CHG_DISABLE;
    val <<= LN8410_CHG_EN_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0A,
			    LN8410_CHG_EN_MASK, val);

	bq_err("%s LN enable_charge = %d\n", cp->log_tag, enable);
    if (enable){
		ret = ln8410_enable_qb(cp, enable);
		msleep(20);
		val = LN8410_CHG_ENABLE;
		val <<= LN8410_CHG_EN_SHIFT;
		ret = regmap_update_bits(cp->regmap, LN8410_REG_0A,
				    LN8410_CHG_EN_MASK, val);

		/*Don't move it up, continue mode setting is best after charging enabled*/
		ret = ln8410_set_adc_mode(cp, 0x01);
	    }
    else{
		ret = ln8410_enable_qb(cp, enable);
		/*Don't move it down, oneshot mode setting is best before charging disabled*/
		ret = ln8410_set_adc_mode(cp, 0x01);
    }

    return ret;
}
int ln8410_check_charge_enabled(struct sc8561_device *cp, bool *enabled)
{
	int ret = 0;
	unsigned int val, reg;

	ret = regmap_read(cp->regmap, LN8410_REG_98, &val);
	bq_err("%s LN check_charge_enabled val = 0x%02X. ret = %d\n", cp->log_tag, val, ret);
	if (!ret) {
		*enabled = !(val & (LN8410_SHUTDOWN_STS_MASK | LN8410_STANDBY_STS_MASK));
		if (0 == (*enabled)) {
			for (reg = 0x98; reg <= 0x9c; reg++)
			{
				ret = regmap_read(cp->regmap, reg, &val);
				if (!ret)
					bq_err("%s LN state Reg [%02X] = 0x%02X\n", cp->log_tag, reg, val);
				else
					bq_err("%s LN check Reg [%02X] failed ret = %d\n", cp->log_tag, reg, ret);
			}
		}
	}

    return ret;
}

/* LN8410_BAT_OVP */
static int ln8410_enable_batovp(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = LN8410_BAT_OVP_ENABLE;
    else
        val = LN8410_BAT_OVP_DISABLE;
    val <<= LN8410_BAT_OVP_DIS_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_01,
                LN8410_BAT_OVP_DIS_MASK, val);
    return ret;
}
static int ln8410_set_batovp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < LN8410_BAT_OVP_BASE)
        threshold = LN8410_BAT_OVP_BASE;
    val = (threshold - LN8410_BAT_OVP_BASE) / LN8410_BAT_OVP_LSB;
    val <<= LN8410_BAT_OVP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_01,
                LN8410_BAT_OVP_MASK, val);
    return ret;
}
/* LN8410_BAT_OCP */
static int ln8410_enable_batocp(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = LN8410_BAT_OCP_ENABLE;
    else
        val = LN8410_BAT_OCP_DISABLE;
    val <<= LN8410_BAT_OCP_DIS_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_02,
                LN8410_BAT_OCP_DIS_MASK, val);
    return ret;
}
static int ln8410_set_batocp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < LN8410_BAT_OCP_BASE)
        threshold = LN8410_BAT_OCP_BASE;
    val = (threshold - LN8410_BAT_OCP_BASE) / LN8410_BAT_OCP_LSB;
    val <<= LN8410_BAT_OCP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_02,
                LN8410_BAT_OCP_MASK, val);
    return ret;
}
/* LN8410_USB_OVP */
static int ln8410_set_usbovp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold == 6500)
        val = LN8410_USB_OVP_6PV5;
    else
        val = (threshold - LN8410_USB_OVP_BASE) / LN8410_USB_OVP_LSB;
    val <<= LN8410_USB_OVP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_03,
                LN8410_USB_OVP_MASK, val);
    return ret;
}
/* LN8410_WPC_OVP */
static int ln8410_set_wpcovp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold == 6500)
        val = LN8410_WPC_OVP_6PV5;
    else
        val = (threshold - LN8410_WPC_OVP_BASE) / LN8410_WPC_OVP_LSB;
    val <<= LN8410_WPC_OVP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_04,
                LN8410_WPC_OVP_MASK, val);
    return ret;
}
/* LN8410_BUS_OVP */
static int ln8410_set_busovp_th(struct sc8561_device *cp, bool threshold)
{
    int ret = 0;
    u8 val;
    if (threshold)
        val = LN8410_VBUS_OVP_HIGH_SET;
    else
        val = LN8410_VBUS_OVP_LOW_SET;
    val <<= LN8410_VBUS_OVP_SET_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0D,
                LN8410_VBUS_OVP_SET_MASK, val);
    return ret;
}
/* LN8410_OUT_OVP */
static int ln8410_set_outovp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < LN8410_OUT_OVP_BASE)
        threshold = LN8410_OUT_OVP_BASE;
    val = (threshold - LN8410_OUT_OVP_BASE) / LN8410_OUT_OVP_LSB;
    val <<= LN8410_OUT_OVP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_79,
                LN8410_OUT_OVP_MASK, val);
    return ret;
}
/* LN8410_BUS_OCP */
static int ln8410_enable_busocp(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = LN8410_BUS_OCP_ENABLE;
    else
        val = LN8410_BUS_OCP_DISABLE;
    val <<= LN8410_BUS_OCP_DIS_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_05,
                LN8410_BUS_OCP_DIS_MASK, val);
    return ret;
}
static int ln8410_set_busocp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;

    if (cp->revision == 0x2 && cp->product_cfg == 0x2)
        threshold = threshold * 3 / 2;

    if (threshold < LN8410_BUS_OCP_BASE)
        threshold = LN8410_BUS_OCP_BASE;
    val = (threshold - LN8410_BUS_OCP_BASE) / LN8410_BUS_OCP_LSB;
    val <<= LN8410_BUS_OCP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_05,
                LN8410_BUS_OCP_MASK, val);
    return ret;
}
/* LN8410_BUS_UCP */
static int ln8410_enable_busucp(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = LN8410_BUS_UCP_ENABLE;
    else
        val = LN8410_BUS_UCP_DISABLE;
    val <<= LN8410_BUS_UCP_DIS_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_06,
                LN8410_BUS_UCP_DIS_MASK, val);
    return ret;
}
static int ln8410_set_busucp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < LN8410_IBUS_UC_BASE)
        threshold = LN8410_IBUS_UC_BASE;
    val = (threshold - LN8410_IBUS_UC_BASE) / LN8410_IBUS_UC_LSB;
    val <<= LN8410_IBUS_UC_CFG_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_7C,
                LN8410_IBUS_UC_CFG_MASK, val);
    return ret;
}
/* LN8410_PMID2OUT_OVP */
static int ln8410_enable_pmid2outovp(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = LN8410_PMID2OUT_OVP_ENABLE;
    else
        val = LN8410_PMID2OUT_OVP_DISABLE;
    val <<= LN8410_PMID2OUT_OVP_DIS_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_07,
                LN8410_PMID2OUT_OVP_DIS_MASK, val);
    return ret;
}
static int ln8410_set_pmid2outovp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < LN8410_PMID2OUT_OVP_BASE)
        threshold = LN8410_PMID2OUT_OVP_BASE;
    val = (threshold - LN8410_PMID2OUT_OVP_BASE) / LN8410_PMID2OUT_OVP_LSB;
    val <<= LN8410_PMID2OUT_OVP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_07,
                LN8410_PMID2OUT_OVP_MASK, val);
    return ret;
}
/* LN8410_PMID2OUT_UVP */
static int ln8410_enable_pmid2outuvp(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;
    if (enable)
        val = LN8410_PMID2OUT_UVP_ENABLE;
    else
        val = LN8410_PMID2OUT_UVP_DISABLE;
    val <<= LN8410_PMID2OUT_UVP_DIS_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_08,
                LN8410_PMID2OUT_UVP_DIS_MASK, val);
    return ret;
}
static int ln8410_set_pmid2outuvp_th(struct sc8561_device *cp, int threshold)
{
    int ret = 0;
    u8 val;
    if (threshold < LN8410_PMID2OUT_UVP_BASE)
        threshold = LN8410_PMID2OUT_UVP_BASE;
    val = (threshold - LN8410_PMID2OUT_UVP_BASE) / LN8410_PMID2OUT_UVP_LSB;
    val <<= LN8410_PMID2OUT_UVP_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_08,
                LN8410_PMID2OUT_UVP_MASK, val);
    return ret;
}

/* LN8410_ADC */
int ln8410_enable_adc(struct sc8561_device *cp, bool enable)
{
	int ret = 0;
    u8 val;

    if (enable)
        val = LN8410_ADC_ENABLE;
    else
        val = LN8410_ADC_DISABLE;
    val <<= LN8410_ADC_EN_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_14,
                LN8410_ADC_EN_MASK, val);
    return ret;
}

#define SC_ADC_REG_BASE LN8410_REG_16
int ln8410_get_adc_data(struct sc8561_device *cp, int channel,  u32 *result)
{
    int ret = 0;
    u8 read_cnt;
    unsigned int val = 0;
    unsigned int old_val_l, val_l, val_h;
    unsigned int reg14_val = 0, reg15_val = 0, reg31_val = 0;

    if(channel >= ADC_MAX_NUM)
		return -1;

    if (0) {
	    /* enable selected channel */
	    if (channel == ADC_IBUS) {
		    regmap_update_bits(cp->regmap, LN8410_REG_ADC_CTRL, LN8410_IBUS_ADC_DIS_MASK, 0);
	    } else {
		    regmap_update_bits(cp->regmap, LN8410_REG_ADC_FN_DISABLE1, (1 << (8-channel)), 0);
	    }
	    regmap_update_bits(cp->regmap, LN8410_REG_ADC_CTRL, LN8410_ADC_EN_MASK, LN8410_ADC_EN_MASK);

	    ret = regmap_read(cp->regmap, LN8410_REG_ADC_CTRL, &reg14_val);
	    ret = regmap_read(cp->regmap, LN8410_REG_ADC_FN_DISABLE1, &reg15_val);
	    ret = regmap_read(cp->regmap, LN8410_REG_LION_CFG_1, &reg31_val);

	    bq_info("%s ADC_CTRL=0x%x, ADC_FN_DISABLE1=0x%x, LION_CFG_1=0x%x; start one-short mode\n", cp->log_tag, reg14_val, reg15_val, reg31_val);

	    /* waiting update adc value (max 100ms) */
	    ret = regmap_read(cp->regmap, SC_ADC_REG_BASE + (channel << 1)+1, &old_val_l);
	    for (read_cnt=0; read_cnt < 20; ++ read_cnt) {
		    msleep(5); // 5 MS
		    /* compare ADC0 value */
		    ret = regmap_read(cp->regmap, SC_ADC_REG_BASE + (channel << 1) + 1, &val_l);
		    bq_err("%s old_val_l=0X%x, val_l=0X%x\n", cp->log_tag, old_val_l, val_l);
		    if (val_l != old_val_l) {
			    break;
		    }
	    }

	    /* read ADC data */
	    ret = regmap_read(cp->regmap, SC_ADC_REG_BASE + (channel << 1), &val_h);
	    ret = regmap_read(cp->regmap, SC_ADC_REG_BASE + (channel << 1) + 1, &val_l);
	    bq_err("%s val_h=0X%x, val_l=0X%x, channel = %d\n", cp->log_tag, val_h, val_l, channel);

	    /* disable selected channel */
	    if (channel == ADC_IBUS) {
		    regmap_update_bits(cp->regmap, LN8410_REG_ADC_CTRL, LN8410_IBUS_ADC_DIS_MASK, 1);
	    } else {
		    regmap_update_bits(cp->regmap, LN8410_REG_ADC_FN_DISABLE1, (1 << (8-channel)), (1 << (8-channel)));
	    }
	    regmap_update_bits(cp->regmap, LN8410_REG_ADC_CTRL, LN8410_ADC_EN_MASK, 0);
    } else {

	    /*PAUSE_ADC_UPDATES=1 */
	    regmap_write(cp->regmap, LN8410_REG_LION_CTRL, 0x5B);
	    regmap_update_bits(cp->regmap, LN8410_REG_76, LN8410_PAUSE_ADC_UPDATES_MASK, LN8410_PAUSE_ADC_UPDATES_MASK);
	    msleep(2); // 2 MS

	    ret = regmap_read(cp->regmap, SC_ADC_REG_BASE + (channel << 1), &val_h);
	    ret = regmap_read(cp->regmap, SC_ADC_REG_BASE + (channel << 1) + 1, &val_l);

	    /*PAUSE_ADC_UPDATES=0 */
	    regmap_update_bits(cp->regmap, LN8410_REG_76, LN8410_PAUSE_ADC_UPDATES_MASK, 0);
	    regmap_write(cp->regmap, LN8410_REG_LION_CTRL, 0x00);
    }

    if (ret < 0)
        return ret;
    val = (val_h << 8) | val_l;
    if(channel == ADC_IBUS){
	    val = val * LN8410_IBUS_ADC_LSB;
	     bq_err("%s revison=%d, product_cfg=%d\n", cp->log_tag, cp->revision, cp->product_cfg);
	    if (cp->revision == 0x2 && cp->product_cfg == 0x2) {
		    val =(val * 2) / 3;
	    }
    }
    else if(channel == ADC_VBUS)		val = val * LN8410_VBUS_ADC_LSB;
    else if(channel == ADC_VUSB)		val = val * LN8410_VUSB_ADC_LSB;
    else if(channel == ADC_VWPC)		val = val * LN8410_VWPC_ADC_LSB;
    else if(channel == ADC_VOUT)		val = val * LN8410_VOUT_ADC_LSB;
    else if(channel == ADC_VBAT)		val = val * LN8410_VBAT_ADC_LSB;
    else if(channel == ADC_IBAT)		val = val * LN8410_IBAT_ADC_LSB;
    else if(channel == ADC_TBAT)		val = val * LN8410_TSBAT_ADC_LSB;
    else if(channel == ADC_TDIE)		val = val * LN8410_TDIE_ADC_LSB;
    *result = val;

    return ret;
}

/* LN8410_SYNC_FUNCTION_EN */
int ln8410_enable_parallel_func(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;

    if (enable)
        val = LN8410_SYNC_FUNCTION_ENABLE;
    else
        val = LN8410_SYNC_FUNCTION_DISABLE;
    val <<= LN8410_SYNC_FUNCTION_EN_SHIFT;
	ret = regmap_update_bits(cp->regmap, LN8410_REG_0D,
				LN8410_SYNC_FUNCTION_EN_MASK, val);
    return ret;
}
/* LN8410_SYNC_MASTER_EN */
int ln8410_enable_config_func(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;

    if (enable)
        val = LN8410_SYNC_CONFIG_MASTER;
    else
        val = LN8410_SYNC_CONFIG_SLAVE;
    val <<= LN8410_SYNC_MASTER_EN_SHIFT;
	ret = regmap_update_bits(cp->regmap, LN8410_REG_0D,
				LN8410_SYNC_MASTER_EN_MASK, val);
    return ret;
}
/* LN8410_REG_RST */
int ln8410_set_reg_reset(struct sc8561_device *cp)
{
	int ret = 0;
	u8 val;

    val = LN8410_REG_RESET;
    val <<= LN8410_REG_RST_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0D,
                LN8410_REG_RST_MASK, val);
	return ret;
}
/* LN8410_MODE */
int ln8410_set_operation_mode(struct sc8561_device *cp, int operation_mode)
{
    int ret = 0;
    u8 val;

    switch (operation_mode) {
        case LN8410_FORWARD_4_1_CHARGER_MODE:
            val = LN8410_FORWARD_4_1_CHARGER_MODE;
        break;
        case LN8410_FORWARD_2_1_CHARGER_MODE:
            val = LN8410_FORWARD_2_1_CHARGER_MODE;
        break;
        case LN8410_FORWARD_1_1_CHARGER_MODE:
        case LN8410_FORWARD_1_1_CHARGER_MODE1:
            val = LN8410_FORWARD_1_1_CHARGER_MODE;
        break;
        case LN8410_REVERSE_1_4_CONVERTER_MODE:
            val = LN8410_REVERSE_1_4_CONVERTER_MODE;
        break;
        case LN8410_REVERSE_1_2_CONVERTER_MODE:
            val = LN8410_REVERSE_1_2_CONVERTER_MODE;
        break;
        case LN8410_REVERSE_1_1_CONVERTER_MODE:
        case LN8410_REVERSE_1_1_CONVERTER_MODE1:
            val = LN8410_REVERSE_1_1_CONVERTER_MODE;
        break;
        default:
            bq_err("%s ln8410 set operation mode fail : not have this mode!\n", cp->log_tag);
            return -1;
        break;
    }
	bq_err("%s ln8410_set_operation_mode %d\n", cp->log_tag, operation_mode);
    val <<= LN8410_MODE_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0D,
                LN8410_MODE_MASK, val);
    return ret;
}
int ln8410_get_operation_mode(struct sc8561_device *cp, int *operation_mode)
{
	int ret = 0;
	unsigned int val;

	ret = regmap_read(cp->regmap, LN8410_REG_0D, &val);
	if (ret) {
		bq_err("%s ln8410 get operation mode fail\n", cp->log_tag);
		return ret;
	}
	*operation_mode = (val & LN8410_MODE_MASK);

	return ret;
}
/* LN8410_ACDRV_MANUAL_EN */
static int ln8410_enable_acdrv_manual(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;

    if (enable)
        val = LN8410_ACDRV_MANUAL_MODE;
    else
        val = LN8410_ACDRV_AUTO_MODE;
    val <<= LN8410_ACDRV_MANUAL_EN_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0A,
                LN8410_ACDRV_MANUAL_EN_MASK, val);
    return ret;
}

/* LN8410_WPCGATE_EN */
int ln8410_enable_wpcgate(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;

    if (enable)
        val = LN8410_WPCGATE_ENABLE;
    else
        val = LN8410_WPCGATE_DISABLE;
    val <<= LN8410_WPCGATE_EN_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0A,
                LN8410_WPCGATE_EN_MASK, val);
    return ret;
}
/* LN8410_OVPGATE_EN */
int ln8410_enable_ovpgate(struct sc8561_device *cp, bool enable)
{
    int ret = 0;
    u8 val;

    if (enable)
        val = LN8410_OVPGATE_ENABLE;
    else
        val = LN8410_OVPGATE_DISABLE;
    val <<= LN8410_OVPGATE_EN_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0A,
                LN8410_OVPGATE_EN_MASK, val);
    return ret;
}
/* LN8410_IBAT_SNS */
static int ln8410_set_sense_resistor(struct sc8561_device *cp, u8 r_mohm)
{
	int ret = 0;
	u8 val;

    if (r_mohm == 1)
        val = LN8410_IBAT_SNS_RES_1MHM;
    else if (r_mohm == 2)
        val = LN8410_IBAT_SNS_RES_2MHM;
    else
		return -1;
    val <<= LN8410_IBAT_SNS_RES_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0D,
                LN8410_IBAT_SNS_RES_MASK,
                val);
	return ret;
}
/* LN8410_SS_TIMEOUT */
static int ln8410_set_ss_timeout(struct sc8561_device *cp, int timeout)
{
	int ret = 0;
	u8 val;

	switch (timeout) {
		case 0:
			val = LN8410_SS_TIMEOUT_DISABLE;
			break;
		case 40:
			val = LN8410_SS_TIMEOUT_40MS;
			break;
		case 80:
			val = LN8410_SS_TIMEOUT_80MS;
			break;
		case 320:
			val = LN8410_SS_TIMEOUT_320MS;
			break;
		case 1280:
			val = LN8410_SS_TIMEOUT_1280MS;
			break;
		case 5120:
			val = LN8410_SS_TIMEOUT_5120MS;
			break;
		case 20480:
			val = LN8410_SS_TIMEOUT_20480MS;
			break;
		case 81920:
			val = LN8410_SS_TIMEOUT_81920MS;
			break;
		default:
			val = LN8410_SS_TIMEOUT_DISABLE;
			break;
	}
	val <<= LN8410_SS_TIMEOUT_SET_SHIFT;
	ret =  regmap_update_bits(cp->regmap, LN8410_REG_0C,
			LN8410_SS_TIMEOUT_SET_MASK,
			val);
	return ret;
}


static int ln8410_set_ucp_fall_dg(struct sc8561_device *cp, u8 date)
{
    int ret = 0;
    u8 val;

	val = date;
    val <<= LN8410_BUS_UCP_FALL_DG_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_06,
                LN8410_BUS_UCP_FALL_DG_MASK, val);
    return ret;
}

/* LN8410_VBUS_SHORT_DIS */ /* Protection */
int ln8410_vbus_short_dis(struct sc8561_device *cp, bool enable)
{
	int ret = 0;
	u8 val;

    if (enable)
        val = LN8410_VBUS_SHORT_DISABLE;
    else
        val = LN8410_VBUS_SHORT_ENABLE;
    val <<= LN8410_VBUS_SHORT_DIS_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_7B,
                LN8410_VBUS_SHORT_DIS_MASK, val);
    return ret;
}

/* LN8410_USE_HVLDO */ /* Protection */
int ln8410_use_hvldo(struct sc8561_device *cp, bool enable)
{
	int ret = 0;
	u8 val;

    if (enable)
        val = LN8410_USE_HVLDO;
    else
        val = LN8410_NOT_USE_HVLDO;
    val <<= LN8410_USE_HVLDO_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_52,
                LN8410_USE_HVLDO_MASK, val);
    return ret;
}
/* LN8410_FSW_SET */
static int ln8410_fsw_set(struct sc8561_device *cp, u8 date)
{
    int ret = 0;
    u8 val;

	val = date;
    val <<= LN8410_FSW_SET_SHIFT;
    ret = regmap_update_bits(cp->regmap, LN8410_REG_0B,
                LN8410_FSW_SET_MASK, val);
    return ret;
}

static int ln8410_soft_reset(struct sc8561_device *cp)
{
    unsigned int sys_st;
    int i;
    int ret;

	/* SOFT_RESET_REQ=1 */
	ret = regmap_write(cp->regmap, LN8410_REG_LION_CTRL, 0xC6);
	ret = regmap_update_bits(cp->regmap, LN8410_REG_TEST_MODE_CTRL, 0x1, 0x1);

	msleep(30); // 30 MS

    /* check status */
    for (i=0; i < 30; ++i) {
		ret = regmap_read(cp->regmap, LN8410_REG_SYS_STS, &sys_st);
	            if (sys_st & 0x3) {
	                    break;
		}
		msleep(5); // 5 MS
    }
	if (i == 30)
		bq_err("%s fail to reset, can't check the valid status(sys_st=0x%x)\n", cp->log_tag, sys_st);

    return 0;
}

static int protected_reg_init(struct sc8561_device *cp)
{
	int ret = 0;

	regmap_write(cp->regmap, LN8410_REG_LION_CTRL, 0xAA);
	ln8410_vbus_short_dis(cp, 1); //disable vbus short

	if (cp->revision == 0x2) {
		ln8410_use_hvldo(cp, 0); //not use hvldo ,no need in cs2
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_LION_STARTUP_CTRL, 0x04, 0x04);  /* C1_C2_CHARGE_TIME_CFG = 1 */
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_CFG9, 0x20, 0x20);               /* USE_TINY_MASK = 1 */
		ret = regmap_update_bits(cp->regmap,  LN8410_REG_CFG8, 0x40, 0x00);               /* C1N_C2N_C3N_FORCE_IDLE_MASK = 0 */
	}

	regmap_write(cp->regmap, LN8410_REG_LION_CTRL, 0x00);

	return ret;
}

static int ln8410_init_protection(struct sc8561_device *cp, int forward_work_mode)
{
	int ret = 0;
	ret = ln8410_enable_batovp(cp, true);
	ret = ln8410_enable_batocp(cp, false);
	ret = ln8410_enable_busocp(cp, true);
	ret = ln8410_enable_busucp(cp, true);
	ret = ln8410_enable_pmid2outovp(cp, true);
	ret = ln8410_enable_pmid2outuvp(cp, true);
//	ret = ln8410_enable_batovp_alarm(cp, true);
//	ret = ln8410_enable_busocp_alarm(cp, true);
	ret = ln8410_set_batovp_th(cp, 4650);
	ret = ln8410_set_batocp_th(cp, 8000);
//	ret = ln8410_set_batovp_alarm_th(cp, cp->bat_ovp_alarm_threshold);
	ret = ln8410_set_busovp_th(cp, 0); /*0 low set, 5.6v for 1:1, 12v for 2:1, 22v for 4:1*/
	ret = ln8410_set_busocp_th(cp, 3750);
	ret = ln8410_set_busucp_th(cp, 100);
	if (forward_work_mode == CP_FORWARD_4_TO_1) {
		ret = ln8410_set_usbovp_th(cp, 22000);
	} else if (forward_work_mode == CP_FORWARD_2_TO_1) {
		ret = ln8410_set_usbovp_th(cp, 14000);
	} else {
		//To do later for forward 1:1 mode
		;
	}
	ret = ln8410_set_wpcovp_th(cp, 22000);
	ret = ln8410_set_outovp_th(cp, 5000);
	ret = ln8410_set_pmid2outuvp_th(cp, 100);
	ret = ln8410_set_pmid2outovp_th(cp, 400);
	return ret;
}

static int ln8410_init_device(struct sc8561_device *cp)
{
	int ret = 0;

	ret = regmap_read(cp->regmap, LN8410_REG_62, &(cp->revision));
	cp->revision = cp->revision >> 4;
	ret = regmap_read(cp->regmap, LN8410_REG_PRODUCT_ID, &(cp->product_cfg));
	cp->product_cfg = cp->product_cfg & 0x0F;

	ln8410_soft_reset(cp);
	regmap_write(cp->regmap, LN8410_REG_RECOVERY_CTRL, 0x00); // turn off all auto recovery
	ln8410_set_REGN_pull_down(cp, true);
	protected_reg_init(cp);
	ln8410_fsw_set(cp, LN8410_FSW_SET_580K);
	ln8410_init_protection(cp, CP_FORWARD_4_TO_1);
	ln8410_enable_acdrv_manual(cp, 0);//ac drive manual mode
	ln8410_enable_wpcgate(cp, true);
	ln8410_enable_ovpgate(cp, true);
	ln8410_set_ss_timeout(cp, 5120);
	ln8410_set_ucp_fall_dg(cp, LN8410_BUS_UCP_FALL_DG_5MS);
	ln8410_set_sense_resistor(cp, 1);
	ret = ln8410_set_adc_mode(cp, 0x01);
	ln8410_set_operation_mode(cp, LN8410_FORWARD_4_1_CHARGER_MODE);

 	/* masked unuse interrupts */
	regmap_update_bits(cp->regmap, LN8410_REG_11, 0xFE, 0xFE); //VUSB INSERT
	regmap_update_bits(cp->regmap, LN8410_REG_14, 0x08, 0x08); //close adc done irq
	regmap_update_bits(cp->regmap, LN8410_REG_07, 0x10, 0x10); //close pmic2out ovp irq
	regmap_update_bits(cp->regmap, LN8410_REG_08, 0x10, 0x10); //close pmic2out uvp irq
	regmap_update_bits(cp->regmap, LN8410_REG_LION_INT_MASK, 0xFF, 0xFF);
	regmap_update_bits(cp->regmap, LN8410_REG_06, 0x0A, 0x0A); //close ibus ucp rise and fail irq
	regmap_update_bits(cp->regmap, LN8410_REG_04, 0x40, 0x40); //close vwpc ovp irq
	regmap_update_bits(cp->regmap, LN8410_REG_03, 0x40, 0x40); //close vusb ovp irq
	regmap_update_bits(cp->regmap, LN8410_REG_01, 0x20, 0x20); //close vbat ovp irq
	return ret;
}

int cp_get_adc_data(struct sc8561_device *bq, int channel,  u32 *result)
{
    int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_get_adc_data(bq, channel, result);
	} else if (bq->chip_vendor == LN8410) {
		ret = ln8410_get_adc_data(bq, channel, result);
	} else
		ret = -1;

	if (ret)
		bq_err("%s failed get ADC value\n", bq->log_tag);

	return ret;
}

int cp_enable_adc(struct sc8561_device *bq, bool enable)
{
	int ret = 0;

	if (bq->chip_vendor == SC8561)
		ret = sc8561_enable_adc(bq, enable);
	else if (bq->chip_vendor == LN8410)
		ret = ln8410_enable_adc(bq, enable);
	else
		ret = -1;

	if (ret)
		bq_err("%s failed to enable/disable ADC\n", bq->log_tag);
		
    return ret;
}

static int ops_cp_enable_charge(struct charger_device *chg_dev, bool enable)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_enable_charge(bq, enable);
	} else if (bq->chip_vendor == LN8410) {
		ret = ln8410_enable_charge(bq, enable);
	} else
		ret = -1;

	if (ret)
		bq_err("%s failed enable cp charge\n", bq->log_tag);

	return ret;
}

static int ops_cp_get_charge_enable(struct charger_device *chg_dev, bool *enabled)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_check_charge_enabled(bq, enabled);
		ret = sc8561_dump_important_regs(bq);
	} else if (bq->chip_vendor == LN8410) {
		ret = ln8410_check_charge_enabled(bq, enabled);
		ret = ln8410_dump_important_regs(bq);
	} else
		ret = -1;

	if (ret)
		bq_err("%s failed get enable cp charge status ret =%d\n", bq->log_tag, ret);

	return ret;
}

static int ops_cp_get_vbus(struct charger_device *chg_dev, u32 *val)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_get_adc_data(bq, ADC_VBUS, val);

	return ret;
}

static int ops_cp_get_ibus(struct charger_device *chg_dev, u32 *val)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_get_adc_data(bq, ADC_IBUS, val);

	return ret;
}

static int ops_cp_get_vbatt(struct charger_device *chg_dev, u32 *val)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_get_adc_data(bq, ADC_VBAT, val);

	return ret;
}

static int ops_cp_get_ibatt(struct charger_device *chg_dev, u32 *val)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_get_adc_data(bq, ADC_IBAT, val);

	return ret;
}

static int ops_cp_set_mode(struct charger_device *chg_dev, int value)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;
	
	if (bq->chip_vendor == SC8561) {
		ret = sc8561_set_operation_mode(bq, value);
	} else if (bq->chip_vendor == LN8410) {
		ret = ln8410_set_operation_mode(bq, value);
	} else
		ret = -1;

	if (ret)
		bq_err("%s failed set cp charge mode\n", bq->log_tag);

	return ret;
}

static int ops_cp_device_init(struct charger_device *chg_dev, int value)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_init_protection(bq, value);
	} else if (bq->chip_vendor == LN8410) {
		ret = ln8410_init_protection(bq, value);
	} else
		ret = -1;

	if (ret)
		bq_err("%s failed init cp charge protection\n", bq->log_tag);

	return ret;
}

static int ops_cp_enable_adc(struct charger_device *chg_dev, bool enable)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_enable_adc(bq, enable);

	return ret;
}

static int ops_cp_is_bypass_enabled(struct charger_device *chg_dev, bool *enabled)
{
	*enabled = false;
	return 0;
}


static int ops_cp_get_bypass_support(struct charger_device *chg_dev, bool *enabled)
{
		*enabled = 0;
		chr_err("%s %d\n", __func__, *enabled);
		return 0;
}

static const struct charger_ops sc8561_chg_ops = {
	.enable = ops_cp_enable_charge,
	.is_enabled = ops_cp_get_charge_enable,
	.get_vbus_adc = ops_cp_get_vbus,
	.get_ibus_adc = ops_cp_get_ibus,
	.cp_get_vbatt = ops_cp_get_vbatt,
	.cp_get_ibatt = ops_cp_get_ibatt,
	.cp_set_mode = ops_cp_set_mode,
	.is_bypass_enabled = ops_cp_is_bypass_enabled,
	.cp_device_init = ops_cp_device_init,
	.cp_enable_adc = ops_cp_enable_adc,
	.cp_get_bypass_support = ops_cp_get_bypass_support,
};

static const struct charger_properties sc8561_master_chg_props = {
	.alias_name = "cp_master",
};

static const struct charger_properties sc8561_slave_chg_props = {
	.alias_name = "cp_slave",
};

static void sc8561_irq_handler(struct work_struct *work)
{
	//struct sc8561_device *bq = container_of(work, struct sc8561_device, irq_handle_work.work);
    //bq_info("%s hanler sc8561_interrupt\n", bq->log_tag);
	//sc8561_dump_important_regs(bq);

	return;
}

static irqreturn_t sc8561_interrupt(int irq, void *private)
{
	//struct sc8561_device *bq = private;

	//bq_info("%s sc8561_interrupt\n", bq->log_tag);

	//schedule_delayed_work(&bq->irq_handle_work, 0);

	return IRQ_HANDLED;
}

static int sc8561_parse_dt(struct sc8561_device *bq)
{
	struct device_node *np = bq->dev->of_node;
	int ret = 0;

	if (!np) {
		bq_err("%s device tree info missing\n", bq->log_tag);
		return -1;
	}

	bq->irq_gpio = of_get_named_gpio(np, "sc8561_irq_gpio", 0);
	if (!gpio_is_valid(bq->irq_gpio)) {
		bq_err("%s failed to parse sc8561_irq_gpio\n", bq->log_tag);
		return -1;
	}

	return ret;
}

static int sc8561_init_irq(struct sc8561_device *bq)
{
	int ret = 0;

	ret = devm_gpio_request(bq->dev, bq->irq_gpio, dev_name(bq->dev));
	if (ret < 0) {
		bq_err("%s failed to request gpio\n", bq->log_tag);
		return -1;
	}

	bq->irq = gpio_to_irq(bq->irq_gpio);
	if (bq->irq < 0) {
		bq_err("%s failed to get gpio_irq\n", bq->log_tag);
		return -1;
	}

	ret = request_irq(bq->irq, sc8561_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(bq->dev), bq);
	if (ret < 0) {
		bq_err("%s failed to request irq\n", bq->log_tag);
		return -1;
	}

	enable_irq_wake(bq->irq);

	return 0;
}

static int sc8561_register_charger(struct sc8561_device *bq, int driver_data)
{
	switch (driver_data) {
	case SC8561_SLAVE:
		bq->chg_dev = charger_device_register("cp_slave", bq->dev, bq, &sc8561_chg_ops, &sc8561_slave_chg_props);
		if (bq->chip_vendor == SC8561) {
			strcpy(bq->log_tag, "[XMCHG_SC8561_SLAVE]");
			sc8561_enable_parallel_func(bq, false);
			sc8561_enable_config_func(bq, false);
		} else if (bq->chip_vendor == LN8410) {
			strcpy(bq->log_tag, "[XMCHG_Ln8410_SLAVE]");
			ln8410_enable_parallel_func(bq, false);
			ln8410_enable_config_func(bq, false);
		} else
			strcpy(bq->log_tag, "[XMCHG_UNKNOW_SLAVE]");
		break;
	case SC8561_MASTER:
		bq->chg_dev = charger_device_register("cp_master", bq->dev, bq, &sc8561_chg_ops, &sc8561_master_chg_props);
		if (bq->chip_vendor == SC8561) {
			strcpy(bq->log_tag, "[XMCHG_SC8561_MASTER]");
			sc8561_enable_parallel_func(bq, false);
			sc8561_enable_config_func(bq, true);
		} else if (bq->chip_vendor == LN8410) {
			strcpy(bq->log_tag, "[XMCHG_Ln8410_MASTER]");
			ln8410_enable_parallel_func(bq, false);
			ln8410_enable_config_func(bq, true);
		} else
			strcpy(bq->log_tag, "[XMCHG_UNKNOW_MASTER]");
		break;
	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static int cp_vbus_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (gm) {
		ret = cp_get_adc_data(gm, ADC_VBUS, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int cp_ibus_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (gm) {
		ret = cp_get_adc_data(gm, ADC_IBUS, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int cp_tdie_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (gm) {
		ret = cp_get_adc_data(gm, ADC_TDIE, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int chip_ok_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->chip_ok;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static ssize_t cp_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct sc8561_device *gm;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gm = (struct sc8561_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(gm, usb_attr, val);

	return count;
}

static ssize_t cp_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct sc8561_device *gm;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct sc8561_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(gm, usb_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

/* Must be in the same order as BMS_PROP_* */
static struct mtk_cp_sysfs_field_info cp_sysfs_field_tbl[] = {
	CP_SYSFS_FIELD_RO(cp_vbus, CP_PROP_VBUS),
	CP_SYSFS_FIELD_RO(cp_ibus, CP_PROP_IBUS),
	CP_SYSFS_FIELD_RO(cp_tdie, CP_PROP_TDIE),
	CP_SYSFS_FIELD_RO(chip_ok, CP_PROP_CHIP_OK),
};

static struct attribute *
	cp_sysfs_attrs[ARRAY_SIZE(cp_sysfs_field_tbl) + 1];

static const struct attribute_group cp_sysfs_attr_group = {
	.attrs = cp_sysfs_attrs,
};

static void cp_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(cp_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		cp_sysfs_attrs[i] = &cp_sysfs_field_tbl[i].attr.attr;

	cp_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int cp_sysfs_create_group(struct power_supply *psy)
{
	cp_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&cp_sysfs_attr_group);
}

static enum power_supply_property sc8561_power_supply_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static int sc8561_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct sc8561_device *bq = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 data = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq->chip_ok;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = cp_get_adc_data(bq, ADC_VBAT, &data);
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = cp_get_adc_data(bq, ADC_IBAT, &data);
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = cp_get_adc_data(bq, ADC_TBAT, &data);
		val->intval = data;
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0) {
		bq_err("%s failed to get prop value = %d\n", bq->log_tag, psp);
		return ret;
	}
	return 0;
}

static int sc8561_power_supply_init(struct sc8561_device *bq,
							struct device *dev,
							int driver_data)
{
	struct power_supply_config psy_cfg = { .drv_data = bq,
						.of_node = dev->of_node, };

	switch (driver_data) {
	case SC8561_MASTER:
		bq->psy_desc.name = "cp_master";
		break;
	case SC8561_SLAVE:
		bq->psy_desc.name = "cp_slave";
		break;
	default:
		return -EINVAL;
	}

	bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	bq->psy_desc.properties = sc8561_power_supply_props,
	bq->psy_desc.num_properties = ARRAY_SIZE(sc8561_power_supply_props),
	bq->psy_desc.get_property = sc8561_get_property,

	bq->cp_psy = devm_power_supply_register(bq->dev, &bq->psy_desc, &psy_cfg);
	if (IS_ERR(bq->cp_psy))
		return -EINVAL;

	return 0;
}

static int cp_charge_detect_device(struct sc8561_device *bq)
{
	int ret = 0;
	unsigned int data;

    ret = regmap_read(bq->regmap, SC8561_REG_6E, &data);
	if (ret < 0) {
		bq_err("%s failed to read device id\n", bq->log_tag);
		return ret;
	}

	bq_err("%s sucess read device id = %x\n", bq->log_tag, data);
	if (data == SC8561_DEVICE_ID)
		bq->chip_vendor = SC8561;
	else if (data == LN8410_DEVICE_ID)
		bq->chip_vendor = LN8410;
	else
		bq->chip_vendor = UNKNOW;

	return ret;
}

static int sc8561_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc8561_device *bq;
	int ret = 0;
#if defined(CONFIG_TARGET_PRODUCT_XAGA)
	const char * buf = get_hw_sku();
	char *xaga = NULL;
	char *xagapro = strnstr(buf, "xagapro", strlen(buf));
	if(!xagapro)
		xaga = strnstr(buf, "xaga", strlen(buf));
	if(xagapro)
		bq_err("%s ++\n", __func__);
	else if(xaga) {
		return -ENODEV;
	} else {
		return -ENODEV;
	}
#endif
	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->client = client;
	bq->dev = dev;

	i2c_set_clientdata(client, bq);
    bq->regmap = devm_regmap_init_i2c(client, &sc8561_regmap_config);
	if (IS_ERR(bq->regmap)) {
		return PTR_ERR(bq->regmap);
	}
	strcpy(bq->log_tag, "[XMCHG_BQ25980_UNKNOW]");
	ret = cp_charge_detect_device(bq);
	if (ret) {
		bq_err("%s failed to detect device\n", bq->log_tag);
		return ret;
	}

	ret = sc8561_parse_dt(bq);
	if (ret) {
		bq_err("%s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	ret = sc8561_init_irq(bq);
	if (ret) {
		bq_err("%s failed to int irq\n", bq->log_tag);
		return ret;
	}

	ret = sc8561_register_charger(bq, id->driver_data);
	if (ret) {
		bq_err("%s failed to register charger\n", bq->log_tag);
		return ret;
	}

	ret = sc8561_power_supply_init(bq, dev, id->driver_data);
	if (ret) {
		bq_err("%s failed to init psy\n", bq->log_tag);
		return ret;
	} else
		cp_sysfs_create_group(bq->cp_psy);

	if (bq->chip_vendor == SC8561)
		ret = sc8561_init_device(bq);
	else if (bq->chip_vendor == LN8410)
		ret = ln8410_init_device(bq);
	else
		ret = -1;
	if (ret) {
		bq_err("%s failed to init registers\n", bq->log_tag);
		return ret;
	}

	INIT_DELAYED_WORK(&bq->irq_handle_work, sc8561_irq_handler);
	bq->chip_ok = true;
	//ret = sc8561_dump_important_regs(bq);
	bq_err("%s %d probe success\n", bq->log_tag, ret);

	return 0;
}

static int sc8561_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8561_device *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = cp_enable_adc(bq, false);
	if (ret)
		bq_err("%s failed to disable ADC\n", bq->log_tag);

	bq_info("%s sc8561 suspend!\n", bq->log_tag);

	return enable_irq_wake(bq->irq);
}

static int sc8561_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8561_device *bq = i2c_get_clientdata(client);

	bq_info("%s sc8561 resume!\n", bq->log_tag);

	return disable_irq_wake(bq->irq);
}

static const struct dev_pm_ops sc8561_pm_ops = {
	.suspend	= sc8561_suspend,
	.resume		= sc8561_resume,
};

static int sc8561_remove(struct i2c_client *client)
{
	struct sc8561_device *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = cp_enable_adc(bq, false);
	if (ret)
		bq_err("%s failed to disable ADC\n", bq->log_tag);

	power_supply_unregister(bq->cp_psy);
	return ret;
}

static void sc8561_shutdown(struct i2c_client *client)
{
	struct sc8561_device *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = cp_enable_adc(bq, false);
	if (ret)
		bq_err("%s failed to disable ADC\n", bq->log_tag);

	bq_info("%s sc8561 shutdown!\n", bq->log_tag);
}

static const struct i2c_device_id sc8561_i2c_ids[] = {
	{ "sc8561_master", SC8561_MASTER },
	{ "sc8561_slave", SC8561_SLAVE },
	{},
};
MODULE_DEVICE_TABLE(i2c, sc8561_i2c_ids);

static const struct of_device_id sc8561_of_match[] = {
	{ .compatible = "sc8561_master", .data = (void *)SC8561_MASTER},
	{ .compatible = "sc8561_slave", .data = (void *)SC8561_SLAVE},
	{ },
};
MODULE_DEVICE_TABLE(of, sc8561_of_match);

static struct i2c_driver sc8561_driver = {
	.driver = {
		.name = "sc8561_charger_pump",
		.of_match_table = sc8561_of_match,
		.pm = &sc8561_pm_ops,
	},
	.id_table = sc8561_i2c_ids,
	.probe = sc8561_probe,
	.remove = sc8561_remove,
	.shutdown = sc8561_shutdown,
};
module_i2c_driver(sc8561_driver);

MODULE_AUTHOR("liujiquan <liujiquan@xiaomi.com>");
MODULE_DESCRIPTION("sc8561 charger pump driver");
MODULE_LICENSE("GPL v2");
