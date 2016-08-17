/*
 * arch/arm/mach-tegra/tegra11_cpu_volt_cap.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/seq_file.h>
#include <mach/edp.h>

#include "cpu-tegra.h"

static int vc_temperatures[] = {20, 30, 40, 50, 60, 70, 80, 90, 100};

static struct tegra_cooling_device vc_cdev = {
			.cdev_type = "tegra_vc",
			.trip_temperatures = vc_temperatures,
			.trip_temperatures_num = ARRAY_SIZE(vc_temperatures)
};

struct volt_cap_data {
	int capped_voltage;
	bool voltage_capping_enabled;
	struct tegra_cooling_device *cd;
	int thermal_idx;
};

static struct volt_cap_data capping_data = {
			.cd = &vc_cdev,
			.thermal_idx = 0,
};

static DEFINE_MUTEX(capping_lock);
static struct kobject *volt_cap_kobj;

static ssize_t tegra_cpu_volt_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", capping_data.capped_voltage);
}

static ssize_t tegra_cpu_volt_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&capping_lock);
	capping_data.capped_voltage = val;
	mutex_unlock(&capping_lock);
	return count;
}

static struct kobj_attribute tegra_cpu_volt =
	__ATTR(volt, 0644, tegra_cpu_volt_show, tegra_cpu_volt_store);

static ssize_t capping_enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", capping_data.voltage_capping_enabled);
}

static ssize_t capping_enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int voltage;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&capping_lock);
	capping_data.voltage_capping_enabled = (bool)val;
	voltage = capping_data.capped_voltage;
	mutex_unlock(&capping_lock);
	if (val && voltage)
		tegra_cpu_set_volt_cap(tegra_edp_find_maxf(voltage));
	else
		tegra_cpu_set_volt_cap(0);

	return count;
}

static struct kobj_attribute capping_enable =
	__ATTR(capping_state, 0644, capping_enable_show, capping_enable_store);

const struct attribute *tegra_volt_cap_attrs[] = {
	&capping_enable.attr,
	&tegra_cpu_volt.attr,
	NULL,
};

static int volt_cap_sysfs_init(void)
{
	volt_cap_kobj = kobject_create_and_add("tegra_cpu_volt_cap",
		kernel_kobj);

	if (!volt_cap_kobj) {
		pr_info("CPU volt_cap failed\n");
		return -1;
	}

	if (sysfs_create_files(volt_cap_kobj, tegra_volt_cap_attrs)) {
		pr_err("tegra:failed to create sysfs cap interface\n");
		return -1;
	}

	return 0;
}

/* Cooling device limits minimum rail voltage at cold temperature in pll mode */
static int tegra_vc_get_max_state(
	struct thermal_cooling_device *cdev, unsigned long *max_state)
{
	struct volt_cap_data *vcd = (struct volt_cap_data *)cdev->devdata;
	*max_state = vcd->cd->trip_temperatures_num;
	return 0;
}

static int tegra_vc_get_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *cur_state)
{
	struct volt_cap_data *vcd = (struct volt_cap_data *)cdev->devdata;

	*cur_state = vcd->thermal_idx;
	return 0;
}

static int tegra_vc_set_cur_state(
	struct thermal_cooling_device *cdev, unsigned long cur_state)
{
	struct volt_cap_data *vcd = (struct volt_cap_data *)cdev->devdata;

	mutex_lock(&capping_lock);
	if (vcd->thermal_idx != cur_state) {
		thermal_generate_netlink_event(
			vcd->cd->trip_temperatures[cur_state],
			cur_state > vcd->thermal_idx ?
				THERMAL_AUX1 : THERMAL_AUX0);
		vcd->thermal_idx = cur_state;
	}
	mutex_unlock(&capping_lock);
	return 0;
}

static struct thermal_cooling_device_ops tegra_vc_notify_cooling_ops = {
	.get_max_state = tegra_vc_get_max_state,
	.get_cur_state = tegra_vc_get_cur_state,
	.set_cur_state = tegra_vc_set_cur_state,
};

struct tegra_cooling_device *tegra_vc_get_cdev(void)
{
	return &vc_cdev;
}

static int __init tegra_volt_cap_init(void)
{
	struct thermal_cooling_device *tcd;

	volt_cap_sysfs_init();
	tcd = thermal_cooling_device_register(
		"tegra_vc", &capping_data,
		&tegra_vc_notify_cooling_ops);
	if (IS_ERR_OR_NULL(tcd))
		pr_err("tegra cooling device %s failed to register\n",
		       "tegra-vc");

	return 0;
}

MODULE_DESCRIPTION("Tegra11 CPU voltage capping driver");
MODULE_LICENSE("GPL");
module_init(tegra_volt_cap_init);
