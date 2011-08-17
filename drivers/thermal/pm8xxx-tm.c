/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

/*
 * Qualcomm PMIC PM8xxx Thermal Manager driver
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/tm.h>
#include <linux/completion.h>
#include <linux/mfd/pm8921-adc.h>

/* Register TEMP_ALARM_CTRL bits */
#define	TEMP_ALARM_CTRL_ST3_SD		0x80
#define	TEMP_ALARM_CTRL_ST2_SD		0x40
#define	TEMP_ALARM_CTRL_STATUS_MASK	0x30
#define	TEMP_ALARM_CTRL_STATUS_SHIFT	4
#define	TEMP_ALARM_CTRL_THRESH_MASK	0x0C
#define	TEMP_ALARM_CTRL_THRESH_SHIFT	2
#define	TEMP_ALARM_CTRL_OVRD_ST3	0x02
#define	TEMP_ALARM_CTRL_OVRD_ST2	0x01
#define	TEMP_ALARM_CTRL_OVRD_MASK	0x03

#define	TEMP_STAGE_STEP			20000	/* Stage step: 20.000 C */
#define	TEMP_STAGE_HYSTERESIS		2000

#define	TEMP_THRESH_MIN			105000	/* Threshold Min: 105 C */
#define	TEMP_THRESH_STEP		5000	/* Threshold step: 5 C */

/* Register TEMP_ALARM_PWM bits */
#define	TEMP_ALARM_PWM_EN_MASK		0xC0
#define	TEMP_ALARM_PWM_EN_SHIFT		6
#define	TEMP_ALARM_PWM_PER_PRE_MASK	0x38
#define	TEMP_ALARM_PWM_PER_PRE_SHIFT	3
#define	TEMP_ALARM_PWM_PER_DIV_MASK	0x07
#define	TEMP_ALARM_PWM_PER_DIV_SHIFT	0

/* Trips: from critical to less critical */
#define TRIP_STAGE3			0
#define TRIP_STAGE2			1
#define TRIP_STAGE1			2
#define TRIP_NUM			3

struct pm8xxx_tm_chip {
	struct pm8xxx_tm_core_data	cdata;
	struct work_struct		irq_work;
	struct device			*dev;
	struct thermal_zone_device	*tz_dev;
	unsigned long			temp;
	enum thermal_device_mode	mode;
	unsigned int			thresh;
	unsigned int			stage;
	unsigned int			tempstat_irq;
	unsigned int			overtemp_irq;
	void				*adc_handle;
};

enum pmic_thermal_override_mode {
	SOFTWARE_OVERRIDE_DISABLED = 0,
	SOFTWARE_OVERRIDE_ENABLED,
};

static inline int pm8xxx_tm_read_ctrl(struct pm8xxx_tm_chip *chip, u8 *reg)
{
	int rc;

	rc = pm8xxx_readb(chip->dev->parent,
			  chip->cdata.reg_addr_temp_alarm_ctrl, reg);
	if (rc)
		pr_err("%s: pm8xxx_readb(0x%03X) failed, rc=%d\n",
			chip->cdata.tm_name,
			chip->cdata.reg_addr_temp_alarm_ctrl, rc);

	return rc;
}

static inline int pm8xxx_tm_write_ctrl(struct pm8xxx_tm_chip *chip, u8 reg)
{
	int rc;

	rc = pm8xxx_writeb(chip->dev->parent,
			   chip->cdata.reg_addr_temp_alarm_ctrl, reg);
	if (rc)
		pr_err("%s: pm8xxx_writeb(0x%03X)=0x%02X failed, rc=%d\n",
		       chip->cdata.tm_name,
		       chip->cdata.reg_addr_temp_alarm_ctrl, reg, rc);

	return rc;
}

static inline int pm8xxx_tm_write_pwm(struct pm8xxx_tm_chip *chip, u8 reg)
{
	int rc;

	rc = pm8xxx_writeb(chip->dev->parent,
			   chip->cdata.reg_addr_temp_alarm_pwm, reg);
	if (rc)
		pr_err("%s: pm8xxx_writeb(0x%03X)=0x%02X failed, rc=%d\n",
			chip->cdata.tm_name,
			chip->cdata.reg_addr_temp_alarm_pwm, reg, rc);

	return rc;
}

