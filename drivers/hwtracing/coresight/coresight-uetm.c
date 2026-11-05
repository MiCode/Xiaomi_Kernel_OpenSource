// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/amba/bus.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/bitmap.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/scmi_protocol.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/platform_device.h>

#include "coresight-priv.h"
#include "coresight-common.h"
#include "coresight-trace-noc.h"
#include "coresight-trace-id.h"

#define PARAM_GET_PLATFORM_CONFIG 0x1
#define PARAM_GET_UETM_CONFIG 0x2
#define PARAM_SET_UETM_CONFIG 0x1
#define START_UETM_TRACE 0x1
#define STOP_UETM_TRACE 0x1
#define SCMI_UETM_ALGO_STR (0x5545544d) /*UETM ASCII*/

#define UETM_MAX_STATE 4
#define UETM_MAX_CFG 2
#define UETM_UNCORE_LANE 4

#define LANE_IDX(n) (n / 2)
#define UETM_ATB_CFG_ATID_MASK GENMASK(6, 0)

static struct scmi_device *sdev;
static const struct qcom_scmi_vendor_ops *ops;
static struct scmi_protocol_handle *ph;
static uint32_t uetm_cnt;


struct __packed uetm_platform_config {
	uint32_t uetm_cnt;
};

struct __packed uetm_config {
	uint8_t lane;
	uint64_t base_address;
	uint32_t size;
	uint8_t cluster_id;
	uint8_t core_id;
};

struct __packed uetm_idx {
	uint32_t idx;
};

struct uetm_reg_config {
	u64     ocla_cfg1;
	u64     atb_cfg;
	u64     uetm_cfg;
	u64     dmask_cfg[UETM_MAX_STATE][UETM_MAX_CFG];
	u64     tmask_cfg[UETM_MAX_STATE][UETM_MAX_CFG];
	u64     dmatch_cfg[UETM_MAX_STATE][UETM_MAX_CFG];
	u64     tmatch_cfg[UETM_MAX_STATE][UETM_MAX_CFG];
	u64     st_cfg[UETM_MAX_STATE];
	u64     cntr_cfg[UETM_MAX_STATE][UETM_MAX_CFG];
	u64     ocla_cfg2;
	u64     ocla_cfg;
	u64     diff_dmask_cfg[UETM_MAX_CFG];
};

struct uetm_drvdata {
	void __iomem            *base;
	struct coresight_device	*csdev;
	spinlock_t              spinlock;
	uint8_t                 lane;
	uint64_t                base_address;
	uint32_t                uetm_id;
	u8                      traceid;
	uint32_t                size;
	uint8_t                 core_id;
	uint8_t                 cluster_id;
	uint8_t                 state_idx;
	uint8_t                 lane_idx;
	bool                    enable;
	bool                    uncore_uetm;
	struct uetm_reg_config  *config;
};

DEFINE_CORESIGHT_DEVLIST(uetm_devs, "uetm");

static int uetm_scmi_get_uetm_platform_config(void)
{
	struct uetm_platform_config rx_value;
	int ret;

	ret = ops->get_param(ph, &rx_value, SCMI_UETM_ALGO_STR,
			PARAM_GET_PLATFORM_CONFIG, 0,
			sizeof(struct uetm_platform_config));

	if (ret)
		return ret;
	else
		return rx_value.uetm_cnt;
}

static int uetm_scmi_get_uetm_config(struct uetm_drvdata *drvdata)
{
	int ret = 0;
	uint32_t idx;
	struct uetm_config rx_value;

	for (idx = 0; idx < uetm_cnt; idx++) {
		memcpy(&rx_value, (void *)&idx, sizeof(uint32_t));
		ret = ops->get_param(ph, &rx_value, SCMI_UETM_ALGO_STR,
			PARAM_GET_UETM_CONFIG, sizeof(uint32_t),
			sizeof(struct uetm_config));

		if (ret)
			continue;

		if (drvdata->uncore_uetm) {
			if (rx_value.cluster_id == drvdata->cluster_id
				&& rx_value.lane == UETM_UNCORE_LANE)
				break;
		} else {
			if (rx_value.cluster_id == drvdata->cluster_id
				&& rx_value.core_id == drvdata->core_id)
				break;
		}

	}

	if (idx == uetm_cnt)
		return -EINVAL;
	drvdata->base_address = rx_value.base_address;
	drvdata->size = rx_value.size;
	drvdata->lane = rx_value.lane;
	drvdata->uetm_id = idx;

	return 0;
}

