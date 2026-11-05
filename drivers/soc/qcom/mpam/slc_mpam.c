// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
 #define pr_fmt(fmt) "mpam_slc: " fmt

#include <linux/io.h>
#include <linux/of.h>
#include <linux/configfs.h>
#include <linux/string.h>
#include <soc/qcom/mpam.h>
#include <soc/qcom/mpam_msc.h>
#include <soc/qcom/mpam_slc.h>

struct slc_mpam_item {
	struct config_group group;
	int part_id;
	int client_id;
	bool cap_mon_enabled;
	bool miss_mon_enabled;
};

static struct config_group *root_group;

static inline struct slc_mpam_item *get_pm_item(
					   struct config_item *item)
{
	return container_of(to_config_group(item),
				struct slc_mpam_item, group);
}

static inline int set_msc_query(struct msc_query *query,
					   struct slc_mpam_item *pm_item)
{
	struct qcom_mpam_msc *qcom_mpam_msc;

	qcom_mpam_msc = qcom_msc_lookup(SLC);
	if (!qcom_mpam_msc)
		return -ENODEV;

	query->qcom_msc_id.qcom_msc_type =
		qcom_mpam_msc->qcom_msc_id.qcom_msc_type;
	query->qcom_msc_id.qcom_msc_class =
		qcom_mpam_msc->qcom_msc_id.qcom_msc_class;
	query->qcom_msc_id.idx =
		qcom_mpam_msc->qcom_msc_id.idx;

	query->client_id = pm_item->client_id;
	query->part_id = pm_item->part_id;

	return 0;
}

static ssize_t slc_mpam_schemata_show(struct config_item *item,
		char *page)
{
	int ret;
	struct msc_query query;
	struct qcom_slc_gear_val gear_config;

	set_msc_query(&query, get_pm_item(item));

	ret = msc_system_get_partition(SLC, &query, &gear_config);
	if (ret)
		return scnprintf(page, PAGE_SIZE,
			"failed to get schemata %d\n", ret);

	return scnprintf(page, PAGE_SIZE, "gear=%d\n",
		gear_config.gear_val);
}

static ssize_t slc_mpam_schemata_store(struct config_item *item,
		const char *page, size_t count)
{
	int ret, input;
	char *token, *param_name;
	struct msc_query query;
	struct qcom_slc_gear_val gear_config;

	set_msc_query(&query, get_pm_item(item));

	while ((token = strsep((char **)&page, ",")) != NULL) {
		param_name = strsep(&token, "=");
		if (param_name == NULL || token == NULL)
			continue;
		if (kstrtouint(token, 0, &input) < 0) {
			pr_err("invalid argument for %s\n", param_name);
			return -EINVAL;
		}

		if (!strcmp("gear", param_name))
			gear_config.gear_val = input;
	}

	ret = msc_system_set_partition(SLC, &query, &gear_config);
	if (ret) {
		pr_err("set slc cache partition failed, ret=%d\n", ret);
		return -EBUSY;
	}

	return count;
}
CONFIGFS_ATTR(slc_mpam_, schemata);

static ssize_t slc_mpam_enable_cap_monitor_show(struct config_item *item,
		char *page)
{
	return scnprintf(page, PAGE_SIZE, "%s\n",
		(get_pm_item(item)->cap_mon_enabled) ? "enabled" : "disabled");
}

static ssize_t slc_mpam_enable_cap_monitor_store(struct config_item *item,
		const char *page, size_t count)
{
	int ret;
	bool input;
	struct msc_query query;
	struct slc_mon_config_val mon_cfg_val;
	struct slc_mpam_item *pm_item = get_pm_item(item);

	set_msc_query(&query, pm_item);

	ret = kstrtobool(page, &input);
	if (ret) {
		pr_err("invalid argument\n");
		return -EINVAL;
	}

	if (!input && pm_item->cap_mon_enabled)
		mon_cfg_val.enable = 0;
	else if (input && !pm_item->cap_mon_enabled)
		mon_cfg_val.enable = 1;
	else
		goto exit;

	mon_cfg_val.slc_mon_function = CACHE_CAPACITY_CONFIG;
	ret = msc_system_mon_config(SLC, &query, &mon_cfg_val);
	if (ret) {
		pr_err("monitor %s failed %d\n",
			(input) ? "enable" : "disable", ret);
		return -EBUSY;
	}
	pm_item->cap_mon_enabled = input;

exit:
	return count;
}
CONFIGFS_ATTR(slc_mpam_, enable_cap_monitor);

