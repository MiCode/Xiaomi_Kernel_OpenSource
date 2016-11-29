/*
 * Intel Baytrail PWM driver.
 *
 * Copyright (C) 2013 Intel corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <linux/pwm.h>
#include "pwm_byt_core.h"
#include <linux/dmi.h>
#include <linux/acpi.h>

/* PWM registers and bits definitions */

#define PWMCR(chip)	(chip->mmio_base + 0)
#define PWMRESET(chip)	(chip->mmio_base + 0x804)
#define PWMGENERAL(chip) (chip->mmio_base + 0x808)
#define PWMCR_EN	(1 << 31)
#define PWMCR_UP	(1 << 30)
#define PWMRESET_EN	3

#define PWMCR_OTD_MASK	0xff
#define PWMCR_BU_MASK	0xff00
#define PWMCR_BUF_MASK	0xff0000

#define PWMCR_OTD_OFFSET	0
#define PWMCR_BU_OFFSET	16
#define PWMCR_BUF_OFFSET	8

struct byt_pwm_chip {
	struct mutex lock;
	unsigned int pwm_num;
	struct pwm_chip	chip;
	struct device	*dev;
	struct list_head list;
	void __iomem	*mmio_base;
	unsigned int   clk_khz;
};

static LIST_HEAD(pwm_chip_list);

static inline struct byt_pwm_chip *to_byt_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct byt_pwm_chip, chip);
}

static int byt_pwm_wait_update_complete(struct byt_pwm_chip *byt_pwm)
{
	uint32_t update;
	int retry = 1000000;

	while (retry--) {
		update = ioread32(PWMCR(byt_pwm));
		if (!(update & PWMCR_UP))
			break;
		if (!(update & PWMCR_EN))
			break;

		usleep_range(1, 10);
	}

	if (retry < 0) {
		pr_err("PWM update failed, update bit is not cleared!");
		return -EBUSY;
	} else {
		return 0;
	}
}

static int byt_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			  int duty_ns, int period_ns)
{
	struct byt_pwm_chip *byt_pwm = to_byt_pwm_chip(chip);
	uint32_t bu;
	uint32_t bu_f;
	uint32_t otd;
	uint32_t update;
	int r;
	uint32_t clock;
	uint32_t temp;
	const char *board_name;
	pm_runtime_get_sync(byt_pwm->dev);
	msleep(50);

	/* frequency = clock * base_unit/256, so:
	   base_unit = frequency * 256 / clock, which result:
	   base_unit = 256 * 10^6 / (clock_khz * period_ns); */
	bu = (256 * 1000000) / (byt_pwm->clk_khz * period_ns);
	bu_f = (256 * 1000000) % (byt_pwm->clk_khz * period_ns);
	bu_f = (bu_f / 1000000 * 256) / (byt_pwm->clk_khz / 1000 * period_ns / 1000);

	/* one time divison calculation:
	   duty_ns / period_ns = (255 - otd) / 255 */
	otd = 255 - duty_ns * 255 / period_ns;
	mutex_lock(&byt_pwm->lock);
	board_name = dmi_get_system_info(DMI_BOARD_NAME);
	if ((strcmp(board_name, "Cherry Trail CR") == 0) ||
			(strcmp(board_name, "Cherry Trail FFD") == 0) ||
			(strcmp(board_name, "Mipad") == 0)) {
		pr_debug("Cherry Trail CR: byt_pwm_config\n");
		/* reset pwm before configuring */
		iowrite32(PWMRESET_EN, PWMRESET(byt_pwm));
		/* set clock */
		clock = ioread32(PWMGENERAL(byt_pwm));
		if (byt_pwm->clk_khz == PWM_CHT_CLK_KHZ) {
			temp = BIT(3);
			clock &= ~temp;
		} else {
			clock |= BIT(3);
		}
		iowrite32(clock, PWMGENERAL(byt_pwm));
	}
	/* update counter */
	update = ioread32(PWMCR(byt_pwm));
	update &= (~PWMCR_OTD_MASK & ~PWMCR_BU_MASK & ~PWMCR_BUF_MASK);
	update |= (otd & 0xff) << PWMCR_OTD_OFFSET;
	update |= (bu & 0xff) << PWMCR_BU_OFFSET;
	update |= (bu_f & 0xff) << PWMCR_BUF_OFFSET;
	if ((strcmp(board_name, "Cherry Trail CR") == 0) ||
			(strcmp(board_name, "Cherry Trail FFD") == 0) ||
			(strcmp(board_name, "Mipad") == 0)) {
		update &= 0x00ffffff;
	}
	iowrite32(update, PWMCR(byt_pwm));


	/* set update flag */
	update |= PWMCR_UP;
	iowrite32(update, PWMCR(byt_pwm));
	r = byt_pwm_wait_update_complete(byt_pwm);
	mutex_unlock(&byt_pwm->lock);

	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);

	return r;
}

