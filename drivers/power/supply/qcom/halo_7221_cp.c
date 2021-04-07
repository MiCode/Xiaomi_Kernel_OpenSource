
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <linux/delay.h>
#include <linux/kernel.h>

#include <linux/poll.h>

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/types.h>

#define DEVICE_NAME  "HALO7221_cp"

struct halo7221_dev;

struct halo7221_dt_props {
	unsigned int enable_gpio;
};

struct halo7221_func {
	int (*read)(struct halo7221_dev *di, u16 reg, u8 *val);
	int (*write)(struct halo7221_dev *di, u16 reg, u8 val);
	int (*read_buf)(struct halo7221_dev *di,
			u16 reg, u8 *buf, u32 size);
	int (*write_buf)(struct halo7221_dev *di,
			u16 reg, u8 *buf, u32 size);
	int (*write_masked)(struct halo7221_dev *di,
			u16 reg, u8 mask, u8 val);
};

struct halo7221_dev {
	char                *name;
	struct i2c_client    *client;
	struct device       *dev;
	struct regmap       *regmap;
	struct halo7221_func  bus;
	struct halo7221_dt_props dt_props;

	struct pinctrl *halo_pinctrl;
	struct pinctrl_state *halo_gpio_active;
	struct pinctrl_state *halo_gpio_suspend;
	struct power_supply	*halo_psy;
};

/*
typedef union{
	u16 value;
	u8  ptr[2];
}vuc;
*/

extern char *saved_command_line;
static int get_board_version(void)
{
	char boot[5] = {'\0'};
	char *match = (char *) strnstr(saved_command_line,
				"androidboot.hwlevel=",
				strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.hwlevel=")),
			sizeof(boot) - 1);
		printk("%s: hwlevel is %s\n", __func__, boot);
		if (!strncmp(boot, "P1.3", strlen("P1.3")))
			return 1;
	}
	return 0;
}

int halo7221_read(struct halo7221_dev *chip, u16 reg, u8 *val)
{
	unsigned int temp;
	int rc;

	rc = regmap_read(chip->regmap, reg, &temp);
	if (rc >= 0)
		*val = (u8) temp;

	return rc;
}

int halo7221_write(struct halo7221_dev *chip, u16 reg, u8 val)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, reg, val);
	if (rc < 0)
		dev_err(chip->dev, "%s: write error: %d\n", __func__, rc);

	return rc;
}

int halo7221_read_buffer(struct halo7221_dev *chip, u16 reg, u8 *buf, u32 size)
{
	return regmap_bulk_read(chip->regmap, reg, buf, size);
}

int halo7221_write_buffer(struct halo7221_dev *chip, u16 reg, u8 *buf, u32 size)
{
	int rc = 0;

	while (size--) {
		rc = chip->bus.write(chip, reg++, *buf++);
		if (rc < 0) {
			dev_err(chip->dev, "write error: %d\n", rc);
			return rc;
		}
	}

	return rc;
}

int halo7221_write_masked(struct halo7221_dev *chip, u16 reg, u8 mask, u8 val)
{
	return regmap_update_bits(chip->regmap, reg, mask, val);
}
#define REG_NONE_ACCESS 0
#define REG_RD_ACCESS  (1 << 0)
#define REG_WR_ACCESS  (1 << 1)
#define REG_BIT_ACCESS  (1 << 2)

#define REG_MAX         0x0F

static int halo7221_set_enable(struct halo7221_dev *chip, int enable)
{
	int ret = 0;

	if (gpio_is_valid(chip->dt_props.enable_gpio)) {
		ret = gpio_request(chip->dt_props.enable_gpio,
				"halo-enable-gpio");
		if (ret) {
			dev_err(chip->dev,
					"%s: unable to request halo enable gpio [%d]\n",
					__func__, chip->dt_props.enable_gpio);
		}
		ret = gpio_direction_output(chip->dt_props.enable_gpio, enable);
		if (ret) {
			dev_err(chip->dev,
					"%s: cannot set direction for halo enable gpio [%d]\n",
					__func__, chip->dt_props.enable_gpio);
		}
		gpio_free(chip->dt_props.enable_gpio);
	}
	return ret;

}

