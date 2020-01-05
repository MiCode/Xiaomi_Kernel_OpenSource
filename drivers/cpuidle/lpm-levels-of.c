// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include "lpm-levels.h"

enum lpm_type {
	IDLE = 0,
	SUSPEND,
	LATENCY,
	LPM_TYPE_NR,
};

struct lpm_type_str {
	enum lpm_type type;
	char *str;
};

static const struct lpm_type_str lpm_types[] = {
	{IDLE, "idle_enabled"},
	{SUSPEND, "suspend_enabled"},
	{LATENCY, "exit_latency_us"},
};

static struct lpm_level_avail *cpu_level_available[NR_CPUS];
static struct platform_device *lpm_pdev;

static int lpm_of_read_u32(struct device_node *dn, const char *key,
					u32 *val, bool is_err)
{
	int ret;

	ret = of_property_read_u32(dn, key, val);
	if (is_err && ret)
		pr_err("%s:failed to read key:%s ret:%d\n", dn->name, key, ret);

	return ret;
}

static void *get_enabled_ptr(struct kobj_attribute *attr,
					struct lpm_level_avail *avail)
{
	void *arg = NULL;

	if (!strcmp(attr->attr.name, lpm_types[IDLE].str))
		arg = (void *) &avail->idle_enabled;
	else if (!strcmp(attr->attr.name, lpm_types[SUSPEND].str))
		arg = (void *) &avail->suspend_enabled;

	return arg;
}

static struct lpm_level_avail *get_avail_ptr(struct kobject *kobj,
					struct kobj_attribute *attr)
{
	struct lpm_level_avail *avail = NULL;

	if (!strcmp(attr->attr.name, lpm_types[IDLE].str))
		avail = container_of(attr, struct lpm_level_avail,
					idle_enabled_attr);
	else if (!strcmp(attr->attr.name, lpm_types[SUSPEND].str))
		avail = container_of(attr, struct lpm_level_avail,
					suspend_enabled_attr);
	else if (!strcmp(attr->attr.name, lpm_types[LATENCY].str))
		avail = container_of(attr, struct lpm_level_avail,
					latency_attr);

	return avail;
}

static ssize_t lpm_latency_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct kernel_param kp;
	struct lpm_level_avail *avail = get_avail_ptr(kobj, attr);

	if (WARN_ON(!avail))
		return -EINVAL;

	kp.arg = &avail->exit_latency;

	ret = param_get_uint(buf, &kp);
	if (ret > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		ret++;
	}

	return ret;
}

ssize_t lpm_enable_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int ret = 0;
	struct kernel_param kp;
	struct lpm_level_avail *avail = get_avail_ptr(kobj, attr);

	if (WARN_ON(!avail))
		return -EINVAL;

	kp.arg = get_enabled_ptr(attr, avail);
	if (WARN_ON(!kp.arg))
		return -EINVAL;

	ret = param_get_bool(buf, &kp);
	if (ret > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		ret++;
	}

	return ret;
}

ssize_t lpm_enable_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t len)
{
	int ret = 0;
	struct kernel_param kp;
	struct lpm_level_avail *avail;

	avail = get_avail_ptr(kobj, attr);
	if (WARN_ON(!avail))
		return -EINVAL;

	kp.arg = get_enabled_ptr(attr, avail);
	ret = param_set_bool(buf, &kp);

	return ret ? ret : len;
}

