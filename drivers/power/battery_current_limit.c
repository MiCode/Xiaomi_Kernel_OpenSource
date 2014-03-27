/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/cpufreq.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/cpu.h>

#define BCL_DEV_NAME "battery_current_limit"
#define BCL_NAME_LENGTH 20
/*
 * Default BCL poll interval 1000 msec
 */
#define BCL_POLL_INTERVAL 1000
/*
 * Mininum BCL poll interval 10 msec
 */
#define MIN_BCL_POLL_INTERVAL 10
#define BATTERY_VOLTAGE_MIN 3400
#define BTM_8084_FREQ_MITIG_LIMIT 1958400

/*
 * Battery Current Limit Enable or Not
 */
enum bcl_device_mode {
	BCL_DEVICE_DISABLED = 0,
	BCL_DEVICE_ENABLED,
};

/*
 * Battery Current Limit Iavail Threshold Mode set
 */
enum bcl_iavail_threshold_mode {
	BCL_IAVAIL_THRESHOLD_DISABLED = 0,
	BCL_IAVAIL_THRESHOLD_ENABLED,
};

/*
 * Battery Current Limit Iavail Threshold Mode
 */
enum bcl_iavail_threshold_type {
	BCL_LOW_THRESHOLD_TYPE = 0,
	BCL_HIGH_THRESHOLD_TYPE,
	BCL_THRESHOLD_TYPE_MAX,
};

enum bcl_monitor_type {
	BCL_IAVAIL_MONITOR_TYPE,
	BCL_IBAT_MONITOR_TYPE,
	BCL_MONITOR_TYPE_MAX,
};

static const char *bcl_type[BCL_MONITOR_TYPE_MAX] = {"bcl", "btm"};
int adc_timer_val_usec[] = {
	[ADC_MEAS1_INTERVAL_0MS] = 0,
	[ADC_MEAS1_INTERVAL_1P0MS] = 1000,
	[ADC_MEAS1_INTERVAL_2P0MS] = 2000,
	[ADC_MEAS1_INTERVAL_3P9MS] = 3900,
	[ADC_MEAS1_INTERVAL_7P8MS] = 7800,
	[ADC_MEAS1_INTERVAL_15P6MS] = 15600,
	[ADC_MEAS1_INTERVAL_31P3MS] = 31300,
	[ADC_MEAS1_INTERVAL_62P5MS] = 62500,
	[ADC_MEAS1_INTERVAL_125MS] = 125000,
	[ADC_MEAS1_INTERVAL_250MS] = 250000,
	[ADC_MEAS1_INTERVAL_500MS] = 500000,
	[ADC_MEAS1_INTERVAL_1S] = 1000000,
	[ADC_MEAS1_INTERVAL_2S] = 2000000,
	[ADC_MEAS1_INTERVAL_4S] = 4000000,
	[ADC_MEAS1_INTERVAL_8S] = 8000000,
	[ADC_MEAS1_INTERVAL_16S] = 16000000,
};

/**
 * BCL control block
 *
 */
struct bcl_context {
	/* BCL device */
	struct device *dev;

	/* BCL related config parameter */
	/* BCL mode enable or not */
	enum bcl_device_mode bcl_mode;
	/* BCL monitoring Iavail or Ibat */
	enum bcl_monitor_type bcl_monitor_type;
	/* BCL Iavail Threshold Activate or Not */
	enum bcl_iavail_threshold_mode
				bcl_threshold_mode[BCL_THRESHOLD_TYPE_MAX];
	/* BCL Iavail Threshold value in milli Amp */
	int bcl_threshold_value_ma[BCL_THRESHOLD_TYPE_MAX];
	/* BCL Type */
	char bcl_type[BCL_NAME_LENGTH];
	/* BCL poll in msec */
	int bcl_poll_interval_msec;

