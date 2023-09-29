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
#include "ln8410.h"

enum ln8410_work_mode {
    LN8410_STANDALONE,
    LN8410_MASTER,
	LN8410_SLAVE,
};

enum ln8410_adc_channel {
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

enum xmc_cp_div_mode {
	XMC_CP_4T1,
	XMC_CP_2T1,
	XMC_CP_1T1,
};

typedef struct ln8410_flag_regs {
	unsigned int int_flag;
	unsigned int flt_flag;
	unsigned int comp_flag0;
	unsigned int comp_stat0;
	unsigned int comp_flag1;
} LN8410_FLAGS;

static struct regmap_config ln8410_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFF,
};

struct ln8410_device {
	struct i2c_client *client;
	struct device *dev;
	struct charger_device *chg_dev;
	struct power_supply *cp_psy;
	struct power_supply_desc psy_desc;
	struct regmap *regmap;
	bool chip_ok;
	char log_tag[25];

	u8 adc_mode;
	unsigned int revision;
	unsigned int product_cfg;

	struct delayed_work irq_handle_work;
	struct wakeup_source *irq_wakelock;
	int irq_gpio;
	int irq;
	int chip_vendor;
};

struct mtk_cp_sysfs_field_info {
	struct device_attribute attr;
	enum cp_property prop;
	int (*set)(struct ln8410_device *chip,
		struct mtk_cp_sysfs_field_info *attr, int val);
	int (*get)(struct ln8410_device *chip,
		struct mtk_cp_sysfs_field_info *attr, int *val);
};


static unsigned int ln8410_reg_list_a[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x11, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
};

static unsigned int ln8410_reg_list_b[] = {
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x29, 0x2A, 0x2B, 0x2C,0x2D, 0x2E, 0x30, 0x31, 0x52, 0x54,
	0x59, 0x60, 0x61, 0x62, 0x63, 0x69, 0x76, 0x79, 0x7B, 0x7E, 0x80, 0x8D, 0x98, 0x99, 0x9A, 0x9B, 0x9C,
};

static void ln8410_dump_register(struct ln8410_device *chip);
static int ln8410_get_div_mode(struct ln8410_device *chip, int *mode);

static int ln8410_set_regn_pull_down(struct ln8410_device *chip, bool enable)
{
	int ret = 0;

	ret = regmap_write(chip->regmap, LN8410_REG_LION_CTRL, 0xAA);
	if (enable) {
		ret = regmap_update_bits(chip->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x80, 0x80);   /* FORCE_VWPC_PD = 1 */
		ret = regmap_update_bits(chip->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x40, 0x00);   /* TM_VWPC_PD = 0 */
		ret = regmap_update_bits(chip->regmap,  LN8410_REG_FORCE_SC_MISC, 0x04, 0x04);      /* TM_REGN_PD = 1 */
	} else {
		ret = regmap_update_bits(chip->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x80, 0x00);   /* FORCE_VWPC_PD = 0 */
		ret = regmap_update_bits(chip->regmap,  LN8410_REG_TEST_MODE_CTRL_2, 0x40, 0x00);   /* TM_VWPC_PD = 0 */
		ret = regmap_update_bits(chip->regmap,  LN8410_REG_FORCE_SC_MISC, 0x04, 0x00);      /* TM_REGN_PD = 0 */
	}
	ret = regmap_write(chip->regmap, LN8410_REG_LION_CTRL, 0x00);

	xmc_info("%s REGN_PD=%d\n", chip->log_tag, enable);
	return ret;
}

static int ln8410_set_adc_mode(struct ln8410_device *chip, u8 mode)
{
	int ret = 0;

	if (mode == 0x00) {
		/* adc one-shot mode disable all channel */ 
		/* the channel will be enable when request read adc */
		ret = regmap_write(chip->regmap, LN8410_REG_14, 0x49);
		ret = regmap_write(chip->regmap, LN8410_REG_ADC_FN_DISABLE1, 0xFF);
		ret = regmap_update_bits(chip->regmap, LN8410_REG_ADC_CTRL2, 0xC0, 0xC0);/* normal mode */
		ret = regmap_update_bits(chip->regmap, LN8410_REG_LION_CFG_1, 0x11, 0x10);/* ADC_SKIP_IDLE=1, OSR=64 */
		xmc_info("%s config : ADC ONESHOT mode\n", chip->log_tag);
		chip->adc_mode = mode;
	//} else if ((mode == 0x01) && (chip->adc_mode != 0x01)) {
		} else if (mode == 0x01) {
		/* adc continuous mode enable all(selected) channel */
		ret = regmap_write(chip->regmap, LN8410_REG_14, 0x88);
		ret = regmap_write(chip->regmap, LN8410_REG_ADC_FN_DISABLE1, 0x02);/* disable TSBAT */
		ret = regmap_update_bits(chip->regmap, LN8410_REG_ADC_CTRL2, 0xC0, 0xC0);/* normal mode */
		ret = regmap_update_bits(chip->regmap, LN8410_REG_LION_CFG_1, 0x11, 0x01);/* ADC_SKIP_IDLE=0, OSR=128 */
		xmc_info("%s config : ADC CONTINUOUS mode\n", chip->log_tag);
		chip->adc_mode = mode;
		msleep(500); // 500MS
	} else {
		xmc_info("%s config : ADC invalid mode\n", chip->log_tag);
	}

	xmc_info("%s setup adc_mode=%d\n", chip->log_tag, chip->adc_mode);
	return ret;
}

static int ln8410_enable_qb(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_QB_ENABLE;
	else
		val = LN8410_QB_DISABLE;
	val <<= LN8410_QB_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0A, LN8410_QB_EN_MASK, val);

	return ret;
}


static ln8410_set_infet_shdn_cfg(struct ln8410_device *chip, bool enable)
{
	int ret = 0;;

	regmap_update_bits(chip->regmap, LN8410_LION_CTRL, 0xFF, 0xAA);
	ret = regmap_update_bits(chip->regmap, LN8410_REG_OVPGATE_CTRL_2,
				BIT(LN8410_INFET_SHDN_CFG_SHIFT),
				enable << LN8410_INFET_SHDN_CFG_SHIFT);
	regmap_update_bits(chip->regmap, LN8410_LION_CTRL, 0xFF, 0x00);

	return ret;
}

