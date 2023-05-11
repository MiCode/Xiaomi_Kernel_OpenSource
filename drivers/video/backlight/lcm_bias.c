
/*
 * Copyright (C) 2011 ST-Ericsson SA.
 * Copyright (C) 2009 Motorola, Inc.
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for National Semiconductor bkl Backlight driver chip
 *
 * Author: Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>
 * based on leds-bkl.c by Dan Murphy <D.Murphy@motorola.com>
 */
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
/**
 * struct bkl_data
 * @led_dev: led class device
 * @client: i2c client
 * @pdata: bkl platform data
 * @mode: mode of operation - manual, ALS, PWM
 * @regulator: regulator
 * @brightness: previous brightness value
 * @enable: regulator is enabled
 */
#define bkl_NAME "lcm-bias"
#define bkl_LED_DEV "lcm-bias"
struct bkl_data {
	struct led_classdev led_dev;
	struct i2c_client *client;
	struct device dev;
	struct i2c_adapter *adapter;
	unsigned short addr;
	struct mutex lock;
	struct work_struct work;
	enum led_brightness brightness;
	bool enable;
	u8 pwm_cfg;
	u8 full_scale_current;
	bool brt_code_enable;
	u16 *brt_code_table;
	int hwen_gpio;
	unsigned int  pwm_mode;
	bool using_lsb;
	unsigned int pwm_period;
	unsigned int full_scale_led;
	unsigned int ramp_on_time;
	unsigned int ramp_off_time;
	unsigned int pwm_trans_dim;
	unsigned int i2c_trans_dim;
	unsigned int channel;
	unsigned int ovp_level;
	unsigned int frequency;
	unsigned int default_brightness;
	unsigned int max_brightness;
	unsigned int induct_current;
	unsigned int flash_current;
	unsigned int flash_timeout;
	unsigned int bl_map;
	struct backlight_device *bl_dev;
};
#define MAX_BRIGHTNESS 2047
#define DEFAULT_BRIGHTNESS 1140
struct bkl_data *g_bias_data;
static int i2c_bkl_write(struct i2c_client *client, uint8_t command, uint8_t data)
{
	int retry/*, loop_i*/;
	uint8_t buf[1 + 1];
	uint8_t toRetry = 5;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1 + 1,
			.buf = buf,
		}
	};
	buf[0] = command;
	buf[1] = data;
	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		//msleep(20);
	}
	if (retry == toRetry) {
		printk("%s: i2c  i2c_write_block retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}
	return 0;
}
/*
static int bkl_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	int ret;
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}
	*val = ret;
	//LOG_DBG("Reading 0x%02x=0x%02x\n", reg, *val);
	return ret;
}
*/
static int bkl_init_registers(struct bkl_data *drvdata)
{
	int ret = 0;
	ret |= i2c_bkl_write(drvdata->client,0x00, 0x13);
	ret |= i2c_bkl_write(drvdata->client,0x01, 0x13);
	return ret;
}
static void bkl_hwen_pin_ctrl(struct bkl_data *drvdata, int en)
{
	if (gpio_is_valid(drvdata->hwen_gpio)) {
		if (en) {
			pr_info(" hwen pin is going to be high!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, true);
			usleep_range(3500, 4000);
		} else {
			pr_info("  hwen pin is going to be low!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, false);
			usleep_range(1000, 2000);
		}
	}
}
static int bkl_gpio_init(struct bkl_data *drvdata)
{
	int ret;
	if (gpio_is_valid(drvdata->hwen_gpio)) {
		ret = gpio_request(drvdata->hwen_gpio, "hwen_gpio");
		if (ret < 0) {
			pr_err("   failed to request gpio\n");
			return -1;
		}
		ret = gpio_direction_output(drvdata->hwen_gpio, 1);
		pr_info("  request gpio init\n");
		if (ret < 0) {
			pr_err("   failed to set output");
			gpio_free(drvdata->hwen_gpio);
			return ret;
		}
		pr_info("   gpio is valid!\n");
		bkl_hwen_pin_ctrl(drvdata, 1);
	}
	return 0;
}
static int bkl_backlight_enable(struct bkl_data *drvdata)
{
	pr_info("%s   enter.\n", __func__);
	drvdata->enable = true;
	return 0;
}
/*
int bkl_brightness_set(struct bkl_data *drvdata, int brt_val)
{
	int err = 0;
	if (drvdata->enable == false) {
		bkl_init_registers(drvdata);
		bkl_backlight_enable(drvdata);
	}
pr_info("%s lcd brightness is 0x%x\n", __func__,brt_val);
	if (brt_val < 0)
		brt_val =0;
	if (brt_val > MAX_BRIGHTNESS)
		brt_val = MAX_BRIGHTNESS;
if(brt_val > 0){
		err = i2c_bkl_write(drvdata->client, 0x04, lm3697_brightness_table_reg22[brt_val]);
		err = i2c_bkl_write(drvdata->client, 0x05, lm3697_brightness_table_reg23[brt_val]);
	pr_err("    i2c_bkl_write  return =%d\n ",err);
	}else{
		err = i2c_bkl_write(drvdata->client, 0x04, 0x00);
		err = i2c_bkl_write(drvdata->client, 0x05, 0x00);
	}
	// set the brightness in brightness control register
//	err |= i2c_bkl_write(drvdata->client, 0x04, (brt_val >> 4));
//	err |= i2c_bkl_write(drvdata->client, 0x05, (brt_val & 0xf));
	drvdata->brightness = brt_val;
	if (drvdata->brightness == 0)
		drvdata->enable = false;
	return err;
}

static int bkl_bl_get_brightness(struct backlight_device *bl_dev)
{
		return bl_dev->props.brightness;
}
static inline int bkl_bl_update_status(struct backlight_device *bl_dev)
{
		struct bkl_data *drvdata = bl_get_data(bl_dev);
		int brt;
		if (bl_dev->props.state & BL_CORE_SUSPENDED)
				bl_dev->props.brightness = 0;
		brt = bl_dev->props.brightness;
		//
		 * Brightness register should always be written
		 * not only register based mode but also in PWM mode.
		 
		return bkl_brightness_set(drvdata, brt);
}

int bkl_backlight_device_set_brightness(struct backlight_device *bl_dev,
				    unsigned long brightness)
{
	int rc = -ENXIO;
	//struct bkl_data *drvdata = bl_get_data(bl_dev);
	mutex_lock(&bl_dev->ops_lock);
	if (bl_dev->ops) {
			if (brightness > bl_dev->props.max_brightness)
			brightness = bl_dev->props.max_brightness;
			pr_debug("   set brightness to %lu\n", brightness);
			bl_dev->props.brightness = brightness;
			rc = bkl_bl_update_status(bl_dev);
			pr_err("   bkl_bl_update_status   return =%d\n ",rc);
		}
	mutex_unlock(&bl_dev->ops_lock);
	//backlight_generate_event(bl_dev, BACKLIGHT_UPDATE_SYSFS);
	return rc;
}
EXPORT_SYMBOL(bkl_backlight_device_set_brightness);

static const struct backlight_ops bkl_bl_ops = {
		.update_status = bkl_bl_update_status,
		.get_brightness = bkl_bl_get_brightness,
};

//add store for read and weite by wuning 
static ssize_t bkl_i2c_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bkl_data *drvdata = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	for (i = 0; i < 0x20; i++) {
	bkl_read_reg(drvdata->client, i, &reg_val);
	len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n",i, reg_val);
	}
	return len;
}
static ssize_t bkl_i2c_reg_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
        struct bkl_data *drvdata = dev_get_drvdata(dev);
        unsigned int databuf[2] = {0, 0};
        if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
                i2c_bkl_write(drvdata->client,
                                (unsigned char)databuf[0],
                                (unsigned char)databuf[1]);
        }
        return count;
}
static DEVICE_ATTR(bkl_reg, 0664, bkl_i2c_reg_show, bkl_i2c_reg_store);
static struct attribute *bkl_attributes[] = {
	&dev_attr_bkl_reg.attr,
	NULL
};
static struct attribute_group bkl_attribute_group = {
	.attrs = bkl_attributes
};
//add store for read and weite by wuning 

static void __bkl_work(struct bkl_data *led,
				enum led_brightness value)
{
	mutex_lock(&led->lock);
	bkl_brightness_set(led, value);
	mutex_unlock(&led->lock);
}
static void bkl_work(struct work_struct *work)
{
	struct bkl_data *drvdata = container_of(work,
					struct bkl_data, work);
	__bkl_work(drvdata, drvdata->led_dev.brightness);
}

static void bkl_set_brightness(struct led_classdev *led_cdev,
			enum led_brightness brt_val)
{
	struct bkl_data *drvdata;
	drvdata = container_of(led_cdev, struct bkl_data, led_dev);
	schedule_work(&drvdata->work);
}
*/
static void bkl_get_dt_data(struct device *dev, struct bkl_data *drvdata)
{
	struct device_node *np = dev->of_node;
	//u32 bl_channel, temp;
	drvdata->hwen_gpio = of_get_named_gpio(np, "bkl,hwen-gpio", 0);
	pr_info("%s   drvdata->hwen_gpio --<%d>\n", __func__, drvdata->hwen_gpio);
}
static int bkl_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct bkl_data *drvdata;
//	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err = 0;
  	pr_err("%s :  bkl: enter lcm_bias probe \n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s :    I2C_FUNC_I2C not supported\n", __func__);
		err = -EIO;
		goto err_out;
	}
	if (!client->dev.of_node) {
		pr_err("%s :  no device node\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}
	drvdata = kzalloc(sizeof(struct bkl_data), GFP_KERNEL);
	if (drvdata == NULL) {
		pr_err("%s :   kzalloc failed\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}
	drvdata->client = client;
	drvdata->adapter = client->adapter;
	drvdata->addr = client->addr;
	drvdata->brightness = LED_OFF;
	drvdata->enable = true;
	drvdata->led_dev.default_trigger = "bkl-trigger";
	drvdata->led_dev.name = bkl_LED_DEV;
//	drvdata->led_dev.brightness_set = bkl_set_brightness;
	drvdata->led_dev.max_brightness = MAX_BRIGHTNESS;
	mutex_init(&drvdata->lock);
//	INIT_WORK(&drvdata->work, bkl_work);
	bkl_get_dt_data(&client->dev, drvdata);
	i2c_set_clientdata(client, drvdata);
	bkl_gpio_init(drvdata);
#if 0
	err = bkl_read_chipid(drvdata);
	if (err < 0) {
		pr_err("%s : ID idenfy failed\n", __func__);
		goto err_init;
	}
#endif
	err = led_classdev_register(&client->dev, &drvdata->led_dev);
	if (err < 0) {
		pr_err("%s :   Register led class failed\n", __func__);
		err = -ENODEV;
		goto err_init;
	} else {
	pr_debug("%s:   Register led class successful\n", __func__);
	}
	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.brightness = DEFAULT_BRIGHTNESS;
	props.max_brightness = MAX_BRIGHTNESS;
//	bl_dev = backlight_device_register(bkl_NAME, &client->dev,
//					drvdata, &bkl_bl_ops, &props);
	g_bias_data = drvdata;
	bkl_init_registers(drvdata);
	bkl_backlight_enable(drvdata);
//	bkl_brightness_set(drvdata, DEFAULT_BRIGHTNESS);
/*	err = sysfs_create_group(&client->dev.kobj, &bkl_attribute_group);
	if (err < 0) {
		dev_info(&client->dev, "%s error creating sysfs attr files\n",
			__func__);
	goto err_sysfs;
	}
*/
	pr_info("%s   exit\n", __func__);
	return 0;
//err_sysfs:
err_init:
	kfree(drvdata);
err_out:
	return err;
}
 static int bkl_remove(struct i2c_client *client)
{
		struct bkl_data *drvdata = i2c_get_clientdata(client);
		led_classdev_unregister(&drvdata->led_dev);
		kfree(drvdata);
		return 0;
}
static const struct i2c_device_id bkl_id[] = {
	{bkl_NAME, 0},
	{}
};
static struct of_device_id match_table[] = {
			{.compatible = "lcm,lcm-bias",}
};
MODULE_DEVICE_TABLE(i2c, bkl_id);
static struct i2c_driver bias_i2c_driver = {
	.probe = bkl_probe,
	.remove = bkl_remove,
	.id_table = bkl_id,
	.driver = {
		.name = bkl_NAME,
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};
//late_initcall(bias_i2c_driver);
module_i2c_driver(bias_i2c_driver);
MODULE_DESCRIPTION("Lcm bias driver for bias");
MODULE_LICENSE("GPL v2");