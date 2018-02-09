#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>

#define DEBUG

struct gpio_rf_data {
	int gpio;
	int irq;
	int debounce_interval;
	struct pinctrl *gpio_rf_pinctrl;
	struct device *dev;
	struct work_struct work;
	unsigned int timer_debounce;
	struct timer_list timer;
	int state;
	struct miscdevice misc_dev;
	wait_queue_head_t wait;
	bool readable;
};

static int gpio_rf_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc_dev = filp->private_data;
	struct gpio_rf_data *gdata;
	gdata = container_of(misc_dev, struct gpio_rf_data, misc_dev);
	filp->private_data = gdata;
	return 0;
}

static int gpio_rf_release(struct inode *inode, struct file *flip)
{
	return 0;
}

static unsigned int gpio_rf_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct gpio_rf_data *gdata = filp->private_data;
	poll_wait(filp, &gdata->wait, wait);
	if (gdata->readable)
		mask |= POLLIN;
	return mask;
}

static ssize_t gpio_rf_read(struct file *filp, char __user *buf, size_t count,
			loff_t *f_pos)
{
	struct gpio_rf_data *gdata = filp->private_data;
	if (!gdata->readable) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(gdata->wait, gdata->readable))
			return -ERESTARTSYS;
	}
	if (put_user(gdata->state, buf))
		return -EFAULT;
	gdata->readable = false;
	return sizeof(gdata->state);
}

static struct file_operations gpio_rf_fops = {
	.open = gpio_rf_open,
	.release = gpio_rf_release,
	.poll = gpio_rf_poll,
	.read = gpio_rf_read,
};

static void gpio_rf_work(struct work_struct *work)
{
	struct gpio_rf_data *gdata = container_of(work, struct gpio_rf_data, work);
	if (gpio_get_value(gdata->gpio) == 1)
		gdata->state = 1;
	else
		gdata->state = 0;
	gdata->readable = true;
	wake_up_interruptible(&gdata->wait);
}

static void gpio_rf_timer(unsigned long data)
{
	struct gpio_rf_data *gdata = (struct gpio_rf_data *)data;
	schedule_work(&gdata->work);
}

static irqreturn_t gpio_rf_handler(int irq, void *dev_id)
{
	struct gpio_rf_data *gdata = (struct gpio_rf_data *)dev_id;
	if (gdata->timer_debounce)
		mod_timer(&gdata->timer,
			jiffies + msecs_to_jiffies(gdata->timer_debounce));
	else
		schedule_work(&gdata->work);
	return IRQ_HANDLED;
}

static int gpio_rf_pinctrl_configure(struct gpio_rf_data *gdata, bool active)
{
	struct pinctrl_state *set_state;
	char *gpio_rf_active = "tlmm_gpio_rf_active";
	char *gpio_rf_suspend = "tlmm_gpio_rf_suspend";
	int ret;
	if (active) {
		set_state = pinctrl_lookup_state(gdata->gpio_rf_pinctrl, gpio_rf_active);
		if (IS_ERR(set_state)) {
			dev_err(gdata->dev, "fail to get pinctrl state %s.\n", gpio_rf_active);
			return PTR_ERR(set_state);
		}
	} else {
		set_state = pinctrl_lookup_state(gdata->gpio_rf_pinctrl, gpio_rf_suspend);
		if (IS_ERR(set_state)) {
			dev_err(gdata->dev, "fail to get pinctrl state %s.\n", gpio_rf_suspend);
			return PTR_ERR(set_state);
		}
	}
	ret = pinctrl_select_state(gdata->gpio_rf_pinctrl, set_state);
	if (ret) {
		dev_err(gdata->dev, "cannot set gpio_rf pinctrl state.");
		return ret;
	}
	return 0;
}

static int gpio_rf_suspend(struct device *dev)
{
	struct gpio_rf_data *gdata = dev_get_drvdata(dev);
	int ret;
	if (gdata->gpio_rf_pinctrl) {
		ret = gpio_rf_pinctrl_configure(gdata, false);
		if (ret) {
			dev_err(dev, "fail to set put pin in suspend state.\n");
			return ret;
		}
	}
	return 0;
}

static int gpio_rf_resume(struct device *dev)
{
	struct gpio_rf_data *gdata = dev_get_drvdata(dev);
	int ret;
	if (gdata->gpio_rf_pinctrl) {
		ret = gpio_rf_pinctrl_configure(gdata, true);
		if (ret) {
			dev_err(dev, "fail to set put pin in active state.\n");
			return ret;
		}
	}
	return 0;
}

