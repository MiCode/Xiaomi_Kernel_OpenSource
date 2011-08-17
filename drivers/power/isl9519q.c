/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/msm-charger.h>
#include <linux/slab.h>
#include <linux/i2c/isl9519.h>
#include <linux/msm_adc.h>

#define CHG_CURRENT_REG		0x14
#define MAX_SYS_VOLTAGE_REG	0x15
#define CONTROL_REG		0x3D
#define MIN_SYS_VOLTAGE_REG	0x3E
#define INPUT_CURRENT_REG	0x3F
#define MANUFACTURER_ID_REG	0xFE
#define DEVICE_ID_REG		0xFF

#define TRCKL_CHG_STATUS_BIT	0x80

#define ISL9519_CHG_PERIOD	((HZ) * 150)

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
};

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
	} else
		*val = ret;

	return 0;
}

static int isl9519q_write_reg(struct i2c_client *client, int reg,
	u16 val)
{
	int ret;
	struct isl9519q_struct *isl_chg;

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

static int isl_read_adc(int channel, int *mv_reading)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	pr_debug("%s: called for %d\n", __func__, channel);
	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("%s: couldnt open channel %d ret=%d\n",
					__func__, channel, ret);
		goto out;
	}
	init_completion(&conv_complete_evt);
	ret = adc_channel_request_conv(h, &conv_complete_evt);
	if (ret) {
		pr_err("%s: couldnt request conv channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = wait_for_completion_interruptible(&conv_complete_evt);
	if (ret) {
		pr_err("%s: wait interrupted channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_read_result(h, &adc_chan_result);
	if (ret) {
		pr_err("%s: couldnt read result channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_close(h);
	if (ret)
		pr_err("%s: couldnt close channel %d ret=%d\n",
					__func__, channel, ret);
	if (mv_reading)
		*mv_reading = (int)adc_chan_result.measurement;

	pr_debug("%s: done for %d\n", __func__, channel);
	return adc_chan_result.physical;
out:
	*mv_reading = 0;
	pr_debug("%s: done with error for %d\n", __func__, channel);
	return -EINVAL;

}

static void isl9519q_charge(struct work_struct *isl9519_work)
{
	u16 temp;
	int ret;
	struct isl9519q_struct *isl_chg;
	int isl_charger_current;
	int mv_reading;

	isl_chg = container_of(isl9519_work, struct isl9519q_struct,
			charge_work.work);

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	if (isl_chg->charging) {
		isl_charger_current = isl_read_adc(CHANNEL_ADC_BATT_AMON,
								&mv_reading);
		dev_dbg(&isl_chg->client->dev, "%s mv_reading=%d\n",
				__func__, mv_reading);
		dev_dbg(&isl_chg->client->dev, "%s isl_charger_current=%d\n",
				__func__, isl_charger_current);
		if (isl_charger_current >= 0
			&& isl_charger_current <= isl_chg->term_current) {
			msm_charger_notify_event(
					&isl_chg->adapter_hw_chg,
					CHG_DONE_EVENT);
		}
		isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG,
				isl_chg->chgcurrent);
		ret = isl9519q_read_reg(isl_chg->client, CONTROL_REG, &temp);
		if (!ret) {
			if (!(temp & TRCKL_CHG_STATUS_BIT))
				msm_charger_notify_event(
						&isl_chg->adapter_hw_chg,
						CHG_BATT_BEGIN_FAST_CHARGING);
		} else {
			dev_err(&isl_chg->client->dev,
				"%s couldnt read cntrl reg\n", __func__);
		}
		schedule_delayed_work(&isl_chg->charge_work,
						ISL9519_CHG_PERIOD);
	}
}

static int isl9519q_start_charging(struct msm_hardware_charger *hw_chg,
		int chg_voltage, int chg_current)
{
	struct isl9519q_struct *isl_chg;
	int ret = 0;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	if (isl_chg->charging)
		/* we are already charging */
		return 0;

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	ret = isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG,
						isl_chg->chgcurrent);
	if (ret) {
		dev_err(&isl_chg->client->dev,
			"%s coulnt write to current_reg\n", __func__);
		goto out;
	}

	dev_dbg(&isl_chg->client->dev, "%s starting timed work\n",
							__func__);
	schedule_delayed_work(&isl_chg->charge_work,
						ISL9519_CHG_PERIOD);
	isl_chg->charging = true;

out:
	return ret;
}

static int isl9519q_stop_charging(struct msm_hardware_charger *hw_chg)
{
	struct isl9519q_struct *isl_chg;
	int ret = 0;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	if (!(isl_chg->charging))
		/* we arent charging */
		return 0;

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	ret = isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, 0);
	if (ret) {
		dev_err(&isl_chg->client->dev,
			"%s coulnt write to current_reg\n", __func__);
		goto out;
	}

	isl_chg->charging = false;
	cancel_delayed_work(&isl_chg->charge_work);
out:
	return ret;
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

#define MAX_VOLTAGE_REG_MASK  0x3FF0
#define MIN_VOLTAGE_REG_MASK  0x3F00
#define DEFAULT_MAX_VOLTAGE_REG_VALUE	0x1070
#define DEFAULT_MIN_VOLTAGE_REG_VALUE	0x0D00

static int __devinit isl9519q_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct isl_platform_data *pdata;
	struct isl9519q_struct *isl_chg;
	int ret;

	ret = 0;
	pdata = client->dev.platform_data;

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

	INIT_DELAYED_WORK(&isl_chg->charge_work, isl9519q_charge);
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

	isl_chg->adapter_hw_chg.type = CHG_TYPE_AC;
	isl_chg->adapter_hw_chg.rating = 2;
	isl_chg->adapter_hw_chg.name = "isl-adapter";
	isl_chg->adapter_hw_chg.start_charging = isl9519q_start_charging;
	isl_chg->adapter_hw_chg.stop_charging = isl9519q_stop_charging;
	isl_chg->adapter_hw_chg.charging_switched = isl9519q_charging_switched;

	if (pdata->chg_detection_config) {
		ret = pdata->chg_detection_config();
		if (ret) {
			dev_err(&client->dev, "%s valid config failed ret=%d\n",
				__func__, ret);
			goto free_isl_chg;
		}
	}

	ret = gpio_request(pdata->valid_n_gpio, "isl_charger_valid");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed for %d ret=%d\n",
			__func__, pdata->valid_n_gpio, ret);
		goto free_isl_chg;
	}

	i2c_set_clientdata(client, isl_chg);

	ret = msm_charger_register(&isl_chg->adapter_hw_chg);
	if (ret) {
		dev_err(&client->dev,
			"%s msm_charger_register failed for ret =%d\n",
			__func__, ret);
		goto free_gpio;
	}

	ret = request_threaded_irq(client->irq, NULL,
				   isl_valid_handler,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "isl_charger_valid", client);
	if (ret) {
		dev_err(&client->dev,
			"%s request_threaded_irq failed for %d ret =%d\n",
			__func__, client->irq, ret);
		goto unregister;
	}
	irq_set_irq_wake(client->irq, 1);

	isl_chg->max_system_voltage &= MAX_VOLTAGE_REG_MASK;
	isl_chg->min_system_voltage &= MIN_VOLTAGE_REG_MASK;
	if (isl_chg->max_system_voltage == 0)
		isl_chg->max_system_voltage = DEFAULT_MAX_VOLTAGE_REG_VALUE;
	if (isl_chg->min_system_voltage == 0)
		isl_chg->min_system_voltage = DEFAULT_MIN_VOLTAGE_REG_VALUE;

	ret = isl9519q_write_reg(isl_chg->client, MAX_SYS_VOLTAGE_REG,
			isl_chg->max_system_voltage);
	if (ret) {
		dev_err(&client->dev,
			"%s couldnt write to MAX_SYS_VOLTAGE_REG ret=%d\n",
			__func__, ret);
		goto free_irq;
	}

	ret = isl9519q_write_reg(isl_chg->client, MIN_SYS_VOLTAGE_REG,
			isl_chg->min_system_voltage);
	if (ret) {
		dev_err(&client->dev,
			"%s couldnt write to MIN_SYS_VOLTAGE_REG ret=%d\n",
			__func__, ret);
		goto free_irq;
	}

	if (isl_chg->input_current) {
		ret = isl9519q_write_reg(isl_chg->client,
				INPUT_CURRENT_REG,
				isl_chg->input_current);
		if (ret) {
			dev_err(&client->dev,
				"%s couldnt write INPUT_CURRENT_REG ret=%d\n",
				__func__, ret);
			goto free_irq;
		}
	}

	ret = gpio_get_value_cansleep(isl_chg->valid_n_gpio);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			pdata->valid_n_gpio, ret);
		/* assume absent */
		ret = 1;
	}
	if (!ret) {
		msm_charger_notify_event(&isl_chg->adapter_hw_chg,
				CHG_INSERTED_EVENT);
		isl_chg->present = 1;
	}

	pr_debug("%s OK chg_present=%d\n", __func__, isl_chg->present);
	return 0;

free_irq:
	free_irq(client->irq, NULL);
unregister:
	msm_charger_register(&isl_chg->adapter_hw_chg);
free_gpio:
	gpio_free(pdata->valid_n_gpio);
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
	msm_charger_notify_event(&isl_chg->adapter_hw_chg, CHG_REMOVED_EVENT);
	msm_charger_unregister(&isl_chg->adapter_hw_chg);
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
	return 0;
}

static int isl9519q_resume(struct device *dev)
{
	struct isl9519q_struct *isl_chg = dev_get_drvdata(dev);

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);
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

module_init(isl9519q_init);

static void __exit isl9519q_exit(void)
{
	return i2c_del_driver(&isl9519q_driver);
}

module_exit(isl9519q_exit);

MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Driver for ISL9519Q Charger chip");
MODULE_LICENSE("GPL v2");
