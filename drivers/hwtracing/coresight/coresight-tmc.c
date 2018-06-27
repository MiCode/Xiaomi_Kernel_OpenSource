/* Copyright (c) 2012, 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight Trace Memory Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/amba/bus.h>
#include <soc/qcom/memory_dump.h>

#include "coresight-priv.h"
#include "coresight-tmc.h"

#define TMC_REG_DUMP_MAGIC 0x42445953

void tmc_wait_for_tmcready(struct tmc_drvdata *drvdata)
{
	/* Ensure formatter, unformatter and hardware fifo are empty */
	if (coresight_timeout(drvdata->base,
			      TMC_STS, TMC_STS_TMCREADY_BIT, 1)) {
		dev_err(drvdata->dev,
			"timeout while waiting for TMC to be Ready\n");
	}
}

void tmc_flush_and_stop(struct tmc_drvdata *drvdata)
{
	u32 ffcr;

	ffcr = readl_relaxed(drvdata->base + TMC_FFCR);
	ffcr |= TMC_FFCR_STOP_ON_FLUSH;
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);
	ffcr |= BIT(TMC_FFCR_FLUSHMAN_BIT);
	writel_relaxed(ffcr, drvdata->base + TMC_FFCR);
	/* Ensure flush completes */
	if (coresight_timeout(drvdata->base,
			      TMC_FFCR, TMC_FFCR_FLUSHMAN_BIT, 0)) {
		dev_err(drvdata->dev,
		"timeout while waiting for completion of Manual Flush\n");
	}

	tmc_wait_for_tmcready(drvdata);
}

static void __tmc_reg_dump(struct tmc_drvdata *drvdata)
{
	struct dump_vaddr_entry *dump_entry;
	struct msm_dump_data *dump_data;
	uint32_t *reg_buf;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		dump_entry = get_msm_dump_ptr(MSM_DUMP_DATA_TMC_ETR_REG);
		dev_dbg(drvdata->dev, "%s: TMC ETR dump entry ptr is %pK\n",
			__func__, dump_entry);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETB ||
			drvdata->config_type == TMC_CONFIG_TYPE_ETF) {
		dump_entry = get_msm_dump_ptr(MSM_DUMP_DATA_TMC_ETF_REG);
		dev_dbg(drvdata->dev, "%s: TMC ETF dump entry ptr is %pK\n",
			__func__, dump_entry);
	} else
		return;

	if (dump_entry == NULL)
		return;

	reg_buf = (uint32_t *)(dump_entry->dump_vaddr);
	dump_data = dump_entry->dump_data_vaddr;

	if (reg_buf == NULL || dump_data == NULL)
		return;

	dev_dbg(drvdata->dev, "%s: TMC dump reg ptr is %pK, dump_data is %pK\n",
		__func__, reg_buf, dump_data);

	reg_buf[1] = readl_relaxed(drvdata->base + TMC_RSZ);
	reg_buf[3] = readl_relaxed(drvdata->base + TMC_STS);
	reg_buf[5] = readl_relaxed(drvdata->base + TMC_RRP);
	reg_buf[6] = readl_relaxed(drvdata->base + TMC_RWP);
	reg_buf[7] = readl_relaxed(drvdata->base + TMC_TRG);
	reg_buf[8] = readl_relaxed(drvdata->base + TMC_CTL);
	reg_buf[10] = readl_relaxed(drvdata->base + TMC_MODE);
	reg_buf[11] = readl_relaxed(drvdata->base + TMC_LBUFLEVEL);
	reg_buf[12] = readl_relaxed(drvdata->base + TMC_CBUFLEVEL);
	reg_buf[13] = readl_relaxed(drvdata->base + TMC_BUFWM);
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		reg_buf[14] = readl_relaxed(drvdata->base + TMC_RRPHI);
		reg_buf[15] = readl_relaxed(drvdata->base + TMC_RWPHI);
		reg_buf[68] = readl_relaxed(drvdata->base + TMC_AXICTL);
		reg_buf[70] = readl_relaxed(drvdata->base + TMC_DBALO);
		reg_buf[71] = readl_relaxed(drvdata->base + TMC_DBAHI);
	}
	reg_buf[192] = readl_relaxed(drvdata->base + TMC_FFSR);
	reg_buf[193] = readl_relaxed(drvdata->base + TMC_FFCR);
	reg_buf[194] = readl_relaxed(drvdata->base + TMC_PSCR);
	reg_buf[1000] = readl_relaxed(drvdata->base + CORESIGHT_CLAIMSET);
	reg_buf[1001] = readl_relaxed(drvdata->base + CORESIGHT_CLAIMCLR);
	reg_buf[1005] = readl_relaxed(drvdata->base + CORESIGHT_LSR);
	reg_buf[1006] = readl_relaxed(drvdata->base + CORESIGHT_AUTHSTATUS);
	reg_buf[1010] = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	reg_buf[1011] = readl_relaxed(drvdata->base + CORESIGHT_DEVTYPE);
	reg_buf[1012] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR4);
	reg_buf[1013] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR5);
	reg_buf[1014] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR6);
	reg_buf[1015] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR7);
	reg_buf[1016] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR0);
	reg_buf[1017] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR1);
	reg_buf[1018] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR2);
	reg_buf[1019] = readl_relaxed(drvdata->base + CORESIGHT_PERIPHIDR3);
	reg_buf[1020] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR0);
	reg_buf[1021] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR1);
	reg_buf[1022] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR2);
	reg_buf[1023] = readl_relaxed(drvdata->base + CORESIGHT_COMPIDR3);

	dump_data->magic = TMC_REG_DUMP_MAGIC;
}

