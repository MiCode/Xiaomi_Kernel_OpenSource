#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

struct xiaomi_fp_data {
	struct device *dev;
	struct mutex lock;
	int fingerdown;
};

static ssize_t get_fingerdown_event(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct xiaomi_fp_data *xiaomi_fp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", xiaomi_fp->fingerdown);
}

static ssize_t set_fingerdown_event(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct xiaomi_fp_data *xiaomi_fp = dev_get_drvdata(dev);

	dev_info(xiaomi_fp->dev, "%s -> %s\n", __func__, buf);
	if (!strncmp(buf, "1", strlen("1"))) {
		xiaomi_fp->fingerdown = 1;
		dev_info(dev, "%s set fingerdown 1 \n", __func__);
		sysfs_notify(&xiaomi_fp->dev->kobj, NULL, "fingerdown");
	}
	else if (!strncmp(buf, "0", strlen("0"))) {
		xiaomi_fp->fingerdown = 0;
		dev_info(dev, "%s set fingerdown 0 \n", __func__);
	}
	else {
		dev_err(dev,"failed to set fingerdown\n");
		return -EINVAL;
	}
	return count;

}

static DEVICE_ATTR(fingerdown, S_IRUSR | S_IWUSR, get_fingerdown_event, set_fingerdown_event);

static struct attribute *attributes[] = {
	&dev_attr_fingerdown.attr,
	NULL
};

static const struct attribute_group attribute_group = {
        .attrs = attributes,
};

static int xiaomi_fp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;

	struct device_node *np = dev->of_node;
	struct xiaomi_fp_data *xiaomi_fp = devm_kzalloc(dev, sizeof(*xiaomi_fp),
						    GFP_KERNEL);

	dev_info(dev, "%s --->: enter! \n", __func__);

	if (!xiaomi_fp) {
		dev_err(dev,"failed to allocate memory for struct xiaomi_fp\n");
		rc = -ENOMEM;
		goto exit;
	}

	xiaomi_fp->dev = dev;
	platform_set_drvdata(pdev, xiaomi_fp);

	if (!np) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}

	mutex_init(&xiaomi_fp->lock);
    
	rc = sysfs_create_group(&dev->kobj, &attribute_group);
	if (rc) {
		dev_err(dev, "xiaomi_fp could not create sysfs\n");
		goto exit;
	}

	xiaomi_fp->fingerdown = 0;

exit:
	dev_info(dev, "%s <---: exit! \n", __func__);
	return rc;
}

static int xiaomi_fp_remove(struct platform_device *pdev)
{
	struct xiaomi_fp_data  *xiaomi_fp = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &attribute_group);
	mutex_destroy(&xiaomi_fp->lock);

	dev_info(&pdev->dev, "%s\n", __func__);

	return 0;
}

static struct of_device_id xiaomi_fp_of_match[] = {
	{.compatible = "xiaomi-fingerprint",},
	{}
};

MODULE_DEVICE_TABLE(of, xiaomi_fp_of_match);

static struct platform_driver xiaomi_fp_driver = {
	.probe = xiaomi_fp_probe,
	.remove = xiaomi_fp_remove,
	.driver = {
		   .name = "xiaomi-fp",
		   .of_match_table = xiaomi_fp_of_match,
	},
};

static void __exit xiaomi_fp_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&xiaomi_fp_driver);
}

static int __init xiaomi_fp_init(void)
{
	int rc;

	rc = platform_driver_register(&xiaomi_fp_driver);
	if (!rc)
		pr_info("%s OK\n", __func__);
	else
		pr_err("%s %d\n", __func__, rc);

	return rc;
}

module_init(xiaomi_fp_init);
module_exit(xiaomi_fp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wuzhen <wuzhen3@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi Fingerprint Class");

