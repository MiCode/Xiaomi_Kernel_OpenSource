/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"AMOLED: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

#define QPNP_AMOLED_REGULATOR_DRIVER_NAME	"qcom,qpnp-amoled-regulator"

/* Register definitions */

#define PERIPH_TYPE			0x04
#define IBB_PERIPH_TYPE			0x20
#define AB_PERIPH_TYPE			0x24
#define OLEDB_PERIPH_TYPE		0x2C

/* AB */
#define AB_STATUS1(chip)		(chip->ab_base + 0x08)
#define AB_LDO_SW_DBG_CTL(chip)		(chip->ab_base + 0x72)

/* IBB */
#define IBB_PS_CTL(chip)		(chip->ibb_base + 0x50)
#define IBB_NLIMIT_DAC(chip)		(chip->ibb_base + 0x61)
#define IBB_SMART_PS_CTL(chip)		(chip->ibb_base + 0x65)

/* AB_STATUS1 */
#define VREG_OK_BIT			BIT(6)
#define VREG_OK_SHIFT			6

struct amoled_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct device_node	*node;
	unsigned int		mode;
	bool			enabled;
};

struct oledb_regulator {
	struct amoled_regulator	vreg;

	/* DT params */
	bool			swire_control;
};

struct ab_regulator {
	struct amoled_regulator	vreg;

	/* DT params */
	bool			swire_control;
};

struct ibb_regulator {
	struct amoled_regulator	vreg;

	/* DT params */
	bool			swire_control;
};

struct qpnp_amoled {
	struct device		*dev;
	struct regmap		*regmap;
	struct oledb_regulator	oledb;
	struct ab_regulator	ab;
	struct ibb_regulator	ibb;
	struct mutex		reg_lock;
	struct work_struct	aod_work;
	struct workqueue_struct *wq;

	/* DT params */
	u32			oledb_base;
	u32			ab_base;
	u32			ibb_base;
};

enum reg_type {
	OLEDB,
	AB,
	IBB,
};

int qpnp_amoled_read(struct qpnp_amoled *chip,
			u16 addr, u8 *value, u8 count)
{
	int rc = 0;

	rc = regmap_bulk_read(chip->regmap, addr, value, count);
	if (rc < 0)
		pr_err("Failed to read from addr=0x%02x rc=%d\n", addr, rc);

	return rc;
}

static int qpnp_amoled_write(struct qpnp_amoled *chip,
			u16 addr, u8 *value, u8 count)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, addr, value, count);
	if (rc < 0)
		pr_err("Failed to write to addr=0x%02x rc=%d\n", addr, rc);

	return rc;
}

int qpnp_amoled_masked_write(struct qpnp_amoled *chip,
				u16 addr, u8 mask, u8 value)
{
	int rc = 0;

	rc = regmap_update_bits(chip->regmap, addr, mask, value);
	if (rc < 0)
		pr_err("Failed to write addr=0x%02x value=0x%02x rc=%d\n",
			addr, value, rc);

	return rc;
}

/* AB regulator */

static int qpnp_ab_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->ab.vreg.enabled;
}

static int qpnp_ab_regulator_enable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ab.vreg.enabled = true;
	return 0;
}

static int qpnp_ab_regulator_disable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ab.vreg.enabled = false;
	return 0;
}

/* IBB regulator */

static int qpnp_ibb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->ibb.vreg.enabled;
}

static int qpnp_ibb_regulator_enable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ibb.vreg.enabled = true;
	return 0;
}

static int qpnp_ibb_regulator_disable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ibb.vreg.enabled = false;
	return 0;
}

/* common to AB and IBB */

static int qpnp_ab_ibb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->ab.swire_control || chip->ibb.swire_control)
		return 0;

	return 0;
}

static int qpnp_ab_ibb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->ab.swire_control || chip->ibb.swire_control)
		return 0;

	return 0;
}

