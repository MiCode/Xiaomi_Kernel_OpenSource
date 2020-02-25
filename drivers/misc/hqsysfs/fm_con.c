#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <asm/setup.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

int fm_lan_enable_pin = -1;
int gpio_flag = -1;
static struct class *fm_lan_enable_class;
static struct device *fm_lan_enable_dev;

#define FM_LAN_ENABLE    "1"
#define FM_LAN_DISABLE   "0"
#define SNPRINTF_MAXLEN 1024

//cat
static ssize_t lan_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	printk("%s\n", __func__);
	snprintf(buf, SNPRINTF_MAXLEN, "%d\n", gpio_flag);
	return strlen(buf);
}
//echo
static ssize_t lan_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
    if (!strncmp(buf, FM_LAN_ENABLE, strlen(FM_LAN_ENABLE))) {
	printk("%s: to enable gpio_87\n", __func__);
	gpio_set_value(fm_lan_enable_pin, 1);
	gpio_flag = 1;

    } else if (!strncmp(buf, FM_LAN_DISABLE, strlen(FM_LAN_DISABLE))) {
	printk("%s: to disable gpio_87\n", __func__);
	gpio_set_value(fm_lan_enable_pin, 0);
	gpio_flag = 0;
    }

    return count;
}

static struct device_attribute lan_enable_dev_attr = {
    .attr = {
	.name = "lan_enable",
	.mode = S_IRWXU|S_IRWXG|S_IRWXO,
    },
    .show = lan_enable_show,
    .store = lan_enable_store,
};



static int fm_lan_enable_probe(struct platform_device *pdev)
{
    int ret = 0;

    printk("enter lan_enable_probe \n");

    fm_lan_enable_pin = of_get_named_gpio(pdev->dev.of_node, "qcom,fm_lan_enable_pin", 0);
    if (fm_lan_enable_pin < 0)
	printk("fm_lan_enable_pin is not available \n");

    ret = gpio_request(fm_lan_enable_pin, "fm_lan_enable_pin");
    if (0 != ret) {
	printk("gpio request %d failed.", fm_lan_enable_pin);
	goto fail1;
    }
    gpio_direction_output(fm_lan_enable_pin, 0);
    gpio_set_value(fm_lan_enable_pin, 0);
    gpio_flag = 0;

    fm_lan_enable_class = class_create(THIS_MODULE, "fm");
    if (IS_ERR(fm_lan_enable_class)) {
	ret = PTR_ERR(fm_lan_enable_class);
	printk("Failed to create class.\n");
	return ret;
    }

    fm_lan_enable_dev = device_create(fm_lan_enable_class, NULL, 0, NULL, "fm_lan_enable");
    if (IS_ERR(fm_lan_enable_dev)) {
	ret = PTR_ERR(fm_lan_enable_class);
	printk("Failed to create device(fm_lan_enable_dev)!\n");
	return ret;
    }

    ret = device_create_file(fm_lan_enable_dev, &lan_enable_dev_attr);
    if (ret) {
	pr_err("%s: fm_lan_enable_dev creat sysfs failed\n", __func__);
	return ret;
    }

    printk(" enter lan_enable_probe, ok \n");

fail1:
    return ret;
}

static int fm_lan_enable_remove(struct platform_device *pdev)
{
    device_destroy(fm_lan_enable_class, 0);
    class_destroy(fm_lan_enable_class);
    device_remove_file(fm_lan_enable_dev, &lan_enable_dev_attr);

    return 0;
}

static int fm_lan_enable_suspend(struct platform_device *pdev, pm_message_t state)
{
    return 0;
}

static int fm_lan_enable_resume(struct platform_device *pdev)
{
    return 0;
}

static struct of_device_id fm_lan_enable_dt_match[] = {
    { .compatible = "fm,fm_lan_enable",},
    { },
};
MODULE_DEVICE_TABLE(of, fm_lan_enable_dt_match);

static struct platform_driver fm_lan_enable_driver = {
	.driver = {
		.name = "fm_lan_enable",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fm_lan_enable_dt_match),
    },
	.probe = fm_lan_enable_probe,
	.remove = fm_lan_enable_remove,
	.suspend = fm_lan_enable_suspend,
	.resume = fm_lan_enable_resume,
};

static __init int fm_lan_enable_init(void)
{
    return platform_driver_register(&fm_lan_enable_driver);
}

static void __exit fm_lan_enable_exit(void)
{
    platform_driver_unregister(&fm_lan_enable_driver);
}

module_init(fm_lan_enable_init);
module_exit(fm_lan_enable_exit);
MODULE_AUTHOR("GPIO_LAN_ENABLE, Inc.");
MODULE_DESCRIPTION("fm lan_enable");
MODULE_LICENSE("GPL");
