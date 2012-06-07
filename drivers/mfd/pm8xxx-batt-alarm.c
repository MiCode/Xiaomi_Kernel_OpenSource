/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 * Qualcomm PMIC PM8xxx Battery Alarm driver
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/batt-alarm.h>

/* Available voltage threshold values */
#define THRESHOLD_MIN_MV		2500
#define THRESHOLD_MAX_MV		5675
#define THRESHOLD_STEP_MV		25

/* Register bit definitions */

/* Threshold register */
#define THRESHOLD_UPPER_MASK		0xF0
#define THRESHOLD_LOWER_MASK		0x0F
#define THRESHOLD_UPPER_SHIFT		4
#define THRESHOLD_LOWER_SHIFT		0

/* CTRL 1 register */
#define CTRL1_BATT_ALARM_ENABLE_MASK	0x80
#define CTRL1_BATT_ALARM_ENABLE		0x80
#define CTRL1_BATT_ALARM_DISABLE	0x00
#define CTRL1_HOLD_TIME_MASK		0x70
#define CTRL1_STATUS_UPPER_MASK		0x02
#define CTRL1_STATUS_LOWER_MASK		0x01
#define CTRL1_HOLD_TIME_SHIFT		4
#define CTRL1_HOLD_TIME_MIN		0
#define CTRL1_HOLD_TIME_MAX		7

/* CTRL 2 register */
#define CTRL2_COMP_UPPER_DISABLE_MASK	0x80
#define CTRL2_COMP_UPPER_ENABLE		0x00
#define CTRL2_COMP_UPPER_DISABLE	0x80
#define CTRL2_COMP_LOWER_DISABLE_MASK	0x40
#define CTRL2_COMP_LOWER_ENABLE		0x00
#define CTRL2_COMP_LOWER_DISABLE	0x40
#define CTRL2_FINE_STEP_UPPER_MASK	0x30
#define CTRL2_RANGE_EXT_UPPER_MASK	0x08
#define CTRL2_FINE_STEP_LOWER_MASK	0x06
#define CTRL2_RANGE_EXT_LOWER_MASK	0x01
#define CTRL2_FINE_STEP_UPPER_SHIFT	4
#define CTRL2_FINE_STEP_LOWER_SHIFT	1

/* PWM control register */
#define PWM_CTRL_ALARM_EN_MASK		0xC0
#define PWM_CTRL_ALARM_EN_NEVER		0x00
#define PWM_CTRL_ALARM_EN_TCXO		0x40
#define PWM_CTRL_ALARM_EN_PWM		0x80
#define PWM_CTRL_ALARM_EN_ALWAYS	0xC0
#define PWM_CTRL_PRE_MASK		0x38
#define PWM_CTRL_DIV_MASK		0x07
#define PWM_CTRL_PRE_SHIFT		3
#define PWM_CTRL_DIV_SHIFT		0
#define PWM_CTRL_PRE_MIN		0
#define PWM_CTRL_PRE_MAX		7
#define PWM_CTRL_DIV_MIN		1
#define PWM_CTRL_DIV_MAX		7

/* PWM control input range */
#define PWM_CTRL_PRE_INPUT_MIN		2
#define PWM_CTRL_PRE_INPUT_MAX		9
#define PWM_CTRL_DIV_INPUT_MIN		2
#define PWM_CTRL_DIV_INPUT_MAX		8

/* Available voltage threshold values */
#define THRESHOLD_BASIC_MIN_MV		2800
#define THRESHOLD_EXT_MIN_MV		4400

/*
 * Default values used during initialization:
 * Slowest PWM rate to ensure minimal status jittering when crossing thresholds.
 * Largest hold time also helps reduce status value jittering.  Comparators
 * are disabled by default and must be turned on by calling
 * pm8xxx_batt_alarm_state_set.
 */
#define DEFAULT_THRESHOLD_LOWER		3200
#define DEFAULT_THRESHOLD_UPPER		4300
#define DEFAULT_HOLD_TIME		PM8XXX_BATT_ALARM_HOLD_TIME_16_MS
#define DEFAULT_USE_PWM			1
#define DEFAULT_PWM_SCALER		9
#define DEFAULT_PWM_DIVIDER		8
#define DEFAULT_LOWER_ENABLE		0
#define DEFAULT_UPPER_ENABLE		0