static  ln8410_set_bst_live_mask(struct ln8410_device * chip, bool enable)
{
	int ret;

	regmap_update_bits(chip->regmap, LN8410_LION_CTRL, 0xFF, 0xAA);
	ret = regmap_update_bits(chip->regmap, LN8410_BST_CP_PD_CFG,
				BIT(LN8410_BST_LIVE_MASK_SHIFT),
				enable << LN8410_BST_LIVE_MASK_SHIFT);
	regmap_update_bits(chip->regmap, LN8410_LION_CTRL, 0xFF, 0x00);

	return ret;
}

static int ln8410_pause_int(struct ln8410_device * chip, bool enable)
{
	int  status;

	if (enable) {
		/* pause interrupt update, ready to read interrupts */
		regmap_update_bits(chip->regmap, LN8410_LION_CTRL, 0xFF, 0x00);
		status = regmap_update_bits(chip->regmap, LN8410_REG_INT_MASK_2,
						LN8410_PAUSE_INT_MASK,
						(0x1 << LN8410_PAUSE_INT_SHIFT));
	} else {
		/* clear interrupts */
		regmap_update_bits(chip->regmap, LN8410_REG_INT_MASK_2,
					LN8410_CLEAR_UNT_MASK,
					(0x1 << LN8410_CLEAR_INT_SHIFT));

		msleep(5);

		regmap_update_bits(chip->regmap, LN8410_REG_INT_MASK_2,
					LN8410_CLEAR_UNT_MASK,
					(0x0 << LN8410_CLEAR_INT_SHIFT));

		/* resume interrupt update */
		status = regmap_update_bits(chip->regmap, LN8410_REG_INT_MASK_2,
						LN8410_PAUSE_INT_MASK,
						(0x0 << LN8410_PAUSE_INT_SHIFT));
	}

	return status;
}

static int ln8410_read_flags(struct ln8410_device * chip, LN8410_FLAGS *flags)
{
	int  status;
	unsigned int data;

	ln8410_pause_int(chip, true);

	status = regmap_read(chip->regmap, LN8410_REG_10, &data);
	if (status)
		return status;
	if (flags)
		flags->int_flag = data;
	status = regmap_read(chip->regmap, LN8410_REG_12, &data);
	if (status)
		return status;
	if (flags)
		flags->flt_flag = data;
	status = regmap_read(chip->regmap, LN8410_REG_COMP_FLAG0, &data);
	if (status)
		return status;
	if (flags)
		flags->comp_flag0 = data;
	status = regmap_read(chip->regmap, LN8410_REG_COMP_STAT0, &data);
	if (status)
		return status;
	if (flags)
		flags->comp_stat0 = data;
	status = regmap_read(chip->regmap, LN8410_REG_COMP_FLAG1, &data);
	if (status)
		return status;
	if (flags)
		flags->comp_flag1 = data;

	ln8410_pause_int(chip, false);

	return 0;
}

static int ln8410_enable_charge(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;
	int value = 0;

	ln8410_set_bst_live_mask(chip, 1);

	val = LN8410_CHG_DISABLE;
	val <<= LN8410_CHG_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0A, LN8410_CHG_EN_MASK, val);
	ret = ln8410_enable_qb(chip, false);
	msleep(20);
	ln8410_set_infet_shdn_cfg(chip, 0);

	xmc_info("%s LN enable_charge = %d\n", chip->log_tag, enable);
	if (enable){
		//read and clear flag registers
		ln8410_read_flags(chip, NULL);
		ret = ln8410_get_div_mode(chip, &value);
		if (ret)
			xmc_err("%s failed to get div_mode\n", chip->log_tag);
		if(value == LN8410_FORWARD_1_1_CHARGER_MODE)
			ln8410_set_infet_shdn_cfg(chip, 1);

		ret = ln8410_enable_qb(chip, enable);
		msleep(20);

		if(value == LN8410_FORWARD_1_1_CHARGER_MODE)
			ln8410_set_bst_live_mask(chip, 0);
		val = LN8410_CHG_ENABLE;
		val <<= LN8410_CHG_EN_SHIFT;
		ret = regmap_update_bits(chip->regmap, LN8410_REG_0A,
				    LN8410_CHG_EN_MASK, val);

		/*Don't move it up, continue mode setting is best after charging enabled*/
		ret = ln8410_set_adc_mode(chip, 0x01);
	} else {
		ret = ln8410_enable_qb(chip, enable);
		/*Don't move it down, oneshot mode setting is best before charging disabled*/
		ret = ln8410_set_adc_mode(chip, 0x01);
	}

	return ret;
}

static int ln8410_get_charge_enable(struct ln8410_device *chip, bool *enabled)
{
	int ret = 0;
	unsigned int val = 0;

	ret = regmap_read(chip->regmap, LN8410_REG_98, &val);
	*enabled = !(val & (LN8410_SHUTDOWN_STS_MASK | LN8410_STANDBY_STS_MASK));

	return ret;
}

static int ln8410_set_div_mode(struct ln8410_device *chip, int mode)
{
	unsigned int val = mode;
	int ret = 0;

	val <<= LN8410_MODE_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0D, LN8410_MODE_MASK, val);

	return ret;
}

static int ln8410_get_div_mode(struct ln8410_device *chip, int *mode)
{
	int ret = 0;
	unsigned int val = 0;

	ret = regmap_read(chip->regmap, LN8410_REG_0D, &val);

	*mode = (val & LN8410_MODE_MASK);
	return ret;
}

static int ln8410_enable_batovp(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_BAT_OVP_ENABLE;
	else
		val = LN8410_BAT_OVP_DISABLE;
	val <<= LN8410_BAT_OVP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_01, LN8410_BAT_OVP_DIS_MASK, val);

	return ret;
}

static int ln8410_set_batovp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold < LN8410_BAT_OVP_BASE)
		threshold = LN8410_BAT_OVP_BASE;
	val = (threshold - LN8410_BAT_OVP_BASE) / LN8410_BAT_OVP_LSB;
	val <<= LN8410_BAT_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_01, LN8410_BAT_OVP_MASK, val);

	return ret;
}

static int ln8410_enable_batocp(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_BAT_OCP_ENABLE;
	else
		val = LN8410_BAT_OCP_DISABLE;
	val <<= LN8410_BAT_OCP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_02, LN8410_BAT_OCP_DIS_MASK, val);

	return ret;
}

