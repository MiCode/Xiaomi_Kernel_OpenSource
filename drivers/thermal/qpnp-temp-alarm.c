/*
 * Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <linux/qpnp/qpnp-adc.h>

#include "thermal_core.h"

#define QPNP_TM_DRIVER_NAME "qcom,qpnp-temp-alarm"

enum qpnp_tm_registers {
	QPNP_TM_REG_TYPE		= 0x04,
	QPNP_TM_REG_SUBTYPE		= 0x05,
	QPNP_TM_REG_STATUS		= 0x08,
	QPNP_TM_REG_SHUTDOWN_CTRL1	= 0x40,
	QPNP_TM_REG_SHUTDOWN_CTRL2	= 0x42,
	QPNP_TM_REG_ALARM_CTRL		= 0x46,
};

#define QPNP_TM_TYPE			0x09
#define QPNP_TM_SUBTYPE_GEN1		0x08
#define QPNP_TM_SUBTYPE_GEN2		0x09

#define STATUS_STATE_MASK		0x70
#define STATUS_STATE_SHIFT		4
#define STATUS_STAGE_MASK		0x03

#define SHUTDOWN_CTRL1_OVERRIDE_STAGE3	0x80
#define SHUTDOWN_CTRL1_OVERRIDE_STAGE2	0x40
#define SHUTDOWN_CTRL1_CLK_RATE_MASK	0x0C
#define SHUTDOWN_CTRL1_CLK_RATE_SHIFT	2
#define SHUTDOWN_CTRL1_THRESHOLD_MASK	0x03

#define SHUTDOWN_CTRL2_CLEAR_STAGE3	0x80
#define SHUTDOWN_CTRL2_CLEAR_STAGE2	0x40

#define ALARM_CTRL_FORCE_ENABLE		0x80
#define ALARM_CTRL_FOLLOW_HW_ENABLE	0x01

#define TEMP_STAGE_STEP			20000	/* Stage step: 20.000 C */
#define TEMP_STAGE_HYSTERESIS		2000

#define TEMP_THRESH_MIN			105000	/* Threshold Min: 105 C */
#define TEMP_THRESH_STEP		5000	/* Threshold step: 5 C */

#define THRESH_MIN			0
#define THRESH_MAX			3

#define CLOCK_RATE_MIN			0
#define CLOCK_RATE_MAX			3

/* Trip points from most critical to least critical */
#define TRIP_STAGE3			0
#define TRIP_STAGE2			1
#define TRIP_STAGE1			2
#define TRIP_NUM			3

enum qpnp_tm_adc_type {
	QPNP_TM_ADC_NONE,	/* Estimates temp based on overload level. */
	QPNP_TM_ADC_QPNP_ADC,
};

/*
 * Temperature in millicelcius reported during stage 0 if no ADC is present and
 * no value has been specified via device tree.
 */
#define DEFAULT_NO_ADC_TEMP		37000

struct qpnp_tm_chip {
	struct delayed_work		irq_work;
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct thermal_zone_device	*tz_dev;
	const char			*tm_name;
	unsigned int			subtype;
	enum qpnp_tm_adc_type		adc_type;
	int				temperature;
	unsigned int			thresh;
	unsigned int			clock_rate;
	unsigned int			stage;
	unsigned int			prev_stage;
	int				irq;
	enum qpnp_vadc_channels		adc_channel;
	u16				base_addr;
	struct qpnp_vadc_chip		*vadc_dev;
};

/* Delay between TEMP_STAT IRQ going high and status value changing in ms. */
#define STATUS_REGISTER_DELAY_MS       40

/* This array maps from GEN2 alarm state to GEN1 alarm stage */
const unsigned int alarm_state_map[8] = {0, 1, 1, 2, 2, 3, 3, 3};

static inline int qpnp_tm_read(struct qpnp_tm_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = regmap_bulk_read(chip->regmap, chip->base_addr + addr, buf, len);

	if (rc)
		dev_err(&chip->pdev->dev,
			"%s: regmap_bulk_readl failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__,
			to_spmi_device(chip->pdev->dev.parent)->usid,
			chip->base_addr + addr,
			len, rc);

	return rc;
}

