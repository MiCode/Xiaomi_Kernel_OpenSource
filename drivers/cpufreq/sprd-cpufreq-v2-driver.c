// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Unisoc, Inc.
#define pr_fmt(fmt) "sprd-apcpu-dvfs: " fmt

#include "sprd-cpufreq-v2.h"
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/pm_qos.h>
#include <linux/thermal.h>

#define low_check(temp)		((int)(temp) < (int)DVFS_TEMP_LOW_LIMIT)
#define upper_check(temp)	((int)(temp) >= (int)DVFS_TEMP_UPPER_LIMIT)
#define temp_check(temp)	(low_check(temp) || upper_check(temp))

#define ON_BOOST		0
#define OUT_BOOST		1
#define SPRD_CPUFREQ_BOOST_DURATION	(60ul * HZ)
#define SPRD_DVFS_DEBUG_MAGIC		(0x5A)

struct cluster_prop {
	char *name;
	u32 *value;
	void **ops;
};

static struct device *dev;
static struct cluster_info *pclusters;
static unsigned long boot_done_timestamp;

/* cluster common interface */
static int sprd_cluster_info(u32 cpu_idx)
{
	struct device_node *cpu_np;
	struct of_phandle_args args;
	int ret, index;

	if (cpu_idx >= nr_cpu_ids)
		return -EINVAL;

	cpu_np = of_cpu_device_node_get(cpu_idx);
	if (!cpu_np)
		return -EINVAL;

	ret = of_parse_phandle_with_args(cpu_np, "sprd,freq-domain",
					 "#freq-domain-cells", 0, &args);
	of_node_put(cpu_np);
	if (ret)
		return -EINVAL;

	index = args.args[0];

	return index;
}

int sprd_cluster_num(void)
{
	unsigned int cpu_max = num_possible_cpus() - 1;

	return sprd_cluster_info(cpu_max) + 1;
}

static int sprd_cpufreq_boost_judge(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster = policy->driver_data;

	if (time_after(jiffies, boot_done_timestamp)) {
		cluster->boost_enable = false;
		pr_info("policy[%d] disables boost it is %lu seconds after boot up\n",
			 policy->cpu, SPRD_CPUFREQ_BOOST_DURATION / HZ);
		return OUT_BOOST;
	}

	if (policy->max < policy->cpuinfo.max_freq) {
		cluster->boost_enable = false;
		pr_info("policy[%d] disables boost due to policy max(%d<%d)\n",
			policy->cpu, policy->max, policy->cpuinfo.max_freq);
		return OUT_BOOST;
	}

	return ON_BOOST;
}

static int sprd_temp_list_init(struct list_head *head)
{
	struct temp_node *node;

	if (!list_empty_careful(head)) {
		pr_warn("temp list is also init\n");
		return 0;
	}

	/* add upper temp node */
	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->temp = DVFS_TEMP_UPPER_LIMIT;

	list_add(&node->list, head);

	/* add low temp node */
	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->temp = DVFS_TEMP_LOW_LIMIT;

	list_add(&node->list, head);

	return 0;
}

static int sprd_temp_list_add(struct list_head *head, int temp)
{
	struct temp_node *node, *pos;

	if (temp_check(temp) || list_empty_careful(head)) {
		pr_err("temp %d or list is error\n", temp);
		return -EINVAL;
	}

	list_for_each_entry(pos, head, list) {
		if (temp == pos->temp)
			return 0;
		if (temp < pos->temp)
			break;
	}

	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->temp = temp;

	list_add_tail(&node->list, &pos->list);

	return 0;
}

static struct temp_node *sprd_temp_list_find(struct list_head *head, int temp)
{
	struct temp_node *pos, *next;

	if (temp_check(temp)) {
		pr_err("temp %d is out of range\n", temp);
		return NULL;
	}

