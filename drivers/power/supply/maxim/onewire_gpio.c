/*
 * Copyright (c) 2016  xiaomi Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */
#define pr_fmt(fmt)	"[Onewire] %s: " fmt, __func__

#include <linux/slab.h> /* kfree() */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/spinlock.h>

#define ow_info	pr_err
#define ow_dbg	pr_err
#define ow_err	pr_err
#define ow_log	pr_err

#define DRV_STRENGTH_16MA		(0x7 << 6)
#define DRV_STRENGTH_4MA		(0x1 << 6)
#define GPIO_OUTPUT				(0x1 << 9)
#define GPIO_INPUT				(0x0 << 9)
#define GPIO_PULL_UP			0x3
#define OUTPUT_HIGH				(0x1 << 1)
#define OUTPUT_LOW				0x1

#define ONE_WIRE_CONFIG_OUT		writel_relaxed(DRV_STRENGTH_16MA | GPIO_OUTPUT | GPIO_PULL_UP, g_onewire_data->gpio_cfg66_reg)// OUT
#define ONE_WIRE_CONFIG_IN		writel_relaxed(DRV_STRENGTH_16MA | GPIO_INPUT | GPIO_PULL_UP, g_onewire_data->gpio_cfg66_reg)// IN
#define ONE_WIRE_OUT_HIGH		writel_relaxed(OUTPUT_HIGH, g_onewire_data->gpio_in_out_reg)// OUT: 1
#define ONE_WIRE_OUT_LOW		writel_relaxed(OUTPUT_LOW, g_onewire_data->gpio_in_out_reg)// OUT: 0

struct onewire_gpio_data {
	struct platform_device *pdev;
	struct device *dev;

	int ow_gpio;

	struct gpio_desc *ow_gpio_desc;
	struct gpio_chip *ow_gpio_chip;
	void *gpio_in_out_reg;
	void *gpio_cfg66_reg;

	raw_spinlock_t lock;

	struct pinctrl *ow_gpio_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_sleep;

	int version;

	int gpio_num;

	unsigned int onewire_gpio_cfg_addr;
	unsigned int onewire_gpio_level_addr;
	unsigned int gpio_offset;
	unsigned int gpio_reg[2];
};

static struct class *onewire_class;
static int onewire_major;

static int onewire_gpio_detected;
static struct onewire_gpio_data *g_onewire_data;

void Delay_us(unsigned int T)
{
	udelay(T);
}
EXPORT_SYMBOL(Delay_us);

void Delay_ns(unsigned int T)
{
	ndelay(T);
}
EXPORT_SYMBOL(Delay_ns);

unsigned char ow_reset(void)
{
	unsigned char presence = 0xFF;
	unsigned long flags;

	raw_spin_lock_irqsave(&g_onewire_data->lock, flags);

	ONE_WIRE_CONFIG_OUT;
	ONE_WIRE_OUT_LOW;
	Delay_us(50);// 48
	ONE_WIRE_OUT_HIGH;
	ONE_WIRE_CONFIG_IN;
	Delay_us(7);
	presence = (unsigned char)readl_relaxed(g_onewire_data->gpio_in_out_reg) & 0x01; // Read
	Delay_us(50);

	raw_spin_unlock_irqrestore(&g_onewire_data->lock, flags);
	return presence;
}
EXPORT_SYMBOL(ow_reset);

unsigned char read_bit(void)
{
	unsigned int vamm;

	ONE_WIRE_CONFIG_OUT;
	ONE_WIRE_OUT_LOW;
	Delay_us(1);////
	ONE_WIRE_CONFIG_IN;
	Delay_ns(500);//
	vamm = readl_relaxed(g_onewire_data->gpio_in_out_reg); // Read
	Delay_us(5);
	ONE_WIRE_OUT_HIGH;
	ONE_WIRE_CONFIG_OUT;
	Delay_us(6);
	return((unsigned char)vamm & 0x01);
}

void write_bit(char bitval)
{
	ONE_WIRE_OUT_LOW;
	Delay_us(1);//
	if (bitval != 0)
		ONE_WIRE_OUT_HIGH;
	Delay_us(10);
	ONE_WIRE_OUT_HIGH;
	Delay_us(6);
}

