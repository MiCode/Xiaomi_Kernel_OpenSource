// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define SPRD_BCL_LEVEL_COLS_MAX			20
#define SPRD_BCL_BAT_NAME			"battery"
#define SPRD_BCL_FGU_NAME			"sc27xx-fgu"

struct sprd_bcl_level_thres_table {
	int soc_thres_cols;
	int soc_thres[SPRD_BCL_LEVEL_COLS_MAX];
	int vbat_thres_cols;
	int vbat_thres[SPRD_BCL_LEVEL_COLS_MAX];
};

struct sprd_bcl_device {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct thermal_zone_device_ops ops;
	struct power_supply *bcl_psy;
	struct notifier_block psy_nb;
	struct work_struct notify_thermald_work;
	int bcl_level;
	int debug_bcl_level;
	int bcl_soc_thres_offset;
	int bcl_vbat_thres_offset;
	struct sprd_bcl_sysfs *sysfs;
	struct sprd_bcl_level_thres_table *bcl_level_thres_table;
};

static struct sprd_bcl_device *bcl_data;

struct sprd_bcl_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sprd_bcl_soc_thres_offset;
	struct device_attribute attr_sprd_bcl_vbat_thres_offset;
	struct attribute *attrs[3];
	struct sprd_bcl_device *data;
};

static int sprd_bcl_parse_level_thres_table(struct sprd_bcl_device *data,
					    int *thres_array,
					    const char *propname)
{
	struct device_node *np = data->dev->of_node;
	int len;

	len = of_property_count_u32_elems(np, propname);
	if (len < 0) {
		dev_err(data->dev, "the thres table does not exist.\n");
	} else if (len > SPRD_BCL_LEVEL_COLS_MAX) {
		dev_err(data->dev, "too many cycles values.\n");
		len = -EINVAL;
	} else if (len > 0) {
		of_property_read_u32_array(np, propname, thres_array, len);
	}

	return len;
}

static int sprd_bcl_set_level(struct thermal_zone_device *tz_dev, int level)
{
	if (level == bcl_data->debug_bcl_level || level < 0)
		return 0;

	bcl_data->debug_bcl_level = level;
	power_supply_changed(bcl_data->bcl_psy);
	dev_info(bcl_data->dev, "%s debug_bcl_level = %d.\n", __func__, level);

	return 0;
}

static int sprd_bcl_get_level(struct thermal_zone_device *tz_dev, int *val)
{
	if (bcl_data->debug_bcl_level >= SPRD_BCL_LEVEL_COLS_MAX) {
		*val = bcl_data->bcl_level;
		dev_info(bcl_data->dev, "%s bcl_level = %d.\n", __func__, *val);
	} else {
		*val = bcl_data->debug_bcl_level;
		dev_info(bcl_data->dev, "%s debug_bcl_level = %d.\n", __func__, *val);
	}

	return 0;
}

static int sprd_bcl_get_bat_property(struct sprd_bcl_device *data,
				     enum power_supply_property psp, int *value)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_name(SPRD_BCL_BAT_NAME);
	if (!psy) {
		dev_err(data->dev, "failed to get battery power supply");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy, psp, &val);
	power_supply_put(psy);
	if (ret)
		return ret;

	*value = val.intval;
	return ret;
}

static int sprd_bcl_get_thres_table_idx(struct sprd_bcl_device *data,
					int value, int *table, int cols, int offset)
{
	int index = 0, i;

	if (value > table[0] + offset)
		return 0;

	if (value <= table[cols - 1] + offset)
		return cols;

	for (i = 1; i < cols; i++) {
		if (value > table[i] + offset) {
			index = i;
			break;
		}
	}

	return index;
}