static int ln8410_set_batocp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold < LN8410_BAT_OCP_BASE)
		threshold = LN8410_BAT_OCP_BASE;
	val = (threshold - LN8410_BAT_OCP_BASE) / LN8410_BAT_OCP_LSB;
	val <<= LN8410_BAT_OCP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_02, LN8410_BAT_OCP_MASK, val);

	return ret;
}

static int ln8410_set_usbovp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold == 6500)
		val = LN8410_USB_OVP_6PV5;
	else
		val = (threshold - LN8410_USB_OVP_BASE) / LN8410_USB_OVP_LSB;
	val <<= LN8410_USB_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_03, LN8410_USB_OVP_MASK, val);

	return ret;
}

static int ln8410_set_wpcovp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold == 6500)
		val = LN8410_WPC_OVP_6PV5;
	else
		val = (threshold - LN8410_WPC_OVP_BASE) / LN8410_WPC_OVP_LSB;
	val <<= LN8410_WPC_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_04, LN8410_WPC_OVP_MASK, val);

	return ret;
}

static int ln8410_set_busovp(struct ln8410_device *chip, bool threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold)
		val = LN8410_VBUS_OVP_HIGH_SET;
	else
		val = LN8410_VBUS_OVP_LOW_SET;
	val <<= LN8410_VBUS_OVP_SET_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0D, LN8410_VBUS_OVP_SET_MASK, val);

	return ret;
}

static int ln8410_set_outovp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold < LN8410_OUT_OVP_BASE)
		threshold = LN8410_OUT_OVP_BASE;
	val = (threshold - LN8410_OUT_OVP_BASE) / LN8410_OUT_OVP_LSB;
	val <<= LN8410_OUT_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_79, LN8410_OUT_OVP_MASK, val);

	return ret;
}

static int ln8410_enable_busocp(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_BUS_OCP_ENABLE;
	else
		val = LN8410_BUS_OCP_DISABLE;
	val <<= LN8410_BUS_OCP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_05, LN8410_BUS_OCP_DIS_MASK, val);

	return ret;
}
static int ln8410_set_busocp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (chip->revision == 0x2 && chip->product_cfg == 0x2)
		threshold = threshold * 3 / 2;

	if (threshold < LN8410_BUS_OCP_BASE)
		threshold = LN8410_BUS_OCP_BASE;
	val = (threshold - LN8410_BUS_OCP_BASE) / LN8410_BUS_OCP_LSB;
	val <<= LN8410_BUS_OCP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_05, LN8410_BUS_OCP_MASK, val);

	return ret;
}

static int ln8410_enable_busucp(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_BUS_UCP_ENABLE;
	else
		val = LN8410_BUS_UCP_DISABLE;
	val <<= LN8410_BUS_UCP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_06, LN8410_BUS_UCP_DIS_MASK, val);

	return ret;
}

static int ln8410_set_busucp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold < LN8410_IBUS_UC_BASE)
		threshold = LN8410_IBUS_UC_BASE;
	val = (threshold - LN8410_IBUS_UC_BASE) / LN8410_IBUS_UC_LSB;
	val <<= LN8410_IBUS_UC_CFG_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_7C, LN8410_IBUS_UC_CFG_MASK, val);

	return ret;
}

static int ln8410_enable_pmid2outovp(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_PMID2OUT_OVP_ENABLE;
	else
		val = LN8410_PMID2OUT_OVP_DISABLE;
	val <<= LN8410_PMID2OUT_OVP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_07, LN8410_PMID2OUT_OVP_DIS_MASK, val);

	return ret;
}

static int ln8410_set_pmid2outovp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold < LN8410_PMID2OUT_OVP_BASE)
		threshold = LN8410_PMID2OUT_OVP_BASE;
	val = (threshold - LN8410_PMID2OUT_OVP_BASE) / LN8410_PMID2OUT_OVP_LSB;
	val <<= LN8410_PMID2OUT_OVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_07, LN8410_PMID2OUT_OVP_MASK, val);

	return ret;
}

static int ln8410_enable_pmid2outuvp(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_PMID2OUT_UVP_ENABLE;
	else
		val = LN8410_PMID2OUT_UVP_DISABLE;
	val <<= LN8410_PMID2OUT_UVP_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_08, LN8410_PMID2OUT_UVP_DIS_MASK, val);

	return ret;
}

static int ln8410_set_pmid2outuvp(struct ln8410_device *chip, int threshold)
{
	int ret = 0;
	u8 val = 0;

	if (threshold < LN8410_PMID2OUT_UVP_BASE)
		threshold = LN8410_PMID2OUT_UVP_BASE;
	val = (threshold - LN8410_PMID2OUT_UVP_BASE) / LN8410_PMID2OUT_UVP_LSB;
	val <<= LN8410_PMID2OUT_UVP_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_08, LN8410_PMID2OUT_UVP_MASK, val);

	return ret;
}

static int ln8410_enable_adc(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_ADC_ENABLE;
	else
		val = LN8410_ADC_DISABLE;
	val <<= LN8410_ADC_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_14, LN8410_ADC_EN_MASK, val);

	return ret;
}

static int ln8410_get_adc(struct ln8410_device *chip, int channel, int *result)
{
	int ret = 0;
	unsigned int val = 0, val_l = 0, val_h = 0;

	if(channel >= ADC_MAX_NUM)
		return -1;

	/*PAUSE_ADC_UPDATES=1 */
	regmap_write(chip->regmap, LN8410_REG_LION_CTRL, 0x5B);
	regmap_update_bits(chip->regmap, LN8410_REG_76, LN8410_PAUSE_ADC_UPDATES_MASK, LN8410_PAUSE_ADC_UPDATES_MASK);
	msleep(2); // 2 MS

	ret = regmap_read(chip->regmap, LN8410_REG_16 + (channel << 1), &val_h);
	ret = regmap_read(chip->regmap, LN8410_REG_16 + (channel << 1) + 1, &val_l);

	/*PAUSE_ADC_UPDATES=0 */
	regmap_update_bits(chip->regmap, LN8410_REG_76, LN8410_PAUSE_ADC_UPDATES_MASK, 0);
	regmap_write(chip->regmap, LN8410_REG_LION_CTRL, 0x00);

	val = (val_h << 8) | val_l;
	if(channel == ADC_IBUS){
		val = val * LN8410_IBUS_ADC_LSB;
		if (chip->revision == 0x2 && chip->product_cfg == 0x2)
			val =(val * 2) / 3;
	} else if (channel == ADC_VBUS)
		val = val * LN8410_VBUS_ADC_LSB;
	else if (channel == ADC_VUSB)
		val = val * LN8410_VUSB_ADC_LSB;
	else if (channel == ADC_VWPC)
		val = val * LN8410_VWPC_ADC_LSB;
	else if (channel == ADC_VOUT)
		val = val * LN8410_VOUT_ADC_LSB;
	else if (channel == ADC_VBAT)
		val = val * LN8410_VBAT_ADC_LSB;
	else if (channel == ADC_IBAT)
		val = val * LN8410_IBAT_ADC_LSB;
	else if (channel == ADC_TBAT)
		val = val * LN8410_TSBAT_ADC_LSB;
	else if (channel == ADC_TDIE)
		val = val * LN8410_TDIE_ADC_LSB;

	*result = val;
	return ret;
}