struct pm8xxx_batt_alarm_chip {
	struct pm8xxx_batt_alarm_core_data	cdata;
	struct srcu_notifier_head		irq_notifier_list;
	struct work_struct			irq_work;
	struct device				*dev;
	struct mutex				lock;
	unsigned int				irq;
	int					notifier_count;
	u8					reg_threshold;
	u8					reg_ctrl1;
	u8					reg_ctrl2;
	u8					reg_pwm_ctrl;
};
static struct pm8xxx_batt_alarm_chip *the_battalarm;

static int pm8xxx_reg_write(struct pm8xxx_batt_alarm_chip *chip, u16 addr,
				u8 val, u8 mask, u8 *reg_save)
{
	int rc = 0;
	u8 reg;

	reg = (*reg_save & ~mask) | (val & mask);
	if (reg != *reg_save)
		rc = pm8xxx_writeb(chip->dev->parent, addr, reg);
	if (rc)
		pr_err("pm8xxx_writeb failed; addr=%03X, rc=%d\n", addr, rc);
	else
		*reg_save = reg;
	return rc;
}

/**
 * pm8xxx_batt_alarm_enable - enable one of the battery voltage threshold
 *			      comparators
 * @comparator:	selects which comparator to enable
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_enable(enum pm8xxx_batt_alarm_comparator comparator)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int rc;
	u8 val_ctrl2 = 0, mask_ctrl2 = 0;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENODEV;
	}

	if (comparator < 0 || comparator > PM8XXX_BATT_ALARM_UPPER_COMPARATOR) {
		pr_err("invalid comparator ID number: %d\n", comparator);
		return -EINVAL;
	}

	if (comparator == PM8XXX_BATT_ALARM_LOWER_COMPARATOR) {
		val_ctrl2 = CTRL2_COMP_LOWER_ENABLE;
		mask_ctrl2 = CTRL2_COMP_LOWER_DISABLE_MASK;
	} else {
		val_ctrl2 = CTRL2_COMP_UPPER_ENABLE;
		mask_ctrl2 = CTRL2_COMP_UPPER_DISABLE_MASK;
	}

	mutex_lock(&chip->lock);

	/* Enable the battery alarm block. */
	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_ctrl1,
				CTRL1_BATT_ALARM_ENABLE,
				CTRL1_BATT_ALARM_ENABLE_MASK, &chip->reg_ctrl1);
	if (rc)
		goto bail;

	/* Enable the individual comparators. */
	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_ctrl2, val_ctrl2,
				mask_ctrl2, &chip->reg_ctrl2);

bail:
	mutex_unlock(&chip->lock);
	return rc;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_enable);

/**
 * pm8xxx_batt_alarm_disable - disable one of the battery voltage threshold
 *			       comparators
 * @comparator:	selects which comparator to disable
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_disable(enum pm8xxx_batt_alarm_comparator comparator)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int rc;
	u8 val_ctrl1 = 0, val_ctrl2 = 0, mask_ctrl2 = 0;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENODEV;
	}

	if (comparator < 0 || comparator > PM8XXX_BATT_ALARM_UPPER_COMPARATOR) {
		pr_err("invalid comparator ID number: %d\n", comparator);
		return -EINVAL;
	}

	if (comparator == PM8XXX_BATT_ALARM_LOWER_COMPARATOR) {
		val_ctrl2 = CTRL2_COMP_LOWER_DISABLE;
		mask_ctrl2 = CTRL2_COMP_LOWER_DISABLE_MASK;
	} else {
		val_ctrl2 = CTRL2_COMP_UPPER_DISABLE;
		mask_ctrl2 = CTRL2_COMP_UPPER_DISABLE_MASK;
	}

	mutex_lock(&chip->lock);

	/* Disable the specified comparator. */
	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_ctrl2, val_ctrl2,
				mask_ctrl2, &chip->reg_ctrl2);
	if (rc)
		goto bail;

	/* Disable the battery alarm block if both comparators are disabled. */
	val_ctrl2 = chip->reg_ctrl2
	      & (CTRL2_COMP_LOWER_DISABLE_MASK | CTRL2_COMP_UPPER_DISABLE_MASK);
	if (val_ctrl2 == (CTRL2_COMP_LOWER_DISABLE | CTRL2_COMP_UPPER_DISABLE))
		val_ctrl1 = CTRL1_BATT_ALARM_DISABLE;
	else
		val_ctrl1 = CTRL1_BATT_ALARM_ENABLE;

	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_ctrl1, val_ctrl1,
				CTRL1_BATT_ALARM_ENABLE_MASK, &chip->reg_ctrl1);

