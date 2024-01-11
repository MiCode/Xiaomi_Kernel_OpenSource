/*
 * sc080cs.c  sc080cs yuv module
 *
 * Author: Bruce <sunchengwei@longcheer.com>
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
#include "sc_sc080cs_ii.h"
#include <linux/videodev2.h>
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include "imgsensor_cfg_table.h"
#include "imgsensor_platform.h"
extern void ISP_MCLK3_EN (bool En);
extern int smartldo_set_vcama(int enable);
#define kal_uint16 unsigned short
#define kal_uint32 unsigned int
/*****************************************************************
* sc080cs marco
******************************************************************/
#define SC080CS_DRIVER_VERSION	"V2.0"
#define SC080CS_PRODUCT_NUM		4
#define SC080CS_PRODUCT_NAME_LEN	8
#define SC080CS_SENSOR_ID   0x3a
#define SC080CS_MCLK_ON   "sc080cs_mclk_on"
#define SC080CS_MCLK_OFF   "sc080cs_mclk_off"
/*****************************************************************
* sc080cs global global variable
******************************************************************/
static unsigned char read_reg_id = 0;
static unsigned char read_reg_value = 0;
static int read_reg_flag = 0;
static int driver_flag = 0;
struct sc080cs *g_sc080cs = NULL;
/**********************************************************
* i2c write and read
**********************************************************/
static void sc080cs_i2c_write(struct sc080cs *sc080cs, int address, int data)
{
	u8 i2c_buf[8];
	struct i2c_client *client = sc080cs->i2c_client;
	struct i2c_msg msg[1];
	msg[0].flags = !I2C_M_RD;
	msg[0].addr = client->addr;
	msg[0].len = 3;
	msg[0].buf = i2c_buf;
	i2c_buf[0] = (address & 0xff00)>>8;
	i2c_buf[1] = (address & 0xff);
	i2c_buf[2] = data;
	i2c_transfer(client->adapter, msg, 1);
	//printk("write sc080cs addr: 0x%4X val:0x%4X\n", address, data);
}
static unsigned char sc080cs_i2c_read(struct sc080cs *sc080cs, int address)
{
	unsigned char rxdata = 0x00;
	unsigned char i2c_buf[4];
	int ret = 0;
	int retry = 2;
	u8 i2c_addr[2];
	struct i2c_client *client = sc080cs->i2c_client;
	struct i2c_msg msgs[2];
	i2c_addr[0] = (address & 0xff00)>>8;
	i2c_addr[1] = (address & 0xff);
	msgs[0].flags = 0;
	msgs[0].addr = (client->addr);
	msgs[0].len = 2;
	msgs[0].buf = i2c_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].addr = (client->addr);
	msgs[1].len = 1;
	msgs[1].buf = i2c_buf;
	while (retry > 0)
	{
		ret = i2c_transfer(client->adapter, msgs, 2);
		//qvga_dev_err(&client->dev, "%s: read step1 ret:%d  msgs[1].addr=%x\n", __func__, ret, msgs[1].addr);
  		if (ret < 0)
  		{
			qvga_dev_err(&client->dev, "[rober]: %s: i2c_transfer error: ret:%d  msgs[1].addr=%x\n", __func__, ret, msgs[1].addr);
  			retry--;
			mdelay(2);
			continue;
  		}
		else
		{
			break;
		}
	}
	rxdata = i2c_buf[0];
	return rxdata;
}
static struct sc080cs *sc080cs_malloc_init(struct i2c_client *client)
{
	struct sc080cs *sc080cs =
	    devm_kzalloc(&client->dev, sizeof(struct sc080cs), GFP_KERNEL);
	if (sc080cs == NULL) {
		qvga_dev_err(&client->dev, "%s: devm_kzalloc failed.\n", __func__);
		return NULL;
	}
	sc080cs->i2c_client = client;
	pr_info("%s enter , client_addr = 0x%02x\n", __func__,
		sc080cs->i2c_client->addr);
	return sc080cs;
}
#if 1
void sc080cs_Init(struct sc080cs *sc080cs)
{
    /*SYS*/
	sc080cs_i2c_write(sc080cs, 0x0103,0x01);
	sc080cs_i2c_write(sc080cs, 0x0100,0x00);
	sc080cs_i2c_write(sc080cs, 0x309b,0xf0);
	sc080cs_i2c_write(sc080cs, 0x30b0,0x0a);
	sc080cs_i2c_write(sc080cs, 0x30b8,0x21);
	sc080cs_i2c_write(sc080cs, 0x320c,0x01);
	sc080cs_i2c_write(sc080cs, 0x320d,0x6a);
	sc080cs_i2c_write(sc080cs, 0x320e,0x01);
	sc080cs_i2c_write(sc080cs, 0x320f,0xba);
	sc080cs_i2c_write(sc080cs, 0x3301,0x04);
	sc080cs_i2c_write(sc080cs, 0x3304,0x0c);
	sc080cs_i2c_write(sc080cs, 0x3305,0x00);
	sc080cs_i2c_write(sc080cs, 0x3306,0x10);
	sc080cs_i2c_write(sc080cs, 0x3307,0x02);
	sc080cs_i2c_write(sc080cs, 0x3308,0x04);
	sc080cs_i2c_write(sc080cs, 0x330a,0x00);
	sc080cs_i2c_write(sc080cs, 0x330b,0x30);
	sc080cs_i2c_write(sc080cs, 0x330e,0x01);
	sc080cs_i2c_write(sc080cs, 0x330f,0x01);
	sc080cs_i2c_write(sc080cs, 0x3310,0x01);
	sc080cs_i2c_write(sc080cs, 0x331e,0x09);
	sc080cs_i2c_write(sc080cs, 0x3333,0x10);
	sc080cs_i2c_write(sc080cs, 0x3334,0x40);
	sc080cs_i2c_write(sc080cs, 0x334c,0x01);
	sc080cs_i2c_write(sc080cs, 0x33b3,0x3e);
	sc080cs_i2c_write(sc080cs, 0x349f,0x02);
	sc080cs_i2c_write(sc080cs, 0x34a6,0x01);
	sc080cs_i2c_write(sc080cs, 0x34a7,0x07);
	sc080cs_i2c_write(sc080cs, 0x34a8,0x3a);
	sc080cs_i2c_write(sc080cs, 0x34a9,0x38);
	sc080cs_i2c_write(sc080cs, 0x34e9,0x38);
	sc080cs_i2c_write(sc080cs, 0x34f8,0x07);
	sc080cs_i2c_write(sc080cs, 0x3630,0x65);
	sc080cs_i2c_write(sc080cs, 0x3637,0x47);
	sc080cs_i2c_write(sc080cs, 0x363a,0xe0);
	sc080cs_i2c_write(sc080cs, 0x3670,0x03);
	sc080cs_i2c_write(sc080cs, 0x3674,0x75);
	sc080cs_i2c_write(sc080cs, 0x3675,0x65);
	sc080cs_i2c_write(sc080cs, 0x3676,0x65);
	sc080cs_i2c_write(sc080cs, 0x367c,0x01);
	sc080cs_i2c_write(sc080cs, 0x367d,0x03);
	sc080cs_i2c_write(sc080cs, 0x3690,0xe0);
	sc080cs_i2c_write(sc080cs, 0x3691,0xe1);
	sc080cs_i2c_write(sc080cs, 0x3692,0xe1);
	sc080cs_i2c_write(sc080cs, 0x3693,0xe1);
	sc080cs_i2c_write(sc080cs, 0x3694,0x03);
	sc080cs_i2c_write(sc080cs, 0x3695,0x07);
	sc080cs_i2c_write(sc080cs, 0x3696,0x07);
	sc080cs_i2c_write(sc080cs, 0x37f9,0x29);
	sc080cs_i2c_write(sc080cs, 0x3900,0x91);
	sc080cs_i2c_write(sc080cs, 0x3904,0x0f);
	sc080cs_i2c_write(sc080cs, 0x3908,0x00);
	sc080cs_i2c_write(sc080cs, 0x391b,0x07);
	sc080cs_i2c_write(sc080cs, 0x391c,0x0a);
	sc080cs_i2c_write(sc080cs, 0x391d,0x15);
	sc080cs_i2c_write(sc080cs, 0x391e,0x28);
	sc080cs_i2c_write(sc080cs, 0x391f,0x41);
	sc080cs_i2c_write(sc080cs, 0x3948,0x00);
	sc080cs_i2c_write(sc080cs, 0x4509,0x10);
	sc080cs_i2c_write(sc080cs, 0x470b,0x0a);
	sc080cs_i2c_write(sc080cs, 0x470d,0x06);
	sc080cs_i2c_write(sc080cs, 0x5000,0xc2);
	sc080cs_i2c_write(sc080cs, 0x5001,0x01);
	sc080cs_i2c_write(sc080cs, 0x5170,0x2c);
	sc080cs_i2c_write(sc080cs, 0x5172,0xc1);
	sc080cs_i2c_write(sc080cs, 0x518b,0x03);
	sc080cs_i2c_write(sc080cs, 0x518c,0x20);
	sc080cs_i2c_write(sc080cs, 0x518d,0x01);
	sc080cs_i2c_write(sc080cs, 0x518e,0xb0);
	sc080cs_i2c_write(sc080cs, 0x518f,0x00);
	sc080cs_i2c_write(sc080cs, 0x519e,0x10);
	sc080cs_i2c_write(sc080cs, 0x300a,0x00);
	sc080cs_i2c_write(sc080cs, 0x0100,0x01);
	sc080cs_i2c_write(sc080cs, 0x518b,0x03);
	sc080cs_i2c_write(sc080cs, 0x518c,0x20);
	sc080cs_i2c_write(sc080cs, 0x518d,0x01);
	sc080cs_i2c_write(sc080cs, 0x518e,0xb0);
}   /*    sensor_init  */
#endif
int sc080cs_GetSensorID(struct sc080cs *sc080cs)
{
	int retry = 2;
	unsigned char reg_data = 0x00;
	//check if sensor ID correct
	do {
		reg_data = sc080cs_i2c_read(sc080cs, 0x3107);
		qvga_dev_err(sc080cs->dev, "drv-%s: Read MSB Sensor ID = 0x%02x\n", __func__, reg_data);
		if (reg_data == SC080CS_SENSOR_ID) {
			qvga_dev_err(sc080cs->dev, "drv-%s: Read Sensor ID sucess = 0x%02x\n", __func__, reg_data);
			driver_flag = 1;
			return 0;
		} else {
			qvga_dev_err(sc080cs->dev, "rv-%s: Read Sensor ID Fail = 0x%02x\n", __func__, reg_data);
			driver_flag = 0;
		}
		mdelay(10);
		retry--;
	} while (retry > 0);
	return -1;
}
static void sc080cs_vcam_control(struct sc080cs *sc080cs, bool flag)
{
	//struct regulator *vcama;
	struct regulator *vcamio;
	//struct regulator *vcamd;
	int return_value = 0;
	qvga_dev_info(sc080cs->dev, "%s enter\n", __func__);
	printk("robe_debug: %s run begin.\n", __func__);
    /*    vcamd = regulator_get(sc080cs->dev,"vcamd");
        if (IS_ERR(vcamd)) {
                qvga_dev_err(sc080cs->dev, "%s get regulator vcamd failed\n", __func__);
                regulator_put(vcamd);
                return;
        }
        if (flag) {
                regulator_set_voltage(vcamd, 1200000, 1200000);
              return_value = regulator_enable(vcamd);
        } else {
                regulator_disable(vcamd);
        }
	vcama = regulator_get(sc080cs->dev,"vcama");
	if (IS_ERR(vcama)) {
		qvga_dev_err(sc080cs->dev, "%s get regulator vcama failed\n", __func__);
		regulator_put(vcama);
		return;
	}*/
	if (flag) {
		smartldo_set_vcama(1);
		//regulator_set_voltage(vcama, 2800000, 2800000);
		//return_value = regulator_enable(vcama);
	} else {
		smartldo_set_vcama(0);
	}
	vcamio = regulator_get(sc080cs->dev,"vcamio");
	if (IS_ERR(vcamio)) {
		qvga_dev_err(sc080cs->dev, "%s get regulator vcamio failed\n", __func__);
		regulator_put(vcamio);
		printk("robe_debug: regulator_get vcamio failed.\n");
		return;
	} else {
		printk("robe_debug: regulator_get vcamio sucess.\n");;
	}
	if (flag) {
		regulator_set_voltage(vcamio, 1800000, 1800000);
		return_value = regulator_enable(vcamio);
	} else {
		regulator_disable(vcamio);
	}
	printk("robe_debug: %s run end.\n", __func__);
    return;
}
static void sc080cs_hw_on_reset(struct sc080cs *sc080cs)
{
	qvga_dev_info(sc080cs->dev, "%s enter\n", __func__);
	if (gpio_is_valid(sc080cs->reset_gpio)) {
		gpio_set_value_cansleep(sc080cs->reset_gpio, 1);
	}
}
static void sc080cs_hw_on_reset1(struct sc080cs *sc080cs)
{
	qvga_dev_info(sc080cs->dev, "%s enter\n", __func__);
	if (gpio_is_valid(sc080cs->reset_gpio1)) {
		gpio_set_value_cansleep(sc080cs->reset_gpio1, 1);
	}
}
static void sc080cs_hw_off_reset(struct sc080cs *sc080cs)
{
	qvga_dev_info(sc080cs->dev, "%s enter\n", __func__);
	if (gpio_is_valid(sc080cs->reset_gpio)) {
		gpio_set_value_cansleep(sc080cs->reset_gpio, 0);
		udelay(50);
		gpio_set_value_cansleep(sc080cs->reset_gpio, 1);
		udelay(50);
		gpio_set_value_cansleep(sc080cs->reset_gpio, 0);
	}
}
static void sc080cs_hw_off_reset1(struct sc080cs *sc080cs)
{
	qvga_dev_info(sc080cs->dev, "%s enter\n", __func__);
	if (gpio_is_valid(sc080cs->reset_gpio1)) {
		gpio_set_value_cansleep(sc080cs->reset_gpio1, 0);
	}
}
static void sc080cs_hw_on(struct sc080cs *sc080cs)
{
	sc080cs_vcam_control(sc080cs,true);
	udelay(20);
	sc080cs_hw_on_reset1(sc080cs);
	sc080cs_hw_on_reset(sc080cs);
	udelay(4000);
	sc080cs_Init(sc080cs);
	sc080cs->hwen_flag = 1;
}
static void sc080cs_hw_off(struct sc080cs *sc080cs)
{
	sc080cs_hw_off_reset(sc080cs);
	sc080cs_vcam_control(sc080cs,false);
	sc080cs->hwen_flag = 0;
}
static ssize_t sc080cs_get_reg(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	if (read_reg_flag) {
		len += snprintf(buf + len, PAGE_SIZE - len, "The reg 0x%02X value is 0x%02X\n",
				read_reg_id, read_reg_value);
		read_reg_flag = 0;
		read_reg_id = 0;
		read_reg_value = 0;
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "Please echo reg id into reg\n");
	}
	return len;
}
static ssize_t sc080cs_set_reg(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	unsigned int databuf[2] = { 0 };
	unsigned char reg_data = 0x00;
	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		sc080cs_i2c_write(g_sc080cs, databuf[0], databuf[1]);
	}
	else if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 1) {
		reg_data = sc080cs_i2c_read(g_sc080cs, databuf[0]);
		read_reg_id = databuf[0];
		read_reg_value = reg_data;
		read_reg_flag = 1;
	}
	return len;
}
static ssize_t sc080cs_get_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	if (driver_flag) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n",
				"sc_sc080cs_ii");
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n",
				"none");
	}
	return len;
}
static ssize_t sc080cs_get_light(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned char reg_data = 0x00;
	u16 light = 0;
	reg_data = sc080cs_i2c_read(g_sc080cs, 0x5160);
	light = reg_data;
	qvga_dev_err(g_sc080cs->dev, "%s: sc080cs light=%d, %d\n",   __func__, light, reg_data);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		light);
	return len;
}
static ssize_t sc080cs_set_light(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	ssize_t ret;
	unsigned int state;
	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		qvga_dev_err(g_sc080cs->dev, "%s: fail to change str to int\n",
			   __func__);
		return ret;
	}
	if (state == 0)
		sc080cs_hw_off(g_sc080cs); /*OFF*/
	else
		sc080cs_hw_on(g_sc080cs); /*ON*/
	return len;
}
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO,
		sc080cs_get_reg, sc080cs_set_reg);
