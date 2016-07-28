/*
 * Copyright (c) 2011-2013, 2016, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <linux/qpnp/qpnp-adc.h>

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
	struct spmi_device		*spmi_dev;
	struct thermal_zone_device	*tz_dev;
	const char			*tm_name;
	unsigned int			subtype;
	enum qpnp_tm_adc_type		adc_type;
	unsigned long			temperature;
	enum thermal_device_mode	mode;
	unsigned int			thresh;
	unsigned int			clock_rate;
	unsigned int			stage;
	unsigned int			prev_stage;
	int				irq;
	enum qpnp_vadc_channels		adc_channel;
	u16				base_addr;
	bool				allow_software_override;
	struct qpnp_vadc_chip		*vadc_dev;
};

/* Delay between TEMP_STAT IRQ going high and status value changing in ms. */
#define STATUS_REGISTER_DELAY_MS       40

enum pmic_thermal_override_mode {
	SOFTWARE_OVERRIDE_DISABLED = 0,
	SOFTWARE_OVERRIDE_ENABLED,
};

/* This array maps from GEN2 alarm state to GEN1 alarm stage */
const unsigned int alarm_state_map[8] = {0, 1, 1, 2, 2, 3, 3, 3};

static inline int qpnp_tm_read(struct qpnp_tm_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = spmi_ext_register_readl(chip->spmi_dev->ctrl,
			chip->spmi_dev->sid, chip->base_addr + addr, buf, len);

	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: spmi_ext_register_readl() failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__, chip->spmi_dev->sid, chip->base_addr + addr,
			len, rc);

	return rc;
}

static inline int qpnp_tm_write(struct qpnp_tm_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl,
			chip->spmi_dev->sid, chip->base_addr + addr, buf, len);

	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: spmi_ext_register_writel() failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__, chip->spmi_dev->sid, chip->base_addr + addr,
			len, rc);

	return rc;
}


static inline int qpnp_tm_shutdown_override(struct qpnp_tm_chip *chip,
			    enum pmic_thermal_override_mode mode)
{
	int rc = 0;
	u8 reg;

	if (chip->allow_software_override) {
		reg = chip->thresh & SHUTDOWN_CTRL1_THRESHOLD_MASK;
		reg |= (chip->clock_rate << SHUTDOWN_CTRL1_CLK_RATE_SHIFT)
			& SHUTDOWN_CTRL1_CLK_RATE_MASK;

		if (mode == SOFTWARE_OVERRIDE_ENABLED)
			reg |= SHUTDOWN_CTRL1_OVERRIDE_STAGE2
				| SHUTDOWN_CTRL1_OVERRIDE_STAGE3;

		rc = qpnp_tm_write(chip, QPNP_TM_REG_SHUTDOWN_CTRL1, &reg, 1);
	}

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
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_vadc_read(%d) failed, rc=%d\n",
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

static int qpnp_tz_get_temp_no_adc(struct thermal_zone_device *thermal,
				     unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int rc;

	if (!temperature)
		return -EINVAL;

	rc = qpnp_tm_update_temp_no_adc(chip);
	if (rc < 0)
		return rc;

	*temperature = chip->temperature;

	return 0;
}

static int qpnp_tz_get_temp_qpnp_adc(struct thermal_zone_device *thermal,
				      unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int rc;

	if (!temperature)
		return -EINVAL;

	rc = qpnp_tm_update_temp(chip);
	if (rc < 0) {
		dev_err(&chip->spmi_dev->dev, "%s: %s: adc read failed, rc = %d\n",
			__func__, chip->tm_name, rc);
		return rc;
	}

	*temperature = chip->temperature;

	return 0;
}

static int qpnp_tz_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct qpnp_tm_chip *chip = thermal->devdata;

	if (!mode)
		return -EINVAL;

	*mode = chip->mode;

	return 0;
}

