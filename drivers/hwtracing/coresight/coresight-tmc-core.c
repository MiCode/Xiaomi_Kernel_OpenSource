// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2012, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Description: CoreSight Trace Memory Controller driver
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/amba/bus.h>
#include <linux/cpu_pm.h>
#include <linux/pm_domain.h>

#include "coresight-priv.h"
#include "coresight-tmc.h"
#include "coresight-common.h"

DEFINE_CORESIGHT_DEVLIST(etb_devs, "tmc_etb");
DEFINE_CORESIGHT_DEVLIST(etf_devs, "tmc_etf");
DEFINE_CORESIGHT_DEVLIST(etr_devs, "tmc_etr");

static LIST_HEAD(delay_probe_list);
static LIST_HEAD(cpu_pm_list);
static enum cpuhp_state hp_online;
static DEFINE_SPINLOCK(delay_lock);

int tmc_wait_for_tmcready(struct tmc_drvdata *drvdata)
{
	struct coresight_device *csdev = drvdata->csdev;
	struct csdev_access *csa = &csdev->access;

	/* Ensure formatter, unformatter and hardware fifo are empty */
	if (coresight_timeout(csa, TMC_STS, TMC_STS_TMCREADY_BIT, 1)) {
		dev_err(&csdev->dev,
			"timeout while waiting for TMC to be Ready\n");
		return -EBUSY;
	}
	return 0;
}

void tmc_flush_and_stop(struct tmc_drvdata *drvdata)
{
	struct coresight_device *csdev = drvdata->csdev;
	struct csdev_access *csa = &csdev->access;
	u32 ffcr;

	ffcr = readl_relaxed(drvdata->base + TMC_FFCR);
	ffcr |= TMC_FFCR_STOP_ON_FLUSH;
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);
	ffcr |= BIT(TMC_FFCR_FLUSHMAN_BIT);
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);
	/* Ensure flush completes */
	if (coresight_timeout(csa, TMC_FFCR, TMC_FFCR_FLUSHMAN_BIT, 0)) {
		dev_err(&csdev->dev,
		"timeout while waiting for completion of Manual Flush\n");
	}

	tmc_wait_for_tmcready(drvdata);
}

void tmc_disable_stop_on_flush(struct tmc_drvdata *drvdata)
{
	drvdata->stop_on_flush = false;
}

void tmc_enable_hw(struct tmc_drvdata *drvdata)
{
	writel_relaxed(TMC_CTL_CAPT_EN, drvdata->base + TMC_CTL);
}

void tmc_disable_hw(struct tmc_drvdata *drvdata)
{
	writel_relaxed(0x0, drvdata->base + TMC_CTL);
}

u32 tmc_get_memwidth_mask(struct tmc_drvdata *drvdata)
{
	u32 mask = 0;

	/*
	 * When moving RRP or an offset address forward, the new values must
	 * be byte-address aligned to the width of the trace memory databus
	 * _and_ to a frame boundary (16 byte), whichever is the biggest. For
	 * example, for 32-bit, 64-bit and 128-bit wide trace memory, the four
	 * LSBs must be 0s. For 256-bit wide trace memory, the five LSBs must
	 * be 0s.
	 */
	switch (drvdata->memwidth) {
	case TMC_MEM_INTF_WIDTH_32BITS:
	case TMC_MEM_INTF_WIDTH_64BITS:
	case TMC_MEM_INTF_WIDTH_128BITS:
		mask = GENMASK(31, 4);
		break;
	case TMC_MEM_INTF_WIDTH_256BITS:
		mask = GENMASK(31, 5);
		break;
	}

	return mask;
}

static int tmc_read_prepare(struct tmc_drvdata *drvdata)
{
	int ret = 0;

	switch (drvdata->config_type) {
	case TMC_CONFIG_TYPE_ETB:
	case TMC_CONFIG_TYPE_ETF:
		ret = tmc_read_prepare_etb(drvdata);
		break;
	case TMC_CONFIG_TYPE_ETR:
		ret = tmc_read_prepare_etr(drvdata);
		break;
	default:
		ret = -EINVAL;
	}

	if (!ret)
		dev_dbg(&drvdata->csdev->dev, "TMC read start\n");

	return ret;
}