void tmc_enable_hw(struct tmc_drvdata *drvdata)
{
	drvdata->enable = true;
	writel_relaxed(TMC_CTL_CAPT_EN, drvdata->base + TMC_CTL);
	if (drvdata->force_reg_dump)
		__tmc_reg_dump(drvdata);
}

void tmc_disable_hw(struct tmc_drvdata *drvdata)
{
	drvdata->enable = false;
	writel_relaxed(0x0, drvdata->base + TMC_CTL);
}

static int tmc_read_prepare(struct tmc_drvdata *drvdata)
{
	int ret = 0;

	if (!drvdata->enable)
		return -EPERM;

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
		dev_info(drvdata->dev, "TMC read start\n");

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
		dev_info(drvdata->dev, "TMC read end\n");

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

	dev_dbg(drvdata->dev, "%s: successfully opened\n", __func__);
	return 0;
}

static ssize_t tmc_read(struct file *file, char __user *data, size_t len,
			loff_t *ppos)
{
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);
	char *bufp;

	mutex_lock(&drvdata->mem_lock);

	bufp = drvdata->buf + *ppos;

	if (*ppos + len > drvdata->len)
		len = drvdata->len - *ppos;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG) {
			if (bufp == (char *)(drvdata->vaddr + drvdata->size))
				bufp = drvdata->vaddr;
			else if (bufp > (char *)(drvdata->vaddr +
				 drvdata->size))
				bufp -= drvdata->size;
			if ((bufp + len) > (char *)(drvdata->vaddr +
			     drvdata->size))
				len = (char *)(drvdata->vaddr + drvdata->size) -
				      bufp;
		} else {
			tmc_etr_sg_compute_read(drvdata, ppos, &bufp, &len);
		}
	}

	if (copy_to_user(data, bufp, len)) {
		dev_dbg(drvdata->dev, "%s: copy_to_user failed\n", __func__);
		mutex_unlock(&drvdata->mem_lock);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(drvdata->dev, "%s: %zu bytes copied, %d bytes left\n",
		__func__, len, (int)(drvdata->len - *ppos));

	mutex_unlock(&drvdata->mem_lock);
	return len;
}

static int tmc_release(struct inode *inode, struct file *file)
{
	int ret;
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);

	ret = tmc_read_unprepare(drvdata);
	if (ret)
		return ret;

	dev_dbg(drvdata->dev, "%s: released\n", __func__);
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

#define coresight_tmc_simple_func(name, offset)			\
	coresight_simple_func(struct tmc_drvdata, NULL, name, offset)

coresight_tmc_simple_func(rsz, TMC_RSZ);
coresight_tmc_simple_func(sts, TMC_STS);
coresight_tmc_simple_func(rrp, TMC_RRP);
coresight_tmc_simple_func(rwp, TMC_RWP);
coresight_tmc_simple_func(trg, TMC_TRG);
coresight_tmc_simple_func(ctl, TMC_CTL);
coresight_tmc_simple_func(ffsr, TMC_FFSR);
coresight_tmc_simple_func(ffcr, TMC_FFCR);
coresight_tmc_simple_func(mode, TMC_MODE);
coresight_tmc_simple_func(pscr, TMC_PSCR);
coresight_tmc_simple_func(devid, CORESIGHT_DEVID);

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

