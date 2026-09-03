/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sysfs.h>

#include "core.h"

static ssize_t freq_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct dbg_log_device *dbg = dev_get_drvdata(dev);
	struct phy_ctx *phy = dbg->phy;
	ssize_t ret = 0;
	int i;
	char temp_sbuf[64] = { 0 };
	char temp_buf[16];

	if (dbg->clk_src[0]) {
		strcat(temp_sbuf, "[");
		for (i = 0; i < CLK_SRC_MAX; i++) {
			if (dbg->clk_src[i]) {
				ret = snprintf(temp_buf, 16, " %u", (unsigned int)(clk_get_rate(dbg->clk_src[i]) / 1000));
				strcat(temp_sbuf, temp_buf);
			}
		}
		strcat(temp_sbuf, "]");
	} else {
		dbg_log_fill_freq_array(dbg, temp_sbuf);
	}

	ret = snprintf(buf, PAGE_SIZE, "%d %s\n", phy->freq, temp_sbuf);
	return ret;
}

static ssize_t freq_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct dbg_log_device *dbg = dev_get_drvdata(dev);
	struct phy_ctx *phy = dbg->phy;
	unsigned int freq;
	u32 channel;
	int ret = 0;

	DEBUG_LOG_PRINT("entry\n");
	ret = kstrtouint(buf, 10, &freq);
	if (ret) {
		pr_err("error: Invalid input for phy freq\n");
		return -EINVAL;
	}
	if (!dbg_log_is_freq_valid(dbg, freq)) {
		pr_err("error: the freq %d not support by this PHY!\n", freq);
		return -EINVAL;
	}
	phy->freq = freq;

	pr_info("input phy freq is %d\n", phy->freq);

	channel = dbg->channel;
	if (channel) {
		dbg->channel = CH_DISABLE;
		dbg_log_channel_sel(dbg);

		dbg->channel = channel;
		dbg_log_channel_sel(dbg);
	} else {
		pr_info("serdes channel was not open\n");
	}

	return count;
}

static DEVICE_ATTR_RW(freq);

static ssize_t channel_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct dbg_log_device *dbg = dev_get_drvdata(dev);
	ssize_t ret = 0;
	int i = 0;
	char temp_sbuf[200] = { 0 }, temp_buf[10];

	for (i = 0; i < dbg->serdes.ch_num; i++) {
		if (dbg->serdes.ch_str[i]) {
			ret =
			    snprintf(temp_buf, 10, " %s",
				     dbg->serdes.ch_str[i]);
			strcat(temp_sbuf, temp_buf);
		}
	}

	if (dbg->channel == CH_DISABLE)
		return snprintf(buf, PAGE_SIZE,
			     STR_CH_DISABLE " [ " STR_CH_DISABLE "%s]\n",
			     temp_sbuf);
	else if (dbg->channel < CH_MAX)
		return snprintf(buf, PAGE_SIZE, "%s [ " STR_CH_DISABLE "%s]\n",
			     dbg->serdes.ch_str[dbg->channel - 1], temp_sbuf);
	else
		return snprintf(buf, PAGE_SIZE, "UNKNOWN [ " STR_CH_DISABLE "%s]\n",
			     temp_sbuf);
}

static ssize_t channel_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct dbg_log_device *dbg = dev_get_drvdata(dev);
	int ret;
	u32 channel;

	DEBUG_LOG_PRINT("entry (%s)\n", buf);

	ret = dbg_log_get_valid_channel(dbg, buf);

	if (-EINVAL == ret) {
		ret = kstrtouint(buf, 10, &channel);
		if (ret) {
			pr_err("error: Invalid input!\n");
			return -EINVAL;
		}

		if (channel >= CH_MAX) {
			pr_err("error: channel not support!\n");
			return -EINVAL;
		}
	} else {
		if (ret < 0) {
			pr_err("error: Can't support the channel %s!\n", buf);
			return -EINVAL;
		}
		channel = ret;
	}

	DEBUG_LOG_PRINT("input serdes channel is %u\n", channel);

	dbg->channel = channel;
	dbg_log_channel_sel(dbg);

	return count;
}

static DEVICE_ATTR_RW(channel);

static struct attribute *dbg_log_attrs[] = {
	&dev_attr_channel.attr,
	&dev_attr_freq.attr,
	NULL,
};

ATTRIBUTE_GROUPS(dbg_log);

int dbg_log_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&dev->kobj, dbg_log_groups);
	if (rc)
		pr_err("create dbg log attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(dbg_log_sysfs_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Bin Ji <bin.ji@spreadtrum.com>");
MODULE_AUTHOR("ken.kuang@spreadtrum.com");
MODULE_AUTHOR("leon.he@spreadtrum.com");
MODULE_DESCRIPTION("Add modem dbg log attribute nodes for userspace");
