/*
 * drivers/power/tps65090-charger.c
 *
 * Battery charger driver for TI's tps65090
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/sbs-battery.h>
#include <linux/mfd/tps65090.h>
#include <linux/power/battery-charger-gauge-comm.h>

#define TPS65090_INTR_STS	0x00
#define TPS65090_CG_CTRL0	0x04
#define TPS65090_CG_CTRL1	0x05
#define TPS65090_CG_CTRL2	0x06
#define TPS65090_CG_CTRL3	0x07
#define TPS65090_CG_CTRL4	0x08
#define TPS65090_CG_CTRL5	0x09
#define TPS65090_CG_STATUS1	0x0a
#define TPS65090_CG_STATUS2	0x0b

#define TPS65090_NOITERM	BIT(5)
#define CHARGER_ENABLE		0x01
#define TPS65090_VACG		0x02

struct tps65090_charger {
	struct	device	*dev;
	int	irq_base;
	int	ac_online;
	int	prev_ac_online;
	struct power_supply	ac;
	struct tps65090_charger_data *chg_pdata;
	struct battery_charger_dev *bc_dev;
	int status;
};

static enum power_supply_property tps65090_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int tps65090_low_chrg_current(struct tps65090_charger *charger)
{
	int ret;

	ret = tps65090_write(charger->dev->parent, TPS65090_CG_CTRL5,
			TPS65090_NOITERM);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): error reading in register 0x%x\n",
			__func__, TPS65090_CG_CTRL5);
		return ret;
	}
	return 0;
}

static int tps65090_enable_charging(struct tps65090_charger *charger,
	uint8_t enable)
{
	int ret;
	uint8_t retval = 0;

	ret = tps65090_read(charger->dev->parent, TPS65090_CG_CTRL0, &retval);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): error reading in register 0x%x\n",
				__func__, TPS65090_CG_CTRL0);
		return ret;
	}

	ret = tps65090_write(charger->dev->parent, TPS65090_CG_CTRL0,
				(retval | CHARGER_ENABLE));
	if (ret < 0) {
		dev_err(charger->dev, "%s(): error reading in register 0x%x\n",
				__func__, TPS65090_CG_CTRL0);
		return ret;
	}
	return 0;
}

static int tps65090_config_charger(struct tps65090_charger *charger)
{
	int ret;

	ret = tps65090_low_chrg_current(charger);
	if (ret < 0) {
		dev_err(charger->dev,
			"error configuring low charge current\n");
		return ret;
	}

	return 0;
}

static int tps65090_ac_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	struct tps65090_charger *charger = container_of(psy,
					struct tps65090_charger, ac);

	if (psp == POWER_SUPPLY_PROP_ONLINE) {
		val->intval = charger->ac_online;
		charger->prev_ac_online = charger->ac_online;
		return 0;
	}
	return -EINVAL;
}

static irqreturn_t tps65090_charger_isr(int irq, void *dev_id)
{
	struct tps65090_charger *charger = dev_id;
	int ret;
	uint8_t retval = 0;
	uint8_t retval2 = 0;

	ret = tps65090_read(charger->dev->parent, TPS65090_CG_STATUS1, &retval);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Error in reading reg 0x%x\n",
				__func__, TPS65090_CG_STATUS1);
		goto error;
	}
	msleep(75);
	ret = tps65090_read(charger->dev->parent, TPS65090_INTR_STS, &retval2);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Error in reading reg 0x%x\n",
				__func__, TPS65090_INTR_STS);
		goto error;
	}


	if (retval2 & TPS65090_VACG) {
		ret = tps65090_enable_charging(charger, 1);
		if (ret < 0)
			goto error;
		charger->ac_online = 1;
		charger->status = BATTERY_CHARGING;
	} else {
		charger->ac_online = 0;
		charger->status = BATTERY_DISCHARGING;
	}

	if (charger->prev_ac_online != charger->ac_online) {
		battery_charging_status_update(charger->bc_dev,
				charger->status);
		power_supply_changed(&charger->ac);
	}
error:
	return IRQ_HANDLED;
}

static int tps65090_charger_get_status(struct battery_charger_dev *bc_dev)
{
	struct tps65090_charger *chip = battery_charger_get_drvdata(bc_dev);

	return chip->status;
}

static struct battery_charging_ops tps65090_charger_bci_ops = {
	.get_charging_status = tps65090_charger_get_status,
};

static struct battery_charger_info tps65090_charger_bci = {
	.cell_id = 0,
	.bc_ops = &tps65090_charger_bci_ops,
};

static int tps65090_charger_probe(struct platform_device *pdev)
{
	uint8_t retval = 0;
	int ret;
	struct tps65090_charger *charger_data;
	struct tps65090_platform_data *pdata;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata) {
		dev_err(&pdev->dev, "%s():no platform data available\n",
				__func__);
		return -ENODEV;
	}

	charger_data = devm_kzalloc(&pdev->dev, sizeof(*charger_data),
			GFP_KERNEL);
	if (!charger_data) {
		dev_err(&pdev->dev, "failed to allocate memory status\n");
		return -ENOMEM;
	}

	charger_data->chg_pdata = pdata->charger_pdata;
	if (!pdata) {
		dev_err(&pdev->dev, "%s()No charger data,exiting\n", __func__);
		return -ENODEV;
	}

	dev_set_drvdata(&pdev->dev, charger_data);

	charger_data->dev = &pdev->dev;

	/* Check for battery presence */
	ret = sbs_battery_detect();
	if (ret < 0) {
		dev_err(charger_data->dev, "No battery. Exiting driver\n");
		return -ENODEV;
	}

	if (charger_data->chg_pdata->irq_base) {
		ret = request_threaded_irq(charger_data->chg_pdata->irq_base
			+ TPS65090_IRQ_VAC_STATUS_CHANGE,
			NULL, tps65090_charger_isr, 0, "tps65090-charger",
			charger_data);
		if (ret) {
			dev_err(charger_data->dev, "Unable to register irq %d err %d\n",
					charger_data->irq_base, ret);
			return ret;
		}
	}

	charger_data->ac.name		= "tps65090-ac";
	charger_data->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	charger_data->ac.get_property	= tps65090_ac_get_property;
	charger_data->ac.properties	= tps65090_ac_props;
	charger_data->ac.num_properties	= ARRAY_SIZE(tps65090_ac_props);

	ret = power_supply_register(&pdev->dev, &charger_data->ac);
	if (ret) {
		dev_err(&pdev->dev, "failed: power supply register\n");
		goto fail_suppy_reg;
	}

	charger_data->bc_dev = battery_charger_register(&pdev->dev,
				&tps65090_charger_bci, charger_data);
	if (IS_ERR(charger_data->bc_dev)) {
		ret = PTR_ERR(charger_data->bc_dev);
		dev_err(&pdev->dev, "battery charger register failed: %d\n",
			ret);
		goto chg_reg_err;
	}

	ret = tps65090_config_charger(charger_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "charger config failed, err %d\n", ret);
		goto fail_config;
	}

	/* Check for charger presence */
	ret = tps65090_read(charger_data->dev->parent, TPS65090_CG_STATUS1,
			&retval);
	if (ret < 0) {
		dev_err(charger_data->dev, "%s(): Error in reading reg 0x%x",
			__func__, TPS65090_CG_STATUS1);
		goto fail_config;
	}

	if (retval != 0) {
		ret = tps65090_enable_charging(charger_data, 1);
		if (ret < 0) {
			dev_err(charger_data->dev, "error enabling charger\n");
			return ret;
		}
		charger_data->ac_online = 1;
		power_supply_changed(&charger_data->ac);
	}

	return 0;
fail_config:
	battery_charger_unregister(charger_data->bc_dev);
chg_reg_err:
	power_supply_unregister(&charger_data->ac);

fail_suppy_reg:
	free_irq(charger_data->irq_base, charger_data);
	return ret;
}

static int tps65090_charger_remove(struct platform_device *pdev)
{
	struct tps65090_charger *charger = dev_get_drvdata(&pdev->dev);

	battery_charger_unregister(charger->bc_dev);
	power_supply_unregister(&charger->ac);
	free_irq(charger->irq_base, charger);
	return 0;
}

static struct platform_driver tps65090_charger_driver = {
	.driver	= {
		.name	= "tps65090-charger",
		.owner	= THIS_MODULE,
	},
	.probe	= tps65090_charger_probe,
	.remove = tps65090_charger_remove,
};

module_platform_driver(tps65090_charger_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com>");
MODULE_DESCRIPTION("tps65090 battery charger driver");
