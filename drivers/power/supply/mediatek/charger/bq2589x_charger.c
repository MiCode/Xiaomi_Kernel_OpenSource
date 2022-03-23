/* SPDX-License-Identifier: GPL-2.0 */

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
#include <linux/of_gpio.h>
#include <mt-plat/v1/charger_class.h>
#include <mt-plat/v1/mtk_charger.h>
#include <linux/delay.h>
#include "mtk_charger_intf.h"
#include "bq2589x_reg.h"
#include <mt-plat/upmu_common.h>
enum bq2589x_vbus_type {
	BQ2589X_VBUS_NONE = 0,
	BQ2589X_VBUS_USB_SDP,
	BQ2589X_VBUS_USB_CDP, /*CDP for bq25890, Adapter for bq25892*/
	BQ2589X_VBUS_USB_DCP,
	BQ2589X_VBUS_MAXC,
	BQ2589X_VBUS_UNKNOWN,
	BQ2589X_VBUS_NONSTAND,
	BQ2589X_VBUS_OTG,
	BQ2589X_VBUS_TYPE_NUM,
};

enum bq2589x_part_no {
	BQ25890 = 0x03,
	BQ25892 = 0x00,
	BQ25895 = 0x07,
};

#define BQ2589X_STATUS_PLUGIN			0x0001
#define BQ2589X_STATUS_PG				0x0002
#define	BQ2589X_STATUS_CHARGE_ENABLE	0x0004
#define BQ2589X_STATUS_FAULT			0x0008
#define BQ2589X_STATUS_VINDPM			0x00010
#define BQ2589X_STATUS_IINDPM			0x00020
#define BQ2589X_STATUS_EXIST			0x0100

struct bq2589x_config {
	bool	enable_auto_dpdm;
/*	bool	enable_12v;*/
	int		charge_voltage;
	int		charge_current;
	bool	enable_term;
	int		term_current;
	bool	enable_ico;
	bool	use_absolute_vindpm;
};


struct bq2589x {
	struct device *dev;
	struct i2c_client *client;
	enum   bq2589x_part_no part_no;
	int    revision;

	unsigned int	status;
	int		vbus_type;

	struct wakeup_source *pe_tune_wakelock;

	bool	enabled;
	bool	charge_enabled;

	int		vbus_volt;
	int		vbat_volt;

	int fixed_input_current;
	int fixed_charge_current;

	int		rsoc;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	const char *chg_dev_name;
	struct	bq2589x_config	cfg;
	struct work_struct irq_work;
	struct work_struct adapter_in_work;
	struct work_struct adapter_out_work;
	struct delayed_work monitor_work;
	struct delayed_work ico_work;
	struct delayed_work pe_volt_tune_work;
	struct delayed_work check_pe_tuneup_work;

	struct power_supply_desc usb;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply_config usb_cfg;
};

struct pe_ctrl {
	bool enable;
	bool tune_up_volt;
	bool tune_down_volt;
	bool tune_done;
	bool tune_fail;
	int  tune_count;
	int  target_volt;
	int	 high_volt_level;/* vbus volt > this threshold means tune up successfully */
	int  low_volt_level; /* vbus volt < this threshold means tune down successfully */
	int  vbat_min_volt;  /* to tune up voltage only when vbat > this threshold */
};


static struct bq2589x *g_bq;
static struct pe_ctrl pe;

static DEFINE_MUTEX(bq2589x_i2c_lock);
static DEFINE_MUTEX(bq2589x_type_det_lock);

/* for mtk ops */
static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data);
static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data);
static int bq2589x_read_byte(struct bq2589x *bq, u8 *data, u8 reg);
static int bq2589x_plug_in(struct charger_device *chg_dev);
static int bq2589x_plug_out(struct charger_device *chg_dev);
static int bq2589x_dump_register(struct charger_device *chg_dev);
static int bq2589x_is_charging_enable(struct charger_device *chg_dev, bool *en);
static int bq2589x_get_ichg(struct charger_device *chg_dev, u32 *curr);
static int bq2589x_set_ichg(struct charger_device *chg_dev, u32 curr);
static int bq2589x_get_icl(struct charger_device *chg_dev, u32 *curr);
//static int bq2589x_set_icl(struct charger_device *chg_dev, u32 curr);
static int bq2589x_get_vchg(struct charger_device *chg_dev, u32 *volt);
static int bq2589x_set_vchg(struct charger_device *chg_dev, u32 volt);
static int bq2589x_kick_wdt(struct charger_device *chg_dev);
static int bq2589x_set_ivl(struct charger_device *chg_dev, u32 volt);
static int bq2589x_is_charging_done(struct charger_device *chg_dev, bool *done);
static int bq2589x_get_min_ichg(struct charger_device *chg_dev, u32 *curr);
static int bq2589x_set_safety_timer(struct charger_device *chg_dev, bool en);
static int bq2589x_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en);
static int bq2589x_set_hz_mode(struct charger_device *chg_dev, bool en);
static int bq2589x_do_event(struct charger_device *chg_dev, u32 event, u32 args);

/* ops function */
static int bq2589x_enable_charging(struct charger_device *chg_dev, bool enable);
static void bq2589x_dump_regs(struct bq2589x *bq);
static int bq2589x_adc_read_charge_current(struct bq2589x *bq);
static int bq2589x_set_chargecurrent(struct bq2589x *bq, u32 curr);
static int bq2589x_get_input_current_limit(struct bq2589x *bq, int *curr);
static int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr);
static int bq2589x_set_chargevoltage(struct bq2589x *bq, int volt);
static int bq2589x_reset_watchdog_timer(struct bq2589x *bq);
static int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt);
static bool bq2589x_is_charge_done(struct bq2589x *bq);
static int bq2589x_enter_hiz_mode(struct bq2589x *bq);
static int bq2589x_exit_hiz_mode(struct bq2589x *bq);
static int bq2589x_enable_charger(struct bq2589x *bq);
static int bq2589x_disable_charger(struct bq2589x *bq);

static void bq2589x_reset_pe_param(void)
{
	pe.enable = true;
	pe.tune_up_volt = false;
	pe.tune_down_volt = false;
	pe.tune_done = true;
	pe.tune_fail = false;
	pe.tune_count = 0;
	pe.target_volt = 5000;
	pe.high_volt_level = 12000;
	pe.low_volt_level = 5000;
	pe.vbat_min_volt = 3000;
}

int bq2589x_irq;
static int bq2589x_plug_in(struct charger_device *chg_dev)
{
	int ret;

	ret = bq2589x_enable_charging(chg_dev, true);
	if (ret < 0)
		pr_info("Failed to enable charging:%d\n", ret);

	return ret;
}

static int bq2589x_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = bq2589x_enable_charging(chg_dev, false);
	if (ret < 0)
		pr_info("Failed to disable charging:%d\n", ret);

	return ret;
}

static int bq2589x_dump_register(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	bq2589x_dump_regs(bq);

	return 0;
}

static int bq2589x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	*en = bq->charge_enabled;

	return 0;
}

static int bq2589x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	*curr = bq2589x_adc_read_charge_current(bq) * 1000;

	return 0;

}

static int bq2589x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	curr = curr / 1000;
	ret = bq2589x_set_input_current_limit(bq, curr);

	if (ret < 0)
		dev_info(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
	return 0;
}