static ssize_t mem_size_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->mem_size;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t mem_size_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	mutex_lock(&drvdata->mem_lock);
	if (kstrtoul(buf, 16, &val)) {
		mutex_unlock(&drvdata->mem_lock);
		return -EINVAL;
	}

	if (drvdata->enable) {
		mutex_unlock(&drvdata->mem_lock);
		pr_err("ETR is in use, disable it to change the mem_size\n");
		return -EINVAL;
	}

	drvdata->mem_size = val;
	mutex_unlock(&drvdata->mem_lock);
	return size;
}
static DEVICE_ATTR_RW(mem_size);

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
	unsigned long flags;
	int ret;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%10s", str) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->mem_lock);
	if (!strcmp(str, str_tmc_etr_out_mode[TMC_ETR_OUT_MODE_MEM])) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			goto out;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		if (!drvdata->enable) {
			drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
			goto out;
		}
		__tmc_etr_disable_to_bam(drvdata);
		tmc_etr_enable_hw(drvdata);
		drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		coresight_cti_map_trigout(drvdata->cti_flush, 3, 0);
		coresight_cti_map_trigin(drvdata->cti_reset, 2, 0);

		tmc_etr_bam_disable(drvdata);
		usb_qdss_close(drvdata->usbch);
	} else if (!strcmp(str, str_tmc_etr_out_mode[TMC_ETR_OUT_MODE_USB])) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			goto out;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		if (!drvdata->enable) {
			drvdata->out_mode = TMC_ETR_OUT_MODE_USB;
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
			goto out;
		}
		if (drvdata->reading) {
			ret = -EBUSY;
			goto err1;
		}
		tmc_etr_disable_hw(drvdata);
		drvdata->out_mode = TMC_ETR_OUT_MODE_USB;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		coresight_cti_unmap_trigout(drvdata->cti_flush, 3, 0);
		coresight_cti_unmap_trigin(drvdata->cti_reset, 2, 0);

		drvdata->usbch = usb_qdss_open("qdss", drvdata,
					       usb_notifier);
		if (IS_ERR(drvdata->usbch)) {
			dev_err(drvdata->dev, "usb_qdss_open failed\n");
			ret = PTR_ERR(drvdata->usbch);
			goto err0;
		}
	}
out:
	mutex_unlock(&drvdata->mem_lock);
	return size;
err1:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
err0:
	mutex_unlock(&drvdata->mem_lock);
	return ret;
}
static DEVICE_ATTR_RW(out_mode);

static ssize_t available_out_modes_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	ssize_t len = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(str_tmc_etr_out_mode); i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s ",
				str_tmc_etr_out_mode[i]);

	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}
static DEVICE_ATTR_RO(available_out_modes);

static ssize_t mem_type_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			str_tmc_etr_mem_type[drvdata->mem_type]);
}

static ssize_t mem_type_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%10s", str) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->mem_lock);
	if (drvdata->enable) {
		mutex_unlock(&drvdata->mem_lock);
		pr_err("ETR is in use, disable it to switch the mode\n");
		return -EINVAL;
	}
	if (!strcmp(str, str_tmc_etr_mem_type[TMC_ETR_MEM_TYPE_CONTIG]))
		drvdata->mem_type = TMC_ETR_MEM_TYPE_CONTIG;
	else if (!strcmp(str, str_tmc_etr_mem_type[TMC_ETR_MEM_TYPE_SG]))
		drvdata->mem_type = TMC_ETR_MEM_TYPE_SG;
	else
		size = -EINVAL;

	mutex_unlock(&drvdata->mem_lock);

	return size;
}
static DEVICE_ATTR_RW(mem_type);

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

	if (val && val < 16) {
		pr_err("Assign minimum block size of 16 bytes\n");
		return -EINVAL;
	}

	mutex_lock(&drvdata->byte_cntr->byte_cntr_lock);
	drvdata->byte_cntr->block_size = val;
	mutex_unlock(&drvdata->byte_cntr->byte_cntr_lock);

	return size;
}
static DEVICE_ATTR_RW(block_size);

static struct attribute *coresight_tmc_etf_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	NULL,
};

static struct attribute *coresight_tmc_etr_attrs[] = {
	&dev_attr_mem_size.attr,
	&dev_attr_mem_type.attr,
	&dev_attr_trigger_cntr.attr,
	&dev_attr_out_mode.attr,
	&dev_attr_available_out_modes.attr,
	&dev_attr_block_size.attr,
	NULL,
};

static const struct attribute_group coresight_tmc_etf_group = {
	.attrs = coresight_tmc_etf_attrs,
};

static const struct attribute_group coresight_tmc_etr_group = {
	.attrs = coresight_tmc_etr_attrs,
};

static const struct attribute_group coresight_tmc_mgmt_group = {
	.attrs = coresight_tmc_mgmt_attrs,
	.name = "mgmt",
};

const struct attribute_group *coresight_tmc_etf_groups[] = {
	&coresight_tmc_etf_group,
	&coresight_tmc_mgmt_group,
	NULL,
};

