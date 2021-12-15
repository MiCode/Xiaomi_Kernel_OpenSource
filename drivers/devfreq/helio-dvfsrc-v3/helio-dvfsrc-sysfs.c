/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>

#include <helio-dvfsrc-qos.h>
#include <helio-dvfsrc-opp.h>
#include "helio-dvfsrc-ip-v2.h"

static struct mtk_pm_qos_request dvfsrc_memory_bw_req;
static struct mtk_pm_qos_request dvfsrc_ddr_opp_req;
static struct mtk_pm_qos_request dvfsrc_vcore_opp_req;
static struct mtk_pm_qos_request dvfsrc_scp_vcore_req;
static struct mtk_pm_qos_request dvfsrc_power_model_ddr_req;
static struct mtk_pm_qos_request dvfsrc_power_model_vcore_req;
static struct mtk_pm_qos_request dvfsrc_vcore_dvfs_opp_force;
static struct mtk_pm_qos_request dvfsrc_isphrt_bw_req;

static DEFINE_MUTEX(dump_lock);

static ssize_t dvfsrc_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_dvfsrc_enabled());
}
static ssize_t dvfsrc_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	helio_dvfsrc_enable(val);

	return count;
}
static DEVICE_ATTR(dvfsrc_enable, 0644,
		dvfsrc_enable_show, dvfsrc_enable_store);

static ssize_t dvfsrc_enable_flag_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", helio_dvfsrc_flag_get());
}
static ssize_t dvfsrc_enable_flag_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 16, &val))
		return -EINVAL;

	helio_dvfsrc_flag_set(val);

	return count;
}

static DEVICE_ATTR(dvfsrc_enable_flag, 0644,
		dvfsrc_enable_flag_show, dvfsrc_enable_flag_store);

static ssize_t dvfsrc_req_memory_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_memory_bw_req, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_req_memory_bw, 0200,
		NULL, dvfsrc_req_memory_bw_store);

static ssize_t dvfsrc_req_isphrt_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_isphrt_bw_req, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_req_isphrt_bw, 0200,
		NULL, dvfsrc_req_isphrt_bw_store);

static ssize_t dvfsrc_req_ddr_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_ddr_opp_req, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_req_ddr_opp, 0200,
		NULL, dvfsrc_req_ddr_opp_store);

static ssize_t dvfsrc_req_vcore_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_vcore_opp_req, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_req_vcore_opp, 0200,
		NULL, dvfsrc_req_vcore_opp_store);

static ssize_t dvfsrc_req_scp_vcore_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_scp_vcore_req, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_req_scp_vcore, 0200,
		NULL, dvfsrc_req_scp_vcore_store);

static ssize_t dvfsrc_req_power_model_ddr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_power_model_ddr_req, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_req_power_model_ddr, 0200,
		NULL, dvfsrc_req_power_model_ddr_store);

static ssize_t dvfsrc_req_power_model_vcore_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_power_model_vcore_req, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_req_power_model_vcore, 0200,
		NULL, dvfsrc_req_power_model_vcore_store);

static ssize_t dvfsrc_set_vcore_uv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int opp = 0, vcore_uv = 0;

	if (sscanf(buf, "%d %d", &opp, &vcore_uv) != 2)
		return -EINVAL;

	set_vcore_uv_table(opp, vcore_uv);
	dvfsrc_opp_table_init();

	return count;
}
static DEVICE_ATTR(dvfsrc_set_vcore_uv, 0200,
		NULL, dvfsrc_set_vcore_uv_store);

static ssize_t dvfsrc_force_vcore_dvfs_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	mtk_pm_qos_update_request(&dvfsrc_vcore_dvfs_opp_force, val);

	return count;
}
static DEVICE_ATTR(dvfsrc_force_vcore_dvfs_opp, 0200,
		NULL, dvfsrc_force_vcore_dvfs_opp_store);

static ssize_t dvfsrc_opp_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct helio_dvfsrc *dvfsrc;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	int i;

	dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc)
		return sprintf(buf, "Failed to access dvfsrc\n");

	mutex_lock(&dump_lock);
	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++) {
		p += snprintf(p, buff_end - p, "[OPP%-2d]: %-8u uv %-8u khz\n",
				i, get_vcore_uv(i), get_ddr_khz(i));
	}

	p += snprintf(p, buff_end - p, "\n");
	mutex_unlock(&dump_lock);

	return p - buf;
}

static DEVICE_ATTR(dvfsrc_opp_table, 0444, dvfsrc_opp_table_show, NULL);

static ssize_t dvfsrc_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	u32 index;

	index = dvfsrc_dump_reg(p, PAGE_SIZE);

	return index;
}

static DEVICE_ATTR(dvfsrc_dump, 0444, dvfsrc_dump_show, NULL);

