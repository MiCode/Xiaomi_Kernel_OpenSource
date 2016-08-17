/*
 * arch/arm/mach-tegra/edp_core.c
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/suspend.h>

#include <mach/edp.h>
#include <mach/thermal.h>

#include "clock.h"
#include "fuse.h"

static DEFINE_MUTEX(core_edp_lock);

static struct tegra_core_edp_limits core_edp_limits;
static const struct tegra_core_edp_limits *limits;

static bool core_edp_disabled;

static bool core_edp_scpu_state;
static int core_edp_profile;
static int core_edp_modules_state;
static int core_edp_thermal_idx;
static int core_edp_suspended_profile = CORE_EDP_PROFILES_NUM;

static const char *profile_names[CORE_EDP_PROFILES_NUM] = {
	[CORE_EDP_PROFILE_BALANCED]  = "profile_balanced",
	[CORE_EDP_PROFILE_FAVOR_GPU] = "profile_favor_gpu",
	[CORE_EDP_PROFILE_FAVOR_EMC] = "profile_favor_emc",
};

static unsigned long *get_cap_rates(bool scpu_state, int profile,
				    int m_state, int t_idx)
{
	unsigned long *cap_rates = scpu_state ?
		limits->cap_rates_scpu_on : limits->cap_rates_scpu_off;

	cap_rates += ((profile * limits->core_modules_states + m_state) *
		      limits->temperature_ranges + t_idx) *
		      limits->cap_clocks_num;

	return cap_rates;
}

static unsigned long *get_current_cap_rates(void)
{
	return get_cap_rates(core_edp_scpu_state, core_edp_profile,
			     core_edp_modules_state, core_edp_thermal_idx);
}

static int set_max_rates(void)
{
	int i, ret;

	if (core_edp_disabled)
		return 0;

	for (i = 0; i < limits->cap_clocks_num; i++) {
		struct clk *c = limits->cap_clocks[i];
		ret = clk_set_rate(c, clk_get_max_rate(c));
		if (ret) {
			pr_err("%s: Failed to set %s max rate %lu\n",
			       __func__, c->name, clk_get_max_rate(c));
			return ret;
		}
	}
	return 0;
}

static int set_cap_rates(unsigned long *new_rates)
{
	int i, ret;

	if (core_edp_disabled)
		return 0;

	for (i = 0; i < limits->cap_clocks_num; i++) {
		struct clk *c = limits->cap_clocks[i];
		ret = clk_set_rate(c, new_rates[i]);
		if (ret) {
			pr_err("%s: Failed to set %s rate %lu\n",
			       __func__, c->name, new_rates[i]);
			return ret;
		}
	}
	return 0;
}

static int update_cap_rates(unsigned long *new_rates, unsigned long *old_rates)
{
	int i, ret;

	if (core_edp_disabled)
		return 0;

	/* 1st lower caps */
	for (i = 0; i < limits->cap_clocks_num; i++) {
		if (new_rates[i] < old_rates[i]) {
			struct clk *c = limits->cap_clocks[i];
			ret = clk_set_rate(c, new_rates[i]);
			if (ret) {
				pr_err("%s: Failed to set %s rate %lu\n",
				       __func__, c->name, new_rates[i]);
				return ret;
			}

		}
	}

	/* then increase caps */
	for (i = 0; i < limits->cap_clocks_num; i++) {
		if (new_rates[i] > old_rates[i]) {
			struct clk *c = limits->cap_clocks[i];
			ret = clk_set_rate(c, new_rates[i]);
			if (ret) {
				pr_err("%s: Failed to set %s rate %lu\n",
				       __func__, c->name, new_rates[i]);
				return ret;
			}

		}
	}
	return 0;
}

static int __init start_core_edp(void)
{
	int ret;

	/*
	 * Default state:
	 * always boot G-cluster (no cpu on core rail),
	 * non-throttled EMC profile
	 * all core modules that affect EDP are On
	 * unknown temperature - assume maximum (WC)
	 */
	core_edp_scpu_state = false;
	core_edp_profile = CORE_EDP_PROFILE_FAVOR_EMC;
	core_edp_modules_state = 0;
	core_edp_thermal_idx = limits->temperature_ranges - 1;

	ret = set_cap_rates(get_current_cap_rates());
	if (ret)
		return ret;

	return 0;
}

