/*
 * system-pmic.c -- Core power off/reset functionality from system PMIC.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/power/reset/system-pmic.h>

struct system_pmic_dev {
	struct device *pmic_dev;
	void *pmic_drv_data;
	bool allow_power_off;
	bool allow_power_reset;
	struct system_pmic_ops *ops;
	bool power_on_event[SYSTEM_PMIC_MAX_POWER_ON_EVENT];
	void *power_on_data[SYSTEM_PMIC_MAX_POWER_ON_EVENT];
};

static struct system_pmic_dev *system_pmic_dev;

static void system_pmic_power_reset(void)
{
	system_pmic_dev->ops->power_reset(system_pmic_dev->pmic_drv_data);
	dev_err(system_pmic_dev->pmic_dev,
		"System PMIC is not able to reset system\n");
	while (1);
}

static void system_pmic_power_off(void)
{
	int i;

	for (i = 0; i < SYSTEM_PMIC_MAX_POWER_ON_EVENT; ++i) {
		if (!system_pmic_dev->power_on_event[i])
			continue;
		if (!system_pmic_dev->ops->configure_power_on)
			break;
		system_pmic_dev->ops->configure_power_on(
			system_pmic_dev->pmic_drv_data, i,
			system_pmic_dev->power_on_data[i]);
	}
	system_pmic_dev->ops->power_off(system_pmic_dev->pmic_drv_data);
	dev_err(system_pmic_dev->pmic_dev,
		"System PMIC is not able to power off system\n");
	while (1);
}

int system_pmic_set_power_on_event(enum system_pmic_power_on_event event,
	void *data)
{
	if (!system_pmic_dev) {
		pr_err("System PMIC is not initialized\n");
		return -EINVAL;
	}

	if (!system_pmic_dev->ops->configure_power_on) {
		pr_err("System PMIC does not support power on event\n");
		return -ENOTSUPP;
	}

	system_pmic_dev->power_on_event[event] = 1;
	system_pmic_dev->power_on_data[event] = data;
	return 0;
}
EXPORT_SYMBOL_GPL(system_pmic_set_power_on_event);

struct system_pmic_dev *system_pmic_register(struct device *dev,
	struct system_pmic_ops *ops, struct system_pmic_config *config,
	void *drv_data)
{
	if (system_pmic_dev) {
		pr_err("System PMIC is already registerd\n");
		return ERR_PTR(-EBUSY);
	}

	system_pmic_dev = kzalloc(sizeof(*system_pmic_dev), GFP_KERNEL);
	if (!system_pmic_dev) {
		dev_err(dev, "Memory alloc for system_pmic_dev failed\n");
		return ERR_PTR(-ENOMEM);
	}

	system_pmic_dev->pmic_dev = dev;
	system_pmic_dev->ops = ops;
	system_pmic_dev->pmic_drv_data = drv_data;
	system_pmic_dev->allow_power_off = config->allow_power_off;
	system_pmic_dev->allow_power_reset = config->allow_power_reset;

	if (system_pmic_dev->allow_power_off) {
		if (!ops->power_off)
			goto scrub;
		pm_power_off = system_pmic_power_off;
	}

	if (system_pmic_dev->allow_power_reset) {
		if (!ops->power_reset)
			goto scrub;
		pm_power_reset = system_pmic_power_reset;
	}

	return system_pmic_dev;

scrub:
	dev_err(dev, "Illegal option\n");
	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL_GPL(system_pmic_register);

void system_pmic_unregister(struct system_pmic_dev *pmic_dev)
{
	if (pmic_dev != system_pmic_dev) {
		pr_err("System PMIC can not unregister\n");
		return;
	}

	if (system_pmic_dev->allow_power_off)
		pm_power_off = NULL;

	if (system_pmic_dev->allow_power_reset)
		pm_power_reset = NULL;

	kfree(system_pmic_dev);
	system_pmic_dev = NULL;
}
EXPORT_SYMBOL_GPL(system_pmic_unregister);
