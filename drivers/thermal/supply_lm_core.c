/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/pm_opp.h>
#include <linux/of_platform.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <clocksource/arm_arch_timer.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/scm.h>
#include <linux/thermal.h>
#include <linux/msm_thermal.h>

#define CREATE_TRACE_POINTS
#define TRACE_SUPPLY_LM
#include <trace/trace_thermal.h>

#define SUPPLY_LM_NAME_MAX           20
#define SUPPLY_LM_DRIVER_NAME        "supply_lm"
#define SUPPLY_LM_INTERRUPT          "supply_lm_modem-interrupt"
#define SUPPLY_LM_MODEM_DATA_OFFSET  0x4
#define MODEM_INTR_CLEAR             0x0
#define MODE_MAX                     32
#define CPU_BUF_SIZE                 64
#define SUPPLY_LM_GPU_OFF_RATE       19200000
#define NO_DIV_CPU_FREQ_HZ           533333333
#define NO_DIV_CPU_FREQ_KHZ          533333
#define MITIGATION_MAX_DELAY         30
#define SUPPLY_LM_GET_MIT_CMD        2
#define SUPPLY_LM_STEP1_REQ_CMD      3
#define __ATTR_RW(attr) __ATTR(attr, 0644, attr##_show, attr##_store)

enum supply_lm_input_device {
	SUPPLY_LM_THERM_DEVICE,
	SUPPLY_LM_NUM_CORE_DEVICE,
	SUPPLY_LM_CPU_DEVICE,
	SUPPLY_LM_MODEM_DEVICE,
	SUPPLY_LM_GPU_DEVICE,
	SUPPLY_LM_DEVICE_MAX,
};

enum corner_state {
	CORNER_STATE_OFF,
	CORNER_STATE_SVS,
	CORNER_STATE_NOMINAL,
	CORNER_STATE_TURBO,
	CORNER_STATE_MAX
};

enum modem_corner_state {
	MODEM_CORNER_STATE_OFF,
	MODEM_CORNER_STATE_RETENTION,
	MODEM_CORNER_STATE_LOW_MINUS,
	MODEM_CORNER_STATE_LOW,
	MODEM_CORNER_STATE_NOMINAL,
	MODEM_CORNER_STATE_NOMINAL_PLUS,
	MODEM_CORNER_STATE_TURBO,
	MODEM_CORNER_STATE_MAX,
};

enum therm_state {
	THERM_NORMAL,
	THERM_HOT,
	THERM_VERY_HOT,
	MAX_THERM_LEVEL,
};

enum supply_lm_dbg_event {
	SUPPLY_LM_DBG_IO,
	SUPPLY_LM_DBG_PRE_HPLG,
	SUPPLY_LM_DBG_PST_HPLG,
	SUPPLY_LM_DBG_PRE_CFRQ,
	SUPPLY_LM_DBG_PST_CFRQ,
	SUPPLY_LM_DBG_STEP1_REL,
};

enum debug_mask {
	SUPPLY_LM_INPUTS = BIT(0),
	SUPPLY_LM_IO_DATA = BIT(1),
	SUPPLY_LM_STEP2_MITIGATION_REQUEST = BIT(2),
	SUPPLY_LM_POST_MITIGATION_REQUEST = BIT(3),
};

struct freq_corner_map {
	unsigned int freq;
	enum corner_state state;
};

union input_device_state {
	enum corner_state            state;
	enum therm_state             therm_state;
	uint32_t                     num_cores;
};

struct supply_lm_mitigation_data {
	uint32_t                     core_offline_req;
	uint32_t                     freq_req;
	uint32_t                     step1_rel;
};

struct supply_lm_input_data {
	uint32_t                     num_cores;
	enum therm_state             therm_state;
	enum corner_state            cpu_state;
	enum corner_state            gpu_state;
	enum corner_state            modem_state;
};

struct input_device_info;
struct device_ops {
	int (*setup)(struct platform_device *, struct input_device_info *);
	int (*read_state)(struct input_device_info *,
				struct supply_lm_input_data *);
	void (*clean_up)(struct platform_device *, struct input_device_info *);
};

struct input_device_info {
	char                         device_name[SUPPLY_LM_NAME_MAX];
	bool                         enabled;
	struct device_ops            ops;
	union input_device_state     curr;
	struct list_head             list_ptr;
	struct mutex                 lock;
	void                         *data;
};

struct supply_lm_modem_hw {
	int                          irq_num;
	struct workqueue_struct      *isr_wq;
	struct work_struct           isr_work;
	void                         *intr_base_reg;
};

struct supply_lm_core_data {
	bool                         enabled;
	struct platform_device       *pdev;
	int                          hot_temp_degC;
	int                          hot_temp_hyst;
	int                          very_hot_temp_degC;
	int                          very_hot_temp_hyst;
	struct platform_device       *gpu_pdev;
	struct clk                   *gpu_handle;
	struct supply_lm_modem_hw    modem_hw;
	cpumask_t                    cpu_idle_mask;
	uint32_t                     supply_lm_limited_max_freq;
	cpumask_t                    supply_lm_offlined_cpu_mask;
	bool                         step2_cpu_freq_initiated;
	bool                         step2_hotplug_initiated;
	bool                         suspend_in_progress;
	uint32_t                     inp_trig_for_mit;
	struct device_clnt_data      *hotplug_handle;
	struct device_clnt_data      *cpufreq_handle;
};

struct supply_lm_debugfs_entry {
	struct dentry                *parent;
	struct dentry                *bypass_real_inp;
	struct dentry                *user_input;
};

struct supply_lm_debug {
	cycle_t                      time;
	enum supply_lm_dbg_event     evt;
	uint32_t                     num_cores;
	/* [31:24]=therm_state, [23:16]=cpu_state
	 * [15:8]=modem_state, [7:0]=gpu_state
	 */
	uint32_t                     hw_state;
	uint32_t                     core_offline_req;
	uint32_t                     freq_req;
	uint32_t                     step1_rel;
	uint32_t                     arg;
};

static DEFINE_MUTEX(supply_lm_tz_lock);
static DEFINE_SPINLOCK(supply_lm_pm_lock);
static LIST_HEAD(supply_lm_device_list);
static struct supply_lm_core_data *supply_lm_data;
static struct input_device_info supply_lm_devices[];
static struct threshold_info *supply_lm_therm_thresh;
static struct task_struct *supply_lm_monitor_task;
static struct completion supply_lm_notify_complete;
static struct completion supply_lm_mitigation_complete;
static struct supply_lm_debugfs_entry *supply_lm_debugfs;
static u32 supply_lm_bypass_inp;
static uint32_t supply_lm_therm_status[MAX_THERM_LEVEL] = {UINT_MAX, 0, 0};
static struct supply_lm_debug *supply_lm_debug;
static phys_addr_t supply_lm_debug_phys;
static const int num_dbg_elements = 0x100;
static uint32_t cpufreq_table_len;
static uint32_t gpufreq_table_len;
static struct freq_corner_map *cpufreq_corner_map;
static struct freq_corner_map *gpufreq_corner_map;
static int supply_lm_debug_mask;
static bool gpufreq_opp_corner_enabled;

static void supply_lm_remove_devices(struct platform_device *pdev);
static int supply_lm_devices_init(struct platform_device *pdev);