static int tmc_read_unprepare(struct tmc_drvdata *drvdata)
{
	int ret = 0;

	switch (drvdata->config_type) {
	case TMC_CONFIG_TYPE_ETB:
	case TMC_CONFIG_TYPE_ETF:
		ret = tmc_read_unprepare_etb(drvdata);
		break;
	case TMC_CONFIG_TYPE_ETR:
		ret = tmc_read_unprepare_etr(drvdata);
		break;
	default:
		ret = -EINVAL;
	}

	if (!ret)
		dev_dbg(&drvdata->csdev->dev, "TMC read end\n");

	return ret;
}

static int tmc_open(struct inode *inode, struct file *file)
{
	int ret;
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);

	ret = tmc_read_prepare(drvdata);
	if (ret)
		return ret;

	nonseekable_open(inode, file);

	dev_dbg(&drvdata->csdev->dev, "%s: successfully opened\n", __func__);
	return 0;
}

static inline ssize_t tmc_get_sysfs_trace(struct tmc_drvdata *drvdata,
					  loff_t pos, size_t len, char **bufpp)
{
	switch (drvdata->config_type) {
	case TMC_CONFIG_TYPE_ETB:
	case TMC_CONFIG_TYPE_ETF:
		return tmc_etb_get_sysfs_trace(drvdata, pos, len, bufpp);
	case TMC_CONFIG_TYPE_ETR:
		return tmc_etr_get_sysfs_trace(drvdata, pos, len, bufpp);
	}

	return -EINVAL;
}

static ssize_t tmc_read(struct file *file, char __user *data, size_t len,
			loff_t *ppos)
{
	char *bufp;
	ssize_t actual;
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);

	mutex_lock(&drvdata->mem_lock);
	actual = tmc_get_sysfs_trace(drvdata, *ppos, len, &bufp);
	if (actual <= 0) {
		mutex_unlock(&drvdata->mem_lock);
		return 0;
	}

	if (copy_to_user(data, bufp, actual)) {
		dev_dbg(&drvdata->csdev->dev,
			"%s: copy_to_user failed\n", __func__);
		mutex_unlock(&drvdata->mem_lock);
		return -EFAULT;
	}

	*ppos += actual;
	dev_dbg(&drvdata->csdev->dev, "%zu bytes copied\n", actual);

	mutex_unlock(&drvdata->mem_lock);
	return actual;
}

static int tmc_release(struct inode *inode, struct file *file)
{
	int ret;
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);

	ret = tmc_read_unprepare(drvdata);
	if (ret)
		return ret;

	dev_dbg(&drvdata->csdev->dev, "%s: released\n", __func__);
	return 0;
}

static const struct file_operations tmc_fops = {
	.owner		= THIS_MODULE,
	.open		= tmc_open,
	.read		= tmc_read,
	.release	= tmc_release,
	.llseek		= no_llseek,
};

static enum tmc_mem_intf_width tmc_get_memwidth(u32 devid)
{
	enum tmc_mem_intf_width memwidth;

	/*
	 * Excerpt from the TRM:
	 *
	 * DEVID::MEMWIDTH[10:8]
	 * 0x2 Memory interface databus is 32 bits wide.
	 * 0x3 Memory interface databus is 64 bits wide.
	 * 0x4 Memory interface databus is 128 bits wide.
	 * 0x5 Memory interface databus is 256 bits wide.
	 */
	switch (BMVAL(devid, 8, 10)) {
	case 0x2:
		memwidth = TMC_MEM_INTF_WIDTH_32BITS;
		break;
	case 0x3:
		memwidth = TMC_MEM_INTF_WIDTH_64BITS;
		break;
	case 0x4:
		memwidth = TMC_MEM_INTF_WIDTH_128BITS;
		break;
	case 0x5:
		memwidth = TMC_MEM_INTF_WIDTH_256BITS;
		break;
	default:
		memwidth = 0;
	}

