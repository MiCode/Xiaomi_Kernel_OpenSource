#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/device.h>

struct platform_driver smartpainfo_driver_func(void);

unsigned int smartpainfo_num;
char *smartpainfo_vendor;

static ssize_t chip_vendor_show(struct device_driver *ddri, char *buf)
{
	char *chip_vendor = smartpainfo_vendor;
	int ret = 0;

	if (buf == NULL) {
		pr_info("[%s] *buf is NULL!\n",  __func__);
		return -EINVAL;
	}
	ret = snprintf(buf, 15, "%s", chip_vendor);
	if (ret < 0)
		pr_info("snprintf failed\n");

	return strlen(buf);
}

static ssize_t pa_num_show(struct device_driver *ddri, char *buf)
{
	unsigned int pa_num = smartpainfo_num;
	int ret = 0;

	if (buf == NULL) {
		pr_info("[%s] *buf is NULL!\n",  __func__);
		return -EINVAL;
	}
	ret = snprintf(buf, 3, "%d", pa_num);
	if (ret < 0)
		pr_info("snprintf failed\n");

	return strlen(buf);
}

static DRIVER_ATTR_RO(chip_vendor);
static DRIVER_ATTR_RO(pa_num);

static struct driver_attribute *smartpainfo_attr_list[] = {
	&driver_attr_chip_vendor,
	&driver_attr_pa_num,
};


static int smartpainfo_create_attr(struct device_driver *driver)
{
	int idx, err;
	int num = ARRAY_SIZE(smartpainfo_attr_list);

	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, smartpainfo_attr_list[idx]);
		if (err) {
			pr_notice("%s() driver_create_file %s err:%d\n",
			__func__, smartpainfo_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int smartpainfo_probe(struct platform_device *dev)
{
	struct platform_driver smartpainfo_driver_hal = smartpainfo_driver_func();
	int ret = 0;

	pr_info("%s() begin!\n", __func__);

	ret = smartpainfo_create_attr(&smartpainfo_driver_hal.driver);
	if (ret) {
		pr_notice("%s create_attr fail, ret = %d\n", __func__, ret);
		return -1;
	}

	pr_info("%s done!\n", __func__);
	return 0;
}

static int smartpainfo_remove(struct platform_device *dev)
{
	pr_debug("%s done!\n", __func__);
	return 0;
}

const struct of_device_id smartpainfo_of_match[] = {
	{ .compatible = "mediatek,smartpainfo", },
	{},
};

static struct platform_driver smartpainfo_driver = {
	.probe = smartpainfo_probe,
	.remove = smartpainfo_remove,
	.driver = {
		.name = "smartpainfo",
		.of_match_table = smartpainfo_of_match,
	},
};

struct platform_driver smartpainfo_driver_func(void)
{
	return smartpainfo_driver;
}

static int smartpainfo_mod_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&smartpainfo_driver);
	if (ret)
		pr_info("smartpainfo platform_driver_register error:(%d)\n", ret);

	pr_info("%s() done!\n", __func__);
	return ret;
}

static void smartpainfo_mod_exit(void)
{
	pr_info("%s()\n", __func__);
	platform_driver_unregister(&smartpainfo_driver);
}

module_init(smartpainfo_mod_init);
module_exit(smartpainfo_mod_exit);

MODULE_DESCRIPTION("smartpainfo driver");
MODULE_AUTHOR("wangyongfu <wangyongfu@longcheer.com>");
MODULE_LICENSE("GPL v2");
