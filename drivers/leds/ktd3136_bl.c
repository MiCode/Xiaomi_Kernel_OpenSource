/*
 * Copyright (C) 2011 ST-Ericsson SA.
 * Copyright (C) 2009 Motorola, Inc.
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for National Semiconductor sgm Backlight driver chip
 *
 * Author: Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>
 * based on leds-sgm.c by Dan Murphy <D.Murphy@motorola.com>
 */

#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
//#include <linux/led-sgm.h>
#include <linux/types.h>

#include <linux/module.h>
#include "ktd3136.h"
#include "ktd_reg_04.h"
#include "ktd_reg_05.h"
#include "ti_reg_22.h"
#include "ti_reg_23.h"


#define SGM_NAME "ktd3136"

int g_backlight_ic = 0;// 1 -->ktd3137, 2 -->lm3697

struct ktd3137_chip *bkl_chip;
int  i2c_flag = 0;
int i2c_sgm_write(struct i2c_client *client, uint8_t command, uint8_t data);
#define HBM_MODE_DEFAULT 0
#define	HBM_MODE_LEVEL1 1
#define	HBM_MODE_LEVEL2 2
#define	HBM_MODE_LEVEL3 3
/**
 * struct sgm_data
 * @led_dev: led class device
 * @client: i2c client
 * @pdata: sgm platform data
 * @mode: mode of operation - manual, ALS, PWM
 * @regulator: regulator
 * @brighness: previous brightness value
 * @enable: regulator is enabled
 */
/*
struct sgm_data {
	struct led_classdev led_dev;
	struct i2c_client *client;
	struct sgm_platform_data *pdata;
	uint16_t brightness;
	bool enable;
};
*/
#define MAX_BRIGHTNESS 2047

/*
int ktd3137_brightness_table_reg4[256] = {0x01, 0x02, 0x04, 0x04, 0x07,
	0x02, 0x00, 0x06, 0x04, 0x02, 0x03, 0x04, 0x05, 0x06, 0x02,
	0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x04, 0x05,
	0x06, 0x05, 0x03, 0x00, 0x05, 0x02, 0x06, 0x02, 0x06, 0x02,
	0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02,
	0x06, 0x01, 0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x06, 0x01,
	0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x05, 0x07, 0x01, 0x03,
	0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02,
	0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x07,
	0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x07, 0x05, 0x03, 0x01,
	0x07, 0x05, 0x03, 0x01, 0x07, 0x05, 0x03, 0x01, 0x07, 0x05,
	0x03, 0x00, 0x05, 0x02, 0x07, 0x04, 0x01, 0x06, 0x03, 0x00,
	0x05, 0x02, 0x07, 0x04, 0x01, 0x06, 0x03, 0x00, 0x05, 0x02,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x06, 0x01,
	0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x06, 0x01, 0x04, 0x07,
	0x02, 0x05, 0x00, 0x03, 0x06, 0x01, 0x04, 0x07, 0x02, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01,
	0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01,
	0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x04, 0x05, 0x06,
	0x07};
int ktd3137_brightness_table_reg5[256] = {0x00, 0x06, 0x0C, 0x11, 0x15,
	0x1A, 0x1E, 0x21, 0x25, 0x29, 0x2C, 0x2F, 0x32, 0x35, 0x38, 0x3A,
	0x3D, 0x3F, 0x42, 0x44, 0x47, 0x49, 0x4C, 0x4E, 0x50, 0x52, 0x54,
	0x56, 0x58, 0x59, 0x5B, 0x5C, 0x5E, 0x5F, 0x61, 0x62, 0x64, 0x65,
	0x67, 0x68, 0x6A, 0x6B, 0x6D, 0x6E, 0x70, 0x71, 0x73, 0x74, 0x75,
	0x77, 0x78, 0x7A, 0x7B, 0x7C, 0x7E, 0x7F, 0x80, 0x82, 0x83, 0x85,
	0x86, 0x87, 0x88, 0x8A, 0x8B, 0x8C, 0x8D, 0x8F, 0x90, 0x91, 0x92,
	0x94, 0x95, 0x96, 0x97, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xAA, 0xAB, 0xAC,
	0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
	0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC0,
	0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC9, 0xC9,
	0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCF, 0xCF, 0xD0, 0xD1, 0xD2,
	0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD5, 0xD6, 0xD7, 0xD7, 0xD8, 0xD8,
	0xD9, 0xDA, 0xDA, 0xDB, 0xDC, 0xDC, 0xDD, 0xDD, 0xDE, 0xDE, 0xDF,
	0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE2, 0xE2, 0xE3, 0xE3, 0xE4, 0xE4, 0xE5,
	0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE8, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEB,
	0xEB, 0xEC, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEE, 0xEF, 0xEF, 0xEF,
	0xF0, 0xF0, 0xF1, 0xF1, 0xF1, 0xF2, 0xF2, 0xF2, 0xF3, 0xF3, 0xF3, 0xF4,
	0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0xF7,
	0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xFA,
	0xFA, 0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD,
	0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF};
*/
//struct sgm_data *sgm_data;