static DEVICE_ATTR(cam_name, S_IWUSR | S_IRUGO,
		sc080cs_get_name, NULL);
static DEVICE_ATTR(light, S_IWUSR | S_IRUGO,
		sc080cs_get_light, sc080cs_set_light);
static struct attribute *sc080cs_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_cam_name.attr,
	&dev_attr_light.attr,
	NULL
};
static struct attribute_group sc080cs_attribute_group = {
	.attrs = sc080cs_attributes
};
static void sc080cs_parse_gpio_dt(struct sc080cs *sc080cs,
					struct device_node *np)
{
	qvga_dev_info(sc080cs->dev, "%s enter, dev_i2c%d@0x%02X\n", __func__,
			sc080cs->i2c_seq, sc080cs->i2c_addr);
	sc080cs->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (sc080cs->reset_gpio < 0) {
		qvga_dev_err(sc080cs->dev,
			   "%s: no reset gpio provided, hardware reset unavailable\n",
			__func__);
		sc080cs->reset_gpio = -1;
	} else {
		qvga_dev_info(sc080cs->dev, "%s: reset gpio provided ok\n",
			 __func__);
	}
	sc080cs->reset_gpio1 = of_get_named_gpio(np, "reset-gpio1", 0);
	if (sc080cs->reset_gpio1 < 0) {
		qvga_dev_err(sc080cs->dev,
			   "%s: no reset gpio1 provided, hardware reset unavailable\n",
			__func__);
		sc080cs->reset_gpio1 = -1;
	} else {
		qvga_dev_info(sc080cs->dev, "%s: reset gpio1 provided ok\n",
			 __func__);
	}
}
static void sc080cs_parse_dt(struct sc080cs *sc080cs, struct device_node *np)
{
	qvga_dev_info(sc080cs->dev, "%s enter, dev_i2c%d@0x%02X\n", __func__,
		    sc080cs->i2c_seq, sc080cs->i2c_addr);
	sc080cs_parse_gpio_dt(sc080cs, np);
}
/****************************************************************************
* sc080cs i2c driver
*****************************************************************************/
static int sc080cs_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct pinctrl *sc080cs_pinctrl;
	struct pinctrl_state *set_state;
	struct pinctrl_state *sc080cs_mclk_on = NULL;
	struct pinctrl_state *sc080cs_mclk_off = NULL;
	struct sc080cs *sc080cs = NULL;
	struct class *qvga_class;
	struct device *dev;
	int ret = -1;
	pr_err("scw %s enter , i2c%d@0x%02x\n", __func__,
		client->adapter->nr, client->addr);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		qvga_dev_err(&client->dev, "%s: check_functionality failed\n",
			   __func__);
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}
	sc080cs = sc080cs_malloc_init(client);
	g_sc080cs = sc080cs;
	sc080cs->i2c_seq = sc080cs->i2c_client->adapter->nr;
	sc080cs->i2c_addr = sc080cs->i2c_client->addr;
	if (sc080cs == NULL) {
		dev_err(&client->dev, "%s: failed to parse device tree node\n",
			__func__);
		ret = -ENOMEM;
		goto exit_devm_kzalloc_failed;
	}
	sc080cs->dev = &client->dev;
	i2c_set_clientdata(client, sc080cs);
	sc080cs_parse_dt(sc080cs, np);
	if (gpio_is_valid(sc080cs->reset_gpio)) {
		ret = devm_gpio_request_one(&client->dev,
					    sc080cs->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "sc080cs_rst");
		if (ret) {
			qvga_dev_err(&client->dev,
				   "%s: rst request failed\n", __func__);
			goto exit_gpio_request_failed;
		}
	}
	if (gpio_is_valid(sc080cs->reset_gpio1)) {
		ret = devm_gpio_request_one(&client->dev,
					    sc080cs->reset_gpio1,
					    GPIOF_OUT_INIT_LOW, "sc080cs_rst1");
		if (ret) {
			qvga_dev_err(&client->dev,
				   "%s: rst1 request failed\n", __func__);
			goto exit_gpio_request_failed;
		}
	}
	sc080cs_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(sc080cs_pinctrl)) {
		qvga_dev_err(&client->dev, "%s: sc080cs_pinctrl not defined\n", __func__);
	} else {
		set_state = pinctrl_lookup_state(sc080cs_pinctrl, SC080CS_MCLK_ON);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: sc080cs_pinctrl lookup failed for mclk on\n", __func__);
		} else {
			sc080cs_mclk_on = set_state;
		}
		set_state = pinctrl_lookup_state(sc080cs_pinctrl, SC080CS_MCLK_OFF);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: sc080cs_pinctrl lookup failed for mclk off\n", __func__);
		} else {
			sc080cs_mclk_off = set_state;
		}
		ret = pinctrl_select_state(sc080cs_pinctrl, sc080cs_mclk_off);
		if (ret < 0) {
			qvga_dev_err(&client->dev, "%s: sc080cs_pinctrl select failed for mclk off\n", __func__);
		}
	}
	//power on camera
    sc080cs_hw_off_reset1(sc080cs);
    mdelay(5);
	sc080cs_vcam_control(sc080cs, true);
	mdelay(1);
	ret = pinctrl_select_state(sc080cs_pinctrl, sc080cs_mclk_on);
	if (ret < 0) {
		qvga_dev_err(&client->dev, "%s: sc080cs_pinctrl select failed for mclk on\n", __func__);
	}
	sc080cs_hw_on_reset1(sc080cs);
	// sc080cs_hw_on_reset(sc080cs);
	mdelay(5);
