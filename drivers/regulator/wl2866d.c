#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/unaligned.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include <linux/regmap.h>

#define wl2866d_err(reg, message, ...) \
	pr_err("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)
#define wl2866d_debug(reg, message, ...) \
	pr_debug("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)

#define WL2866D_ID  0x64
#define WL2866D_ID1  0x0
#define AW_I2C_RETRIES 2
#define AW_I2C_RETRY_DELAY 2

#define MAX_REG_NAME 20
#define WL2866D_MAX_LDO 4 

#define VSET_BASE_12 6000 
#define VSET_STEP_MV 60
#define VSET_BASE_34 12000 
#define VSET_STEP_MV_34 125
#define WL2866D_REG_LDO 0x03
#define LDO_VSET_REG(offset) ((offset) + WL2866D_REG_LDO)
//#define WL2866D_DEBUG

struct wl2866d_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap    *regmap;
	int    en_gpio;
};

struct wl2866d_chip *camera_chip;

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
};

struct wl2866d_map {
	u8 reg;
	u8 value;
};

enum {
	OUT_DVDD1,
	OUT_DVDD2,
	OUT_AVDD1,
	OUT_AVDD2,
	VOL_DISABLE,
	VOL_ENABLE,
	DISCHARGE_ENABLE,
	DISCHARGE_DISABLE,
};

struct regulator_data {
	char *name;
	char *supply_name;
	int default_mv;
	int  min_dropout_uv;
	int iout_ua;
};

static const struct wl2866d_map wl2866d_on_config[] = {
	{0x03, 0x64},
	{0x04, 0x4B},
	{0x05, 0x80},
	{0x06, 0x80},
	{0x0E, 0x00},
	{0x0E, 0x0F},
	{0x02, 0x8F},
	{0x02, 0x00},
};

static struct regulator_data reg_data[] = {
	/* name,  parent,   headroom */
	{ "wl2866d_dvdd1", "vdd11_12", 1200000, 225000, 650000},
	{ "wl2866d_dvdd2", "vdd11_12", 1050000, 225000, 650000},
	{ "wl2866d_avdd1", "vdd13_14", 2800000, 200000, 650000},
	{ "wl2866d_avdd2", "vdd13_14", 2800000, 200000, 650000},
};

static const struct regmap_config wl2866d_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,
};

#if 0
static int wl2866d_i2c_write(struct wl2866d_chip *chip,
			    unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		    i2c_smbus_write_byte_data(chip->client, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt,
			       ret);
		} else {
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int wl2866d_masked_write(struct wl2866d_chip *chip, unsigned char reg_addr, u8 mask, u8 val)
{
	int ret = -1;

	val &= mask;
	ret = wl2866d_i2c_write(chip, reg_addr, val);//bit1
	if (ret < 0) {
		pr_err("wl2866d set enable failed\n");
		return ret;
	}
	return ret;
}

static int wl2866d_i2c_read(struct wl2866d_chip *chip,
			   unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(chip->client, reg_addr);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt,
				ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}
#else
static int wl2866d_i2c_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		pr_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int wl2866d_i2c_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	pr_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		pr_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int wl2866d_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	pr_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		pr_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	return rc;
}
#endif

static ssize_t wl2866d_vol_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct wl2866d_chip *chip = dev_get_drvdata(dev);
	unsigned char reg_val = 0;

	wl2866d_i2c_read(chip->regmap, wl2866d_on_config[VOL_ENABLE].reg, &reg_val, 1);
	return snprintf(buf, 10, "%d\n", reg_val);

}

static ssize_t wl2866d_vol_enable_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret = 0;
	unsigned char reg_val = 0;
	struct wl2866d_chip *chip = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 0, &reg_val);
	if (ret < 0)
		return ret;

	ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[VOL_ENABLE].reg, &reg_val, 1);
	if (ret < 0) {
		pr_err("wl2866d set enable failed\n");
		return ret;
	}
	return count;
}


static ssize_t wl2866d_out_avdd2_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct wl2866d_chip *chip = dev_get_drvdata(dev);
	unsigned char reg_val = 0;

	wl2866d_i2c_read(chip->regmap, wl2866d_on_config[OUT_AVDD2].reg, &reg_val, 1);
	return snprintf(buf, 10, "%d\n", reg_val);

}

static ssize_t wl2866d_out_avdd2_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret = 0;
	unsigned char reg_val = 0;
	struct wl2866d_chip *chip = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 0, &reg_val);
	if (ret < 0)
		return ret;

	ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[OUT_AVDD2].reg, &reg_val, 1);
	if (ret < 0) {
		pr_err("wl2866d open avdd2 failed\n");
		return ret;
	}
	return count;
}