static int gpio_rf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct gpio_rf_data *gdata;
	int errno;
	unsigned long irqflags;

	gdata = devm_kzalloc(dev, sizeof(struct gpio_rf_data), GFP_KERNEL);
	if (!gdata) {
		dev_err(dev, "fail to allocate memory.\n");
		return -ENOMEM;
	}
	gdata->dev = dev;
	INIT_WORK(&gdata->work, gpio_rf_work);
	setup_timer(&gdata->timer, gpio_rf_timer, (unsigned long)gdata);
	init_waitqueue_head(&gdata->wait);
	gdata->readable = true;

	gdata->misc_dev.name = "gpio_rf";
	gdata->misc_dev.fops = &gpio_rf_fops;
	misc_register(&gdata->misc_dev);

	platform_set_drvdata(pdev, gdata);

	/* Read debounce_interval from device tree and set gpio debounce interval*/
	if (of_property_read_u32(node, "debounce-interval", &gdata->debounce_interval)) {
		gdata->debounce_interval = 15;
	}
	errno = gpio_set_debounce(gdata->gpio, gdata->debounce_interval * 1000);
	if (errno < 0) {
		pr_info("gpio_rf: gpiolib not provide debounce.\n");
		gdata->timer_debounce = gdata->debounce_interval;
	}

	/* Read gpio number from device tree and request gpio port */
	gdata->gpio = of_get_gpio(node, 0);
	pr_info("gpio_rf: gpio number is %d.\n", gdata->gpio);
	if (gdata->gpio < 0) {
		errno = gdata->gpio;
		dev_err(dev, "fail to get gpio number, errno: %d.\n", errno);
		return errno;
	}
	errno = devm_gpio_request(dev, gdata->gpio, "gpio_rf");
	if (errno < 0) {
		dev_err(dev, "fail to request gpio number %d, errno: %d.\n",
			gdata->gpio, errno);
		return errno;
	}
	/* Set gpio_rf as input port */
	errno = gpio_direction_input(gdata->gpio);
	if (errno) {
		dev_err(dev, "can't set gpio_rf as input port.\n");
		return errno;
	}


	/* Check if use pinctrl, if use then set pinctrl state active. */
	gdata->gpio_rf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(gdata->gpio_rf_pinctrl)) {
		pr_info("gpio_rf: Targert does not use pinctrl\n");
		gdata->gpio_rf_pinctrl = NULL;
	}

	if (gdata->gpio_rf_pinctrl) {
		errno = gpio_rf_pinctrl_configure(gdata, true);
		if (errno) {
			dev_err(dev, "cannot set ts pinctrl active state\n");
			return errno;
		}
	}

	/* set init gpio state */
	if (gpio_get_value(gdata->gpio) == 1)
		gdata->state = 1;
	else
		gdata->state = 0;

	/* get irq number and request it */
	gdata->irq = gpio_to_irq(gdata->gpio);
	if (gdata->irq < 0) {
		errno = gdata->irq;
		dev_err(dev, "fail to get irq number for GPIO %d, errno: %d.\n",
				gdata->gpio, errno);
		return errno;
	}
	irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
	errno = devm_request_irq(dev, gdata->irq, gpio_rf_handler, irqflags, "gpio_rf", gdata);
	if (errno < 0) {
		dev_err(dev, "fail to request irq number: %d, errno: %d.\n",
				gdata->irq, errno);
		return errno;
	}
	return 0;
};

static int gpio_rf_remove(struct platform_device *pdev)
{
	struct gpio_rf_data *gdata = platform_get_drvdata(pdev);
	struct miscdevice *misc_dev = &gdata->misc_dev;
	misc_deregister(misc_dev);
	return 0;
};

static const struct of_device_id gpio_rf_of_match[] = {
	{ .compatible = "gpio_rf", },
	{ },
};
static SIMPLE_DEV_PM_OPS(gpio_rf_pm_ops, gpio_rf_suspend, gpio_rf_resume);

static struct platform_driver gpio_rf_driver = {
	.probe = gpio_rf_probe,
	.remove = gpio_rf_remove,
	.driver = {
		.name = "gpio_rf",
		.pm = &gpio_rf_pm_ops,
		.of_match_table  = gpio_rf_of_match,
	}
};

static int __init gpio_rf_init(void)
{
	return platform_driver_register(&gpio_rf_driver);
}

static void __exit gpio_rf_exit(void)
{
	platform_driver_unregister(&gpio_rf_driver);
}

module_init(gpio_rf_init);
module_exit(gpio_rf_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangkai wangkai3@xiaomi.com");
MODULE_DESCRIPTION("driver for gpio_rf");
