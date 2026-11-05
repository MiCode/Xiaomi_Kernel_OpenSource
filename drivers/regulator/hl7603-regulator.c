/*
 * HL7603 ON Semiconductor LDO PMIC Driver.
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
#include <linux/pinctrl/consumer.h>
#include <linux/version.h>


#define hl7603_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define hl7603_debug(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)

#define HL7603_SLAVE_ADDR      0xEE

#define HL7603_REG_ENABLE      0x01
#define LDO_VSET_REG           0x02
#define ILIMSET1               0x03
#define STATUS                 0x05

#define HL7603_DEFAULT_ILIMSET 0x09

#define VSET_DVDD_BASE_UV   2850000
#define VSET_DVDD_STEP_UV   50000

#define MAX_REG_NAME     64
#define HL7603_MAX_LDO  1

#define HL7603_CLASS_NAME       "satellite_tt_ldo"
#define HL7603_NAME_FMT         "hl7603%u"
#define HL7603_NAME_STR_LEN_MAX 64
#define HL7603_MAX_NUMBER       5

#define HL7603_PINCTRL_ENABLE       "hl7603_enable"
#define HL7603_PINCTRL_DISABLE      "hl7603_disable"
struct hl7603_char_dev {
    dev_t dev_no;
    struct cdev *pcdev;
    struct device *pdevice;
};

static struct class *phl7603_class = NULL;
static struct hl7603_char_dev hl7603_dev_list[HL7603_MAX_NUMBER];

struct hl7603_regulator{
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
      { "hl7603-msc06a-l1", "none", 4500000, 50000, 300000},
//    { "hl7603-msc06a-l2", "none", 1200000, 80000, 800000},
//    { "hl7603-msc06a-l3", "none", 3400000, 85000, 300000},
//    { "hl7603-msc06a-l4", "none", 1800000, 85000, 300000},
//    { "hl7603-msc06a-l5", "none", 1800000, 85000, 300000},
//    { "hl7603-msc06a-l6", "none", 1800000, 85000, 300000},
//    { "hl7603-msc06a-l7", "none", 2800000, 85000, 300000},
};

static const struct regmap_config hl7603_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
};

/*common functions*/
static int hl7603_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
    int rc;

    rc = regmap_bulk_read(regmap, reg, val, count);
    if (rc < 0)
        hl7603_err("failed to read 0x%04x\n", reg);
    return rc;
}

static int hl7603_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
    int rc;

    hl7603_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
    rc = regmap_bulk_write(regmap, reg, val, count);
    if (rc < 0)
        hl7603_err("failed to write 0x%04x\n", reg);

    return rc;
}

static int hl7603_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
    int rc;
    hl7603_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
    rc = regmap_update_bits(regmap, reg, mask, val);
    if (rc < 0)
        hl7603_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
    return rc;
}

static int hl7603_regulator_is_enabled(struct regulator_dev *rdev)
{
    struct hl7603_regulator *fan_reg = rdev_get_drvdata(rdev);
    int rc;
    u8 reg;

    rc = hl7603_read(fan_reg->regmap,
        HL7603_REG_ENABLE, &reg, 1);
    if (rc < 0) {
        hl7603_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
        return rc;
    }
    return !!(reg & (1u << fan_reg->offset));
}

static int hl7603_regulator_enable(struct regulator_dev *rdev)
{
    struct hl7603_regulator *fan_reg = rdev_get_drvdata(rdev);
    int rc;
    u8 ilimset = HL7603_DEFAULT_ILIMSET;

    if (fan_reg->parent_supply) {
        rc = regulator_enable(fan_reg->parent_supply);
        if (rc < 0) {
            hl7603_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
            return rc;
        }
    }

    rc = hl7603_write(fan_reg->regmap, ILIMSET1, &ilimset, 1);
    if (rc < 0) {
        hl7603_err("[%s] failed to write ilimset rc = %d\n", fan_reg->rdesc.name,rc);
        goto remove_vote;
    }

    rc = hl7603_masked_write(fan_reg->regmap,
        HL7603_REG_ENABLE,
        1u << fan_reg->offset, 1u << fan_reg->offset);
    if (rc < 0) {
        hl7603_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
        goto remove_vote;
    }
    hl7603_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
    return 0;

remove_vote:
    if (fan_reg->parent_supply)
        rc = regulator_disable(fan_reg->parent_supply);
    if (rc < 0)
        hl7603_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
    return -ETIME;
}