static int byt_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct byt_pwm_chip *byt_pwm = to_byt_pwm_chip(chip);
	uint32_t val;
	int r;

	pm_runtime_get_sync(byt_pwm->dev);
	mutex_lock(&byt_pwm->lock);

	val = ioread32(PWMCR(byt_pwm));
	val |= PWMCR_UP;
	iowrite32(val | PWMCR_EN, PWMCR(byt_pwm));
	r = byt_pwm_wait_update_complete(byt_pwm);

	mutex_unlock(&byt_pwm->lock);
	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);

	return r;
}

static void byt_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct byt_pwm_chip *byt_pwm = to_byt_pwm_chip(chip);
	uint32_t val;

	pm_runtime_get_sync(byt_pwm->dev);
	mutex_lock(&byt_pwm->lock);

	val = ioread32(PWMCR(byt_pwm));
	val |= 0xff;
	iowrite32(val, PWMCR(byt_pwm));

	val |= PWMCR_UP;
	iowrite32(val, PWMCR(byt_pwm));
	msleep(20);

	iowrite32(val & ~PWMCR_EN, PWMCR(byt_pwm));
	mutex_unlock(&byt_pwm->lock);
	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);
}

static struct pwm_ops byt_pwm_ops = {
	.config = byt_pwm_config,
	.enable = byt_pwm_enable,
	.disable = byt_pwm_disable,
	.owner = THIS_MODULE,
};


static struct byt_pwm_chip *find_pwm_chip(unsigned int pwm_num)
{
	struct byt_pwm_chip *p;
	list_for_each_entry(p, &pwm_chip_list, list) {
		if (p->pwm_num == pwm_num)
			return p;
	}
	return NULL;
}

struct pwm_chip *find_pwm_dev(unsigned int pwm_num)
{
	struct byt_pwm_chip *p;
	list_for_each_entry(p, &pwm_chip_list, list) {
		if (p->pwm_num == pwm_num)
			return &p->chip;
	}
	return NULL;
}
EXPORT_SYMBOL(find_pwm_dev);

/* directly read a value to a PWM register */
int lpio_bl_read(uint8_t pwm_num, uint32_t reg)
{
	struct byt_pwm_chip *byt_pwm;
	int ret;

	/* only PWM_CTRL register is supported */
	if (reg != LPIO_PWM_CTRL)
		return -EINVAL;

	byt_pwm = find_pwm_chip(pwm_num);
	if (!byt_pwm) {
		pr_err("%s: can't find pwm device with pwm_num %d\n",
				__func__, (int) pwm_num);
		return -EINVAL;
	}

	pm_runtime_get_sync(byt_pwm->dev);
	mutex_lock(&byt_pwm->lock);

	ret = ioread32(PWMCR(byt_pwm));

	mutex_unlock(&byt_pwm->lock);
	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);

	return ret;

}
EXPORT_SYMBOL(lpio_bl_read);

/* directly write a value to a PWM register */
int lpio_bl_write(uint8_t pwm_num, uint32_t reg, uint32_t val)
{
	struct byt_pwm_chip *byt_pwm;

	/* only PWM_CTRL register is supported */
	if (reg != LPIO_PWM_CTRL)
		return -EINVAL;

	byt_pwm = find_pwm_chip(pwm_num);
	if (!byt_pwm) {
		pr_err("%s: can't find pwm device with pwm_num %d\n",
				__func__, (int) pwm_num);
		return -EINVAL;
	}

	pm_runtime_get_sync(byt_pwm->dev);
	mutex_lock(&byt_pwm->lock);

	iowrite32(val, PWMCR(byt_pwm));

	mutex_unlock(&byt_pwm->lock);
	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);

	return 0;

}
EXPORT_SYMBOL(lpio_bl_write);

