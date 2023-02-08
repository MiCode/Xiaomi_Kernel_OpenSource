/* code at 2022/08/1 start */
/*
 * bf30a2.c  bf30a2 yuv module
 *
 * Author:  <wuzhenyue@huaqin.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>
#include "bf30a2_ofilm_ii.h"
extern void ISP_MCLK3_EN (bool En);
/*****************************************************************
* bf30a2 marco
******************************************************************/
#define BF30A2_DRIVER_VERSION	"V8_1"
#define BF30A2_PRODUCT_NUM		4
#define BF30A2_PRODUCT_NAME_LEN	8
#define BF30A2_SENSOR_ID   0x3b
#define BF30A2_MCLK_ON   "bf30a2_mclk_on"
#define BF30A2_MCLK_OFF   "bf30a2_mclk_off"
static struct pinctrl_state *bf30a2_mclk_on;
static struct pinctrl_state *bf30a2_mclk_off;
/*****************************************************************
* bf30a2 global global variable
******************************************************************/
static unsigned char read_reg_id = 0;
static unsigned char read_reg_value = 0;
static int read_reg_flag = 0;
static int driver_flag = 0;
static struct bf30a2 *g_bf30a2 = NULL;
/**********************************************************
* i2c write and read
**********************************************************/
static int bf30a2_i2c_write(struct bf30a2 *bf30a2,
			     unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	while (cnt < BF30A2_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(bf30a2->i2c_client,
						reg_addr, reg_data);
		if (ret < 0) {
			qvga_dev_err(bf30a2->dev,"%s: bf30afi2c_write cnt=%d error=%d\n", __func__,cnt, ret);
		} else {
			break;
		}
		cnt++;
		msleep(BF30A2_I2C_RETRY_DELAY);
	}
	return ret;
}
static int bf30a2_i2c_read(struct bf30a2 *bf30a2,
			    unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	while (cnt < BF30A2_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(bf30a2->i2c_client, reg_addr);
		if (ret < 0) {
			qvga_dev_err(bf30a2->dev,"%s: bf30afi2c_read cnt=%d error=%d\n", __func__,cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(BF30A2_I2C_RETRY_DELAY);
	}
	return ret;
}
static struct bf30a2 *bf30a2_malloc_init(struct i2c_client *client)
{
	struct bf30a2 *bf30a2 =
	    devm_kzalloc(&client->dev, sizeof(struct bf30a2), GFP_KERNEL);
	if (bf30a2 == NULL) {
		dev_err(&client->dev, "%s: bf30afdevm_kzalloc failed.\n", __func__);
		return NULL;
	}
	bf30a2->i2c_client = client;
	pr_info("%s bf30afenter , client_addr = 0x%02x\n", __func__,bf30a2->i2c_client->addr);
	return bf30a2;
}
#if 1
static  void bf30a2_Init(struct bf30a2 *bf30a2)
{
    /*SYS*/
	bf30a2_i2c_write(bf30a2, 0xf2, 0x01);
	bf30a2_i2c_write(bf30a2, 0x12, 0x20);
	bf30a2_i2c_write(bf30a2, 0x15, 0x80);
	bf30a2_i2c_write(bf30a2, 0x6b, 0x71);
	bf30a2_i2c_write(bf30a2, 0x04, 0x00);
	bf30a2_i2c_write(bf30a2, 0x06, 0x26);
	bf30a2_i2c_write(bf30a2, 0x08, 0x07);
	bf30a2_i2c_write(bf30a2, 0x1c, 0x12);
	bf30a2_i2c_write(bf30a2, 0x1e, 0x26);
	bf30a2_i2c_write(bf30a2, 0x1f, 0x01);
	bf30a2_i2c_write(bf30a2, 0x20, 0x20);
	bf30a2_i2c_write(bf30a2, 0x21, 0x20);
	bf30a2_i2c_write(bf30a2, 0x34, 0x00);
	bf30a2_i2c_write(bf30a2, 0x35, 0x00);
	bf30a2_i2c_write(bf30a2, 0x36, 0x21);
	bf30a2_i2c_write(bf30a2, 0x37, 0x13);
	bf30a2_i2c_write(bf30a2, 0xca, 0x03);
	bf30a2_i2c_write(bf30a2, 0xcb, 0x22);
	bf30a2_i2c_write(bf30a2, 0xcc, 0x89);
	bf30a2_i2c_write(bf30a2, 0xcd, 0x6c);
	bf30a2_i2c_write(bf30a2, 0xce, 0x6b);
	bf30a2_i2c_write(bf30a2, 0xcf, 0xf0);
	bf30a2_i2c_write(bf30a2, 0xa0, 0x8e);
	bf30a2_i2c_write(bf30a2, 0x01, 0x1b);
	bf30a2_i2c_write(bf30a2, 0x02, 0x1d);
	bf30a2_i2c_write(bf30a2, 0x13, 0x48);
	bf30a2_i2c_write(bf30a2, 0x87, 0x20);
	bf30a2_i2c_write(bf30a2, 0x8a, 0x33);
	bf30a2_i2c_write(bf30a2, 0x8b, 0x70);
	bf30a2_i2c_write(bf30a2, 0x70, 0x1f);
	bf30a2_i2c_write(bf30a2, 0x71, 0x40);
	bf30a2_i2c_write(bf30a2, 0x72, 0x0a);
	bf30a2_i2c_write(bf30a2, 0x73, 0x62);
	bf30a2_i2c_write(bf30a2, 0x74, 0xa2);
	bf30a2_i2c_write(bf30a2, 0x75, 0xbf);
	bf30a2_i2c_write(bf30a2, 0x76, 0x02);
	bf30a2_i2c_write(bf30a2, 0x77, 0xcc);
	bf30a2_i2c_write(bf30a2, 0x40, 0x32);
	bf30a2_i2c_write(bf30a2, 0x41, 0x28);
	bf30a2_i2c_write(bf30a2, 0x42, 0x26);
	bf30a2_i2c_write(bf30a2, 0x43, 0x1d);
	bf30a2_i2c_write(bf30a2, 0x44, 0x1a);
	bf30a2_i2c_write(bf30a2, 0x45, 0x14);
	bf30a2_i2c_write(bf30a2, 0x46, 0x11);
	bf30a2_i2c_write(bf30a2, 0x47, 0x0f);
	bf30a2_i2c_write(bf30a2, 0x48, 0x0e);
	bf30a2_i2c_write(bf30a2, 0x49, 0x0d); 
	bf30a2_i2c_write(bf30a2, 0x4b, 0x0c);
	bf30a2_i2c_write(bf30a2, 0x4c, 0x0b);
	bf30a2_i2c_write(bf30a2, 0x4e, 0x0a);
	bf30a2_i2c_write(bf30a2, 0x4f, 0x09);
	bf30a2_i2c_write(bf30a2, 0x50, 0x09);
	bf30a2_i2c_write(bf30a2, 0x24, 0x50);
	bf30a2_i2c_write(bf30a2, 0x25, 0x36);
	bf30a2_i2c_write(bf30a2, 0x80, 0x00);
	bf30a2_i2c_write(bf30a2, 0x81, 0x20);
	bf30a2_i2c_write(bf30a2, 0x82, 0x40);
	bf30a2_i2c_write(bf30a2, 0x83, 0x30);
	bf30a2_i2c_write(bf30a2, 0x84, 0x50);
	bf30a2_i2c_write(bf30a2, 0x85, 0x30);
	bf30a2_i2c_write(bf30a2, 0x86, 0xd8);
	bf30a2_i2c_write(bf30a2, 0x89, 0x45);
	bf30a2_i2c_write(bf30a2, 0x8f, 0x81);
	bf30a2_i2c_write(bf30a2, 0x91, 0xff);
	bf30a2_i2c_write(bf30a2, 0x92, 0x08);
	bf30a2_i2c_write(bf30a2, 0x94, 0x82);
	bf30a2_i2c_write(bf30a2, 0x95, 0xfd);
	bf30a2_i2c_write(bf30a2, 0x9a, 0x20);
	bf30a2_i2c_write(bf30a2, 0x9e, 0xbc);
	bf30a2_i2c_write(bf30a2, 0xf0, 0x8f);
	bf30a2_i2c_write(bf30a2, 0xf1, 0x02);
	bf30a2_i2c_write(bf30a2, 0x51, 0x06);
	bf30a2_i2c_write(bf30a2, 0x52, 0x25);
	bf30a2_i2c_write(bf30a2, 0x53, 0x2b);
	bf30a2_i2c_write(bf30a2, 0x54, 0x0f);
	bf30a2_i2c_write(bf30a2, 0x57, 0x2a);
	bf30a2_i2c_write(bf30a2, 0x58, 0x22);
	bf30a2_i2c_write(bf30a2, 0x59, 0x2c);
	bf30a2_i2c_write(bf30a2, 0x23, 0x33);
	bf30a2_i2c_write(bf30a2, 0xa0, 0x8f);
	bf30a2_i2c_write(bf30a2, 0xa1, 0x13);
	bf30a2_i2c_write(bf30a2, 0xa2, 0x0f);
	bf30a2_i2c_write(bf30a2, 0xa3, 0x2a);
	bf30a2_i2c_write(bf30a2, 0xa4, 0x08);
	bf30a2_i2c_write(bf30a2, 0xa5, 0x26);
	bf30a2_i2c_write(bf30a2, 0xa7, 0x80);
	bf30a2_i2c_write(bf30a2, 0xa8, 0x80);
	bf30a2_i2c_write(bf30a2, 0xa9, 0x1e);
	bf30a2_i2c_write(bf30a2, 0xaa, 0x19);
	bf30a2_i2c_write(bf30a2, 0xab, 0x18);
	bf30a2_i2c_write(bf30a2, 0xae, 0x50);
	bf30a2_i2c_write(bf30a2, 0xaf, 0x04);
	bf30a2_i2c_write(bf30a2, 0xc8, 0x10);
	bf30a2_i2c_write(bf30a2, 0xc9, 0x15);
	bf30a2_i2c_write(bf30a2, 0xd3, 0x0c);
	bf30a2_i2c_write(bf30a2, 0xd4, 0x16);
	bf30a2_i2c_write(bf30a2, 0xee, 0x06);
	bf30a2_i2c_write(bf30a2, 0xef, 0x04);
	bf30a2_i2c_write(bf30a2, 0x55, 0x30);
	bf30a2_i2c_write(bf30a2, 0x56, 0x9c);
	bf30a2_i2c_write(bf30a2, 0xb1, 0x98);
	bf30a2_i2c_write(bf30a2, 0xb2, 0x98);
	bf30a2_i2c_write(bf30a2, 0xb3, 0xc4);
	bf30a2_i2c_write(bf30a2, 0xb4, 0x0c);
	bf30a2_i2c_write(bf30a2, 0x00, 0x40);
/* code at 2022/08/12 start */
	bf30a2_i2c_write(bf30a2, 0x13, 0x48);
/* code at 2022/08/12 end */
}   /*    sensor_init  */
#endif
static int bf30a2_GetSensorID(struct bf30a2 *bf30a2)
{
    int retry = 5;
    int len;
    unsigned char reg_data = 0x00;
    //check if sensor ID correct
    do {
        len = bf30a2_i2c_read(bf30a2, 0xfc, &reg_data);
    if (reg_data == BF30A2_SENSOR_ID) {
        qvga_dev_info(bf30a2->dev, "%s: Read Sensor ID sucess = 0x%02x\n", __func__, reg_data);
        driver_flag = 1;
        return 0;
    } else {
        qvga_dev_err(bf30a2->dev, "%s: Read Sensor ID Fail = 0x%02x\n", __func__, reg_data);
        driver_flag = 0;
    }
        retry--;
		pr_info("%s bf30a2 get sensorid retry %d time\n", __func__, retry);
    } while (retry > 0);
    return -1;
}
// define AVDD
static void bf30a2_avdd_control(struct bf30a2 *bf30a2, bool flag)
{
	struct regulator *vcama;
	qvga_dev_info(bf30a2->dev, "%senter\n", __func__);
	vcama = regulator_get(bf30a2->dev,"vcama");
	if (IS_ERR(vcama)) {
		qvga_dev_err(bf30a2->dev, "%s AVDD get regulator failed\n", __func__);
		regulator_put(vcama);
		return;
	}
	if (flag) {
		regulator_set_voltage(vcama, 2800000, 2800000);
		regulator_enable(vcama);
	} else {
		regulator_disable(vcama);
	}
	return;
}
// define  IOVDD
static void bf30a2_iovdd_control(struct bf30a2 *bf30a2, bool flag)
{
	struct regulator *vcamio;
	qvga_dev_info(bf30a2->dev, "%senter\n", __func__);
	vcamio = regulator_get(bf30a2->dev,"vcamio");
	if (IS_ERR(vcamio)) {
		qvga_dev_err(bf30a2->dev, "%s IOVDD get regulator  failed\n", __func__);
		regulator_put(vcamio);
		return;
	}
	if (flag) {
		regulator_set_voltage(vcamio, 1800000, 1800000);
		regulator_enable(vcamio);
	} else {
		regulator_disable(vcamio);
	}
        return;
}
static void bf30a2_hw_on_reset(struct bf30a2 *bf30a2)
{
	qvga_dev_info(bf30a2->dev, "%s enter\n", __func__);
	if (gpio_is_valid(bf30a2->reset_gpio)) {
		gpio_set_value_cansleep(bf30a2->reset_gpio, 0);
		udelay(50);
		gpio_set_value_cansleep(bf30a2->reset_gpio, 1);
		udelay(100);
		gpio_set_value_cansleep(bf30a2->reset_gpio, 0);
	}
}
static void bf30a2_hw_off_reset(struct bf30a2 *bf30a2)
{
	qvga_dev_info(bf30a2->dev, "%s enter\n", __func__);
	if (gpio_is_valid(bf30a2->reset_gpio)) {
		gpio_set_value_cansleep(bf30a2->reset_gpio, 0);
		udelay(50);
		gpio_set_value_cansleep(bf30a2->reset_gpio, 1);
		udelay(50);
		gpio_set_value_cansleep(bf30a2->reset_gpio, 0);
	}
}
static void bf30a2_hw_on(struct bf30a2 *bf30a2)
{
	int ret;
	ret = pinctrl_select_state(bf30a2->bf30a2_pinctrl, bf30a2_mclk_on);
	if (ret < 0) {
		qvga_dev_err(g_bf30a2->dev, "%s: pinctrl select failed for mclk on\n", __func__);
	}
	bf30a2_iovdd_control(bf30a2,true);
	bf30a2_avdd_control(bf30a2,true);
	udelay(100);
	bf30a2_hw_on_reset(bf30a2);
	udelay(10);
	bf30a2_GetSensorID(bf30a2);
	bf30a2_Init(bf30a2);
	bf30a2->hwen_flag = 1;
}
static void bf30a2_hw_off(struct bf30a2 *bf30a2)
{
	int ret;
	bf30a2_hw_off_reset(bf30a2);
	udelay(50);
	bf30a2_avdd_control(bf30a2,false);
	bf30a2_iovdd_control(bf30a2,false);
	ret = pinctrl_select_state(bf30a2->bf30a2_pinctrl, bf30a2_mclk_off);
	if (ret < 0) {
		qvga_dev_err(g_bf30a2->dev, "%s: pinctrl select failed for mclk off\n", __func__);
	}
	bf30a2->hwen_flag = 0;
}
static ssize_t bf30a2_get_reg(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	if (read_reg_flag) {
		len += snprintf(buf + len, PAGE_SIZE - len, "The reg 0x%02X value is 0x%02X\n",read_reg_id, read_reg_value);
		read_reg_flag = 0;
		read_reg_id = 0;
		read_reg_value = 0;
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "Please echo reg id into reg\n");
	}
	return len;
}
static ssize_t bf30a2_set_reg(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	unsigned int databuf[2] = { 0 };
	unsigned char reg_data = 0x00;
	int length;
	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		bf30a2_i2c_write(g_bf30a2, databuf[0], databuf[1]);
	}
	else if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 1) {
		length = bf30a2_i2c_read(g_bf30a2, databuf[0], &reg_data);
		read_reg_id = databuf[0];
		read_reg_value = reg_data;
		read_reg_flag = 1;
	}
	return len;
}
static ssize_t bf30a2_get_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	if (driver_flag) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n","bf30a2_ofilm_ii");
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n","none");
	}
	return len;
}
static ssize_t bf30a2_get_light(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
/* code at 2022/09/1 start */
	unsigned char reg_data = 0xff;
/* code at 2022/09/1 end */
	int length;
	length = bf30a2_i2c_read(g_bf30a2, 0x88, &reg_data);
	len += snprintf(buf + len, PAGE_SIZE - len, "light:%d  \n",reg_data);
	pr_info("%s start !!!!",__func__);
	return len;
}
static ssize_t bf30a2_set_light(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	ssize_t ret;
	unsigned int state;
	ret = kstrtouint(buf, 10, &state);
    pr_info(" %s start !!! \n", __func__);
	if (ret) {
		qvga_dev_err(g_bf30a2->dev, "%s: fail to change str to int\n",
			   __func__);
		return ret;
	}
	if (state == 0)
		{
		bf30a2_hw_off(g_bf30a2); /*OFF*/
            pr_err(" %s failed, light_hw_off\n", __func__);
        }
	else
		{
		bf30a2_hw_on(g_bf30a2); /*ON*/
            pr_err(" %s sucess ,light_hw_on\n", __func__);
         } /*ON*/
	return len;
}
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO,
		bf30a2_get_reg, bf30a2_set_reg);