static int hl7603_regulator_disable(struct regulator_dev *rdev)
{
    struct hl7603_regulator *fan_reg = rdev_get_drvdata(rdev);
    int rc;

    rc = hl7603_masked_write(fan_reg->regmap,
        HL7603_REG_ENABLE,
        1u << fan_reg->offset, 0);

    if (rc < 0) {
        hl7603_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
        return rc;
    }

    /*remove voltage vot from parent regulator */
    if (fan_reg->parent_supply) {
        rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
        if (rc < 0) {
            hl7603_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
            return rc;
        }
        rc = regulator_disable(fan_reg->parent_supply);
        if (rc < 0) {
            hl7603_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
            return rc;
        }
    }

    hl7603_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
    return 0;
}

static int hl7603_regulator_get_voltage(struct regulator_dev *rdev)
{
    struct hl7603_regulator *fan_reg = rdev_get_drvdata(rdev);
    u8  vset = 0;
    int rc   = 0;
    int uv   = 0;

    rc = hl7603_read(fan_reg->regmap, LDO_VSET_REG,
    &vset, 1);
    if (rc < 0) {
        hl7603_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
        return rc;
    }

    if (vset == 0) {
        uv = reg_data[0].default_uv;
    } else {
        hl7603_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
            uv = VSET_DVDD_BASE_UV + vset * VSET_DVDD_STEP_UV; //DVDD
    }
    return uv;
}

static int hl7603_write_voltage(struct hl7603_regulator* fan_reg, int min_uv,
    int max_uv)
{
    int rc   = 0;
    u8  vset = 0;

    if (min_uv > max_uv) {
        hl7603_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
        return -EINVAL;
    }

        vset = DIV_ROUND_UP(min_uv - VSET_DVDD_BASE_UV, VSET_DVDD_STEP_UV); //DVDD

    rc = hl7603_write(fan_reg->regmap, LDO_VSET_REG,
        &vset, 1);
    if (rc < 0) {
        hl7603_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
        return rc;
    }

    hl7603_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
    return 0;
}

static int hl7603_regulator_set_voltage(struct regulator_dev *rdev,
    int min_uv, int max_uv, unsigned int* selector)
{
    struct hl7603_regulator *fan_reg = rdev_get_drvdata(rdev);
    int rc;

    if (fan_reg->parent_supply) {
        rc = regulator_set_voltage(fan_reg->parent_supply,
            fan_reg->min_dropout_uv + min_uv,
            INT_MAX);
        if (rc < 0) {
            hl7603_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
            return rc;
        }
    }

    rc = hl7603_write_voltage(fan_reg, min_uv, max_uv);
    if (rc < 0) {
        /* remove parentn's voltage vote */
        if (fan_reg->parent_supply)
            regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
    }
    hl7603_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
    return rc;
}

static struct regulator_ops hl7603_regulator_ops = {
    .enable      = hl7603_regulator_enable,
    .disable     = hl7603_regulator_disable,
    .is_enabled  = hl7603_regulator_is_enabled,
    .set_voltage = hl7603_regulator_set_voltage,
    .get_voltage = hl7603_regulator_get_voltage,
};

