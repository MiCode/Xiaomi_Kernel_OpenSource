/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

//#include <linux/coresight.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include "sprd_coresight-priv.h"
#include "sprd_coresight-tmc.h"
#include "sprd_coresight-phy.h"

static struct mutex coresight_mutex_lock;

static char cs_buf[CS_BUF_SZ];

extern void sprd_enable_coresight_atb_clk(struct device *dev);

static bool is_power_on(struct cs_context_t *cs_context)
{
	u32 regs;

	if (!cs_context->config.is_subsys)
		return true;

	regs = readl_relaxed(cs_context->config.vir_pmu_check);

	if (regs & (cs_context->config.pmu_mask))	/* deepsleep is 0x0 */
		return true;
	return false;
}

static int store_cs_regs(char *buf, void __iomem *vaddr, u32 num)
{
	u32 regs;
	void __iomem *base;
	int i;

	base = vaddr;
	CS_UNLOCK(vaddr);
	for (i = 0; i < num; i++) {
		if (i == 4) {
			/* 0x10: TMC RRD */
			memset(buf, 0x0, 4);
		} else {
			regs = readl_relaxed(base);
			memcpy(buf, &regs, 4);
		}
		base += 4;
		buf += 4;
	}
	CS_LOCK(vaddr);

	return 0;
}

static int tmc_read_prepare(struct tmc_drvdata_group *drvdata)
{
	int ret = 0, err;
	int i;
	int reg_num;
	char *buf, *buf0;

	buf0 = cs_buf;

	drvdata->len = drvdata->size; /* len is important */
	drvdata->buf = buf0;
	drvdata->mode = CS_MODE_SYSFS;

	/* prepare header */
	buf = buf0;
	strcpy(buf, "ETB_GROUP_V1.0");

	/* prepare cs context */
	buf = buf0 + 0x10;
	for (i = 0; i < drvdata->cs_element_num; i++) {

		reg_num = (drvdata->cs_context[i].config.type == CS_FUNNEL) ? CS_FUNNEL_REGS_NUM : CS_TMC_REGS_NUM;
		if (is_power_on(&(drvdata->cs_context[i]))) {
			store_cs_regs(buf, drvdata->cs_context[i].config.vir_start, reg_num);
		} else {
			memset(buf, 0xAA, reg_num * sizeof(int));
		}
		buf += reg_num * sizeof(int);
	}

	/* prepare tmc */
	buf = buf0 + 0x200;

	for (i = 0; i < drvdata->cs_element_num; i++) {

		if (drvdata->cs_context[i].config.type != CS_TMC)
			continue;

		if (is_power_on(&(drvdata->cs_context[i]))) {
			drvdata->cs_context[i].context.mode = CS_MODE_SYSFS;
			drvdata->cs_context[i].context.buf = kzalloc(drvdata->cs_context[i].context.size, GFP_KERNEL);
			if (!drvdata->cs_context[i].context.buf) {
				dev_err(drvdata->dev, "kzalloc fail\n");
				return -ENOMEM;
			}
			err = sprd_tmc_read_prepare_etb(&(drvdata->cs_context[i].context));

			if (err) {
				dev_err(drvdata->dev, "call sprd_tmc_read_prepare_etb() fail\n");
				memset(buf, 0xCC, drvdata->cs_context[i].context.size);
			} else
				memcpy(buf, drvdata->cs_context[i].context.buf, drvdata->cs_context[i].context.size);
		} else {
			memset(buf, 0xBB, drvdata->cs_context[i].context.size);
		}

		buf += drvdata->cs_context[i].config.size;
	}

	if (!ret)
		dev_info(drvdata->dev, "TMC read start\n");

	return ret;
}

static int tmc_read_unprepare(struct tmc_drvdata_group *drvdata)
{
	int ret = 0;
	int i;

	for (i = 0; i < drvdata->cs_element_num; i++) {
		if (is_power_on(&(drvdata->cs_context[i]))) {
			ret = sprd_tmc_read_unprepare_etb(&(drvdata->cs_context[i].context));
			if (drvdata->cs_context[i].context.mode == CS_MODE_SYSFS)
				kfree(drvdata->cs_context[i].context.buf);

			if (ret) {
				pr_err("%s: sprd_tmc_read_unprepare_etb fail!\n", __func__);
				ret = -EINVAL;
				break;
			}
		}
	}

	dev_info(drvdata->dev, "TMC read end\n");

	return ret;
}