module_param_named(
	debug_mask, supply_lm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static int apps_corner_map[] = {
	[RPM_REGULATOR_CORNER_NONE] = CORNER_STATE_OFF,
	[RPM_REGULATOR_CORNER_SVS_SOC] = CORNER_STATE_SVS,
	[RPM_REGULATOR_CORNER_NORMAL] = CORNER_STATE_NOMINAL,
	[RPM_REGULATOR_CORNER_SUPER_TURBO] = CORNER_STATE_TURBO,
};

static int modem_corner_map[] = {
	[MODEM_CORNER_STATE_OFF] = CORNER_STATE_OFF,
	[MODEM_CORNER_STATE_RETENTION] = CORNER_STATE_OFF,
	[MODEM_CORNER_STATE_LOW_MINUS] = CORNER_STATE_OFF,
	[MODEM_CORNER_STATE_LOW] = CORNER_STATE_SVS,
	[MODEM_CORNER_STATE_NOMINAL] = CORNER_STATE_NOMINAL,
	[MODEM_CORNER_STATE_NOMINAL_PLUS] = CORNER_STATE_NOMINAL,
	[MODEM_CORNER_STATE_TURBO] = CORNER_STATE_TURBO,
};

static void update_mtgtn_debug_event(enum supply_lm_dbg_event event,
		struct supply_lm_input_data *inp_data,
		struct supply_lm_mitigation_data *mtgtn_data,
		uint32_t arg)
{
	struct supply_lm_debug *dbg;
	uint32_t idx;
	static int mtgtn_idx;
	static DEFINE_SPINLOCK(debug_lock);

	if (!supply_lm_debug)
		return;

	spin_lock(&debug_lock);
	idx = mtgtn_idx++;
	dbg = &supply_lm_debug[idx & (num_dbg_elements - 1)];

	dbg->evt = event;
	dbg->time = arch_counter_get_cntpct();
	if (inp_data) {
		dbg->num_cores = inp_data->num_cores;
		dbg->hw_state = (inp_data->therm_state << 24)|
				(inp_data->cpu_state << 16)|
				(inp_data->modem_state << 8)|
				(inp_data->gpu_state);
	} else {
		dbg->num_cores = 0xDECADEED;
		dbg->hw_state = 0xDECADEED;
	}
	if (mtgtn_data) {
		dbg->core_offline_req = mtgtn_data->core_offline_req;
		dbg->freq_req = mtgtn_data->freq_req;
		dbg->step1_rel = mtgtn_data->step1_rel;
	} else {
		dbg->core_offline_req = 0xDECADEED;
		dbg->freq_req = 0xDECADEED;
		dbg->step1_rel = 0xDECADEED;
	}
	dbg->arg = arg;
	spin_unlock(&debug_lock);
}

static ssize_t supply_lm_input_write(struct file *fp,
					const char __user *user_buffer,
					size_t count, loff_t *position)
{
	int ret = 0;
	char buf[MODE_MAX];
	char *cmp;
	enum therm_state therm;
	uint32_t num_cores;
	enum corner_state cpu;
	enum corner_state gpu;
	enum corner_state modem;

	if (count > (MODE_MAX - 1)) {
		pr_err("Invalid user input\n");
		return -EINVAL;
	}

	if (copy_from_user(&buf, user_buffer, count))
		return -EFAULT;

	buf[count] = '\0';
	cmp =  strstrip(buf);

	ret = sscanf(cmp, "%d %d %d %d %d", (int *)&therm, &num_cores,
			(int *)&cpu, (int *)&modem, (int *)&gpu);
	if (ret != SUPPLY_LM_DEVICE_MAX) {
		pr_err("Invalid user input. ret:%d\n", ret);
		return -EINVAL;
	}

	pr_debug("T%d N%d C%d M%d G%d\n", therm, num_cores, cpu, modem, gpu);
	supply_lm_devices[SUPPLY_LM_THERM_DEVICE].curr.therm_state = therm;
	supply_lm_devices[SUPPLY_LM_NUM_CORE_DEVICE].curr.num_cores = num_cores;
	supply_lm_devices[SUPPLY_LM_CPU_DEVICE].curr.state = cpu;
	supply_lm_devices[SUPPLY_LM_MODEM_DEVICE].curr.state = modem;
	supply_lm_devices[SUPPLY_LM_GPU_DEVICE].curr.state = gpu;

	if (supply_lm_monitor_task && !supply_lm_data->suspend_in_progress)
		complete(&supply_lm_notify_complete);

	return count;
}

static const struct file_operations supply_lm_user_input_ops = {
	.write = supply_lm_input_write,
};

static int create_supply_lm_debugfs(struct platform_device *pdev)
{
	int ret = 0;

	if (supply_lm_debugfs)
		return ret;

	supply_lm_debugfs = devm_kzalloc(&pdev->dev,
			sizeof(struct supply_lm_debugfs_entry), GFP_KERNEL);
	if (!supply_lm_debugfs) {
		ret = -ENOMEM;
		pr_err("Memory alloc failed. err:%d\n", ret);
		return ret;
	}

	supply_lm_debugfs->parent =
		debugfs_create_dir(SUPPLY_LM_DRIVER_NAME, NULL);
	if (IS_ERR(supply_lm_debugfs->parent)) {
		ret = PTR_ERR(supply_lm_debugfs->parent);
		pr_err("Error creating debugfs:[%s]. err:%d\n",
			SUPPLY_LM_DRIVER_NAME, ret);
		goto create_exit;
	}
	supply_lm_debugfs->user_input = debugfs_create_file("user_input", 0600,
		supply_lm_debugfs->parent, &supply_lm_devices,
		&supply_lm_user_input_ops);
	if (IS_ERR(supply_lm_debugfs->user_input)) {
		ret = PTR_ERR(supply_lm_debugfs->user_input);
		pr_err("Error creating debugfs:[%s]. err:%d\n",
			"user_input", ret);
		goto create_exit;
	}
	supply_lm_debugfs->bypass_real_inp =
					debugfs_create_bool("bypass_real_inp",
					0600, supply_lm_debugfs->parent,
					&supply_lm_bypass_inp);
	if (IS_ERR(supply_lm_debugfs->bypass_real_inp)) {
		ret = PTR_ERR(supply_lm_debugfs->bypass_real_inp);
		pr_err("Error creating debugfs:[%s]. err:%d\n",
			"bypass_real_inp", ret);
		goto create_exit;
	}
create_exit:
	if (ret) {
		debugfs_remove_recursive(supply_lm_debugfs->parent);
		devm_kfree(&pdev->dev, supply_lm_debugfs);
	}
	return ret;
}

static void clear_all_mitigation(void)
{
	union device_request disable_req;
	int ret = 0;

	mutex_lock(&supply_lm_tz_lock);
	trace_supply_lm_pre_scm(0);
	ret = scm_call_atomic1(SCM_SVC_PWR, SUPPLY_LM_STEP1_REQ_CMD, 0);
	trace_supply_lm_post_scm(ret);
	mutex_unlock(&supply_lm_tz_lock);
	if (ret < 0)
		pr_err("scm_call failed\n");

	if (supply_lm_data->cpufreq_handle) {
		disable_req.freq.max_freq = CPUFREQ_MAX_NO_MITIGATION;
		disable_req.freq.min_freq = CPUFREQ_MIN_NO_MITIGATION;
		devmgr_client_request_mitigation(supply_lm_data->cpufreq_handle,
				CPUFREQ_MITIGATION_REQ, &disable_req);
		supply_lm_data->supply_lm_limited_max_freq =
					CPUFREQ_MAX_NO_MITIGATION;
	}
	if (supply_lm_data->hotplug_handle) {
		HOTPLUG_NO_MITIGATION(&disable_req.offline_mask);
		devmgr_client_request_mitigation(supply_lm_data->hotplug_handle,
				HOTPLUG_MITIGATION_REQ, &disable_req);
		HOTPLUG_NO_MITIGATION(
			&supply_lm_data->supply_lm_offlined_cpu_mask);
	}
}

static ssize_t mode_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	if (!supply_lm_data)
		return -EPERM;

	if (!strcmp(buf, "enable")) {
		if (supply_lm_data->enabled)
			goto store_exit;
		/* disable and re-enable all devices */
		supply_lm_remove_devices(supply_lm_data->pdev);
		ret = supply_lm_devices_init(supply_lm_data->pdev);
		if (ret)
			return -EINVAL;

		supply_lm_data->enabled = true;
		if (supply_lm_monitor_task &&
				!supply_lm_data->suspend_in_progress)
			complete(&supply_lm_notify_complete);
	} else if (!strcmp(buf, "disable")) {
		if (!supply_lm_data->enabled)
			goto store_exit;
		supply_lm_remove_devices(supply_lm_data->pdev);
		supply_lm_data->enabled = false;
		clear_all_mitigation();
	} else {
		pr_err("write error %s\n", buf);
		return -EINVAL;
	}
store_exit:
	return count;
}

static ssize_t mode_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	if (!supply_lm_data)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "%s\n",
		supply_lm_data->enabled ? "enabled" : "disabled");
}

static struct kobj_attribute supply_lm_dev_attr =
				__ATTR_RW(mode);
static int create_supply_lm_sysfs(void)
{
	int ret = 0;
	struct kobject *module_kobj = NULL;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("cannot find kobject\n");
		return -ENOENT;
	}
	sysfs_attr_init(&supply_lm_dev_attr.attr);
	ret = sysfs_create_file(module_kobj, &supply_lm_dev_attr.attr);
	if (ret) {
		pr_err(
		"cannot create mode sysfs kobject attribute. err:%d\n", ret);
		return ret;
	}

	return ret;
}

static void devmgr_mitigation_callback(struct device_clnt_data *clnt,
					union device_request *req,
					void *data)
{
	uint32_t step1_rel = (int)data;
	int ret = 0;

	if (!clnt) {
		pr_err("Invalid client\n");
		return;
	}
	if (!supply_lm_data->step2_cpu_freq_initiated &&
		!supply_lm_data->step2_hotplug_initiated) {
		return;
	}

	if (supply_lm_debug_mask & SUPPLY_LM_POST_MITIGATION_REQUEST)
		pr_info(
		"status:Hotplug:%d freq:%d po_vote:%d input-req:%d\n",
			supply_lm_data->step2_cpu_freq_initiated,
			supply_lm_data->step2_hotplug_initiated,
			step1_rel,
			supply_lm_data->inp_trig_for_mit);

	if (supply_lm_data->step2_cpu_freq_initiated &&
		supply_lm_data->cpufreq_handle == clnt) {
		supply_lm_data->step2_cpu_freq_initiated = false;
		update_mtgtn_debug_event(SUPPLY_LM_DBG_PST_CFRQ,
				NULL, NULL, req->freq.max_freq);
	} else if (supply_lm_data->step2_hotplug_initiated &&
		supply_lm_data->hotplug_handle == clnt) {
		supply_lm_data->step2_hotplug_initiated = false;
		update_mtgtn_debug_event(SUPPLY_LM_DBG_PST_HPLG,
				NULL, NULL, 0);
	} else {
		return;
	}

	if (!supply_lm_data->step2_cpu_freq_initiated &&
		!supply_lm_data->step2_hotplug_initiated) {
		if (!supply_lm_data->inp_trig_for_mit &&
				((step1_rel & 1) ^ (step1_rel >> 16))) {
			update_mtgtn_debug_event(SUPPLY_LM_DBG_STEP1_REL,
						NULL, NULL, step1_rel);
			mutex_lock(&supply_lm_tz_lock);
			trace_supply_lm_pre_scm(step1_rel);
			ret = scm_call_atomic1(SCM_SVC_PWR,
					SUPPLY_LM_STEP1_REQ_CMD,
					step1_rel >> 16);
			trace_supply_lm_post_scm(ret);
			mutex_unlock(&supply_lm_tz_lock);
			if (ret < 0)
				pr_err("scm_call failed. with ret:%d\n", ret);
			if (supply_lm_monitor_task)
				complete(&supply_lm_mitigation_complete);
		}
	}
}