	/* BCL realtime value based on poll */
	/* BCL realtime vbat in mV*/
	int bcl_vbat_mv;
	/* BCL realtime rbat in mOhms*/
	int bcl_rbat_mohm;
	/*BCL realtime iavail in milli Amp*/
	int bcl_iavail;
	/*BCL vbatt min in mV*/
	int bcl_vbat_min;
	/* BCL period poll delay work structure  */
	struct delayed_work bcl_iavail_work;
	/* The max CPU frequency the BTM restricts during high load */
	uint32_t btm_freq_max;
	/* Indicates whether there is a high load */
	bool btm_high_load;
	/* battery current high load clr threshold */
	int btm_low_threshold_uv;
	/* battery current high load threshold */
	int btm_high_threshold_uv;
	/* ADC battery current polling timer interval */
	enum qpnp_adc_meas_timer_1 btm_adc_interval;
	/* ADC config parameters */
	struct qpnp_adc_tm_chip *btm_adc_tm_dev;
	struct qpnp_vadc_chip *btm_vadc_dev;
	int btm_ibat_chan;
	struct qpnp_adc_tm_btm_param btm_adc_param;
	uint32_t btm_uv_to_ua_numerator;
	uint32_t btm_uv_to_ua_denominator;
};

static struct bcl_context *gbcl;

static int bcl_cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;

	switch (event) {
	case CPUFREQ_INCOMPATIBLE:
		if (gbcl->btm_high_load) {
			pr_debug("Requesting Max freq:%d for CPU%d\n",
				gbcl->btm_freq_max, policy->cpu);
			cpufreq_verify_within_limits(policy, 0,
				gbcl->btm_freq_max);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block bcl_cpufreq_notifier = {
	.notifier_call = bcl_cpufreq_callback,
};

static void update_cpu_freq(void)
{
	int cpu, ret = 0;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		ret = cpufreq_update_policy(cpu);
		if (ret)
			pr_err("Error updating policy for CPU%d. ret:%d\n",
				cpu, ret);
	}
	put_online_cpus();
}

static int bcl_get_battery_voltage(int *vbatt_mv)
{
	static struct power_supply *psy;
	union power_supply_propval ret = {0,};

	if (psy == NULL) {
		psy = power_supply_get_by_name("battery");
		if (psy  == NULL) {
			pr_err("failed to get ps battery\n");
			return -EINVAL;
		}
	}

	if (psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret))
		return -EINVAL;

	if (ret.intval <= 0)
		return -EINVAL;

	*vbatt_mv = ret.intval / 1000;
	return 0;
}


static int bcl_get_resistance(int *rbatt_mohm)
{
	static struct power_supply *psy;
	union power_supply_propval ret = {0,};

	if (psy == NULL) {
		psy = power_supply_get_by_name("bms");
		if (psy == NULL) {
			pr_err("failed to get ps bms\n");
			return -EINVAL;
		}
	}
	if (psy->get_property(psy, POWER_SUPPLY_PROP_RESISTANCE, &ret))
		return -EINVAL;

	if (ret.intval <= 0)
		return -EINVAL;

	*rbatt_mohm = ret.intval / 1000;

	return 0;
}

/*
 * BCL iavail calculation and trigger notification to user space
 * if iavail cross threshold
 */
static void bcl_calculate_iavail_trigger(void)
{
	int iavail_ma = 0;
	int vbatt_mv;
	int rbatt_mohm;
	bool threshold_cross = false;

	if (!gbcl) {
		pr_err("called before initialization\n");
		return;
	}

	if (bcl_get_battery_voltage(&vbatt_mv))
		return;

	if (bcl_get_resistance(&rbatt_mohm))
		return;

	iavail_ma = (vbatt_mv - gbcl->bcl_vbat_min) * 1000 / rbatt_mohm;

	gbcl->bcl_rbat_mohm = rbatt_mohm;
	gbcl->bcl_vbat_mv = vbatt_mv;
	gbcl->bcl_iavail = iavail_ma;

	pr_debug("iavail %d, vbatt %d rbatt %d\n", iavail_ma, vbatt_mv,
			rbatt_mohm);

	if ((gbcl->bcl_threshold_mode[BCL_HIGH_THRESHOLD_TYPE] ==
				BCL_IAVAIL_THRESHOLD_ENABLED)
		&& (iavail_ma >=
		gbcl->bcl_threshold_value_ma[BCL_HIGH_THRESHOLD_TYPE]))
		threshold_cross = true;
	else if ((gbcl->bcl_threshold_mode[BCL_LOW_THRESHOLD_TYPE]
				== BCL_IAVAIL_THRESHOLD_ENABLED)
		&& (iavail_ma <=
		gbcl->bcl_threshold_value_ma[BCL_LOW_THRESHOLD_TYPE]))
		threshold_cross = true;

	if (threshold_cross)
		sysfs_notify(&gbcl->dev->kobj, NULL, "type");
}

