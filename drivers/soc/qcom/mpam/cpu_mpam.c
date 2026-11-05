// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
 #define pr_fmt(fmt) "cpu_mpam: " fmt

#include <linux/io.h>
#include <linux/of.h>
#include <linux/configfs.h>
#include <linux/string.h>
#include <linux/bitmap.h>
#include <linux/sched/walt.h>
#include <trace/hooks/mpam.h>
#include <soc/qcom/mpam.h>

struct cpu_mpam_partition {
	struct config_group group;
	int part_id;
	int monitor_id;
	struct mpam_config_val *val;
};

struct cpu_mpam_msc {
	int msc_id;
	const char *msc_name;
};

static unsigned long *part_id_free_bitmap;
static unsigned long *monitor_free_bitmap;
static struct mpam_config_val mpam_default_val;
static struct monitors_value *mpam_mon_base;
static struct cpu_mpam_msc *mpam_mscs;
static int mpam_msc_cnt;
static bool can_monitor;

static inline struct cpu_mpam_partition *to_partition(
					   struct config_item *item)
{
	return container_of(to_config_group(item),
				struct cpu_mpam_partition, group);
}

static inline int get_part_id(struct config_item *item)
{
	return to_partition(item)->part_id;
}

static inline int get_monitor_id(struct config_item *item)
{
	return to_partition(item)->monitor_id;
}

static inline void set_part_id(struct config_item *item,
				int part_id)
{
	to_partition(item)->part_id = part_id;
}

static inline void set_monitor_id(struct config_item *item,
				int monitor_id)
{
	to_partition(item)->monitor_id = monitor_id;
}

static void cpu_mpam_partition_transfer(int old, int new)
{
	struct task_struct *p, *t;
	struct walt_task_struct *wts;

	if (old == new)
		return;

	rcu_read_lock();
	for_each_process_thread(p, t) {
		wts = (struct walt_task_struct *) t->android_vendor_data1;
		if (wts->mpam_part_id == old)
			wts->mpam_part_id = new;
	}
	rcu_read_unlock();
}

static ssize_t cpu_mpam_part_id_show(struct config_item *item, char *page)
{
	return scnprintf(page, PAGE_SIZE, "%d\n", get_part_id(item));
}
CONFIGFS_ATTR_RO(cpu_mpam_, part_id);

static void cpu_mpam_set_param(struct config_item *item,
		enum msc_id mscid, char *buf)
{
	int ret;
	uint32_t input;
	char temp[256];
	bool bypass_cache = false;
	char *token, *param_name, *param = temp;
	struct mpam_set_cache_partition mpam_param;
	struct cpu_mpam_partition *partition = to_partition(item);

	mpam_param.msc_id = mscid;
	mpam_param.part_id = get_part_id(item) + PARTID_RESERVED;
	mpam_param.cache_capacity = partition->val[mscid].capacity;
	mpam_param.cpbm_mask = partition->val[mscid].cpbm;
	mpam_param.dspri = partition->val[mscid].dspri;
	mpam_param.slc_partition_id = partition->val[mscid].slc_partition_id;
	mpam_param.mpam_config_ctrl = SET_ALL_CPU_TUNABLE;

	strscpy(temp, buf, sizeof(temp));
	while ((token = strsep(&param, ",")) != NULL) {
		param_name = strsep(&token, "=");
		if (param_name == NULL || token == NULL)
			continue;
		if (kstrtouint(token, 0, &input) < 0) {
			pr_err("invalid argument for %s\n", param_name);
			continue;
		}

		if (!strcmp("cmax", param_name))
			mpam_param.cache_capacity = input;
		else if (!strcmp("cpbm", param_name))
			mpam_param.cpbm_mask = input;
		else if (!strcmp("prio", param_name))
			mpam_param.dspri = input;
		else if (!strcmp("slc_partid", param_name))
			mpam_param.slc_partition_id = input;
	}