static inline int
pm8xxx_tm_shutdown_override(struct pm8xxx_tm_chip *chip,
			    enum pmic_thermal_override_mode mode)
{
	int rc;
	u8 reg;

	rc = pm8xxx_tm_read_ctrl(chip, &reg);
	if (rc < 0)
		return rc;

	reg &= ~(TEMP_ALARM_CTRL_OVRD_MASK | TEMP_ALARM_CTRL_STATUS_MASK);
	if (mode == SOFTWARE_OVERRIDE_ENABLED)
		reg |= (TEMP_ALARM_CTRL_OVRD_ST3 | TEMP_ALARM_CTRL_OVRD_ST2) &
			TEMP_ALARM_CTRL_OVRD_MASK;

	rc = pm8xxx_tm_write_ctrl(chip, reg);

	return rc;
}

/*
 * This function initializes the internal temperature value based on only the
 * current thermal stage and threshold.
 */
static int pm8xxx_tm_init_temp_no_adc(struct pm8xxx_tm_chip *chip)
{
	int rc;
	u8 reg;

	rc = pm8xxx_tm_read_ctrl(chip, &reg);
	if (rc < 0)
		return rc;

	chip->stage = (reg & TEMP_ALARM_CTRL_STATUS_MASK)
			>> TEMP_ALARM_CTRL_STATUS_SHIFT;
	chip->thresh = (reg & TEMP_ALARM_CTRL_THRESH_MASK)
			>> TEMP_ALARM_CTRL_THRESH_SHIFT;

	if (chip->stage)
		chip->temp = chip->thresh * TEMP_THRESH_MIN +
			   (chip->stage - 1) * TEMP_STAGE_STEP +
			   TEMP_THRESH_MIN;
	else
		chip->temp = chip->cdata.default_no_adc_temp;

	return 0;
}

/*
 * This function updates the internal temperature value based on the
 * current thermal stage and threshold as well as the previous stage
 */
static int pm8xxx_tm_update_temp_no_adc(struct pm8xxx_tm_chip *chip)
{
	unsigned int stage;
	int rc;
	u8 reg;

	rc = pm8xxx_tm_read_ctrl(chip, &reg);
	if (rc < 0)
		return rc;

	stage = (reg & TEMP_ALARM_CTRL_STATUS_MASK)
		>> TEMP_ALARM_CTRL_STATUS_SHIFT;
	chip->thresh = (reg & TEMP_ALARM_CTRL_THRESH_MASK)
			>> TEMP_ALARM_CTRL_THRESH_SHIFT;

	if (stage > chip->stage) {
		/* increasing stage, use lower bound */
		chip->temp = (stage - 1) * TEMP_STAGE_STEP
				+ chip->thresh * TEMP_THRESH_STEP
				+ TEMP_STAGE_HYSTERESIS + TEMP_THRESH_MIN;
	} else if (stage < chip->stage) {
		/* decreasing stage, use upper bound */
		chip->temp = stage * TEMP_STAGE_STEP
				+ chip->thresh * TEMP_THRESH_STEP
				- TEMP_STAGE_HYSTERESIS + TEMP_THRESH_MIN;
	}

	chip->stage = stage;

	return 0;
}

static int pm8xxx_tz_get_temp_no_adc(struct thermal_zone_device *thermal,
				     unsigned long *temp)
{
	struct pm8xxx_tm_chip *chip = thermal->devdata;
	int rc;

	if (!chip || !temp)
		return -EINVAL;

	rc = pm8xxx_tm_update_temp_no_adc(chip);
	if (rc < 0)
		return rc;

	*temp = chip->temp;

	return 0;
}

static int pm8xxx_tz_get_temp_pm8921_adc(struct thermal_zone_device *thermal,
				      unsigned long *temp)
{
	struct pm8xxx_tm_chip *chip = thermal->devdata;
	struct pm8921_adc_chan_result result = {
		.physical = 0lu,
	};
	int rc;

	if (!chip || !temp)
		return -EINVAL;

	*temp = chip->temp;

	rc = pm8921_adc_read(chip->cdata.adc_channel, &result);
	if (rc < 0) {
		pr_err("%s: adc_channel_read_result() failed, rc = %d\n",
			chip->cdata.tm_name, rc);
		return rc;
	}

	*temp = result.physical;
	chip->temp = result.physical;

	return 0;
}

