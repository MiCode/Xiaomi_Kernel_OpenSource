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

#include "xmc_core.h"
#include "sc8561.h"

enum sc8561_work_mode {
        SC8561_STANDALONE,
        SC8561_MASTER,
	SC8561_SLAVE,
};

enum sc8561_adc_channel {
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
};

static struct regmap_config sc8561_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFF,
};

struct sc8561_device {
	struct i2c_client *client;
	struct device *dev;
	struct xmc_device *xmc_dev;
	struct power_supply *cp_psy;
	struct power_supply_desc psy_desc;
	struct regmap *regmap;
	bool chip_ok;
	char log_tag[25];
	int work_mode;

	struct delayed_work irq_handle_work;
	struct wakeup_source *irq_wakelock;
	int irq_gpio;
	int irq;
};

static unsigned int sc8561_reg_list[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x13, 0x6E, 0x70, 0x7C,
};

static int sc8561_enable_adc(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_ADC_ENABLE;
	else
		val = SC8561_ADC_DISABLE;

	val <<= SC8561_ADC_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_15, SC8561_ADC_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write ADC_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_charge(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_CHG_ENABLE;
	else
		val = SC8561_CHG_DISABLE;

	val <<= SC8561_CHG_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0B, SC8561_CHG_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write CHG_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_get_charge_enabled(struct sc8561_device *chip, bool *enabled)
{
	unsigned int val = 0;
	int ret = 0;

	ret = regmap_read(chip->regmap, SC8561_REG_0B, &val);
	if (ret)
		xmc_err("%s failed to read CHG_EN reg", chip->log_tag);

	*enabled = !!(val & SC8561_CHG_EN_MASK);
	return ret;
}

static int sc8561_enable_batovp(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_BAT_OVP_ENABLE;
	else
		val = SC8561_BAT_OVP_DISABLE;

	val <<= SC8561_BAT_OVP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_01, SC8561_BAT_OVP_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write BATOVP_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_set_batovp(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold < SC8561_BAT_OVP_BASE)
		threshold = SC8561_BAT_OVP_BASE;

	val = (threshold - SC8561_BAT_OVP_BASE) / SC8561_BAT_OVP_LSB;
	val <<= SC8561_BAT_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_01, SC8561_BAT_OVP_MASK, val);
	if (ret)
		xmc_err("%s failed to write BATOVP reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_batocp(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_BAT_OCP_ENABLE;
	else
		val = SC8561_BAT_OCP_DISABLE;

	val <<= SC8561_BAT_OCP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_02, SC8561_BAT_OCP_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write BATOCP_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_set_batocp(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold < SC8561_BAT_OCP_BASE)
		threshold = SC8561_BAT_OCP_BASE;

	val = (threshold - SC8561_BAT_OCP_BASE) / SC8561_BAT_OCP_LSB;
	val <<= SC8561_BAT_OCP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_02, SC8561_BAT_OCP_MASK, val);
	if (ret)
		xmc_err("%s failed to write BATOCP reg", chip->log_tag);

	return ret;
}

static int sc8561_set_usbovp(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold == 6500)
		val = SC8561_USB_OVP_6PV5;
	else
		val = (threshold - SC8561_USB_OVP_BASE) / SC8561_USB_OVP_LSB;

	val <<= SC8561_USB_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_03, SC8561_USB_OVP_MASK, val);
	if (ret)
		xmc_err("%s failed to write USBOVP reg", chip->log_tag);

	return ret;
}

static int sc8561_set_ovpgate_on_dg_set(struct sc8561_device *chip, int time)
{
	unsigned int val = 0;
	int ret = 0;

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
	ret = regmap_update_bits(chip->regmap, SC8561_REG_03, SC8561_OVPGATE_ON_DG_MASK, val);
	if (ret)
		xmc_err("%s failed to write OVPGATE_ON_DG reg", chip->log_tag);

	return ret;
}

static int sc8561_set_wpcovp(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold == 6500)
		val = SC8561_WPC_OVP_6PV5;
	else
		val = (threshold - SC8561_WPC_OVP_BASE) / SC8561_WPC_OVP_LSB;

	val <<= SC8561_WPC_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_04, SC8561_WPC_OVP_MASK, val);
	if (ret)
		xmc_err("%s failed to write WPCOVP reg", chip->log_tag);

	return ret;
}

