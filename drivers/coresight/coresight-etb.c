/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define etb_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define etb_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define ETB_LOCK(drvdata)						\
do {									\
	mb();								\
	etb_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define ETB_UNLOCK(drvdata)						\
do {									\
	etb_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define ETB_RAM_DEPTH_REG	(0x004)
#define ETB_STATUS_REG		(0x00C)
#define ETB_RAM_READ_DATA_REG	(0x010)
#define ETB_RAM_READ_POINTER	(0x014)
#define ETB_RAM_WRITE_POINTER	(0x018)
#define ETB_TRG			(0x01C)
#define ETB_CTL_REG		(0x020)
#define ETB_RWD_REG		(0x024)
#define ETB_FFSR		(0x300)
#define ETB_FFCR		(0x304)
#define ETB_ITMISCOP0		(0xEE0)
#define ETB_ITTRFLINACK		(0xEE4)
#define ETB_ITTRFLIN		(0xEE8)
#define ETB_ITATBDATA0		(0xEEC)
#define ETB_ITATBCTR2		(0xEF0)
#define ETB_ITATBCTR1		(0xEF4)
#define ETB_ITATBCTR0		(0xEF8)

#define BYTES_PER_WORD		4
#define ETB_SIZE_WORDS		4096
#define FRAME_SIZE_WORDS	4

struct etb_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	struct clk		*clk;
	spinlock_t		spinlock;
	bool			reading;
	atomic_t		in_use;
	uint8_t			*buf;
	bool			enable;
	uint32_t		trigger_cntr;
};

static void __etb_enable(struct etb_drvdata *drvdata)
{
	int i;

	ETB_UNLOCK(drvdata);

	etb_writel(drvdata, 0x0, ETB_RAM_WRITE_POINTER);
	for (i = 0; i < ETB_SIZE_WORDS; i++)
		etb_writel(drvdata, 0x0, ETB_RWD_REG);

	etb_writel(drvdata, 0x0, ETB_RAM_WRITE_POINTER);
	etb_writel(drvdata, 0x0, ETB_RAM_READ_POINTER);

	etb_writel(drvdata, drvdata->trigger_cntr, ETB_TRG);
	etb_writel(drvdata, BIT(13) | BIT(0), ETB_FFCR);
	etb_writel(drvdata, BIT(0), ETB_CTL_REG);

	ETB_LOCK(drvdata);
}

static int etb_enable(struct coresight_device *csdev)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;
	unsigned long flags;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	__etb_enable(drvdata);
	drvdata->enable = true;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "ETB enabled\n");
	return 0;
}

static void __etb_disable(struct etb_drvdata *drvdata)
{
	int count;
	uint32_t ffcr;

	ETB_UNLOCK(drvdata);

	ffcr = etb_readl(drvdata, ETB_FFCR);
	ffcr |= BIT(12);
	etb_writel(drvdata, ffcr, ETB_FFCR);
	ffcr |= BIT(6);
	etb_writel(drvdata, ffcr, ETB_FFCR);
	for (count = TIMEOUT_US; BVAL(etb_readl(drvdata, ETB_FFCR), 6) != 0
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while flushing DRVDATA, ETB_FFCR: %#x\n",
	     etb_readl(drvdata, ETB_FFCR));

	etb_writel(drvdata, 0x0, ETB_CTL_REG);
	for (count = TIMEOUT_US; BVAL(etb_readl(drvdata, ETB_FFSR), 1) != 1
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while disabling DRVDATA, ETB_FFSR: %#x\n",
	     etb_readl(drvdata, ETB_FFSR));

	ETB_LOCK(drvdata);
}

static void __etb_dump(struct etb_drvdata *drvdata)
{
	int i;
	uint8_t *buf_ptr;
	uint32_t read_data;
	uint32_t read_ptr;
	uint32_t write_ptr;
	uint32_t frame_off;
	uint32_t frame_endoff;

	ETB_UNLOCK(drvdata);

	read_ptr = etb_readl(drvdata, ETB_RAM_READ_POINTER);
	write_ptr = etb_readl(drvdata, ETB_RAM_WRITE_POINTER);

	frame_off = write_ptr % FRAME_SIZE_WORDS;
	frame_endoff = FRAME_SIZE_WORDS - frame_off;
	if (frame_off) {
		dev_err(drvdata->dev, "write_ptr: %lu not aligned to formatter "
				"frame size\n", (unsigned long)write_ptr);
		dev_err(drvdata->dev, "frameoff: %lu, frame_endoff: %lu\n",
			(unsigned long)frame_off, (unsigned long)frame_endoff);
		write_ptr += frame_endoff;
	}

	if ((etb_readl(drvdata, ETB_STATUS_REG) & BIT(0)) == 0)
		etb_writel(drvdata, 0x0, ETB_RAM_READ_POINTER);
	else
		etb_writel(drvdata, write_ptr, ETB_RAM_READ_POINTER);

	buf_ptr = drvdata->buf;
	for (i = 0; i < ETB_SIZE_WORDS; i++) {
		read_data = etb_readl(drvdata, ETB_RAM_READ_DATA_REG);
		*buf_ptr++ = read_data >> 0;
		*buf_ptr++ = read_data >> 8;
		*buf_ptr++ = read_data >> 16;
		*buf_ptr++ = read_data >> 24;
	}

	if (frame_off) {
		buf_ptr -= (frame_endoff * BYTES_PER_WORD);
		for (i = 0; i < frame_endoff; i++) {
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
		}
	}

	etb_writel(drvdata, read_ptr, ETB_RAM_READ_POINTER);

	ETB_LOCK(drvdata);
}