	list_for_each_entry(pos, head, list) {
		next = list_entry(pos->list.next, typeof(*pos), list);
		if ((temp >= pos->temp) && (temp < next->temp))
			break;
	}

	return pos;
}

static void sprd_cpufreq_get_debug_flag(u32 *value)
{
	struct device_node *cmdline_node;
	const char *cmd_line, *dvfs_set;
	int ret;

	if (!value) {
		pr_err("para value is error\n");
		return;
	}

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	if (ret) {
		pr_err("Fail to find cmdline bootargs property\n");
		return;
	}

	dvfs_set = strstr(cmd_line, "sprdboot.dvfs_set=0x");
	if (!dvfs_set) {
		pr_info("no property sprdboot.dvfs_set found\n");
		return;
	}

	ret = sscanf(dvfs_set, "sprdboot.dvfs_set=0x%x,%x,%x", &value[0], &value[1], &value[2]);
	if (ret != 3)
		pr_err("property dvfs_set para is error\n");
}

static void sprd_cluster_get_supply_mode(char *dcdc_supply)
{
	struct device_node *cmdline_node;
	const char *cmd_line, *dcdc_type, *ver_str = "-v2";
	int value = -1, ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	if (ret) {
		pr_err("Fail to find cmdline bootargs property\n");
		return;
	}

	dcdc_type = strstr(cmd_line, "power.from.extern=");
	if (!dcdc_type) {
		pr_info("no property power.from.extern found\n");
		return;
	}

	sscanf(dcdc_type, "power.from.extern=%d", &value);
	if (value)
		return;

	strcat(dcdc_supply, ver_str);
}

static int sprd_policy_table_update(struct cpufreq_policy *policy, struct temp_node *node)
{
	struct cpufreq_frequency_table *new_table __maybe_unused;
	struct cluster_info *cluster;
	struct device *cpu;
	u32 table_entry_num = 0;
	u64 freq, vol;
	int i, ret;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster || !cluster->table_update || !cluster->pair_get) {
		pr_err("get cpu %u cluster info error\n", policy->cpu);
		return -EINVAL;
	}

	cpu = get_cpu_device(cluster->cpu);
	if (!cpu) {
		pr_err("get cpu %u dev error\n", policy->cpu);
		return -EINVAL;
	}

	pr_info("update cluster %u temp %d dvfs table\n", cluster->id, node->temp);

	ret = cluster->table_update(cluster->id, node->temp, &table_entry_num);
	if (ret) {
		pr_err("update cluster %u temp %d table error\n", cluster->id, node->temp);
		return ret;
	}

	cluster->table_entry_num = table_entry_num;

	pr_debug("cluster %u dvfs table entry num is %u\n", cluster->id, cluster->table_entry_num);

	for (i = 0; i < cluster->table_entry_num; ++i) {
		ret = cluster->pair_get(cluster->id, i, &freq, &vol);
		if (ret) {
			pr_err("get cluster %u index %u pair error\n", cluster->id, i);
			return ret;
		}

		pr_debug("add %lluHz/%lluuV to opp\n", freq, vol);

		if (policy->freq_table)
			dev_pm_opp_remove(cpu, freq);

		ret = dev_pm_opp_add(cpu, freq, vol);
		if (ret) {
			pr_err("add %lluHz/%lluuV pair to opp error(%d)\n", freq, vol, ret);
			return ret;
		}
	}

	if (node->temp_table)
		policy->freq_table = node->temp_table;
	else {
		ret = dev_pm_opp_init_cpufreq_table(cpu, &new_table);
		if (ret) {
			pr_err("init cluster %u freq table error(%d)\n", cluster->id, ret);
			return ret;
		}

		policy->freq_table = new_table;
		node->temp_table = new_table;
	}

	policy->suspend_freq = policy->freq_table[DVFS_SUSPEND_INDEX].frequency;

	return 0;
}