bail:
	mutex_unlock(&chip->lock);
	return rc;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_disable);

/**
 * pm8xxx_batt_alarm_threshold_set - set the lower and upper alarm thresholds
 * @comparator:		selects which comparator to set the threshold of
 * @threshold_mV:	battery voltage threshold in millivolts
 *			set points = 2500-5675 mV in 25 mV steps
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_threshold_set(
	enum pm8xxx_batt_alarm_comparator comparator, int threshold_mV)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int step, fine_step, rc;
	u8 val_threshold = 0, val_ctrl2 = 0;
	int threshold_mask, threshold_shift, range_ext_mask, fine_step_mask;
	int fine_step_shift;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENXIO;
	}

	if (comparator < 0 || comparator > PM8XXX_BATT_ALARM_UPPER_COMPARATOR) {
		pr_err("invalid comparator ID number: %d\n", comparator);
		return -EINVAL;
	}

	if (threshold_mV < THRESHOLD_MIN_MV
	    || threshold_mV > THRESHOLD_MAX_MV) {
		pr_err("threshold value, %d mV, is outside of allowable "
			"range: [%d, %d] mV\n", threshold_mV,
			THRESHOLD_MIN_MV, THRESHOLD_MAX_MV);
		return -EINVAL;
	}

	if (comparator == PM8XXX_BATT_ALARM_LOWER_COMPARATOR) {
		threshold_mask = THRESHOLD_LOWER_MASK;
		threshold_shift = THRESHOLD_LOWER_SHIFT;
		range_ext_mask = CTRL2_RANGE_EXT_LOWER_MASK;
		fine_step_mask = CTRL2_FINE_STEP_LOWER_MASK;
		fine_step_shift = CTRL2_FINE_STEP_LOWER_SHIFT;
	} else {
		threshold_mask = THRESHOLD_UPPER_MASK;
		threshold_shift = THRESHOLD_UPPER_SHIFT;
		range_ext_mask = CTRL2_RANGE_EXT_UPPER_MASK;
		fine_step_mask = CTRL2_FINE_STEP_UPPER_MASK;
		fine_step_shift = CTRL2_FINE_STEP_UPPER_SHIFT;
	}

	/* Determine register settings to achieve the threshold. */
	if (threshold_mV < THRESHOLD_BASIC_MIN_MV) {
		/* Extended low range */
		val_ctrl2 |= range_ext_mask;

		step = (threshold_mV - THRESHOLD_MIN_MV) / THRESHOLD_STEP_MV;

		fine_step = step & 0x3;
		/* Extended low range is for steps 0 to 2 */
		step >>= 2;
	} else if (threshold_mV >= THRESHOLD_EXT_MIN_MV) {
		/* Extended high range */
		val_ctrl2 |= range_ext_mask;

		step = (threshold_mV - THRESHOLD_EXT_MIN_MV)
			/ THRESHOLD_STEP_MV;

		fine_step = step & 0x3;
		/* Extended high range is for steps 3 to 15 */
		step = (step >> 2) + 3;
	} else {
		/* Basic range */
		step = (threshold_mV - THRESHOLD_BASIC_MIN_MV)
			/ THRESHOLD_STEP_MV;

		fine_step = step & 0x3;
		step >>= 2;
	}
	val_threshold |= step << threshold_shift;
	val_ctrl2 |= (fine_step << fine_step_shift) & fine_step_mask;

	mutex_lock(&chip->lock);
	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_threshold,
			val_threshold, threshold_mask, &chip->reg_threshold);
	if (rc)
		goto bail;

	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_ctrl2, val_ctrl2,
			range_ext_mask | fine_step_mask, &chip->reg_ctrl2);

bail:
	mutex_unlock(&chip->lock);
	return rc;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_threshold_set);

/**
 * pm8xxx_batt_alarm_status_read - get status of both threshold comparators
 *
 * RETURNS:	< 0	   = error
 *		  0	   = battery voltage ok
 *		BIT(0) set = battery voltage below lower threshold
 *		BIT(1) set = battery voltage above upper threshold
 */