static int ktd3137_read_reg(struct i2c_client *client, int reg, u8 *val)
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
static int ktd3137_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int ktd3137_masked_write(struct i2c_client *client,
					int reg, u8 mask, u8 val)
{
	int rc;
	u8 temp = 0;

	rc = ktd3137_read_reg(client, reg, &temp);
	if (rc < 0) {
		dev_err(&client->dev, "failed to read reg\n");
	} else {
		temp &= ~mask;
		temp |= val & mask;
		rc = ktd3137_write_reg(client, reg, temp);
		if (rc < 0)
			dev_err(&client->dev, "failed to write masked data\n");
	}

	//ktd3137_read_reg(client, reg, &temp);
	return rc;
}
int i2c_sgm_write(struct i2c_client *client, uint8_t command, uint8_t data)
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
		printk("%s: i2c_write_block retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}
	return 0;

}
static int sgm_init_registers(void)
{
	int ret = 0;
	u8 rbuf = 2;

	ret = ktd3137_read_reg(bkl_chip->client,0x00, &rbuf);
	pr_info("%s: 0x00 reg = 0x%x, ret = %d\n",__func__, rbuf, ret);
	if(0x18 == rbuf)
	{
		pr_info("backlight ic is ktd3137 !\n");
		g_backlight_ic = 1;
		ret = i2c_sgm_write(bkl_chip->client,0x06, 0x1B);
		ret = i2c_sgm_write(bkl_chip->client,0x02, 0xc9);
	}else{
		pr_info("backlight ic is lm3697 !\n");
		g_backlight_ic = 2;
		ret = i2c_sgm_write(bkl_chip->client,0x10, 0x03);
		ret = i2c_sgm_write(bkl_chip->client,0x13, 0x01);
		ret = i2c_sgm_write(bkl_chip->client,0x16, 0x00);
		ret = i2c_sgm_write(bkl_chip->client,0x17, 0x19);
		ret = i2c_sgm_write(bkl_chip->client,0x18, 0x19);
		ret = i2c_sgm_write(bkl_chip->client,0x19, 0x03);
		ret = i2c_sgm_write(bkl_chip->client,0x1A, 0x0C);
		ret = i2c_sgm_write(bkl_chip->client,0x1C, 0x0f);
		ret = i2c_sgm_write(bkl_chip->client,0x22, 0x07);
		ret = i2c_sgm_write(bkl_chip->client,0x23, 0xff);
		ret = i2c_sgm_write(bkl_chip->client,0x24, 0x02);
	}

	return ret;
}