	/*
	 * If cpbm_mask set from userspace is 0, it means the least allocation
	 * for MPAM is needed. For the current hardware, the least allocation
	 * is cpbm_mask==1, cache_capacity == 0. And when cpbm_mask == 0,
	 * cache_capacity will always limit to 0.
	 */
	if (mpam_param.cpbm_mask == 0) {
		bypass_cache = true;
		mpam_param.cpbm_mask = 0x1;
		mpam_param.cache_capacity = 0;
	}

	ret = qcom_mpam_set_cache_partition(&mpam_param);
	if (!ret) {
		partition->val[mscid].capacity = mpam_param.cache_capacity;
		partition->val[mscid].dspri = mpam_param.dspri;
		partition->val[mscid].slc_partition_id = mpam_param.slc_partition_id;
		if (unlikely(bypass_cache))
			partition->val[mscid].cpbm = 0;
		else
			partition->val[mscid].cpbm = mpam_param.cpbm_mask;
	} else
		pr_err("set msc mpam settings failed, ret = %d\n", ret);
}

static void cpu_mpam_set_by_schemata(struct config_item *item,
		char *msc_name, char *param)
{
	int i;

	/*
	 * Go through all MSCs, set specific MSC if msc_name exists, or
	 * set all MSCs if msc_name not exists.
	 */
	for (i = 0; i < mpam_msc_cnt; i++)
		if ((msc_name == NULL) || (!strcmp(msc_name,
				mpam_mscs[i].msc_name)))
			cpu_mpam_set_param(item, mpam_mscs[i].msc_id, param);
}

static ssize_t cpu_mpam_schemata_show(struct config_item *item,
		char *page)
{
	int i, ret = 0;
	u32 msc_id;
	struct mpam_config_val *mpam_val;
	struct cpu_mpam_partition *partition = to_partition(item);

	for (i = 0; i < mpam_msc_cnt; i++) {
		msc_id = mpam_mscs[i].msc_id;
		mpam_val = &partition->val[msc_id];
		if (can_monitor)
			ret += scnprintf(page + ret, PAGE_SIZE,
				"%s:cmax=%d,cpbm=0x%x,prio=%d,slc_partid=%d\n",
				mpam_mscs[i].msc_name, mpam_val->capacity,
				mpam_val->cpbm, mpam_val->dspri, mpam_val->slc_partition_id);
		else
			ret += scnprintf(page + ret, PAGE_SIZE, "%s:cpbm=0x%x,slc_partid=%d\n",
				mpam_mscs[i].msc_name, mpam_val->cpbm, mpam_val->slc_partition_id);
	}

	return ret;
}

/*
 * Schemata supports setting multiple parameters for multiple MSCs
 * simultaneously.
 * Each basic parameter consists of a key-value pair separated by an
 * equal sign, and multiple parameters are delimited by commas.
 *
 *     <key1>=<value1>,<key2>=<value2>
 *
 * Parameters by default will be applied to all available MSCs. If you
 * only need to apply them to a specific MSC, add the MSC name followed
 * by a colon at the beginning.
 *
 *     <MSC_name>:<patameters>
 *
 * Multiple parameter lines need to be separated by semicolon.
 *
 *     <patameters line>;<patameters line>
 *
 * Exceptions for incorrectly passed parameters will be ignored.
 *
 * Example:
 *
 *     cmax=60,cpbm=0xf;L2_0:cmax=40,prio=1;L2_1:prio=2
 *
 * In the example above, the initial configuration sets cmax=60 and
 * cpbm=0xf for all MSCs. Subsequently, it adjusts the parameters to
 * cmax=40 and prio=1 for L2_0, and prio=2 for L2_1.
 */

static ssize_t cpu_mpam_schemata_store(struct config_item *item,
		const char *page, size_t count)
{
	char *token, *buf;

	/* Separate multiple parameter lines */
	while ((token = strsep((char **)&page, ";")) != NULL) {
		buf = strsep(&token, ":");
		if (token == NULL)
			cpu_mpam_set_by_schemata(item, NULL, buf);
		else
			cpu_mpam_set_by_schemata(item, buf, token);
	}

	return count;
}
CONFIGFS_ATTR(cpu_mpam_, schemata);

