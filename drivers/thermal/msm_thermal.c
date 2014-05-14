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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/msm_tsens.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/thermal.h>
#include <mach/rpm-regulator.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/msm_thermal_ioctl.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/scm.h>

#define MAX_CURRENT_UA 100000
#define MAX_RAILS 5
#define MAX_THRESHOLD 2
#define MONITOR_ALL_TSENS -1
#define TSENS_NAME_MAX 20
#define TSENS_NAME_FORMAT "tsens_tz_sensor%d"
#define THERM_SECURE_BITE_CMD 8

static struct msm_thermal_data msm_thermal_info;
static struct delayed_work check_temp_work;
static bool core_control_enabled;
static uint32_t cpus_offlined;
static DEFINE_MUTEX(core_control_mutex);
static struct kobject *cc_kobj;
static struct kobject *mx_kobj;
static struct task_struct *hotplug_task;
static struct task_struct *freq_mitigation_task;
static struct task_struct *thermal_monitor_task;
static struct completion hotplug_notify_complete;
static struct completion freq_mitigation_complete;
static struct completion thermal_monitor_complete;

static int enabled;
static int polling_enabled;
static int rails_cnt;
static int psm_rails_cnt;
static int ocr_rail_cnt;
static int limit_idx;
static int limit_idx_low;
static int limit_idx_high;
static int max_tsens_num;
static struct cpufreq_frequency_table *table;
static uint32_t usefreq;
static int freq_table_get;
static bool vdd_rstr_enabled;
static bool vdd_rstr_nodes_called;
static bool vdd_rstr_probed;
static bool psm_enabled;
static bool psm_nodes_called;
static bool psm_probed;
static bool freq_mitigation_enabled;
static bool ocr_enabled;
static bool ocr_nodes_called;
static bool ocr_probed;
static bool ocr_reg_init_defer;
static bool hotplug_enabled;
static bool interrupt_mode_enable;
static bool msm_thermal_probed;
static bool gfx_phase_ctrl_enabled;
static bool cx_phase_ctrl_enabled;
static bool vdd_mx_enabled;
static bool therm_reset_enabled;
static int *tsens_id_map;
static DEFINE_MUTEX(vdd_rstr_mutex);
static DEFINE_MUTEX(psm_mutex);
static DEFINE_MUTEX(cx_mutex);
static DEFINE_MUTEX(gfx_mutex);
static DEFINE_MUTEX(ocr_mutex);
static DEFINE_MUTEX(vdd_mx_mutex);
static uint32_t min_freq_limit;
static uint32_t curr_gfx_band;
static uint32_t curr_cx_band;
static struct kobj_attribute cx_mode_attr;
static struct kobj_attribute gfx_mode_attr;
static struct kobj_attribute mx_enabled_attr;
static struct attribute_group cx_attr_gp;
static struct attribute_group gfx_attr_gp;
static struct attribute_group mx_attr_group;
static struct regulator *vdd_mx;

enum thermal_threshold {
	HOTPLUG_THRESHOLD_HIGH,
	HOTPLUG_THRESHOLD_LOW,
	FREQ_THRESHOLD_HIGH,
	FREQ_THRESHOLD_LOW,
	THRESHOLD_MAX_NR,
};

enum sensor_id_type {
	THERM_ZONE_ID,
	THERM_TSENS_ID,
	THERM_ID_MAX_NR,
};

struct cpu_info {
	uint32_t cpu;
	const char *sensor_type;
	enum sensor_id_type id_type;
	uint32_t sensor_id;
	bool offline;
	bool user_offline;
	bool hotplug_thresh_clear;
	struct sensor_threshold threshold[THRESHOLD_MAX_NR];
	bool max_freq;
	uint32_t user_max_freq;
	uint32_t user_min_freq;
	uint32_t limited_max_freq;
	uint32_t limited_min_freq;
	bool freq_thresh_clear;
};

struct threshold_info;
struct therm_threshold {
	int32_t sensor_id;
	enum sensor_id_type id_type;
	struct sensor_threshold threshold[MAX_THRESHOLD];
	int32_t trip_triggered;
	void (*notify)(struct therm_threshold *);
	struct threshold_info *parent;
};

struct threshold_info {
	uint32_t thresh_ct;
	bool thresh_triggered;
	struct therm_threshold *thresh_list;
};

struct rail {
	const char *name;
	uint32_t freq_req;
	uint32_t min_level;
	uint32_t num_levels;
	int32_t curr_level;
	uint32_t levels[3];
	struct kobj_attribute value_attr;
	struct kobj_attribute level_attr;
	struct regulator *reg;
	struct attribute_group attr_gp;
};

struct psm_rail {
	const char *name;
	uint8_t init;
	uint8_t mode;
	struct kobj_attribute mode_attr;
	struct rpm_regulator *reg;
	struct regulator *phase_reg;
	struct attribute_group attr_gp;
};

enum msm_thresh_list {
	MSM_THERM_RESET,
	MSM_VDD_RESTRICTION,
	MSM_CX_PHASE_CTRL_HOT,
	MSM_GFX_PHASE_CTRL_WARM,
	MSM_GFX_PHASE_CTRL_HOT,
	MSM_OCR,
	MSM_VDD_MX_RESTRICTION,
	MSM_LIST_MAX_NR,
};

enum msm_thermal_phase_ctrl {
	MSM_CX_PHASE_CTRL,
	MSM_GFX_PHASE_CTRL,
	MSM_PHASE_CTRL_NR,
};

enum msm_temp_band {
	MSM_COLD_CRITICAL = 1,
	MSM_COLD,
	MSM_COOL,
	MSM_NORMAL,
	MSM_WARM,
	MSM_HOT,
	MSM_HOT_CRITICAL,
	MSM_TEMP_MAX_NR,
};

static struct psm_rail *psm_rails;
static struct psm_rail *ocr_rails;
static struct rail *rails;
static struct cpu_info cpus[NR_CPUS];
static struct threshold_info *thresh;
static bool mx_restr_applied;

struct vdd_rstr_enable {
	struct kobj_attribute ko_attr;
	uint32_t enabled;
};

/* For SMPS only*/
enum PMIC_SW_MODE {
	PMIC_AUTO_MODE  = RPM_REGULATOR_MODE_AUTO,
	PMIC_IPEAK_MODE = RPM_REGULATOR_MODE_IPEAK,
	PMIC_PWM_MODE   = RPM_REGULATOR_MODE_HPM,
};

enum ocr_request {
	OPTIMUM_CURRENT_MIN,
	OPTIMUM_CURRENT_MAX,
	OPTIMUM_CURRENT_NR,
};

#define VDD_RES_RO_ATTRIB(_rail, ko_attr, j, _name) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0444; \
	ko_attr.show = vdd_rstr_reg_##_name##_show; \
	ko_attr.store = NULL; \
	sysfs_attr_init(&ko_attr.attr); \
	_rail.attr_gp.attrs[j] = &ko_attr.attr;

#define VDD_RES_RW_ATTRIB(_rail, ko_attr, j, _name) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0644; \
	ko_attr.show = vdd_rstr_reg_##_name##_show; \
	ko_attr.store = vdd_rstr_reg_##_name##_store; \
	sysfs_attr_init(&ko_attr.attr); \
	_rail.attr_gp.attrs[j] = &ko_attr.attr;

#define VDD_RSTR_ENABLE_FROM_ATTRIBS(attr) \
	(container_of(attr, struct vdd_rstr_enable, ko_attr));

#define VDD_RSTR_REG_VALUE_FROM_ATTRIBS(attr) \
	(container_of(attr, struct rail, value_attr));

#define VDD_RSTR_REG_LEVEL_FROM_ATTRIBS(attr) \
	(container_of(attr, struct rail, level_attr));

#define OCR_RW_ATTRIB(_rail, ko_attr, j, _name) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0644; \
	ko_attr.show = ocr_reg_##_name##_show; \
	ko_attr.store = ocr_reg_##_name##_store; \
	sysfs_attr_init(&ko_attr.attr); \
	_rail.attr_gp.attrs[j] = &ko_attr.attr;

#define PSM_RW_ATTRIB(_rail, ko_attr, j, _name) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0644; \
	ko_attr.show = psm_reg_##_name##_show; \
	ko_attr.store = psm_reg_##_name##_store; \
	sysfs_attr_init(&ko_attr.attr); \
	_rail.attr_gp.attrs[j] = &ko_attr.attr;

#define PSM_REG_MODE_FROM_ATTRIBS(attr) \
	(container_of(attr, struct psm_rail, mode_attr));

#define PHASE_RW_ATTR(_phase, _name, _attr, j, _attr_gr) \
	_attr.attr.name = __stringify(_name); \
	_attr.attr.mode = 0644; \
	_attr.show = _phase##_phase_show; \
	_attr.store = _phase##_phase_store; \
	sysfs_attr_init(&_attr.attr); \
	_attr_gr.attrs[j] = &_attr.attr;

#define MX_RW_ATTR(ko_attr, _name, _attr_gp) \
	ko_attr.attr.name = __stringify(_name); \
	ko_attr.attr.mode = 0644; \
	ko_attr.show = show_mx_##_name; \
	ko_attr.store = store_mx_##_name; \
	sysfs_attr_init(&ko_attr.attr); \
	_attr_gp.attrs[0] = &ko_attr.attr;

static int  msm_thermal_cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	uint32_t max_freq_req = cpus[policy->cpu].limited_max_freq;
	uint32_t min_freq_req = cpus[policy->cpu].limited_min_freq;

	switch (event) {
	case CPUFREQ_INCOMPATIBLE:
		pr_debug("mitigating CPU%d to freq max: %u min: %u\n",
		policy->cpu, max_freq_req, min_freq_req);

		cpufreq_verify_within_limits(policy, min_freq_req,
			max_freq_req);

		if (max_freq_req < min_freq_req)
			pr_err("Invalid frequency request Max:%u Min:%u\n",
				max_freq_req, min_freq_req);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block msm_thermal_cpufreq_notifier = {
	.notifier_call = msm_thermal_cpufreq_callback,
};

/* If freq table exists, then we can send freq request */
static int check_freq_table(void)
{
	int ret = 0;
	struct cpufreq_frequency_table *table = NULL;

	table = cpufreq_frequency_get_table(0);
	if (!table) {
		pr_debug("error reading cpufreq table\n");
		return -EINVAL;
	}
	freq_table_get = 1;

	return ret;
}

static void update_cpu_freq(int cpu)
{
	int ret = 0;

	if (cpu_online(cpu)) {
		ret = cpufreq_update_policy(cpu);
		if (ret)
			pr_err("Unable to update policy for cpu:%d. err:%d\n",
				cpu, ret);
	}
}

static int update_cpu_min_freq_all(uint32_t min)
{
	uint32_t cpu = 0;
	int ret = 0;

	if (!freq_table_get) {
		ret = check_freq_table();
		if (ret) {
			pr_err("Fail to get freq table. err:%d\n", ret);
			return ret;
		}
	}
	/* If min is larger than allowed max */
	min = min(min, table[limit_idx_high].frequency);

	pr_debug("Requesting min freq:%u for all CPU's\n", min);
	if (freq_mitigation_task) {
		min_freq_limit = min;
		complete(&freq_mitigation_complete);
	} else {
		get_online_cpus();
		for_each_possible_cpu(cpu) {
			cpus[cpu].limited_min_freq = min;
			update_cpu_freq(cpu);
		}
		put_online_cpus();
	}

	return ret;
}

static int vdd_restriction_apply_freq(struct rail *r, int level)
{
	int ret = 0;

	if (level == r->curr_level)
		return ret;

	/* level = -1: disable, level = 0,1,2..n: enable */
	if (level == -1) {
		ret = update_cpu_min_freq_all(r->min_level);
		if (ret)
			return ret;
		else
			r->curr_level = -1;
	} else if (level >= 0 && level < (r->num_levels)) {
		ret = update_cpu_min_freq_all(r->levels[level]);
		if (ret)
			return ret;
		else
			r->curr_level = level;
	} else {
		pr_err("level input:%d is not within range\n", level);
		return -EINVAL;
	}

	return ret;
}

static int vdd_restriction_apply_voltage(struct rail *r, int level)
{
	int ret = 0;

	if (r->reg == NULL) {
		pr_err("%s don't have regulator handle. can't apply vdd\n",
				r->name);
		return -EFAULT;
	}
	if (level == r->curr_level)
		return ret;

	/* level = -1: disable, level = 0,1,2..n: enable */
	if (level == -1) {
		ret = regulator_set_voltage(r->reg, r->min_level,
			r->levels[r->num_levels - 1]);
		if (!ret)
			r->curr_level = -1;
		pr_debug("Requested min level for %s. curr level: %d\n",
				r->name, r->curr_level);
	} else if (level >= 0 && level < (r->num_levels)) {
		ret = regulator_set_voltage(r->reg, r->levels[level],
			r->levels[r->num_levels - 1]);
		if (!ret)
			r->curr_level = level;
		pr_debug("Requesting level %d for %s. curr level: %d\n",
			r->levels[level], r->name, r->levels[r->curr_level]);
	} else {
		pr_err("level input:%d is not within range\n", level);
		return -EINVAL;
	}

	return ret;
}

/* Setting all rails the same mode */
static int psm_set_mode_all(int mode)
{
	int i = 0;
	int fail_cnt = 0;
	int ret = 0;

	pr_debug("Requesting PMIC Mode: %d\n", mode);
	for (i = 0; i < psm_rails_cnt; i++) {
		if (psm_rails[i].mode != mode) {
			ret = rpm_regulator_set_mode(psm_rails[i].reg, mode);
			if (ret) {
				pr_err("Cannot set mode:%d for %s. err:%d",
					mode, psm_rails[i].name, ret);
				fail_cnt++;
			} else
				psm_rails[i].mode = mode;
		}
	}

	return fail_cnt ? (-EFAULT) : ret;
}

static int vdd_rstr_en_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vdd_rstr_enable *en = VDD_RSTR_ENABLE_FROM_ATTRIBS(attr);

	return snprintf(buf, PAGE_SIZE, "%d\n", en->enabled);
}

static ssize_t vdd_rstr_en_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int i = 0;
	uint8_t en_cnt = 0;
	uint8_t dis_cnt = 0;
	uint32_t val = 0;
	struct kernel_param kp;
	struct vdd_rstr_enable *en = VDD_RSTR_ENABLE_FROM_ATTRIBS(attr);

	mutex_lock(&vdd_rstr_mutex);
	kp.arg = &val;
	ret = param_set_bool(buf, &kp);
	if (ret) {
		pr_err("Invalid input %s for enabled\n", buf);
		goto done_vdd_rstr_en;
	}

	if ((val == 0) && (en->enabled == 0))
		goto done_vdd_rstr_en;

	for (i = 0; i < rails_cnt; i++) {
		if (rails[i].freq_req == 1 && freq_table_get)
			ret = vdd_restriction_apply_freq(&rails[i],
					(val) ? 0 : -1);
		else
			ret = vdd_restriction_apply_voltage(&rails[i],
			(val) ? 0 : -1);

		/*
		 * Even if fail to set one rail, still try to set the
		 * others. Continue the loop
		 */
		if (ret)
			pr_err("Set vdd restriction for %s failed\n",
					rails[i].name);
		else {
			if (val)
				en_cnt++;
			else
				dis_cnt++;
		}
	}
	/* As long as one rail is enabled, vdd rstr is enabled */
	if (val && en_cnt)
		en->enabled = 1;
	else if (!val && (dis_cnt == rails_cnt))
		en->enabled = 0;
	pr_debug("%s vdd restriction. curr: %d\n",
			(val) ? "Enable" : "Disable", en->enabled);