	return memwidth;
}

static ssize_t coresight_tmc_reg32_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct cs_off_attribute *cs_attr = container_of(attr, struct cs_off_attribute, attr);
	int ret;
	u32 val;

	ret = pm_runtime_resume_and_get(dev->parent);
	if (ret < 0)
		return ret;

	spin_lock(&drvdata->spinlock);
	if (!drvdata->pm_config.hw_powered) {
		ret = -EINVAL;
		goto out;
	}

	val = readl_relaxed(drvdata->base + cs_attr->off);
out:
	spin_unlock(&drvdata->spinlock);
	pm_runtime_put_sync(dev->parent);
	if (ret)
		return ret;
	else
		return sysfs_emit(buf, "0x%x\n", val);
}
static ssize_t coresight_tmc_reg64_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct cs_pair_attribute *cs_attr = container_of(attr, struct cs_pair_attribute, attr);
	int ret;
	u64 val;

	ret = pm_runtime_resume_and_get(dev->parent);
	if (ret < 0)
		return ret;

	spin_lock(&drvdata->spinlock);
	if (!drvdata->pm_config.hw_powered) {
		ret = -EINVAL;
		goto out;
	}

	val = readl_relaxed(drvdata->base + cs_attr->lo_off) |
			((u64)readl_relaxed(drvdata->base + cs_attr->hi_off) << 32);
out:
	spin_unlock(&drvdata->spinlock);
	pm_runtime_put_sync(dev->parent);

	if (ret)
		return ret;
	else
		return sysfs_emit(buf, "0x%llx\n", val);
}

#define coresight_tmc_reg32(name, offset)				\
	(&((struct cs_off_attribute[]) {				\
	   {								\
		__ATTR(name, 0444, coresight_tmc_reg32_show, NULL),	\
		offset							\
	   }								\
	})[0].attr.attr)
#define coresight_tmc_reg64(name, lo_off, hi_off)			\
	(&((struct cs_pair_attribute[]) {				\
	   {								\
		__ATTR(name, 0444, coresight_tmc_reg64_show, NULL),	\
		lo_off, hi_off						\
	   }								\
	})[0].attr.attr)
static struct attribute *coresight_tmc_mgmt_attrs[] = {
	coresight_tmc_reg32(rsz, TMC_RSZ),
	coresight_tmc_reg32(sts, TMC_STS),
	coresight_tmc_reg64(rrp, TMC_RRP, TMC_RRPHI),
	coresight_tmc_reg64(rwp, TMC_RWP, TMC_RWPHI),
	coresight_tmc_reg32(trg, TMC_TRG),
	coresight_tmc_reg32(ctl, TMC_CTL),
	coresight_tmc_reg32(ffsr, TMC_FFSR),
	coresight_tmc_reg32(ffcr, TMC_FFCR),
	coresight_tmc_reg32(mode, TMC_MODE),
	coresight_tmc_reg32(pscr, TMC_PSCR),
	coresight_tmc_reg32(devid, CORESIGHT_DEVID),
	coresight_tmc_reg64(dba, TMC_DBALO, TMC_DBAHI),
	coresight_tmc_reg32(axictl, TMC_AXICTL),
	coresight_tmc_reg32(authstatus, TMC_AUTHSTATUS),
	NULL,
};

static ssize_t trigger_cntr_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->trigger_cntr;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t trigger_cntr_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	drvdata->trigger_cntr = val;
	return size;
}
static DEVICE_ATTR_RW(trigger_cntr);

static ssize_t buffer_size_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%#x\n", drvdata->size);
}

static ssize_t buffer_size_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	/* Only permitted for TMC-ETRs */
	if (drvdata->config_type != TMC_CONFIG_TYPE_ETR)
		return -EPERM;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;
	/* The buffer size should be page aligned */
	if (val & (PAGE_SIZE - 1))
		return -EINVAL;
	drvdata->size = val;
	return size;
}

static DEVICE_ATTR_RW(buffer_size);