static int sc8561_set_busovp(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (chip->work_mode == SC8561_FORWARD_4_1_CHARGER_MODE || chip->work_mode == SC8561_REVERSE_1_4_CONVERTER_MODE) {
		if (threshold < 14000)
			threshold = 14000;
		else if (threshold > 22000)
			threshold = 22000;
		val = (threshold - SC8561_BUS_OVP_41MODE_BASE) / SC8561_BUS_OVP_41MODE_LSB;
	} else if (chip->work_mode == SC8561_FORWARD_2_1_CHARGER_MODE || chip->work_mode == SC8561_REVERSE_1_2_CONVERTER_MODE) {
		if (threshold < 7000)
			threshold = 7000;
		else if (threshold > 13300)
			threshold = 13300;
		val = (threshold - SC8561_BUS_OVP_21MODE_BASE) / SC8561_BUS_OVP_21MODE_LSB;
	} else {
		if (threshold < 3500)
			threshold = 3500;
		else if (threshold > 5500)
			threshold = 5500;
		val = (threshold - SC8561_BUS_OVP_11MODE_BASE) / SC8561_BUS_OVP_11MODE_LSB;
	}

	xmc_info("%s set BUSOVP = %d", chip->log_tag, threshold);
	val <<= SC8561_BUS_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_05, SC8561_BUS_OVP_MASK, val);
	if (ret)
		xmc_err("%s failed to write BUSOVP reg", chip->log_tag);

	return ret;
}

static int sc8561_set_outovp(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold < SC8561_OUT_OVP_BASE)
		threshold = SC8561_OUT_OVP_BASE;

	val = (threshold - SC8561_OUT_OVP_BASE) / SC8561_OUT_OVP_LSB;
	val <<= SC8561_OUT_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_05, SC8561_OUT_OVP_MASK, val);
	if (ret)
		xmc_err("%s failed to write OUTOVP reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_busocp(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_BUS_OCP_ENABLE;
	else
		val = SC8561_BUS_OCP_DISABLE;

	val <<= SC8561_BUS_OCP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_06, SC8561_BUS_OCP_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write BUSOCP_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_set_busocp(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold < SC8561_BUS_OCP_BASE)
		threshold = SC8561_BUS_OCP_BASE;

	val = (threshold - SC8561_BUS_OCP_BASE) / SC8561_BUS_OCP_LSB;
	val <<= SC8561_BUS_OCP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_06, SC8561_BUS_OCP_MASK, val);
	if (ret)
		xmc_err("%s failed to write BUSOCP reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_busucp(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_BUS_UCP_ENABLE;
	else
		val = SC8561_BUS_UCP_DISABLE;

	val <<= SC8561_BUS_UCP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_07, SC8561_BUS_UCP_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write BUSUCP_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_pmid2outovp(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_PMID2OUT_OVP_ENABLE;
	else
		val = SC8561_PMID2OUT_OVP_DISABLE;

	val <<= SC8561_PMID2OUT_OVP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_08, SC8561_PMID2OUT_OVP_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write PMID2OVP_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_set_pmid2outovp_th(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold < SC8561_PMID2OUT_OVP_BASE)
		threshold = SC8561_PMID2OUT_OVP_BASE;

	val = (threshold - SC8561_PMID2OUT_OVP_BASE) / SC8561_PMID2OUT_OVP_LSB;
	val <<= SC8561_PMID2OUT_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_08, SC8561_PMID2OUT_OVP_MASK, val);
	if (ret)
		xmc_err("%s failed to write PMID2OVP reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_pmid2outuvp(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_PMID2OUT_UVP_ENABLE;
	else
		val = SC8561_PMID2OUT_UVP_DISABLE;

	val <<= SC8561_PMID2OUT_UVP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_09, SC8561_PMID2OUT_UVP_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write PMID2UVP_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_set_pmid2outuvp_th(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold < SC8561_PMID2OUT_UVP_BASE)
		threshold = SC8561_PMID2OUT_UVP_BASE;

	val = (threshold - SC8561_PMID2OUT_UVP_BASE) / SC8561_PMID2OUT_UVP_LSB;
	val <<= SC8561_PMID2OUT_UVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_09, SC8561_PMID2OUT_UVP_MASK, val);
	if (ret)
		xmc_err("%s failed to write PMID2UVP reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_batovp_alarm(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_BAT_OVP_ALM_ENABLE;
	else
		val = SC8561_BAT_OVP_ALM_DISABLE;

	val <<= SC8561_BAT_OVP_ALM_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_6C, SC8561_BAT_OVP_ALM_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write BATOVP_ALARM_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_set_batovp_alarm_th(struct sc8561_device *chip, int threshold)
{
	unsigned int val = 0;
	int ret = 0;

	if (threshold < SC8561_BAT_OVP_ALM_BASE)
		threshold = SC8561_BAT_OVP_ALM_BASE;

	val = (threshold - SC8561_BAT_OVP_ALM_BASE) / SC8561_BAT_OVP_ALM_LSB;
	val <<= SC8561_BAT_OVP_ALM_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_6C, SC8561_BAT_OVP_ALM_MASK, val);
	if (ret)
		xmc_err("%s failed to write BATOVP_ALARM reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_busocp_alarm(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_BUS_OCP_ALM_ENABLE;
	else
		val = SC8561_BUS_OCP_ALM_DISABLE;

	val <<= SC8561_BUS_OCP_ALM_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_6D, SC8561_BUS_OCP_ALM_DIS_MASK, val);
	if (ret)
		xmc_err("%s failed to write BATOVP_ALARM_EN reg", chip->log_tag);

	return ret;
}

static int sc8561_set_adc_scanrate(struct sc8561_device *chip, bool oneshot)
{
	unsigned int val = 0;
	int ret = 0;

	if (oneshot)
		val = SC8561_ADC_RATE_ONESHOT;
	else
		val = SC8561_ADC_RATE_CONTINOUS;

	val <<= SC8561_ADC_RATE_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_15, SC8561_ADC_RATE_MASK, val);
	if (ret)
		xmc_err("%s failed to write ADC_RATE reg", chip->log_tag);

	return ret;
}