static void sprd_cpufreq_temp_work_func(struct work_struct *work)
{
	int temp = 0, ret;
	unsigned int freq;
	struct delayed_work *dwork = to_delayed_work(work);
	struct cluster_info *cluster =
		container_of(dwork, struct cluster_info, temp_work);

	if (!cluster) {
		pr_err("get temp work cluster info error\n");
		return;
	}

	if (IS_ERR_OR_NULL(cluster->cpu_tz)) {
		cluster->cpu_tz = thermal_zone_get_zone_by_name(cluster->tz_name);
		if (IS_ERR_OR_NULL(cluster->cpu_tz)) {
			pr_warn("failed to get cluster %u thmzone\n", cluster->id);
			queue_delayed_work(system_highpri_wq, &cluster->temp_work,
					   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
			return;
		}

		pr_info("get cluster %u thmzone successfully\n", cluster->id);
	}

	ret = thermal_zone_get_temp(cluster->cpu_tz, &temp);
	if (ret) {
		pr_warn("failed to get the cluster %u temp of cpu thmzone\n", cluster->id);
		queue_delayed_work(system_highpri_wq, &cluster->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
		return;
	}

	freq = sprd_cpufreq_update_opp(cluster, temp);
	if (freq)
		pr_info("cluster[%u] update max freq[%u] by temp[%d]\n",
			cluster->id, freq, temp);

	queue_delayed_work(system_highpri_wq, &cluster->temp_work,
			   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
}

/* sprd_cpufreq_driver interface */
static int sprd_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;
	u64 freq = 0;
	int ret;

	cluster = pclusters + sprd_cluster_info(policy->cpu);
	if (!cluster || !cluster->freq_get || !cluster->dvfs_enable) {
		pr_err("get cpu %u cluster info error\n", policy->cpu);
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	/* CPUs in the same cluster share same clock and power domain */
	ret = of_perf_domain_get_sharing_cpumask(policy->cpu, "sprd,freq-domain",
						 "#freq-domain-cells",
						 policy->cpus);
	if (ret < 0) {
		pr_err("cpufreq cluster cpumask error");
		goto unlock_ret;
	}
	cpumask_copy(&cluster->cpus, policy->cpus);

	/* init other cpu policy link in same cluster */
	policy->dvfs_possible_from_any_cpu = true;

	policy->transition_delay_us = cluster->transition_delay;
	policy->driver_data = cluster;
	cluster->policy_data = policy;

	/* init dvfs table use current temp */
	ret = sprd_policy_table_update(policy, cluster->temp_currt_node);
	if (ret) {
		pr_err("update cluster %u table error(%d)\n", cluster->id, ret);
		goto unlock_ret;
	}

	/* get current freq for policy */
	ret = cluster->freq_get(cluster->id, &freq);
	if (ret) {
		pr_err("get cluster %u current freq error\n", cluster->id);
		goto unlock_ret;
	}

	do_div(freq, 1000); /* from Hz to KHz */
	policy->cur = (u32)freq;

	/* enable dvfs phy */
	ret = cluster->dvfs_enable(cluster->id);
	if (ret) {
		pr_err("enable cluster %u dvfs error\n", cluster->id);
		goto unlock_ret;
	}

	/* init debugfs interface to debug dvfs */
	ret = sprd_debug_cluster_init(policy);
	if (ret) {
		pr_err("init cluster %u debug error(%d)\n", cluster->id, ret);
		goto unlock_ret;
	}

	if (cluster->temp_enable) {
		ret = freq_qos_add_request(&policy->constraints,
					   &cluster->max_req, FREQ_QOS_MAX,
					   FREQ_QOS_MAX_DEFAULT_VALUE);
		if (ret < 0) {
			pr_err("add cluster %u freq qos error(%d)\n", cluster->id, ret);
			goto unlock_ret;
		}

		queue_delayed_work(system_highpri_wq, &cluster->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
	}

	mutex_unlock(&cluster->mutex);

	return 0;

unlock_ret:
	mutex_unlock(&cluster->mutex);

	return ret;
}

static int sprd_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;
	struct device *cpu;
	int ret;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		pr_err("policy is not init\n");
		return -EINVAL;
	}

	cpu = get_cpu_device(policy->cpu);
	if (!cpu) {
		pr_err("get cpu %u device error\n", policy->cpu);
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	if (cluster->temp_enable) {
		cancel_delayed_work_sync(&cluster->temp_work);
		freq_qos_remove_request(&cluster->max_req);
	}

	dev_pm_opp_free_cpufreq_table(cpu, &policy->freq_table);
	dev_pm_opp_of_remove_table(cpu);

	ret = sprd_debug_cluster_exit(policy);
	if (ret)
		pr_warn("cluster %u debug exit error\n", cluster->id);

	policy->driver_data = NULL;

	mutex_unlock(&cluster->mutex);

	return 0;
}

static int sprd_cpufreq_table_verify(struct cpufreq_policy_data *policy_data)
{
	return cpufreq_generic_frequency_table_verify(policy_data);
}

static int sprd_cpufreq_set_target_index(struct cpufreq_policy *policy, u32 index)
{
	struct cluster_info *cluster;
	int ret;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster || !cluster->freq_set) {
		pr_err("policy is not init\n");
		return -EINVAL;
	}

	if (unlikely(cluster->boost_enable)) {
		ret = sprd_cpufreq_boost_judge(policy);
		if (ret == ON_BOOST)
			return 0;
	}

	mutex_lock(&cluster->mutex);

	if (unlikely(cluster->table_entry_num && index >= cluster->table_entry_num)) {
		pr_info("cluster %u index %u has been reduced\n", cluster->id, index);
		index = cluster->table_entry_num - 1;
	}

	ret = cluster->freq_set(cluster->id, index);
	if (ret)
		pr_err("set cluster %u index %u error(%d)\n", cluster->id, index, ret);

	mutex_unlock(&cluster->mutex);

	return ret;
}

static u32 sprd_cpufreq_get(u32 cpu)
{
	struct cluster_info *cluster;
	u64 freq;
	int ret;

	cluster = pclusters + sprd_cluster_info(cpu);
	if (!cluster || !cluster->freq_get) {
		pr_err("get cpu %u cluster info error\n", cpu);
		return 0;
	}

	mutex_lock(&cluster->mutex);

	ret = cluster->freq_get(cluster->id, &freq);
	if (ret) {
		pr_err("get cluster %u current freq error\n", cluster->id);
		mutex_unlock(&cluster->mutex);
		return 0;
	}

	mutex_unlock(&cluster->mutex);

	do_div(freq, 1000); /* from Hz to KHz */

	return (u32)freq;
}

static int sprd_cpufreq_suspend(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster = policy->driver_data;

	if (cluster->temp_enable)
		cancel_delayed_work_sync(&cluster->temp_work);

	if (!strcmp(policy->governor->name, "userspace")) {
		pr_info("do nothing for governor-%s\n", policy->governor->name);
		return 0;
	}

	if (unlikely(cluster->boost_enable)) {
		cluster->boost_enable = false;
		sprd_cpufreq_set_target_index(policy, DVFS_SUSPEND_INDEX);
	}

	return cpufreq_generic_suspend(policy);
}

static int sprd_cpufreq_resume(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster = policy->driver_data;

	if (cluster->temp_enable)
		queue_delayed_work(system_highpri_wq, &cluster->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));

	if (!strcmp(policy->governor->name, "userspace")) {
		pr_info("do nothing for governor-%s\n", policy->governor->name);
		return 0;
	}

	return cpufreq_generic_suspend(policy);
}

static int sprd_cpufreq_online(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		pr_err("policy is not init\n");
		return -EINVAL;
	}

	if (cluster->temp_enable)
		queue_delayed_work(system_highpri_wq, &cluster->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));

	return 0;
}

