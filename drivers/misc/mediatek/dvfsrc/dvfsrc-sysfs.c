// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interconnect.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include "dvfsrc-helper.h"
#include <linux/sysfs.h>

static DEFINE_MUTEX(bw_lock);

static ssize_t dvfsrc_req_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!dvfsrc->bw_path)
		return -EINVAL;

	icc_set_bw(dvfsrc->bw_path, MBps_to_icc(val), 0);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_bw);

static ssize_t dvfsrc_req_hrtbw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!dvfsrc->hrt_path)
		return -EINVAL;

	icc_set_bw(dvfsrc->hrt_path, MBps_to_icc(val), MBps_to_icc(val));

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_hrtbw);

static ssize_t dvfsrc_req_ddr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!dvfsrc->perf_path)
		return -EINVAL;

	if (val < dvfsrc->num_perf)
		icc_set_bw(dvfsrc->perf_path, 0, dvfsrc->perfs_peak_bw[val]);
	else
		icc_set_bw(dvfsrc->perf_path, 0, 0);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_ddr);

static ssize_t dvfsrc_req_vcore_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvfsrc_vcore_power)
		regulator_set_voltage(dvfsrc->dvfsrc_vcore_power,
			val, INT_MAX);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_vcore);

static ssize_t dvfsrc_req_vscp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvfsrc_vscp_power)
		regulator_set_voltage(dvfsrc->dvfsrc_vscp_power,
			val, INT_MAX);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_vscp);

static ssize_t dvfsrc_force_vcore_dvfs_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->force_opp)
		dvfsrc->force_opp(dvfsrc, val);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_force_vcore_dvfs_opp);

static ssize_t dvfsrc_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	p = dvfsrc->dump_info(dvfsrc, p, dump_size);
	p = config->dump_reg(dvfsrc, p, dump_size - (p - buf));
	p = config->dump_record(dvfsrc, p, dump_size - (p - buf));
	if (config->dump_spm_info && dvfsrc->spm_regs) {
		p = config->dump_spm_info(dvfsrc, p,
			dump_size - (p - buf));
	}

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_dump);

static ssize_t dvfsrc_opp_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	p += snprintf(p, buff_end - p,
		"NUM_VCORE_OPP : %d\n",
		dvfsrc->opp_desc->num_vcore_opp);
	p += snprintf(p, buff_end - p,
		"NUM_DDR_OPP : %d\n",
		dvfsrc->opp_desc->num_dram_opp);
	p += snprintf(p, buff_end - p,
		"NUM_DVFSRC_OPP : %d\n\n",
		dvfsrc->opp_desc->num_opp);

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		j = dvfsrc->opp_desc->num_opp - (i + 1);
		p += snprintf(p, buff_end - p,
			"[OPP%-2d]: %-8u uv %-8u khz\n",
			i,
			dvfsrc->opp_desc->opps[j].vcore_uv,
			dvfsrc->opp_desc->opps[j].dram_khz);
	}

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_opp_table);

static struct attribute *dvfsrc_sysfs_attrs[] = {
	&dev_attr_dvfsrc_req_bw.attr,
	&dev_attr_dvfsrc_req_hrtbw.attr,
	&dev_attr_dvfsrc_req_vcore.attr,
	&dev_attr_dvfsrc_req_ddr.attr,
	&dev_attr_dvfsrc_req_vscp.attr,
	&dev_attr_dvfsrc_dump.attr,
	&dev_attr_dvfsrc_force_vcore_dvfs_opp.attr,
	&dev_attr_dvfsrc_opp_table.attr,
	NULL,
};

static struct attribute_group dvfsrc_sysfs_attr_group = {
	.attrs = dvfsrc_sysfs_attrs,
};

int dvfsrc_register_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
	if (ret)
		return ret;

	ret = sysfs_create_link(&dev->parent->kobj, &dev->kobj,
		"helio-dvfsrc");

	return ret;
}

void dvfsrc_unregister_sysfs(struct device *dev)
{
	sysfs_remove_link(&dev->parent->kobj, "helio-dvfsrc");
	sysfs_remove_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
}