void __init tegra_init_core_edp_limits(unsigned int regulator_mA)
{
	int i;
	unsigned long *cap_rates;

	switch (tegra_chip_id) {
	case TEGRA11X:
		if (tegra11x_select_core_edp_table(
			regulator_mA, &core_edp_limits))
			return;
		break;
	default:
		pr_err("%s: core edp is not supported on chip ID %d\n",
		       __func__, tegra_chip_id);
		return;
	}

	limits = &core_edp_limits;

	if (start_core_edp()) {
		WARN(1, "Core EDP failed to set initiali limits");
		return;
	}

	cap_rates = get_current_cap_rates();
	pr_info("Core EDP limits are initialized at:\n");
	for (i = 0; i < limits->cap_clocks_num; i++)
		pr_info("    %10s: %lu\n",
			limits->cap_clocks[i]->name, cap_rates[i]);
}

/* core edp cpu state update */
int tegra_core_edp_cpu_state_update(bool scpu_state)
{
	int ret = 0;
	unsigned long *old_cap_rates;
	unsigned long *new_cap_rates;

	if (!limits) {
		core_edp_scpu_state = scpu_state;
		return 0;
	}

	mutex_lock(&core_edp_lock);

	if (core_edp_scpu_state != scpu_state) {
		old_cap_rates = get_current_cap_rates();
		new_cap_rates = get_cap_rates(scpu_state, core_edp_profile,
				core_edp_modules_state, core_edp_thermal_idx);
		ret = update_cap_rates(new_cap_rates, old_cap_rates);
		if (ret)
			update_cap_rates(old_cap_rates, new_cap_rates);
		else
			core_edp_scpu_state = scpu_state;
	}
	mutex_unlock(&core_edp_lock);

	return ret;
}

/* core edp profiles update */
static int  _profile_update(int profile)
{
	int ret = 0;
	unsigned long *old_cap_rates;
	unsigned long *new_cap_rates;

	if (core_edp_profile != profile) {
		old_cap_rates = get_current_cap_rates();
		new_cap_rates = get_cap_rates(core_edp_scpu_state, profile,
				core_edp_modules_state, core_edp_thermal_idx);
		ret = update_cap_rates(new_cap_rates, old_cap_rates);
		if (ret)
			update_cap_rates(old_cap_rates, new_cap_rates);
		else
			core_edp_profile = profile;
	}

	return ret;
}

static int profile_update(int profile)
{
	int ret = 0;

	if (!limits) {
		core_edp_profile = profile;
		return 0;
	}

	mutex_lock(&core_edp_lock);
	if (core_edp_suspended_profile == CORE_EDP_PROFILES_NUM)
		ret = _profile_update(profile);
	mutex_unlock(&core_edp_lock);
	return ret;
}

static ssize_t
core_edp_profile_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	return sprintf(buf, "%s\n", profile_names[core_edp_profile]);
}
static ssize_t
core_edp_profile_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int i, ret;
	size_t l;
	const char *name;

	for (i = 0; i < ARRAY_SIZE(profile_names); i++) {
		name = profile_names[i];
		l = strlen(name);
		if ((l <= count) && (strncmp(buf, name, l) == 0))
			break;
	}
	if (i == ARRAY_SIZE(profile_names))
		return -ENOENT;

	ret = profile_update(i);
	if (ret)
		return ret;

	return count;
}

static ssize_t
available_profiles_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	int i;
	ssize_t n = 0;
	const char *name;

	for (i = 0; i < ARRAY_SIZE(profile_names); i++) {
		name = profile_names[i];
		if ((n + strlen(name) + 2) > PAGE_SIZE)
			break;
		n += sprintf(&buf[n], "%s\n", name);
	}

	return n;
}

static struct kobj_attribute profile_attribute =
	__ATTR(profile, 0644, core_edp_profile_show, core_edp_profile_store);
static struct kobj_attribute available_profiles_attribute =
	__ATTR_RO(available_profiles);

const struct attribute *core_edp_attributes[] = {
	&profile_attribute.attr,
	&available_profiles_attribute.attr,
	NULL,
};

static struct kobject *core_edp_kobj;

/* core edp temperature update */
static int core_edp_get_cdev_max_state(struct thermal_cooling_device *cdev,
				       unsigned long *max_state)
{
	*max_state = limits ? limits->temperature_ranges - 1 : 0;
	return 0;
}

static int core_edp_get_cdev_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long *cur_state)
{
	*cur_state = core_edp_thermal_idx;
	return 0;
}

static int core_edp_set_cdev_state(struct thermal_cooling_device *cdev,
				   unsigned long cur_state)
{
	int ret = 0;
	unsigned long *old_cap_rates;
	unsigned long *new_cap_rates;

	if (!limits) {
		core_edp_thermal_idx = cur_state;
		return 0;
	}

	mutex_lock(&core_edp_lock);

	if (core_edp_thermal_idx != cur_state) {
		old_cap_rates = get_current_cap_rates();
		new_cap_rates = get_cap_rates(
			core_edp_scpu_state, core_edp_profile,
			core_edp_modules_state, cur_state);
		ret = update_cap_rates(new_cap_rates, old_cap_rates);
		/*
		 * Unlike updating other state variables, temperature change
		 * must be always "accepted" (it's already happened) - just
		 * re-try one more time in case of error
		 */
		if (ret)
			update_cap_rates(new_cap_rates, old_cap_rates);

		core_edp_thermal_idx = cur_state;
	}
	mutex_unlock(&core_edp_lock);

	return 0;
}

