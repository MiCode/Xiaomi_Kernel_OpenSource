/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/spmi.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/hwmon-sysfs.h>

/* QPNP BUCK PS register definition */
#define QPNP_SMPS_REG_TYPE		0x04

#define QPNP_SMPS_REG_HCINT_EN		0x80
#define HF_HCINT_EN_MASK		BIT(7)
#define HF_EN_SHIFT			0x07

#define QPNP_SMPS_REG_HCINT_CTRL	0x81
#define HF_ICRIT_MASK			0x0C
#define HF_IWARN_MASK			0x03
#define HF_ICRIT_SHIFT			2

#define QPNP_SMPS_REG_RT_STS		0x10
#define HF_ICRIT_RT_MASK		BIT(1)
#define HF_IWARN_RT_MASK		BIT(0)

#define STEP_SIZE			10
#define EN_CURRENT_MON			1
#define MAX_CFG				3
#define NOTIFY_ICRIT			BIT(0)
#define NOTIFY_IWARN			BIT(1)
#define IWARN_POLLING_DELAY_MSEC	1000
#define ICRIT_POLLING_DELAY_MSEC	2000

#define QPNP_BCM_DEV_NAME   "qcom,qpnp-buck-current-monitor"

struct map {
	u8	pc;
	u8	reg_val;
};

static const struct map qpnp_ult_hf_icrit_map[] = {
	{60, 0x03},
	{70, 0x02},
	{80, 0x01},
	{90, 0x00},
};

static const struct map qpnp_ult_hf_iwarn_map[] = {
	{40, 0x03},
	{50, 0x02},
	{60, 0x01},
	{70, 0x00},
};

enum qpnp_buck_type {
	QPNP_ULT_HF_TYPE = 0x22,
};

enum qpnp_buck_subtype {
	QPNP_ULT_HF_SUBTYPE = 0x2,
};

enum qpnp_buck_threshold {
	IWARN_THRESHOLD,
	ICRIT_THRESHOLD,
};

struct buck_irq {
	int		irq;
	unsigned long	disabled;
};

struct qpnp_buck {
	struct spmi_device	*spmi_dev;
	struct device		*hwmon_dev;
	struct buck_irq		icrit_irq;
	struct buck_irq		iwarn_irq;
	struct delayed_work	icrit_work;
	struct delayed_work	iwarn_work;
	const struct map	*icrit_map;
	const struct map	*iwarn_map;
	int			icrit_period_msec;
	int			iwarn_period_msec;
	bool			icrit_alarm;
	bool			iwarn_alarm;
	u8			notify;
	u8			ithreshold_pc[2];
	u8			hcint_en;
	u8			hcint_ctrl_reg;
	u8			hcint_en_reg;
	u16			buck_ps_base;
};

static void enable_buck_irq(struct buck_irq *irq)
{
	if (__test_and_clear_bit(0, &irq->disabled)) {
		enable_irq(irq->irq);
		pr_debug("enabled irq %d\n", irq->irq);
	}
}

static void disable_buck_irq(struct buck_irq *irq)
{
	if (!__test_and_set_bit(0, &irq->disabled)) {
		disable_irq_nosync(irq->irq);
		pr_debug("disabled irq %d\n", irq->irq);
	}
}

static int qpnp_spmi_read_reg(struct qpnp_buck *chip,
				u16 offset, u8 *reg_val, int count)
{
	struct spmi_device *spmi = chip->spmi_dev;
	int rc;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid,
				chip->buck_ps_base + offset, reg_val, count);
	if (rc)
		pr_err("SPMI read failed offset %x rc = %d\n", offset, rc);

	return rc;
}

static int qpnp_spmi_write_reg(struct qpnp_buck *chip,
				u16 offset, u8 *reg_val, int count)
{
	struct spmi_device *spmi = chip->spmi_dev;
	int rc;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid,
				chip->buck_ps_base + offset, reg_val, count);
	if (rc)
		pr_err("SPMI write failed offset %x rc = %d\n", offset, rc);

	return rc;
}

static int qpnp_update_enable(struct qpnp_buck *chip, u8 enable)
{
	int rc;
	u8 reg_val;

	reg_val = (chip->hcint_en_reg & ~HF_HCINT_EN_MASK) |
						(enable << HF_EN_SHIFT);
	rc = qpnp_spmi_write_reg(chip, QPNP_SMPS_REG_HCINT_EN, &reg_val, 1);
	if (rc) {
		pr_err("Failed to %s rc = %d\n", enable ? "enable" : "disable",
						rc);
		return -EINVAL;
	}

	chip->hcint_en_reg = reg_val;
	chip->hcint_en = enable;

	pr_debug("HCINT_EN = %x enable = %u\n", chip->hcint_en_reg,
					chip->hcint_en);
	return rc;
}