static int supply_lm_devices_read(struct supply_lm_input_data *input)
{
	int ret = 0;
	struct input_device_info *curr_device = NULL;

	list_for_each_entry(curr_device, &supply_lm_device_list, list_ptr) {
		if (!curr_device->ops.read_state) {
			pr_err("Sensor read not defined for %s\n",
				curr_device->device_name);
			ret = -EINVAL;
			break;
		}
		ret = curr_device->ops.read_state(curr_device, input);
		if (ret) {
			pr_err("Sensor read failed for %s\n",
				curr_device->device_name);
			ret = -EINVAL;
			break;
		}
	}
	return ret;
}

static int get_curr_hotplug_request(uint8_t core_num, cpumask_t *cpu_mask,
				char *buf)
{
	int cpu = 0;
	cpumask_t offline_mask = CPU_MASK_NONE;

	for_each_possible_cpu(cpu) {
		if (!cpu_online(cpu))
			cpumask_set_cpu(cpu, &offline_mask);
	}
	if (core_num == cpumask_weight(&offline_mask)) {
		cpumask_copy(cpu_mask, &offline_mask);
	} else if (core_num > cpumask_weight(&offline_mask)) {
		for (cpu = num_possible_cpus() - 1; cpu >= 0; cpu--) {
			if (!cpu_online(cpu))
				continue;
			cpumask_set_cpu(cpu, &offline_mask);
			if (core_num == cpumask_weight(&offline_mask))
				break;
		}
		cpumask_copy(cpu_mask, &offline_mask);
	} else if (core_num < cpumask_weight(&offline_mask)) {
		for_each_possible_cpu(cpu) {
			if (cpu_online(cpu))
				continue;
			if (cpumask_test_cpu(cpu, &offline_mask)) {
				cpumask_clear_cpu(cpu, &offline_mask);
				if (core_num == cpumask_weight(&offline_mask))
					break;
			}
		}
		cpumask_copy(cpu_mask, &offline_mask);
	}
	if (SUPPLY_LM_STEP2_MITIGATION_REQUEST & supply_lm_debug_mask) {
		cpumask_scnprintf(buf, CPU_BUF_SIZE, &offline_mask);
		pr_info("core req %d offline_mask %s\n", core_num, buf);
	}

	return 0;
}

static int handle_step2_mitigation(
			struct supply_lm_mitigation_data *mit_state)
{
	int ret = 0;
	cpumask_t req_cpu_mask = CPU_MASK_NONE;
	bool hotplug_req = false;
	bool freq_req = false;
	union device_request curr_req;
	char buf[CPU_BUF_SIZE];

	ret = get_curr_hotplug_request(mit_state->core_offline_req,
					&req_cpu_mask, buf);
	if (ret)
		goto step2_exit;

	if (!cpumask_equal(&supply_lm_data->supply_lm_offlined_cpu_mask,
					&req_cpu_mask)) {
		hotplug_req = true;
		cpumask_copy(&supply_lm_data->supply_lm_offlined_cpu_mask,
					&req_cpu_mask);
	}

	if (supply_lm_data->supply_lm_limited_max_freq != mit_state->freq_req) {
		freq_req = true;
		supply_lm_data->supply_lm_limited_max_freq =
						mit_state->freq_req;
	}

	/* Handle hotplug request */
	if (supply_lm_data->hotplug_handle && hotplug_req) {
		if (SUPPLY_LM_STEP2_MITIGATION_REQUEST & supply_lm_debug_mask)
			pr_info("hotplug request for cpu mask:0x%s\n", buf);

		update_mtgtn_debug_event(SUPPLY_LM_DBG_PRE_HPLG,
				NULL, mit_state,
				0);

		supply_lm_data->hotplug_handle->usr_data =
				(void *)mit_state->step1_rel;
		supply_lm_data->step2_hotplug_initiated = true;
		cpumask_copy(&curr_req.offline_mask,
				&supply_lm_data->supply_lm_offlined_cpu_mask);
		ret = devmgr_client_request_mitigation(
						supply_lm_data->hotplug_handle,
						HOTPLUG_MITIGATION_REQ,
						&curr_req);
		if (ret) {
			pr_err("hotplug request failed. err:%d\n", ret);
			goto step2_exit;
		}
	}

	/* Handle cpufreq request */
	if (supply_lm_data->cpufreq_handle && freq_req) {
		if (SUPPLY_LM_STEP2_MITIGATION_REQUEST & supply_lm_debug_mask)
			pr_info("cpufreq request for max freq %u\n",
				supply_lm_data->supply_lm_limited_max_freq);

		update_mtgtn_debug_event(SUPPLY_LM_DBG_PRE_CFRQ,
				NULL, mit_state,
				supply_lm_data->supply_lm_limited_max_freq);

		supply_lm_data->cpufreq_handle->usr_data =
				(void *)mit_state->step1_rel;
		supply_lm_data->step2_cpu_freq_initiated = true;
		curr_req.freq.max_freq =
			supply_lm_data->supply_lm_limited_max_freq;
		curr_req.freq.min_freq = CPUFREQ_MIN_NO_MITIGATION;
		ret = devmgr_client_request_mitigation(
						supply_lm_data->cpufreq_handle,
						CPUFREQ_MITIGATION_REQ,
						&curr_req);
		if (ret) {
			pr_err("cpufreq request failed. err:%d\n", ret);
			goto step2_exit;
		}
	}
step2_exit:
	return ret;
}

static int read_and_update_mitigation_state(bool step2_req)
{
	int ret = 0;
	struct supply_lm_input_data input;
	struct supply_lm_mitigation_data mit_state;
	enum corner_state req_state;
	uint32_t core_online_req = num_possible_cpus();

	ret = supply_lm_devices_read(&input);
	if (ret)
		goto read_exit;

	/*
	 * Optimize states of cpu, gpu and modem
	 * due to single supply architecture.
	 */
	pr_debug("States before OPT T:%d N:%d C:%d G:%d M:%d\n",
				input.therm_state,
				input.num_cores,
				input.cpu_state,
				input.gpu_state,
				input.modem_state);
	req_state =
		max(input.gpu_state, max(input.cpu_state, input.modem_state));
	input.cpu_state = req_state;

	if (input.gpu_state > CORNER_STATE_OFF)
		input.gpu_state = req_state;

	if (input.modem_state > CORNER_STATE_OFF)
		input.modem_state = req_state;

	pr_debug("states c:%d g:%d m:%d\n",
			input.cpu_state,
			input.gpu_state,
			input.modem_state);
	mutex_lock(&supply_lm_tz_lock);
	trace_supply_lm_pre_scm(input.therm_state | (input.num_cores << 4) |
				(input.cpu_state << 8) |
				(input.gpu_state << 12) |
				(input.modem_state << 16));
	ret = scm_call_atomic5_3(SCM_SVC_PWR, SUPPLY_LM_GET_MIT_CMD,
					input.therm_state, input.num_cores,
					input.cpu_state, input.modem_state,
					input.gpu_state,
					&core_online_req,
					&mit_state.freq_req,
					&mit_state.step1_rel);
	mit_state.core_offline_req = num_possible_cpus() - core_online_req;
	trace_supply_lm_post_scm(ret);
	if (ret) {
		/* log all return variables for debug */
		pr_err("scm call error. ret:%d O1:%d O2:%d O3:0x%x\n", ret,
				core_online_req, mit_state.freq_req,
				mit_state.step1_rel);
		mutex_unlock(&supply_lm_tz_lock);
		goto read_exit;
	}
	update_mtgtn_debug_event(SUPPLY_LM_DBG_IO, &input, &mit_state, 0);
	mutex_unlock(&supply_lm_tz_lock);
	if (SUPPLY_LM_IO_DATA & supply_lm_debug_mask)
		pr_info(
		"I/O:T:%d N:%d C:%d M:%d G:%d: Hplg:%d Fm:%u Step1_rel:0x%x\n",
			input.therm_state, input.num_cores,
			input.cpu_state,
			input.modem_state,
			input.gpu_state,
			mit_state.core_offline_req,
			mit_state.freq_req,
			mit_state.step1_rel);

	if (step2_req)
		ret = handle_step2_mitigation(&mit_state);

read_exit:
	return ret;
}