static int sc8561_get_adc(struct sc8561_device *chip, int channel, u32 *result)
{
	int ret = 0;
	unsigned int val_l = 0, val_h = 0;
	u16 val = 0;

	if(channel >= ADC_MAX_NUM) {
		xmc_err("%s ADC channel is invalid, channel = %d", chip->log_tag, channel);
		return -1;
	}

	ret = regmap_read(chip->regmap, SC8561_REG_17 + (channel << 1), &val_h);
	if (ret) {
		xmc_err("%s failed to read ADC val_h, channel = %d", chip->log_tag, channel);
		return ret;
	}

	ret = regmap_read(chip->regmap, SC8561_REG_17 + (channel << 1) + 1, &val_l);
	if (ret) {
		xmc_err("%s failed to read ADC val_l, channel = %d", chip->log_tag, channel);
		return ret;
	}

	val = (val_h << 8) | val_l;

	if(channel == ADC_IBUS)
		val = val * SC8561_IBUS_ADC_LSB;
	else if(channel == ADC_VBUS)
		val = val * SC8561_VBUS_ADC_LSB;
	else if(channel == ADC_VUSB)
		val = val * SC8561_VUSB_ADC_LSB;
	else if(channel == ADC_VWPC)
		val = val * SC8561_VWPC_ADC_LSB;
	else if(channel == ADC_VOUT)
		val = val * SC8561_VOUT_ADC_LSB;
	else if(channel == ADC_VBAT)
		val = val * SC8561_VBAT_ADC_LSB;
	else if(channel == ADC_IBAT)
		val = val * SC8561_IBAT_ADC_LSB;
	else if(channel == ADC_TBAT)
		val = val * SC8561_TSBAT_ADC_LSB;
	else if(channel == ADC_TDIE)
		val = val * SC8561_TDIE_ADC_LSB;

	*result = val;
	return ret;
}

