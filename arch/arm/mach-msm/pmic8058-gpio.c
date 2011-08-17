/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
 * Qualcomm PMIC8058 GPIO driver
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mfd/pmic8058.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#ifndef CONFIG_GPIOLIB
#include "gpio_chip.h"
#endif

/* GPIO registers */
#define	SSBI_REG_ADDR_GPIO_BASE		0x150
#define	SSBI_REG_ADDR_GPIO(n)		(SSBI_REG_ADDR_GPIO_BASE + n)

/* GPIO */
#define	PM8058_GPIO_BANK_MASK		0x70
#define	PM8058_GPIO_BANK_SHIFT		4
#define	PM8058_GPIO_WRITE		0x80

/* Bank 0 */
#define	PM8058_GPIO_VIN_MASK		0x0E
#define	PM8058_GPIO_VIN_SHIFT		1
#define	PM8058_GPIO_MODE_ENABLE		0x01

/* Bank 1 */
#define	PM8058_GPIO_MODE_MASK		0x0C
#define	PM8058_GPIO_MODE_SHIFT		2
#define	PM8058_GPIO_OUT_BUFFER		0x02
#define	PM8058_GPIO_OUT_INVERT		0x01

#define	PM8058_GPIO_MODE_OFF		3
#define	PM8058_GPIO_MODE_OUTPUT		2
#define	PM8058_GPIO_MODE_INPUT		0
#define	PM8058_GPIO_MODE_BOTH		1

/* Bank 2 */
#define	PM8058_GPIO_PULL_MASK		0x0E
#define	PM8058_GPIO_PULL_SHIFT		1

/* Bank 3 */
#define	PM8058_GPIO_OUT_STRENGTH_MASK	0x0C
#define	PM8058_GPIO_OUT_STRENGTH_SHIFT	2
#define	PM8058_GPIO_PIN_ENABLE		0x00
#define	PM8058_GPIO_PIN_DISABLE		0x01

/* Bank 4 */
#define	PM8058_GPIO_FUNC_MASK		0x0E
#define	PM8058_GPIO_FUNC_SHIFT		1

/* Bank 5 */
#define	PM8058_GPIO_NON_INT_POL_INV	0x08
#define PM8058_GPIO_BANKS		6

struct pm8058_gpio_chip {
	struct gpio_chip	gpio_chip;
	struct pm8058_chip	*pm_chip;
	struct mutex		pm_lock;
	u8			bank1[PM8058_GPIOS];
};

static int pm8058_gpio_get(struct pm8058_gpio_chip *chip, unsigned gpio)
{
	struct pm8058_gpio_platform_data	*pdata;
	int	mode;

	if (gpio >= PM8058_GPIOS || chip == NULL)
		return -EINVAL;

	pdata = chip->gpio_chip.dev->platform_data;

	/* Get gpio value from config bank 1 if output gpio.
	   Get gpio value from IRQ RT status register for all other gpio modes.
	 */
	mode = (chip->bank1[gpio] & PM8058_GPIO_MODE_MASK) >>
		PM8058_GPIO_MODE_SHIFT;
	if (mode == PM8058_GPIO_MODE_OUTPUT)
		return chip->bank1[gpio] & PM8058_GPIO_OUT_INVERT;
	else
		return pm8058_irq_get_rt_status(chip->pm_chip,
				pdata->irq_base + gpio);
}

static int pm8058_gpio_set(struct pm8058_gpio_chip *chip,
		unsigned gpio, int value)
{
	int	rc;
	u8	bank1;

	if (gpio >= PM8058_GPIOS || chip == NULL)
		return -EINVAL;

	mutex_lock(&chip->pm_lock);
	bank1 = chip->bank1[gpio] & ~PM8058_GPIO_OUT_INVERT;

	if (value)
		bank1 |= PM8058_GPIO_OUT_INVERT;

	chip->bank1[gpio] = bank1;
	rc = pm8058_write(chip->pm_chip, SSBI_REG_ADDR_GPIO(gpio), &bank1, 1);
	mutex_unlock(&chip->pm_lock);

	if (rc)
		pr_err("%s: FAIL pm8058_write(): rc=%d. "
		       "(gpio=%d, value=%d)\n",
		       __func__, rc, gpio, value);

	return rc;
}