/* directly update bits of a PWM register */
int lpio_bl_write_bits(uint8_t pwm_num, uint32_t reg, uint32_t val,
		uint32_t mask)
{
	struct byt_pwm_chip *byt_pwm;
	uint32_t update;
	int ret;

	/* only PWM_CTRL register is supported */
	if (reg != LPIO_PWM_CTRL)
		return -EINVAL;

	byt_pwm = find_pwm_chip(pwm_num);
	if (!byt_pwm) {
		pr_err("%s: can't find pwm device with pwm_num %d\n",
				__func__, (int) pwm_num);
		return -EINVAL;
	}

	pm_runtime_get_sync(byt_pwm->dev);
	mutex_lock(&byt_pwm->lock);

	ret = byt_pwm_wait_update_complete(byt_pwm);
	update = ioread32(PWMCR(byt_pwm));
	update = (update & ~mask) | (val & mask);
	iowrite32(update, PWMCR(byt_pwm));

	mutex_unlock(&byt_pwm->lock);
	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);
	return ret;
}
EXPORT_SYMBOL(lpio_bl_write_bits);

/* set the update bit of the PWM control register to force PWM device to use the
new configuration */
int lpio_bl_update(uint8_t pwm_num, uint32_t reg)
{
	struct byt_pwm_chip *byt_pwm;
	uint32_t update;

	/* only PWM_CTRL register is supported */
	if (reg != LPIO_PWM_CTRL)
		return -EINVAL;

	byt_pwm = find_pwm_chip(pwm_num);
	if (!byt_pwm) {
		pr_err("%s: can't find pwm device with pwm_num %d\n",
				__func__, (int) pwm_num);
		return -EINVAL;
	}

	pm_runtime_get_sync(byt_pwm->dev);
	mutex_lock(&byt_pwm->lock);

	update = ioread32(PWMCR(byt_pwm));
	update |= PWMCR_UP;
	iowrite32(update, PWMCR(byt_pwm));

	mutex_unlock(&byt_pwm->lock);
	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);
	return 0;
}
EXPORT_SYMBOL(lpio_bl_update);

static ssize_t attr_ctl_reg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);
	uint32_t val;

	pm_runtime_get_sync(byt_pwm->dev);
	mutex_lock(&byt_pwm->lock);

	val = ioread32(PWMCR(byt_pwm));

	mutex_unlock(&byt_pwm->lock);
	pm_runtime_mark_last_busy(byt_pwm->dev);
	pm_runtime_put_autosuspend(byt_pwm->dev);

	return sprintf(buf, "0x%x\n", val);
}

static ssize_t attr_test_pwm_config(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);
	int duty_ns, period_ns;
	int r;
	int pwm_id;
	struct pwm_device *pwm;

	r = sscanf(buf, "%d %d", &duty_ns, &period_ns);
	if (r != 2)
		return -EINVAL;

	pwm_id = byt_pwm->chip.pwms[0].pwm;
	pwm = pwm_request(pwm_id, "test");
	if (!pwm)
		return -ENODEV;
	r = pwm_config(pwm, duty_ns, period_ns);
	pwm_free(pwm);
	if (r)
		return r;

	return size;
}

static ssize_t attr_test_write(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);
	u32 val;

	if (kstrtou32(buf, 16, &val))
		return -EINVAL;

	lpio_bl_write(byt_pwm->pwm_num, LPIO_PWM_CTRL, val);
	return size;
}

static ssize_t attr_test_update(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);
	lpio_bl_update(byt_pwm->pwm_num, LPIO_PWM_CTRL);
	return size;
}

static ssize_t attr_test_write_bits(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);
	unsigned int val, mask;
	int r;

	r = sscanf(buf, "%x %x", &val, &mask);
	if (r != 2)
		return -EINVAL;

	lpio_bl_write_bits(byt_pwm->pwm_num, LPIO_PWM_CTRL, val, mask);
	return size;
}

static DEVICE_ATTR(ctl_reg, S_IRUSR, attr_ctl_reg_show, NULL);
static DEVICE_ATTR(pwm_config, S_IWUSR, NULL, attr_test_pwm_config);
static DEVICE_ATTR(test_write, S_IWUSR, NULL, attr_test_write);
static DEVICE_ATTR(test_update, S_IWUSR, NULL, attr_test_update);
static DEVICE_ATTR(test_write_bits, S_IWUSR, NULL, attr_test_write_bits);

static struct attribute *byt_pwm_attrs[] = {
	&dev_attr_ctl_reg.attr,
	&dev_attr_pwm_config.attr,
	&dev_attr_test_write.attr,
	&dev_attr_test_update.attr,
	&dev_attr_test_write_bits.attr,
	NULL
};

static const struct attribute_group byt_pwm_attr_group = {
	.attrs = byt_pwm_attrs,
};