done_vdd_rstr_en:
	mutex_unlock(&vdd_rstr_mutex);
	return count;
}

static int send_temperature_band(enum msm_thermal_phase_ctrl phase,
	enum msm_temp_band req_band)
{
	int ret = 0;
	uint32_t msg_id;
	struct msm_rpm_request *rpm_req;
	unsigned int band = req_band;
	uint32_t key, resource, resource_id;

	if (phase < 0 || phase >= MSM_PHASE_CTRL_NR ||
		req_band <= 0 || req_band >= MSM_TEMP_MAX_NR) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto phase_ctrl_exit;
	}
	switch (phase) {
	case MSM_CX_PHASE_CTRL:
		key = msm_thermal_info.cx_phase_request_key;
		break;
	case MSM_GFX_PHASE_CTRL:
		key = msm_thermal_info.gfx_phase_request_key;
		break;
	default:
		goto phase_ctrl_exit;
		break;
	}

	resource = msm_thermal_info.phase_rpm_resource_type;
	resource_id = msm_thermal_info.phase_rpm_resource_id;
	pr_debug("Sending %s temperature band %d\n",
		(phase == MSM_CX_PHASE_CTRL) ? "CX" : "GFX",
		req_band);
	rpm_req = msm_rpm_create_request(MSM_RPM_CTX_ACTIVE_SET,
			resource, resource_id, 1);
	if (!rpm_req) {
		pr_err("Creating RPM request failed\n");
		ret = -ENXIO;
		goto phase_ctrl_exit;
	}

	ret = msm_rpm_add_kvp_data(rpm_req, key, (const uint8_t *)&band,
		(int)sizeof(band));
	if (ret) {
		pr_err("Adding KVP data failed. err:%d\n", ret);
		goto free_rpm_handle;
	}

	msg_id = msm_rpm_send_request(rpm_req);
	if (!msg_id) {
		pr_err("RPM send request failed\n");
		ret = -ENXIO;
		goto free_rpm_handle;
	}

	ret = msm_rpm_wait_for_ack(msg_id);
	if (ret) {
		pr_err("RPM wait for ACK failed. err:%d\n", ret);
		goto free_rpm_handle;
	}

free_rpm_handle:
	msm_rpm_free_request(rpm_req);
phase_ctrl_exit:
	return ret;
}

static uint32_t msm_thermal_str_to_int(const char *inp)
{
	int i, len;
	uint32_t output = 0;

	len = strnlen(inp, sizeof(uint32_t));
	for (i = 0; i < len; i++)
		output |= inp[i] << (i * 8);

	return output;
}

static struct vdd_rstr_enable vdd_rstr_en = {
	.ko_attr.attr.name = __stringify(enabled),
	.ko_attr.attr.mode = 0644,
	.ko_attr.show = vdd_rstr_en_show,
	.ko_attr.store = vdd_rstr_en_store,
	.enabled = 1,
};

static struct attribute *vdd_rstr_en_attribs[] = {
	&vdd_rstr_en.ko_attr.attr,
	NULL,
};

static struct attribute_group vdd_rstr_en_attribs_gp = {
	.attrs  = vdd_rstr_en_attribs,
};

static int vdd_rstr_reg_value_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int val = 0;
	struct rail *reg = VDD_RSTR_REG_VALUE_FROM_ATTRIBS(attr);
	/* -1:disabled, -2:fail to get regualtor handle */
	if (reg->curr_level < 0)
		val = reg->curr_level;
	else
		val = reg->levels[reg->curr_level];

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static int vdd_rstr_reg_level_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct rail *reg = VDD_RSTR_REG_LEVEL_FROM_ATTRIBS(attr);
	return snprintf(buf, PAGE_SIZE, "%d\n", reg->curr_level);
}

static ssize_t vdd_rstr_reg_level_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	struct rail *reg = VDD_RSTR_REG_LEVEL_FROM_ATTRIBS(attr);

	mutex_lock(&vdd_rstr_mutex);
	if (vdd_rstr_en.enabled == 0)
		goto done_store_level;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s for level\n", buf);
		goto done_store_level;
	}

	if (val < 0 || val > reg->num_levels - 1) {
		pr_err(" Invalid number %d for level\n", val);
		goto done_store_level;
	}

	if (val != reg->curr_level) {
		if (reg->freq_req == 1 && freq_table_get)
			update_cpu_min_freq_all(reg->levels[val]);
		else {
			ret = vdd_restriction_apply_voltage(reg, val);
			if (ret) {
				pr_err( \
				"Set vdd restriction for regulator %s failed. err:%d\n",
				reg->name, ret);
				goto done_store_level;
			}
		}
		reg->curr_level = val;
		pr_debug("Request level %d for %s\n",
				reg->curr_level, reg->name);
	}

done_store_level:
	mutex_unlock(&vdd_rstr_mutex);
	return count;
}

static int request_optimum_current(struct psm_rail *rail, enum ocr_request req)
{
	int ret = 0;

	if ((!rail) || (req >= OPTIMUM_CURRENT_NR) ||
		(req < 0)) {
		pr_err("Invalid input %d\n", req);
		ret = -EINVAL;
		goto request_ocr_exit;
	}

	ret = regulator_set_optimum_mode(rail->phase_reg,
		(req == OPTIMUM_CURRENT_MAX) ? MAX_CURRENT_UA : 0);
	if (ret < 0) {
		pr_err("Optimum current request failed. err:%d\n", ret);
		goto request_ocr_exit;
	}
	ret = 0; /*regulator_set_optimum_mode returns the mode on success*/
	pr_debug("Requested optimum current mode: %d\n", req);

request_ocr_exit:
	return ret;
}

static int ocr_set_mode_all(enum ocr_request req)
{
	int ret = 0, i;

	for (i = 0; i < ocr_rail_cnt; i++) {
		if (ocr_rails[i].mode == req)
			continue;
		ret = request_optimum_current(&ocr_rails[i], req);
		if (ret)
			goto ocr_set_mode_exit;
		ocr_rails[i].mode = req;
	}

ocr_set_mode_exit:
	return ret;
}

static ssize_t ocr_reg_mode_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct psm_rail *reg = PSM_REG_MODE_FROM_ATTRIBS(attr);
	return snprintf(buf, PAGE_SIZE, "%d\n", reg->mode);
}

static ssize_t ocr_reg_mode_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct psm_rail *reg = PSM_REG_MODE_FROM_ATTRIBS(attr);

	if (!ocr_enabled)
		return count;

	mutex_lock(&ocr_mutex);
	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s for mode. err:%d\n",
			buf, ret);
		goto done_ocr_store;
	}

	if ((val != OPTIMUM_CURRENT_MAX) &&
		(val != OPTIMUM_CURRENT_MIN)) {
		pr_err("Invalid value %d for mode\n", val);
		goto done_ocr_store;
	}

	if (val != reg->mode) {
		ret = request_optimum_current(reg, val);
		if (ret)
			goto done_ocr_store;
		reg->mode = val;
	}

done_ocr_store:
	mutex_unlock(&ocr_mutex);
	return count;
}

static ssize_t store_phase_request(const char *buf, size_t count, bool is_cx)
{
	int ret = 0, val;
	struct mutex *phase_mutex = (is_cx) ? (&cx_mutex) : (&gfx_mutex);
	enum msm_thermal_phase_ctrl phase_req = (is_cx) ? MSM_CX_PHASE_CTRL :
		MSM_GFX_PHASE_CTRL;

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s for %s temperature band\n",
			buf, (is_cx) ? "CX" : "GFX");
		goto phase_store_exit;
	}
	if ((val <= 0) || (val >= MSM_TEMP_MAX_NR)) {
		pr_err("Invalid input %d for %s temperature band\n",
			val, (is_cx) ? "CX" : "GFX");
		ret = -EINVAL;
		goto phase_store_exit;
	}
	mutex_lock(phase_mutex);
	if (val != ((is_cx) ? curr_cx_band : curr_gfx_band)) {
		ret = send_temperature_band(phase_req, val);
		if (!ret) {
			*((is_cx) ? &curr_cx_band : &curr_gfx_band) = val;
		} else {
			pr_err("Failed to send %d temp. band to %s rail\n", val,
					(is_cx) ? "CX" : "GFX");
			goto phase_store_unlock_exit;
		}
	}
	ret = count;
phase_store_unlock_exit:
	mutex_unlock(phase_mutex);
phase_store_exit:
	return ret;
}

#define show_phase(_name, _variable) \
static ssize_t _name##_phase_show(struct kobject *kobj, \
	struct kobj_attribute *attr, char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%u\n", _variable); \
}

#define store_phase(_name, _variable, _iscx) \
static ssize_t _name##_phase_store(struct kobject *kobj, \
	struct kobj_attribute *attr, const char *buf, size_t count) \
{ \
	return store_phase_request(buf, count, _iscx); \
}

show_phase(gfx, curr_gfx_band)
show_phase(cx, curr_cx_band)
store_phase(gfx, curr_gfx_band, false)
store_phase(cx, curr_cx_band, true)

static int psm_reg_mode_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct psm_rail *reg = PSM_REG_MODE_FROM_ATTRIBS(attr);
	return snprintf(buf, PAGE_SIZE, "%d\n", reg->mode);
}

static ssize_t psm_reg_mode_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct psm_rail *reg = PSM_REG_MODE_FROM_ATTRIBS(attr);

	mutex_lock(&psm_mutex);
	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s for mode\n", buf);
		goto done_psm_store;
	}

	if ((val != PMIC_PWM_MODE) && (val != PMIC_AUTO_MODE)) {
		pr_err("Invalid number %d for mode\n", val);
		goto done_psm_store;
	}

	if (val != reg->mode) {
		ret = rpm_regulator_set_mode(reg->reg, val);
		if (ret) {
			pr_err("Fail to set Mode:%d for %s. err:%d\n",
			val, reg->name, ret);
			goto done_psm_store;
		}
		reg->mode = val;
	}

done_psm_store:
	mutex_unlock(&psm_mutex);
	return count;
}

static int check_sensor_id(int sensor_id)
{
	int i = 0;
	bool hw_id_found = false;
	int ret = 0;

	for (i = 0; i < max_tsens_num; i++) {
		if (sensor_id == tsens_id_map[i]) {
			hw_id_found = true;
			break;
		}
	}
	if (!hw_id_found) {
		pr_err("Invalid sensor hw id:%d\n", sensor_id);
		return -EINVAL;
	}

	return ret;
}

static int create_sensor_id_map(void)
{
	int i = 0;
	int ret = 0;

	tsens_id_map = kzalloc(sizeof(int) * max_tsens_num,
			GFP_KERNEL);
	if (!tsens_id_map) {
		pr_err("Cannot allocate memory for tsens_id_map\n");
		return -ENOMEM;
	}

	for (i = 0; i < max_tsens_num; i++) {
		ret = tsens_get_hw_id_mapping(i, &tsens_id_map[i]);
		/* If return -ENXIO, hw_id is default in sequence */
		if (ret) {
			if (ret == -ENXIO) {
				tsens_id_map[i] = i;
				ret = 0;
			} else {
				pr_err("Failed to get hw id for id:%d.err:%d\n",
						i, ret);
				goto fail;
			}
		}
	}

	return ret;
fail:
	kfree(tsens_id_map);
	return ret;
}

/* 1:enable, 0:disable */
static int vdd_restriction_apply_all(int en)
{
	int i = 0;
	int en_cnt = 0;
	int dis_cnt = 0;
	int fail_cnt = 0;
	int ret = 0;

	for (i = 0; i < rails_cnt; i++) {
		if (rails[i].freq_req == 1 && freq_table_get)
			ret = vdd_restriction_apply_freq(&rails[i],
					en ? 0 : -1);
		else
			ret = vdd_restriction_apply_voltage(&rails[i],
					en ? 0 : -1);
		if (ret) {
			pr_err("Failed to %s for %s. err:%d",
					(en) ? "enable" : "disable",
					rails[i].name, ret);
			fail_cnt++;
		} else {
			if (en)
				en_cnt++;
			else
				dis_cnt++;
		}
	}

	/* As long as one rail is enabled, vdd rstr is enabled */
	if (en && en_cnt)
		vdd_rstr_en.enabled = 1;
	else if (!en && (dis_cnt == rails_cnt))
		vdd_rstr_en.enabled = 0;

	/*
	 * Check fail_cnt again to make sure all of the rails are applied
	 * restriction successfully or not
	 */
	if (fail_cnt)
		return -EFAULT;
	return ret;
}

static int msm_thermal_get_freq_table(void)
{
	int ret = 0;
	int i = 0;

	table = cpufreq_frequency_get_table(0);
	if (table == NULL) {
		pr_err("error reading cpufreq table\n");
		ret = -EINVAL;
		goto fail;
	}

	while (table[i].frequency != CPUFREQ_TABLE_END)
		i++;

	limit_idx_low = 0;
	limit_idx_high = limit_idx = i - 1;
	BUG_ON(limit_idx_high <= 0 || limit_idx_high <= limit_idx_low);
fail:
	return ret;
}

static int set_and_activate_threshold(uint32_t sensor_id,
	struct sensor_threshold *threshold)
{
	int ret = 0;

	ret = sensor_set_trip(sensor_id, threshold);
	if (ret != 0) {
		pr_err("sensor:%u Error in setting trip:%d. err:%d\n",
			sensor_id, threshold->trip, ret);
		goto set_done;
	}

	ret = sensor_activate_trip(sensor_id, threshold, true);
	if (ret != 0) {
		pr_err("sensor:%u Error in enabling trip:%d. err:%d\n",
			sensor_id, threshold->trip, ret);
		goto set_done;
	}

set_done:
	return ret;
}

static int therm_get_temp(uint32_t id, enum sensor_id_type type, long *temp)
{
	int ret = 0;
	struct tsens_device tsens_dev;

	if (!temp) {
		pr_err("Invalid value\n");
		ret = -EINVAL;
		goto get_temp_exit;
	}

	switch (type) {
	case THERM_ZONE_ID:
		ret = sensor_get_temp(id, temp);
		if (ret) {
			pr_err("Unable to read thermal zone sensor:%d\n", id);
			goto get_temp_exit;
		}
		break;
	case THERM_TSENS_ID:
		tsens_dev.sensor_num = id;
		ret = tsens_get_temp(&tsens_dev, temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d\n",
				tsens_dev.sensor_num);
			goto get_temp_exit;
		}
		break;
	default:
		pr_err("Invalid type\n");
		ret = -EINVAL;
		goto get_temp_exit;
	}

get_temp_exit:
	return ret;
}

static int set_threshold(uint32_t zone_id,
	struct sensor_threshold *threshold)
{
	int i = 0, ret = 0;
	long temp;