static ssize_t slc_mpam_cap_monitor_data_show(struct config_item *item,
		char *page)
{
	struct msc_query query;
	union mon_values mon_data;

	if (!get_pm_item(item)->cap_mon_enabled)
		return scnprintf(page, PAGE_SIZE, "monitor not enabled\n");

	set_msc_query(&query, get_pm_item(item));

	msc_system_mon_alloc_info(SLC, &query, &mon_data);

	return scnprintf(page, PAGE_SIZE, "timestamp=%llu,cap_cnt=%u\n",
			mon_data.capacity.last_capture_time,
			mon_data.capacity.num_cache_lines);
}
CONFIGFS_ATTR_RO(slc_mpam_, cap_monitor_data);

static ssize_t slc_mpam_enable_miss_monitor_show(struct config_item *item,
		char *page)
{
	return scnprintf(page, PAGE_SIZE, "%s\n",
		(get_pm_item(item)->miss_mon_enabled) ? "enabled" : "disabled");
}

static ssize_t slc_mpam_enable_miss_monitor_store(struct config_item *item,
		const char *page, size_t count)
{
	int ret;
	bool input;
	struct msc_query query;
	struct slc_mon_config_val mon_cfg_val;
	struct slc_mpam_item *pm_item = get_pm_item(item);

	set_msc_query(&query, pm_item);

	ret = kstrtobool(page, &input);
	if (ret) {
		pr_err("invalid argument\n");
		return -EINVAL;
	}

	if (!input && pm_item->miss_mon_enabled)
		mon_cfg_val.enable = 0;
	else if (input && !pm_item->miss_mon_enabled)
		mon_cfg_val.enable = 1;
	else
		goto exit;

	mon_cfg_val.slc_mon_function = CACHE_READ_MISS_CONFIG;
	ret = msc_system_mon_config(SLC, &query, &mon_cfg_val);
	if (ret) {
		pr_err("monitor %s failed %d\n",
			(input) ? "enable" : "disable", ret);
		return -EBUSY;
	}
	pm_item->miss_mon_enabled = input;

exit:
	return count;
}
CONFIGFS_ATTR(slc_mpam_, enable_miss_monitor);

static ssize_t slc_mpam_miss_monitor_data_show(struct config_item *item,
		char *page)
{
	struct msc_query query;
	union mon_values mon_data;

	if (!get_pm_item(item)->miss_mon_enabled)
		return scnprintf(page, PAGE_SIZE, "monitor not enabled\n");

	set_msc_query(&query, get_pm_item(item));

	msc_system_mon_read_miss_info(SLC, &query, &mon_data);

	return scnprintf(page, PAGE_SIZE, "timestamp=%llu,miss_cnt=%llu\n",
			mon_data.misses.last_capture_time, mon_data.misses.num_rd_misses);
}
CONFIGFS_ATTR_RO(slc_mpam_, miss_monitor_data);

static ssize_t slc_mpam_available_gear_show(struct config_item *item,
		char *page)
{
	int i, ret, gear_num;
	ssize_t len = 0;
	struct msc_query query;
	struct slc_partid_capability slc_partid_cap;

	set_msc_query(&query, get_pm_item(item));

	ret = msc_system_get_device_capability(SLC, &query, &slc_partid_cap);
	if (ret)
		return scnprintf(page, PAGE_SIZE,
			"failed to get available gear %d\n", ret);

	for (i = 0; i < slc_partid_cap.num_gears; i++) {
		gear_num = slc_partid_cap.part_id_gears[i];
		len += scnprintf(page + len, PAGE_SIZE - len,
			"%d - %s\n", gear_num, gear_index[gear_num]);
	}

	return len;
}
CONFIGFS_ATTR_RO(slc_mpam_, available_gear);

static struct configfs_attribute *slc_mpam_attrs[] = {
	&slc_mpam_attr_schemata,
	&slc_mpam_attr_enable_cap_monitor,
	&slc_mpam_attr_cap_monitor_data,
	&slc_mpam_attr_enable_miss_monitor,
	&slc_mpam_attr_miss_monitor_data,
	&slc_mpam_attr_available_gear,
	NULL,
};

