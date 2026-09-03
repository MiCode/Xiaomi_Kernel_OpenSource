// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
//#include <linux/coresight.h>
#include <linux/amba/bus.h>

#include "sprd_coresight-priv.h"
#include "sprd_coresight-tmc.h"

DEFINE_CORESIGHT_DEVLIST(etb_devs, "tmc_etb");
DEFINE_CORESIGHT_DEVLIST(etf_devs, "tmc_etf");
//DEFINE_CORESIGHT_DEVLIST(etr_devs, "tmc_etr");

void sprd_tmc_wait_for_tmcready(struct tmc_drvdata *drvdata)
{
//	struct coresight_device *csdev = drvdata->csdev;
//	struct csdev_access *csa = &csdev->access;

	/* Ensure formatter, unformatter and hardware fifo are empty */
	if (sprd_coresight_timeout(drvdata->base/*csa*/, TMC_STS, TMC_STS_TMCREADY_BIT, 1)) {
//		dev_err(&csdev->dev,
//			"timeout while waiting for TMC to be Ready\n");
		pr_err("timeout while waiting for TMC to be Ready\n");
	}
}

void sprd_tmc_flush_and_stop(struct tmc_drvdata *drvdata)
{
//	struct coresight_device *csdev = drvdata->csdev;
//	struct csdev_access *csa = &csdev->access;
	u32 ffcr;

	ffcr = readl_relaxed(drvdata->base + TMC_FFCR);
	ffcr |= TMC_FFCR_STOP_ON_FLUSH;
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);
	ffcr |= BIT(TMC_FFCR_FLUSHMAN_BIT);
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);

//	pr_info("sprd_tmc_flush_and_stop 0x%08x\n", drvdata->base);
//	pr_info("sprd_tmc_flush_and_stop csa=0x%08x\n", csa);
//	pr_info("sprd_tmc_flush_and_stop 0x%08x\n", csa->base);

	/* Ensure flush completes */
	if (sprd_coresight_timeout(drvdata->base/*csa*/, TMC_FFCR, TMC_FFCR_FLUSHMAN_BIT, 0)) {
//		dev_err(&csdev->dev,
//		"timeout while waiting for completion of Manual Flush\n");
		pr_err("timeout while waiting for completion of Manual Flush\n");
	}

	sprd_tmc_wait_for_tmcready(drvdata);
}

void sprd_tmc_enable_hw(struct tmc_drvdata *drvdata)
{
	writel_relaxed(TMC_CTL_CAPT_EN, drvdata->base + TMC_CTL);
}

void sprd_tmc_disable_hw(struct tmc_drvdata *drvdata)
{
	writel_relaxed(0x0, drvdata->base + TMC_CTL);
}

u32 sprd_tmc_get_memwidth_mask(struct tmc_drvdata *drvdata)
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
		ret = sprd_tmc_read_prepare_etb(drvdata);
		break;
#if 0
	case TMC_CONFIG_TYPE_ETR:
		ret = sprd_tmc_read_prepare_etr(drvdata);
		break;
#endif
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
		ret = sprd_tmc_read_unprepare_etb(drvdata);
		break;
#if 0
	case TMC_CONFIG_TYPE_ETR:
		ret = tmc_read_unprepare_etr(drvdata);
		break;
#endif
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
		return sprd_tmc_etb_get_sysfs_trace(drvdata, pos, len, bufpp);

#if 0
	case TMC_CONFIG_TYPE_ETR:
		return sprd_tmc_etr_get_sysfs_trace(drvdata, pos, len, bufpp);