#define AB_VREG_OK_POLL_TRIES		50
#define AB_VREG_OK_POLL_TIME_US		2000
#define AB_VREG_OK_POLL_HIGH_TRIES	8
#define AB_VREG_OK_POLL_HIGH_TIME_US	10000
#define AB_VREG_OK_POLL_AGAIN_TRIES	10

static int qpnp_ab_poll_vreg_ok(struct qpnp_amoled *chip, bool status)
{
	u32 i = AB_VREG_OK_POLL_TRIES, poll_us = AB_VREG_OK_POLL_TIME_US;
	bool swire_high = false, poll_again = false, monitor = false;
	u32 wait_time_us = 0;
	int rc;
	u8 val;

loop:
	while (i--) {
		/* Write a dummy value before reading AB_STATUS1 */
		rc = qpnp_amoled_write(chip, AB_STATUS1(chip), &val, 1);
		if (rc < 0)
			return rc;

		rc = qpnp_amoled_read(chip, AB_STATUS1(chip), &val, 1);
		if (rc < 0)
			return rc;

		wait_time_us += poll_us;
		if (((val & VREG_OK_BIT) >> VREG_OK_SHIFT) == status) {
			pr_debug("Waited for %d us\n", wait_time_us);

			/*
			 * Return if we're polling for VREG_OK low. Else, poll
			 * for VREG_OK high for at least 80 ms. IF VREG_OK stays
			 * high, then consider it as a valid SWIRE pulse.
			 */

			if (status) {
				swire_high = true;
				if (!poll_again && !monitor) {
					pr_debug("SWIRE is high, start monitoring\n");
					i = AB_VREG_OK_POLL_HIGH_TRIES;
					poll_us = AB_VREG_OK_POLL_HIGH_TIME_US;
					wait_time_us = 0;
					monitor = true;
				}

				if (poll_again)
					poll_again = false;
			} else {
				return 0;
			}
		} else {
			/*
			 * If we're here when polling for VREG_OK high, then it
			 * is possibly because of an intermittent SWIRE pulse.
			 * Ignore it and poll for valid SWIRE pulse again.
			 */
			if (status && swire_high && monitor) {
				pr_debug("SWIRE is low\n");
				poll_again = true;
				swire_high = false;
				break;
			}

			if (poll_again)
				poll_again = false;
		}

		usleep_range(poll_us, poll_us + 1);
	}

	/*
	 * If poll_again is set, then VREG_OK should be polled for another
	 * 100 ms for valid SWIRE signal.
	 */

	if (poll_again) {
		pr_debug("polling again for SWIRE\n");
		i = AB_VREG_OK_POLL_AGAIN_TRIES;
		poll_us = AB_VREG_OK_POLL_HIGH_TIME_US;
		wait_time_us = 0;
		goto loop;
	}

	/* If swire_high is set, then it's a valid SWIRE signal, return 0. */
	if (swire_high) {
		pr_debug("SWIRE is high\n");
		return 0;
	}

	pr_err("AB_STATUS1: %x poll for VREG_OK %d timed out\n", val, status);
	return -ETIMEDOUT;
}

static int qpnp_ibb_aod_config(struct qpnp_amoled *chip, bool aod)
{
	int rc;
	u8 ps_ctl, smart_ps_ctl, nlimit_dac;

	pr_debug("aod: %d\n", aod);
	if (aod) {
		ps_ctl = 0x82;
		smart_ps_ctl = 0;
		nlimit_dac = 0;
	} else {
		ps_ctl = 0x02;
		smart_ps_ctl = 0x80;
		nlimit_dac = 0x3;
	}

	rc = qpnp_amoled_write(chip, IBB_SMART_PS_CTL(chip), &smart_ps_ctl, 1);
	if (rc < 0)
		return rc;

	rc = qpnp_amoled_write(chip, IBB_NLIMIT_DAC(chip), &nlimit_dac, 1);
	if (rc < 0)
		return rc;

	rc = qpnp_amoled_write(chip, IBB_PS_CTL(chip), &ps_ctl, 1);
	return rc;
}