static int hl7603_register_ldo(struct hl7603_regulator *hl7603_reg,
    const char *name)
{
    struct regulator_config reg_config = {};
    struct regulator_init_data *init_data;

    struct device_node *reg_node = hl7603_reg->of_node;
    struct device *dev           = hl7603_reg->dev;
    int rc, i, init_voltage;
    char buff[MAX_REG_NAME];

    /* try to find ldo pre-defined in the regulator table */
    for (i = 0; i< HL7603_MAX_LDO; i++) {
        if (!strcmp(reg_data[i].name, name))
            break;
    }

    if (i == HL7603_MAX_LDO) {
        hl7603_err("Invalid regulator name %s\n", name);
        return -EINVAL;
    }

    rc = of_property_read_u16(reg_node, "offset", &hl7603_reg->offset);
    if (rc < 0) {
        hl7603_err("%s:failed to get regulator offset rc = %d\n", name, rc);
        return rc;
    }

    //assign default value defined in code.
    hl7603_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
    of_property_read_u32(reg_node, "min-dropout-voltage",
        &hl7603_reg->min_dropout_uv);

    hl7603_reg->iout_ua = reg_data[i].iout_ua;
    of_property_read_u32(reg_node, "iout_ua",
        &hl7603_reg->iout_ua);

    init_voltage = -EINVAL;
    of_property_read_u32(reg_node, "init-voltage", &init_voltage);

    scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
    if (of_find_property(dev->of_node, buff, NULL)) {
        hl7603_reg->parent_supply = devm_regulator_get(dev,
            reg_data[i].supply_name);
        if (IS_ERR(hl7603_reg->parent_supply)) {
            rc = PTR_ERR(hl7603_reg->parent_supply);
            if (rc != EPROBE_DEFER)
                hl7603_err("%s: failed to get parent regulator rc = %d\n",
                    name, rc);
                return rc;
        }
    }

    init_data = of_get_regulator_init_data(dev, reg_node, &hl7603_reg->rdesc);
    if (init_data == NULL) {
        hl7603_err("%s: failed to get regulator data\n", name);
        return -ENODATA;
    }


    if (!init_data->constraints.name) {
        hl7603_err("%s: regulator name missing\n", name);
        return -EINVAL;
    }

    /* configure the initial voltage for the regulator */
    if (init_voltage > 0) {
        rc = hl7603_write_voltage(hl7603_reg, init_voltage,
            init_data->constraints.max_uV);
        if (rc < 0)
            hl7603_err("%s:failed to set initial voltage rc = %d\n", name, rc);
    }

    init_data->constraints.input_uV = init_data->constraints.max_uV;
    init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
        | REGULATOR_CHANGE_VOLTAGE;

    reg_config.dev         = dev;
    reg_config.init_data   = init_data;
    reg_config.driver_data = hl7603_reg;
    reg_config.of_node     = reg_node;

    hl7603_reg->rdesc.owner      = THIS_MODULE;
    hl7603_reg->rdesc.type       = REGULATOR_VOLTAGE;
    hl7603_reg->rdesc.ops        = &hl7603_regulator_ops;
    hl7603_reg->rdesc.name       = init_data->constraints.name;
    hl7603_reg->rdesc.n_voltages = 1;

    hl7603_debug("try to register ldo %s\n", name);
    hl7603_reg->rdev = devm_regulator_register(dev, &hl7603_reg->rdesc,
        &reg_config);
    if (IS_ERR(hl7603_reg->rdev)) {
        rc = PTR_ERR(hl7603_reg->rdev);
        hl7603_err("%s: failed to register regulator rc =%d\n",
        hl7603_reg->rdesc.name, rc);
        return rc;
    }

    rc = hl7603_regulator_disable(hl7603_reg->rdev);
    if(rc < 0) {
        hl7603_err("%s: regulator disable failed rc = %d\n", name, rc);
    }
    hl7603_debug("%s regulator register done\n", name);
    return 0;
}

static int hl7603_parse_regulator(struct regmap *regmap, struct device *dev)
{
    int rc           = 0;
    int index        = 0;
    const char *name = NULL;
    struct device_node *child             = NULL;
    struct hl7603_regulator *hl7603_reg = NULL;

    of_property_read_u32(dev->of_node, "index", &index);

    /* parse each regulator */
    for_each_available_child_of_node(dev->of_node, child) {
        hl7603_reg = devm_kzalloc(dev, sizeof(*hl7603_reg), GFP_KERNEL);
        if (!hl7603_reg)
            return -ENOMEM;

        hl7603_reg->regmap  = regmap;
        hl7603_reg->of_node = child;
        hl7603_reg->dev     = dev;
        hl7603_reg->index   = index;
        hl7603_reg->parent_supply = NULL;

        rc = of_property_read_string(child, "regulator-name", &name);
        if (rc)
            continue;

        rc = hl7603_register_ldo(hl7603_reg, name);
        if (rc <0 ) {
            hl7603_err("failed to register regulator %s rc = %d\n", name, rc);
            return rc;
        }
    }
    return 0;
}