static int bq2589x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	ret = bq2589x_get_input_current_limit(bq, curr);
	if (ret < 0)
		dev_info(bq->dev, "%s:Failed to get charge current limit:%d\n", __func__, ret);

	return 0;
}

static int bq2589x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	curr = curr / 1000;
	ret = bq2589x_set_chargecurrent(bq, curr);

	if (ret < 0)
		dev_info(bq->dev, "%s:Failed to set charge current limit:%d\n", __func__, ret);
	return 0;
}

static int bq2589x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = bq2589x_read_byte(bq, &reg_val, BQ2589X_REG_06);
	if (!ret) {
		vchg = (reg_val & BQ2589X_VREG_MASK) >> BQ2589X_VREG_SHIFT;
		vchg = vchg * BQ2589X_VREG_LSB + BQ2589X_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}
static int bq2589x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	volt = volt/1000;
	ret = bq2589x_set_chargevoltage(bq, volt);
	if (ret < 0)
		dev_info(bq->dev, "%s:Failed to set charge voltage:%d\n", __func__, ret);

	return ret;
}
static int bq2589x_kick_wdt(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq2589x_reset_watchdog_timer(bq);
}

static int bq2589x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_info("limit volt = %d\n", volt);

	return bq2589x_set_input_volt_limit(bq, volt/1000);
}

static int bq2589x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	*done = bq2589x_is_charge_done(bq);
	return 0;
}

static int bq2589x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr  = 60 * 1000;
	return 0;
}
static int bq2589x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	if (en) {
		val = BQ2589X_CHG_TIMER_ENABLE << BQ2589X_EN_TIMER_SHIFT;
		ret = bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK, val);
	} else {
		val = BQ2589X_CHG_TIMER_DISABLE << BQ2589X_EN_TIMER_SHIFT;
		ret = bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK, val);
	}

	return ret;
}

static int bq2589x_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = bq2589x_read_byte(bq, &reg_val, BQ2589X_REG_07);

	if (!ret)
		*en = !!(reg_val & BQ2589X_EN_TIMER_MASK);

	return ret;
}
static int bq2589x_set_hz_mode(struct charger_device *chg_dev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		bq2589x_enter_hiz_mode(bq);
	} else {
		bq2589x_exit_hiz_mode(bq);
		bq2589x_set_input_current_limit(bq, 2050);
	}

	return 0;
}

static int bq2589x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
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
	return 0;
}
static int bq2589x_enable_charging(struct charger_device *chg_dev, bool enable)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = bq2589x_enable_charger(bq);
	else
		ret = bq2589x_disable_charger(bq);

	pr_info("%s charger %s\n",
		enable ? "enable":"disable", !ret ? "successfully":"failed");

	if (!ret)
		bq->charge_enabled = !!(val & BQ2589X_CHG_CONFIG_MASK);
	return ret;
}

static void bq2589x_dump_regs(struct bq2589x *bq)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x14; addr++) {
		msleep(20);
		ret = bq2589x_read_byte(bq, &val, addr);
		if (!ret)
			pr_info("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

int bq2589x_get_input_current_limit(struct bq2589x *bq, int *curr)
{
	u8 reg_val;
	int icl;
	int ret;

	ret = bq2589x_read_byte(bq, &reg_val, BQ2589X_REG_00);
	if (!ret) {
		icl = (reg_val & BQ2589X_ENILIM_MASK) >> BQ2589X_ENILIM_SHIFT;
		icl = icl * BQ2589X_IINLIM_LSB + BQ2589X_IINLIM_BASE;
		*curr = icl * 1000;
	}
	return ret;
}

static int bq2589x_read_byte(struct bq2589x *bq, u8 *data, u8 reg)
{
	int ret;

	mutex_lock(&bq2589x_i2c_lock);
	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		dev_info(bq->dev, "failed to read 0x%.2x\n", reg);
		mutex_unlock(&bq2589x_i2c_lock);
		return ret;
	}

	*data = (u8)ret;
	mutex_unlock(&bq2589x_i2c_lock);

	return 0;
}

static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&bq2589x_i2c_lock);
	ret = i2c_smbus_write_byte_data(bq->client, reg, data);
	mutex_unlock(&bq2589x_i2c_lock);
	return ret;
}

static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = bq2589x_read_byte(bq, &tmp, reg);

	if (ret)
		return ret;

	tmp &= ~mask;
	tmp |= data & mask;

	return bq2589x_write_byte(bq, reg, tmp);
}


static enum bq2589x_vbus_type bq2589x_get_vbus_type(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0)
		return 0;
	val &= BQ2589X_VBUS_STAT_MASK;
	val >>= BQ2589X_VBUS_STAT_SHIFT;

	return val;
}


static int bq2589x_enable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_ENABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);

}

static int bq2589x_disable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_DISABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_disable_otg);

static int bq2589x_set_otg_volt(struct bq2589x *bq, int volt)
{
	u8 val = 0;

	if (volt < BQ2589X_BOOSTV_BASE)
		volt = BQ2589X_BOOSTV_BASE;
	if (volt > BQ2589X_BOOSTV_BASE
		+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT)
		* BQ2589X_BOOSTV_LSB)
		volt = BQ2589X_BOOSTV_BASE
		+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT)
		* BQ2589X_BOOSTV_LSB;

	val = ((volt - BQ2589X_BOOSTV_BASE) / BQ2589X_BOOSTV_LSB) << BQ2589X_BOOSTV_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_0A, BQ2589X_BOOSTV_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_set_otg_volt);

static int bq2589x_set_otg_current(struct charger_device *chg_dev, unsigned int curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 temp;

	pr_info("set otg current %d\n", curr);
	if (curr == 500)
		temp = BQ2589X_BOOST_LIM_500MA;
	else if (curr == 700)
		temp = BQ2589X_BOOST_LIM_700MA;
	else if (curr == 1100)
		temp = BQ2589X_BOOST_LIM_1100MA;
	else if (curr == 1600)
		temp = BQ2589X_BOOST_LIM_1600MA;
	else if (curr == 1800)
		temp = BQ2589X_BOOST_LIM_1800MA;
	else if (curr == 2100)
		temp = BQ2589X_BOOST_LIM_2100MA;
	else if (curr == 2400)
		temp = BQ2589X_BOOST_LIM_2400MA;
	else
		temp = BQ2589X_BOOST_LIM_1300MA;

	return bq2589x_update_bits(bq,
		BQ2589X_REG_0A,
		BQ2589X_BOOST_LIM_MASK,
		temp << BQ2589X_BOOST_LIM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_otg_current);

static int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status |= BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}

static int bq2589x_disable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_DISABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status &= ~BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_disable_charger);


/* interfaces that can be called by other module */
int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_02);
	if (ret < 0) {
		dev_info(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT)
		== BQ2589X_ADC_CONTINUE_ENABLE)
		return 0; /*is doing continuous scan*/

	if (oneshot)
		ret = bq2589x_update_bits(bq,
		BQ2589X_REG_02,
		BQ2589X_CONV_START_MASK,
		BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	else
		ret = bq2589x_update_bits(bq,
		BQ2589X_REG_02,
		BQ2589X_CONV_RATE_MASK,
		BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_start);

int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0E);
	if (ret < 0) {
		dev_info(bq->dev, "read battery voltage failed :%d\n", ret);
		return ret;
	}

	volt = BQ2589X_BATV_BASE
		+ ((val & BQ2589X_BATV_MASK) >> BQ2589X_BATV_SHIFT)
		* BQ2589X_BATV_LSB;

	return volt;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_battery_volt);


