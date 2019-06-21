#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define sn_readl(drvdata, off)	__raw_readl(drvdata->base + off)
#define cfi_readl(drvdata, off)	__raw_readl(drvdata->ram_base + off)

#define SERIAL_NUM		(0x000)

static uint32_t sn;
static uint32_t cfi;

struct sn_drvdata {
	void __iomem		*base;
	struct device		*dev;
	void __iomem		*ram_base;
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

	seq_printf(m, "0x%x\n", sn);

	return 0;

}

static int sn_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sn_read, NULL);
}

static const struct file_operations sn_fops = {
	.open		= sn_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int sn_create_proc(void)
{
	struct proc_dir_entry *entry;
	struct sn_drvdata *drvdata = sndrvdata;
	entry = proc_create("serial_num", 0 /* default mode */,
			NULL /* parent dir */, &sn_fops);
	if (!entry) {
		dev_err(drvdata->dev, "sn_create_proc fail\n");
		return -ENOMEM;
	}
	return 0;
}

/*add by 101007198 for cpu feature id*/
static int cfi_read(struct seq_file *m, void *v)
{
	struct sn_drvdata *drvdata = sndrvdata;

	if (!drvdata)
		return false;

	if (cfi == 0)
		cfi = cfi_readl(drvdata, SERIAL_NUM);

	dev_dbg(drvdata->dev, "cpu_feature_id: %x\n", cfi);

	seq_printf(m, "0x%x\n", cfi);

	return 0;

}

static int cfi_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cfi_read, NULL);
}

static const struct file_operations cfi_fops = {
	.open		= cfi_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int cfi_create_proc(void)
{
	struct proc_dir_entry *entry;
	struct sn_drvdata *drvdata = sndrvdata;
	entry = proc_create("cpu_feature_id", 0 /* default mode */,
			NULL /* parent dir */, &cfi_fops);
	if (!entry) {
		dev_err(drvdata->dev, "cfi_create_proc fail\n");
		return -ENOMEM;
	}
	return 0;
}

static int sn_fuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sn_drvdata *drvdata;
	struct resource *res;
	struct resource *rescfi;

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

	/*add by 101007198 for cpu feature id*/
	rescfi = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfi-base");
	if (!rescfi)
		return -ENODEV;
	drvdata->ram_base = devm_ioremap(dev, rescfi->start, resource_size(rescfi));
	if (!drvdata->ram_base)
		return -ENOMEM;

	sn_create_proc();
	dev_info(dev, "SN interface initialized\n");
	/*add by 101007198 for cpu feature id*/
	cfi_create_proc();
	dev_info(dev, "CFI interface initialized\n");
	return 0;
}

static int sn_fuse_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id sn_fuse_match[] = {
	{.compatible = "qcom,sn-fuse"},
	{}
};

static struct platform_driver sn_fuse_driver = {
	.probe          = sn_fuse_probe,
	.remove         = sn_fuse_remove,
	.driver         = {
		.name   = "msm-sn-fuse",
		.owner	= THIS_MODULE,
		.of_match_table = sn_fuse_match,
	},
};

static int __init sn_fuse_init(void)
{
	return platform_driver_register(&sn_fuse_driver);
}
arch_initcall(sn_fuse_init);

static void __exit sn_fuse_exit(void)
{
	platform_driver_unregister(&sn_fuse_driver);
}
module_exit(sn_fuse_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("JTag Fuse driver");