static int pm8xxx_tz_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct pm8xxx_tm_chip *chip = thermal->devdata;

	if (!chip || !mode)
		return -EINVAL;

	*mode = chip->mode;

	return 0;
}

static int pm8xxx_tz_set_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode mode)
{
	struct pm8xxx_tm_chip *chip = thermal->devdata;

	if (!chip)
		return -EINVAL;

	if (mode != chip->mode) {
		if (mode == THERMAL_DEVICE_ENABLED)
			pm8xxx_tm_shutdown_override(chip,
						    SOFTWARE_OVERRIDE_ENABLED);
		else
			pm8xxx_tm_shutdown_override(chip,
						    SOFTWARE_OVERRIDE_DISABLED);
	}
	chip->mode = mode;

	return 0;
}

static int pm8xxx_tz_get_trip_type(struct thermal_zone_device *thermal,
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

static int pm8xxx_tz_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, unsigned long *temp)
{
	struct pm8xxx_tm_chip *chip = thermal->devdata;
	int thresh_temp;

	if (!chip || trip < 0 || !temp)
		return -EINVAL;

	thresh_temp = chip->thresh * TEMP_THRESH_STEP +
			TEMP_THRESH_MIN;

	switch (trip) {
	case TRIP_STAGE3:
		thresh_temp += 2 * TEMP_STAGE_STEP;
		break;
	case TRIP_STAGE2:
		thresh_temp += TEMP_STAGE_STEP;
		break;
	case TRIP_STAGE1:
		break;
	default:
		return -EINVAL;
	}

	*temp = thresh_temp;

	return 0;
}

static int pm8xxx_tz_get_crit_temp(struct thermal_zone_device *thermal,
				   unsigned long *temp)
{
	struct pm8xxx_tm_chip *chip = thermal->devdata;

	if (!chip || !temp)
		return -EINVAL;

	*temp = chip->thresh * TEMP_THRESH_STEP + TEMP_THRESH_MIN +
		2 * TEMP_STAGE_STEP;

	return 0;
}

static struct thermal_zone_device_ops pm8xxx_thermal_zone_ops_no_adc = {
	.get_temp = pm8xxx_tz_get_temp_no_adc,
	.get_mode = pm8xxx_tz_get_mode,
	.set_mode = pm8xxx_tz_set_mode,
	.get_trip_type = pm8xxx_tz_get_trip_type,
	.get_trip_temp = pm8xxx_tz_get_trip_temp,
	.get_crit_temp = pm8xxx_tz_get_crit_temp,
};

static struct thermal_zone_device_ops pm8xxx_thermal_zone_ops_pm8921_adc = {
	.get_temp = pm8xxx_tz_get_temp_pm8921_adc,
	.get_mode = pm8xxx_tz_get_mode,
	.set_mode = pm8xxx_tz_set_mode,
	.get_trip_type = pm8xxx_tz_get_trip_type,
	.get_trip_temp = pm8xxx_tz_get_trip_temp,
	.get_crit_temp = pm8xxx_tz_get_crit_temp,
};