static int uetm_scmi_set_uetm_config(uint32_t uetm_id)
{
	struct uetm_idx uetm_idx;

	uetm_idx.idx = uetm_id;
	return ops->set_param(ph, &uetm_idx, SCMI_UETM_ALGO_STR,
			PARAM_SET_UETM_CONFIG, sizeof(struct uetm_idx));
}

static int uetm_scmi_start_uetm_trace(uint32_t uetm_id)
{
	struct uetm_idx uetm_idx;

	uetm_idx.idx = uetm_id;
	return ops->start_activity(ph, &uetm_id, SCMI_UETM_ALGO_STR,
			START_UETM_TRACE, sizeof(uint32_t));
}

static int uetm_scmi_stop_uetm_trace(uint32_t uetm_id)
{
	struct uetm_idx uetm_idx;

	uetm_idx.idx = uetm_id;
	return ops->stop_activity(ph, &uetm_id, SCMI_UETM_ALGO_STR,
			STOP_UETM_TRACE, sizeof(uint32_t));
}

static int uetm_scmi_init(void)
{
	int val, ret = 0;

	sdev = get_qcom_scmi_device();

	if (IS_ERR(sdev)) {
		ret = PTR_ERR(sdev);
		pr_err("Error getting scmi_dev ret = %d\n", ret);
		return ret;
	}

	ops = sdev->handle->devm_protocol_get(sdev,
				QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	if (IS_ERR(ops)) {
		pr_err("Error getting qcom_smci_vendor_protocal\n");
		return -EFAULT;
	}
	val = uetm_scmi_get_uetm_platform_config();

	if (val > 0)
		uetm_cnt = val;
	else
		uetm_cnt = 0;

	return ret;
}

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;

	spin_lock(&drvdata->spinlock);
	memset(config, 0, sizeof(struct uetm_reg_config));
	drvdata->lane_idx = 0;
	drvdata->state_idx = 0;
	spin_unlock(&drvdata->spinlock);

	return size;
};
static DEVICE_ATTR_WO(reset);