static int halo7221_hw_init(struct halo7221_dev *chip)
{
	int rc = 0;

	rc = chip->bus.write(chip, 0x61, 0x80);
	msleep(200);
	rc = chip->bus.write(chip, 0x61, 0x00);
	msleep(200);
	rc = chip->bus.write_masked(chip, 0x03, GENMASK(1, 0), 0x01); // set 11v to enable cp mode
	msleep(50);
	rc = chip->bus.write_masked(chip, 0x01, GENMASK(7, 3), 0x1A); // Iin_Limit 1500mA

	return rc;
}

/* chip enable attrs */
static ssize_t chip_enable_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int ret = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct halo7221_dev *chip = i2c_get_clientdata(client);

	if (gpio_is_valid(chip->dt_props.enable_gpio))
		ret = gpio_get_value(chip->dt_props.enable_gpio);
	else {
		dev_err(chip->dev, "%s: enable gpio not provided\n", __func__);
		ret = -1;
	}

	dev_info(chip->dev, "chip enable gpio: %d\n", ret);

	return snprintf(buf, sizeof(buf), "Chip enable: %d\n", ret);
}

static ssize_t chip_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	int ret, enable;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct halo7221_dev *chip = i2c_get_clientdata(client);

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;

	halo7221_set_enable(chip, enable);

	return count;
}

static ssize_t chip_init_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int ret = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct halo7221_dev *chip = i2c_get_clientdata(client);

	ret = halo7221_hw_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: chip hw init error [%d]\n",
				__func__, ret);
	}
	return snprintf(buf, sizeof(buf), "Chip init: %d\n", ret);
}

/*
enum REG_INDEX
{
	CHIPID = 0,
	VOUT,
	INT_FLAG,
	INTCTLR,
	VOUTSET,
	VFC,
	CMD,
	INDEX_MAX,
};

const u16 reg_access[INDEX_MAX][2] = {
	[CHIPID] = {REG_CHIPID, REG_RD_ACCESS},
	[VOUT] = {REG_VOUT, REG_RD_ACCESS},
	[INT_FLAG] = {REG_INTFLAG,REG_RD_ACCESS},
	[INTCTLR] = {REG_INTCLR,REG_WR_ACCESS},
	[VOUTSET] = {REG_VOUTSET, REG_RD_ACCESS|REG_WR_ACCESS},
	[VFC] = {REG_VFC, REG_RD_ACCESS|REG_WR_ACCESS},
	[CMD] = {REG_CMD, REG_RD_ACCESS|REG_WR_ACCESS|REG_BIT_ACCESS},
};


static ssize_t get_reg(struct device* cd,struct device_attribute *attr, char* buf)
{
	u8 val[2];
	ssize_t len = 0;
	int i = 0;

	for(i = 0; i< INDEX_MAX ;i++) {
		if(reg_access[i][1] & REG_RD_ACCESS) {
			halo7221_read_buffer(mte,reg_access[i][0],val,2);
			len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x%02x\n", reg_access[i][0],val[0],val[1]);
		}
	}

	return len;
}

static ssize_t set_reg(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned int databuf[2];
	vuc val;
	u8  tmp[2];
	u16 regdata;
	int i = 0;
	int ret = 0;

	ret = sscanf(buf,"%x %x",&databuf[0], &databuf[1]);
	printk("%s : %d 0x%x 0x%x\n",  __func__, ret, databuf[0],databuf[1]);
	if(2 == ret) {
		for(i = 0; i< INDEX_MAX ;i++) {
			if(databuf[0] == reg_access[i][0]) {
				if (reg_access[i][1] & REG_WR_ACCESS) {
					val.ptr[1] = databuf[1]& 0x00ff;   //big endian
					val.ptr[0] = (databuf[1]& 0xff00) >> 8;
					if (reg_access[i][1] & REG_BIT_ACCESS) {
						halo7221_read_buffer(mte,databuf[0],tmp,2);
						regdata = tmp[0]<<8|tmp[1];
						val.value |= regdata;
						printk("get reg: 0x%04x  set reg: 0x%04x \n", regdata,val.value);
						halo7221_write_buffer(mte,databuf[0],val.ptr,2);
					}
					else {
						printk("Set reg : [0x%04x]  0x%x 0x%x \n",databuf[1], val.ptr[0], val.ptr[1]);
						halo7221_write_buffer(mte,databuf[0],val.ptr,2);
					}
				}
				break;
			}
		}
	}
	return len;
}

static ssize_t chip_version_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	u8 fwver[2];
	ssize_t len = 0;  // must to set 0
	pr_debug("chip_version_show\n");
	halo7221_read_buffer(mte,REG_FW_VER,fwver,2);
	pr_debug("chip_version : 0x%02x,0x%02x\n",fwver[0],fwver[1]);
	len += snprintf(buf+len, PAGE_SIZE-len, "chip_version : %02x,%02x\n", fwver[0],fwver[1]);
	return len;
}
*/