static struct thermal_cooling_device_ops core_edp_cooling_ops = {
	.get_max_state = core_edp_get_cdev_max_state,
	.get_cur_state = core_edp_get_cdev_cur_state,
	.set_cur_state = core_edp_set_cdev_state,
};

static struct tegra_cooling_device core_edp_cdev;

struct tegra_cooling_device *tegra_core_edp_get_cdev(void)
{
	if (!limits)
		return NULL;

	if (!core_edp_cdev.cdev_type) {
		core_edp_cdev.cdev_type = "core_edp";
		core_edp_cdev.trip_temperatures = limits->temperatures;
		core_edp_cdev.trip_temperatures_num =
			limits->temperature_ranges-1;
	}
	return &core_edp_cdev;
}

/*
 * Since EMC rate on suspend exit is set to boot configuration with no regards
 * to EDP constraints, force profile_favor_emc on suspend entry, and restore
 * suspended profile after resume. This guarantees that other clocks (GPU)
 * are throttled enough to prevent regulator over-current.
 */
static int core_edp_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	int ret = 0;
	if (!limits)
		return NOTIFY_OK;

	mutex_lock(&core_edp_lock);
	if (event == PM_SUSPEND_PREPARE) {
		core_edp_suspended_profile = core_edp_profile;
		ret = _profile_update(CORE_EDP_PROFILE_FAVOR_EMC);
		if (ret)
			pr_err("Core EDP suspend: failed to set %s\n",
			       profile_names[CORE_EDP_PROFILE_FAVOR_EMC]);
		else
			pr_info("Core EDP suspend: set %s\n",
				profile_names[CORE_EDP_PROFILE_FAVOR_EMC]);
	} else if (event == PM_POST_SUSPEND) {
		ret = _profile_update(core_edp_suspended_profile);
		if (ret)
			pr_err("Core EDP resume: failed to restore %s\n",
			       profile_names[core_edp_suspended_profile]);
		else
			pr_info("Core EDP resume: restored %s\n",
				profile_names[core_edp_suspended_profile]);
		core_edp_suspended_profile = CORE_EDP_PROFILES_NUM;
		ret = 0; /* don't stop resume */
	}
	mutex_unlock(&core_edp_lock);
	return notifier_from_errno(ret);
}

static struct notifier_block core_edp_pm_notifier = {
	.notifier_call = core_edp_pm_notify,
};

/* initialize update interfaces */
static int __init tegra_core_edp_late_init(void)
{
	if (!limits)
		return 0;

	/* continue on error - initialized at max temperature, anyway */
	if (IS_ERR_OR_NULL(thermal_cooling_device_register(
		core_edp_cdev.cdev_type, NULL, &core_edp_cooling_ops)))
		pr_err("%s: failed to register edp cooling device\n", __func__);

	/* exit on error - prevent changing profile_favor_emc  */
	if (register_pm_notifier(&core_edp_pm_notifier)) {
		pr_err("%s: failed to register edp pm notifier\n", __func__);
		return 0;
	}

	core_edp_kobj = kobject_create_and_add("tegra_core_edp", kernel_kobj);
	if (!core_edp_kobj) {
		pr_err("%s: failed to create edp sysfs object\n", __func__);
		return 0;
	}

	if (sysfs_create_files(core_edp_kobj, core_edp_attributes)) {
		pr_err("%s: failed to create edp profile sysfs interface\n",
		       __func__);
		return 0;
	}
	pr_info("Core EDP sysfs interface is initialized\n");

	return 0;
}
late_initcall(tegra_core_edp_late_init);

#ifdef CONFIG_DEBUG_FS