static int qpnp_tz_set_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode mode)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int rc = 0;

	if (mode != chip->mode) {
		if (mode == THERMAL_DEVICE_ENABLED)
			rc = qpnp_tm_shutdown_override(chip,
				SOFTWARE_OVERRIDE_ENABLED);
		else
			rc = qpnp_tm_shutdown_override(chip,
				SOFTWARE_OVERRIDE_DISABLED);

		chip->mode = mode;
	}

	return rc;
}

static int qpnp_tz_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	if (trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case TRIP_STAGE3:
		*type = THERMAL_TRIP_CRITICAL;
		break;
	case TRIP_STAGE2:
		*type = THERMAL_TRIP_HOT;
		break;
	case TRIP_STAGE1:
		*type = THERMAL_TRIP_HOT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qpnp_tz_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;
	int thresh_temperature;

	if (trip < 0 || !temperature)
		return -EINVAL;

	thresh_temperature = chip->thresh * TEMP_THRESH_STEP + TEMP_THRESH_MIN;

	switch (trip) {
	case TRIP_STAGE3:
		thresh_temperature += 2 * TEMP_STAGE_STEP;
		break;
	case TRIP_STAGE2:
		thresh_temperature += TEMP_STAGE_STEP;
		break;
	case TRIP_STAGE1:
		break;
	default:
		return -EINVAL;
	}

	*temperature = thresh_temperature;

	return 0;
}

static int qpnp_tz_get_crit_temp(struct thermal_zone_device *thermal,
				   unsigned long *temperature)
{
	struct qpnp_tm_chip *chip = thermal->devdata;

	if (!temperature)
		return -EINVAL;

	*temperature = chip->thresh * TEMP_THRESH_STEP + TEMP_THRESH_MIN +
		2 * TEMP_STAGE_STEP;

	return 0;
}

static struct thermal_zone_device_ops qpnp_thermal_zone_ops_no_adc = {
	.get_temp = qpnp_tz_get_temp_no_adc,
	.get_mode = qpnp_tz_get_mode,
	.set_mode = qpnp_tz_set_mode,
	.get_trip_type = qpnp_tz_get_trip_type,
	.get_trip_temp = qpnp_tz_get_trip_temp,
	.get_crit_temp = qpnp_tz_get_crit_temp,
};

static struct thermal_zone_device_ops qpnp_thermal_zone_ops_qpnp_adc = {
	.get_temp = qpnp_tz_get_temp_qpnp_adc,
	.get_mode = qpnp_tz_get_mode,
	.set_mode = qpnp_tz_set_mode,
	.get_trip_type = qpnp_tz_get_trip_type,
	.get_trip_temp = qpnp_tz_get_trip_temp,
	.get_crit_temp = qpnp_tz_get_crit_temp,
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
			pr_crit("%s: PMIC Temp Alarm - stage=%u, threshold=%u, temperature=%lu mC\n",
				chip->tm_name, chip->stage, chip->thresh,
				chip->temperature);
		else
			pr_crit("%s: PMIC Temp Alarm - stage=%u, state=%u, threshold=%u, temperature=%lu mC\n",
				chip->tm_name, stage_new, chip->stage,
				chip->thresh, chip->temperature);

		thermal_zone_device_update(chip->tz_dev);