/* voltage limit attrs */
/*
static ssize_t chip_vout_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	unsigned char   fwver[2];
	unsigned short  vout_value;
	ssize_t len = 0;
	pr_debug("chip_vout_show\n");
	// data_list = self.read(REG_VOUT, 2)
	//     val = data_list[1]|(data_list[0]<<8)
	//     self.insertPlainText('Vout: %0.3fV\n'%(val/1000.0))
	halo7221_read_buffer(mte,REG_VOUT,fwver,2);
	vout_value = fwver[0] << 8 |  fwver[1];
	pr_debug("chip Vout : %d\n",vout_value);
	len += snprintf(buf+len, PAGE_SIZE-len, "chip Vout : %d mV\n",vout_value);
	return len;
}

static ssize_t chip_vout_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	vuc val;
	int error;
	unsigned int temp;
	u8 vptemp;
	error = kstrtouint(buf, 10, &temp);
	if (error)
		return error;
	if( (temp < 0) ||( temp > 20000)){
		pr_debug(" Parameter error\n");
		return count;
	}
	val.value = temp;
	vptemp = val.ptr[0];
	val.ptr[0] = val.ptr[1];
	val.ptr[1] = vptemp;
	halo7221_write_buffer(mte,REG_VOUTSET,val.ptr,2);
	pr_debug("Set Vout : %d \n", val.value);
	// mv=int(self.dsbVout.value()*1000)
	//     self.write(REG_VOUTSET, [(mv>>8)&0xff, mv&0xff])
	//     self.insertPlainText('Set Vout: %0.3fV\n'%(mv/1000.0))

	return count;
}

static ssize_t fast_charging_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	vuc val;
	int error;
	unsigned int temp;
	u8  fcflag;
	error = kstrtouint(buf, 10, &temp);
	if (error)
		return error;
	if( (temp < 0) ||( temp > 20000)){
		pr_debug(" Parameter error\n");
		return count;
	}
	val.value = temp;
	fcflag = val.ptr[0];
	val.ptr[0] = val.ptr[1];
	val.ptr[1] = fcflag;
	halo7221_write_buffer(mte,REG_VFC,val.ptr,2);
	pr_debug("FC send data : 0x%02x,0x%02x \n", val.ptr[0],val.ptr[1]);
	val.value = FASTCHARGE;
	halo7221_write_buffer(mte,REG_CMD,val.ptr,2);
	return count;
}
static ssize_t fast_charging_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	unsigned char   fwver[2];
	unsigned short  fc_value;
	ssize_t len = 0;
	pr_debug("fast_charging_show\n");
	// data_list = self.read(REG_VOUT, 2)
	//     val = data_list[1]|(data_list[0]<<8)
	//     self.insertPlainText('Vout: %0.3fV\n'%(val/1000.0))
	halo7221_read_buffer(mte,REG_VFC,fwver,2);
	fc_value = fwver[0] << 8 |  fwver[1];
	pr_debug("FC read data : %d\n",fc_value);
	len += snprintf(buf+len, PAGE_SIZE-len, "FC read data : %d mV\n",fc_value);
	return len;
}
*/

/*
static DEVICE_ATTR(chip_version, S_IRUGO | S_IWUSR, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IRUGO | S_IWUSR, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(fast_charging,S_IRUGO | S_IWUSR, fast_charging_show, fast_charging_store);
static DEVICE_ATTR(reg,S_IRUGO | S_IWUSR, get_reg, set_reg);
*/
static DEVICE_ATTR(chip_enable, S_IWUSR | S_IRUGO, chip_enable_show, chip_enable_store);
static DEVICE_ATTR(chip_init, S_IWUSR | S_IRUGO, chip_init_show, NULL);

