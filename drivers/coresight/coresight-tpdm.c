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

#define tpdm_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define tpdm_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define TPDM_LOCK(drvdata)						\
do {									\
	mb();								\
	tpdm_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define TPDM_UNLOCK(drvdata)						\
do {									\
	tpdm_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

/* CMB Subunit Registers */
#define TPDM_CMB_CR		(0xA00)
#define TPDM_CMB_TIER		(0xA04)
#define TPDM_CMB_TPR(n)		(0xA08 + (n * 4))
#define TPDM_CMB_TPMR(n)	(0xA10 + (n * 4))
#define TPDM_CMB_XPR(n)		(0xA18 + (n * 4))
#define TPDM_CMB_XPMR(n)	(0xA20 + (n * 4))

/* TPDM Specific Registers */
#define TPDM_ITATBCNTRL		(0xEF0)

#define TPDM_DATASETS		32
#define TPDM_CMB_PATT_CMP	2

enum tpdm_dataset {
	TPDM_DS_IMPLDEF,
	TPDM_DS_DSB,
	TPDM_DS_CMB,
	TPDM_DS_TC,
	TPDM_DS_BC,
	TPDM_DS_GPR,
};

enum tpdm_cmb_mode {
	TPDM_CMB_MODE_CONTINUOUS,
	TPDM_CMB_MODE_TRACE_ON_CHANGE,
};

enum tpdm_cmb_patt_bits {
	TPDM_CMB_LSB,
	TPDM_CMB_MSB,
};

#ifdef CONFIG_CORESIGHT_TPDM_DEFAULT_ENABLE
static int boot_enable = 1;
#else
static int boot_enable;
#endif

module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

struct cmb_dataset {
	enum tpdm_cmb_mode	mode;
	uint32_t		patt_val[TPDM_CMB_PATT_CMP];
	uint32_t		patt_mask[TPDM_CMB_PATT_CMP];
	bool			patt_ts;
	uint32_t		trig_patt_val[TPDM_CMB_PATT_CMP];
	uint32_t		trig_patt_mask[TPDM_CMB_PATT_CMP];
	bool			trig_ts;
};

struct tpdm_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct clk		*clk;
	struct mutex		lock;
	DECLARE_BITMAP(datasets, TPDM_DATASETS);
	struct cmb_dataset	*cmb;
};

static void __tpdm_enable_cmb(struct tpdm_drvdata *drvdata)
{
	uint32_t val;

	tpdm_writel(drvdata, drvdata->cmb->patt_val[TPDM_CMB_LSB],
		    TPDM_CMB_TPR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->patt_mask[TPDM_CMB_LSB],
		    TPDM_CMB_TPMR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->patt_val[TPDM_CMB_MSB],
		    TPDM_CMB_TPR(TPDM_CMB_MSB));
	tpdm_writel(drvdata, drvdata->cmb->patt_mask[TPDM_CMB_MSB],
		    TPDM_CMB_TPMR(TPDM_CMB_MSB));

	tpdm_writel(drvdata, drvdata->cmb->trig_patt_val[TPDM_CMB_LSB],
		    TPDM_CMB_XPR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->trig_patt_mask[TPDM_CMB_LSB],
		    TPDM_CMB_XPMR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->trig_patt_val[TPDM_CMB_MSB],
		    TPDM_CMB_XPR(TPDM_CMB_MSB));
	tpdm_writel(drvdata, drvdata->cmb->trig_patt_mask[TPDM_CMB_MSB],
		    TPDM_CMB_XPMR(TPDM_CMB_MSB));

	val = tpdm_readl(drvdata, TPDM_CMB_TIER);
	if (drvdata->cmb->patt_ts == true)
		val = val | BIT(0);
	else
		val = val & ~BIT(0);
	if (drvdata->cmb->trig_ts == true)
		val = val | BIT(1);
	else
		val = val & ~BIT(1);
	tpdm_writel(drvdata, val, TPDM_CMB_TIER);

	val = tpdm_readl(drvdata, TPDM_CMB_CR);
	/* Set the flow control bit */
	val = val & ~BIT(2);
	if (drvdata->cmb->mode == TPDM_CMB_MODE_CONTINUOUS)
		val = val & ~BIT(1);
	else
		val = val | BIT(1);
	tpdm_writel(drvdata, val, TPDM_CMB_CR);
	/* Set the enable bit */
	val = val | BIT(0);
	tpdm_writel(drvdata, val, TPDM_CMB_CR);
}

static void __tpdm_enable(struct tpdm_drvdata *drvdata)
{
	TPDM_UNLOCK(drvdata);

	if (test_bit(TPDM_DS_CMB, drvdata->datasets))
		__tpdm_enable_cmb(drvdata);

	TPDM_LOCK(drvdata);
}

static int tpdm_enable(struct coresight_device *csdev)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	mutex_lock(&drvdata->lock);
	__tpdm_enable(drvdata);
	mutex_unlock(&drvdata->lock);

	dev_info(drvdata->dev, "TPDM tracing enabled\n");
	return 0;
}