/*
 * BCL iavail work
 */
static void bcl_iavail_work(struct work_struct *work)
{
	struct bcl_context *bcl = container_of(work,
			struct bcl_context, bcl_iavail_work.work);

	if (gbcl->bcl_mode == BCL_DEVICE_ENABLED) {
		bcl_calculate_iavail_trigger();
		/* restart the delay work for caculating imax */
		schedule_delayed_work(&bcl->bcl_iavail_work,
			msecs_to_jiffies(bcl->bcl_poll_interval_msec));
	}
}

static void bcl_ibat_notification(enum qpnp_tm_state state, void *ctx);
static int bcl_config_btm(struct bcl_context *bcl,
			enum bcl_iavail_threshold_type thresh_type);

static int current_to_voltage(struct bcl_context *bcl, int ua)
{
	return DIV_ROUND_CLOSEST(ua * bcl->btm_uv_to_ua_denominator,
			bcl->btm_uv_to_ua_numerator);
}

static int voltage_to_current(struct bcl_context *bcl, int uv)
{
	return DIV_ROUND_CLOSEST(uv * bcl->btm_uv_to_ua_numerator,
			bcl->btm_uv_to_ua_denominator);
}

static int adc_time_to_uSec(struct bcl_context *bcl,
		enum qpnp_adc_meas_timer_1 t)
{
	return adc_timer_val_usec[t];
}

static int uSec_to_adc_time(struct bcl_context *bcl, int us)
{
	int i;

	for (i = ARRAY_SIZE(adc_timer_val_usec) - 1;
		i >= 0 && adc_timer_val_usec[i] > us; i--)
		;

	/* disallow continous mode */
	if (i <= 0)
		return -EINVAL;

	return i;
}

/*
 * Set BCL mode
 */
static void bcl_mode_set(enum bcl_device_mode mode)
{
	int ret = 0;

	if (!gbcl)
		return;
	if (gbcl->bcl_mode == mode)
		return;

	gbcl->bcl_mode = mode;
	switch (gbcl->bcl_monitor_type) {
	case BCL_IAVAIL_MONITOR_TYPE:
		if (mode == BCL_DEVICE_ENABLED)
			schedule_delayed_work(&gbcl->bcl_iavail_work, 0);
		else
			cancel_delayed_work_sync(&(gbcl->bcl_iavail_work));
		break;
	case BCL_IBAT_MONITOR_TYPE:
		if (mode == BCL_DEVICE_ENABLED) {
			gbcl->btm_high_load = false;
			ret = cpufreq_register_notifier(&bcl_cpufreq_notifier,
				CPUFREQ_POLICY_NOTIFIER);
			if (ret < 0) {
				pr_err("Err with cpufreq register err:%d\n",
						ret);
				gbcl->bcl_mode = BCL_DEVICE_DISABLED;
				return;
			}
			bcl_config_btm(gbcl, BCL_HIGH_THRESHOLD_TYPE);
		} else {
			ret = qpnp_adc_tm_disable_chan_meas(
				gbcl->btm_adc_tm_dev, &gbcl->btm_adc_param);
			if (ret) {
				pr_err("Error disabling ADC. err:%d\n", ret);
				gbcl->bcl_mode = BCL_DEVICE_ENABLED;
				return;
			}
			gbcl->btm_high_load = false;
			update_cpu_freq();
			ret = cpufreq_unregister_notifier(&bcl_cpufreq_notifier,
				CPUFREQ_POLICY_NOTIFIER);
			if (ret < 0) {
				pr_err("Err with cpufreq unregister err:%d\n",
						ret);
				return;
			}
		}
		break;
	default:
		pr_err("Invalid monitor type:%d\n", gbcl->bcl_monitor_type);
		break;
	}

	return;
}

#define show_bcl(name, variable, format) \
static ssize_t \
name##_show(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	if (gbcl) \
		return snprintf(buf, PAGE_SIZE, format, gbcl->variable); \
	else \
		return  -EPERM; \
}