static int pm8058_gpio_set_direction(struct pm8058_gpio_chip *chip,
			      unsigned gpio, int direction)
{
	int	rc;
	u8	bank1;
	static int	dir_map[] = {
		PM8058_GPIO_MODE_OFF,
		PM8058_GPIO_MODE_OUTPUT,
		PM8058_GPIO_MODE_INPUT,
		PM8058_GPIO_MODE_BOTH,
	};

	if (!direction || chip == NULL)
		return -EINVAL;

	mutex_lock(&chip->pm_lock);
	bank1 = chip->bank1[gpio] & ~PM8058_GPIO_MODE_MASK;

	bank1 |= ((dir_map[direction] << PM8058_GPIO_MODE_SHIFT)
		  & PM8058_GPIO_MODE_MASK);

	chip->bank1[gpio] = bank1;
	rc = pm8058_write(chip->pm_chip, SSBI_REG_ADDR_GPIO(gpio), &bank1, 1);
	mutex_unlock(&chip->pm_lock);

	if (rc)
		pr_err("%s: Failed on pm8058_write(): rc=%d (GPIO config)\n",
				__func__, rc);

	return rc;
}

static int pm8058_gpio_init_bank1(struct pm8058_gpio_chip *chip)
{
	int i, rc;
	u8 bank;

	for (i = 0; i < PM8058_GPIOS; i++) {
		bank = 1 << PM8058_GPIO_BANK_SHIFT;
		rc = pm8058_write(chip->pm_chip,
				SSBI_REG_ADDR_GPIO(i),
				&bank, 1);
		if (rc) {
			pr_err("%s: error setting bank\n", __func__);
			return rc;
		}

		rc = pm8058_read(chip->pm_chip,
				SSBI_REG_ADDR_GPIO(i),
				&chip->bank1[i], 1);
		if (rc) {
			pr_err("%s: error reading bank 1\n", __func__);
			return rc;
		}
	}
	return 0;
}

#ifndef CONFIG_GPIOLIB
static int pm8058_gpio_configure(struct gpio_chip *chip,
				 unsigned int gpio,
				 unsigned long flags)
{
	int	rc = 0, direction;
	struct pm8058_gpio_chip	*gpio_chip;

	gpio -= chip->start;

	if (flags & (GPIOF_INPUT | GPIOF_DRIVE_OUTPUT)) {
		direction = 0;
		if (flags & GPIOF_INPUT)
			direction |= PM_GPIO_DIR_IN;
		if (flags & GPIOF_DRIVE_OUTPUT)
			direction |= PM_GPIO_DIR_OUT;

		gpio_chip = dev_get_drvdata(chip->dev);

		if (flags & (GPIOF_OUTPUT_LOW | GPIOF_OUTPUT_HIGH)) {
			if (flags & GPIOF_OUTPUT_HIGH)
				rc = pm8058_gpio_set(gpio_chip,
						gpio, 1);
			else
				rc = pm8058_gpio_set(gpio_chip,
						gpio, 0);

			if (rc) {
				pr_err("%s: FAIL pm8058_gpio_set(): rc=%d.\n",
					__func__, rc);
				goto bail_out;
			}
		}

		rc = pm8058_gpio_set_direction(gpio_chip,
				gpio, direction);
		if (rc)
			pr_err("%s: FAIL pm8058_gpio_config(): rc=%d.\n",
				__func__, rc);
	}

bail_out:
	return rc;
}

static int pm8058_gpio_get_irq_num(struct gpio_chip *chip,
				   unsigned int gpio,
				   unsigned int *irqp,
				   unsigned long *irqnumflagsp)
{
	struct pm8058_gpio_platform_data *pdata;

	pdata = chip->dev->platform_data;
	gpio -= chip->start;
	*irqp = pdata->irq_base + gpio;
	if (irqnumflagsp)
		*irqnumflagsp = 0;
	return 0;
}

