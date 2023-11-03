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
#include <../drivers/misc/hwid/hwid.h>

static int product_name = UNKNOWN;
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
	int cp_role;

	struct delayed_work irq_handle_work;
	int irq_gpio;
	int irq;
};

union cp_propval {
	unsigned int uintval;
	int intval;
	char strval[PAGE_SIZE];
};

struct mtk_cp_sysfs_field_info {
	struct device_attribute attr;
	enum cp_property prop;
	int (*set)(struct sc8561_device *gm,
		struct mtk_cp_sysfs_field_info *attr, int val);
	int (*get)(struct sc8561_device *gm,
		struct mtk_cp_sysfs_field_info *attr, union cp_propval *val);
};

static void charger_parse_cmdline(void)
{
	char *zircon_match= strnstr(product_name_get(), "zircon", strlen("zircon"));
	char *corot_match = strnstr(product_name_get(), "corot", strlen("corot"));

	if(zircon_match)
		product_name = ZIRCON;
	else if(corot_match)
		product_name = COROT;
}

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
int sc8561_check_adc_enabled(struct sc8561_device *bq, bool *enabled)
{
	int ret = 0;
	unsigned int val;
    ret = regmap_read(bq->regmap, SC8561_REG_15, &val);
    if (!ret)
        *enabled = !!(val & SC8561_ADC_EN_MASK);
    return ret;
}

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
    val <<= SC8561_BUS_OVP_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_05,
                SC8561_BUS_OVP_MASK, val);
    return ret;
}
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
		return ret;
	}
	*operation_mode = (val & SC8561_MODE_MASK);
	return ret;
}
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