show_bcl(type, bcl_type, "%s\n")
show_bcl(vbat, bcl_vbat_mv, "%d\n")
show_bcl(rbat, bcl_rbat_mohm, "%d\n")
show_bcl(iavail, bcl_iavail, "%d\n")
show_bcl(vbat_min, bcl_vbat_min, "%d\n");
show_bcl(poll_interval, bcl_poll_interval_msec, "%d\n")
show_bcl(high_load_state, btm_high_load, "%d\n")

static ssize_t
mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%s\n",
		gbcl->bcl_mode == BCL_DEVICE_ENABLED ? "enabled"
			: "disabled");
}

static ssize_t
mode_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (!gbcl)
		return -EPERM;

	if (!strcmp(buf, "enable"))
		bcl_mode_set(BCL_DEVICE_ENABLED);
	else if (!strcmp(buf, "disable"))
		bcl_mode_set(BCL_DEVICE_DISABLED);
	else
		return -EINVAL;

	return count;
}

static ssize_t
poll_interval_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int value = 0;

	if (!gbcl)
		return -EPERM;

	if (!sscanf(buf, "%d", &value))
		return -EINVAL;

	if (value < MIN_BCL_POLL_INTERVAL)
		return -EINVAL;

	gbcl->bcl_poll_interval_msec = value;

	return count;
}

static ssize_t vbat_min_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int value = 0;
	int ret = 0;

	if (!gbcl)
		return -EPERM;

	ret = kstrtoint(buf, 10, &value);

	if (ret || (value < 0)) {
		pr_err("Incorrect vbatt min value\n");
		return -EINVAL;
	}

	gbcl->bcl_vbat_min = value;
	return count;
}

static ssize_t iavail_low_threshold_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%s\n",
		gbcl->bcl_threshold_mode[BCL_LOW_THRESHOLD_TYPE]
		== BCL_IAVAIL_THRESHOLD_ENABLED ? "enabled" : "disabled");
}

static ssize_t iavail_low_threshold_mode_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!gbcl)
		return -EPERM;

	if (!strcmp(buf, "enable"))
		gbcl->bcl_threshold_mode[BCL_LOW_THRESHOLD_TYPE]
			= BCL_IAVAIL_THRESHOLD_ENABLED;
	else if (!strcmp(buf, "disable"))
		gbcl->bcl_threshold_mode[BCL_LOW_THRESHOLD_TYPE]
			= BCL_IAVAIL_THRESHOLD_DISABLED;
	else
		return -EINVAL;

	return count;
}
static ssize_t iavail_high_threshold_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%s\n",
		gbcl->bcl_threshold_mode[BCL_HIGH_THRESHOLD_TYPE]
		== BCL_IAVAIL_THRESHOLD_ENABLED ? "enabled" : "disabled");
}

static ssize_t iavail_high_threshold_mode_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!gbcl)
		return -EPERM;

	if (!strcmp(buf, "enable"))
		gbcl->bcl_threshold_mode[BCL_HIGH_THRESHOLD_TYPE]
			= BCL_IAVAIL_THRESHOLD_ENABLED;
	else if (!strcmp(buf, "disable"))
		gbcl->bcl_threshold_mode[BCL_HIGH_THRESHOLD_TYPE]
			= BCL_IAVAIL_THRESHOLD_DISABLED;
	else
		return -EINVAL;

	return count;
}

static ssize_t iavail_low_threshold_value_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%d\n",
		gbcl->bcl_threshold_value_ma[BCL_LOW_THRESHOLD_TYPE]);
}


static ssize_t iavail_low_threshold_value_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	if (!gbcl)
		return -EPERM;

	ret = kstrtoint(buf, 10, &val);

	if (ret || (val < 0)) {
		pr_err("Incorrect available current threshold value\n");
		return -EINVAL;
	}

	gbcl->bcl_threshold_value_ma[BCL_LOW_THRESHOLD_TYPE] = val;

	return count;
}
static ssize_t iavail_high_threshold_value_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%d\n",
		gbcl->bcl_threshold_value_ma[BCL_HIGH_THRESHOLD_TYPE]);
}