	if (!threshold) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto set_threshold_exit;
	}

	ret = therm_get_temp(zone_id, THERM_ZONE_ID, &temp);
	if (ret) {
		pr_err("Unable to read temperature for zone:%d. err:%d\n",
			zone_id, ret);
		goto set_threshold_exit;
	}

	while (i < MAX_THRESHOLD) {
		switch (threshold[i].trip) {
		case THERMAL_TRIP_CONFIGURABLE_HI:
			if (threshold[i].temp >= temp) {
				ret = set_and_activate_threshold(zone_id,
					&threshold[i]);
				if (ret)
					goto set_threshold_exit;
			}
			break;
		case THERMAL_TRIP_CONFIGURABLE_LOW:
			if (threshold[i].temp <= temp) {
				ret = set_and_activate_threshold(zone_id,
					&threshold[i]);
				if (ret)
					goto set_threshold_exit;
			}
			break;
		default:
			pr_err("zone:%u Invalid trip:%d\n", zone_id,
					threshold[i].trip);
			break;
		}
		i++;
	}
set_threshold_exit:
	return ret;
}

static int apply_vdd_mx_restriction(void)
{
	int ret = 0;

	if (mx_restr_applied)
		goto done;

	ret = regulator_set_voltage(vdd_mx, msm_thermal_info.vdd_mx_min,
			INT_MAX);
	if (ret) {
		pr_err("Failed to add mx vote, error %d\n", ret);
		goto done;
	}

	ret = regulator_enable(vdd_mx);
	if (ret)
		pr_err("Failed to vote for mx voltage %d, error %d\n",
				msm_thermal_info.vdd_mx_min, ret);
	else
		mx_restr_applied = true;

done:
	return ret;
}

static int remove_vdd_mx_restriction(void)
{
	int ret = 0;

	if (!mx_restr_applied)
		goto done;

	ret = regulator_disable(vdd_mx);
	if (ret) {
		pr_err("Failed to disable mx voting, error %d\n", ret);
		goto done;
	}

	ret = regulator_set_voltage(vdd_mx, 0, INT_MAX);
	if (ret)
		pr_err("Failed to remove mx vote, error %d\n", ret);
	else
		mx_restr_applied = false;

done:
	return ret;
}

static int do_vdd_mx(void)
{
	long temp = 0;
	int ret = 0;
	int i = 0;
	int dis_cnt = 0;

	if (!vdd_mx_enabled)
		return ret;

	mutex_lock(&vdd_mx_mutex);
	for (i = 0; i < thresh[MSM_VDD_MX_RESTRICTION].thresh_ct; i++) {
		ret = therm_get_temp(
			thresh[MSM_VDD_MX_RESTRICTION].thresh_list[i].sensor_id,
			thresh[MSM_VDD_MX_RESTRICTION].thresh_list[i].id_type,
			&temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d, err:%d\n",
				thresh[MSM_VDD_MX_RESTRICTION].thresh_list[i].
					sensor_id, ret);
			dis_cnt++;
			continue;
		}
		if (temp <=  msm_thermal_info.vdd_mx_temp_degC) {
			ret = apply_vdd_mx_restriction();
			if (ret)
				pr_err(
				"Failed to apply mx restriction\n");
			goto exit;
		} else if (temp >= (msm_thermal_info.vdd_mx_temp_degC +
				msm_thermal_info.vdd_mx_temp_hyst_degC)) {
			dis_cnt++;
		}
	}

	if ((dis_cnt == thresh[MSM_VDD_MX_RESTRICTION].thresh_ct)) {
		ret = remove_vdd_mx_restriction();
		if (ret)
			pr_err("Failed to remove vdd mx restriction\n");
	}

exit:
	mutex_unlock(&vdd_mx_mutex);
	return ret;
}

static void vdd_mx_notify(struct therm_threshold *trig_thresh)
{
	static uint32_t mx_sens_status;
	int ret;

	pr_debug("Sensor%d trigger recevied for type %d\n",
		trig_thresh->sensor_id,
		trig_thresh->trip_triggered);

	if (!vdd_mx_enabled)
		return;

	mutex_lock(&vdd_mx_mutex);

	switch (trig_thresh->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		mx_sens_status |= BIT(trig_thresh->sensor_id);
		break;
	case THERMAL_TRIP_CONFIGURABLE_HI:
		if (mx_sens_status & BIT(trig_thresh->sensor_id))
			mx_sens_status ^= BIT(trig_thresh->sensor_id);
		break;
	default:
		pr_err("Unsupported trip type\n");
		break;
	}

	if (mx_sens_status) {
		ret = apply_vdd_mx_restriction();
		if (ret)
			pr_err("Failed to apply mx restriction\n");
	} else if (!mx_sens_status) {
		ret = remove_vdd_mx_restriction();
		if (ret)
			pr_err("Failed to remove vdd mx restriction\n");
	}
	mutex_unlock(&vdd_mx_mutex);
	set_threshold(trig_thresh->sensor_id, trig_thresh->threshold);
}

static void msm_thermal_bite(int tsens_id, long temp)
{
	pr_err("TSENS:%d reached temperature:%ld. System reset\n",
		tsens_id, temp);
	scm_call_atomic1(SCM_SVC_BOOT, THERM_SECURE_BITE_CMD, 0);
}

static int do_therm_reset(void)
{
	int ret = 0, i;
	long temp = 0;

	if (!therm_reset_enabled)
		return ret;

	for (i = 0; i < thresh[MSM_THERM_RESET].thresh_ct; i++) {
		ret = therm_get_temp(
			thresh[MSM_THERM_RESET].thresh_list[i].sensor_id,
			thresh[MSM_THERM_RESET].thresh_list[i].id_type,
			&temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d. err:%d\n",
			thresh[MSM_THERM_RESET].thresh_list[i].sensor_id,
			ret);
			continue;
		}

		if (temp >= msm_thermal_info.therm_reset_temp_degC)
			msm_thermal_bite(
			thresh[MSM_THERM_RESET].thresh_list[i].sensor_id, temp);
	}

	return ret;
}

static void therm_reset_notify(struct therm_threshold *thresh_data)
{
	long temp;
	int ret = 0;

	if (!therm_reset_enabled)
		return;

	if (!thresh_data) {
		pr_err("Invalid input\n");
		return;
	}

	switch (thresh_data->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		ret = therm_get_temp(thresh_data->sensor_id,
				thresh_data->id_type, &temp);
		if (ret)
			pr_err("Unable to read TSENS sensor:%d. err:%d\n",
				thresh_data->sensor_id, ret);
		msm_thermal_bite(tsens_id_map[thresh_data->sensor_id],
					temp);
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		break;
	default:
		pr_err("Invalid trip type\n");
		break;
	}
	set_threshold(thresh_data->sensor_id, thresh_data->threshold);
}

#ifdef CONFIG_SMP
static void __ref do_core_control(long temp)
{
	int i = 0;
	int ret = 0;

	if (!core_control_enabled)
		return;

	mutex_lock(&core_control_mutex);
	if (msm_thermal_info.core_control_mask &&
		temp >= msm_thermal_info.core_limit_temp_degC) {
		for (i = num_possible_cpus(); i > 0; i--) {
			if (!(msm_thermal_info.core_control_mask & BIT(i)))
				continue;
			if (cpus_offlined & BIT(i) && !cpu_online(i))
				continue;
			pr_info("Set Offline: CPU%d Temp: %ld\n",
					i, temp);
			ret = cpu_down(i);
			if (ret)
				pr_err("Error %d offline core %d\n",
					ret, i);
			cpus_offlined |= BIT(i);
			break;
		}
	} else if (msm_thermal_info.core_control_mask && cpus_offlined &&
		temp <= (msm_thermal_info.core_limit_temp_degC -
			msm_thermal_info.core_temp_hysteresis_degC)) {
		for (i = 0; i < num_possible_cpus(); i++) {
			if (!(cpus_offlined & BIT(i)))
				continue;
			cpus_offlined &= ~BIT(i);
			pr_info("Allow Online CPU%d Temp: %ld\n",
					i, temp);
			/*
			 * If this core is already online, then bring up the
			 * next offlined core.
			 */
			if (cpu_online(i))
				continue;
			ret = cpu_up(i);
			if (ret)
				pr_err("Error %d online core %d\n",
						ret, i);
			break;
		}
	}
	mutex_unlock(&core_control_mutex);
}
/* Call with core_control_mutex locked */
static int __ref update_offline_cores(int val)
{
	uint32_t cpu = 0;
	int ret = 0;

	if (!core_control_enabled)
		return 0;

	cpus_offlined = msm_thermal_info.core_control_mask & val;

	for_each_possible_cpu(cpu) {
		if (!(cpus_offlined & BIT(cpu)))
			continue;
		if (!cpu_online(cpu))
			continue;
		ret = cpu_down(cpu);
		if (ret)
			pr_err("Unable to offline CPU%d. err:%d\n",
				cpu, ret);
		else
			pr_debug("Offlined CPU%d\n", cpu);
	}
	return ret;
}

static __ref int do_hotplug(void *data)
{
	int ret = 0;
	uint32_t cpu = 0, mask = 0;

	if (!core_control_enabled) {
		pr_debug("Core control disabled\n");
		return -EINVAL;
	}

	while (!kthread_should_stop()) {
		while (wait_for_completion_interruptible(
			&hotplug_notify_complete) != 0)
			;
		INIT_COMPLETION(hotplug_notify_complete);
		mask = 0;

		mutex_lock(&core_control_mutex);
		for_each_possible_cpu(cpu) {
			if (hotplug_enabled &&
				cpus[cpu].hotplug_thresh_clear) {
				set_threshold(cpus[cpu].sensor_id,
				&cpus[cpu].threshold[HOTPLUG_THRESHOLD_HIGH]);

				cpus[cpu].hotplug_thresh_clear = false;
			}
			if (cpus[cpu].offline || cpus[cpu].user_offline)
				mask |= BIT(cpu);
		}
		if (mask != cpus_offlined)
			update_offline_cores(mask);
		mutex_unlock(&core_control_mutex);
		sysfs_notify(cc_kobj, NULL, "cpus_offlined");
	}

	return ret;
}
#else
static void __ref do_core_control(long temp)
{
	return;
}

static __ref int do_hotplug(void *data)
{
	return 0;
}
#endif

static int do_gfx_phase_cond(void)
{
	long temp = 0;
	int ret = 0;
	uint32_t new_req_band = curr_gfx_band;

	if (!gfx_phase_ctrl_enabled)
		return ret;

	mutex_lock(&gfx_mutex);
	ret = therm_get_temp(
		thresh[MSM_GFX_PHASE_CTRL_WARM].thresh_list->sensor_id,
		thresh[MSM_GFX_PHASE_CTRL_WARM].thresh_list->id_type,
		&temp);
	if (ret) {
		pr_err("Unable to read TSENS sensor:%d. err:%d\n",
			thresh[MSM_GFX_PHASE_CTRL_WARM].thresh_list->sensor_id,
			ret);
		goto gfx_phase_cond_exit;
	}

	switch (curr_gfx_band) {
	case MSM_HOT_CRITICAL:
		if (temp < (msm_thermal_info.gfx_phase_hot_temp_degC -
			msm_thermal_info.gfx_phase_hot_temp_hyst_degC))
			new_req_band = MSM_WARM;
		break;
	case MSM_WARM:
		if (temp >= msm_thermal_info.gfx_phase_hot_temp_degC)
			new_req_band = MSM_HOT_CRITICAL;
		else if (temp < (msm_thermal_info.gfx_phase_warm_temp_degC -
			msm_thermal_info.gfx_phase_warm_temp_hyst_degC))
			new_req_band = MSM_NORMAL;
		break;
	case MSM_NORMAL:
		if (temp >= msm_thermal_info.gfx_phase_warm_temp_degC)
			new_req_band = MSM_WARM;
		break;
	default:
		if (temp >= msm_thermal_info.gfx_phase_hot_temp_degC)
			new_req_band = MSM_HOT_CRITICAL;
		else if (temp >= msm_thermal_info.gfx_phase_warm_temp_degC)
			new_req_band = MSM_WARM;
		else
			new_req_band = MSM_NORMAL;
		break;
	}

	if (new_req_band != curr_gfx_band) {
		ret = send_temperature_band(MSM_GFX_PHASE_CTRL, new_req_band);
		if (!ret) {
			pr_debug("Reached %d band. Temp:%ld\n", new_req_band,
					temp);
			curr_gfx_band = new_req_band;
		} else {
			pr_err("Error sending temp. band:%d. Temp:%ld. err:%d",
					new_req_band, temp, ret);
		}
	}

gfx_phase_cond_exit:
	mutex_unlock(&gfx_mutex);
	return ret;
}

static int do_cx_phase_cond(void)
{
	long temp = 0;
	int i, ret = 0, dis_cnt = 0;

	if (!cx_phase_ctrl_enabled)
		return ret;

	mutex_lock(&cx_mutex);
	for (i = 0; i < thresh[MSM_CX_PHASE_CTRL_HOT].thresh_ct; i++) {
		ret = therm_get_temp(
			thresh[MSM_CX_PHASE_CTRL_HOT].thresh_list[i].sensor_id,
			thresh[MSM_CX_PHASE_CTRL_HOT].thresh_list[i].id_type,
			&temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d. err:%d\n",
			thresh[MSM_CX_PHASE_CTRL_HOT].thresh_list[i].sensor_id,
			ret);
			dis_cnt++;
			continue;
		}

		if (temp >=  msm_thermal_info.cx_phase_hot_temp_degC) {
			if (curr_cx_band != MSM_HOT_CRITICAL) {
				ret = send_temperature_band(MSM_CX_PHASE_CTRL,
					MSM_HOT_CRITICAL);
				if (!ret) {
					pr_debug("band:HOT_CRITICAL Temp:%ld\n",
							temp);
					curr_cx_band = MSM_HOT_CRITICAL;
				} else {
					pr_err("Error %d sending HOT_CRITICAL",
							ret);
				}
			}
			goto cx_phase_cond_exit;
		} else if (temp < (msm_thermal_info.cx_phase_hot_temp_degC -
			msm_thermal_info.cx_phase_hot_temp_hyst_degC))
			dis_cnt++;
	}
	if (dis_cnt == max_tsens_num && curr_cx_band != MSM_WARM) {
		ret = send_temperature_band(MSM_CX_PHASE_CTRL, MSM_WARM);
		if (!ret) {
			pr_debug("band:WARM Temp:%ld\n", temp);
			curr_cx_band = MSM_WARM;
		} else {
			pr_err("Error sending WARM temp band. err:%d",
					ret);
		}
	}
cx_phase_cond_exit:
	mutex_unlock(&cx_mutex);
	return ret;
}