static int sprd_cpufreq_offline(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		pr_err("policy is not init\n");
		return -EINVAL;
	}

	if (cluster->temp_enable)
		cancel_delayed_work_sync(&cluster->temp_work);

	return 0;
}

static struct cpufreq_driver sprd_cpufreq_driver = {
	.name = "sprd-cpufreq-v2",
	.flags = CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		 CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
		 CPUFREQ_IS_COOLING_DEV,
	.init = sprd_cpufreq_init,
	.exit = sprd_cpufreq_exit,
	.verify = sprd_cpufreq_table_verify,
	.target_index = sprd_cpufreq_set_target_index,
	.register_em = cpufreq_register_em_with_opp,
	.get = sprd_cpufreq_get,
	.suspend = sprd_cpufreq_suspend,
	.resume = sprd_cpufreq_resume,
	.online = sprd_cpufreq_online,
	.offline = sprd_cpufreq_offline,
	.attr = cpufreq_generic_attr,
};

/* init inerface */
static int sprd_cluster_temp_init(struct cluster_info *cluster)
{
	const char *name = "sprd,temp-threshold";
	struct temp_node *node;
	struct property *prop;
	u32 num, val;
	int i, ret;

	INIT_LIST_HEAD(&cluster->temp_list_head);

	ret = sprd_temp_list_init(&cluster->temp_list_head);
	if (ret) {
		pr_err("init cluster %u temp limit error(%d)\n", cluster->id, ret);
		return ret;
	}

	prop = of_find_property(cluster->node, name, &num);
	if (!prop || !num) {
		pr_warn("find cluster %u temp property error\n", cluster->id);
		cluster->temp_enable = false;
		goto temp_node_init;
	}

	cluster->temp_enable = true;
	INIT_DELAYED_WORK(&cluster->temp_work, sprd_cpufreq_temp_work_func);

	if (of_property_read_string(cluster->node, "sprd,thmzone-names", &cluster->tz_name)) {
		pr_err("get cluster %u thmzone name error\n", cluster->id);
		return -EINVAL;
	}

	for (i = 0; i < num / sizeof(u32); i++) {
		ret = of_property_read_u32_index(cluster->node, name, i, &val);
		if (ret) {
			pr_err("get cluster %u temp error\n", cluster->id);
			return ret;
		}

		ret = sprd_temp_list_add(&cluster->temp_list_head, (int)val);
		if (ret) {
			pr_err("add cluster %u temp error(%d)\n", cluster->id, ret);
			return ret;
		}
	}

temp_node_init:
	node = sprd_temp_list_find(&cluster->temp_list_head, DVFS_TEMP_DEFAULT);
	if (!node) {
		pr_err("init cluster %u temp(%d) node error\n", cluster->id, DVFS_TEMP_DEFAULT);
		return -EINVAL;
	}

	cluster->temp_level_node = node;
	cluster->temp_currt_node = node;
	cluster->best_temp = node->temp;
	cluster->temp_tick = 0U;

	return 0;
}


