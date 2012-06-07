/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/msm-charger.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/i2c/isl9519.h>
#include <linux/msm_adc.h>
#include <linux/spinlock.h>

#define CHG_CURRENT_REG		0x14
#define MAX_SYS_VOLTAGE_REG	0x15
#define CONTROL_REG		0x3D
#define MIN_SYS_VOLTAGE_REG	0x3E
#define INPUT_CURRENT_REG	0x3F
#define MANUFACTURER_ID_REG	0xFE
#define DEVICE_ID_REG		0xFF

#define TRCKL_CHG_STATUS_BIT	0x80

#define ISL9519_CHG_PERIOD_SEC	150

struct isl9519q_struct {
	struct i2c_client		*client;
	struct delayed_work		charge_work;
	int				present;
	int				batt_present;
	bool				charging;
	int				chgcurrent;
	int				term_current;
	int				input_current;
	int				max_system_voltage;
	int				min_system_voltage;
	int				valid_n_gpio;
	struct dentry			*dent;
	struct msm_hardware_charger	adapter_hw_chg;
	int				suspended;
	int				charge_at_resume;
	struct power_supply		dc_psy;
	spinlock_t			lock;
	bool				notify_by_pmic;
	bool				trickle;
};

static struct isl9519q_struct *the_isl_chg;

static int isl9519q_read_reg(struct i2c_client *client, int reg,
	u16 *val)
{
	int ret;
	struct isl9519q_struct *isl_chg;

	isl_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_read_word_data(isl_chg->client, reg);

	if (ret < 0) {
		dev_err(&isl_chg->client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return -EAGAIN;
	} else {
		*val = ret;
	}

	pr_debug("reg=0x%x.val=0x%x.\n", reg, *val);

	return 0;
}

static int isl9519q_write_reg(struct i2c_client *client, int reg,
	u16 val)
{
	int ret;
	struct isl9519q_struct *isl_chg;

	pr_debug("reg=0x%x.val=0x%x.\n", reg, val);

	isl_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_write_word_data(isl_chg->client, reg, val);

	if (ret < 0) {
		dev_err(&isl_chg->client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return -EAGAIN;
	}
	return 0;
}

/**
 * Read charge-current via ADC.
 *
 * The ISL CCMON (charge-current-monitor) pin is connected to
 * the PMIC MPP#X pin.
 * This not required when notify_by_pmic is used where the PMIC
 * uses BMS to notify the ISL on charging-done / charge-resume.
 */
static int isl_read_adc(int channel, int *mv_reading)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	pr_debug("called for %d\n", channel);
	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("couldnt open channel %d ret=%d\n", channel, ret);
		goto out;
	}
	init_completion(&conv_complete_evt);
	ret = adc_channel_request_conv(h, &conv_complete_evt);
	if (ret) {
		pr_err("couldnt request conv channel %d ret=%d\n",
						channel, ret);
		goto out;
	}
	ret = wait_for_completion_interruptible(&conv_complete_evt);
	if (ret) {
		pr_err("wait interrupted channel %d ret=%d\n", channel, ret);
		goto out;
	}
	ret = adc_channel_read_result(h, &adc_chan_result);
	if (ret) {
		pr_err("couldnt read result channel %d ret=%d\n",
						channel, ret);
		goto out;
	}
	ret = adc_channel_close(h);
	if (ret)
		pr_err("couldnt close channel %d ret=%d\n", channel, ret);
	if (mv_reading)
		*mv_reading = (int)adc_chan_result.measurement;

	pr_debug("done for %d\n", channel);
	return adc_chan_result.physical;
out:
	*mv_reading = 0;
	pr_debug("done with error for %d\n", channel);

	return -EINVAL;
}

static bool is_trickle_charging(struct isl9519q_struct *isl_chg)
{
	u16 ctrl = 0;
	int ret;

	ret = isl9519q_read_reg(isl_chg->client, CONTROL_REG, &ctrl);

	if (!ret) {
		pr_debug("control_reg=0x%x.\n", ctrl);
	} else {
		dev_err(&isl_chg->client->dev,
			"%s couldnt read cntrl reg\n", __func__);
	}

	if (ctrl & TRCKL_CHG_STATUS_BIT)
		return true;

	return false;
}

