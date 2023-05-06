/*
 * WL2866D ON Semiconductor LDO PMIC Driver.
 *
 * Copyright (c) 2020 On XiaoMi.
 * liuqinhong@xiaomi.com
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/cdev.h>
#include <linux/fs.h>

#include <linux/module.h>
#include <linux/param.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>


#define wl2866d_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define wl2866d_debug(message, ...) \
        pr_debug("[%s] : "message, __func__, ##__VA_ARGS__)

#define WL2866D_REG_ENABLE      0x0E
#define WL2866D_REG_DISCHARGE   0x02
#define WL2866D_REG_LDO0        0x03

#define LDO_VSET_REG(offset) ((offset) + WL2866D_REG_LDO0)

#define VSET_DVDD_BASE_UV   600000
#define VSET_DVDD_STEP_UV   6000

#define VSET_AVDD_BASE_UV   1200000
#define VSET_AVDD_STEP_UV   12500

#define MAX_REG_NAME     20
#define WL2866D_MAX_LDO  4

#define WL2866D_CLASS_NAME       "wl2866d_ldo"
#define WL2866D_NAME_FMT         "wl2866d%u"
#define WL2866D_NAME_STR_LEN_MAX 50
#define WL2866D_MAX_NUMBER       5



struct wl2866d_char_dev {
	dev_t dev_no;
	struct cdev *pcdev;
	struct device *pdevice;
};

static struct class *pwl2866d_class = NULL;
static struct wl2866d_char_dev wl2866d_dev_list[WL2866D_MAX_NUMBER];

struct wl2866d_regulator{
	struct device    *dev;
	struct regmap    *regmap;
	struct regulator_desc rdesc;
	struct regulator_dev  *rdev;
	struct regulator      *parent_supply;
	struct regulator      *en_supply;
	struct device_node    *of_node;
	u16         offset;
	int         min_dropout_uv;
	int         iout_ua;
	int         index;
};

struct regulator_data {
	char *name;
	char *supply_name;
	int   default_uv;
	int   min_dropout_uv;
	int   iout_ua;
};

static struct regulator_data reg_data[] = {
	{ "wl2866d-dvdd1", "none", 1200000, 80000, 500000},
	{ "wl2866d-dvdd2", "none", 1200000, 80000, 500000},
	{ "wl2866d-avdd1", "none", 2800000, 90000, 300000},
	{ "wl2866d-avdd2", "none", 2800000, 90000, 300000},
};

static const struct regmap_config wl2866d_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*common functions*/
static int wl2866d_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		wl2866d_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int wl2866d_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	wl2866d_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		wl2866d_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int wl2866d_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	wl2866d_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		wl2866d_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	return rc;
}

static int wl2866d_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = wl2866d_read(fan_reg->regmap,
		WL2866D_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		wl2866d_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
		return rc;
	}
	return !!(reg & (1u << fan_reg->offset));
}

static int wl2866d_regulator_enable(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			wl2866d_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	rc = wl2866d_masked_write(fan_reg->regmap,
		WL2866D_REG_ENABLE,
		1u << fan_reg->offset, 1u << fan_reg->offset);
	if (rc < 0) {
		wl2866d_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
		goto remove_vote;
	}
	wl2866d_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;

remove_vote:
	if (fan_reg->parent_supply)
		rc = regulator_disable(fan_reg->parent_supply);
	if (rc < 0)
		wl2866d_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
	return -ETIME;
}

static int wl2866d_regulator_disable(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = wl2866d_masked_write(fan_reg->regmap,
		WL2866D_REG_ENABLE,
		1u << fan_reg->offset, 0);

	if (rc < 0) {
		wl2866d_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
		if (rc < 0) {
			wl2866d_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
		rc = regulator_disable(fan_reg->parent_supply);
		if (rc < 0) {
			wl2866d_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	wl2866d_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;
}

static int wl2866d_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8  vset = 0;
	int rc   = 0;
	int uv   = 0;

	rc = wl2866d_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		wl2866d_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_uv;
	} else {
		wl2866d_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
		if (fan_reg->offset == 0 || fan_reg->offset == 1)
			uv = VSET_DVDD_BASE_UV + vset * VSET_DVDD_STEP_UV; //DVDD
		else
			uv = VSET_AVDD_BASE_UV + vset * VSET_AVDD_STEP_UV; //AVDD
	}
	return uv;
}

static int wl2866d_write_voltage(struct wl2866d_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc   = 0;
	u8  vset = 0;

	if (min_uv > max_uv) {
		wl2866d_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
		return -EINVAL;
	}

	if (fan_reg->offset == 0 || fan_reg->offset == 1)
		vset = DIV_ROUND_UP(min_uv - VSET_DVDD_BASE_UV, VSET_DVDD_STEP_UV); //DVDD
	else
		vset = DIV_ROUND_UP(min_uv - VSET_AVDD_BASE_UV, VSET_AVDD_STEP_UV); //AVDD

	rc = wl2866d_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		wl2866d_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	wl2866d_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
	return 0;
}

static int wl2866d_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply,
			fan_reg->min_dropout_uv + min_uv,
			INT_MAX);
		if (rc < 0) {
			wl2866d_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
	}

	rc = wl2866d_write_voltage(fan_reg, min_uv, max_uv);
	if (rc < 0) {
		/* remove parentn's voltage vote */
		if (fan_reg->parent_supply)
			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	}
	wl2866d_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
	return rc;
}