const struct attribute_group *coresight_tmc_etr_groups[] = {
	&coresight_tmc_etr_group,
	&coresight_tmc_mgmt_group,
	NULL,
};

static int tmc_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret = 0;
	u32 devid;
	void __iomem *base;
	struct device *dev = &adev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct tmc_drvdata *drvdata;
	struct resource *res = &adev->res;
	struct coresight_desc desc = { 0 };
	struct device_node *np = adev->dev.of_node;
	struct coresight_cti_data *ctidata;

	pdata = of_get_coresight_platform_data(dev, np);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto out;
	}
	adev->dev.platform_data = pdata;

	ret = -ENOMEM;
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		goto out;

	drvdata->dev = &adev->dev;
	dev_set_drvdata(dev, drvdata);

	/* Validity for the resource is already checked by the AMBA core */
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		ret = PTR_ERR(base);
		goto out;
	}

	drvdata->base = base;

	spin_lock_init(&drvdata->spinlock);
	mutex_init(&drvdata->mem_lock);

	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	drvdata->config_type = BMVAL(devid, 6, 7);
	drvdata->memwidth = tmc_get_memwidth(devid);

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		ret = of_property_read_u32(np, "arm,buffer-size",
					   &drvdata->size);
		if (ret)
			drvdata->size = SZ_1M;

		if (of_property_read_bool(np, "arm,sg-enable"))
			drvdata->memtype  = TMC_ETR_MEM_TYPE_SG;
		else
			drvdata->memtype  = TMC_ETR_MEM_TYPE_CONTIG;
		drvdata->mem_size = drvdata->size;
		drvdata->mem_type = drvdata->memtype;
		drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
	} else {
		drvdata->size = readl_relaxed(drvdata->base + TMC_RSZ) * 4;
	}

	ctidata = of_get_coresight_cti_data(dev, adev->dev.of_node);
	if (IS_ERR(ctidata)) {
		dev_err(dev, "invalid cti data\n");
	} else if (ctidata && ctidata->nr_ctis == 2) {
		drvdata->cti_flush = coresight_cti_get(ctidata->names[0]);
		if (IS_ERR(drvdata->cti_flush))
			dev_err(dev, "failed to get flush cti\n");

		drvdata->cti_reset = coresight_cti_get(ctidata->names[1]);
		if (IS_ERR(drvdata->cti_reset))
			dev_err(dev, "failed to get reset cti\n");
	}

	ret = of_get_coresight_csr_name(adev->dev.of_node, &drvdata->csr_name);
	if (ret) {
		dev_err(dev, "No csr data\n");
	} else{
		drvdata->csr = coresight_csr_get(drvdata->csr_name);
		if (IS_ERR(drvdata->csr)) {
			dev_err(dev, "failed to get csr, defer probe\n");
			return -EPROBE_DEFER;
		}
	}
	if (of_property_read_bool(drvdata->dev->of_node, "qcom,force-reg-dump"))
		drvdata->force_reg_dump = true;

	desc.pdata = pdata;
	desc.dev = dev;
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		desc.type = CORESIGHT_DEV_TYPE_SINK;
		desc.ops = &tmc_etb_cs_ops;
		desc.groups = coresight_tmc_etf_groups;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		desc.type = CORESIGHT_DEV_TYPE_SINK;
		desc.ops = &tmc_etr_cs_ops;
		desc.groups = coresight_tmc_etr_groups;
		desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;

		drvdata->byte_cntr = byte_cntr_init(adev, drvdata);

		ret = tmc_etr_bam_init(adev, drvdata);
		if (ret)
			goto out;
		/*
		 * ETR configuration uses a 40-bit AXI master in place of
		 * the embedded SRAM of ETB/ETF.
		 */
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(40));
		if (ret)
			goto out;
	} else {
		desc.type = CORESIGHT_DEV_TYPE_LINKSINK;
		desc.ops = &tmc_etf_cs_ops;
		desc.groups = coresight_tmc_etf_groups;
		desc.subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_FIFO;
	}

	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto out;
	}

	drvdata->miscdev.name = pdata->name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &tmc_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		coresight_unregister(drvdata->csdev);

	if (!ret)
		pm_runtime_put(&adev->dev);

out:
	return ret;
}

static struct amba_id tmc_ids[] = {
	{
		.id     = 0x0003b961,
		.mask   = 0x0003ffff,
	},
	{ 0, 0},
};

static struct amba_driver tmc_driver = {
	.drv = {
		.name   = "coresight-tmc",
		.owner  = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe		= tmc_probe,
	.id_table	= tmc_ids,
};
builtin_amba_driver(tmc_driver);