static int do_ocr(void)
{
	long temp = 0;
	int ret = 0;
	int i = 0, j = 0;
	int pfm_cnt = 0;

	if (!ocr_enabled)
		return ret;

	mutex_lock(&ocr_mutex);
	for (i = 0; i < thresh[MSM_OCR].thresh_ct; i++) {
		ret = therm_get_temp(
			thresh[MSM_OCR].thresh_list[i].sensor_id,
			thresh[MSM_OCR].thresh_list[i].id_type,
			&temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor %d. err:%d\n",
			thresh[MSM_OCR].thresh_list[i].sensor_id,
			ret);
			pfm_cnt++;
			continue;
		}

		if (temp > msm_thermal_info.ocr_temp_degC) {
			if (ocr_rails[0].init != OPTIMUM_CURRENT_NR)
				for (j = 0; j < ocr_rail_cnt; j++)
					ocr_rails[j].init = OPTIMUM_CURRENT_NR;
			ret = ocr_set_mode_all(OPTIMUM_CURRENT_MAX);
			if (ret)
				pr_err("Error setting max ocr. err:%d\n",
					ret);
			else
				pr_debug("Requested MAX OCR. tsens:%d Temp:%ld",
				thresh[MSM_OCR].thresh_list[i].sensor_id, temp);
			goto do_ocr_exit;
		} else if (temp <= (msm_thermal_info.ocr_temp_degC -
			msm_thermal_info.ocr_temp_hyst_degC))
			pfm_cnt++;
	}

	if (pfm_cnt == thresh[MSM_OCR].thresh_ct ||
		ocr_rails[0].init != OPTIMUM_CURRENT_NR) {
		/* 'init' not equal to OPTIMUM_CURRENT_NR means this is the
		** first polling iteration after device probe. During first
		** iteration, if temperature is less than the set point, clear
		** the max current request made and reset the 'init'.
		*/
		if (ocr_rails[0].init != OPTIMUM_CURRENT_NR)
			for (j = 0; j < ocr_rail_cnt; j++)
				ocr_rails[j].init = OPTIMUM_CURRENT_NR;
		ret = ocr_set_mode_all(OPTIMUM_CURRENT_MIN);
		if (ret) {
			pr_err("Error setting min ocr. err:%d\n",
				ret);
			goto do_ocr_exit;
		} else {
			pr_debug("Requested MIN OCR. Temp:%ld", temp);
		}
	}
do_ocr_exit:
	mutex_unlock(&ocr_mutex);
	return ret;
}

static int do_vdd_restriction(void)
{
	long temp = 0;
	int ret = 0;
	int i = 0;
	int dis_cnt = 0;

	if (!vdd_rstr_enabled)
		return ret;

	if (usefreq && !freq_table_get) {
		if (check_freq_table())
			return ret;
	}

	mutex_lock(&vdd_rstr_mutex);
	for (i = 0; i < thresh[MSM_VDD_RESTRICTION].thresh_ct; i++) {
		ret = therm_get_temp(
			thresh[MSM_VDD_RESTRICTION].thresh_list[i].sensor_id,
			thresh[MSM_VDD_RESTRICTION].thresh_list[i].id_type,
			&temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d. err:%d\n",
			thresh[MSM_VDD_RESTRICTION].thresh_list[i].sensor_id,
			ret);
			dis_cnt++;
			continue;
		}
		if (temp <=  msm_thermal_info.vdd_rstr_temp_degC) {
			ret = vdd_restriction_apply_all(1);
			if (ret) {
				pr_err( \
				"Enable vdd rstr for all failed. err:%d\n",
					ret);
				goto exit;
			}
			pr_debug("Enabled Vdd Restriction tsens:%d. Temp:%ld\n",
			thresh[MSM_VDD_RESTRICTION].thresh_list[i].sensor_id,
			temp);
			goto exit;
		} else if (temp > msm_thermal_info.vdd_rstr_temp_hyst_degC)
			dis_cnt++;
	}
	if (dis_cnt == max_tsens_num) {
		ret = vdd_restriction_apply_all(0);
		if (ret) {
			pr_err("Disable vdd rstr for all failed. err:%d\n",
					ret);
			goto exit;
		}
		pr_debug("Disabled Vdd Restriction\n");
	}
exit:
	mutex_unlock(&vdd_rstr_mutex);
	return ret;
}

static int do_psm(void)
{
	long temp = 0;
	int ret = 0;
	int i = 0;
	int auto_cnt = 0;

	mutex_lock(&psm_mutex);
	for (i = 0; i < max_tsens_num; i++) {
		ret = therm_get_temp(tsens_id_map[i], THERM_TSENS_ID, &temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d. err:%d\n",
					tsens_id_map[i], ret);
			auto_cnt++;
			continue;
		}

		/*
		 * As long as one sensor is above the threshold, set PWM mode
		 * on all rails, and loop stops. Set auto mode when all rails
		 * are below thershold
		 */
		if (temp >  msm_thermal_info.psm_temp_degC) {
			ret = psm_set_mode_all(PMIC_PWM_MODE);
			if (ret) {
				pr_err("Set pwm mode for all failed. err:%d\n",
						ret);
				goto exit;
			}
			pr_debug("Requested PMIC PWM Mode tsens:%d. Temp:%ld\n",
					tsens_id_map[i], temp);
			break;
		} else if (temp <= msm_thermal_info.psm_temp_hyst_degC)
			auto_cnt++;
	}

	if (auto_cnt == max_tsens_num) {
		ret = psm_set_mode_all(PMIC_AUTO_MODE);
		if (ret) {
			pr_err("Set auto mode for all failed. err:%d\n", ret);
			goto exit;
		}
		pr_debug("Requested PMIC AUTO Mode\n");
	}

exit:
	mutex_unlock(&psm_mutex);
	return ret;
}

static void do_freq_control(long temp)
{
	uint32_t cpu = 0;
	uint32_t max_freq = cpus[cpu].limited_max_freq;

	if (temp >= msm_thermal_info.limit_temp_degC) {
		if (limit_idx == limit_idx_low)
			return;

		limit_idx -= msm_thermal_info.bootup_freq_step;
		if (limit_idx < limit_idx_low)
			limit_idx = limit_idx_low;
		max_freq = table[limit_idx].frequency;
	} else if (temp < msm_thermal_info.limit_temp_degC -
		 msm_thermal_info.temp_hysteresis_degC) {
		if (limit_idx == limit_idx_high)
			return;

		limit_idx += msm_thermal_info.bootup_freq_step;
		if (limit_idx >= limit_idx_high) {
			limit_idx = limit_idx_high;
			max_freq = UINT_MAX;
		} else
			max_freq = table[limit_idx].frequency;
	}

	if (max_freq == cpus[cpu].limited_max_freq)
		return;

	/* Update new limits */
	get_online_cpus();
	for_each_possible_cpu(cpu) {
		if (!(msm_thermal_info.bootup_freq_control_mask & BIT(cpu)))
			continue;
		pr_info("Limiting CPU%d max frequency to %u. Temp:%ld\n",
			cpu, max_freq, temp);
		cpus[cpu].limited_max_freq = max_freq;
		update_cpu_freq(cpu);
	}
	put_online_cpus();
}

static void check_temp(struct work_struct *work)
{
	static int limit_init;
	long temp = 0;
	int ret = 0;

	do_therm_reset();

	ret = therm_get_temp(msm_thermal_info.sensor_id, THERM_TSENS_ID, &temp);
	if (ret) {
		pr_err("Unable to read TSENS sensor:%d. err:%d\n",
				msm_thermal_info.sensor_id, ret);
		goto reschedule;
	}
	do_core_control(temp);
	do_vdd_mx();
	do_psm();
	do_gfx_phase_cond();
	do_cx_phase_cond();
	do_ocr();

	if (!limit_init) {
		ret = msm_thermal_get_freq_table();
		if (ret)
			goto reschedule;
		else
			limit_init = 1;
	}

	do_vdd_restriction();
	do_freq_control(temp);

reschedule:
	if (polling_enabled)
		schedule_delayed_work(&check_temp_work,
				msecs_to_jiffies(msm_thermal_info.poll_ms));
}

static int __ref msm_thermal_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	uint32_t cpu = (uint32_t)hcpu;

	if (action == CPU_UP_PREPARE || action == CPU_UP_PREPARE_FROZEN) {
		if (core_control_enabled &&
			(msm_thermal_info.core_control_mask & BIT(cpu)) &&
			(cpus_offlined & BIT(cpu))) {
			pr_debug("Preventing CPU%d from coming online.\n",
				cpu);
			return NOTIFY_BAD;
		}
	}

	pr_debug("voting for CPU%d to be online\n", cpu);
	return NOTIFY_OK;
}

static struct notifier_block __refdata msm_thermal_cpu_notifier = {
	.notifier_call = msm_thermal_cpu_callback,
};
static int hotplug_notify(enum thermal_trip_type type, int temp, void *data)
{
	struct cpu_info *cpu_node = (struct cpu_info *)data;

	pr_info("%s reach temp threshold: %d\n", cpu_node->sensor_type, temp);

	if (!(msm_thermal_info.core_control_mask & BIT(cpu_node->cpu)))
		return 0;
	switch (type) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		if (!(cpu_node->offline))
			cpu_node->offline = 1;
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		if (cpu_node->offline)
			cpu_node->offline = 0;
		break;
	default:
		break;
	}
	if (hotplug_task) {
		cpu_node->hotplug_thresh_clear = true;
		complete(&hotplug_notify_complete);
	} else
		pr_err("Hotplug task is not initialized\n");
	return 0;
}
/* Adjust cpus offlined bit based on temperature reading. */
static int hotplug_init_cpu_offlined(void)
{
	long temp = 0;
	uint32_t cpu = 0;

	if (!hotplug_enabled)
		return 0;

	mutex_lock(&core_control_mutex);
	for_each_possible_cpu(cpu) {
		if (!(msm_thermal_info.core_control_mask & BIT(cpus[cpu].cpu)))
			continue;
		if (therm_get_temp(cpus[cpu].sensor_id, cpus[cpu].id_type,
					&temp)) {
			pr_err("Unable to read TSENS sensor:%d.\n",
				cpus[cpu].sensor_id);
			mutex_unlock(&core_control_mutex);
			return -EINVAL;
		}

		if (temp >= msm_thermal_info.hotplug_temp_degC)
			cpus[cpu].offline = 1;
		else if (temp <= (msm_thermal_info.hotplug_temp_degC -
			msm_thermal_info.hotplug_temp_hysteresis_degC))
			cpus[cpu].offline = 0;
	}
	mutex_unlock(&core_control_mutex);

	if (hotplug_task)
		complete(&hotplug_notify_complete);
	else {
		pr_err("Hotplug task is not initialized\n");
		return -EINVAL;
	}
	return 0;
}

static void hotplug_init(void)
{
	uint32_t cpu = 0;
	struct sensor_threshold *hi_thresh = NULL, *low_thresh = NULL;

	if (hotplug_task)
		return;

	if (!hotplug_enabled)
		goto init_kthread;

	for_each_possible_cpu(cpu) {
		cpus[cpu].sensor_id =
			sensor_get_id((char *)cpus[cpu].sensor_type);
		cpus[cpu].id_type = THERM_ZONE_ID;
		if (!(msm_thermal_info.core_control_mask & BIT(cpus[cpu].cpu)))
			continue;

		hi_thresh = &cpus[cpu].threshold[HOTPLUG_THRESHOLD_HIGH];
		low_thresh = &cpus[cpu].threshold[HOTPLUG_THRESHOLD_LOW];
		hi_thresh->temp = msm_thermal_info.hotplug_temp_degC;
		hi_thresh->trip = THERMAL_TRIP_CONFIGURABLE_HI;
		low_thresh->temp = msm_thermal_info.hotplug_temp_degC -
				msm_thermal_info.hotplug_temp_hysteresis_degC;
		low_thresh->trip = THERMAL_TRIP_CONFIGURABLE_LOW;
		hi_thresh->notify = low_thresh->notify = hotplug_notify;
		hi_thresh->data = low_thresh->data = (void *)&cpus[cpu];

		set_threshold(cpus[cpu].sensor_id, hi_thresh);
	}
init_kthread:
	init_completion(&hotplug_notify_complete);
	hotplug_task = kthread_run(do_hotplug, NULL, "msm_thermal:hotplug");
	if (IS_ERR(hotplug_task)) {
		pr_err("Failed to create do_hotplug thread. err:%ld\n",
				PTR_ERR(hotplug_task));
		return;
	}
	/*
	 * Adjust cpus offlined bit when hotplug intitializes so that the new
	 * cpus offlined state is based on hotplug threshold range
	 */
	if (hotplug_init_cpu_offlined())
		kthread_stop(hotplug_task);
}

static __ref int do_freq_mitigation(void *data)
{
	int ret = 0;
	uint32_t cpu = 0, max_freq_req = 0, min_freq_req = 0;

	while (!kthread_should_stop()) {
		while (wait_for_completion_interruptible(
			&freq_mitigation_complete) != 0)
			;
		INIT_COMPLETION(freq_mitigation_complete);

		get_online_cpus();
		for_each_possible_cpu(cpu) {
			max_freq_req = (cpus[cpu].max_freq) ?
					msm_thermal_info.freq_limit :
					UINT_MAX;
			max_freq_req = min(max_freq_req,
					cpus[cpu].user_max_freq);

			min_freq_req = max(min_freq_limit,
					cpus[cpu].user_min_freq);

			if ((max_freq_req == cpus[cpu].limited_max_freq)
				&& (min_freq_req ==
				cpus[cpu].limited_min_freq))
				goto reset_threshold;

			cpus[cpu].limited_max_freq = max_freq_req;
			cpus[cpu].limited_min_freq = min_freq_req;
			update_cpu_freq(cpu);
reset_threshold:
			if (freq_mitigation_enabled &&
				cpus[cpu].freq_thresh_clear) {
				set_threshold(cpus[cpu].sensor_id,
				&cpus[cpu].threshold[FREQ_THRESHOLD_HIGH]);

				cpus[cpu].freq_thresh_clear = false;
			}
		}
		put_online_cpus();
	}
	return ret;
}

static int freq_mitigation_notify(enum thermal_trip_type type,
	int temp, void *data)
{
	struct cpu_info *cpu_node = (struct cpu_info *) data;

	pr_debug("%s reached temp threshold: %d\n",
		cpu_node->sensor_type, temp);

	if (!(msm_thermal_info.freq_mitig_control_mask &
		BIT(cpu_node->cpu)))
		return 0;

	switch (type) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		if (!cpu_node->max_freq) {
			pr_info("Mitigating CPU%d frequency to %d\n",
				cpu_node->cpu,
				msm_thermal_info.freq_limit);

			cpu_node->max_freq = true;
		}
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		if (cpu_node->max_freq) {
			pr_info("Removing frequency mitigation for CPU%d\n",
				cpu_node->cpu);

			cpu_node->max_freq = false;
		}
		break;
	default:
		break;
	}

	if (freq_mitigation_task) {
		cpu_node->freq_thresh_clear = true;
		complete(&freq_mitigation_complete);
	} else {
		pr_err("Frequency mitigation task is not initialized\n");
	}

	return 0;
}

static void freq_mitigation_init(void)
{
	uint32_t cpu = 0;
	struct sensor_threshold *hi_thresh = NULL, *low_thresh = NULL;

	if (freq_mitigation_task)
		return;
	if (!freq_mitigation_enabled)
		goto init_freq_thread;

	for_each_possible_cpu(cpu) {
		if (!(msm_thermal_info.freq_mitig_control_mask & BIT(cpu)))
			continue;
		hi_thresh = &cpus[cpu].threshold[FREQ_THRESHOLD_HIGH];
		low_thresh = &cpus[cpu].threshold[FREQ_THRESHOLD_LOW];

		hi_thresh->temp = msm_thermal_info.freq_mitig_temp_degc;
		hi_thresh->trip = THERMAL_TRIP_CONFIGURABLE_HI;
		low_thresh->temp = msm_thermal_info.freq_mitig_temp_degc -
			msm_thermal_info.freq_mitig_temp_hysteresis_degc;
		low_thresh->trip = THERMAL_TRIP_CONFIGURABLE_LOW;
		hi_thresh->notify = low_thresh->notify =
			freq_mitigation_notify;
		hi_thresh->data = low_thresh->data = (void *)&cpus[cpu];

		set_threshold(cpus[cpu].sensor_id, hi_thresh);
	}
init_freq_thread:
	init_completion(&freq_mitigation_complete);
	freq_mitigation_task = kthread_run(do_freq_mitigation, NULL,
		"msm_thermal:freq_mitig");

	if (IS_ERR(freq_mitigation_task)) {
		pr_err("Failed to create frequency mitigation thread. err:%ld\n",
				PTR_ERR(freq_mitigation_task));
		return;
	}
}

