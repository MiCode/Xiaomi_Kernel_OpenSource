/*
 * TI LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_data/lp855x.h>
#include <linux/pwm.h>
#include <linux/mutex.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/timer.h>

#ifndef CONFIG_ACPI
#define CONFIG_ACPI
#endif

#ifdef CONFIG_ACPI
/*
 * ACPI driver will get the i2c client and gpio pin no(if needed) from ASL code set in bios.
 *  the names should be identical in bios set, consult with bios engineer.
 */
#define LP_ACPI_NAME "LP855600"
#define LP_BLEN_NAME "LP8556_EN"
#endif

/* LP8550/1/2/3/6 Registers */
#define LP855X_BRIGHTNESS_CTRL		0x00
#define LP855X_DEVICE_CTRL		0x01
#define LP855X_LED_EN			0x16
#define LP855X_EEPROM_START		0xA0
#define LP855X_EEPROM_END		0xA7
#define LP8556_EPROM_START		0xA0
#define LP8556_EPROM_END		0xAF
#define LP8556_BL_MASK			0x01
#define LP8556_BL_ON			0x01
#define LP8556_BL_OFF			0x00
#define LP8556_FAST_MASK		0x80

/* LP8555/7 Registers */
#define LP8557_BL_CMD			0x00
#define LP8557_BL_MASK			0x01
#define LP8557_BL_ON			0x01
#define LP8557_BL_OFF			0x00
#define LP8557_BRIGHTNESS_CTRL		0x04
#define LP8557_CONFIG			0x10
#define LP8555_EPROM_START		0x10
#define LP8555_EPROM_END		0x7A
#define LP8557_EPROM_START		0x10
#define LP8557_EPROM_END		0x1E

#define DEFAULT_BL_NAME		"lcd-backlight"
#define MAX_BRIGHTNESS		255
#define INIT_BRIGHTNESS		35

enum lp855x_brightness_ctrl_mode {
	PWM_BASED = 1,
	REGISTER_BASED,
};

struct lp855x;
static struct lp855x *glp;

/*
 * struct lp855x_device_config
 * @pre_init_device: init device function call before updating the brightness
 * @reg_brightness: register address for brigthenss control
 * @reg_devicectrl: register address for device control
 * @post_init_device: late init device function call
 */
struct lp855x_device_config {
	int (*pre_init_device)(struct lp855x *);
	u8 reg_brightness;
	u8 reg_devicectrl;
	u8 reg_led;
	int (*post_init_device)(struct lp855x *);
};

struct lp855x {
	const char *chipname;
	enum lp855x_chip_id chip_id;
	enum lp855x_brightness_ctrl_mode mode;
	struct lp855x_device_config *cfg;
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
	struct lp855x_platform_data *pdata;
	struct pwm_device *pwm;
};

static int lp855x_write_byte(struct lp855x *lp, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(lp->client, reg, data);
}

static int lp855x_read_byte(struct lp855x *lp, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(lp->client, reg);

	if (ret < 0) {
		dev_err(lp->dev, "failed to read register 0x%.2x\n", reg);
		return ret;
	}
	dev_info(lp->dev, "lcd backlight read brightness +++ %d +++\n", ret);
	return ret;
}


static int lp855x_update_bit(struct lp855x *lp, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	tmp = (u8)ret;
	tmp &= ~mask;
	tmp |= data & mask;

	return lp855x_write_byte(lp, reg, tmp);
}

static bool lp855x_is_valid_rom_area(struct lp855x *lp, u8 addr)
{
	u8 start, end;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
		start = LP855X_EEPROM_START;
		end = LP855X_EEPROM_END;
		break;
	case LP8556:
		start = LP8556_EPROM_START;
		end = LP8556_EPROM_END;
		break;
	case LP8555:
		start = LP8555_EPROM_START;
		end = LP8555_EPROM_END;
		break;
	case LP8557:
		start = LP8557_EPROM_START;
		end = LP8557_EPROM_END;
		break;
	default:
		return false;
	}

	return (addr >= start && addr <= end);
}

