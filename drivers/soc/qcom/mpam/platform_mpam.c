// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
 #define pr_fmt(fmt) "platform_mpam: " fmt

#include <linux/io.h>
#include <linux/of.h>
#include <linux/configfs.h>
#include <linux/string.h>
#include <soc/qcom/mpam.h>

#define platform_mpam_get(ptr, member) ({	\
	(mpam_version.version >= 0x10002) ?	\
	ptr->V2.member : ptr->V1.member; })

struct platform_mpam_item {
	struct config_group group;
	int msc_id;
	int client_id;
	int monitor_index;
	bool monitor_enabled;
	struct platform_mpam_bw_ctrl_config *cfg;
};

struct platform_mpam_gear {
	int gear_id;
	const char *gear_name;
};

static int support_gear_cnt;
static struct mpam_ver_ret mpam_version;
static struct config_group *root_group;
static struct platform_mpam_gear *support_gears;
static void *mpam_mon_base;

static inline struct platform_mpam_item *get_pm_item(
					   struct config_item *item)
{
	return container_of(to_config_group(item),
				struct platform_mpam_item, group);
}

static ssize_t platform_mpam_schemata_show(struct config_item *item,
		char *page)
{
	return scnprintf(page, PAGE_SIZE, "gear=%d\n",
		get_pm_item(item)->cfg->platform_mpam_gear);
}

static ssize_t platform_mpam_schemata_store(struct config_item *item,
		const char *page, size_t count)
{
	int ret, input;
	char *token, *param_name;
	struct platform_mpam_bw_ctrl_cfg cfg;
	struct platform_mpam_item *pm_item = get_pm_item(item);

	cfg.msc_id = pm_item->msc_id;
	cfg.client_id = pm_item->client_id;
	cfg.config_ctrl = 0;

	while ((token = strsep((char **)&page, ",")) != NULL) {
		param_name = strsep(&token, "=");
		if (param_name == NULL || token == NULL)
			continue;
		if (kstrtouint(token, 0, &input) < 0) {
			pr_err("invalid argument for %s\n", param_name);
			continue;
		}

		if (!strcmp("gear", param_name))
			cfg.platform_mpam_gear = input;
	}

	ret = qcom_mpam_set_platform_bw_ctrl(&cfg);
	if (!ret)
		pm_item->cfg->platform_mpam_gear = cfg.platform_mpam_gear;
	else
		pr_err("set platform bw ctrl failed, ret=%d\n", ret);

	return count;
}
CONFIGFS_ATTR(platform_mpam_, schemata);

static ssize_t platform_mpam_enable_monitor_show(struct config_item *item,
		char *page)
{
	return scnprintf(page, PAGE_SIZE, "%s\n",
		(get_pm_item(item)->monitor_enabled) ? "enabled" : "disabled");
}

static ssize_t platform_mpam_enable_monitor_store(struct config_item *item,
		const char *page, size_t count)
{
	int ret;
	bool input;
	struct platform_mpam_bw_monitor_cfg monitor_param;
	struct platform_mpam_item *pm_item = get_pm_item(item);

	monitor_param.msc_id = pm_item->msc_id;
	monitor_param.client_id = pm_item->client_id;

	ret = kstrtobool(page, &input);
	if (ret) {
		pr_err("invalid argument\n");
		goto exit;
	}

	if (!input && pm_item->monitor_enabled)
		monitor_param.config_ctrl = 0;
	else if (input && !pm_item->monitor_enabled)
		monitor_param.config_ctrl = 1;
	else
		goto exit;

	ret = qcom_mpam_set_platform_bw_monitor(&monitor_param);
	if (ret) {
		pr_err("monitor %s failed %d\n",
			(input) ? "enable" : "disable", ret);
		goto exit;
	}
	pm_item->monitor_enabled = input;

exit:
	return count;
}
CONFIGFS_ATTR(platform_mpam_, enable_monitor);