unsigned char read_byte(void)
{
	unsigned char i;
	unsigned char value = 0;
	unsigned long flags;

	raw_spin_lock_irqsave(&g_onewire_data->lock, flags);
	for (i = 0; i < 8; i++) {
		if (read_bit())
			value |= 0x01 << i;// reads byte in, one byte at a time and then shifts it left
	}

	raw_spin_unlock_irqrestore(&g_onewire_data->lock, flags);
	return value;
}
EXPORT_SYMBOL(read_byte);

void write_byte(char val)
{
	unsigned char i;
	unsigned char temp;
	unsigned long flags;

	raw_spin_lock_irqsave(&g_onewire_data->lock, flags);
	ONE_WIRE_CONFIG_OUT;
	// writes byte, one bit at a time
	for (i = 0; i < 8; i++) {
		temp = val >> i ; // shifts val right ‘i’ spaces
		temp &= 0x01; // copy that bit to temp
		write_bit(temp); // write bit in temp into
	}
	raw_spin_unlock_irqrestore(&g_onewire_data->lock, flags);
}
EXPORT_SYMBOL(write_byte);

int onewire_gpio_get_status(void)
{
	return onewire_gpio_detected;
}
EXPORT_SYMBOL(onewire_gpio_get_status);

// parse dts
static int onewire_gpio_parse_dt(struct device *dev,
				struct onewire_gpio_data *pdata)
{
	int error, val;
	struct device_node *np = dev->of_node;

	// parse version
	pdata->version = 0;
	error = of_property_read_u32(np, "xiaomi,version", &val);
	if (error && (error != -EINVAL))
		ow_err("Unable to read version\n");
	else if (error != -EINVAL)
		pdata->version = val;

	// parse gpio
	pdata->ow_gpio = of_get_named_gpio_flags(np,
					"xiaomi,ow_gpio", 0, NULL);
	ow_dbg("ow_gpio: %d\n", pdata->ow_gpio);

	// parse gpio_num
	pdata->gpio_num = 0;
	error = of_property_read_u32(np, "xiaomi,gpio_number", &val);
	if (error && (error != -EINVAL))
		ow_err("Unable to read gpio number\n");
	else if (error != -EINVAL)
		pdata->gpio_num = val;

	error = of_property_read_u32_array(np, "mi,onewire-gpio-cfg-addr", pdata->gpio_reg, 2);
	if (error < 0)
		ow_err("Unable to read onewire gpio addr\n");

	pdata->onewire_gpio_cfg_addr = pdata->gpio_reg[0];
	pdata->gpio_offset = pdata->gpio_reg[1];
	pdata->onewire_gpio_level_addr = pdata->onewire_gpio_cfg_addr + pdata->gpio_offset;

	return 0;
}

// pinctrl init
static int onewire_gpio_pinctrl_init(struct onewire_gpio_data *onewire_data)
{
	int retval;

	onewire_data->ow_gpio_pinctrl = devm_pinctrl_get(&onewire_data->pdev->dev);
	if (IS_ERR_OR_NULL(onewire_data->ow_gpio_pinctrl)) {
		retval = PTR_ERR(onewire_data->ow_gpio_pinctrl);
		ow_err("Target does not use pinctrl %d\n", retval);
		goto ow_gpio_err_pinctrl_get;
	}

	onewire_data->pinctrl_state_active
		= pinctrl_lookup_state(onewire_data->ow_gpio_pinctrl,
					"onewire_active");
	if (IS_ERR_OR_NULL(onewire_data->pinctrl_state_active)) {
		retval = PTR_ERR(onewire_data->pinctrl_state_active);
		ow_err("Can not lookup onewire_active pinstate %d\n", retval);
		goto ow_gpio_err_pinctrl_lookup;
	}

	onewire_data->pinctrl_state_sleep
		= pinctrl_lookup_state(onewire_data->ow_gpio_pinctrl,
					"onewire_sleep");
	if (IS_ERR_OR_NULL(onewire_data->pinctrl_state_sleep)) {
		retval = PTR_ERR(onewire_data->pinctrl_state_sleep);
		ow_err("Can not lookup onewire_sleep  pinstate %d\n", retval);
		goto ow_gpio_err_pinctrl_lookup;
	}

	return 0;

ow_gpio_err_pinctrl_get:
	devm_pinctrl_put(onewire_data->ow_gpio_pinctrl);
ow_gpio_err_pinctrl_lookup:
	onewire_data->ow_gpio_pinctrl = NULL;

	return retval;
}