static ssize_t wl2866d_out_avdd1_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct wl2866d_chip *chip = dev_get_drvdata(dev);
	unsigned char reg_val = 0;

	wl2866d_i2c_read(chip->regmap, wl2866d_on_config[OUT_AVDD1].reg, &reg_val, 1);
	return snprintf(buf, 10, "%d\n", reg_val);

}

static ssize_t wl2866d_out_avdd1_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret = 0;
	unsigned char reg_val = 0;
	struct wl2866d_chip *chip = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 0, &reg_val);
	if (ret < 0)
		return ret;

	ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[OUT_AVDD1].reg, &reg_val, 1);
	if (ret < 0)	{
		pr_err("wl2866d open avdd1 failed\n");
		return ret;
	}
	return count;
}


static ssize_t wl2866d_out_dvdd2_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct wl2866d_chip *chip = dev_get_drvdata(dev);
	unsigned char reg_val = 0;

	wl2866d_i2c_read(chip->regmap, wl2866d_on_config[OUT_DVDD1].reg, &reg_val, 1);
	return snprintf(buf, 10, "%d\n", reg_val);

}

static ssize_t wl2866d_out_dvdd2_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret = 0;
	unsigned char reg_val = 0;
	struct wl2866d_chip *chip = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 0, &reg_val);
	if (ret < 0)
		return ret;

	ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[OUT_DVDD1].reg, &reg_val, 1);
	if (ret < 0)	{
		pr_err("wl2866d open dvdd2 failed\n");
		return ret;
	}
	return count;
}


static ssize_t wl2866d_out_dvdd1_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct wl2866d_chip *chip = dev_get_drvdata(dev);
	unsigned char reg_val = 0;

	wl2866d_i2c_read(chip->regmap, wl2866d_on_config[OUT_DVDD1].reg, &reg_val, 1);
	return snprintf(buf, 10, "%d\n", reg_val);

}

static ssize_t wl2866d_out_dvdd1_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret = 0;
	unsigned char reg_val = 0;
	struct wl2866d_chip *chip = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 0, &reg_val);
	if (ret < 0)
		return ret;
	ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[OUT_DVDD1].reg, &reg_val, 1);
	if (ret < 0)	{
		pr_err("wl2866d open dvdd1 failed\n");
		return ret;
	}
	return count;
}

static DEVICE_ATTR(vol_enable, 0664, wl2866d_vol_enable_show, wl2866d_vol_enable_store);
static DEVICE_ATTR(out_avdd2, 0664, wl2866d_out_avdd2_show, wl2866d_out_avdd2_store);
static DEVICE_ATTR(out_avdd1, 0664, wl2866d_out_avdd1_show, wl2866d_out_avdd1_store);
static DEVICE_ATTR(out_dvdd2, 0664, wl2866d_out_dvdd2_show, wl2866d_out_dvdd2_store);
static DEVICE_ATTR(out_dvdd1, 0664, wl2866d_out_dvdd1_show, wl2866d_out_dvdd1_store);

static struct attribute *wl2866d_attributes[] = {
	&dev_attr_out_dvdd1.attr,
	&dev_attr_out_dvdd2.attr,
	&dev_attr_out_avdd1.attr,
	&dev_attr_out_avdd2.attr,
	&dev_attr_vol_enable.attr,
	NULL
};

static struct attribute_group wl2866d_attribute_group = {
	.attrs = wl2866d_attributes
};

static int wl2866d_get_id(struct  wl2866d_chip *chip)
{
	unsigned char reg_val = 0;
	unsigned int reg_val1 = 0; 
	int ret = 0;
	struct regmap *regmap;

	wl2866d_i2c_read(chip->regmap, wl2866d_on_config[OUT_DVDD1].reg, &reg_val, 1);
	pr_err("%s:wl2866d id is %d\n", __func__, reg_val);

	if ((reg_val != WL2866D_ID) && (reg_val != WL2866D_ID1)) {
		ret = -1;
		return ret;
	}
	return 0;
}

