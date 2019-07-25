#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define feature_id_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define FEATURE_ID		(0x000)

static uint32_t feature_id;

struct feature_id_drvdata {
	void __iomem		*base;
	struct device		*dev;
};

static struct feature_id_drvdata *iddrvdata;

static int feature_id_read(struct seq_file *m, void *v)
{
	struct feature_id_drvdata *drvdata = iddrvdata;

	if (!drvdata)
	  return false;

	if (feature_id == 0)
	  feature_id = feature_id_readl(drvdata, FEATURE_ID);

	dev_dbg(drvdata->dev, "cpu-feature-id register: %x\n", feature_id);

	seq_printf(m, "0x%x\n", feature_id);

	return 0;
}

static int featureid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, feature_id_read, NULL);
}

static const struct file_operations featureid_fops = {
	.open		= featureid_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void feature_id_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create("cpu_feature_id", 0 /* default mode */,
			NULL /* parent dir */, &featureid_fops);
}

static int feature_id_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct feature_id_drvdata *drvdata;
	struct resource *res;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	/* Store the driver data pointer for use in exported functions */
	iddrvdata = drvdata;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpu-feature-id");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	feature_id_create_proc();
	dev_info(dev, "Cpu-feature-id interface initialized\n");
	return 0;
}

static int feature_id_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id feature_id_match[] = {
	{.compatible = "qcom,cpu-feature-id"},
	{}
};

static struct platform_driver feature_id_driver = {
	.probe          = feature_id_probe,
	.remove         = feature_id_remove,
	.driver         = {
		.name   = "msm-cpufeature-id",
		.owner	= THIS_MODULE,
		.of_match_table = feature_id_match,
	},
};

static int __init feature_id_init(void)
{
	return platform_driver_register(&feature_id_driver);
}
arch_initcall(feature_id_init);

static void __exit feature_id_exit(void)
{
	platform_driver_unregister(&feature_id_driver);
}
module_exit(feature_id_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("JTag Fuse driver");