		/* Notify user space */
		sysfs_notify(&chip->tz_dev->device.kobj, NULL, "type");
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

static int qpnp_tm_probe(struct spmi_device *spmi)
{
	struct device_node *node;
	struct resource *res;
	struct qpnp_tm_chip *chip;
	struct thermal_zone_device_ops *tz_ops;
	char *tm_name;
	u32 default_temperature;
	int rc = 0;
	u8 raw_type[2], type, subtype;

	if (!spmi || !(&spmi->dev) || !spmi->dev.of_node) {
		dev_err(&spmi->dev, "%s: device tree node not found\n",
			__func__);
		return -EINVAL;
	}

	node = spmi->dev.of_node;

	chip = kzalloc(sizeof(struct qpnp_tm_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&spmi->dev, "%s: Can't allocate qpnp_tm_chip\n",
			__func__);
		return -ENOMEM;
	}

	dev_set_drvdata(&spmi->dev, chip);

	res = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing base address\n",
			__func__);
		rc = -EINVAL;
		goto free_chip;
	}
	chip->base_addr	= res->start;
	chip->spmi_dev	= spmi;

	chip->irq = spmi_get_irq(spmi, NULL, 0);
	if (chip->irq < 0) {
		rc = chip->irq;
		dev_err(&spmi->dev, "%s: node is missing irq, rc=%d\n",
			__func__, rc);
		goto free_chip;
	}

	chip->tm_name = of_get_property(node, "label", NULL);
	if (chip->tm_name == NULL) {
		dev_err(&spmi->dev, "%s: node is missing label\n",
			__func__);
		rc = -EINVAL;
		goto free_chip;
	}

	tm_name = kstrdup(chip->tm_name, GFP_KERNEL);
	if (tm_name == NULL) {
		dev_err(&spmi->dev, "%s: could not allocate memory for label\n",
			__func__);
		rc = -ENOMEM;
		goto free_chip;
	}
	chip->tm_name = tm_name;

	INIT_DELAYED_WORK(&chip->irq_work, qpnp_tm_work);

	/* These bindings are optional, so it is okay if they are not found. */
	chip->thresh = THRESH_MAX + 1;
	rc = of_property_read_u32(node, "qcom,threshold-set", &chip->thresh);
	if (!rc && (chip->thresh < THRESH_MIN || chip->thresh > THRESH_MAX))
		dev_err(&spmi->dev, "%s: invalid qcom,threshold-set=%u specified\n",
			__func__, chip->thresh);

	chip->clock_rate = CLOCK_RATE_MAX + 1;
	rc = of_property_read_u32(node, "qcom,clock-rate", &chip->clock_rate);
	if (!rc && (chip->clock_rate < CLOCK_RATE_MIN
		    || chip->clock_rate > CLOCK_RATE_MAX))
		dev_err(&spmi->dev,
			"%s: invalid qcom,clock-rate=%u specified\n", __func__,
			chip->clock_rate);

	chip->adc_type = QPNP_TM_ADC_NONE;
	rc = of_property_read_u32(node, "qcom,channel-num", &chip->adc_channel);
	if (!rc) {
		if (chip->adc_channel < 0 || chip->adc_channel >= ADC_MAX_NUM) {
			dev_err(&spmi->dev, "%s: invalid qcom,channel-num=%d specified\n",
				__func__, chip->adc_channel);
		} else {
			chip->adc_type = QPNP_TM_ADC_QPNP_ADC;
			chip->vadc_dev = qpnp_get_vadc(&spmi->dev,
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

	chip->allow_software_override
		= of_property_read_bool(node, "qcom,allow-override");

	default_temperature = DEFAULT_NO_ADC_TEMP;
	rc = of_property_read_u32(node, "qcom,default-temp",
					&default_temperature);
	chip->temperature = default_temperature;

	rc = qpnp_tm_read(chip, QPNP_TM_REG_TYPE, raw_type, 2);
	if (rc) {
		dev_err(&spmi->dev, "%s: could not read type register, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}
	type = raw_type[0];
	subtype = raw_type[1];

	if (type != QPNP_TM_TYPE || (subtype != QPNP_TM_SUBTYPE_GEN1
				     && subtype != QPNP_TM_SUBTYPE_GEN2)) {
		dev_err(&spmi->dev, "%s: invalid type=%02X or subtype=%02X register value\n",
			__func__, type, subtype);
		rc = -ENODEV;
		goto err_cancel_work;
	}

	chip->subtype = subtype;

	rc = qpnp_tm_init_reg(chip);
	if (rc) {
		dev_err(&spmi->dev, "%s: qpnp_tm_init_reg() failed, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}

	if (chip->adc_type == QPNP_TM_ADC_NONE) {
		rc = qpnp_tm_init_temp_no_adc(chip);
		if (rc) {
			dev_err(&spmi->dev, "%s: qpnp_tm_init_temp_no_adc() failed, rc=%d\n",
				__func__, rc);
			goto err_cancel_work;
		}
	}

	/* Start in HW control; switch to SW control when user changes mode. */
	chip->mode = THERMAL_DEVICE_DISABLED;
	rc = qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);
	if (rc) {
		dev_err(&spmi->dev, "%s: qpnp_tm_shutdown_override() failed, rc=%d\n",
			__func__, rc);
		goto err_cancel_work;
	}

	chip->tz_dev = thermal_zone_device_register(tm_name, TRIP_NUM, 0, chip,
			tz_ops, NULL, 0, 0);
	if (chip->tz_dev == NULL) {
		dev_err(&spmi->dev, "%s: thermal_zone_device_register() failed.\n",
			__func__);
		rc = -ENODEV;
		goto err_cancel_work;
	}

	rc = request_irq(chip->irq, qpnp_tm_isr, IRQF_TRIGGER_RISING, tm_name,
			chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "%s: request_irq(%d) failed: %d\n",
			__func__, chip->irq, rc);
		goto err_free_tz;
	}

	return 0;

err_free_tz:
	thermal_zone_device_unregister(chip->tz_dev);
err_cancel_work:
	cancel_delayed_work_sync(&chip->irq_work);
	kfree(chip->tm_name);
free_chip:
	dev_set_drvdata(&spmi->dev, NULL);
	kfree(chip);
	return rc;
}

static int qpnp_tm_remove(struct spmi_device *spmi)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(&spmi->dev);

	dev_set_drvdata(&spmi->dev, NULL);
	thermal_zone_device_unregister(chip->tz_dev);
	kfree(chip->tm_name);
	qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);
	free_irq(chip->irq, chip);
	cancel_delayed_work_sync(&chip->irq_work);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
static int qpnp_tm_suspend(struct device *dev)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(dev);

	/* Clear override bits in suspend to allow hardware control */
	qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);

	return 0;
}

static int qpnp_tm_resume(struct device *dev)
{
	struct qpnp_tm_chip *chip = dev_get_drvdata(dev);

	/* Override hardware actions so software can control */
	if (chip->mode == THERMAL_DEVICE_ENABLED)
		qpnp_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_ENABLED);

	return 0;
}