static int set_init_voltage(struct wl2866d_chip *chip)
{
	int ret = 0;
	int i = 0;
	u8 val = 0;

	for (i = 0 ; i < (ARRAY_SIZE(wl2866d_on_config) - 2); i++)	{
		val = wl2866d_on_config[i].value;
		ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[i].reg, &val, 1);
		if (ret < 0) {
			pr_err("wl2866d init voltage failed\n");
			return ret;
		}
	}
	//enable dischager function
	val = wl2866d_on_config[DISCHARGE_ENABLE].value;
	ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[DISCHARGE_ENABLE].reg, &val, 1);
	if (ret < 0) {
		pr_err("wl2866d  dischager function enable failed\n");
		return ret;
	}
	return 0;
}

void wl2866d_print_reg(struct  wl2866d_chip *chip)
{
	int i;
	unsigned char reg_val = 0;

	for (i = 0 ; i < ARRAY_SIZE(wl2866d_on_config); i++) {
		wl2866d_i2c_read(chip->regmap, wl2866d_on_config[i].reg, &reg_val, 1);
		pr_err("%s:wl2866d info is reg %d, value %d\n", __func__, wl2866d_on_config[i].reg, reg_val);
	}

}

static int wl2866d_init(struct  wl2866d_chip *chip)
{
	int ret = 0;
	u8 val =0, val1 = 0x4B;

	chip->en_gpio = of_get_named_gpio(chip->dev->of_node,
			 "en-gpio", 0);
	if (!gpio_is_valid(chip->en_gpio)) {
		pr_err("%s:%d, en gpio not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}

	pr_err("%s: en_gpio is %d\n", __func__, chip->en_gpio);
	ret = gpio_request(chip->en_gpio, "wl2866d_en");
	if (ret < 0) {
			pr_err("wl2866d enable gpio request failed\n");
			return ret;
	}

	gpio_direction_output(chip->en_gpio, 0);

	msleep(10);

	ret = wl2866d_get_id(chip);
	if (ret < 0) {
		pr_err("wl2866d read id failed\n");
		printk(KERN_ERR "wl2866d read id failed\n");
//		return ret;
	}

	ret = set_init_voltage(chip);
	if (ret < 0)
		pr_err("wl2866d init failed\n");

#ifdef WL2866D_DEBUG
	msleep(10);
	wl2866d_print_reg(chip);
#endif

	return 0;
}

static int wl2866d_regulator_enable(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			wl2866d_err(fan_reg, "failed to enable parent rc=%d\n", rc);
			return rc;
		}
	}

	rc = wl2866d_masked_write(camera_chip->regmap,
		wl2866d_on_config[VOL_ENABLE].reg,
		1u<<fan_reg->offset, 1u<<fan_reg->offset);
	if (rc < 0) {
		wl2866d_err(fan_reg, "failed to enable regulator rc=%d\n", rc);
		goto remove_vote;
	}

	return 0;

remove_vote:
	if (fan_reg->parent_supply)
		rc = regulator_disable(fan_reg->parent_supply);
	if (rc < 0)
		wl2866d_err(fan_reg, "failed to disable parent regulator rc=%d\n",rc);
	return -ETIME;
}

static int wl2866d_regulator_disable(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = wl2866d_masked_write(camera_chip->regmap,
		wl2866d_on_config[VOL_ENABLE].reg,
		1u<<fan_reg->offset, 0);

	if (rc < 0) {
		wl2866d_err(fan_reg,
			"failed to disable regulator rc=%d\n", rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
		if (rc < 0) {
			wl2866d_err(fan_reg,
				"failed to remove parent voltage rc=%d\n", rc);
			return rc;
		}
		rc = regulator_disable(fan_reg->parent_supply);
		if (rc < 0) {
			wl2866d_err(fan_reg,
				"failed to disable parent rc=%d\n", rc);
			return rc;
		}
	}

	wl2866d_debug(fan_reg, "regulator disabled\n");
	return 0;
}

static int wl2866d_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 val;
	
	rc = wl2866d_i2c_read(camera_chip->regmap, wl2866d_on_config[VOL_ENABLE].reg, &val, 1);
	if (rc < 0) {
		wl2866d_err(fan_reg,
			"failed to read regulator voltage rc = %d\n", rc);
		return rc;
	}
	
	return !!(val & (1u << fan_reg->offset));
}

static int wl2866d_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8 vset;
	int rc;
	int uv;

	printk(KERN_ERR "%s reg1 is %d\n",__func__,fan_reg->offset);
	printk(KERN_ERR "%s reg2 is %d\n",__func__,LDO_VSET_REG(fan_reg->offset));
	rc = wl2866d_i2c_read(camera_chip->regmap, LDO_VSET_REG(fan_reg->offset), &vset, 1);
	if (rc < 0) {
		wl2866d_err(fan_reg,
			"failed to read regulator voltage rc = %d\n", rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_mv;
	} else {
		wl2866d_debug(fan_reg, "voltage read [%x]\n", vset);
		if (fan_reg->offset == 0 || fan_reg->offset == 1)
			uv = (VSET_BASE_12 + vset*VSET_STEP_MV)*100;
		else
			uv = (VSET_BASE_34 + vset*VSET_STEP_MV_34)*100;
	}
	return uv;
}

