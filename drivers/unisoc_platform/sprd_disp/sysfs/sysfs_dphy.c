// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "disp_lib.h"
#include "sprd_dphy.h"
#include "sprd_dsi.h"
#include "sysfs_display.h"
#include "../dphy/sprd_dphy_api.h"

struct dphy_sysfs {
	int hop_freq;
	int ssc_en;
	int ulps_en;
	u32 input_param[64];
	u32 read_buf[64];
};

static struct dphy_sysfs *sysfs;

static ssize_t reg_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	struct regmap *regmap = ctx->regmap;
	int i;
	u32 reg;
	u8 reg_stride;

	if (!regmap)
		return -ENODEV;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized.\n");
		return -ENXIO;
	}

	str_to_u32_array(buf, 0, sysfs->input_param, 64);

	reg_stride = regmap_get_reg_stride(regmap);

	for (i = 0; i < (sysfs->input_param[1] ? : 1); i++) {
		if (i >= sizeof(sysfs->read_buf) / 4) {
			pr_err("%s() read data is overwrite read buf, i = %d\n", __func__, i);
			break;
		}
		reg = sysfs->input_param[0] + i * reg_stride;
		regmap_read(regmap, reg, &sysfs->read_buf[i]);
	}
	mutex_unlock(&dphy->ctx.lock);

	return count;
}

static ssize_t reg_read_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	struct regmap *regmap = ctx->regmap;
	const char *fmt = NULL;
	int i;
	int ret = 0;
	u8 val_width;
	u8 reg_stride;

	if (!regmap)
		return -ENODEV;

	val_width = regmap_get_val_bytes(regmap);
	reg_stride = regmap_get_reg_stride(regmap);

	if (val_width == 1) {
		ret += snprintf(buf + ret, PAGE_SIZE,
				" ADDR | VALUE\n");
		ret += snprintf(buf + ret, PAGE_SIZE,
				"------+------\n");
		fmt = " 0x%02x | 0x%02x\n";
	} else if (val_width == 4) {
		ret += snprintf(buf + ret, PAGE_SIZE,
				"  ADDRESS  |   VALUE\n");
		ret += snprintf(buf + ret, PAGE_SIZE,
				"-----------+-----------\n");
		fmt = "0x%08x | 0x%08x\n";
	} else
		return -ENODEV;

	for (i = 0; i < (sysfs->input_param[1] ? : 1); i++) {
		if (i >= sizeof(sysfs->read_buf) / 4) {
			pr_err("%s() read data is overwrite read buf, i = %d\n", __func__, i);
			break;
		}
		ret += snprintf(buf + ret, PAGE_SIZE, fmt,
				sysfs->input_param[0] + i * reg_stride,
				sysfs->read_buf[i]);
	}

	return ret;
}
static DEVICE_ATTR_RW(reg_read);

static ssize_t reg_write_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	struct regmap *regmap = ctx->regmap;
	int i, len;
	u8 reg_stride;
	u32 reg, val;

	if (!regmap)
		return -ENODEV;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	len = str_to_u32_array(buf, 16, sysfs->input_param, 64);
	reg_stride = regmap_get_reg_stride(regmap);

	for (i = 1; i < len; i++) {
		val = sysfs->input_param[i];

		if (reg_stride == 8) {
			if (val >> 8) {
				pr_err("input param over regmap stride limit\n");
				return -EINVAL;
			}
		}
	}

	for (i = 0; i < len - 1; i++) {
		reg = sysfs->input_param[0] + i * reg_stride;
		val = sysfs->input_param[1 + i];
		regmap_write(regmap, reg, val);
	}
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_WO(reg_write);

static ssize_t ssc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", sysfs->ssc_en);

	return ret;
}

static ssize_t ssc_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	ret = kstrtoint(buf, 10, &sysfs->ssc_en);
	if (ret) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("Invalid input value\n");
		return -EINVAL;
	}

	sprd_dphy_ssc_en(dphy, sysfs->ssc_en);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_RW(ssc);