static int pm8058_gpio_read(struct gpio_chip *chip, unsigned n)
{
	struct pm8058_gpio_chip	*gpio_chip;

	n -= chip->start;
	gpio_chip = dev_get_drvdata(chip->dev);
	return pm8058_gpio_get(gpio_chip, n);
}

static int pm8058_gpio_write(struct gpio_chip *chip, unsigned n, unsigned on)
{
	struct pm8058_gpio_chip	*gpio_chip;

	n -= chip->start;
	gpio_chip = dev_get_drvdata(chip->dev);
	return pm8058_gpio_set(gpio_chip, n, on);
}

static struct pm8058_gpio_chip pm8058_gpio_chip = {
	.gpio_chip = {
		.configure = pm8058_gpio_configure,
		.get_irq_num = pm8058_gpio_get_irq_num,
		.read = pm8058_gpio_read,
		.write = pm8058_gpio_write,
	},
};

static int __devinit pm8058_gpio_probe(struct platform_device *pdev)
{
	int	rc = 0;
	struct pm8058_gpio_platform_data *pdata = pdev->dev.platform_data;

	mutex_init(&pm8058_gpio_chip.pm_lock);
	pm8058_gpio_chip.gpio_chip.dev = &pdev->dev;
	pm8058_gpio_chip.gpio_chip.start = pdata->gpio_base;
	pm8058_gpio_chip.gpio_chip.end = pdata->gpio_base +
		PM8058_GPIOS - 1;
	pm8058_gpio_chip.pm_chip = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, &pm8058_gpio_chip);

	rc = register_gpio_chip(&pm8058_gpio_chip.gpio_chip);
	if (!rc)
		goto bail;

	rc = pm8058_gpio_init_bank1(&pm8058_gpio_chip);
	if (rc)
		goto bail;

	if (pdata->init)
		rc = pdata->init();

bail:
	if (rc)
		platform_set_drvdata(pdev, pm8058_gpio_chip.pm_chip);

	pr_info("%s: register_gpio_chip(): rc=%d\n", __func__, rc);
	return rc;
}

static int __devexit pm8058_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

#else

static int pm8058_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct pm8058_gpio_platform_data *pdata;
	pdata = chip->dev->platform_data;
	return pdata->irq_base + offset;
}

static int pm8058_gpio_read(struct gpio_chip *chip, unsigned offset)
{
	struct pm8058_gpio_chip *gpio_chip;
	gpio_chip = dev_get_drvdata(chip->dev);
	return pm8058_gpio_get(gpio_chip, offset);
}

static void pm8058_gpio_write(struct gpio_chip *chip,
		unsigned offset, int val)
{
	struct pm8058_gpio_chip *gpio_chip;
	gpio_chip = dev_get_drvdata(chip->dev);
	pm8058_gpio_set(gpio_chip, offset, val);
}

static int pm8058_gpio_direction_input(struct gpio_chip *chip,
		unsigned offset)
{
	struct pm8058_gpio_chip *gpio_chip;
	gpio_chip = dev_get_drvdata(chip->dev);
	return pm8058_gpio_set_direction(gpio_chip, offset, PM_GPIO_DIR_IN);
}

static int pm8058_gpio_direction_output(struct gpio_chip *chip,
		unsigned offset,
		int val)
{
	struct pm8058_gpio_chip *gpio_chip;
	int ret;

	gpio_chip = dev_get_drvdata(chip->dev);
	ret = pm8058_gpio_set_direction(gpio_chip, offset, PM_GPIO_DIR_OUT);
	if (!ret)
		ret = pm8058_gpio_set(gpio_chip, offset, val);

	return ret;
}