int pm8xxx_batt_alarm_status_read(void)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int status, rc;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENXIO;
	}

	mutex_lock(&chip->lock);
	rc = pm8xxx_readb(chip->dev->parent, chip->cdata.reg_addr_ctrl1,
			  &chip->reg_ctrl1);

	status = ((chip->reg_ctrl1 & CTRL1_STATUS_LOWER_MASK)
			? PM8XXX_BATT_ALARM_STATUS_BELOW_LOWER : 0)
		| ((chip->reg_ctrl1 & CTRL1_STATUS_UPPER_MASK)
			? PM8XXX_BATT_ALARM_STATUS_ABOVE_UPPER : 0);
	mutex_unlock(&chip->lock);

	if (rc) {
		pr_err("pm8xxx_readb failed, rc=%d\n", rc);
		return rc;
	}

	return status;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_status_read);

/**
 * pm8xxx_batt_alarm_hold_time_set - set hold time of interrupt output *
 * @hold_time:	amount of time that battery voltage must remain outside of the
 *		threshold range before the battery alarm interrupt triggers
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_hold_time_set(enum pm8xxx_batt_alarm_hold_time hold_time)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int rc;
	u8 reg_ctrl1 = 0;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENXIO;
	}

	if (hold_time < CTRL1_HOLD_TIME_MIN
	    || hold_time > CTRL1_HOLD_TIME_MAX) {

		pr_err("hold time, %d, is outside of allowable range: "
			"[%d, %d]\n", hold_time, CTRL1_HOLD_TIME_MIN,
			CTRL1_HOLD_TIME_MAX);
		return -EINVAL;
	}

	reg_ctrl1 = hold_time << CTRL1_HOLD_TIME_SHIFT;

	mutex_lock(&chip->lock);
	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_ctrl1, reg_ctrl1,
			      CTRL1_HOLD_TIME_MASK, &chip->reg_ctrl1);
	mutex_unlock(&chip->lock);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_hold_time_set);

/**
 * pm8xxx_batt_alarm_pwm_rate_set - set battery alarm update rate *
 * @use_pwm:		1 = use PWM update rate, 0 = comparators always active
 * @clock_scaler:	PWM clock scaler = 2 to 9
 * @clock_divider:	PWM clock divider = 2 to 8
 *
 * This function sets the rate at which the battery alarm module enables
 * the threshold comparators.  The rate is determined by the following equation:
 *
 * f_update = (1024 Hz) / (clock_divider * (2 ^ clock_scaler))
 *
 * Thus, the update rate can range from 0.25 Hz to 128 Hz.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_pwm_rate_set(int use_pwm, int clock_scaler,
				   int clock_divider)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int rc;
	u8 reg_pwm_ctrl = 0, mask = 0;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENXIO;
	}

	if (use_pwm && (clock_scaler < PWM_CTRL_PRE_INPUT_MIN
	    || clock_scaler > PWM_CTRL_PRE_INPUT_MAX)) {
		pr_err("PWM clock scaler, %d, is outside of allowable range: "
			"[%d, %d]\n", clock_scaler, PWM_CTRL_PRE_INPUT_MIN,
			PWM_CTRL_PRE_INPUT_MAX);
		return -EINVAL;
	}

	if (use_pwm && (clock_divider < PWM_CTRL_DIV_INPUT_MIN
	    || clock_divider > PWM_CTRL_DIV_INPUT_MAX)) {
		pr_err("PWM clock divider, %d, is outside of allowable range: "
			"[%d, %d]\n", clock_divider, PWM_CTRL_DIV_INPUT_MIN,
			PWM_CTRL_DIV_INPUT_MAX);
		return -EINVAL;
	}

	if (!use_pwm) {
		/* Turn off PWM control and always enable. */
		reg_pwm_ctrl = PWM_CTRL_ALARM_EN_ALWAYS;
		mask = PWM_CTRL_ALARM_EN_MASK;
	} else {
		/* Use PWM control. */
		reg_pwm_ctrl = PWM_CTRL_ALARM_EN_PWM;
		mask = PWM_CTRL_ALARM_EN_MASK | PWM_CTRL_PRE_MASK
			| PWM_CTRL_DIV_MASK;

		clock_scaler -= PWM_CTRL_PRE_INPUT_MIN - PWM_CTRL_PRE_MIN;
		clock_divider -= PWM_CTRL_DIV_INPUT_MIN - PWM_CTRL_DIV_MIN;

		reg_pwm_ctrl |= (clock_scaler << PWM_CTRL_PRE_SHIFT)
				& PWM_CTRL_PRE_MASK;
		reg_pwm_ctrl |= (clock_divider << PWM_CTRL_DIV_SHIFT)
				& PWM_CTRL_DIV_MASK;
	}

	mutex_lock(&chip->lock);
	rc = pm8xxx_reg_write(chip, chip->cdata.reg_addr_pwm_ctrl, reg_pwm_ctrl,
			      mask, &chip->reg_pwm_ctrl);
	mutex_unlock(&chip->lock);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_pwm_rate_set);

