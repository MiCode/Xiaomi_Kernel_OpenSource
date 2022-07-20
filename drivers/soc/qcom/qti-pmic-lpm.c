// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/regmap.h>

#define QTI_PMIC_LPM_DEV_NAME	"qti,pmic-lpm"

#define SDAM_PBS_ARG_REG	0x42

#define SDAM_INT_REASON_REG	0x47
#define APPS_LPM_EXIT_BIT	BIT(1)
#define APPS_LPM_ENTRY_BIT	BIT(0)

#define SDAM_INT_TEST1		0xE0
#define TEST_MODE_EN_BIT	BIT(7)

#define SDAM_INT_TEST_VAL	0xE1
#define SDAM_INT_TEST_VAL_BIT	BIT(1)

static void qti_pmic_lpm_syscore_shutdown(void);

/**
 * struct qti_pmic_lpm - Structure for QTI pmic lpm device
 * @dev:			Device pointer to the QTI pmic lpm device
 * @regmap:			Regmap structure for reads/writes
 * @twm_class:			Pointer to twm_class class
 * @sdam_base:			Base address of the pmic sdam
 * @twm_enable:			Flag to indicate TWM is enabled
 */
struct qti_pmic_lpm {
	struct device		*dev;
	struct regmap		*regmap;
	struct class		twm_class;
	int			sdam_base;
	bool			twm_enable;
	bool			twm_exit;
	bool			ds_exit;
};

static struct qti_pmic_lpm *gchip;

static int pmic_lpm_read(struct qti_pmic_lpm *chip, int addr, u8 *data, int len)
{
	int rc;

	rc = regmap_bulk_read(chip->regmap, chip->sdam_base + addr, data, len);
	if (rc < 0)
		pr_err("Failed to read from sdam addr:%#x,rc=%d\n",
			chip->sdam_base + addr, rc);

	return rc;
}

static int pmic_lpm_write(struct qti_pmic_lpm *chip, int addr, u8 *data, int len)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, chip->sdam_base + addr, data, len);
	if (rc < 0)
		pr_err("Failed to write to sdam addr:%#x,rc=%d\n",
			chip->sdam_base + addr, rc);

	return rc;
}

static int qti_pmic_handle_lpm(struct qti_pmic_lpm *chip, bool entry)
{
	int rc;
	u8 val;

	val = entry ? APPS_LPM_ENTRY_BIT : APPS_LPM_EXIT_BIT;
	rc = pmic_lpm_write(chip, SDAM_INT_REASON_REG, &val, 1);
	if (rc < 0) {
		pr_err("Failed to write to pmic sdam offset %#x, rc=%d\n",
			SDAM_INT_REASON_REG, rc);
		return rc;
	}

	val = SDAM_INT_TEST_VAL_BIT;
	rc = pmic_lpm_write(chip, SDAM_INT_TEST_VAL, &val, 1);
	if (rc < 0)
		pr_err("Failed to write to pmic sdam offset %#x, rc=%d\n",
			SDAM_INT_TEST_VAL, rc);

	val = 0;
	rc = pmic_lpm_write(chip, SDAM_INT_TEST_VAL, &val, 1);
	if (rc < 0)
		pr_err("Failed to write to pmic sdam offset %#x, rc=%d\n",
			SDAM_INT_TEST_VAL, rc);

	pr_debug("PMIC LPM %s\n", entry ? "entry" : "exit");
	return rc;
}

static ssize_t pmic_twm_enable_store(struct class *c,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct qti_pmic_lpm *chip = container_of(c, struct qti_pmic_lpm,
						twm_class);
	u8 val = 0;
	ssize_t rc;

	rc = kstrtou8(buf, 10, &val);
	if (rc < 0)
		return rc;

	chip->twm_enable = val ? true : false;

	return count;
}

static ssize_t pmic_twm_enable_show(struct class *c,
			struct class_attribute *attr, char *buf)
{
	struct qti_pmic_lpm *chip = container_of(c, struct qti_pmic_lpm,
						twm_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->twm_enable);
}