static void isl_adapter_check_ichg(struct isl9519q_struct *isl_chg)
{
	int ichg; /* isl charger current */
	int mv_reading = 0;

	ichg = isl_read_adc(CHANNEL_ADC_BATT_AMON, &mv_reading);

	dev_dbg(&isl_chg->client->dev, "%s mv_reading=%d\n",
		__func__, mv_reading);
	dev_dbg(&isl_chg->client->dev, "%s isl_charger_current=%d\n",
		__func__, ichg);

	if (ichg >= 0 && ichg <= isl_chg->term_current)
		msm_charger_notify_event(&isl_chg->adapter_hw_chg,
					 CHG_DONE_EVENT);

	isl_chg->trickle = is_trickle_charging(isl_chg);
	if (isl_chg->trickle)
		msm_charger_notify_event(&isl_chg->adapter_hw_chg,
					 CHG_BATT_BEGIN_FAST_CHARGING);
}

/**
 * isl9519q_worker
 *
 * Periodic task required to kick the ISL HW watchdog to keep
 * charging.
 *
 * @isl9519_work: work context.
 */
static void isl9519q_worker(struct work_struct *isl9519_work)
{
	struct isl9519q_struct *isl_chg;

	isl_chg = container_of(isl9519_work, struct isl9519q_struct,
			charge_work.work);

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	if (!isl_chg->charging) {
		pr_debug("stop charging.\n");
		isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, 0);
		return; /* Stop periodic worker */
	}

	/* Kick the dog by writting to CHG_CURRENT_REG */
	isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG,
			   isl_chg->chgcurrent);

	if (isl_chg->notify_by_pmic)
		isl_chg->trickle = is_trickle_charging(isl_chg);
	else
		isl_adapter_check_ichg(isl_chg);

	schedule_delayed_work(&isl_chg->charge_work,
			      (ISL9519_CHG_PERIOD_SEC * HZ));
}

static int isl9519q_start_charging(struct isl9519q_struct *isl_chg,
				   int chg_voltage, int chg_current)
{
	pr_debug("\n");

	if (isl_chg->charging) {
		pr_warn("already charging.\n");
		return 0;
	}

	if (isl_chg->suspended) {
		pr_warn("suspended - can't start charging.\n");
		isl_chg->charge_at_resume = 1;
		return 0;
	}

	dev_dbg(&isl_chg->client->dev,
		"%s starting timed work.period=%d seconds.\n",
		__func__, (int) ISL9519_CHG_PERIOD_SEC);

	/*
	 * The ISL will start charging from the worker context.
	 * This API might be called from interrupt context.
	 */
	schedule_delayed_work(&isl_chg->charge_work, 1);

	isl_chg->charging = true;

	return 0;
}

static int isl9519q_stop_charging(struct isl9519q_struct *isl_chg)
{
	pr_debug("\n");

	if (!(isl_chg->charging)) {
		pr_warn("already not charging.\n");
		return 0;
	}

	if (isl_chg->suspended) {
		isl_chg->charge_at_resume = 0;
		return 0;
	}

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	isl_chg->charging = false;
	isl_chg->trickle = false;
	/*
	 * The ISL will stop charging from the worker context.
	 * This API might be called from interrupt context.
	 */
	schedule_delayed_work(&isl_chg->charge_work, 1);

	return 0;
}

static int isl_adapter_start_charging(struct msm_hardware_charger *hw_chg,
				      int chg_voltage, int chg_current)
{
	int rc;
	struct isl9519q_struct *isl_chg;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	rc = isl9519q_start_charging(isl_chg, chg_voltage, chg_current);

	return rc;
}

static int isl_adapter_stop_charging(struct msm_hardware_charger *hw_chg)
{
	int rc;
	struct isl9519q_struct *isl_chg;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	rc = isl9519q_stop_charging(isl_chg);

	return rc;
}

static int isl9519q_charging_switched(struct msm_hardware_charger *hw_chg)
{
	struct isl9519q_struct *isl_chg;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);
	return 0;
}

static irqreturn_t isl_valid_handler(int irq, void *dev_id)
{
	int val;
	struct isl9519q_struct *isl_chg;
	struct i2c_client *client = dev_id;

	isl_chg = i2c_get_clientdata(client);
	val = gpio_get_value_cansleep(isl_chg->valid_n_gpio);
	if (val < 0) {
		dev_err(&isl_chg->client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			isl_chg->valid_n_gpio, val);
		goto err;
	}
	dev_dbg(&isl_chg->client->dev, "%s val=%d\n", __func__, val);

	if (val) {
		if (isl_chg->present == 1) {
			msm_charger_notify_event(&isl_chg->adapter_hw_chg,
						 CHG_REMOVED_EVENT);
			isl_chg->present = 0;
		}
	} else {
		if (isl_chg->present == 0) {
			msm_charger_notify_event(&isl_chg->adapter_hw_chg,
						 CHG_INSERTED_EVENT);
			isl_chg->present = 1;
		}
	}
err:
	return IRQ_HANDLED;
}