//	sc080cs->hwen_flag = 1;
	/* sc080cs sensor id */
	ret = sc080cs_GetSensorID(sc080cs);
	if (ret < 0) {
		qvga_dev_err(&client->dev,
			   "%s: sc080csread_sensorid failed ret=%d\n", __func__,
			   ret);
		goto exit_i2c_check_id_failed;
	}
        //power off camera
	sc080cs_vcam_control(sc080cs, false);
    sc080cs_hw_off_reset1(sc080cs);
//	sc080cs_Init(sc080cs);
	qvga_class = class_create(THIS_MODULE, "qvga_cam");
	dev = device_create(qvga_class, NULL, client->dev.devt, NULL, "qvga_depth");
	ret = sysfs_create_group(&dev->kobj, &sc080cs_attribute_group);
	if (ret < 0) {
		qvga_dev_err(&client->dev,
			    "%s failed to create sysfs nodes\n", __func__);
	}
	return 0;
	exit_i2c_check_id_failed:
	sc080cs_vcam_control(sc080cs, false);
        sc080cs_hw_off_reset1(sc080cs);
        if (gpio_is_valid(sc080cs->reset_gpio))
                devm_gpio_free(&client->dev, sc080cs->reset_gpio);
 exit_gpio_request_failed:
	devm_kfree(&client->dev, sc080cs);
	sc080cs = NULL;
 exit_devm_kzalloc_failed:
 exit_check_functionality_failed:
	return ret;
}
static int sc080cs_i2c_remove(struct i2c_client *client)
{
	struct sc080cs *sc080cs = i2c_get_clientdata(client);
	if (gpio_is_valid(sc080cs->reset_gpio))
		devm_gpio_free(&client->dev, sc080cs->reset_gpio);
	if (gpio_is_valid(sc080cs->reset_gpio1))
		devm_gpio_free(&client->dev, sc080cs->reset_gpio1);
	devm_kfree(&client->dev, sc080cs);
	sc080cs = NULL;
	return 0;
}
static const struct of_device_id sc080cs_of_match[] = {
	{.compatible = "sc,sc080cs_yuv"},
	{},
};
static struct i2c_driver sc080cs_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name =  "sc080cs_yuv",
		   .of_match_table = sc080cs_of_match,
		   },
	.probe = sc080cs_i2c_probe,
	.remove = sc080cs_i2c_remove,
};
enum IMGSENSOR_RETURN sc080cs_yuv_init(void)
{
	int ret;
	pr_info("%s: driver version: %s\n", __func__,
				SC080CS_DRIVER_VERSION);
	ret = i2c_add_driver(&sc080cs_i2c_driver);
	if (ret) {
		pr_info("****[%s] Unable to register driver (%d)\n",
			__func__, ret);
		return ret;
	}
	return IMGSENSOR_RETURN_SUCCESS;
}
void sc080cs_yuv_init_robe(void)
{
    printk("robe_debug: %s run begin.\n", __func__);
    sc080cs_yuv_init();
    printk("robe_debug: %s run end.\n", __func__);
}
enum IMGSENSOR_RETURN sc080cs_yuv_exit(void)
{
	pr_info("%s enter\n", __func__);
	i2c_del_driver(&sc080cs_i2c_driver);
	return IMGSENSOR_RETURN_SUCCESS;
}
void sc080cs_yuv_exit_robe(void)
{
    printk("robe_debug: %s run begin.\n", __func__);
    sc080cs_yuv_exit();
    printk("robe_debug: %s run end.\n", __func__);
}