static ssize_t pmic_twm_exit_show(struct class *c,
			struct class_attribute *attr, char *buf)
{
	struct qti_pmic_lpm *chip = container_of(c, struct qti_pmic_lpm,
						twm_class);

	return scnprintf(buf, PAGE_SIZE, "%x\n", chip->twm_exit);
}

static ssize_t pmic_ds_exit_show(struct class *c,
			struct class_attribute *attr, char *buf)
{
	struct qti_pmic_lpm *chip = container_of(c, struct qti_pmic_lpm,
						twm_class);

	return scnprintf(buf, PAGE_SIZE, "%x\n", chip->ds_exit);
}

static CLASS_ATTR_RW(pmic_twm_enable);
static CLASS_ATTR_RO(pmic_twm_exit);
static CLASS_ATTR_RO(pmic_ds_exit);

static struct attribute *twm_attrs[] = {
	&class_attr_pmic_twm_enable.attr,
	&class_attr_pmic_twm_exit.attr,
	&class_attr_pmic_ds_exit.attr,
	NULL,
};
ATTRIBUTE_GROUPS(twm);

static struct syscore_ops qti_pmic_lpm_syscore_ops = {
	.shutdown = qti_pmic_lpm_syscore_shutdown,
};

static int pmic_get_wakeup_status(struct qti_pmic_lpm *chip)
{
	u8 val = 0;
	int rc = 0;

	chip->twm_exit = false;
	chip->ds_exit = false;

	rc = pmic_lpm_read(chip, SDAM_PBS_ARG_REG, &val, 1);
	if (rc < 0) {
		pr_err("Failed to read pmic sdam offset %#x, rc=%d\n",
			SDAM_PBS_ARG_REG, rc);
		return rc;
	}

	switch (val) {
	case 0x06:
		chip->twm_exit = true;
		break;
	case 0x04:
		chip->ds_exit = true;
		break;
	default:
		break;
	}

	return rc;
}

static int qti_pmic_lpm_probe(struct platform_device *pdev)
{
	struct qti_pmic_lpm *chip;
	int rc;
	u8 val;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Failed to get regmap\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "reg", &chip->sdam_base);
	if (rc < 0) {
		pr_err("Failed to get base address of pmic sdam, rc=%d\n", rc);
		return rc;
	}

	rc = pmic_lpm_read(chip, SDAM_INT_TEST1, &val, 1);
	if (rc < 0) {
		pr_err("Failed to read from pmic sdam offset %#x, rc=%d\n",
			SDAM_INT_TEST1, rc);
		return rc;
	}

	/* Enable interrupt test mode if not enabled already */
	if (val ^ TEST_MODE_EN_BIT) {
		val = TEST_MODE_EN_BIT;
		rc = pmic_lpm_write(chip, SDAM_INT_TEST1, &val, 1);
		if (rc < 0) {
			pr_err("Failed to write to pmic sdam offset %#x, rc=%d\n",
				SDAM_INT_TEST1, rc);
			return rc;
		}
	}

	rc = pmic_get_wakeup_status(chip);
	if (rc < 0) {
		pr_err("Failed to get the twm exit status rc=%d\n", rc);
		return rc;
	}

	chip->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, chip);

	chip->twm_class.name = "pmic-lpm";
	chip->twm_class.class_groups = twm_groups;

	rc = class_register(&chip->twm_class);
	if (rc < 0) {
		pr_err("Failed to register twm_class class rc=%d\n", rc);
		return rc;
	}

	gchip = chip;

	/**
	 * There is a possiblity where-in driver shutdown callback of
	 * LPM driver is called first before other PM drivers shutdown
	 * callback. This can lead to misconfiguration of INT registers.
	 * Hence notify co-proc in syscore_ops shutdown callback which is
	 * called after all the driver shutdown callbacks are called.
	 */
	register_syscore_ops(&qti_pmic_lpm_syscore_ops);

	dev_info(chip->dev, "qti-pmic-lpm probe successful\n");

	return 0;
}