static u32 sprd_cluster_get_dcdc_type(struct cluster_info *cluster, u32 dcdc_id)
{
	const char *name = "sprd,dcdc-id";
	struct property *prop;
	u32 num, val, id, pmic_type;
	int i, ret;

	prop = of_find_property(cluster->node, name, &num);
	if (!prop || !num) {
		pr_warn("cluster %u dcdc_type property is not set\n", cluster->id);
		return 0;
	}

	for (i = 0; i < (num / sizeof(u32)) / 2; i++) {
		ret = of_property_read_u32_index(cluster->node, name, 2 * i, &val);
		if (ret) {
			pr_err("get cluster %u dcdc_type error\n", cluster->id);
			return 0;
		}
		pmic_type = val;
		ret = of_property_read_u32_index(cluster->node, name, 2 * i + 1, &val);
		if (ret) {
			pr_err("get cluster %u dcdc_type error\n", cluster->id);
			return 0;
		}
		id = val;

		if (dcdc_id == id) {
			pr_info("cluster[%u] dc.id=%d pmic.type=%d\n", cluster->id, id, pmic_type);
			return pmic_type;
		}
	}
	return 0;
}

static u32 sprd_cluster_get_dcdc_id(struct cluster_info *cluster)
{
	struct device_node *cmdline_node;
	const char *cmd_line, *dcdc_type;
	u32 value = 0;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	if (ret) {
		pr_err("Fail to find cmdline bootargs property\n");
		return 0;
	}

	dcdc_type = strstr(cmd_line, "dcdc.id=");
	if (!dcdc_type) {
		pr_info("no property dcdc.id found\n");
		return 0;
	}

	sscanf(dcdc_type, "dcdc.id=%d", &value);
	if (value) {
		pr_info("get_dcdc_id: value= %d\n", value);
		return value;
	}

	pr_info("get_dcdc_id: failed %d\n", value);
	return 0;
}