static int sc8561_set_adc_scan(struct sc8561_device *chip, int channel, bool enable)
{
	int ret = 0;
	u8 reg = 0, mask = 0, shift = 0, val = 0;

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

	ret = regmap_update_bits(chip->regmap, reg, mask, val);
	if (ret)
		xmc_err("%s failed to write ADC_SCAN reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_parallel_func(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_SYNC_FUNCTION_ENABLE;
	else
		val = SC8561_SYNC_FUNCTION_DISABLE;

	val <<= SC8561_SYNC_FUNCTION_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0E, SC8561_SYNC_FUNCTION_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write SYNC_FUNC reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_config_func(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_SYNC_CONFIG_MASTER;
	else
		val = SC8561_SYNC_CONFIG_SLAVE;

	val <<= SC8561_SYNC_MASTER_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0E, SC8561_SYNC_MASTER_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write OPMODE reg", chip->log_tag);

	return ret;
}

static int sc8561_set_operation_mode(struct sc8561_device *chip, int mode)
{
	unsigned int val = mode;
	int ret = 0;

	chip->work_mode = val;
	val <<= SC8561_MODE_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0E, SC8561_MODE_MASK, val);
	if (ret)
		xmc_err("%s failed to write DIV_MODE reg", chip->log_tag);

	return ret;
}

static int sc8561_get_operation_mode(struct sc8561_device *chip, int *mode)
{
	unsigned int val = 0;
	int ret = 0;

	ret = regmap_read(chip->regmap, SC8561_REG_0E, &val);
	if (0 != ret) {
		xmc_err("%s failed to read DIV_MODE reg", chip->log_tag);
		return ret;
	}

	*mode = (val & SC8561_MODE_MASK);
	return ret;
}

static int sc8561_enable_acdrv_manual(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_ACDRV_MANUAL_MODE;
	else
		val = SC8561_ACDRV_AUTO_MODE;

	val <<= SC8561_ACDRV_MANUAL_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0B, SC8561_ACDRV_MANUAL_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write ACDRV reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_wpcgate(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_WPCGATE_ENABLE;
	else
		val = SC8561_WPCGATE_DISABLE;

	val <<= SC8561_WPCGATE_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0B, SC8561_WPCGATE_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write WPCGATE reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_ovpgate(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_OVPGATE_ENABLE;
	else
		val = SC8561_OVPGATE_DISABLE;

	val <<= SC8561_OVPGATE_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0B, SC8561_OVPGATE_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write OVPGATE reg", chip->log_tag);

	return ret;
}

static int sc8561_set_sense_resistor(struct sc8561_device *chip, int r_mohm)
{
	unsigned int val = 0;
	int ret = 0;

	if (r_mohm == 1)
		val = SC8561_IBAT_SNS_RES_1MHM;
	else if (r_mohm == 2)
		val = SC8561_IBAT_SNS_RES_2MHM;
	else
		return -1;

	val <<= SC8561_IBAT_SNS_RES_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0E, SC8561_IBAT_SNS_RES_MASK, val);
	if (ret)
		xmc_err("%s failed to write IBAT_SNS reg", chip->log_tag);

	return ret;
}

static int sc8561_set_ss_timeout(struct sc8561_device *chip, int timeout)
{
	unsigned int val = 0;
	int ret = 0;

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
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0D, SC8561_SS_TIMEOUT_SET_MASK, val);
	if (ret)
		xmc_err("%s failed to write SS_TIMEOUT reg", chip->log_tag);

	return ret;
}

static int sc8561_set_batovp_alarm_int_mask(struct sc8561_device *chip, u8 mask)
{
	unsigned int val = 0;
	int ret = 0;

	ret = regmap_read(chip->regmap, SC8561_REG_6C, &val);
	if (ret)
		return ret;

	val |= (mask << SC8561_BAT_OVP_ALM_MASK_SHIFT);
	ret = regmap_write(chip->regmap, SC8561_REG_6C, val);
	if (ret)
		xmc_err("%s failed to write BATOVP_ALARM_INT_MASK reg", chip->log_tag);

	return ret;
}

static int sc8561_set_busocp_alarm_int_mask(struct sc8561_device *chip, u8 mask)
{
	unsigned int val = 0;
	int ret = 0;

	ret = regmap_read(chip->regmap, SC8561_REG_6D, &val);
	if (ret)
		return ret;

	val |= (mask << SC8561_BUS_OCP_ALM_MASK_SHIFT);
	ret = regmap_write(chip->regmap, SC8561_REG_6D, val);
	if (ret)
		xmc_err("%s failed to write BUSOCP_ALARM_INT_MASK reg", chip->log_tag);

	return ret;
}

static int sc8561_set_ucp_fall_dg(struct sc8561_device *chip, u8 date)
{
	unsigned int val = 0;
	int ret = 0;

	val = date;
	val <<= SC8561_BUS_UCP_FALL_DG_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_07, SC8561_BUS_UCP_FALL_DG_MASK, val);
	if (ret)
		xmc_err("%s failed to write USB_FALL_DG reg", chip->log_tag);

	return ret;
}

static int sc8561_enable_tsbat(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_TSBAT_ENABLE;
	else
		val = SC8561_TSBAT_DISABLE;

	val <<= SC8561_TSBAT_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_70, SC8561_TSBAT_EN_MASK, val);
	if (ret)
		xmc_err("%s failed to write TSBAT reg", chip->log_tag);

	return ret;
}

static int sc8561_set_sync(struct sc8561_device *chip, u8 date)
{
	unsigned int val = 0;
	int ret = 0;

	val = date;
	val <<= SC8561_SYNC_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_0C, SC8561_SYNC_MASK, val);
	if (ret)
		xmc_err("%s failed to write SYNC reg", chip->log_tag);

	return ret;
}

static int sc8561_set_acdrv_up(struct sc8561_device *chip, bool enable)
{
	unsigned int val = 0;
	int ret = 0;

	if (enable)
		val = SC8561_ACDRV_UP_ENABLE;
	else
		val = SC8561_ACDRV_UP_DISABLE;

	val <<= SC8561_ACDRV_UP_SHIFT;
	ret = regmap_update_bits(chip->regmap, SC8561_REG_7C, SC8561_ACDRV_UP_MASK, val);
	if (ret)
		xmc_err("failed to write ACDRV_UP reg");

	return ret;
}