static ssize_t cpu_mpam_tasks_show(struct config_item *item, char *page)
{
	int part_id;
	ssize_t len = 0;
	struct task_struct *p, *t;
	struct walt_task_struct *wts;

	part_id = get_part_id(item);
	rcu_read_lock();
	for_each_process_thread(p, t) {
		wts = (struct walt_task_struct *) t->android_vendor_data1;
		if (wts->mpam_part_id == part_id)
			len += scnprintf(page + len, PAGE_SIZE - len, "%d ", t->pid);
	}
	rcu_read_unlock();
	len += scnprintf(page + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t cpu_mpam_tasks_store(struct config_item *item,
		const char *page, size_t count)
{
	int ret, part_id;
	pid_t pid_input;
	char *kbuf, *token;
	struct task_struct *p;
	struct walt_task_struct *wts;

	part_id = get_part_id(item);
	kbuf = (char *)page;
	while ((token = strsep(&kbuf, " ")) != NULL) {
		ret = kstrtouint(token, 10, &pid_input);
		if (ret < 0) {
			pr_err("invalid argument\n");
			goto err;
		}

		p = find_task_by_vpid(pid_input);
		if (IS_ERR_OR_NULL(p)) {
			pr_err("pid %d not exist\n", pid_input);
			continue;
		}

		wts = (struct walt_task_struct *) p->android_vendor_data1;
		wts->mpam_part_id = part_id;
	}

err:
	return count;
}
CONFIGFS_ATTR(cpu_mpam_, tasks);

static void cpu_mpam_enable_monitor(int monitor_id, int part_id,
		enum mpam_monitor_type type)
{
	int i;
	struct mpam_monitor_configuration monitor_param;

	monitor_param.part_id = part_id + PARTID_RESERVED;
	monitor_param.mon_instance = monitor_id;
	monitor_param.mon_type = type;
	monitor_param.mpam_config_ctrl = 1;


	for (i = 0; i < mpam_msc_cnt; i++) {
		monitor_param.msc_id = mpam_mscs[i].msc_id;
		qcom_mpam_config_monitor(&monitor_param);
	}
}

static void cpu_mpam_disable_monitor(int monitor_id,
		enum mpam_monitor_type type)
{
	int i;
	struct mpam_monitor_configuration monitor_param;

	monitor_param.mon_instance = monitor_id;
	monitor_param.mon_type = type;
	monitor_param.mpam_config_ctrl = 0;

	for (i = 0; i < mpam_msc_cnt; i++) {
		monitor_param.msc_id = mpam_mscs[i].msc_id;
		qcom_mpam_config_monitor(&monitor_param);
	}
}

static ssize_t cpu_mpam_enable_monitor_show(struct config_item *item,
		char *page)
{
	int monitor_id;

	monitor_id = get_monitor_id(item);
	return scnprintf(page, PAGE_SIZE, "%s\n", (monitor_id == INT_MAX) ?
		"disabled" : "enabled");
}

static ssize_t cpu_mpam_enable_monitor_store(struct config_item *item,
		const char *page, size_t count)
{
	int ret, monitor_id, part_id;
	bool input;

	part_id = get_part_id(item);
	monitor_id = get_monitor_id(item);

	ret = kstrtobool(page, &input);
	if (ret) {
		pr_err("invalid param\n");
		goto exit;
	}

	if (!input && monitor_id != INT_MAX) {
		bitmap_clear(monitor_free_bitmap, monitor_id, 1);
		set_monitor_id(item, INT_MAX);
		cpu_mpam_disable_monitor(monitor_id, MPAM_TYPE_CSU_MONITOR);
		cpu_mpam_disable_monitor(monitor_id, MPAM_TYPE_MBW_MONITOR);
	} else if (input && monitor_id == INT_MAX) {
		monitor_id = bitmap_find_next_zero_area(monitor_free_bitmap,
				MONITOR_MAX, 0, 1, 0);
		if (monitor_id > MONITOR_MAX) {
			pr_err("no available monitor\n");
			goto exit;
		}

		bitmap_set(monitor_free_bitmap, monitor_id, 1);
		set_monitor_id(item, monitor_id);
		cpu_mpam_enable_monitor(monitor_id, part_id,
				MPAM_TYPE_CSU_MONITOR);
		cpu_mpam_enable_monitor(monitor_id, part_id,
				MPAM_TYPE_MBW_MONITOR);
	}

exit:
	return count;
}
CONFIGFS_ATTR(cpu_mpam_, enable_monitor);

static ssize_t cpu_mpam_monitor_data_show(struct config_item *item,
		char *page)
{
	int i, monitor_id, retry_cnt = 0;
	ssize_t len = 0;
	uint32_t csu_value, mscid;
	uint64_t mbw_value, timestamp, capture_status;
	struct monitors_value *mpam_mon_data;

	monitor_id = get_monitor_id(item);
	if (monitor_id != INT_MAX) {
		for (i = 0; i < mpam_msc_cnt; i++) {
			mscid = mpam_mscs[i].msc_id;
			mpam_mon_data = &mpam_mon_base[mscid];
			do {
				while (unlikely((capture_status =
						mpam_mon_data->capture_status) % 2) &&
						(retry_cnt < MPAM_MAX_RETRY))
					retry_cnt++;
				timestamp = mpam_mon_data->last_capture_time;
				csu_value = mpam_mon_data->csu_mon_value[monitor_id];
				mbw_value = mpam_mon_data->mbw_mon_value[monitor_id];
			} while (capture_status != mpam_mon_data->capture_status);
			len += scnprintf(page + len, PAGE_SIZE - len,
				"%s:timestamp=%llu,csu=%u,mbwu=%llu\n",
				mpam_mscs[i]. msc_name, timestamp,
				csu_value, mbw_value);
		}
		return len;
	} else
		return scnprintf(page, PAGE_SIZE, "monitor not enabled\n");
}
CONFIGFS_ATTR_RO(cpu_mpam_, monitor_data);

static struct configfs_attribute *cpu_mpam_attrs[] = {
	&cpu_mpam_attr_part_id,
	&cpu_mpam_attr_schemata,
	&cpu_mpam_attr_tasks,
	&cpu_mpam_attr_enable_monitor,
	&cpu_mpam_attr_monitor_data,
	NULL,
};

static struct configfs_attribute *cpu_mpam_attrs_legacy[] = {
	&cpu_mpam_attr_part_id,
	&cpu_mpam_attr_schemata,
	&cpu_mpam_attr_tasks,
	NULL,
};

static void cpu_mpam_reset_param(int part_id)
{
	int i;
	struct mpam_set_cache_partition mpam_param;

	mpam_param.part_id = part_id + PARTID_RESERVED;
	mpam_param.dspri = mpam_default_val.dspri;
	mpam_param.cpbm_mask = mpam_default_val.cpbm;
	mpam_param.cache_capacity = mpam_default_val.capacity;
	mpam_param.slc_partition_id = mpam_default_val.slc_partition_id;
	mpam_param.mpam_config_ctrl = SET_ALL_CPU_TUNABLE;

	for (i = 0; i < mpam_msc_cnt; i++) {
		mpam_param.msc_id = mpam_mscs[i].msc_id;
		qcom_mpam_set_cache_partition(&mpam_param);
	}
}

static void cpu_mpam_drop_item(struct config_group *group,
		struct config_item *item)
{
	int part_id, monitor_id;

	part_id = get_part_id(item);
	monitor_id = get_monitor_id(item);

	cpu_mpam_partition_transfer(part_id, PARTID_DEFAULT);
	bitmap_clear(part_id_free_bitmap, part_id, 1);
	if (monitor_id != INT_MAX) {
		bitmap_clear(monitor_free_bitmap, monitor_id, 1);
		cpu_mpam_disable_monitor(monitor_id, MPAM_TYPE_CSU_MONITOR);
		cpu_mpam_disable_monitor(monitor_id, MPAM_TYPE_MBW_MONITOR);
	}
	cpu_mpam_reset_param(part_id);

	kfree(to_partition(item)->val);
	kfree(to_partition(item));
}

static const struct config_item_type cpu_mpam_item_type = {
	.ct_attrs	= cpu_mpam_attrs,
};

static const struct config_item_type cpu_mpam_item_type_legacy = {
	.ct_attrs	= cpu_mpam_attrs_legacy,
};

static struct config_group *cpu_mpam_make_group(
		struct config_group *group, const char *name)
{
	int i, part_id;
	struct cpu_mpam_partition *partition;

	part_id = bitmap_find_next_zero_area(part_id_free_bitmap,
				   PARTID_AVAILABLE, 0, 1, 0);

	if (part_id > PARTID_AVAILABLE)
		return ERR_PTR(-ENOMEM);

	partition = kzalloc(sizeof(struct cpu_mpam_partition), GFP_KERNEL);
	if (!partition)
		return ERR_PTR(-ENOMEM);

	bitmap_set(part_id_free_bitmap, part_id, 1);
	partition->part_id = part_id;
	partition->monitor_id = INT_MAX;

	partition->val = kcalloc(mpam_msc_cnt, sizeof(struct mpam_config_val), GFP_KERNEL);
	if (!partition->val)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < mpam_msc_cnt; i++)
		memcpy(&(partition->val[i]), &mpam_default_val, sizeof(struct mpam_config_val));

	cpu_mpam_reset_param(part_id);

	if (can_monitor)
		config_group_init_type_name(&partition->group, name,
				   &cpu_mpam_item_type);
	else
		config_group_init_type_name(&partition->group, name,
			&cpu_mpam_item_type_legacy);

	return &partition->group;
}

static struct configfs_group_operations cpu_mpam_group_ops = {
	.make_group	= cpu_mpam_make_group,
	.drop_item	= cpu_mpam_drop_item,
};

static const struct config_item_type cpu_mpam_subsys_type = {
	.ct_group_ops	= &cpu_mpam_group_ops,
	.ct_owner	= THIS_MODULE,
};

static struct configfs_subsystem cpu_mpam_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "qcom_mpam",
			.ci_type = &cpu_mpam_subsys_type,
		},
	},
};