static inline int qpnp_tm_write(struct qpnp_tm_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, chip->base_addr + addr, buf, len);

	if (rc)
		dev_err(&chip->pdev->dev,
			"%s: regmap_bulk_write failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__,
			to_spmi_device(chip->pdev->dev.parent)->usid,
			chip->base_addr + addr,
			len, rc);

	return rc;
}

static int qpnp_tm_update_temp(struct qpnp_tm_chip *chip)
{
	struct qpnp_vadc_result adc_result;
	int rc;

	rc = qpnp_vadc_read(chip->vadc_dev, chip->adc_channel, &adc_result);
	if (!rc)
		chip->temperature = adc_result.physical;
	else
		dev_err(&chip->pdev->dev,
			"%s: qpnp_vadc_read(%d) failed, rc=%d\n",
			__func__, chip->adc_channel, rc);

	return rc;
}

static int qpnp_tm_get_temp_stage(struct qpnp_tm_chip *chip,
			unsigned int *stage)
{
	int rc;
	u8 reg;

	rc = qpnp_tm_read(chip, QPNP_TM_REG_STATUS, &reg, 1);
	if (rc < 0)
		return rc;

	if (chip->subtype == QPNP_TM_SUBTYPE_GEN1)
		*stage = reg & STATUS_STAGE_MASK;
	else
		*stage = (reg & STATUS_STATE_MASK) >> STATUS_STATE_SHIFT;

	return 0;
}

/*
 * This function initializes the internal temperature value based on only the
 * current thermal stage and threshold.
 */
static int qpnp_tm_init_temp_no_adc(struct qpnp_tm_chip *chip)
{
	unsigned int stage;
	int rc;

	rc = qpnp_tm_get_temp_stage(chip, &chip->stage);
	if (rc < 0)
		return rc;

	stage = chip->subtype == QPNP_TM_SUBTYPE_GEN1
		? chip->stage : alarm_state_map[chip->stage];

	if (stage)
		chip->temperature = chip->thresh * TEMP_THRESH_STEP +
			   (stage - 1) * TEMP_STAGE_STEP +
			   TEMP_THRESH_MIN;

	return 0;
}

/*
 * This function updates the internal temperature value based on the
 * current thermal stage and threshold as well as the previous stage
 */
static int qpnp_tm_update_temp_no_adc(struct qpnp_tm_chip *chip)
{
	unsigned int stage, stage_new, stage_old;
	int rc;

	rc = qpnp_tm_get_temp_stage(chip, &stage);
	if (rc < 0)
		return rc;

	if (chip->subtype == QPNP_TM_SUBTYPE_GEN1) {
		stage_new = stage;
		stage_old = chip->stage;
	} else {
		stage_new = alarm_state_map[stage];
		stage_old = alarm_state_map[chip->stage];
	}

	if (stage_new > stage_old) {
		/* increasing stage, use lower bound */
		chip->temperature = (stage_new - 1) * TEMP_STAGE_STEP
				+ chip->thresh * TEMP_THRESH_STEP
				+ TEMP_STAGE_HYSTERESIS + TEMP_THRESH_MIN;
	} else if (stage_new < stage_old) {
		/* decreasing stage, use upper bound */
		chip->temperature = stage_new * TEMP_STAGE_STEP
				+ chip->thresh * TEMP_THRESH_STEP
				- TEMP_STAGE_HYSTERESIS + TEMP_THRESH_MIN;
	}

	chip->stage = stage;

	return 0;
}

static int qpnp_tz_get_temp_no_adc(void *data, int *temperature)
{
	struct qpnp_tm_chip *chip = (struct qpnp_tm_chip *)data;
	int rc;

	if (!temperature)
		return -EINVAL;

	rc = qpnp_tm_update_temp_no_adc(chip);
	if (rc < 0)
		return rc;

	*temperature = chip->temperature;

	return 0;
}