static ssize_t dvfsrc_level_intr_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t dvfsrc_level_intr_log_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	dvfsrc_enable_level_intr(val);

	return count;
}

static DEVICE_ATTR(dvfsrc_level_intr_log, 0644,
		dvfsrc_level_intr_log_show, dvfsrc_level_intr_log_store);

static ssize_t dvfsrc_num_opps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", VCORE_DVFS_OPP_NUM);
}
static DEVICE_ATTR(dvfsrc_num_opps, 0444, dvfsrc_num_opps_show, NULL);

static ssize_t dvfsrc_get_dvfs_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 last;
	u64 time_1, time_2;
	u64 dvfs_time_us;

	if (!(dvfsrc_read(DVFSRC_BASIC_CONTROL) & (1 << FORCE_EN_TAR_SHIFT)))
		return sprintf(buf, "Not in force mode\n");

	last = dvfsrc_read(DVFSRC_LAST);
	time_1 = dvfsrc_read(DVFSRC_RECORD_0_1 + RECORD_SHIFT * last);
	time_1 = time_1 << 32;
	time_1 = dvfsrc_read(DVFSRC_RECORD_0_0 + RECORD_SHIFT * last) + time_1;
	last = (last + 7) % 8;
	time_2 = dvfsrc_read(DVFSRC_RECORD_0_1 + RECORD_SHIFT * last);
	time_2 = time_2 << 32;
	time_2 = dvfsrc_read(DVFSRC_RECORD_0_0 + RECORD_SHIFT * last) + time_2;
#if BITS_PER_LONG == 32
	dvfs_time_us = div_u64((time_1 - time_2), 13);
#else
	dvfs_time_us = (time_1 - time_2) / 13;
#endif

	return sprintf(buf, "dvfs_time = %llu us\n", dvfs_time_us);
}
static DEVICE_ATTR(dvfsrc_get_dvfs_time, 0444,
		dvfsrc_get_dvfs_time_show, NULL);

static struct attribute *helio_dvfsrc_attrs[] = {
	&dev_attr_dvfsrc_enable.attr,
	&dev_attr_dvfsrc_enable_flag.attr,
	&dev_attr_dvfsrc_req_memory_bw.attr,
	&dev_attr_dvfsrc_req_ddr_opp.attr,
	&dev_attr_dvfsrc_req_vcore_opp.attr,
	&dev_attr_dvfsrc_req_scp_vcore.attr,
	&dev_attr_dvfsrc_req_power_model_ddr.attr,
	&dev_attr_dvfsrc_req_power_model_vcore.attr,
	&dev_attr_dvfsrc_force_vcore_dvfs_opp.attr,
	&dev_attr_dvfsrc_set_vcore_uv.attr,
	&dev_attr_dvfsrc_opp_table.attr,
	&dev_attr_dvfsrc_dump.attr,
	&dev_attr_dvfsrc_level_intr_log.attr,
	&dev_attr_dvfsrc_req_isphrt_bw.attr,
	&dev_attr_dvfsrc_num_opps.attr,
	&dev_attr_dvfsrc_get_dvfs_time.attr,
	NULL,
};

static struct attribute_group helio_dvfsrc_attr_group = {
	.name = "helio-dvfsrc",
	.attrs = helio_dvfsrc_attrs,
};

int helio_dvfsrc_add_interface(struct device *dev)
{
	mtk_pm_qos_add_request(&dvfsrc_memory_bw_req, PM_QOS_APU_MEMORY_BANDWIDTH,
			PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_ddr_opp_req, MTK_PM_QOS_DDR_OPP,
			0);
	mtk_pm_qos_add_request(&dvfsrc_vcore_opp_req, MTK_PM_QOS_VCORE_OPP,
			0);
	mtk_pm_qos_add_request(&dvfsrc_scp_vcore_req, MTK_PM_QOS_SCP_VCORE_REQUEST,
			MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_power_model_ddr_req,
			MTK_PM_QOS_POWER_MODEL_DDR_REQUEST,
			MTK_PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_power_model_vcore_req,
			MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST,
			MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_vcore_dvfs_opp_force,
			MTK_PM_QOS_VCORE_DVFS_FORCE_OPP,
			MTK_PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE);
	mtk_pm_qos_add_request(&dvfsrc_isphrt_bw_req,
			MTK_PM_QOS_ISP_HRT_BANDWIDTH,
			PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE);

	return sysfs_create_group(&dev->kobj, &helio_dvfsrc_attr_group);
}

void helio_dvfsrc_qos_init_done(void)
{
	mtk_pm_qos_update_request(&dvfsrc_vcore_opp_req, MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	mtk_pm_qos_update_request(&dvfsrc_ddr_opp_req, MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

}

void helio_dvfsrc_remove_interface(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &helio_dvfsrc_attr_group);
}