static void pm8058_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	static const char *cmode[] = { "in", "in/out", "out", "off" };
	struct pm8058_gpio_chip *gpio_chip = dev_get_drvdata(chip->dev);
	u8 mode, state, bank;
	const char *label;
	int i, j;

	for (i = 0; i < PM8058_GPIOS; i++) {
		label = gpiochip_is_requested(chip, i);
		mode = (gpio_chip->bank1[i] & PM8058_GPIO_MODE_MASK) >>
			PM8058_GPIO_MODE_SHIFT;
		state = pm8058_gpio_get(gpio_chip, i);
		seq_printf(s, "gpio-%-3d (%-12.12s) %-10.10s"
				" %s",
				chip->base + i,
				label ? label : "--",
				cmode[mode],
				state ? "hi" : "lo");
		for (j = 0; j < PM8058_GPIO_BANKS; j++) {
			bank = j << PM8058_GPIO_BANK_SHIFT;
			pm8058_write(gpio_chip->pm_chip,
					SSBI_REG_ADDR_GPIO(i),
					&bank, 1);
			pm8058_read(gpio_chip->pm_chip,
					SSBI_REG_ADDR_GPIO(i),
					&bank, 1);
			seq_printf(s, " 0x%02x", bank);
		}
		seq_printf(s, "\n");
	}
}

static struct pm8058_gpio_chip pm8058_gpio_chip = {
	.gpio_chip = {
		.label			= "pm8058-gpio",
		.direction_input	= pm8058_gpio_direction_input,
		.direction_output	= pm8058_gpio_direction_output,
		.to_irq			= pm8058_gpio_to_irq,
		.get			= pm8058_gpio_read,
		.set			= pm8058_gpio_write,
		.dbg_show		= pm8058_gpio_dbg_show,
		.ngpio			= PM8058_GPIOS,
		.can_sleep		= 1,
	},
};

static int __devinit pm8058_gpio_probe(struct platform_device *pdev)
{
	int ret;
	struct pm8058_gpio_platform_data *pdata = pdev->dev.platform_data;

	mutex_init(&pm8058_gpio_chip.pm_lock);
	pm8058_gpio_chip.gpio_chip.dev = &pdev->dev;
	pm8058_gpio_chip.gpio_chip.base = pdata->gpio_base;
	pm8058_gpio_chip.pm_chip = dev_get_drvdata(pdev->dev.parent);
	platform_set_drvdata(pdev, &pm8058_gpio_chip);

	ret = gpiochip_add(&pm8058_gpio_chip.gpio_chip);
	if (ret)
		goto unset_drvdata;

	ret = pm8058_gpio_init_bank1(&pm8058_gpio_chip);
	if (ret)
		goto remove_chip;

	if (pdata->init)
		ret = pdata->init();
	if (!ret)
		goto ok;

remove_chip:
	if (gpiochip_remove(&pm8058_gpio_chip.gpio_chip))
		pr_err("%s: failed to remove gpio chip\n", __func__);
unset_drvdata:
	platform_set_drvdata(pdev, pm8058_gpio_chip.pm_chip);
ok:
	pr_info("%s: gpiochip_add(): rc=%d\n", __func__, ret);

	return ret;
}

static int __devexit pm8058_gpio_remove(struct platform_device *pdev)
{
	return gpiochip_remove(&pm8058_gpio_chip.gpio_chip);
}

#endif

