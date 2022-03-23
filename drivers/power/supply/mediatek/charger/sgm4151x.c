/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <mt-plat/v1/charger_class.h>
#include <mt-plat/v1/mtk_charger.h>
#include "sgm4151x.h"

static int __sgm4151x_read_byte(struct sgm4151x_device *sgm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sgm->client, reg);
	if (ret < 0) {
		pr_info("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sgm4151x_write_byte(struct sgm4151x_device *sgm, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
	if (ret < 0) {
		pr_info("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		return ret;
	}
	return 0;
}

static int sgm4151x_read_reg(struct sgm4151x_device *sgm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm4151x_read_byte(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	return ret;
}

static int sgm4151x_update_bits(struct sgm4151x_device *sgm, u8 reg,
		u8 mask, u8 val)
{
	int ret;
	u8 tmp;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm4151x_read_byte(sgm, reg, &tmp);
	if (ret) {
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= val & mask;

	ret = __sgm4151x_write_byte(sgm, reg, tmp);
	if (ret)
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}

static int sgm4151x_check_pn(struct charger_device *chg_dev)
{
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);
	u8 reg_val;
	int ret, pn;

	ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_CTRL_B, &reg_val);
	if (ret)
		return -1;
	pn = (reg_val>>3)&0x0f;
	switch (pn) {
	case 0x2:
		//hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SGM41511");
		dev_info(sgm->dev, "[%s] pn:0010 ic:SGM41511\n", __func__);
		break;
	case 0x9:
		//hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SY6974");
		dev_info(sgm->dev, "[%s] pn:1001 ic:SY6974\n", __func__);
		break;
	default:
		//hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "UNKNOWN");
		dev_info(sgm->dev, "[%s] pn:0x%02x ic:UNKNOWN\n", __func__, pn);
		break;
	}
	return 0;
}

static int sgm4151x_set_term_curr(struct charger_device *chg_dev, int term_current)
{
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);
	int reg_val;

	if (term_current < SGM4151x_TERMCHRG_I_MIN_uA)
		term_current = SGM4151x_TERMCHRG_I_MIN_uA;
	else if (term_current > SGM4151x_TERMCHRG_I_MAX_uA)
		term_current = SGM4151x_TERMCHRG_I_MAX_uA;

	reg_val = term_current / SGM4151x_TERMCHRG_CURRENT_STEP_uA;

	return sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_3,
			SGM4151x_TERMCHRG_CUR_MASK, reg_val);
}

static int sgm4151x_set_ichrg_curr(struct charger_device *chg_dev, unsigned int chrg_curr)
{
	int ret;
	int reg_val;
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);

	dev_info(sgm->dev, "set_ichrg_curr = %d\n", chrg_curr);
	if (chrg_curr < SGM4151x_ICHRG_I_MIN_uA)
		chrg_curr = SGM4151x_ICHRG_I_MIN_uA;

	reg_val = chrg_curr / SGM4151x_ICHRG_CURRENT_STEP_uA;

	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_2,
			SGM4151x_ICHRG_CUR_MASK, reg_val);

	return ret;
}

static int sgm4151x_set_chrg_volt(struct charger_device *chg_dev, unsigned int chrg_volt)
{
	int ret;
	int reg_val;
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);

	if (chrg_volt < SGM4151x_VREG_V_MIN_uV)
		chrg_volt = SGM4151x_VREG_V_MIN_uV;

	reg_val = (chrg_volt-SGM4151x_VREG_V_MIN_uV) / SGM4151x_VREG_V_STEP_uV;
	reg_val = reg_val<<3;
	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_4,
			SGM4151x_VREG_V_MASK, reg_val);

	return ret;
}

static int sgm4151x_set_input_volt_lim(struct charger_device *chg_dev, unsigned int vindpm)
{
	int ret;
	int offset;
	u8 vlim;
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);

	if (vindpm < SGM4151x_VINDPM_V_MIN_uV)
		vindpm = SGM4151x_VINDPM_V_MIN_uV;
	if (vindpm > SGM4151x_VINDPM_V_MAX_uV)
		vindpm = SGM4151x_VINDPM_V_MAX_uV;

	offset = SGM4151x_VINDPM_V_MIN_uV; //uv
	vlim = (vindpm - offset) / SGM4151x_VINDPM_STEP_uV;
	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_6,
			SGM4151x_VINDPM_V_MASK, vlim);

	return ret;
}

