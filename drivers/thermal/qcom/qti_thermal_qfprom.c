// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/nvmem-consumer.h>
#include <linux/thermal.h>

#include "../thermal_core.h"

static int thermal_qfprom_read(struct platform_device *pdev,
			const char *cname, unsigned int *efuse_val)
{
	struct nvmem_cell *cell;
	size_t len;
	char *buf;

	cell = nvmem_cell_get(&pdev->dev, cname);
	if (IS_ERR(cell)) {
		dev_err(&pdev->dev, "failed to get nvmem cell %s\n", cname);
		return -EINVAL;
	}

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(&pdev->dev, "failed to read nvmem cell %s\n", cname);
		return -EINVAL;
	}

	if (len <= 0 || len > sizeof(u32)) {
		dev_err(&pdev->dev, "nvmem cell length out of range:%d\n", len);
		kfree(buf);
		return -EINVAL;
	}
	memcpy(efuse_val, buf, min(len, sizeof(*efuse_val)));
	kfree(buf);

	return 0;
}

static int thermal_zone_set_mode(struct platform_device *pdev,
			enum thermal_device_mode mode)
{
	const char *name;
	struct property *prop = NULL;

	of_property_for_each_string(pdev->dev.of_node,
		mode == THERMAL_DEVICE_ENABLED ?
		"qcom,thermal-zone-enable-list" :
		"qcom,thermal-zone-disable-list", prop, name) {
		struct thermal_zone_device *zone;
		struct thermal_instance *pos;

		zone = thermal_zone_get_zone_by_name(name);
		if (IS_ERR(zone)) {
			dev_err(&pdev->dev,
				"could not find %s thermal zone\n", name);
			continue;
		}

		if (!(zone->ops && zone->ops->set_mode)) {
			dev_err(&pdev->dev,
				"thermal zone ops is not supported for %s\n",
				name);
			continue;
		}

		zone->ops->set_mode(zone, mode);
		if (mode == THERMAL_DEVICE_DISABLED) {
			/* Clear thermal zone device */
			mutex_lock(&zone->lock);
			zone->temperature = THERMAL_TEMP_INVALID;
			zone->passive = 0;
			list_for_each_entry(pos, &zone->thermal_instances,
				tz_node) {
				pos->initialized = false;
				pos->target = THERMAL_NO_TARGET;
				mutex_lock(&pos->cdev->lock);
				pos->cdev->updated = false;
				mutex_unlock(&pos->cdev->lock);
				thermal_cdev_update(pos->cdev);
			}
			mutex_unlock(&zone->lock);
		}
		dev_dbg(&pdev->dev, "thermal zone %s is %s\n", name,
			mode == THERMAL_DEVICE_ENABLED ?
			"enabled" : "disabled");
	}

	return 0;
}

static void update_thermal_zones(struct platform_device *pdev)
{
	thermal_zone_set_mode(pdev, THERMAL_DEVICE_ENABLED);
	thermal_zone_set_mode(pdev, THERMAL_DEVICE_DISABLED);
}

static int thermal_qfprom_probe(struct platform_device *pdev)
{
	int err = 0;
	const char *name;
	struct property *prop = NULL;
	u8 efuse_pass_cnt = 0;

	of_property_for_each_string(pdev->dev.of_node,
		"nvmem-cell-names", prop, name) {
		u32 efuse_val = 0, efuse_match_val = 0;

		err = thermal_qfprom_read(pdev, name, &efuse_val);
		if (err)
			return err;

		err = of_property_read_u32_index(pdev->dev.of_node,
			"qcom,thermal-qfprom-bit-values", efuse_pass_cnt,
			&efuse_match_val);
		if (err) {
			dev_err(&pdev->dev,
				"Invalid qfprom bit value for index %d\n",
				efuse_pass_cnt);
			return err;
		}

		dev_dbg(&pdev->dev, "efuse[%s] val:0x%x match val[%d]:0x%x\n",
				name, efuse_val, efuse_pass_cnt,
				efuse_match_val);

		/* if any of efuse condition fails, just exit */
		if (efuse_val != efuse_match_val)
			return 0;

		efuse_pass_cnt++;
	}

	if (efuse_pass_cnt)
		update_thermal_zones(pdev);

	return err;
}

static const struct of_device_id thermal_qfprom_match[] = {
	{ .compatible = "qcom,thermal-qfprom-device", },
	{},
};

static struct platform_driver thermal_qfprom_driver = {
	.probe = thermal_qfprom_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = thermal_qfprom_match,
	},
};

int __init thermal_qfprom_init(void)
{
	int err;

	err = platform_driver_register(&thermal_qfprom_driver);
	if (err)
		pr_err("Failed to register thermal qfprom platform driver:%d\n",
			err);
	return err;
}

late_initcall(thermal_qfprom_init);