static int sprd_cluster_props_init(struct cluster_info *cluster)
{
	int (*ops)(u32 id, u32 val);
	struct cluster_prop *p;
	int i, ret;
	u32 dcdc_id, pmic_type = 0;
	char dcdc_supply[32] = "sprd,pmic-type";
	struct cluster_prop props[] = {
		{
			.name = "sprd,voltage-step",
			.value = &cluster->voltage_step,
			.ops = (void **)&cluster->step_set,
		}, {
			.name = "sprd,voltage-margin",
			.value = &cluster->voltage_margin,
			.ops = (void **)&cluster->margin_set,
		}, {
			.name = "sprd,transition-delay",
			.value = &cluster->transition_delay,
			.ops = NULL,
		}
	};

	for (i = 0; i < ARRAY_SIZE(props); ++i) {
		p = props + i;
		ops = p->ops ? *p->ops : NULL;

		ret = of_property_read_u32(cluster->node, p->name, p->value);
		if (ret) {
			pr_warn("get cluster %u '%s' value error\n", cluster->id, p->name);
			*p->value = 0U;
			continue;
		}

		ret = ops ? ops(cluster->id, *p->value) : 0;
		if (ret) {
			pr_err("set cluster %u '%s' value error\n", cluster->id, p->name);
			return -EINVAL;
		}
	}

	if (of_property_read_bool(cluster->node, "sprd,multi-supply"))
		sprd_cluster_get_supply_mode(dcdc_supply);

	pr_info("cluster %u dcdc supply[%s]\n", cluster->id, dcdc_supply);

	ret = of_property_read_u32(cluster->node, dcdc_supply, &cluster->pmic_type);

	dcdc_id = sprd_cluster_get_dcdc_id(cluster);
	if (dcdc_id)
		pmic_type = sprd_cluster_get_dcdc_type(cluster, dcdc_id);

	if (pmic_type) {
		cluster->pmic_type = pmic_type;
		pr_info("cluster %u dcdc.id[%u] pmic_type[%u]\n",
			cluster->id, dcdc_id, cluster->pmic_type);
	} else {
		pr_info("cluster %u dcdc.id[%u] pmic_type is not set, pmic_type use def[%u]\n",
			cluster->id, dcdc_id, cluster->pmic_type);
	}

	if (!ret) {
		ret = cluster->pmic_set(cluster->id, cluster->pmic_type);
		if (ret) {
			pr_err("set cluster %u 'pmic-type' value error\n", cluster->id);
			return -EINVAL;
		}
	}

	if (cluster->id == 0) {
		ret = cluster->dvfs_debug_init();
		if (ret) {
			pr_err("dvfs debug init error\n");
			return ret;
		}
	}

	if (of_property_read_bool(cluster->node, "sprd,cpufreq-boost"))
		cluster->boost_enable = true;

	return 0;
}