static int sc8561_init_int_src(struct sc8561_device *chip)
{
	int ret = 0;

	ret = sc8561_set_batovp_alarm_int_mask(chip, SC8561_BAT_OVP_ALM_NOT_MASK);
	if (ret)
		xmc_info("%s failed to set BATOVP alarm mask\n", chip->log_tag);

	ret = sc8561_set_busocp_alarm_int_mask(chip, SC8561_BUS_OCP_ALM_NOT_MASK);
	if (ret)
		xmc_info("%s failed to set BUSOCP  alarm mask\n", chip->log_tag);

	return ret;
}

static void sc8561_dump_register(struct sc8561_device *chip)
{
	unsigned int data[100];
	int i = 0, reg_num = ARRAY_SIZE(sc8561_reg_list);

	for (i = 0; i < reg_num; i++) {
		if (i == 0)
			printk(KERN_CONT "[XMCHG] %s REG: ", chip->log_tag);
		printk(KERN_CONT "0x%02x ", sc8561_reg_list[i]);
		if (i == reg_num - 1)
			printk(KERN_CONT "\n");
	}

	for (i = 0; i < reg_num; i++)
		regmap_read(chip->regmap, sc8561_reg_list[i], &data[i]);

	for (i = 0; i < reg_num; i++) {
		if (i == 0)
			printk(KERN_CONT "[XMCHG] %s VAL: ", chip->log_tag);
		printk(KERN_CONT "0x%02x ", data[i]);
		if (i == reg_num - 1)
			printk(KERN_CONT "\n");
	}
}

static int sc8561_init_protection(struct sc8561_device *chip, int forward_work_mode)
{
	int ret = 0;

	ret = sc8561_enable_batovp(chip, true);
	ret = sc8561_enable_batocp(chip, false);
	ret = sc8561_enable_busocp(chip, true);
	ret = sc8561_enable_busucp(chip, true);
	ret = sc8561_enable_pmid2outovp(chip, true);
	ret = sc8561_enable_pmid2outuvp(chip, true);
	ret = sc8561_enable_batovp_alarm(chip, false);
	ret = sc8561_enable_busocp_alarm(chip, false);
	ret = sc8561_set_batovp(chip, 4650);
	ret = sc8561_set_batocp(chip, 8000);
	ret = sc8561_set_batovp_alarm_th(chip, 4600);
	if (forward_work_mode == CP_FORWARD_4_TO_1) {
		ret = sc8561_set_busovp(chip, 22000);
		ret = sc8561_set_busocp(chip, 3750);
		ret = sc8561_set_usbovp(chip, 22000);
	} else if (forward_work_mode == CP_FORWARD_2_TO_1) {
		ret = sc8561_set_busovp(chip, 11000);
		ret = sc8561_set_busocp(chip, 3750);
		ret = sc8561_set_usbovp(chip, 14000);
	} else {
		//To do later for forward 1:1 mode
		;
	}
	//ret = sc8561_set_busovp(chip, chip->bus_ovp_threshold);
	//ret = sc8561_set_busocp(chip, chip->bus_ocp_threshold);
	//ret = sc8561_set_usbovp(chip, 22000);
	ret = sc8561_set_wpcovp(chip, 22000);
	ret = sc8561_set_outovp(chip, 5000);
	ret = sc8561_set_pmid2outuvp_th(chip, 100);
	ret = sc8561_set_pmid2outovp_th(chip, 600);
	return ret;
}

static int sc8561_init_adc(struct sc8561_device *chip)
{
	sc8561_set_adc_scanrate(chip, false);
	sc8561_set_adc_scan(chip, ADC_IBUS, true);
	sc8561_set_adc_scan(chip, ADC_VBUS, true);
	sc8561_set_adc_scan(chip, ADC_VUSB, true);
	sc8561_set_adc_scan(chip, ADC_VWPC, true);
	sc8561_set_adc_scan(chip, ADC_VOUT, true);
	sc8561_set_adc_scan(chip, ADC_VBAT, true);
	sc8561_set_adc_scan(chip, ADC_IBAT, true);
	sc8561_set_adc_scan(chip, ADC_TBAT, true);
	sc8561_set_adc_scan(chip, ADC_TDIE, true);
	sc8561_enable_adc(chip, false);

	return 0;
}