static int sc8561_set_switch_freq(struct sc8561_device *bq, u8 data)
{
    int ret = 0;
    u8 val;
	val = data;
    val <<= SC8561_FSW_SET_SHIFT;
    ret = regmap_update_bits(bq->regmap, SC8561_REG_0C,
                SC8561_FSW_SET_MASK, val);
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

static void ln8410_abnormal_charging_judge(unsigned int *data)
{
	if(data == NULL)
		return;
	if((data[15] & 0x08) == 0)
	{
		bq_info("%s VOUT UVLO\n", __func__);
	}
	if((data[15] & 0x3F) != 0x3F)
	{
		bq_info("%s VIN have problem\n", __func__);
	}
	if(data[9] & 0x08)
	{
		bq_info("%s VBUS_ERRORHI_STAT\n", __func__);
	}
	if(data[9] & 0x10)
	{
		bq_info("%s VBUS_ERRORLO_STAT\n", __func__);
	}
	if(data[0] & 0x01)
	{
		bq_info("%s IBUS_UCP_FALL_FLAG\n", __func__);
	}
	if(data[9] & 0x01)
	{
		bq_info("%s CBOOT dio\n", __func__);
	}
	if(data[0] & 0x20)
	{
		bq_info("%s VBAT_OVP\n", __func__);
	}
	if(data[1] & 0x10)
	{
		bq_info("%s IBAT_OCP\n", __func__);
	}
	if(data[2] & 0x20)
	{
		bq_info("%s VBUS_OVP\n", __func__);
	}
	if(data[3] & 0x20)
	{
		bq_info("%s VWPC_OVP\n", __func__);
	}
	if(data[5] & 0x20)
	{
		bq_info("%s IBUS_OCP\n", __func__);
	}
	if(data[6] & 0x01)
	{
		bq_info("%s IBUS_UCP\n", __func__);
	}
	if(data[7] & 0x08)
	{
		bq_info("%s PMID2OUT_OVP\n", __func__);
	}
	if(data[8] & 0x08)
	{
		bq_info("%s PMID2OUT_UVP\n", __func__);
	}
	if(data[9] & 0x80)
	{
		bq_info("%s POR_FLAG\n", __func__);
	}
	return;
}

static unsigned int sc8561_reg_list[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
	0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x6E, 0x70, 0x7C,
};

static int sc8561_dump_important_regs(struct sc8561_device *bq, union cp_propval *val)
{
	unsigned int data[100] = {0,};
	int i = 0, reg_num = ARRAY_SIZE(sc8561_reg_list);
	int len = 0, idx = 0, idx_total = 0, len_sysfs = 0;
	char buf_tmp[256] = {0,};

	for (i = 0; i < reg_num; i++) {
		regmap_read(bq->regmap, sc8561_reg_list[i], &data[i]);
		len = scnprintf(buf_tmp + strlen(buf_tmp), PAGE_SIZE - idx,
						"[0x%02X]=0x%02X,", sc8561_reg_list[i], data[i]);
		idx += len;

		if (((i + 1) % 8 == 0) || ((i + 1) == reg_num)) {
			bq_info("%s %s\n", bq->log_tag, buf_tmp);

			if(val)
				len_sysfs = scnprintf(val->strval+ strlen(val->strval), PAGE_SIZE - idx_total, "%s\n", buf_tmp);

			memset(buf_tmp, 0x0, sizeof(buf_tmp));

			idx_total += len_sysfs;
			idx = 0;
		}
	}
	if((data[9] & 0x02) == 0)
		{
			ln8410_abnormal_charging_judge(data);
		}

	return 0;
}

static int sc8561_init_int_src(struct sc8561_device *bq)
{
    int ret = 0;
    ret = sc8561_set_batovp_alarm_int_mask(bq, SC8561_BAT_OVP_ALM_NOT_MASK);
    if (ret) {
        return ret;
    }
    ret = sc8561_set_busocp_alarm_int_mask(bq, SC8561_BUS_OCP_ALM_NOT_MASK);
    if (ret) {
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
		ret = sc8561_set_busovp_th(cp, 6000);
		ret = sc8561_set_busocp_th(cp, 4500);
		ret = sc8561_set_usbovp_th(cp, 6500);
	}
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

	int retry_cnt = 0;

	while (retry_cnt < 5)
	{
		ret |= sc8561_enable_acdrv_manual(cp, false);
		ret |= sc8561_enable_wpcgate(cp, true);
		ret |= sc8561_enable_ovpgate(cp, true);
		ret |= sc8561_set_ss_timeout(cp, 5120);
		ret |= sc8561_set_ucp_fall_dg(cp, SC8561_BUS_UCP_FALL_DG_5MS);
		ret |= sc8561_set_enable_tsbat(cp, false);
		ret |= sc8561_set_sync(cp, SC8561_SYNC_NO_SHIFT);
		ret |= sc8561_set_acdrv_up(cp, true);
		ret |= sc8561_set_sense_resistor(cp, 1);
		ret |= sc8561_set_ovpgate_on_dg_set(cp, 20);
		ret |= sc8561_init_protection(cp, CP_FORWARD_4_TO_1);
		ret |= sc8561_init_adc(cp);
		ret |= sc8561_init_int_src(cp);
		ret |= sc8561_set_operation_mode(cp, SC8561_FORWARD_4_1_CHARGER_MODE);
		ret |= sc8561_set_busovp_th(cp, 22000);

		if(product_name == ZIRCON){
			switch (cp->cp_role)
			{
			case SC8561_MASTER:
				sc8561_set_switch_freq(cp,SC8561_FSW_SET_625K);
				break;
			case SC8561_SLAVE:
				sc8561_set_switch_freq(cp,SC8561_FSW_SET_575K);
				break;
			default:
				break;
			}
		}

		if (ret < 0) {
			retry_cnt++;
		} else
			break;
	}

	return ret;
}

int cp_get_adc_data(struct sc8561_device *bq, int channel,  u32 *result)
{
    int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_get_adc_data(bq, channel, result);
	}else
	{
		ret = -1;
	}

	return ret;
}

int cp_enable_adc(struct sc8561_device *bq, bool enable)
{
	int ret = 0;

	if (bq->chip_vendor == SC8561)
		ret = sc8561_enable_adc(bq, enable);
	else
		ret = -1;


    return ret;
}

static int ops_cp_dump_register(struct charger_device *chg_dev)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_dump_important_regs(bq,NULL);
	}else
		ret = -1;


	return ret;
}

static int ops_cp_get_chip_vendor(struct charger_device *chg_dev, int *chip_id)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);

	*chip_id = bq->chip_vendor;
	bq_err("%s =%d  \n", bq->log_tag, *chip_id);

	return 0;
}

static int ops_cp_enable_charge(struct charger_device *chg_dev, bool enable)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_enable_charge(bq, enable);
	}else
		ret = -1;

	return ret;
}