static int sprd_cluster_ops_init(struct cluster_info *cluster)
{
	struct sprd_sip_svc_dvfs_ops *ops;
	struct sprd_sip_svc_handle *sip;

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		pr_err("get sip error\n");
		return -EINVAL;
	}

	ops = &sip->dvfs_ops;

	cluster->dvfs_enable = ops->dvfs_enable;
	cluster->table_update = ops->table_update;
	cluster->step_set = ops->step_set;
	cluster->margin_set = ops->margin_set;
	cluster->freq_set = ops->freq_set;
	cluster->freq_get = ops->freq_get;
	cluster->pair_get = ops->pair_get;
	cluster->pmic_set = ops->pmic_set;
	cluster->dvfs_init = ops->dvfs_init;
	cluster->dvfs_debug_init = ops->dvfs_debug_init;

	return 0;
}

static struct device_node *sprd_cluster_node_init(u32 cpu_idx)
{
	struct device *cpu_dev;

	cpu_dev = get_cpu_device(cpu_idx);
	if (!cpu_dev)
		return NULL;

	return of_parse_phandle(cpu_dev->of_node, "sprd,freq-domain", 0);
}

static int sprd_cluster_info_init(struct cluster_info *clusters)
{
	struct cluster_info *cluster;
	unsigned int cpu;
	int ret;

	for_each_possible_cpu(cpu) {
		cluster = clusters + sprd_cluster_info(cpu);
		if (cluster->node) {
			pr_debug("cluster %u info is also init\n", cluster->id);
			continue;
		}

		cluster->node = sprd_cluster_node_init(cpu);
		if (!cluster->node) {
			pr_err("init cluster %u node error\n", cluster->id);
			return -EINVAL;
		}

		cluster->id = (u32)sprd_cluster_info(cpu);
		cluster->cpu = cpu;

		mutex_init(&cluster->mutex);

		ret = sprd_cluster_ops_init(cluster);
		if (ret) {
			pr_err("init cluster %u ops error(%d)\n", cluster->id, ret);
			return ret;
		}

		ret = sprd_cluster_props_init(cluster);
		if (ret) {
			pr_err("init cluster %u props error(%d)\n", cluster->id, ret);
			return ret;
		}

		ret = sprd_cluster_temp_init(cluster);
		if (ret) {
			pr_err("init cluster %u temp error(%d)\n", cluster->id, ret);
			return ret;
		}
	}

	return 0;
}

static int sprd_cpufreq_driver_probe(struct platform_device *pdev)
{
	int ret;
	u32 flag[3] = {0};

	sprd_cpufreq_get_debug_flag(flag);
	if ((flag[0] & 0xFF) == SPRD_DVFS_DEBUG_MAGIC) {
		pr_info("disable apcpu dvfs for debug!\n");
		return 0;
	}

	boot_done_timestamp = jiffies + SPRD_CPUFREQ_BOOST_DURATION;

	dev = &pdev->dev;

	pr_debug("probe sprd cpufreq v2 driver\n");

	pclusters = devm_kzalloc(dev, sizeof(*pclusters) * sprd_cluster_num(), GFP_KERNEL);
	if (!pclusters)
		return -ENOMEM;

	ret = sprd_cluster_info_init(pclusters);
	if (ret) {
		pr_err("init cluster info error(%d)\n", ret);
		return ret;
	}

	ret = pclusters->dvfs_init(flag[0], flag[1], flag[2]);
	if (ret) {
		pr_err("init dvfs device error\n");
		return ret;
	}

	ret = sprd_debug_init(dev);
	if (ret) {
		pr_err("init dvfs debug error(%d)\n", ret);
		return ret;
	}

	ret = cpufreq_register_driver(&sprd_cpufreq_driver);
	if (ret)
		pr_err("register cpufreq driver error\n");
	else
		pr_info("register cpufreq driver success\n");

	return ret;
}