static void cpu_mpam_write_partid(u8 part_id)
{
	u64 reg;

	part_id += PARTID_RESERVED;
	reg = (part_id << PARTID_I_SHIFT) | (part_id << PARTID_D_SHIFT);

	write_sysreg_s(reg, SYS_MPAM0_EL1);
	write_sysreg_s(reg, SYS_MPAM1_EL1);
}

static void cpu_mpam_switch_task(void *unused, struct task_struct *prev,
							struct task_struct *next)
{
	struct walt_task_struct *wts;

	wts = (struct walt_task_struct *) next->android_vendor_data1;
	cpu_mpam_write_partid(wts->mpam_part_id);
}

static int cpu_mpam_configfs_init(void)
{
	int ret;
	struct config_group *default_group;

	part_id_free_bitmap = bitmap_zalloc(PARTID_AVAILABLE, GFP_KERNEL);
	monitor_free_bitmap = bitmap_zalloc(MONITOR_MAX, GFP_KERNEL);
	if (!part_id_free_bitmap) {
		pr_err("Error alloc bitmap\n");
		return -ENOMEM;
	}

	config_group_init(&cpu_mpam_subsys.su_group);
	mutex_init(&cpu_mpam_subsys.su_mutex);

	default_group = cpu_mpam_make_group(NULL, "default");
	if (IS_ERR(default_group)) {
		pr_err("Error create group\n");
		return PTR_ERR(default_group);
	}
	configfs_add_default_group(default_group, &cpu_mpam_subsys.su_group);

	ret = configfs_register_subsystem(&cpu_mpam_subsys);
	if (ret) {
		mutex_destroy(&cpu_mpam_subsys.su_mutex);
		pr_err("Error while registering subsystem %d\n", ret);
		return ret;
	}

	register_trace_android_vh_mpam_set(cpu_mpam_switch_task, NULL);

	return 0;
}