static int create_lvl_avail_nodes(const char *name,
			struct kobject *parent, struct lpm_level_avail *avail,
			void *data, int index, bool cpu_node)
{
	struct attribute_group *attr_group = NULL;
	struct attribute **attr = NULL;
	struct kobject *kobj = NULL;
	int ret = 0;

	kobj = kobject_create_and_add(name, parent);
	if (!kobj)
		return -ENOMEM;

	attr_group = devm_kcalloc(&lpm_pdev->dev, 1, sizeof(*attr_group),
				  GFP_KERNEL);
	if (!attr_group) {
		ret = -ENOMEM;
		goto failed;
	}

	attr = devm_kcalloc(&lpm_pdev->dev, LPM_TYPE_NR + 1, sizeof(*attr),
			    GFP_KERNEL);
	if (!attr) {
		ret = -ENOMEM;
		goto failed;
	}

	sysfs_attr_init(&avail->idle_enabled_attr.attr);
	avail->idle_enabled_attr.attr.name = lpm_types[IDLE].str;
	avail->idle_enabled_attr.attr.mode = 0644;
	avail->idle_enabled_attr.show = lpm_enable_show;
	avail->idle_enabled_attr.store = lpm_enable_store;

	sysfs_attr_init(&avail->suspend_enabled_attr.attr);
	avail->suspend_enabled_attr.attr.name = lpm_types[SUSPEND].str;
	avail->suspend_enabled_attr.attr.mode = 0644;
	avail->suspend_enabled_attr.show = lpm_enable_show;
	avail->suspend_enabled_attr.store = lpm_enable_store;

	sysfs_attr_init(&avail->latency_attr.attr);
	avail->latency_attr.attr.name = lpm_types[LATENCY].str;
	avail->latency_attr.attr.mode = 0444;
	avail->latency_attr.show = lpm_latency_show;
	avail->latency_attr.store = NULL;

	attr[0] = &avail->idle_enabled_attr.attr;
	attr[1] = &avail->suspend_enabled_attr.attr;
	attr[2] = &avail->latency_attr.attr;
	attr[3] = NULL;
	attr_group->attrs = attr;

	ret = sysfs_create_group(kobj, attr_group);
	if (ret)
		goto failed;

	avail->idle_enabled = true;
	avail->suspend_enabled = true;
	avail->kobj = kobj;
	avail->data = data;
	avail->idx = index;
	avail->cpu_node = cpu_node;

	return 0;

failed:
	kobject_put(kobj);
	return ret;
}

static int create_cpu_lvl_nodes(struct lpm_cluster *p, struct kobject *parent)
{
	int cpu;
	int i, cpu_idx;
	struct kobject **cpu_kobj = NULL;
	struct lpm_level_avail *level_list = NULL;
	char cpu_name[20] = {0};
	int ret = 0;
	struct list_head *pos;

	cpu_kobj = devm_kcalloc(&lpm_pdev->dev, cpumask_weight(&p->child_cpus),
				sizeof(*cpu_kobj), GFP_KERNEL);
	if (!cpu_kobj)
		return -ENOMEM;

	cpu_idx = 0;
	list_for_each(pos, &p->cpu) {
		struct lpm_cpu *lpm_cpu = list_entry(pos, struct lpm_cpu, list);

		for_each_cpu(cpu, &lpm_cpu->related_cpus) {
			snprintf(cpu_name, sizeof(cpu_name), "cpu%d", cpu);
			cpu_kobj[cpu_idx] = kobject_create_and_add(cpu_name,
					parent);
			if (!cpu_kobj[cpu_idx]) {
				ret = -ENOMEM;
				goto release_kobj;
			}

			level_list = devm_kcalloc(&lpm_pdev->dev,
						  lpm_cpu->nlevels,
						  sizeof(*level_list),
						  GFP_KERNEL);
			if (!level_list) {
				ret = -ENOMEM;
				goto release_kobj;
			}

			/*
			 * Skip enable/disable for WFI. cpuidle expects WFI to
			 * be available at all times.
			 */
			for (i = 1; i < lpm_cpu->nlevels; i++) {
				level_list[i].exit_latency =
					p->levels[i].pwr.exit_latency;
				ret = create_lvl_avail_nodes(
						lpm_cpu->levels[i].name,
						cpu_kobj[cpu_idx],
						&level_list[i],
						(void *)lpm_cpu, cpu, true);
				if (ret)
					goto release_kobj;
			}

			cpu_level_available[cpu] = level_list;
			cpu_idx++;
		}
	}

	return ret;

release_kobj:
	for (i = 0; i < cpumask_weight(&p->child_cpus); i++)
		kobject_put(cpu_kobj[i]);

	return ret;
}

int create_cluster_lvl_nodes(struct lpm_cluster *p, struct kobject *kobj)
{
	int ret = 0;
	struct lpm_cluster *child = NULL;
	int i;
	struct kobject *cluster_kobj = NULL;

	if (!p)
		return -ENODEV;

	cluster_kobj = kobject_create_and_add(p->cluster_name, kobj);
	if (!cluster_kobj)
		return -ENOMEM;

	for (i = 0; i < p->nlevels; i++) {
		p->levels[i].available.exit_latency =
					p->levels[i].pwr.exit_latency;
		ret = create_lvl_avail_nodes(p->levels[i].level_name,
				cluster_kobj, &p->levels[i].available,
				(void *)p, 0, false);
		if (ret)
			return ret;
	}

	list_for_each_entry(child, &p->child, list) {
		ret = create_cluster_lvl_nodes(child, cluster_kobj);
		if (ret)
			return ret;
	}

	if (!list_empty(&p->cpu))
		ret = create_cpu_lvl_nodes(p, cluster_kobj);

	return ret;
}