static void qpnp_amoled_aod_work(struct work_struct *work)
{
	struct qpnp_amoled *chip = container_of(work, struct qpnp_amoled,
					aod_work);
	u8 val = 0;
	unsigned int mode;
	int rc;

	mutex_lock(&chip->reg_lock);
	mode = chip->ab.vreg.mode;
	mutex_unlock(&chip->reg_lock);

	pr_debug("mode: %d\n", mode);
	if (mode == REGULATOR_MODE_NORMAL) {
		rc = qpnp_ibb_aod_config(chip, true);
		if (rc < 0)
			goto error;

		/* poll for VREG_OK high */
		rc = qpnp_ab_poll_vreg_ok(chip, true);
		if (rc < 0)
			goto error;

		/*
		 * As per the hardware recommendation, Wait for ~10 ms after
		 * polling for VREG_OK before changing the configuration when
		 * exiting from AOD mode.
		 */

		usleep_range(10000, 10001);

		rc = qpnp_ibb_aod_config(chip, false);
		if (rc < 0)
			goto error;
	} else if (mode == REGULATOR_MODE_IDLE) {
		/* poll for VREG_OK low */
		rc = qpnp_ab_poll_vreg_ok(chip, false);
		if (rc < 0)
			goto error;

		val = 0xF1;
	} else if (mode == REGULATOR_MODE_STANDBY) {
		/* Restore the normal configuration without any delay */
		rc = qpnp_ibb_aod_config(chip, false);
		if (rc < 0)
			goto error;
	}

	rc = qpnp_amoled_write(chip, AB_LDO_SW_DBG_CTL(chip), &val, 1);
error:
	if (rc < 0)
		pr_err("Failed to configure for mode %d\n", mode);
}

static int qpnp_ab_ibb_regulator_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_STANDBY &&
		mode != REGULATOR_MODE_IDLE) {
		pr_err("Unsupported mode %u\n", mode);
		return -EINVAL;
	}

	pr_debug("mode: %d\n", mode);
	if (mode == chip->ab.vreg.mode || mode == chip->ibb.vreg.mode)
		return 0;

	mutex_lock(&chip->reg_lock);
	chip->ab.vreg.mode = chip->ibb.vreg.mode = mode;
	mutex_unlock(&chip->reg_lock);

	queue_work(chip->wq, &chip->aod_work);
	return 0;
}

static unsigned int qpnp_ab_ibb_regulator_get_mode(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->ibb.vreg.mode;
}

static struct regulator_ops qpnp_amoled_ab_ops = {
	.enable		= qpnp_ab_regulator_enable,
	.disable	= qpnp_ab_regulator_disable,
	.is_enabled	= qpnp_ab_regulator_is_enabled,
	.set_voltage	= qpnp_ab_ibb_regulator_set_voltage,
	.get_voltage	= qpnp_ab_ibb_regulator_get_voltage,
	.set_mode	= qpnp_ab_ibb_regulator_set_mode,
	.get_mode	= qpnp_ab_ibb_regulator_get_mode,
};

static struct regulator_ops qpnp_amoled_ibb_ops = {
	.enable		= qpnp_ibb_regulator_enable,
	.disable	= qpnp_ibb_regulator_disable,
	.is_enabled	= qpnp_ibb_regulator_is_enabled,
	.set_voltage	= qpnp_ab_ibb_regulator_set_voltage,
	.get_voltage	= qpnp_ab_ibb_regulator_get_voltage,
	.set_mode	= qpnp_ab_ibb_regulator_set_mode,
	.get_mode	= qpnp_ab_ibb_regulator_get_mode,
};

/* OLEDB regulator */

static int qpnp_oledb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->oledb.vreg.enabled;
}

static int qpnp_oledb_regulator_enable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.enabled = true;
	return 0;
}

