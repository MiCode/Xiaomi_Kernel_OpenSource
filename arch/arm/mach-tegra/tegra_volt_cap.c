/*
 * arch/arm/mach-tegra/tegra_volt_cap.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <mach/edp.h>
#include "cpu-tegra.h"

#define WATCHDOG_TIMER_RATE		3900000 /* 1hr5min in msecs */
#define CPU_THERMAL_ZONE_TYPE		"CPU-therm"
#define ALTERNATE_CPU_TZ_TYPE		"Tdiode_tegra"

/* If the watch dog times out, use this as the voltage cap */
#define DEFAULT_CPU_VMAX_CAP		1180

static struct timer_list watchdog_timer;
static struct work_struct reset_cap_work;
static struct thermal_zone_device *cpu_tz;

static int cpu_vc_temperatures[] = {
	30, 40, 50, 60, 70, 80, 90, 100, 110, 120
};

static struct tegra_cooling_device cpu_vc_cdev = {
	.cdev_type = "tegra_cpu_vc",
	.trip_temperatures = cpu_vc_temperatures,
	.trip_temperatures_num = ARRAY_SIZE(cpu_vc_temperatures)
};

static int cpu_voltage_cap;

struct volt_cap_data {
	struct tegra_cooling_device *cd;
	int thermal_idx;
};

static struct volt_cap_data cpuv_capping_data = {
	.cd = &cpu_vc_cdev,
	.thermal_idx = 0,
};

/* Protects cpu_voltage_cap and serializes calls to update the voltage cap */
DEFINE_MUTEX(cpu_volt_cap_lock);

static struct kobject *volt_cap_kobj;

static ssize_t tegra_cpu_volt_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", cpu_voltage_cap);
}

static ssize_t tegra_cpu_volt_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	unsigned int val, freq;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&cpu_volt_cap_lock);
	freq = 0;
	if (cpu_voltage_cap != val) {
		if (val)
			freq = tegra_edp_find_maxf(val) / 1000;
		else
			freq = tegra_edp_find_maxf(DEFAULT_CPU_VMAX_CAP) / 1000;
		tegra_cpu_set_volt_cap(freq);
	}
	cpu_voltage_cap = val;
	mod_timer(&watchdog_timer, jiffies +
		  msecs_to_jiffies(WATCHDOG_TIMER_RATE));
	mutex_unlock(&cpu_volt_cap_lock);

	return count;
}

static struct kobj_attribute tegra_cpu_volt =
	__ATTR(cpu_volt, 0644, tegra_cpu_volt_show, tegra_cpu_volt_store);

static ssize_t watchdog_rate_sec_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", WATCHDOG_TIMER_RATE / 1000);
}

static struct kobj_attribute watchdog_rate =
	__ATTR_RO(watchdog_rate_sec);

const struct attribute *tegra_volt_cap_attrs[] = {
	&tegra_cpu_volt.attr,
	&watchdog_rate.attr,
	NULL,
};

static int volt_cap_sysfs_init(void)
{
	volt_cap_kobj = kobject_create_and_add("tegra_volt_cap", kernel_kobj);

	if (!volt_cap_kobj) {
		pr_info("Tegra volt cap kobject create failed\n");
		return -1;
	}

	if (sysfs_create_files(volt_cap_kobj, tegra_volt_cap_attrs)) {
		pr_err("tegra_volt_cap: failed to create sysfs interface\n");
		return -1;
	}

	return 0;
}

static struct thermal_zone_device *get_cpu_tz(void)
{
	struct thermal_zone_device *tz;
	tz = thermal_zone_device_find_by_name(CPU_THERMAL_ZONE_TYPE);
	if (!tz)
		tz = thermal_zone_device_find_by_name(ALTERNATE_CPU_TZ_TYPE);
	return tz;
}

/* Cooling device limits minimum rail voltage at cold temperature in pll mode */
static int tegra_vc_get_max_state(struct thermal_cooling_device *cdev,
				  unsigned long *max_state)
{
	struct volt_cap_data *vcd = (struct volt_cap_data *)cdev->devdata;
	*max_state = vcd->cd->trip_temperatures_num;
	return 0;
}

static int tegra_vc_get_cur_state(struct thermal_cooling_device *cdev,
				  unsigned long *cur_state)
{
	struct volt_cap_data *vcd = (struct volt_cap_data *)cdev->devdata;

	*cur_state = vcd->thermal_idx;
	return 0;
}

static int tegra_vc_set_cur_state(struct thermal_cooling_device *cdev,
				  unsigned long cur_state)
{
	unsigned long prev_state;
	unsigned long idx;
	struct volt_cap_data *vcd = (struct volt_cap_data *)cdev->devdata;

	prev_state = vcd->thermal_idx;
	vcd->thermal_idx = cur_state;
	if (prev_state != cur_state && cpu_voltage_cap != 0) {
		if (!cpu_tz)
			cpu_tz = get_cpu_tz();
		if (!cpu_tz) {
			pr_err("tegra_volt_cap: Couldn't find cpu tz\n");
			return 0;
		}
		idx = cur_state;

		/* Actual trip point being crossed */
		if (idx)
			idx = idx - 1;
		thermal_generate_netlink_event(cpu_tz,
				prev_state < cur_state ? THERMAL_AUX0 :
				THERMAL_AUX1,
				vcd->cd->trip_temperatures[idx]);
	}
	return 0;
}

static struct thermal_cooling_device_ops tegra_vc_notify_cooling_ops = {
	.get_max_state = tegra_vc_get_max_state,
	.get_cur_state = tegra_vc_get_cur_state,
	.set_cur_state = tegra_vc_set_cur_state,
};

struct tegra_cooling_device *tegra_vc_get_cdev(void)
{
	return &cpu_vc_cdev;
}

static void reset_cpu_volt_cap(struct work_struct *work)
{
	unsigned int freq;
	mutex_lock(&cpu_volt_cap_lock);
	cpu_voltage_cap = 0;
	freq = tegra_edp_find_maxf(DEFAULT_CPU_VMAX_CAP) / 1000;
	tegra_cpu_set_volt_cap(freq);
	mutex_unlock(&cpu_volt_cap_lock);
	pr_warn("tegra_volt_cap:Timeout. Setting default Vmax-vdd_cpu to: %d\n",
		DEFAULT_CPU_VMAX_CAP);
}

static void watchdog_timeout(unsigned long data)
{
	schedule_work(&reset_cap_work);
}

static int __init tegra_volt_cap_init(void)
{
	struct thermal_cooling_device *tcd;

	volt_cap_sysfs_init();

	/* Register cpu voltage capping related cooling device */
	tcd = thermal_cooling_device_register("tegra_cpu_vc",
					      &cpuv_capping_data,
					      &tegra_vc_notify_cooling_ops);
	if (IS_ERR_OR_NULL(tcd))
		pr_err("tegra cooling device %s failed to register\n",
		       "tegra-vc");
	cpu_tz = get_cpu_tz();

	INIT_WORK(&reset_cap_work, reset_cpu_volt_cap);
	init_timer(&watchdog_timer);
	watchdog_timer.function = watchdog_timeout;

	return 0;
}

MODULE_DESCRIPTION("Tegra voltage capping driver");
MODULE_LICENSE("GPL");
module_init(tegra_volt_cap_init);