static int sprd_bcl_judge_bcl_level(struct sprd_bcl_device *data, int soc, int vbat_mv)
{
	int bcl_level, soc_level = 0, vbat_level = 0;

	if (data->bcl_level_thres_table->soc_thres_cols > 0)
		soc_level =
			sprd_bcl_get_thres_table_idx(data, soc,
						     data->bcl_level_thres_table->soc_thres,
						     data->bcl_level_thres_table->soc_thres_cols,
						     data->bcl_soc_thres_offset);
	if (data->bcl_level_thres_table->vbat_thres_cols > 0)
		vbat_level =
			sprd_bcl_get_thres_table_idx(data, vbat_mv,
						     data->bcl_level_thres_table->vbat_thres,
						     data->bcl_level_thres_table->vbat_thres_cols,
						     data->bcl_vbat_thres_offset);

	dev_info(data->dev, "soc_level = %d, vbat_level = %d\n", soc_level, vbat_level);

	bcl_level = max(soc_level, vbat_level);

	return bcl_level;
}

static void sprd_bcl_notify_thermald_work(struct work_struct *work)
{
	struct sprd_bcl_device *data = container_of(work, struct sprd_bcl_device,
						    notify_thermald_work);
	int ret, bat_soc, vbat_uv, bcl_level;
	static int last_bcl_level;

	ret = sprd_bcl_get_bat_property(data, POWER_SUPPLY_PROP_CAPACITY, &bat_soc);
	if (ret) {
		dev_err(data->dev, "failed to get bat_soc!");
		return;
	}

	ret = sprd_bcl_get_bat_property(data, POWER_SUPPLY_PROP_VOLTAGE_AVG, &vbat_uv);
	if (ret) {
		dev_err(data->dev, "failed to get vbat_uv!");
		return;
	}

	bcl_level = sprd_bcl_judge_bcl_level(data, bat_soc, vbat_uv / 1000);
	if (bcl_level != last_bcl_level) {
		dev_info(data->dev, "%s %d bat_soc = %d, vbat_uv = %d, last_bcl_level = %d, bcl_level = %d.\n",
			 __func__, __LINE__, bat_soc, vbat_uv, last_bcl_level, bcl_level);
		bcl_data->bcl_level = bcl_level;
		last_bcl_level = bcl_level;
		power_supply_changed(bcl_data->bcl_psy);
	}
}

static int sprd_bcl_callback(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct sprd_bcl_device *bcl_data = container_of(nb, struct sprd_bcl_device, psy_nb);

	if (strcmp(psy->desc->name, SPRD_BCL_BAT_NAME) &&
	    strcmp(psy->desc->name, SPRD_BCL_FGU_NAME))
		return NOTIFY_OK;

	schedule_work(&bcl_data->notify_thermald_work);

	return NOTIFY_OK;
}

static int sprd_bcl_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
		if (bcl_data->debug_bcl_level >= SPRD_BCL_LEVEL_COLS_MAX)
			val->intval = bcl_data->bcl_level;
		else
			val->intval = bcl_data->debug_bcl_level;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property default_bcl_props[] = {
	POWER_SUPPLY_PROP_TEMP,
};

static const struct power_supply_desc sprd_bcl_desc = {
	.name = "sprd_bcl",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = default_bcl_props,
	.num_properties = ARRAY_SIZE(default_bcl_props),
	.get_property = sprd_bcl_get_property,
	.no_thermal = true,
};

static ssize_t sprd_bcl_soc_thres_offset_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct sprd_bcl_sysfs *sysfs =
		container_of(attr, struct sprd_bcl_sysfs,
			     attr_sprd_bcl_soc_thres_offset);
	struct sprd_bcl_device *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sprd_bcl_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_bcl_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[bcl_soc_thres_offset:%d]\n",
			bcl_data->bcl_soc_thres_offset);
}