static ssize_t hop_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", sysfs->hop_freq);

	return ret;
}

static ssize_t hop_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	int ret;
	int delta;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	ret = kstrtoint(buf, 10, &sysfs->hop_freq);
	if (ret) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("Invalid input freq\n");
		return -EINVAL;
	}

	/*
	 * because of double edge trigger,
	 * the rule is actual freq * 10 / 2,
	 * Eg: Required freq is 500M
	 * Equation: 2500*2*1000/10=500*1000=2500*200=500M
	 */
	if (sysfs->hop_freq == 0)
		sysfs->hop_freq = ctx->freq;
	else
		sysfs->hop_freq *= 200;
	pr_info("input freq is %d\n", sysfs->hop_freq);

	delta = sysfs->hop_freq - ctx->freq;
	sprd_dphy_hop_config(dphy, delta, 200);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_RW(hop);

static ssize_t ulps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", sysfs->ulps_en);

	return ret;
}

static ssize_t ulps_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	ret = kstrtoint(buf, 10, &sysfs->ulps_en);
	if (ret) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("Invalid input freq\n");
		return -EINVAL;
	}

	if (sysfs->ulps_en)
		sprd_dphy_ulps_enter(dphy);
	else
		sprd_dphy_ulps_exit(dphy);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_RW(ulps);

static ssize_t freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", ctx->freq);

	return ret;
}

static ssize_t freq_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	int ret;
	int freq;

	ret = kstrtoint(buf, 10, &freq);
	if (ret) {
		pr_err("Invalid input freq\n");
		return -EINVAL;
	}

	if (freq == ctx->freq) {
		pr_info("input freq is the same as old\n");
		return count;
	}

	pr_info("input freq is %d\n", freq);

	ctx->freq = freq;

	return count;
}
static DEVICE_ATTR_RW(freq);

static ssize_t suspend_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct device *dsi_dev = sprd_disp_pipe_get_input(dphy->dev.parent);
	struct sprd_dsi *dsi = dev_get_drvdata(dsi_dev);

	if ((dsi->ctx.enabled) && (dphy->ctx.enabled)) {
		sprd_dphy_disable(dphy);
		dphy->ctx.enabled = false;
	}

	return count;
}
static DEVICE_ATTR_WO(suspend);

static ssize_t resume_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct device *dsi_dev = sprd_disp_pipe_get_input(dphy->dev.parent);
	struct sprd_dsi *dsi = dev_get_drvdata(dsi_dev);

	if ((dsi->ctx.enabled) && (!dphy->ctx.enabled)) {
		sprd_dphy_enable(dphy);
		dphy->ctx.enabled = true;
	}

	return count;
}
static DEVICE_ATTR_WO(resume);

static ssize_t reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	sprd_dphy_reset(dphy);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_WO(reset);

static ssize_t shutdown_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	sprd_dphy_shutdown(dphy);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_WO(shutdown);

static struct attribute *dphy_attrs[] = {
	&dev_attr_reg_read.attr,
	&dev_attr_reg_write.attr,
	&dev_attr_ssc.attr,
	&dev_attr_hop.attr,
	&dev_attr_ulps.attr,
	&dev_attr_freq.attr,
	&dev_attr_suspend.attr,
	&dev_attr_resume.attr,
	&dev_attr_reset.attr,
	&dev_attr_shutdown.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dphy);

int sprd_dphy_sysfs_init(struct device *dev)
{
	int rc;

	sysfs = kzalloc(sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs) {
		pr_err("alloc dphy sysfs failed\n");
		return -ENOMEM;
	}
	rc = sysfs_create_groups(&dev->kobj, dphy_groups);
	if (rc)
		pr_err("create dphy attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_dphy_sysfs_init);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Add dphy attribute nodes for userspace");
MODULE_LICENSE("GPL v2");