static int supply_lm_monitor(void *data)
{
	while (!kthread_should_stop()) {
		while (wait_for_completion_interruptible(
			&supply_lm_notify_complete) != 0)
			;
		INIT_COMPLETION(supply_lm_notify_complete);

		if (supply_lm_data->enabled) {
			supply_lm_data->inp_trig_for_mit = 0;
			read_and_update_mitigation_state(true);
		}

		/*  To serialize mitigation, wait for devmgr callback */
		if (supply_lm_data->step2_cpu_freq_initiated ||
			supply_lm_data->step2_hotplug_initiated) {
			while (wait_for_completion_interruptible_timeout(
				&supply_lm_mitigation_complete,
				msecs_to_jiffies(MITIGATION_MAX_DELAY)) < 0)
				;
			INIT_COMPLETION(supply_lm_mitigation_complete);
		}
	}
	return 0;
}

static void supply_lm_remove_devices(struct platform_device *pdev)
{
	struct input_device_info *curr_device = NULL, *next_device = NULL;

	list_for_each_entry_safe(curr_device, next_device,
				&supply_lm_device_list, list_ptr) {
		curr_device->ops.clean_up(pdev, curr_device);
		list_del(&curr_device->list_ptr);
		pr_debug("Deregistering Sensor:[%s]\n",
			curr_device->device_name);
	}
}

static int supply_lm_modem_state_read(struct input_device_info *device,
				struct supply_lm_input_data *curr_state)
{
	enum modem_corner_state modem;

	if (supply_lm_bypass_inp) {
		curr_state->modem_state = device->curr.state;
		return 0;
	}
	modem = readl_relaxed(supply_lm_data->modem_hw.intr_base_reg +
				SUPPLY_LM_MODEM_DATA_OFFSET);
	device->curr.state = modem_corner_map[modem];
	curr_state->modem_state = device->curr.state;
	return 0;
}

static void supply_lm_modem_notify(struct work_struct *work)
{
	enum modem_corner_state modem_state;
	struct input_device_info *modem =
				&supply_lm_devices[SUPPLY_LM_MODEM_DEVICE];

	trace_supply_lm_inp_start_trig(SUPPLY_LM_MODEM_DEVICE,
						modem->curr.state);
	if (!supply_lm_data->enabled ||
		supply_lm_bypass_inp)
		goto modem_exit;

	modem_state = readl_relaxed(supply_lm_data->modem_hw.intr_base_reg +
					SUPPLY_LM_MODEM_DATA_OFFSET);
	if (modem->curr.state == modem_corner_map[modem_state])
		goto modem_exit;

	modem->curr.state = modem_corner_map[modem_state];
	if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
		pr_info("s1 lm modem interrupt: modem data%d corner%d\n",
			modem_state, modem->curr.state);
	read_and_update_mitigation_state(false);
	supply_lm_data->inp_trig_for_mit |= BIT(SUPPLY_LM_MODEM_DEVICE);
	if (supply_lm_monitor_task && supply_lm_data->suspend_in_progress)
		complete(&supply_lm_notify_complete);
modem_exit:
	enable_irq(supply_lm_data->modem_hw.irq_num);
	trace_supply_lm_inp_end_trig(SUPPLY_LM_MODEM_DEVICE,
					modem->curr.state);
	return;
}

static void clear_modem_interrupt(void)
{
	writel_relaxed(MODEM_INTR_CLEAR,
			supply_lm_data->modem_hw.intr_base_reg);
}

static irqreturn_t supply_lm_modem_handle_isr(int irq, void *data)
{
	struct supply_lm_core_data *supply_lm_data =
					(struct supply_lm_core_data *)data;
	clear_modem_interrupt();
	disable_irq_nosync(supply_lm_data->modem_hw.irq_num);
	queue_work(supply_lm_data->modem_hw.isr_wq,
			&supply_lm_data->modem_hw.isr_work);

	return IRQ_HANDLED;
}

static int supply_lm_modem_device_init(struct platform_device *pdev,
					struct input_device_info *device)
{
	int ret = 0;

	supply_lm_data->modem_hw.isr_wq =
			alloc_workqueue("supply_lm_modem_isr_wq",
						WQ_HIGHPRI, 0);
	if (!supply_lm_data->modem_hw.isr_wq) {
		pr_err("Error allocating workqueue\n");
		ret = -ENOMEM;
		goto init_exit;
	}
	INIT_WORK(&supply_lm_data->modem_hw.isr_work, supply_lm_modem_notify);

	supply_lm_data->modem_hw.irq_num = platform_get_irq(pdev, 0);
	if (supply_lm_data->modem_hw.irq_num < 0) {
		pr_err("Error getting IRQ number. %d\n",
				supply_lm_data->modem_hw.irq_num);
		ret = supply_lm_data->modem_hw.irq_num;
		goto init_exit;
	}

	ret = request_irq(supply_lm_data->modem_hw.irq_num,
				supply_lm_modem_handle_isr,
				IRQF_TRIGGER_HIGH,
				SUPPLY_LM_INTERRUPT,
				supply_lm_data);
	if (ret) {
		pr_err("Error getting irq for SUPPLY LM. err:%d\n", ret);
		goto init_exit;
	}

init_exit:
	if (ret)
		supply_lm_remove_devices(pdev);
	return ret;
}

static unsigned int get_voltage_from_opp(struct device *dev,
					unsigned long freq,
					unsigned int *volt_id)
{
	int ret = 0;
	struct opp *opp = NULL;

	rcu_read_lock();
	opp = dev_pm_opp_find_freq_exact(dev, freq, true);
	if (IS_ERR(opp)) {
		pr_err("failed to find valid OPP for freq: %lu\n",
					freq);
		ret = -EINVAL;
		goto opp_exit;
	} else {
		*volt_id = dev_pm_opp_get_voltage(opp);
		if (!*volt_id) {
			pr_err("invalid OPP for freq %lu\n", freq);
			ret = -EINVAL;
			goto opp_exit;
		}
	}
opp_exit:
	rcu_read_unlock();
	return ret;
}

static int supply_lm_hotplug_state_read(struct input_device_info *device,
				struct supply_lm_input_data *curr_state)
{
	if (!supply_lm_bypass_inp)
		device->curr.num_cores = num_online_cpus();
	curr_state->num_cores = device->curr.num_cores;
	return 0;
}

static int supply_lm_cpu_state_read(struct input_device_info *device,
				struct supply_lm_input_data *curr_state)
{
	curr_state->cpu_state = device->curr.state;
	return 0;
}

static int __ref supply_lm_hotplug_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	uint32_t cpu = (uintptr_t)hcpu;

	if (!supply_lm_data->enabled ||
		supply_lm_bypass_inp)
		return NOTIFY_OK;

	if (supply_lm_data->step2_hotplug_initiated)
		return NOTIFY_OK;

	trace_supply_lm_inp_start_trig(SUPPLY_LM_NUM_CORE_DEVICE, cpu);
	if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
		pr_info("cpu %d event %ld\n", cpu, action);

	if (action == CPU_ONLINE || action == CPU_ONLINE_FROZEN
		|| action == CPU_DEAD || action == CPU_DEAD_FROZEN) {
		read_and_update_mitigation_state(false);
		supply_lm_data->inp_trig_for_mit |=
				BIT(SUPPLY_LM_NUM_CORE_DEVICE);
		if (supply_lm_monitor_task &&
				!supply_lm_data->suspend_in_progress)
			complete(&supply_lm_notify_complete);
	}
	trace_supply_lm_inp_end_trig(SUPPLY_LM_NUM_CORE_DEVICE, cpu);

	return NOTIFY_OK;
}

static struct notifier_block __refdata supply_lm_hotplug_notifier = {
	.notifier_call = supply_lm_hotplug_callback,
};

static int supply_lm_cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	int i = 0;
	enum corner_state new_corner = CORNER_STATE_MAX;
	enum corner_state old_corner = CORNER_STATE_MAX;
	struct cpufreq_freqs *freqs = data;
	struct input_device_info *cpu =
			&supply_lm_devices[SUPPLY_LM_CPU_DEVICE];
	bool mitigate = false;
	static unsigned long local_event;
	static unsigned int new_freq;

	if (!supply_lm_data->enabled ||
			supply_lm_bypass_inp)
		return NOTIFY_OK;

	if ((local_event == event) &&
		(new_freq == freqs->new))
		return NOTIFY_OK;

	trace_supply_lm_inp_start_trig(SUPPLY_LM_CPU_DEVICE, freqs->old);
	local_event = event;
	new_freq = freqs->new;

	for (i = cpufreq_table_len - 1; i >= 0; i--) {

		if (cpufreq_corner_map[i].freq == freqs->new)
			new_corner = cpufreq_corner_map[i].state;
		else if (cpufreq_corner_map[i].freq == freqs->old)
			old_corner = cpufreq_corner_map[i].state;

		if (new_corner != CORNER_STATE_MAX &&
			old_corner != CORNER_STATE_MAX)
			break;
	}

	if (new_corner == CORNER_STATE_MAX ||
		old_corner == CORNER_STATE_MAX)
		goto callback_exit;

	switch (event) {
	case CPUFREQ_PRECHANGE:
		if (new_corner > old_corner) {
			if (cpu->curr.state == new_corner)
				break;
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"PRE:old:%u new:%u old_crnr:%d new_crnr:%d\n",
				freqs->old, freqs->new,
				old_corner, new_corner);
			cpu->curr.state = new_corner;
			if (supply_lm_data->step2_cpu_freq_initiated)
				break;
			mitigate = true;
		}
		break;
	case CPUFREQ_POSTCHANGE:
		if (new_corner < old_corner) {
			if (cpu->curr.state == new_corner)
				break;
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"POST:old:%u new:%u old_crnr:%d new_crnr:%d\n",
				freqs->old, freqs->new,
				old_corner, new_corner);
			cpu->curr.state = new_corner;
			if (supply_lm_data->step2_cpu_freq_initiated)
				break;
			mitigate = true;
		}
		break;
	default:
		pr_err("Unsupported event %ld\n", event);
		break;
	}
	if (mitigate) {
		read_and_update_mitigation_state(false);
		supply_lm_data->inp_trig_for_mit |= BIT(SUPPLY_LM_CPU_DEVICE);
		if (supply_lm_monitor_task &&
				!supply_lm_data->suspend_in_progress)
			complete(&supply_lm_notify_complete);
	}