static int tmc_open(struct inode *inode, struct file *file)
{
	int ret;
	int tmr_cnt = 10;
	struct tmc_drvdata_group *drvdata = container_of(file->private_data,
						   struct tmc_drvdata_group, miscdev);

	while (tmr_cnt--) {
		if (mutex_trylock(&coresight_mutex_lock)) {
			dev_info(drvdata->dev, "%s:get lock ok!\n", __func__);
			break;
		}
		if (tmr_cnt == 0) {
			dev_info(drvdata->dev, "%s:get lock time out!\n", __func__);
			return -EBUSY;
		}
		msleep(50);
	}

	ret = tmc_read_prepare(drvdata);
	if (ret) {
		dev_info(drvdata->dev, "%s:tmc_read_prepare fail!\n", __func__);
		mutex_unlock(&coresight_mutex_lock);
		return ret;
	}

	nonseekable_open(inode, file);

	dev_info(drvdata->dev, "%s: successfully opened\n", __func__);
	return 0;
}

static ssize_t tmc_read(struct file *file, char __user *data, size_t len,
			loff_t *ppos)
{
	struct tmc_drvdata_group *drvdata = container_of(file->private_data,
						   struct tmc_drvdata_group, miscdev);

	char *bufp = drvdata->buf + *ppos;

	if (*ppos + len > drvdata->len)
		len = drvdata->len - *ppos;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (bufp == (char *)(drvdata->vaddr + drvdata->size))
			bufp = drvdata->vaddr;
		else if (bufp > (char *)(drvdata->vaddr + drvdata->size))
			bufp -= drvdata->size;
		if ((bufp + len) > (char *)(drvdata->vaddr + drvdata->size))
			len = (char *)(drvdata->vaddr + drvdata->size) - bufp;
	}

	dev_info(drvdata->dev, "%s: %zu len, %d drvdata->len, %d ppos\n",
		__func__, len, (int)drvdata->len, (int)*ppos);

	if (copy_to_user(data, bufp, len)) {
		dev_info(drvdata->dev, "%s: copy_to_user failed\n", __func__);
		mutex_unlock(&coresight_mutex_lock);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(drvdata->dev, "%s: %zu bytes copied, %d bytes left\n",
		__func__, len, (int)(drvdata->len - *ppos));
	return len;
}

static int tmc_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct tmc_drvdata_group *drvdata = container_of(file->private_data,
						   struct tmc_drvdata_group, miscdev);

	ret = tmc_read_unprepare(drvdata);
	if (ret) {
		dev_info(drvdata->dev, "%s:tmc_read_unprepare fail!\n", __func__);
		goto out;
	}

	dev_info(drvdata->dev, "%s: released end!\n", __func__);

out:
	mutex_unlock(&coresight_mutex_lock);
	return ret;

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
/*
coresight_tmc_reg(rsz, TMC_RSZ);
coresight_tmc_reg(sts, TMC_STS);
coresight_tmc_reg(trg, TMC_TRG);
coresight_tmc_reg(ctl, TMC_CTL);
coresight_tmc_reg(ffsr, TMC_FFSR);
coresight_tmc_reg(ffcr, TMC_FFCR);
coresight_tmc_reg(mode, TMC_MODE);
coresight_tmc_reg(pscr, TMC_PSCR);
coresight_tmc_reg(axictl, TMC_AXICTL);
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
	NULL,
};
*/
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

static struct attribute *coresight_tmc_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	NULL,
};

static const struct attribute_group coresight_tmc_group = {
	.attrs = coresight_tmc_attrs,
};

/*
static const struct attribute_group coresight_tmc_mgmt_group = {
	.attrs = coresight_tmc_mgmt_attrs,
	.name = "mgmt",
};
*/

const struct attribute_group *coresight_tmc_groups_ex[] = {
	&coresight_tmc_group,
	/* &coresight_tmc_mgmt_group, */
	NULL,
};