int msm_thermal_set_frequency(uint32_t cpu, uint32_t freq, bool is_max)
{
	int ret = 0;

	if (cpu >= num_possible_cpus()) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto set_freq_exit;
	}

	pr_debug("Userspace requested %s frequency %u for CPU%u\n",
			(is_max) ? "Max" : "Min", freq, cpu);
	if (is_max) {
		if (cpus[cpu].user_max_freq == freq)
			goto set_freq_exit;

		cpus[cpu].user_max_freq = freq;
	} else {
		if (cpus[cpu].user_min_freq == freq)
			goto set_freq_exit;

		cpus[cpu].user_min_freq = freq;
	}

	if (freq_mitigation_task) {
		complete(&freq_mitigation_complete);
	} else {
		pr_err("Frequency mitigation task is not initialized\n");
		ret = -ESRCH;
		goto set_freq_exit;
	}

set_freq_exit:
	return ret;
}

int therm_set_threshold(struct threshold_info *thresh_inp)
{
	int ret = 0, i = 0, err = 0;
	struct therm_threshold *thresh_ptr;

	if (!thresh_inp) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto therm_set_exit;
	}

	thresh_inp->thresh_triggered = false;
	for (i = 0; i < thresh_inp->thresh_ct; i++) {
		thresh_ptr = &thresh_inp->thresh_list[i];
		thresh_ptr->trip_triggered = -1;
		err = set_threshold(thresh_ptr->sensor_id,
			thresh_ptr->threshold);
		if (err) {
			ret = err;
			err = 0;
		}
	}

therm_set_exit:
	return ret;
}

static void cx_phase_ctrl_notify(struct therm_threshold *trig_thresh)
{
	static uint32_t cx_sens_status;
	int ret = 0;

	if (!cx_phase_ctrl_enabled)
		return;

	if (trig_thresh->trip_triggered < 0)
		goto cx_phase_ctrl_exit;

	mutex_lock(&cx_mutex);
	pr_debug("sensor:%d reached %s thresh for CX\n",
		tsens_id_map[trig_thresh->sensor_id],
		(trig_thresh->trip_triggered == THERMAL_TRIP_CONFIGURABLE_HI) ?
		"hot critical" : "warm");

	switch (trig_thresh->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		cx_sens_status |= BIT(trig_thresh->sensor_id);
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		if (cx_sens_status & BIT(trig_thresh->sensor_id))
			cx_sens_status ^= BIT(trig_thresh->sensor_id);
		break;
	default:
		pr_err("Unsupported trip type\n");
		goto cx_phase_unlock_exit;
		break;
	}

	if ((cx_sens_status && (curr_cx_band == MSM_HOT_CRITICAL)) ||
		(!cx_sens_status && (curr_cx_band == MSM_WARM)))
		goto cx_phase_unlock_exit;
	ret = send_temperature_band(MSM_CX_PHASE_CTRL, (cx_sens_status) ?
		MSM_HOT_CRITICAL : MSM_WARM);
	if (!ret)
		curr_cx_band = (cx_sens_status) ? MSM_HOT_CRITICAL : MSM_WARM;

cx_phase_unlock_exit:
	mutex_unlock(&cx_mutex);
cx_phase_ctrl_exit:
	set_threshold(trig_thresh->sensor_id, trig_thresh->threshold);
	return;
}

static void gfx_phase_ctrl_notify(struct therm_threshold *trig_thresh)
{
	uint32_t new_req_band = curr_gfx_band;
	int ret = 0;

	if (!gfx_phase_ctrl_enabled)
		return;

	if (trig_thresh->trip_triggered < 0)
		goto gfx_phase_ctrl_exit;

	mutex_lock(&gfx_mutex);
	switch (thresh[MSM_GFX_PHASE_CTRL_HOT].thresh_list->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		new_req_band = MSM_HOT_CRITICAL;
		pr_debug("sensor:%d reached hot critical thresh for GFX\n",
			tsens_id_map[trig_thresh->sensor_id]);
		goto notify_new_band;
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		new_req_band = MSM_WARM;
		pr_debug("sensor:%d reached warm thresh for GFX\n",
			tsens_id_map[trig_thresh->sensor_id]);
		goto notify_new_band;
		break;
	default:
		break;
	}
	switch (thresh[MSM_GFX_PHASE_CTRL_WARM].thresh_list->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		new_req_band = MSM_WARM;
		pr_debug("sensor:%d reached warm thresh for GFX\n",
			tsens_id_map[trig_thresh->sensor_id]);
		goto notify_new_band;
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		new_req_band = MSM_NORMAL;
		pr_debug("sensor:%d reached normal thresh for GFX\n",
			tsens_id_map[trig_thresh->sensor_id]);
		goto notify_new_band;
		break;
	default:
		break;
	}

notify_new_band:
	if (new_req_band != curr_gfx_band) {
		ret = send_temperature_band(MSM_GFX_PHASE_CTRL, new_req_band);
		if (!ret)
			curr_gfx_band = new_req_band;
	}
	mutex_unlock(&gfx_mutex);
gfx_phase_ctrl_exit:
	switch (curr_gfx_band) {
	case MSM_HOT_CRITICAL:
		therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_HOT]);
		break;
	case MSM_NORMAL:
		therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_WARM]);
		break;
	case MSM_WARM:
	default:
		therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_HOT]);
		therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_WARM]);
		break;
	}
	return;
}

static void vdd_restriction_notify(struct therm_threshold *trig_thresh)
{
	int ret = 0;
	static uint32_t vdd_sens_status;

	if (!vdd_rstr_enabled)
		return;
	if (!trig_thresh) {
		pr_err("Invalid input\n");
		return;
	}
	if (trig_thresh->trip_triggered < 0)
		goto set_and_exit;

	mutex_lock(&vdd_rstr_mutex);
	pr_debug("sensor:%d reached %s thresh for Vdd restriction\n",
		tsens_id_map[trig_thresh->sensor_id],
		(trig_thresh->trip_triggered == THERMAL_TRIP_CONFIGURABLE_HI) ?
		"high" : "low");
	switch (trig_thresh->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		if (vdd_sens_status & BIT(trig_thresh->sensor_id))
			vdd_sens_status ^= BIT(trig_thresh->sensor_id);
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		vdd_sens_status |= BIT(trig_thresh->sensor_id);
		break;
	default:
		pr_err("Unsupported trip type\n");
		goto unlock_and_exit;
		break;
	}

	ret = vdd_restriction_apply_all((vdd_sens_status) ? 1 : 0);
	if (ret) {
		pr_err("%s vdd rstr votlage for all failed\n",
			(vdd_sens_status) ?
			"Enable" : "Disable");
			goto unlock_and_exit;
	}

unlock_and_exit:
	mutex_unlock(&vdd_rstr_mutex);
set_and_exit:
	set_threshold(trig_thresh->sensor_id, trig_thresh->threshold);
	return;
}

static void ocr_notify(struct therm_threshold *trig_thresh)
{
	int ret = 0;
	static uint32_t ocr_sens_status;

	if (!ocr_enabled)
		return;
	if (!trig_thresh) {
		pr_err("Invalid input\n");
		return;
	}
	if (trig_thresh->trip_triggered < 0)
		goto set_and_exit;

	mutex_lock(&ocr_mutex);
	pr_debug("sensor%d reached %d thresh for Optimum current request\n",
		tsens_id_map[trig_thresh->sensor_id],
		trig_thresh->trip_triggered);
	switch (trig_thresh->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		ocr_sens_status |= BIT(trig_thresh->sensor_id);
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		if (ocr_sens_status & BIT(trig_thresh->sensor_id))
			ocr_sens_status ^= BIT(trig_thresh->sensor_id);
		break;
	default:
		pr_err("Unsupported trip type\n");
		goto unlock_and_exit;
		break;
	}

	ret = ocr_set_mode_all(ocr_sens_status ? OPTIMUM_CURRENT_MAX :
				OPTIMUM_CURRENT_MIN);
	if (ret) {
		pr_err("%s Optimum current mode for all failed. err:%d\n",
			(ocr_sens_status) ?
			"Enable" : "Disable", ret);
			goto unlock_and_exit;
	}

unlock_and_exit:
	mutex_unlock(&ocr_mutex);
set_and_exit:
	set_threshold(trig_thresh->sensor_id, trig_thresh->threshold);
	return;
}

static __ref int do_thermal_monitor(void *data)
{
	int ret = 0, i, j;
	struct therm_threshold *sensor_list;

	while (!kthread_should_stop()) {
		while (wait_for_completion_interruptible(
			&thermal_monitor_complete) != 0)
			;
		INIT_COMPLETION(thermal_monitor_complete);

		for (i = 0; i < MSM_LIST_MAX_NR; i++) {
			if (!thresh[i].thresh_triggered)
				continue;
			thresh[i].thresh_triggered = false;
			for (j = 0; j < thresh[i].thresh_ct; j++) {
				sensor_list = &thresh[i].thresh_list[j];
				if (sensor_list->trip_triggered < 0)
					continue;
				sensor_list->notify(sensor_list);
				sensor_list->trip_triggered = -1;
			}
		}
	}
	return ret;
}

static int convert_to_zone_id(struct threshold_info *thresh_inp)
{
	int ret = 0, i, zone_id;
	struct therm_threshold *thresh_array;

	if (!thresh_inp) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto convert_to_exit;
	}
	thresh_array = thresh_inp->thresh_list;

	for (i = 0; i < thresh_inp->thresh_ct; i++) {
		char tsens_name[TSENS_NAME_MAX] = "";

		if (thresh_array[i].id_type == THERM_ZONE_ID)
			continue;
		snprintf(tsens_name, TSENS_NAME_MAX, TSENS_NAME_FORMAT,
			thresh_array[i].sensor_id);
		zone_id = sensor_get_id(tsens_name);
		if (zone_id < 0) {
			pr_err("Error getting zone id for %s. err:%d\n",
				tsens_name, ret);
			ret = zone_id;
			goto convert_to_exit;
		}
		thresh_array[i].sensor_id = zone_id;
		thresh_array[i].id_type = THERM_ZONE_ID;
	}

convert_to_exit:
	return ret;
}

static void thermal_monitor_init(void)
{
	if (thermal_monitor_task)
		return;

	init_completion(&thermal_monitor_complete);
	thermal_monitor_task = kthread_run(do_thermal_monitor, NULL,
		"msm_thermal:therm_monitor");
	if (IS_ERR(thermal_monitor_task)) {
		pr_err("Failed to create thermal monitor thread. err:%ld\n",
				PTR_ERR(thermal_monitor_task));
		goto init_exit;
	}

	if (therm_reset_enabled &&
		!(convert_to_zone_id(&thresh[MSM_THERM_RESET])))
		therm_set_threshold(&thresh[MSM_THERM_RESET]);

	if ((cx_phase_ctrl_enabled) &&
		!(convert_to_zone_id(&thresh[MSM_CX_PHASE_CTRL_HOT])))
		therm_set_threshold(&thresh[MSM_CX_PHASE_CTRL_HOT]);

	if ((vdd_rstr_enabled) &&
		!(convert_to_zone_id(&thresh[MSM_VDD_RESTRICTION])))
		therm_set_threshold(&thresh[MSM_VDD_RESTRICTION]);

	if ((gfx_phase_ctrl_enabled) &&
		!(convert_to_zone_id(&thresh[MSM_GFX_PHASE_CTRL_WARM])) &&
		!(convert_to_zone_id(&thresh[MSM_GFX_PHASE_CTRL_HOT]))) {
		therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_WARM]);
		therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_HOT]);
	}

	if ((ocr_enabled) &&
		!(convert_to_zone_id(&thresh[MSM_OCR])))
		therm_set_threshold(&thresh[MSM_OCR]);

	if (vdd_mx_enabled &&
		!(convert_to_zone_id(&thresh[MSM_VDD_MX_RESTRICTION])))
		therm_set_threshold(&thresh[MSM_VDD_MX_RESTRICTION]);

init_exit:
	return;
}

static int msm_thermal_notify(enum thermal_trip_type type, int temp, void *data)
{
	struct therm_threshold *thresh_data = (struct therm_threshold *)data;

	if (thermal_monitor_task) {
		thresh_data->trip_triggered = type;
		thresh_data->parent->thresh_triggered = true;
		complete(&thermal_monitor_complete);
	} else {
		pr_err("Thermal monitor task is not initialized\n");
	}
	return 0;
}

static int init_threshold(enum msm_thresh_list index,
	int sensor_id, int32_t hi_temp, int32_t low_temp,
	void (*callback)(struct therm_threshold *))
{
	int ret = 0, i;
	struct therm_threshold *thresh_ptr;

	if (!callback || index >= MSM_LIST_MAX_NR || index < 0
		|| sensor_id == -ENODEV) {
		pr_err("Invalid input. sensor:%d. index:%d\n",
				sensor_id, index);
		ret = -EINVAL;
		goto init_thresh_exit;
	}
	if (thresh[index].thresh_list) {
		pr_err("threshold id:%d already initialized\n", index);
		ret = -EEXIST;
		goto init_thresh_exit;
	}

	thresh[index].thresh_ct = (sensor_id == MONITOR_ALL_TSENS) ?
						max_tsens_num : 1;
	thresh[index].thresh_triggered = false;
	thresh[index].thresh_list = kzalloc(sizeof(struct therm_threshold) *
					thresh[index].thresh_ct, GFP_KERNEL);
	if (!thresh[index].thresh_list) {
		pr_err("kzalloc failed for thresh index:%d\n", index);
		ret = -ENOMEM;
		goto init_thresh_exit;
	}

	thresh_ptr = thresh[index].thresh_list;
	if (sensor_id == MONITOR_ALL_TSENS) {
		for (i = 0; i < max_tsens_num; i++) {
			thresh_ptr[i].sensor_id = tsens_id_map[i];
			thresh_ptr[i].id_type = THERM_TSENS_ID;
			thresh_ptr[i].notify = callback;
			thresh_ptr[i].trip_triggered = -1;
			thresh_ptr[i].parent = &thresh[index];
			thresh_ptr[i].threshold[0].temp = hi_temp;
			thresh_ptr[i].threshold[0].trip =
				THERMAL_TRIP_CONFIGURABLE_HI;
			thresh_ptr[i].threshold[1].temp = low_temp;
			thresh_ptr[i].threshold[1].trip =
				THERMAL_TRIP_CONFIGURABLE_LOW;
			thresh_ptr[i].threshold[0].notify =
			thresh_ptr[i].threshold[1].notify = msm_thermal_notify;
			thresh_ptr[i].threshold[0].data =
			thresh_ptr[i].threshold[1].data =
				(void *)&thresh_ptr[i];
		}
	} else {
		thresh_ptr->sensor_id = sensor_id;
		thresh_ptr->id_type = THERM_TSENS_ID;
		thresh_ptr->notify = callback;
		thresh_ptr->trip_triggered = -1;
		thresh_ptr->parent = &thresh[index];
		thresh_ptr->threshold[0].temp = hi_temp;
		thresh_ptr->threshold[0].trip =
			THERMAL_TRIP_CONFIGURABLE_HI;
		thresh_ptr->threshold[1].temp = low_temp;
		thresh_ptr->threshold[1].trip =
			THERMAL_TRIP_CONFIGURABLE_LOW;
		thresh_ptr->threshold[0].notify =
		thresh_ptr->threshold[1].notify = msm_thermal_notify;
		thresh_ptr->threshold[0].data =
		thresh_ptr->threshold[1].data = (void *)thresh_ptr;
	}

init_thresh_exit:
	return ret;
}