static int qpnp_tz_get_temp_qpnp_adc(void *data, int *temperature)
{
	struct qpnp_tm_chip *chip = (struct qpnp_tm_chip *)data;
	int rc;

	if (!temperature)
		return -EINVAL;

	rc = qpnp_tm_update_temp(chip);
	if (rc < 0) {
		dev_err(&chip->pdev->dev,
			"%s: %s: adc read failed, rc = %d\n",
			__func__, chip->tm_name, rc);
		return rc;
	}

	*temperature = chip->temperature;

	return 0;
}

static struct thermal_zone_of_device_ops qpnp_thermal_zone_ops_no_adc = {
	.get_temp = qpnp_tz_get_temp_no_adc,
};

static struct thermal_zone_of_device_ops qpnp_thermal_zone_ops_qpnp_adc = {
	.get_temp = qpnp_tz_get_temp_qpnp_adc,
};

static void qpnp_tm_work(struct work_struct *work)
{
	struct delayed_work *dwork
		= container_of(work, struct delayed_work, work);
	struct qpnp_tm_chip *chip
		= container_of(dwork, struct qpnp_tm_chip, irq_work);
	unsigned int stage_new, stage_old;
	int rc;

	if (chip->adc_type == QPNP_TM_ADC_NONE) {
		rc = qpnp_tm_update_temp_no_adc(chip);
		if (rc < 0)
			goto bail;
	} else {
		rc = qpnp_tm_get_temp_stage(chip, &chip->stage);
		if (rc < 0)
			goto bail;

		rc = qpnp_tm_update_temp(chip);
		if (rc < 0)
			goto bail;
	}

	if (chip->subtype == QPNP_TM_SUBTYPE_GEN1) {
		stage_new = chip->stage;
		stage_old = chip->prev_stage;
	} else {
		stage_new = alarm_state_map[chip->stage];
		stage_old = alarm_state_map[chip->prev_stage];
	}

	chip->prev_stage = chip->stage;

	if (stage_new != stage_old) {
		if (chip->subtype == QPNP_TM_SUBTYPE_GEN1)
			pr_crit("%s: PMIC Temp Alarm - stage=%u, threshold=%u, temperature=%d mC\n",
				chip->tm_name, chip->stage, chip->thresh,
				chip->temperature);
		else
			pr_crit("%s: PMIC Temp Alarm - stage=%u, state=%u, threshold=%u, temperature=%d mC\n",
				chip->tm_name, stage_new, chip->stage,
				chip->thresh, chip->temperature);

		of_thermal_handle_trip(chip->tz_dev);
	}

bail:
	return;
}

static irqreturn_t qpnp_tm_isr(int irq, void *data)
{
	struct qpnp_tm_chip *chip = data;

	schedule_delayed_work(&chip->irq_work,
			msecs_to_jiffies(STATUS_REGISTER_DELAY_MS) + 1);

	return IRQ_HANDLED;
}

static int qpnp_tm_init_reg(struct qpnp_tm_chip *chip)
{
	int rc = 0;
	u8 reg;

	rc = qpnp_tm_read(chip, QPNP_TM_REG_SHUTDOWN_CTRL1, &reg, 1);
	if (rc < 0)
		return rc;

	if (chip->thresh < THRESH_MIN || chip->thresh > THRESH_MAX) {
		/* Use hardware threshold value if configuration is invalid. */
		chip->thresh = reg & SHUTDOWN_CTRL1_THRESHOLD_MASK;
	}

	if (chip->clock_rate < CLOCK_RATE_MIN
	    || chip->clock_rate > CLOCK_RATE_MAX) {
		/* Use hardware clock rate value if configuration is invalid. */
		chip->clock_rate = (reg & SHUTDOWN_CTRL1_CLK_RATE_MASK)
					>> SHUTDOWN_CTRL1_CLK_RATE_SHIFT;
	}

	/*
	 * Set threshold and clock rate and also disable software override of
	 * stage 2 and 3 shutdowns.
	 */
	reg = chip->thresh & SHUTDOWN_CTRL1_THRESHOLD_MASK;
	reg |= (chip->clock_rate << SHUTDOWN_CTRL1_CLK_RATE_SHIFT)
		& SHUTDOWN_CTRL1_CLK_RATE_MASK;
	rc = qpnp_tm_write(chip, QPNP_TM_REG_SHUTDOWN_CTRL1, &reg, 1);
	if (rc < 0)
		return rc;

	/* Enable the thermal alarm PMIC module in always-on mode. */
	reg = ALARM_CTRL_FORCE_ENABLE;
	rc = qpnp_tm_write(chip, QPNP_TM_REG_ALARM_CTRL, &reg, 1);

	return rc;
}