static int sprd_cpufreq_driver_remove(struct platform_device *pdev)
{
	return cpufreq_unregister_driver(&sprd_cpufreq_driver);
}

/**
 * sprd_cpufreq_update_opp() - returns the max freq of a cluster and update dvfs
 * table by temp_now
 *
 * @cluster: which cluster you want to update dvfs table
 * @now_temp: current temperature on this cluster, mini-degree.
 *
 * Return:
 * 1.cluster is not working, then return 0
 * 2.succeed to update dvfs table then return max freq(KHZ) of this cluster
 */
unsigned int sprd_cpufreq_update_opp(struct cluster_info *cluster, int now_temp)
{
	struct cpufreq_policy *policy;
	int temp = now_temp / 1000;
	struct temp_node *node;
	u64 freq;
	int ret;

	policy = cluster->policy_data;
	if (!policy) {
		pr_err("get cluster %u policy error\n", cluster->id);
		return 0;
	}

	mutex_lock(&cluster->mutex);

	node = sprd_temp_list_find(&cluster->temp_list_head, temp);

	if (!node) {
		cluster->temp_level_node = cluster->temp_currt_node;
		cluster->temp_tick = 0U;
		goto ret_error;
	}

	/* immediate response to away from the best_temp */
	if (abs(node->temp - cluster->best_temp)
	    - abs(cluster->temp_currt_node->temp - cluster->best_temp) > 0) {
		cluster->temp_level_node = node;
		cluster->temp_tick = DVFS_TEMP_MAX_TICKS;
	} else if (node != cluster->temp_level_node) {
		cluster->temp_level_node = node;
		cluster->temp_tick = 0U;
		goto ret_error;
	}

	++cluster->temp_tick;
	if (cluster->temp_tick < DVFS_TEMP_MAX_TICKS)
		goto ret_error;

	cluster->temp_tick = 0U;

	if (cluster->temp_level_node == cluster->temp_currt_node)
		goto ret_error;

	pr_debug("update cluster %u table to %d(%d)\n", cluster->id, temp,
		 cluster->temp_level_node->temp);

	/* delay is required to ensure that the last process is completed */
	udelay(100);

	ret = sprd_policy_table_update(policy, cluster->temp_level_node);
	if (ret) {
		pr_err("update cluster %u table error(%d)\n", cluster->id, ret);
		goto ret_error;
	}

	cluster->temp_currt_node = cluster->temp_level_node;

	ret = cluster->pair_get(cluster->id, cluster->table_entry_num - 1, &freq, NULL);
	if (ret) {
		pr_err("get cluster %u max freq error\n", cluster->id);
		goto ret_error;
	}

	do_div(freq, 1000); /* from Hz to KHz */

	ret = freq_qos_update_request(&cluster->max_req, freq);
	if (ret < 0) {
		pr_err("cluster %u qos update max freq(%llu) error\n", cluster->id, freq);
		goto ret_error;
	}

	udelay(100);

	mutex_unlock(&cluster->mutex);

	return (unsigned int)freq;

ret_error:
	mutex_unlock(&cluster->mutex);

	return 0;
}

static const struct of_device_id sprd_cpufreq_of_match[] = {
	{
		.compatible = "sprd,cpufreq-v2",
	},
	{
		/* Sentinel */
	},
};

static struct platform_driver sprd_cpufreq_platform_driver = {
	.driver = {
		.name = "sprd-cpufreq-v2",
		.of_match_table = sprd_cpufreq_of_match,
	},
	.probe = sprd_cpufreq_driver_probe,
	.remove = sprd_cpufreq_driver_remove,
};

static int __init sprd_cpufreq_platform_driver_register(void)
{
	return platform_driver_register(&sprd_cpufreq_platform_driver);
}

device_initcall(sprd_cpufreq_platform_driver_register);

MODULE_DESCRIPTION("sprd cpufreq v2 driver");
MODULE_LICENSE("GPL v2");
