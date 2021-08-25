#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

struct gpio_data {
	int irq;
	int gpio_num;
	int gpio_status;
};

struct ant_gpio_data {
	struct device *dev;
	int count;  //GPIO num
	int debounce_time;
	int current_irq;
	struct gpio_data *data;
	struct delayed_work debounce_work;
};

static ssize_t gpio_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result = 0;
	int i;
	// int gpio_status_result = 0;
	struct ant_gpio_data *ant_data = dev_get_drvdata(dev);
	for ( i=0; i<ant_data->count; i++ ) {
	     dev_info(dev, "GPIO [%d] status: %d\n",i, ant_data->data[i].gpio_status);
	     if(ant_data->data[i].gpio_status == 1) {
                result = 1;
             }
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}
static DEVICE_ATTR(status, S_IRUGO, gpio_status_show, NULL);

#define MAX_MSG_LENGTH 20
static void gpio_debounce_work(struct work_struct *work)
{
	int i;
	struct ant_gpio_data *ant_data =
			container_of(work, struct ant_gpio_data, debounce_work.work);
	struct device *dev = ant_data->dev;
	char status_env[MAX_MSG_LENGTH];
	char *envp[] = { status_env, NULL };

	//only update changed gpio.
	for ( i=0; i<ant_data->count; i++ ) {
		if ( ant_data->data[i].irq == ant_data->current_irq ) {
			int gpio_status = gpio_get_value(ant_data->data[i].gpio_num);

			if (gpio_status == ant_data->data[i].gpio_status) {
				snprintf(status_env, MAX_MSG_LENGTH, "STATUS=%d", gpio_status);
				dev_info(dev, "Update testing mode status: %d\n", gpio_status);
				kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
			}
			break;
		}
	}
}

static irqreturn_t testing_threaded_irq_handler(int irq, void *irq_data)
{
	int i;
	struct ant_gpio_data *ant_data = irq_data;
	struct device *dev = ant_data->dev;

	dev_info(dev, "irq [%d] triggered\n", irq);
	for ( i=0; i<ant_data->count; i++ ) {
		if ( irq == ant_data->data[i].irq )
		{
			ant_data->data[i].gpio_status = gpio_get_value(ant_data->data[i].gpio_num);
			break;
		}
	}
	ant_data->current_irq = irq;

	mod_delayed_work(system_wq, &ant_data->debounce_work, msecs_to_jiffies(ant_data->debounce_time));

	return IRQ_HANDLED;
}

static int testing_mode_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct ant_gpio_data *ant_data;
	int ant_gpio_num,i;

	pr_info("%s enter\n", __func__);

	ant_data = devm_kzalloc(dev, sizeof(struct ant_gpio_data), GFP_KERNEL);
	if (!ant_data)
		return -ENOMEM;

	// data->status_gpio = of_get_named_gpio(np, "status-gpio", 0);
	ant_data->count = of_gpio_named_count(np, "status-gpio");
	if ( ant_data->count < 1)
		return -EINVAL;

	ant_data->data = devm_kcalloc(dev, ant_data->count, sizeof(struct gpio_data), GFP_KERNEL);
	if (!ant_data->data)
		return -ENOMEM;

	if ( of_property_read_u32(np, "debounce-time", &ant_data->debounce_time) ) {
		dev_info(dev, "Failed to get debounce-time, use default.\n");
		ant_data->debounce_time = 5;
	}

	for ( i=0; i<ant_data->count; i++) {
		ant_gpio_num = of_get_named_gpio(np, "status-gpio", i);
		if (ret < 0) {
			dev_err(dev, "Failed to get ant gpio#%u (%d)\n",i,ant_gpio_num);
		}
		ret = devm_gpio_request(dev, ant_gpio_num, "status-gpio");
		if (ret) {
			dev_err(dev, "Request gpio failed #%u (%d)\n", i ,ant_gpio_num);
			return ret;
		}
		gpio_direction_input(ant_gpio_num);

		dev_dbg(dev, "ant_gpio#%u = %u\n", i, ant_gpio_num);
		ant_data->data[i].irq = gpio_to_irq(ant_gpio_num);
		ant_data->data[i].gpio_num = ant_gpio_num;
		ant_data->data[i].gpio_status = gpio_get_value(ant_data->data[i].gpio_num);

		ret = devm_request_threaded_irq(dev, ant_data->data[i].irq, NULL,
			testing_threaded_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "testing_mode", ant_data);
		if (ret < 0) {
			dev_err(dev, "Failed to request irq.\n");
			return -EINVAL;
		}
		dev_pm_set_wake_irq(dev, ant_data->data[i].irq);
	}

	device_init_wakeup(dev, true);
	INIT_DELAYED_WORK(&ant_data->debounce_work, gpio_debounce_work);
	ret = sysfs_create_file(&dev->kobj, &dev_attr_status.attr);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs node.\n");
		return -EINVAL;
	}

	ant_data->dev = dev;
	platform_set_drvdata(pdev, ant_data);

	return ret;
}

static int testing_mode_remove(struct platform_device *pdev)
{
	struct ant_gpio_data *ant_data = platform_get_drvdata(pdev);

	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_status.attr);
	cancel_delayed_work_sync(&ant_data->debounce_work);
	dev_pm_clear_wake_irq(ant_data->dev);

	return 0;
}

static const struct of_device_id testing_mode_of_match[] = {
	{ .compatible = "modem,testing-mode", },
	{},
};

static struct platform_driver testing_mode_driver = {
	.driver = {
		.name = "testing-mode",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(testing_mode_of_match),
	},
	.probe = testing_mode_probe,
	.remove = testing_mode_remove,
};

module_platform_driver(testing_mode_driver);
MODULE_DESCRIPTION("A simple driver for GPIO testing mode");
MODULE_LICENSE("GPL");