static int ln8410_enable_parallel_func(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_SYNC_FUNCTION_ENABLE;
	else
		val = LN8410_SYNC_FUNCTION_DISABLE;
	val <<= LN8410_SYNC_FUNCTION_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0D, LN8410_SYNC_FUNCTION_EN_MASK, val);

	return ret;
}

static int ln8410_enable_config_func(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_SYNC_CONFIG_MASTER;
	else
		val = LN8410_SYNC_CONFIG_SLAVE;
	val <<= LN8410_SYNC_MASTER_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0D, LN8410_SYNC_MASTER_EN_MASK, val);

	return ret;
}

static int ln8410_enable_acdrv_manual(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_ACDRV_MANUAL_MODE;
	else
		val = LN8410_ACDRV_AUTO_MODE;
	val <<= LN8410_ACDRV_MANUAL_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0A, LN8410_ACDRV_MANUAL_EN_MASK, val);

	return ret;
}

static int ln8410_enable_wpcgate(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_WPCGATE_ENABLE;
	else
		val = LN8410_WPCGATE_DISABLE;
	val <<= LN8410_WPCGATE_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0A, LN8410_WPCGATE_EN_MASK, val);

	return ret;
}

static int ln8410_enable_ovpgate(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_OVPGATE_ENABLE;
	else
		val = LN8410_OVPGATE_DISABLE;
	val <<= LN8410_OVPGATE_EN_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0A, LN8410_OVPGATE_EN_MASK, val);

	return ret;
}

static int ln8410_set_sense_resistor(struct ln8410_device *chip, u8 r_mohm)
{
	int ret = 0;
	u8 val = 0;

	if (r_mohm == 1)
		val = LN8410_IBAT_SNS_RES_1MHM;
	else if (r_mohm == 2)
		val = LN8410_IBAT_SNS_RES_2MHM;
	else
		return -1;
	val <<= LN8410_IBAT_SNS_RES_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0D, LN8410_IBAT_SNS_RES_MASK, val);

	return ret;
}

static int ln8410_set_ss_timeout(struct ln8410_device *chip, int timeout)
{
	int ret = 0;
	u8 val = 0;

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
	ret =  regmap_update_bits(chip->regmap, LN8410_REG_0C, LN8410_SS_TIMEOUT_SET_MASK, val);

	return ret;
}

static int ln8410_set_ucp_fall_dg(struct ln8410_device *chip, u8 date)
{
	int ret = 0;
	u8 val = 0;

	val = date;
	val <<= LN8410_BUS_UCP_FALL_DG_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_06, LN8410_BUS_UCP_FALL_DG_MASK, val);

	return ret;
}

static int ln8410_vbus_short_dis(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_VBUS_SHORT_DISABLE;
	else
		val = LN8410_VBUS_SHORT_ENABLE;
	val <<= LN8410_VBUS_SHORT_DIS_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_7B, LN8410_VBUS_SHORT_DIS_MASK, val);
	ret = regmap_update_bits(chip->regmap, LN8410_REG_OVPGATE_CTRL_0, LN8410_OVPGATE1_PD_CFG_MASK, 0x3 << LN8410_OVPGATE1_PD_CFG_SHIFT);
	ret = regmap_update_bits(chip->regmap, LN8410_REG_OVPGATE_CTRL_0, LN8410_OVPGATE2_PD_CFG_MASK, 0x3 << LN8410_OVPGATE2_PD_CFG_SHIFT);

	return ret;
}

static int ln8410_use_hvldo(struct ln8410_device *chip, bool enable)
{
	int ret = 0;
	u8 val = 0;

	if (enable)
		val = LN8410_USE_HVLDO;
	else
		val = LN8410_NOT_USE_HVLDO;
	val <<= LN8410_USE_HVLDO_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_52, LN8410_USE_HVLDO_MASK, val);
	return ret;
}

static int ln8410_fsw_set(struct ln8410_device *chip, u8 date)
{
	int ret = 0;
	u8 val = 0;

	val = date;
	val <<= LN8410_FSW_SET_SHIFT;
	ret = regmap_update_bits(chip->regmap, LN8410_REG_0B, LN8410_FSW_SET_MASK, val);

	return ret;
}

static int ln8410_soft_reset(struct ln8410_device *chip)
{
	unsigned int sys_st = 0;
	int i = 0, ret = 0;

	/* SOFT_RESET_REQ=1 */
	ret = regmap_write(chip->regmap, LN8410_REG_LION_CTRL, 0xC6);
	ret = regmap_update_bits(chip->regmap, LN8410_REG_TEST_MODE_CTRL, 0x1, 0x1);
	msleep(30);

	/* check status */
	for (i=0; i < 30; ++i) {
		ret = regmap_read(chip->regmap, LN8410_REG_SYS_STS, &sys_st);
		if (sys_st & 0x3)
			break;
		msleep(5);
    	}

	if (i == 30)
		xmc_info("%s fail to reset, can't check the valid status(sys_st=0x%x)\n", chip->log_tag, sys_st);

	return 0;
}