callback_exit:
	trace_supply_lm_inp_end_trig(SUPPLY_LM_CPU_DEVICE, freqs->new);
	return NOTIFY_OK;
}

static struct notifier_block supply_lm_cpufreq_notifier = {
	.notifier_call = supply_lm_cpufreq_callback,
};

static int create_cpufreq_opp_corner_table(struct device *dev)
{
	int ret = 0, corner = 0;
	uint32_t i = 0;
	struct cpufreq_frequency_table *cpufreq_table;
	struct device *cpu_dev = NULL;

	if (cpufreq_corner_map) {
		pr_info("cpufreq corner map is already created\n");
		goto fail_exit;
	}

	/* using cpu 0 dev since all cpus are in sync */
	cpu_dev = get_cpu_device(0);

	cpufreq_table = cpufreq_frequency_get_table(0);
	if (!cpufreq_table) {
		pr_debug("error reading cpufreq table\n");
		ret = -EINVAL;
		goto fail_exit;
	}
	while (cpufreq_table[i].frequency != CPUFREQ_TABLE_END)
		i++;

	cpufreq_table_len = i - 1;
	if (cpufreq_table_len < 1) {
		WARN(1, "CPU0 frequency table length:%d\n",
					cpufreq_table_len);
		ret = -EINVAL;
		goto fail_exit;
	}

	cpufreq_corner_map = devm_kzalloc(dev,
				sizeof(struct freq_corner_map) *
				cpufreq_table_len,
				GFP_KERNEL);
	if (!cpufreq_corner_map) {
		pr_err("Memory allocation failed\n");
		ret = -ENOMEM;
		goto fail_exit;
	}

	i = 0;
	while (cpufreq_table[i].frequency != CPUFREQ_TABLE_END) {
		cpufreq_corner_map[i].freq = cpufreq_table[i].frequency;
		/* Get corner for freq which is not multiplier of 1000 in HZ */
		if (cpufreq_table[i].frequency == NO_DIV_CPU_FREQ_KHZ) {
			ret = get_voltage_from_opp(cpu_dev,
						NO_DIV_CPU_FREQ_HZ,
						&corner);
			if (ret)
				goto fail_exit;
		} else {
			ret = get_voltage_from_opp(cpu_dev,
					cpufreq_table[i].frequency * 1000,
					&corner);
			if (ret)
				goto fail_exit;
		}
		cpufreq_corner_map[i].state = apps_corner_map[corner];
		i++;
	}

fail_exit:
	if (ret) {
		if (cpufreq_corner_map)
			devm_kfree(dev, cpufreq_corner_map);
		cpufreq_table_len = 0;
	}
	return ret;
}

static int supply_lm_cpu_device_init(struct platform_device *pdev,
				struct input_device_info *cpu)
{
	int ret = 0;

	ret = create_cpufreq_opp_corner_table(&pdev->dev);
	if (ret)
		return ret;

	cpu->curr.state = CORNER_STATE_TURBO;
	ret = cpufreq_register_notifier(&supply_lm_cpufreq_notifier,
			CPUFREQ_TRANSITION_NOTIFIER);
	if (ret) {
		pr_err("cannot register cpufreq notifier. err:%d\n", ret);
		return ret;
	}

	return ret;
}

static int supply_lm_hotplug_device_init(struct platform_device *pdev,
				struct input_device_info *num_core)
{

	if (num_possible_cpus() > 1)
		register_cpu_notifier(&supply_lm_hotplug_notifier);

	return 0;
}

static int supply_lm_gpu_clk_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	int i;
	uint8_t new_volt_id = CORNER_STATE_MAX, old_volt_id = CORNER_STATE_MAX;
	struct msm_clk_notifier_data *clk_data =
				(struct msm_clk_notifier_data *)data;
	struct input_device_info *gpu =
			&supply_lm_devices[SUPPLY_LM_GPU_DEVICE];
	bool mitigate = false;

	trace_supply_lm_inp_start_trig(SUPPLY_LM_GPU_DEVICE,
					clk_data->old_rate);
	if (!supply_lm_data->enabled ||
			supply_lm_bypass_inp)
		goto callback_exit;

	for (i = 0; i < gpufreq_table_len; i++) {
		if (clk_data->new_rate == gpufreq_corner_map[i].freq)
			new_volt_id = gpufreq_corner_map[i].state;
		if (clk_data->old_rate == gpufreq_corner_map[i].freq)
			old_volt_id = gpufreq_corner_map[i].state;

		if (new_volt_id != CORNER_STATE_MAX &&
			old_volt_id != CORNER_STATE_MAX)
			break;
	}

	if (i >= gpufreq_table_len)
		goto callback_exit;

	switch (event) {
	case PRE_RATE_CHANGE:
		if (new_volt_id > old_volt_id) {
			if (gpu->curr.state == new_volt_id)
				break;
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"PRE:old:%lu new:%lu old_vlt:%d new_vlt:%d\n",
				clk_data->old_rate, clk_data->new_rate,
				old_volt_id, new_volt_id);
			gpu->curr.state = new_volt_id;
			mitigate = true;
		}
		break;
	case POST_RATE_CHANGE:
		if (new_volt_id < old_volt_id) {
			if (gpu->curr.state == new_volt_id)
				break;
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"POST:old:%lu new:%lu old_vlt:%d new_vlt:%d\n",
				clk_data->old_rate, clk_data->new_rate,
				old_volt_id, new_volt_id);
			gpu->curr.state = new_volt_id;
			mitigate = true;
		}
		break;
	default:
		break;
	}
	if (mitigate) {
		read_and_update_mitigation_state(false);
		supply_lm_data->inp_trig_for_mit |= BIT(SUPPLY_LM_GPU_DEVICE);
		if (supply_lm_monitor_task &&
				!supply_lm_data->suspend_in_progress)
			complete(&supply_lm_notify_complete);
	}
callback_exit:
	trace_supply_lm_inp_end_trig(SUPPLY_LM_GPU_DEVICE, clk_data->new_rate);
	return NOTIFY_OK;
}

static int supply_lm_gpu_state_read(struct input_device_info *device,
				struct supply_lm_input_data *curr_state)
{
	curr_state->gpu_state =  device->curr.state;
	return 0;
}

static struct notifier_block supply_lm_gpu_notifier = {
	.notifier_call = supply_lm_gpu_clk_callback,
};

static int get_gpufreq_table_from_devicetree(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct device_node *child_node = NULL;
	struct device_node *gpu_pwr = NULL;

	gpu_pwr = of_find_compatible_node(NULL, NULL,
				"qcom,gpu-pwrlevels");
	if (!gpu_pwr) {
		pr_err("Unable to find DT for qcom,gpu-pwrlevelsi\n");
		ret = -EINVAL;
		goto read_fail;
	}

	gpufreq_table_len = of_get_child_count(gpu_pwr);
	if (gpufreq_table_len == 0) {
		pr_err("No gpu power levels nodes\n");
		ret = -ENODEV;
		goto read_fail;
	}

	gpufreq_corner_map = devm_kzalloc(&pdev->dev,
			sizeof(struct freq_corner_map) * gpufreq_table_len,
			GFP_KERNEL);
	if (!gpufreq_corner_map) {
		pr_err("Memory cannot create\n");
		ret = -ENOMEM;
		goto read_fail;
	}

	for_each_child_of_node(gpu_pwr, child_node) {
		if (i >= gpufreq_table_len) {
			pr_err("Invalid number of child node. err:%d\n", i);
			ret = -EINVAL;
			goto read_fail;
		}
		ret = of_property_read_u32(child_node,
			"qcom,gpu-freq", &gpufreq_corner_map[i].freq);
		if (ret) {
			pr_err("No gpu-freq node read error. err:%d\n", ret);
			goto read_fail;
		}
		i++;
	}
read_fail:
	if (ret) {
		if (gpufreq_corner_map)
			devm_kfree(&pdev->dev, gpufreq_corner_map);
	}
	return ret;
}

