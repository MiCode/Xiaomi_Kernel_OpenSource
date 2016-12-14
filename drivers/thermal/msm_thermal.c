/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/msm_thermal_ioctl.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/scm.h>
#include <linux/debugfs.h>
#include <linux/pm_opp.h>
#include <linux/sched/rt.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <soc/qcom/msm-core.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/uio_driver.h>

#include <asm/cacheflush.h>

#define CREATE_TRACE_POINTS
#define TRACE_MSM_THERMAL
#include <trace/trace_thermal.h>

#define MSM_LIMITS_DCVSH		0x10
#define MSM_LIMITS_NODE_DCVS		0x44435653
#define MSM_LIMITS_SUB_FN_GENERAL	0x47454E00
#define MSM_LIMITS_SUB_FN_CRNT		0x43524E54
#define MSM_LIMITS_SUB_FN_REL		0x52454C00
#define MSM_LIMITS_DOMAIN_MAX		0x444D4158
#define MSM_LIMITS_DOMAIN_MIN		0x444D494E
#define MSM_LIMITS_CLUSTER_0		0x6370302D
#define MSM_LIMITS_CLUSTER_1		0x6370312D
#define MSM_LIMITS_ALGO_MODE_ENABLE	0x454E424C

#define MAX_CURRENT_UA 100000
#define MAX_RAILS 5
#define TSENS_NAME_FORMAT "tsens_tz_sensor%d"
#define THERM_SECURE_BITE_CMD 8
#define SENSOR_SCALING_FACTOR 1
#define MSM_THERMAL_NAME "msm_thermal"
#define MSM_TSENS_PRINT  "log_tsens_temperature"
#define CPU_BUF_SIZE 64
#define CPU_DEVICE "cpu%d"
#define MAX_DEBUGFS_CONFIG_LEN   32
#define MSM_THERMAL_CONFIG        "config"
#define MSM_CONFIG_DATA           "data"
#define DEBUGFS_DISABLE_ALL_MIT   "disable"
#define DEBUGFS_CONFIG_UPDATE     "update"
#define MSM_THERMAL_THRESH        "thresh_degc"
#define MSM_THERMAL_THRESH_CLR    "thresh_clr_degc"
#define MSM_THERMAL_THRESH_UPDATE "update"
#define DEVM_NAME_MAX 30
#define HOTPLUG_RETRY_INTERVAL_MS 100
#define UIO_VERSION "1.0"

#define VALIDATE_AND_SET_MASK(_node, _key, _mask, _cpu) \
	do { \
		if (of_property_read_bool(_node, _key)) \
			_mask |= BIT(_cpu); \
	} while (0)

#define THERM_CREATE_DEBUGFS_DIR(_node, _name, _parent, _ret) \
	do { \
		_node = debugfs_create_dir(_name, _parent); \
		if (IS_ERR(_node)) { \
			_ret = PTR_ERR(_node); \
			pr_err("Error creating debugfs dir:%s. err:%d\n", \
					_name, _ret); \
		} \
	} while (0)

#define UPDATE_THRESHOLD_SET(_val, _trip) do {		\
	if (_trip == THERMAL_TRIP_CONFIGURABLE_HI)	\
		_val |= 1;				\
	else if (_trip == THERMAL_TRIP_CONFIGURABLE_LOW)\
		_val |= 2;				\
} while (0)

#define UPDATE_CPU_CONFIG_THRESHOLD(_mask, _id, _high, _low) \
	do { \
		int cpu; \
		for_each_possible_cpu(cpu) { \
			if (!(_mask & BIT(cpus[cpu].cpu))) \
				continue; \
			cpus[cpu].threshold[_id].temp = _high \
				* tsens_scaling_factor; \
			cpus[cpu].threshold[_id + 1].temp = _low \
				* tsens_scaling_factor; \
			set_and_activate_threshold( \
				cpus[cpu].sensor_id, \
				&cpus[cpu].threshold[_id]); \
			set_and_activate_threshold( \
				cpus[cpu].sensor_id, \
				&cpus[cpu].threshold[_id + 1]); \
		} \
	} while (0)

static struct msm_thermal_data msm_thermal_info;
static struct delayed_work check_temp_work, retry_hotplug_work;
static bool core_control_enabled;
static uint32_t cpus_offlined;
static cpumask_var_t cpus_previously_online;
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
static int sensor_cnt;
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
static bool sensor_info_nodes_called;
static bool sensor_info_probed;
static bool psm_enabled;
static bool psm_nodes_called;
static bool psm_probed;
static bool freq_mitigation_enabled;
static bool boot_freq_mitig_enabled;
static bool ocr_enabled;
static bool ocr_nodes_called;
static bool ocr_probed;
static bool ocr_reg_init_defer;
static bool hotplug_enabled;
static bool msm_thermal_probed;
static bool gfx_crit_phase_ctrl_enabled;
static bool gfx_warm_phase_ctrl_enabled;
static bool cx_phase_ctrl_enabled;
static bool vdd_mx_enabled;
static bool therm_reset_enabled;
static bool online_core;
static bool cluster_info_probed;
static bool cluster_info_nodes_called;
static bool in_suspend, retry_in_progress;
static bool lmh_dcvs_available;
static int *tsens_id_map;
static int *zone_id_tsens_map;
static DEFINE_MUTEX(vdd_rstr_mutex);
static DEFINE_MUTEX(psm_mutex);
static DEFINE_MUTEX(cx_mutex);
static DEFINE_MUTEX(gfx_mutex);
static DEFINE_MUTEX(ocr_mutex);
static DEFINE_MUTEX(vdd_mx_mutex);
static DEFINE_MUTEX(threshold_mutex);
static uint32_t curr_gfx_band;
static uint32_t curr_cx_band;
static struct kobj_attribute cx_mode_attr;
static struct kobj_attribute gfx_mode_attr;
static struct kobj_attribute mx_enabled_attr;
static struct attribute_group cx_attr_gp;
static struct attribute_group gfx_attr_gp;
static struct attribute_group mx_attr_group;
static struct regulator *vdd_mx, *vdd_cx;
static int *tsens_temp_at_panic;
static bool tsens_temp_print;
static uint32_t bucket;
static cpumask_t throttling_mask;
static int tsens_scaling_factor = SENSOR_SCALING_FACTOR;

static LIST_HEAD(devices_list);
static LIST_HEAD(thresholds_list);
static int mitigation = 1;

enum thermal_threshold {
	HOTPLUG_THRESHOLD_HIGH,
	HOTPLUG_THRESHOLD_LOW,
	FREQ_THRESHOLD_HIGH,
	FREQ_THRESHOLD_LOW,
	THRESHOLD_MAX_NR,
};

struct cluster_info {
	int cluster_id;
	uint32_t entity_count;
	struct cluster_info *child_entity_ptr;
	struct cluster_info *parent_ptr;
	struct cpufreq_frequency_table *freq_table;
	int freq_idx;
	int freq_idx_low;
	int freq_idx_high;
	struct cpumask cluster_cores;
	uint32_t limited_max_freq;
	uint32_t limited_min_freq;
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
	uint32_t shutdown_max_freq;
	uint32_t suspend_max_freq;
	uint32_t vdd_max_freq;
	uint32_t user_min_freq;
	uint32_t limited_max_freq;
	uint32_t limited_min_freq;
	bool freq_thresh_clear;
	struct cluster_info *parent_ptr;
};

struct rail {
	const char			*name;
	uint32_t			freq_req;
	uint32_t			min_level;
	uint32_t			num_levels;
	int32_t				curr_level;
	uint32_t			levels[3];
	struct kobj_attribute		value_attr;
	struct kobj_attribute		level_attr;
	struct regulator		*reg;
	struct attribute_group		attr_gp;
	uint32_t			max_frequency_limit;
	struct device_clnt_data		*device_handle[NR_CPUS];
	union device_request		request[NR_CPUS];
};