static int msm_thermal_add_gfx_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *gfx_kobj = NULL;
	int ret = 0;

	if (!gfx_phase_ctrl_enabled)
		return -EINVAL;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		ret = -ENOENT;
		goto gfx_node_exit;
	}

	gfx_kobj = kobject_create_and_add("gfx_phase_ctrl", module_kobj);
	if (!gfx_kobj) {
		pr_err("cannot create gfx kobject\n");
		ret = -ENOMEM;
		goto gfx_node_exit;
	}

	gfx_attr_gp.attrs = kzalloc(sizeof(struct attribute *) * 2, GFP_KERNEL);
	if (!gfx_attr_gp.attrs) {
		pr_err("kzalloc failed\n");
		ret = -ENOMEM;
		goto gfx_node_fail;
	}

	PHASE_RW_ATTR(gfx, temp_band, gfx_mode_attr, 0, gfx_attr_gp);
	gfx_attr_gp.attrs[1] = NULL;

	ret = sysfs_create_group(gfx_kobj, &gfx_attr_gp);
	if (ret) {
		pr_err("cannot create GFX attribute group. err:%d\n", ret);
		goto gfx_node_fail;
	}

gfx_node_fail:
	if (ret) {
		kobject_put(gfx_kobj);
		kfree(gfx_attr_gp.attrs);
		gfx_attr_gp.attrs = NULL;
	}
gfx_node_exit:
	return ret;
}

static int msm_thermal_add_cx_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *cx_kobj = NULL;
	int ret = 0;

	if (!cx_phase_ctrl_enabled)
		return -EINVAL;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		ret = -ENOENT;
		goto cx_node_exit;
	}

	cx_kobj = kobject_create_and_add("cx_phase_ctrl", module_kobj);
	if (!cx_kobj) {
		pr_err("cannot create cx kobject\n");
		ret = -ENOMEM;
		goto cx_node_exit;
	}

	cx_attr_gp.attrs = kzalloc(sizeof(struct attribute *) * 2, GFP_KERNEL);
	if (!cx_attr_gp.attrs) {
		pr_err("kzalloc failed\n");
		ret = -ENOMEM;
		goto cx_node_fail;
	}

	PHASE_RW_ATTR(cx, temp_band, cx_mode_attr, 0, cx_attr_gp);
	cx_attr_gp.attrs[1] = NULL;

	ret = sysfs_create_group(cx_kobj, &cx_attr_gp);
	if (ret) {
		pr_err("cannot create CX attribute group. err:%d\n", ret);
		goto cx_node_fail;
	}

cx_node_fail:
	if (ret) {
		kobject_put(cx_kobj);
		kfree(cx_attr_gp.attrs);
		cx_attr_gp.attrs = NULL;
	}
cx_node_exit:
	return ret;
}

/*
 * We will reset the cpu frequencies limits here. The core online/offline
 * status will be carried over to the process stopping the msm_thermal, as
 * we dont want to online a core and bring in the thermal issues.
 */
static void __ref disable_msm_thermal(void)
{
	uint32_t cpu = 0;

	/* make sure check_temp is no longer running */
	cancel_delayed_work_sync(&check_temp_work);

	get_online_cpus();
	for_each_possible_cpu(cpu) {
		if (cpus[cpu].limited_max_freq == UINT_MAX &&
			cpus[cpu].limited_min_freq == 0)
			continue;
		pr_info("Max frequency reset for CPU%d\n", cpu);
		cpus[cpu].limited_max_freq = UINT_MAX;
		cpus[cpu].limited_min_freq = 0;
		update_cpu_freq(cpu);
	}
	put_online_cpus();
}

static void interrupt_mode_init(void)
{
	if (!msm_thermal_probed) {
		interrupt_mode_enable = true;
		return;
	}
	if (polling_enabled) {
		pr_info("Interrupt mode init\n");
		polling_enabled = 0;
		disable_msm_thermal();
		hotplug_init();
		freq_mitigation_init();
		thermal_monitor_init();
		msm_thermal_add_cx_nodes();
		msm_thermal_add_gfx_nodes();
	}
}

static int __ref set_enabled(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_bool(val, kp);
	if (!enabled)
		interrupt_mode_init();
	else
		pr_info("no action for enabled = %d\n",
			enabled);

	pr_info("enabled = %d\n", enabled);

	return ret;
}

static struct kernel_param_ops module_ops = {
	.set = set_enabled,
	.get = param_get_bool,
};

module_param_cb(enabled, &module_ops, &enabled, 0644);
MODULE_PARM_DESC(enabled, "enforce thermal limit on cpu");

static ssize_t show_cc_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", core_control_enabled);
}

static ssize_t __ref store_cc_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s. err:%d\n", buf, ret);
		goto done_store_cc;
	}

	if (core_control_enabled == !!val)
		goto done_store_cc;

	core_control_enabled = !!val;
	if (core_control_enabled) {
		pr_info("Core control enabled\n");
		register_cpu_notifier(&msm_thermal_cpu_notifier);
		if (hotplug_task)
			complete(&hotplug_notify_complete);
		else
			pr_err("Hotplug task is not initialized\n");
	} else {
		pr_info("Core control disabled\n");
		unregister_cpu_notifier(&msm_thermal_cpu_notifier);
	}

done_store_cc:
	return count;
}

static ssize_t show_cpus_offlined(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cpus_offlined);
}

static ssize_t __ref store_cpus_offlined(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t cpu;

	mutex_lock(&core_control_mutex);
	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s. err:%d\n", buf, ret);
		goto done_cc;
	}

	if (polling_enabled) {
		pr_err("Ignoring request; polling thread is enabled.\n");
		goto done_cc;
	}

	for_each_possible_cpu(cpu) {
		if (!(msm_thermal_info.core_control_mask & BIT(cpu)))
			continue;
		cpus[cpu].user_offline = !!(val & BIT(cpu));
		pr_debug("\"%s\"(PID:%i) requests %s CPU%d.\n", current->comm,
			current->pid, (cpus[cpu].user_offline) ? "offline" :
			"online", cpu);
	}

	if (hotplug_task)
		complete(&hotplug_notify_complete);
	else
		pr_err("Hotplug task is not initialized\n");
done_cc:
	mutex_unlock(&core_control_mutex);
	return count;
}

static __refdata struct kobj_attribute cc_enabled_attr =
__ATTR(enabled, 0644, show_cc_enabled, store_cc_enabled);

static __refdata struct kobj_attribute cpus_offlined_attr =
__ATTR(cpus_offlined, 0644, show_cpus_offlined, store_cpus_offlined);

static __refdata struct attribute *cc_attrs[] = {
	&cc_enabled_attr.attr,
	&cpus_offlined_attr.attr,
	NULL,
};

static __refdata struct attribute_group cc_attr_group = {
	.attrs = cc_attrs,
};
static __init int msm_thermal_add_cc_nodes(void)
{
	struct kobject *module_kobj = NULL;
	int ret = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		ret = -ENOENT;
		goto done_cc_nodes;
	}

	cc_kobj = kobject_create_and_add("core_control", module_kobj);
	if (!cc_kobj) {
		pr_err("cannot create core control kobj\n");
		ret = -ENOMEM;
		goto done_cc_nodes;
	}

	ret = sysfs_create_group(cc_kobj, &cc_attr_group);
	if (ret) {
		pr_err("cannot create sysfs group. err:%d\n", ret);
		goto done_cc_nodes;
	}

	return 0;

done_cc_nodes:
	if (cc_kobj)
		kobject_del(cc_kobj);
	return ret;
}

static ssize_t show_mx_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", vdd_mx_enabled);
}

static ssize_t __ref store_mx_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input %s\n", buf);
		goto done_store_mx;
	}

	if (vdd_mx_enabled == !!val)
		goto done_store_mx;

	vdd_mx_enabled = !!val;

	mutex_lock(&vdd_mx_mutex);
	if (!vdd_mx_enabled)
		remove_vdd_mx_restriction();
	else if (!(convert_to_zone_id(&thresh[MSM_VDD_MX_RESTRICTION])))
		therm_set_threshold(&thresh[MSM_VDD_MX_RESTRICTION]);
	mutex_unlock(&vdd_mx_mutex);

done_store_mx:
	return count;
}

static __init int msm_thermal_add_mx_nodes(void)
{
	struct kobject *module_kobj = NULL;
	int ret = 0;

	if (!vdd_mx_enabled)
		return -EINVAL;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject for module\n");
		ret = -ENOENT;
		goto done_mx_nodes;
	}

	mx_kobj = kobject_create_and_add("vdd_mx", module_kobj);
	if (!mx_kobj) {
		pr_err("cannot create mx restriction kobj\n");
		ret = -ENOMEM;
		goto done_mx_nodes;
	}

	mx_attr_group.attrs = kzalloc(sizeof(struct attribute *) * 2,
					GFP_KERNEL);
	if (!mx_attr_group.attrs) {
		ret = -ENOMEM;
		pr_err("cannot allocate memory for mx_attr_group.attrs");
		goto done_mx_nodes;
	}

	MX_RW_ATTR(mx_enabled_attr, enabled, mx_attr_group);
	mx_attr_group.attrs[1] = NULL;

	ret = sysfs_create_group(mx_kobj, &mx_attr_group);
	if (ret) {
		pr_err("cannot create group\n");
		goto done_mx_nodes;
	}

done_mx_nodes:
	if (ret) {
		if (mx_kobj)
			kobject_del(mx_kobj);
		kfree(mx_attr_group.attrs);
	}
	return ret;
}

int msm_thermal_pre_init(void)
{
	int ret = 0;

	if (tsens_is_ready() <= 0) {
		pr_err("Tsens driver is not ready yet\n");
		return -EPROBE_DEFER;
	}

	ret = tsens_get_max_sensor_num(&max_tsens_num);
	if (ret < 0) {
		pr_err("failed to get max sensor number, err:%d\n", ret);
		return ret;
	}

	if (create_sensor_id_map()) {
		pr_err("Creating sensor id map failed\n");
		ret = -EINVAL;
		goto pre_init_exit;
	}

	if (!thresh) {
		thresh = kzalloc(
				sizeof(struct threshold_info) * MSM_LIST_MAX_NR,
				GFP_KERNEL);
		if (!thresh) {
			pr_err("kzalloc failed\n");
			ret = -ENOMEM;
			goto pre_init_exit;
		}
		memset(thresh, 0, sizeof(struct threshold_info) *
			MSM_LIST_MAX_NR);
	}
pre_init_exit:
	return ret;
}

int msm_thermal_init(struct msm_thermal_data *pdata)
{
	int ret = 0;
	uint32_t cpu;

	for_each_possible_cpu(cpu) {
		cpus[cpu].cpu = cpu;
		cpus[cpu].offline = 0;
		cpus[cpu].user_offline = 0;
		cpus[cpu].hotplug_thresh_clear = false;
		cpus[cpu].max_freq = false;
		cpus[cpu].user_max_freq = UINT_MAX;
		cpus[cpu].user_min_freq = 0;
		cpus[cpu].limited_max_freq = UINT_MAX;
		cpus[cpu].limited_min_freq = 0;
		cpus[cpu].freq_thresh_clear = false;
	}
	BUG_ON(!pdata);
	memcpy(&msm_thermal_info, pdata, sizeof(struct msm_thermal_data));

	if (check_sensor_id(msm_thermal_info.sensor_id)) {
		pr_err("Invalid sensor:%d for polling\n",
				msm_thermal_info.sensor_id);
		return -EINVAL;
	}

	enabled = 1;
	polling_enabled = 1;
	ret = cpufreq_register_notifier(&msm_thermal_cpufreq_notifier,
			CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		pr_err("cannot register cpufreq notifier. err:%d\n", ret);

	INIT_DELAYED_WORK(&check_temp_work, check_temp);
	schedule_delayed_work(&check_temp_work, 0);

	if (num_possible_cpus() > 1)
		register_cpu_notifier(&msm_thermal_cpu_notifier);

	return ret;
}

static int ocr_reg_init(struct platform_device *pdev)
{
	int ret = 0;
	int i, j;

	for (i = 0; i < ocr_rail_cnt; i++) {
		/* Check if vdd_restriction has already initialized any
		 * regualtor handle. If so use the same handle.*/
		for (j = 0; j < rails_cnt; j++) {
			if (!strcmp(ocr_rails[i].name, rails[j].name)) {
				if (rails[j].reg == NULL)
					break;
				ocr_rails[i].phase_reg = rails[j].reg;
				goto reg_init;
			}

		}
		ocr_rails[i].phase_reg = devm_regulator_get(&pdev->dev,
					ocr_rails[i].name);
		if (IS_ERR_OR_NULL(ocr_rails[i].phase_reg)) {
			ret = PTR_ERR(ocr_rails[i].phase_reg);
			if (ret != -EPROBE_DEFER) {
				pr_err("Could not get regulator: %s, err:%d\n",
					ocr_rails[i].name, ret);
				ocr_rails[i].phase_reg = NULL;
				ocr_rails[i].mode = 0;
				ocr_rails[i].init = 0;
			}
			return ret;
		}
reg_init:
		ocr_rails[i].mode = OPTIMUM_CURRENT_MIN;
	}
	return ret;
}

static int vdd_restriction_reg_init(struct platform_device *pdev)
{
	int ret = 0;
	int i;

	for (i = 0; i < rails_cnt; i++) {
		if (rails[i].freq_req == 1) {
			usefreq |= BIT(i);
			check_freq_table();
			/*
			 * Restrict frequency by default until we have made
			 * our first temp reading
			 */
			if (freq_table_get)
				ret = vdd_restriction_apply_freq(&rails[i], 0);
			else
				pr_info("Defer vdd rstr freq init.\n");
		} else {
			rails[i].reg = devm_regulator_get(&pdev->dev,
					rails[i].name);
			if (IS_ERR_OR_NULL(rails[i].reg)) {
				ret = PTR_ERR(rails[i].reg);
				if (ret != -EPROBE_DEFER) {
					pr_err( \
					"could not get regulator: %s. err:%d\n",
					rails[i].name, ret);
					rails[i].reg = NULL;
					rails[i].curr_level = -2;
					return ret;
				}
				pr_info("Defer regulator %s probe\n",
					rails[i].name);
				return ret;
			}
			/*
			 * Restrict votlage by default until we have made
			 * our first temp reading
			 */
			ret = vdd_restriction_apply_voltage(&rails[i], 0);
		}
	}

	return ret;
}

static int psm_reg_init(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	int j = 0;

	for (i = 0; i < psm_rails_cnt; i++) {
		psm_rails[i].reg = rpm_regulator_get(&pdev->dev,
				psm_rails[i].name);
		if (IS_ERR_OR_NULL(psm_rails[i].reg)) {
			ret = PTR_ERR(psm_rails[i].reg);
			if (ret != -EPROBE_DEFER) {
				pr_err("couldn't get rpm regulator %s. err%d\n",
					psm_rails[i].name, ret);
				psm_rails[i].reg = NULL;
				goto psm_reg_exit;
			}
			pr_info("Defer regulator %s probe\n",
					psm_rails[i].name);
			return ret;
		}
		/* Apps default vote for PWM mode */
		psm_rails[i].init = PMIC_PWM_MODE;
		ret = rpm_regulator_set_mode(psm_rails[i].reg,
				psm_rails[i].init);
		if (ret) {
			pr_err("Cannot set PMIC PWM mode. err:%d\n", ret);
			return ret;
		} else
			psm_rails[i].mode = PMIC_PWM_MODE;
	}

	return ret;

psm_reg_exit:
	if (ret) {
		for (j = 0; j < i; j++) {
			if (psm_rails[j].reg != NULL)
				rpm_regulator_put(psm_rails[j].reg);
		}
	}

	return ret;
}

static int msm_thermal_add_vdd_rstr_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *vdd_rstr_kobj = NULL;
	struct kobject *vdd_rstr_reg_kobj[MAX_RAILS] = {0};
	int rc = 0;
	int i = 0;

	if (!vdd_rstr_probed) {
		vdd_rstr_nodes_called = true;
		return rc;
	}

	if (vdd_rstr_probed && rails_cnt == 0)
		return rc;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		rc = -ENOENT;
		goto thermal_sysfs_add_exit;
	}

	vdd_rstr_kobj = kobject_create_and_add("vdd_restriction", module_kobj);
	if (!vdd_rstr_kobj) {
		pr_err("cannot create vdd_restriction kobject\n");
		rc = -ENOMEM;
		goto thermal_sysfs_add_exit;
	}

	rc = sysfs_create_group(vdd_rstr_kobj, &vdd_rstr_en_attribs_gp);
	if (rc) {
		pr_err("cannot create kobject attribute group. err:%d\n", rc);
		rc = -ENOMEM;
		goto thermal_sysfs_add_exit;
	}

	for (i = 0; i < rails_cnt; i++) {
		vdd_rstr_reg_kobj[i] = kobject_create_and_add(rails[i].name,
					vdd_rstr_kobj);
		if (!vdd_rstr_reg_kobj[i]) {
			pr_err("cannot create kobject for %s\n",
					rails[i].name);
			rc = -ENOMEM;
			goto thermal_sysfs_add_exit;
		}

		rails[i].attr_gp.attrs = kzalloc(sizeof(struct attribute *) * 3,
					GFP_KERNEL);
		if (!rails[i].attr_gp.attrs) {
			pr_err("kzalloc failed\n");
			rc = -ENOMEM;
			goto thermal_sysfs_add_exit;
		}

		VDD_RES_RW_ATTRIB(rails[i], rails[i].level_attr, 0, level);
		VDD_RES_RO_ATTRIB(rails[i], rails[i].value_attr, 1, value);
		rails[i].attr_gp.attrs[2] = NULL;

		rc = sysfs_create_group(vdd_rstr_reg_kobj[i],
				&rails[i].attr_gp);
		if (rc) {
			pr_err("cannot create attribute group for %s. err:%d\n",
					rails[i].name, rc);
			goto thermal_sysfs_add_exit;
		}
	}

	return rc;