static int create_gpufreq_opp_corner_table(void)
{
	int ret = 0, i;
	uint32_t gpu_state = RPM_REGULATOR_CORNER_NONE;

	if (!gpufreq_corner_map) {
		pr_err("gpu frequency table is not initialized\n");
		return -EINVAL;
	}

	for (i = 0; i < gpufreq_table_len; i++) {
		if (gpufreq_corner_map[i].freq == SUPPLY_LM_GPU_OFF_RATE) {
			gpufreq_corner_map[i].state = CORNER_STATE_OFF;
			continue;
		}
		ret = get_voltage_from_opp(&supply_lm_data->gpu_pdev->dev,
						gpufreq_corner_map[i].freq,
						&gpu_state);
		if (ret) {
			pr_err("Couldn't get corner for gpu freq:%u.ret:%d\n",
					gpufreq_corner_map[i].freq, ret);
			return -EINVAL;
		}
		gpufreq_corner_map[i].state = apps_corner_map[gpu_state];
	}
	gpufreq_opp_corner_enabled = true;

	return ret;
}

static int supply_lm_gpu_device_init(struct platform_device *pdev,
				struct input_device_info *gpu)
{
	int ret = 0;

	if (!gpufreq_opp_corner_enabled) {
		ret = create_gpufreq_opp_corner_table();
		if (ret) {
			pr_err("can't create gpufreq corner table. err:%d\n",
					ret);
			return ret;
		}
	}

	ret = msm_clk_notif_register(supply_lm_data->gpu_handle,
			&supply_lm_gpu_notifier);
	if (ret) {
		pr_err("cannot register cpufreq notifier. err:%d\n", ret);
		return ret;
	}
	return ret;
}

static int supply_lm_therm_read(struct input_device_info *device,
				struct supply_lm_input_data *curr_state)
{
	curr_state->therm_state = device->curr.therm_state;
	return 0;
}

static void supply_lm_thermal_notify(struct therm_threshold *trig_sens)
{
	int i = 0;
	struct input_device_info *therm_sens =
			&supply_lm_devices[SUPPLY_LM_THERM_DEVICE];
	struct threshold_info *trig_thresh = NULL;
	enum therm_state curr_therm_state = THERM_NORMAL;

	trace_supply_lm_inp_start_trig(SUPPLY_LM_THERM_DEVICE,
					therm_sens->curr.therm_state);
	if (!supply_lm_data->enabled ||
			supply_lm_bypass_inp)
		goto set_and_exit;

	if (!trig_sens || trig_sens->trip_triggered < 0)
		goto set_and_exit;

	trig_thresh = trig_sens->parent;

	mutex_lock(&therm_sens->lock);
	switch (trig_sens->trip_triggered) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		if (trig_thresh ==
			&supply_lm_therm_thresh[THERM_VERY_HOT - 1]) {
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"sensor:%d triggered very hot thresh for\n",
					trig_sens->sensor_id);
			supply_lm_therm_status[THERM_VERY_HOT] |=
				 BIT(trig_sens->sensor_id);
		} else {
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"sensor:%d triggered hot thresh for\n",
				trig_sens->sensor_id);
			supply_lm_therm_status[THERM_HOT] |=
				 BIT(trig_sens->sensor_id);
		}
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		if (trig_thresh ==
			&supply_lm_therm_thresh[THERM_VERY_HOT - 1]) {
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"sensor:%d cleared very hot thresh for\n",
				trig_sens->sensor_id);
			if (supply_lm_therm_status[THERM_VERY_HOT] &
				BIT(trig_sens->sensor_id))
				supply_lm_therm_status[THERM_VERY_HOT] ^=
					BIT(trig_sens->sensor_id);
		} else {
			if (SUPPLY_LM_INPUTS & supply_lm_debug_mask)
				pr_info(
				"sensor:%d cleared hot thresh for\n",
				trig_sens->sensor_id);
			if (supply_lm_therm_status[THERM_HOT] &
				BIT(trig_sens->sensor_id))
				supply_lm_therm_status[THERM_HOT] ^=
					BIT(trig_sens->sensor_id);
		}
		break;
	default:
		pr_err("Unsupported trip type\n");
		mutex_unlock(&therm_sens->lock);
		goto set_and_exit;
		break;
	}

	for (i = MAX_THERM_LEVEL - 1; i >= 0; i--) {
		if (supply_lm_therm_status[i]) {
			curr_therm_state = i;
			break;
		}
	}
	if (i < 0)
		curr_therm_state = THERM_NORMAL;

	pr_debug("current state is %d req %d V%d H%d N%d\n",
			therm_sens->curr.therm_state,
			curr_therm_state,
			supply_lm_therm_status[THERM_VERY_HOT],
			supply_lm_therm_status[THERM_HOT],
			supply_lm_therm_status[THERM_NORMAL]);
	if (therm_sens->curr.therm_state == curr_therm_state) {
		mutex_unlock(&therm_sens->lock);
		goto set_and_exit;
	}

	therm_sens->curr.therm_state = curr_therm_state;
	mutex_unlock(&therm_sens->lock);

	read_and_update_mitigation_state(false);
	supply_lm_data->inp_trig_for_mit |= BIT(SUPPLY_LM_THERM_DEVICE);
	if (supply_lm_monitor_task &&
			!supply_lm_data->suspend_in_progress)
		complete(&supply_lm_notify_complete);
set_and_exit:
	sensor_mgr_set_threshold(trig_sens->sensor_id, trig_sens->threshold);
	trace_supply_lm_inp_end_trig(SUPPLY_LM_THERM_DEVICE,
					therm_sens->curr.therm_state);
	return;
}

static int get_therm_devicetree_data(struct device *dev, char *key,
				int *hi_temp, int *low_temp)
{
	int ret = 0;
	int cnt;

	if (!of_get_property(dev->of_node, key, &cnt)
		|| cnt <= 0) {
		pr_err("Property %s not defined.\n", key);
		ret = -ENODEV;
		goto therm_data_exit;
	}

	if (cnt % (sizeof(__be32) * 2)) {
		pr_err("Invalid number(%d) of entry for %s\n",
			cnt, key);
		ret = -EINVAL;
		goto therm_data_exit;
	}

	ret = of_property_read_u32_index(dev->of_node, key, 0,
					hi_temp);
	if (ret) {
		pr_err("Error reading index%d\n", 0);
		goto therm_data_exit;
	}
	ret = of_property_read_u32_index(dev->of_node, key, 1,
					low_temp);
	if (ret) {
		pr_err("Error reading index%d\n", 1);
		goto therm_data_exit;
	}
therm_data_exit:
	return ret;
}

static int initialize_therm_device_state(
				struct input_device_info *therm_device)
{
	int ret = 0, i;
	long temp = 0;
	struct threshold_info *thresh =
				&supply_lm_therm_thresh[THERM_HOT - 1];
	enum therm_state curr_therm_state = THERM_NORMAL;

	supply_lm_therm_status[THERM_VERY_HOT] = 0;
	supply_lm_therm_status[THERM_HOT] = 0;
	supply_lm_therm_status[THERM_NORMAL] = UINT_MAX;

	mutex_lock(&therm_device->lock);
	for (i = 0; i < thresh->thresh_ct; i++) {
		ret = sensor_get_temp(thresh->thresh_list[i].sensor_id,
					&temp);
		if (ret) {
			pr_err("Unable to read TSENS sensor:%d. err:%d\n",
			thresh->thresh_list[i].sensor_id,
			ret);
			continue;
		}
		if (temp >= supply_lm_data->very_hot_temp_degC) {
			supply_lm_therm_status[THERM_VERY_HOT] |=
					BIT(thresh->thresh_list[i].sensor_id);
			supply_lm_therm_status[THERM_HOT] |=
					BIT(thresh->thresh_list[i].sensor_id);
		} else if (temp >= supply_lm_data->hot_temp_degC) {
			supply_lm_therm_status[THERM_HOT] |=
					BIT(thresh->thresh_list[i].sensor_id);
		}
	}

	for (i = MAX_THERM_LEVEL - 1; i >= 0; i--) {
		if (supply_lm_therm_status[i]) {
			curr_therm_state = i;
			break;
		}
	}
	therm_device->curr.therm_state = curr_therm_state;
	mutex_unlock(&therm_device->lock);
	return 0;
}

static int supply_lm_therm_device_init(struct platform_device *pdev,
				struct input_device_info *therm_device)
{
	int ret = 0;

	struct supply_lm_core_data *supply_lm_data =
					platform_get_drvdata(pdev);

	if (!supply_lm_therm_thresh) {
		supply_lm_therm_thresh = devm_kzalloc(&pdev->dev,
				sizeof(struct threshold_info) *
				(MAX_THERM_LEVEL - 1),
				GFP_KERNEL);
		if (!supply_lm_therm_thresh) {
			pr_err("kzalloc failed\n");
			ret = -ENOMEM;
			goto therm_exit;
		}
	}

	ret = sensor_mgr_init_threshold(&pdev->dev,
				&supply_lm_therm_thresh[THERM_HOT - 1],
				MONITOR_ALL_TSENS,
				supply_lm_data->hot_temp_degC,
				supply_lm_data->hot_temp_hyst,
				supply_lm_thermal_notify);
	if (ret) {
		pr_err(
		"Error in initializing thresholds for index:%d. err:%d\n",
		THERM_HOT, ret);
		goto therm_exit;
	}