static ssize_t platform_mpam_monitor_data_show(struct config_item *item,
		char *page)
{
	int index, retry_cnt = 0, match_seq_cnt = 0;
	uint64_t byte_cnt, timestamp, capture_status;
	union platform_monitor_value data, *pdata;

	if (get_pm_item(item)->monitor_enabled) {
		index = get_pm_item(item)->monitor_index;
		pdata = mpam_mon_base + index * ((mpam_version.version >= 0x10002) ?
				sizeof(data.V2) : sizeof(data.V1));
		do {
			while (unlikely((capture_status = platform_mpam_get(
					pdata, capture_status)) % 2) &&
					(retry_cnt < MPAM_MAX_RETRY))
				retry_cnt++;
			timestamp = platform_mpam_get(
				pdata, last_capture_time);
			byte_cnt = platform_mpam_get(
				pdata, bwmon_byte_count);
		} while ((capture_status !=
			platform_mpam_get(pdata, capture_status)) &&
			(match_seq_cnt++ < MPAM_MAX_MATCH_SEQ_RETRY));

		if (match_seq_cnt == MPAM_MAX_MATCH_SEQ_RETRY)
			return scnprintf(page, PAGE_SIZE, "get monitor data failed\n");

		return scnprintf(page, PAGE_SIZE, "timestamp=%llu,byte_cnt=%llu\n",
			timestamp, byte_cnt);
	} else
		return scnprintf(page, PAGE_SIZE, "monitor not enabled\n");
}
CONFIGFS_ATTR_RO(platform_mpam_, monitor_data);

static ssize_t platform_mpam_available_gear_show(struct config_item *item,
		char *page)
{
	int i;
	size_t len = 0;

	for (i = 0; i < support_gear_cnt; i++) {
		len += scnprintf(page + len, PAGE_SIZE - len,
			"%d - %s\n", support_gears[i].gear_id,
			support_gears[i].gear_name);
	}

	return len;
}
CONFIGFS_ATTR_RO(platform_mpam_, available_gear);

static struct configfs_attribute *platform_mpam_attrs[] = {
	&platform_mpam_attr_schemata,
	&platform_mpam_attr_enable_monitor,
	&platform_mpam_attr_monitor_data,
	&platform_mpam_attr_available_gear,
	NULL,
};

static const struct config_item_type platform_mpam_item_type = {
	.ct_attrs	= platform_mpam_attrs,
};

static struct platform_mpam_item *platform_mpam_make_group(
		struct device *dev, const char *name)
{
	struct platform_mpam_item *item;

	item = devm_kzalloc(dev,
		sizeof(struct platform_mpam_item), GFP_KERNEL);
	if (!item)
		return ERR_PTR(-ENOMEM);

	item->cfg = devm_kzalloc(dev,
		sizeof(struct platform_mpam_bw_ctrl_config), GFP_KERNEL);
	if (!item->cfg)
		return ERR_PTR(-ENOMEM);

	config_group_init_type_name(&item->group, name,
				   &platform_mpam_item_type);

	return item;
}

static const struct config_item_type platform_mpam_base_type = {
	.ct_owner	= THIS_MODULE,
};

static struct configfs_subsystem platform_mpam_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "platform_mpam",
			.ci_type = &platform_mpam_base_type,
		},
	},
};