static void etb_disable(struct coresight_device *csdev)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	__etb_disable(drvdata);
	__etb_dump(drvdata);
	drvdata->enable = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "ETB disabled\n");
}

static void etb_abort(struct coresight_device *csdev)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	__etb_disable(drvdata);
	__etb_dump(drvdata);
	drvdata->enable = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "ETB aborted\n");
}

static const struct coresight_ops_sink etb_sink_ops = {
	.enable		= etb_enable,
	.disable	= etb_disable,
	.abort		= etb_abort,
};

static const struct coresight_ops etb_cs_ops = {
	.sink_ops	= &etb_sink_ops,
};

static void etb_dump(struct etb_drvdata *drvdata)
{
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->enable) {
		__etb_disable(drvdata);
		__etb_dump(drvdata);
		__etb_enable(drvdata);
	}
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "ETB dumped\n");
}

static int etb_open(struct inode *inode, struct file *file)
{
	struct etb_drvdata *drvdata = container_of(file->private_data,
						   struct etb_drvdata, miscdev);

	if (atomic_cmpxchg(&drvdata->in_use, 0, 1))
		return -EBUSY;

	dev_dbg(drvdata->dev, "%s: successfully opened\n", __func__);
	return 0;
}

static ssize_t etb_read(struct file *file, char __user *data,
				size_t len, loff_t *ppos)
{
	struct etb_drvdata *drvdata = container_of(file->private_data,
						   struct etb_drvdata, miscdev);

	if (drvdata->reading == false) {
		etb_dump(drvdata);
		drvdata->reading = true;
	}

	if (*ppos + len > ETB_SIZE_WORDS * BYTES_PER_WORD)
		len = ETB_SIZE_WORDS * BYTES_PER_WORD - *ppos;

	if (copy_to_user(data, drvdata->buf + *ppos, len)) {
		dev_dbg(drvdata->dev, "%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(drvdata->dev, "%s: %d bytes copied, %d bytes left\n",
		__func__, len, (int) (ETB_SIZE_WORDS * BYTES_PER_WORD - *ppos));
	return len;
}

static int etb_release(struct inode *inode, struct file *file)
{
	struct etb_drvdata *drvdata = container_of(file->private_data,
						   struct etb_drvdata, miscdev);

	drvdata->reading = false;
	atomic_set(&drvdata->in_use, 0);

	dev_dbg(drvdata->dev, "%s: released\n", __func__);
	return 0;
}

static const struct file_operations etb_fops = {
	.owner		= THIS_MODULE,
	.open		= etb_open,
	.read		= etb_read,
	.release	= etb_release,
	.llseek		= no_llseek,
};

static ssize_t etb_show_trigger_cntr(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->trigger_cntr;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etb_store_trigger_cntr(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->trigger_cntr = val;
	return size;
}
static DEVICE_ATTR(trigger_cntr, S_IRUGO | S_IWUSR, etb_show_trigger_cntr,
		   etb_store_trigger_cntr);

static struct attribute *etb_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	NULL,
};

static struct attribute_group etb_attr_grp = {
	.attrs = etb_attrs,
};

static const struct attribute_group *etb_attr_grps[] = {
	&etb_attr_grp,
	NULL,
};

static int __devinit etb_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct etb_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	spin_lock_init(&drvdata->spinlock);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	drvdata->buf = devm_kzalloc(dev, ETB_SIZE_WORDS * BYTES_PER_WORD,
				    GFP_KERNEL);
	if (!drvdata->buf)
		return -ENOMEM;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_SINK;
	desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
	desc->ops = &etb_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = etb_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	drvdata->miscdev.name = ((struct coresight_platform_data *)
				 (pdev->dev.platform_data))->name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &etb_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		goto err;

	dev_info(dev, "ETB initialized\n");
	return 0;
err:
	coresight_unregister(drvdata->csdev);
	return ret;
}

static int __devexit etb_remove(struct platform_device *pdev)
{
	struct etb_drvdata *drvdata = platform_get_drvdata(pdev);

	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id etb_match[] = {
	{.compatible = "arm,coresight-etb"},
	{}
};

static struct platform_driver etb_driver = {
	.probe          = etb_probe,
	.remove         = __devexit_p(etb_remove),
	.driver         = {
		.name   = "coresight-etb",
		.owner	= THIS_MODULE,
		.of_match_table = etb_match,
	},
};

static int __init etb_init(void)
{
	return platform_driver_register(&etb_driver);
}
module_init(etb_init);

static void __exit etb_exit(void)
{
	platform_driver_unregister(&etb_driver);
}
module_exit(etb_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Embedded Trace Buffer driver");