static int ops_cp_get_charge_enable(struct charger_device *chg_dev, bool *enabled)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_check_charge_enabled(bq, enabled);
	}else
		ret = -1;

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
	}else
		ret = -1;


	return ret;
}

static int ops_cp_get_mode(struct charger_device *chg_dev, int *mode)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_get_operation_mode(bq, mode);
	}else
		ret = -1;
	return ret;
}

static int ops_cp_device_init(struct charger_device *chg_dev, int value)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_init_protection(bq, value);
	}else
		ret = -1;


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
		*enabled = 1;
		chr_err("%s %d\n", __func__, *enabled);
		return 0;
}

static int ops_enable_acdrv_manual(struct charger_device *chg_dev, bool enable)
{
	struct sc8561_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq->chip_vendor == SC8561) {
		ret = sc8561_enable_acdrv_manual(bq, enable);
	}else
		ret = -1;

	if (ret)
		bq_err("%s failed enable cp acdrv manual\n", bq->log_tag);

	return ret;
}

static const struct charger_ops sc8561_chg_ops = {
	.enable = ops_cp_enable_charge,
	.is_enabled = ops_cp_get_charge_enable,
	.get_vbus_adc = ops_cp_get_vbus,
	.get_ibus_adc = ops_cp_get_ibus,
	.cp_get_vbatt = ops_cp_get_vbatt,
	.cp_get_ibatt = ops_cp_get_ibatt,
	.cp_set_mode = ops_cp_set_mode,
	.cp_get_mode = ops_cp_get_mode,
	.is_bypass_enabled = ops_cp_is_bypass_enabled,
	.cp_device_init = ops_cp_device_init,
	.cp_enable_adc = ops_cp_enable_adc,
	.cp_get_bypass_support = ops_cp_get_bypass_support,
	.cp_dump_register = ops_cp_dump_register,
	.cp_get_chip_vendor = ops_cp_get_chip_vendor,
	.enable_acdrv_manual = ops_enable_acdrv_manual,
};

static const struct charger_properties sc8561_master_chg_props = {
	.alias_name = "cp_master",
};

static const struct charger_properties sc8561_slave_chg_props = {
	.alias_name = "cp_slave",
};

static void sc8561_irq_handler(struct work_struct *work)
{
	return;
}

static irqreturn_t sc8561_interrupt(int irq, void *private)
{
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
		}else
			strcpy(bq->log_tag, "[XMCHG_UNKNOW_SLAVE]");
		break;
	case SC8561_MASTER:
		bq->chg_dev = charger_device_register("cp_master", bq->dev, bq, &sc8561_chg_ops, &sc8561_master_chg_props);
		if (bq->chip_vendor == SC8561) {
			strcpy(bq->log_tag, "[XMCHG_SC8561_MASTER]");
			sc8561_enable_parallel_func(bq, false);
			sc8561_enable_config_func(bq, true);
		}else
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
	union cp_propval *val)
{
	int ret = 0;
	union cp_propval data;

	if (gm) {
		ret = cp_get_adc_data(gm, ADC_VBUS, &data.uintval);
		val->uintval = data.uintval;
	} else
		val->uintval = 0;
	chr_err("%s %d\n", __func__, val->uintval);
	return 0;
}

static int cp_ibus_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	union cp_propval *val)
{
	int ret = 0;
	union cp_propval data;

	if (gm) {
		ret = cp_get_adc_data(gm, ADC_IBUS, &data.uintval);
		val->uintval = data.uintval;
	} else
		val->uintval = 0;
	chr_err("%s %d\n", __func__, val->uintval);
	return 0;
}

static int cp_tdie_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	union cp_propval *val)
{
	int ret = 0;
	union cp_propval data;

	if (gm) {
		ret = cp_get_adc_data(gm, ADC_TDIE, &data.uintval);
		val->uintval = data.uintval;
	} else
		val->uintval = 0;
	chr_err("%s %d\n", __func__, val->uintval);
	return 0;
}

static int chip_ok_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	union cp_propval *val)
{
	if (gm)
		val->uintval = gm->chip_ok;
	else
		val->uintval = 0;
	chr_err("%s %d\n", __func__, val->uintval);
	return 0;
}

static int dump_reg_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	union cp_propval *val)
{
	int count = 0;

	if(gm)
		count = sc8561_dump_important_regs(gm,val);
	else
		bq_err("%s %s: Error! no sc8561_device\n",gm->log_tag,__FUNCTION__);

	return count;
}