int lpm_cpu_mode_allow(unsigned int cpu,
		unsigned int index, bool from_idle)
{
	struct lpm_level_avail *avail = cpu_level_available[cpu];

	if (lpm_pdev && !index)
		return 1;

	if (!lpm_pdev || !avail)
		return !from_idle;

	return !!(from_idle ? avail[index].idle_enabled :
				avail[index].suspend_enabled);
}

bool lpm_cluster_mode_allow(struct lpm_cluster *cluster,
		unsigned int mode, bool from_idle)
{
	struct lpm_level_avail *avail = &cluster->levels[mode].available;

	if (!lpm_pdev || !avail)
		return false;

	return !!(from_idle ? avail->idle_enabled :
				avail->suspend_enabled);
}

static int parse_cluster_params(struct device_node *dn, struct lpm_cluster *c)
{
	int ret;

	ret = of_property_read_string(dn, "label", &c->cluster_name);
	if (ret) {
		pr_err("Failed to read label ret: %d\n", ret);
		return ret;
	}

	ret = lpm_of_read_u32(dn, "qcom,psci-mode-shift",
			      &c->psci_mode_shift, true);
	if (ret)
		return ret;

	ret = lpm_of_read_u32(dn, "qcom,psci-mode-mask",
			      &c->psci_mode_mask, true);
	if (ret)
		return ret;

	c->lpm_prediction = !(of_property_read_bool(dn,
						    "qcom,disable-prediction"));

	if (c->lpm_prediction) {
		ret = lpm_of_read_u32(dn, "qcom,clstr-tmr-add", &c->tmr_add,
				      false);
		if (ret || c->tmr_add < TIMER_ADD_LOW ||
					c->tmr_add > TIMER_ADD_HIGH) {
			c->tmr_add = DEFAULT_TIMER_ADD;
			ret = 0;
		}
	}

	/* Set default_level to 0 as default */
	c->default_level = 0;

	return ret;
}

static int parse_power_params(struct device_node *dn, struct power_params *pwr)
{
	int ret;

	ret  = lpm_of_read_u32(dn, "qcom,entry-latency-us",
			       &pwr->entry_latency, true);
	if (ret)
		return ret;

	ret  = lpm_of_read_u32(dn, "qcom,exit-latency-us",
			       &pwr->exit_latency, true);
	if (ret)
		return ret;

	ret = lpm_of_read_u32(dn, "qcom,min-residency-us",
			      &pwr->min_residency, true);

	return ret;
}

static int parse_cluster_level(struct device_node *dn,
			       struct lpm_cluster *cluster)
{
	struct lpm_cluster_level *level = &cluster->levels[cluster->nlevels];
	int ret = -ENOMEM;

	ret = of_property_read_string(dn, "label", &level->level_name);
	if (ret) {
		pr_err("Failed to read label ret: %d\n", ret);
		return ret;
	}

	ret = lpm_of_read_u32(dn, "qcom,psci-mode", &level->psci_id, true);
	if (ret)
		return ret;

	level->is_reset = of_property_read_bool(dn, "qcom,is-reset");

	if (cluster->nlevels != cluster->default_level) {
		ret = lpm_of_read_u32(dn, "qcom,min-child-idx",
				      &level->min_child_level, true);
		if (ret)
			return ret;

		if (cluster->min_child_level > level->min_child_level)
			cluster->min_child_level = level->min_child_level;
	}

	level->notify_rpm = of_property_read_bool(dn, "qcom,notify-rpm");

	ret = parse_power_params(dn, &level->pwr);
	if (ret) {
		pr_err("Failed to parse power params ret:%d\n", ret);
		return ret;
	}

	ret = lpm_of_read_u32(dn, "qcom,reset-level",
			      &level->reset_level, false);
	if (ret == -EINVAL)
		level->reset_level = LPM_RESET_LVL_NONE;
	else if (ret)
		return ret;

	cluster->nlevels++;

	return 0;
}

static int parse_cpu_mode(struct device_node *n, struct lpm_cpu_level *l)
{
	int ret;

	ret = of_property_read_string(n, "label", &l->name);
	if (ret) {
		pr_err("Failed to read label level: %s\n", l->name);
		return ret;
	}

	return lpm_of_read_u32(n, "qcom,psci-cpu-mode", &l->psci_id, true);
}