static int hl7603_open(struct inode *a_inode, struct file *a_file)
{
    return 0;
}

static int hl7603_release(struct inode *a_inode, struct file *a_file)
{
    return 0;
}

static long hl7603_ioctl(struct file *a_file, unsigned int a_cmd,
             unsigned long a_param)
{
    return 0;
}

static const struct file_operations hl7603_file_operations = {
    .owner          = THIS_MODULE,
    .open           = hl7603_open,
    .release        = hl7603_release,
    .unlocked_ioctl = hl7603_ioctl,
};

static ssize_t hl7603_show_status(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct regmap *regmap = (struct regmap *)dev->driver_data;
    int rc;
    unsigned int val      = 0xFF;
    unsigned int len      = 0;

    rc = regmap_read(regmap, 0x00, &val);
    if (rc < 0) {
        hl7603_err("HL7603 failed to get PID\n");
        len = sprintf(buf, "fail\n");
    }
    else {
        hl7603_debug("HL7603 get Product ID: [%02x]\n", val);
        len = sprintf(buf, "success\n");
    }

    return len;
}

static ssize_t hl7603_show_info(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct regmap *regmap = (struct regmap *)dev->driver_data;
    int rc;
    unsigned int val      = 0xFF;
    unsigned int len      = 0;
    int i = 0;

    for (i = 0; i <= 0x05; i++) {
        rc = regmap_read(regmap, i, &val);
        if (rc < 0) {
            len += sprintf(buf+len, "read 0x%02x ==> fail\n", i);
        }
        else {
            len += sprintf(buf+len, "read 0x%02x ==> 0x%02x\n",i, val);
        }
    }

    return len;
}


static ssize_t hl7603_show_enable(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct regmap *regmap = (struct regmap *)dev->driver_data;
    int rc;
    unsigned int val      = 0xFF;
    unsigned int len      = 0;

    rc = regmap_read(regmap, HL7603_REG_ENABLE, &val);
    if (rc < 0) {
        len = sprintf(buf, "read 0x%02x ==> fail\n", HL7603_REG_ENABLE);
    }
    else {
        len = sprintf(buf, "read 0x%02x ==> 0x%x\n", HL7603_REG_ENABLE, val);
    }

    return len;
}

static ssize_t hl7603_set_enable(struct device *dev,
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
    hl7603_write(regmap, HL7603_REG_ENABLE, &val, 1);

    return len;
}


static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, hl7603_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, hl7603_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, hl7603_show_enable, hl7603_set_enable);




