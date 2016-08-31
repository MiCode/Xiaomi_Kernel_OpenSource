/*
 * Battery driver for Maxim MAX8907C
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/mfd/max8907c.h>
#include <linux/power/max8907c-charger.h>
#include <linux/slab.h>

struct max8907c_charger {
	struct max8907c_charger_pdata *pdata;
	struct max8907c *chip;
	struct i2c_client *i2c;
	int online;
};

static void max8907c_set_charger(struct max8907c_charger *charger)
{
	struct max8907c_charger_pdata *pdata = charger->pdata;
	int ret;
	if (charger->online) {
		ret = max8907c_reg_write(charger->i2c, MAX8907C_REG_CHG_CNTL1,
					 (pdata->topoff_threshold << 5) |
					 (pdata->restart_hysteresis << 3) |
					 (pdata->fast_charging_current));
		if (unlikely(ret != 0))
			pr_err("Failed to set CHG_CNTL1: %d\n", ret);

		ret = max8907c_set_bits(charger->i2c, MAX8907C_REG_CHG_CNTL2,
					0x30, pdata->fast_charger_time << 4);
		if (unlikely(ret != 0))
			pr_err("Failed to set CHG_CNTL2: %d\n", ret);
	} else {
		ret = max8907c_set_bits(charger->i2c, MAX8907C_REG_CHG_CNTL1, 0x80, 0x1);
		if (unlikely(ret != 0))
			pr_err("Failed to set CHG_CNTL1: %d\n", ret);
	}
}

static irqreturn_t max8907c_charger_isr(int irq, void *dev_id)
{
	struct max8907c_charger *charger = dev_id;
	struct max8907c *chip = charger->chip;

	switch (irq - chip->irq_base) {
	case MAX8907C_IRQ_VCHG_DC_R:
		charger->online = 1;
		max8907c_set_charger(charger);
		break;
	case MAX8907C_IRQ_VCHG_DC_F:
		charger->online = 0;
		max8907c_set_charger(charger);
		break;
	}

	return IRQ_HANDLED;
}

static int max8907c_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	const static int types[] = {
		POWER_SUPPLY_CHARGE_TYPE_TRICKLE,
		POWER_SUPPLY_CHARGE_TYPE_FAST,
		POWER_SUPPLY_CHARGE_TYPE_FAST,
		POWER_SUPPLY_CHARGE_TYPE_NONE,
	};
	int ret = -ENODEV;
	int status;

	struct max8907c_charger *charger = dev_get_drvdata(psy->dev->parent);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->online;
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_STATUS:
		/* Get charger status from CHG_EN_STAT */
		status = max8907c_reg_read(charger->i2c, MAX8907C_REG_CHG_STAT);
		val->intval = ((status & 0x10) == 0x10) ?
				POWER_SUPPLY_STATUS_CHARGING :
				POWER_SUPPLY_STATUS_NOT_CHARGING;
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		/* Get charging type from CHG_MODE */
		status = max8907c_reg_read(charger->i2c, MAX8907C_REG_CHG_STAT);
		val->intval = types[(status & 0x0C) >> 2];
		ret = 0;
		break;

	default:
		val->intval = 0;
		ret = -EINVAL;
		break;
	}
	return ret;
}

static enum power_supply_property max8907c_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static struct power_supply max8907c_charger_ps = {
	.name = "charger",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = max8907c_charger_props,
	.num_properties = ARRAY_SIZE(max8907c_charger_props),
	.get_property = max8907c_charger_get_property,
};

static int max8907c_charger_probe(struct platform_device *pdev)
{
	struct max8907c_charger_pdata *pdata = pdev->dev.platform_data;
	struct max8907c_charger *charger = 0;
	struct max8907c *chip = dev_get_drvdata(pdev->dev.parent);
	int ret;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->pdata = pdata;
	charger->online = 0;
	charger->chip = chip;
	charger->i2c = chip->i2c_power;

	platform_set_drvdata(pdev, charger);

	ret = max8907c_reg_read(charger->i2c, MAX8907C_REG_CHG_STAT);
	if (ret & (1 << 7)) {
		charger->online = 1;
		max8907c_set_charger(charger);
	}

	ret = request_threaded_irq(chip->irq_base + MAX8907C_IRQ_VCHG_DC_F, NULL,
				   max8907c_charger_isr, IRQF_ONESHOT,
				   "power-remove", charger);
	if (unlikely(ret < 0)) {
		pr_debug("max8907c: failed to request IRQ	%X\n", ret);
		goto out;
	}

	ret = request_threaded_irq(chip->irq_base + MAX8907C_IRQ_VCHG_DC_R, NULL,
				   max8907c_charger_isr, IRQF_ONESHOT,
				   "power-insert", charger);
	if (unlikely(ret < 0)) {
		pr_debug("max8907c: failed to request IRQ	%X\n", ret);
		goto out1;
	}


	ret = power_supply_register(&pdev->dev, &max8907c_charger_ps);
	if (unlikely(ret != 0)) {
		pr_err("Failed to register max8907c_charger driver: %d\n", ret);
		goto out2;
	}

	return 0;
out2:
	free_irq(chip->irq_base + MAX8907C_IRQ_VCHG_DC_R, charger);
out1:
	free_irq(chip->irq_base + MAX8907C_IRQ_VCHG_DC_F, charger);
out:
	kfree(charger);
	return ret;
}

static int max8907c_charger_remove(struct platform_device *pdev)
{
	struct max8907c_charger *charger = platform_get_drvdata(pdev);
	struct max8907c *chip = charger->chip;
	int ret;

	ret = max8907c_reg_write(charger->i2c, MAX8907C_REG_CHG_IRQ1_MASK, 0xFF);
	if (unlikely(ret != 0)) {
		pr_err("Failed to set IRQ1_MASK: %d\n", ret);
		goto out;
	}

	free_irq(chip->irq_base + MAX8907C_IRQ_VCHG_DC_R, charger);
	free_irq(chip->irq_base + MAX8907C_IRQ_VCHG_DC_F, charger);
	power_supply_unregister(&max8907c_charger_ps);
out:
	kfree(charger);
	return 0;
}

static struct platform_driver max8907c_charger_driver = {
	.probe		= max8907c_charger_probe,
	.remove		= max8907c_charger_remove,
	.driver		= {
		.name	= "max8907c-charger",
	},
};

static int __init max8907c_charger_init(void)
{
	return platform_driver_register(&max8907c_charger_driver);
}
module_init(max8907c_charger_init);

static void __exit max8907c_charger_exit(void)
{
	platform_driver_unregister(&max8907c_charger_driver);
}
module_exit(max8907c_charger_exit);

MODULE_DESCRIPTION("Charger driver for MAX8907C");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL");
