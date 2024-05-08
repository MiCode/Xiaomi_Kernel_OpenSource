// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 *
 * Configure the LED display mode to show the running status
 *
 */
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <media/ispv4_defs.h>
#define GPIOLED_CNT 1
#define GPIOLED_NAME "ispv4_led"
#define LEDOFF 0
#define LEDON 1

static struct dentry *led_debugfs;
enum led_colors { GREEN = 0, RED, BLUE, LED_MAX };
struct gpioled_dev {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct device_node *nd;
	struct timer_list timer;
	int led_gpio[LED_MAX]; /* led GPIO node*/
	spinlock_t lock;
	int timeperiod; /* timeout ms */
	int ledstat;
	int green_stat;
	int red_stat;
	int blue_stat;
};
struct gpioled_dev gpioled;
void my_timer_function(struct timer_list *mytimer)
{
	switch (gpioled.ledstat) {
	case LED_GREEN_2S:
	case LED_GREEN_05S:
		gpioled.green_stat = !gpioled.green_stat;
		break;
	case LED_GREEN_BLUE_ALTER_1s:
		gpioled.green_stat = !gpioled.green_stat;
		gpioled.blue_stat = !gpioled.blue_stat;
		break;
	default:
		break;
	}
	gpio_set_value(gpioled.led_gpio[GREEN], gpioled.green_stat);
	gpio_set_value(gpioled.led_gpio[BLUE], gpioled.blue_stat);
	gpio_set_value(gpioled.led_gpio[RED], gpioled.red_stat);
	mod_timer(&gpioled.timer,
		  jiffies + msecs_to_jiffies(gpioled.timeperiod));
}
static int led_open(struct inode *inode, struct file *filp)
{
	return 0;
}
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt,
			loff_t *offt)
{
	return 0;
}
void led_mode_set(int mode)
{
	switch (mode) {
	case LED_GREEN_2S:
		gpioled.timeperiod = 2000;
		mod_timer(&gpioled.timer,
			  jiffies + msecs_to_jiffies(gpioled.timeperiod));
		pr_info("ispv4 led green led 2s blink\n");
		break;
	case LED_GREEN_05S:
		gpioled.timeperiod = 500;
		mod_timer(&gpioled.timer,
			  jiffies + msecs_to_jiffies(gpioled.timeperiod));
		pr_info("ispv4 led green led 0.5s blink\n");
		break;
	case LED_GREEN_BLUE_ALTER_1s:
		gpioled.green_stat = LEDON, gpioled.red_stat = LEDOFF,
		gpioled.blue_stat = LEDOFF;
		gpioled.timeperiod = 1000;
		mod_timer(&gpioled.timer,
			  jiffies + msecs_to_jiffies(gpioled.timeperiod));
		pr_info("ispv4 led green and red led 1s blink\n");
		break;
	case LED_GREEN_ALL:
		del_timer_sync(&gpioled.timer);
		gpio_set_value(gpioled.led_gpio[BLUE], LEDOFF);
		gpio_set_value(gpioled.led_gpio[GREEN], LEDON);
		gpio_set_value(gpioled.led_gpio[RED], LEDOFF);
		pr_info("ispv4 led green led always on\n");
		break;
	case LED_RED_ALL:
		del_timer_sync(&gpioled.timer);
		gpio_set_value(gpioled.led_gpio[BLUE], LEDOFF);
		gpio_set_value(gpioled.led_gpio[GREEN], LEDOFF);
		gpio_set_value(gpioled.led_gpio[RED], LEDON);
		pr_info("ispv4 led red led always on\n");
		break;
	case LED_GREEN_RED_ALL:
		del_timer_sync(&gpioled.timer);
		gpio_set_value(gpioled.led_gpio[BLUE], LEDOFF);
		gpio_set_value(gpioled.led_gpio[GREEN], LEDON);
		gpio_set_value(gpioled.led_gpio[RED], LEDON);
		pr_info("ispv4 led green and red led always on\n");
		break;
	default:
		del_timer_sync(&gpioled.timer);
		gpio_set_value(gpioled.led_gpio[BLUE], LEDOFF);
		gpio_set_value(gpioled.led_gpio[GREEN], LEDOFF);
		gpio_set_value(gpioled.led_gpio[RED], LEDOFF);
		pr_info("ispv4 led all led always off\n");
		break;
	}
}
static long led_control(struct file *filp, unsigned int cmd, unsigned long arg)
{
	gpioled.ledstat = arg;
	gpioled.green_stat = LEDOFF;
	gpioled.red_stat = LEDOFF;
	gpioled.blue_stat = LEDOFF;
	switch (cmd) {
	case ISPV4_LED_CTL:
		led_mode_set(gpioled.ledstat);
		break;
	default:
		pr_err("ispv4 led ioctl error\n");
		return -EINVAL;
	}
	return 0;
}
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}
static struct file_operations gpioled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.release = led_release,
	.unlocked_ioctl = led_control,
};
void free_gpio(void)
{
	int index = 0;
	for (index = 0; index < LED_MAX; index++) {
		if (gpioled.led_gpio[index])
			gpio_free(gpioled.led_gpio[index]);
	}
}
static int gpio_led_dt_init(void)
{
	int ret = 0;
	int index = 0;
	char ioname[3][128] = { "ispv4_led_green", "ispv4_led_red",
				"ispv4_led_blue" };
	gpioled.nd = of_find_node_by_name(NULL, "xm_ispv4_led");
	if (gpioled.nd == NULL) {
		pr_err("ispv4 led gpioled node not find!\n");
		return -EINVAL;
	} else {
		pr_info("ispv4 led gpioled node find!\n");
	}
	for (index = 0; index < LED_MAX; index++) {
		gpioled.led_gpio[index] =
			of_get_named_gpio(gpioled.nd, ioname[index], 0);
		if (gpioled.led_gpio[index] < 0) {
			pr_err("ispv4 led gpio-num get failed!\n");
			return -EINVAL;
		}
		pr_info("ispv4 led led-gpio num = %d\n",
			gpioled.led_gpio[index]);
		ret = gpio_request(gpioled.led_gpio[index], ioname[index]);
		if (ret) {
			pr_err("ispv4 led request gpio[%d] failed!\n", index);
			return ret;
		}
		ret = gpio_direction_output(gpioled.led_gpio[index], 0);
		if (ret < 0) {
			pr_err("ispv4 led can't set gpio[%d]!\n", index);
			return ret;
		}
	}
	return 0;
}
static ssize_t mode_set(struct file *file, const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	(void)kstrtouint_from_user(user_buf, count, 10, &gpioled.ledstat);
	pr_info("ispv4 led set mode %d\n", gpioled.ledstat);
	led_mode_set(gpioled.ledstat);
	return count;
}
static struct file_operations led_mode_fops = {
	.open = simple_open,
	.write = mode_set,
};
static int __init led_init(void)
{
	int ret = 0;
	struct dentry *debugfs_file;
	gpioled.ledstat = LED_ALL_OFF;

	ret = gpio_led_dt_init();
	if (ret != 0) {
		pr_err("ispv4 led gpio init fail!\n");
		goto err_gpio;
	}
	gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(gpioled.class)) {
		ret = PTR_ERR(gpioled.class);
		goto err_gpio;
	}
	ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
	if (ret) {
		pr_err("ispv4 led alloc_chrdev_region fail.\n");
		goto err_alloc_chrdev;
	}
	gpioled.major = MAJOR(gpioled.devid);
	gpioled.minor = MINOR(gpioled.devid);
	pr_info("ispv4 led gpioled major=%d,minor=%d\n", gpioled.major,
		gpioled.minor);
	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL,
				       GPIOLED_NAME);
	if (IS_ERR(gpioled.device)) {
		ret = PTR_ERR(gpioled.device);
		goto err_device_create;
	}
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);
	ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
	if (ret) {
		pr_err("ispv4 led cdev_add fail.\n");
		goto err_cdev_add;
	}
	led_debugfs = debugfs_create_dir("ispv4_led", NULL);
	if (!IS_ERR_OR_NULL(led_debugfs)) {
		debugfs_file = debugfs_create_file("mode", 0222, led_debugfs,
						   NULL, &led_mode_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("ispv4 led mode debugfs init failed %d\n",
				PTR_ERR(debugfs_file));
		}
	}
	timer_setup(&gpioled.timer, my_timer_function, 0);
	return 0;
err_cdev_add:
	device_destroy(gpioled.class, gpioled.devid);
err_device_create:
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
err_alloc_chrdev:
	class_destroy(gpioled.class);
err_gpio:
	free_gpio();
	return ret;
}
static void __exit led_exit(void)
{
	free_gpio();
	del_timer_sync(&gpioled.timer);
	cdev_del(&gpioled.cdev);
	device_destroy(gpioled.class, gpioled.devid);
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
	class_destroy(gpioled.class);
	if (!IS_ERR_OR_NULL(led_debugfs)) {
		debugfs_remove(led_debugfs);
	}
}
module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lizexin <lizexin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4 led driver");