int bq2589x_adc_read_sys_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0F);
	if (ret < 0) {
		dev_info(bq->dev, "read system voltage failed :%d\n", ret);
		return ret;
	}

	volt = BQ2589X_SYSV_BASE
		+ ((val & BQ2589X_SYSV_MASK) >> BQ2589X_SYSV_SHIFT)
		* BQ2589X_SYSV_LSB;

	return volt;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_sys_volt);

int bq2589x_adc_read_vbus_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_11);
	if (ret < 0) {
		dev_info(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	}
	volt = BQ2589X_VBUSV_BASE
		+ ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT)
		* BQ2589X_VBUSV_LSB;

	return volt;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_vbus_volt);

int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_10);
	if (ret < 0) {
		dev_info(bq->dev, "read temperature failed :%d\n", ret);
		return ret;
	}
	temp = BQ2589X_TSPCT_BASE
		+ ((val & BQ2589X_TSPCT_MASK) >> BQ2589X_TSPCT_SHIFT)
		* BQ2589X_TSPCT_LSB;

	return temp;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_temperature);

int bq2589x_adc_read_charge_current(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_12);
	if (ret < 0) {
		dev_info(bq->dev, "read charge current failed :%d\n", ret);
		return ret;
	}
	volt = (int)(BQ2589X_ICHGR_BASE
		+ ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT)
		* BQ2589X_ICHGR_LSB);

	return volt;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_charge_current);

int bq2589x_set_chargecurrent(struct bq2589x *bq, u32 curr)
{
	u8 ichg;

	if (bq->fixed_charge_current > 0)
		curr = bq->fixed_charge_current;
	ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
	return bq2589x_update_bits(bq,
		BQ2589X_REG_04,
		BQ2589X_ICHG_MASK,
		ichg << BQ2589X_ICHG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_chargecurrent);

int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;

	iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;

	return bq2589x_update_bits(bq,
		BQ2589X_REG_05,
		BQ2589X_ITERM_MASK,
		iterm << BQ2589X_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_term_current);


int bq2589x_set_prechg_current(struct bq2589x *bq, int curr)
{
	u8 iprechg;

	iprechg = (curr - BQ2589X_IPRECHG_BASE) / BQ2589X_IPRECHG_LSB;

	return bq2589x_update_bits(bq,
		BQ2589X_REG_05,
		BQ2589X_IPRECHG_MASK,
		iprechg << BQ2589X_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_prechg_current);

int bq2589x_set_chargevoltage(struct bq2589x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2589X_VREG_BASE)/BQ2589X_VREG_LSB;
	return bq2589x_update_bits(bq,
		BQ2589X_REG_06,
		BQ2589X_VREG_MASK,
		val << BQ2589X_VREG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_chargevoltage);


int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2589X_VINDPM_BASE) / BQ2589X_VINDPM_LSB;
	return bq2589x_update_bits(bq,
		BQ2589X_REG_0D,
		BQ2589X_VINDPM_MASK,
		val << BQ2589X_VINDPM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_input_volt_limit);

int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr)
{
	u8 val;

	if (bq->fixed_input_current > 0)
		curr = bq->fixed_input_current;
	val = (curr - BQ2589X_IINLIM_BASE) / BQ2589X_IINLIM_LSB;
	return bq2589x_update_bits(bq,
		BQ2589X_REG_00,
		BQ2589X_IINLIM_MASK,
		val << BQ2589X_IINLIM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_input_current_limit);


int bq2589x_set_vindpm_offset(struct bq2589x *bq, int offset)
{
	u8 val;

	val = (offset - BQ2589X_VINDPMOS_BASE)/BQ2589X_VINDPMOS_LSB;
	return bq2589x_update_bits(bq,
		BQ2589X_REG_01,
		BQ2589X_VINDPMOS_MASK,
		val << BQ2589X_VINDPMOS_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_vindpm_offset);

int bq2589x_get_charging_status(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		dev_info(bq->dev, "%s Failed to read register 0x0b:%d\n", __func__, ret);
		return ret;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	return val;
}
EXPORT_SYMBOL_GPL(bq2589x_get_charging_status);

int bq2589x_set_otg(struct charger_device *chg_dev, bool enable)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_info("set otg %d\n", enable);
	if (enable) {
		ret = bq2589x_enable_otg(bq);
		if (ret < 0) {
			dev_info(bq->dev, "%s:Failed to enable otg-%d\n", __func__, ret);
			return ret;
		}
	} else {
		ret = bq2589x_disable_otg(bq);
		if (ret < 0) {
			dev_info(bq->dev, "%s:Failed to disable otg-%d\n", __func__, ret);
			return ret;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_set_otg);

int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout)
{
	return bq2589x_update_bits(bq,
		BQ2589X_REG_07,
		BQ2589X_WDT_MASK,
		(u8)((timeout - BQ2589X_WDT_BASE) / BQ2589X_WDT_LSB) << BQ2589X_WDT_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_watchdog_timer);

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_disable_watchdog_timer);

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_reset_watchdog_timer);

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	int ret, timeout = 50;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_FORCE_DPDM_MASK, val);
	if (ret)
		return ret;

	while (timeout >= 0) {
		ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_02);
		if (ret)
			return ret;
		if (!(val&BQ2589X_FORCE_DPDM_MASK))
			break;
		msleep(20);
		timeout--;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_force_dpdm);

int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14, BQ2589X_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_reset_chip);

int bq2589x_enter_ship_mode(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_BATFET_OFF << BQ2589X_BATFET_DIS_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_BATFET_DIS_MASK, val);
	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enter_ship_mode);

int bq2589x_enter_hiz_mode(struct bq2589x *bq)
{
	u8 val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_enter_hiz_mode);

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_exit_hiz_mode);

int bq2589x_get_hiz_mode(struct bq2589x *bq, u8 *state)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_00);
	if (ret)
		return ret;
	*state = (val & BQ2589X_ENHIZ_MASK) >> BQ2589X_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_get_hiz_mode);


int bq2589x_pumpx_enable(struct bq2589x *bq, int enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_PUMPX_ENABLE << BQ2589X_EN_PUMPX_SHIFT;
	else
		val = BQ2589X_PUMPX_DISABLE << BQ2589X_EN_PUMPX_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_04, BQ2589X_EN_PUMPX_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_enable);

int bq2589x_pumpx_increase_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_UP << BQ2589X_PUMPX_UP_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_UP_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_increase_volt);

int bq2589x_pumpx_increase_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if (ret)
		return ret;

	if (val & BQ2589X_PUMPX_UP_MASK)
		return 1;   /* not finished*/
	else
		return 0;   /* pumpx up finished*/

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_increase_volt_done);

int bq2589x_pumpx_decrease_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_DOWN << BQ2589X_PUMPX_DOWN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_DOWN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_decrease_volt);

int bq2589x_pumpx_decrease_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if (ret)
		return ret;

	if (val & BQ2589X_PUMPX_DOWN_MASK)
		return 1;   /* not finished*/
	else
		return 0;   /* pumpx down finished*/

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_decrease_volt_done);