static ssize_t iavail_high_threshold_value_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	if (!gbcl)
		return -EPERM;
	ret = kstrtoint(buf, 10, &val);

	if (ret || (val < 0)) {
		pr_err("Incorrect available current threshold value\n");
		return -EINVAL;
	}

	gbcl->bcl_threshold_value_ma[BCL_HIGH_THRESHOLD_TYPE] = val;

	return count;
}

static ssize_t high_ua_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%d\n",
		voltage_to_current(gbcl, gbcl->btm_high_threshold_uv));
}

static ssize_t high_ua_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	if (!gbcl)
		return -EPERM;
	if (gbcl->bcl_mode != BCL_DEVICE_DISABLED) {
		pr_err("BCL is not disabled\n");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &val);

	if (ret || (val < 0)) {
		pr_err("Invalid high threshold %s val:%d ret:%d\n", buf,
			val, ret);
		return -EINVAL;
	}

	gbcl->btm_high_threshold_uv = current_to_voltage(gbcl, val);

	return count;
}

static ssize_t low_ua_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%d\n",
		voltage_to_current(gbcl, gbcl->btm_low_threshold_uv));
}

static ssize_t low_ua_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	if (!gbcl)
		return -EPERM;
	if (gbcl->bcl_mode != BCL_DEVICE_DISABLED) {
		pr_err("BCL is not disabled\n");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &val);
	if (ret || (val < 0)) {
		pr_err("Invalid low threshold %s val:%d ret:%d\n", buf,
			val, ret);
		return -EINVAL;
	}

	gbcl->btm_low_threshold_uv = current_to_voltage(gbcl, val);

	return count;
}

static ssize_t adc_interval_us_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			adc_time_to_uSec(gbcl, gbcl->btm_adc_interval));
}

static ssize_t freq_max_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	if (!gbcl)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%d\n", gbcl->btm_freq_max);
}

static ssize_t freq_max_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	if (!gbcl)
		return -EPERM;
	if (gbcl->bcl_mode != BCL_DEVICE_DISABLED) {
		pr_err("BCL is not disabled\n");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &val);
	if (ret || (val < 0)) {
		pr_err("Incorrect value %s val:%d. ret:%d\n", buf, val, ret);
		return -EINVAL;
	}
	gbcl->btm_freq_max = (val >= BTM_8084_FREQ_MITIG_LIMIT) ?
				val : BTM_8084_FREQ_MITIG_LIMIT;

	return count;
}

/*
 * BCL device attributes
 */
static struct device_attribute bcl_dev_attr[] = {
	__ATTR(type, 0444, type_show, NULL),
	__ATTR(iavail, 0444, iavail_show, NULL),
	__ATTR(vbat_min, 0644, vbat_min_show, vbat_min_store),
	__ATTR(vbat, 0444, vbat_show, NULL),
	__ATTR(rbat, 0444, rbat_show, NULL),
	__ATTR(mode, 0644, mode_show, mode_store),
	__ATTR(poll_interval, 0644,
		poll_interval_show, poll_interval_store),
	__ATTR(iavail_low_threshold_mode, 0644,
		iavail_low_threshold_mode_show,
		iavail_low_threshold_mode_store),
	__ATTR(iavail_high_threshold_mode, 0644,
		iavail_high_threshold_mode_show,
		iavail_high_threshold_mode_store),
	__ATTR(iavail_low_threshold_value, 0644,
		iavail_low_threshold_value_show,
		iavail_low_threshold_value_store),
	__ATTR(iavail_high_threshold_value, 0644,
		iavail_high_threshold_value_show,
		iavail_high_threshold_value_store),
};

static struct device_attribute btm_dev_attr[] = {
	__ATTR(type, 0444, type_show, NULL),
	__ATTR(mode, 0644, mode_show, mode_store),
	__ATTR(high_load_state, 0444, high_load_state_show, NULL),
	__ATTR(high_threshold_ua, 0644, high_ua_show, high_ua_store),
	__ATTR(low_threshold_ua, 0644, low_ua_show, low_ua_store),
	__ATTR(adc_interval_us, 0444,
		adc_interval_us_show, NULL),
	__ATTR(freq_max, 0644,
		freq_max_show,
		freq_max_store),
};