static void pm8xxx_tm_work(struct work_struct *work)
{
	struct pm8xxx_tm_chip *chip
		= container_of(work, struct pm8xxx_tm_chip, irq_work);
	int rc;
	u8 reg;

	rc = pm8xxx_tm_read_ctrl(chip, &reg);
	if (rc < 0)
		goto bail;

	if (chip->cdata.adc_type == PM8XXX_TM_ADC_NONE) {
		rc = pm8xxx_tm_update_temp_no_adc(chip);
		if (rc < 0)
			goto bail;
		pr_info("%s: Temp Alarm - stage=%u, threshold=%u, "
			"temp=%lu mC\n", chip->cdata.tm_name, chip->stage,
			chip->thresh, chip->temp);
	} else {
		chip->stage = (reg & TEMP_ALARM_CTRL_STATUS_MASK)
				>> TEMP_ALARM_CTRL_STATUS_SHIFT;
		chip->thresh = (reg & TEMP_ALARM_CTRL_THRESH_MASK)
				>> TEMP_ALARM_CTRL_THRESH_SHIFT;
		pr_info("%s: Temp Alarm - stage=%u, threshold=%u\n",
			chip->cdata.tm_name, chip->stage, chip->thresh);
	}

	/* Clear status bits. */
	if (reg & (TEMP_ALARM_CTRL_ST2_SD | TEMP_ALARM_CTRL_ST3_SD)) {
		reg &= ~(TEMP_ALARM_CTRL_ST2_SD | TEMP_ALARM_CTRL_ST3_SD
			 | TEMP_ALARM_CTRL_STATUS_MASK);

		pm8xxx_tm_write_ctrl(chip, reg);
	}

	thermal_zone_device_update(chip->tz_dev);

	/* Notify user space */
	if (chip->mode == THERMAL_DEVICE_ENABLED)
		kobject_uevent(&chip->tz_dev->device.kobj, KOBJ_CHANGE);

bail:
	enable_irq(chip->tempstat_irq);
	enable_irq(chip->overtemp_irq);
}

static irqreturn_t pm8xxx_tm_isr(int irq, void *data)
{
	struct pm8xxx_tm_chip *chip = data;

	disable_irq_nosync(chip->tempstat_irq);
	disable_irq_nosync(chip->overtemp_irq);
	schedule_work(&chip->irq_work);

	return IRQ_HANDLED;
}

static int pm8xxx_tm_init_reg(struct pm8xxx_tm_chip *chip)
{
	int rc;
	u8 reg;

	rc = pm8xxx_tm_read_ctrl(chip, &reg);
	if (rc < 0)
		return rc;

	chip->stage = (reg & TEMP_ALARM_CTRL_STATUS_MASK)
			>> TEMP_ALARM_CTRL_STATUS_SHIFT;
	chip->temp = 0;

	/* Use temperature threshold set 0: (105, 125, 145) */
	chip->thresh = 0;
	reg = (chip->thresh << TEMP_ALARM_CTRL_THRESH_SHIFT)
		& TEMP_ALARM_CTRL_THRESH_MASK;
	rc = pm8xxx_tm_write_ctrl(chip, reg);
	if (rc < 0)
		return rc;

	/*
	 * Set the PMIC alarm module PWM to have a frequency of 8 Hz. This
	 * helps cut down on the number of unnecessary interrupts fired when
	 * changing between thermal stages.  Also, Enable the over temperature
	 * PWM whenever the PMIC is enabled.
	 */
	reg =  (1 << TEMP_ALARM_PWM_EN_SHIFT)
		| (3 << TEMP_ALARM_PWM_PER_PRE_SHIFT)
		| (3 << TEMP_ALARM_PWM_PER_DIV_SHIFT);

	rc = pm8xxx_tm_write_pwm(chip, reg);

	return rc;
}

