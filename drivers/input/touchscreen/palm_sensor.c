#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "palm_sensor.h"

static void palmsensor_ops_work(struct work_struct *work)
{
	struct palm_sensor_device *palm_sensor = NULL;
	int ret;

	palm_sensor = container_of(work, struct palm_sensor_device, palm_work);

	if (!palm_sensor)
		return;

	ret = palm_sensor->palmsensor_switch(palm_sensor->palmsensor_onoff);
	if (ret == 0)
		return;

	pr_err("%s palmsensor_switch error\n", __func__);
	palm_sensor->palmsensor_onoff = false;
}

static int palmsensor_dev_open(struct inode *inode, struct file *file)
{
	struct palm_sensor_device *palm_sensor = NULL;
	int i = MINOR(inode->i_rdev);

	pr_info("%s\n", __func__);
	palm_sensor = palmsensor_dev_get(i);
	if (palm_sensor == NULL ||
		palm_sensor->palmsensor_switch == NULL) {
		pr_info("%s can't get palm sensor\n", __func__);
		return -ENODEV;
	}
	palm_sensor->open_status = true;
	palm_sensor->report_value = 0;
	file->private_data = palm_sensor;
	return 0;
}

static bool palmsensor_get_status(struct palm_sensor_device *palm_sensor)
{
	bool status = false;

	mutex_lock(&palm_sensor->mutex);
	status = palm_sensor->status_changed;
	mutex_unlock(&palm_sensor->mutex);

	return status;
}

static void palmsensor_set_status(struct palm_sensor_device *palm_sensor,
	bool status)
{
	mutex_lock(&palm_sensor->mutex);
	palm_sensor->status_changed = status;
	mutex_unlock(&palm_sensor->mutex);
}

static ssize_t palmsensor_dev_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	ssize_t ret = 0;
	struct palm_sensor_device *palm_sensor = file->private_data;

	if (palm_sensor->palmsensor_onoff)
		wait_event_interruptible(palm_sensor->wait_queue,
			palmsensor_get_status(palm_sensor));
	else {
		pr_info("%s has been off, skip report\n", __func__);
		return 0;
	}
	pr_info("%s, report value:%d\n", __func__, palm_sensor->report_value);
	if (copy_to_user(buf, &palm_sensor->report_value,
			sizeof(palm_sensor->report_value)))
		return -EFAULT;

	palmsensor_set_status(palm_sensor, false);

	ret = sizeof(palm_sensor->report_value);

	return ret;
}

static ssize_t palmsensor_dev_write(struct file *file,
		const char __user *buf, size_t count, loff_t *pos)
{
	char buff[10];
	struct palm_sensor_device *palm_sensor = file->private_data;

	if (copy_from_user(buff, buf, sizeof(palm_sensor->palmsensor_onoff)))
		return -EFAULT;
	if (!palm_sensor->open_status) {
		pr_err("%s has closed\n", __func__);
		return -EFAULT;
	}

	if (buff[0] == '1')
		palm_sensor->palmsensor_onoff = true;
	else
		palm_sensor->palmsensor_onoff = false;

	flush_work(&palm_sensor->palm_work);
	schedule_work(&palm_sensor->palm_work);

	return sizeof(palm_sensor->palmsensor_onoff);
}

static unsigned int palmsensor_dev_poll(struct file *file,
		poll_table *wait)
{
	int ret = 0;
	struct palm_sensor_device *palm_sensor = file->private_data;

	poll_wait(file, &palm_sensor->wait_queue, wait);
	ret = palmsensor_get_status(palm_sensor);
	if (!palm_sensor->open_status) {
		pr_err("%s has closed\n", __func__);
		ret = 0;
	}
	pr_info("%s:%d\n", __func__, ret);

	return ret;
}

static long palmsensor_dev_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = -EINVAL;
	int buf;
	struct palm_sensor_device *palm_sensor = file->private_data;
	void __user *argp = (void __user *) arg;

	ret = copy_from_user(&buf, (int __user *)argp, sizeof(int));

	switch (cmd) {
	case PALM_SENSOR_SWITCH:
		palm_sensor->palmsensor_onoff = (buf == 1 ? true : false);
		palm_sensor->report_value = 0;
		flush_work(&palm_sensor->palm_work);
		schedule_work(&palm_sensor->palm_work);
		wake_up_interruptible(&palm_sensor->wait_queue);
		break;
	default:
		break;
	}

	pr_info("%s: onoff:%d, ret:%d\n", __func__,
			palm_sensor->palmsensor_onoff, ret);

	return ret;
}