static ssize_t block_size_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	uint32_t val = 0;

	if (drvdata->byte_cntr)
		val = drvdata->byte_cntr->block_size;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			val);
}

static ssize_t block_size_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (!drvdata->byte_cntr)
		return -EINVAL;

	if (val && val < 4096) {
		pr_err("Assign minimum block size of 4096 bytes\n");
		return -EINVAL;
	}

	mutex_lock(&drvdata->byte_cntr->byte_cntr_lock);
	drvdata->byte_cntr->block_size = val;
	mutex_unlock(&drvdata->byte_cntr->byte_cntr_lock);

	return size;
}
static DEVICE_ATTR_RW(block_size);

static ssize_t out_mode_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			str_tmc_etr_out_mode[drvdata->out_mode]);
}

static ssize_t out_mode_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	int ret;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	ret = tmc_etr_switch_mode(drvdata, str);
	return ret ? ret : size;
}
static DEVICE_ATTR_RW(out_mode);

static ssize_t stop_on_flush_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	u32 val;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (drvdata->stop_on_flush)
		val = 1;
	else
		val = 0;

	return scnprintf(buf, PAGE_SIZE, "%x\n", val);
}

static ssize_t stop_on_flush_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	unsigned long val;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if ((kstrtoul(buf, 0, &val)) || (val & ~1UL))
		return -EINVAL;

	if (val)
		drvdata->stop_on_flush = true;
	else
		drvdata->stop_on_flush = false;

	return size;
}
static DEVICE_ATTR_RW(stop_on_flush);

static struct attribute *coresight_tmc_etr_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	&dev_attr_buffer_size.attr,
	&dev_attr_block_size.attr,
	&dev_attr_out_mode.attr,
	&dev_attr_stop_on_flush.attr,
	NULL,
};

static struct attribute *coresight_tmc_etf_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	&dev_attr_stop_on_flush.attr,
	NULL,
};

static const struct attribute_group coresight_tmc_etr_group = {
	.attrs = coresight_tmc_etr_attrs,
};

static const struct attribute_group coresight_tmc_etf_group = {
	.attrs = coresight_tmc_etf_attrs,
};


static const struct attribute_group coresight_tmc_mgmt_group = {
	.attrs = coresight_tmc_mgmt_attrs,
	.name = "mgmt",
};

static const struct attribute_group *coresight_tmc_etr_groups[] = {
	&coresight_tmc_etr_group,
	&coresight_tmc_mgmt_group,
	NULL,
};

static const struct attribute_group *coresight_tmc_etf_groups[] = {
	&coresight_tmc_etf_group,
	&coresight_tmc_mgmt_group,
	NULL,
};

static inline bool tmc_etr_can_use_sg(struct device *dev)
{
	return fwnode_property_present(dev->fwnode, "arm,scatter-gather");
}

static inline bool tmc_etr_has_non_secure_access(struct tmc_drvdata *drvdata)
{
	u32 auth = readl_relaxed(drvdata->base + TMC_AUTHSTATUS);

	return (auth & TMC_AUTH_NSID_MASK) == 0x3;
}

/* Detect and initialise the capabilities of a TMC ETR */
static int tmc_etr_setup_caps(struct device *parent, u32 devid, void *dev_caps)
{
	int rc;
	u32 dma_mask = 0;
	struct tmc_drvdata *drvdata = dev_get_drvdata(parent);

	if (!tmc_etr_has_non_secure_access(drvdata))
		return -EACCES;

	/* Set the unadvertised capabilities */
	tmc_etr_init_caps(drvdata, (u32)(unsigned long)dev_caps);

	if (!(devid & TMC_DEVID_NOSCAT) && tmc_etr_can_use_sg(parent))
		tmc_etr_set_cap(drvdata, TMC_ETR_SG);

	/* Check if the AXI address width is available */
	if (devid & TMC_DEVID_AXIAW_VALID)
		dma_mask = ((devid >> TMC_DEVID_AXIAW_SHIFT) &
				TMC_DEVID_AXIAW_MASK);

	/*
	 * Unless specified in the device configuration, ETR uses a 40-bit
	 * AXI master in place of the embedded SRAM of ETB/ETF.
	 */
	switch (dma_mask) {
	case 32:
	case 40:
	case 44:
	case 48:
	case 52:
		dev_info(parent, "Detected dma mask %dbits\n", dma_mask);
		break;
	default:
		dma_mask = 40;
	}

	rc = dma_set_mask_and_coherent(parent, DMA_BIT_MASK(dma_mask));
	if (rc)
		dev_err(parent, "Failed to setup DMA mask: %d\n", rc);
	return rc;
}