static struct regulator_ops wl2866d_regulator_ops = {
	.enable      = wl2866d_regulator_enable,
	.disable     = wl2866d_regulator_disable,
	.is_enabled  = wl2866d_regulator_is_enabled,
	.set_voltage = wl2866d_regulator_set_voltage,
	.get_voltage = wl2866d_regulator_get_voltage,
};

static int wl2866d_register_ldo(struct wl2866d_regulator *wl2866d_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = wl2866d_reg->of_node;
	struct device *dev           = wl2866d_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i< WL2866D_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if (i == WL2866D_MAX_LDO) {
		wl2866d_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &wl2866d_reg->offset);
	if (rc < 0) {
		wl2866d_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	wl2866d_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&wl2866d_reg->min_dropout_uv);

	wl2866d_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&wl2866d_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		wl2866d_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(wl2866d_reg->parent_supply)) {
			rc = PTR_ERR(wl2866d_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				wl2866d_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &wl2866d_reg->rdesc);
	if (init_data == NULL) {
		wl2866d_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		wl2866d_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = wl2866d_write_voltage(wl2866d_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			wl2866d_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev         = dev;
	reg_config.init_data   = init_data;
	reg_config.driver_data = wl2866d_reg;
	reg_config.of_node     = reg_node;

	wl2866d_reg->rdesc.owner      = THIS_MODULE;
	wl2866d_reg->rdesc.type       = REGULATOR_VOLTAGE;
	wl2866d_reg->rdesc.ops        = &wl2866d_regulator_ops;
	wl2866d_reg->rdesc.name       = init_data->constraints.name;
	wl2866d_reg->rdesc.n_voltages = 1;

	wl2866d_debug("try to register ldo %s\n", name);
	wl2866d_reg->rdev = devm_regulator_register(dev, &wl2866d_reg->rdesc,
		&reg_config);
	if (IS_ERR(wl2866d_reg->rdev)) {
		rc = PTR_ERR(wl2866d_reg->rdev);
		wl2866d_err("%s: failed to register regulator rc =%d\n",
		wl2866d_reg->rdesc.name, rc);
		return rc;
	}

	wl2866d_debug("%s regulator register done\n", name);
	return 0;
}

static int wl2866d_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc           = 0;
	int index        = 0;
	const char *name = NULL;
	struct device_node *child             = NULL;
	struct wl2866d_regulator *wl2866d_reg = NULL;

	of_property_read_u32(dev->of_node, "index", &index);

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		wl2866d_reg = devm_kzalloc(dev, sizeof(*wl2866d_reg), GFP_KERNEL);
		if (!wl2866d_reg)
			return -ENOMEM;

		wl2866d_reg->regmap  = regmap;
		wl2866d_reg->of_node = child;
		wl2866d_reg->dev     = dev;
		wl2866d_reg->index   = index;
		wl2866d_reg->parent_supply = NULL;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = wl2866d_register_ldo(wl2866d_reg, name);
		if (rc <0 ) {
			wl2866d_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int wl2866d_open(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static int wl2866d_release(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static long wl2866d_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	return 0;
}

static const struct file_operations wl2866d_file_operations = {
	.owner          = THIS_MODULE,
	.open           = wl2866d_open,
	.release        = wl2866d_release,
	.unlocked_ioctl = wl2866d_ioctl,
};

static ssize_t wl2866d_show_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		wl2866d_err("WL2866D failed to get PID\n");
		len = sprintf(buf, "fail\n");
	}
	else {
		wl2866d_debug("WL2866D get Product ID: [%02x]\n", val);
		len = sprintf(buf, "success\n");
	}

	return len;
}

static ssize_t wl2866d_show_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;
	int i = 0;

	for (i = 0; i <= 0xF; i++) {
		rc = regmap_read(regmap, i, &val);
		if (rc < 0) {
			len += sprintf(buf+len, "read 0x%x ==> fail\n", i);
		}
		else {
			len += sprintf(buf+len, "read 0x%x ==> 0x%x\n",i, val);
		}
	}

	return len;
}


static ssize_t wl2866d_show_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, WL2866D_REG_ENABLE, &val);
	if (rc < 0) {
		len = sprintf(buf, "read 0x0E ==> fail\n");
	}
	else {
		len = sprintf(buf, "read 0x0E ==> 0x%x\n", val);
	}

	return len;
}

