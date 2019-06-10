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

#include "dvfsrc-mt6779.h"

static u32 bw, hrt_bw;
static DEFINE_MUTEX(bw_lock);

static ssize_t dvfsrc_req_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&bw_lock);
	bw = val;
	icc_set_bw(dvfsrc->path, MBps_to_icc(bw), MBps_to_icc(hrt_bw));
	mutex_unlock(&bw_lock);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_bw);

static ssize_t dvfsrc_req_hrtbw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&bw_lock);
	hrt_bw = val;
	icc_set_bw(dvfsrc->path, MBps_to_icc(bw), MBps_to_icc(hrt_bw));
	mutex_unlock(&bw_lock);

	return count;
}

static DEVICE_ATTR_WO(dvfsrc_req_hrtbw);

static ssize_t dvfsrc_req_performance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < dvfsrc->num_perf)
		dev_pm_genpd_set_performance_state(dev, dvfsrc->perfs[val]);
	else
		dev_pm_genpd_set_performance_state(dev, 0);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_performance);

static ssize_t dvfsrc_req_vcore_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
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
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvfsrc_vscp_power)
		regulator_set_voltage(dvfsrc->dvfsrc_vscp_power,
			val, INT_MAX);

	return count;
}

static DEVICE_ATTR_WO(dvfsrc_req_vscp);

static ssize_t dvfsrc_opp_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	int dram_type = dvfsrc->dram_type;

	for (i = 0; i < dvfsrc->dvd->num_opp; i++) {
		p += snprintf(p, buff_end - p,
			"[OPP%-2d]: %-8u uv %-8u khz\n",
			i,
			dvfsrc->dvd->opps[dram_type][i].vcore_uv,
			dvfsrc->dvd->opps[dram_type][i].dram_khz);
	}

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_opp_table);

static ssize_t dvfsrc_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;

	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	p = dvfsrc->dvd->dump_info(dvfsrc, p, PAGE_SIZE - 1);
	p = dvfsrc->dvd->dump_reg(dvfsrc, p, dump_size - (p - buf));
	p = dvfsrc->dvd->dump_record(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_dump);

static struct attribute *dvfsrc_sysfs_attrs[] = {
	&dev_attr_dvfsrc_req_bw.attr,
	&dev_attr_dvfsrc_req_hrtbw.attr,
	&dev_attr_dvfsrc_req_vcore.attr,
	&dev_attr_dvfsrc_req_performance.attr,
	&dev_attr_dvfsrc_req_vscp.attr,
	&dev_attr_dvfsrc_opp_table.attr,
	&dev_attr_dvfsrc_dump.attr,
	NULL,
};

static struct attribute_group dvfsrc_sysfs_attr_group = {
	.name = "dvfsrc_sysfs",
	.attrs = dvfsrc_sysfs_attrs,
};

int mt6779_dvfsrc_register_sysfs(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
}

void mt6779_dvfsrc_unregister_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
}