static struct attribute *halo7221_sysfs_attrs[] = {
/*
	&dev_attr_chip_version.attr,
	&dev_attr_chip_vout.attr,
	&dev_attr_fast_charging.attr,
	&dev_attr_reg.attr,
*/
	&dev_attr_chip_enable.attr,
	&dev_attr_chip_init.attr,
	NULL,
};

static const struct attribute_group halo7221_sysfs_group = {
	.name = "halo7221group",
	.attrs = halo7221_sysfs_attrs,
};

static const struct of_device_id match_table[] = {
	{.compatible = "halo7221,halo7221_cp",},
	{ },
};

static const struct regmap_config halo7221_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static int halo7221_parse_dt(struct halo7221_dev *chip)
{
	struct device_node *node = chip->dev->of_node;
	if (!node) {
		dev_err(chip->dev, "device tree node missing\n");
		return -EINVAL;
	}

	chip->dt_props.enable_gpio = of_get_named_gpio(node, "halo,enable", 0);
	if ((!gpio_is_valid(chip->dt_props.enable_gpio)))
		return -EINVAL;

	return 0;
}


static int halo7221_gpio_init(struct halo7221_dev *chip)
{
	int ret = 0;

	chip->halo_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->halo_pinctrl)) {
		dev_err(chip->dev, "No pinctrl config specified\n");
		ret = PTR_ERR(chip->dev);
		return ret;
	}
	chip->halo_gpio_active =
		pinctrl_lookup_state(chip->halo_pinctrl, "halo7221_active");
	if (IS_ERR_OR_NULL(chip->halo_gpio_active)) {
		dev_err(chip->dev, "No active config specified\n");
		ret = PTR_ERR(chip->halo_gpio_active);
		return ret;
	}
	chip->halo_gpio_suspend =
		pinctrl_lookup_state(chip->halo_pinctrl, "halo7221_suspend");
	if (IS_ERR_OR_NULL(chip->halo_gpio_suspend)) {
		dev_err(chip->dev, "No suspend config specified\n");
		ret = PTR_ERR(chip->halo_gpio_suspend);
		return ret;
	}

	ret = pinctrl_select_state(chip->halo_pinctrl,
			chip->halo_gpio_active);
	if (ret < 0) {
		dev_err(chip->dev, "fail to select pinctrl active rc=%d\n",
			ret);
		return ret;
	}

	return ret;
}

static int halo_get_mode(struct halo7221_dev *chip)
{
	int rc, ret;
	u8 val;

	rc = chip->bus.read(chip, 0x08, &val);
	dev_info(chip->dev, "get status is: 0x%x\n", val);
	if (rc < 0)
		ret = -1;
	else {
		ret = val & BIT(5);
	}
	if (ret == 0)
		ret = 2; // return switching mode

	dev_info(chip->dev, "get opmode is: %d\n", ret);

	return ret;
}

#define FORWARD_BYPASS		1
#define FORWARD_SWITCH		2
#define REVERSE_SWITCH		3
static int halo_set_mode(struct halo7221_dev *chip, int mode)
{
	int rc = 0;
	switch (mode) {
	case FORWARD_BYPASS:
		rc = chip->bus.write(chip, 0x00, 0xff);
		rc = chip->bus.write(chip, 0x07, 0x78);
		rc = chip->bus.write(chip, 0xA7, 0xf9);
		rc = chip->bus.write(chip, 0x0A, 0x10);
		rc = chip->bus.write(chip, 0x02, 0xa8); //set ilim to 1500mA
		rc = chip->bus.write(chip, 0x05, 0x61);
		rc = chip->bus.write(chip, 0x03, 0x53);
		break;
	case FORWARD_SWITCH:
		rc = chip->bus.write(chip, 0x00, 0xff);
		rc = chip->bus.write(chip, 0x07, 0x78);
		rc = chip->bus.write(chip, 0xA7, 0xf9);
		rc = chip->bus.write(chip, 0x0A, 0x10);
		rc = chip->bus.write(chip, 0x02, 0xa9); //set ilim to 1500mA
		rc = chip->bus.write(chip, 0x05, 0x61);
		rc = chip->bus.write(chip, 0x03, 0x53);
		break;
	case REVERSE_SWITCH:
		rc = chip->bus.write(chip, 0x00, 0xff);
		rc = chip->bus.write(chip, 0x07, 0x78);
		rc = chip->bus.write(chip, 0xA7, 0xf9);
		rc = chip->bus.write(chip, 0x0A, 0x10);
		rc = chip->bus.write(chip, 0x0B, 0x01);
		break;
	default:
		dev_err(chip->dev, "%s: invalid settings\n",
				 __func__);
	}

	return rc;
}