static ssize_t wl2866d_set_enable(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t len)
{
	u8 val = 0;
	struct regmap *regmap = (struct regmap *)dev->driver_data;

	if (buf[0] == '0' && buf[1] == 'x') {
		val = (u8)simple_strtoul(buf, NULL, 16);
	} else {
		val = (u8)simple_strtoul(buf, NULL, 10);
	}
	wl2866d_write(regmap, WL2866D_REG_ENABLE, &val, 1);

	return len;
}


static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, wl2866d_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, wl2866d_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, wl2866d_show_enable, wl2866d_set_enable);




static int wl2866d_driver_register(int index, struct regmap *regmap)
{
	char device_drv_name[WL2866D_NAME_STR_LEN_MAX] = { 0 };
	struct wl2866d_char_dev wl2866d_dev = wl2866d_dev_list[index];

	snprintf(device_drv_name, WL2866D_NAME_STR_LEN_MAX - 1,
		WL2866D_NAME_FMT, index);

	/* Register char driver */
	if (alloc_chrdev_region(&(wl2866d_dev.dev_no), 0, 1,
			device_drv_name)) {
		wl2866d_debug("[WL2866D] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	wl2866d_dev.pcdev = cdev_alloc();
	if (wl2866d_dev.pcdev == NULL) {
		unregister_chrdev_region(wl2866d_dev.dev_no, 1);
		wl2866d_debug("[WL2866D] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(wl2866d_dev.pcdev, &wl2866d_file_operations);
	wl2866d_dev.pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(wl2866d_dev.pcdev, wl2866d_dev.dev_no, 1)) {
		wl2866d_debug("Attatch file operation failed\n");
		unregister_chrdev_region(wl2866d_dev.dev_no, 1);
		return -EAGAIN;
	}

	if (pwl2866d_class == NULL) {
		pwl2866d_class = class_create(THIS_MODULE, WL2866D_CLASS_NAME);
		if (IS_ERR(pwl2866d_class)) {
			int ret = PTR_ERR(pwl2866d_class);
			wl2866d_debug("Unable to create class, err = %d\n", ret);
			return ret;
		}
	}

	wl2866d_dev.pdevice = device_create(pwl2866d_class, NULL,
			wl2866d_dev.dev_no, NULL, device_drv_name);
	if (wl2866d_dev.pdevice == NULL) {
		wl2866d_debug("[WL2866D] Allocate device_create for kobject failed\n");
		return -ENOMEM;
	}

	wl2866d_dev.pdevice->driver_data = regmap;
	sysfs_create_file(&(wl2866d_dev.pdevice->kobj), &dev_attr_status.attr);
	sysfs_create_file(&(wl2866d_dev.pdevice->kobj), &dev_attr_info.attr);
	sysfs_create_file(&(wl2866d_dev.pdevice->kobj), &dev_attr_enable.attr);

	return 0;
}
static int wl2866d_regulator_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc                = 0;
	unsigned int val      = 0xFF;
	struct regmap *regmap = NULL;
	int index = 0;

	rc = of_property_read_u32(client->dev.of_node, "index", &index);
	if (rc) {
		wl2866d_err("failed to read index");
		return rc;
	}

	regmap = devm_regmap_init_i2c(client, &wl2866d_regmap_config);
	if (IS_ERR(regmap)) {
		wl2866d_err("WL2866D failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	wl2866d_driver_register(index, regmap);

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		wl2866d_err("WL2866D failed to get PID\n");
		return 0;
	}
	else
		wl2866d_debug("WL2866D get Product ID: [%02x]\n", val);

	val = 0xFF;
	wl2866d_write(regmap, WL2866D_REG_DISCHARGE, (u8*)&val, 1);

	rc = wl2866d_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		wl2866d_err("WL2866D failed to parse device tree rc=%d\n", rc);
		return 0;
	}

	return 0;
}

static const struct of_device_id wl2866d_dt_ids[] = {
	{
		.compatible = "willsemi,wl2866d",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, wl2866d_dt_ids);

static const struct i2c_device_id wl2866d_id[] = {
	{
		.name = "wl2866d-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, wl2866d_id);

static struct i2c_driver wl2866d_regulator_driver = {
	.driver = {
		.name = "wl2866d-regulator",
		.of_match_table = of_match_ptr(wl2866d_dt_ids),
	},
	.probe = wl2866d_regulator_probe,
	.id_table = wl2866d_id,
};

module_i2c_driver(wl2866d_regulator_driver);
MODULE_DESCRIPTION("willsimi rgulatorDriver");
MODULE_AUTHOR("willsimi");
MODULE_LICENSE("GPL");