static int qti_pmic_lpm_remove(struct platform_device *pdev)
{
	struct qti_pmic_lpm *chip = platform_get_drvdata(pdev);
	int rc;
	u8 val = 0;

	rc = pmic_lpm_write(chip, SDAM_INT_REASON_REG, &val, 1);
	if (rc < 0) {
		pr_err("Failed to write to pmic sdam offset %#x, rc=%d\n",
			SDAM_INT_REASON_REG, rc);
		return rc;
	}

	class_unregister(&chip->twm_class);

	return rc;
}

static void qti_pmic_lpm_syscore_shutdown(void)
{
	int rc = 0;

	if (gchip == NULL) {
		pr_err("gchip is NULL\n");
		return;
	}

	pr_debug("LPM Syscore shutdown twm_state : %d\n", gchip->twm_enable);

	if (gchip->twm_enable) {
		rc = qti_pmic_handle_lpm(gchip, true);
		if (rc < 0)
			dev_err(gchip->dev, "Failed to handle twm entry, rc:%d\n",
				rc);
		pr_debug("PMIC TWM enabled\n");
	}
}

#ifdef CONFIG_DEEPSLEEP
static int qti_pmic_lpm_suspend_late(struct device *dev)
{
	int rc = 0;
	struct qti_pmic_lpm *chip = dev_get_drvdata(dev);

	/* mem_sleep_current = PM_SUSPEND_MEM in DeepSleep */
	if (pm_suspend_via_firmware()) {
		rc = qti_pmic_handle_lpm(chip, true);
		if (rc < 0)
			dev_err(dev, "Failed to handle suspend_late(), rc:%d\n",
				rc);
	}

	return rc;
}

static int qti_pmic_lpm_resume_early(struct device *dev)
{
	int rc = 0;
	struct qti_pmic_lpm *chip = dev_get_drvdata(dev);

	/* mem_sleep_current = PM_SUSPEND_MEM in DeepSleep */
	if (pm_suspend_via_firmware()) {
		rc = qti_pmic_handle_lpm(chip, false);
		if (rc < 0) {
			dev_err(dev, "Failed to handle resume_early(), rc:%d\n",
				rc);
			return rc;
		}
		rc = pmic_get_wakeup_status(chip);
		if (rc < 0)
			dev_err(dev, "Failed to get deepsleep exit status, rc:%d\n", rc);
	}

	return rc;
}
#endif

static int qti_pmic_lpm_freeze_late(struct device *dev)
{
	int rc;
	struct qti_pmic_lpm *chip = dev_get_drvdata(dev);

	rc = qti_pmic_handle_lpm(chip, true);
	if (rc < 0)
		dev_err(dev, "Failed to handle freeze_late(), rc:%d\n", rc);

	return rc;
}

static int qti_pmic_lpm_restore_early(struct device *dev)
{
	int rc;
	struct qti_pmic_lpm *chip = dev_get_drvdata(dev);

	rc = qti_pmic_handle_lpm(chip, false);
	if (rc < 0)
		dev_err(dev, "Failed to handle restore_early(), rc:%d\n", rc);

	return rc;
}

static const struct dev_pm_ops qti_pmic_lpm_pm_ops = {
	.freeze_late = qti_pmic_lpm_freeze_late,
	.restore_early = qti_pmic_lpm_restore_early,
#ifdef CONFIG_DEEPSLEEP
	.suspend_late = qti_pmic_lpm_suspend_late,
	.resume_early = qti_pmic_lpm_resume_early,
#endif
};

static const struct of_device_id qti_pmic_lpm_match_table[] = {
	{ .compatible = QTI_PMIC_LPM_DEV_NAME },
	{}
};

static struct platform_driver qti_pmic_lpm_driver = {
	.probe		= qti_pmic_lpm_probe,
	.remove		= qti_pmic_lpm_remove,
	.driver		= {
		.name		= "qti,pmic-lpm",
		.of_match_table	= qti_pmic_lpm_match_table,
		.pm = &qti_pmic_lpm_pm_ops,
	},
};
module_platform_driver(qti_pmic_lpm_driver);

MODULE_DESCRIPTION("QTI PMIC LPM driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QTI_PMIC_LPM_DEV_NAME);