	ret = sensor_mgr_init_threshold(&pdev->dev,
				&supply_lm_therm_thresh[THERM_VERY_HOT - 1],
				MONITOR_ALL_TSENS,
				supply_lm_data->very_hot_temp_degC,
				supply_lm_data->very_hot_temp_hyst,
				supply_lm_thermal_notify);
	if (ret) {
		pr_err(
		"Error in initializing thresholds for index:%d. err:%d\n",
		THERM_VERY_HOT, ret);
		goto therm_exit;
	}

	initialize_therm_device_state(therm_device);

	ret = sensor_mgr_convert_id_and_set_threshold(
			&supply_lm_therm_thresh[THERM_HOT - 1]);
	if (ret) {
		pr_err("Error in setting thresholds for index:%d. err:%d\n",
			THERM_HOT, ret);
		goto therm_exit;
	}
	ret = sensor_mgr_convert_id_and_set_threshold(
			&supply_lm_therm_thresh[THERM_VERY_HOT - 1]);
	if (ret) {
		pr_err("Error in setting thresholds for index:%d. err:%d\n",
			THERM_VERY_HOT, ret);
		goto therm_exit;
	}

therm_exit:
	return ret;
}

static int supply_lm_devices_init(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0;

	/* initialize all devices here */
	for (i = 0; i < SUPPLY_LM_DEVICE_MAX; i++) {
		mutex_init(&supply_lm_devices[i].lock);
		ret = supply_lm_devices[i].ops.setup(pdev,
					&supply_lm_devices[i]);
		if (ret)
			goto device_exit;
		list_add_tail(&supply_lm_devices[i].list_ptr,
				&supply_lm_device_list);
		supply_lm_devices[i].enabled = true;
	}
device_exit:
	return ret;
}

static void supply_lm_therm_cleanup(struct platform_device *pdev,
					struct input_device_info *therm)
{
	if (!therm->enabled)
		return;

	if (supply_lm_therm_thresh) {
		sensor_mgr_remove_threshold(&pdev->dev,
			&supply_lm_therm_thresh[THERM_VERY_HOT - 1]);
		sensor_mgr_remove_threshold(&pdev->dev,
			&supply_lm_therm_thresh[THERM_HOT - 1]);
		devm_kfree(&pdev->dev, supply_lm_therm_thresh);
		supply_lm_therm_thresh = NULL;
	}
	therm->enabled = false;
}

static void supply_lm_hotplug_device_exit(struct platform_device *pdev,
					struct input_device_info *core)
{
	if (!core->enabled)
		return;
	unregister_cpu_notifier(&supply_lm_hotplug_notifier);
	core->enabled = false;
}

static void supply_lm_cpu_state_device_exit(struct platform_device *pdev,
					struct input_device_info *cpu)
{
	int ret = 0;

	if (!cpu->enabled)
		return;

	ret = cpufreq_unregister_notifier(&supply_lm_cpufreq_notifier,
				CPUFREQ_TRANSITION_NOTIFIER);
	if (ret) {
		pr_err("cannot unregister cpufreq notifier. err:%d\n", ret);
		return;
	}

	if (cpufreq_corner_map) {
		devm_kfree(&pdev->dev, cpufreq_corner_map);
		cpufreq_corner_map = NULL;
	}

	cpu->enabled = false;
}

static void supply_lm_gpu_state_device_exit(struct platform_device *pdev,
					struct input_device_info *gpu)
{
	int ret = 0;

	if (!gpu->enabled)
		return;
	ret = msm_clk_notif_unregister(supply_lm_data->gpu_handle,
				&supply_lm_gpu_notifier);
	if (ret) {
		pr_err("cannot unregister gpu clk notifier. err:%d\n", ret);
		return;
	}
	gpu->enabled = false;
}


static void supply_lm_modem_state_device_exit(struct platform_device *pdev,
					struct input_device_info *modem)
{
	struct supply_lm_core_data *supply_lm_data = platform_get_drvdata(pdev);

	if (!modem->enabled)
		return;
	destroy_workqueue(supply_lm_data->modem_hw.isr_wq);
	free_irq(supply_lm_data->modem_hw.irq_num, supply_lm_data);
	modem->enabled = false;
}

