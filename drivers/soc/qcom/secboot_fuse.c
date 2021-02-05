#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define secboot_fuse_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define SECBOOT_FUSE		(0x000)

static uint32_t secboot_fuse = 0;

struct secboot_fuse_drvdata {
	void __iomem		*base;
	struct device		*dev;
};

static struct secboot_fuse_drvdata *secdrvdata;

static int secboot_fuse_read(struct seq_file *m, void *v)
{
	struct secboot_fuse_drvdata *drvdata = secdrvdata;

	if (!drvdata)
		return false;

	if (secboot_fuse == 0)
		secboot_fuse = secboot_fuse_readl(drvdata, SECBOOT_FUSE);

	dev_dbg(drvdata->dev, "secboot register: %x\n", secboot_fuse);

	seq_printf(m, "0x%x\n", secboot_fuse);

	return 0;

}

static int secboot_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, secboot_fuse_read, NULL);
}

static const struct file_operations secboot_fops = {
	.open		= secboot_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void secboot_fuse_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create("secboot_fuse_reg", 0 /* default mode */,
			NULL /* parent dir */, &secboot_fops);
}

static int secboot_fuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct secboot_fuse_drvdata *drvdata;
	struct resource *res;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	/* Store the driver data pointer for use in exported functions */
	secdrvdata = drvdata;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sec-boot-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	secboot_fuse_create_proc();
	dev_info(dev, "Secboot-fuse interface initialized\n");
	return 0;
}

static int secboot_fuse_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id secboot_fuse_match[] = {
	{.compatible = "qcom,sec-boot-fuse"},
	{}
};

static struct platform_driver secboot_fuse_driver = {
	.probe          = secboot_fuse_probe,
	.remove         = secboot_fuse_remove,
	.driver         = {
		.name   = "msm-secboot-fuse",
		.owner	= THIS_MODULE,
		.of_match_table = secboot_fuse_match,
	},
};

static int __init secboot_fuse_init(void)
{
	return platform_driver_register(&secboot_fuse_driver);
}
arch_initcall(secboot_fuse_init);

static void __exit secboot_fuse_exit(void)
{
	platform_driver_unregister(&secboot_fuse_driver);
}
module_exit(secboot_fuse_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("JTag Fuse driver");