// read data from file
static ssize_t onewire_gpio_ow_gpio_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int status;
	struct onewire_gpio_data *onewire_data = dev_get_drvdata(dev);

	status = gpio_get_value(onewire_data->ow_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d", status);
}

// write data to file
static ssize_t onewire_gpio_ow_gpio_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	unsigned char result = 0x01;
	int buf_int;
	unsigned char i;
	unsigned char RomID[8] = {0};

	if (sscanf(buf, "%1u", &buf_int) != 1)
		return -EINVAL;

	if (buf_int == 0) {
		ONE_WIRE_OUT_LOW;
		ow_log("gpio : OUT 0");
	} else if (buf_int == 1) {
		ONE_WIRE_OUT_HIGH;
		ow_log("gpio : OUT 1");
	} else if (buf_int == 2) {
		ONE_WIRE_CONFIG_OUT;
		ow_log("gpio : OUT");
	} else if (buf_int == 3) {
		ONE_WIRE_CONFIG_IN;
		ow_log("gpio : IN");
	} else if (buf_int == 4) {
		result = ow_reset();
		if (result)
			ow_log("ow_reset: no device.result = %02x", result);
		else
			ow_log("ow_reset: device exist.result = %02x", result);
	} else if (buf_int == 5) {
		result = read_bit();
		ow_log("read_bit: %02x", result);
	} else if (buf_int == 6) {
		write_bit(0x01);
		ow_log("write_bit 0");
	} else if (buf_int == 7) {
		result = ow_reset();
		if (result)
			ow_log("ow_reset: no device.result = %02x", result);
		else
			ow_log("ow_reset: device exist.result = %02x", result);

		ow_dbg("Ready to write 0x33 to maxim IC!\n");
		write_byte(0x33);

		for (i = 0; i < 8; i++)
			RomID[i] = read_byte();

		ow_log("RomID = %02x%02x%02x%02x%02x%02x%02x%02x\n", RomID[0], RomID[1], RomID[2], RomID[3], RomID[4], RomID[5], RomID[6], RomID[7]);
	} else if (buf_int == 8) {
		ONE_WIRE_CONFIG_OUT;
		ONE_WIRE_OUT_HIGH;
		ONE_WIRE_OUT_LOW;
		ONE_WIRE_OUT_HIGH;
		ONE_WIRE_OUT_LOW;
		ONE_WIRE_OUT_HIGH;
		ONE_WIRE_OUT_LOW;
		ONE_WIRE_OUT_HIGH;
		ONE_WIRE_OUT_LOW;
		ONE_WIRE_OUT_HIGH;
		ONE_WIRE_OUT_LOW;

		Delay_us(1000);
		ONE_WIRE_OUT_HIGH;
		Delay_us(1);
		ONE_WIRE_OUT_LOW;
		Delay_us(1);
		ONE_WIRE_OUT_HIGH;
		Delay_us(1);
		ONE_WIRE_OUT_LOW;
		Delay_us(1);
		ONE_WIRE_OUT_HIGH;
		Delay_us(1);
		ONE_WIRE_OUT_LOW;
		Delay_us(1);
		ONE_WIRE_OUT_HIGH;
		Delay_us(1);
		ONE_WIRE_OUT_LOW;
		Delay_us(1);
		ONE_WIRE_OUT_HIGH;
		Delay_us(1);
		ONE_WIRE_OUT_LOW;
		Delay_us(1);

		Delay_us(1000);
		ONE_WIRE_OUT_HIGH;
		Delay_us(5);
		ONE_WIRE_OUT_LOW;
		Delay_us(5);
		ONE_WIRE_OUT_HIGH;
		Delay_us(5);
		ONE_WIRE_OUT_LOW;
		Delay_us(5);
		ONE_WIRE_OUT_HIGH;
		Delay_us(5);
		ONE_WIRE_OUT_LOW;
		Delay_us(5);
		ONE_WIRE_OUT_HIGH;
		Delay_us(5);
		ONE_WIRE_OUT_LOW;
		Delay_us(5);
		ONE_WIRE_OUT_HIGH;
		Delay_us(5);
		ONE_WIRE_OUT_LOW;
		Delay_us(5);
	}

	return count;
}