static int qpnp_oledb_regulator_disable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.enabled = false;
	return 0;
}

static int qpnp_oledb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->oledb.swire_control)
		return 0;

	return 0;
}

static int qpnp_oledb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->oledb.swire_control)
		return 0;

	return 0;
}

static int qpnp_oledb_regulator_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.mode = mode;
	return 0;
}

static unsigned int qpnp_oledb_regulator_get_mode(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->oledb.vreg.mode;
}

static struct regulator_ops qpnp_amoled_oledb_ops = {
	.enable		= qpnp_oledb_regulator_enable,
	.disable	= qpnp_oledb_regulator_disable,
	.is_enabled	= qpnp_oledb_regulator_is_enabled,
	.set_voltage	= qpnp_oledb_regulator_set_voltage,
	.get_voltage	= qpnp_oledb_regulator_get_voltage,
	.set_mode	= qpnp_oledb_regulator_set_mode,
	.get_mode	= qpnp_oledb_regulator_get_mode,
};

static int qpnp_amoled_regulator_register(struct qpnp_amoled *chip,
					enum reg_type type)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	struct regulator_desc *rdesc;
	struct regulator_dev *rdev;
	struct device_node *node;

	if (type == OLEDB) {
		node		= chip->oledb.vreg.node;
		rdesc		= &chip->oledb.vreg.rdesc;
		rdesc->ops	= &qpnp_amoled_oledb_ops;
		rdev		= chip->oledb.vreg.rdev;
	} else if (type == AB) {
		node		= chip->ab.vreg.node;
		rdesc		= &chip->ab.vreg.rdesc;
		rdesc->ops	= &qpnp_amoled_ab_ops;
		rdev		= chip->ab.vreg.rdev;
	} else if (type == IBB) {
		node		= chip->ibb.vreg.node;
		rdesc		= &chip->ibb.vreg.rdesc;
		rdesc->ops	= &qpnp_amoled_ibb_ops;
		rdev		= chip->ibb.vreg.rdev;
	} else {
		pr_err("Invalid regulator type %d\n", type);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(chip->dev, node, rdesc);
	if (!init_data) {
		pr_err("Failed to get regulator_init_data for type %d\n", type);
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		rdesc->owner	= THIS_MODULE;
		rdesc->type	= REGULATOR_VOLTAGE;
		rdesc->name	= init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = node;

		if (of_get_property(chip->dev->of_node, "parent-supply",
				NULL))
			init_data->supply_regulator = "parent";

		init_data->constraints.valid_ops_mask
				|= REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS
				| REGULATOR_CHANGE_MODE;
		init_data->constraints.valid_modes_mask
				|= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE
					| REGULATOR_MODE_STANDBY;

		rdev = devm_regulator_register(chip->dev, rdesc, &cfg);
		if (IS_ERR(rdev)) {
			rc = PTR_ERR(rdev);
			rdev = NULL;
			pr_err("Failed to register amoled regulator for type %d rc = %d\n",
				type, rc);
			return rc;
		}

		if (type == OLEDB)
			chip->oledb.vreg.mode = REGULATOR_MODE_NORMAL;
		else if (type == IBB)
			chip->ibb.vreg.mode = REGULATOR_MODE_NORMAL;
		else
			chip->ab.vreg.mode = REGULATOR_MODE_NORMAL;
	} else {
		pr_err("regulator name missing for type %d\n", type);
		return -EINVAL;
	}

	return rc;
}