static int protected_reg_init(struct ln8410_device *chip)
{
	int ret = 0;

	regmap_write(chip->regmap, LN8410_REG_LION_CTRL, 0xAA);
	ln8410_vbus_short_dis(chip, 1); //disable vbus short

	if (chip->revision == 0x2) {
		ln8410_use_hvldo(chip, 0); //not use hvldo ,no need in cs2
		ret = regmap_update_bits(chip->regmap, LN8410_REG_LION_STARTUP_CTRL, 0x04, 0x04);  /* C1_C2_CHARGE_TIME_CFG = 1 */
		ret = regmap_update_bits(chip->regmap, LN8410_REG_CFG9, 0x20, 0x20);               /* USE_TINY_MASK = 1 */
		ret = regmap_update_bits(chip->regmap, LN8410_REG_CFG8, 0x40, 0x00);               /* C1N_C2N_C3N_FORCE_IDLE_MASK = 0 */
	}

	regmap_write(chip->regmap, LN8410_REG_LION_CTRL, 0x00);

	return ret;
}

static int ln8410_init_protection(struct ln8410_device *chip, int mode)
{
	int ret = 0;

	ret = ln8410_enable_batovp(chip, true);
	ret = ln8410_enable_batocp(chip, false);
	ret = ln8410_enable_busocp(chip, true);
	ret = ln8410_enable_busucp(chip, true);
	ret = ln8410_enable_pmid2outovp(chip, true);
	ret = ln8410_enable_pmid2outuvp(chip, true);
//	ret = ln8410_enable_batovp_alarm(chip, true);
//	ret = ln8410_enable_busocp_alarm(chip, true);
	ret = ln8410_set_batovp(chip, 4650);
	ret = ln8410_set_batocp(chip, 8000);
//	ret = ln8410_set_batovp_alarm(chip, chip->bat_ovp_alarmreshold);
	ret = ln8410_set_busovp(chip, 0); /*0 low set, 5.6v for 1:1, 12v for 2:1, 22v for 4:1*/
	ret = ln8410_set_busocp(chip, 4500);
	ret = ln8410_set_busucp(chip, 100);

	xmc_err("ln8410 init mode=%d\n", mode);
	if (mode == LN8410_FORWARD_4_1_CHARGER_MODE) {
		ret = ln8410_set_usbovp(chip, 22000);
	} else if (mode == LN8410_FORWARD_2_1_CHARGER_MODE) {
		ret = ln8410_set_usbovp(chip, 14000);
	} else {
		//To do later for forward 1:1 mode
		ret = ln8410_set_usbovp(chip, 6500);
	}

	ret = ln8410_set_wpcovp(chip, 22000);
	ret = ln8410_set_outovp(chip, 5000);
	ret = ln8410_set_pmid2outuvp(chip, 100);
	ret = ln8410_set_pmid2outovp(chip, 300);

	return ret;
}

static int ln8410_init_device(struct ln8410_device *chip, int driver_data)
{
	int ret = 0;

	ret = ln8410_enable_parallel_func(chip, false);

	if (driver_data == LN8410_MASTER)
		ret = ln8410_enable_config_func(chip, true);
	else if (driver_data == LN8410_SLAVE)
		ret = ln8410_enable_config_func(chip, false);

	ret = regmap_read(chip->regmap, LN8410_REG_62, &(chip->revision));
	chip->revision = chip->revision >> 4;
	ret = regmap_read(chip->regmap, LN8410_REG_PRODUCT_ID, &(chip->product_cfg));
	chip->product_cfg = chip->product_cfg & 0x0F;

	ln8410_soft_reset(chip);
	regmap_write(chip->regmap, LN8410_REG_RECOVERY_CTRL, 0x00); // turn off all auto recovery
	ln8410_set_regn_pull_down(chip, false);
	protected_reg_init(chip);
	ln8410_fsw_set(chip, LN8410_FSW_SET_580K);
	ln8410_init_protection(chip, LN8410_FORWARD_4_1_CHARGER_MODE);
	ln8410_enable_acdrv_manual(chip, 0);//ac drive manual mode
	ln8410_enable_wpcgate(chip, true);
	ln8410_enable_ovpgate(chip, true);
	ln8410_set_ss_timeout(chip, 5120);
	ln8410_set_ucp_fall_dg(chip, LN8410_BUS_UCP_FALL_DG_8US);

	ln8410_set_sense_resistor(chip, 1);
	ret = ln8410_set_adc_mode(chip, 0x01);
	ln8410_set_div_mode(chip, LN8410_FORWARD_4_1_CHARGER_MODE);
	ln8410_enable_adc(chip,false);

 	/* masked unuse interrupts */
	regmap_update_bits(chip->regmap, LN8410_REG_11, 0xFE, 0xFE); //VUSB INSERT
	regmap_update_bits(chip->regmap, LN8410_REG_14, 0x08, 0x08); //close adc done irq
	regmap_update_bits(chip->regmap, LN8410_REG_07, 0x10, 0x10); //close pmic2out ovp irq
	regmap_update_bits(chip->regmap, LN8410_REG_08, 0x10, 0x10); //close pmic2out uvp irq
	regmap_update_bits(chip->regmap, LN8410_REG_LION_INT_MASK, 0xFF, 0xFF);
	regmap_update_bits(chip->regmap, LN8410_REG_06, 0x0A, 0x0A); //close ibus ucp rise and fail irq
	regmap_update_bits(chip->regmap, LN8410_REG_04, 0x40, 0x40); //close vwpc ovp irq
	regmap_update_bits(chip->regmap, LN8410_REG_03, 0x40, 0x40); //close vusb ovp irq
	regmap_update_bits(chip->regmap, LN8410_REG_01, 0x20, 0x20); //close vbat ovp irq

	return ret;
}

static void ln8410_abnormal_charging_judge(unsigned int *data)
{
	if(data == NULL)
		return;
	if((data[14] & 0x08) == 0)
	{
		xmc_info("%s VOUT UVLO\n", __func__);
	}
	if((data[14] & 0x3F) != 0x3F)
	{
		xmc_info("%s VIN have problem\n", __func__);
	}
	if(data[8] & 0x08)
	{
		xmc_info("%s VBUS_ERRORHI_STAT\n", __func__);
	}
	if(data[8] & 0x10)
	{
		xmc_info("%s VBUS_ERRORLO_STAT\n", __func__);
	}
	if(data[37] & 0x20)
	{
		xmc_info("%s IBUS_UCP_FALL_FLAG\n", __func__);
	}
	if(data[8] & 0x01)
	{
		xmc_info("%s CBOOT dio\n", __func__);
	}
	if(data[37] & 0x01)
	{
		xmc_info("%s VBAT_OVP\n", __func__);
	}
	if(data[37] & 0x02)
	{
		xmc_info("%s IBAT_OCP\n", __func__);
	}
	if(data[37] & 0x04)
	{
		xmc_info("%s VBUS_OVP\n", __func__);
	}
	if(data[37] & 0x08)
	{
		xmc_info("%s VWPC_OVP\n", __func__);
	}
	if(data[37] & 0x10)
	{
		xmc_info("%s IBUS_OCP\n", __func__);
	}
	if(data[37] & 0x20)
	{
		xmc_info("%s IBUS_UCP\n", __func__);
	}
	if(data[39] & 0x01)
	{
		xmc_info("%s PMID2OUT_OVP\n", __func__);
	}
	if(data[39] & 0x02)
	{
		xmc_info("%s PMID2OUT_UVP\n", __func__);
	}
	if(data[8] & 0x80)
	{
		xmc_info("%s POR_FLAG\n", __func__);
	}
	return;
}