static int get_cs_group_data(struct device *dev, struct tmc_drvdata_group *drvdata)
{
	int i;
	u32 cs_element_num;
	struct tmc_drvdata *tmc_data;
	struct cs_element_t *cs_table = cs_group_ptr();

	/* header */
	drvdata->size = 0x200;

	cs_element_num = cs_group_size();
	if (cs_element_num > CS_ELEMENT_NUM)
		return -1;

	drvdata->cs_element_num = cs_element_num;

	/* map virual address */
	for (i = 0; i < cs_element_num; i++) {
		cs_table[i].vir_start =  devm_ioremap(dev, cs_table[i].phy_start, cs_table[i].size);
		cs_table[i].vir_pmu_check =  devm_ioremap(dev, cs_table[i].pmu_check, 4);
	}

	/* copy cs_table to drvdata */
	for (i = 0; i < cs_element_num; i++) {
		memcpy(&(drvdata->cs_context[i].config), &(cs_table[i]), sizeof(struct cs_element_t));

		tmc_data = &(drvdata->cs_context[i].context);

		tmc_data->base = cs_table[i].vir_start;
		tmc_data->config_type = TMC_CONFIG_TYPE_ETB;
		tmc_data->memwidth = TMC_MEM_INTF_WIDTH_32BITS;
		tmc_data->size = drvdata->cs_context[i].config.size;
		drvdata->size += tmc_data->size;
	}

	return 0;
}

DEFINE_CORESIGHT_DEVLIST(etb_devs, "tmc_etb");

static int sprd_tmc_group_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct tmc_drvdata_group *drvdata;
	struct coresight_desc desc = { 0 };

	dev_info(dev, "sprd_tmc_group_probe entry\n");

	pdata = sprd_coresight_get_platform_data(dev);
	if (IS_ERR(pdata)) {
		dev_err(dev, "get platform data fail!\n");
		ret = PTR_ERR(pdata);
		goto out;
	}
	pdev->dev.platform_data = pdata;

	ret = -ENOMEM;
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		dev_err(dev, "alloc drvdata fail!\n");
		goto out;
	}

	drvdata->dev = &pdev->dev;
	dev_set_drvdata(dev, drvdata);

	tmc_get_memwidth(0);

	drvdata->config_type = TMC_CONFIG_TYPE_ETB;
	drvdata->memwidth = TMC_MEM_INTF_WIDTH_32BITS;

	mutex_init(&coresight_mutex_lock);

	if (-1 == get_cs_group_data(dev, drvdata)) {
		dev_err(dev, "get_cs_group_data fail!\n");
		goto out;
	}

	pm_runtime_put(&pdev->dev);

	desc.pdata = pdata;
	desc.dev = dev;
	desc.groups = coresight_tmc_groups_ex;

	desc.type = CORESIGHT_DEV_TYPE_SINK;
	desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
	desc.ops = &tmc_etb_cs_ops;

	desc.name = sprd_coresight_alloc_device_name(&etb_devs, dev);
	if (!desc.name) {
		dev_err(dev, "alloc_device_name error!\n");
		ret = -ENOMEM;
		goto out;
	}

	drvdata->csdev = sprd_coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		dev_err(dev, "drvdata->csdev error!\n");
		ret = PTR_ERR(drvdata->csdev);
		goto out;
	}

	sprd_enable_coresight_atb_clk(dev);

	/* fix etb dev name as "/dev/tmc_etb" for modem */
	drvdata->miscdev.name = "tmc_etb";

	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &tmc_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		sprd_coresight_unregister(drvdata->csdev);
out:
	dev_info(dev, "sprd_tmc_group_probe end\n");
	return ret;
}

static const struct of_device_id dt_ids[] = {
	{.compatible = "arm,sprd_coresight-tmc_group",},
	{},
};

static struct platform_driver sprd_tmc_group_driver = {
	.probe		= sprd_tmc_group_probe,
	.driver = {
		   .name = "coresight-tmc_ex",
		   .of_match_table = dt_ids,
		   },
};

module_platform_driver(sprd_tmc_group_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum SoC Coresight TMC_GROUP Driver");