static int supply_lm_cpu_pm_notify(struct notifier_block *nb,
				unsigned long action,
				void *data)
{
	unsigned int cpu = smp_processor_id();
	unsigned int cpu_idle_cnt = 0x0;
	struct input_device_info *modem =
			&supply_lm_devices[SUPPLY_LM_MODEM_DEVICE];
	enum modem_corner_state modem_state;

	if (!supply_lm_data->enabled ||
			supply_lm_bypass_inp)
		return NOTIFY_OK;

	switch (action) {
	case CPU_PM_ENTER:
		spin_lock(&supply_lm_pm_lock);
		cpumask_set_cpu(cpu, &supply_lm_data->cpu_idle_mask);
		cpu_idle_cnt =
			cpumask_weight(&supply_lm_data->cpu_idle_mask);
		if (cpu_idle_cnt == num_online_cpus())
			disable_irq_nosync(supply_lm_data->modem_hw.irq_num);
		spin_unlock(&supply_lm_pm_lock);
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		spin_lock(&supply_lm_pm_lock);
		cpu_idle_cnt =
			cpumask_weight(&supply_lm_data->cpu_idle_mask);
		if (cpu_idle_cnt == num_online_cpus()) {
			/* Handle missing interrupt here */
			modem_state =
			readl_relaxed(supply_lm_data->modem_hw.intr_base_reg +
					SUPPLY_LM_MODEM_DATA_OFFSET);
			if (modem->curr.state !=
				modem_corner_map[modem_state]) {
				modem->curr.state =
					modem_corner_map[modem_state];
				if (supply_lm_monitor_task &&
					!supply_lm_data->suspend_in_progress)
					complete(&supply_lm_notify_complete);
			}
			enable_irq(supply_lm_data->modem_hw.irq_num);
		}
		cpumask_clear_cpu(cpu, &supply_lm_data->cpu_idle_mask);
		spin_unlock(&supply_lm_pm_lock);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block supply_lm_cpu_pm_notifier = {
	.notifier_call = supply_lm_cpu_pm_notify,
};

static int supply_lm_devm_ioremap(struct platform_device *pdev,
		const char *res_name, void __iomem **virt)
{

	struct resource *supply_lm_res =
		platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);
	if (!supply_lm_res) {
		pr_err("error missing config of %s reg-space\n", res_name);
		return -ENODEV;
	}

	*virt = devm_ioremap(&pdev->dev, supply_lm_res->start,
				resource_size(supply_lm_res));
	if (!*virt) {
		pr_err("error %s ioremap(phy:0x%lx len:0x%lx) failed\n",
			res_name, (ulong) supply_lm_res->start,
			(ulong) resource_size(supply_lm_res));
		return -ENOMEM;
	}
	pr_debug("%s ioremap(phy:0x%lx vir:0x%p len:0x%lx)\n", res_name,
		(ulong) supply_lm_res->start, *virt,
		(ulong) resource_size(supply_lm_res));

	return 0;
}

static int opp_clk_get_from_handle(struct platform_device *pdev,
					const char *phandle,
					struct platform_device **opp_pdev,
					struct clk **opp_clk)
{
	int ret = 0;
	struct device_node *opp_dev_node = NULL;

	if (!pdev || !phandle
	    || (!opp_pdev && !opp_clk)) {
		pr_err("Invalid Input\n");
		ret = -EINVAL;
		goto clk_exit;
	}

	opp_dev_node = of_parse_phandle(pdev->dev.of_node, phandle, 0);
	if (!opp_dev_node) {
		pr_err("Could not find %s device nodes\n", phandle);
		ret = -EINVAL;
		goto clk_exit;
	}

	*opp_pdev = of_find_device_by_node(opp_dev_node);
	if (!*opp_pdev) {
		pr_err("can't find device for node for %s\n", phandle);
		ret = -EINVAL;
		goto clk_exit;
	}

	if (!opp_clk)
		goto clk_exit;

	*opp_clk = devm_clk_get(&(*opp_pdev)->dev, "core_clk");
	if (IS_ERR(*opp_clk)) {
		pr_err("Error getting core clk: %lu\n", PTR_ERR(*opp_clk));
		ret = PTR_ERR(*opp_clk);
		goto clk_exit;
	}
clk_exit:
	if (ret)
		*opp_clk = NULL;
	return ret;
}

static int supply_lm_get_devicetree_data(struct platform_device *pdev)
{
	int ret = 0;

	ret = supply_lm_devm_ioremap(pdev, "intr_reg",
				&supply_lm_data->modem_hw.intr_base_reg);
	if (ret)
		goto dev_exit;

	ret = opp_clk_get_from_handle(pdev, "gpu-dev-opp",
					&supply_lm_data->gpu_pdev,
					&supply_lm_data->gpu_handle);
	if (ret)
		goto dev_exit;

	ret = get_gpufreq_table_from_devicetree(pdev);
	if (ret)
		goto dev_exit;

	ret = get_therm_devicetree_data(&pdev->dev,
				"qcom,supply-lm-hot-temp-range",
				&supply_lm_data->hot_temp_degC,
				&supply_lm_data->hot_temp_hyst);
	if (ret)
		goto dev_exit;

	ret = get_therm_devicetree_data(&pdev->dev,
				"qcom,supply-lm-very-hot-temp-range",
				&supply_lm_data->very_hot_temp_degC,
				&supply_lm_data->very_hot_temp_hyst);
	if (ret)
		goto dev_exit;

dev_exit:
	if (ret)
		pr_err("Error reading. err:%d\n", ret);
	return ret;
}

static int supply_lm_core_remove(struct platform_device *pdev)
{
	struct supply_lm_core_data *supply_lm_data =
					platform_get_drvdata(pdev);

	supply_lm_remove_devices(pdev);
	if (supply_lm_data->modem_hw.intr_base_reg)
		iounmap(supply_lm_data->modem_hw.intr_base_reg);
	/* De-register KTM handle */
	if (supply_lm_data->hotplug_handle)
		devmgr_unregister_mitigation_client(&pdev->dev,
					supply_lm_data->hotplug_handle);
	if (supply_lm_data->cpufreq_handle)
		devmgr_unregister_mitigation_client(&pdev->dev,
					supply_lm_data->cpufreq_handle);
	devm_kfree(&pdev->dev, supply_lm_data);
	supply_lm_data = NULL;

	return 0;
}

static void initialize_supply_lm_data(struct supply_lm_core_data *supply_lm)
{
	if (!supply_lm)
		return;

	supply_lm_data->suspend_in_progress = false;
	supply_lm->step2_cpu_freq_initiated = false;
	supply_lm->step2_hotplug_initiated = false;
	supply_lm->supply_lm_limited_max_freq = UINT_MAX;
	supply_lm->supply_lm_offlined_cpu_mask = CPU_MASK_NONE;
}

static int supply_lm_core_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_clnt_data *handle = NULL;

	if (supply_lm_data) {
		pr_err("Reinitializing supply_lm core driver\n");
		ret = -EEXIST;
		goto probe_exit;
	}

	supply_lm_data = devm_kzalloc(&pdev->dev,
			sizeof(struct supply_lm_core_data),
			GFP_KERNEL);
	if (!supply_lm_data) {
		pr_err("kzalloc failed\n");
		ret = -ENOMEM;
		goto probe_exit;
	}
	supply_lm_data->pdev = pdev;

	initialize_supply_lm_data(supply_lm_data);
	ret = supply_lm_get_devicetree_data(pdev);
	if (ret) {
		pr_err("Error getting device tree data. err:%d\n", ret);
		goto devicetree_exit;
	}
	/* Initialize mitigation KTM interface */
	if (num_possible_cpus() > 1) {
		handle = devmgr_register_mitigation_client(&pdev->dev,
					HOTPLUG_DEVICE,
					devmgr_mitigation_callback);
		if (IS_ERR_OR_NULL(handle)) {
			ret = PTR_ERR(handle);
			pr_err("Error registering for hotplug. ret:%d\n", ret);
			goto ktm_handle_exit;
		}
		supply_lm_data->hotplug_handle = handle;
	}

	handle = devmgr_register_mitigation_client(&pdev->dev,
					CPU0_DEVICE,
					devmgr_mitigation_callback);
	if (IS_ERR_OR_NULL(handle)) {
		ret = PTR_ERR(handle);
		pr_err("Error registering for cpufreq. ret:%d\n", ret);
		goto ktm_handle_exit;
	}
	supply_lm_data->cpufreq_handle = handle;

	platform_set_drvdata(pdev, supply_lm_data);

	ret = supply_lm_devices_init(pdev);
	if (ret) {
		pr_err("Sensor Init failed. err:%d\n", ret);
		goto devices_exit;
	}

	init_completion(&supply_lm_notify_complete);
	init_completion(&supply_lm_mitigation_complete);
	supply_lm_monitor_task =
	kthread_run(supply_lm_monitor, NULL, "supply-lm:monitor");
	if (IS_ERR(supply_lm_monitor_task)) {
		pr_err("Failed to create SUPPLY LM monitor thread. err:%ld\n",
				PTR_ERR(supply_lm_monitor_task));
		ret = PTR_ERR(supply_lm_monitor_task);
		goto devices_exit;
	}

	spin_lock_init(&supply_lm_pm_lock);
	cpu_pm_register_notifier(&supply_lm_cpu_pm_notifier);

	ret = create_supply_lm_debugfs(pdev);
	if (ret) {
		pr_err("Creating debug_fs failed\n");
		goto kthread_exit;
	}

	supply_lm_debug = dma_alloc_coherent(&pdev->dev,
			num_dbg_elements * sizeof(struct supply_lm_debug),
			&supply_lm_debug_phys, GFP_KERNEL);
	if (!supply_lm_debug) {
		pr_err("Debug counter init is failed\n");
		ret = -EINVAL;
		goto debugfs_exit;
	}
	ret = create_supply_lm_sysfs();
	if (ret)
		goto debugfs_exit;

	/* Disable driver by deafult */
	supply_lm_data->enabled = false;
	supply_lm_remove_devices(pdev);

	/* Read and update inital mitigation state */
	if (supply_lm_monitor_task)
		complete(&supply_lm_notify_complete);
	return ret;

debugfs_exit:
	if (supply_lm_debugfs) {
		debugfs_remove_recursive(supply_lm_debugfs->parent);
		devm_kfree(&pdev->dev, supply_lm_debugfs);
	}
kthread_exit:
	if (supply_lm_monitor_task)
		kthread_stop(supply_lm_monitor_task);
	cpu_pm_unregister_notifier(&supply_lm_cpu_pm_notifier);

devices_exit:
	supply_lm_remove_devices(pdev);

ktm_handle_exit:
	if (supply_lm_data->hotplug_handle)
		devmgr_unregister_mitigation_client(&pdev->dev,
					supply_lm_data->hotplug_handle);
	if (supply_lm_data->cpufreq_handle)
		devmgr_unregister_mitigation_client(&pdev->dev,
					supply_lm_data->cpufreq_handle);

devicetree_exit:
	devm_kfree(&pdev->dev, supply_lm_data);
	supply_lm_data = NULL;

probe_exit:
	return ret;
}

static struct input_device_info supply_lm_devices[] = {
	[SUPPLY_LM_THERM_DEVICE] = {
		.device_name = "therm_state",
		.ops = {
			.setup    = supply_lm_therm_device_init,
			.read_state = supply_lm_therm_read,
			.clean_up = supply_lm_therm_cleanup,
		},
	},
	[SUPPLY_LM_NUM_CORE_DEVICE] = {
		.device_name = "core_num",
		.ops = {
			.setup     = supply_lm_hotplug_device_init,
			.read_state = supply_lm_hotplug_state_read,
			.clean_up = supply_lm_hotplug_device_exit,
		},
	},
	[SUPPLY_LM_CPU_DEVICE] = {
		.device_name = "cpu_state",
		.ops = {
			.setup     = supply_lm_cpu_device_init,
			.read_state = supply_lm_cpu_state_read,
			.clean_up = supply_lm_cpu_state_device_exit,
		},
	},
	[SUPPLY_LM_MODEM_DEVICE] = {
		.device_name = "modem_state",
		.ops = {
			.setup     = supply_lm_modem_device_init,
			.read_state = supply_lm_modem_state_read,
			.clean_up = supply_lm_modem_state_device_exit,
		},
	},
	[SUPPLY_LM_GPU_DEVICE] = {
		.device_name = "gpu_state",
		.ops = {
			.setup     = supply_lm_gpu_device_init,
			.read_state = supply_lm_gpu_state_read,
			.clean_up = supply_lm_gpu_state_device_exit,
		},
	},
};

static int supply_lm_suspend_noirq(struct device *dev)
{
	pr_debug("Suspend in process, disabling step2 mitigation\n");
	supply_lm_data->suspend_in_progress = true;
	return 0;
}

static int supply_lm_resume_noirq(struct device *dev)
{
	pr_debug("Resuming from suspend, enabling step2 mitigation\n");
	supply_lm_data->suspend_in_progress = false;
	return 0;
}

static const struct dev_pm_ops supply_lm_pm_ops = {
	.suspend_noirq	= supply_lm_suspend_noirq,
	.resume_noirq   = supply_lm_resume_noirq,
};

static struct of_device_id supply_lm_match[] = {
	{
		.compatible = "qcom,supply-lm",
	},
	{},
};

static struct platform_driver supply_lm_driver = {
	.probe  = supply_lm_core_probe,
	.remove = supply_lm_core_remove,
	.driver = {
		.name           = SUPPLY_LM_DRIVER_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = supply_lm_match,
		.pm             = &supply_lm_pm_ops,
	},
};

int __init supply_lm_driver_init(void)
{
	return platform_driver_register(&supply_lm_driver);
}

static void __exit supply_lm_driver_exit(void)
{
	platform_driver_unregister(&supply_lm_driver);
}

late_initcall(supply_lm_driver_init);
module_exit(supply_lm_driver_exit);

MODULE_DESCRIPTION("SUPPLY LM CORE");
MODULE_ALIAS("platform:" SUPPLY_LM_DRIVER_NAME);
MODULE_LICENSE("GPL v2");