static int __devinit pm8xxx_tm_probe(struct platform_device *pdev)
{
	const struct pm8xxx_tm_core_data *cdata = pdev->dev.platform_data;
	struct thermal_zone_device_ops *tz_ops;
	struct pm8xxx_tm_chip *chip;
	struct resource *res;
	int rc = 0;

	if (!cdata) {
		pr_err("missing core data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8xxx_tm_chip), GFP_KERNEL);
	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	memcpy(&(chip->cdata), cdata, sizeof(struct pm8xxx_tm_core_data));

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		chip->cdata.irq_name_temp_stat);
	if (res) {
		chip->tempstat_irq = res->start;
	} else {
		pr_err("temp stat IRQ not specified\n");
		goto err_free_chip;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		chip->cdata.irq_name_over_temp);
	if (res) {
		chip->overtemp_irq = res->start;
	} else {
		pr_err("over temp IRQ not specified\n");
		goto err_free_chip;
	}

	/* Select proper thermal zone ops functions based on ADC type. */
	if (chip->cdata.adc_type == PM8XXX_TM_ADC_PM8921_ADC)
		tz_ops = &pm8xxx_thermal_zone_ops_pm8921_adc;
	else
		tz_ops = &pm8xxx_thermal_zone_ops_no_adc;

	chip->tz_dev = thermal_zone_device_register(chip->cdata.tm_name,
			TRIP_NUM, chip, tz_ops, 0, 0, 0, 0);
	if (chip->tz_dev == NULL) {
		pr_err("thermal_zone_device_register() failed.\n");
		rc = -ENODEV;
		goto err_free_chip;
	}

	rc = pm8xxx_tm_init_reg(chip);
	if (rc < 0)
		goto err_free_tz;
	rc = pm8xxx_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);
	if (rc < 0)
		goto err_free_tz;

	if (chip->cdata.adc_type == PM8XXX_TM_ADC_NONE) {
		rc = pm8xxx_tm_init_temp_no_adc(chip);
		if (rc < 0)
			goto err_free_tz;
	}

	/* Start in HW control; switch to SW control when user changes mode. */
	chip->mode = THERMAL_DEVICE_DISABLED;
	thermal_zone_device_update(chip->tz_dev);

	INIT_WORK(&chip->irq_work, pm8xxx_tm_work);

	rc = request_irq(chip->tempstat_irq, pm8xxx_tm_isr, IRQF_TRIGGER_RISING,
		chip->cdata.irq_name_temp_stat, chip);
	if (rc < 0) {
		pr_err("request_irq(%d) failed: %d\n", chip->tempstat_irq, rc);
		goto err_cancel_work;
	}

	rc = request_irq(chip->overtemp_irq, pm8xxx_tm_isr, IRQF_TRIGGER_RISING,
		chip->cdata.irq_name_over_temp, chip);
	if (rc < 0) {
		pr_err("request_irq(%d) failed: %d\n", chip->overtemp_irq, rc);
		goto err_free_irq_tempstat;
	}

	platform_set_drvdata(pdev, chip);

	pr_info("OK\n");

	return 0;

err_free_irq_tempstat:
	free_irq(chip->tempstat_irq, chip);
err_cancel_work:
	cancel_work_sync(&chip->irq_work);
err_free_tz:
	thermal_zone_device_unregister(chip->tz_dev);
err_free_chip:
	kfree(chip);
	return rc;
}

static int __devexit pm8xxx_tm_remove(struct platform_device *pdev)
{
	struct pm8xxx_tm_chip *chip = platform_get_drvdata(pdev);

	if (chip) {
		platform_set_drvdata(pdev, NULL);
		cancel_work_sync(&chip->irq_work);
		free_irq(chip->overtemp_irq, chip);
		free_irq(chip->tempstat_irq, chip);
		pm8xxx_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);
		thermal_zone_device_unregister(chip->tz_dev);
		kfree(chip);
	}
	return 0;
}

#ifdef CONFIG_PM
static int pm8xxx_tm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pm8xxx_tm_chip *chip = platform_get_drvdata(pdev);

	/* Clear override bits in suspend to allow hardware control */
	pm8xxx_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_DISABLED);

	return 0;
}

static int pm8xxx_tm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pm8xxx_tm_chip *chip = platform_get_drvdata(pdev);

	/* Override hardware actions so software can control */
	if (chip->mode == THERMAL_DEVICE_ENABLED)
		pm8xxx_tm_shutdown_override(chip, SOFTWARE_OVERRIDE_ENABLED);

	return 0;
}

static const struct dev_pm_ops pm8xxx_tm_pm_ops = {
	.suspend = pm8xxx_tm_suspend,
	.resume = pm8xxx_tm_resume,
};

#define PM8XXX_TM_PM_OPS	(&pm8xxx_tm_pm_ops)
#else
#define PM8XXX_TM_PM_OPS	NULL
#endif

static struct platform_driver pm8xxx_tm_driver = {
	.probe	= pm8xxx_tm_probe,
	.remove	= __devexit_p(pm8xxx_tm_remove),
	.driver	= {
		.name = PM8XXX_TM_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = PM8XXX_TM_PM_OPS,
	},
};

static int __init pm8xxx_tm_init(void)
{
	return platform_driver_register(&pm8xxx_tm_driver);
}

static void __exit pm8xxx_tm_exit(void)
{
	platform_driver_unregister(&pm8xxx_tm_driver);
}

module_init(pm8xxx_tm_init);
module_exit(pm8xxx_tm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PM8xxx Thermal Manager driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_TM_DEV_NAME);