static int bq2589x_force_ico(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_FORCE_ICO << BQ2589X_FORCE_ICO_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_FORCE_ICO_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_force_ico);

static int bq2589x_check_force_ico_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_14);
	if (ret)
		return ret;

	if (val & BQ2589X_ICO_OPTIMIZED_MASK)
		return 1;  /*finished*/
	else
		return 0;   /* in progress*/
}
EXPORT_SYMBOL_GPL(bq2589x_check_force_ico_done);

static int bq2589x_enable_term(struct bq2589x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_TERM_ENABLE << BQ2589X_EN_TERM_SHIFT;
	else
		val = BQ2589X_TERM_DISABLE << BQ2589X_EN_TERM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_term);

static int bq2589x_enable_auto_dpdm(struct bq2589x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_AUTO_DPDM_ENABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;
	else
		val = BQ2589X_AUTO_DPDM_DISABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_AUTO_DPDM_EN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enable_auto_dpdm);

static int bq2589x_use_absolute_vindpm(struct bq2589x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_FORCE_VINDPM_ENABLE << BQ2589X_FORCE_VINDPM_SHIFT;
	else
		val = BQ2589X_FORCE_VINDPM_DISABLE << BQ2589X_FORCE_VINDPM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_0D, BQ2589X_FORCE_VINDPM_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_use_absolute_vindpm);

static int bq2589x_enable_ico(struct bq2589x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_ICO_ENABLE << BQ2589X_ICOEN_SHIFT;
	else
		val = BQ2589X_ICO_DISABLE << BQ2589X_ICOEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_ICOEN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enable_ico);


static int bq2589x_read_idpm_limit(struct bq2589x *bq)
{
	uint8_t val;
	int curr;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_13);
	if (ret < 0) {
		dev_info(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	}

	curr = BQ2589X_IDPM_LIM_BASE
		+ ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT)
		* BQ2589X_IDPM_LIM_LSB;
	return curr;
}
EXPORT_SYMBOL_GPL(bq2589x_read_idpm_limit);

static bool bq2589x_is_charge_done(struct bq2589x *bq)
{
	int ret;
	u8 val;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		dev_info(bq->dev, "%s:read REG0B failed :%d\n", __func__, ret);
		return false;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;

	return (val == BQ2589X_CHRG_STAT_CHGDONE);
}
EXPORT_SYMBOL_GPL(bq2589x_is_charge_done);

static int bq2589x_set_hz(struct bq2589x *bq, bool en)
{
	if (en) {
		bq2589x_enter_hiz_mode(bq);
	} else {
		bq2589x_exit_hiz_mode(bq);
		bq2589x_set_input_current_limit(bq, 2050);
	}

	return 0;
}

#ifdef CONFIG_CUSTOMER_SUPPORT
static int bq2589x_enable_shipping_mode(struct charger_device *chg_dev, bool val)
{
	int ret;
	u8 temp;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (val) {
		temp = BQ2589X_BATFET_OFF << BQ2589X_BATFET_DIS_SHIFT;
		ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_BATFET_DIS_MASK, temp);
		dev_info(bq->dev, "%s: enable shipping mode,dump registers\n", __func__);
		bq2589x_dump_regs(bq);
	}
	return ret;
}
#endif

