// SPDX-License-Identifier: GPL-2.0+
/*
 *  Pvpanic Device Support
 *
 *  Copyright (C) 2013 Fujitsu.
 *  Copyright (C) 2018 ZTE.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>
#include <linux/err.h>

//static void __iomem *base;
/*BSP.SYS add for cpu feature id*/
#define cfi_readl(drvdata, off)	__raw_readl(drvdata->base + off)
#define sn_readl(drvdata, off)	__raw_readl(drvdata->base + off)
/*BSP.SYS add for cpu feature id*/
#define FEATURE_NUM		(0x000)
#define SERIAL_NUM		(0x000)

static uint32_t sn;

/*BSP.SYS add for cpu feature id start*/
static unsigned long cfi;
struct cfi_drvdata {
	void __iomem		*base;
	struct device		*dev;
};

static struct cfi_drvdata *cfidrvdata;

static int cfi_read(struct seq_file *m, void *v)
{
	struct cfi_drvdata *drvdata = cfidrvdata;
	if (!drvdata)
		return false;
	if (cfi == 0)
		cfi = cfi_readl(drvdata, FEATURE_NUM);
	dev_dbg(drvdata->dev, "cpu feature id: %x\n", cfi);
	seq_printf(m, "0x%x\n", cfi);
	return 0;
}
/*BSP.SYS add for cpu feature id end*/

struct sn_drvdata {
	void __iomem		*base;
	struct device		*dev;
};

static struct sn_drvdata *sndrvdata;

static int sn_read(struct seq_file *m, void *v)
{
	struct sn_drvdata *drvdata = sndrvdata;
	if (!drvdata)
		return false;
	if (sn == 0)
		sn = sn_readl(drvdata, SERIAL_NUM);
	dev_dbg(drvdata->dev, "serial num: %x\n", sn);
	seq_printf(m, "0x%08x\n", sn);
	return 0;
}

static int sn_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sn_read, NULL);
}

/*BSP.SYS add for cpu feature id start*/
static int cfi_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cfi_read, NULL);
}
/*BSP.SYS add for cpu feature id end*/

static const struct file_operations sn_fops = {
	.open		= sn_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*BSP.SYS add for cpu feature id start*/
static const struct file_operations cfi_fops = {
	.open		= cfi_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int cfi_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create("cpu_feature_id", 0 /* default mode */,
			NULL /* parent dir */, &cfi_fops);
	return 0;
}
/*BSP.SYS add for cpu feature id end*/

static void sn_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create("serial_num", 0 /* default mode */,
			NULL /* parent dir */, &sn_fops);
}

/*BSP.SYS add for cpu feature id start*/
static int cfi_fuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cfi_drvdata *drvdata;
	struct resource *res;
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata){
		return -ENOMEM;
	}
	/* Store the driver data pointer for use in exported functions */
	cfidrvdata = drvdata;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfi-base");
	if (!res){
		return -ENODEV;
	}
	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base){
		return -ENOMEM;
	}
	cfi_create_proc();
	dev_info(dev, "cfi interface initialized\n");
	return 0;
}
/*BSP.SYS add for cpu feature id end*/

static int sn_fuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sn_drvdata *drvdata;
	struct resource *res;
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	/* Store the driver data pointer for use in exported functions */
	sndrvdata = drvdata;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sn-base");
	if (!res)
		return -ENODEV;
	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;
	sn_create_proc();
	dev_info(dev, "SN interface initialized\n");
	return 0;
}

/*BSP.SYS add for cpu feature id start*/
static int cfi_fuse_remove(struct platform_device *pdev)
{
	return 0;
}

void cfi_exit(void)
{
	remove_proc_entry("cpu_feature_id", NULL);
}
/*BSP.SYS add for cpu feature id end*/

static int sn_fuse_remove(struct platform_device *pdev)
{
	return 0;
}

/*BSP.SYS add for cpu feature id start*/
static struct of_device_id cfi_fuse_match[] = {
	{.compatible = "qcom,cfi-fuse"},
	{}
};

static struct platform_driver cfi_fuse_driver = {
	.probe          = cfi_fuse_probe,
	.remove         = cfi_fuse_remove,
	.driver         = {
		.name   = "msm-cfi-fuse",
		.owner  = THIS_MODULE,
		.of_match_table = cfi_fuse_match,
	},
};
/*BSP.SYS add for cpu feature id end*/

static const struct of_device_id sn_fuse_match[] = {
	{ .compatible = "qcom,sn-fuse", },
	{}
};

static struct platform_driver sn_fuse_driver = {
	.driver = {
		.name = "msm-sn-fuse",
		.of_match_table = sn_fuse_match,
	},
	.probe = sn_fuse_probe,
	.remove = sn_fuse_remove,
};

static int __init sn_fuse_init(void)
{
	return platform_driver_register(&sn_fuse_driver);
}

arch_initcall(sn_fuse_init);

/*BSP.SYS add for cpu feature id start*/
static int __init cfi_fuse_init(void)
{
	return platform_driver_register(&cfi_fuse_driver);
}
arch_initcall(cfi_fuse_init);

static void __exit cfi_fuse_exit(void)
{
	platform_driver_unregister(&cfi_fuse_driver);
}
/*BSP.SYS add for cpu feature id end*/

static void __exit sn_fuse_exit(void)
{
	platform_driver_unregister(&sn_fuse_driver);
}
/*BSP.SYS add for cpu feature id*/
module_exit(cfi_fuse_exit);
module_init(sn_fuse_init);
module_exit(sn_fuse_exit);