static u32 tmc_etr_get_default_buffer_size(struct device *dev)
{
	u32 size;

	if (fwnode_property_read_u32(dev->fwnode, "arm,buffer-size", &size))
		size = SZ_1M;
	return size;
}

static u32 tmc_etr_get_max_burst_size(struct device *dev)
{
	u32 burst_size;

	if (fwnode_property_read_u32(dev->fwnode, "arm,max-burst-size",
				     &burst_size))
		return TMC_AXICTL_WR_BURST_16;

	/* Only permissible values are 0 to 15 */
	if (burst_size > 0xF)
		burst_size = TMC_AXICTL_WR_BURST_16;

	return burst_size;
}

static int tmc_add_coresight_dev(struct amba_device *adev, const struct amba_id *id)
{
	int ret = 0;
	u32 devid;
	void __iomem *base;
	struct device *dev = &adev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct tmc_drvdata *drvdata;
	struct resource *res = &adev->res;
	struct coresight_desc desc = { 0 };
	struct coresight_dev_list *dev_list = NULL;

	ret = -ENOMEM;
	drvdata = dev_get_drvdata(dev);
	if (!drvdata)
		goto out;

	/* Validity for the resource is already checked by the AMBA core */
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		ret = PTR_ERR(base);
		goto out;
	}

	drvdata->atclk = devm_clk_get(dev, "atclk"); /* optional */
	if (IS_ERR(drvdata->atclk) &&
			of_property_read_bool(dev->of_node, "qcom,atclk-dependence")) {
		dev_err(dev, "get atclk fail %ld\n",  PTR_ERR(drvdata->atclk));
		return  PTR_ERR(drvdata->atclk);
	}

	if (!IS_ERR(drvdata->atclk)) {
		ret = clk_prepare_enable(drvdata->atclk);
		if (ret)
			return ret == -ETIMEDOUT ? -EPROBE_DEFER : ret;
	}

	drvdata->base = base;
	desc.access = CSDEV_ACCESS_IOMEM(base);

	spin_lock_init(&drvdata->spinlock);
	mutex_init(&drvdata->mem_lock);

	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	drvdata->config_type = BMVAL(devid, 6, 7);
	drvdata->memwidth = tmc_get_memwidth(devid);
	/* This device is not associated with a session */
	drvdata->pid = -1;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
		drvdata->size = tmc_etr_get_default_buffer_size(dev);
		drvdata->max_burst_size = tmc_etr_get_max_burst_size(dev);
	} else {
		drvdata->size = readl_relaxed(drvdata->base + TMC_RSZ) * 4;
	}

	ret = of_get_coresight_csr_name(adev->dev.of_node, &drvdata->csr_name);
	if (ret)
		dev_dbg(dev, "No csr data\n");
	else {
		drvdata->csr = coresight_csr_get(drvdata->csr_name);
		if (IS_ERR(drvdata->csr)) {
			dev_dbg(dev, "failed to get csr, defer probe\n");
			ret = -EPROBE_DEFER;
			goto out_disable_clk;
		}
	}

	desc.dev = dev;
	drvdata->stop_on_flush = false;

	switch (drvdata->config_type) {
	case TMC_CONFIG_TYPE_ETB:
		desc.type = CORESIGHT_DEV_TYPE_SINK;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
		desc.ops = &tmc_etb_cs_ops;
		desc.groups = coresight_tmc_etf_groups;
		dev_list = &etb_devs;
		break;
	case TMC_CONFIG_TYPE_ETR:
		desc.type = CORESIGHT_DEV_TYPE_SINK;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_SYSMEM;
		desc.ops = &tmc_etr_cs_ops;
		desc.groups = coresight_tmc_etr_groups;
		ret = tmc_etr_setup_caps(dev, devid,
					 coresight_get_uci_data(id));
		if (ret)
			goto out_disable_clk;

		idr_init(&drvdata->idr);
		mutex_init(&drvdata->idr_mutex);
		dev_list = &etr_devs;

		drvdata->byte_cntr = byte_cntr_init(adev, drvdata);

		ret = tmc_etr_usb_init(adev, drvdata);
		if (ret)
			goto out_disable_clk;

		break;
	case TMC_CONFIG_TYPE_ETF:
		desc.type = CORESIGHT_DEV_TYPE_LINKSINK;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
		desc.subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_FIFO;
		desc.ops = &tmc_etf_cs_ops;
		desc.groups = coresight_tmc_etf_groups;
		dev_list = &etf_devs;
		break;
	default:
		pr_err("%s: Unsupported TMC config\n", desc.name);
		ret = -EINVAL;
		goto out_disable_clk;
	}

	desc.name = coresight_alloc_device_name(dev_list, dev);
	if (!desc.name) {
		ret = -ENOMEM;
		goto out_disable_clk;
	}

	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto out_disable_clk;
	}
	adev->dev.platform_data = pdata;
	desc.pdata = pdata;

	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto out_disable_clk;
	}

	drvdata->miscdev.name = desc.name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &tmc_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		coresight_unregister(drvdata->csdev);
	else {
		drvdata->pm_config.hw_powered = true;
		pm_runtime_put_sync(&adev->dev);
	}