static int lp8557_bl_off(struct lp855x *lp)
{
	/* BL_ON = 0 before updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_OFF);
}

static int lp8557_bl_on(struct lp855x *lp)
{
	/* BL_ON = 1 after updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_ON);
}

static struct lp855x_device_config lp855x_dev_cfg = {
	.reg_brightness = LP855X_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP855X_DEVICE_CTRL,
	.reg_led = LP855X_LED_EN,
};

static struct lp855x_device_config lp8557_dev_cfg = {
	.reg_brightness = LP8557_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP8557_CONFIG,
	.pre_init_device = lp8557_bl_off,
	.post_init_device = lp8557_bl_on,
};

/*
 * Device specific configuration flow
 *
 *    a) pre_init_device(optional)
 *    b) update the brightness register
 *    c) update device control register
 *    d) update ROM area(optional)
 *    e) post_init_device(optional)
 *
 */
static int lp855x_configure(struct lp855x *lp)
{
	u8 val, addr;
	int i, ret = 0;
	struct lp855x_platform_data *pd = lp->pdata;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
	case LP8556:
		lp->cfg = &lp855x_dev_cfg;
		break;
	case LP8555:
	case LP8557:
		lp->cfg = &lp8557_dev_cfg;
		break;
	default:
		return -EINVAL;
	}

	return 0;

err:
	return ret;
}

static void lp855x_pwm_ctrl(struct lp855x *lp, int br, int max_br)
{
	unsigned int period = lp->pdata->period_ns;
	unsigned int duty = br * period / max_br;
	struct pwm_device *pwm;

	/* request pwm device with the consumer name */
	if (!lp->pwm) {
		pwm = devm_pwm_get(lp->dev, lp->chipname);
		if (IS_ERR(pwm))
			return;

		lp->pwm = pwm;
	}

	pwm_config(lp->pwm, duty, period);
	if (duty)
		pwm_enable(lp->pwm);
	else
		pwm_disable(lp->pwm);
}

static int lp855x_bl_update_status(struct backlight_device *bl)
{
	int ret = 0;
	/*static u8 last_val = 0x1f;*/
	struct lp855x *lp = bl_get_data(bl);

	if (bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		bl->props.brightness = 0;

	if (lp->mode == PWM_BASED) {
		int br = bl->props.brightness;
		int max_br = bl->props.max_brightness;
		lp855x_pwm_ctrl(lp, br, max_br);
	} else if (lp->mode == REGISTER_BASED) {
		u8 val = bl->props.brightness;
		ret = lp855x_write_byte(lp, lp->cfg->reg_brightness, val);
		if (ret != 0) {
			dev_err(lp->dev, "failed to write %u to reg_brightness\n", val);
			return ret;
		}
	}
	return ret;
}

static int lp855x_bl_get_brightness(struct backlight_device *bl)
{
	int ret;
	struct lp855x *lp = bl_get_data(bl);

	ret = lp855x_read_byte(lp, lp->cfg->reg_brightness);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read brightness register in lp855x\n");
		return ret;
	}

	return ret;
}

static const struct backlight_ops lp855x_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lp855x_bl_update_status,
	.get_brightness = lp855x_bl_get_brightness,
};

static int lp855x_backlight_register(struct lp855x *lp)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	struct lp855x_platform_data *pdata = lp->pdata;
	const char *name = pdata->name ? : DEFAULT_BL_NAME;

	memset((void *)&props, 0 , sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = MAX_BRIGHTNESS;

	if (pdata->initial_brightness > props.max_brightness)
		pdata->initial_brightness = props.max_brightness;

	props.brightness = pdata->initial_brightness;

	bl = devm_backlight_device_register(lp->dev, name, lp->dev, lp,
				       &lp855x_bl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	lp->bl = bl;

	return 0;
}

static void lp855x_backlight_unregister(struct lp855x *lp)
{
	if (lp->bl)
		backlight_device_unregister(lp->bl);
}

static ssize_t lp855x_get_chip_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n", lp->chipname);
}

static ssize_t lp855x_get_bl_ctl_mode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	char *strmode = NULL;

	if (lp->mode == PWM_BASED)
		strmode = "pwm based";
	else if (lp->mode == REGISTER_BASED)
		strmode = "register based";

	return scnprintf(buf, PAGE_SIZE, "%s\n", strmode);
}

static DEVICE_ATTR(chip_id, S_IRUGO, lp855x_get_chip_id, NULL);
static DEVICE_ATTR(bl_ctl_mode, S_IRUGO, lp855x_get_bl_ctl_mode, NULL);

static struct attribute *lp855x_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_bl_ctl_mode.attr,
	NULL,
};

static const struct attribute_group lp855x_attr_group = {
	.attrs = lp855x_attributes,
};