#endif

	default:
		break;

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
	actual = tmc_get_sysfs_trace(drvdata, *ppos, len, &bufp);
	if (actual <= 0)
		return 0;

	if (copy_to_user(data, bufp, actual)) {
		dev_dbg(&drvdata->csdev->dev,
			"%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	*ppos += actual;
	dev_dbg(&drvdata->csdev->dev, "%zu bytes copied\n", actual);

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

#define coresight_tmc_reg(name, offset)			\
	coresight_simple_reg32(struct tmc_drvdata, name, offset)
#define coresight_tmc_reg64(name, lo_off, hi_off)	\
	coresight_simple_reg64(struct tmc_drvdata, name, lo_off, hi_off)

coresight_tmc_reg(rsz, TMC_RSZ);
coresight_tmc_reg(sts, TMC_STS);
coresight_tmc_reg(trg, TMC_TRG);
coresight_tmc_reg(ctl, TMC_CTL);
coresight_tmc_reg(ffsr, TMC_FFSR);
coresight_tmc_reg(ffcr, TMC_FFCR);
coresight_tmc_reg(mode, TMC_MODE);
coresight_tmc_reg(pscr, TMC_PSCR);
coresight_tmc_reg(axictl, TMC_AXICTL);
coresight_tmc_reg(authstatus, TMC_AUTHSTATUS);
coresight_tmc_reg(devid, CORESIGHT_DEVID);
coresight_tmc_reg64(rrp, TMC_RRP, TMC_RRPHI);
coresight_tmc_reg64(rwp, TMC_RWP, TMC_RWPHI);
coresight_tmc_reg64(dba, TMC_DBALO, TMC_DBAHI);

static struct attribute *coresight_tmc_mgmt_attrs[] = {
	&dev_attr_rsz.attr,
	&dev_attr_sts.attr,
	&dev_attr_rrp.attr,
	&dev_attr_rwp.attr,
	&dev_attr_trg.attr,
	&dev_attr_ctl.attr,
	&dev_attr_ffsr.attr,
	&dev_attr_ffcr.attr,
	&dev_attr_mode.attr,
	&dev_attr_pscr.attr,
	&dev_attr_devid.attr,
	&dev_attr_dba.attr,
	&dev_attr_axictl.attr,
	&dev_attr_authstatus.attr,
	NULL,
};

static ssize_t trigger_cntr_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->trigger_cntr;

	return sprintf(buf, "%#lx\n", val);
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

	return sprintf(buf, "%#x\n", drvdata->size);
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

static struct attribute *coresight_tmc_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	&dev_attr_buffer_size.attr,
	NULL,
};

static const struct attribute_group coresight_tmc_group = {
	.attrs = coresight_tmc_attrs,
};

static const struct attribute_group coresight_tmc_mgmt_group = {
	.attrs = coresight_tmc_mgmt_attrs,
	.name = "mgmt",
};

static const struct attribute_group *coresight_tmc_groups[] = {
	&coresight_tmc_group,
	&coresight_tmc_mgmt_group,
	NULL,
};
#if 0
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
#endif

int sprd_tmc_enable_sink_show(struct device *dev)
{
	int val;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev);

	pr_info("%s entry\n", __func__);

	if (!drvdata)
		return 0;

	val = sprd_coresight_enable_sink_show_export(drvdata->csdev);

	return val;
}
EXPORT_SYMBOL_GPL(sprd_tmc_enable_sink_show);

int sprd_tmc_enable_sink_store(struct device *dev, int val)
{
	int ret;
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev);

	pr_info("%s entry\n", __func__);

	if (!drvdata)
		return -1;

	if (val)
		ret = sprd_coresight_enable_sink_store_export(drvdata->csdev, true);
	else
		ret = sprd_coresight_enable_sink_store_export(drvdata->csdev, false);

	return ret;
}
EXPORT_SYMBOL_GPL(sprd_tmc_enable_sink_store);

static int sprd_tmc_probe(struct amba_device *adev, const struct amba_id *id)
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
	const char *tmc_name;

	/* Use device name as sysfs handle */
	tmc_name = dev_name(dev);

	dev_info(dev, "%s: enter\n", __func__);

	ret = -ENOMEM;
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		goto out;

	dev_set_drvdata(dev, drvdata);

	/* Validity for the resource is already checked by the AMBA core */
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		ret = PTR_ERR(base);
		goto out;
	}

	drvdata->base = base;
	desc.access = CSDEV_ACCESS_IOMEM(base);

	spin_lock_init(&drvdata->spinlock);

	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	drvdata->config_type = BMVAL(devid, 6, 7);
	drvdata->memwidth = tmc_get_memwidth(devid);
	/* This device is not associated with a session */
	drvdata->pid = -1;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
#if 0
		drvdata->size = tmc_etr_get_default_buffer_size(dev);
		drvdata->max_burst_size = tmc_etr_get_max_burst_size(dev);