static int create_bcl_sysfs(struct bcl_context *bcl)
{
	int result = 0, num_attr = 0, i;
	struct device_attribute *attr_ptr = NULL;

	switch (bcl->bcl_monitor_type) {
	case BCL_IAVAIL_MONITOR_TYPE:
		num_attr = sizeof(bcl_dev_attr)/sizeof(struct device_attribute);
		attr_ptr = bcl_dev_attr;
		break;
	case BCL_IBAT_MONITOR_TYPE:
		num_attr = sizeof(btm_dev_attr)/sizeof(struct device_attribute);
		attr_ptr = btm_dev_attr;
		break;
	default:
		pr_err("Invalid monitor type:%d\n", bcl->bcl_monitor_type);
		return -EINVAL;
	}

	for (i = 0; i < num_attr; i++) {
		result = device_create_file(bcl->dev, &attr_ptr[i]);
		if (result < 0)
			return result;
	}

	return result;
}

static void remove_bcl_sysfs(struct bcl_context *bcl)
{
	int num_attr = 0, i;
	struct device_attribute *attr_ptr = NULL;

	switch (bcl->bcl_monitor_type) {
	case BCL_IAVAIL_MONITOR_TYPE:
		num_attr = sizeof(bcl_dev_attr)/sizeof(struct device_attribute);
		attr_ptr = bcl_dev_attr;
		break;
	case BCL_IBAT_MONITOR_TYPE:
		num_attr = sizeof(btm_dev_attr)/sizeof(struct device_attribute);
		attr_ptr = btm_dev_attr;
		break;
	default:
		pr_err("Invalid monitor type:%d\n", bcl->bcl_monitor_type);
		return;
	}

	for (i = 0; i < num_attr; i++)
		device_remove_file(bcl->dev, &attr_ptr[i]);

	return;
}

static int bcl_config_btm(struct bcl_context *bcl,
		enum bcl_iavail_threshold_type thresh_type)
{
	int ret = 0;

	if (bcl->bcl_mode == BCL_DEVICE_DISABLED ||
		bcl->bcl_monitor_type != BCL_IBAT_MONITOR_TYPE ||
		bcl->btm_ibat_chan == -EINVAL)
		return -EINVAL;

	switch (thresh_type) {
	case BCL_HIGH_THRESHOLD_TYPE:
		bcl->btm_adc_param.state_request = ADC_TM_HIGH_THR_ENABLE;
		break;
	case BCL_LOW_THRESHOLD_TYPE:
		bcl->btm_adc_param.state_request = ADC_TM_LOW_THR_ENABLE;
		break;
	default:
		pr_err("Invalid threshold type:%d\n", thresh_type);
		return -EINVAL;
	}
	bcl->btm_adc_param.low_thr = bcl->btm_low_threshold_uv;
	bcl->btm_adc_param.high_thr = bcl->btm_high_threshold_uv;
	bcl->btm_adc_param.timer_interval = bcl->btm_adc_interval;
	bcl->btm_adc_param.btm_ctx = bcl;
	bcl->btm_adc_param.threshold_notification = bcl_ibat_notification;
	bcl->btm_adc_param.channel = bcl->btm_ibat_chan;

	ret = qpnp_adc_tm_channel_measure(bcl->btm_adc_tm_dev,
			&bcl->btm_adc_param);
	if (ret < 0)
		pr_err("Error configuring BTM. ret:%d\n", ret);
	else
		pr_debug("BTM config. poll:%d high_uv:%d(%s) low_uv:%d(%s)\n",
		    bcl->btm_adc_interval,
		    bcl->btm_adc_param.high_thr,
		    (bcl->btm_adc_param.state_request ==
			ADC_TM_HIGH_THR_ENABLE) ? "enabled" : "disabled",
		    bcl->btm_adc_param.low_thr,
		    (bcl->btm_adc_param.state_request ==
			ADC_TM_LOW_THR_ENABLE) ? "enabled" : "disabled");
	return ret;
}