thermal_sysfs_add_exit:
	if (rc) {
		for (i = 0; i < rails_cnt; i++) {
			kobject_del(vdd_rstr_reg_kobj[i]);
			kfree(rails[i].attr_gp.attrs);
		}
		if (vdd_rstr_kobj)
			kobject_del(vdd_rstr_kobj);
	}
	return rc;
}

static int msm_thermal_add_ocr_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *ocr_kobj = NULL;
	struct kobject *ocr_reg_kobj[MAX_RAILS] = {0};
	int rc = 0;
	int i = 0;

	if (!ocr_probed) {
		ocr_nodes_called = true;
		return rc;
	}

	if (ocr_probed && ocr_rail_cnt == 0)
		return rc;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("Cannot find kobject\n");
		rc = -ENOENT;
		goto ocr_node_exit;
	}

	ocr_kobj = kobject_create_and_add("opt_curr_req", module_kobj);
	if (!ocr_kobj) {
		pr_err("Cannot create ocr kobject\n");
		rc = -ENOMEM;
		goto ocr_node_exit;
	}

	for (i = 0; i < ocr_rail_cnt; i++) {
		ocr_reg_kobj[i] = kobject_create_and_add(ocr_rails[i].name,
					ocr_kobj);
		if (!ocr_reg_kobj[i]) {
			pr_err("Cannot create kobject for %s\n",
				ocr_rails[i].name);
			rc = -ENOMEM;
			goto ocr_node_exit;
		}
		ocr_rails[i].attr_gp.attrs = kzalloc(
				sizeof(struct attribute *) * 2, GFP_KERNEL);
		if (!ocr_rails[i].attr_gp.attrs) {
			pr_err("Fail to allocate memory for attribute for %s\n",
				ocr_rails[i].name);
			rc = -ENOMEM;
			goto ocr_node_exit;
		}

		OCR_RW_ATTRIB(ocr_rails[i], ocr_rails[i].mode_attr, 0, mode);
		ocr_rails[i].attr_gp.attrs[1] = NULL;

		rc = sysfs_create_group(ocr_reg_kobj[i], &ocr_rails[i].attr_gp);
		if (rc) {
			pr_err("Cannot create attribute group for %s. err:%d\n",
				ocr_rails[i].name, rc);
			goto ocr_node_exit;
		}
	}

ocr_node_exit:
	if (rc) {
		for (i = 0; i < ocr_rail_cnt; i++) {
			if (ocr_reg_kobj[i])
				kobject_del(ocr_reg_kobj[i]);
			kfree(ocr_rails[i].attr_gp.attrs);
			ocr_rails[i].attr_gp.attrs = NULL;
		}
		if (ocr_kobj)
			kobject_del(ocr_kobj);
	}
	return rc;
}

static int msm_thermal_add_psm_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *psm_kobj = NULL;
	struct kobject *psm_reg_kobj[MAX_RAILS] = {0};
	int rc = 0;
	int i = 0;

	if (!psm_probed) {
		psm_nodes_called = true;
		return rc;
	}

	if (psm_probed && psm_rails_cnt == 0)
		return rc;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		rc = -ENOENT;
		goto psm_node_exit;
	}

	psm_kobj = kobject_create_and_add("pmic_sw_mode", module_kobj);
	if (!psm_kobj) {
		pr_err("cannot create psm kobject\n");
		rc = -ENOMEM;
		goto psm_node_exit;
	}

	for (i = 0; i < psm_rails_cnt; i++) {
		psm_reg_kobj[i] = kobject_create_and_add(psm_rails[i].name,
					psm_kobj);
		if (!psm_reg_kobj[i]) {
			pr_err("cannot create kobject for %s\n",
					psm_rails[i].name);
			rc = -ENOMEM;
			goto psm_node_exit;
		}
		psm_rails[i].attr_gp.attrs = kzalloc( \
				sizeof(struct attribute *) * 2, GFP_KERNEL);
		if (!psm_rails[i].attr_gp.attrs) {
			pr_err("kzalloc failed\n");
			rc = -ENOMEM;
			goto psm_node_exit;
		}

		PSM_RW_ATTRIB(psm_rails[i], psm_rails[i].mode_attr, 0, mode);
		psm_rails[i].attr_gp.attrs[1] = NULL;

		rc = sysfs_create_group(psm_reg_kobj[i], &psm_rails[i].attr_gp);
		if (rc) {
			pr_err("cannot create attribute group for %s. err:%d\n",
					psm_rails[i].name, rc);
			goto psm_node_exit;
		}
	}

	return rc;

psm_node_exit:
	if (rc) {
		for (i = 0; i < psm_rails_cnt; i++) {
			kobject_del(psm_reg_kobj[i]);
			kfree(psm_rails[i].attr_gp.attrs);
		}
		if (psm_kobj)
			kobject_del(psm_kobj);
	}
	return rc;
}

static int probe_vdd_mx(struct device_node *node,
		struct msm_thermal_data *data, struct platform_device *pdev)
{
	int ret = 0;
	char *key = NULL;

	key = "qcom,mx-restriction-temp";
	ret = of_property_read_u32(node, key, &data->vdd_mx_temp_degC);
	if (ret)
		goto read_node_done;

	key = "qcom,mx-restriction-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->vdd_mx_temp_hyst_degC);
	if (ret)
		goto read_node_done;

	key = "qcom,mx-retention-min";
	ret = of_property_read_u32(node, key, &data->vdd_mx_min);
	if (ret)
		goto read_node_done;

	vdd_mx = devm_regulator_get(&pdev->dev, "vdd-mx");
	if (IS_ERR_OR_NULL(vdd_mx)) {
		ret = PTR_ERR(vdd_mx);
		if (ret != -EPROBE_DEFER) {
			pr_err(
			"Could not get regulator: vdd-mx, err:%d\n", ret);
		}
		goto read_node_done;
	}

	ret = init_threshold(MSM_VDD_MX_RESTRICTION, MONITOR_ALL_TSENS,
			data->vdd_mx_temp_degC + data->vdd_mx_temp_hyst_degC,
			data->vdd_mx_temp_degC, vdd_mx_notify);

read_node_done:
	if (!ret)
		vdd_mx_enabled = true;
	else if (ret != -EPROBE_DEFER)
		dev_info(&pdev->dev,
			"%s:Failed reading node=%s, key=%s. KTM continues\n",
			__func__, node->full_name, key);

	return ret;
}

static int probe_vdd_rstr(struct device_node *node,
		struct msm_thermal_data *data, struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	int arr_size;
	char *key = NULL;
	struct device_node *child_node = NULL;

	rails = NULL;

	key = "qcom,vdd-restriction-temp";
	ret = of_property_read_u32(node, key, &data->vdd_rstr_temp_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,vdd-restriction-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->vdd_rstr_temp_hyst_degC);
	if (ret)
		goto read_node_fail;

	for_each_child_of_node(node, child_node) {
		rails_cnt++;
	}

	if (rails_cnt == 0)
		goto read_node_fail;
	if (rails_cnt >= MAX_RAILS) {
		pr_err("Too many rails:%d.\n", rails_cnt);
		return -EFAULT;
	}

	rails = kzalloc(sizeof(struct rail) * rails_cnt,
				GFP_KERNEL);
	if (!rails) {
		pr_err("Fail to allocate memory for rails.\n");
		return -ENOMEM;
	}

	i = 0;
	for_each_child_of_node(node, child_node) {
		key = "qcom,vdd-rstr-reg";
		ret = of_property_read_string(child_node, key, &rails[i].name);
		if (ret)
			goto read_node_fail;

		key = "qcom,levels";
		if (!of_get_property(child_node, key, &arr_size))
			goto read_node_fail;
		rails[i].num_levels = arr_size/sizeof(__be32);
		if (rails[i].num_levels >
			sizeof(rails[i].levels)/sizeof(uint32_t)) {
			pr_err("Array size:%d too large for index:%d\n",
				rails[i].num_levels, i);
			return -EFAULT;
		}
		ret = of_property_read_u32_array(child_node, key,
				rails[i].levels, rails[i].num_levels);
		if (ret)
			goto read_node_fail;

		key = "qcom,freq-req";
		rails[i].freq_req = of_property_read_bool(child_node, key);
		if (rails[i].freq_req)
			rails[i].min_level = 0;
		else {
			key = "qcom,min-level";
			ret = of_property_read_u32(child_node, key,
				&rails[i].min_level);
			if (ret)
				goto read_node_fail;
		}

		rails[i].curr_level = -1;
		rails[i].reg = NULL;
		i++;
	}

	if (rails_cnt) {
		ret = vdd_restriction_reg_init(pdev);
		if (ret) {
			pr_err("Err regulator init. err:%d. KTM continues.\n",
					ret);
			goto read_node_fail;
		}
		ret = init_threshold(MSM_VDD_RESTRICTION, MONITOR_ALL_TSENS,
			data->vdd_rstr_temp_hyst_degC, data->vdd_rstr_temp_degC,
			vdd_restriction_notify);
		if (ret) {
			pr_err("Error in initializing thresholds. err:%d\n",
					ret);
			goto read_node_fail;
		}
		vdd_rstr_enabled = true;
	}
read_node_fail:
	vdd_rstr_probed = true;
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			__func__, node->full_name, key, ret);
		kfree(rails);
		rails_cnt = 0;
	}
	if (ret == -EPROBE_DEFER)
		vdd_rstr_probed = false;
	return ret;
}

static int probe_ocr(struct device_node *node, struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	int ret = 0;
	int j = 0;
	char *key = NULL;

	if (ocr_probed) {
		pr_info("Nodes already probed\n");
		goto read_ocr_exit;
	}
	ocr_rails = NULL;

	key = "qcom,pmic-opt-curr-temp";
	ret = of_property_read_u32(node, key, &data->ocr_temp_degC);
	if (ret)
		goto read_ocr_fail;

	key = "qcom,pmic-opt-curr-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->ocr_temp_hyst_degC);
	if (ret)
		goto read_ocr_fail;

	key = "qcom,pmic-opt-curr-regs";
	ocr_rail_cnt = of_property_count_strings(node, key);
	if (ocr_rail_cnt <= 0) {
		pr_err("Invalid ocr rail count. err:%d\n", ocr_rail_cnt);
		goto read_ocr_fail;
	}
	ocr_rails = kzalloc(sizeof(struct psm_rail) * ocr_rail_cnt,
			GFP_KERNEL);
	if (!ocr_rails) {
		pr_err("Fail to allocate memory for ocr rails\n");
		ocr_rail_cnt = 0;
		return -ENOMEM;
	}

	for (j = 0; j < ocr_rail_cnt; j++) {
		ret = of_property_read_string_index(node, key, j,
				&ocr_rails[j].name);
		if (ret)
			goto read_ocr_fail;
		ocr_rails[j].phase_reg = NULL;
		ocr_rails[j].init = OPTIMUM_CURRENT_MAX;
	}

	key = "qcom,pmic-opt-curr-sensor-id";
	ret = of_property_read_u32(node, key, &data->ocr_sensor_id);
	if (ret) {
		pr_info("ocr sensor is not configured, use all TSENS. err:%d\n",
			ret);
		data->ocr_sensor_id = MONITOR_ALL_TSENS;
	}

	ret = ocr_reg_init(pdev);
	if (ret) {
		if (ret == -EPROBE_DEFER) {
			ocr_reg_init_defer = true;
			pr_info("ocr reg init is defered\n");
		} else {
			pr_err(
			"Failed to get regulators. KTM continues. err:%d\n",
			ret);
			goto read_ocr_fail;
		}
	}

	ret = init_threshold(MSM_OCR, data->ocr_sensor_id,
		data->ocr_temp_degC,
		data->ocr_temp_degC - data->ocr_temp_hyst_degC,
		ocr_notify);
	if (ret)
		goto read_ocr_fail;

	if (!ocr_reg_init_defer)
		ocr_enabled = true;
	ocr_nodes_called = false;
	/*
	 * Vote for max optimum current by default until we have made
	 * our first temp reading
	 */
	if (ocr_enabled) {
		ret = ocr_set_mode_all(OPTIMUM_CURRENT_MAX);
		if (ret) {
			pr_err("Set max optimum current failed. err:%d\n",
				ret);
			ocr_enabled = false;
		}
	}

read_ocr_fail:
	ocr_probed = true;
	if (ret) {
		if (ret == -EPROBE_DEFER) {
			ret = 0;
			goto read_ocr_exit;
		}
		dev_err(
		&pdev->dev,
		"%s:Failed reading node=%s, key=%s err:%d. KTM continues\n",
		__func__, node->full_name, key, ret);
		kfree(ocr_rails);
		ocr_rails = NULL;
		ocr_rail_cnt = 0;
	}
read_ocr_exit:
	return ret;
}