static int sc8561_init_device(struct sc8561_device *chip, int driver_data)
{
	int ret = 0;

	ret = sc8561_enable_parallel_func(chip, false);

	if (driver_data == SC8561_MASTER)
		ret = sc8561_enable_config_func(chip, true);
	else if (driver_data == SC8561_SLAVE)
		ret = sc8561_enable_parallel_func(chip, false);

//	ret = sc8561_set_wdt(chip, 0);
	ret = sc8561_enable_acdrv_manual(chip, true);
	ret = sc8561_enable_wpcgate(chip, true);
	ret = sc8561_enable_ovpgate(chip, true);
	ret = sc8561_set_ss_timeout(chip, 5120);
	ret = sc8561_set_ucp_fall_dg(chip, SC8561_BUS_UCP_FALL_DG_5MS);
	ret = sc8561_enable_tsbat(chip, false);
	ret = sc8561_set_sync(chip, SC8561_SYNC_NO_SHIFT);
	ret = sc8561_set_acdrv_up(chip, true);
	ret = sc8561_set_sense_resistor(chip, 1);
	ret = sc8561_set_ovpgate_on_dg_set(chip, 20);
	ret = sc8561_init_protection(chip, CP_FORWARD_4_TO_1);
	ret = sc8561_init_adc(chip);
	ret = sc8561_init_int_src(chip);
	ret = sc8561_set_operation_mode(chip, SC8561_FORWARD_4_1_CHARGER_MODE);
	ret = sc8561_set_busovp(chip, 22000);

	return ret;
}

static int sc8561_ops_charge_enable(struct xmc_device *xmc_dev, bool enable)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	int ret = 0;

	ret = sc8561_enable_charge(chip, enable);
	if (ret)
		xmc_info("%s failed to enable charge\n", chip->log_tag);

	return ret;
}

static int sc8561_ops_get_charge_enable(struct xmc_device *xmc_dev, bool *enabled)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	int ret = 0;

	ret = sc8561_get_charge_enabled(chip, enabled);
	if (ret)
		xmc_info("%s failed to get charge_enable\n", chip->log_tag);

	return ret;
}

static int sc8561_ops_get_vbus(struct xmc_device *xmc_dev, int *value)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	u32 vbus = 0;
	int ret = 0;

	ret = sc8561_get_adc(chip, ADC_VBUS, &vbus);
	if (ret) {
		xmc_err("failed to get ADC_VBUS\n");
		vbus = 0;
	}

	*value = (int)vbus;
	return ret;
}

static int sc8561_ops_get_ibus(struct xmc_device *xmc_dev, int *value)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	u32 ibus = 0;
	int ret = 0;

	ret = sc8561_get_adc(chip, ADC_IBUS, &ibus);
	if (ret) {
		xmc_err("failed to get ADC_VBUS\n");
		ibus = 0;
	}

	*value = (int)ibus;
	return ret;
}

static int sc8561_ops_set_div_mode(struct xmc_device *xmc_dev, enum xmc_cp_div_mode mode)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	int ret = 0;

	switch (mode) {
	case XMC_CP_1T1:
		ret = sc8561_set_operation_mode(chip, SC8561_FORWARD_1_1_CHARGER_MODE);
		break;
	case XMC_CP_2T1:
		ret = sc8561_set_operation_mode(chip, SC8561_FORWARD_2_1_CHARGER_MODE);
		break;
	case XMC_CP_4T1:
		ret = sc8561_set_operation_mode(chip, SC8561_FORWARD_4_1_CHARGER_MODE);
		break;
	default:
		xmc_err("%s set div_mode is unsupported\n", chip->log_tag);
	}

	return ret;
}

static int sc8561_ops_get_div_mode(struct xmc_device *xmc_dev, enum xmc_cp_div_mode *mode)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	int value = 0, ret = 0;

	ret = sc8561_get_operation_mode(chip, &value);
	if (ret)
		xmc_err("%s failed to get div_mode\n", chip->log_tag);

	switch (value) {
	case SC8561_FORWARD_1_1_CHARGER_MODE:
		*mode = XMC_CP_1T1;
		break;
	case SC8561_FORWARD_2_1_CHARGER_MODE:
		*mode = XMC_CP_2T1;
		break;
	case SC8561_FORWARD_4_1_CHARGER_MODE:
		*mode = XMC_CP_4T1;
		break;
	default:
		xmc_err("%s get div_mode is unsupported\n", chip->log_tag);
	}

	return ret;
}

static int sc8561_ops_adc_enable(struct xmc_device *xmc_dev, bool enable)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	int ret = 0;

	ret = sc8561_enable_adc(chip, enable);
	if (ret)
		xmc_err("%s failed to enable ADC\n", chip->log_tag);

	return ret;
}