static int sgm4151x_set_input_curr_lim(struct charger_device *chg_dev, unsigned int iindpm)
{
	int ret;
	u8 reg_val;
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);

	if (iindpm < SGM4151x_IINDPM_I_MIN_uA)
		iindpm = SGM4151x_IINDPM_I_MIN_uA;
	if (iindpm > SGM4151x_IINDPM_I_MAX_uA)
		iindpm = SGM4151x_IINDPM_I_MAX_uA;

	reg_val = (iindpm-SGM4151x_IINDPM_I_MIN_uA) / SGM4151x_IINDPM_STEP_uA;
	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_0,
			SGM4151x_IINDPM_I_MASK, reg_val);
	return ret;
}

static int sgm4151x_set_hiz_en(struct charger_device *chg_dev, bool hiz_en)
{
	int reg_val;
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);

	dev_notice(sgm->dev, "%s:%d", __func__, hiz_en);
	reg_val = hiz_en ? SGM4151x_HIZ_EN : 0;

	return sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_0,
			SGM4151x_HIZ_EN, reg_val);
}

#ifdef CONFIG_CUSTOMER_SUPPORT
static int sgm4151x_enable_shipping_mode(struct charger_device *chg_dev, bool en)
{
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);
	int ret;

	if (en) {
		dev_info(sgm->dev, "%s:%d", __func__, en);
		ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_7, 0x04, 0);
		if (ret) {
			dev_info(sgm->dev, "[%s] set batfet_dly failed:%d", __func__, ret);
			return ret;
		}
		ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_7, 0x20, 0x20);
		if (ret) {
			dev_info(sgm->dev, "[%s] set batfet_dis failed:%d", __func__, ret);
			return ret;
		}
	} else {
		/* do nothing, should never happen */
	}
	return 0;
}
#endif

static int sgm4151x_dump_register(struct charger_device *chg_dev)
{
	int i = 0;
	u8 reg = 0;
	struct sgm4151x_device *sgm = charger_get_data(chg_dev);

	for (i = 0; i <= SGM4151x_CHRG_CTRL_B; i++) {
		sgm4151x_read_reg(sgm, i, &reg);
		pr_info("%s REG%02x  %02x\n", __func__, i, reg);
	}
	return 0;
}

static enum power_supply_property sgm4151x_charger_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
};

static int sgm4151x_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sgm4151x_power_supply_init(struct sgm4151x_device *sgm,
		struct device *dev)
{
	sgm->psy_cfg.drv_data = sgm;
	sgm->psy_cfg.of_node = dev->of_node;
	sgm->sgm4151x_power_supply_desc.name = "sgm4151x";
	sgm->sgm4151x_power_supply_desc.type = POWER_SUPPLY_TYPE_USB;
	sgm->sgm4151x_power_supply_desc.properties = sgm4151x_charger_props;
	sgm->sgm4151x_power_supply_desc.num_properties = ARRAY_SIZE(sgm4151x_charger_props);
	sgm->sgm4151x_power_supply_desc.get_property = sgm4151x_charger_get_property;
	sgm->sgm4151x_power_supply_desc.type = POWER_SUPPLY_TYPE_USB;
	sgm->charger = power_supply_register(sgm->dev,
			&sgm->sgm4151x_power_supply_desc, &sgm->psy_cfg);
	if (IS_ERR(sgm->charger))
		return -EINVAL;
	return 0;
}

static int sgm4151x_hw_init(struct sgm4151x_device *sgm)
{
	int ret;

	/* set hz */
	ret = sgm4151x_set_hiz_en(sgm->chg_dev, true);
	if (ret)
		return ret;
	/* disable wdt */
	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_5,
			SGM4151x_WDT_TIMER_MASK, SGM4151x_WDT_TIMER_DISABLE);
	if (ret)
		return ret;
	/* disable safe timer*/
	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_5,
			SGM4151x_SAFETY_TIMER_MASK, SGM4151x_SAFETY_TIMER_DISABLE);
	if (ret)
		return ret;
	/* set ovp 14V */
	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_6,
			SGM4151x_VAC_OVP_MASK, 0xC0);
	if (ret)
		return ret;
	/* set Iterm 500mA */
	ret = sgm4151x_set_term_curr(sgm->chg_dev, 500000);
	if (ret)
		return ret;
	/* set CV 4.1V */
	ret = sgm4151x_set_chrg_volt(sgm->chg_dev, 4100000);
	if (ret)
		return ret;
	/* set Ilim 0mA */
	ret = sgm4151x_set_input_curr_lim(sgm->chg_dev, 0);
	if (ret)
		return ret;
	/* set Ichg 0mA */
	ret = sgm4151x_set_ichrg_curr(sgm->chg_dev, 0);
	if (ret)
		return ret;
	/* set Vindpm 4.6V */
	ret = sgm4151x_set_input_volt_lim(sgm->chg_dev, 4600000);
	if (ret)
		return ret;

	sgm4151x_set_hiz_en(sgm->chg_dev, 0);

	return ret;
}

