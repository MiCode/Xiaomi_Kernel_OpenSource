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
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define etb_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define etb_readl(drvdata, off)		__raw_readl(drvdata->base + off)

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

#define ETB_LOCK()							\
do {									\
	mb();								\
	etb_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define ETB_UNLOCK()							\
do {									\
	etb_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

struct etb_drvdata {
	uint8_t		*buf;
	void __iomem	*base;
	bool		enabled;
	bool		reading;
	spinlock_t	spinlock;
	atomic_t	in_use;
	struct device	*dev;
	struct kobject	*kobj;
	struct clk	*clk;
	uint32_t	trigger_cntr;
};

static struct etb_drvdata *drvdata;

static void __etb_enable(void)
{
	int i;

	ETB_UNLOCK();

	etb_writel(drvdata, 0x0, ETB_RAM_WRITE_POINTER);
	for (i = 0; i < ETB_SIZE_WORDS; i++)
		etb_writel(drvdata, 0x0, ETB_RWD_REG);

	etb_writel(drvdata, 0x0, ETB_RAM_WRITE_POINTER);
	etb_writel(drvdata, 0x0, ETB_RAM_READ_POINTER);

	etb_writel(drvdata, drvdata->trigger_cntr, ETB_TRG);
	etb_writel(drvdata, BIT(13) | BIT(0), ETB_FFCR);
	etb_writel(drvdata, BIT(0), ETB_CTL_REG);

	ETB_LOCK();
}

int etb_enable(void)
{
	int ret;
	unsigned long flags;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	__etb_enable();
	drvdata->enabled = true;
	dev_info(drvdata->dev, "ETB enabled\n");
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return 0;
}

static void __etb_disable(void)
{
	int count;
	uint32_t ffcr;

	ETB_UNLOCK();

	ffcr = etb_readl(drvdata, ETB_FFCR);
	ffcr |= (BIT(12) | BIT(6));
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

	ETB_LOCK();
}

void etb_disable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	__etb_disable();
	drvdata->enabled = false;
	dev_info(drvdata->dev, "ETB disabled\n");
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	clk_disable_unprepare(drvdata->clk);
}

static void __etb_dump(void)
{
	int i;
	uint8_t *buf_ptr;
	uint32_t read_data;
	uint32_t read_ptr;
	uint32_t write_ptr;
	uint32_t frame_off;
	uint32_t frame_endoff;

	ETB_UNLOCK();

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

	ETB_LOCK();
}

void etb_dump(void)
{
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->enabled) {
		__etb_disable();
		__etb_dump();
		__etb_enable();

		dev_info(drvdata->dev, "ETB dumped\n");
	}
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
}

static int etb_open(struct inode *inode, struct file *file)
{
	if (atomic_cmpxchg(&drvdata->in_use, 0, 1))
		return -EBUSY;

	dev_dbg(drvdata->dev, "%s: successfully opened\n", __func__);
	return 0;
}

static ssize_t etb_read(struct file *file, char __user *data,
				size_t len, loff_t *ppos)
{
	if (drvdata->reading == false) {
		etb_dump();
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
	drvdata->reading = false;

	atomic_set(&drvdata->in_use, 0);

	dev_dbg(drvdata->dev, "%s: released\n", __func__);

	return 0;
}

static const struct file_operations etb_fops = {
	.owner =	THIS_MODULE,
	.open =		etb_open,
	.read =		etb_read,
	.release =	etb_release,
};

static struct miscdevice etb_misc = {
	.name =		"msm_etb",
	.minor =	MISC_DYNAMIC_MINOR,
	.fops =		&etb_fops,
};

static ssize_t etb_show_trigger_cntr(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	unsigned long val = drvdata->trigger_cntr;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etb_store_trigger_cntr(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->trigger_cntr = val;
	return size;
}
static DEVICE_ATTR(trigger_cntr, S_IRUGO | S_IWUSR, etb_show_trigger_cntr,
		   etb_store_trigger_cntr);

static int __devinit etb_sysfs_init(void)
{
	int ret;

	drvdata->kobj = kobject_create_and_add("etb", qdss_get_modulekobj());
	if (!drvdata->kobj) {
		dev_err(drvdata->dev, "failed to create ETB sysfs kobject\n");
		ret = -ENOMEM;
		goto err_create;
	}

	ret = sysfs_create_file(drvdata->kobj, &dev_attr_trigger_cntr.attr);
	if (ret) {
		dev_err(drvdata->dev, "failed to create ETB sysfs trigger_cntr"
		" attribute\n");
		goto err_file;
	}

	return 0;
err_file:
	kobject_put(drvdata->kobj);
err_create:
	return ret;
}

static void __devexit etb_sysfs_exit(void)
{
	sysfs_remove_file(drvdata->kobj, &dev_attr_trigger_cntr.attr);
	kobject_put(drvdata->kobj);
}

static int __devinit etb_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		ret = -ENOMEM;
		goto err_kzalloc_drvdata;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res;
	}

	drvdata->base = ioremap_nocache(res->start, resource_size(res));
	if (!drvdata->base) {
		ret = -EINVAL;
		goto err_ioremap;
	}

	drvdata->dev = &pdev->dev;

	spin_lock_init(&drvdata->spinlock);

	drvdata->clk = clk_get(drvdata->dev, "core_clk");
	if (IS_ERR(drvdata->clk)) {
		ret = PTR_ERR(drvdata->clk);
		goto err_clk_get;
	}

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		goto err_clk_rate;

	ret = misc_register(&etb_misc);
	if (ret)
		goto err_misc;

	drvdata->buf = kzalloc(ETB_SIZE_WORDS * BYTES_PER_WORD, GFP_KERNEL);
	if (!drvdata->buf) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	etb_sysfs_init();

	dev_info(drvdata->dev, "ETB initialized\n");
	return 0;

err_alloc:
	misc_deregister(&etb_misc);
err_misc:
err_clk_rate:
	clk_put(drvdata->clk);
err_clk_get:
	iounmap(drvdata->base);
err_ioremap:
err_res:
	kfree(drvdata);
err_kzalloc_drvdata:
	dev_err(drvdata->dev, "ETB init failed\n");
	return ret;
}

static int __devexit etb_remove(struct platform_device *pdev)
{
	if (drvdata->enabled)
		etb_disable();
	etb_sysfs_exit();
	kfree(drvdata->buf);
	misc_deregister(&etb_misc);
	clk_put(drvdata->clk);
	iounmap(drvdata->base);
	kfree(drvdata);

	return 0;
}

static struct of_device_id etb_match[] = {
	{.compatible = "qcom,msm-etb"},
	{}
};

static struct platform_driver etb_driver = {
	.probe          = etb_probe,
	.remove         = __devexit_p(etb_remove),
	.driver         = {
		.name   = "msm_etb",
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