#endif
		ret = PTR_ERR(base);
		goto out;
	} else {
		drvdata->size = readl_relaxed(drvdata->base + TMC_RSZ) * 4;
	}

	desc.dev = dev;
	desc.groups = coresight_tmc_groups;

	switch (drvdata->config_type) {
	case TMC_CONFIG_TYPE_ETB:
		desc.type = CORESIGHT_DEV_TYPE_SINK;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
		desc.ops = &tmc_etb_cs_ops;
		dev_list = &etb_devs;
		break;
#if 0
	case TMC_CONFIG_TYPE_ETR:
		desc.type = CORESIGHT_DEV_TYPE_SINK;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_SYSMEM;
		desc.ops = &tmc_etr_cs_ops;
		ret = tmc_etr_setup_caps(dev, devid,
					 coresight_get_uci_data(id));
		if (ret)
			goto out;
		idr_init(&drvdata->idr);
		mutex_init(&drvdata->idr_mutex);
		dev_list = &etr_devs;
		break;
#endif
	case TMC_CONFIG_TYPE_ETF:
		desc.type = CORESIGHT_DEV_TYPE_LINKSINK;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
		desc.subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_FIFO;
		desc.ops = &tmc_etf_cs_ops;
		dev_list = &etf_devs;
		break;
	default:
		pr_err("%s: Unsupported TMC config\n", desc.name);
		ret = -EINVAL;
		goto out;
	}

	desc.name = sprd_coresight_alloc_device_name(dev_list, dev);
	if (!desc.name) {
		ret = -ENOMEM;
		goto out;
	}

	pdata = sprd_coresight_get_platform_data(dev);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto out;
	}
	adev->dev.platform_data = pdata;
	desc.pdata = pdata;

	drvdata->csdev = sprd_coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto out;
	}

#ifndef CONFIG_UNISOC_CORESIGHT_TMC_GROUP_CTRL
	/* fix etb dev name as "/dev/tmc_etb" for modem */
	if (strnstr(tmc_name, "etb", strlen(tmc_name))) {
		dev_info(dev,"set etb_dev_name as tmc_etb\n");
		drvdata->miscdev.name = "tmc_etb";
	} else
#endif
	 {
		snprintf(drvdata->etf_name, sizeof(drvdata->etf_name), "etf-%8lx",
			(unsigned long)res->start);
		drvdata->miscdev.name = drvdata->etf_name;
	}
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &tmc_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		sprd_coresight_unregister(drvdata->csdev);
	else
		pm_runtime_put(&adev->dev);
out:
	return ret;
}

static void sprd_tmc_shutdown(struct amba_device *adev)
{
	unsigned long flags;
	struct tmc_drvdata *drvdata = amba_get_drvdata(adev);

	spin_lock_irqsave(&drvdata->spinlock, flags);

	if (drvdata->mode == CS_MODE_DISABLED)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR)
		//sprd_tmc_etr_disable_hw(drvdata);
		goto out;

	/*
	 * We do not care about coresight unregister here unlike remove
	 * callback which is required for making coresight modular since
	 * the system is going down after this.
	 */
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
}

static void sprd_tmc_remove(struct amba_device *adev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(&adev->dev);

	/*
	 * Since misc_open() holds a refcount on the f_ops, which is
	 * etb fops in this case, device is there until last file
	 * handler to this device is closed.
	 */
	misc_deregister(&drvdata->miscdev);
	sprd_coresight_unregister(drvdata->csdev);
}

static const struct amba_id sprd_tmc_ids[] = {
	CS_AMBA_ID(0x000bb961),
	/* Coresight SoC 600 TMC-ETR/ETS */
	CS_AMBA_ID_DATA(0x000bb9e8, (unsigned long)CORESIGHT_SOC_600_ETR_CAPS),
	/* Coresight SoC 600 TMC-ETB */
	CS_AMBA_ID(0x000bb9e9),
	/* Coresight SoC 600 TMC-ETF */
	CS_AMBA_ID(0x000bb9ea),
	{ 0, 0},
};

MODULE_DEVICE_TABLE(amba, sprd_tmc_ids);

static struct amba_driver sprd_tmc_driver = {
	.drv = {
		.name   = "coresight-tmc",
		.owner  = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe		= sprd_tmc_probe,
	.shutdown	= sprd_tmc_shutdown,
	.remove		= sprd_tmc_remove,
	.id_table	= sprd_tmc_ids,
};

module_amba_driver(sprd_tmc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum SoC CoreSight Trace Memory Controller driver");