int pm8058_gpio_config(int gpio, struct pm8058_gpio *param)
{
	int	rc;
	u8	bank[8];
	static int	dir_map[] = {
		PM8058_GPIO_MODE_OFF,
		PM8058_GPIO_MODE_OUTPUT,
		PM8058_GPIO_MODE_INPUT,
		PM8058_GPIO_MODE_BOTH,
	};

	if (param == NULL)
		return -EINVAL;

	/* Select banks and configure the gpio */
	bank[0] = PM8058_GPIO_WRITE |
		((param->vin_sel << PM8058_GPIO_VIN_SHIFT) &
			PM8058_GPIO_VIN_MASK) |
		PM8058_GPIO_MODE_ENABLE;
	bank[1] = PM8058_GPIO_WRITE |
		((1 << PM8058_GPIO_BANK_SHIFT) &
			PM8058_GPIO_BANK_MASK) |
		((dir_map[param->direction] <<
			PM8058_GPIO_MODE_SHIFT) &
			PM8058_GPIO_MODE_MASK) |
		((param->direction & PM_GPIO_DIR_OUT) ?
			((param->output_buffer & 1) ?
			 PM8058_GPIO_OUT_BUFFER : 0) : 0) |
		((param->direction & PM_GPIO_DIR_OUT) ?
			param->output_value & 0x01 : 0);
	bank[2] = PM8058_GPIO_WRITE |
		((2 << PM8058_GPIO_BANK_SHIFT) &
			PM8058_GPIO_BANK_MASK) |
		((param->pull << PM8058_GPIO_PULL_SHIFT) &
			PM8058_GPIO_PULL_MASK);
	bank[3] = PM8058_GPIO_WRITE |
		((3 << PM8058_GPIO_BANK_SHIFT) &
			PM8058_GPIO_BANK_MASK) |
		((param->out_strength <<
			PM8058_GPIO_OUT_STRENGTH_SHIFT) &
			PM8058_GPIO_OUT_STRENGTH_MASK) |
		(param->disable_pin ?
			PM8058_GPIO_PIN_DISABLE : PM8058_GPIO_PIN_ENABLE);
	bank[4] = PM8058_GPIO_WRITE |
		((4 << PM8058_GPIO_BANK_SHIFT) &
			PM8058_GPIO_BANK_MASK) |
		((param->function << PM8058_GPIO_FUNC_SHIFT) &
			PM8058_GPIO_FUNC_MASK);
	bank[5] = PM8058_GPIO_WRITE |
		((5 << PM8058_GPIO_BANK_SHIFT) & PM8058_GPIO_BANK_MASK) |
		(param->inv_int_pol ? 0 : PM8058_GPIO_NON_INT_POL_INV);

	mutex_lock(&pm8058_gpio_chip.pm_lock);
	/* Remember bank1 for later use */
	pm8058_gpio_chip.bank1[gpio] = bank[1];
	rc = pm8058_write(pm8058_gpio_chip.pm_chip,
			SSBI_REG_ADDR_GPIO(gpio), bank, 6);
	mutex_unlock(&pm8058_gpio_chip.pm_lock);

	if (rc)
		pr_err("%s: Failed on pm8058_write(): rc=%d (GPIO config)\n",
				__func__, rc);

	return rc;
}
EXPORT_SYMBOL(pm8058_gpio_config);

static struct platform_driver pm8058_gpio_driver = {
	.probe		= pm8058_gpio_probe,
	.remove		= __devexit_p(pm8058_gpio_remove),
	.driver		= {
		.name = "pm8058-gpio",
		.owner = THIS_MODULE,
	},
};

#if defined(CONFIG_DEBUG_FS)

#define DEBUG_MAX_RW_BUF   128
#define DEBUG_MAX_FNAME    8

static struct dentry *debug_dent;

static char debug_read_buf[DEBUG_MAX_RW_BUF];
static char debug_write_buf[DEBUG_MAX_RW_BUF];

