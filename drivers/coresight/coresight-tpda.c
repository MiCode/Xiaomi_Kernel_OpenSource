/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/bitmap.h>
#include <linux/of.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define tpda_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define tpda_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define TPDA_LOCK(drvdata)						\
do {									\
	mb();								\
	tpda_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define TPDA_UNLOCK(drvdata)						\
do {									\
	tpda_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define TPDA_CR			(0x000)
#define TPDA_Pn_CR(n)		(0x004 + (n * 4))
#define TPDA_FPID_CR		(0x084)
#define TPDA_FREQREQ_VAL	(0x088)
#define TPDA_SYNCR		(0x08C)
#define TPDA_FLUSH_CR		(0x090)
#define TPDA_FLUSH_SR		(0x094)
#define TPDA_FLUSH_ERR		(0x098)

#define TPDA_MAX_INPORTS	32

struct tpda_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct clk		*clk;
	struct mutex		lock;
	bool			enable;
	uint32_t		atid;
	uint32_t		bc_esize[TPDA_MAX_INPORTS];
	uint32_t		tc_esize[TPDA_MAX_INPORTS];
	uint32_t		dsb_esize[TPDA_MAX_INPORTS];
	uint32_t		cmb_esize[TPDA_MAX_INPORTS];
	bool			trig_async;
	bool			trig_flag_ts;
	bool			trig_freq;
	bool			freq_ts;
	uint32_t		freq_req_val;
	bool			freq_req;
};

static void __tpda_enable_pre_port(struct tpda_drvdata *drvdata)
{
	uint32_t val;

	val = tpda_readl(drvdata, TPDA_CR);
	/* Set the master id */
	val = val & ~(0x7F << 13);
	val = val & ~(0x7F << 6);
	val |= (drvdata->atid << 6);
	if (drvdata->trig_async)
		val = val | BIT(5);
	else
		val = val & ~BIT(5);
	if (drvdata->trig_flag_ts)
		val = val | BIT(4);
	else
		val = val & ~BIT(4);
	if (drvdata->trig_freq)
		val = val | BIT(3);
	else
		val = val & ~BIT(3);
	if (drvdata->freq_ts)
		val = val | BIT(2);
	else
		val = val & ~BIT(2);
	tpda_writel(drvdata, val, TPDA_CR);

	/*
	 * If FLRIE bit is set, set the master and channel
	 * id as zero
	 */
	if (BVAL(tpda_readl(drvdata, TPDA_CR), 4))
		tpda_writel(drvdata, 0x0, TPDA_FPID_CR);
}

static void __tpda_enable_port(struct tpda_drvdata *drvdata, int port)
{
	uint32_t val;

	val = tpda_readl(drvdata, TPDA_Pn_CR(port));
	if (drvdata->bc_esize[port] == 32)
		val = val & ~BIT(4);
	else if (drvdata->bc_esize[port] == 64)
		val = val | BIT(4);

	if (drvdata->tc_esize[port] == 32)
		val = val & ~BIT(5);
	else if (drvdata->tc_esize[port] == 64)
		val = val | BIT(5);

	if (drvdata->dsb_esize[port] == 32)
		val = val & ~BIT(8);
	else if (drvdata->dsb_esize[port] == 64)
		val = val | BIT(8);

	val = val & ~(0x3 << 6);
	if (drvdata->cmb_esize[port] == 8)
		val &= ~(0x3 << 6);
	else if (drvdata->cmb_esize[port] == 32)
		val |= (0x1 << 6);
	else if (drvdata->cmb_esize[port] == 64)
		val |= (0x2 << 6);

	/* Set the hold time */
	val = val & ~(0x7 << 1);
	val |= (0x5 << 1);
	tpda_writel(drvdata, val, TPDA_Pn_CR(port));
	/* Enable the port */
	val = val | BIT(0);
	tpda_writel(drvdata, val, TPDA_Pn_CR(port));
}

static void __tpda_enable_post_port(struct tpda_drvdata *drvdata)
{
	uint32_t val;

	val = tpda_readl(drvdata, TPDA_SYNCR);
	/* Clear the mode */
	val = val & ~BIT(12);
	/* Program the counter value */
	val = val | 0xFFF;
	tpda_writel(drvdata, val, TPDA_SYNCR);

	if (drvdata->freq_req_val)
		tpda_writel(drvdata, drvdata->freq_req_val, TPDA_FREQREQ_VAL);
	else
		tpda_writel(drvdata, 0x0, TPDA_FREQREQ_VAL);

	val = tpda_readl(drvdata, TPDA_CR);
	if (drvdata->freq_req)
		val = val | BIT(1);
	else
		val = val & ~BIT(1);
	tpda_writel(drvdata, val, TPDA_CR);
}

static void __tpda_enable(struct tpda_drvdata *drvdata, int port)
{
	TPDA_UNLOCK(drvdata);

	if (!drvdata->enable)
		__tpda_enable_pre_port(drvdata);

	__tpda_enable_port(drvdata, port);

	if (!drvdata->enable)
		__tpda_enable_post_port(drvdata);

	TPDA_LOCK(drvdata);
}

static int tpda_enable(struct coresight_device *csdev, int inport, int outport)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	mutex_lock(&drvdata->lock);
	__tpda_enable(drvdata, inport);
	drvdata->enable = true;
	mutex_unlock(&drvdata->lock);

	dev_info(drvdata->dev, "TPDA inport %d enabled\n", inport);
	return 0;
}