struct msm_sensor_info {
	const char *name;
	const char *alias;
	const char *type;
	uint32_t scaling_factor;
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

struct devmgr_devices {
	struct device_manager_data *hotplug_dev;
	struct device_manager_data *cpufreq_dev[NR_CPUS];
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

enum cpu_mit_type {
	CPU_FREQ_MITIGATION    = 0x1,
	CPU_HOTPLUG_MITIGATION = 0x2,
};

enum cpu_config {
	HOTPLUG_CONFIG,
	CPUFREQ_CONFIG,
	MAX_CPU_CONFIG
};

enum freq_limits {
	FREQ_LIMIT_MIN = 0x1,
	FREQ_LIMIT_MAX = 0x2,
	FREQ_LIMIT_ALL = 0x3,
};

struct msm_thermal_debugfs_thresh_config {
	char config_name[MAX_DEBUGFS_CONFIG_LEN];
	long thresh;
	long thresh_clr;
	bool update;
	void (*disable_config)(void);
	struct dentry *dbg_config;
	struct dentry *dbg_thresh;
	struct dentry *dbg_thresh_clr;
	struct dentry *dbg_thresh_update;
};

struct msm_thermal_debugfs_entry {
	struct dentry *parent;
	struct dentry *tsens_print;
	struct dentry *config;
	struct dentry *config_data;
};

static struct psm_rail *psm_rails;
static struct psm_rail *ocr_rails;
static struct rail *rails;
static struct msm_sensor_info *sensors;
static struct cpu_info cpus[NR_CPUS];
static struct threshold_info *thresh;
static bool mx_restr_applied;
static struct cluster_info *core_ptr;
static struct msm_thermal_debugfs_entry *msm_therm_debugfs;
static struct devmgr_devices *devices;
static struct msm_thermal_debugfs_thresh_config *mit_config;

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

static int thermal_config_debugfs_read(struct seq_file *m, void *data);
static ssize_t thermal_config_debugfs_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *ppos);

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

#define THERM_MITIGATION_DISABLE(_flag, _id) \
	do { \
		if (!_flag) \
			return; \
		if (_id >= 0) \
			sensor_mgr_disable_threshold( \
				&thresh[_id]); \
		_flag = 0; \
	} while (0)

#define APPLY_VDD_RESTRICTION(vdd, level, name, ret)                   \
	do {                                                              \
		ret = regulator_set_voltage(vdd, level, INT_MAX);         \
		if (ret) {                                                \
			pr_err("Failed to vote %s to level %d, err %d\n", \
			 #name, level, ret);                              \
		} else {                                                  \
			ret = regulator_enable(vdd);                      \
			if (ret)                                          \
				pr_err("Failed to enable %s, err %d\n",   \
					#name, ret);                      \
			else                                              \
				pr_debug("Vote %s with level %d\n",       \
					#name, level);                    \
		}                                                         \
	} while (0)

#define REMOVE_VDD_RESTRICTION(vdd, name, ret)                             \
	do {                                                                  \
		ret = regulator_disable(vdd);                                 \
		if (ret) {                                                    \
			pr_err("Failed to disable %s, error %d\n",            \
				#name, ret);                                  \
		} else {                                                      \
			ret = regulator_set_voltage(vdd, 0, INT_MAX);      \
			if (ret)                                              \
				pr_err("Failed to remove %s vote, error %d\n",\
					#name, ret);                          \
			else                                                  \
				pr_debug("Remove voting to %s\n", #name);     \
		}                                                             \
	} while (0)

static void uio_init(struct platform_device *pdev)
{
	int ret = 0;
	struct uio_info *uio_reg_info = NULL;
	struct resource *clnt_res = NULL;
	u32 mem_size = 0;
	phys_addr_t mem_pyhsical = 0;

	clnt_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!clnt_res) {
		pr_debug("resource not found\n");
		goto exit;
	}
	mem_size = resource_size(clnt_res);
	if (mem_size == 0) {
		pr_err("resource memory size is zero\n");
		goto exit;
	}

	uio_reg_info = devm_kzalloc(&pdev->dev, sizeof(struct uio_info),
			GFP_KERNEL);
	if (!uio_reg_info)
		goto exit;

	mem_pyhsical = clnt_res->start;

	/* Setup device */
	uio_reg_info->name = clnt_res->name;
	uio_reg_info->version = UIO_VERSION;
	uio_reg_info->mem[0].addr = mem_pyhsical;
	uio_reg_info->mem[0].size = mem_size;
	uio_reg_info->mem[0].memtype = UIO_MEM_PHYS;

	ret = uio_register_device(&pdev->dev, uio_reg_info);
	if (ret) {
		devm_kfree(&pdev->dev, uio_reg_info);
		pr_err("uio register failed ret=%d\n", ret);
		goto exit;
	}
	dev_set_drvdata(&pdev->dev, uio_reg_info);

exit:
	return;
}

static void get_cluster_mask(uint32_t cpu, cpumask_t *mask)
{
	int i;

	cpumask_set_cpu(cpu, mask);
	if (core_ptr) {
		for (i = 0; i < core_ptr->entity_count; i++) {
			struct cluster_info *cluster_ptr =
				&core_ptr->child_entity_ptr[i];
			if (*cluster_ptr->cluster_cores.bits & BIT(cpu)) {
				cpumask_copy(mask,
					&cluster_ptr->cluster_cores);
				break;
			}
		}
	}
}

static uint32_t get_core_max_freq(uint32_t cpu)
{
	int i;
	uint32_t max_freq = 0;

	if (core_ptr) {
		for (i = 0; i < core_ptr->entity_count; i++) {
			struct cluster_info *cluster_ptr =
				&core_ptr->child_entity_ptr[i];
			if (*cluster_ptr->cluster_cores.bits & BIT(cpu)) {
				if (cluster_ptr->freq_table)
					max_freq =
					cluster_ptr->freq_table
					[cluster_ptr->freq_idx_high].frequency;
				break;
			}
		}
	} else {
		if (table)
			max_freq = table[limit_idx_high].frequency;
	}

	return max_freq;
}

static void cpus_previously_online_update(void)
{
	get_online_cpus();
	cpumask_or(cpus_previously_online, cpus_previously_online,
		   cpu_online_mask);
	put_online_cpus();
	pr_debug("%*pb\n", cpumask_pr_args(cpus_previously_online));
}

static uint32_t get_core_min_freq(uint32_t cpu)
{
	int i;
	uint32_t min_freq = UINT_MAX;

	if (core_ptr) {
		for (i = 0; i < core_ptr->entity_count; i++) {
			struct cluster_info *cluster_ptr =
				&core_ptr->child_entity_ptr[i];
			if (*cluster_ptr->cluster_cores.bits & BIT(cpu)) {
				if (cluster_ptr->freq_table)
					min_freq =
					cluster_ptr->freq_table[0].frequency;
				break;
			}
		}
	} else {
		if (table)
			min_freq = table[0].frequency;
	}

	return min_freq;
}

static void msm_thermal_update_freq(bool is_shutdown, bool mitigate)
{
	uint32_t cpu;
	bool update = false;

	for_each_possible_cpu(cpu) {
		if (msm_thermal_info.freq_mitig_control_mask
			& BIT(cpu)) {
			uint32_t *freq = (is_shutdown)
				? &cpus[cpu].shutdown_max_freq
				: &cpus[cpu].suspend_max_freq;
			uint32_t mitigation_freq = (mitigate) ?
				get_core_min_freq(cpu) : UINT_MAX;

			if (*freq == mitigation_freq)
				continue;
			*freq = mitigation_freq;
			update = true;
			pr_debug("%s mitigate CPU%u to %u\n",
				(is_shutdown) ? "Shutdown" : "Suspend", cpu,
				mitigation_freq);
		}
	}

	if (!update)
		goto notify_exit;

	if (freq_mitigation_task)
		complete(&freq_mitigation_complete);
	else
		pr_err("Freq mitigation task is not initialized\n");
notify_exit:
	return;
}

static int msm_thermal_power_down_callback(
		struct notifier_block *nfb, unsigned long action, void *data)
{

	switch (action) {
	case SYS_RESTART:
	case SYS_POWER_OFF:
	case SYS_HALT:
		msm_thermal_update_freq(true, true);
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static int msm_thermal_suspend_callback(
		struct notifier_block *nfb, unsigned long action, void *data)
{
	switch (action) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		msm_thermal_update_freq(false, true);
		in_suspend = true;
		retry_in_progress = false;
		cancel_delayed_work_sync(&retry_hotplug_work);
		break;

	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		msm_thermal_update_freq(false, false);
		in_suspend = false;
		if (hotplug_task)
			complete(&hotplug_notify_complete);
		else
			pr_debug("Hotplug task not initialized\n");
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block msm_thermal_reboot_notifier = {
	.notifier_call = msm_thermal_power_down_callback,
};

static struct device_manager_data *find_device_by_name(const char *device_name)
{
	struct device_manager_data *dev_mgr = NULL;

	list_for_each_entry(dev_mgr, &devices_list, dev_ptr) {
		if (strcmp(dev_mgr->device_name, device_name) == 0)
			return dev_mgr;
	}

	return NULL;
}

static int validate_client(struct device_clnt_data *clnt)
{
	int ret = 0;
	struct device_manager_data *dev_mgr = NULL;
	struct device_clnt_data *client_ptr = NULL;

	if (!clnt || !clnt->dev_mgr) {
		pr_err("Invalid client\n");
		ret = -EINVAL;
		goto validate_exit;
	}

	list_for_each_entry(dev_mgr, &devices_list, dev_ptr) {
		if (dev_mgr == clnt->dev_mgr)
			break;
	}
	if (dev_mgr != clnt->dev_mgr) {
		pr_err("Invalid device manager\n");
		ret = -EINVAL;
		goto validate_exit;
	}

	mutex_lock(&dev_mgr->clnt_lock);
	list_for_each_entry(client_ptr, &dev_mgr->client_list, clnt_ptr) {
		if (clnt == client_ptr)
			break;
	}
	if (clnt != client_ptr) {
		pr_err("Invalid client\n");
		ret = -EINVAL;
		goto validate_unlock;
	}
validate_unlock:
	mutex_unlock(&dev_mgr->clnt_lock);

validate_exit:
	return ret;
}

static int devmgr_client_cpufreq_update(struct device_manager_data *dev_mgr)
{
	int ret = 0;
	struct device_clnt_data *clnt = NULL;
	uint32_t max_freq = UINT_MAX;
	uint32_t min_freq = 0;

	mutex_lock(&dev_mgr->clnt_lock);
	list_for_each_entry(clnt, &dev_mgr->client_list, clnt_ptr) {
		if (!clnt->req_active)
			continue;
		max_freq = min(max_freq, clnt->request.freq.max_freq);
		min_freq = max(min_freq, clnt->request.freq.min_freq);
	}
	if (dev_mgr->active_req.freq.max_freq == max_freq &&
		dev_mgr->active_req.freq.min_freq == min_freq) {
		goto update_exit;
	}
	dev_mgr->active_req.freq.max_freq = max_freq;
	dev_mgr->active_req.freq.min_freq = min_freq;

	if (freq_mitigation_task) {
		complete(&freq_mitigation_complete);
	} else {
		pr_err("Frequency mitigation task is not initialized\n");
		ret = -ESRCH;
	}

update_exit:
	mutex_unlock(&dev_mgr->clnt_lock);
	return ret;
}

static int devmgr_client_hotplug_update(struct device_manager_data *dev_mgr)
{
	int ret = 0;
	struct device_clnt_data *clnt = NULL;
	cpumask_t offline_mask = CPU_MASK_NONE;

	mutex_lock(&dev_mgr->clnt_lock);
	list_for_each_entry(clnt, &dev_mgr->client_list, clnt_ptr) {
		if (!clnt->req_active)
			continue;
		cpumask_or(&offline_mask, &offline_mask,
				&clnt->request.offline_mask);
	}
	if (cpumask_equal(&dev_mgr->active_req.offline_mask, &offline_mask))
		goto update_exit;

	cpumask_copy(&dev_mgr->active_req.offline_mask, &offline_mask);

	if (hotplug_task) {
		complete(&hotplug_notify_complete);
	} else {
		pr_err("Hotplug task is not initialized\n");
		ret = -ESRCH;
	}

update_exit:
	mutex_unlock(&dev_mgr->clnt_lock);
	return ret;
}

static int devmgr_hotplug_client_request_validate_and_update(
				struct device_clnt_data *clnt,
				union device_request *req,
				enum device_req_type type)
{
	if (type != HOTPLUG_MITIGATION_REQ)
		return -EINVAL;

	cpumask_copy(&clnt->request.offline_mask, &req->offline_mask);

	if (!cpumask_empty(&req->offline_mask))
		clnt->req_active = true;
	else
		clnt->req_active = false;

	return 0;
}

static int devmgr_cpufreq_client_request_validate_and_update(
						struct device_clnt_data *clnt,
						union device_request *req,
						enum device_req_type type)
{
	if (type != CPUFREQ_MITIGATION_REQ)
		return -EINVAL;

	if (req->freq.max_freq < req->freq.min_freq) {
		pr_err("Invalid Max and Min freq req. max:%u min:%u\n",
			req->freq.max_freq, req->freq.min_freq);
		return -EINVAL;
	}

	clnt->request.freq.max_freq = req->freq.max_freq;
	clnt->request.freq.min_freq = req->freq.min_freq;

	if ((req->freq.max_freq == CPUFREQ_MAX_NO_MITIGATION) &&
		(req->freq.min_freq == CPUFREQ_MIN_NO_MITIGATION))
		clnt->req_active = false;
	else
		clnt->req_active = true;

	return 0;
}

int devmgr_client_request_mitigation(struct device_clnt_data *clnt,
					enum device_req_type type,
					union device_request *req)
{
	int ret = 0;
	struct device_manager_data *dev_mgr = NULL;

	if (!mitigation) {
		pr_err("Thermal Mitigations disabled.\n");
		goto req_exit;
	}

	if (!clnt || !req) {
		pr_err("Invalid inputs for mitigation.\n");
		ret = -EINVAL;
		goto req_exit;
	}

	ret = validate_client(clnt);
	if (ret) {
		pr_err("Invalid mitigation client. ret:%d\n", ret);
		goto req_exit;
	}

	if (!clnt->dev_mgr->request_validate) {
		pr_err("Invalid dev mgr request update\n");
		ret = -EINVAL;
		goto req_exit;
	}

	dev_mgr = clnt->dev_mgr;
	mutex_lock(&dev_mgr->clnt_lock);
	ret = dev_mgr->request_validate(clnt, req, type);
	if (ret) {
		pr_err("Invalid client request\n");
		goto req_unlock;
	}

req_unlock:
	mutex_unlock(&dev_mgr->clnt_lock);
	if (!ret && dev_mgr->update)
		dev_mgr->update(dev_mgr);

req_exit:
	return ret;
}

struct device_clnt_data *devmgr_register_mitigation_client(struct device *dev,
				const char *device_name,
				void (*callback)(struct device_clnt_data *,
				union device_request *, void *))
{
	struct device_clnt_data *client = NULL;
	struct device_manager_data *dev_mgr = NULL;

	if (!dev || !device_name) {
		pr_err("Invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	dev_mgr = find_device_by_name(device_name);
	if (!dev_mgr) {
		pr_err("Invalid device %s\n", device_name);
		return ERR_PTR(-EINVAL);
	}

	client = devm_kzalloc(dev,
		sizeof(struct device_clnt_data), GFP_KERNEL);
	if (!client) {
		pr_err("Memory alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_lock(&dev_mgr->clnt_lock);
	client->dev_mgr = dev_mgr;
	client->callback = callback;
	list_add_tail(&client->clnt_ptr, &dev_mgr->client_list);
	mutex_unlock(&dev_mgr->clnt_lock);

	return client;
}

void devmgr_unregister_mitigation_client(struct device *dev,
					struct device_clnt_data *clnt)
{
	int ret = 0;
	struct device_manager_data *dev_mgr = NULL;

	if (!clnt) {
		pr_err("Invalid input\n");
		return;
	}

	ret = validate_client(clnt);
	if (ret)
		return;

	dev_mgr = clnt->dev_mgr;
	mutex_lock(&dev_mgr->clnt_lock);
	list_del(&clnt->clnt_ptr);
	mutex_unlock(&dev_mgr->clnt_lock);
	devm_kfree(dev, clnt);
	if (dev_mgr->update)
		dev_mgr->update(dev_mgr);
}

static int  msm_thermal_cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	uint32_t max_freq_req, min_freq_req;

	switch (event) {
	case CPUFREQ_ADJUST:
		max_freq_req = (lmh_dcvs_available) ? UINT_MAX :
			cpus[policy->cpu].parent_ptr->limited_max_freq;
		min_freq_req = cpus[policy->cpu].parent_ptr->limited_min_freq;
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

static int msm_lmh_dcvs_write(uint32_t node_id, uint32_t fn, uint32_t setting,
				uint32_t val)
{
	int ret;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;

	payload = kzalloc(sizeof(uint32_t) * 5, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	payload[0] = fn;
	payload[1] = 0; /* unused sub-algorithm */
	payload[2] = setting;
	payload[3] = 1; /* number of values */
	payload[4] = val;

	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = sizeof(uint32_t) * 5;
	desc_arg.args[2] = MSM_LIMITS_NODE_DCVS;
	desc_arg.args[3] = node_id;
	desc_arg.args[4] = 0; /* version */
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL,
					SCM_VAL, SCM_VAL);

	dmac_flush_range(payload, (void *)payload + 5 * (sizeof(uint32_t)));
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, MSM_LIMITS_DCVSH), &desc_arg);

	kfree(payload);
	return ret;
}

static int msm_lmh_dcvs_update(int cpu)
{
	uint32_t id = cpus[cpu].parent_ptr->cluster_id;
	uint32_t max_freq = cpus[cpu].limited_max_freq;
	uint32_t min_freq = cpus[cpu].limited_min_freq;
	uint32_t affinity;
	int ret;

	switch (id) {
	case 0:
		affinity = MSM_LIMITS_CLUSTER_0;
		break;
	case 1:
		affinity = MSM_LIMITS_CLUSTER_1;
		break;
	default:
		pr_err("%s: unknown affinity %d\n", __func__, id);
		return -EINVAL;
	};

	ret = msm_lmh_dcvs_write(affinity, MSM_LIMITS_SUB_FN_GENERAL,
					MSM_LIMITS_DOMAIN_MAX, max_freq);
	if (ret)
		return ret;

	ret = msm_lmh_dcvs_write(affinity, MSM_LIMITS_SUB_FN_GENERAL,
					MSM_LIMITS_DOMAIN_MIN, min_freq);
	if (ret)
		return ret;
	/*
	 * Notify LMH dcvs driver about the new software limit. This will
	 * trigger LMH DCVS driver polling for the mitigated frequency.
	 */
	msm_lmh_dcvsh_sw_notify(cpu);

	return ret;
}

static void update_cpu_freq(int cpu, enum freq_limits changed)
{
	int ret = 0;
	cpumask_t mask;

	/*
	 * If the limits overshoot each other, choose the min requirement
	 * over the max freq requirement.
	 */
	if (cpus[cpu].limited_min_freq > cpus[cpu].limited_max_freq)
		cpus[cpu].limited_max_freq = cpus[cpu].limited_min_freq;

	get_cluster_mask(cpu, &mask);
	if (cpu_online(cpu)) {
		if ((cpumask_intersects(&mask, &throttling_mask))
			&& (cpus[cpu].limited_max_freq
				>= get_core_max_freq(cpu))) {
			cpumask_xor(&throttling_mask, &mask, &throttling_mask);
			set_cpu_throttled(&mask, false);
		} else if (!cpumask_intersects(&mask, &throttling_mask)) {
			cpumask_or(&throttling_mask, &mask, &throttling_mask);
			set_cpu_throttled(&mask, true);
		}
		trace_thermal_pre_frequency_mit(cpu,
			cpus[cpu].limited_max_freq,
			cpus[cpu].limited_min_freq);

		/*
		 * If LMH DCVS is available, we update the hardware directly
		 * for faster response. However, the LMH DCVS does not aggregate
		 * min freq correctly - cpufreq could be voting for a min
		 * freq lesser than what we desire and that would be honored.
		 * Update cpufreq, so the min freq remains consistent in the hw.
		 */
		if (lmh_dcvs_available) {
			msm_lmh_dcvs_update(cpu);
			if (changed & FREQ_LIMIT_MIN)
				cpufreq_update_policy(cpu);
		} else {
			cpufreq_update_policy(cpu);
		}

		trace_thermal_post_frequency_mit(cpu,
			cpufreq_quick_get_max(cpu),
			cpus[cpu].limited_min_freq);
		if (ret)
			pr_err("Unable to update policy for cpu:%d. err:%d\n",
				cpu, ret);
	}
}

static ssize_t cluster_info_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	uint32_t i = 0;
	ssize_t tot_size = 0, size = 0;

	for (; i < core_ptr->entity_count; i++) {
		struct cluster_info *cluster_ptr =
				&core_ptr->child_entity_ptr[i];

		size = snprintf(&buf[tot_size], PAGE_SIZE - tot_size,
			"%d:%lu:1 ", cluster_ptr->cluster_id,
			*cluster_ptr->cluster_cores.bits);
		if ((tot_size + size) >= PAGE_SIZE) {
			pr_err("Not enough buffer size");
			break;
		}
		tot_size += size;
	}

	return tot_size;
}

static int thermal_config_debugfs_open(struct inode *inode,
					struct file *file)
{
	return single_open(file, thermal_config_debugfs_read,
				inode->i_private);
}

static const struct file_operations thermal_debugfs_config_ops = {
	.open = thermal_config_debugfs_open,
	.read = seq_read,
	.write = thermal_config_debugfs_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int create_config_debugfs(
		struct msm_thermal_debugfs_thresh_config *config_ptr,
		struct dentry *parent)
{
	int ret = 0;

	if (!strlen(config_ptr->config_name))
		return -ENODEV;

	THERM_CREATE_DEBUGFS_DIR(config_ptr->dbg_config,
		config_ptr->config_name, parent, ret);
	if (ret)
		goto create_exit;

	config_ptr->dbg_thresh = debugfs_create_u64(MSM_THERMAL_THRESH,
		0600, config_ptr->dbg_config, (u64 *)&config_ptr->thresh);
	if (IS_ERR(config_ptr->dbg_thresh)) {
		ret = PTR_ERR(config_ptr->dbg_thresh);
		pr_err("Error creating thresh debugfs:[%s]. error:%d\n",
			config_ptr->config_name, ret);
		goto create_exit;
	}

	config_ptr->dbg_thresh_clr = debugfs_create_u64(MSM_THERMAL_THRESH_CLR,
		0600, config_ptr->dbg_config, (u64 *)&config_ptr->thresh_clr);
	if (IS_ERR(config_ptr->dbg_thresh)) {
		ret = PTR_ERR(config_ptr->dbg_thresh);
		pr_err("Error creating thresh_clr debugfs:[%s]. error:%d\n",
			config_ptr->config_name, ret);
		goto create_exit;
	}

	config_ptr->dbg_thresh_update = debugfs_create_bool(
		MSM_THERMAL_THRESH_UPDATE, 0600, config_ptr->dbg_config,
		&config_ptr->update);
	if (IS_ERR(config_ptr->dbg_thresh_update)) {
		ret = PTR_ERR(config_ptr->dbg_thresh_update);
		pr_err("Error creating enable debugfs:[%s]. error:%d\n",
			config_ptr->config_name, ret);
		goto create_exit;
	}

create_exit:
	if (ret)
		debugfs_remove_recursive(parent);

	return ret;
}

static int create_thermal_debugfs(void)
{
	int ret = 0, idx = 0;

	if (msm_therm_debugfs)
		return ret;

	msm_therm_debugfs = devm_kzalloc(&msm_thermal_info.pdev->dev,
			sizeof(struct msm_thermal_debugfs_entry), GFP_KERNEL);
	if (!msm_therm_debugfs) {
		ret = -ENOMEM;
		pr_err("Memory alloc failed. err:%d\n", ret);
		return ret;
	}

	THERM_CREATE_DEBUGFS_DIR(msm_therm_debugfs->parent, MSM_THERMAL_NAME,
		NULL, ret);
	if (ret)
		goto create_exit;

	msm_therm_debugfs->tsens_print = debugfs_create_bool(MSM_TSENS_PRINT,
			0600, msm_therm_debugfs->parent, &tsens_temp_print);
	if (IS_ERR(msm_therm_debugfs->tsens_print)) {
		ret = PTR_ERR(msm_therm_debugfs->tsens_print);
		pr_err("Error creating debugfs:[%s]. err:%d\n",
			MSM_TSENS_PRINT, ret);
		goto create_exit;
	}

	THERM_CREATE_DEBUGFS_DIR(msm_therm_debugfs->config, MSM_THERMAL_CONFIG,
		msm_therm_debugfs->parent, ret);
	if (ret)
		goto create_exit;

	msm_therm_debugfs->config_data = debugfs_create_file(MSM_CONFIG_DATA,
			0600, msm_therm_debugfs->config, NULL,
			&thermal_debugfs_config_ops);
	if (!msm_therm_debugfs->config_data) {
		pr_err("Error creating debugfs:[%s]\n",
			MSM_CONFIG_DATA);
		goto create_exit;
	}
	for (idx = 0; idx < MSM_LIST_MAX_NR + MAX_CPU_CONFIG; idx++)
		create_config_debugfs(&mit_config[idx],
			msm_therm_debugfs->config);

create_exit:
	if (ret) {
		debugfs_remove_recursive(msm_therm_debugfs->parent);
		devm_kfree(&msm_thermal_info.pdev->dev, msm_therm_debugfs);
	}
	return ret;
}

static struct kobj_attribute cluster_info_attr = __ATTR_RO(cluster_info);
static int create_cpu_topology_sysfs(void)
{
	int ret = 0;
	struct kobject *module_kobj = NULL;

	if (!cluster_info_probed) {
		cluster_info_nodes_called = true;
		return ret;
	}
	if (!core_ptr)
		return ret;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		return -ENODEV;
	}

	sysfs_attr_init(&cluster_info_attr.attr);
	ret = sysfs_create_file(module_kobj, &cluster_info_attr.attr);
	if (ret) {
		pr_err("cannot create cluster info attr group. err:%d\n", ret);
		return ret;
	}

	return ret;
}

static int get_device_tree_cluster_info(struct device *dev, int *cluster_id,
			cpumask_t *cluster_cpus)
{
	int idx = 0, ret = 0, max_entry = 0, core_cnt = 0, c_idx = 0, cpu = 0;
	uint32_t val = 0;
	char *key = "qcom,synchronous-cluster-map";
	struct device_node *core_phandle = NULL;

	if (!of_get_property(dev->of_node, key, &max_entry)
		|| max_entry <= 0) {
		pr_debug("Property %s not defined.\n", key);
		return -ENODEV;
	}
	max_entry /= sizeof(__be32);

	for (idx = 0; idx < max_entry; idx++, c_idx++) {
		/* Read Cluster ID */
		ret = of_property_read_u32_index(dev->of_node, key, idx++,
			&val);
		if (ret) {
			pr_err("Error reading index%d. err:%d\n", idx - 1,
				ret);
			return -EINVAL;
		}
		/* Read number of cores inside a cluster */
		cluster_id[c_idx] = val;
		cpumask_clear(&cluster_cpus[c_idx]);
		ret = of_property_read_u32_index(dev->of_node, key, idx,
			&val);
		if (ret || val < 1) {
			pr_err("Invalid core count[%d] for Cluster%d. err:%d\n"
					, val, cluster_id[c_idx - 1], ret);
			return -EINVAL;
		}
		core_cnt = val + idx;
		/* map the cores to logical CPUs and get sibiling mask */
		for (; core_cnt != idx; core_cnt--) {
			core_phandle = of_parse_phandle(dev->of_node, key,
						core_cnt);
			if (!core_phandle) {
				pr_debug("Invalid phandle. core%d cluster%d\n",
					core_cnt, cluster_id[c_idx - 1]);
				continue;
			}

			for_each_possible_cpu(cpu) {
				if (of_get_cpu_node(cpu, NULL)
					== core_phandle)
					break;
			}
			if (cpu >= num_possible_cpus()) {
				pr_debug("Skipping core%d in cluster%d\n",
					core_cnt, cluster_id[c_idx - 1]);
				continue;
			}
			cpumask_set_cpu(cpu, &cluster_cpus[c_idx]);
		}
		idx += val;
	}

	return c_idx;
}

static int get_kernel_cluster_info(int *cluster_id, cpumask_t *cluster_cpus)
{
	uint32_t _cpu, cluster_index, cluster_cnt;

	for (_cpu = 0, cluster_cnt = 0; _cpu < num_possible_cpus(); _cpu++) {
		if (topology_physical_package_id(_cpu) < 0) {
			pr_err("CPU%d topology not initialized.\n", _cpu);
			return -ENODEV;
		}
		/* Do not use the sibling cpumask from topology module.
		** kernel topology module updates the sibling cpumask
		** only when the cores are brought online for the first time.
		** KTM figures out the sibling cpumask using the
		** cluster and core ID mapping.
		*/
		for (cluster_index = 0; cluster_index < num_possible_cpus();
			cluster_index++) {
			if (cluster_id[cluster_index] == -1) {
				cluster_id[cluster_index] =
					topology_physical_package_id(_cpu);
				cpumask_clear(&cluster_cpus[cluster_index]);
				cpumask_set_cpu(_cpu,
					&cluster_cpus[cluster_index]);
				cluster_cnt++;
				break;
			}
			if (cluster_id[cluster_index] ==
				topology_physical_package_id(_cpu)) {
				cpumask_set_cpu(_cpu,
					&cluster_cpus[cluster_index]);
				break;
			}
		}
	}

	return cluster_cnt;
}

static void update_cpu_topology(struct device *dev)
{
	int cluster_id[NR_CPUS] = {[0 ... NR_CPUS-1] = -1};
	cpumask_t cluster_cpus[NR_CPUS];
	uint32_t i;
	int cluster_cnt;
	struct cluster_info *temp_ptr = NULL;

	cluster_info_probed = true;
	cluster_cnt = get_kernel_cluster_info(cluster_id, cluster_cpus);
	if (cluster_cnt <= 0) {
		cluster_cnt = get_device_tree_cluster_info(dev, cluster_id,
						cluster_cpus);
		if (cluster_cnt <= 0) {
			core_ptr = NULL;
			pr_debug("Cluster Info not defined. KTM continues.\n");
			return;
		}
	}

	core_ptr = devm_kzalloc(dev, sizeof(struct cluster_info), GFP_KERNEL);
	if (!core_ptr) {
		pr_err("Memory alloc failed\n");
		return;
	}
	core_ptr->parent_ptr = NULL;
	core_ptr->entity_count = cluster_cnt;
	core_ptr->cluster_id = -1;

	temp_ptr = devm_kzalloc(dev, sizeof(struct cluster_info) * cluster_cnt,
					GFP_KERNEL);
	if (!temp_ptr) {
		pr_err("Memory alloc failed\n");
		devm_kfree(dev, core_ptr);
		core_ptr = NULL;
		return;
	}

	for (i = 0; i < cluster_cnt; i++) {
		int idx = 0;

		pr_debug("Cluster_ID:%d CPU's:%lu\n", cluster_id[i],
				*cpumask_bits(&cluster_cpus[i]));
		temp_ptr[i].cluster_id = cluster_id[i];
		temp_ptr[i].parent_ptr = core_ptr;
		cpumask_copy(&temp_ptr[i].cluster_cores, &cluster_cpus[i]);
		temp_ptr[i].limited_max_freq = UINT_MAX;
		temp_ptr[i].limited_min_freq = 0;
		temp_ptr[i].freq_idx = 0;
		temp_ptr[i].freq_idx_low = 0;
		temp_ptr[i].freq_idx_high = 0;
		temp_ptr[i].freq_table = NULL;
		temp_ptr[i].entity_count = cpumask_weight(&cluster_cpus[i]);
		for_each_cpu(idx, &temp_ptr[i].cluster_cores) {
			cpus[idx].parent_ptr = &temp_ptr[i];
		}
		temp_ptr[i].child_entity_ptr = NULL;
	}
	core_ptr->child_entity_ptr = temp_ptr;
}

static int get_cpu_freq_plan_len(int cpu)
{
	int table_len = 0;
	struct device *cpu_dev = NULL;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("Error in get CPU%d device\n", cpu);
		goto exit;
	}

	rcu_read_lock();
	table_len = dev_pm_opp_get_opp_count(cpu_dev);
	if (table_len <= 0) {
		pr_err("Error reading CPU%d freq table len. error:%d\n",
			cpu, table_len);
		table_len = 0;
		goto unlock_and_exit;
	}

unlock_and_exit:
	rcu_read_unlock();

exit:
	return table_len;
}

static int get_cpu_freq_plan(int cpu,
		 struct cpufreq_frequency_table *freq_table_ptr)
{
	int table_len = 0;
	struct dev_pm_opp *opp = NULL;
	unsigned long freq = 0;
	struct device *cpu_dev = NULL;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("Error in get CPU%d device\n", cpu);
		goto exit;
	}

	rcu_read_lock();
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq))) {
		/* Convert from Hz to kHz */
		freq_table_ptr[table_len].frequency = freq / 1000;
		pr_debug("cpu%d freq %d :%d\n", cpu, table_len,
			freq_table_ptr[table_len].frequency);
		freq++;
		table_len++;
	}
	rcu_read_unlock();

exit:
	return table_len;
}

static int init_cluster_freq_table(void)
{
	uint32_t _cluster = 0;
	int table_len = 0;
	int ret = 0;
	struct cluster_info *cluster_ptr = NULL;

	for (; _cluster < core_ptr->entity_count; _cluster++, table_len = 0) {
		cluster_ptr = &core_ptr->child_entity_ptr[_cluster];
		if (cluster_ptr->freq_table)
			continue;

		table_len = get_cpu_freq_plan_len(
				cpumask_first(&cluster_ptr->cluster_cores));
		if (!table_len) {
			ret = -EAGAIN;
			continue;
		}
		cluster_ptr->freq_idx_low = 0;
		cluster_ptr->freq_idx_high = cluster_ptr->freq_idx =
				table_len - 1;
		if (cluster_ptr->freq_idx_high < 0
			|| (cluster_ptr->freq_idx_high
			< cluster_ptr->freq_idx_low)) {
			cluster_ptr->freq_idx = cluster_ptr->freq_idx_low =
				cluster_ptr->freq_idx_high = 0;
			WARN(1, "Cluster%d frequency table length:%d\n",
				cluster_ptr->cluster_id, table_len);
			ret = -EINVAL;
			goto exit;
		}
		cluster_ptr->freq_table = kzalloc(
			sizeof(struct cpufreq_frequency_table) * table_len,
			GFP_KERNEL);
		if (!cluster_ptr->freq_table) {
			pr_err("memory alloc failed\n");
			cluster_ptr->freq_idx = cluster_ptr->freq_idx_low =
				cluster_ptr->freq_idx_high = 0;
			ret = -ENOMEM;
			goto exit;
		}
		table_len = get_cpu_freq_plan(
				cpumask_first(&cluster_ptr->cluster_cores),
				cluster_ptr->freq_table);
		if (!table_len) {
			kfree(cluster_ptr->freq_table);
			cluster_ptr->freq_table = NULL;
			pr_err("Error reading cluster%d cpufreq table\n",
				cluster_ptr->cluster_id);
			ret = -EAGAIN;
			continue;
		}
	}

exit:
	return ret;
}

static void update_cluster_freq(void)
{
	int online_cpu = -1;
	struct cluster_info *cluster_ptr = NULL;
	uint32_t _cluster = 0, _cpu = 0, max = UINT_MAX, min = 0;
	uint32_t changed;

	if (!core_ptr)
		return;

	for (; _cluster < core_ptr->entity_count; _cluster++, _cpu = 0,
			online_cpu = -1, max = UINT_MAX, min = 0) {
		/*
		** Go over the frequency limits
		** of each core in the cluster and aggregate the minimum
		** and maximum frequencies. After aggregating, request for
		** frequency update on the first online core in that cluster.
		** Cpufreq driver takes care of updating the frequency of
		** other cores in a synchronous cluster.
		*/
		cluster_ptr = &core_ptr->child_entity_ptr[_cluster];

		for_each_cpu(_cpu, &cluster_ptr->cluster_cores) {
			if (online_cpu == -1 && cpu_online(_cpu))
				online_cpu = _cpu;
			max = min(max, cpus[_cpu].limited_max_freq);
			min = max(min, cpus[_cpu].limited_min_freq);
		}
		if (cluster_ptr->limited_max_freq == max
			&& cluster_ptr->limited_min_freq == min)
			continue;
		changed = 0;
		if (max != cluster_ptr->limited_max_freq)
			changed |= FREQ_LIMIT_MAX;
		if (min != cluster_ptr->limited_min_freq)
			changed |= FREQ_LIMIT_MIN;
		cluster_ptr->limited_max_freq = max;
		cluster_ptr->limited_min_freq = min;
		if (online_cpu != -1)
			update_cpu_freq(online_cpu, changed);
	}
}

static void do_cluster_freq_ctrl(int temp)
{
	uint32_t _cluster = 0;
	int _cpu = -1, freq_idx = 0;
	bool mitigate = false;
	struct cluster_info *cluster_ptr = NULL;

	if (temp >= msm_thermal_info.limit_temp_degC)
		mitigate = true;
	else if (temp < msm_thermal_info.limit_temp_degC -
		 msm_thermal_info.temp_hysteresis_degC)
		mitigate = false;
	else
		return;

	get_online_cpus();
	for (; _cluster < core_ptr->entity_count; _cluster++) {
		cluster_ptr = &core_ptr->child_entity_ptr[_cluster];
		if (!cluster_ptr->freq_table)
			continue;

		if (mitigate)
			freq_idx = max_t(int, cluster_ptr->freq_idx_low,
				(cluster_ptr->freq_idx
				- msm_thermal_info.bootup_freq_step));
		else
			freq_idx = min_t(int, cluster_ptr->freq_idx_high,
				(cluster_ptr->freq_idx
				+ msm_thermal_info.bootup_freq_step));
		if (freq_idx == cluster_ptr->freq_idx)
			continue;

		cluster_ptr->freq_idx = freq_idx;
		for_each_cpu(_cpu, &cluster_ptr->cluster_cores) {
			if (!(msm_thermal_info.bootup_freq_control_mask
				& BIT(_cpu)))
				continue;
			pr_info("Limiting CPU%d max frequency to %u. Temp:%d\n"
				, _cpu
				, cluster_ptr->freq_table[freq_idx].frequency
				, temp);
			cpus[_cpu].limited_max_freq = min(
				cluster_ptr->freq_table[freq_idx].frequency,
				cpus[_cpu].vdd_max_freq);
		}
	}
	if (_cpu != -1)
		update_cluster_freq();
	put_online_cpus();
}

/**
 * msm_thermal_lmh_dcvs_init: Initialize LMH DCVS hardware block
 *
 * @pdev: handle to the thermal device node
 *
 * Probe for the 'OSM clock' and initialize the LMH DCVS blocks.
 */
static int msm_thermal_lmh_dcvs_init(struct platform_device *pdev)
{
	struct clk *osm_clk;
	const char *clk_name = "osm";
	int ret = 0;

	/* We are okay if the osm clock is not present in DT */
	osm_clk = devm_clk_get(&pdev->dev, clk_name);
	if (IS_ERR(osm_clk))
		return ret;

	/*
	 * We actually don't need the clock, we just wanted to make sure
	 * the OSM block is ready.
	 */
	devm_clk_put(&pdev->dev, osm_clk);

	/* Enable the CRNT and Reliability algorithm. Again, we dont
	 * care if this fails
	 */
	ret = msm_lmh_dcvs_write(MSM_LIMITS_CLUSTER_0,
				MSM_LIMITS_SUB_FN_REL,
				MSM_LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		pr_err("Unable to enable REL algo for cluster0\n");
	ret = msm_lmh_dcvs_write(MSM_LIMITS_CLUSTER_1,
				MSM_LIMITS_SUB_FN_REL,
				MSM_LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		pr_err("Unable to enable REL algo for cluster1\n");

	ret = msm_lmh_dcvs_write(MSM_LIMITS_CLUSTER_0,
				MSM_LIMITS_SUB_FN_CRNT,
				MSM_LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		pr_err("Unable enable CRNT algo for cluster0\n");
	ret = msm_lmh_dcvs_write(MSM_LIMITS_CLUSTER_1,
				MSM_LIMITS_SUB_FN_CRNT,
				MSM_LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		pr_err("Unable enable CRNT algo for cluster1\n");

	lmh_dcvs_available = true;

	return ret;
}

/* If freq table exists, then we can send freq request */
static int check_freq_table(void)
{
	int ret = 0;
	static bool invalid_table;
	int table_len = 0;

	if (invalid_table)
		return -EINVAL;
	if (freq_table_get)
		return 0;

	if (core_ptr) {
		ret = init_cluster_freq_table();
		if (!ret)
			freq_table_get = 1;
		else if (ret == -EINVAL)
			invalid_table = true;
		goto exit;
	}

	table_len = get_cpu_freq_plan_len(0);
	if (!table_len)
		return -EINVAL;

	table = kzalloc(sizeof(struct cpufreq_frequency_table)
			* table_len, GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto exit;
	}
	table_len = get_cpu_freq_plan(0, table);
	if (!table_len) {
		pr_err("error reading cpufreq table\n");
		ret = -EINVAL;
		goto free_and_exit;
	}

	limit_idx_low = 0;
	limit_idx_high = limit_idx = table_len - 1;
	if (limit_idx_high < 0 || limit_idx_high < limit_idx_low) {
		invalid_table = true;
		limit_idx_low = limit_idx_high = limit_idx = 0;
		WARN(1, "CPU0 frequency table length:%d\n", table_len);
		ret = -EINVAL;
		goto free_and_exit;
	}
	freq_table_get = 1;

free_and_exit:
	if (ret) {
		kfree(table);
		table = NULL;
	}

exit:
	if (!ret) {
		int err;

		err = msm_thermal_lmh_dcvs_init(msm_thermal_info.pdev);
		if (err)
			pr_err("Error initializing OSM\n");
	}
	return ret;
}

static int update_cpu_min_freq_all(struct rail *apss_rail, uint32_t min)
{
	uint32_t cpu = 0, _cluster = 0, max_freq = UINT_MAX;
	int ret = 0;
	struct cluster_info *cluster_ptr = NULL;
	bool valid_table = false;

	if (!freq_table_get) {
		ret = check_freq_table();
		if (ret && !core_ptr) {
			pr_err("Fail to get freq table. err:%d\n", ret);
			return ret;
		}
	}
	if (min != apss_rail->min_level)
		max_freq = apss_rail->max_frequency_limit;

	get_online_cpus();
	/* If min is larger than allowed max */
	if (core_ptr) {
		for (; _cluster < core_ptr->entity_count; _cluster++) {
			cluster_ptr = &core_ptr->child_entity_ptr[_cluster];
			if (!cluster_ptr->freq_table)
				continue;
			valid_table = true;
			min = min(min,
				cluster_ptr->freq_table[
				cluster_ptr->freq_idx_high].frequency);
			max_freq = max(max_freq, cluster_ptr->freq_table[
				cluster_ptr->freq_idx_low].frequency);
		}
		if (!valid_table)
			goto update_freq_exit;
	} else {
		min = min(min, table[limit_idx_high].frequency);
		max_freq = max(max_freq, table[limit_idx_low].frequency);
	}

	pr_debug("Requesting min freq:%u max freq:%u for all CPU's\n",
		min, max_freq);
	if (freq_mitigation_task) {
		if (!apss_rail->device_handle[0]) {
			pr_err("device manager handle not registered\n");
			ret = -ENODEV;
			goto update_freq_exit;
		}
		for_each_possible_cpu(cpu) {
			cpus[cpu].vdd_max_freq = max_freq;
			apss_rail->request[cpu].freq.max_freq = max_freq;
			apss_rail->request[cpu].freq.min_freq = min;
			ret = devmgr_client_request_mitigation(
				apss_rail->device_handle[cpu],
				CPUFREQ_MITIGATION_REQ,
				&apss_rail->request[cpu]);
		}
	} else if (core_ptr) {
		for (_cluster = 0; _cluster < core_ptr->entity_count;
			_cluster++) {
			cluster_ptr = &core_ptr->child_entity_ptr[_cluster];
			if (!cluster_ptr->freq_table)
				continue;
			for_each_cpu(cpu, &cluster_ptr->cluster_cores) {
				uint32_t max;
				uint32_t changed = 0;

				cpus[cpu].vdd_max_freq = max_freq;
				max = min(cluster_ptr->freq_table[
					cluster_ptr->freq_idx].frequency,
					cpus[cpu].vdd_max_freq);

				if (max != cpus[cpu].limited_max_freq)
					changed |= FREQ_LIMIT_MAX;
				if (min != cpus[cpu].limited_min_freq)
					changed |= FREQ_LIMIT_MIN;

				cpus[cpu].limited_min_freq = min;
				cpus[cpu].limited_max_freq = max;
			}
			update_cluster_freq();
		}
	} else {
		for_each_possible_cpu(cpu) {
			uint32_t max;
			uint32_t changed = 0;

			cpus[cpu].vdd_max_freq = max_freq;
			max = min(table[limit_idx].frequency,
				cpus[cpu].vdd_max_freq);

			if (max != cpus[cpu].limited_max_freq)
				changed |= FREQ_LIMIT_MAX;
			if (min != cpus[cpu].limited_min_freq)
				changed |= FREQ_LIMIT_MIN;

			cpus[cpu].limited_min_freq = min;
			cpus[cpu].limited_max_freq = max;
		}
		update_cluster_freq();
	}

update_freq_exit:
	put_online_cpus();
	return ret;
}

static int vdd_restriction_apply_freq(struct rail *r, int level)
{
	int ret = 0;

	if (level == r->curr_level)
		return ret;

	/* level = -1: disable, level = 0,1,2..n: enable */
	if (level == -1) {
		ret = update_cpu_min_freq_all(r, r->min_level);
		if (ret)
			return ret;
		else
			r->curr_level = -1;
	} else if (level >= 0 && level < (r->num_levels)) {
		ret = update_cpu_min_freq_all(r, r->levels[level]);
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
			INT_MAX);
		if (!ret)
			r->curr_level = -1;
		pr_debug("Requested min level for %s. curr level: %d\n",
				r->name, r->curr_level);
	} else if (level >= 0 && level < (r->num_levels)) {
		ret = regulator_set_voltage(r->reg, r->levels[level],
			INT_MAX);
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

static ssize_t vdd_rstr_en_show(
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

static ssize_t sensor_info_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	ssize_t tot_size = 0, size = 0;

	for (i = 0; i < sensor_cnt; i++) {
		size = snprintf(&buf[tot_size], PAGE_SIZE - tot_size,
			"%s:%s:%s:%d ",
			sensors[i].type, sensors[i].name,
			sensors[i].alias ? : "",
			sensors[i].scaling_factor);
		if (tot_size + size >= PAGE_SIZE) {
			pr_err("Not enough buffer size\n");
			break;
		}
		tot_size += size;
	}
	if (tot_size)
		buf[tot_size - 1] = '\n';

	return tot_size;
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

static ssize_t vdd_rstr_reg_value_show(
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

static ssize_t vdd_rstr_reg_level_show(
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
			update_cpu_min_freq_all(reg, reg->levels[val]);
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

	ret = regulator_set_load(rail->phase_reg,
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

static ssize_t psm_reg_mode_show(
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

static int zone_id_to_tsen_id(int zone_id, int *tsens_id)
{
	int i = 0;
	int ret = 0;

	if (!zone_id_tsens_map) {
		pr_debug("zone_id_tsens_map is not initialized.\n");
		*tsens_id = zone_id;
		return ret;
	}

	for (i = 0; i < max_tsens_num; i++) {
		if (zone_id == zone_id_tsens_map[i]) {
			*tsens_id = tsens_id_map[i];
			break;
		}
	}
	if (i == max_tsens_num) {
		pr_err("Invalid sensor zone id:%d\n", zone_id);
		return -EINVAL;
	}

	return ret;
}

static int create_sensor_zone_id_map(void)
{
	int i = 0;
	int zone_id = -1;

	zone_id_tsens_map = devm_kzalloc(&msm_thermal_info.pdev->dev,
		sizeof(int) * max_tsens_num, GFP_KERNEL);

	if (!zone_id_tsens_map) {
		pr_err("Cannot allocate memory for zone_id_tsens_map\n");
		return -ENOMEM;
	}

	for (i = 0; i < max_tsens_num; i++) {
		char tsens_name[TSENS_NAME_MAX] = "";

		snprintf(tsens_name, TSENS_NAME_MAX, TSENS_NAME_FORMAT,
			tsens_id_map[i]);
		zone_id = sensor_get_id(tsens_name);
		if (zone_id < 0) {
			pr_err("Error getting zone id for %s. err:%d\n",
				tsens_name, zone_id);
			goto fail;
		} else {
			zone_id_tsens_map[i] = zone_id;
		}
	}
	return 0;

fail:
	devm_kfree(&msm_thermal_info.pdev->dev, zone_id_tsens_map);
	return zone_id;
}

static int create_sensor_id_map(struct device *dev)
{
	int ret = 0;

	tsens_id_map = devm_kzalloc(dev,
		sizeof(int) * max_tsens_num, GFP_KERNEL);

	if (!tsens_id_map) {
		pr_err("Cannot allocate memory for tsens_id_map\n");
		return -ENOMEM;
	}

	ret = tsens_get_hw_id_mapping(max_tsens_num, tsens_id_map);
	if (ret) {
		pr_err("Failed to get tsens id's:%d\n", ret);
		goto fail;
	}

	return ret;
fail:
	devm_kfree(dev, tsens_id_map);
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
		if (rails[i].freq_req == 1)
			if (freq_table_get)
				ret = vdd_restriction_apply_freq(&rails[i],
					en ? 0 : -1);
			else
				continue;
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

static int therm_get_temp(uint32_t id, enum sensor_id_type type, int *temp)
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

	if (tsens_scaling_factor)
		*temp = *temp / tsens_scaling_factor;

get_temp_exit:
	return ret;
}

static int msm_thermal_panic_callback(struct notifier_block *nfb,
			unsigned long event, void *data)
{
	int i;

	for (i = 0; i < max_tsens_num; i++) {
		therm_get_temp(tsens_id_map[i],
				THERM_TSENS_ID,
				&tsens_temp_at_panic[i]);
		if (tsens_temp_print)
			pr_err("tsens%d temperature:%dC\n",
				tsens_id_map[i], tsens_temp_at_panic[i]);
	}

	return NOTIFY_OK;
}

static struct notifier_block msm_thermal_panic_notifier = {
	.notifier_call = msm_thermal_panic_callback,
};

int sensor_mgr_set_threshold(uint32_t zone_id,
	struct sensor_threshold *threshold)
{
	int i = 0, ret = 0;
	int temp;

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
	pr_debug("Sensor:[%d] temp:[%d]\n", zone_id, temp);
	while (i < MAX_THRESHOLD) {
		switch (threshold[i].trip) {
		case THERMAL_TRIP_CONFIGURABLE_HI:
			if (threshold[i].temp / tsens_scaling_factor >= temp) {
				ret = set_and_activate_threshold(zone_id,
					&threshold[i]);
				if (ret)
					goto set_threshold_exit;
				UPDATE_THRESHOLD_SET(ret,
					THERMAL_TRIP_CONFIGURABLE_HI);
			}
			break;
		case THERMAL_TRIP_CONFIGURABLE_LOW:
			if (threshold[i].temp / tsens_scaling_factor <= temp) {
				ret = set_and_activate_threshold(zone_id,
					&threshold[i]);
				if (ret)
					goto set_threshold_exit;
				UPDATE_THRESHOLD_SET(ret,
					THERMAL_TRIP_CONFIGURABLE_LOW);
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
	int ret_mx = 0, ret_cx = 0;

	if (mx_restr_applied)
		goto done;

	APPLY_VDD_RESTRICTION(vdd_mx, msm_thermal_info.vdd_mx_min, mx, ret_mx);
	if (vdd_cx)
		APPLY_VDD_RESTRICTION(vdd_cx, msm_thermal_info.vdd_cx_min,
			cx, ret_cx);
	if (!ret_mx && !ret_cx)
		mx_restr_applied = true;

done:
	return (ret_mx | ret_cx);
}

static int remove_vdd_mx_restriction(void)
{
	int ret_mx = 0, ret_cx = 0;

	if (!mx_restr_applied)
		goto done;

	REMOVE_VDD_RESTRICTION(vdd_mx, mx, ret_mx);
	if (vdd_cx)
		REMOVE_VDD_RESTRICTION(vdd_cx, cx, ret_cx);
	if (!ret_mx && !ret_cx)
		mx_restr_applied = false;

done:
	return (ret_mx | ret_cx);
}

static int do_vdd_mx(void)
{
	int temp = 0;
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

	if (trig_thresh->cur_state != trig_thresh->trip_triggered) {
		sensor_mgr_set_threshold(trig_thresh->sensor_id,
					trig_thresh->threshold);
		trig_thresh->cur_state = trig_thresh->trip_triggered;
	}
}

static void msm_thermal_bite(int zone_id, int temp)
{
	struct scm_desc desc;
	int tsens_id = 0;
	int ret = 0;

	ret = zone_id_to_tsen_id(zone_id, &tsens_id);
	if (ret < 0) {
		pr_err("Zone:%d reached temperature:%d. Err = %d System reset\n",
			zone_id, temp, ret);
	} else {
		pr_err("Tsens:%d reached temperature:%d. System reset\n",
			tsens_id, temp);
	}
	if (!is_scm_armv8()) {
		scm_call_atomic1(SCM_SVC_BOOT, THERM_SECURE_BITE_CMD, 0);
	} else {
		desc.args[0] = 0;
		desc.arginfo = SCM_ARGS(1);
		scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_BOOT,
				 THERM_SECURE_BITE_CMD), &desc);
	}
}

static int do_therm_reset(void)
{
	int ret = 0, i;
	int temp = 0;

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
	int temp = 0;
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
		msm_thermal_bite(thresh_data->sensor_id, temp);
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		break;
	default:
		pr_err("Invalid trip type\n");
		break;
	}
	sensor_mgr_set_threshold(thresh_data->sensor_id,
					thresh_data->threshold);
}

static void retry_hotplug(struct work_struct *work)
{
	mutex_lock(&core_control_mutex);
	if (retry_in_progress) {
		pr_debug("Retrying hotplug\n");
		retry_in_progress = false;
		complete(&hotplug_notify_complete);
	}
	mutex_unlock(&core_control_mutex);
}

#ifdef CONFIG_SMP
static void __ref do_core_control(int temp)
{
	int i = 0;
	int ret = 0;
	struct device *cpu_dev = NULL;

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
			pr_info("Set Offline: CPU%d Temp: %d\n",
					i, temp);
			lock_device_hotplug();
			if (cpu_online(i)) {
				cpu_dev = get_cpu_device(i);
				trace_thermal_pre_core_offline(i);
				ret = device_offline(cpu_dev);
				if (ret < 0)
					pr_err("Error %d offline core %d\n",
					       ret, i);
				trace_thermal_post_core_offline(i,
					cpumask_test_cpu(i, cpu_online_mask));
			}
			unlock_device_hotplug();
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
			pr_info("Allow Online CPU%d Temp: %d\n",
					i, temp);
			/*
			 * If this core is already online, then bring up the
			 * next offlined core.
			 */
			lock_device_hotplug();
			if (cpu_online(i)) {
				unlock_device_hotplug();
				continue;
			}
			/* If this core wasn't previously online don't put it
			   online */
			if (!(cpumask_test_cpu(i, cpus_previously_online))) {
				unlock_device_hotplug();
				continue;
			}
			cpu_dev = get_cpu_device(i);
			trace_thermal_pre_core_online(i);
			ret = device_online(cpu_dev);
			if (ret)
				pr_err("Error %d online core %d\n",
						ret, i);
			trace_thermal_post_core_online(i,
				cpumask_test_cpu(i, cpu_online_mask));
			unlock_device_hotplug();
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
	uint32_t previous_cpus_offlined = 0;
	bool pend_hotplug_req = false;
	struct device *cpu_dev = NULL;

	if (!core_control_enabled)
		return 0;

	previous_cpus_offlined = cpus_offlined;
	cpus_offlined = msm_thermal_info.core_control_mask & val;

	for_each_possible_cpu(cpu) {
		if (cpus_offlined & BIT(cpu)) {
			lock_device_hotplug();
			if (!cpu_online(cpu)) {
				unlock_device_hotplug();
				continue;
			}
			cpu_dev = get_cpu_device(cpu);
			trace_thermal_pre_core_offline(cpu);
			ret = device_offline(cpu_dev);
			if (ret < 0) {
				cpus_offlined &= ~BIT(cpu);
				pr_err_ratelimited(
					"Unable to offline CPU%d. err:%d\n",
					cpu, ret);
				pend_hotplug_req = true;
			} else {
				pr_debug("Offlined CPU%d\n", cpu);
			}
			trace_thermal_post_core_offline(cpu,
				cpumask_test_cpu(cpu, cpu_online_mask));
			unlock_device_hotplug();
		} else if (online_core && (previous_cpus_offlined & BIT(cpu))) {
			lock_device_hotplug();
			if (cpu_online(cpu)) {
				unlock_device_hotplug();
				continue;
			}
			/* If this core wasn't previously online don't put it
			   online */
			if (!(cpumask_test_cpu(cpu, cpus_previously_online))) {
				unlock_device_hotplug();
				continue;
			}
			cpu_dev = get_cpu_device(cpu);
			trace_thermal_pre_core_online(cpu);
			ret = device_online(cpu_dev);
			if (ret && ret == notifier_to_errno(NOTIFY_BAD)) {
				pr_debug("Onlining CPU%d is vetoed\n", cpu);
			} else if (ret) {
				cpus_offlined |= BIT(cpu);
				pend_hotplug_req = true;
				pr_err_ratelimited(
					"Unable to online CPU%d. err:%d\n",
					cpu, ret);
			} else {
				pr_debug("Onlined CPU%d\n", cpu);
				trace_thermal_post_core_online(cpu,
					cpumask_test_cpu(cpu, cpu_online_mask));
			}
			unlock_device_hotplug();
		}
	}

	if (pend_hotplug_req && !in_suspend && !retry_in_progress) {
		retry_in_progress = true;
		schedule_delayed_work(&retry_hotplug_work,
			msecs_to_jiffies(HOTPLUG_RETRY_INTERVAL_MS));
	}

	return ret;
}

static __ref int do_hotplug(void *data)
{
	int ret = 0;
	uint32_t cpu = 0, mask = 0;
	struct device_clnt_data *clnt = NULL;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO-2};

	if (!core_control_enabled) {
		pr_debug("Core control disabled\n");
		return -EINVAL;
	}

	sched_setscheduler(current, SCHED_FIFO, &param);
	while (!kthread_should_stop()) {
		while (wait_for_completion_interruptible(
			&hotplug_notify_complete) != 0)
			;
		reinit_completion(&hotplug_notify_complete);

		/*
		 * Suspend framework will have disabled the
		 * hotplug functionality. So wait till the suspend exits
		 * and then re-evaluate.
		 */
		if (in_suspend)
			continue;
		mask = 0;

		mutex_lock(&core_control_mutex);
		for_each_possible_cpu(cpu) {
			if (hotplug_enabled &&
				cpus[cpu].hotplug_thresh_clear) {
				ret =
				sensor_mgr_set_threshold(cpus[cpu].sensor_id,
				&cpus[cpu].threshold[HOTPLUG_THRESHOLD_HIGH]);

				if (cpus[cpu].offline
					&& !IS_LOW_THRESHOLD_SET(ret))
					cpus[cpu].offline = 0;
				cpus[cpu].hotplug_thresh_clear = false;
			}
			if (cpus[cpu].offline || cpus[cpu].user_offline)
				mask |= BIT(cpu);
		}
		if (devices && devices->hotplug_dev) {
			mutex_lock(&devices->hotplug_dev->clnt_lock);
			for_each_cpu(cpu,
				&devices->hotplug_dev->active_req.offline_mask)
				mask |= BIT(cpu);
			mutex_unlock(&devices->hotplug_dev->clnt_lock);
		}
		if (mask != cpus_offlined)
			update_offline_cores(mask);
		mutex_unlock(&core_control_mutex);

		if (devices && devices->hotplug_dev) {
			union device_request req;

			req.offline_mask = CPU_MASK_NONE;
			mutex_lock(&devices->hotplug_dev->clnt_lock);
			for_each_cpu(cpu,
				&devices->hotplug_dev->active_req.offline_mask)
				if (mask & BIT(cpu))
					cpumask_test_and_set_cpu(cpu,
						&req.offline_mask);

			list_for_each_entry(clnt,
					&devices->hotplug_dev->client_list,
					clnt_ptr) {
				if (clnt->callback)
					clnt->callback(clnt, &req,
							clnt->usr_data);
			}
			mutex_unlock(&devices->hotplug_dev->clnt_lock);
		}
		sysfs_notify(cc_kobj, NULL, "cpus_offlined");
	}

	return ret;
}
#else
static void __ref do_core_control(int temp)
{
	return;
}

static __ref int do_hotplug(void *data)
{
	return 0;
}

static int __ref update_offline_cores(int val)
{
	return 0;
}
#endif

static int do_gfx_phase_cond(void)
{
	int temp = 0;
	int ret = 0;
	uint32_t new_req_band = curr_gfx_band;

	if (!gfx_warm_phase_ctrl_enabled && !gfx_crit_phase_ctrl_enabled)
		return ret;

	mutex_lock(&gfx_mutex);
	if (gfx_warm_phase_ctrl_enabled) {
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
	} else {
		ret = therm_get_temp(
			thresh[MSM_GFX_PHASE_CTRL_HOT].thresh_list->sensor_id,
			thresh[MSM_GFX_PHASE_CTRL_HOT].thresh_list->id_type,
			&temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d. err:%d\n",
			thresh[MSM_GFX_PHASE_CTRL_HOT].thresh_list->sensor_id,
			ret);
			goto gfx_phase_cond_exit;
		}
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
			pr_debug("Reached %d band. Temp:%d\n", new_req_band,
					temp);
			curr_gfx_band = new_req_band;
		} else {
			pr_err("Error sending temp. band:%d. Temp:%d. err:%d",
					new_req_band, temp, ret);
		}
	}

gfx_phase_cond_exit:
	mutex_unlock(&gfx_mutex);
	return ret;
}

static int do_cx_phase_cond(void)
{
	int temp = 0;
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
					pr_debug("band:HOT_CRITICAL Temp:%d\n",
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
			pr_debug("band:WARM Temp:%d\n", temp);
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
	int temp = 0;
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
				pr_debug("Requested MAX OCR. tsens:%d Temp:%d",
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
			pr_debug("Requested MIN OCR. Temp:%d", temp);
		}
	}
do_ocr_exit:
	mutex_unlock(&ocr_mutex);
	return ret;
}

static int do_vdd_restriction(void)
{
	int temp = 0;
	int ret = 0;
	int i = 0;
	int dis_cnt = 0;

	if (!vdd_rstr_enabled)
		return ret;

	if (usefreq && !freq_table_get) {
		if (check_freq_table() && !core_ptr)
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
			pr_debug("Enabled Vdd Restriction tsens:%d. Temp:%d\n",
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
	int temp = 0;
	int ret = 0;
	int i = 0;
	int auto_cnt = 0;

	if (!psm_enabled)
		return ret;

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
			pr_debug("Requested PMIC PWM Mode tsens:%d. Temp:%d\n",
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

static void do_freq_control(int temp)
{
	uint32_t cpu = 0;
	uint32_t max_freq = cpus[cpu].limited_max_freq;

	if (!boot_freq_mitig_enabled)
		return;
	if (core_ptr)
		return do_cluster_freq_ctrl(temp);
	if (!freq_table_get)
		return;

	if (temp >= msm_thermal_info.limit_temp_degC) {
		if (limit_idx == limit_idx_low)
			return;

		limit_idx -= msm_thermal_info.bootup_freq_step;
		if (limit_idx < limit_idx_low)
			limit_idx = limit_idx_low;
	} else if (temp < msm_thermal_info.limit_temp_degC -
		 msm_thermal_info.temp_hysteresis_degC) {
		if (limit_idx == limit_idx_high)
			return;

		limit_idx += msm_thermal_info.bootup_freq_step;
		if (limit_idx >= limit_idx_high)
			limit_idx = limit_idx_high;
	}

	/* Update new limits */
	get_online_cpus();
	max_freq = table[limit_idx].frequency;
	if (max_freq == cpus[cpu].limited_max_freq) {
		put_online_cpus();
		return;
	}

	for_each_possible_cpu(cpu) {
		if (!(msm_thermal_info.bootup_freq_control_mask & BIT(cpu)))
			continue;
		pr_info("Limiting CPU%d max frequency to %u. Temp:%d\n",
			cpu, max_freq, temp);
		cpus[cpu].limited_max_freq =
				min(max_freq, cpus[cpu].vdd_max_freq);
	}
	update_cluster_freq();
	put_online_cpus();
}

static void check_temp(struct work_struct *work)
{
	int temp = 0;
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

	/*
	** All mitigation involving CPU frequency should be
	** placed below this check. The mitigation following this
	** frequency table check, should be able to handle the failure case.
	*/
	if (!freq_table_get)
		check_freq_table();

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
	uint32_t cpu = (uintptr_t)hcpu;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
		if (!cpumask_test_and_set_cpu(cpu, cpus_previously_online))
			pr_debug("Total prev cores online tracked %u\n",
				cpumask_weight(cpus_previously_online));
		if (core_control_enabled &&
			(msm_thermal_info.core_control_mask & BIT(cpu)) &&
			(cpus_offlined & BIT(cpu))) {
			pr_debug("Preventing CPU%d from coming online.\n",
				cpu);
			return NOTIFY_BAD;
		}
		break;
	case CPU_DOWN_PREPARE:
		if (!cpumask_test_and_set_cpu(cpu, cpus_previously_online))
			pr_debug("Total prev cores online tracked %u\n",
				cpumask_weight(cpus_previously_online));
		break;
	case CPU_ONLINE:
		if (core_control_enabled &&
			(msm_thermal_info.core_control_mask & BIT(cpu)) &&
			(cpus_offlined & BIT(cpu))) {
			if (hotplug_task) {
				pr_debug("Re-evaluate and hotplug CPU%d\n",
					cpu);
				complete(&hotplug_notify_complete);
			} else {
				/*
				 * This will be auto-corrected next time
				 * do_core_control() is called
				 */
				pr_err("CPU%d online, after thermal veto\n",
					cpu);
			}
		}
		break;
	default:
		break;
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

	pr_info_ratelimited("%s reach temp threshold: %d\n",
			       cpu_node->sensor_type, temp);

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
	int temp = 0;
	uint32_t cpu = 0;

	if (!hotplug_enabled || !hotplug_task)
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
		else
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
		hi_thresh->temp = (msm_thermal_info.hotplug_temp_degC)
				* tsens_scaling_factor;
		hi_thresh->trip = THERMAL_TRIP_CONFIGURABLE_HI;
		low_thresh->temp = (msm_thermal_info.hotplug_temp_degC -
				msm_thermal_info.hotplug_temp_hysteresis_degC)
				* tsens_scaling_factor;
		low_thresh->trip = THERMAL_TRIP_CONFIGURABLE_LOW;
		hi_thresh->notify = low_thresh->notify = hotplug_notify;
		hi_thresh->data = low_thresh->data = (void *)&cpus[cpu];

		sensor_mgr_set_threshold(cpus[cpu].sensor_id, hi_thresh);
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
	struct sched_param param = {.sched_priority = MAX_RT_PRIO-1};
	struct device_clnt_data *clnt = NULL;
	struct device_manager_data *cpu_dev = NULL;
	uint32_t changed;

	sched_setscheduler(current, SCHED_FIFO, &param);
	while (!kthread_should_stop()) {
		while (wait_for_completion_interruptible(
			&freq_mitigation_complete) != 0)
			;
		reinit_completion(&freq_mitigation_complete);

		for_each_possible_cpu(cpu) {
			max_freq_req = (cpus[cpu].max_freq) ?
					msm_thermal_info.freq_limit :
					UINT_MAX;
			max_freq_req = min(max_freq_req,
					cpus[cpu].user_max_freq);

			max_freq_req = min(max_freq_req,
					cpus[cpu].shutdown_max_freq);

			max_freq_req = min(max_freq_req,
					cpus[cpu].suspend_max_freq);

			if (devices && devices->cpufreq_dev[cpu]) {
				cpu_dev = devices->cpufreq_dev[cpu];
				mutex_lock(&cpu_dev->clnt_lock);
				max_freq_req = min(max_freq_req,
					cpu_dev->active_req.freq.max_freq);
				min_freq_req =
					cpu_dev->active_req.freq.min_freq;
				mutex_unlock(&cpu_dev->clnt_lock);
			}

			if ((max_freq_req == cpus[cpu].limited_max_freq)
				&& (min_freq_req ==
				cpus[cpu].limited_min_freq))
				goto reset_threshold;

			changed = 0;
			if (max_freq_req != cpus[cpu].limited_max_freq)
				changed |= FREQ_LIMIT_MAX;
			if (min_freq_req != cpus[cpu].limited_min_freq)
				changed |= FREQ_LIMIT_MIN;

			cpus[cpu].limited_max_freq = max_freq_req;
			cpus[cpu].limited_min_freq = min_freq_req;
reset_threshold:
			if (devices && devices->cpufreq_dev[cpu]) {
				union device_request req;

				req.freq.max_freq = max_freq_req;
				req.freq.min_freq = min_freq_req;
				cpu_dev = devices->cpufreq_dev[cpu];
				mutex_lock(&cpu_dev->clnt_lock);
				list_for_each_entry(clnt,
					&cpu_dev->client_list,
					clnt_ptr) {
					if (clnt->callback)
						clnt->callback(clnt,
							&req,
							clnt->usr_data);
				}
				mutex_unlock(&cpu_dev->clnt_lock);
			}
			if (freq_mitigation_enabled &&
				cpus[cpu].freq_thresh_clear) {
				ret =
				sensor_mgr_set_threshold(cpus[cpu].sensor_id,
				&cpus[cpu].threshold[FREQ_THRESHOLD_HIGH]);

				if (cpus[cpu].max_freq
					&& !IS_LOW_THRESHOLD_SET(ret)) {
					cpus[cpu].max_freq = false;
					complete(&freq_mitigation_complete);
				}
				cpus[cpu].freq_thresh_clear = false;
			}
		}
		update_cluster_freq();
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
			pr_info_ratelimited(
				"Mitigating CPU%d frequency to %d\n",
				cpu_node->cpu, msm_thermal_info.freq_limit);

			cpu_node->max_freq = true;
		}
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		if (cpu_node->max_freq) {
			pr_info_ratelimited(
				"Removing frequency mitigation for CPU%d\n",
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
		/*
		 * Hotplug may not be enabled,
		 * make sure core sensor id is initialized.
		 */
		cpus[cpu].sensor_id =
			sensor_get_id((char *)cpus[cpu].sensor_type);
		cpus[cpu].id_type = THERM_ZONE_ID;
		if (!(msm_thermal_info.freq_mitig_control_mask & BIT(cpu)))
			continue;
		hi_thresh = &cpus[cpu].threshold[FREQ_THRESHOLD_HIGH];
		low_thresh = &cpus[cpu].threshold[FREQ_THRESHOLD_LOW];

		hi_thresh->temp = msm_thermal_info.freq_mitig_temp_degc
				* tsens_scaling_factor;
		hi_thresh->trip = THERMAL_TRIP_CONFIGURABLE_HI;
		low_thresh->temp = (msm_thermal_info.freq_mitig_temp_degc -
			msm_thermal_info.freq_mitig_temp_hysteresis_degc)
				* tsens_scaling_factor;
		low_thresh->trip = THERMAL_TRIP_CONFIGURABLE_LOW;
		hi_thresh->notify = low_thresh->notify =
			freq_mitigation_notify;
		hi_thresh->data = low_thresh->data = (void *)&cpus[cpu];

		sensor_mgr_set_threshold(cpus[cpu].sensor_id, hi_thresh);
	}
init_freq_thread:
	init_completion(&freq_mitigation_complete);
	freq_mitigation_task = kthread_run(do_freq_mitigation, NULL,
		"msm_thermal:freq_mitig");

	if (IS_ERR(freq_mitigation_task)) {
		pr_err("Failed to create frequency mitigation thread. err:%ld\n",
				PTR_ERR(freq_mitigation_task));
		return;
	} else {
		complete(&freq_mitigation_complete);
	}
}

int msm_thermal_get_freq_plan_size(uint32_t cluster, unsigned int *table_len)
{
	uint32_t i = 0;
	struct cluster_info *cluster_ptr = NULL;

	if (!core_ptr) {
		pr_err("Topology ptr not initialized\n");
		return -ENODEV;
	}
	if (!table_len) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	if (!freq_table_get)
		check_freq_table();

	for (; i < core_ptr->entity_count; i++) {
		cluster_ptr = &core_ptr->child_entity_ptr[i];
		if (cluster_ptr->cluster_id == cluster) {
			if (!cluster_ptr->freq_table) {
				pr_err("Cluster%d clock plan not initialized\n",
						cluster);
				return -EINVAL;
			}
			*table_len = cluster_ptr->freq_idx_high + 1;
			return 0;
		}
	}

	pr_err("Invalid cluster ID:%d\n", cluster);
	return -EINVAL;
}

int msm_thermal_get_cluster_voltage_plan(uint32_t cluster, uint32_t *table_ptr)
{
	int i = 0, corner = 0;
	struct dev_pm_opp *opp = NULL;
	unsigned int table_len = 0;
	struct device *cpu_dev = NULL;
	struct cluster_info *cluster_ptr = NULL;

	if (!core_ptr) {
		pr_err("Topology ptr not initialized\n");
		return -ENODEV;
	}
	if (!table_ptr) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	if (!freq_table_get)
		check_freq_table();

	for (i = 0; i < core_ptr->entity_count; i++) {
		cluster_ptr = &core_ptr->child_entity_ptr[i];
		if (cluster_ptr->cluster_id == cluster)
			break;
	}
	if (i == core_ptr->entity_count) {
		pr_err("Invalid cluster ID:%d\n", cluster);
		return -EINVAL;
	}
	if (!cluster_ptr->freq_table) {
		pr_err("Cluster%d clock plan not initialized\n", cluster);
		return -EINVAL;
	}

	cpu_dev = get_cpu_device(cpumask_first(&cluster_ptr->cluster_cores));
	table_len =  cluster_ptr->freq_idx_high + 1;

	rcu_read_lock();
	for (i = 0; i < table_len; i++) {
		opp = dev_pm_opp_find_freq_exact(cpu_dev,
			cluster_ptr->freq_table[i].frequency * 1000, true);
		if (IS_ERR(opp)) {
			pr_err("Error on OPP freq :%d\n",
				cluster_ptr->freq_table[i].frequency);
			return -EINVAL;
		}
		corner = dev_pm_opp_get_voltage(opp);
		if (corner == 0) {
			pr_err("Bad voltage corner for OPP freq :%d\n",
				cluster_ptr->freq_table[i].frequency);
			return -EINVAL;
		}
		table_ptr[i] = corner / 1000;
		pr_debug("Cluster:%d freq:%d Khz voltage:%d mV\n",
			cluster, cluster_ptr->freq_table[i].frequency,
			table_ptr[i]);
	}
	rcu_read_unlock();

	return 0;
}

int msm_thermal_get_cluster_freq_plan(uint32_t cluster, unsigned int *table_ptr)
{
	uint32_t i = 0;
	struct cluster_info *cluster_ptr = NULL;

	if (!core_ptr) {
		pr_err("Topology ptr not initialized\n");
		return -ENODEV;
	}
	if (!table_ptr) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	if (!freq_table_get)
		check_freq_table();

	for (; i < core_ptr->entity_count; i++) {
		cluster_ptr = &core_ptr->child_entity_ptr[i];
		if (cluster_ptr->cluster_id == cluster)
			break;
	}
	if (i == core_ptr->entity_count) {
		pr_err("Invalid cluster ID:%d\n", cluster);
		return -EINVAL;
	}
	if (!cluster_ptr->freq_table) {
		pr_err("Cluster%d clock plan not initialized\n", cluster);
		return -EINVAL;
	}

	for (i = 0; i <= cluster_ptr->freq_idx_high; i++)
		table_ptr[i] = cluster_ptr->freq_table[i].frequency;

	return 0;
}

int msm_thermal_set_cluster_freq(uint32_t cluster, uint32_t freq, bool is_max)
{
	int ret = 0;
	uint32_t i = 0;
	struct cluster_info *cluster_ptr = NULL;
	bool notify = false;

	if (!mitigation) {
		pr_err("Thermal Mitigations disabled.\n");
		return -ENODEV;
	}

	if (!core_ptr) {
		pr_err("Topology ptr not initialized\n");
		return -ENODEV;
	}

	for (; i < core_ptr->entity_count; i++) {
		cluster_ptr = &core_ptr->child_entity_ptr[i];
		if (cluster_ptr->cluster_id != cluster)
			continue;
		pr_debug("Update Cluster%d %s frequency to %d\n",
			cluster, (is_max) ? "max" : "min", freq);
		break;
	}
	if (i == core_ptr->entity_count) {
		pr_err("Invalid cluster ID:%d\n", cluster);
		return -EINVAL;
	}

	for_each_cpu(i, &cluster_ptr->cluster_cores) {
		uint32_t *freq_ptr = (is_max) ? &cpus[i].user_max_freq
					: &cpus[i].user_min_freq;
		if (*freq_ptr == freq)
			continue;
		notify = true;
		*freq_ptr = freq;
	}

	if (freq_mitigation_task) {
		if (notify)
			complete(&freq_mitigation_complete);
	} else {
		pr_err("Frequency mitigation task is not initialized\n");
		return -ESRCH;
	}

	return ret;
}

int msm_thermal_set_frequency(uint32_t cpu, uint32_t freq, bool is_max)
{
	int ret = 0;

	if (!mitigation) {
		pr_err("Thermal Mitigations disabled.\n");
		goto set_freq_exit;
	}

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
		err = sensor_mgr_set_threshold(thresh_ptr->sensor_id,
			thresh_ptr->threshold);
		if (err < 0) {
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
	if (trig_thresh->cur_state != trig_thresh->trip_triggered) {
		sensor_mgr_set_threshold(trig_thresh->sensor_id,
					trig_thresh->threshold);
		trig_thresh->cur_state = trig_thresh->trip_triggered;
	}
	return;
}

static void gfx_phase_ctrl_notify(struct therm_threshold *trig_thresh)
{
	uint32_t new_req_band = curr_gfx_band;
	int ret = 0;

	if (!gfx_warm_phase_ctrl_enabled && !gfx_crit_phase_ctrl_enabled)
		return;

	if (trig_thresh->trip_triggered < 0)
		goto gfx_phase_ctrl_exit;

	mutex_lock(&gfx_mutex);
	if (gfx_crit_phase_ctrl_enabled) {
		switch (
		thresh[MSM_GFX_PHASE_CTRL_HOT].thresh_list->trip_triggered) {
		case THERMAL_TRIP_CONFIGURABLE_HI:
			new_req_band = MSM_HOT_CRITICAL;
			pr_debug(
			"sensor:%d reached hot critical thresh for GFX\n",
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
	}
	if (gfx_warm_phase_ctrl_enabled) {
		switch (
		thresh[MSM_GFX_PHASE_CTRL_WARM].thresh_list->trip_triggered) {
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
		if (gfx_crit_phase_ctrl_enabled)
			therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_HOT]);
		break;
	case MSM_NORMAL:
		if (gfx_warm_phase_ctrl_enabled)
			therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_WARM]);
		break;
	case MSM_WARM:
	default:
		if (gfx_crit_phase_ctrl_enabled)
			therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_HOT]);
		if (gfx_warm_phase_ctrl_enabled)
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
	if (trig_thresh->cur_state != trig_thresh->trip_triggered) {
		sensor_mgr_set_threshold(trig_thresh->sensor_id,
					trig_thresh->threshold);
		trig_thresh->cur_state = trig_thresh->trip_triggered;
	}
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
	if (trig_thresh->cur_state != trig_thresh->trip_triggered) {
		sensor_mgr_set_threshold(trig_thresh->sensor_id,
				trig_thresh->threshold);
		trig_thresh->cur_state = trig_thresh->trip_triggered;
	}
	return;
}

static __ref int do_thermal_monitor(void *data)
{
	int ret = 0, j;
	struct therm_threshold *sensor_list;
	struct threshold_info *thresholds = NULL;

	while (!kthread_should_stop()) {
		while (wait_for_completion_interruptible(
			&thermal_monitor_complete) != 0)
			;
		reinit_completion(&thermal_monitor_complete);

		mutex_lock(&threshold_mutex);
		list_for_each_entry(thresholds, &thresholds_list, list_ptr) {
			if (!thresholds->thresh_triggered)
				continue;
			thresholds->thresh_triggered = false;
			for (j = 0; j < thresholds->thresh_ct; j++) {
				sensor_list = &thresholds->thresh_list[j];
				if (sensor_list->trip_triggered < 0)
					continue;
				sensor_list->notify(sensor_list);
				sensor_list->trip_triggered = -1;
			}
		}
		mutex_unlock(&threshold_mutex);
	}
	return ret;
}

static int vdd_rstr_apss_freq_dev_init(void)
{
	int idx = 0, ret = 0;
	char device_str[DEVM_NAME_MAX] = "";
	struct rail *r = NULL;

	for (idx = 0; idx < rails_cnt; idx++) {
		if (rails[idx].freq_req) {
			r = &rails[idx];
			break;
		}
	}
	if (!r) {
		pr_err("APSS rail not initialized\n");
		return -ENODEV;
	}

	for_each_possible_cpu(idx) {
		if (r->device_handle[idx])
			continue;
		snprintf(device_str, DEVM_NAME_MAX, CPU_DEVICE, idx);
		r->device_handle[idx]
			= devmgr_register_mitigation_client(
				&msm_thermal_info.pdev->dev,
				device_str, NULL);
		if (IS_ERR(r->device_handle[idx])) {
			ret = PTR_ERR(r->device_handle[idx]);
			pr_err("Error registering %s handle. err:%d\n",
				device_str, ret);
			goto freq_init_exit;
		}
		r->request[idx].freq.max_freq = CPUFREQ_MAX_NO_MITIGATION;
		r->request[idx].freq.min_freq = CPUFREQ_MIN_NO_MITIGATION;
	}

freq_init_exit:
	if (ret) {
		for_each_possible_cpu(idx) {
			devmgr_unregister_mitigation_client(
				&msm_thermal_info.pdev->dev,
				r->device_handle[idx]);
			r->device_handle[idx] = NULL;
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

int sensor_mgr_convert_id_and_set_threshold(struct threshold_info *thresh_inp)
{
	int ret = 0;

	if (!thresh_inp) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto therm_set_exit;
	}
	ret = convert_to_zone_id(thresh_inp);
	if (ret)
		goto therm_set_exit;
	ret = therm_set_threshold(thresh_inp);

therm_set_exit:
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

	if (vdd_rstr_enabled) {
		if (vdd_rstr_apss_freq_dev_init())
			pr_err("vdd APSS mitigation device init failed\n");
		else if (!(convert_to_zone_id(&thresh[MSM_VDD_RESTRICTION])))
			therm_set_threshold(&thresh[MSM_VDD_RESTRICTION]);
	}

	if ((gfx_warm_phase_ctrl_enabled) &&
		!(convert_to_zone_id(&thresh[MSM_GFX_PHASE_CTRL_WARM]))) {
		therm_set_threshold(&thresh[MSM_GFX_PHASE_CTRL_WARM]);
	}

	if ((gfx_crit_phase_ctrl_enabled) &&
		!(convert_to_zone_id(&thresh[MSM_GFX_PHASE_CTRL_HOT]))) {
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

int sensor_mgr_init_threshold(struct threshold_info *thresh_inp,
	int sensor_id, int32_t high_temp, int32_t low_temp,
	void (*callback)(struct therm_threshold *))
{
	int ret = 0, i;
	struct therm_threshold *thresh_ptr;

	if (!callback || !thresh_inp
		|| sensor_id == -ENODEV) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto init_thresh_exit;
	}
	if (thresh_inp->thresh_list) {
		pr_info("threshold id already initialized\n");
		goto init_thresh_exit;
	}

	thresh_inp->thresh_ct = (sensor_id == MONITOR_ALL_TSENS) ?
						max_tsens_num : 1;
	thresh_inp->thresh_triggered = false;
	thresh_inp->thresh_list = kzalloc(sizeof(struct therm_threshold) *
					thresh_inp->thresh_ct, GFP_KERNEL);
	if (!thresh_inp->thresh_list) {
		pr_err("kzalloc failed for thresh\n");
		ret = -ENOMEM;
		goto init_thresh_exit;
	}

	thresh_ptr = thresh_inp->thresh_list;
	if (sensor_id == MONITOR_ALL_TSENS) {
		for (i = 0; i < max_tsens_num; i++) {
			thresh_ptr[i].sensor_id = tsens_id_map[i];
			thresh_ptr[i].id_type = THERM_TSENS_ID;
			thresh_ptr[i].notify = callback;
			thresh_ptr[i].trip_triggered = -1;
			thresh_ptr[i].parent = thresh_inp;
			thresh_ptr[i].cur_state = -1;
			thresh_ptr[i].threshold[0].temp =
				high_temp * tsens_scaling_factor;
			thresh_ptr[i].threshold[0].trip =
				THERMAL_TRIP_CONFIGURABLE_HI;
			thresh_ptr[i].threshold[1].temp =
				low_temp * tsens_scaling_factor;
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
		thresh_ptr->parent = thresh_inp;
		thresh_ptr->cur_state = -1;
		thresh_ptr->threshold[0].temp = high_temp * tsens_scaling_factor;
		thresh_ptr->threshold[0].trip =
			THERMAL_TRIP_CONFIGURABLE_HI;
		thresh_ptr->threshold[1].temp = low_temp * tsens_scaling_factor;
		thresh_ptr->threshold[1].trip =
			THERMAL_TRIP_CONFIGURABLE_LOW;
		thresh_ptr->threshold[0].notify =
		thresh_ptr->threshold[1].notify = msm_thermal_notify;
		thresh_ptr->threshold[0].data =
		thresh_ptr->threshold[1].data = (void *)thresh_ptr;
	}
	mutex_lock(&threshold_mutex);
	list_add_tail(&thresh_inp->list_ptr, &thresholds_list);
	mutex_unlock(&threshold_mutex);

init_thresh_exit:
	return ret;
}

void sensor_mgr_disable_threshold(struct threshold_info *thresh_inp)
{
	int i;
	struct therm_threshold *thresh_ptr;

	mutex_lock(&threshold_mutex);
	for (i = 0; i < thresh_inp->thresh_ct; i++) {
		thresh_ptr = &thresh_inp->thresh_list[i];
		thresh_ptr->trip_triggered = -1;
		sensor_cancel_trip(thresh_ptr->sensor_id,
				&thresh_ptr->threshold[0]);
		sensor_cancel_trip(thresh_ptr->sensor_id,
				&thresh_ptr->threshold[1]);
	}
	thresh_inp->thresh_triggered = false;
	mutex_unlock(&threshold_mutex);
}

void sensor_mgr_remove_threshold(struct threshold_info *thresh_inp)
{
	sensor_mgr_disable_threshold(thresh_inp);
	mutex_lock(&threshold_mutex);
	kfree(thresh_inp->thresh_list);
	thresh_inp->thresh_list = NULL;
	thresh_inp->thresh_ct = 0;
	list_del(&thresh_inp->list_ptr);
	mutex_unlock(&threshold_mutex);
}

static int msm_thermal_add_gfx_nodes(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *gfx_kobj = NULL;
	int ret = 0;

	if (!gfx_warm_phase_ctrl_enabled && !gfx_crit_phase_ctrl_enabled)
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
		cpus[cpu].vdd_max_freq = UINT_MAX;
		cpus[cpu].limited_min_freq = 0;
	}
	update_cluster_freq();
	put_online_cpus();
}

static void interrupt_mode_init(void)
{
	if (!msm_thermal_probed)
		return;

	if (polling_enabled) {
		polling_enabled = 0;
		create_sensor_zone_id_map();
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
	uint32_t cpu = 0;

	if (!mitigation) {
		pr_err("Thermal Mitigations disabled.\n");
		goto done_store_cc;
	}

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
		cpus_previously_online_update();
		register_cpu_notifier(&msm_thermal_cpu_notifier);
		/*
		 * Re-evaluate thermal core condition, update current status
		 * and set threshold for all cpus.
		 */
		hotplug_init_cpu_offlined();
		mutex_lock(&core_control_mutex);
		update_offline_cores(cpus_offlined);
		if (hotplug_enabled) {
			for_each_possible_cpu(cpu) {
				if (!(msm_thermal_info.core_control_mask &
					BIT(cpus[cpu].cpu)))
					continue;
				sensor_mgr_set_threshold(cpus[cpu].sensor_id,
				&cpus[cpu].threshold[HOTPLUG_THRESHOLD_HIGH]);
			}
		}
		mutex_unlock(&core_control_mutex);
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

	if (!mitigation) {
		pr_err("Thermal Mitigations disabled.\n");
		goto done_cc;
	}
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

static void msm_thermal_panic_notifier_init(struct device *dev)
{
	int i;

	tsens_temp_at_panic = devm_kzalloc(dev,	sizeof(int) * max_tsens_num,
				GFP_KERNEL);
	if (!tsens_temp_at_panic) {
		pr_err("kzalloc failed\n");
		return;
	}

	for (i = 0; i < max_tsens_num; i++)
		tsens_temp_at_panic[i] = INT_MIN;

	atomic_notifier_chain_register(&panic_notifier_list,
		&msm_thermal_panic_notifier);
}

static int msm_thermal_pre_init(struct device *dev)
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

	if (create_sensor_id_map(dev)) {
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
	mit_config = devm_kzalloc(dev,
			sizeof(struct msm_thermal_debugfs_thresh_config)
			* (MSM_LIST_MAX_NR + MAX_CPU_CONFIG), GFP_KERNEL);
	if (!mit_config) {
		pr_err("kzalloc failed\n");
		ret = -ENOMEM;
		goto pre_init_exit;
	}

pre_init_exit:
	return ret;
}

static int devmgr_devices_init(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t cpu;
	struct device_manager_data *dev_mgr = NULL;

	devices = devm_kzalloc(&pdev->dev,
				sizeof(struct devmgr_devices),
				GFP_KERNEL);
	if (!devices) {
		pr_err("Malloc failed for devmgr devices\n");
		ret = -ENOMEM;
		goto device_exit;
	}
	if (num_possible_cpus() > 1) {
		/* Add hotplug device */
		dev_mgr = devm_kzalloc(&pdev->dev,
		sizeof(struct device_manager_data),
			GFP_KERNEL);
		if (!dev_mgr) {
			pr_err("Malloc failed for hotplug device\n");
			ret = -ENOMEM;
			goto device_exit;
		}
		snprintf(dev_mgr->device_name,
				TSENS_NAME_MAX, HOTPLUG_DEVICE);
		dev_mgr->request_validate =
			devmgr_hotplug_client_request_validate_and_update;
		dev_mgr->update = devmgr_client_hotplug_update;
		HOTPLUG_NO_MITIGATION(&dev_mgr->active_req.offline_mask);
		mutex_init(&dev_mgr->clnt_lock);
		INIT_LIST_HEAD(&dev_mgr->client_list);
		list_add_tail(&dev_mgr->dev_ptr, &devices_list);
		devices->hotplug_dev = dev_mgr;
	}
	/*  Add cpu devices */
	for_each_possible_cpu(cpu) {
		dev_mgr = devm_kzalloc(&pdev->dev,
		sizeof(struct device_manager_data),
			GFP_KERNEL);
		if (!dev_mgr) {
			pr_err("Malloc failed for cpu%d device\n", cpu);
			ret = -ENOMEM;
			goto device_exit;
		}
		snprintf(dev_mgr->device_name, TSENS_NAME_MAX, CPU_DEVICE, cpu);
		dev_mgr->request_validate =
			devmgr_cpufreq_client_request_validate_and_update;
		dev_mgr->update = devmgr_client_cpufreq_update;
		dev_mgr->active_req.freq.max_freq = CPUFREQ_MAX_NO_MITIGATION;
		dev_mgr->active_req.freq.min_freq = CPUFREQ_MIN_NO_MITIGATION;
		mutex_init(&dev_mgr->clnt_lock);
		INIT_LIST_HEAD(&dev_mgr->client_list);
		list_add_tail(&dev_mgr->dev_ptr, &devices_list);
		devices->cpufreq_dev[cpu] = dev_mgr;
	}
device_exit:
	if (ret) {
		if (devices) {
			if (devices->hotplug_dev)
				devm_kfree(&pdev->dev,
					devices->hotplug_dev);
			for_each_possible_cpu(cpu) {
				if (devices->cpufreq_dev[cpu])
					devm_kfree(&pdev->dev,
					devices->cpufreq_dev[cpu]);
			}
		}
	}
	return ret;
}

static void msm_thermal_init_cpu_mit(enum cpu_mit_type cpu_mit)
{
	uint32_t cpu;

	for_each_possible_cpu(cpu) {
		cpus[cpu].cpu = cpu;
		if (cpu_mit & CPU_HOTPLUG_MITIGATION) {
			cpus[cpu].offline = 0;
			cpus[cpu].user_offline = 0;
			cpus[cpu].hotplug_thresh_clear = false;
		}
		if (cpu_mit & CPU_FREQ_MITIGATION) {
			cpus[cpu].max_freq = false;
			cpus[cpu].user_max_freq = UINT_MAX;
			cpus[cpu].shutdown_max_freq = UINT_MAX;
			cpus[cpu].suspend_max_freq = UINT_MAX;
			cpus[cpu].vdd_max_freq = UINT_MAX;
			cpus[cpu].user_min_freq = 0;
			cpus[cpu].limited_max_freq = UINT_MAX;
			cpus[cpu].limited_min_freq = 0;
			cpus[cpu].freq_thresh_clear = false;
		}
	}
}

int msm_thermal_init(struct msm_thermal_data *pdata)
{
	int ret = 0;

	msm_thermal_ioctl_init();
	ret = devmgr_devices_init(pdata->pdev);
	if (ret)
		pr_err("cannot initialize devm devices. err:%d\n", ret);

	msm_thermal_init_cpu_mit(CPU_FREQ_MITIGATION | CPU_HOTPLUG_MITIGATION);
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

	if (!lmh_dcvs_available) {
		register_reboot_notifier(&msm_thermal_reboot_notifier);
		pm_notifier(msm_thermal_suspend_callback, 0);
	}
	INIT_DELAYED_WORK(&retry_hotplug_work, retry_hotplug);

	if (num_possible_cpus() > 1) {
		cpus_previously_online_update();
		register_cpu_notifier(&msm_thermal_cpu_notifier);
	}

	INIT_DELAYED_WORK(&check_temp_work, check_temp);
	schedule_delayed_work(&check_temp_work, 0);
	msm_thermal_panic_notifier_init(&pdata->pdev->dev);

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

static ssize_t bucket_info_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	uint32_t val = 0;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input:%s. ret:%d", buf, ret);
		goto done_store;
	}

	bucket = val & 0xff;
	pr_debug("\"%s\"(PID:%i) request cluster:%d bucket:%d\n",
		current->comm, current->pid, (bucket & 0xf0) >> 4,
		bucket & 0xf);

done_store:
	return count;
}

static ssize_t bucket_info_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", bucket);
}

static struct kobj_attribute bucket_info_attr =
		__ATTR_RW(bucket_info);
static int msm_thermal_add_bucket_info_nodes(void)
{
	struct kobject *module_kobj = NULL;
	int ret = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		return -ENOENT;
	}
	sysfs_attr_init(&bucket_info_attr.attr);
	ret = sysfs_create_file(module_kobj, &bucket_info_attr.attr);
	if (ret) {
		pr_err(
		"cannot create bucket info kobject attribute. err:%d\n", ret);
		return ret;
	}

	return ret;
}

static struct kobj_attribute sensor_info_attr =
		__ATTR_RO(sensor_info);
static int msm_thermal_add_sensor_info_nodes(void)
{
	struct kobject *module_kobj = NULL;
	int ret = 0;

	if (!sensor_info_probed) {
		sensor_info_nodes_called = true;
		return ret;
	}
	if (sensor_info_probed && sensor_cnt == 0)
		return ret;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		return -ENOENT;
	}
	sysfs_attr_init(&sensor_info_attr.attr);
	ret = sysfs_create_file(module_kobj, &sensor_info_attr.attr);
	if (ret) {
		pr_err(
		"cannot create sensor info kobject attribute. err:%d\n",
		ret);
		return ret;
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

static void thermal_cpu_freq_mit_disable(void)
{
	uint32_t cpu = 0, th_cnt = 0;
	struct device_manager_data *dev_mgr = NULL;
	struct device_clnt_data *clnt = NULL;
	char device_name[TSENS_NAME_MAX] = {0};

	freq_mitigation_enabled = 0;
	msm_thermal_init_cpu_mit(CPU_FREQ_MITIGATION);
	for_each_possible_cpu(cpu) {
		for (th_cnt = FREQ_THRESHOLD_HIGH;
			th_cnt <= FREQ_THRESHOLD_LOW; th_cnt++)
			sensor_cancel_trip(cpus[cpu].sensor_id,
				&cpus[cpu].threshold[th_cnt]);

		snprintf(device_name, TSENS_NAME_MAX, CPU_DEVICE, cpu);
		dev_mgr = find_device_by_name(device_name);
		if (!dev_mgr) {
			pr_err("Invalid device %s\n", device_name);
			return;
		}
		mutex_lock(&dev_mgr->clnt_lock);
		list_for_each_entry(clnt, &dev_mgr->client_list, clnt_ptr) {
			if (!clnt->req_active)
				continue;
			clnt->request.freq.max_freq
				= CPUFREQ_MAX_NO_MITIGATION;
			clnt->request.freq.min_freq
				= CPUFREQ_MIN_NO_MITIGATION;
		}
		dev_mgr->active_req.freq.max_freq = CPUFREQ_MAX_NO_MITIGATION;
		dev_mgr->active_req.freq.min_freq = CPUFREQ_MIN_NO_MITIGATION;
		mutex_unlock(&dev_mgr->clnt_lock);
	}
	if (freq_mitigation_task)
		complete(&freq_mitigation_complete);
	else
		pr_err("Freq mit task is not initialized\n");
}

static void thermal_cpu_hotplug_mit_disable(void)
{
	uint32_t cpu = 0, th_cnt = 0;
	struct device_manager_data *dev_mgr = NULL;
	struct device_clnt_data *clnt = NULL;

	mutex_lock(&core_control_mutex);
	hotplug_enabled = 0;
	msm_thermal_init_cpu_mit(CPU_HOTPLUG_MITIGATION);
	for_each_possible_cpu(cpu) {
		if (!(msm_thermal_info.core_control_mask & BIT(cpu)))
			continue;

		for (th_cnt = HOTPLUG_THRESHOLD_HIGH;
			th_cnt <= HOTPLUG_THRESHOLD_LOW; th_cnt++)
			sensor_cancel_trip(cpus[cpu].sensor_id,
				&cpus[cpu].threshold[th_cnt]);
	}

	dev_mgr = find_device_by_name(HOTPLUG_DEVICE);
	if (!dev_mgr) {
		pr_err("Invalid device %s\n", HOTPLUG_DEVICE);
		mutex_unlock(&core_control_mutex);
		return;
	}
	mutex_lock(&dev_mgr->clnt_lock);
	list_for_each_entry(clnt, &dev_mgr->client_list, clnt_ptr) {
		if (!clnt->req_active)
			continue;
		HOTPLUG_NO_MITIGATION(&clnt->request.offline_mask);
	}
	HOTPLUG_NO_MITIGATION(&dev_mgr->active_req.offline_mask);
	mutex_unlock(&dev_mgr->clnt_lock);

	if (hotplug_task)
		complete(&hotplug_notify_complete);
	else
		pr_err("Hotplug task is not initialized\n");

	mutex_unlock(&core_control_mutex);
}

static void thermal_reset_disable(void)
{
	THERM_MITIGATION_DISABLE(therm_reset_enabled, MSM_THERM_RESET);
}

static void thermal_mx_mit_disable(void)
{
	int ret = 0;

	THERM_MITIGATION_DISABLE(vdd_mx_enabled, MSM_VDD_MX_RESTRICTION);
	ret = remove_vdd_mx_restriction();
	if (ret)
		pr_err("Failed to remove vdd mx restriction\n");
}

static void thermal_vdd_mit_disable(void)
{
	int ret = 0;

	THERM_MITIGATION_DISABLE(vdd_rstr_enabled, MSM_VDD_RESTRICTION);
	ret = vdd_restriction_apply_all(0);
	if (ret)
		pr_err("Disable vdd rstr for all failed. err:%d\n", ret);
}

static void thermal_psm_mit_disable(void)
{
	int ret = 0;

	THERM_MITIGATION_DISABLE(psm_enabled, -1);
	ret = psm_set_mode_all(PMIC_AUTO_MODE);
	if (ret)
		pr_err("Set auto mode for all failed. err:%d\n", ret);
}

static void thermal_ocr_mit_disable(void)
{
	int ret = 0;

	THERM_MITIGATION_DISABLE(ocr_enabled, MSM_OCR);
	ret = ocr_set_mode_all(OPTIMUM_CURRENT_MAX);
	if (ret)
		pr_err("Set max optimum current failed. err:%d\n", ret);
}

static void thermal_cx_phase_ctrl_mit_disable(void)
{
	int ret = 0;

	THERM_MITIGATION_DISABLE(cx_phase_ctrl_enabled, MSM_CX_PHASE_CTRL_HOT);
	ret = send_temperature_band(MSM_CX_PHASE_CTRL, MSM_WARM);
	if (ret)
		pr_err("cx band set to WARM failed. err:%d\n", ret);
}

static void thermal_gfx_phase_warm_ctrl_mit_disable(void)
{
	int ret = 0;

	if (gfx_warm_phase_ctrl_enabled) {
		THERM_MITIGATION_DISABLE(gfx_warm_phase_ctrl_enabled,
					MSM_GFX_PHASE_CTRL_WARM);
		ret = send_temperature_band(MSM_GFX_PHASE_CTRL, MSM_NORMAL);
		if (!ret)
			pr_err("gfx phase set to NORMAL failed. err:%d\n",
				ret);
	}
}

static void thermal_gfx_phase_crit_ctrl_mit_disable(void)
{
	int ret = 0;

	if (gfx_crit_phase_ctrl_enabled) {
		THERM_MITIGATION_DISABLE(gfx_crit_phase_ctrl_enabled,
					MSM_GFX_PHASE_CTRL_HOT);
		ret = send_temperature_band(MSM_GFX_PHASE_CTRL, MSM_NORMAL);
		if (!ret)
			pr_err("gfx phase set to NORMAL failed. err:%d\n",
				ret);
	}
}

static int probe_vdd_mx(struct device_node *node,
		struct msm_thermal_data *data, struct platform_device *pdev)
{
	int ret = 0;
	char *key = NULL;

	key = "qcom,disable-vdd-mx";
	if (of_property_read_bool(node, key)) {
		vdd_mx_enabled = false;
		return ret;
	}

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

	/*
	 * Monitor only this sensor if defined, otherwise monitor all tsens
	 */
	key = "qcom,mx-restriction-sensor_id";
	if (of_property_read_u32(node, key, &data->vdd_mx_sensor_id))
		data->vdd_mx_sensor_id = MONITOR_ALL_TSENS;

	vdd_mx = devm_regulator_get(&pdev->dev, "vdd-mx");
	if (IS_ERR_OR_NULL(vdd_mx)) {
		ret = PTR_ERR(vdd_mx);
		if (ret != -EPROBE_DEFER) {
			pr_err(
			"Could not get regulator: vdd-mx, err:%d\n", ret);
		}
		goto read_node_done;
	}

	key = "qcom,cx-retention-min";
	ret = of_property_read_u32(node, key, &data->vdd_cx_min);
	if (!ret) {
		vdd_cx = devm_regulator_get(&pdev->dev, "vdd-cx");
		if (IS_ERR_OR_NULL(vdd_cx)) {
			ret = PTR_ERR(vdd_cx);
			if (ret != -EPROBE_DEFER) {
				pr_err(
				"Could not get regulator: vdd-cx, err:%d\n",
				ret);
			}
			goto read_node_done;
		}
	}

	ret = sensor_mgr_init_threshold(&thresh[MSM_VDD_MX_RESTRICTION],
			data->vdd_mx_sensor_id,
			data->vdd_mx_temp_degC + data->vdd_mx_temp_hyst_degC,
			data->vdd_mx_temp_degC, vdd_mx_notify);

read_node_done:
	if (!ret) {
		vdd_mx_enabled = true;
		snprintf(mit_config[MSM_VDD_MX_RESTRICTION].config_name,
			MAX_DEBUGFS_CONFIG_LEN, "mx");
		mit_config[MSM_VDD_MX_RESTRICTION].disable_config
			= thermal_mx_mit_disable;
	} else if (ret != -EPROBE_DEFER) {
		dev_info(&pdev->dev,
			"%s:Failed reading node=%s, key=%s. KTM continues\n",
			__func__, node->full_name, key);
	}

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

	key = "qcom,disable-vdd-rstr";
	if (of_property_read_bool(node, key)) {
		vdd_rstr_probed = true;
		vdd_rstr_enabled = false;
		rails_cnt = 0;
		return ret;
	}

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
		if (rails[i].freq_req) {
			rails[i].min_level = 0;
			key = "qcom,max-freq-level";
			ret = of_property_read_u32(child_node, key,
				&rails[i].max_frequency_limit);
			if (ret)
				rails[i].max_frequency_limit
					= UINT_MAX;
			ret = 0;
		} else {
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
		ret = sensor_mgr_init_threshold(&thresh[MSM_VDD_RESTRICTION],
			MONITOR_ALL_TSENS,
			data->vdd_rstr_temp_hyst_degC, data->vdd_rstr_temp_degC,
			vdd_restriction_notify);
		if (ret) {
			pr_err("Error in initializing thresholds. err:%d\n",
					ret);
			goto read_node_fail;
		}
		vdd_rstr_enabled = true;
		snprintf(mit_config[MSM_VDD_RESTRICTION].config_name,
			MAX_DEBUGFS_CONFIG_LEN, "vdd");
		mit_config[MSM_VDD_RESTRICTION].disable_config
			= thermal_vdd_mit_disable;
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

static int create_alias_name(int _cpu, struct device_node *limits,
		struct platform_device *pdev)
{
	char device_name[DEVM_NAME_MAX] = "";
	int sensor_idx = 0, sensor_ct = 0, idx = 0, err = 0;
	struct device_node *tsens = NULL;
	const char *sensor_name = NULL;

	if (!sensors) {
		pr_debug("sensor info not defined\n");
		return -ENOSYS;
	}
	snprintf(device_name, DEVM_NAME_MAX, CPU_DEVICE, _cpu);

	if (!of_get_property(limits, "qcom,temperature-sensor", &sensor_ct)
		|| sensor_ct <= 0) {
		pr_err("Sensor not defined\n");
		return -ENODEV;
	}
	sensor_ct /= sizeof(__be32);
	do {
		tsens = of_parse_phandle(limits, "qcom,temperature-sensor",
				idx);
		if (!tsens) {
			pr_err("No temperature sensor defined for CPU%d\n",
				_cpu);
			return -ENODEV;
		}

		err = of_property_read_string(tsens, "qcom,sensor-name",
			&sensor_name);
		if (err) {
			pr_err("Sensor name not populated for CPU%d. err:%d\n",
					_cpu, err);
			return -ENODEV;
		}
		for (sensor_idx = 0; sensor_idx < sensor_cnt; sensor_idx++) {
			char cat_str[DEVM_NAME_MAX] = "";

			if (strcmp(sensors[sensor_idx].name, sensor_name))
				continue;
			if (!sensors[sensor_idx].alias) {
				sensors[sensor_idx].alias = devm_kzalloc(
					&pdev->dev, DEVM_NAME_MAX, GFP_KERNEL);
				if (!sensors[sensor_idx].alias) {
					pr_err("Memory alloc failed\n");
					return -ENOMEM;
				}
				strlcpy((char *)sensors[sensor_idx].alias,
					device_name, DEVM_NAME_MAX);
				if (sensor_ct > 1) {
					/* Multiple sensor monitoring
					 * single device */
					snprintf(cat_str, DEVM_NAME_MAX, "_%d",
						idx);
					strlcat((char *)
						sensors[sensor_idx].alias,
						cat_str, DEVM_NAME_MAX);
				}
			} else {
				/* Single sensor monitoring multiple devices */
				snprintf(cat_str, DEVM_NAME_MAX,
					"-"CPU_DEVICE, _cpu);
				strlcat((char *)sensors[sensor_idx].alias,
					cat_str, DEVM_NAME_MAX);
			}
			break;
		}
		idx++;
	} while (idx < sensor_ct);

	return 0;
}

static int fetch_cpu_mitigaiton_info(struct msm_thermal_data *data,
		struct platform_device *pdev)
{

	int _cpu = 0, err = 0;
	struct device_node *cpu_node = NULL, *limits = NULL, *tsens = NULL;
	char *key = NULL;
	struct device_node *node = pdev->dev.of_node;

	key = "qcom,sensor-id";
	err = of_property_read_u32(node, key, &data->sensor_id);
	if (err)
		goto fetch_mitig_exit;

	key = "qcom,poll-ms";
	err = of_property_read_u32(node, key, &data->poll_ms);
	if (err)
		goto fetch_mitig_exit;

	for_each_possible_cpu(_cpu) {
		const char *sensor_name = NULL;

		cpu_node = of_get_cpu_node(_cpu, NULL);
		if (!cpu_node) {
			pr_err("No CPU phandle for CPU%d\n", _cpu);
			__WARN();
			continue;
		}
		limits = of_parse_phandle(cpu_node, "qcom,limits-info", 0);
		if (!limits) {
			pr_err("No mitigation info defined for CPU%d\n", _cpu);
			continue;
		}
		VALIDATE_AND_SET_MASK(limits, "qcom,boot-frequency-mitigate",
			data->bootup_freq_control_mask, _cpu);
		VALIDATE_AND_SET_MASK(limits,
			"qcom,emergency-frequency-mitigate",
			data->freq_mitig_control_mask, _cpu);
		VALIDATE_AND_SET_MASK(limits, "qcom,hotplug-mitigation-enable",
			data->core_control_mask, _cpu);

		tsens = of_parse_phandle(limits, "qcom,temperature-sensor", 0);
		if (!tsens) {
			pr_err("No temperature sensor defined for CPU%d\n",
				_cpu);
			continue;
		}

		err = of_property_read_string(tsens, "qcom,sensor-name",
			&sensor_name);
		if (err) {
			pr_err("Sensor name not populated for CPU%d. err:%d\n",
					_cpu, err);
			continue;
		}
		cpus[_cpu].sensor_type = devm_kzalloc(&pdev->dev,
					strlen(sensor_name) + 1, GFP_KERNEL);
		if (!cpus[_cpu].sensor_type) {
			pr_err("Memory alloc failed\n");
			err = -ENOMEM;
			goto fetch_mitig_exit;
		}
		strlcpy((char *) cpus[_cpu].sensor_type, sensor_name,
			strlen(sensor_name) + 1);
		create_alias_name(_cpu, limits, pdev);
	}

fetch_mitig_exit:
	return err;
}

static void probe_sensor_info(struct device_node *node,
		struct msm_thermal_data *data, struct platform_device *pdev)
{
	int err = 0;
	int i = 0;
	char *key = NULL;
	struct device_node *child_node = NULL;
	struct device_node *np = NULL;
	int scale_tsens_found = 0;

	key = "qcom,disable-sensor-info";
	if (of_property_read_bool(node, key)) {
		sensor_info_probed = true;
		return;
	}

	np = of_find_compatible_node(NULL, NULL, "qcom,sensor-information");
	if (!np) {
		dev_info(&pdev->dev,
		"%s:unable to find DT for sensor-information.KTM continues\n",
		__func__);
		sensor_info_probed = true;
		return;
	}
	sensor_cnt = of_get_child_count(np);
	if (sensor_cnt == 0) {
		err = -ENODEV;
		goto read_node_fail;
	}

	sensors = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_sensor_info) * sensor_cnt,
			GFP_KERNEL);
	if (!sensors) {
		pr_err("Fail to allocate memory for sensor_info.\n");
		err = -ENOMEM;
		goto read_node_fail;
	}

	for_each_child_of_node(np, child_node) {
		const char *alias_name = NULL;

		key = "qcom,sensor-type";
		err = of_property_read_string(child_node,
				key, &sensors[i].type);
		if (err)
			goto read_node_fail;

		key = "qcom,sensor-name";
		err = of_property_read_string(child_node,
				key, &sensors[i].name);
		if (err)
			goto read_node_fail;

		key = "qcom,alias-name";
		of_property_read_string(child_node, key, &alias_name);
		if (alias_name && !strnstr(alias_name, "cpu",
			strlen(alias_name)))
			sensors[i].alias = alias_name;

		key = "qcom,scaling-factor";
		err = of_property_read_u32(child_node, key,
				&sensors[i].scaling_factor);
		if (err || sensors[i].scaling_factor == 0) {
			sensors[i].scaling_factor = SENSOR_SCALING_FACTOR;
			err = 0;
		}
		if (scale_tsens_found == 0) {
			if (!strcmp(sensors[i].type, "tsens")) {
				scale_tsens_found = 1;
				tsens_scaling_factor =
					sensors[i].scaling_factor;
			}
		}
		i++;
	}

read_node_fail:
	sensor_info_probed = true;
	if (err) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			__func__, np->full_name, key, err);
		devm_kfree(&pdev->dev, sensors);
	}
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

	key = "qcom,disable-ocr";
	if (of_property_read_bool(node, key)) {
		ocr_probed = true;
		ocr_enabled = false;
		ocr_rail_cnt = 0;
		goto read_ocr_exit;
	}

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

	ret = sensor_mgr_init_threshold(&thresh[MSM_OCR], data->ocr_sensor_id,
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
	} else {
		snprintf(mit_config[MSM_OCR].config_name,
			MAX_DEBUGFS_CONFIG_LEN, "ocr");
		mit_config[MSM_OCR].disable_config = thermal_ocr_mit_disable;
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

	key = "qcom,disable-psm";
	if (of_property_read_bool(node, key)) {
		psm_probed = true;
		psm_enabled = false;
		psm_rails_cnt = 0;
		return ret;
	}

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
	int ret = 0;

	if (num_possible_cpus() > 1) {
		core_control_enabled = 1;
		hotplug_enabled = 1;
	}

	key = "qcom,online-hotplug-core";
	if (of_property_read_bool(node, key))
		online_core = true;
	else
		online_core = false;

	key = "qcom,core-limit-temp";
	ret = of_property_read_u32(node, key, &data->core_limit_temp_degC);
	if (ret)
		goto read_node_fail;

	key = "qcom,core-temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->core_temp_hysteresis_degC);
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

read_node_fail:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
		core_control_enabled = 0;
	} else {
		snprintf(mit_config[MSM_LIST_MAX_NR + HOTPLUG_CONFIG]
			.config_name, MAX_DEBUGFS_CONFIG_LEN,
			"hotplug");
		mit_config[MSM_LIST_MAX_NR + HOTPLUG_CONFIG].disable_config
			= thermal_cpu_hotplug_mit_disable;
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

	key = "qcom,disable-gfx-phase-ctrl";
	if (of_property_read_bool(node, key)) {
		gfx_crit_phase_ctrl_enabled = false;
		gfx_warm_phase_ctrl_enabled = false;
		return ret;
	}

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

	key = "qcom,gfx-phase-warm-temp";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_warm_temp_degC);
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
		data->gfx_phase_warm_temp_degC = INT_MIN;
		goto probe_gfx_crit;
	}

	key = "qcom,gfx-phase-warm-temp-hyst";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_warm_temp_hyst_degC);
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
		goto probe_gfx_crit;
	}

	ret = sensor_mgr_init_threshold(&thresh[MSM_GFX_PHASE_CTRL_WARM],
		data->gfx_sensor,
		data->gfx_phase_warm_temp_degC, data->gfx_phase_warm_temp_degC -
		data->gfx_phase_warm_temp_hyst_degC,
		gfx_phase_ctrl_notify);
	if (ret) {
		pr_err("init WARM threshold failed. err:%d\n", ret);
		goto probe_gfx_crit;
	}
	gfx_warm_phase_ctrl_enabled = true;
	snprintf(mit_config[MSM_GFX_PHASE_CTRL_WARM].config_name,
		MAX_DEBUGFS_CONFIG_LEN, "gfx_phase_warm");
	mit_config[MSM_GFX_PHASE_CTRL_WARM].disable_config
		= thermal_gfx_phase_warm_ctrl_mit_disable;

probe_gfx_crit:
	key = "qcom,gfx-phase-hot-crit-temp";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_hot_temp_degC);
	if (ret) {
		data->gfx_phase_hot_temp_degC = INT_MAX;
		goto probe_gfx_exit;
	}

	key = "qcom,gfx-phase-hot-crit-temp-hyst";
	ret = of_property_read_u32(node, key,
		&data->gfx_phase_hot_temp_hyst_degC);
	if (ret)
		goto probe_gfx_exit;

	ret = sensor_mgr_init_threshold(&thresh[MSM_GFX_PHASE_CTRL_HOT],
		data->gfx_sensor,
		data->gfx_phase_hot_temp_degC, data->gfx_phase_hot_temp_degC -
		data->gfx_phase_hot_temp_hyst_degC,
		gfx_phase_ctrl_notify);
	if (ret) {
		pr_err("init HOT threshold failed. err:%d\n", ret);
		goto probe_gfx_exit;
	}

	gfx_crit_phase_ctrl_enabled = true;
	snprintf(mit_config[MSM_GFX_PHASE_CTRL_HOT].config_name,
		MAX_DEBUGFS_CONFIG_LEN, "gfx_phase_crit");
	mit_config[MSM_GFX_PHASE_CTRL_HOT].disable_config
		= thermal_gfx_phase_crit_ctrl_mit_disable;

probe_gfx_exit:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			KBUILD_MODNAME, node->full_name, key, ret);
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

	key = "qcom,disable-cx-phase-ctrl";
	if (of_property_read_bool(node, key)) {
		cx_phase_ctrl_enabled = false;
		return ret;
	}

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

	ret = sensor_mgr_init_threshold(&thresh[MSM_CX_PHASE_CTRL_HOT],
		MONITOR_ALL_TSENS,
		data->cx_phase_hot_temp_degC, data->cx_phase_hot_temp_degC -
		data->cx_phase_hot_temp_hyst_degC,
		cx_phase_ctrl_notify);
	if (ret) {
		pr_err("init HOT threshold failed. err:%d\n", ret);
		goto probe_cx_exit;
	}

	cx_phase_ctrl_enabled = true;
	snprintf(mit_config[MSM_CX_PHASE_CTRL_HOT].config_name,
		MAX_DEBUGFS_CONFIG_LEN, "cx_phase");
	mit_config[MSM_CX_PHASE_CTRL_HOT].disable_config
		= thermal_cx_phase_ctrl_mit_disable;

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

	ret = sensor_mgr_init_threshold(&thresh[MSM_THERM_RESET],
		MONITOR_ALL_TSENS,
		data->therm_reset_temp_degC, data->therm_reset_temp_degC - 10,
		therm_reset_notify);
	if (ret) {
		pr_err("Therm reset data structure init failed\n");
		goto PROBE_RESET_EXIT;
	}

	therm_reset_enabled = true;
	snprintf(mit_config[MSM_THERM_RESET].config_name,
		MAX_DEBUGFS_CONFIG_LEN, "reset");
	mit_config[MSM_THERM_RESET].disable_config
		= thermal_reset_disable;

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

	key = "qcom,limit-temp";
	ret = of_property_read_u32(node, key, &data->limit_temp_degC);
	if (ret)
		goto PROBE_FREQ_EXIT;

	key = "qcom,temp-hysteresis";
	ret = of_property_read_u32(node, key, &data->temp_hysteresis_degC);
	if (ret)
		goto PROBE_FREQ_EXIT;

	key = "qcom,freq-step";
	ret = of_property_read_u32(node, key, &data->bootup_freq_step);
	if (ret)
		goto PROBE_FREQ_EXIT;
	boot_freq_mitig_enabled = true;

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

	freq_mitigation_enabled = 1;
	snprintf(mit_config[MSM_LIST_MAX_NR + CPUFREQ_CONFIG].config_name,
		MAX_DEBUGFS_CONFIG_LEN, "cpufreq");
	mit_config[MSM_LIST_MAX_NR + CPUFREQ_CONFIG].disable_config
		= thermal_cpu_freq_mit_disable;

PROBE_FREQ_EXIT:
	if (ret) {
		dev_info(&pdev->dev,
		"%s:Failed reading node=%s, key=%s. err=%d. KTM continues\n",
			__func__, node->full_name, key, ret);
		freq_mitigation_enabled = 0;
	}
	return ret;
}

static void thermal_boot_config_read(struct seq_file *m, void *data)
{

	seq_puts(m, "---------Boot Mitigation------------\n");
	seq_printf(m, "tsens sensor:tsens_tz_sensor%d\n",
			msm_thermal_info.sensor_id);
	seq_printf(m, "polling rate:%d ms\n", msm_thermal_info.poll_ms);
	seq_printf(m, "frequency threshold:%d degC\n",
			msm_thermal_info.limit_temp_degC);
	seq_printf(m, "frequency threshold clear:%d degC\n",
			msm_thermal_info.limit_temp_degC
			- msm_thermal_info.temp_hysteresis_degC);
	seq_printf(m, "frequency step:%d\n",
			msm_thermal_info.bootup_freq_step);
	seq_printf(m, "frequency mask:0x%x\n",
			msm_thermal_info.bootup_freq_control_mask);
	seq_printf(m, "hotplug threshold:%d degC\n",
			msm_thermal_info.core_limit_temp_degC);
	seq_printf(m, "hotplug threshold clear:%d degC\n",
			msm_thermal_info.core_limit_temp_degC
			- msm_thermal_info.core_temp_hysteresis_degC);
	seq_printf(m, "hotplug mask:0x%x\n",
			msm_thermal_info.core_control_mask);
	seq_printf(m, "reset threshold:%d degC\n",
			msm_thermal_info.therm_reset_temp_degC);
}

static void thermal_emergency_config_read(struct seq_file *m, void *data)
{
	int cpu = 0;

	seq_puts(m, "\n---------Emergency Mitigation------------\n");
	for_each_possible_cpu(cpu)
		seq_printf(m, "cpu%d sensor:%s\n", cpu, cpus[cpu].sensor_type);
	seq_printf(m, "frequency threshold:%d degC\n",
			msm_thermal_info.freq_mitig_temp_degc);
	seq_printf(m, "frequency threshold clr:%d degC\n",
			msm_thermal_info.freq_mitig_temp_degc
			- msm_thermal_info.freq_mitig_temp_hysteresis_degc);
	seq_printf(m, "frequency value:%d KHz\n",
			msm_thermal_info.freq_limit);
	seq_printf(m, "frequency mask:0x%x\n",
			msm_thermal_info.freq_mitig_control_mask);
	seq_printf(m, "hotplug threshold:%d degC\n",
			msm_thermal_info.hotplug_temp_degC);
	seq_printf(m, "hotplug threshold clr:%d degC\n",
			msm_thermal_info.hotplug_temp_degC
			- msm_thermal_info.hotplug_temp_hysteresis_degC);
	seq_printf(m, "hotplug mask:0x%x\n",
			msm_thermal_info.core_control_mask);
	seq_printf(m, "online hotplug core:%s\n", online_core
			? "true" : "false");

}

static void thermal_mx_config_read(struct seq_file *m, void *data)
{
	if (vdd_mx_enabled) {
		seq_puts(m, "\n---------Mx Retention------------\n");
		seq_printf(m, "threshold:%d degC\n",
				msm_thermal_info.vdd_mx_temp_degC);
		seq_printf(m, "threshold clear:%d degC\n",
				msm_thermal_info.vdd_mx_temp_degC
				+ msm_thermal_info.vdd_mx_temp_hyst_degC);
		seq_printf(m, "mx retention value:%d\n",
				msm_thermal_info.vdd_mx_min);
		if (vdd_cx)
			seq_printf(m, "cx retention value:%d\n",
				msm_thermal_info.vdd_cx_min);
		if (msm_thermal_info.vdd_mx_sensor_id != MONITOR_ALL_TSENS)
			seq_printf(m, "tsens sensor:tsens_tz_sensor%d\n",
				msm_thermal_info.vdd_mx_sensor_id);
	}
}

static void thermal_vdd_config_read(struct seq_file *m, void *data)
{
	int i = 0;

	if (vdd_rstr_enabled) {
		seq_puts(m, "\n---------VDD restriction------------\n");
		seq_printf(m, "threshold:%d degC\n",
				msm_thermal_info.vdd_rstr_temp_degC);
		seq_printf(m, "threshold clear:%d degC\n",
				msm_thermal_info.vdd_rstr_temp_hyst_degC);
		for (i = 0; i < rails_cnt; i++) {
			if (!strcmp(rails[i].name, "vdd-dig")
				&& rails[i].num_levels)
				seq_printf(m, "vdd_dig restriction value:%d\n",
					rails[i].levels[0]);
			if (!strcmp(rails[i].name, "vdd-gfx")
				&& rails[i].num_levels)
				seq_printf(m, "vdd_gfx restriction value:%d\n",
					rails[i].levels[0]);
			if (!strcmp(rails[i].name, "vdd-apps")
				&& rails[i].num_levels)
				seq_printf(m,
					"vdd_apps restriction value:%d KHz\n",
					rails[i].levels[0]);
		}
	}
}

static void thermal_psm_config_read(struct seq_file *m, void *data)
{
	if (psm_enabled) {
		seq_puts(m, "\n------PMIC Software Mode(PSM)-------\n");
		seq_printf(m, "threshold:%d degC\n",
				msm_thermal_info.psm_temp_degC);
		seq_printf(m, "threshold clear:%d degC\n",
				msm_thermal_info.psm_temp_degC
				- msm_thermal_info.psm_temp_hyst_degC);
	}
}

static void thermal_ocr_config_read(struct seq_file *m, void *data)
{
	if (ocr_enabled) {
		seq_puts(m, "\n-----Optimum Current Request(OCR)-----\n");
		seq_printf(m, "threshold:%d degC\n",
				msm_thermal_info.ocr_temp_degC);
		seq_printf(m, "threshold clear:%d degC\n",
				msm_thermal_info.ocr_temp_degC
				- msm_thermal_info.ocr_temp_hyst_degC);
		seq_printf(m, "tsens sensor:tsens_tz_sensor%d\n",
				msm_thermal_info.ocr_sensor_id);
	}
}

static void thermal_phase_ctrl_config_read(struct seq_file *m, void *data)
{
	if (cx_phase_ctrl_enabled) {
		seq_puts(m, "\n---------Phase control------------\n");
		seq_printf(m, "cx hot critical threshold:%d degC\n",
				msm_thermal_info.cx_phase_hot_temp_degC);
		seq_printf(m, "cx hot critical threshold clear:%d degC\n",
			msm_thermal_info.cx_phase_hot_temp_degC
			- msm_thermal_info.cx_phase_hot_temp_hyst_degC);
	}
	if (gfx_crit_phase_ctrl_enabled) {
		seq_printf(m, "gfx hot critical threshold:%d degC\n",
				msm_thermal_info.gfx_phase_hot_temp_degC);
		seq_printf(m, "gfx hot critical threshold clear:%d degC\n",
			msm_thermal_info.gfx_phase_hot_temp_degC
			- msm_thermal_info.gfx_phase_hot_temp_hyst_degC);
	}
	if (gfx_warm_phase_ctrl_enabled) {
		seq_printf(m, "gfx warm threshold:%d degC\n",
				msm_thermal_info.gfx_phase_warm_temp_degC);
		seq_printf(m, "gfx warm threshold clear:%d degC\n",
			msm_thermal_info.gfx_phase_warm_temp_degC
			- msm_thermal_info.gfx_phase_warm_temp_hyst_degC);
	}
	if (gfx_crit_phase_ctrl_enabled || gfx_warm_phase_ctrl_enabled)
		seq_printf(m, "gfx tsens sensor:tsens_tz_sensor%d\n",
			msm_thermal_info.gfx_sensor);
}

static void thermal_disable_all_mitigation(void)
{
	thermal_cpu_freq_mit_disable();
	thermal_cpu_hotplug_mit_disable();
	thermal_reset_disable();
	thermal_mx_mit_disable();
	thermal_vdd_mit_disable();
	thermal_psm_mit_disable();
	thermal_ocr_mit_disable();
	thermal_cx_phase_ctrl_mit_disable();
	thermal_gfx_phase_warm_ctrl_mit_disable();
	thermal_gfx_phase_crit_ctrl_mit_disable();
}

static void enable_config(int config_id)
{
	switch (config_id) {
	case MSM_THERM_RESET:
		therm_reset_enabled = 1;
		break;
	case MSM_VDD_RESTRICTION:
		vdd_rstr_enabled = 1;
		break;
	case MSM_CX_PHASE_CTRL_HOT:
		cx_phase_ctrl_enabled = 1;
		break;
	case MSM_GFX_PHASE_CTRL_WARM:
		gfx_warm_phase_ctrl_enabled = 1;
		break;
	case MSM_GFX_PHASE_CTRL_HOT:
		gfx_crit_phase_ctrl_enabled = 1;
		break;
	case MSM_OCR:
		ocr_enabled = 1;
		break;
	case MSM_VDD_MX_RESTRICTION:
		vdd_mx_enabled = 1;
		break;
	case MSM_LIST_MAX_NR + HOTPLUG_CONFIG:
		hotplug_enabled = 1;
		break;
	case MSM_LIST_MAX_NR + CPUFREQ_CONFIG:
		freq_mitigation_enabled = 1;
		break;
	default:
		pr_err("Bad config:%d\n", config_id);
		break;
	}
}

static void thermal_update_mit_threshold(
		struct msm_thermal_debugfs_thresh_config *config, int max_mit)
{
	int idx = 0, i = 0;

	for (idx = 0; idx < max_mit; idx++) {
		if (!config[idx].update)
			continue;
		config[idx].disable_config();
		enable_config(idx);
		if (idx >= MSM_LIST_MAX_NR) {
			if (idx == MSM_LIST_MAX_NR + HOTPLUG_CONFIG)
				UPDATE_CPU_CONFIG_THRESHOLD(
					msm_thermal_info.core_control_mask,
					HOTPLUG_THRESHOLD_HIGH,
					config[idx].thresh,
					config[idx].thresh_clr);
			else if (idx == MSM_LIST_MAX_NR + CPUFREQ_CONFIG)
				UPDATE_CPU_CONFIG_THRESHOLD(
					msm_thermal_info
					.freq_mitig_control_mask,
					FREQ_THRESHOLD_HIGH,
					config[idx].thresh,
					config[idx].thresh_clr);
		} else {
			for (i = 0; i < thresh[idx].thresh_ct; i++) {
				thresh[idx].thresh_list[i].threshold[0].temp
					= config[idx].thresh
					* tsens_scaling_factor;
				thresh[idx].thresh_list[i].threshold[1].temp
					= config[idx].thresh_clr
					* tsens_scaling_factor;
				set_and_activate_threshold(
					thresh[idx].thresh_list[i].sensor_id,
					&thresh[idx].thresh_list[i]
					.threshold[0]);
				set_and_activate_threshold(
					thresh[idx].thresh_list[i].sensor_id,
					&thresh[idx].thresh_list[i]
					.threshold[1]);
			}
		}
		config[idx].update = 0;
	}
}

static ssize_t thermal_config_debugfs_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	char config_string[MAX_DEBUGFS_CONFIG_LEN] = { '\0' };

	if (!mitigation || count > (MAX_DEBUGFS_CONFIG_LEN - 1)) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	if (copy_from_user(config_string, buffer, count)) {
		pr_err("Error reading debugfs command\n");
		ret = -EFAULT;
		goto exit_debugfs_write;
	}
	pr_debug("Debugfs config command string: %s\n", config_string);
	if (!strcmp(config_string, DEBUGFS_DISABLE_ALL_MIT)) {
		mitigation = 0;
		pr_err("KTM mitigations disabled via debugfs\n");
		thermal_disable_all_mitigation();
	} else if (!strcmp(config_string, DEBUGFS_CONFIG_UPDATE)) {
		thermal_update_mit_threshold(mit_config, MSM_LIST_MAX_NR
			+ MAX_CPU_CONFIG);
	}

exit_debugfs_write:
	if (!ret)
		return count;
	return ret;
}

static int thermal_config_debugfs_read(struct seq_file *m, void *data)
{
	if (!mitigation) {
		seq_puts(m, "KTM Mitigations Disabled\n");
		return 0;
	}
	thermal_boot_config_read(m, data);
	thermal_emergency_config_read(m, data);
	thermal_mx_config_read(m, data);
	thermal_vdd_config_read(m, data);
	thermal_psm_config_read(m, data);
	thermal_ocr_config_read(m, data);
	thermal_phase_ctrl_config_read(m, data);

	return 0;
}

static void msm_thermal_late_sysfs_init(void)
{
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
	if (sensor_info_nodes_called) {
		msm_thermal_add_sensor_info_nodes();
		sensor_info_nodes_called = false;
	}
	if (ocr_nodes_called) {
		msm_thermal_add_ocr_nodes();
		ocr_nodes_called = false;
	}
	if (cluster_info_nodes_called) {
		create_cpu_topology_sysfs();
		cluster_info_nodes_called = false;
	}
}

static int probe_deferrable_properties(struct device_node *node,
	struct msm_thermal_data *data, struct platform_device *pdev)
{
	int ret = 0;

	/*
	 * Probe optional properties below. Call probe_psm before
	 * probe_vdd_rstr because rpm_regulator_get has to be called
	 * before devm_regulator_get
	 * probe_ocr should be called after probe_vdd_rstr to reuse the
	 * regualtor handle. calling devm_regulator_get more than once
	 * will fail.
	 */
	ret = probe_psm(node, data, pdev);
	if (ret == -EPROBE_DEFER)
		return ret;

	ret = probe_vdd_rstr(node, data, pdev);
	if (ret == -EPROBE_DEFER)
		return ret;

	probe_ocr(node, data, pdev);
	ret = probe_vdd_mx(node, data, pdev);
	if (ret == -EPROBE_DEFER)
		return ret;

	return 0;
}

static int msm_thermal_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	struct msm_thermal_data data;

	if (!mitigation)
		return ret;

	memset(&data, 0, sizeof(struct msm_thermal_data));
	data.pdev = pdev;

	ret = msm_thermal_pre_init(&pdev->dev);
	if (ret) {
		pr_err("thermal pre init failed. err:%d\n", ret);
		goto probe_exit;
	}
	ret = probe_deferrable_properties(node, &data, pdev);
	if (ret)
		goto probe_exit;

	probe_sensor_info(node, &data, pdev);
	probe_cc(node, &data, pdev);
	probe_freq_mitigation(node, &data, pdev);
	probe_cx_phase_ctrl(node, &data, pdev);
	probe_gfx_phase_ctrl(node, &data, pdev);
	probe_therm_reset(node, &data, pdev);
	update_cpu_topology(&pdev->dev);
	ret = fetch_cpu_mitigaiton_info(&data, pdev);
	if (ret) {
		pr_err("Error fetching CPU mitigation information. err:%d\n",
				ret);
		goto probe_exit;
	}
	msm_thermal_late_sysfs_init();
	ret = msm_thermal_init(&data);
	if (ret)
		goto probe_exit;
	msm_thermal_probed = true;

probe_exit:
	return ret;
}

static int msm_thermal_dev_exit(struct platform_device *inp_dev)
{
	int i = 0;
	uint32_t _cluster = 0;
	struct cluster_info *cluster_ptr = NULL;
	struct uio_info *info = dev_get_drvdata(&inp_dev->dev);
	struct rail *r = NULL;

	uio_unregister_device(info);
	unregister_reboot_notifier(&msm_thermal_reboot_notifier);
	if (msm_therm_debugfs && msm_therm_debugfs->parent)
		debugfs_remove_recursive(msm_therm_debugfs->parent);
	msm_thermal_ioctl_cleanup();
	if (thresh) {
		if (vdd_rstr_enabled) {
			sensor_mgr_remove_threshold(
				&thresh[MSM_VDD_RESTRICTION]);
			kfree(thresh[MSM_VDD_RESTRICTION].thresh_list);
			for (i = 0; i < rails_cnt; i++) {
				if (!rails[i].freq_req)
					continue;
				r = &rails[i];
				for_each_possible_cpu(i) {
					devmgr_unregister_mitigation_client(
						&msm_thermal_info.pdev->dev,
						r->device_handle[i]);
					r->device_handle[i] = NULL;
				}
			}
			kfree(rails);
		}
		if (cx_phase_ctrl_enabled) {
			sensor_mgr_remove_threshold(
				&thresh[MSM_CX_PHASE_CTRL_HOT]);
			kfree(thresh[MSM_CX_PHASE_CTRL_HOT].thresh_list);
		}
		if (gfx_warm_phase_ctrl_enabled) {
			sensor_mgr_remove_threshold(
				&thresh[MSM_GFX_PHASE_CTRL_WARM]);
			kfree(thresh[MSM_GFX_PHASE_CTRL_WARM].thresh_list);
		}
		if (gfx_crit_phase_ctrl_enabled) {
			sensor_mgr_remove_threshold(
				&thresh[MSM_GFX_PHASE_CTRL_HOT]);
			kfree(thresh[MSM_GFX_PHASE_CTRL_HOT].thresh_list);
		}
		if (ocr_enabled) {
			for (i = 0; i < ocr_rail_cnt; i++)
				kfree(ocr_rails[i].attr_gp.attrs);
			kfree(ocr_rails);
			ocr_rails = NULL;
			sensor_mgr_remove_threshold(
				&thresh[MSM_OCR]);
			kfree(thresh[MSM_OCR].thresh_list);
		}
		if (vdd_mx_enabled) {
			kfree(mx_kobj);
			kfree(mx_attr_group.attrs);
			sensor_mgr_remove_threshold(
				&thresh[MSM_VDD_MX_RESTRICTION]);
			kfree(thresh[MSM_VDD_MX_RESTRICTION].thresh_list);
		}
		kfree(thresh);
		thresh = NULL;
	}
	kfree(table);
	if (core_ptr) {
		for (; _cluster < core_ptr->entity_count; _cluster++) {
			cluster_ptr = &core_ptr->child_entity_ptr[_cluster];
			kfree(cluster_ptr->freq_table);
		}
	}

	return 0;
}

static int __init ktm_params(char *str)
{
	if (str != NULL && !strcmp(str, "disable")) {
		pr_info("KTM Disabled at Boot\n");
		mitigation = 0;
	}

	return 0;
}

early_param("qcomthermal", ktm_params);

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
	if (!msm_thermal_probed)
		return 0;

	if (num_possible_cpus() > 1)
		msm_thermal_add_cc_nodes();
	msm_thermal_add_psm_nodes();
	msm_thermal_add_vdd_rstr_nodes();
	msm_thermal_add_sensor_info_nodes();
	if (ocr_reg_init_defer) {
		if (!ocr_reg_init(msm_thermal_info.pdev)) {
			ocr_enabled = true;
			msm_thermal_add_ocr_nodes();
		}
	}
	msm_thermal_add_mx_nodes();
	create_cpu_topology_sysfs();
	create_thermal_debugfs();
	msm_thermal_add_bucket_info_nodes();
	uio_init(msm_thermal_info.pdev);

	return 0;
}
late_initcall(msm_thermal_late_init);