static const struct config_item_type slc_mpam_item_type = {
	.ct_attrs	= slc_mpam_attrs,
};

static struct slc_mpam_item *slc_mpam_make_group(
		struct device *dev, const char *name)
{
	struct slc_mpam_item *item;

	item = devm_kzalloc(dev, sizeof(struct slc_mpam_item), GFP_KERNEL);
	if (!item)
		return ERR_PTR(-ENOMEM);

	config_group_init_type_name(&item->group, name,
				   &slc_mpam_item_type);

	return item;
}

static const struct config_item_type slc_mpam_base_type = {
	.ct_owner	= THIS_MODULE,
};

static int create_config_node(const char *name,
		struct device *dev,
		int client_id, int part_id,
		struct config_group *parent_group)
{
	int ret;
	struct slc_mpam_item *new_item;

	new_item = slc_mpam_make_group(dev, name);
	if (IS_ERR(new_item)) {
		pr_err("Error create group %s\n", name);
		return PTR_ERR(new_item);
	}
	new_item->client_id = client_id;
	new_item->part_id = part_id;

	ret = configfs_register_group(parent_group, &new_item->group);
	if (ret) {
		pr_err("Error register group %s\n", name);
		return ret;
	}

	return 0;
}

static int slc_mpam_probe(struct platform_device *pdev)
{
	int ret, clientid, partid;
	char buf[CONFIGFS_ITEM_NAME_LEN];
	int client_cnt;
	const char *msc_name_dt;
	struct qcom_mpam_msc *qcom_mpam_msc;
	struct device_node *node, *sub_node;
	struct config_group *p_group, *sub_group;
	struct device_node *np = pdev->dev.of_node;

	qcom_mpam_msc = qcom_msc_lookup(SLC);
	if (!qcom_mpam_msc ||
		qcom_mpam_msc->mpam_available != MPAM_MONITRS_AVAILABLE)
		return -EPROBE_DEFER;

	p_group = platform_mpam_get_root_group();
	if (!p_group)
		return -EPROBE_DEFER;

	client_cnt = of_get_child_count(np);
	if (!client_cnt) {
		dev_err(&pdev->dev, "No client found\n");
		return -ENODEV;
	}

	of_property_read_string(np, "qcom,msc-name", &msc_name_dt);
	root_group = configfs_register_default_group(p_group,
		msc_name_dt, &slc_mpam_base_type);
	if (IS_ERR(root_group)) {
		dev_err(&pdev->dev, "Error register group %s\n", msc_name_dt);
		return PTR_ERR(root_group);
	}

	for_each_child_of_node(np, node) {
		ret = of_property_read_u32(node, "qcom,client-id", &clientid);
		of_property_read_string(node, "qcom,client-name", &msc_name_dt);
		if (ret || IS_ERR_OR_NULL(msc_name_dt))
			continue;

		if (of_get_child_count(node) > 0) {
			sub_group = configfs_register_default_group(root_group,
				msc_name_dt, &slc_mpam_base_type);
			for_each_child_of_node(node, sub_node) {
				ret = of_property_read_u32(sub_node, "qcom,part-id", &partid);
				snprintf(buf, sizeof(buf), "partid%d", partid);
				if (create_config_node(buf, &pdev->dev, clientid,
						partid, sub_group))
					continue;
			}
		} else
			if (create_config_node(msc_name_dt, &pdev->dev,
					clientid, 0, root_group))
				continue;
	}

	return 0;
}

int slc_mpam_remove(struct platform_device *pdev)
{
	configfs_unregister_group(root_group);
	kfree(root_group);
	return 0;
}

static const struct of_device_id slc_mpam_table[] = {
	{ .compatible = "qcom,mpam-slc" },
	{}
};
MODULE_DEVICE_TABLE(of, slc_mpam_table);

static struct platform_driver slc_mpam_driver = {
	.driver = {
		.name = "mpam-slc",
		.of_match_table = slc_mpam_table,
		.suppress_bind_attrs = true,
	},
	.probe = slc_mpam_probe,
	.remove = slc_mpam_remove,
};

module_platform_driver(slc_mpam_driver);

MODULE_SOFTDEP("pre: mpam");
MODULE_DESCRIPTION("QCOM SLC MPAM driver");
MODULE_LICENSE("GPL");