/*
 * Handle the BATT_ALARM interrupt:
 * Battery voltage is above or below threshold range.
 */
static irqreturn_t pm8xxx_batt_alarm_isr(int irq, void *data)
{
	struct pm8xxx_batt_alarm_chip *chip = data;

	disable_irq_nosync(chip->irq);
	schedule_work(&chip->irq_work);

	return IRQ_HANDLED;
}

static void pm8xxx_batt_alarm_isr_work(struct work_struct *work)
{
	struct pm8xxx_batt_alarm_chip *chip
		= container_of(work, struct pm8xxx_batt_alarm_chip, irq_work);
	int status;

	if (!chip)
		return;

	status = pm8xxx_batt_alarm_status_read();

	if (status < 0)
		pr_err("failed to read status, rc=%d\n", status);
	else
		srcu_notifier_call_chain(&chip->irq_notifier_list,
						status, NULL);

	enable_irq(chip->irq);
}

/**
 * pm8xxx_batt_alarm_register_notifier - register a notifier to run when a
 *	battery voltage change interrupt fires
 * @nb:	notifier block containing callback function to register
 *
 * nb->notifier_call must point to a function of this form -
 * int (*notifier_call)(struct notifier_block *nb, unsigned long status,
 *			void *unused);
 * "status" will receive the battery alarm status; "unused" will be NULL.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_register_notifier(struct notifier_block *nb)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int rc;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENXIO;
	}

	rc = srcu_notifier_chain_register(&chip->irq_notifier_list, nb);
	mutex_lock(&chip->lock);
	if (rc == 0) {
		if (chip->notifier_count == 0) {
			enable_irq(chip->irq);
			rc = irq_set_irq_wake(chip->irq, 1);
		}

		chip->notifier_count++;
	}

	mutex_unlock(&chip->lock);
	return rc;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_register_notifier);

/**
 * pm8xxx_batt_alarm_unregister_notifier - unregister a notifier that is run
 *	when a battery voltage change interrupt fires
 * @nb:	notifier block containing callback function to unregister
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_unregister_notifier(struct notifier_block *nb)
{
	struct pm8xxx_batt_alarm_chip *chip = the_battalarm;
	int rc;

	if (!chip) {
		pr_err("no battery alarm device found.\n");
		return -ENXIO;
	}

	rc = srcu_notifier_chain_unregister(&chip->irq_notifier_list, nb);
	if (rc == 0) {
		mutex_lock(&chip->lock);

		chip->notifier_count--;

		if (chip->notifier_count == 0) {
			rc = irq_set_irq_wake(chip->irq, 0);
			disable_irq(chip->irq);
		}

		WARN_ON(chip->notifier_count < 0);

		mutex_unlock(&chip->lock);
	}

	return rc;
}
EXPORT_SYMBOL(pm8xxx_batt_alarm_unregister_notifier);

static int pm8xxx_batt_alarm_reg_init(struct pm8xxx_batt_alarm_chip *chip)
{
	int rc = 0;

	/* save the current register states */
	rc = pm8xxx_readb(chip->dev->parent, chip->cdata.reg_addr_threshold,
			  &chip->reg_threshold);
	if (rc)
		goto bail;

	rc = pm8xxx_readb(chip->dev->parent, chip->cdata.reg_addr_ctrl1,
			  &chip->reg_ctrl1);
	if (rc)
		goto bail;

	rc = pm8xxx_readb(chip->dev->parent, chip->cdata.reg_addr_ctrl2,
			  &chip->reg_ctrl2);
	if (rc)
		goto bail;

	rc = pm8xxx_readb(chip->dev->parent, chip->cdata.reg_addr_pwm_ctrl,
			  &chip->reg_pwm_ctrl);
	if (rc)
		goto bail;

bail:
	if (rc)
		pr_err("pm8xxx_readb failed; initial register states "
			"unknown, rc=%d\n", rc);
	return rc;
}