static int edp_table_show(struct seq_file *s, void *data)
{
	int i, j, k, l;
	unsigned long *cap_rates;

	seq_printf(s, "VDD_CORE EDP TABLE (cap rates in kHz)\n");

	seq_printf(s, "%10s", " Temp.");
	for (l = 0; l < limits->cap_clocks_num; l++)
		seq_printf(s, "%10s", limits->cap_clocks[l]->name);
	seq_printf(s, "\n");
	for (l = 0; l < 10+10*limits->cap_clocks_num; l++)
		seq_printf(s, "-");
	seq_printf(s, "\n");

	seq_printf(s, "SCPU ON\n");
	for (i = 0; i < CORE_EDP_PROFILES_NUM; i++) {
		seq_printf(s, "%-19s%d\n", profile_names[i], i);
		for (j = 0; j < limits->core_modules_states; j++) {
			seq_printf(s, "%-19s%d\n", "modules_state", j);
			for (k = 0; k < limits->temperature_ranges; k++) {
				seq_printf(s, "%8dC:", limits->temperatures[k]);
				cap_rates = get_cap_rates(true, i, j, k);
				for (l = 0; l < limits->cap_clocks_num; l++)
					seq_printf(s, "%10lu",
						   cap_rates[l]/1000);
				seq_printf(s, "\n");
			}
		}
	}

	seq_printf(s, "SCPU OFF\n");
	for (i = 0; i < CORE_EDP_PROFILES_NUM; i++) {
		seq_printf(s, "%-19s%d\n", profile_names[i], i);
		for (j = 0; j < limits->core_modules_states; j++) {
			seq_printf(s, "%-19s%d\n", "modules_state", j);
			for (k = 0; k < limits->temperature_ranges; k++) {
				seq_printf(s, "%8dC:", limits->temperatures[k]);
				cap_rates = get_cap_rates(false, i, j, k);
				for (l = 0; l < limits->cap_clocks_num; l++)
					seq_printf(s, "%10lu",
						   cap_rates[l]/1000);
				seq_printf(s, "\n");
			}
		}
	}

	return 0;
}
static int edp_table_open(struct inode *inode, struct file *file)
{
	return single_open(file, edp_table_show, inode->i_private);
}
static const struct file_operations edp_table_fops = {
	.open		= edp_table_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int profile_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%s\n", profile_names[core_edp_profile]);
	return 0;
}
static int profile_open(struct inode *inode, struct file *file)
{
	return single_open(file, profile_show, inode->i_private);
}
static const struct file_operations profile_fops = {
	.open		= profile_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int range_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%d\n", limits->temperatures[core_edp_thermal_idx]);
	return 0;
}
static int range_open(struct inode *inode, struct file *file)
{
	return single_open(file, range_show, inode->i_private);
}
static const struct file_operations range_fops = {
	.open		= range_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int rates_show(struct seq_file *s, void *data)
{
	int i;
	unsigned long *cap_rates;

	mutex_lock(&core_edp_lock);
	cap_rates = get_current_cap_rates();
	mutex_unlock(&core_edp_lock);

	for (i = 0; i < limits->cap_clocks_num; i++)
		seq_printf(s, "%-10srate (kHz): %lu\n",
			   limits->cap_clocks[i]->name, cap_rates[i] / 1000);
	return 0;
}
static int rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, rates_show, inode->i_private);
}
static const struct file_operations rates_fops = {
	.open		= rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int disable_edp_get(void *data, u64 *val)
{
	*val = core_edp_disabled;
	return 0;
}
static int disable_edp_set(void *data, u64 val)
{
	int ret;
	unsigned long *cap_rates;
	bool disable = val ? true : false;

	if (!limits) {
		core_edp_disabled = disable;
		return 0;
	}

	mutex_lock(&core_edp_lock);

	if (core_edp_disabled != disable) {
		if (disable) {
			ret = set_max_rates();
			core_edp_disabled = true;
		} else {
			core_edp_disabled = false;
			cap_rates = get_current_cap_rates();
			ret = set_cap_rates(cap_rates);
		}
		pr_info("Core EDP %s%s\n", disable ? "disabled" : "enabled",
			ret ? " with incomplete limits" : "");
	}
	mutex_unlock(&core_edp_lock);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(disable_edp_fops,
			disable_edp_get, disable_edp_set, "%llu\n");

int __init tegra_core_edp_debugfs_init(struct dentry *edp_dir)
{
	struct dentry *dir, *d;

	if (!limits)
		return 0;

	dir = debugfs_create_dir("vdd_core", edp_dir);
	if (!dir)
		return -ENOMEM;

	d = debugfs_create_file("edp", S_IRUGO, dir, NULL, &edp_table_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_bool("scpu_state", S_IRUGO, dir,
				(u32 *)&core_edp_scpu_state);
	if (!d)
		goto err_out;

	d = debugfs_create_file("profile", S_IRUGO, dir, NULL, &profile_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("modules_state", S_IRUGO, dir,
			       (u32 *)&core_edp_modules_state);
	if (!d)
		goto err_out;

	d = debugfs_create_file("therm_range", S_IRUGO, dir, NULL, &range_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file("rates", S_IRUGO, dir, NULL, &rates_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file("disable_edp", S_IRUGO | S_IWUSR, dir, NULL,
				&disable_edp_fops);
	if (!d)
		goto err_out;
	return 0;

err_out:
	debugfs_remove_recursive(dir);
	return -ENOMEM;


}

#endif