out_disable_clk:
	if (ret && !IS_ERR_OR_NULL(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);
out:
	return ret;
}

#ifdef CONFIG_PM
static int tmc_runtime_suspend(struct device *dev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);

	return 0;
}

static int tmc_runtime_resume(struct device *dev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_prepare_enable(drvdata->atclk);

	return 0;
}
#endif

static const struct dev_pm_ops tmc_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(tmc_runtime_suspend, tmc_runtime_resume, NULL)
};


static int tmc_cpu_pm_notify(struct notifier_block *nb, unsigned long cmd,
			      void *v)
{
	unsigned int cpu = smp_processor_id();
	struct tmc_drvdata *drvdata, *tmp;
	struct pm_config *pm_config;
	unsigned long flags;

	switch (cmd) {
	case CPU_PM_ENTER:
		list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
			pm_config = &drvdata->pm_config;
			if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
				continue;

			spin_lock_irqsave(&drvdata->spinlock, flags);
			if (!cpumask_test_cpu(cpu, &pm_config->online_cpus)) {
				spin_unlock_irqrestore(&drvdata->spinlock, flags);
				continue;
			}

			cpumask_clear_cpu(cpu, &pm_config->powered_cpus);
			if (cpumask_empty(&pm_config->powered_cpus))
				pm_config->hw_powered = false;

			spin_unlock_irqrestore(&drvdata->spinlock, flags);
		}
		break;
	case CPU_PM_EXIT:
	case CPU_PM_ENTER_FAILED:
		list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
			pm_config = &drvdata->pm_config;
			if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
				continue;
			spin_lock_irqsave(&drvdata->spinlock, flags);

			if (!cpumask_test_cpu(cpu, &pm_config->online_cpus)) {
				spin_unlock_irqrestore(&drvdata->spinlock, flags);
				continue;
			}

			pm_config->hw_powered = true;
			cpumask_set_cpu(cpu, &pm_config->powered_cpus);
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
		}
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block tmc_cpu_pm_nb = {
	.notifier_call = tmc_cpu_pm_notify,
};

