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

#include "dvfsrc.h"

static u32 bw, hrt_bw;
static DEFINE_MUTEX(bw_lock);

static ssize_t dvfsrc_req_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (!dvfsrc->path)
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

	if (!dvfsrc->path)
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

	if ((val >= 0) && (val < dvfsrc->num_perf))
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

static ssize_t dvfsrc_force_vcore_dvfs_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvd->config->force_opp)
		dvfsrc->dvd->config->force_opp(dvfsrc, val);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_force_vcore_dvfs_opp);

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

static ssize_t dvfsrc_met_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	char **name;
	unsigned int *value;
	int i, res_num;

	p += snprintf(p, buff_end - p,
		"NUM_VCORE_OPP : %d\n",
		vcorefs_get_num_opp());

	res_num = vcorefs_get_opp_info_num();
	name = vcorefs_get_opp_info_name();
	value = vcorefs_get_opp_info();
	p += snprintf(p, buff_end - p,
		"NUM_OPP_INFO : %d\n", res_num);
	for (i = 0; i < res_num; i++) {
		p += snprintf(p, buff_end - p,
			"%s : %d\n",
			name[i], value[i]);
	}

	res_num = vcorefs_get_src_req_num();
	name = vcorefs_get_src_req_name();
	value = vcorefs_get_src_req();
	p += snprintf(p, buff_end - p,
		"NUM SRC_REQ: %d\n", res_num);
	for (i = 0; i < res_num; i++) {
		p += snprintf(p, buff_end - p,
			"%s : %d\n",
			name[i], value[i]);
	}

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_met);

static ssize_t dvfsrc_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;

	p = config->dump_info(dvfsrc, p, dump_size);
	p = config->dump_reg(dvfsrc, p, dump_size - (p - buf));
	p = config->dump_record(dvfsrc, p, dump_size - (p - buf));

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
	&dev_attr_dvfsrc_force_vcore_dvfs_opp.attr,
	&dev_attr_dvfsrc_met.attr,
	NULL,
};

static struct attribute_group dvfsrc_sysfs_attr_group = {
	.name = "dvfsrc_sysfs",
	.attrs = dvfsrc_sysfs_attrs,
};

int dvfsrc_register_sysfs(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
}

void dvfsrc_unregister_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
}