static int qpnp_amoled_hw_init(struct qpnp_amoled *chip)
{
	int rc;

	rc = qpnp_amoled_regulator_register(chip, OLEDB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register OLEDB regulator rc=%d\n",
			rc);
		return rc;
	}

	rc = qpnp_amoled_regulator_register(chip, AB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register AB regulator rc=%d\n",
			rc);
		return rc;
	}

	rc = qpnp_amoled_regulator_register(chip, IBB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register IBB regulator rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

static int qpnp_amoled_parse_dt(struct qpnp_amoled *chip)
{
	struct device_node *temp, *node = chip->dev->of_node;
	const __be32 *prop_addr;
	bool swire_control;
	int rc = 0;
	u32 base, val;

	for_each_available_child_of_node(node, temp) {
		prop_addr = of_get_address(temp, 0, NULL, NULL);
		if (!prop_addr) {
			pr_err("Couldn't get reg address\n");
			return -EINVAL;
		}

		base = be32_to_cpu(*prop_addr);
		rc = regmap_read(chip->regmap, base + PERIPH_TYPE, &val);
		if (rc < 0) {
			pr_err("Couldn't read PERIPH_TYPE for base %x\n", base);
			return rc;
		}

		switch (val) {
		case OLEDB_PERIPH_TYPE:
			chip->oledb_base = base;
			chip->oledb.vreg.node = temp;
			swire_control = of_property_read_bool(temp,
						"qcom,swire-control");
			chip->oledb.swire_control = swire_control;
			break;
		case AB_PERIPH_TYPE:
			chip->ab_base = base;
			chip->ab.vreg.node = temp;
			swire_control = of_property_read_bool(temp,
						"qcom,swire-control");
			chip->ab.swire_control = swire_control;
			break;
		case IBB_PERIPH_TYPE:
			chip->ibb_base = base;
			chip->ibb.vreg.node = temp;
			swire_control = of_property_read_bool(temp,
						"qcom,swire-control");
			chip->ibb.swire_control = swire_control;
			break;
		default:
			pr_err("Unknown peripheral type 0x%x\n", val);
			return -EINVAL;
		}

	}

	return 0;
}

static int qpnp_amoled_regulator_probe(struct platform_device *pdev)
{
	int rc;
	struct device_node *node;
	struct qpnp_amoled *chip;

	node = pdev->dev.of_node;
	if (!node) {
		pr_err("No nodes defined\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	/*
	 * We need this workqueue to order the mode transitions that require
	 * timing considerations. This way, we can ensure whenever the mode
	 * transition is requested, it can be queued with high priority.
	 */
	chip->wq = alloc_ordered_workqueue("qpnp_amoled_wq", WQ_HIGHPRI);
	if (!chip->wq) {
		dev_err(chip->dev, "Unable to alloc workqueue\n");
		return -ENOMEM;
	}

	mutex_init(&chip->reg_lock);
	INIT_WORK(&chip->aod_work, qpnp_amoled_aod_work);
	chip->dev = &pdev->dev;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Failed to get the regmap handle\n");
		rc = -EINVAL;
		goto error;
	}

	dev_set_drvdata(&pdev->dev, chip);

	rc = qpnp_amoled_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to parse DT params rc=%d\n", rc);
		goto error;
	}

	rc = qpnp_amoled_hw_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to initialize HW rc=%d\n", rc);
		goto error;
	}

	return 0;
error:
	destroy_workqueue(chip->wq);
	return rc;
}

static int qpnp_amoled_regulator_remove(struct platform_device *pdev)
{
	struct qpnp_amoled *chip = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&chip->aod_work);
	destroy_workqueue(chip->wq);
	return 0;
}

static const struct of_device_id amoled_match_table[] = {
	{ .compatible = QPNP_AMOLED_REGULATOR_DRIVER_NAME, },
	{ },
};

static struct platform_driver qpnp_amoled_regulator_driver = {
	.driver		= {
		.name		= QPNP_AMOLED_REGULATOR_DRIVER_NAME,
		.of_match_table	= amoled_match_table,
	},
	.probe		= qpnp_amoled_regulator_probe,
	.remove		= qpnp_amoled_regulator_remove,
};
module_platform_driver(qpnp_amoled_regulator_driver);

MODULE_DESCRIPTION("QPNP AMOLED regulator driver");
MODULE_LICENSE("GPL v2");