static enum power_supply_property halo_props[] = {
	POWER_SUPPLY_PROP_DIV_2_MODE,
};

static int halo_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct halo7221_dev *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_DIV_2_MODE:
		val->intval = halo_get_mode(chip);
		break;
	default:
			return -EINVAL;
	}
	return 0;
}

static int halo_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct halo7221_dev *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_DIV_2_MODE:
		rc = halo_set_mode(chip, val->intval);
		break;
	default:
			return -EINVAL;
	}

	return rc;
}

static int halo_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_DIV_2_MODE:
		return 1;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc halo_psy_desc = {
	.name = "halo",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = halo_props,
	.num_properties = ARRAY_SIZE(halo_props),
	.get_property = halo_get_prop,
	.set_property = halo_set_prop,
	.property_is_writeable = halo_prop_is_writeable,
};

static int halo7221_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct halo7221_dev *chip;
	int rc = 0, ret = 0;
	struct power_supply_config halo_cfg = {};
	//vuc chipid;

	dev_info(&client->dev, "halo7221 probe!\n");

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	chip->regmap = regmap_init_i2c(client, &halo7221_regmap_config);
	if (!chip->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}
	chip->name = DEVICE_NAME;
	chip->client = client;
	chip->dev = &client->dev;

	chip->bus.read = halo7221_read;
	chip->bus.write = halo7221_write;
	chip->bus.read_buf = halo7221_read_buffer;
	chip->bus.write_buf = halo7221_write_buffer;
	chip->bus.write_masked = halo7221_write_masked;

	device_init_wakeup(chip->dev, true);
	i2c_set_clientdata(client, chip);

	ret = halo7221_parse_dt(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: parse dt error [%d]\n",
				__func__, ret);
		goto cleanup;
	}

	ret = halo7221_gpio_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: gpio init error [%d]\n",
				__func__, ret);
		goto cleanup;
	}
/*
	ret = halo7221_hw_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s: chip hw init error [%d]\n",
				__func__, ret);
		goto cleanup;
	}
*/

	sysfs_create_group(&client->dev.kobj, &halo7221_sysfs_group);

	halo_cfg.drv_data = chip;
	chip->halo_psy = power_supply_register(chip->dev,
			&halo_psy_desc,
			&halo_cfg);

	dev_info(chip->dev, "Success probe halo7221 driver\n");

	return 0;
cleanup:
	i2c_set_clientdata(client, NULL);

	return rc;

}

static int halo7221_remove(struct i2c_client *client)
{


	sysfs_remove_group(&client->dev.kobj, &halo7221_sysfs_group);

	return 0;
}


static const struct i2c_device_id halo7221_dev_id[] = {
	{DEVICE_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, halo7221_dev_id);

static struct i2c_driver halo7221_driver = {
	.driver   = {
		.name           = DEVICE_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = match_table,
	},
	.probe    = halo7221_probe,
	.remove   = halo7221_remove,
	.id_table = halo7221_dev_id,
};

static int __init halo7221_init(void)
{
	int ret;
	int drv_load = 0;

	drv_load = get_board_version();
	if (!drv_load)
		return 0;

	ret = i2c_add_driver(&halo7221_driver);
	if (ret)
		printk(KERN_ERR "halo7221 i2c driver init failed!\n");

	return ret;
}

static void __exit halo7221_exit(void)
{
	i2c_del_driver(&halo7221_driver);
}

module_init(halo7221_init);
module_exit(halo7221_exit);

MODULE_AUTHOR("Yangwl@maxictech.com");
MODULE_DESCRIPTION("halo7221 Wireless Power Receiver");
MODULE_LICENSE("GPL v2");
