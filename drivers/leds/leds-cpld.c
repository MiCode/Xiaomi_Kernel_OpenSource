/* include/asm/mach-msm/leds-cpld.c
 *
 * Copyright (C) 2008 HTC Corporation.
 *
 * Author: Farmer Tseng
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <asm/mach-types.h>

#define DEBUG_LED_CHANGE 0

static int _g_cpld_led_addr;

struct CPLD_LED_data {
	spinlock_t data_lock;
	struct led_classdev leds[4];	/* blue, green, red */
};

static ssize_t led_blink_solid_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct CPLD_LED_data *CPLD_LED;
	int idx = 2;
	uint8_t reg_val;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = 0;

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else if (!strcmp(led_cdev->name, "green"))
		idx = 1;
	else
		idx = 2;

	CPLD_LED = container_of(led_cdev, struct CPLD_LED_data, leds[idx]);

	spin_lock(&CPLD_LED->data_lock);
	reg_val = readb(_g_cpld_led_addr);
	reg_val = reg_val >> (2 * idx + 1);
	reg_val &= 0x1;
	spin_unlock(&CPLD_LED->data_lock);

	/* no lock needed for this */
	sprintf(buf, "%u\n", reg_val);
	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t led_blink_solid_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct CPLD_LED_data *CPLD_LED;
	int idx = 2;
	uint8_t reg_val;
	char *after;
	unsigned long state;
	ssize_t ret = -EINVAL;
	size_t count;

	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else if (!strcmp(led_cdev->name, "green"))
		idx = 1;
	else
		idx = 2;

	CPLD_LED = container_of(led_cdev, struct CPLD_LED_data, leds[idx]);

	state = simple_strtoul(buf, &after, 10);

	count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		spin_lock(&CPLD_LED->data_lock);
		reg_val = readb(_g_cpld_led_addr);
		if (state)
			reg_val |= 1 << (2 * idx + 1);
		else
			reg_val &= ~(1 << (2 * idx + 1));

		writeb(reg_val, _g_cpld_led_addr);
		spin_unlock(&CPLD_LED->data_lock);
	}

	return ret;
}

static DEVICE_ATTR(blink, 0644, led_blink_solid_show, led_blink_solid_store);

static ssize_t cpldled_blink_all_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	uint8_t reg_val;
	struct CPLD_LED_data *CPLD_LED = dev_get_drvdata(dev);
	ssize_t ret = 0;

	spin_lock(&CPLD_LED->data_lock);
	reg_val = readb(_g_cpld_led_addr);
	reg_val &= 0x2A;
	if (reg_val == 0x2A)
		reg_val = 1;
	else
		reg_val = 0;
	spin_unlock(&CPLD_LED->data_lock);

	/* no lock needed for this */
	sprintf(buf, "%u\n", reg_val);
	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t cpldled_blink_all_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	uint8_t reg_val;
	char *after;
	unsigned long state;
	ssize_t ret = -EINVAL;
	size_t count;
	struct CPLD_LED_data *CPLD_LED = dev_get_drvdata(dev);

	state = simple_strtoul(buf, &after, 10);

	count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		spin_lock(&CPLD_LED->data_lock);
		reg_val = readb(_g_cpld_led_addr);
		if (state)
			reg_val |= 0x2A;
		else
			reg_val &= ~0x2A;

		writeb(reg_val, _g_cpld_led_addr);
		spin_unlock(&CPLD_LED->data_lock);
	}

	return ret;
}

static struct device_attribute dev_attr_blink_all = {
	.attr = {
		 .name = "blink",
		 .mode = 0644,
		 },
	.show = cpldled_blink_all_show,
	.store = cpldled_blink_all_store,
};

static void led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	struct CPLD_LED_data *CPLD_LED;
	int idx = 2;
	struct led_classdev *led;
	uint8_t reg_val;

	if (!strcmp(led_cdev->name, "jogball-backlight")) {
		if (brightness > 7)
			reg_val = 1;
		else
			reg_val = brightness;
		writeb(0, _g_cpld_led_addr + 0x8);
		writeb(reg_val, _g_cpld_led_addr + 0x8);
#if DEBUG_LED_CHANGE
		printk(KERN_INFO "LED change: jogball backlight = %d \n",
		       reg_val);
#endif
		return;
	} else if (!strcmp(led_cdev->name, "red")) {
		idx = 0;
	} else if (!strcmp(led_cdev->name, "green")) {
		idx = 1;
	} else {
		idx = 2;
	}

	CPLD_LED = container_of(led_cdev, struct CPLD_LED_data, leds[idx]);
	spin_lock(&CPLD_LED->data_lock);
	reg_val = readb(_g_cpld_led_addr);
	led = &CPLD_LED->leds[idx];

	if (led->brightness > LED_OFF)
		reg_val |= 1 << (2 * idx);
	else
		reg_val &= ~(1 << (2 * idx));

	writeb(reg_val, _g_cpld_led_addr);
#if DEBUG_LED_CHANGE
	printk(KERN_INFO "LED change: %s = %d \n", led_cdev->name, led->brightness);
#endif
	spin_unlock(&CPLD_LED->data_lock);
}