static int tmc_offline_cpu(unsigned int cpu)
{
	struct tmc_drvdata *drvdata, *tmp;
	struct pm_config *pm_config;
	unsigned long flags;

	list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
		pm_config = &drvdata->pm_config;
		if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
			continue;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		cpumask_clear_cpu(cpu, &pm_config->online_cpus);
		cpumask_clear_cpu(cpu, &pm_config->powered_cpus);
		if (cpumask_empty(&pm_config->powered_cpus))
			pm_config->hw_powered = false;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
	}
	return 0;
}

static int tmc_online_cpu(unsigned int cpu)
{
	int ret;
	struct tmc_drvdata *drvdata, *tmp;
	struct pm_config *pm_config;
	unsigned long flags;
	struct delay_probe_arg *init_arg, *arg_tmp;

	list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
		pm_config = &drvdata->pm_config;
		if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
			continue;
		spin_lock_irqsave(&drvdata->spinlock, flags);
		cpumask_set_cpu(cpu, &pm_config->powered_cpus);
		cpumask_set_cpu(cpu, &pm_config->online_cpus);
		pm_config->hw_powered = true;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
	}

	list_for_each_entry_safe(init_arg, arg_tmp, &delay_probe_list, link) {
		if (cpumask_test_cpu(cpu, init_arg->cpumask)) {
			drvdata = amba_get_drvdata(init_arg->adev);
			pm_config = &drvdata->pm_config;
			spin_lock(&delay_lock);
			drvdata->delayed = NULL;
			list_del(&init_arg->link);
			spin_unlock(&delay_lock);
			ret = pm_runtime_resume_and_get(&init_arg->adev->dev);
			if (ret < 0)
				return ret;
			ret = tmc_add_coresight_dev(init_arg->adev, init_arg->id);
			if (ret)
				pm_runtime_put_sync(&init_arg->adev->dev);
			else {
				pm_config->pd_cpumask = init_arg->cpumask;
				cpumask_set_cpu(cpu, &pm_config->powered_cpus);
				cpumask_set_cpu(cpu, &pm_config->online_cpus);
				pm_config->pm_enable = true;
				spin_lock(&delay_lock);
				list_add(&drvdata->link, &cpu_pm_list);
				spin_unlock(&delay_lock);
			}
		}
	}

	return 0;
}

static int tmc_probe(struct amba_device *adev, const struct amba_id *id)
{
	struct device *dev = &adev->dev;
	struct generic_pm_domain *pd;
	struct delay_probe_arg *init_arg;
	struct tmc_drvdata *drvdata;
	int cpu, ret;
	struct cpumask *cpumask;
	struct pm_config *pm_config;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	dev_set_drvdata(dev, drvdata);
	pm_config = &drvdata->pm_config;

	if (dev->pm_domain) {
		pd = pd_to_genpd(dev->pm_domain);
		cpumask = pd->cpus;

		if (cpumask_empty(cpumask))
			return tmc_add_coresight_dev(adev, id);

		cpus_read_lock();
		for_each_online_cpu(cpu) {
			if (cpumask_test_cpu(cpu, cpumask)) {
				ret = tmc_add_coresight_dev(adev, id);
				if (ret)
					dev_dbg(dev, "add coresight_dev fail:%d\n", ret);
				else {
					pm_config->pd_cpumask = cpumask;
					cpumask_and(&pm_config->powered_cpus,
							cpumask, cpu_online_mask);
					cpumask_copy(&pm_config->online_cpus,
							&pm_config->powered_cpus);
					pm_config->pm_enable = true;
					spin_lock(&delay_lock);
					list_add(&drvdata->link, &cpu_pm_list);
					spin_unlock(&delay_lock);
				}
				cpus_read_unlock();
				return ret;
			}
		}

		init_arg = devm_kzalloc(dev, sizeof(*init_arg), GFP_KERNEL);
		if (!init_arg) {
			cpus_read_unlock();
			return -ENOMEM;
		}
		spin_lock(&delay_lock);
		init_arg->adev = adev;
		init_arg->cpumask = pd->cpus;
		init_arg->id = id;
		list_add(&init_arg->link, &delay_probe_list);
		drvdata->delayed = init_arg;
		spin_unlock(&delay_lock);
		cpus_read_unlock();
		pm_runtime_put_sync(&adev->dev);
		return 0;
	}

	return tmc_add_coresight_dev(adev, id);
}