static void ln8410_dump_register(struct ln8410_device *chip)
{
	unsigned int data[100];
	int i = 0, reg_num_a = ARRAY_SIZE(ln8410_reg_list_a), reg_num_b = ARRAY_SIZE(ln8410_reg_list_b);

	for (i = 0; i < reg_num_a; i++) {
		if (i == 0)
			printk(KERN_CONT "[LN8410_DUMP_IMPORTANT_REG] %s: \n", chip->log_tag);
		regmap_read(chip->regmap, ln8410_reg_list_a[i], &data[i]);
		printk(KERN_CONT "0x%02x : 0x%02x\n", ln8410_reg_list_a[i], data[i]);
	}

	for (; i < reg_num_b; i++) {
		regmap_read(chip->regmap, ln8410_reg_list_b[i], &data[i]);
		printk(KERN_CONT "0x%02x : 0x%02x\n", ln8410_reg_list_b[i], data[i]);
	}
	if((data[8] & 0x02) == 0)
	{
			xmc_err("cp switching stop, enter abnormal charging judge 0x%02x 0x%02x\n", data[8], data[8] & 0x02);
			ln8410_abnormal_charging_judge(data);
	}
}

static int ln8410_ops_charge_enable(struct charger_device *chg_dev, bool enable)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = ln8410_enable_charge(chip, enable);
	if (ret)
		xmc_info("%s failed to enable charge\n", chip->log_tag);
	ln8410_dump_register(chip);

	return ret;
}

static int ln8410_ops_get_charge_enable(struct charger_device *chg_dev, bool *enabled)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	int ret = 0;
	ln8410_dump_register(chip);
	ret = ln8410_get_charge_enable(chip, enabled);
	if (ret)
		xmc_info("%s failed to get charge_enable\n", chip->log_tag);

	return ret;
}

static int ln8410_ops_get_vbus(struct charger_device *chg_dev, u32 *value)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	u32 vbus = 0;
	int ret = 0;

	ret = ln8410_get_adc(chip, ADC_VBUS, &vbus);
	if (ret) {
		xmc_err("failed to get ADC_VBUS\n");
		vbus = 0;
	}

	*value = (int)vbus;
	return ret;
}

static int ln8410_ops_get_ibus(struct charger_device *chg_dev, u32 *value)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	u32 ibus = 0;
	int ret = 0;

	ret = ln8410_get_adc(chip, ADC_IBUS, &ibus);
	if (ret) {
		xmc_err("failed to get ADC_VBUS\n");
		ibus = 0;
	}

	*value = (int)ibus;
	return ret;
}

static int ln8410_ops_get_vbat(struct charger_device *chg_dev, u32 *value)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	u32 vbat = 0;
	int ret = 0;

	ret = ln8410_get_adc(chip, ADC_VBAT, &vbat);
	if (ret) {
		xmc_err("failed to get ADC_VAT\n");
		vbat = 0;
	}

	*value = (int)vbat;
	return ret;
}

static int ln8410_ops_get_ibat(struct charger_device *chg_dev, u32 *value)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	u32 ibat = 0;
	int ret = 0;

	ret = ln8410_get_adc(chip, ADC_IBAT, &ibat);
	if (ret) {
		xmc_err("failed to get ADC_IBAT\n");
		ibat = 0;
	}

	*value = (int)ibat;
	return ret;
}

static int ln8410_ops_set_div_mode(struct charger_device *chg_dev, int mode)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	int ret = 0;

	switch (mode) {
	case XMC_CP_1T1:
		ret = ln8410_set_div_mode(chip, LN8410_FORWARD_1_1_CHARGER_MODE);
		break;
	case XMC_CP_2T1:
		ret = ln8410_set_div_mode(chip, LN8410_FORWARD_2_1_CHARGER_MODE);
		break;
	case XMC_CP_4T1:
		ret = ln8410_set_div_mode(chip, LN8410_FORWARD_4_1_CHARGER_MODE);
		break;
	default:
		xmc_err("%s set div_mode is unsupported\n", chip->log_tag);
	}

	return ret;
}

static int ln8410_ops_get_div_mode(struct charger_device *chg_dev, int *mode)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	int value = 0, ret = 0;

	ret = ln8410_get_div_mode(chip, &value);
	if (ret)
		xmc_err("%s failed to get div_mode\n", chip->log_tag);

	switch (value) {
	case LN8410_FORWARD_1_1_CHARGER_MODE:
		*mode = XMC_CP_1T1;
		break;
	case LN8410_FORWARD_2_1_CHARGER_MODE:
		*mode = XMC_CP_2T1;
		break;
	case LN8410_FORWARD_4_1_CHARGER_MODE:
		*mode = XMC_CP_4T1;
		break;
	default:
		xmc_err("%s get div_mode is unsupported\n", chip->log_tag);
	}

	return ret;
}

static int ln8410_ops_adc_enable(struct charger_device *chg_dev, bool enable)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = ln8410_enable_adc(chip, enable);
	if (ret)
		xmc_err("%s failed to enable ADC\n", chip->log_tag);

	return ret;
}

static int ln8410_ops_device_init(struct charger_device *chg_dev, int mode)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	int ret = 0;

	switch (mode) {
	case XMC_CP_1T1:
		ret = ln8410_init_protection(chip, LN8410_FORWARD_1_1_CHARGER_MODE);
		break;
	case XMC_CP_2T1:
		ret = ln8410_init_protection(chip, LN8410_FORWARD_2_1_CHARGER_MODE);
		break;
	case XMC_CP_4T1:
		ret = ln8410_init_protection(chip, LN8410_FORWARD_4_1_CHARGER_MODE);
		break;
	default:
		xmc_err("%s init device div_mode is unsupported\n", chip->log_tag);
	}

	return ret;
}