static void __tpda_disable(struct tpda_drvdata *drvdata, int port)
{
	uint32_t val;

	TPDA_UNLOCK(drvdata);

	val = tpda_readl(drvdata, TPDA_Pn_CR(port));
	val = val & ~BIT(0);
	tpda_writel(drvdata, val, TPDA_Pn_CR(port));

	TPDA_LOCK(drvdata);
}

static void tpda_disable(struct coresight_device *csdev, int inport,
			   int outport)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	mutex_lock(&drvdata->lock);
	__tpda_disable(drvdata, inport);
	drvdata->enable = false;
	mutex_unlock(&drvdata->lock);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "TPDA inport %d disabled\n", inport);
}

static const struct coresight_ops_link tpda_link_ops = {
	.enable		= tpda_enable,
	.disable	= tpda_disable,
};

static const struct coresight_ops tpda_cs_ops = {
	.link_ops	= &tpda_link_ops,
};

static ssize_t tpda_show_trig_async_enable(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->trig_async);
}

static ssize_t tpda_store_trig_async_enable(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->trig_async = true;
	else
		drvdata->trig_async = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(trig_async_enable, S_IRUGO | S_IWUSR,
		   tpda_show_trig_async_enable,
		   tpda_store_trig_async_enable);

static ssize_t tpda_show_trig_flag_ts_enable(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->trig_flag_ts);
}

static ssize_t tpda_store_trig_flag_ts_enable(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf,
					      size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->trig_flag_ts = true;
	else
		drvdata->trig_flag_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(trig_flag_ts_enable, S_IRUGO | S_IWUSR,
		   tpda_show_trig_flag_ts_enable,
		   tpda_store_trig_flag_ts_enable);

static ssize_t tpda_show_trig_freq_enable(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->trig_freq);
}

static ssize_t tpda_store_trig_freq_enable(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->trig_freq = true;
	else
		drvdata->trig_freq = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(trig_freq_enable, S_IRUGO | S_IWUSR,
		   tpda_show_trig_freq_enable,
		   tpda_store_trig_freq_enable);

static ssize_t tpda_show_freq_ts_enable(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned)drvdata->freq_ts);
}