static DEVICE_ATTR(cam_name, S_IWUSR | S_IRUGO,
		bf30a2_get_name, NULL);
static DEVICE_ATTR(light, S_IWUSR | S_IRUGO,
		bf30a2_get_light, bf30a2_set_light);
static struct attribute *bf30a2_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_cam_name.attr,
	&dev_attr_light.attr,
	NULL
};
static struct attribute_group bf30a2_attribute_group = {
	.attrs = bf30a2_attributes
};
static void bf30a2_parse_gpio_dt(struct bf30a2 *bf30a2,
					struct device_node *np)
{
	qvga_dev_info(bf30a2->dev, "%s bf30afenter, dev_i2c%d@0x%02X\n", __func__,
			bf30a2->i2c_seq, bf30a2->i2c_addr);
	bf30a2->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (bf30a2->reset_gpio < 0) {
		qvga_dev_err(bf30a2->dev,
			   "%s: bf30afno reset gpio provided, hardware reset unavailable\n",
			__func__);
		bf30a2->reset_gpio = -1;
	} else {
		qvga_dev_info(bf30a2->dev, "%s: bf30afreset gpio provided ok\n",
			 __func__);
	}
}
static void bf30a2_parse_dt(struct bf30a2 *bf30a2, struct device_node *np)
{
	qvga_dev_info(bf30a2->dev, "%s enter, bf30afdev_i2c%d@0x%02X\n", __func__,
		    bf30a2->i2c_seq, bf30a2->i2c_addr);
	bf30a2_parse_gpio_dt(bf30a2, np);
}
/****************************************************************************
* bf30a2 i2c driver
*****************************************************************************/
static int bf30a2_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct pinctrl_state *set_state;
	struct bf30a2 *bf30a2 = NULL;
	struct class *qvga_class;
	struct device *dev;
	int ret = -1;
	pr_info("%s enter , %d@0x%02x\n", __func__,client->adapter->nr, client->addr);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		qvga_dev_err(&client->dev, "%s: bf30afcheck_functionality failed\n",__func__);
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}
	bf30a2 = bf30a2_malloc_init(client);
	g_bf30a2 = bf30a2;
	bf30a2->i2c_seq = bf30a2->i2c_client->adapter->nr;
	bf30a2->i2c_addr = bf30a2->i2c_client->addr;
	if (bf30a2 == NULL) {
		dev_err(&client->dev, "%s: failed to parse device tree node\n",__func__);
		ret = -ENOMEM;
		goto exit_devm_kzalloc_failed;
	}
	bf30a2->dev = &client->dev;
	i2c_set_clientdata(client, bf30a2);
	bf30a2_parse_dt(bf30a2, np);
	if (gpio_is_valid(bf30a2->reset_gpio)) {
		ret = devm_gpio_request_one(&client->dev,
					    bf30a2->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "bf30a2_rst");//byd
		if (ret) {
			qvga_dev_err(&client->dev,
				   "%s: bf30afrst request failed\n", __func__);
			goto exit_gpio_request_failed;
		}
	}
	bf30a2->bf30a2_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(bf30a2->bf30a2_pinctrl)) {
		qvga_dev_err(&client->dev, "%s: bf30afbf30a2_pinctrl not defined\n", __func__);
	} else {
		set_state = pinctrl_lookup_state(bf30a2->bf30a2_pinctrl, BF30A2_MCLK_ON);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: pinctrl lookup failed for mclk on\n", __func__);
		} else {
			bf30a2_mclk_on = set_state;
		}
		set_state = pinctrl_lookup_state(bf30a2->bf30a2_pinctrl, BF30A2_MCLK_OFF);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: pinctrl lookup failed for mclk off\n", __func__);
		} else {
			bf30a2_mclk_off = set_state;
		}
		//ret = pinctrl_select_state(bf30a2_pinctrl, bf30a2_mclk_off);
		//if (ret < 0) {
		//	qvga_dev_err(&client->dev, "%s: bf30afbf30a2_pinctrl select failed for mclk off\n", __func__);
	}
	//power on camera
	ret = pinctrl_select_state(bf30a2->bf30a2_pinctrl, bf30a2_mclk_on);
	if (ret < 0) {
		qvga_dev_err(g_bf30a2->dev, "%s: pinctrl select failed for mclk on\n", __func__);
	}
	bf30a2_iovdd_control(bf30a2,true);
	bf30a2_avdd_control(bf30a2,true);
	udelay(100);
	bf30a2_hw_on_reset(bf30a2);
	udelay(10);
	bf30a2->hwen_flag = 1;
	/* bf30a2 sensor id */
	ret = bf30a2_GetSensorID(bf30a2);
	if (ret < 0) {
		qvga_dev_err(&client->dev,"%s: read_sensorid failed ret=%d\n", __func__,ret);
		goto exit_i2c_check_id_failed;
	}