static ssize_t sprd_bcl_soc_thres_offset_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct sprd_bcl_sysfs *sysfs =
		container_of(attr, struct sprd_bcl_sysfs,
			     attr_sprd_bcl_soc_thres_offset);
	struct sprd_bcl_device *data = sysfs->data;
	u32 bcl_soc_thres_offset;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_bcl_data is null\n", __func__);
		return count;
	}

	ret = kstrtouint(buf, 10, &bcl_soc_thres_offset);
	if (ret) {
		dev_err(data->dev, "fail to get bcl_soc_thres_offset, ret = %d\n", ret);
		return count;
	}

	if (bcl_soc_thres_offset != data->bcl_soc_thres_offset) {
		data->bcl_soc_thres_offset = bcl_soc_thres_offset;
		schedule_work(&bcl_data->notify_thermald_work);
		dev_info(data->dev, "Try to set [bcl_soc_thres_offset] to [%d]\n",
			 bcl_soc_thres_offset);
	}

	return count;
}

static ssize_t sprd_bcl_vbat_thres_offset_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct sprd_bcl_sysfs *sysfs =
		container_of(attr, struct sprd_bcl_sysfs,
			     attr_sprd_bcl_vbat_thres_offset);
	struct sprd_bcl_device *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sprd_bcl_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_bcl_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[bcl_vbat_thres_offset:%d]\n",
			bcl_data->bcl_vbat_thres_offset);
}

static ssize_t sprd_bcl_vbat_thres_offset_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct sprd_bcl_sysfs *sysfs =
		container_of(attr, struct sprd_bcl_sysfs,
			     attr_sprd_bcl_vbat_thres_offset);
	struct sprd_bcl_device *data = sysfs->data;
	u32 bcl_vbat_thres_offset;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_bcl_data is null\n", __func__);
		return count;
	}

	ret = kstrtouint(buf, 10, &bcl_vbat_thres_offset);
	if (ret) {
		dev_err(data->dev, "fail to get bcl_vbat_thres_offset, ret = %d\n", ret);
		return count;
	}

	if (bcl_vbat_thres_offset != data->bcl_vbat_thres_offset) {
		data->bcl_vbat_thres_offset = bcl_vbat_thres_offset;
		schedule_work(&bcl_data->notify_thermald_work);
		dev_info(data->dev, "Try to set [bcl_vbat_thres_offset] to [%d]\n",
			 bcl_vbat_thres_offset);
	}

	return count;
}

