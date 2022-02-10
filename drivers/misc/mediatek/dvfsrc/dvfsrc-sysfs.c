// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interconnect.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include "dvfsrc-common.h"
#include "dvfsrc-helper.h"
#include <linux/soc/mediatek/mtk_sip_svc.h>
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

	icc_set_bw(dvfsrc->hrt_path, MBps_to_icc(val), 0);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_hrtbw);

static ssize_t dvfsrc_req_ddr_opp_store(struct device *dev,
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
static DEVICE_ATTR_WO(dvfsrc_req_ddr_opp);

static ssize_t dvfsrc_req_vcore_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 val = 0;
	u32 max_opp;
	u32 gear;
	int vcore_uV = 0;

	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	max_opp = dvfsrc->opp_desc->num_vcore_opp - 1;
	gear = (val <= max_opp) ? max_opp - val : 0;

	if (dvfsrc->dvfsrc_vcore_power) {
		vcore_uV = regulator_list_voltage(dvfsrc->dvfsrc_vcore_power, gear);
		if (vcore_uV > 0)
			regulator_set_voltage(dvfsrc->dvfsrc_vcore_power, vcore_uV, INT_MAX);
	}

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_vcore_opp);

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

	if (config->dump_vmode_info)
		p = config->dump_vmode_info(dvfsrc, p, dump_size - (p - buf));

	p = config->dump_reg(dvfsrc, p, dump_size - (p - buf));
	p = config->dump_record(dvfsrc, p, dump_size - (p - buf));

	if (config->dump_spm_info && dvfsrc->spm_regs)
		p = config->dump_spm_info(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_dump);

static ssize_t spm_cmd_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	if (config->dump_spm_cmd && dvfsrc->spm_regs)
		p = config->dump_spm_cmd(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(spm_cmd_dump);

static ssize_t spm_timer_latch_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	ssize_t dump_size = PAGE_SIZE - 1;
	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;
	if (config->dump_spm_timer_latch && dvfsrc->spm_regs)
		p = config->dump_spm_timer_latch(dvfsrc, p, dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(spm_timer_latch_dump);

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

static ssize_t dvfsrc_ddr_opp_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	int level = 0;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		if (dvfsrc->opp_desc->opps[i].dram_opp == level) {
			p += snprintf(p, buff_end - p,
			"%lu ",
			(unsigned long)(dvfsrc->opp_desc->opps[i].dram_khz)*1000);
			level = level + 1;
		}
	}
	p += snprintf(p, buff_end - p, "\n");

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_ddr_opp_table);

static ssize_t dvfsrc_num_opps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", dvfsrc->opp_desc->num_opp);
}
static DEVICE_ATTR_RO(dvfsrc_num_opps);

static ssize_t dvfsrc_get_dvfs_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u64 dvfs_time_us;

	const struct dvfsrc_config *config;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	config = dvfsrc->dvd->config;

	if (dvfsrc->force_opp_idx >= dvfsrc->opp_desc->num_opp)
		return sprintf(buf, "Not in force mode\n");

	if (config->query_dvfs_time)
		dvfs_time_us = config->query_dvfs_time(dvfsrc);
	else
		return sprintf(buf, "Not Suuport query\n");

	return sprintf(buf, "dvfs_time = %llu us\n", dvfs_time_us);
}
DEVICE_ATTR_RO(dvfsrc_get_dvfs_time);

static inline ssize_t dvfsrc_qos_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", dvfsrc->qos_mode);
}

static inline ssize_t dvfsrc_qos_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int mode = 0;
	struct arm_smccc_res ares;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtouint(buf, 0, &mode) != 0)
		return -EINVAL;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_QOS_MODE,
		mode, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->qos_mode = mode;

	return count;
}
DEVICE_ATTR_RW(dvfsrc_qos_mode);


static struct attribute *dvfsrc_sysfs_attrs[] = {
	&dev_attr_dvfsrc_req_bw.attr,
	&dev_attr_dvfsrc_req_hrtbw.attr,
	&dev_attr_dvfsrc_req_vcore_opp.attr,
	&dev_attr_dvfsrc_req_ddr_opp.attr,
	&dev_attr_dvfsrc_dump.attr,
	&dev_attr_dvfsrc_force_vcore_dvfs_opp.attr,
	&dev_attr_dvfsrc_opp_table.attr,
	&dev_attr_dvfsrc_ddr_opp_table.attr,
	&dev_attr_dvfsrc_num_opps.attr,
	&dev_attr_dvfsrc_get_dvfs_time.attr,
	&dev_attr_spm_cmd_dump.attr,
	&dev_attr_spm_timer_latch_dump.attr,
	&dev_attr_dvfsrc_qos_mode.attr,
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
	if (ret)
		return ret;

	ret = sysfs_create_link(kernel_kobj, &dev->kobj,
		"helio-dvfsrc");
	return ret;
}

void dvfsrc_unregister_sysfs(struct device *dev)
{
	sysfs_remove_link(&dev->parent->kobj, "helio-dvfsrc");
	sysfs_remove_group(&dev->kobj, &dvfsrc_sysfs_attr_group);
}

