/*
 * max77932 charger pump watch dog driver
 *
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/pm_wakeup.h>
#include <linux/hwid.h>

#define MAX77932_REG_NUMBER		0x17

#define MAX77932_OVP_UVLO_REG		0x06
#define MAX77932_OVP_MASK		0x30
#define MAX77932_OVP_SHIFT		4
#define MAX77932_CHIP_ID_REG		0x16
#define MAX77932_CHIP_ID		0x60

#define MAX77932_OCP_REG		0x07
#define MAX77932_OCP_MASK		0x1F
#define MAX77932_OCP_10_4_A		0x1D
#define MAX77932_OCP_11_0_A		0x1E

#define MAX77932_OCP_VOLT_REG		0x08
#define	MAX77932_OCP_VOLT_MASK		0x0F
#define MAX77932_OCP_330_MV		0x0E
#define MAX77932_OCP_VOLT_OFF		0x0F

#define MAX77932_OOVP_REG		0x09
#define MAX77932_OOVP_MASK		0x1F
#define MAX77932_OOVP_5_0V		0x12
#define MAX77932_OOVP_5_1V		0x13
#define MAX77932_OOVP_5_2V		0x14
#define MAX77932_OOVP_5_3V		0x15
#define MAX77932_OOVP_5_5V		0x17

enum product_name {
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

struct max77932_chip {
	struct device 		*dev;
	struct i2c_client	*client;
	struct regmap		*regmap;
	struct wakeup_source 	*irq_handle_wakelock;
	struct delayed_work	irq_handle_work;
	int			irq_gpio;
	int			irq;
	bool			chip_ok;
};

enum max77932_ovp {
	MAX77932_OVP_9_5V,
	MAX77932_OVP_10_0V,
	MAX77932_OVP_10_5V,
	MAX77932_OVP_11_0V,
};

product_name = UNKNOW;
static int log_level = 2;

#define max_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[XMCHG_MAX77932] " fmt, ##__VA_ARGS__);	\
} while (0)

#define max_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[XMCHG_MAX77932] " fmt, ##__VA_ARGS__);	\
} while (0)

#define max_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[XMCHG_MAX77932] " fmt, ##__VA_ARGS__);	\
} while (0)

static int max_set_oovp(struct max77932_chip *max, int oovp_val)
{

	int ret = 0;

	ret = regmap_update_bits(max->regmap, MAX77932_OOVP_REG, MAX77932_OOVP_MASK, oovp_val);
	if (ret < 0)
		max_err("Failed to set oovp, ret = %d\n", ret);

	return ret;
}

static int max_set_ovp(struct max77932_chip *max, enum max77932_ovp ovp_val)
{

	int ret = 0;

	ret = regmap_update_bits(max->regmap, MAX77932_OVP_UVLO_REG, MAX77932_OVP_MASK, ovp_val << MAX77932_OVP_SHIFT);
	if (ret < 0)
		max_err("Failed to set ovp, ret = %d\n", ret);

	return ret;
}

static int max_set_ocp_curr(struct max77932_chip *max, int ocp_val)
{
	int ret = 0;

	ret = regmap_update_bits(max->regmap, MAX77932_OCP_REG, MAX77932_OCP_MASK, ocp_val);
	if (ret < 0)
		max_err("Failed to set ocp, ret = %d\n", ret);

	return ret;
}

static int max_set_ocp_volt(struct max77932_chip *max, int ocp_volt)
{
	int ret = 0;

	ret = regmap_update_bits(max->regmap, MAX77932_OCP_VOLT_REG, MAX77932_OCP_VOLT_MASK, ocp_volt);
	if (ret < 0)
		max_err("Failed to set ovp volt, ret = %d\n", ret);

	return ret;
}

static int max77932_dump_reg(struct max77932_chip *max)
{
	unsigned int data[MAX77932_REG_NUMBER];
	int i = 0, ret = 0;

	for (i = 0; i < MAX77932_REG_NUMBER; i++) {
		ret = regmap_read(max->regmap, i, &data[i]);
		if (ret) {
			max_err("failed to read register: 0x%02x\n", i);
			return ret;
		}
	}

	for (i = 0; i < MAX77932_REG_NUMBER; i++) {
		if (i == 0)
			printk(KERN_CONT "[XMCHG_MAX77932] REGS: ");
		printk(KERN_CONT "0x%02x ", i);
		if (i == MAX77932_REG_NUMBER - 1)
			printk(KERN_CONT "\n");
	}

	for (i = 0; i < MAX77932_REG_NUMBER; i++) {
		if (i == 0)
			printk(KERN_CONT "[XMCHG_MAX77932] REGS: ");
		printk(KERN_CONT "0x%02x ", data[i]);
		if (i == MAX77932_REG_NUMBER - 1)
			printk(KERN_CONT "\n");
	}

	return ret;
}

static void max77932_irq_handler(struct work_struct *work)
{
	struct max77932_chip *max = container_of(work, struct max77932_chip, irq_handle_work.work);
	int ret = 0;

	ret = max77932_dump_reg(max);
	if (ret)
		max_err("failed to dump registers\n");

	__pm_relax(max->irq_handle_wakelock);

	return;
}

static irqreturn_t max77932_interrupt(int irq, void *dev_id)
{
	struct max77932_chip *max = dev_id;

	max_info("max77932_interrupt\n");

	if (max->irq_handle_wakelock->active)
		return IRQ_HANDLED;
	else
		__pm_stay_awake(max->irq_handle_wakelock);

	schedule_delayed_work(&max->irq_handle_work, 0);

	return IRQ_HANDLED;
}

static int max77932_init_irq(struct max77932_chip *max)
{
	int ret = 0;

	ret = devm_gpio_request(max->dev, max->irq_gpio, dev_name(max->dev));
	if (ret) {
		max_err("failed to request gpio\n");
		return ret;
	}

	max->irq = gpio_to_irq(max->irq_gpio);
	if (max->irq < 0) {
		max_err("failed to get gpio_irq\n");
		return -1;
	}

	ret = request_irq(max->irq, max77932_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(max->dev), max);
	if (ret) {
		max_err("failed to request irq\n");
		return ret;
	}

	return ret;
}

static int max_parse_dt(struct max77932_chip *max)
{
	struct device_node *node = max->dev->of_node;

	if (!node) {
		max_err("device tree info missing\n");
		return -1;
	}

	max->irq_gpio = of_get_named_gpio(node, "max77932_irq_gpio", 0);
	if (!gpio_is_valid(max->irq_gpio)) {
		max_err("failed to parse max77932_irq_gpio\n");
		return -1;
	}

	return 0;
}

static struct regmap_config i2c_max77932_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = MAX77932_REG_NUMBER - 1,
};

static ssize_t max77932_show_register(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max77932_chip *chip = i2c_get_clientdata(client);
	u8 tmpbuf[300];
	unsigned int reg = 0, data = 0;
	int len = 0, idx = 0, ret = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "MAX77932_REG");
	for (reg = 0x00; reg <= 0x16; reg++) {
		ret = regmap_read(chip->regmap, reg, &data);
		if (ret) {
			max_err("failed to read register\n");
			return idx;
		}

		len = snprintf(tmpbuf, PAGE_SIZE - idx, "Reg[0x%02x] = 0x%02x\n", reg, data);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}

	return idx;
}

static ssize_t max77932_store_register(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max77932_chip *chip = i2c_get_clientdata(client);
	unsigned int reg = 0, val = 0;
	int ret = 0;

	ret = sscanf(buf, "%d %d", &reg, &val);
	max_info("reg = 0x%02x, val = 0x%02x, ret = %d\n", reg, val, ret);

	if (ret == 2 && reg >= 0x00 && reg <= 0x16) {
		regmap_write(chip->regmap, reg, val);
	}

	return count;
}

static ssize_t max_attr_show_chip_ok(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max77932_chip *max = i2c_get_clientdata(client);
	int len = 0, ret = 0, val = 0;

	ret = regmap_read(max->regmap, MAX77932_CHIP_ID_REG, &val);
	if (ret < 0) {
		max_err("Failed to read chip id, ret = %d\n", ret);
		max->chip_ok = false;
	}

	if (val == MAX77932_CHIP_ID)
		max->chip_ok = true;

	len = snprintf(buf, PAGE_SIZE, "%d\n", max->chip_ok);

	return len;
}

static DEVICE_ATTR(register, S_IRUGO | S_IWUSR, max77932_show_register, max77932_store_register);
static DEVICE_ATTR(chip_ok, S_IRUGO, max_attr_show_chip_ok, NULL);

static struct attribute *max_attributes[] = {
	&dev_attr_chip_ok.attr,
	&dev_attr_register.attr,
	NULL,
};

static const struct attribute_group max_attr_group = {
	.attrs = max_attributes,
};

static int max77932_hw_init(struct max77932_chip *max)
{
	int ret = 0;

	ret = max_set_ovp(max, MAX77932_OVP_10_5V);
	ret = max_set_oovp(max, MAX77932_OOVP_5_5V);
	ret = max_set_ocp_curr(max, MAX77932_OCP_11_0_A);
	ret = max_set_ocp_volt(max, MAX77932_OCP_VOLT_OFF);

	return ret;
}

static void max77932_parse_cmdline(void)
{
	char *ruby = NULL, *rubypro = NULL, *rubyplus = NULL;
	const char *sku = get_hw_sku();

	ruby = strnstr(sku, "ruby", strlen(sku));
	rubypro = strnstr(sku, "rubypro", strlen(sku));
	rubyplus = strnstr(sku, "rubyplus", strlen(sku));

	if (rubyplus)
		product_name = RUBYPLUS;
	else if (rubypro)
		product_name = RUBYPRO;
	else if (ruby)
		product_name = RUBY;

	max_info("product_name = %d, ruby = %d, rubypro = %d, rubyplus = %d\n", product_name, ruby ? 1 : 0, rubypro ? 1 : 0, rubyplus ? 1 : 0);
}

static int max77932_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max77932_chip *max;
	int ret = 0;

	max77932_parse_cmdline();

	if (product_name == RUBYPLUS) {
		max_info("MAX77932 probe start\n");
	} else {
		max_info("ruby and rubypro no need to probe MAX77932\n");
		return -ENODEV;
	}
	max_info("MAX77932 probe start\n");

	max = devm_kzalloc(&client->dev, sizeof(*max), GFP_DMA);
	if (!max)
		return -ENOMEM;

	max->dev = &client->dev;
	max->client = client;
	i2c_set_clientdata(client, max);

	max->irq_handle_wakelock = wakeup_source_register(NULL, "max77932_irq_handle_wakelock");
	INIT_DELAYED_WORK(&max->irq_handle_work, max77932_irq_handler);

	ret = max_parse_dt(max);
	if (ret) {
		max_err("failed to parse DTS\n");
		return ret;
	}

	ret = max77932_init_irq(max);
	if (ret) {
		max_err("failed to init IRQ\n");
		return ret;
	}

	max->regmap = devm_regmap_init_i2c(client, &i2c_max77932_regmap_config);
	if (!max->regmap)
		return -ENODEV;

	ret = max77932_hw_init(max);
	if (ret < 0) {
		max_err("failed to hw_init\n");
		return ret;
	}

	ret = sysfs_create_group(&max->dev->kobj, &max_attr_group);
	if (ret) {
		max_err("failed to register sysfs\n");
		return ret;
	}

	ret = max77932_dump_reg(max);
	if (ret) {
		max_err("failed to dump registers\n");
		return ret;
	}

	max_info("MAX77932 probe successfully\n");

	return 0;
}

static int max77932_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max77932_chip *max = i2c_get_clientdata(client);

	return enable_irq_wake(max->irq);
}

static int max77932_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max77932_chip *max = i2c_get_clientdata(client);

	return disable_irq_wake(max->irq);
}

static const struct dev_pm_ops max77932_pm_ops = {
	.suspend = max77932_suspend,
	.resume = max77932_resume,
};

static int max77932_remove(struct i2c_client *client)
{
	struct max77932_chip *max = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&max->irq_handle_work);

	return 0;
}

static void max77932_shutdown(struct i2c_client *client)
{
	struct max77932_chip *max = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&max->irq_handle_work);

	return;
}

static struct of_device_id max77932_match_table[] = {
	{.compatible = "max77932",},
	{},
};
MODULE_DEVICE_TABLE(of, max77932_match_table);

static const struct i2c_device_id max77932_id[] = {
	{ "max77932", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, max77932_id);

static struct i2c_driver max77932_driver = {
	.driver	= {
		.name   = "max77932",
		.owner  = THIS_MODULE,
		.of_match_table = max77932_match_table,
		.pm		= &max77932_pm_ops,
	},
	.id_table       = max77932_id,

	.probe          = max77932_probe,
	.remove		= max77932_remove,
	.shutdown	= max77932_shutdown,

};

module_i2c_driver(max77932_driver);

MODULE_DESCRIPTION("Mamim Max77932 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