static int bq2589x_update_chg_type(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	union power_supply_propval propval;
	struct power_supply *chrdet_psy;
	enum charger_type chg_type = CHARGER_UNKNOWN;
	int wait_cdp_cnt = 200, wait_plugin_cnt = 5;
	static bool first_connect = true;

	chrdet_psy = power_supply_get_by_name("charger");
	if (!chrdet_psy) {
		pr_notice("[%s]: get power supply failed\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&bq2589x_type_det_lock);
	if (en) {
		if (first_connect) {
			while (wait_cdp_cnt >= 0) {
				if (is_usb_rdy()) {
					pr_info("CDP, PASS\n");
					break;
				}
				pr_info("CDP, block\n");
				msleep(100);
				wait_cdp_cnt--;
			}
			if (wait_cdp_cnt <= 0)
				pr_info("CDP, timeout\n");
			else
				pr_info("CDP, free\n");
		}
		Charger_Detect_Init();
		while (wait_plugin_cnt >= 0) {
			if (bq->status&BQ2589X_STATUS_PLUGIN)
				break;
			msleep(100);
			wait_plugin_cnt--;
		}
		bq2589x_force_dpdm(bq);
		bq->vbus_type = bq2589x_get_vbus_type(bq);
		switch ((int)bq->vbus_type) {
		case BQ2589X_VBUS_USB_SDP:
			chg_type = STANDARD_HOST;
			break;
		case BQ2589X_VBUS_USB_CDP:
			chg_type = CHARGING_HOST;
			break;
		case BQ2589X_VBUS_USB_DCP:
		case BQ2589X_VBUS_MAXC:
			chg_type = STANDARD_CHARGER;
			break;
		case BQ2589X_VBUS_NONSTAND:
		case BQ2589X_VBUS_UNKNOWN:
			chg_type = NONSTANDARD_CHARGER;
			break;
		default:
			chg_type = CHARGER_UNKNOWN;
			break;
		}
	}

	if (chg_type != STANDARD_CHARGER) {
		Charger_Detect_Release();

#ifdef CONFIG_CUSTOMER_SUPPORT
	} else if (propval.intval != CHARGER_PD_12V) {
		//schedule_delayed_work(&bq->ico_work, 0);
		if (pe.tune_done || pe.tune_fail) {
			bq2589x_reset_pe_param();
			schedule_delayed_work(&bq->check_pe_tuneup_work, 0);
		}
#endif
	}

	//if (chg_type!=CHARGER_UNKNOWN && bq->cfg.use_absolute_vindpm)
	//	bq2589x_adjust_absolute_vindpm(bq);

	pr_info("[%s] en:%d vbus_type:%d chg_type:%d\n", __func__, en, bq->vbus_type, chg_type);
	ret = power_supply_get_property(chrdet_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0) {
		pr_info("[%s]: get charge type failed, ret = %d\n", __func__, ret);
		mutex_unlock(&bq2589x_type_det_lock);
		return ret;
	}
#ifdef CONFIG_CUSTOMER_SUPPORT
	if (propval.intval != CHARGER_PD_12V && propval.intval != CHARGER_PD_9V
		&& propval.intval != CHARGER_PE_12V && propval.intval != CHARGER_PE_9V) {
		propval.intval = chg_type;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
		if (ret < 0) {
			pr_info("[%s]: set charge type failed, ret = %d\n", __func__, ret);
			mutex_unlock(&bq2589x_type_det_lock);
			return ret;
		}
	}
#else
	propval.intval = chg_type;
	ret = power_supply_set_property(chrdet_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0) {
		pr_info("[%s]: set charge type failed, ret = %d\n", __func__, ret);
		mutex_unlock(&bq2589x_type_det_lock);
		return ret;
	}

#endif
	mutex_unlock(&bq2589x_type_det_lock);
	return 0;
}

static int bq2589x_run_aicl(struct charger_device *chg_dev, u32 *uA)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret, wait_ico_cnt = 30;
	u8 status;

	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
	if (!ret) {
		if (status & BQ2589X_VDPM_STAT_MASK) {
			bq->status |= BQ2589X_STATUS_VINDPM;
			dev_info(bq->dev, "%s:VINDPM occurred\n", __func__);
		} else {
			bq->status &= ~BQ2589X_STATUS_VINDPM;
		}
	} else {
		return ret;
	}
	if (bq->status & BQ2589X_STATUS_VINDPM) {
		ret = bq2589x_force_ico(bq);
		if (ret < 0) {
			dev_info(bq->dev, "%s:ICO command issued failed:%d\n", __func__, ret);
			return ret;
		}
		while (wait_ico_cnt >= 0) {
			ret = bq2589x_check_force_ico_done(bq);
			if (ret) {/*ico done*/
				ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
				if (!ret) {
					*uA = ((status & BQ2589X_IDPM_LIM_MASK))
						* BQ2589X_IDPM_LIM_LSB
						+ BQ2589X_IDPM_LIM_BASE * 1000;
					dev_info(bq->dev, "%s:ICO done, result is:%d uA\n",
							__func__, *uA);
				}
				return ret;
			}
			msleep(100);
			wait_ico_cnt--;
		}
	}

	return ret;
}

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;

    /*common initialization*/

	bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_HVDCPEN_MASK, 0);
	bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_MAXCEN_MASK, 0);

	bq2589x_disable_watchdog_timer(bq);

	bq2589x_enable_auto_dpdm(bq, bq->cfg.enable_auto_dpdm);
	bq2589x_enable_term(bq, bq->cfg.enable_term);
	bq2589x_enable_ico(bq, bq->cfg.enable_ico);
	/*force use absolute vindpm if auto_dpdm not enabled*/
	if (!bq->cfg.enable_auto_dpdm)
		bq->cfg.use_absolute_vindpm = true;
	bq2589x_use_absolute_vindpm(bq, bq->cfg.use_absolute_vindpm);


	ret = bq2589x_set_vindpm_offset(bq, 600);
	if (ret < 0) {
		dev_info(bq->dev, "%s:Failed to set vindpm offset:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_term_current(bq, bq->cfg.term_current);
	if (ret < 0) {
		dev_info(bq->dev, "%s:Failed to set termination current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_chargevoltage(bq, bq->cfg.charge_voltage);
	if (ret < 0) {
		dev_info(bq->dev, "%s:Failed to set charge voltage:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_input_current_limit(bq, 1500);
	if (ret < 0) {
		dev_info(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_chargecurrent(bq, bq->cfg.charge_current);
	if (ret < 0) {
		dev_info(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_enable_charger(bq);
	if (ret < 0) {
		dev_info(bq->dev, "%s:Failed to enable charger:%d\n", __func__, ret);
		return ret;
	}

	bq2589x_adc_start(bq, false);

	ret = bq2589x_pumpx_enable(bq, 1);
	if (ret) {
		dev_info(bq->dev, "%s:Failed to enable pumpx:%d\n", __func__, ret);
		return ret;
	}

	bq2589x_set_watchdog_timer(bq, 160);

	bq2589x_set_hz(bq, 0);


	return ret;
}


static int bq2589x_charge_status(struct bq2589x *bq)
{
	u8 val = 0;

	bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	switch (val) {
	case BQ2589X_CHRG_STAT_FASTCHG:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case BQ2589X_CHRG_STAT_PRECHG:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	case BQ2589X_CHRG_STAT_CHGDONE:
	case BQ2589X_CHRG_STAT_IDLE:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	default:
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static enum power_supply_property bq2589x_charger_props[] = {
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
};


static int bq2589x_usb_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{

	struct bq2589x *bq = power_supply_get_drvdata(psy);
	int voltage;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		voltage = bq2589x_adc_read_vbus_volt(bq);
		if (voltage > 4400)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = bq2589x_charge_status(bq);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq2589x_psy_register(struct bq2589x *bq)
{
	int ret;

	bq->usb.name = "bq2589x";
	bq->usb.type = POWER_SUPPLY_TYPE_USB;
	bq->usb.properties = bq2589x_charger_props;
	bq->usb.num_properties = ARRAY_SIZE(bq2589x_charger_props);
	bq->usb.get_property = bq2589x_usb_get_property;
	bq->usb.external_power_changed = NULL;

	bq->usb_cfg.drv_data = bq;
	bq->usb_cfg.of_node = bq->dev->of_node;

	bq->usb_psy = power_supply_register(bq->dev, &bq->usb, &bq->usb_cfg);
	if (IS_ERR(bq->usb_psy)) {
		ret = PTR_ERR(bq->usb_psy);
		dev_info(bq->dev, "%s:failed to register usb psy:%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void bq2589x_psy_unregister(struct bq2589x *bq)
{
	power_supply_unregister(bq->usb_psy);
}

static int bq2589x_parse_dt(struct device *dev, struct bq2589x *bq)
{
	int ret;
	struct device_node *np = dev->of_node;

	if (of_property_read_string(np, "charger_name", &bq->chg_dev_name) < 0) {
		bq->chg_dev_name = "primary_chg";
		dev_info(bq->dev, "%s:failed to read charger name\n", __func__);
	}
	ret = of_property_read_u32(np, "ti,bq2589x,vbus-volt-high-level", &pe.high_volt_level);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,vbus-volt-low-level", &pe.low_volt_level);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,vbat-min-volt-to-tuneup", &pe.vbat_min_volt);
	if (ret)
		return ret;

	bq->cfg.enable_auto_dpdm = of_property_read_bool(np, "ti,bq2589x,enable-auto-dpdm");
	bq->cfg.enable_term = of_property_read_bool(np, "ti,bq2589x,enable-termination");
	bq->cfg.enable_ico = of_property_read_bool(np, "ti,bq2589x,enable-ico");
	bq->cfg.use_absolute_vindpm = of_property_read_bool(np, "ti,bq2589x,use-absolute-vindpm");

	ret = of_property_read_u32(np, "ti,bq2589x,charge-voltage", &bq->cfg.charge_voltage);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current", &bq->cfg.charge_current);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,term-current", &bq->cfg.term_current);
	if (ret)
		return ret;

	return 0;
}

static int bq2589x_detect_device(struct bq2589x *bq)
{
	int ret;
	u8 data;

	ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_14);
	if (ret == 0) {
		bq->part_no = (data & BQ2589X_PN_MASK) >> BQ2589X_PN_SHIFT;
		bq->revision = (data & BQ2589X_DEV_REV_MASK) >> BQ2589X_DEV_REV_SHIFT;
	}

	return ret;
}

static int bq2589x_read_batt_rsoc(struct bq2589x *bq)
{
	union power_supply_propval ret = {0,};

	if (!bq->batt_psy)
		bq->batt_psy = power_supply_get_by_name("battery");

	if (bq->batt_psy) {
		power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	} else {
		return 50;
	}
}

#ifdef CONFIG_CUSTOMER_SUPPORT
static void bq2589x_adjust_absolute_vindpm(struct bq2589x *bq)
{
	u16 vbus_volt;
	u16 vindpm_volt;
	int ret;

	ret = bq2589x_disable_charger(bq);
	if (ret < 0) {
		dev_info(bq->dev, "%s:failed to disable charger\n", __func__);
		//return;
	}
	//wait for new adc data
	msleep(1000);
	vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	ret = bq2589x_enable_charger(bq);
	if (ret < 0) {
		dev_info(bq->dev, "%s:failed to enable charger\n", __func__);
		return;
	}

	if (vbus_volt < 6000)
		vindpm_volt = vbus_volt - 600;
	else
		vindpm_volt = vbus_volt - 1200;
	ret = bq2589x_set_input_volt_limit(bq, vindpm_volt);
	if (ret < 0)
		dev_info(bq->dev, "%s:Set absolute vindpm threshold %d Failed:%d\n",
		__func__, vindpm_volt, ret);
	else
		dev_info(bq->dev, "%s:Set absolute vindpm threshold %d successfully\n",
		__func__, vindpm_volt);

}
#endif

static void bq2589x_adapter_in_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_in_work);
	int ret;
	struct power_supply *chrdet_psy;
	union power_supply_propval propval;

	chrdet_psy = power_supply_get_by_name("charger");
	if (chrdet_psy) {
		propval.intval = 1;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_ONLINE, &propval);
		if (ret) {
			dev_info(bq->dev, "[%s] set chr_online failed:%d\n", __func__, ret);
			return;
		}
		ret = bq2589x_update_chg_type(bq->chg_dev, true);
		if (ret)
			dev_info(bq->dev, "[%s] update chr_type failed:%d\n", __func__, ret);

		bq2589x_set_hz(bq, 0);
	} else {
		dev_info(bq->dev, "[%s] get chrdet_psy failed:%d\n", __func__, ret);
	}
	if (pe.enable)
		schedule_delayed_work(&bq->monitor_work, 0);
}

static void bq2589x_adapter_out_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_out_work);
	int ret;
	struct power_supply *chrdet_psy;
	union power_supply_propval propval;

	ret = bq2589x_set_input_volt_limit(bq, 4600);
	if (ret < 0)
		dev_info(bq->dev, "%s:reset vindpm threshold to 4400 failed:%d\n", __func__, ret);
	else
		dev_info(bq->dev, "%s:reset vindpm threshold to 4400 successfully\n", __func__);

	if (pe.enable)
		cancel_delayed_work_sync(&bq->monitor_work);

	cancel_delayed_work_sync(&bq->pe_volt_tune_work);
	bq2589x_reset_pe_param();
	__pm_relax(bq->pe_tune_wakelock);

	chrdet_psy = power_supply_get_by_name("charger");
	if (chrdet_psy) {
		propval.intval = CHARGER_UNKNOWN;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
		if (ret)
			dev_info(bq->dev, "[%s] reset chr_type failed:%d\n", __func__, ret);
		propval.intval = 0;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_ONLINE, &propval);
		if (ret)
			dev_info(bq->dev, "[%s] reset chr_online failed:%d\n", __func__, ret);
	} else {
		dev_info(bq->dev, "[%s] get chrdet_psy failed:%d\n", __func__, ret);
	}
}

static void bq2589x_ico_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, ico_work.work);
	int ret;
	int idpm;
	u8 status;
	static bool ico_issued;

	if (!ico_issued) {
		ret = bq2589x_force_ico(bq);
		if (ret < 0) {
			schedule_delayed_work(&bq->ico_work, HZ); /* retry 1 second later*/
			dev_info(bq->dev, "%s:ICO command issued failed:%d\n", __func__, ret);
		} else {
			ico_issued = true;
			schedule_delayed_work(&bq->ico_work, 3 * HZ);
			dev_info(bq->dev, "%s:ICO command issued successfully\n", __func__);
		}
	} else {
		ico_issued = false;
		ret = bq2589x_check_force_ico_done(bq);
		if (ret) {/*ico done*/
			ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
			if (ret == 0) {
				idpm = ((status & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT);
				idpm *= BQ2589X_IDPM_LIM_LSB + BQ2589X_IDPM_LIM_BASE;
				dev_info(bq->dev, "%s:ICO done, result is:%d mA\n", __func__, idpm);
			}
		}
	}
}

static void bq2589x_check_pe_tuneup_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, check_pe_tuneup_work.work);

	if (!pe.enable) {
		schedule_delayed_work(&bq->ico_work, 0);
		return;
	}

	bq->vbat_volt = bq2589x_adc_read_battery_volt(bq);
	bq->rsoc = bq2589x_read_batt_rsoc(bq);

	dev_info(bq->dev, "%s:vbat = %d, bq->rsoc = %d, high_volt_level = %d\n",
			__func__, bq->vbat_volt, bq->rsoc, pe.high_volt_level);
	if (bq->vbat_volt > pe.vbat_min_volt) {
		dev_info(bq->dev, "%s:trying to tune up vbus voltage\n", __func__);
		pe.target_volt = 12000;
		pe.tune_up_volt = true;
		pe.tune_down_volt = false;
		pe.tune_done = false;
		pe.tune_count = 0;
		pe.tune_fail = false;
		bq->fixed_input_current = 1666666;//avoid adapter overload
		bq->fixed_charge_current = 2500000;//avoid adapter overload
		bq2589x_set_input_current_limit(bq, bq->fixed_input_current);
		bq2589x_set_chargecurrent(bq, bq->fixed_charge_current);
		schedule_delayed_work(&bq->pe_volt_tune_work, 0);
		if (!bq->pe_tune_wakelock->active)
			__pm_stay_awake(bq->pe_tune_wakelock);
	} else {
		/* wait battery voltage up enough to check again */
		schedule_delayed_work(&bq->check_pe_tuneup_work, 2*HZ);
	}
}

static void bq2589x_report_fchg_type(struct bq2589x *bq)
{
	struct power_supply *chrdet_psy;
	union power_supply_propval propval;
	int ret;

	bq->fixed_input_current = -1;
	bq->fixed_charge_current = -1;
	chrdet_psy = power_supply_get_by_name("charger");
	if (!chrdet_psy)
		return;
	ret = power_supply_get_property(chrdet_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret)
		return;
	dev_info(bq->dev, "%s:charge_type:%d, vbus:%d\n", __func__,
			propval.intval, bq->vbus_volt);
#ifdef CONFIG_CUSTOMER_SUPPORT
	if (bq->vbus_volt >= 11500 && propval.intval != CHARGER_PE_12V
		&& propval.intval != CHARGER_PD_12V) {
		propval.intval = CHARGER_PE_12V;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	} else if (bq->vbus_volt >= 8500 && bq->vbus_volt < 11500
			&& propval.intval != CHARGER_PD_9V
			&& propval.intval != CHARGER_PE_9V) {
		propval.intval = CHARGER_PE_9V;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	}
#endif
}

static void bq2589x_tune_volt_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, pe_volt_tune_work.work);
	int ret;
	static bool pumpx_cmd_issued;

	if (!(bq->status&BQ2589X_STATUS_PLUGIN)) {
		pumpx_cmd_issued = false;
		__pm_relax(bq->pe_tune_wakelock);
		return;
	}
	bq2589x_dump_regs(bq);
	bq->vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	dev_info(bq->dev, "%s:count:%d, vbus:%d,target:%d\n", __func__, pe.tune_count,
			bq->vbus_volt, pe.target_volt);
	if ((pe.tune_up_volt && bq->vbus_volt >= (pe.target_volt-200)) ||
	    (pe.tune_down_volt && bq->vbus_volt <= (pe.target_volt+200))) {

		dev_info(bq->dev, "%s:voltage tune successfully\n", __func__);
		//bq2589x_adjust_absolute_vindpm(bq);

		//if (pe.tune_up_volt)
		//	schedule_delayed_work(&bq->ico_work, 0);

		bq2589x_report_fchg_type(bq);
		pe.tune_done = true;
		__pm_relax(bq->pe_tune_wakelock);
		return;
	}

	if (pe.tune_count > 10) {
		dev_info(bq->dev, "%s:voltage tune failed,reach max retry count\n", __func__);
		//bq2589x_adjust_absolute_vindpm(bq);


		//if (pe.tune_up_volt)
		//	schedule_delayed_work(&bq->ico_work, 0);

		bq2589x_report_fchg_type(bq);
		pe.tune_fail = true;
		__pm_relax(bq->pe_tune_wakelock);
		return;
	}

	if (!pumpx_cmd_issued) {
		ret = 0;
		if (pe.tune_up_volt && (bq->vbus_volt < pe.target_volt)) {
			dev_info(bq->dev, "%s:pumpx_increase_volt\n", __func__);
			ret = bq2589x_pumpx_increase_volt(bq);
		} else if (pe.tune_down_volt && bq->vbus_volt > pe.target_volt) {
			dev_info(bq->dev, "%s:pumpx_decrease_volt\n", __func__);
			ret =  bq2589x_pumpx_decrease_volt(bq);
		}
		if (ret) {
			dev_info(bq->dev, "%s: reschedule tune work\n", __func__);
			schedule_delayed_work(&bq->pe_volt_tune_work, 4*HZ);
		} else {
			dev_info(bq->dev, "%s:pumpx command issued.\n", __func__);
			pumpx_cmd_issued = true;
			pe.tune_count++;
			schedule_delayed_work(&bq->pe_volt_tune_work, 3*HZ);
		}
	} else {
		if (pe.tune_up_volt)
			ret = bq2589x_pumpx_increase_volt_done(bq);
		else if (pe.tune_down_volt)
			ret = bq2589x_pumpx_decrease_volt_done(bq);
		if (!ret) {
			dev_info(bq->dev, "%s:pumpx command finishedd!\n", __func__);
			//bq2589x_adjust_absolute_vindpm(bq);
			pumpx_cmd_issued = 0;
		}
		schedule_delayed_work(&bq->pe_volt_tune_work, HZ);
	}
	if (!(bq->status&BQ2589X_STATUS_PLUGIN)) {
		pumpx_cmd_issued = false;
		cancel_delayed_work_sync(&bq->pe_volt_tune_work);
		__pm_relax(bq->pe_tune_wakelock);
	}
}


static void bq2589x_monitor_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, monitor_work.work);
	u8 status = 0;
	int ret;
	int chg_current;

	bq2589x_reset_watchdog_timer(bq);

	bq->rsoc = bq2589x_read_batt_rsoc(bq);

	bq->vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	bq->vbat_volt = bq2589x_adc_read_battery_volt(bq);
	chg_current = bq2589x_adc_read_charge_current(bq);

	dev_info(bq->dev, "%s:vbus volt:%d,vbat volt:%d,charge current:%d\n", __func__,
				bq->vbus_volt,
				bq->vbat_volt,
				chg_current);

	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
	if (!ret) {
		if (status & BQ2589X_VDPM_STAT_MASK) {
			bq->status |= BQ2589X_STATUS_VINDPM;
			dev_info(bq->dev, "%s:VINDPM occurred\n", __func__);
		} else {
			bq->status &= ~BQ2589X_STATUS_VINDPM;
		}
		if (status & BQ2589X_IDPM_STAT_MASK) {
			bq->status |= BQ2589X_STATUS_IINDPM;
			dev_info(bq->dev, "%s:IINDPM occurred\n", __func__);
		} else {
			bq->status &= ~BQ2589X_STATUS_IINDPM;
		}
	}