static int sc8561_ops_device_init(struct xmc_device *xmc_dev, enum xmc_cp_div_mode mode)
{
	struct sc8561_device *chip = xmc_ops_get_data(xmc_dev);
	int ret = 0;

	switch (mode) {
	case XMC_CP_1T1:
		ret = sc8561_init_protection(chip, SC8561_FORWARD_1_1_CHARGER_MODE);
		break;
	case XMC_CP_2T1:
		ret = sc8561_init_protection(chip, SC8561_FORWARD_2_1_CHARGER_MODE);
		break;
	case XMC_CP_4T1:
		ret = sc8561_init_protection(chip, SC8561_FORWARD_4_1_CHARGER_MODE);
		break;
	default:
		xmc_err("%s init device div_mode is unsupported\n", chip->log_tag);
	}

	return ret;
}

static const struct xmc_ops sc8561_ops = {
	.charge_enable = sc8561_ops_charge_enable,
	.get_charge_enable = sc8561_ops_get_charge_enable,
	.get_vbus = sc8561_ops_get_vbus,
	.get_ibus = sc8561_ops_get_ibus,
	.set_div_mode = sc8561_ops_set_div_mode,
	.get_div_mode = sc8561_ops_get_div_mode,
	.adc_enable = sc8561_ops_adc_enable,
	.device_init = sc8561_ops_device_init,
};

static int sc8561_register_charger(struct sc8561_device *chip, int driver_data)
{
	switch (driver_data) {
	case SC8561_SLAVE:
		chip->xmc_dev = xmc_device_register("cp_slave", &sc8561_ops, chip);
		break;
	case SC8561_MASTER:
		chip->xmc_dev = xmc_device_register("cp_master", &sc8561_ops, chip);
		break;
	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static void sc8561_irq_handler(struct work_struct *work)
{
	struct sc8561_device *chip = container_of(work, struct sc8561_device, irq_handle_work.work);

	sc8561_dump_register(chip);
	__pm_relax(chip->irq_wakelock);

	return;
}

static irqreturn_t sc8561_interrupt(int irq, void *data)
{
	struct sc8561_device *chip = data;

	xmc_info("%s sc8561_interrupt\n", chip->log_tag);

	if (chip->irq_wakelock->active)
		return IRQ_HANDLED;

	__pm_stay_awake(chip->irq_wakelock);
	schedule_delayed_work(&chip->irq_handle_work, 0);

	return IRQ_HANDLED;
}

static int sc8561_parse_dt(struct sc8561_device *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret = 0;

	if (!np) {
		xmc_err("%s device tree info missing\n", chip->log_tag);
		return -1;
	}

	chip->irq_gpio = of_get_named_gpio(np, "sc8561_irq_gpio", 0);
	if (!gpio_is_valid(chip->irq_gpio)) {
		xmc_err("%s failed to parse sc8561_irq_gpio\n", chip->log_tag);
		return -1;
	}

	return ret;
}

static int sc8561_init_irq(struct sc8561_device *chip)
{
	int ret = 0;

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, dev_name(chip->dev));
	if (ret < 0) {
		xmc_err("%s failed to request gpio\n", chip->log_tag);
		return -1;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq < 0) {
		xmc_err("%s failed to get gpio_irq\n", chip->log_tag);
		return -1;
	}

	ret = request_irq(chip->irq, sc8561_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(chip->dev), chip);
	if (ret < 0) {
		xmc_err("%s failed to request irq\n", chip->log_tag);
		return -1;
	}

	enable_irq_wake(chip->irq);

	return 0;
}

static enum power_supply_property sc8561_power_supply_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static int sc8561_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct sc8561_device *chip = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 data = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->chip_ok;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sc8561_get_adc(chip, ADC_VBUS, &data);
		if (ret)
			val->intval = 0;
		else
			val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sc8561_get_adc(chip, ADC_IBUS, &data);
		if (ret)
			val->intval = 0;
		else
			val->intval = data;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = sc8561_get_adc(chip, ADC_TDIE, &data);
		if (ret)
			val->intval = 0;
		else
			val->intval = data;
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0) {
		xmc_info("%s failed to get prop value = %d\n", chip->log_tag, psp);
		return ret;
	}
	return 0;
}

static int sc8561_psy_init(struct sc8561_device *chip, struct device *dev, int driver_data)
{
	struct power_supply_config psy_cfg = {};

	switch (driver_data) {
	case SC8561_MASTER:
		chip->psy_desc.name = "cp_master";
		break;
	case SC8561_SLAVE:
		chip->psy_desc.name = "cp_slave";
		break;
	default:
		return -EINVAL;
	}

	psy_cfg.drv_data = chip;
	psy_cfg.of_node = chip->dev->of_node;
	chip->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	chip->psy_desc.properties = sc8561_power_supply_props,
	chip->psy_desc.num_properties = ARRAY_SIZE(sc8561_power_supply_props),
	chip->psy_desc.get_property = sc8561_get_property,

	chip->cp_psy = devm_power_supply_register(chip->dev, &chip->psy_desc, &psy_cfg);
	if (IS_ERR(chip->cp_psy))
		return -EINVAL;

	return 0;
}