static void cpu_mpam_configfs_remove(void)
{
	configfs_unregister_subsystem(&cpu_mpam_subsys);
	unregister_trace_android_vh_mpam_set(cpu_mpam_switch_task, NULL);
}

static int cpu_mpam_probe(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	uint32_t mscid;
	struct resource *res;
	struct device_node *node;
	const char *msc_name_dt;
	struct mpam_ver_ret mpam_version;
	struct mpam_read_cache_portion mpam_param;

	ret = qcom_mpam_get_version(&mpam_version);
	if (ret || mpam_version.version < 0x10000) {
		dev_err(&pdev->dev, "CPU MPAM is not available\n");
		return -ENODEV;
	}

	mpam_msc_cnt = of_get_child_count(pdev->dev.of_node);
	if (!mpam_msc_cnt) {
		dev_err(&pdev->dev, "No MSC found\n");
		return -ENODEV;
	}

	mpam_mscs = devm_kcalloc(&pdev->dev, mpam_msc_cnt,
		sizeof(struct cpu_mpam_msc), GFP_KERNEL);
	if (!mpam_mscs)
		return -ENOMEM;

	for_each_child_of_node(pdev->dev.of_node, node) {
		ret = of_property_read_u32(node, "qcom,msc-id", &mscid);
		of_property_read_string(node, "qcom,msc-name", &msc_name_dt);
		if (ret || mscid >= MSC_MAX || IS_ERR_OR_NULL(msc_name_dt))
			continue;
		mpam_mscs[i].msc_id = mscid;
		mpam_mscs[i].msc_name = msc_name_dt;
		i++;
	}
	mpam_msc_cnt = i;

	mpam_param.msc_id = 0;
	mpam_param.part_id = PARTID_MAX - 1;
	ret = qcom_mpam_get_cache_partition(&mpam_param, &mpam_default_val);
	if (ret) {
		dev_err(&pdev->dev, "Error getting default value %d\n", ret);
		mpam_default_val.cpbm = UINT_MAX;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mon-base");
	if (!res) {
		dev_err(&pdev->dev, "cpu mpam does not have monitoring support\n");
		can_monitor = false;
	} else {
		mpam_mon_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR_OR_NULL(mpam_mon_base)) {
			dev_err(&pdev->dev, "Error ioremap mpam_mon_base\n");
			return -ENODEV;
		}
		can_monitor = true;
	}
	if (can_monitor) {
		mpam_default_val.capacity = 100;
		mpam_default_val.dspri = 0;
	}

	ret = cpu_mpam_configfs_init();
	if (ret)
		dev_err(&pdev->dev, "Error creating configfs %d\n", ret);

	return ret;
}

static int cpu_mpam_remove(struct platform_device *pdev)
{
	cpu_mpam_configfs_remove();
	return 0;
}

static const struct of_device_id cpu_mpam_table[] = {
	{ .compatible = "qcom,cpu-mpam" },
	{}
};
MODULE_DEVICE_TABLE(of, cpu_mpam_table);

static struct platform_driver cpu_mpam_driver = {
	.driver = {
		.name = "cpu-mpam",
		.of_match_table = cpu_mpam_table,
		.suppress_bind_attrs = true,
	},
	.probe = cpu_mpam_probe,
	.remove = cpu_mpam_remove,
};

module_platform_driver(cpu_mpam_driver);

MODULE_SOFTDEP("pre: mpam");
MODULE_DESCRIPTION("QCOM CPU MPAM driver");
MODULE_LICENSE("GPL");