/* TODO: should this default setting function be removed? */
static int pm8xxx_batt_alarm_config_defaults(void)
{
	int rc = 0;

	/* Use default values when no platform data is provided. */
	rc = pm8xxx_batt_alarm_threshold_set(PM8XXX_BATT_ALARM_LOWER_COMPARATOR,
		DEFAULT_THRESHOLD_LOWER);
	if (rc) {
		pr_err("threshold_set failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_threshold_set(PM8XXX_BATT_ALARM_UPPER_COMPARATOR,
		DEFAULT_THRESHOLD_UPPER);
	if (rc) {
		pr_err("threshold_set failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_hold_time_set(DEFAULT_HOLD_TIME);
	if (rc) {
		pr_err("hold_time_set failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_pwm_rate_set(DEFAULT_USE_PWM,
			DEFAULT_PWM_SCALER, DEFAULT_PWM_DIVIDER);
	if (rc) {
		pr_err("pwm_rate_set failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (rc) {
		pr_err("disable lower failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (rc) {
		pr_err("disable upper failed, rc=%d\n", rc);
		goto done;
	}

done:
	return rc;
}

static int __devinit pm8xxx_batt_alarm_probe(struct platform_device *pdev)
{
	const struct pm8xxx_batt_alarm_core_data *cdata
			= pdev->dev.platform_data;
	struct pm8xxx_batt_alarm_chip *chip;
	struct resource *res;
	int rc;

	if (the_battalarm) {
		pr_err("A PMIC battery alarm device has already probed.\n");
		return -ENODEV;
	}

	if (!cdata) {
		pr_err("missing core data\n");
		return -EINVAL;
	}

	if (!cdata->irq_name) {
		pr_err("missing IRQ name\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8xxx_batt_alarm_chip), GFP_KERNEL);
	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		cdata->irq_name);
	if (res) {
		chip->irq = res->start;
	} else {
		pr_err("Battery alarm IRQ not specified\n");
		rc = -EINVAL;
		goto err_free_chip;
	}

	chip->dev = &pdev->dev;
	memcpy(&(chip->cdata), cdata,
		sizeof(struct pm8xxx_batt_alarm_core_data));

	srcu_init_notifier_head(&chip->irq_notifier_list);

	chip->notifier_count = 0;
	mutex_init(&chip->lock);

	the_battalarm = chip;

	rc = pm8xxx_batt_alarm_reg_init(chip);
	if (rc)
		goto err_free_mutex;

	rc = pm8xxx_batt_alarm_config_defaults();
	if (rc)
		goto err_free_mutex;

	INIT_WORK(&chip->irq_work, pm8xxx_batt_alarm_isr_work);

/* TODO: Is it best to trigger on both edges? Should this be configurable? */
	rc = request_irq(chip->irq, pm8xxx_batt_alarm_isr,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, cdata->irq_name,
		chip);
	if (rc < 0) {
		pr_err("request_irq(%d) failed, rc=%d\n", chip->irq, rc);
		goto err_cancel_work;
	}

	/* Disable the IRQ until a notifier is registered. */
	disable_irq(chip->irq);

	platform_set_drvdata(pdev, chip);

	return 0;

err_cancel_work:
	cancel_work_sync(&chip->irq_work);
err_free_mutex:
	mutex_destroy(&chip->lock);
	srcu_cleanup_notifier_head(&chip->irq_notifier_list);
err_free_chip:
	kfree(chip);
	the_battalarm = NULL;

	return rc;
}

static int __devexit pm8xxx_batt_alarm_remove(struct platform_device *pdev)
{
	struct pm8xxx_batt_alarm_chip *chip = platform_get_drvdata(pdev);

	if (chip) {
		platform_set_drvdata(pdev, NULL);
		irq_set_irq_wake(chip->irq, 0);
		free_irq(chip->irq, chip);
		cancel_work_sync(&chip->irq_work);
		srcu_cleanup_notifier_head(&chip->irq_notifier_list);
		mutex_destroy(&chip->lock);
		kfree(chip);
		the_battalarm = NULL;
	}

	return 0;
}

static struct platform_driver pm8xxx_batt_alarm_driver = {
	.probe	= pm8xxx_batt_alarm_probe,
	.remove	= __devexit_p(pm8xxx_batt_alarm_remove),
	.driver	= {
		.name = PM8XXX_BATT_ALARM_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init pm8xxx_batt_alarm_init(void)
{
	return platform_driver_register(&pm8xxx_batt_alarm_driver);
}

static void __exit pm8xxx_batt_alarm_exit(void)
{
	platform_driver_unregister(&pm8xxx_batt_alarm_driver);
}

module_init(pm8xxx_batt_alarm_init);
module_exit(pm8xxx_batt_alarm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC PM8xxx Battery Alarm");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_BATT_ALARM_DEV_NAME);