static void bcl_ibat_notification(enum qpnp_tm_state state, void *ctx)
{
	struct bcl_context *bcl = ctx;
	int ret = 0;

	if (bcl->btm_high_load && state == ADC_TM_LOW_STATE) {
		pr_debug("low load enter\n");
		bcl->btm_high_load = false;
		update_cpu_freq();
	} else if (!bcl->btm_high_load && state == ADC_TM_HIGH_STATE) {
		pr_debug("high load enter\n");
		bcl->btm_high_load = true;
		update_cpu_freq();
	}
	ret = bcl_config_btm(bcl, (state == ADC_TM_LOW_STATE) ?
		BCL_HIGH_THRESHOLD_TYPE : BCL_LOW_THRESHOLD_TYPE);
	if (ret < 0)
		pr_err("Error configuring %s thresh. err:%d\n",
			(state == ADC_TM_LOW_STATE) ? "high" : "low", ret);
}

static int bcl_suspend(struct device *dev)
{
	int ret = 0;
	struct bcl_context *bcl = dev_get_drvdata(dev);

	if (bcl->bcl_monitor_type == BCL_IBAT_MONITOR_TYPE &&
		bcl->bcl_mode == BCL_DEVICE_ENABLED) {
		ret = qpnp_adc_tm_disable_chan_meas(
			bcl->btm_adc_tm_dev, &bcl->btm_adc_param);
		if (ret < 0)
			pr_err("Error disabling btm. ret = %d\n", ret);
		bcl->btm_high_load = false;
	}
	return 0;
}

static int bcl_resume(struct device *dev)
{
	struct bcl_context *bcl = dev_get_drvdata(dev);

	if (bcl->bcl_monitor_type == BCL_IBAT_MONITOR_TYPE &&
		bcl->bcl_mode == BCL_DEVICE_ENABLED) {
		bcl->btm_high_load = false;
		bcl_config_btm(bcl, BCL_HIGH_THRESHOLD_TYPE);
	}
	return 0;
}

static int probe_btm_properties(struct bcl_context *bcl)
{
	int ret = 0, curr_ua = 0;
	int adc_interval_us;
	struct device_node *ibat_node = NULL, *dev_node = bcl->dev->of_node;
	char *key = NULL;

	key = "qcom,ibat-monitor";
	ibat_node = of_find_node_by_name(dev_node, key);
	if (!ibat_node) {
		ret = -ENODEV;
		goto btm_probe_exit;
	}

	key = "uv-to-ua-numerator";
	ret = of_property_read_u32(ibat_node, key,
			&bcl->btm_uv_to_ua_numerator);
	if (ret < 0)
		goto btm_probe_exit;

	key = "uv-to-ua-denominator";
	ret = of_property_read_u32(ibat_node, key,
			&bcl->btm_uv_to_ua_denominator);
	if (ret < 0)
		goto btm_probe_exit;

	key = "low-threshold-uamp";
	ret = of_property_read_u32(ibat_node, key, &curr_ua);
	if (ret < 0)
		goto btm_probe_exit;
	bcl->btm_low_threshold_uv = current_to_voltage(bcl, curr_ua);

	key = "high-threshold-uamp";
	ret = of_property_read_u32(ibat_node, key, &curr_ua);
	if (ret < 0)
		goto btm_probe_exit;
	bcl->btm_high_threshold_uv = current_to_voltage(bcl, curr_ua);

	key = "mitigation-freq-khz";
	ret = of_property_read_u32(ibat_node, key, &bcl->btm_freq_max);
	if (ret < 0)
		goto btm_probe_exit;
	bcl->btm_freq_max = (bcl->btm_freq_max >= BTM_8084_FREQ_MITIG_LIMIT) ?
				bcl->btm_freq_max : BTM_8084_FREQ_MITIG_LIMIT;

	key = "ibat-channel";
	ret = of_property_read_u32(ibat_node, key, &bcl->btm_ibat_chan);
	if (ret < 0)
		goto btm_probe_exit;

	key = "adc-interval-usec";
	ret = of_property_read_u32(ibat_node, key, &adc_interval_us);
	if (ret < 0)
		goto btm_probe_exit;
	bcl->btm_adc_interval = uSec_to_adc_time(bcl, adc_interval_us);

	key = "ibat-threshold";
	bcl->btm_adc_tm_dev = qpnp_get_adc_tm(bcl->dev, key);
	if (IS_ERR(bcl->btm_adc_tm_dev)) {
		ret = PTR_ERR(bcl->btm_adc_tm_dev);
		goto btm_probe_exit;
	}

	key = "ibat";
	bcl->btm_vadc_dev = qpnp_get_vadc(bcl->dev, key);
	if (IS_ERR(bcl->btm_vadc_dev)) {
		ret = PTR_ERR(bcl->btm_vadc_dev);
		goto btm_probe_exit;
	}

	bcl->btm_high_load = false;
	bcl->bcl_monitor_type = BCL_IBAT_MONITOR_TYPE;
	snprintf(bcl->bcl_type, BCL_NAME_LENGTH, "%s",
			bcl_type[BCL_IBAT_MONITOR_TYPE]);
btm_probe_exit:
	if (ret && ret != -EPROBE_DEFER)
		dev_info(bcl->dev, "%s:%s Error reading key:%s. ret = %d\n",
				KBUILD_MODNAME, __func__, key, ret);

	return ret;
}