static const struct dev_pm_ops qpnp_tm_pm_ops = {
	.suspend = qpnp_tm_suspend,
	.resume = qpnp_tm_resume,
};

#define QPNP_TM_PM_OPS	(&qpnp_tm_pm_ops)
#else
#define QPNP_TM_PM_OPS	NULL
#endif

static struct of_device_id qpnp_tm_match_table[] = {
	{ .compatible = QPNP_TM_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id qpnp_tm_id[] = {
	{ QPNP_TM_DRIVER_NAME, 0 },
	{}
};

static struct spmi_driver qpnp_tm_driver = {
	.driver = {
		.name		= QPNP_TM_DRIVER_NAME,
		.of_match_table	= qpnp_tm_match_table,
		.owner		= THIS_MODULE,
		.pm		= QPNP_TM_PM_OPS,
	},
	.probe	  = qpnp_tm_probe,
	.remove	  = qpnp_tm_remove,
	.id_table = qpnp_tm_id,
};

int __init qpnp_tm_init(void)
{
	return spmi_driver_register(&qpnp_tm_driver);
}

static void __exit qpnp_tm_exit(void)
{
	spmi_driver_unregister(&qpnp_tm_driver);
}

module_init(qpnp_tm_init);
module_exit(qpnp_tm_exit);

MODULE_DESCRIPTION("QPNP PMIC Temperature Alarm driver");
MODULE_LICENSE("GPL v2");