pr_err("%s: sensorid=0x%d\n", __func__,ret);
//power off camera
	bf30a2_hw_off_reset(bf30a2);
	udelay(50);
	bf30a2_avdd_control(bf30a2,false);
	bf30a2_iovdd_control(bf30a2,false);
	ret = pinctrl_select_state(bf30a2->bf30a2_pinctrl, bf30a2_mclk_off);
	if (ret < 0) {
		qvga_dev_err(g_bf30a2->dev, "%s: pinctrl select failed for mclk off\n", __func__);
	}
	bf30a2->hwen_flag = 0;
//create device
	qvga_class = class_create(THIS_MODULE, "qvga_cam");
	dev = device_create(qvga_class, NULL, client->dev.devt, NULL, "qvga_depth");
	ret = sysfs_create_group(&dev->kobj, &bf30a2_attribute_group);
	if (ret < 0) {
		qvga_dev_err(&client->dev,"%s failed to create sysfs nodes\n", __func__);
	}
	return 0;
 exit_i2c_check_id_failed:
	bf30a2_avdd_control(bf30a2,false);
	bf30a2_iovdd_control(bf30a2,false);
        if (gpio_is_valid(bf30a2->reset_gpio))
                devm_gpio_free(&client->dev, bf30a2->reset_gpio);
 exit_gpio_request_failed:
	devm_kfree(&client->dev, bf30a2);
	bf30a2 = NULL;
 exit_devm_kzalloc_failed:
 exit_check_functionality_failed:
	return ret;
}
static int bf30a2_i2c_remove(struct i2c_client *client)
{
	struct bf30a2 *bf30a2 = i2c_get_clientdata(client);
	if (gpio_is_valid(bf30a2->reset_gpio))
		devm_gpio_free(&client->dev, bf30a2->reset_gpio);
	devm_kfree(&client->dev, bf30a2);
	bf30a2 = NULL;
	return 0;
}
static const struct of_device_id bf30a2_of_match[] = {
	{.compatible = "byd,bf30a2_yuv"},
	{},
};
static struct i2c_driver bf30a2_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name =  "bf30a2_yuv",
		   .of_match_table = bf30a2_of_match,
		   },
	.probe = bf30a2_i2c_probe,
	.remove = bf30a2_i2c_remove,
};
static int __init bf30a2_yuv_init(void)
{
	int ret;
	pr_info("%s: bf30afdriver version: %s\n", __func__,BF30A2_DRIVER_VERSION);
	ret = i2c_add_driver(&bf30a2_i2c_driver);
	if (ret) {
		pr_err("[%s] Unable to register driver (%d)\n",__func__, ret);
		return ret;
	}
	return 0;
}
static void __exit bf30a2_yuv_exit(void)
{
	pr_info("%s enter\n", __func__);
	i2c_del_driver(&bf30a2_i2c_driver);
}
module_init(bf30a2_yuv_init);
module_exit(bf30a2_yuv_exit);
MODULE_AUTHOR("wuzhenyue@huaqin.com>");
MODULE_DESCRIPTION("bf30a2 yuv driver");
MODULE_LICENSE("GPL v2");
/* code at 2022/08/1 end */