static DEVICE_ATTR(ow_gpio, S_IRUGO | S_IWUSR | S_IWGRP,
		onewire_gpio_ow_gpio_status_read,
		onewire_gpio_ow_gpio_store);

static int onewire_gpio_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct onewire_gpio_data *onewire_data;
	struct kobject *p;

	ow_log("onewire probe entry");

	if (!pdev->dev.of_node || !of_device_is_available(pdev->dev.of_node))
		return -ENODEV;
	ow_log("lvchen1\n");
	if (pdev->dev.of_node) {
		onewire_data = devm_kzalloc(&pdev->dev,
			sizeof(struct onewire_gpio_data),
			GFP_KERNEL);
		if (!onewire_data) {
			ow_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	ow_log("lvchen2\n");
		retval = onewire_gpio_parse_dt(&pdev->dev, onewire_data);
		if (retval) {
			retval = -EINVAL;
		ow_log("lvchen3\n");
			goto onewire_parse_dt_err;
		}
	} else {
		onewire_data = pdev->dev.platform_data;
	}
	ow_log("lvchen4\n");
	if (!onewire_data) {
		ow_err("No platform data found\n");
		return -EINVAL;
	}
	ow_log("lvchen5\n");
	g_onewire_data = onewire_data;
	onewire_data->pdev = pdev;
	platform_set_drvdata(pdev, onewire_data);

	raw_spin_lock_init(&g_onewire_data->lock);

	// pinctrl init
	retval = onewire_gpio_pinctrl_init(onewire_data);
	if (!retval && onewire_data->ow_gpio_pinctrl) {
		retval = pinctrl_select_state(onewire_data->ow_gpio_pinctrl,
					onewire_data->pinctrl_state_active);
		if (retval < 0)
			ow_err("Failed to select active pinstate %d\n", retval);
	}
	if (retval)
		goto onewire_pinctrl_err;
	ow_log("lvchen6\n");
	// request onewire gpio
	gpio_free(onewire_data->ow_gpio);
	if (gpio_is_valid(onewire_data->ow_gpio)) {
		retval = gpio_request(onewire_data->ow_gpio,
						"onewire gpio");
	} else {
		retval = -EINVAL;
	}
	if (retval) {
		ow_err("request onewire gpio failed, retval=%d\n",
				retval);
		goto onewire_ow_gpio_err;
	}
	ow_log("lvchen7\n");
	// gpio output 1
	gpio_direction_output(onewire_data->ow_gpio, 1);

	onewire_data->ow_gpio_desc = gpio_to_desc(onewire_data->ow_gpio);
	onewire_data->ow_gpio_chip = gpiod_to_chip(onewire_data->ow_gpio_desc);

	onewire_data->gpio_in_out_reg = devm_ioremap(&pdev->dev,
					(uint32_t)onewire_data->onewire_gpio_level_addr, 0x4);
	onewire_data->gpio_cfg66_reg = devm_ioremap(&pdev->dev,
					(uint32_t)onewire_data->onewire_gpio_cfg_addr, 0x4);
	ow_log("onewire_gpio_level_addr is %x; onewire_gpio_cfg_addr is %x", (uint32_t)(onewire_data->onewire_gpio_level_addr), (uint32_t)(onewire_data->onewire_gpio_cfg_addr));
	ow_log("onewire_data->gpio_cfg66_reg is %x; onewire_data->gpio_in_out_reg is %x", (uint32_t)(onewire_data->gpio_cfg66_reg), (uint32_t)(onewire_data->gpio_in_out_reg));

	// create device node
	onewire_data->dev = device_create(onewire_class,
		pdev->dev.parent->parent, onewire_major, onewire_data, "onewirectrl");
	if (IS_ERR(onewire_data->dev)) {
		ow_err("Failed to create interface device\n");
		goto onewire_interface_dev_create_err;
	}
	ow_log("lvchen8\n");
	p = &onewire_data->dev->kobj;

	// create attr file
	retval = sysfs_create_file(p, &dev_attr_ow_gpio.attr);
	if (retval < 0) {
		ow_err("Failed to create sysfs attr file\n");
		goto onewire_sysfs_ow_gpio_err;
	}
	ow_log("lvchen9\n");
	retval = sysfs_create_link(&onewire_data->dev->kobj, &pdev->dev.kobj,
								"pltdev");
	if (retval) {
		ow_err("Failed to create sysfs link\n");
		goto onewire_syfs_create_link_err;
	}
	ow_log("lvchen10\n");
	return 0;
onewire_syfs_create_link_err:
	if (gpio_is_valid(onewire_data->ow_gpio))
		sysfs_remove_file(&pdev->dev.kobj, &dev_attr_ow_gpio.attr);
onewire_sysfs_ow_gpio_err:
	device_destroy(onewire_class, onewire_major);
onewire_interface_dev_create_err:
	if (gpio_is_valid(onewire_data->ow_gpio))
		gpio_free(onewire_data->ow_gpio);
onewire_ow_gpio_err:
onewire_pinctrl_err:
onewire_parse_dt_err:
	kfree(onewire_data);
	return retval;
}