static int sc8561_detect_device(struct sc8561_device *chip, int driver_data)
{
	int retry_count = 0, ret = 0;
	unsigned int data = 0;

retry:
	ret = regmap_read(chip->regmap, SC8561_REG_6E, &data);
	if (ret < 0) {
		xmc_err("%s failed to read device_id, retry = %d\n", chip->log_tag, retry_count);
		retry_count++;
		if (retry_count < 3) {
			msleep(250);
			goto retry;
		} else {
			return ret;
		}
	}

	xmc_info("%s device_id = 0x%02x\n", chip->log_tag, data);
	if (data == SC8561_DEVICE_ID) {
		if (driver_data == SC8561_MASTER)
			strcpy(chip->log_tag, "[XMC_CP_SC8561_MASTER]");
		else if (driver_data == SC8561_SLAVE)
			strcpy(chip->log_tag, "[XMC_CP_SC8561_SLAVE]");
	} else {
		xmc_err("%s device_id is invalid\n", chip->log_tag);
		return -1;
	}

	return ret;
}

static int sc8561_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc8561_device *chip;
	int ret = 0;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = dev;
	strcpy(chip->log_tag, "[CP_UNKNOWN]");
	chip->irq_wakelock = wakeup_source_register(NULL, "sc8561_irq_wakelock");

	i2c_set_clientdata(client, chip);
   	chip->regmap = devm_regmap_init_i2c(client, &sc8561_regmap_config);
	if (IS_ERR(chip->regmap)) {
		return PTR_ERR(chip->regmap);
	}

	ret = sc8561_detect_device(chip, id->driver_data);
	if (ret) {
		xmc_err("%s failed to detect device\n", chip->log_tag);
		return ret;
	}

	ret = sc8561_init_device(chip, id->driver_data);
	if (ret)
		xmc_err("%s failed to init device\n", chip->log_tag);

	ret = sc8561_parse_dt(chip);
	if (ret) {
		xmc_info("%s failed to parse DTS\n", chip->log_tag);
		return ret;
	}

	ret = sc8561_init_irq(chip);
	if (ret) {
		xmc_info("%s failed to int irq\n", chip->log_tag);
		return ret;
	}

	ret = sc8561_register_charger(chip, id->driver_data);
	if (ret) {
		xmc_info("%s failed to register charger\n", chip->log_tag);
		return ret;
	}

	ret = sc8561_psy_init(chip, dev, id->driver_data);
	if (ret) {
		xmc_info("%s failed to init psy\n", chip->log_tag);
		return ret;
	}

	INIT_DELAYED_WORK(&chip->irq_handle_work, sc8561_irq_handler);

	chip->chip_ok = true;
	xmc_info("%s probe success\n", chip->log_tag);
	return 0;
}

static int sc8561_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8561_device *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = sc8561_enable_adc(chip, false);
	if (ret)
		xmc_info("%s failed to disable ADC\n", chip->log_tag);

	xmc_info("%s sc8561 suspend!\n", chip->log_tag);

	return enable_irq_wake(chip->irq);
}

static int sc8561_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8561_device *chip = i2c_get_clientdata(client);

	xmc_info("%s sc8561 resume!\n", chip->log_tag);

	return disable_irq_wake(chip->irq);
}

static const struct dev_pm_ops sc8561_pm_ops = {
	.suspend	= sc8561_suspend,
	.resume		= sc8561_resume,
};

static int sc8561_remove(struct i2c_client *client)
{
	struct sc8561_device *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = sc8561_enable_adc(chip, false);
	if (ret)
		xmc_info("%s failed to disable ADC\n", chip->log_tag);

	power_supply_unregister(chip->cp_psy);
	return ret;
}

static void sc8561_shutdown(struct i2c_client *client)
{
	struct sc8561_device *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = sc8561_enable_adc(chip, false);
	if (ret)
		xmc_info("%s failed to disable ADC\n", chip->log_tag);

	xmc_info("%s sc8561 shutdown!\n", chip->log_tag);
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
		.name = "sc8561_charger",
		.of_match_table = sc8561_of_match,
		.pm = &sc8561_pm_ops,
	},
	.id_table = sc8561_i2c_ids,
	.probe = sc8561_probe,
	.remove = sc8561_remove,
	.shutdown = sc8561_shutdown,
};

bool sc8561_init(void)
{
	if (i2c_add_driver(&sc8561_driver))
		return false;
	else
		return true;
}