static void tmc_shutdown(struct amba_device *adev)
{
	unsigned long flags;
	struct tmc_drvdata *drvdata = amba_get_drvdata(adev);

	spin_lock_irqsave(&drvdata->spinlock, flags);

	if (drvdata->mode == CS_MODE_DISABLED)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR &&
		(drvdata->out_mode == TMC_ETR_OUT_MODE_MEM ||
		 (drvdata->out_mode == TMC_ETR_OUT_MODE_USB &&
		  drvdata->usb_data->usb_mode == TMC_ETR_USB_SW)))
		tmc_etr_disable_hw(drvdata);

	/*
	 * We do not care about coresight unregister here unlike remove
	 * callback which is required for making coresight modular since
	 * the system is going down after this.
	 */
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
}

static void tmc_remove(struct amba_device *adev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(&adev->dev);

	spin_lock(&delay_lock);
	if (drvdata->delayed) {
		list_del(&drvdata->delayed->link);
		spin_unlock(&delay_lock);
		return;
	}
	if (drvdata->pm_config.pm_enable)
		list_del(&drvdata->link);
	spin_unlock(&delay_lock);

	if (!drvdata->csdev)
		return;
	/*
	 * Since misc_open() holds a refcount on the f_ops, which is
	 * etb fops in this case, device is there until last file
	 * handler to this device is closed.
	 */

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR
			&& drvdata->byte_cntr)
		byte_cntr_remove(drvdata->byte_cntr);

	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
}

static int __init tmc_pm_setup(void)
{
	int ret;

	ret = cpu_pm_register_notifier(&tmc_cpu_pm_nb);
	if (ret)
		return ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
				"arm/coresight-tmc:online",
				tmc_online_cpu, tmc_offline_cpu);

	if (ret > 0) {
		hp_online = ret;
		return 0;
	}

	cpu_pm_unregister_notifier(&tmc_cpu_pm_nb);
	return ret;
}

static void tmc_pm_clear(void)
{
	cpu_pm_unregister_notifier(&tmc_cpu_pm_nb);
	if (hp_online) {
		cpuhp_remove_state_nocalls(hp_online);
		hp_online = 0;
	}
}


static const struct amba_id tmc_ids[] = {
	CS_AMBA_ID(0x000bb961),
	/* Coresight SoC 600 TMC-ETR/ETS */
	CS_AMBA_ID_DATA(0x000bb9e8, (unsigned long)CORESIGHT_SOC_600_ETR_CAPS),
	/* Coresight SoC 600 TMC-ETB */
	CS_AMBA_ID(0x000bb9e9),
	/* Coresight SoC 600 TMC-ETF */
	CS_AMBA_ID(0x000bb9ea),
	{ 0, 0},
};

MODULE_DEVICE_TABLE(amba, tmc_ids);

static struct amba_driver tmc_driver = {
	.drv = {
		.name   = "coresight-tmc",
		.owner  = THIS_MODULE,
		.suppress_bind_attrs = true,
		.pm = &tmc_dev_pm_ops,
	},
	.probe		= tmc_probe,
	.shutdown	= tmc_shutdown,
	.remove		= tmc_remove,
	.id_table	= tmc_ids,
};

static int __init tmc_init(void)
{
	int ret;

	ret = tmc_pm_setup();

	if (ret)
		return ret;

	ret = amba_driver_register(&tmc_driver);
	if (ret) {
		pr_err("Error registering tmc AMBA driver\n");
		tmc_pm_clear();
		return ret;
	}

	return ret;
}

static void __exit tmc_exit(void)
{
	amba_driver_unregister(&tmc_driver);
	tmc_pm_clear();
}

module_init(tmc_init);
module_exit(tmc_exit);

MODULE_AUTHOR("Pratik Patel <pratikp@codeaurora.org>");
MODULE_DESCRIPTION("Arm CoreSight Trace Memory Controller driver");
MODULE_LICENSE("GPL v2");