#ifdef CONFIG_CUSTOMER_SUPPORT
	if (bq->vbus_type == BQ2589X_VBUS_USB_DCP &&
		bq->vbus_volt > pe.high_volt_level &&
		bq->rsoc > 95 && !pe.tune_down_volt)  {

		pe.tune_down_volt = true;
		pe.tune_up_volt = false;
		pe.target_volt = pe.low_volt_level;
		pe.tune_done = false;
		pe.tune_count = 0;
		pe.tune_fail = false;
		schedule_delayed_work(&bq->pe_volt_tune_work, 0);
	}
#endif

	/* read temperature,or any other check if need to decrease charge current*/

	schedule_delayed_work(&bq->monitor_work, 10 * HZ);
}

static void bq2589x_charger_irq_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, irq_work);
	u8 status = 0;
	u8 fault = 0;
	u8 charge_status = 0;
	u8 temp = 0;
	int ret;

	mdelay(100);

	/* Read STATUS and FAULT registers */
	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_0B);
	if (ret)
		return;

	ret = bq2589x_read_byte(bq, &fault, BQ2589X_REG_0C);
	if (ret)
		return;

	ret = bq2589x_read_byte(bq, &temp, BQ2589X_REG_11);
	if (ret)
		return;

	bq->vbus_type = (status & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT;

	bq2589x_dump_regs(bq);
	dev_info(bq->dev, "%s:bq status = %.2x, bq->vbus_type = %.2x\n",
			__func__, bq->status, bq->vbus_type);
	if (!(temp & BQ2589X_VBUS_GD_MASK) && (bq->status & BQ2589X_STATUS_PLUGIN)) {
		dev_info(bq->dev, "%s:adapter removed\n", __func__);
		bq->status &= ~BQ2589X_STATUS_PLUGIN;
		schedule_work(&bq->adapter_out_work);
	} else if ((temp & BQ2589X_VBUS_GD_MASK) && !(bq->status & BQ2589X_STATUS_PLUGIN)) {
		dev_info(bq->dev, "%s:adapter plugged in\n", __func__);
		bq->status |= BQ2589X_STATUS_PLUGIN;
		schedule_work(&bq->adapter_in_work);
	}

	if ((status & BQ2589X_PG_STAT_MASK) && !(bq->status & BQ2589X_STATUS_PG))
		bq->status |= BQ2589X_STATUS_PG;
	else if (!(status & BQ2589X_PG_STAT_MASK) && (bq->status & BQ2589X_STATUS_PG))
		bq->status &= ~BQ2589X_STATUS_PG;

	if (fault && !(bq->status & BQ2589X_STATUS_FAULT))
		bq->status |= BQ2589X_STATUS_FAULT;
	else if (!fault && (bq->status & BQ2589X_STATUS_FAULT))
		bq->status &= ~BQ2589X_STATUS_FAULT;

	charge_status = (status & BQ2589X_CHRG_STAT_MASK) >> BQ2589X_CHRG_STAT_SHIFT;
	if (charge_status == BQ2589X_CHRG_STAT_IDLE)
		dev_info(bq->dev, "%s:not charging\n", __func__);
	else if (charge_status == BQ2589X_CHRG_STAT_PRECHG)
		dev_info(bq->dev, "%s:precharging\n", __func__);
	else if (charge_status == BQ2589X_CHRG_STAT_FASTCHG)
		dev_info(bq->dev, "%s:fast charging\n", __func__);
	else if (charge_status == BQ2589X_CHRG_STAT_CHGDONE)
		dev_info(bq->dev, "%s:charge done!\n", __func__);

	if (fault)
		dev_info(bq->dev, "%s:charge fault:%02x\n", __func__, fault);
}