static int qpnp_update_current_threshold(struct qpnp_buck *chip,
		u8 threshold_pc, int threshold_type)
{
	u8 mask = 0, reg_val = 0;
	int rc, i;

	threshold_pc = rounddown(threshold_pc, STEP_SIZE);
	if (chip->ithreshold_pc[threshold_type] == threshold_pc)
		return 0;

	switch (threshold_type) {
	case ICRIT_THRESHOLD:
		if ((threshold_pc < chip->icrit_map[0].pc) ||
			(threshold_pc > chip->icrit_map[MAX_CFG].pc)) {
			pr_err("Icrit threshold %u  outside range [%u %u]\n",
					threshold_pc, chip->icrit_map[0].pc,
					chip->icrit_map[MAX_CFG].pc);
			return -EINVAL;
		}

		mask = ~HF_ICRIT_MASK;
		for (i = 0; i <= MAX_CFG; i++)
			if (threshold_pc == chip->icrit_map[i].pc) {
				reg_val = chip->icrit_map[i].reg_val;
				reg_val <<= HF_ICRIT_SHIFT;
				break;
			}
		break;
	case IWARN_THRESHOLD:
		if ((threshold_pc < chip->iwarn_map[0].pc) ||
			(threshold_pc > chip->iwarn_map[MAX_CFG].pc)) {
			pr_err("Iwarn threshold %u outside range [%u %u]\n",
					threshold_pc, chip->iwarn_map[0].pc,
					chip->iwarn_map[MAX_CFG].pc);
			return -EINVAL;
		}

		mask = ~HF_IWARN_MASK;
		for (i = 0; i <= MAX_CFG; i++)
			if (threshold_pc == chip->iwarn_map[i].pc) {
				reg_val = chip->iwarn_map[i].reg_val;
				break;
			}
		break;
	default:
		break;
	}

	reg_val = (chip->hcint_ctrl_reg & mask) | reg_val;
	rc = qpnp_spmi_write_reg(chip, QPNP_SMPS_REG_HCINT_CTRL, &reg_val, 1);
	if (rc) {
		pr_err("Unable to set threshold rc = %d\n", rc);
		return -EINVAL;
	}

	chip->hcint_ctrl_reg = reg_val;
	chip->ithreshold_pc[threshold_type] = threshold_pc;
	pr_debug("HCINT_CTRL = %x  %s threshold value %u\n",
		chip->hcint_ctrl_reg,
		threshold_type ? "Icrit" : "Iwarn",
		chip->ithreshold_pc[threshold_type]);

	return rc;
}

static ssize_t qpnp_show_current_threshold(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct qpnp_buck *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
					chip->ithreshold_pc[attr->index]);
}

static ssize_t qpnp_store_current_threshold(struct device *dev,
		struct device_attribute *devattr, const char *buf,
		size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct qpnp_buck *chip = dev_get_drvdata(dev);
	int rc = -1;
	u8 threshold_pc;

	rc = kstrtou8(buf, 10, &threshold_pc);
	if (rc) {
		pr_err("Invalid %s threshold rc = %d\n",
				attr->index ? "Icrit" : "Iwarn", rc);
		return -EINVAL;
	}

	rc = qpnp_update_current_threshold(chip, threshold_pc, attr->index);
	if (rc) {
		pr_err("Threshold update failed: %s rc = %d\n",
				attr->index ? "Icrit" : "Iwarn", rc);
		return rc;
	}

	pr_debug("Updated %s threshold to %d percent\n",
			attr->index ? "Icrit" : "Iwarn", threshold_pc);
	return count;
}

static ssize_t qpnp_show_enable(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct qpnp_buck *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->hcint_en);
}

static ssize_t qpnp_store_enable(struct device *dev,
		struct device_attribute *devattr, const char *buf,
		size_t count)
{
	struct qpnp_buck *chip = dev_get_drvdata(dev);
	int rc;
	u8 val;

	rc = kstrtou8(buf, 10, &val);
	if (rc) {
		pr_err("Invalid value rc = %d\n", rc);
		return -EINVAL;
	}

	rc = qpnp_update_enable(chip, val);
	if (rc) {
		pr_err("Failed to update HCINT_EN rc = %d\n", rc);
		return -EINVAL;
	}

	pr_debug("HCINT_EN = %d\n", val);
	return count;
}