static int ln8410_ops_dump_register(struct charger_device *chg_dev)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);

	ln8410_dump_register(chip);

	return 0;
}

static int ln8410_ops_get_bypass_support(struct charger_device *chg_dev, bool *enabled )
{
	*enabled = 1;
	xmc_err("%s %d\n", __func__, *enabled);
	return 0;
}

static int ln8410_ops_get_chip_vendor(struct charger_device *chg_dev, int *chip_id)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	*chip_id = chip->chip_vendor;
	xmc_err("%s = %d\n", __func__, *chip_id);
	return 0;
}

static int ln8410_ops_enable_acdrv_manual(struct charger_device *chg_dev, bool enable)
{
	struct ln8410_device *chip = charger_get_data(chg_dev);
	int ret = 0;
	ret = ln8410_enable_acdrv_manual(chip, enable);
	if (ret)
		xmc_err("%s failed enable cp acdrv manual\n", chip->log_tag);

	return ret;
}

static const struct charger_ops ln8410_ops = {
	.enable = ln8410_ops_charge_enable,
	.is_enabled = ln8410_ops_get_charge_enable,
	.get_vbus_adc = ln8410_ops_get_vbus,
	.get_ibus_adc = ln8410_ops_get_ibus,
    .cp_get_vbatt = ln8410_ops_get_vbat,
	.cp_get_ibatt = ln8410_ops_get_ibat,
	.cp_set_mode = ln8410_ops_set_div_mode,
	.cp_get_mode = ln8410_ops_get_div_mode,
	.cp_enable_adc = ln8410_ops_adc_enable,
	.cp_device_init = ln8410_ops_device_init,
	.cp_dump_register = ln8410_ops_dump_register,
	.cp_get_bypass_support = ln8410_ops_get_bypass_support,
	.cp_get_chip_vendor = ln8410_ops_get_chip_vendor,
	.enable_acdrv_manual = ln8410_ops_enable_acdrv_manual,
};

static const struct charger_properties ln8410_master_chg_props = {
	.alias_name = "cp_master",
};

static const struct charger_properties ln8410_slave_chg_props = {
	.alias_name = "cp_slave",
};