static enum power_supply_property pm_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int get_prop_charge_type(struct isl9519q_struct *isl_chg)
{
	if (!isl_chg->present)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;

	if (isl_chg->trickle)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	if (isl_chg->charging)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}
static int pm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct isl9519q_struct *isl_chg = container_of(psy,
						struct isl9519q_struct,
						dc_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = isl_chg->chgcurrent;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (int)isl_chg->present;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type(isl_chg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int pm_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct isl9519q_struct *isl_chg = container_of(psy,
						struct isl9519q_struct,
						dc_psy);
	unsigned long flags;
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval) {
			isl_chg->present = val->intval;
		} else {
			isl_chg->present = 0;
			if (isl_chg->charging)
				goto stop_charging;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (val->intval) {
			if (isl_chg->chgcurrent != val->intval)
				return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (val->intval && isl_chg->present) {
			if (val->intval == POWER_SUPPLY_CHARGE_TYPE_FAST)
				goto start_charging;
			if (val->intval == POWER_SUPPLY_CHARGE_TYPE_NONE)
				goto stop_charging;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(&isl_chg->dc_psy);
	return 0;

start_charging:
	spin_lock_irqsave(&isl_chg->lock, flags);
	rc = isl9519q_start_charging(isl_chg, 0, isl_chg->chgcurrent);
	if (rc)
		pr_err("Failed to start charging rc=%d\n", rc);
	spin_unlock_irqrestore(&isl_chg->lock, flags);
	power_supply_changed(&isl_chg->dc_psy);
	return rc;

stop_charging:
	spin_lock_irqsave(&isl_chg->lock, flags);
	rc = isl9519q_stop_charging(isl_chg);
	if (rc)
		pr_err("Failed to start charging rc=%d\n", rc);
	spin_unlock_irqrestore(&isl_chg->lock, flags);
	power_supply_changed(&isl_chg->dc_psy);
	return rc;
}

#define MAX_VOLTAGE_REG_MASK  0x3FF0
#define MIN_VOLTAGE_REG_MASK  0x3F00
#define DEFAULT_MAX_VOLTAGE_REG_VALUE	0x1070
#define DEFAULT_MIN_VOLTAGE_REG_VALUE	0x0D00

static int __devinit isl9519q_init_adapter(struct isl9519q_struct *isl_chg)
{
	int ret;
	struct i2c_client *client = isl_chg->client;
	struct isl_platform_data *pdata = client->dev.platform_data;

	isl_chg->adapter_hw_chg.type = CHG_TYPE_AC;
	isl_chg->adapter_hw_chg.rating = 2;
	isl_chg->adapter_hw_chg.name = "isl-adapter";
	isl_chg->adapter_hw_chg.start_charging = isl_adapter_start_charging;
	isl_chg->adapter_hw_chg.stop_charging = isl_adapter_stop_charging;
	isl_chg->adapter_hw_chg.charging_switched = isl9519q_charging_switched;

	ret = gpio_request(pdata->valid_n_gpio, "isl_charger_valid");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed "
				      "for %d ret=%d\n",
			__func__, pdata->valid_n_gpio, ret);
		goto out;
	}

	ret = msm_charger_register(&isl_chg->adapter_hw_chg);
	if (ret) {
		dev_err(&client->dev,
			"%s msm_charger_register failed for ret =%d\n",
			__func__, ret);
		goto free_gpio;
	}

	ret = request_threaded_irq(client->irq, NULL,
				   isl_valid_handler,
				   IRQF_TRIGGER_FALLING |
				   IRQF_TRIGGER_RISING,
				   "isl_charger_valid", client);
	if (ret) {
		dev_err(&client->dev,
			"%s request_threaded_irq failed "
			"for %d ret =%d\n",
			__func__, client->irq, ret);
		goto unregister;
	}
	irq_set_irq_wake(client->irq, 1);

	ret = gpio_get_value_cansleep(isl_chg->valid_n_gpio);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s gpio_get_value failed for %d ret=%d\n",
			__func__, pdata->valid_n_gpio, ret);
		/* assume absent */
		ret = 1;
	}
	if (!ret) {
		msm_charger_notify_event(&isl_chg->adapter_hw_chg,
				CHG_INSERTED_EVENT);
		isl_chg->present = 1;
	}

	return 0;

unregister:
	msm_charger_unregister(&isl_chg->adapter_hw_chg);
free_gpio:
	gpio_free(pdata->valid_n_gpio);
out:
	return ret;

}