static int platform_mpam_probe(struct platform_device *pdev)
{
	int i, ret, mscid, clientid;
	int client_cnt;
	const char *msc_name_dt;
	struct resource *res;
	struct device_node *node;
	struct config_group *p_group;
	struct platform_mpam_item *new_item;
	struct platform_mpam_read_bw_ctrl bw_ctrl_param;
	struct device_node *np = pdev->dev.of_node;

	ret = qcom_mpam_get_version(&mpam_version);
	if (ret || mpam_version.version < 0x10000) {
		dev_err(&pdev->dev, "Platform MPAM is not available\n");
		return -ENODEV;
	}

	config_group_init(&platform_mpam_subsys.su_group);
	mutex_init(&platform_mpam_subsys.su_mutex);

	ret = configfs_register_subsystem(&platform_mpam_subsys);
	if (ret) {
		mutex_destroy(&platform_mpam_subsys.su_mutex);
		pr_err("Error while registering subsystem %d\n", ret);
		return ret;
	}

	client_cnt = of_get_child_count(np);
	if (!client_cnt) {
		dev_err(&pdev->dev, "No client found\n");
		return -ENODEV;
	}

	support_gear_cnt = of_property_count_strings(np, "qcom,gears");
	if (!support_gear_cnt) {
		dev_err(&pdev->dev, "No available gears found\n");
		return -ENODEV;
	}

	support_gears = devm_kcalloc(&pdev->dev, support_gear_cnt,
		sizeof(struct platform_mpam_gear), GFP_KERNEL);
	if (!support_gears)
		return -ENOMEM;

	for (i = 0; i < support_gear_cnt; i++) {
		of_property_read_string_index(np, "qcom,gears", i,
				&support_gears[i].gear_name);
		of_property_read_u32_index(np, "qcom,gear-id", i,
				&support_gears[i].gear_id);
	}

	ret = of_property_read_u32(np, "qcom,msc-id", &mscid);
	of_property_read_string(np, "qcom,msc-name", &msc_name_dt);
	if (ret || mscid >= MSC_MAX || IS_ERR_OR_NULL(msc_name_dt))
		return -ENODEV;

	root_group = &platform_mpam_subsys.su_group;
	p_group = configfs_register_default_group(root_group,
		msc_name_dt, &platform_mpam_base_type);
	if (IS_ERR(p_group)) {
		dev_err(&pdev->dev, "Error register group %s\n", msc_name_dt);
		return PTR_ERR(p_group);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mon-base");
	mpam_mon_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(mpam_mon_base)) {
		dev_err(&pdev->dev, "Error ioremap mpam_mon_base\n");
		return -ENODEV;
	}

	i = 0;
	for_each_child_of_node(np, node) {
		ret = of_property_read_u32(node, "qcom,client-id", &clientid);
		of_property_read_string(node, "qcom,client-name", &msc_name_dt);
		if (ret || IS_ERR_OR_NULL(msc_name_dt))
			continue;

		new_item = platform_mpam_make_group(&pdev->dev, msc_name_dt);
		if (IS_ERR(new_item)) {
			pr_err("Error create group %s\n", msc_name_dt);
			continue;
		}
		new_item->monitor_index = i;
		new_item->msc_id = mscid;
		new_item->client_id = clientid;

		bw_ctrl_param.msc_id = mscid;
		bw_ctrl_param.client_id = clientid;
		ret = qcom_mpam_get_platform_bw_ctrl(&bw_ctrl_param, new_item->cfg);
		if (ret) {
			pr_err("Error get bw_ctrl\n");
			continue;
		}

		ret = configfs_register_group(p_group, &new_item->group);
		if (ret) {
			pr_err("Error register group %s\n", msc_name_dt);
			continue;
		}

		i++;
	}

	return 0;
}

static int platform_mpam_remove(struct platform_device *pdev)
{
	configfs_unregister_subsystem(&platform_mpam_subsys);
	return 0;
}

struct config_group *platform_mpam_get_root_group(void)
{
	return root_group;
}
EXPORT_SYMBOL_GPL(platform_mpam_get_root_group);

static const struct of_device_id platform_mpam_table[] = {
	{ .compatible = "qcom,platform-mpam" },
	{}
};
MODULE_DEVICE_TABLE(of, platform_mpam_table);

static struct platform_driver platform_mpam_driver = {
	.driver = {
		.name = "platform-mpam",
		.of_match_table = platform_mpam_table,
		.suppress_bind_attrs = true,
	},
	.probe = platform_mpam_probe,
	.remove = platform_mpam_remove,
};

module_platform_driver(platform_mpam_driver);

MODULE_SOFTDEP("pre: mpam");
MODULE_DESCRIPTION("QCOM Platform MPAM driver");
MODULE_LICENSE("GPL");