static int ln8410_register_charger(struct ln8410_device *chip, int driver_data)
{
	switch (driver_data) {
	case LN8410_SLAVE:
		chip->chg_dev = charger_device_register("cp_slave", chip->dev, chip, &ln8410_ops, &ln8410_slave_chg_props);
		break;
	case LN8410_MASTER:
		chip->chg_dev = charger_device_register("cp_master", chip->dev, chip, &ln8410_ops, &ln8410_master_chg_props);
		break;
	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static void ln8410_irq_handler(struct work_struct *work)
{
	struct ln8410_device *chip = container_of(work, struct ln8410_device, irq_handle_work.work);
	ln8410_dump_register(chip);
	xmc_info("%s", __func__);
	__pm_relax(chip->irq_wakelock);

	return;
}

static irqreturn_t ln8410_interrupt(int irq, void *data)
{
	struct ln8410_device *chip = data;

	xmc_info("%s ln8410_interrupt\n", chip->log_tag);

	if (chip->irq_wakelock->active)
		return IRQ_HANDLED;

	__pm_stay_awake(chip->irq_wakelock);
	schedule_delayed_work(&chip->irq_handle_work, 0);

	return IRQ_HANDLED;
}

static int ln8410_parse_dt(struct ln8410_device *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret = 0;
    xmc_err("%s ln8410_parse_dt start\n", chip->log_tag);
	if (!np) {
		xmc_err("%s device tree info missing\n", chip->log_tag);
		return -1;
	}

	chip->irq_gpio = of_get_named_gpio(np, "sc8561_irq_gpio", 0);
	if (!gpio_is_valid(chip->irq_gpio)) {
		xmc_err("%s failed to parse ln8410_irq_gpio\n", chip->log_tag);
		return -1;
	}
     xmc_err("%s ln8410_parse_dt end\n", chip->log_tag);
	return ret;
}

static int ln8410_init_irq(struct ln8410_device *chip)
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

	ret = request_irq(chip->irq, ln8410_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(chip->dev), chip);
	if (ret < 0) {
		xmc_err("%s failed to request irq\n", chip->log_tag);
		return -1;
	}

	enable_irq_wake(chip->irq);

	return 0;
}

static int cp_vbus_get(struct ln8410_device *chip,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (chip) {
		ret = ln8410_get_adc(chip, ADC_VBUS, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int cp_ibus_get(struct ln8410_device *chip,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (chip) {
		ret = ln8410_get_adc(chip, ADC_IBUS, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int cp_tdie_get(struct ln8410_device *chip,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (chip) {
		ret = ln8410_get_adc(chip, ADC_TDIE, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int chip_ok_get(struct ln8410_device *chip,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (chip)
		*val = chip->chip_ok;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static ssize_t cp_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct ln8410_device *chip;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	chip = (struct ln8410_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(chip, usb_attr, val);

	return count;
}

static ssize_t cp_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct ln8410_device *chip;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	chip = (struct ln8410_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(chip, usb_attr, &val);

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

static enum power_supply_property ln8410_power_supply_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int ln8410_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct ln8410_device *chip = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 data = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->chip_ok;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = ln8410_get_adc(chip, ADC_VBAT, &data);
		if (ret)
			val->intval = 0;
		else
			val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = ln8410_get_adc(chip, ADC_IBAT, &data);
		if (ret)
			val->intval = 0;
		else
			val->intval = data;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = ln8410_get_adc(chip, ADC_TDIE, &data);
		if (ret)
			val->intval = 0;
		else
			val->intval = data;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "LN8410";
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

static int ln8410_psy_init(struct ln8410_device *chip, struct device *dev, int driver_data)
{
	struct power_supply_config psy_cfg = {};

	switch (driver_data) {
	case LN8410_MASTER:
		chip->psy_desc.name = "cp_master";
		break;
	case LN8410_SLAVE:
		chip->psy_desc.name = "cp_slave";
		break;
	default:
		return -EINVAL;
	}

	psy_cfg.drv_data = chip;
	psy_cfg.of_node = chip->dev->of_node;
	chip->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	chip->psy_desc.properties = ln8410_power_supply_props,
	chip->psy_desc.num_properties = ARRAY_SIZE(ln8410_power_supply_props),
	chip->psy_desc.get_property = ln8410_get_property,

	chip->cp_psy = devm_power_supply_register(chip->dev, &chip->psy_desc, &psy_cfg);
	if (IS_ERR(chip->cp_psy))
		return -EINVAL;

	return 0;
}

static int ln8410_detect_device(struct i2c_client *client, struct ln8410_device *chip, int driver_data)
{
	int retry_count = 0;
	s32 data = 0;

retry:
	data = i2c_smbus_read_byte_data(client, LN8410_REG_6E);
	if (data < 0) {
		xmc_err("%s failed to read device_id, retry = %d\n", chip->log_tag, retry_count);
		retry_count++;
		if (retry_count < 3) {
			msleep(250);
			goto retry;
		} else {
			return -1;
		}
	}

	xmc_info("%s device_id = 0x%02x\n", chip->log_tag, data);
	if (data == LN8410_DEVICE_ID) {
		chip->chip_vendor = LN8410;
		if (driver_data == LN8410_MASTER)
			strcpy(chip->log_tag, "[XMC_CP_LN8410_MASTER]");
		else if (driver_data == LN8410_SLAVE)
			strcpy(chip->log_tag, "[XMC_CP_LN8410_SLAVE]");
	} else {
		xmc_err("%s device_id is invalid\n", chip->log_tag);
		return -1;
	}

	return 0;
}

static int ln8410_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ln8410_device *chip;
	int ret = 0;

    xmc_err("[LN8410_PROBE] start ln8410 probe\n");
	if (id->driver_data == LN8410_MASTER)
		client->addr = 0x64;
	else if (id->driver_data == LN8410_SLAVE)
		client->addr = 0x65;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		xmc_err("LN8410 probe failed to zalloc chip\n");
		goto fail;
	}

	chip->client = client;
	chip->dev = dev;
	strcpy(chip->log_tag, "[CP_UNKNOWN]");
	chip->irq_wakelock = wakeup_source_register(NULL, "ln8410_irq_wakelock");

	ret = ln8410_detect_device(client, chip, id->driver_data);
	if (ret) {
		xmc_err("%s failed to detect device\n", chip->log_tag);
		goto fail;
	}

	i2c_set_clientdata(client, chip);
   	chip->regmap = devm_regmap_init_i2c(client, &ln8410_regmap_config);
	if (IS_ERR(chip->regmap)) {
		xmc_err("%s failed to init regmap\n", chip->log_tag);
		goto fail;
	}

	ret = ln8410_init_device(chip, id->driver_data);
	if (ret)
		xmc_err("%s failed to init device\n", chip->log_tag);

    xmc_err("[LN8410_PROBE] ln8410 probe node3\n");

	ret = ln8410_parse_dt(chip);
	if (ret) {
		xmc_info("%s failed to parse DTS\n", chip->log_tag);
		goto fail;
	}

	ret = ln8410_init_irq(chip);
	if (ret) {
		xmc_info("%s failed to int irq\n", chip->log_tag);
		goto fail;
	}


	ret = ln8410_register_charger(chip, id->driver_data);
	if (ret) {
		xmc_info("%s failed to register charger\n", chip->log_tag);
		goto fail;
	}

    xmc_err("[LN8410_PROBE] ln8410 probe node6\n");
	ret = ln8410_psy_init(chip, dev, id->driver_data);
	if (ret) {
		xmc_info("%s failed to init psy\n", chip->log_tag);
		goto fail;
	}else{
        cp_sysfs_create_group(chip->cp_psy);
    }

    xmc_err("[LN8410_PROBE] ln8410 probe node7\n");

	INIT_DELAYED_WORK(&chip->irq_handle_work, ln8410_irq_handler);

	chip->chip_ok = true;
	xmc_err("%s probe success\n", chip->log_tag);
	return 0;

fail:
	devm_kfree(dev, chip);
	return ret;
}

static int ln8410_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ln8410_device *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = ln8410_enable_adc(chip, false);
	if (ret)
		xmc_info("%s failed to disable ADC\n", chip->log_tag);

	xmc_info("%s ln8410 suspend!\n", chip->log_tag);

	return enable_irq_wake(chip->irq);
}

static int ln8410_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ln8410_device *chip = i2c_get_clientdata(client);

	xmc_info("%s ln8410 resume!\n", chip->log_tag);

	return disable_irq_wake(chip->irq);
}

static const struct dev_pm_ops ln8410_pm_ops = {
	.suspend	= ln8410_suspend,
	.resume		= ln8410_resume,
};

static int ln8410_remove(struct i2c_client *client)
{
	struct ln8410_device *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = ln8410_enable_adc(chip, false);
	if (ret)
		xmc_info("%s failed to disable ADC\n", chip->log_tag);

	power_supply_unregister(chip->cp_psy);
	return ret;
}

static void ln8410_shutdown(struct i2c_client *client)
{
	struct ln8410_device *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = ln8410_enable_adc(chip, false);
	if (ret)
		xmc_info("%s failed to disable ADC\n", chip->log_tag);

	xmc_info("%s ln8410 shutdown!\n", chip->log_tag);
}

static const struct i2c_device_id ln8410_i2c_ids[] = {
	{ "sc8561_master", LN8410_MASTER },
	{ "sc8561_slave", LN8410_SLAVE },
	{},
};
MODULE_DEVICE_TABLE(i2c, ln8410_i2c_ids);

static const struct of_device_id ln8410_of_match[] = {
	{ .compatible = "sc8561_master", .data = (void *)LN8410_MASTER},
	{ .compatible = "sc8561_slave", .data = (void *)LN8410_SLAVE},
	{ },
};
MODULE_DEVICE_TABLE(of, ln8410_of_match);

static struct i2c_driver ln8410_driver = {
	.driver = {
		.name = "ln8410_charger",
		.of_match_table = ln8410_of_match,
		.pm = &ln8410_pm_ops,
	},
	.id_table = ln8410_i2c_ids,
	.probe = ln8410_probe,
	.remove = ln8410_remove,
	.shutdown = ln8410_shutdown,
};
module_i2c_driver(ln8410_driver);

MODULE_AUTHOR("xiezhichang <xiezhichang@xiaomi.com>");
MODULE_DESCRIPTION("ln8410 charger pump driver");
MODULE_LICENSE("GPL v2");