static int debug_gpios[PM8058_GPIOS];

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int debug_read_gpio_bank(int gpio, int bank, u8 *data)
{
	int rc;

	mutex_lock(&pm8058_gpio_chip.pm_lock);

	*data = bank << PM8058_GPIO_BANK_SHIFT;
	rc = pm8058_write(pm8058_gpio_chip.pm_chip,
			SSBI_REG_ADDR_GPIO(gpio), data, 1);
	if (rc)
		goto bail_out;

	*data = bank << PM8058_GPIO_BANK_SHIFT;
	rc = pm8058_read(pm8058_gpio_chip.pm_chip,
			SSBI_REG_ADDR_GPIO(gpio), data, 1);

bail_out:
	mutex_unlock(&pm8058_gpio_chip.pm_lock);

	return rc;
}

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int gpio = *((int *) file->private_data);
	int len = 0;
	int rc = -EINVAL;
	u8 bank[PM8058_GPIO_BANKS];
	int val = -1;
	int mode;
	int i;

	for (i = 0; i < PM8058_GPIO_BANKS; i++) {
		rc = debug_read_gpio_bank(gpio, i, &bank[i]);
		if (rc)
			pr_err("pmic failed to read bank %d\n", i);
	}

	if (rc) {
		len = snprintf(debug_read_buf, DEBUG_MAX_RW_BUF - 1, "-1\n");
		goto bail_out;
	}

	val = pm8058_gpio_get(&pm8058_gpio_chip, gpio);

	/* print the mode and the value */
	mode = (bank[1] & PM8058_GPIO_MODE_MASK) >>  PM8058_GPIO_MODE_SHIFT;
	if (mode == PM8058_GPIO_MODE_BOTH)
		len = snprintf(debug_read_buf, DEBUG_MAX_RW_BUF - 1,
			       "BOTH %d ", val);
	else if (mode == PM8058_GPIO_MODE_INPUT)
		len = snprintf(debug_read_buf, DEBUG_MAX_RW_BUF - 1,
			       "IN   %d ", val);
	else if (mode == PM8058_GPIO_MODE_OUTPUT)
		len = snprintf(debug_read_buf, DEBUG_MAX_RW_BUF - 1,
			       "OUT  %d ", val);
	else
		len = snprintf(debug_read_buf, DEBUG_MAX_RW_BUF - 1,
			       "OFF  %d ", val);

	/* print the control register values */
	len += snprintf(debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n",
			bank[0], bank[1], bank[2], bank[3], bank[4], bank[5]);

bail_out:
	rc = simple_read_from_buffer((void __user *) buf, len,
				     ppos, (void *) debug_read_buf, len);

	return rc;
}

static ssize_t debug_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	int gpio = *((int *) file->private_data);
	unsigned long val;
	int mode, rc;

	mode = (pm8058_gpio_chip.bank1[gpio] & PM8058_GPIO_MODE_MASK) >>
		PM8058_GPIO_MODE_SHIFT;
	if (mode == PM8058_GPIO_MODE_OFF || mode == PM8058_GPIO_MODE_INPUT)
		return count;

	if (count > sizeof(debug_write_buf))
		return -EFAULT;

	if (copy_from_user(debug_write_buf, buf, count)) {
		pr_err("failed to copy from user\n");
		return -EFAULT;
	}
	debug_write_buf[count] = '\0';

	rc = strict_strtoul(debug_write_buf, 10, &val);
	if (rc)
		return rc;

	if (pm8058_gpio_set(&pm8058_gpio_chip, gpio, val)) {
		pr_err("gpio write failed\n");
		return -EINVAL;
	}

	return count;
}

static const struct file_operations debug_ops = {
	.open =         debug_open,
	.read =         debug_read,
	.write =        debug_write,
};

static void debug_init(void)
{
	int i;
	char name[DEBUG_MAX_FNAME];

	debug_dent = debugfs_create_dir("pm_gpio", NULL);
	if (IS_ERR(debug_dent)) {
		pr_err("pmic8058 debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(debug_dent));
		return;
	}

	for (i = 0; i < PM8058_GPIOS; i++) {
		snprintf(name, DEBUG_MAX_FNAME-1, "%d", i+1);
		debug_gpios[i] = i;
		if (debugfs_create_file(name, 0644, debug_dent,
					&debug_gpios[i], &debug_ops) == NULL) {
			pr_err("pmic8058 debugfs_create_file %s failed\n",
			       name);
		}
	}
}

static void debug_exit(void)
{
	debugfs_remove_recursive(debug_dent);
}

#else
static void debug_init(void) { }
static void debug_exit(void) { }
#endif

static int __init pm8058_gpio_init(void)
{
	int rc = platform_driver_register(&pm8058_gpio_driver);
	if (!rc)
		debug_init();
	return rc;
}

static void __exit pm8058_gpio_exit(void)
{
	platform_driver_unregister(&pm8058_gpio_driver);
	debug_exit();
}

subsys_initcall(pm8058_gpio_init);
module_exit(pm8058_gpio_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 GPIO driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8058-gpio");