static void __tpdm_disable_cmb(struct tpdm_drvdata *drvdata)
{
	uint32_t config;

	if (test_bit(TPDM_DS_CMB, drvdata->datasets)) {
		config = tpdm_readl(drvdata, TPDM_CMB_CR);
		config = config & ~BIT(0);
		tpdm_writel(drvdata, config, TPDM_CMB_CR);
	}
}

static void __tpdm_disable(struct tpdm_drvdata *drvdata)
{
	TPDM_UNLOCK(drvdata);

	if (test_bit(TPDM_DS_CMB, drvdata->datasets))
		__tpdm_disable_cmb(drvdata);

	TPDM_LOCK(drvdata);
}

static void tpdm_disable(struct coresight_device *csdev)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	mutex_lock(&drvdata->lock);
	__tpdm_disable(drvdata);
	mutex_unlock(&drvdata->lock);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "TPDM tracing disabled\n");
}

static const struct coresight_ops_source tpdm_source_ops = {
	.enable		= tpdm_enable,
	.disable	= tpdm_disable,
};

static const struct coresight_ops tpdm_cs_ops = {
	.source_ops	= &tpdm_source_ops,
};

static ssize_t tpdm_show_available_datasets(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;

	if (test_bit(TPDM_DS_IMPLDEF, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s",
				  "IMPLDEF");

	if (test_bit(TPDM_DS_DSB, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "DSB");

	if (test_bit(TPDM_DS_CMB, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "CMB");

	if (test_bit(TPDM_DS_TC, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "TC");

	if (test_bit(TPDM_DS_BC, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "BC");

	if (test_bit(TPDM_DS_GPR, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "GPR");

	size += scnprintf(buf + size, PAGE_SIZE - size, "\n");
	return size;
}
static DEVICE_ATTR(available_datasets, S_IRUGO, tpdm_show_available_datasets,
		   NULL);

static ssize_t tpdm_show_cmb_available_modes(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", "continuous trace_on_change");
}
static DEVICE_ATTR(cmb_available_modes, S_IRUGO, tpdm_show_cmb_available_modes,
		   NULL);

static ssize_t tpdm_show_cmb_mode(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->cmb->mode == TPDM_CMB_MODE_CONTINUOUS ?
			 "continuous" : "trace_on_change");
}

static ssize_t tpdm_store_cmb_mode(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[20] = "";

	if (strlen(buf) >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!strcmp(str, "continuous")) {
		drvdata->cmb->mode = TPDM_CMB_MODE_CONTINUOUS;
	} else if (!strcmp(str, "trace_on_change")) {
		drvdata->cmb->mode = TPDM_CMB_MODE_TRACE_ON_CHANGE;
	} else {
		mutex_unlock(&drvdata->lock);
		return -EINVAL;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_mode, tpdm_store_cmb_mode);

static ssize_t tpdm_show_cmb_patt_val_lsb(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_val[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_val_lsb(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_val[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_val_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_val_lsb,
		   tpdm_store_cmb_patt_val_lsb);

static ssize_t tpdm_show_cmb_patt_mask_lsb(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_mask[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_mask_lsb(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_mask[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_mask_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_mask_lsb, tpdm_store_cmb_patt_mask_lsb);

static ssize_t tpdm_show_cmb_patt_val_msb(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_val[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_val_msb(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_val[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_val_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_val_msb,
		   tpdm_store_cmb_patt_val_msb);

static ssize_t tpdm_show_cmb_patt_mask_msb(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_mask[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_mask_msb(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_mask[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_mask_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_mask_msb, tpdm_store_cmb_patt_mask_msb);

static ssize_t tpdm_show_cmb_patt_ts(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->cmb->patt_ts);
}

static ssize_t tpdm_store_cmb_patt_ts(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->cmb->patt_ts = true;
	else
		drvdata->cmb->patt_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_ts, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_ts, tpdm_store_cmb_patt_ts);

static ssize_t tpdm_show_cmb_trig_patt_val_lsb(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_val[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_val_lsb(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_val[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_val_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_val_lsb,
		   tpdm_store_cmb_trig_patt_val_lsb);

static ssize_t tpdm_show_cmb_trig_patt_mask_lsb(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_mask[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_mask_lsb(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_mask[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_mask_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_mask_lsb,
		   tpdm_store_cmb_trig_patt_mask_lsb);

static ssize_t tpdm_show_cmb_trig_patt_val_msb(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_val[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_val_msb(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_val[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_val_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_val_msb,
		   tpdm_store_cmb_trig_patt_val_msb);

static ssize_t tpdm_show_cmb_trig_patt_mask_msb(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_mask[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_mask_msb(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_mask[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_mask_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_mask_msb,
		   tpdm_store_cmb_trig_patt_mask_msb);

static ssize_t tpdm_show_cmb_trig_ts(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->cmb->trig_ts);
}

static ssize_t tpdm_store_cmb_trig_ts(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->cmb->trig_ts = true;
	else
		drvdata->cmb->trig_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_ts, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_ts, tpdm_store_cmb_trig_ts);

static struct attribute *tpdm_cmb_attrs[] = {
	&dev_attr_cmb_available_modes.attr,
	&dev_attr_cmb_mode.attr,
	&dev_attr_cmb_patt_val_lsb.attr,
	&dev_attr_cmb_patt_mask_lsb.attr,
	&dev_attr_cmb_patt_val_msb.attr,
	&dev_attr_cmb_patt_mask_msb.attr,
	&dev_attr_cmb_patt_ts.attr,
	&dev_attr_cmb_trig_patt_val_lsb.attr,
	&dev_attr_cmb_trig_patt_mask_lsb.attr,
	&dev_attr_cmb_trig_patt_val_msb.attr,
	&dev_attr_cmb_trig_patt_mask_msb.attr,
	&dev_attr_cmb_trig_ts.attr,
	NULL,
};

static struct attribute_group tpdm_cmb_attr_grp = {
	.attrs = tpdm_cmb_attrs,
};

static struct attribute *tpdm_attrs[] = {
	&dev_attr_available_datasets.attr,
	NULL,
};

static struct attribute_group tpdm_attr_grp = {
	.attrs = tpdm_attrs,
};
static const struct attribute_group *tpdm_attr_grps[] = {
	&tpdm_attr_grp,
	&tpdm_cmb_attr_grp,
	NULL,
};

static int tpdm_datasets_alloc(struct tpdm_drvdata *drvdata)
{
	if (test_bit(TPDM_DS_CMB, drvdata->datasets)) {
		drvdata->cmb = devm_kzalloc(drvdata->dev, sizeof(*drvdata->cmb),
					    GFP_KERNEL);
		if (!drvdata->cmb)
			return -ENOMEM;
	}
	return 0;
}

static void tpdm_init_default_data(struct tpdm_drvdata *drvdata)
{
	if (test_bit(TPDM_DS_CMB, drvdata->datasets))
		drvdata->cmb->trig_ts = true;
}

static int tpdm_probe(struct platform_device *pdev)
{
	int ret, i;
	uint32_t pidr;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct tpdm_drvdata *drvdata;
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tpdm-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	mutex_init(&drvdata->lock);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	pidr = tpdm_readl(drvdata, CORESIGHT_PERIPHIDR0);

	clk_disable_unprepare(drvdata->clk);

	for (i = 0; i < TPDM_DATASETS; i++) {
		if (pidr & BIT(i))
			__set_bit(i, drvdata->datasets);
	}

	ret = tpdm_datasets_alloc(drvdata);
	if (ret)
		return ret;

	tpdm_init_default_data(drvdata);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_PROC;
	desc->ops = &tpdm_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = tpdm_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(drvdata->dev, "TPDM initialized\n");

	if (boot_enable)
		coresight_enable(drvdata->csdev);

	return 0;
}

static int tpdm_remove(struct platform_device *pdev)
{
	struct tpdm_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id tpdm_match[] = {
	{.compatible = "qcom,coresight-tpdm"},
	{}
};

static struct platform_driver tpdm_driver = {
	.probe          = tpdm_probe,
	.remove         = tpdm_remove,
	.driver         = {
		.name   = "coresight-tpdm",
		.owner	= THIS_MODULE,
		.of_match_table = tpdm_match,
	},
};

static int __init tpdm_init(void)
{
	return platform_driver_register(&tpdm_driver);
}
module_init(tpdm_init);

static void __exit tpdm_exit(void)
{
	platform_driver_unregister(&tpdm_driver);
}
module_exit(tpdm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trace, Profiling & Diagnostic Monitor driver");