int pwm_byt_init(struct device *dev, void __iomem *base,
		int pwm_num, unsigned int clk_khz)
{

	struct byt_pwm_chip *byt_pwm;
	int r;

	byt_pwm = devm_kzalloc(dev, sizeof(*byt_pwm), GFP_KERNEL);
	if (!byt_pwm) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	mutex_init(&byt_pwm->lock);
	byt_pwm->dev = dev;
	byt_pwm->chip.dev = dev;
	byt_pwm->chip.ops = &byt_pwm_ops;
	byt_pwm->chip.base = -1;
	byt_pwm->chip.npwm = 1;
	byt_pwm->mmio_base = base;
	byt_pwm->pwm_num = pwm_num;
	byt_pwm->clk_khz = clk_khz;
	dev_set_drvdata(dev, byt_pwm);

	r = pwmchip_add(&byt_pwm->chip);
	if (r < 0) {
		dev_err(dev, "pwmchip_add() failed: %d\n", r);
		r = -ENODEV;
		goto err_kfree;
	}
	r = sysfs_create_group(&dev->kobj, &byt_pwm_attr_group);
	if (r) {
		dev_err(dev, "failed to create sysfs files: %d\n", r);
		goto err_remove_chip;
	}
	list_add_tail(&byt_pwm->list, &pwm_chip_list);
	dev_info(dev, "PWM device probed: pwm_num=%d, mmio_base=%p clk_khz=%d\n",
			byt_pwm->pwm_num, byt_pwm->mmio_base, byt_pwm->clk_khz);

	return 0;

err_remove_chip:
	pwmchip_remove(&byt_pwm->chip);
err_kfree:
	devm_kfree(dev, byt_pwm);
	dev_err(dev, "PWM device probe failed!\n");
	return r;
}
EXPORT_SYMBOL(pwm_byt_init);

void pwm_byt_remove(struct device *dev)
{
	struct byt_pwm_chip *byt_pwm;

	sysfs_remove_group(&dev->kobj, &byt_pwm_attr_group);
	byt_pwm = dev_get_drvdata(dev);
	list_del(&byt_pwm->list);
	pwmchip_remove(&byt_pwm->chip);
	mutex_destroy(&byt_pwm->lock);
}
EXPORT_SYMBOL(pwm_byt_remove);

static int pwm_byt_runtime_suspend(struct device *dev)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);
	uint32_t val;
	int r = 0;

	if (!mutex_trylock(&byt_pwm->lock)) {
		dev_err(dev, "PWM suspend called! can't get lock\n");
		return -EAGAIN;
	}

	val = ioread32(PWMCR(byt_pwm));
	r = (val & PWMCR_EN) ? -EAGAIN : 0;

	mutex_unlock(&byt_pwm->lock);
	return r;
}

static int pwm_byt_runtime_resume(struct device *dev)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);

	if (!mutex_trylock(&byt_pwm->lock)) {
		dev_err(dev, "Can't get lock\n");
		return -EAGAIN;
	}

	iowrite32(PWMRESET_EN, PWMRESET(byt_pwm));

	mutex_unlock(&byt_pwm->lock);
	return 0;
}
static int pwm_byt_suspend(struct device *dev)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);
	uint32_t val;
	int r = 0;
	struct acpi_device *adev;
	acpi_handle handle;

	handle = ACPI_HANDLE(dev);
	if (!handle) {
		pr_warn("Failed to acpi get handle in %s\n", __func__);
		return r;
	}

	r = acpi_bus_get_device(handle, &adev);
	if (r) {
		pr_warn("Failed to acpi device in %s\n", __func__);
		return r;
	}

	if (!adev->power.state) {
		pr_info("Device %s is already in non D0 state\n",
				dev_name(dev));
		return r;
	}

	if (!mutex_trylock(&byt_pwm->lock)) {
		dev_err(dev, "PWM suspend called! can't get lock\n");
		return -EAGAIN;
	}

	val = ioread32(PWMCR(byt_pwm));
	r = (val & PWMCR_EN) ? -EAGAIN : 0;

	mutex_unlock(&byt_pwm->lock);
	return r;
}

static int pwm_byt_resume(struct device *dev)
{
	struct byt_pwm_chip *byt_pwm = dev_get_drvdata(dev);

	if (!mutex_trylock(&byt_pwm->lock)) {
		dev_err(dev, "Can't get lock\n");
		return -EAGAIN;
	}

	iowrite32(PWMRESET_EN, PWMRESET(byt_pwm));

	mutex_unlock(&byt_pwm->lock);
	return 0;
}

const struct dev_pm_ops pwm_byt_pm = {
	.suspend_late = pwm_byt_suspend,
	.resume_early = pwm_byt_resume,
	SET_RUNTIME_PM_OPS(pwm_byt_runtime_suspend,
					pwm_byt_runtime_resume, NULL)
};
EXPORT_SYMBOL(pwm_byt_pm);
