/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-01-20 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_SYSFS)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/version.h>

#define REG_RD_ACCESS (1 << 0)

const unsigned char fsm_reg_flag[0xFF] = {
	[0x00] = REG_RD_ACCESS,
	[0x01] = REG_RD_ACCESS,
	[0x03] = REG_RD_ACCESS,
	[0x04] = REG_RD_ACCESS,
	[0x10] = REG_RD_ACCESS,
	[0x11] = REG_RD_ACCESS,
	[0x18] = REG_RD_ACCESS,
	[0x19] = REG_RD_ACCESS,
	[0x1f] = REG_RD_ACCESS,
	[0x21] = REG_RD_ACCESS,
	[0x22] = REG_RD_ACCESS,
	[0x23] = REG_RD_ACCESS,
	[0x28] = REG_RD_ACCESS,
	[0x3e] = REG_RD_ACCESS,
	[0x3f] = REG_RD_ACCESS,
	[0x58] = REG_RD_ACCESS,
	[0x59] = REG_RD_ACCESS,
	[0x5a] = REG_RD_ACCESS,
	[0x5b] = REG_RD_ACCESS,
	[0xbc] = REG_RD_ACCESS,
};

static ssize_t fsm_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	fsm_dev_t *fsm_dev;
	uint16_t reg, val;
	ssize_t len;
	int ret;

	fsm_dev = dev_get_drvdata(dev);
	if (fsm_dev == NULL) {
		pr_err("Not found device\n");
		return -EINVAL;
	}

	len = snprintf(buf, PAGE_SIZE,
			"Device[0x%02x] Dump:\n", fsm_dev->addr);
	for (reg = 0; reg < 0xF0; reg++) {
		if (!(fsm_reg_flag[reg] & REG_RD_ACCESS))
			continue;
		fsm_mutex_lock();
		ret = fsm_reg_read(fsm_dev, reg, &val);
		fsm_mutex_unlock();
		if (ret) {
			pr_err("read reg: 0x%02x failed\n", reg);
			return ret;
		}
		len += snprintf(buf + len, PAGE_SIZE - len,
			"%02Xh:0x%04X\n", reg, val);
	}

	return len;
}

static ssize_t fsm_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	fsm_dev_t *fsm_dev;
	unsigned int databuf[2];
	int ret;

	fsm_dev = dev_get_drvdata(dev);
	if (fsm_dev == NULL) {
		pr_err("Not found device\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) != 2) {
		pr_err("Invalid input data!\n");
		return -EINVAL;
	}

	fsm_mutex_lock();
	ret = fsm_reg_write(fsm_dev, (uint8_t)databuf[0],
			(uint16_t)databuf[1]);
	fsm_mutex_unlock();
	if (ret) {
		pr_err("write 0x%04x to 0x%02x failed\n",
				databuf[1], databuf[0]);
		return ret;
	}

	return len;
}

static ssize_t fsm_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	fsm_version_t version;
	struct preset_file *pfile;
	int dev_count;
	int len = 0;

	fsm_get_version(&version);
	len  = scnprintf(buf + len, PAGE_SIZE, "version: %s\n",
			version.code_version);
	len += scnprintf(buf + len, PAGE_SIZE, "branch : %s\n",
			version.git_branch);
	len += scnprintf(buf + len, PAGE_SIZE, "commit : %s\n",
			version.git_commit);
	len += scnprintf(buf + len, PAGE_SIZE, "date   : %s\n",
			version.code_date);
	pfile = fsm_get_presets();
	dev_count = (pfile ? pfile->hdr.ndev : 0);
	len += scnprintf(buf + len, PAGE_SIZE, "device : [%d, %d]\n",
			dev_count, fsm_dev_count());

	return len;
}

static DEVICE_ATTR_RW(fsm_reg);
static DEVICE_ATTR_RO(fsm_info);

static struct attribute *fsm_attributes[] = {
	&dev_attr_fsm_reg.attr,
	&dev_attr_fsm_info.attr,
	NULL,
};

static const struct attribute_group fsm_attr_group = {
	.attrs = fsm_attributes,
};

int fsm_sysfs_init(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &fsm_attr_group);
}

void fsm_sysfs_deinit(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &fsm_attr_group);
}
#endif
