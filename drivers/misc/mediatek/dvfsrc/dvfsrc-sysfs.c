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
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include "dvfsrc-debug.h"
#include <linux/sysfs.h>


static struct mtk_pm_qos_request dvfsrc_memory_bw_req;
static struct mtk_pm_qos_request dvfsrc_memory_ext_bw_req;
static struct mtk_pm_qos_request dvfsrc_memory_hrtbw_req;
static struct mtk_pm_qos_request dvfsrc_ddr_opp_req;
static struct mtk_pm_qos_request dvfsrc_vcore_opp_req;
static struct mtk_pm_qos_request dvfsrc_scp_vcore_req;

static u32 bw, hrt_bw;
static DEFINE_MUTEX(bw_lock);

static ssize_t dvfsrc_req_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvd->pmqos_enable)
		mtk_pm_qos_update_request(&dvfsrc_memory_bw_req, val);
	else {
		if (!dvfsrc->path)
			return -EINVAL;

		mutex_lock(&bw_lock);
		bw = val;
		icc_set_bw(dvfsrc->path, MBps_to_icc(bw), MBps_to_icc(hrt_bw));
		mutex_unlock(&bw_lock);
	}

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_bw);

static ssize_t dvfsrc_req_ext_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvd->pmqos_enable)
		mtk_pm_qos_update_request(&dvfsrc_memory_ext_bw_req, val);

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_ext_bw);

static ssize_t dvfsrc_req_hrtbw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvd->pmqos_enable)
		mtk_pm_qos_update_request(&dvfsrc_memory_hrtbw_req, val);
	else {
		if (!dvfsrc->path)
			return -EINVAL;

		mutex_lock(&bw_lock);
		hrt_bw = val;
		icc_set_bw(dvfsrc->path, MBps_to_icc(bw), MBps_to_icc(hrt_bw));
		mutex_unlock(&bw_lock);
	}

	return count;
}

static DEVICE_ATTR_WO(dvfsrc_req_hrtbw);

static ssize_t dvfsrc_req_ddr_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvd->pmqos_enable) {
		if ((val > -1) && (val < dvfsrc->num_perf))
			mtk_pm_qos_update_request(&dvfsrc_ddr_opp_req,
				dvfsrc->perfs[val]);
		else
			mtk_pm_qos_update_request(&dvfsrc_ddr_opp_req,
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
	} else {
		if ((val > -1) && (val < dvfsrc->num_perf))
			dev_pm_genpd_set_performance_state(
				dev, dvfsrc->perfs[val]);
		else
			dev_pm_genpd_set_performance_state(
				dev, 0);
	}

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_ddr_opp);

static ssize_t dvfsrc_req_vcore_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvd->pmqos_enable)
		mtk_pm_qos_update_request(&dvfsrc_vcore_opp_req, val);
	else {
		if (dvfsrc->dvfsrc_vcore_power)
			regulator_set_voltage(dvfsrc->dvfsrc_vcore_power,
				val, INT_MAX);
	}

	return count;
}
static DEVICE_ATTR_WO(dvfsrc_req_vcore_opp);

static ssize_t dvfsrc_req_vscp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (dvfsrc->dvd->pmqos_enable)
		mtk_pm_qos_update_request(&dvfsrc_scp_vcore_req, val);
	else {
		if (dvfsrc->dvfsrc_vscp_power)
			regulator_set_voltage(dvfsrc->dvfsrc_vscp_power,
				val, INT_MAX);
	}

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
	if (config->dump_spm_info)
		p = config->dump_spm_info(dvfsrc, p,
			dump_size - (p - buf));

	return p - buf;
}
static DEVICE_ATTR_RO(dvfsrc_dump);

#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
#define VCOREFS_SMC_CMD_DVFS_HOPPING_STATE 20
static ssize_t dvfsrc_freq_hopping_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct arm_smccc_res ares;
	int gps_on = 0;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
		VCOREFS_SMC_CMD_DVFS_HOPPING_STATE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		gps_on = ares.a1;

	return sprintf(buf, "%d\n", gps_on);
}

static ssize_t dvfsrc_freq_hopping_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	dvfsrc_enable_dvfs_freq_hopping(val);

	return count;
}

static DEVICE_ATTR_RW(dvfsrc_freq_hopping);
#endif

static struct attribute *dvfsrc_sysfs_attrs[] = {
	&dev_attr_dvfsrc_req_bw.attr,
	&dev_attr_dvfsrc_req_ext_bw.attr,
	&dev_attr_dvfsrc_req_hrtbw.attr,
	&dev_attr_dvfsrc_req_vcore_opp.attr,
	&dev_attr_dvfsrc_req_ddr_opp.attr,
	&dev_attr_dvfsrc_req_vscp.attr,
	&dev_attr_dvfsrc_dump.attr,
	&dev_attr_dvfsrc_force_vcore_dvfs_opp.attr,
#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
	&dev_attr_dvfsrc_freq_hopping.attr,
#endif
	NULL,
};

static struct attribute_group dvfsrc_sysfs_attr_group = {
	.attrs = dvfsrc_sysfs_attrs,
};

int dvfsrc_register_sysfs(struct device *dev)
{
	int ret;

	mtk_pm_qos_add_request(&dvfsrc_memory_bw_req,
			MTK_PM_QOS_MEMORY_BANDWIDTH,
			MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_memory_ext_bw_req,
			MTK_PM_QOS_MEMORY_EXT_BANDWIDTH,
			MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_memory_hrtbw_req,
			MTK_PM_QOS_HRT_BANDWIDTH,
			MTK_PM_QOS_HRT_BANDWIDTH_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_ddr_opp_req,
			MTK_PM_QOS_DDR_OPP,
			MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_vcore_opp_req,
			MTK_PM_QOS_VCORE_OPP,
			MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_scp_vcore_req,
			MTK_PM_QOS_SCP_VCORE_REQUEST,
			MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE);

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
	mtk_pm_qos_remove_request(&dvfsrc_memory_bw_req);
	mtk_pm_qos_remove_request(&dvfsrc_memory_ext_bw_req);
	mtk_pm_qos_remove_request(&dvfsrc_memory_hrtbw_req);
	mtk_pm_qos_remove_request(&dvfsrc_ddr_opp_req);
	mtk_pm_qos_remove_request(&dvfsrc_vcore_opp_req);
	mtk_pm_qos_remove_request(&dvfsrc_scp_vcore_req);
}