static int wl2866d_write_voltage(struct wl2866d_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc = 0, mv;
	u8 vset;

	mv = DIV_ROUND_UP(min_uv, 100);
	if (mv*100 > max_uv) {
		wl2866d_err(fan_reg, "requestd voltage above maximum limit\n");
		return -EINVAL;
	}

	if (fan_reg->offset == 0 || fan_reg->offset == 1)
		vset = DIV_ROUND_UP(mv-VSET_BASE_12, VSET_STEP_MV);
	else
		vset = DIV_ROUND_UP(mv-VSET_BASE_34, VSET_STEP_MV_34);

	rc = wl2866d_i2c_write(camera_chip->regmap, LDO_VSET_REG(fan_reg->offset), &vset, 1);
	if (rc < 0) {
		wl2866d_err(fan_reg, "failed to write voltage rc = %d\n", rc);
		return rc;
	}

	wl2866d_debug(fan_reg, "VSET=[%2x]\n", vset);
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
			wl2866d_err(fan_reg,
				"failed to request parent supply voltage rc=%d\n", rc);
			return rc;
		}
	}

	rc = wl2866d_write_voltage(fan_reg, min_uv, max_uv);
	if (rc < 0) {
		/* remove parentn's voltage vote */
		if (fan_reg->parent_supply)
			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	}
	wl2866d_debug(fan_reg, "voltage set to %d\n", min_uv);
	return rc;
}

static struct regulator_ops wl2866d_regulator_ops = {
	.enable = wl2866d_regulator_enable,
	.disable = wl2866d_regulator_disable,
	.is_enabled = wl2866d_regulator_is_enabled,
	.set_voltage = wl2866d_regulator_set_voltage,
	.get_voltage = wl2866d_regulator_get_voltage,
};