static int hl7603_driver_register(int index, struct regmap *regmap)
{
    unsigned long ret;
    char device_drv_name[HL7603_NAME_STR_LEN_MAX] = { 0 };
    struct hl7603_char_dev hl7603_dev = hl7603_dev_list[index];

    snprintf(device_drv_name, HL7603_NAME_STR_LEN_MAX - 1,
        HL7603_NAME_FMT, index);

    /* Register char driver */
    if (alloc_chrdev_region(&(hl7603_dev.dev_no), 0, 1,
            device_drv_name)) {
        hl7603_debug("[HL7603] Allocate device no failed\n");
        return -EAGAIN;
    }

    /* Allocate driver */
    hl7603_dev.pcdev = cdev_alloc();
    if (hl7603_dev.pcdev == NULL) {
        unregister_chrdev_region(hl7603_dev.dev_no, 1);
        hl7603_debug("[HL7603] Allocate mem for kobject failed\n");
        return -ENOMEM;
    }

    /* Attatch file operation. */
    cdev_init(hl7603_dev.pcdev, &hl7603_file_operations);
    hl7603_dev.pcdev->owner = THIS_MODULE;

    /* Add to system */
    if (cdev_add(hl7603_dev.pcdev, hl7603_dev.dev_no, 1)) {
        hl7603_debug("Attatch file operation failed\n");
        unregister_chrdev_region(hl7603_dev.dev_no, 1);
        return -EAGAIN;
    }

    if (phl7603_class == NULL) {
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
        phl7603_class = class_create(HL7603_CLASS_NAME);
#else
        phl7603_class = class_create(THIS_MODULE, HL7603_CLASS_NAME);
#endif
        if (IS_ERR(phl7603_class)) {
            int ret = PTR_ERR(phl7603_class);
            hl7603_debug("Unable to create class, err = %d\n", ret);
            return ret;
        }
    }

    hl7603_dev.pdevice = device_create(phl7603_class, NULL,
            hl7603_dev.dev_no, NULL, device_drv_name);
    if (IS_ERR(hl7603_dev.pdevice)) {
        int ret = PTR_ERR(hl7603_dev.pdevice);
        hl7603_debug("[HL7603] device_create failed, err = %d\n", ret);
        return ret;
    }

    hl7603_dev.pdevice->driver_data = regmap;
    ret = sysfs_create_file(&(hl7603_dev.pdevice->kobj), &dev_attr_status.attr);
    ret = sysfs_create_file(&(hl7603_dev.pdevice->kobj), &dev_attr_info.attr);
    ret = sysfs_create_file(&(hl7603_dev.pdevice->kobj), &dev_attr_enable.attr);

    return 0;
}
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
static int hl7603_regulator_probe(struct i2c_client *client)
#else
static int hl7603_regulator_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
#endif
{
    int rc                = 0;
    unsigned int val      = 0xFF;
    struct regmap *regmap = NULL;
    int index = 0;
    int retryCount = 0;

    struct pinctrl *pinctrl = NULL;
    //struct pinctrl_state *gpio_state_enable = NULL;
    //struct pinctrl_state *gpio_state_disable = NULL;
    hl7603_err("hl7603 probe");

    pinctrl = devm_pinctrl_get(&client->dev);
    if (!pinctrl) {
        hl7603_err("failed to get pinctrl");
        return -ENODEV;
    }
    msleep(25);

    client->addr =  (HL7603_SLAVE_ADDR >> 1);

    rc = of_property_read_u32(client->dev.of_node, "index", &index);
    if (rc) {
        hl7603_err("failed to read index");
        return rc;
    }

    regmap = devm_regmap_init_i2c(client, &hl7603_regmap_config);
    if (IS_ERR(regmap)) {
        hl7603_err("HL7603 failed to allocate regmap\n");
        return PTR_ERR(regmap);
    }

    hl7603_driver_register(index, regmap);

    rc = regmap_read(regmap, 0x00, &val);
    while (rc < 0) {
        if (retryCount == 3)
        {
            return -ENODEV;
        }
        hl7603_err("HL7603 failed to get PID at %d time\n", retryCount++);
        rc = regmap_read(regmap, 0x00, &val);
        msleep(1);
    }

    hl7603_debug("HL7603 get Product ID: [%02x]\n", val);


    rc = hl7603_parse_regulator(regmap, &client->dev);
    if (rc < 0) {
        hl7603_err("HL7603 failed to parse device tree rc=%d\n", rc);
        return -ENODEV;
    }

    return 0;
}

static const struct of_device_id hl7603_dt_ids[] = {
    {
        .compatible = "halo,hl7603-msc06a",
    },
    { }
};
MODULE_DEVICE_TABLE(of, hl7603_dt_ids);

static const struct i2c_device_id hl7603_id[] = {
    {
        .name = "hl7603-i2c",
        .driver_data = 0,
    },
    { },
};
MODULE_DEVICE_TABLE(i2c, hl7603_id);

static struct i2c_driver hl7603_regulator_driver = {
    .driver = {
        .name = "hl7603-regulator",
        .of_match_table = of_match_ptr(hl7603_dt_ids),
    },
    .probe = hl7603_regulator_probe,
    .id_table = hl7603_id,
};

module_i2c_driver(hl7603_regulator_driver);
MODULE_LICENSE("GPL v2");
