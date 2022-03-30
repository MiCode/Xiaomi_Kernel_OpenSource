// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>

#include "edma_driver.h"
#include "edma_dbgfs.h"
#include "edma_reg.h"


u8 g_edma_log_lv = EDMA_LOG_WARN;

static ssize_t edma_register_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0, i, desp;
	struct edma_device *edma_device = dev_get_drvdata(dev);
	unsigned int core = edma_device->dbgfs_reg_core;

	struct edma_sub *edma_sub = edma_device->edma_sub[core];

	if (core >= edma_device->edma_sub_num) {
		count += scnprintf(buf + count, 2*PAGE_SIZE - count,
				 "not support core: %d\n", core);
		return count;
	}

	count += scnprintf(buf + count, 2*PAGE_SIZE - count,
			 "core: %d\n", core);
	for (i = 0; i < (EDMA_REG_SHOW_RANGE >> 2); i++) {
		count += scnprintf(buf + count, 2*PAGE_SIZE - count,
			 "0x%03x: 0x%08x\n", i * 4,
			 edma_read_reg32(edma_sub->base_addr, i * 4));
	}

	for (desp = 0; desp < 4; desp++) {
		u32 base;

		count += scnprintf(buf + count, 2*PAGE_SIZE - count,
			 "descriptor %d\n", desp);
		base = APU_EDMA2_DESP0_0 + desp * APU_EDMA2_DESP_OFFSET;
		for (i = 0; i < (APU_EDMA2_EX_DESP_OFFSET >> 2); i++) {
			count += scnprintf(buf + count, 2*PAGE_SIZE - count,
				 "0x%03x: 0x%08x\n", base + i * 4,
				 edma_read_reg32(edma_sub->base_addr,
							base + i * 4));
		}
	}

	return count;
}

static ssize_t edma_register_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct edma_device *edma_device = dev_get_drvdata(dev);
	unsigned int core = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &core);
	if (ret || core >= EDMA_SUB_NUM) {
		dev_notice(dev, "input parameter is worng\n");
		return count;
	}

	edma_device->dbgfs_reg_core = core;

	return count;
}

static ssize_t edma_power_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0, i;
	struct edma_device *edma_device = dev_get_drvdata(dev);
	struct edma_sub *edma_sub;

	for (i = 0; i < edma_device->edma_sub_num; i++) {
		edma_sub = edma_device->edma_sub[i];
		count += scnprintf(buf + count, PAGE_SIZE - count,
						"edma%d power:\n", i);
		if (edma_sub->power_state == EDMA_POWER_ON)
			count += scnprintf(buf + count, PAGE_SIZE - count,
							"ON\n");
		else
			count += scnprintf(buf + count, PAGE_SIZE - count,
							"OFF\n");
	}

	return count;
}

static ssize_t edma_power_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	unsigned int input = 0;
	int ret = 0;
	struct edma_device *edma_device = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &input);

	dev_notice(dev, "input parameter is %d\n", input);

	if (input == 666)
		edma_device->dbg_cfg |= EDMA_DBG_DISABLE_PWR_OFF;
	else
		edma_device->dbg_cfg &= ~EDMA_DBG_DISABLE_PWR_OFF;

	dev_notice(dev, "edma_device->dbg_cfg = 0x%x\n",
	edma_device->dbg_cfg);

	return count;
}

static ssize_t edma_debuglv_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;

	count += scnprintf(buf + count, PAGE_SIZE - count,
					"g_edma_log_lv =%d:\n", g_edma_log_lv);

	return count;
}

static ssize_t edma_debuglv_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	unsigned int input = 0;
	int ret;

	ret = kstrtouint(buf, 10, &input);

	dev_notice(dev, "set debug lv = %d\n", input);

	g_edma_log_lv = input;

	return count;
}


DEVICE_ATTR_RW(edma_register);
DEVICE_ATTR_RW(edma_power);
DEVICE_ATTR_RW(edma_debuglv);

static struct attribute *edma_sysfs_entries[] = {
	&dev_attr_edma_register.attr,
	&dev_attr_edma_power.attr,
	&dev_attr_edma_debuglv.attr,
	NULL,
};

static const struct attribute_group edma_attr_group = {
	.attrs = edma_sysfs_entries,
};

int edma_create_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &edma_attr_group);
	if (ret)
		dev_notice(dev, "create sysfs group error, %d\n", ret);

	return ret;
}

void edma_remove_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &edma_attr_group);
}