static int palmsensor_dev_release(struct inode *inode, struct file *file)
{
	struct palm_sensor_device *palm_sensor = file->private_data;

	pr_info("%s\n", __func__);
	flush_work(&palm_sensor->palm_work);

	palmsensor_set_status(palm_sensor, false);
	palm_sensor->report_value = 0;
	palm_sensor->open_status = false;
	wake_up_interruptible(&palm_sensor->wait_queue);

	return 0;
}

static const struct file_operations palmsensor_dev_fops = {
	.owner = THIS_MODULE,
	.open = palmsensor_dev_open,
	.read = palmsensor_dev_read,
	.write = palmsensor_dev_write,
	.poll = palmsensor_dev_poll,
	.unlocked_ioctl = palmsensor_dev_ioctl,
	.compat_ioctl = palmsensor_dev_ioctl,
	.release = palmsensor_dev_release,
	.llseek	= no_llseek,
};

static struct palm_sensor_device palm_sensor_dev = {
	.misc_dev = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "xiaomi_palm_sensor",
		.fops = &palmsensor_dev_fops,
		.parent = NULL,
	},
	.wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER(palm_sensor_dev.wait_queue),
	.mutex = __MUTEX_INITIALIZER(palm_sensor_dev.mutex),
};

struct palm_sensor_device *palmsensor_dev_get(int minor)
{
	if (palm_sensor_dev.misc_dev.minor == minor)
		return &palm_sensor_dev;
	else
		return NULL;
}

void palmsensor_update_data(int data)
{
	mutex_lock(&palm_sensor_dev.mutex);
	pr_info("%s ++, value:%d\n", __func__, data);
	if (palm_sensor_dev.palmsensor_onoff &&
			palm_sensor_dev.report_value != data) {
		palm_sensor_dev.report_value = data;
		palm_sensor_dev.status_changed = true;
		pr_info("%s --, value:%d\n", __func__, data);
		wake_up_interruptible(&palm_sensor_dev.wait_queue);
	}
	mutex_unlock(&palm_sensor_dev.mutex);
}

int palmsensor_register_switch(int (*cb)(bool on))
{
	if (cb) {
		palm_sensor_dev.palmsensor_switch = cb;
		return 0;
	}
	return -EINVAL;
}

static int palm_sensor_parse_dt(struct device *dev, struct palm_sensor_data *data)
{
	int ret;
	struct device_node *np;

	np = dev->of_node;
	if (!np)
		return -ENODEV;

	ret = of_property_read_string(np, "ps,name", &data->name);
	if (ret)
		return ret;

	pr_info("%s ps,name:%s\n", __func__, data->name);

	return 0;
}

static int palm_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct palm_sensor_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(struct palm_sensor_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = palm_sensor_parse_dt(dev, pdata);
	if (ret < 0)
		goto out;

	ret = misc_register(&palm_sensor_dev.misc_dev);
	if (ret)
		goto out;

	INIT_WORK(&palm_sensor_dev.palm_work, palmsensor_ops_work);

out:
	return ret;

}

static int palm_sensor_remove(struct platform_device *pdev)
{
	misc_deregister(&palm_sensor_dev.misc_dev);

	return 0;
}

static const struct of_device_id palm_sensor_of_match[] = {
	{ .compatible = "palm-sensor", },
	{ },
};

static struct platform_driver palm_sensor_device_driver = {
	.probe		= palm_sensor_probe,
	.remove		= palm_sensor_remove,
	.driver		= {
		.name	= "palm-sensor",
		.of_match_table = of_match_ptr(palm_sensor_of_match),
	}
};

static int __init palm_sensor_init(void)
{
	return platform_driver_register(&palm_sensor_device_driver);
}

static void __exit palm_sensor_exit(void)
{
	platform_driver_unregister(&palm_sensor_device_driver);
}

late_initcall(palm_sensor_init);
module_exit(palm_sensor_exit);

MODULE_LICENSE("GPL");