static int qpnp_tm_probe(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int base;
	struct qpnp_tm_chip *chip;
	struct thermal_zone_of_device_ops *tz_ops;
	char *tm_name;
	u32 default_temperature;
	int rc = 0;
	u8 raw_type[2], type, subtype;

	if (!pdev || !(&pdev->dev) || !pdev->dev.of_node) {
		dev_err(&pdev->dev, "%s: device tree node not found\n",
			__func__);
		return -EINVAL;
	}

	node = pdev->dev.of_node;

	chip = kzalloc(sizeof(struct qpnp_tm_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	dev_set_drvdata(&pdev->dev, chip);

	rc = of_property_read_u32(pdev->dev.of_node, "reg", &base);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Couldn't find reg in node = %s rc = %d\n",
			pdev->dev.of_node->full_name, rc);
		goto free_chip;
	}
	chip->base_addr	= base;
	chip->pdev	= pdev;

	chip->irq = platform_get_irq(pdev, 0);
	if (chip->irq < 0) {
		rc = chip->irq;
		dev_err(&pdev->dev, "%s: node is missing irq, rc=%d\n",
			__func__, rc);
		goto free_chip;
	}

	chip->tm_name = of_get_property(node, "label", NULL);
	if (chip->tm_name == NULL) {
		dev_err(&pdev->dev, "%s: node is missing label\n", __func__);
		rc = -EINVAL;
		goto free_chip;
	}

	tm_name = kstrdup(chip->tm_name, GFP_KERNEL);
	if (tm_name == NULL) {
		rc = -ENOMEM;
		goto free_chip;
	}
	chip->tm_name = tm_name;

	INIT_DELAYED_WORK(&chip->irq_work, qpnp_tm_work);

	/* These bindings are optional, so it is okay if they are not found. */
	chip->thresh = THRESH_MAX + 1;
	rc = of_property_read_u32(node, "qcom,threshold-set", &chip->thresh);
	if (!rc && (chip->thresh < THRESH_MIN || chip->thresh > THRESH_MAX))
		dev_err(&pdev->dev,
			"%s: invalid qcom,threshold-set=%u specified\n",
			__func__, chip->thresh);

	chip->clock_rate = CLOCK_RATE_MAX + 1;
	rc = of_property_read_u32(node, "qcom,clock-rate", &chip->clock_rate);
	if (!rc && (chip->clock_rate < CLOCK_RATE_MIN
		    || chip->clock_rate > CLOCK_RATE_MAX))
		dev_err(&pdev->dev,
			"%s: invalid qcom,clock-rate=%u specified\n", __func__,
			chip->clock_rate);

	chip->adc_type = QPNP_TM_ADC_NONE;
	rc = of_property_read_u32(node, "qcom,channel-num", &chip->adc_channel);
	if (!rc) {
		if (chip->adc_channel < 0 || chip->adc_channel >= ADC_MAX_NUM) {
			dev_err(&pdev->dev,
				"%s: invalid qcom,channel-num=%d specified\n",
				__func__, chip->adc_channel);
		} else {
			chip->adc_type = QPNP_TM_ADC_QPNP_ADC;
			chip->vadc_dev = qpnp_get_vadc(&pdev->dev,
							"temp_alarm");
			if (IS_ERR(chip->vadc_dev)) {
				rc = PTR_ERR(chip->vadc_dev);
				if (rc != -EPROBE_DEFER)
					pr_err("vadc property missing\n");
				goto err_cancel_work;
			}
		}
	}

	if (chip->adc_type == QPNP_TM_ADC_QPNP_ADC)
		tz_ops = &qpnp_thermal_zone_ops_qpnp_adc;
	else
		tz_ops = &qpnp_thermal_zone_ops_no_adc;

	default_temperature = DEFAULT_NO_ADC_TEMP;
	rc = of_property_read_u32(node, "qcom,default-temp",
					&default_temperature);
	chip->temperature = default_temperature;

	rc = qpnp_tm_read(chip, QPNP_TM_REG_TYPE, raw_type, 2);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: could not read type register, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}
	type = raw_type[0];
	subtype = raw_type[1];

	if (type != QPNP_TM_TYPE || (subtype != QPNP_TM_SUBTYPE_GEN1
				     && subtype != QPNP_TM_SUBTYPE_GEN2)) {
		dev_err(&pdev->dev,
			"%s: invalid type=%02X or subtype=%02X register value\n",
			__func__, type, subtype);
		rc = -ENODEV;
		goto err_cancel_work;
	}

	chip->subtype = subtype;

	rc = qpnp_tm_init_reg(chip);
	if (rc) {
		dev_err(&pdev->dev, "%s: qpnp_tm_init_reg() failed, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}

	if (chip->adc_type == QPNP_TM_ADC_NONE) {
		rc = qpnp_tm_init_temp_no_adc(chip);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: qpnp_tm_init_temp_no_adc() failed, rc=%d\n",
				__func__, rc);
			goto err_cancel_work;
		}
	}

	chip->tz_dev = thermal_zone_of_sensor_register(&pdev->dev, 0, chip,
							tz_ops);
	if (IS_ERR(chip->tz_dev)) {
		rc = PTR_ERR(chip->tz_dev);
		dev_err(&pdev->dev,
			"%s: thermal_zone_of_sensor_register() failed, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}

	rc = request_irq(chip->irq, qpnp_tm_isr, IRQF_TRIGGER_RISING, tm_name,
			chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "%s: request_irq(%d) failed: %d\n",
			__func__, chip->irq, rc);
		goto err_free_tz;
	}

	return 0;

err_free_tz:
	thermal_zone_of_sensor_unregister(&pdev->dev, chip->tz_dev);
err_cancel_work:
	cancel_delayed_work_sync(&chip->irq_work);
	kfree(chip->tm_name);
free_chip:
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(chip);
	return rc;
}