static int probe_psm(struct device_node *node, struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	int ret = 0;
	int j = 0;
	char *key = NULL;

	psm_rails = NULL;

	key = "qcom,pmic-sw-mode-temp";
	ret = of_property_read_u32(node, key, &data->psm_temp_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,pmic-sw-mode-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->psm_temp_hyst_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,pmic-sw-mode-regs";
	psm_rails_cnt = of_property_count_strings(node, key);
	psm_rails = kzalloc(sizeof(struct psm_rail) * psm_rails_cnt,
			GFP_KERNEL);
	if (!psm_rails) {
		pr_err("Fail to allocate memory for psm rails\n");
		psm_rails_cnt = 0;
		return -ENOMEM;
	}

	for (j = 0; j < psm_rails_cnt; j++) {
		ret = of_property_read_string_index(node, key, j,
				&psm_rails[j].name);
		if (ret)
			goto read_node_fail;
	}

	if (psm_rails_cnt) {
		ret = psm_reg_init(pdev);
		if (ret) {
			pr_err("Err regulator init. err:%d. KTM continues.\n",
					ret);
			goto read_node_fail;
		}
		psm_enabled = true;
	}

read_node_fail:
	psm_probed = true;
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			__func__, node->full_name, key, ret);
		kfree(psm_rails);
		psm_rails_cnt = 0;
	}
	if (ret == -EPROBE_DEFER)
		psm_probed = false;
	return ret;
}

static int probe_cc(struct device_node *node, struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	char *key = NULL;
	uint32_t cpu_cnt = 0;
	int ret = 0;
	uint32_t cpu = 0;

	if (num_possible_cpus() > 1) {
		core_control_enabled = 1;
		hotplug_enabled = 1;
	}

	key = "qcom,core-limit-temp";
	ret = of_property_read_u32(node, key, &data->core_limit_temp_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,core-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->core_temp_hysteresis_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,core-control-mask";
	ret = of_property_read_u32(node, key, &data->core_control_mask);
	if (ret)
		goto read_node_fail;

	key = "qcom,hotplug-temp";
	ret = of_property_read_u32(node, key, &data->hotplug_temp_degC);
	if (ret)
		goto hotplug_node_fail;

	key = "qcom,hotplug-temp-hysteresis";
	ret = of_property_read_u32(node, key,
			&data->hotplug_temp_hysteresis_degC);
	if (ret)
		goto hotplug_node_fail;

	key = "qcom,cpu-sensors";
	cpu_cnt = of_property_count_strings(node, key);
	if (cpu_cnt < num_possible_cpus()) {
		pr_err("Wrong number of cpu sensors:%d\n", cpu_cnt);
		ret = -EINVAL;
		goto hotplug_node_fail;
	}

	for_each_possible_cpu(cpu) {
		ret = of_property_read_string_index(node, key, cpu,
				&cpus[cpu].sensor_type);
		if (ret)
			goto hotplug_node_fail;
	}

read_node_fail:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
		core_control_enabled = 0;
	}

	return ret;

hotplug_node_fail:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
		hotplug_enabled = 0;
	}

	return ret;
}

static int probe_gfx_phase_ctrl(struct device_node *node,
		struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	char *key = NULL;
	const char *tmp_str = NULL;
	int ret = 0;

	key = "qcom,gfx-phase-warm-temp";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_warm_temp_degC);
	if (ret)
		goto probe_gfx_exit;

	key = "qcom,gfx-phase-warm-temp-hyst";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_warm_temp_hyst_degC);
	if (ret)
		goto probe_gfx_exit;

	key = "qcom,gfx-phase-hot-crit-temp";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_hot_temp_degC);
	if (ret)
		goto probe_gfx_exit;

	key = "qcom,gfx-phase-hot-crit-temp-hyst";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_hot_temp_hyst_degC);
	if (ret)
		goto probe_gfx_exit;

	key = "qcom,gfx-sensor-id";
	ret = of_property_read_u32(node, key,
		&data->gfx_sensor);
	if (ret)
		goto probe_gfx_exit;

	key = "qcom,gfx-phase-resource-key";
	ret = of_property_read_string(node, key,
		&tmp_str);
	if (ret)
		goto probe_gfx_exit;
	data->gfx_phase_request_key = msm_thermal_str_to_int(tmp_str);

	ret = init_threshold(MSM_GFX_PHASE_CTRL_WARM, data->gfx_sensor,
		data->gfx_phase_warm_temp_degC, data->gfx_phase_warm_temp_degC -
		data->gfx_phase_warm_temp_hyst_degC,
		gfx_phase_ctrl_notify);
	if (ret) {
		pr_err("init WARM threshold failed. err:%d\n", ret);
		goto probe_gfx_exit;
	}

	ret = init_threshold(MSM_GFX_PHASE_CTRL_HOT, data->gfx_sensor,
		data->gfx_phase_hot_temp_degC, data->gfx_phase_hot_temp_degC -
		data->gfx_phase_hot_temp_hyst_degC,
		gfx_phase_ctrl_notify);
	if (ret) {
		pr_err("init HOT threshold failed. err:%d\n", ret);
		goto probe_gfx_exit;
	}

	gfx_phase_ctrl_enabled = true;

probe_gfx_exit:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
		gfx_phase_ctrl_enabled = false;
	}
	return ret;
}

static int probe_cx_phase_ctrl(struct device_node *node,
		struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	char *key = NULL;
	const char *tmp_str;
	int ret = 0;

	key = "qcom,rpm-phase-resource-type";
	ret = of_property_read_string(node, key,
		&tmp_str);
	if (ret)
		goto probe_cx_exit;
	data->phase_rpm_resource_type = msm_thermal_str_to_int(tmp_str);

	key = "qcom,rpm-phase-resource-id";
	ret = of_property_read_u32(node, key,
		&data->phase_rpm_resource_id);
	if (ret)
		goto probe_cx_exit;

	key = "qcom,cx-phase-resource-key";
	ret = of_property_read_string(node, key,
		&tmp_str);
	if (ret)
		goto probe_cx_exit;
	data->cx_phase_request_key = msm_thermal_str_to_int(tmp_str);

	key = "qcom,cx-phase-hot-crit-temp";
	ret = of_property_read_u32(node, key,
		&data->cx_phase_hot_temp_degC);
	if (ret)
		goto probe_cx_exit;

	key = "qcom,cx-phase-hot-crit-temp-hyst";
	ret = of_property_read_u32(node, key,
		&data->cx_phase_hot_temp_hyst_degC);
	if (ret)
		goto probe_cx_exit;

	ret = init_threshold(MSM_CX_PHASE_CTRL_HOT, MONITOR_ALL_TSENS,
		data->cx_phase_hot_temp_degC, data->cx_phase_hot_temp_degC -
		data->cx_phase_hot_temp_hyst_degC,
		cx_phase_ctrl_notify);
	if (ret) {
		pr_err("init HOT threshold failed. err:%d\n", ret);
		goto probe_cx_exit;
	}

	cx_phase_ctrl_enabled = true;

probe_cx_exit:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
		cx_phase_ctrl_enabled = false;
	}
	return ret;
}

static int probe_therm_reset(struct device_node *node,
		struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	char *key = NULL;
	int ret = 0;

	key = "qcom,therm-reset-temp";
	ret = of_property_read_u32(node, key, &data->therm_reset_temp_degC);
	if (ret)
		goto PROBE_RESET_EXIT;

	ret = init_threshold(MSM_THERM_RESET, MONITOR_ALL_TSENS,
		data->therm_reset_temp_degC, data->therm_reset_temp_degC - 10,
		therm_reset_notify);
	if (ret) {
		pr_err("Therm reset data structure init failed\n");
		goto PROBE_RESET_EXIT;
	}

	therm_reset_enabled = true;

PROBE_RESET_EXIT:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s err=%d. KTM continues\n",
			__func__, node->full_name, key, ret);
		therm_reset_enabled = false;
	}
	return ret;
}

static int probe_freq_mitigation(struct device_node *node,
		struct msm_thermal_data *data,
		struct platform_device *pdev)
{
	char *key = NULL;
	int ret = 0;

	key = "qcom,freq-mitigation-temp";
	ret = of_property_read_u32(node, key, &data->freq_mitig_temp_degc);
	if (ret)
		goto PROBE_FREQ_EXIT;

	key = "qcom,freq-mitigation-temp-hysteresis";
	ret = of_property_read_u32(node, key,
		&data->freq_mitig_temp_hysteresis_degc);
	if (ret)
		goto PROBE_FREQ_EXIT;

	key = "qcom,freq-mitigation-value";
	ret = of_property_read_u32(node, key, &data->freq_limit);
	if (ret)
		goto PROBE_FREQ_EXIT;

	key = "qcom,freq-mitigation-control-mask";
	ret = of_property_read_u32(node, key, &data->freq_mitig_control_mask);
	if (ret)
		goto PROBE_FREQ_EXIT;

	freq_mitigation_enabled = 1;

PROBE_FREQ_EXIT:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			__func__, node->full_name, key, ret);
		freq_mitigation_enabled = 0;
	}
	return ret;
}

static int msm_thermal_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
	char *key = NULL;
	struct device_node *node = pdev->dev.of_node;
	struct msm_thermal_data data;

	memset(&data, 0, sizeof(struct msm_thermal_data));
	data.pdev = pdev;

	ret = msm_thermal_pre_init();
	if (ret) {
		pr_err("thermal pre init failed. err:%d\n", ret);
		goto fail;
	}

	key = "qcom,sensor-id";
	ret = of_property_read_u32(node, key, &data.sensor_id);
	if (ret)
		goto fail;

	key = "qcom,poll-ms";
	ret = of_property_read_u32(node, key, &data.poll_ms);
	if (ret)
		goto fail;

	key = "qcom,limit-temp";
	ret = of_property_read_u32(node, key, &data.limit_temp_degC);
	if (ret)
		goto fail;

	key = "qcom,temp-hysteresis";
	ret = of_property_read_u32(node, key, &data.temp_hysteresis_degC);
	if (ret)
		goto fail;

	key = "qcom,freq-step";
	ret = of_property_read_u32(node, key, &data.bootup_freq_step);
	if (ret)
		goto fail;

	key = "qcom,freq-control-mask";
	ret = of_property_read_u32(node, key, &data.bootup_freq_control_mask);

	ret = probe_cc(node, &data, pdev);

	ret = probe_freq_mitigation(node, &data, pdev);
	ret = probe_cx_phase_ctrl(node, &data, pdev);
	ret = probe_gfx_phase_ctrl(node, &data, pdev);
	ret = probe_therm_reset(node, &data, pdev);

	ret = probe_vdd_mx(node, &data, pdev);
	if (ret == -EPROBE_DEFER)
		goto fail;
	/*
	 * Probe optional properties below. Call probe_psm before
	 * probe_vdd_rstr because rpm_regulator_get has to be called
	 * before devm_regulator_get
	 * probe_ocr should be called after probe_vdd_rstr to reuse the
	 * regualtor handle. calling devm_regulator_get more than once
	 * will fail.
	 */
	ret = probe_psm(node, &data, pdev);
	if (ret == -EPROBE_DEFER)
		goto fail;
	ret = probe_vdd_rstr(node, &data, pdev);
	if (ret == -EPROBE_DEFER)
		goto fail;
	ret = probe_ocr(node, &data, pdev);

	/*
	 * In case sysfs add nodes get called before probe function.
	 * Need to make sure sysfs node is created again
	 */
	if (psm_nodes_called) {
		msm_thermal_add_psm_nodes();
		psm_nodes_called = false;
	}
	if (vdd_rstr_nodes_called) {
		msm_thermal_add_vdd_rstr_nodes();
		vdd_rstr_nodes_called = false;
	}
	if (ocr_nodes_called) {
		msm_thermal_add_ocr_nodes();
		ocr_nodes_called = false;
	}
	msm_thermal_ioctl_init();
	ret = msm_thermal_init(&data);
	msm_thermal_probed = true;

	if (interrupt_mode_enable) {
		interrupt_mode_init();
		interrupt_mode_enable = false;
	}

	return ret;
fail:
	if (ret)
		pr_err("Failed reading node=%s, key=%s. err:%d\n",
			node->full_name, key, ret);

	return ret;
}

static int msm_thermal_dev_exit(struct platform_device *inp_dev)
{
	int i = 0;

	msm_thermal_ioctl_cleanup();
	if (thresh) {
		if (vdd_rstr_enabled)
			kfree(thresh[MSM_VDD_RESTRICTION].thresh_list);
		if (cx_phase_ctrl_enabled)
			kfree(thresh[MSM_CX_PHASE_CTRL_HOT].thresh_list);
		if (gfx_phase_ctrl_enabled) {
			kfree(thresh[MSM_GFX_PHASE_CTRL_WARM].thresh_list);
			kfree(thresh[MSM_GFX_PHASE_CTRL_HOT].thresh_list);
		}
		if (ocr_enabled) {
			for (i = 0; i < ocr_rail_cnt; i++)
				kfree(ocr_rails[i].attr_gp.attrs);
			kfree(ocr_rails);
			ocr_rails = NULL;
			kfree(thresh[MSM_OCR].thresh_list);
		}
		if (vdd_mx_enabled) {
			kfree(mx_kobj);
			kfree(mx_attr_group.attrs);
			kfree(thresh[MSM_VDD_MX_RESTRICTION].thresh_list);
		}
		kfree(thresh);
		thresh = NULL;
	}
	return 0;
}

static struct of_device_id msm_thermal_match_table[] = {
	{.compatible = "qcom,msm-thermal"},
	{},
};

static struct platform_driver msm_thermal_device_driver = {
	.probe = msm_thermal_dev_probe,
	.driver = {
		.name = "msm-thermal",
		.owner = THIS_MODULE,
		.of_match_table = msm_thermal_match_table,
	},
	.remove = msm_thermal_dev_exit,
};

int __init msm_thermal_device_init(void)
{
	return platform_driver_register(&msm_thermal_device_driver);
}
arch_initcall(msm_thermal_device_init);

int __init msm_thermal_late_init(void)
{
	if (num_possible_cpus() > 1)
		msm_thermal_add_cc_nodes();
	msm_thermal_add_psm_nodes();
	msm_thermal_add_vdd_rstr_nodes();
	if (ocr_reg_init_defer) {
		if (!ocr_reg_init(msm_thermal_info.pdev)) {
			ocr_enabled = true;
			msm_thermal_add_ocr_nodes();
		}
	}
	msm_thermal_add_mx_nodes();
	interrupt_mode_init();
	return 0;
}
late_initcall(msm_thermal_late_init);