static ssize_t diff_dmask_cfg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	lane_idx = LANE_IDX(drvdata->lane_idx);
	val = config->diff_dmask_cfg[lane_idx];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t diff_dmask_cfg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	spin_lock(&drvdata->spinlock);
	lane_idx = LANE_IDX(drvdata->lane_idx);
	config->diff_dmask_cfg[lane_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(diff_dmask_cfg);

static ssize_t ocla_cfg1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	val = config->ocla_cfg1;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t ocla_cfg1_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->ocla_cfg1 = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(ocla_cfg1);

static ssize_t ocla_cfg2_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	val = config->ocla_cfg2;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t ocla_cfg2_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	spin_lock(&drvdata->spinlock);
	config->ocla_cfg2 = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(ocla_cfg2);

static ssize_t cntr_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	lane_idx = LANE_IDX(drvdata->lane_idx);
	val = config->cntr_cfg[drvdata->state_idx][lane_idx];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t cntr_cfg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	spin_lock(&drvdata->spinlock);
	lane_idx = LANE_IDX(drvdata->lane_idx);
	config->cntr_cfg[drvdata->state_idx][lane_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(cntr_cfg);

static ssize_t st_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	val = config->st_cfg[drvdata->state_idx];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t st_cfg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->st_cfg[drvdata->state_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(st_cfg);

static ssize_t tmatch_cfg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	lane_idx = LANE_IDX(drvdata->lane_idx);
	val = config->tmatch_cfg[drvdata->state_idx][lane_idx];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tmatch_cfg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	lane_idx = LANE_IDX(drvdata->lane_idx);
	config->tmatch_cfg[drvdata->state_idx][lane_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(tmatch_cfg);

static ssize_t dmatch_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	lane_idx = LANE_IDX(drvdata->lane_idx);
	val = config->dmatch_cfg[drvdata->state_idx][lane_idx];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t dmatch_cfg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	lane_idx = LANE_IDX(drvdata->lane_idx);
	config->dmatch_cfg[drvdata->state_idx][lane_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(dmatch_cfg);

static ssize_t tmask_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	lane_idx = LANE_IDX(drvdata->lane_idx);
	val = config->tmask_cfg[drvdata->state_idx][lane_idx];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tmask_cfg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	lane_idx = LANE_IDX(drvdata->lane_idx);
	config->tmask_cfg[drvdata->state_idx][lane_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(tmask_cfg);

static ssize_t dmask_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	lane_idx = LANE_IDX(drvdata->lane_idx);
	val = config->dmask_cfg[drvdata->state_idx][lane_idx];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t dmask_cfg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;
	int lane_idx;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	lane_idx = LANE_IDX(drvdata->lane_idx);
	config->dmask_cfg[drvdata->state_idx][lane_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(dmask_cfg);

static ssize_t uetm_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	val = config->uetm_cfg;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t uetm_cfg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->uetm_cfg = (u64)val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(uetm_cfg);

static ssize_t atb_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	val = config->atb_cfg;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t atb_cfg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->atb_cfg = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(atb_cfg);

static ssize_t ocla_cfg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	val = config->ocla_cfg;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t ocla_cfg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct uetm_reg_config *config = drvdata->config;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->ocla_cfg = (u64)val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(ocla_cfg);

static ssize_t lane_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%#hhx\n", drvdata->lane_idx);
}

static ssize_t lane_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val > drvdata->lane)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->lane_idx = val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(lane);

static ssize_t state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%#hhx\n", drvdata->state_idx);
}

static ssize_t state_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= 4)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->state_idx = val;
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(state);

static ssize_t traceid_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct uetm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->traceid;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(traceid);

static struct attribute *uetm_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_diff_dmask_cfg.attr,
	&dev_attr_ocla_cfg1.attr,
	&dev_attr_ocla_cfg2.attr,
	&dev_attr_cntr_cfg.attr,
	&dev_attr_st_cfg.attr,
	&dev_attr_tmatch_cfg.attr,
	&dev_attr_dmatch_cfg.attr,
	&dev_attr_tmask_cfg.attr,
	&dev_attr_dmask_cfg.attr,
	&dev_attr_uetm_cfg.attr,
	&dev_attr_atb_cfg.attr,
	&dev_attr_ocla_cfg.attr,
	&dev_attr_lane.attr,
	&dev_attr_state.attr,
	&dev_attr_traceid.attr,
	NULL,
};

static const struct attribute_group uetm_group = {
	.attrs = uetm_attrs,
};

const struct attribute_group *uetm_groups[] = {
	&uetm_group,
	NULL,
};

static void uetm_store_config(struct uetm_drvdata *drvdata)
{
	int i, j;
	u64 *base = drvdata->base;
	int cfg_num;
	struct uetm_reg_config *config = drvdata->config;

	cfg_num = drvdata->lane / 2;

	if (drvdata->uncore_uetm)
		*base++ = config->ocla_cfg1;
	else
		*base++ = config->ocla_cfg;
	*base++ = config->atb_cfg;
	*base++ = config->uetm_cfg;

	for (i = 0; i < UETM_MAX_STATE; i++)
		for (j = 0; j < cfg_num; j++)
			*base++ = config->dmask_cfg[i][j];

	for (i = 0; i < UETM_MAX_STATE; i++)
		for (j = 0; j < cfg_num; j++)
			*base++ = config->tmask_cfg[i][j];

	for (i = 0; i < UETM_MAX_STATE; i++)
		for (j = 0; j < cfg_num; j++)
			*base++ = config->dmatch_cfg[i][j];

	for (i = 0; i < UETM_MAX_STATE; i++)
		for (j = 0; j < cfg_num; j++)
			*base++ = config->tmatch_cfg[i][j];

	for (i = 0; i < UETM_MAX_STATE; i++)
		*base++ = config->st_cfg[i];

	for (i = 0; i < UETM_MAX_STATE; i++)
		for (j = 0; j < cfg_num; j++)
			*base++ = config->cntr_cfg[i][j];

	if (drvdata->uncore_uetm) {
		*base++ = config->ocla_cfg2;
		*base++ = config->ocla_cfg;
	}

	for (j = 0; j < cfg_num; j++)
		*base++ = config->diff_dmask_cfg[j];
	/* Wait for config to settle */
	mb();
}
static int uetm_enable(struct coresight_device *csdev,
			struct perf_event *event, enum cs_mode mode)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct uetm_reg_config *config = drvdata->config;
	int ret, trace_id;

	if (drvdata->enable) {
		dev_err(&csdev->dev,
		"uetm %d already enabled,Skipping enable\n",
		drvdata->uetm_id);
		return -EBUSY;
	}

	trace_id = coresight_trace_id_get_system_id();
	if (trace_id < 0)
		return trace_id;

	drvdata->traceid = (u8)trace_id;
	config->atb_cfg &= ~UETM_ATB_CFG_ATID_MASK;
	config->atb_cfg |= drvdata->traceid;

	spin_lock(&drvdata->spinlock);
	uetm_store_config(drvdata);
	spin_unlock(&drvdata->spinlock);

	coresight_csr_set_etr_atid(csdev, drvdata->traceid, true, NULL);

	ret = uetm_scmi_set_uetm_config(drvdata->uetm_id);

	if (ret)
		goto release_atid;

	ret = uetm_scmi_start_uetm_trace(drvdata->uetm_id);
	if (ret)
		goto release_atid;

	drvdata->enable = true;
	return 0;

release_atid:
	coresight_trace_id_put_system_id(drvdata->traceid);
	coresight_csr_set_etr_atid(csdev, drvdata->traceid, false, NULL);
	return ret;
};

static void uetm_disable(struct coresight_device *csdev,
			 struct perf_event *event)
{
	struct uetm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	uetm_scmi_stop_uetm_trace(drvdata->uetm_id);
	coresight_trace_id_put_system_id(drvdata->traceid);
	coresight_csr_set_etr_atid(csdev, drvdata->traceid, false, NULL);
	drvdata->enable = false;
};

static const struct coresight_ops_source uetm_source_ops = {
	.enable		= uetm_enable,
	.disable	= uetm_disable,
};

static const struct coresight_ops uetm_cs_ops = {
	.source_ops	= &uetm_source_ops,
};

static int uetm_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct uetm_drvdata *drvdata;
	struct uetm_reg_config *config;
	uint32_t value;
	struct coresight_desc desc = { 0 };

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	config = devm_kzalloc(dev, sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	drvdata->config = config;

	dev_set_drvdata(dev, drvdata);
	ret = of_property_read_u32(pdev->dev.of_node, "cluster",
		&value);
	if (ret)
		return ret;

	drvdata->cluster_id = (uint8_t)value;
	drvdata->uncore_uetm = of_property_read_bool(pdev->dev.of_node,
			"qcom,uncore_uetm");

	if (!drvdata->uncore_uetm) {
		ret = of_property_read_u32(pdev->dev.of_node, "core",
		&value);
		if (ret)
			return ret;
		drvdata->core_id = (uint8_t)value;
	}

	ret = uetm_scmi_get_uetm_config(drvdata);
	if (ret)
		return ret;

	spin_lock_init(&drvdata->spinlock);

	drvdata->base = devm_ioremap(dev, drvdata->base_address, drvdata->size);
	if (!drvdata->base)
		return -ENOMEM;

	pdata = coresight_get_platform_data(dev);
	desc.name = coresight_alloc_device_name(&uetm_devs, dev);
	if (!desc.name)
		return -ENOMEM;

	desc.dev = dev;
	desc.pdata = pdata;
	desc.ops = &uetm_cs_ops;
	desc.groups = uetm_groups;
	desc.type = CORESIGHT_DEV_TYPE_SOURCE;
	desc.subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE;
	drvdata->csdev = coresight_register(&desc);
	ret = PTR_ERR_OR_ZERO(drvdata->csdev);
	if (ret)
		return ret;

	return 0;
}

static int uetm_remove(struct platform_device *pdev)
{
	struct uetm_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static const struct of_device_id uetm_match[] = {
	{.compatible = "qcom,coresight-uetm"},
	{}
};

static struct platform_driver uetm_driver = {
	.probe          = uetm_probe,
	.remove         = uetm_remove,
	.driver         = {
	.name   = "coresight-uetm",
	.of_match_table = uetm_match,
	.suppress_bind_attrs = true,
	},
};

static int __init uetm_init(void)
{
	int ret;

	ret = uetm_scmi_init();

	if (ret)
		return ret;

	return platform_driver_register(&uetm_driver);
};

static void __exit uetm_exit(void)
{
	platform_driver_unregister(&uetm_driver);
};

module_init(uetm_init);
module_exit(uetm_exit)

MODULE_LICENSE("GPL");