static int qpnp_tm_remove(struct platform_device *pdev)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(&pdev->dev);

	thermal_zone_of_sensor_unregister(&pdev->dev, chip->tz_dev);
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(chip->tm_name);
	free_irq(chip->irq, chip);
	cancel_delayed_work_sync(&chip->irq_work);
	kfree(chip);

	return 0;
}

static void qpnp_tm_shutdown(struct platform_device *pdev)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(&pdev->dev);
	int rc;
	u8 reg;

	/* configure TEMP_ALARM to follow HW_EN */
	reg = ALARM_CTRL_FOLLOW_HW_ENABLE;
	rc = qpnp_tm_write(chip, QPNP_TM_REG_ALARM_CTRL, &reg, 1);
	if (rc)
		pr_err("Failed to cfg. TEMP_ALARM to follow HW_EN rc=%d\n", rc);
}


static const struct of_device_id qpnp_tm_match_table[] = {
	{ .compatible = QPNP_TM_DRIVER_NAME, },
	{}
};

static const struct platform_device_id qpnp_tm_id[] = {
	{ QPNP_TM_DRIVER_NAME, 0 },
	{}
};

static struct platform_driver qpnp_tm_driver = {
	.driver = {
		.name		= QPNP_TM_DRIVER_NAME,
		.of_match_table	= qpnp_tm_match_table,
		.owner		= THIS_MODULE,
	},
	.probe	  = qpnp_tm_probe,
	.remove	  = qpnp_tm_remove,
	.shutdown = qpnp_tm_shutdown,
	.id_table = qpnp_tm_id,
};

int __init qpnp_tm_init(void)
{
	return platform_driver_register(&qpnp_tm_driver);
}

static void __exit qpnp_tm_exit(void)
{
	platform_driver_unregister(&qpnp_tm_driver);
}

module_init(qpnp_tm_init);
module_exit(qpnp_tm_exit);

MODULE_DESCRIPTION("QPNP PMIC Temperature Alarm driver");
MODULE_LICENSE("GPL v2");