static int sprd_bcl_register_sysfs(struct sprd_bcl_device *data)
{
	struct sprd_bcl_sysfs *sysfs;
	int ret;

	sysfs = devm_kzalloc(data->dev, sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs)
		return -ENOMEM;

	data->sysfs = sysfs;
	sysfs->data = data;
	sysfs->name = "sprd_bcl_sysfs";
	sysfs->attrs[0] = &sysfs->attr_sprd_bcl_soc_thres_offset.attr;
	sysfs->attrs[1] = &sysfs->attr_sprd_bcl_vbat_thres_offset.attr;
	sysfs->attrs[2] = NULL;
	sysfs->attr_g.name = "debug";
	sysfs->attr_g.attrs = sysfs->attrs;

	sysfs_attr_init(&sysfs->attr_sprd_bcl_soc_thres_offset.attr);
	sysfs->attr_sprd_bcl_soc_thres_offset.attr.name = "bcl_soc_thres_offset";
	sysfs->attr_sprd_bcl_soc_thres_offset.attr.mode = 0644;
	sysfs->attr_sprd_bcl_soc_thres_offset.show = sprd_bcl_soc_thres_offset_show;
	sysfs->attr_sprd_bcl_soc_thres_offset.store = sprd_bcl_soc_thres_offset_store;

	sysfs_attr_init(&sysfs->attr_sprd_bcl_vbat_thres_offset.attr);
	sysfs->attr_sprd_bcl_vbat_thres_offset.attr.name = "bcl_vbat_thres_offset";
	sysfs->attr_sprd_bcl_vbat_thres_offset.attr.mode = 0644;
	sysfs->attr_sprd_bcl_vbat_thres_offset.show = sprd_bcl_vbat_thres_offset_show;
	sysfs->attr_sprd_bcl_vbat_thres_offset.store = sprd_bcl_vbat_thres_offset_store;

	ret = sysfs_create_group(&data->dev->kobj, &sysfs->attr_g);
	if (ret < 0)
		dev_err(data->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int sprd_bcl_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct power_supply_config sprd_bcl_cfg = { };

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}

	bcl_data = devm_kzalloc(&pdev->dev, sizeof(*bcl_data), GFP_KERNEL);
	if (!bcl_data)
		return -ENOMEM;

	bcl_data->bcl_level = 0;
	bcl_data->debug_bcl_level = SPRD_BCL_LEVEL_COLS_MAX;
	bcl_data->bcl_soc_thres_offset = 0;
	bcl_data->bcl_vbat_thres_offset = 0;

	sprd_bcl_cfg.drv_data = bcl_data;
	sprd_bcl_cfg.of_node = np;
	bcl_data->bcl_psy = devm_power_supply_register(dev, &sprd_bcl_desc, &sprd_bcl_cfg);
	if (IS_ERR(bcl_data->bcl_psy)) {
		dev_err(dev, "failed to register power supply");
		return -ENXIO;
	}

	bcl_data->ops.get_temp = sprd_bcl_get_level;
	bcl_data->ops.set_emul_temp = sprd_bcl_set_level;
	platform_set_drvdata(pdev, bcl_data);
	bcl_data->dev = dev;
	bcl_data->tz_dev = thermal_zone_device_register("bcl-thmzone", 0, 0,
							bcl_data, &bcl_data->ops, NULL, 0, 0);
	if (IS_ERR(bcl_data->tz_dev))
		return PTR_ERR(bcl_data->tz_dev);

	bcl_data->bcl_level_thres_table =
		devm_kzalloc(bcl_data->dev, sizeof(struct sprd_bcl_level_thres_table), GFP_KERNEL);
	bcl_data->bcl_level_thres_table->soc_thres_cols =
		sprd_bcl_parse_level_thres_table(bcl_data,
						 bcl_data->bcl_level_thres_table->soc_thres,
						 "bcl-soc-thres");
	if (bcl_data->bcl_level_thres_table->soc_thres_cols < 0)
		dev_err(&pdev->dev, "%s soc_thres_table is not define!!!\n", __func__);
	bcl_data->bcl_level_thres_table->vbat_thres_cols =
		sprd_bcl_parse_level_thres_table(bcl_data,
						 bcl_data->bcl_level_thres_table->vbat_thres,
						 "bcl-vbat-thres");
	if (bcl_data->bcl_level_thres_table->vbat_thres_cols < 0)
		dev_err(&pdev->dev, "%s vbat_thres_table is not define!!!\n", __func__);

	INIT_WORK(&bcl_data->notify_thermald_work, sprd_bcl_notify_thermald_work);

	bcl_data->psy_nb.notifier_call = sprd_bcl_callback;
	ret = power_supply_reg_notifier(&bcl_data->psy_nb);
	if (ret) {
		dev_err(dev, "%s, failed to register notifier, ret = %d\n", __func__, ret);
		thermal_zone_device_unregister(bcl_data->tz_dev);
		return -EPROBE_DEFER;
	}

	ret = sprd_bcl_register_sysfs(bcl_data);
	if (ret)
		dev_err(&pdev->dev, "register sysfs fail, ret = %d\n", ret);

	dev_info(&pdev->dev, "sprd_bcl probe success!!!\n");

	return 0;
}

static int sprd_bcl_remove(struct platform_device *pdev)
{
	if (!IS_ERR_OR_NULL(bcl_data->tz_dev))
		thermal_zone_device_unregister(bcl_data->tz_dev);
	if (work_pending(&bcl_data->notify_thermald_work))
		cancel_work_sync(&bcl_data->notify_thermald_work);

	return 0;
}

static const struct of_device_id sprd_bcl_of_match[] = {
	{.compatible = "sprd,bcl"},
	{},
};

static struct platform_driver sprd_bcl_driver = {
	.probe = sprd_bcl_probe,
	.remove = sprd_bcl_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_bcl",
		.of_match_table = sprd_bcl_of_match,
	},
};

module_platform_driver(sprd_bcl_driver);
MODULE_LICENSE("GPL");