static int cp_adc_enable_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	union cp_propval *val)
{
	int ret = 0;
	union cp_propval data;

	if (gm) {
		ret = sc8561_check_adc_enabled(gm, (bool *)&data.uintval);
		val->uintval = data.uintval;
	} else
		val->uintval = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int charge_enabled_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	union cp_propval *val)
{
	int ret = 0;
	union cp_propval data;

	if (gm) {
		ret = sc8561_check_charge_enabled(gm, (bool *)&data.uintval);
		val->uintval = data.uintval;
	} else
		val->uintval = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int work_mode_get(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	union cp_propval *val)
{
	switch (gm->work_mode)
	{
	case SC8561_FORWARD_4_1_CHARGER_MODE:
		val->uintval = 4;
		break;
	case SC8561_FORWARD_2_1_CHARGER_MODE:
		val->uintval = 2;
		break;
	case SC8561_FORWARD_1_1_CHARGER_MODE1:
	case SC8561_FORWARD_1_1_CHARGER_MODE:
		val->uintval = 1;
		break;
	default:
		break;
	}

	return 0;
}

static int cp_adc_enable_set(struct sc8561_device *gm,
	struct mtk_cp_sysfs_field_info *attr,
	int val)
{
	int ret = 0;

	val = !!val;

	if (gm->chip_vendor == SC8561)
		ret = sc8561_enable_adc(gm, val);
	else
		ret = -1;

	return ret;
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
	union cp_propval val;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct sc8561_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);

	if (usb_attr->get != NULL)
		usb_attr->get(gm, usb_attr, &val);

	switch (usb_attr->prop)
	{
	case CP_PROP_DUMP_REG:
		count = scnprintf(buf, PAGE_SIZE, "%s\n", val.strval);
		break;

	case CP_PROP_INT_MIN ... CP_PROP_INT_MAX:
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val.uintval);
		break;
	default:
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val.intval);
		break;
	}

	return count;
}

static struct mtk_cp_sysfs_field_info cp_sysfs_field_tbl[] = {
	CP_SYSFS_FIELD_RO(cp_vbus, CP_PROP_VBUS),
	CP_SYSFS_FIELD_RO(cp_ibus, CP_PROP_IBUS),
	CP_SYSFS_FIELD_RO(cp_tdie, CP_PROP_TDIE),
	CP_SYSFS_FIELD_RO(chip_ok, CP_PROP_CHIP_OK),
	CP_SYSFS_FIELD_RO(dump_reg, CP_PROP_DUMP_REG),
	CP_SYSFS_FIELD_RW(cp_adc_enable, CP_PROP_ADC_ENABLE),
	CP_SYSFS_FIELD_RO(charge_enabled, CP_PROP_CHARGE_ENABLED),
	CP_SYSFS_FIELD_RO(work_mode, CP_PROP_WORK_MODE),
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

	cp_sysfs_attrs[limit] = NULL;
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
	int retry_cnt = 0;

	while (retry_cnt < 5)
	{
		ret = regmap_read(bq->regmap, SC8561_REG_6E, &data);
		if (ret < 0) {
			retry_cnt++;
		} else
			break;
	}

	if (retry_cnt == 5 && ret < 0){
		return ret;
	}

	if (data == SC8561_DEVICE_ID)
	{
		bq->chip_vendor = SC8561;
	}else
	{
		return -1;
	}

	return ret;
}

static int sc8561_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc8561_device *bq;
	int ret = 0;

	charger_parse_cmdline();

	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->client = client;
	bq->dev = dev;
	bq->cp_role = id->driver_data;

	i2c_set_clientdata(client, bq);
    bq->regmap = devm_regmap_init_i2c(client, &sc8561_regmap_config);
	if (IS_ERR(bq->regmap)) {
		return PTR_ERR(bq->regmap);
	}
	strcpy(bq->log_tag, "[XMCHG_CP_UNKNOW]");
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
	else
		ret = -1;
	if (ret) {
		bq_err("%s failed to init registers\n", bq->log_tag);
		return ret;
	}
	INIT_DELAYED_WORK(&bq->irq_handle_work, sc8561_irq_handler);
	bq->chip_ok = true;

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