static ssize_t qpnp_show_alarm(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct qpnp_buck *chip = dev_get_drvdata(dev);
	unsigned stat;

	if (attr->index)
		stat = chip->icrit_alarm;
	else
		stat = chip->iwarn_alarm;

	return snprintf(buf, PAGE_SIZE, "%u\n", stat);
}

static SENSOR_DEVICE_ATTR(curr1_crit, S_IWUSR | S_IRUGO,
		qpnp_show_current_threshold, qpnp_store_current_threshold, 1);
static SENSOR_DEVICE_ATTR(curr1_warn, S_IWUSR | S_IRUGO,
		qpnp_show_current_threshold, qpnp_store_current_threshold, 0);
static SENSOR_DEVICE_ATTR(curr1_crit_alarm, S_IRUGO,
		qpnp_show_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(curr1_warn_alarm, S_IRUGO,
		qpnp_show_alarm, NULL, 0);
static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		qpnp_show_enable, qpnp_store_enable);

static struct attribute *buck_ps_attributes[] = {
	&dev_attr_enable.attr,
	&sensor_dev_attr_curr1_crit.dev_attr.attr,
	&sensor_dev_attr_curr1_warn.dev_attr.attr,
	&sensor_dev_attr_curr1_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_curr1_warn_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group buck_ps_group = {
	.attrs = buck_ps_attributes,
};

static void icrit_polling_work(struct work_struct *work)
{
	struct qpnp_buck *chip = container_of(work, struct qpnp_buck,
							icrit_work.work);
	struct device *dev = &chip->spmi_dev->dev;
	int rc, icrit;
	u8 reg_val;

	if (chip->icrit_alarm && (chip->notify & NOTIFY_ICRIT)) {
		sysfs_notify(&dev->kobj, NULL, "curr1_crit_alarm");
		chip->notify &= ~NOTIFY_ICRIT;
		goto reschedule_crit;
	}

	rc = qpnp_spmi_read_reg(chip, QPNP_SMPS_REG_RT_STS, &reg_val, 1);
	if (rc) {
		pr_err("Unable to read HCINT RT STAT rc = %d\n", rc);
		goto reschedule_crit;
	}

	icrit = (reg_val & HF_ICRIT_RT_MASK) ? true : false;

	/* Current below ICRIT threshold */
	if (chip->icrit_alarm && !icrit) {
		chip->icrit_alarm = icrit;
		sysfs_notify(&dev->kobj, NULL, "curr1_crit_alarm");
		enable_buck_irq(&chip->icrit_irq);
		return;
	}

reschedule_crit:
	if (chip->icrit_alarm)
		schedule_delayed_work(&chip->icrit_work,
				msecs_to_jiffies(chip->icrit_period_msec));
}

static irqreturn_t icrit_trigger(int irq, void *data)
{
	struct qpnp_buck *chip = data;

	pr_debug("icrit interrupt tirggered\n");
	/*
	 * Disable IRQ to prevent interrupt storm due to fluctuation
	 * in current.
	 * Re-enable interrupt in the work function.
	 */
	disable_buck_irq(&chip->icrit_irq);
	chip->notify |= NOTIFY_ICRIT;

	chip->icrit_alarm = true;
	schedule_delayed_work(&chip->icrit_work, 0);

	return IRQ_HANDLED;
}

static void iwarn_polling_work(struct work_struct *work)
{
	struct qpnp_buck *chip = container_of(work, struct qpnp_buck,
							iwarn_work.work);
	struct device *dev = &chip->spmi_dev->dev;
	int rc, iwarn;
	u8 reg_val;

	if (chip->iwarn_alarm && (chip->notify & NOTIFY_IWARN)) {
		sysfs_notify(&dev->kobj, NULL, "curr1_warn_alarm");
		chip->notify &= ~NOTIFY_IWARN;
		goto reschedule_iwarn;
	}

	rc = qpnp_spmi_read_reg(chip, QPNP_SMPS_REG_RT_STS, &reg_val, 1);
	if (rc) {
		pr_err("Unable to read HCINT RT STAT rc = %d\n", rc);
		goto reschedule_iwarn;
	}

	iwarn = (reg_val & HF_IWARN_RT_MASK) ? true : false;

	/* Current below IWARN threshold */
	if (chip->iwarn_alarm && !iwarn) {
		chip->iwarn_alarm = iwarn;
		sysfs_notify(&dev->kobj, NULL, "curr1_warn_alarm");
		enable_buck_irq(&chip->iwarn_irq);
		return;
	}

reschedule_iwarn:
	if (chip->iwarn_alarm)
		schedule_delayed_work(&chip->iwarn_work,
				msecs_to_jiffies(chip->iwarn_period_msec));
}

static irqreturn_t iwarn_trigger(int irq, void *data)
{
	struct qpnp_buck *chip = data;

	pr_debug("iwarn interrupt tirggered\n");
	/*
	 * Disable IRQ to prevent interrupt storm due to fluctuation
	 * in current.
	 * Re-enable interrupt in the work function.
	 */
	disable_buck_irq(&chip->iwarn_irq);
	chip->notify |= NOTIFY_IWARN;

	chip->iwarn_alarm = true;
	schedule_delayed_work(&chip->iwarn_work, 0);

	return IRQ_HANDLED;
}

static int configure_properties(struct qpnp_buck *chip)
{
	struct spmi_device *spmi = chip->spmi_dev;
	int rc;
	unsigned icrit_init_pc, iwarn_init_pc;

	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,icrit-init-threshold-pc", &icrit_init_pc);
	if (rc && rc != -EINVAL) {
		pr_err("Error reading icrit-init-threshold rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,iwarn-init-threshold-pc", &iwarn_init_pc);
	if (rc && rc != -EINVAL) {
		pr_err("Error reading iwarn-init-threshold rc = %d\n", rc);
		return rc;
	}

	/* Polling delay */
	rc = of_property_read_u32(spmi->dev.of_node,
					"qcom,icrit-polling-delay-msec",
					&chip->icrit_period_msec);
	if (rc && rc != -EINVAL) {
		pr_err("Error reading polling-delay-msec rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(spmi->dev.of_node,
					"qcom,iwarn-polling-delay-msec",
					&chip->iwarn_period_msec);
	if (rc && rc != -EINVAL) {
		pr_err("Error reading polling-delay-msec rc = %d\n", rc);
		return rc;
	}

	/* Setup initial threshold values */
	rc = qpnp_update_current_threshold(chip, icrit_init_pc,
							ICRIT_THRESHOLD);
	if (rc) {
		pr_err("Failed to update ICRIT threshold rc = %d\n", rc);
		return rc;
	}

	rc = qpnp_update_current_threshold(chip, iwarn_init_pc,
							IWARN_THRESHOLD);
	if (rc) {
		pr_err("Failed to update IWARN threshold rc = %d\n", rc);
		return rc;
	}

	if (of_property_read_bool(spmi->dev.of_node,
					"qcom,enable-current-monitor")) {
		rc = qpnp_update_enable(chip, EN_CURRENT_MON);
		if (rc) {
			pr_err("Failed to update HCINT_EN rc = %d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int qpnp_buck_init_hw(struct qpnp_buck *chip)
{
	u8 reg_val[2];
	int rc;

	rc = qpnp_spmi_read_reg(chip, QPNP_SMPS_REG_TYPE, reg_val, 2);
	if (rc) {
		pr_err("Unable to read SMPS TYPE reg rc = %d\n", rc);
		return rc;
	}

	switch (reg_val[0]) {
	case QPNP_ULT_HF_TYPE:
		if (reg_val[1]  == QPNP_ULT_HF_SUBTYPE) {
			chip->icrit_map = qpnp_ult_hf_icrit_map;
			chip->iwarn_map = qpnp_ult_hf_iwarn_map;
		} else {
			rc = -EINVAL;
		}
		break;
	default:
		rc = -EINVAL;
	}

	if (rc) {
		pr_err("Invalid type %x subtype %x rc = %d\n",
						reg_val[0], reg_val[1], rc);
		return rc;
	}

	/* Read initial value of HCINT CONTROL reg */
	rc = qpnp_spmi_read_reg(chip, QPNP_SMPS_REG_HCINT_CTRL,
			&chip->hcint_ctrl_reg, 1);
	if (rc) {
		pr_err("Unable to read HCINT reg rc = %d\n", rc);
		return rc;
	}

	/* Read initial value of HCINT ENABLE reg */
	rc = qpnp_spmi_read_reg(chip, QPNP_SMPS_REG_HCINT_EN,
			&chip->hcint_en_reg, 1);
	if (rc) {
		pr_err("Unable to read HCINT reg rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int qpnp_buck_current_monitor_probe(struct spmi_device *spmi)
{
	struct device *dev = &spmi->dev;
	struct qpnp_buck *chip;
	struct resource *resource;
	int rc;

	chip = devm_kzalloc(dev, sizeof(struct qpnp_buck), GFP_KERNEL);
	if (!chip) {
		pr_err("Unable to allocate memory\n");
		return -ENOMEM;
	}

	/* Get the peripheral address */
	resource = spmi_get_resource(spmi, 0, IORESOURCE_MEM, 0);
	if (!resource) {
		pr_err("IORESOURCE absent\n");
		return -ENXIO;
	}
	chip->buck_ps_base = resource->start;
	chip->spmi_dev = spmi;
	chip->icrit_period_msec = ICRIT_POLLING_DELAY_MSEC;
	chip->iwarn_period_msec = IWARN_POLLING_DELAY_MSEC;
	dev_set_drvdata(dev, chip);

	/* Check version and initial state */
	rc = qpnp_buck_init_hw(chip);
	if (rc) {
		pr_err("HW init failed rc = %d\n", rc);
		goto exit;
	}

	INIT_DELAYED_WORK(&chip->icrit_work, icrit_polling_work);
	INIT_DELAYED_WORK(&chip->iwarn_work, iwarn_polling_work);

	/* Setup IRQs */
	chip->icrit_irq.irq = spmi_get_irq_byname(spmi, NULL, "icritical");
	chip->iwarn_irq.irq = spmi_get_irq_byname(spmi, NULL, "iwarning");
	if ((chip->icrit_irq.irq < 0) || (chip->iwarn_irq.irq < 0)) {
		pr_err("IRQ RESOURCE absent\n");
		return -ENXIO;
	}

	/* Setup Valid current table */
	rc = configure_properties(chip);
	if (rc) {
		pr_err("DT parsing failed rc = %d\n", rc);
		goto exit;
	}

	/* Register sysfs hooks */
	rc = sysfs_create_group(&dev->kobj, &buck_ps_group);
	if (rc) {
		pr_err("Unable to create sysfs file rc = %d\n", rc);
		goto exit;
	}

	chip->hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(chip->hwmon_dev)) {
		rc = PTR_ERR(chip->hwmon_dev);
		pr_err("Unable to register with hwmon rc = %d\n", rc);
		goto remove_sysfs;
	}

	rc = devm_request_irq(dev, chip->icrit_irq.irq, icrit_trigger,
				IRQF_TRIGGER_RISING, "icritical", chip);
	if (rc < 0) {
		pr_err("Unable to request irq %d rc = %d\n",
						chip->icrit_irq.irq, rc);
		goto remove_sysfs;
	}

	rc = devm_request_irq(dev, chip->iwarn_irq.irq, iwarn_trigger,
				IRQF_TRIGGER_RISING, "iwarning", chip);
	if (rc < 0) {
		pr_err("Unable to request irq %d rc = %d\n",
						chip->iwarn_irq.irq, rc);
		goto remove_sysfs;
	}

	pr_info("Current monitor probed HCINT_EN=%x HCINT_CTRL=%x\n",
				chip->hcint_en_reg, chip->hcint_ctrl_reg);
	return 0;

remove_sysfs:
	sysfs_remove_group(&dev->kobj, &buck_ps_group);
exit:
	qpnp_update_enable(chip, 0);
	dev_set_drvdata(dev, NULL);
	return rc;
}

static int qpnp_buck_current_monitor_remove(struct spmi_device *spmi)
{
	struct qpnp_buck *chip = dev_get_drvdata(&spmi->dev);

	qpnp_update_enable(chip, 0);
	cancel_delayed_work_sync(&chip->iwarn_work);
	cancel_delayed_work_sync(&chip->icrit_work);
	sysfs_remove_group(&spmi->dev.kobj, &buck_ps_group);
	hwmon_device_unregister(chip->hwmon_dev);
	dev_set_drvdata(&spmi->dev, NULL);

	return 0;
}

static const struct of_device_id qpnp_bcm_match_table[] = {
	{ .compatible = QPNP_BCM_DEV_NAME, },
	{}
};

static struct spmi_driver qpnp_buck_current_monitor_driver = {
	.probe		= qpnp_buck_current_monitor_probe,
	.remove		= qpnp_buck_current_monitor_remove,
	.driver		= {
		.name		= QPNP_BCM_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_bcm_match_table,
	},
};

static int __init qpnp_buck_current_monitor_init(void)
{
	return spmi_driver_register(&qpnp_buck_current_monitor_driver);
}
module_init(qpnp_buck_current_monitor_init);

static void __exit qpnp_buck_current_monitor_exit(void)
{
	spmi_driver_unregister(&qpnp_buck_current_monitor_driver);
}
module_exit(qpnp_buck_current_monitor_exit);

MODULE_DESCRIPTION("QPNP BUCK current monitoring driver");
MODULE_LICENSE("GPL v2");