static ssize_t cpldled_grpfreq_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", 0);
}

static ssize_t cpldled_grpfreq_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return 0;
}

static DEVICE_ATTR(grpfreq, 0644, cpldled_grpfreq_show, cpldled_grpfreq_store);

static ssize_t cpldled_grppwm_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", 0);
}

static ssize_t cpldled_grppwm_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	return 0;
}

static DEVICE_ATTR(grppwm, 0644, cpldled_grppwm_show, cpldled_grppwm_store);

static int CPLD_LED_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i, j;
	struct resource *res;
	struct CPLD_LED_data *CPLD_LED;

	CPLD_LED = kzalloc(sizeof(struct CPLD_LED_data), GFP_KERNEL);
	if (CPLD_LED == NULL) {
		printk(KERN_ERR "CPLD_LED_probe: no memory for device\n");
		ret = -ENOMEM;
		goto err_alloc_failed;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENOMEM;
		goto err_alloc_failed;
	}

	_g_cpld_led_addr = res->start;
	if (!_g_cpld_led_addr) {
		ret = -ENOMEM;
		goto err_alloc_failed;
	}

	memset(CPLD_LED, 0, sizeof(struct CPLD_LED_data));
	writeb(0x00, _g_cpld_led_addr);

	CPLD_LED->leds[0].name = "red";
	CPLD_LED->leds[0].brightness_set = led_brightness_set;

	CPLD_LED->leds[1].name = "green";
	CPLD_LED->leds[1].brightness_set = led_brightness_set;

	CPLD_LED->leds[2].name = "blue";
	CPLD_LED->leds[2].brightness_set = led_brightness_set;

	CPLD_LED->leds[3].name = "jogball-backlight";
	CPLD_LED->leds[3].brightness_set = led_brightness_set;

	spin_lock_init(&CPLD_LED->data_lock);

	for (i = 0; i < 4; i++) {	/* red, green, blue jogball */
		ret = led_classdev_register(&pdev->dev, &CPLD_LED->leds[i]);
		if (ret) {
			printk(KERN_ERR
			       "CPLD_LED: led_classdev_register failed\n");
			goto err_led_classdev_register_failed;
		}
	}

	for (i = 0; i < 3; i++) {
		ret =
		    device_create_file(CPLD_LED->leds[i].dev, &dev_attr_blink);
		if (ret) {
			printk(KERN_ERR
			       "CPLD_LED: device_create_file failed\n");
			goto err_out_attr_blink;
		}
	}

	dev_set_drvdata(&pdev->dev, CPLD_LED);
	ret = device_create_file(&pdev->dev, &dev_attr_blink_all);
	if (ret) {
		printk(KERN_ERR
		       "CPLD_LED: create dev_attr_blink_all failed\n");
		goto err_out_attr_blink;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_grppwm);
	if (ret) {
		printk(KERN_ERR
		       "CPLD_LED: create dev_attr_grppwm failed\n");
		goto err_out_attr_grppwm;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_grpfreq);
	if (ret) {
		printk(KERN_ERR
		       "CPLD_LED: create dev_attr_grpfreq failed\n");
		goto err_out_attr_grpfreq;
	}

	return 0;

err_out_attr_grpfreq:
	device_remove_file(&pdev->dev, &dev_attr_grppwm);
err_out_attr_grppwm:
	device_remove_file(&pdev->dev, &dev_attr_blink_all);
err_out_attr_blink:
	for (j = 0; j < i; j++)
		device_remove_file(CPLD_LED->leds[j].dev, &dev_attr_blink);
	i = 3;

err_led_classdev_register_failed:
	for (j = 0; j < i; j++)
		led_classdev_unregister(&CPLD_LED->leds[j]);

err_alloc_failed:
	kfree(CPLD_LED);

	return ret;
}

static int __devexit CPLD_LED_remove(struct platform_device *pdev)
{
	struct CPLD_LED_data *CPLD_LED;
	int i;

	CPLD_LED = platform_get_drvdata(pdev);

	for (i = 0; i < 3; i++) {
		device_remove_file(CPLD_LED->leds[i].dev, &dev_attr_blink);
		led_classdev_unregister(&CPLD_LED->leds[i]);
	}

	device_remove_file(&pdev->dev, &dev_attr_blink_all);
	device_remove_file(&pdev->dev, &dev_attr_grppwm);
	device_remove_file(&pdev->dev, &dev_attr_grpfreq);

	kfree(CPLD_LED);
	return 0;
}

static struct platform_driver CPLD_LED_driver = {
	.probe = CPLD_LED_probe,
	.remove = __devexit_p(CPLD_LED_remove),
	.driver = {
		   .name = "leds-cpld",
		   .owner = THIS_MODULE,
		   },
};

static int __init CPLD_LED_init(void)
{
	return platform_driver_register(&CPLD_LED_driver);
}

static void __exit CPLD_LED_exit(void)
{
	platform_driver_unregister(&CPLD_LED_driver);
}

MODULE_AUTHOR("Farmer Tseng");
MODULE_DESCRIPTION("CPLD_LED driver");
MODULE_LICENSE("GPL");

module_init(CPLD_LED_init);
module_exit(CPLD_LED_exit);