int lm3697_brightness_set(uint16_t brightness)
{
	int err;

	if (brightness > MAX_BRIGHTNESS)
		brightness = MAX_BRIGHTNESS;
	
	if(brightness > 0){
		err = i2c_sgm_write(bkl_chip->client, 0x22, lm3697_brightness_table_reg22[brightness]);
		err = i2c_sgm_write(bkl_chip->client, 0x23, lm3697_brightness_table_reg23[brightness]);
	}else{
		err = i2c_sgm_write(bkl_chip->client, 0x22, 0x00);
		err = i2c_sgm_write(bkl_chip->client, 0x23, 0x00);
	}
	if(err < 0)
		pr_info("lm3697 set Backlight fail !\n");
	return err;

}
void ktd3137_brightness_set_workfunc(struct ktd3137_chip *chip, int brightness)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;

	if (brightness > pdata->max_brightness)
		brightness = pdata->max_brightness;

	if((pdata->prev_bl_current == 0)&&(brightness != 0)){
		if (pdata->linear_backlight == 1) {
			ktd3137_masked_write(chip->client, REG_CONTROL, 0x02, 0x02);// set linear mode
		}else{
			ktd3137_masked_write(chip->client, REG_CONTROL, 0x02, 0x00);// set exponetial mode
		}
	}
	
	if (brightness == 0) {
		ktd3137_write_reg(chip->client, REG_MODE, 0x98);
	} else {
		ktd3137_write_reg(chip->client, REG_MODE, 0xC9);

	if (pdata->using_lsb) {
		ktd3137_masked_write(chip->client, REG_RATIO_LSB,
						0x07, brightness);
		ktd3137_masked_write(chip->client, REG_RATIO_MSB,
						0xff, brightness>>3);
	} else {
		ktd3137_masked_write(chip->client, REG_RATIO_LSB, 0x07,
			ktd3137_brightness_table_reg4[brightness]);
		ktd3137_masked_write(chip->client, REG_RATIO_MSB, 0xff,
			ktd3137_brightness_table_reg5[brightness]);
		}
	}

	pdata->prev_bl_current = brightness;

}
int sgm_brightness_set(uint16_t brightness)
{
	pr_info("[brightness]%s brightness = %d\n", __func__, brightness);
	if(g_backlight_ic == 1)
		ktd3137_brightness_set_workfunc(bkl_chip, brightness);
	else
		lm3697_brightness_set(brightness);	
	return 0;
}
EXPORT_SYMBOL_GPL(sgm_brightness_set);
int ktd_hbm_set(int hbm_mode)
{
	switch (hbm_mode) {
	case HBM_MODE_DEFAULT:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0x81);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		pr_err("Turn off  hbm mode \n");
		break;
	case HBM_MODE_LEVEL1:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0x99);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		pr_err("This is hbm mode 1\n");
		break;
	case HBM_MODE_LEVEL2:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xB1);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		pr_err("This is hbm mode 2\n");
		break;
	case HBM_MODE_LEVEL3:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xC9);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		pr_err("This is hbm mode 3\n");
		break;
	default:
		pr_info("This isn't hbm mode\n");
		break;
	 }

	return 0;
}
int lm_hbm_set(int hbm_mode)
{

	switch (hbm_mode) {
	case HBM_MODE_DEFAULT:
		i2c_sgm_write(bkl_chip->client,0x18, 0x10);
		pr_info("Turn off  hbm mode \n");
		break;
	case HBM_MODE_LEVEL1:
		i2c_sgm_write(bkl_chip->client,0x18, 0x13);
		pr_info("This is hbm mode 1\n");
		break;
	case HBM_MODE_LEVEL2:
		i2c_sgm_write(bkl_chip->client,0x18, 0x16);
		pr_info("This is hbm mode 2\n");
		break;
	case HBM_MODE_LEVEL3:
		i2c_sgm_write(bkl_chip->client,0x18, 0x19);
		pr_info("This is hbm mode 3\n");
		break;
	default:
		pr_info("This isn't hbm mode\n");
		break;
	 }

	return  0;
}
int backlight_hbm_set(int hbm_mode)
{
	pr_info("%s hbm mode = %d\n", __func__, hbm_mode);
	if(g_backlight_ic == 1)
		ktd_hbm_set(hbm_mode);
	else
		lm_hbm_set(hbm_mode);
	return 0;
}
EXPORT_SYMBOL_GPL(backlight_hbm_set);
static ssize_t sgm_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	sprintf(buf, "%d\n", i2c_flag);

	return 6;
}

static ssize_t sgm_brightness_store(struct device *dev, struct device_attribute
				   *attr, const char *buf, size_t size)
{

	int len;
	char buf_bri[6];
	int  brightness = 0;

	len = (int)strlen(buf);
	if (len > 5)
		return -1;
	memcpy(buf_bri, buf, len);
	buf_bri[len] = '\0';
	sscanf(buf_bri, "%d", &brightness);

	sgm_brightness_set(brightness);
   
	return size;
}
static DEVICE_ATTR(brightness_show, 0644, sgm_brightness_show, sgm_brightness_store);

static struct attribute *sgm_attrs[] = {
	&dev_attr_brightness_show.attr,
	NULL
};
static struct attribute_group sgm_attribute_group = {
	.attrs = sgm_attrs
};
//ATTRIBUTE_GROUPS(sgm);

static int sgm_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int err = 0;

	struct ktd3137_bl_pdata *pdata = dev_get_drvdata(&client->dev);
	struct ktd3137_chip *chip;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C_FUNC_I2C not supported\n");
		return -EIO;
	}

	client->addr = 0x36;
	pr_err("probe start! 1 \n");
	chip = devm_kzalloc(&client->dev, sizeof(struct ktd3137_chip),
				GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;
	 
	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata){
		pr_err("probe start! 2 \n");
		return -1;// -ENOMEM;
	}
	 pdata->linear_backlight = 1;
	 pdata->default_brightness = 0x7ff;
	 pdata->max_brightness = 2047;

	chip->client = client;
	chip->pdata = pdata;
	chip->dev = &client->dev;

	bkl_chip = chip;
	i2c_set_clientdata(client, chip);
	err = sysfs_create_group(&client->dev.kobj, &sgm_attribute_group);
	
	err =sgm_init_registers();
	pr_err("ktd3136 probe err = %d\n", err);
	if(err == 0)
		i2c_flag = 1;
	else 
		i2c_flag = 0;
	return 0;
}

static const struct i2c_device_id sgm_id[] = {
	{SGM_NAME, 0},
	{}
};
static const struct of_device_id lm3697_bl_of_match[] = {
	{ .compatible = "ktd,ktd3136", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sgm_id);

static struct i2c_driver sgm_i2c_driver = {
	.probe = sgm_probe,
	.id_table = sgm_id,
	.driver = {
		.name = SGM_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3697_bl_of_match,
	},
};

module_i2c_driver(sgm_i2c_driver);

MODULE_DESCRIPTION("Back Light driver for sgm");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>");