static int get_cpumask_for_node(struct device_node *node, struct cpumask *mask)
{
	struct device_node *cpu_node;
	int cpu;
	int idx = 0;

	cpu_node = of_parse_phandle(node, "qcom,cpu", idx++);
	if (!cpu_node) {
		pr_info("%s: No CPU phandle, assuming single cluster\n",
				node->full_name);
		/*
		 * Not all targets have the cpu node populated in the device
		 * tree. If cpu node is not populated assume all possible
		 * nodes belong to this cluster
		 */
		cpumask_copy(mask, cpu_possible_mask);
		return 0;
	}

	while (cpu_node) {
		for_each_possible_cpu(cpu) {
			if (of_get_cpu_node(cpu, NULL) == cpu_node) {
				cpumask_set_cpu(cpu, mask);
				break;
			}
		}
		of_node_put(cpu_node);
		cpu_node = of_parse_phandle(node, "qcom,cpu", idx++);
	}

	return 0;
}

static int parse_cpu(struct device_node *node, struct lpm_cpu *cpu)
{

	struct device_node *n;
	int ret, i;

	for_each_child_of_node(node, n) {
		struct lpm_cpu_level *l = &cpu->levels[cpu->nlevels];

		cpu->nlevels++;

		ret = parse_cpu_mode(n, l);
		if (ret) {
			of_node_put(n);
			return ret;
		}

		ret = parse_power_params(n, &l->pwr);
		if (ret) {
			of_node_put(n);
			return ret;
		}

		l->use_bc_timer = of_property_read_bool(n,
					"qcom,use-broadcast-timer");

		l->is_reset = of_property_read_bool(n, "qcom,is-reset");

		ret = lpm_of_read_u32(n, "qcom,reset-level", &l->reset_level,
								false);
		of_node_put(n);

		if (ret == -EINVAL)
			l->reset_level = LPM_RESET_LVL_NONE;
		else if (ret)
			return ret;
	}

	for (i = 1; i < cpu->nlevels; i++)
		cpu->levels[i-1].pwr.max_residency =
			cpu->levels[i].pwr.min_residency - 1;

	cpu->levels[i-1].pwr.max_residency = UINT_MAX;

	return 0;
}

static int parse_cpu_levels(struct device_node *dn, struct lpm_cluster *c)
{
	int ret;
	struct lpm_cpu *cpu;

	cpu = devm_kcalloc(&lpm_pdev->dev, 1, sizeof(*cpu), GFP_KERNEL);
	if (!cpu)
		return -ENOMEM;

	if (get_cpumask_for_node(dn, &cpu->related_cpus))
		return -EINVAL;

	cpu->parent = c;

	ret = lpm_of_read_u32(dn, "qcom,psci-mode-shift",
			      &cpu->psci_mode_shift, true);
	if (ret)
		return ret;

	ret = lpm_of_read_u32(dn, "qcom,psci-mode-mask",
			      &cpu->psci_mode_mask, true);
	if (ret)
		return ret;

	cpu->ipi_prediction = !(of_property_read_bool(dn,
					"qcom,disable-ipi-prediction"));

	cpu->lpm_prediction = !(of_property_read_bool(dn,
					"qcom,disable-prediction"));

	if (cpu->lpm_prediction) {
		ret = lpm_of_read_u32(dn, "qcom,ref-stddev",
				      &cpu->ref_stddev, false);
		if (ret || cpu->ref_stddev < STDDEV_LOW ||
					cpu->ref_stddev > STDDEV_HIGH)
			cpu->ref_stddev = DEFAULT_STDDEV;

		ret = lpm_of_read_u32(dn, "qcom,tmr-add",
				      &cpu->tmr_add, false);
		if (ret || cpu->tmr_add < TIMER_ADD_LOW ||
					cpu->tmr_add > TIMER_ADD_HIGH)
			cpu->tmr_add = DEFAULT_TIMER_ADD;

		ret = lpm_of_read_u32(dn, "qcom,ref-premature-cnt",
				      &cpu->ref_premature_cnt, false);
		if (ret || cpu->ref_premature_cnt < PREMATURE_CNT_LOW ||
				cpu->ref_premature_cnt > PREMATURE_CNT_HIGH)
			cpu->ref_premature_cnt = DEFAULT_PREMATURE_CNT;
	}

	ret = parse_cpu(dn, cpu);
	if (ret) {
		pr_err("Failed to parse cpu %s\n", dn->name);
		return ret;
	}

	cpumask_or(&c->child_cpus, &c->child_cpus, &cpu->related_cpus);
	list_add(&cpu->list, &c->cpu);

	return ret;
}