static irqreturn_t bq2589x_charger_interrupt(int irq, void *data)
{
	struct bq2589x *bq = data;

	dev_info(bq->dev, "in %s\n", __func__);
	schedule_work(&bq->irq_work);
	return IRQ_HANDLED;
}

static struct charger_ops bq2589x_chg_ops = {
	/*normal charging*/
	.plug_in = bq2589x_plug_in,
	.plug_out = bq2589x_plug_out,
	.dump_registers = bq2589x_dump_register,
	.enable = bq2589x_enable_charging,
	.is_enabled = bq2589x_is_charging_enable,
	.get_charging_current = bq2589x_get_ichg,
	.set_charging_current = bq2589x_set_ichg,
	.get_input_current = bq2589x_get_icl,
	.set_input_current = bq2589x_set_icl,
	.get_constant_voltage = bq2589x_get_vchg,
	.set_constant_voltage = bq2589x_set_vchg,
	.kick_wdt = bq2589x_kick_wdt,
	.set_mivr = bq2589x_set_ivl,
	.is_charging_done = bq2589x_is_charging_done,
	.get_min_charging_current = bq2589x_get_min_ichg,
	.enable_chg_type_det = bq2589x_update_chg_type,
	.run_aicl = bq2589x_run_aicl,

	/* Safety timer */
	.enable_safety_timer = bq2589x_set_safety_timer,
	.is_safety_timer_enabled = bq2589x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = bq2589x_set_otg,
	.set_boost_current_limit = bq2589x_set_otg_current,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.enable_hz = bq2589x_set_hz_mode,
	.event = bq2589x_do_event,