static int sgm4151x_parse_dt(struct sgm4151x_device *sgm)
{
	int ret;
	int irq_gpio = 0, irqn = 0;
	int chg_en_gpio = 0;

	irq_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,irq-gpio", 0);
	if (!gpio_is_valid(irq_gpio)) {
		dev_info(sgm->dev, "%s: %d gpio get failed\n", __func__, irq_gpio);
		return -EINVAL;
	}
	ret = gpio_request(irq_gpio, "sgm4151x irq pin");
	if (ret) {
		dev_info(sgm->dev, "%s: %d gpio request failed\n", __func__, irq_gpio);
		return ret;
	}
	gpio_direction_input(irq_gpio);
	irqn = gpio_to_irq(irq_gpio);
	if (irqn < 0) {
		dev_info(sgm->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		return irqn;
	}
	sgm->client->irq = irqn;

	chg_en_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,chg-en-gpio", 0);
	if (!gpio_is_valid(chg_en_gpio)) {
		dev_info(sgm->dev, "%s: %d gpio get failed\n", __func__, chg_en_gpio);
		return -EINVAL;
	}
	ret = gpio_request(chg_en_gpio, "sgm chg en pin");
	if (ret) {
		dev_info(sgm->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
		return ret;
	}
	/* default enable charge */
	gpio_direction_output(chg_en_gpio, 0);

	return 0;
}

static struct charger_ops sgm4151x_chg_ops = {
	.enable_hz = sgm4151x_set_hiz_en,
	/* .enable_shipping_mode = sgm4151x_enable_shipping_mode, */

	/* Normal charging */
	.dump_registers = sgm4151x_dump_register,
	.set_charging_current = sgm4151x_set_ichrg_curr,
	.set_input_current = sgm4151x_set_input_curr_lim,
	.set_constant_voltage = sgm4151x_set_chrg_volt,
	.set_mivr = sgm4151x_set_input_volt_lim,
};

static int sgm4151x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct sgm4151x_device *sgm;

	pr_info("sgm4151x start probe\n");
	sgm = devm_kzalloc(&client->dev, sizeof(struct sgm4151x_device), GFP_KERNEL);
	if (!sgm)
		return -ENOMEM;

	mutex_init(&sgm->i2c_rw_lock);

	sgm->client = client;
	sgm->dev = &client->dev;
	i2c_set_clientdata(client, sgm);

	// Customer customization
	ret = sgm4151x_parse_dt(sgm);
	if (ret) {
		dev_info(sgm->dev, "Failed to read device tree properties<errno:%d>\n", ret);
		return ret;
	}

	/* Register charger device */
	sgm->sgm4151x_chg_props.alias_name = "sgm4151x";
	sgm->chg_dev = charger_device_register("primary_divider_chg",
			sgm->dev, sgm, &sgm4151x_chg_ops, &sgm->sgm4151x_chg_props);
	if (IS_ERR_OR_NULL(sgm->chg_dev)) {
		pr_info("%s: register charger device  failed\n", __func__);
		ret = PTR_ERR(sgm->chg_dev);
		return ret;
	}

	ret = sgm4151x_power_supply_init(sgm, sgm->dev);
	if (ret) {
		dev_info(sgm->dev, "Failed to init power supply\n");
		return ret;
	}

	ret = sgm4151x_hw_init(sgm);
	if (ret) {
		dev_info(sgm->dev, "Cannot initialize the chip.\n");
		return ret;
	}

	sgm4151x_check_pn(sgm->chg_dev);
	sgm4151x_dump_register(sgm->chg_dev);
	return ret;
}

static int sgm4151x_charger_remove(struct i2c_client *client)
{
	struct sgm4151x_device *sgm = i2c_get_clientdata(client);

	power_supply_unregister(sgm->charger);
	mutex_destroy(&sgm->i2c_rw_lock);

	return 0;
}

static const struct i2c_device_id sgm4151x_i2c_ids[] = {
	{ "sgm41511", 0 },
	{ "sgm41512", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sgm4151x_i2c_ids);

static const struct of_device_id sgm4151x_of_match[] = {
	{.compatible = "sgm,sgm41511",},
	{},
};
MODULE_DEVICE_TABLE(of, sgm4151x_of_match);

static struct i2c_driver sgm4151x_driver = {
	.driver = {
		.name = "sgm4151x-charger",
		.of_match_table = sgm4151x_of_match,
	},
	.probe = sgm4151x_probe,
	.remove = sgm4151x_charger_remove,
	.id_table = sgm4151x_i2c_ids,
};
module_i2c_driver(sgm4151x_driver);

MODULE_AUTHOR(" qhq <allen_qin@sg-micro.com>");
MODULE_DESCRIPTION("sgm4151x charger driver");
MODULE_LICENSE("GPL v2");