static int wl2866d_register_ldo(struct wl2866d_regulator *wl2866d_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = wl2866d_reg->of_node;
	struct device *dev = wl2866d_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i<WL2866D_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if ( i == WL2866D_MAX_LDO) {
		pr_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &wl2866d_reg->offset);
	if (rc < 0) {
		pr_err("%s:failed to get regulator offset rc = %d\n", name, rc);
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
				pr_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &wl2866d_reg->rdesc);
	if (init_data == NULL) {
		pr_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		pr_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = wl2866d_write_voltage(wl2866d_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			pr_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = wl2866d_reg;
	reg_config.of_node = reg_node;

	wl2866d_reg->rdesc.owner = THIS_MODULE;
	wl2866d_reg->rdesc.type = REGULATOR_VOLTAGE;
	wl2866d_reg->rdesc.ops = &wl2866d_regulator_ops;
	wl2866d_reg->rdesc.name = init_data->constraints.name;
	wl2866d_reg->rdesc.n_voltages = 1;

	pr_info("try to register ldo %s\n", name);
	wl2866d_reg->rdev = devm_regulator_register(dev, &wl2866d_reg->rdesc,
		&reg_config);
	if (IS_ERR(wl2866d_reg->rdev)) {
		rc = PTR_ERR(wl2866d_reg->rdev);
		pr_err("%s: failed to register regulator rc =%d\n",
		wl2866d_reg->rdesc.name, rc);
		return rc;
	}

	pr_info("%s regulator register done\n", name);
	return 0;
}

static int wl2866d_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc = 0;
	const char *name;
	struct device_node *child;
	struct wl2866d_regulator *wl2866d_reg;

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		wl2866d_reg = devm_kzalloc(dev, sizeof(*wl2866d_reg), GFP_KERNEL);
		if (!wl2866d_reg)
			return -ENOMEM;

		wl2866d_reg->regmap = regmap;
		wl2866d_reg->of_node = child;
		wl2866d_reg->dev = dev;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = wl2866d_register_ldo(wl2866d_reg, name);
		if (rc <0 ) {
			pr_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int wl2866d_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int ret = 0;
	struct regmap *regmap;
	struct wl2866d_chip *chip;

	pr_err("%s,wl2866d_probe ++++ i2c addr 0x%x",__func__,client->addr);
	chip = devm_kzalloc(&client->dev, sizeof(struct wl2866d_chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto err_mem;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "check_functionality failed\n");
		ret = -EIO;
		goto  init_err;
	}
	
	regmap = devm_regmap_init_i2c(client, &wl2866d_regmap_config);
	if (IS_ERR(regmap)) {
		pr_err("FAN53870 failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}
	
	chip->client = client;
	chip->regmap = regmap;
	chip->dev = &client->dev;
	dev_set_drvdata(chip->dev, chip);
	i2c_set_clientdata(chip->client, chip);
	camera_chip = chip;

	ret = wl2866d_init(chip);

	if (ret < 0) {
		dev_err(&client->dev, "wl2866d init fail!\n");
		ret = -ENODEV;
		goto init_err;
	}

	ret = sysfs_create_group(&client->dev.kobj,
					   &wl2866d_attribute_group);
	if (ret < 0) {
		dev_info(&client->dev, "%s error creating sysfs attr files\n",
			 __func__);
		goto err_sysfs;
	}

	ret = wl2866d_parse_regulator(regmap, &client->dev);
	if (ret < 0) {
		printk(KERN_ERR "wl2866d failed to parse device tree ret=%d\n", ret);
		return ret;
	}	

	pr_err("%s,successfully\n", __func__);
	return 0;
err_sysfs:
init_err:
vreg_init_err:
	devm_kfree(chip->dev, chip);
	chip = NULL;
err_mem:
	return ret;
}

static int wl2866d_remove(struct i2c_client *client)
{
	struct wl2866d_chip *chip = i2c_get_clientdata(client);

	devm_kfree(chip->dev, chip);
	chip = NULL;
	return 0;
}

//#ifdef CONFIG_PM_SLEEP
#if 0
static int wl2866d_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wl2866d_chip *chip = i2c_get_clientdata(client);
	int ret = 0;
	u8 val = 0;

	pr_err("%s\n", __func__);
	val = wl2866d_on_config[VOL_DISABLE].value;
	ret = wl2866d_i2c_write(chip->regmap, wl2866d_on_config[VOL_DISABLE].reg, &val, 1);
	if (ret < 0)
	   pr_err("wl2866d close voltage failed\n");

//	wl2866d_disable_power(chip);
	return 0;
}

static int wl2866d_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wl2866d_chip *chip = i2c_get_clientdata(client);
	//int ret = 0;

	pr_err("%s\n", __func__);
//	wl2866d_enable_power(chip);
	gpio_direction_output(chip->en_gpio, 0);
	/*
	ret = wl2866d_i2c_write(chip, wl2866d_on_config[VOL_ENABLE].reg, wl2866d_on_config[VOL_ENABLE].value);
	if (ret < 0) {
		pr_err("wl2866d set enable failed\n");
		return ret;
	}
	*/
	return 0;
}

static const struct dev_pm_ops wl2866d_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(wl2866d_suspend, wl2866d_resume)
};
#endif

static const struct i2c_device_id wl2866d_id_table[] = {
	{"wl2866d", 0},
	{} /* NULL terminated */
};

MODULE_DEVICE_TABLE(i2c, wl2866d_id_table);


//#ifdef CONFIG_OF
static const struct of_device_id wl2866d_i2c_of_match_table[] = {
		{ .compatible = "xiaomi,wl2866d", },
		{},
};
MODULE_DEVICE_TABLE(of, wl2866d_i2c_of_match_table);
//#endif

static struct i2c_driver wl2866d_driver = {
	.driver = {
		.name = "xiaomi,wl2866d",
//#ifdef CONFIG_PM_SLEEP
#if 0
		.pm = &wl2866d_pm_ops,
#endif
		.of_match_table = of_match_ptr(wl2866d_i2c_of_match_table),
		},
	.probe = wl2866d_probe,
	.remove = wl2866d_remove,
	.id_table = wl2866d_id_table,
};

static int __init wl2866d_i2c_init(void)
{
	printk(KERN_ERR "%s\n", __func__);
	return i2c_add_driver(&wl2866d_driver);
}
subsys_initcall(wl2866d_i2c_init);
//module_init(wl2866d_i2c_init);
static void __exit wl2866d_i2c_exit(void)
{
	printk(KERN_ERR "%s\n", __func__);
	i2c_del_driver(&wl2866d_driver);
}
module_exit(wl2866d_i2c_exit);

MODULE_DESCRIPTION("wl2866d driver for xiaomi");
MODULE_AUTHOR("xiaomi,lnc.");
MODULE_LICENSE("GPL");