static int bcl_probe(struct platform_device *pdev)
{
	struct bcl_context *bcl = NULL;
	int ret = 0;

	bcl = devm_kzalloc(&pdev->dev, sizeof(struct bcl_context), GFP_KERNEL);
	if (!bcl) {
		pr_err("Cannot allocate bcl_context\n");
		return -ENOMEM;
	}

	/* For BCL */
	/* Init default BCL params */
	if (of_property_read_bool(pdev->dev.of_node, "qcom,bcl-enable"))
		bcl->bcl_mode = BCL_DEVICE_ENABLED;
	else
		bcl->bcl_mode = BCL_DEVICE_DISABLED;
	bcl->dev = &pdev->dev;
	bcl->bcl_monitor_type = BCL_IAVAIL_MONITOR_TYPE;
	bcl->bcl_threshold_mode[BCL_LOW_THRESHOLD_TYPE] =
					BCL_IAVAIL_THRESHOLD_DISABLED;
	bcl->bcl_threshold_mode[BCL_HIGH_THRESHOLD_TYPE] =
					BCL_IAVAIL_THRESHOLD_DISABLED;
	bcl->bcl_threshold_value_ma[BCL_LOW_THRESHOLD_TYPE] = 0;
	bcl->bcl_threshold_value_ma[BCL_HIGH_THRESHOLD_TYPE] = 0;
	bcl->bcl_vbat_min = BATTERY_VOLTAGE_MIN;
	snprintf(bcl->bcl_type, BCL_NAME_LENGTH, "%s",
			bcl_type[BCL_IAVAIL_MONITOR_TYPE]);
	bcl->bcl_poll_interval_msec = BCL_POLL_INTERVAL;

	ret = probe_btm_properties(bcl);
	if (ret == -EPROBE_DEFER)
		return ret;

	ret = create_bcl_sysfs(bcl);
	if (ret < 0) {
		pr_err("Cannot create bcl sysfs\n");
		return ret;
	}

	gbcl = bcl;
	platform_set_drvdata(pdev, bcl);
	INIT_DEFERRABLE_WORK(&bcl->bcl_iavail_work, bcl_iavail_work);
	if (bcl->bcl_mode == BCL_DEVICE_ENABLED)
		bcl_mode_set(bcl->bcl_mode);

	return 0;
}

static int bcl_remove(struct platform_device *pdev)
{
	remove_bcl_sysfs(gbcl);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id bcl_match_table[] = {
	{.compatible = "qcom,bcl"},
	{},
};

static const struct dev_pm_ops bcl_pm_ops = {
	.resume         = bcl_resume,
	.suspend        = bcl_suspend,
};

static struct platform_driver bcl_driver = {
	.probe  = bcl_probe,
	.remove = bcl_remove,
	.driver = {
		.name           = BCL_DEV_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = bcl_match_table,
		.pm             = &bcl_pm_ops,
	},
};

static int __init bcl_init(void)
{
	return platform_driver_register(&bcl_driver);
}

static void __exit bcl_exit(void)
{
	platform_driver_unregister(&bcl_driver);
}

late_initcall(bcl_init);
module_exit(bcl_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("battery current limit driver");
MODULE_ALIAS("platform:" BCL_DEV_NAME);