#ifdef CONFIG_OF
static int lp855x_parse_dt(struct device *dev, struct device_node *node)
{
	struct lp855x_platform_data *pdata;
	int rom_length;

	if (!node) {
		dev_err(dev, "no platform data\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	of_property_read_string(node, "bl-name", &pdata->name);
	of_property_read_u8(node, "dev-ctrl", &pdata->device_control);
	of_property_read_u8(node, "init-brt", &pdata->initial_brightness);
	of_property_read_u32(node, "pwm-period", &pdata->period_ns);

	/* Fill ROM platform data if defined */
	rom_length = of_get_child_count(node);
	if (rom_length > 0) {
		struct lp855x_rom_data *rom;
		struct device_node *child;
		int i = 0;

		rom = devm_kzalloc(dev, sizeof(*rom) * rom_length, GFP_KERNEL);
		if (!rom)
			return -ENOMEM;

		for_each_child_of_node(node, child) {
			of_property_read_u8(child, "rom-addr", &rom[i].addr);
			of_property_read_u8(child, "rom-val", &rom[i].val);
			i++;
		}

		pdata->size_program = rom_length;
		pdata->rom_data = &rom[0];
	}

	dev->platform_data = pdata;

	return 0;
}
#elif CONFIG_ACPI

static struct lp855x_platform_data lp_board_data = {
	.name = DEFAULT_BL_NAME,
	.device_control = 3,
	.initial_brightness = INIT_BRIGHTNESS,
	.led_string = 0x0F,
	.period_ns = 0,
	.size_program = 0,
};

/*
 *  lp855x_parse_acpi :
 *		Desc : to parse info in acpi table to get gpio pin no, if needed.
 *             some other infos are already defined in lp_board_data param.
 */
static int lp855x_parse_acpi(struct device *dev)
{
	struct gpio_desc *gpio;
	struct lp855x_platform_data *pdata = dev->platform_data;

	/*
	   to get backlight enable gpio pin from acpi table,
		now this pin is default pull high in bios,
		so no need here in driver currently.
	*/
	gpio = devm_gpiod_get_index(dev, LP_BLEN_NAME, 0);

	if (IS_ERR(gpio)) {
		dev_err(dev, "acpi gpio get bl gpio failed\n");
		return PTR_ERR(gpio);
	}
	printk("backlight enable gpio pin: %d - \n", desc_to_gpio(gpio));

	gpiod_direction_output(gpio, 1);
	msleep(20);
	pdata->gpio_bl_en = gpio;
	return 0;
}


#else
static int lp855x_parse_dt(struct device *dev, struct device_node *node)
{
	return -EINVAL;
}
#endif

static int lp855x_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lp855x *lp;
	int ret;
#ifdef CONFIG_ACPI
	struct lp855x_platform_data *pdata = &lp_board_data;
	struct acpi_device_id *aid;

	aid = acpi_match_device(cl->dev.driver->acpi_match_table, &cl->dev);
	if (!aid)
		return -ENODEV;

	cl->dev.platform_data = pdata;
	ret = lp855x_parse_acpi(&cl->dev);
	if (ret < 0)
		return ret;
#else
	struct lp855x_platform_data *pdata = dev_get_platdata(&cl->dev);
	struct device_node *node = cl->dev.of_node;
	if (!pdata) {
		ret = lp855x_parse_dt(&cl->dev, node);
		if (ret < 0)
			return ret;

		pdata = dev_get_platdata(&cl->dev);
	}
#endif

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct lp855x), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	if (pdata->period_ns > 0)
		lp->mode = PWM_BASED;
	else
		lp->mode = REGISTER_BASED;

	lp->client = cl;
	lp->dev = &cl->dev;
	lp->pdata = pdata;
	lp->chipname = "lp8556";
	lp->chip_id = LP8556;
	i2c_set_clientdata(cl, lp);

	ret = lp855x_configure(lp);
	if (ret) {
		dev_err(lp->dev, "device config err: %d", ret);
		return ret;
	}

	ret = lp855x_backlight_register(lp);
	if (ret) {
		dev_err(lp->dev,
			"failed to register backlight. err: %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&lp->dev->kobj, &lp855x_attr_group);
	if (ret) {
		dev_err(lp->dev, "failed to register sysfs. err: %d\n", ret);
		return ret;
	}

	backlight_update_status(lp->bl);
	glp = lp;

	return 0;
}

static int lp855x_remove(struct i2c_client *cl)
{
	struct lp855x *lp = i2c_get_clientdata(cl);

	lp->bl->props.brightness = 0;
	backlight_update_status(lp->bl);
	sysfs_remove_group(&lp->dev->kobj, &lp855x_attr_group);
	lp855x_backlight_unregister(lp);

	return 0;
}

static int lp855x_shutdown(struct i2c_client *cl)
{
	struct lp855x *lp = i2c_get_clientdata(cl);
	struct lp855x_platform_data *pdata = lp->pdata;

	lp->bl->props.brightness = 0;
	backlight_update_status(lp->bl);
	if (pdata) {
		gpiod_direction_output(pdata->gpio_bl_en, 0);
		dev_info(lp->dev, " %s - \n", __func__);
	}
	lp = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lp8556_suspend(struct device *dev)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	struct lp855x_platform_data *pdata = lp->pdata;
	if (pdata) {
		gpiod_direction_output(pdata->gpio_bl_en, 0);
		dev_info(lp->dev, " %s - \n", __func__);
	}

	return 0;
}

static int lp8556_resume(struct device *dev)
{
	u8 val;
	int ret;
	struct lp855x *lp = dev_get_drvdata(dev);
	struct lp855x_platform_data *pdata = lp->pdata;
	if (pdata) {
		gpiod_direction_output(pdata->gpio_bl_en, 1);
		dev_dbg(lp->dev, " %s - \n", __func__);
		usleep_range(1000, 3000);

		ret  = lp855x_write_byte(lp, 0x16, 0x0f);
		ret |= lp855x_write_byte(lp, 0x00, 0x00);
		ret |= lp855x_write_byte(lp, 0x98, 0x80);
		ret |= lp855x_write_byte(lp, 0x9e, 0x22);
		ret |= lp855x_write_byte(lp, 0xa1, 0x3f);
		ret |= lp855x_write_byte(lp, 0xa3, 0x38);
		ret |= lp855x_write_byte(lp, 0xa5, 0x24);
		ret |= lp855x_write_byte(lp, 0xa7, 0xf5);
		ret |= lp855x_write_byte(lp, 0xa9, 0xb2);
		ret |= lp855x_write_byte(lp, 0xaa, 0x8f);
		ret |= lp855x_write_byte(lp, 0x01, 0x83);
		if (ret != 0) {
			dev_err(lp->dev, "failed to re-program eeprom in LP855X during resume\n");
			return -EPERM;
		}

		backlight_update_status(lp->bl);
	}
	return 0;
}
static SIMPLE_DEV_PM_OPS(lp8556_pm_ops, lp8556_suspend, lp8556_resume);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id lp855x_acpi_match[] = {
	{ LP_ACPI_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(acpi, lp855x_acpi_match);
#else
static const struct of_device_id lp855x_dt_ids[] = {
	{ .compatible = "ti,lp8550", },
	{ .compatible = "ti,lp8551", },
	{ .compatible = "ti,lp8552", },
	{ .compatible = "ti,lp8553", },
	{ .compatible = "ti,lp8555", },
	{ .compatible = "ti,lp8556", },
	{ .compatible = "ti,lp8557", },
	{ }
};
MODULE_DEVICE_TABLE(of, lp855x_dt_ids);
#endif

static const struct i2c_device_id lp855x_ids[] = {
	{"lp8550", LP8550},
	{"lp8551", LP8551},
	{"lp8552", LP8552},
	{"lp8553", LP8553},
	{"lp8555", LP8555},
	{"lp8556", LP8556},
	{"lp8557", LP8557},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp855x_ids);

static struct i2c_driver lp855x_driver = {
	.driver = {
		   .name = "lp855x",
#ifdef CONFIG_ACPI
		   .acpi_match_table = ACPI_PTR(lp855x_acpi_match),
#else
		   .of_match_table = of_match_ptr(lp855x_dt_ids),
#endif
#ifdef CONFIG_PM_SLEEP
		   .pm	= &lp8556_pm_ops,
#endif

		   },
	.probe = lp855x_probe,
	.remove = lp855x_remove,
	.shutdown = lp855x_shutdown,
	.id_table = lp855x_ids,
};

static int __init lp855x_init(void)
{
	int err;
	printk("Enter:%s\n", __func__);
	err = i2c_add_driver(&lp855x_driver);
	if (err) {
		printk(KERN_ERR "lp855x driver failed "
				"(errno = %d)\n", err);
	} else {
		printk("Successfully added driver %s\n",
				lp855x_driver.driver.name);
	}
	return err;
}

static void __exit lp855x_exit(void)
{
	printk("%s\n", __func__);
	i2c_del_driver(&lp855x_driver);
}

late_initcall(lp855x_init);
module_exit(lp855x_exit);

MODULE_DESCRIPTION("Texas Instruments LP855x Backlight driver");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_LICENSE("GPL");