static ssize_t tpda_store_freq_ts_enable(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->freq_ts = true;
	else
		drvdata->freq_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(freq_ts_enable, S_IRUGO | S_IWUSR, tpda_show_freq_ts_enable,
		   tpda_store_freq_ts_enable);

static ssize_t tpda_show_freq_req_val(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->freq_req_val;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpda_store_freq_req_val(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);
	drvdata->freq_req_val = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(freq_req_val, S_IRUGO | S_IWUSR, tpda_show_freq_req_val,
		   tpda_store_freq_req_val);

static ssize_t tpda_show_freq_req(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->freq_req);
}

static ssize_t tpda_store_freq_req(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->freq_req = true;
	else
		drvdata->freq_req = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(freq_req, S_IRUGO | S_IWUSR, tpda_show_freq_req,
		   tpda_store_freq_req);

static ssize_t tpda_show_global_flush_req(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	mutex_lock(&drvdata->lock);

	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDA_UNLOCK(drvdata);
	val = tpda_readl(drvdata, TPDA_CR);
	TPDA_LOCK(drvdata);

	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpda_store_global_flush_req(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);

	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDA_UNLOCK(drvdata);
		val = tpda_readl(drvdata, TPDA_CR);
		val = val | BIT(0);
		tpda_writel(drvdata, val, TPDA_CR);
		TPDA_LOCK(drvdata);
	}

	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(global_flush_req, S_IRUGO | S_IWUSR,
		   tpda_show_global_flush_req, tpda_store_global_flush_req);

static ssize_t tpda_show_port_flush_req(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	mutex_lock(&drvdata->lock);

	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDA_UNLOCK(drvdata);
	val = tpda_readl(drvdata, TPDA_FLUSH_CR);
	TPDA_LOCK(drvdata);

	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpda_store_port_flush_req(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpda_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->lock);

	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDA_UNLOCK(drvdata);
		tpda_writel(drvdata, val, TPDA_FLUSH_CR);
		TPDA_LOCK(drvdata);
	}

	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(port_flush_req, S_IRUGO | S_IWUSR, tpda_show_port_flush_req,
		   tpda_store_port_flush_req);

static struct attribute *tpda_attrs[] = {
	&dev_attr_trig_async_enable.attr,
	&dev_attr_trig_flag_ts_enable.attr,
	&dev_attr_trig_freq_enable.attr,
	&dev_attr_freq_ts_enable.attr,
	&dev_attr_freq_req_val.attr,
	&dev_attr_freq_req.attr,
	&dev_attr_global_flush_req.attr,
	&dev_attr_port_flush_req.attr,
	NULL,
};

static struct attribute_group tpda_attr_grp = {
	.attrs = tpda_attrs,
};

static const struct attribute_group *tpda_attr_grps[] = {
	&tpda_attr_grp,
	NULL,
};

static int tpda_parse_of_data(struct tpda_drvdata *drvdata)
{
	int len, port, i, ret;
	const __be32 *prop;
	struct device_node *node = drvdata->dev->of_node;

	ret = of_property_read_u32(node, "qcom,tpda-atid", &drvdata->atid);
	if (ret) {
		dev_err(drvdata->dev, "TPDA ATID is not specified\n");
		return -EINVAL;
	}

	prop = of_get_property(node, "qcom,bc-elem-size", &len);
	if (prop) {
		len /= sizeof(__be32);
		if (len < 2 || len > 63 || len % 2 != 0) {
			dev_err(drvdata->dev,
				"Dataset BC width entries are wrong\n");
			return -EINVAL;
		}

		for (i = 0; i < len; i++) {
			port = be32_to_cpu(prop[i++]);
			if (port >= TPDA_MAX_INPORTS) {
				dev_err(drvdata->dev,
					"Wrong port specified for BC\n");
				return -EINVAL;
			}
			drvdata->bc_esize[port] = be32_to_cpu(prop[i]);
		}
	}

	prop = of_get_property(node, "qcom,tc-elem-size", &len);
	if (prop) {
		len /= sizeof(__be32);
		if (len < 2 || len > 63 || len % 2 != 0) {
			dev_err(drvdata->dev,
				"Dataset TC width entries are wrong\n");
			return -EINVAL;
		}

		for (i = 0; i < len; i++) {
			port = be32_to_cpu(prop[i++]);
			if (port >= TPDA_MAX_INPORTS) {
				dev_err(drvdata->dev,
					"Wrong port specified for TC\n");
				return -EINVAL;
			}
			drvdata->tc_esize[port] = be32_to_cpu(prop[i]);
		}
	}

	prop = of_get_property(node, "qcom,dsb-elem-size", &len);
	if (prop) {
		len /= sizeof(__be32);
		if (len < 2 || len > 63 || len % 2 != 0) {
			dev_err(drvdata->dev,
				"Dataset DSB width entries are wrong\n");
			return -EINVAL;
		}

		for (i = 0; i < len; i++) {
			port = be32_to_cpu(prop[i++]);
			if (port >= TPDA_MAX_INPORTS) {
				dev_err(drvdata->dev,
					"Wrong port specified for DSB\n");
				return -EINVAL;
			}
			drvdata->dsb_esize[port] = be32_to_cpu(prop[i]);
		}
	}

	prop = of_get_property(node, "qcom,cmb-elem-size", &len);
	if (prop) {
		len /= sizeof(__be32);
		if (len < 2 || len > 63 || len % 2 != 0) {
			dev_err(drvdata->dev,
				"Dataset CMB width entries are wrong\n");
			return -EINVAL;
		}

		for (i = 0; i < len; i++) {
			port = be32_to_cpu(prop[i++]);
			if (port >= TPDA_MAX_INPORTS) {
				dev_err(drvdata->dev,
					"Wrong port specified for CMB\n");
				return -EINVAL;
			}
			drvdata->cmb_esize[port] = be32_to_cpu(prop[i]);
		}
	}
	return 0;
}

static void tpda_init_default_data(struct tpda_drvdata *drvdata)
{
	drvdata->freq_ts = true;
}

static int tpda_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct tpda_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (coresight_fuse_access_disabled())
		return -EPERM;

	pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tpda-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	mutex_init(&drvdata->lock);

	ret = tpda_parse_of_data(drvdata);
	if (ret)
		return ret;

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	tpda_init_default_data(drvdata);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_LINK;
	desc->subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_MERG;
	desc->ops = &tpda_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = tpda_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(drvdata->dev, "TPDA initialized\n");
	return 0;
}

static int tpda_remove(struct platform_device *pdev)
{
	struct tpda_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id tpda_match[] = {
	{.compatible = "qcom,coresight-tpda"},
	{}
};

static struct platform_driver tpda_driver = {
	.probe          = tpda_probe,
	.remove         = tpda_remove,
	.driver         = {
		.name   = "coresight-tpda",
		.owner	= THIS_MODULE,
		.of_match_table = tpda_match,
	},
};

static int __init tpda_init(void)
{
	return platform_driver_register(&tpda_driver);
}
module_init(tpda_init);

static void __exit tpda_exit(void)
{
	platform_driver_unregister(&tpda_driver);
}
module_exit(tpda_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trace, Profiling & Diagnostic Aggregator driver");
