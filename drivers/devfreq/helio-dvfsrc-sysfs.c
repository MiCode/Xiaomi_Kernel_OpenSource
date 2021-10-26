/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/sysfs.h>

#include <helio-dvfsrc.h>
#if defined(CONFIG_MTK_DRAMC)
#include <mtk_dramc.h>
#endif

#include <mt-plat/upmu_common.h>
#include "mtk_dvfsrc_reg.h"

#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#include <mtk_vcorefs_governor.h>

static struct pm_qos_request dvfsrc_emi_request;
static struct pm_qos_request dvfsrc_vcore_request;

#if !defined(CONFIG_MACH_MT6771)

__weak unsigned int get_dram_data_rate(void) { return 0; }
__weak unsigned int get_vcore_opp_volt(unsigned int seg) { return 0; }
__weak int dram_steps_freq(unsigned int step) { return 0; }

int vcorefs_get_curr_vcore(void)
{
	int ret = 0;

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	ret = pmic_get_register_value(PMIC_VCORE_ADDR);
	ret = vcore_pmic_to_uv(ret);
#endif

	return ret;
}

int vcorefs_get_curr_ddr(void)
{
	return get_dram_data_rate() * 1000;
}


int dvfsrc_get_vcore_by_steps(u32 opp)
{
	return vcore_pmic_to_uv(get_vcore_opp_volt(opp));
}

int dvfsrc_get_ddr_by_steps(u32 opp)
{
	int ddr_khz;

	ddr_khz = dram_steps_freq(opp) * 1000;

	return ddr_khz;
}
#endif

static ssize_t opp_table_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	char *p = buf;

	dvfsrc_update_opp_table();
	p = dvfsrc_get_opp_table_info(p);

	return p - buf;
}

static DEVICE_ATTR(opp_table, 0444, opp_table_show, NULL);

static ssize_t dvfsrc_debug_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct helio_dvfsrc *dvfsrc;
	char *p = buf;
	char *buff_end = p + PAGE_SIZE;
	int uv = vcorefs_get_curr_vcore();

	dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc)
		return sprintf(buf, "Failed to access dvfsrc\n");

	p += snprintf(p, buff_end - p, "[%-12s] uv : %-8u (0x%x)\n",
			"vcore", uv, vcore_uv_to_pmic(uv));
	p += snprintf(p, buff_end - p, "[%-12s] khz: %-8u\n",
			"ddr", vcorefs_get_curr_ddr());

	p += snprintf(p, buff_end - p, "[%-12s]: %d\n",
			"Enable", dvfsrc->enable);
	p += snprintf(p, buff_end - p, "[%-12s]: %d\n",
			"skip", dvfsrc->skip);
	p += snprintf(p, buff_end - p, "[%-12s]: %d\n",
			"log_mask", dvfsrc->log_mask);

	p += snprintf(p, buff_end - p, "[vcore_dvs]: %d\n", dvfsrc->vcore_dvs);
	p += snprintf(p, buff_end - p, "[ddr_dfs  ]: %d\n", dvfsrc->ddr_dfs);
	p += snprintf(p, buff_end - p, "[mm_clk   ]: %d\n", dvfsrc->mm_clk);

	p += snprintf(p, buff_end - p, "%-24s: 0x%x\n",
			"DVFSRC_RECORD_COUNT",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_COUNT));
	p += snprintf(p, buff_end - p, "%-24s: 0x%x\n",
			"DVFSRC_LAST",
			dvfsrc_read(dvfsrc, DVFSRC_LAST));
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_0_1~3_1",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0_1),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_1_1),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_2_1),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_3_1));
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_4_1~7_1",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_4_1),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_5_1),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_6_1),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_7_1));
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_0_0~3_0",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0_0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_1_0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_2_0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_3_0));
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_4_0~7_0",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_4_0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_5_0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_6_0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_7_0));
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_MD_0~3",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_1),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_2),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_3));
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"DVFSRC_RECORD_MD_4~7",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_4),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_5),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_6),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_MD_7));

	p += snprintf(p, buff_end - p, "%-24s: 0x%x\n",
			"DVFSRC_LEVEL",
			dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
	p += snprintf(p, buff_end - p, "%-24s: 0x%x\n",
			"DVFSRC_VCORE_REQUEST",
			dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST));
	p += snprintf(p, buff_end - p, "%-24s: 0x%x\n",
			"DVFSRC_EMI_REQUEST",
			dvfsrc_read(dvfsrc, DVFSRC_EMI_REQUEST));
	p += snprintf(p, buff_end - p, "%-24s: 0x%x\n",
			"DVFSRC_MD_REQUEST",
			dvfsrc_read(dvfsrc, DVFSRC_MD_REQUEST));
	p += snprintf(p, buff_end - p, "%-24s: 0x%x\n",
			"DVFSRC_RSRV_0",
			dvfsrc_read(dvfsrc, DVFSRC_RSRV_0));
	p += snprintf(p, buff_end - p, "\n");

	p += snprintf(p, buff_end - p, "%-24s: %d\n",
			"QOS_EMI_OPP",
			pm_qos_request(PM_QOS_EMI_OPP));
	p += snprintf(p, buff_end - p, "%-24s: %d\n",
			"QOS_VCORE_OPP",
			pm_qos_request(PM_QOS_VCORE_OPP));
	p += snprintf(p, buff_end - p, "%-24s: %3d (100MB/s)\n",
			"DVFSRC_EMI_QOS0 THRES",
			dvfsrc_read(dvfsrc, DVFSRC_EMI_QOS0));
	p += snprintf(p, buff_end - p, "%-24s: %3d (100MB/s)\n",
			"DVFSRC_EMI_QOS1 THRES",
			dvfsrc_read(dvfsrc, DVFSRC_EMI_QOS1));
	p += snprintf(p, buff_end - p, "%-24s: %3d (100MB/s)\n",
			"DVFSRC_SW_BW_0 (Test)",
			dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0));
	p += snprintf(p, buff_end - p, "%-24s: %3d (100MB/s)\n",
			"DVFSRC_SW_BW_1 (CPU)",
			dvfsrc_read(dvfsrc, DVFSRC_SW_BW_1));
	p += snprintf(p, buff_end - p, "%-24s: %3d (100MB/s)\n",
			"DVFSRC_SW_BW_2 (GPU)",
			dvfsrc_read(dvfsrc, DVFSRC_SW_BW_2));
	p += snprintf(p, buff_end - p, "%-24s: %3d (100MB/s)\n",
			"DVFSRC_SW_BW_3 (MM)",
			dvfsrc_read(dvfsrc, DVFSRC_SW_BW_3));
	p += snprintf(p, buff_end - p, "%-24s: %3d (100MB/s)\n",
			"DVFSRC_SW_BW_4 (OTHER)",
			dvfsrc_read(dvfsrc, DVFSRC_SW_BW_4));
	p += snprintf(p, buff_end - p, "\n");

	return p - buf;
}