	/* Shipping mode */
	//.enable_shipping_mode = bq2589x_enable_shipping_mode,
};

static int bq2589x_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct bq2589x *bq;
	int irqn;
	int ret;

	bq = devm_kzalloc(&client->dev, sizeof(struct bq2589x), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;
	i2c_set_clientdata(client, bq);

	ret = bq2589x_detect_device(bq);
	if (!ret) {
		bq->status |= BQ2589X_STATUS_EXIST;
		dev_info(bq->dev, "%s: charger device bq25890 detected, revision:%d\n",
			__func__, bq->revision);
	} else {
		dev_info(bq->dev, "%s: no bq25890 charger device found:%d\n", __func__, ret);
		return -ENODEV;
	}

	bq->batt_psy = power_supply_get_by_name("battery");
	bq->fixed_input_current = -1;
	bq->fixed_charge_current = -1;

	pe.tune_done = true;
	g_bq = bq;

	if (client->dev.of_node)
		bq2589x_parse_dt(&client->dev, bq);

	/*Register charger device*/
	bq->chg_props.alias_name = "bq2589x",
	bq->chg_dev = charger_device_register(bq->chg_dev_name,
	&client->dev, bq, &bq2589x_chg_ops, &bq->chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		dev_info(bq->dev, "%s: Register charger device failed\n", __func__);
		ret = PTR_ERR(bq->chg_dev);
		return ret;
	}

	ret = bq2589x_init_device(bq);
	if (ret) {
		dev_info(bq->dev, "device init failure: %d\n", ret);
		goto err_0;
	}

	bq2589x_irq = of_get_named_gpio(client->dev.of_node, "bq2589x_irq", 0);
	if (bq2589x_irq < 0)
		dev_info(bq->dev, "%s: %d get gpio failed\n", __func__, bq2589x_irq);

	ret = gpio_request(bq2589x_irq, "bq2589x irq pin");
	if (ret) {
		dev_info(bq->dev, "%s: %d gpio request failed\n", __func__, bq2589x_irq);
		goto err_0;
	}
	gpio_direction_input(bq2589x_irq);

	irqn = gpio_to_irq(bq2589x_irq);
	if (irqn < 0) {
		dev_info(bq->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		ret = irqn;
		goto err_1;
	}
	client->irq = irqn;

	ret = bq2589x_psy_register(bq);
	if (ret)
		goto err_0;

	bq->pe_tune_wakelock = wakeup_source_register(bq->dev, "bq25890 suspend wakelock");
	INIT_WORK(&bq->irq_work, bq2589x_charger_irq_workfunc);
	INIT_WORK(&bq->adapter_in_work, bq2589x_adapter_in_workfunc);
	INIT_WORK(&bq->adapter_out_work, bq2589x_adapter_out_workfunc);
	INIT_DELAYED_WORK(&bq->monitor_work, bq2589x_monitor_workfunc);
	INIT_DELAYED_WORK(&bq->ico_work, bq2589x_ico_workfunc);
	INIT_DELAYED_WORK(&bq->pe_volt_tune_work, bq2589x_tune_volt_workfunc);
	INIT_DELAYED_WORK(&bq->check_pe_tuneup_work, bq2589x_check_pe_tuneup_workfunc);

	ret = request_irq(client->irq, bq2589x_charger_interrupt,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "bq2589x_charger1_irq", bq);
	if (ret) {
		dev_info(bq->dev, "%s:Request IRQ %d failed: %d\n", __func__, client->irq, ret);
		goto err_irq;
	} else {
		dev_info(bq->dev, "%s:irq = %d\n", __func__, client->irq);
	}

	pe.enable = true;
	schedule_work(&bq->irq_work);/*in case of adapter has been in when power off*/
	//hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "BQ2589X");
	return 0;

err_irq:
	cancel_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_delayed_work_sync(&bq->ico_work);
	cancel_delayed_work_sync(&bq->check_pe_tuneup_work);
	if (pe.enable) {
		cancel_delayed_work_sync(&bq->pe_volt_tune_work);
		cancel_delayed_work_sync(&bq->monitor_work);
	}
err_1:
	gpio_free(bq2589x_irq);
err_0:
	g_bq = NULL;

return ret;


}

static void bq2589x_charger_shutdown(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);

	dev_info(bq->dev, "%s: shutdown\n", __func__);

	bq2589x_psy_unregister(bq);

	cancel_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_delayed_work_sync(&bq->ico_work);
	cancel_delayed_work_sync(&bq->check_pe_tuneup_work);
	if (pe.enable) {
		cancel_delayed_work_sync(&bq->pe_volt_tune_work);
		cancel_delayed_work_sync(&bq->monitor_work);
	}

	free_irq(bq->client->irq, NULL);
	gpio_free(bq2589x_irq);
	g_bq = NULL;
}

static const struct of_device_id bq2589x_charger_match_table[] = {
	{.compatible = "ti,bq2589x-1",},
	{},
};


static const struct i2c_device_id bq2589x_charger_id[] = {
	{ "bq2589x-1", BQ25890 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2589x_charger_id);

static struct i2c_driver bq2589x_charger_driver = {
	.driver		= {
		.name	= "bq2589x-1",
		.of_match_table = bq2589x_charger_match_table,
	},
	.id_table	= bq2589x_charger_id,

	.probe		= bq2589x_charger_probe,
	.shutdown   = bq2589x_charger_shutdown,
};

module_i2c_driver(bq2589x_charger_driver);

MODULE_DESCRIPTION("TI BQ2589x Charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