static int __devinit isl9519q_init_ext_chg(struct isl9519q_struct *isl_chg)
{
	int ret;

	isl_chg->dc_psy.name = "dc";
	isl_chg->dc_psy.type = POWER_SUPPLY_TYPE_MAINS;
	isl_chg->dc_psy.supplied_to = pm_power_supplied_to;
	isl_chg->dc_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	isl_chg->dc_psy.properties = pm_power_props;
	isl_chg->dc_psy.num_properties = ARRAY_SIZE(pm_power_props);
	isl_chg->dc_psy.get_property = pm_power_get_property;
	isl_chg->dc_psy.set_property = pm_power_set_property;

	ret = power_supply_register(&isl_chg->client->dev, &isl_chg->dc_psy);
	if (ret) {
		pr_err("failed to register dc charger.ret=%d.\n", ret);
		return ret;
	}

	return 0;
}
static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u16 temp;

	temp = (u16) val;
	ret = isl9519q_write_reg(the_isl_chg->client, addr, temp);

	if (ret) {
		pr_err("isl9519q_write_reg to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}
static int get_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int ret;
	u16 temp;

	ret = isl9519q_read_reg(the_isl_chg->client, addr, &temp);
	if (ret) {
		pr_err("isl9519q_read_reg to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}

	*val = temp;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static void create_debugfs_entries(struct isl9519q_struct *isl_chg)
{
	isl_chg->dent = debugfs_create_dir("isl9519q", NULL);

	if (IS_ERR(isl_chg->dent)) {
		pr_err("isl9519q driver couldn't create debugfs dir\n");
		return;
	}

	debugfs_create_file("CHG_CURRENT_REG", 0644, isl_chg->dent,
				(void *) CHG_CURRENT_REG, &reg_fops);
	debugfs_create_file("MAX_SYS_VOLTAGE_REG", 0644, isl_chg->dent,
				(void *) MAX_SYS_VOLTAGE_REG, &reg_fops);
	debugfs_create_file("CONTROL_REG", 0644, isl_chg->dent,
				(void *) CONTROL_REG, &reg_fops);
	debugfs_create_file("MIN_SYS_VOLTAGE_REG", 0644, isl_chg->dent,
				(void *) MIN_SYS_VOLTAGE_REG, &reg_fops);
	debugfs_create_file("INPUT_CURRENT_REG", 0644, isl_chg->dent,
				(void *) INPUT_CURRENT_REG, &reg_fops);
	debugfs_create_file("MANUFACTURER_ID_REG", 0644, isl_chg->dent,
				(void *) MANUFACTURER_ID_REG, &reg_fops);
	debugfs_create_file("DEVICE_ID_REG", 0644, isl_chg->dent,
				(void *) DEVICE_ID_REG, &reg_fops);
}

static void remove_debugfs_entries(struct isl9519q_struct *isl_chg)
{
	if (isl_chg->dent)
		debugfs_remove_recursive(isl_chg->dent);
}

static int __devinit isl9519q_hwinit(struct isl9519q_struct *isl_chg)
{
	int ret;

	ret = isl9519q_write_reg(isl_chg->client, MAX_SYS_VOLTAGE_REG,
			isl_chg->max_system_voltage);
	if (ret) {
		pr_err("Failed to set MAX_SYS_VOLTAGE rc=%d\n", ret);
		return ret;
	}

	ret = isl9519q_write_reg(isl_chg->client, MIN_SYS_VOLTAGE_REG,
			isl_chg->min_system_voltage);
	if (ret) {
		pr_err("Failed to set MIN_SYS_VOLTAGE rc=%d\n", ret);
		return ret;
	}

	if (isl_chg->input_current) {
		ret = isl9519q_write_reg(isl_chg->client,
				INPUT_CURRENT_REG,
				isl_chg->input_current);
		if (ret) {
			pr_err("Failed to set INPUT_CURRENT rc=%d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int __devinit isl9519q_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct isl_platform_data *pdata;
	struct isl9519q_struct *isl_chg;
	int ret;

	ret = 0;
	pdata = client->dev.platform_data;

	pr_debug("\n");

	if (pdata == NULL) {
		dev_err(&client->dev, "%s no platform data\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_WORD_DATA)) {
		ret = -EIO;
		goto out;
	}

	isl_chg = kzalloc(sizeof(*isl_chg), GFP_KERNEL);
	if (!isl_chg) {
		ret = -ENOMEM;
		goto out;
	}

	spin_lock_init(&isl_chg->lock);

	INIT_DELAYED_WORK(&isl_chg->charge_work, isl9519q_worker);
	isl_chg->client = client;
	isl_chg->chgcurrent = pdata->chgcurrent;
	isl_chg->term_current = pdata->term_current;
	isl_chg->input_current = pdata->input_current;
	isl_chg->max_system_voltage = pdata->max_system_voltage;
	isl_chg->min_system_voltage = pdata->min_system_voltage;
	isl_chg->valid_n_gpio = pdata->valid_n_gpio;

	/* h/w ignores lower 7 bits of charging current and input current */
	isl_chg->chgcurrent &= ~0x7F;
	isl_chg->input_current &= ~0x7F;

	/**
	 * ISL is Notified by PMIC to start/stop charging, rather than
	 * handling interrupt from ISL for End-Of-Chargring, and
	 * monitoring the charge-current periodically. The valid_n_gpio
	 * is also not used, dc-present is detected by PMIC.
	 */
	isl_chg->notify_by_pmic = (client->irq == 0);
	i2c_set_clientdata(client, isl_chg);

	if (pdata->chg_detection_config) {
		ret = pdata->chg_detection_config();
		if (ret) {
			dev_err(&client->dev, "%s valid config failed ret=%d\n",
				__func__, ret);
			goto free_isl_chg;
		}
	}

	isl_chg->max_system_voltage &= MAX_VOLTAGE_REG_MASK;
	isl_chg->min_system_voltage &= MIN_VOLTAGE_REG_MASK;
	if (isl_chg->max_system_voltage == 0)
		isl_chg->max_system_voltage = DEFAULT_MAX_VOLTAGE_REG_VALUE;
	if (isl_chg->min_system_voltage == 0)
		isl_chg->min_system_voltage = DEFAULT_MIN_VOLTAGE_REG_VALUE;

	ret = isl9519q_hwinit(isl_chg);
	if (ret)
		goto free_isl_chg;

	if (isl_chg->notify_by_pmic)
		ret = isl9519q_init_ext_chg(isl_chg);
	else
		ret = isl9519q_init_adapter(isl_chg);

	if (ret)
		goto free_isl_chg;

	the_isl_chg = isl_chg;
	create_debugfs_entries(isl_chg);

	pr_info("OK.\n");

	return 0;

free_isl_chg:
	kfree(isl_chg);
out:
	return ret;
}

static int __devexit isl9519q_remove(struct i2c_client *client)
{
	struct isl_platform_data *pdata;
	struct isl9519q_struct *isl_chg = i2c_get_clientdata(client);

	pdata = client->dev.platform_data;
	gpio_free(pdata->valid_n_gpio);
	free_irq(client->irq, client);
	cancel_delayed_work_sync(&isl_chg->charge_work);
	if (isl_chg->notify_by_pmic) {
		power_supply_unregister(&isl_chg->dc_psy);
	} else {
		msm_charger_notify_event(&isl_chg->adapter_hw_chg,
							CHG_REMOVED_EVENT);
		msm_charger_unregister(&isl_chg->adapter_hw_chg);
	}
	remove_debugfs_entries(isl_chg);
	the_isl_chg = NULL;
	kfree(isl_chg);
	return 0;
}

static const struct i2c_device_id isl9519q_id[] = {
	{"isl9519q", 0},
	{},
};

#ifdef CONFIG_PM
static int isl9519q_suspend(struct device *dev)
{
	struct isl9519q_struct *isl_chg = dev_get_drvdata(dev);

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);
	/*
	 * do not suspend while we are charging
	 * because we need to periodically update the register
	 * for charging to proceed
	 */
	if (isl_chg->charging)
		return -EBUSY;

	isl_chg->suspended  = 1;
	return 0;
}

static int isl9519q_resume(struct device *dev)
{
	struct isl9519q_struct *isl_chg = dev_get_drvdata(dev);

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);
	isl_chg->suspended  = 0;
	if (isl_chg->charge_at_resume) {
		isl_chg->charge_at_resume = 0;
		isl9519q_start_charging(isl_chg, 0, 0);
	}
	return 0;
}

static const struct dev_pm_ops isl9519q_pm_ops = {
	.suspend = isl9519q_suspend,
	.resume = isl9519q_resume,
};
#endif

static struct i2c_driver isl9519q_driver = {
	.driver = {
		   .name = "isl9519q",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &isl9519q_pm_ops,
#endif
	},
	.probe = isl9519q_probe,
	.remove = __devexit_p(isl9519q_remove),
	.id_table = isl9519q_id,
};

static int __init isl9519q_init(void)
{
	return i2c_add_driver(&isl9519q_driver);
}

late_initcall_sync(isl9519q_init);

static void __exit isl9519q_exit(void)
{
	return i2c_del_driver(&isl9519q_driver);
}

module_exit(isl9519q_exit);

MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Driver for ISL9519Q Charger chip");
MODULE_LICENSE("GPL v2");