void free_cluster_node(struct lpm_cluster *cluster)
{
	struct lpm_cpu *cpu, *n;
	struct lpm_cluster *cl, *m;

	list_for_each_entry_safe(cl, m, &cluster->child, list) {
		list_del(&cl->list);
		free_cluster_node(cl);
	}

	list_for_each_entry_safe(cpu, n, &cluster->cpu, list)
		list_del(&cpu->list);
}

/*
 * TODO:
 * Expects a CPU or a cluster only. This ensures that affinity
 * level of a cluster is consistent with reference to its
 * child nodes.
 */
struct lpm_cluster *parse_cluster(struct device_node *node,
				  struct lpm_cluster *parent)
{
	struct lpm_cluster *c;
	struct device_node *n;
	int ret = 0, i;

	c = devm_kcalloc(&lpm_pdev->dev, 1, sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	ret = parse_cluster_params(node, c);
	if (ret)
		return NULL;

	INIT_LIST_HEAD(&c->child);
	INIT_LIST_HEAD(&c->cpu);
	c->parent = parent;
	spin_lock_init(&c->sync_lock);
	c->min_child_level = NR_LPM_LEVELS;

	for_each_child_of_node(node, n) {
		if (!n->name) {
			of_node_put(n);
			continue;
		}

		if (!of_node_cmp(n->name, "qcom,pm-cluster-level")) {
			if (parse_cluster_level(n, c)) {
				pr_err("Failed parse pm-cluster-level\n");
				goto failed_parse_cluster;
			}
		} else if (!of_node_cmp(n->name, "qcom,pm-cluster")) {
			struct lpm_cluster *child;

			child = parse_cluster(n, c);
			if (!child) {
				pr_err("Failed parse pm-cluster\n");
				goto failed_parse_cluster;
			}

			list_add(&child->list, &c->child);
			cpumask_or(&c->child_cpus, &c->child_cpus,
					&child->child_cpus);
			c->aff_level = child->aff_level + 1;
		} else if (!of_node_cmp(n->name, "qcom,pm-cpu")) {
			if (parse_cpu_levels(n, c)) {
				pr_err("Failed parse pm-cpu\n");
				goto failed_parse_cluster;
			}

			c->aff_level = 1;
		}

		of_node_put(n);
	}

	if (cpumask_intersects(&c->child_cpus, cpu_online_mask))
		c->last_level = c->default_level;
	else
		c->last_level = c->nlevels-1;

	for (i = 1; i < c->nlevels; i++)
		c->levels[i-1].pwr.max_residency =
			c->levels[i].pwr.min_residency - 1;

	c->levels[i-1].pwr.max_residency = UINT_MAX;

	return c;

failed_parse_cluster:
	of_node_put(n);
	if (parent)
		list_del(&c->list);
	free_cluster_node(c);
	return NULL;
}

struct lpm_cluster *lpm_of_parse_cluster(struct platform_device *pdev)
{
	struct device_node *top = NULL;
	struct lpm_cluster *c;

	top = of_find_node_by_name(pdev->dev.of_node, "qcom,pm-cluster");
	if (!top) {
		pr_err("Failed to find root node\n");
		return ERR_PTR(-ENODEV);
	}

	lpm_pdev = pdev;
	c = parse_cluster(top, NULL);
	of_node_put(top);
	return c;
}

void cluster_dt_walkthrough(struct lpm_cluster *cluster)
{
	struct list_head *list;
	struct lpm_cpu *cpu;
	int i, j;
	static int id;
	char str[10] = {0};

	if (!cluster)
		return;

	for (i = 0; i < id; i++)
		snprintf(str+i, 10 - i, "\t");

	for (i = 0; i < cluster->nlevels; i++) {
		struct lpm_cluster_level *l = &cluster->levels[i];

		pr_info("cluster: %s \t level: %s\n", cluster->cluster_name,
							l->level_name);
	}

	list_for_each_entry(cpu, &cluster->cpu, list) {
		for (j = 0; j < cpu->nlevels; j++)
			pr_info("%s\tCPU level name: %s\n", str,
						cpu->levels[j].name);
	}

	id++;

	list_for_each(list, &cluster->child) {
		struct lpm_cluster *n;

		n = list_entry(list, typeof(*n), list);
		cluster_dt_walkthrough(n);
	}
	id--;
}