static ssize_t dvfsrc_debug_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int val, val2, r = 0;
	char cmd[32];
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc)
		return -ENODEV;

	if (sscanf(buf, "%31s 0x%x 0x%x", cmd, &val, &val2) == 3 ||
			sscanf(buf, "%31s %d %d", cmd, &val, &val2) == 3)
		pr_info("dvfsrc_debug cmd: %s, val1: %d(0x%x) val2: %d(0x%x)\n",
			cmd, val, val, val2, val2);
	else if (sscanf(buf, "%31s 0x%x", cmd, &val) == 2 ||
			sscanf(buf, "%31s %d", cmd, &val) == 2)
		pr_info("dvfsrc_debug cmd: %s, val: %d(0x%x)\n", cmd, val, val);

	if (!strcmp(cmd, "kir_emi"))
		pm_qos_update_request(&dvfsrc_emi_request, val);
	else if (!strcmp(cmd, "kir_vcore"))
		pm_qos_update_request(&dvfsrc_vcore_request, val);
	else if (!strcmp(cmd, "skip"))
		dvfsrc->skip = val;
	else if (!strcmp(cmd, "log_mask"))
		dvfsrc->log_mask = val;
	else
		r = -EPERM;

	return count;
}

static DEVICE_ATTR(dvfsrc_debug, 0644, dvfsrc_debug_show, dvfsrc_debug_store);

static ssize_t dvfsrc_enable_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct helio_dvfsrc *dvfsrc;

	dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc)
		return sprintf(buf, "Failed to access dvfsrc\n");

	return sprintf(buf, "%d\n", dvfsrc->enable);
}
static ssize_t dvfsrc_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct helio_dvfsrc *dvfsrc;
	s32 value;

	dvfsrc = dev_get_drvdata(dev);

	if (!dvfsrc)
		return -ENODEV;

	if (kstrtos32(buf, 0, &value))
		return -EINVAL;

	if (value < 0)
		return -EINVAL;

	dvfsrc->enable = value;

	/* ToDo: If disable, fix highest opp? */
	return count;
}
static
DEVICE_ATTR(dvfsrc_enable, 0644, dvfsrc_enable_show, dvfsrc_enable_store);

static struct attribute *helio_dvfsrc_attrs[] = {
	&dev_attr_dvfsrc_debug.attr,
	&dev_attr_dvfsrc_enable.attr,
	&dev_attr_opp_table.attr,
	NULL,
};

static struct attribute_group helio_dvfsrc_attr_group = {
	.name = "helio-dvfsrc",
	.attrs = helio_dvfsrc_attrs,
};

int helio_dvfsrc_add_interface(struct device *dev)
{
	pm_qos_add_request(&dvfsrc_emi_request,
		PM_QOS_EMI_OPP, PM_QOS_EMI_OPP_DEFAULT_VALUE);
	pm_qos_add_request(&dvfsrc_vcore_request, PM_QOS_VCORE_OPP,
		PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	return sysfs_create_group(&dev->kobj, &helio_dvfsrc_attr_group);
}

void helio_dvfsrc_remove_interface(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &helio_dvfsrc_attr_group);
}