static int onewire_gpio_remove(struct platform_device *pdev)
{
	struct onewire_gpio_data *onewire_data = platform_get_drvdata(pdev);

	if (gpio_is_valid(onewire_data->ow_gpio)) {
		sysfs_remove_file(&pdev->dev.kobj, &dev_attr_ow_gpio.attr);
		gpio_free(onewire_data->ow_gpio);
	}
	kfree(onewire_data);

	return 0;
}

static long onewire_dev_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	ow_dbg("%d, cmd: 0x%x\n", __LINE__, cmd);
	return 0;
}
static int onewire_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int onewire_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations onewire_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= onewire_dev_open,
	.unlocked_ioctl = onewire_dev_ioctl,
	.release	= onewire_dev_release,
};

static const struct of_device_id onewire_gpio_dt_match[] = {
	{.compatible = "xiaomi,onewire_gpio"},
	{},
};

static struct platform_driver onewire_gpio_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "onewire_gpio",
		.of_match_table = onewire_gpio_dt_match,
	},
	.probe = onewire_gpio_probe,
	.remove = onewire_gpio_remove,
};

static int __init onewire_gpio_init(void)
{
	int retval;
	onewire_gpio_detected = false;

	ow_log("onewire gpio init entry.");

	onewire_class = class_create(THIS_MODULE, "onewire");
	if (IS_ERR(onewire_class)) {
		ow_err("coudn't create class");
		return PTR_ERR(onewire_class);
	}

	onewire_major = register_chrdev(0, "onewirectrl", &onewire_dev_fops);
	if (onewire_major < 0) {
		ow_err("failed to allocate char dev\n");
		retval = onewire_major;
		goto class_unreg;
	}

	return platform_driver_register(&onewire_gpio_driver);

class_unreg:
	class_destroy(onewire_class);
	return retval;
}

static void __exit onewire_gpio_exit(void)
{
	ow_log("onewire gpio exit entry.");
	platform_driver_unregister(&onewire_gpio_driver);

	unregister_chrdev(onewire_major, "onewirectrl");
	class_destroy(onewire_class);
}

module_init(onewire_gpio_init);
module_exit(onewire_gpio_exit);

MODULE_AUTHOR("xiaomi Inc.");
MODULE_DESCRIPTION("onewire driver");
MODULE_LICENSE("GPL");
