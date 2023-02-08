/* code at 2022/08/1 start */
/*
 * gc6133c.c  gc6133c yuv module
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
#include "gc6133c_hlt_i.h"
extern void ISP_MCLK3_EN (bool En);
/*****************************************************************
* gc6133c marco
******************************************************************/
#define GC6133C_DRIVER_VERSION	"V8_1"
#define GC6133C_PRODUCT_NUM	4
#define GC6133C_PRODUCT_NAME_LEN	8
#define GC6133C_SENSOR_ID   0xba
#define GC6133C_MCLK_ON   "gc6133c_mclk_on"
#define GC6133C_MCLK_OFF   "gc6133c_mclk_off"
static struct pinctrl_state *gc6133c_mclk_on;
static struct pinctrl_state *gc6133c_mclk_off;
/*****************************************************************
* gc6133c global global variable
******************************************************************/
static unsigned char read_reg_id = 0;
static unsigned char read_reg_value = 0;
static int read_reg_flag = 0;
static int driver_flag = 0;
static struct gc6133c *g_gc6133c = NULL;
/**********************************************************
* i2c write and read
**********************************************************/
static int gc6133c_i2c_write(struct gc6133c *gc6133c,
			     unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	while (cnt < GC6133C_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(gc6133c->i2c_client,
						reg_addr, reg_data);
		if (ret < 0) {
			qvga_dev_err(gc6133c->dev,"%s: gc6133ci2c_write cnt=%d error=%d addr[0x%02x]\n", __func__,cnt, ret,reg_addr);
		} else {
			break;
		}
		cnt++;
		msleep(GC6133C_I2C_RETRY_DELAY);
	}
	return ret;
}
static int gc6133c_i2c_read(struct gc6133c *gc6133c,
			    unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	while (cnt < GC6133C_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(gc6133c->i2c_client, reg_addr);
		if (ret < 0) {
			qvga_dev_err(gc6133c->dev,"%s: gc6133ci2c_read cnt=%d error=%d\n", __func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(GC6133C_I2C_RETRY_DELAY);
	}
	return ret;
}
static struct gc6133c *gc6133c_malloc_init(struct i2c_client *client)
{
	struct gc6133c *gc6133c =
	    devm_kzalloc(&client->dev, sizeof(struct gc6133c), GFP_KERNEL);
	if (gc6133c == NULL) {
		dev_err(&client->dev, "%s: gc6133cdevm_kzalloc failed.\n", __func__);
		return NULL;
	}
	gc6133c->i2c_client = client;
	pr_info("%s gc6133center , client_addr = 0x%02x\n", __func__,gc6133c->i2c_client->addr);
	return gc6133c;
}
static void GC6133C_Init(struct gc6133c *gc6133c)
{
    pr_info("%s:gc6133c init", __func__);
	/*SYS*/
	gc6133c_i2c_write(gc6133c, 0xfe, 0xa0);
	gc6133c_i2c_write(gc6133c, 0xfe, 0xa0);
	gc6133c_i2c_write(gc6133c, 0xfe, 0xa0);
	gc6133c_i2c_write(gc6133c, 0xf6, 0x00);
	gc6133c_i2c_write(gc6133c, 0xfa, 0x11);
	gc6133c_i2c_write(gc6133c, 0xfc, 0x12);
	gc6133c_i2c_write(gc6133c, 0xfe, 0x00);
//	gc6133c_i2c_write(gc6133c, 0x4a, 0x40);
//	gc6133c_i2c_write(gc6133c, 0x4b, 0x5d);
	/*Analog*/
	gc6133c_i2c_write(gc6133c, 0x03, 0x01);
	gc6133c_i2c_write(gc6133c, 0x04, 0xf4);
	gc6133c_i2c_write(gc6133c, 0x01, 0x41);
	gc6133c_i2c_write(gc6133c, 0x02, 0x12);
	gc6133c_i2c_write(gc6133c, 0x0f, 0x01);
	gc6133c_i2c_write(gc6133c, 0x0d, 0x30);
	gc6133c_i2c_write(gc6133c, 0x12, 0xc8);
	gc6133c_i2c_write(gc6133c, 0x14, 0x54);
	gc6133c_i2c_write(gc6133c, 0x15, 0x32);
	gc6133c_i2c_write(gc6133c, 0x16, 0x04);
	gc6133c_i2c_write(gc6133c, 0x17, 0x19);
	gc6133c_i2c_write(gc6133c, 0x1d, 0xb9);
	gc6133c_i2c_write(gc6133c, 0x1f, 0x15);
	gc6133c_i2c_write(gc6133c, 0x7a, 0x00);
	gc6133c_i2c_write(gc6133c, 0x7b, 0x14);
	gc6133c_i2c_write(gc6133c, 0x7d, 0x36);
	gc6133c_i2c_write(gc6133c, 0xfe, 0x10);
	/*ISP*/
	gc6133c_i2c_write(gc6133c, 0x20, 0x7e);
	gc6133c_i2c_write(gc6133c, 0x22, 0xf8);
	gc6133c_i2c_write(gc6133c, 0x24, 0x54);
	gc6133c_i2c_write(gc6133c, 0x26, 0x87);
//	gc6133c_i2c_write(gc6133c, 0x29, 0x10);
//	gc6133c_i2c_write(gc6133c, 0x39, 0x00);
//	gc6133c_i2c_write(gc6133c, 0x3a, 0x80);
//	gc6133c_i2c_write(gc6133c, 0x3b, 0x01);
//	gc6133c_i2c_write(gc6133c, 0x3c, 0x40);
//	gc6133c_i2c_write(gc6133c, 0x3e, 0xf0);
    /*BLK*/
	gc6133c_i2c_write(gc6133c, 0x2a, 0x2f);
    gc6133c_i2c_write(gc6133c, 0x37, 0x46);
	/*Gain*/
	gc6133c_i2c_write(gc6133c, 0x3f, 0x18);
    /*DNDD*/
	gc6133c_i2c_write(gc6133c, 0x50, 0x3c);
	gc6133c_i2c_write(gc6133c, 0x52, 0x4f);
	gc6133c_i2c_write(gc6133c, 0x53, 0x81);
	gc6133c_i2c_write(gc6133c, 0x54, 0x43);
	gc6133c_i2c_write(gc6133c, 0x56, 0x78);
	gc6133c_i2c_write(gc6133c, 0x57, 0xaa);//20160901
	gc6133c_i2c_write(gc6133c, 0x58, 0xff);//20160901
    /*ASDE*/
	gc6133c_i2c_write(gc6133c, 0x5b, 0x60); //dd&ee th
	gc6133c_i2c_write(gc6133c, 0x5c, 0x80); //60/OT_th
	gc6133c_i2c_write(gc6133c, 0xab, 0x28);
	gc6133c_i2c_write(gc6133c, 0xac, 0xb5);
    /*INTPEE*/
	gc6133c_i2c_write(gc6133c, 0x60, 0x45);
	gc6133c_i2c_write(gc6133c, 0x62, 0x68); //20160901
	gc6133c_i2c_write(gc6133c, 0x63, 0x13); //edge effect
	gc6133c_i2c_write(gc6133c, 0x64, 0x43);
    /*CC*/
	gc6133c_i2c_write(gc6133c, 0x65, 0x13); //Y
	gc6133c_i2c_write(gc6133c, 0x66, 0x26);
	gc6133c_i2c_write(gc6133c, 0x67, 0x07);
	gc6133c_i2c_write(gc6133c, 0x68, 0xf5); //Cb
	gc6133c_i2c_write(gc6133c, 0x69, 0xea);
	gc6133c_i2c_write(gc6133c, 0x6a, 0x21);
	gc6133c_i2c_write(gc6133c, 0x6b, 0x21); //Cr
	gc6133c_i2c_write(gc6133c, 0x6c, 0xe4);
	gc6133c_i2c_write(gc6133c, 0x6d, 0xfb);
    /*YCP*/
	gc6133c_i2c_write(gc6133c, 0x81, 0x30); //cb
	gc6133c_i2c_write(gc6133c, 0x82, 0x30); //cr
	gc6133c_i2c_write(gc6133c, 0x83, 0x4a); //luma contrast
	gc6133c_i2c_write(gc6133c, 0x85, 0x06);  //luma offset
	gc6133c_i2c_write(gc6133c, 0x8d, 0x78); //edge dec sa
	gc6133c_i2c_write(gc6133c, 0x8e, 0x25); //autogray
    /*AEC*/
	gc6133c_i2c_write(gc6133c, 0x90, 0x60);//20160901
	gc6133c_i2c_write(gc6133c, 0x92, 0x50); //target
	gc6133c_i2c_write(gc6133c, 0x9d, 0x32);//STEP
	gc6133c_i2c_write(gc6133c, 0x9e, 0x61);//[7:4]margin  10fps
	gc6133c_i2c_write(gc6133c, 0x9f, 0xf4);
	gc6133c_i2c_write(gc6133c, 0xa0, 0x21); //pregain
	gc6133c_i2c_write(gc6133c, 0xa1, 0xf4); //pregain
	gc6133c_i2c_write(gc6133c, 0xa3, 0x20); //pregain
	gc6133c_i2c_write(gc6133c, 0xa4, 0x01);
    /*AWB*/
#if 0//AWB2
	gc6133c_i2c_write(gc6133c, 0xb0, 0xf2); //Y_to_C_diff
	gc6133c_i2c_write(gc6133c, 0xb1, 0x10); //Y_to_C_diff
	gc6133c_i2c_write(gc6133c, 0xb2, 0x08); //AWB_Y_to_C_diff_big
	gc6133c_i2c_write(gc6133c, 0xb3, 0x30); //C_max   //20
	gc6133c_i2c_write(gc6133c, 0xb4, 0x40);
	gc6133c_i2c_write(gc6133c, 0xb5, 0x20); //inter
	gc6133c_i2c_write(gc6133c, 0xb6, 0x34); //inter2
	gc6133c_i2c_write(gc6133c, 0xb7, 0x48); //AWB_C_inter3	 18
	gc6133c_i2c_write(gc6133c, 0xba, 0x40);   // big c  20
	gc6133c_i2c_write(gc6133c, 0xbb, 0x71); //62//AWB adjust   72
	gc6133c_i2c_write(gc6133c, 0xbd, 0x7a); //R_limit  80
	gc6133c_i2c_write(gc6133c, 0xbe, 0x40); //G_limit   58
	gc6133c_i2c_write(gc6133c, 0xbf, 0x80); //B_limit    a0
#else
	gc6133c_i2c_write(gc6133c, 0xb1, 0x1e); //Y_to_C_diff
	gc6133c_i2c_write(gc6133c, 0xb3, 0x20); //C_max
	gc6133c_i2c_write(gc6133c, 0xbd, 0x70); //R_limit
	gc6133c_i2c_write(gc6133c, 0xbe, 0x58); //G_limit
	gc6133c_i2c_write(gc6133c, 0xbf, 0xa0); //B_limit
//	gc6133c_i2c_write(gc6133c, 0xfe, 0x00);	//20160901 update for AWB
	gc6133c_i2c_write(gc6133c, 0x43, 0xa8);
	gc6133c_i2c_write(gc6133c, 0xb0, 0xf2);
	gc6133c_i2c_write(gc6133c, 0xb5, 0x40);
	gc6133c_i2c_write(gc6133c, 0xb8, 0x05);
	gc6133c_i2c_write(gc6133c, 0xba, 0x60);
#endif
    /*Banding*/
	//gc6133c_i2c_write(gc6133c, 0x01, 0x41); //hb
	//gc6133c_i2c_write(gc6133c, 0x02, 0x12); //vb
	//gc6133c_i2c_write(gc6133c, 0x0f, 0x01);
	//gc6133c_i2c_write(gc6133c, 0x9d, 0x32); //step
	//gc6133c_i2c_write(gc6133c, 0x9e, 0x61); //[7:4]margin  10fps
	//gc6133c_i2c_write(gc6133c, 0x9f, 0xf4);
    /*SPI*/
	//GC6133C_SERIAL_SET_PAGE2;
	gc6133c_i2c_write(gc6133c, 0xfe, 0x02);
	gc6133c_i2c_write(gc6133c, 0x01, 0x01); //spi enable
	gc6133c_i2c_write(gc6133c, 0x02, 0x80);   //LSB & Falling edge sample; ddr disable
	gc6133c_i2c_write(gc6133c, 0x03, 0x20);	//1-wire
	gc6133c_i2c_write(gc6133c, 0x04, 0x20);   //[4] master_outformat
	gc6133c_i2c_write(gc6133c, 0x0a, 0x00);   //Data ID, 0x00-YUV422, 0x01-RGB565
	gc6133c_i2c_write(gc6133c, 0x13, 0x10);
	gc6133c_i2c_write(gc6133c, 0x24, 0x02); //[1]sck_always [0]BT656
	gc6133c_i2c_write(gc6133c, 0x28, 0x03); //clock_div_spi
	//GC6133C_SERIAL_SET_PAGE0;
	gc6133c_i2c_write(gc6133c, 0xfe, 0x00);
    /*output*/
//	gc6133c_i2c_write(gc6133c, 0x22, 0xf8); //open awb
	gc6133c_i2c_write(gc6133c, 0xf1, 0x03); //output enable
}   /*    sensor_init  */
static int GC6133C_GetSensorID(struct gc6133c *gc6133c)
{
    int retry = 5;
    int len;
    unsigned char reg_data = 0x00;
    // check if sensor ID correct
    do {
		len = gc6133c_i2c_read(gc6133c, 0xf0, &reg_data);
        if (reg_data == GC6133C_SENSOR_ID) {
			qvga_dev_info(gc6133c->dev, "%s: GC6133C Read Sensor ID sucess = 0x%02x\n", __func__, reg_data);
			driver_flag = 1;
			return 0;
		} else {
			qvga_dev_err(gc6133c->dev, "%s: GC6133C Read Sensor ID Fail = 0x%02x\n", __func__, reg_data);
			driver_flag = 0;
		}
        retry--;
		pr_info("%s gc6133c get sensorid retry %d time\n", __func__, retry);
    } while (retry > 0);
	return -1;
}
// define AVDD
static void gc6133c_avdd_control(struct gc6133c *gc6133c, bool flag)
{
	struct regulator *vcama;
	qvga_dev_info(gc6133c->dev, "%s enter\n", __func__);
	vcama = regulator_get(gc6133c->dev,"vcama");
	if (IS_ERR(vcama)) {
		qvga_dev_err(gc6133c->dev, "%s AVDD get regulator failed\n", __func__);
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
// define IOVDD
static void gc6133c_iovdd_control(struct gc6133c *gc6133c, bool flag)
{
    struct regulator *vcamio;
    qvga_dev_info(gc6133c->dev, "%s enter\n", __func__);
    vcamio = regulator_get(gc6133c->dev,"vcamio");
    if (IS_ERR(vcamio)) {
        qvga_dev_err(gc6133c->dev, "%s  IOVDD get regulator failed\n", __func__);
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

static void gc6133c_hw_on_reset(struct gc6133c *gc6133c)
{
    qvga_dev_info(gc6133c->dev, "%s enter\n", __func__);
    gc6133c_i2c_write(gc6133c, 0xf1, 0x03);
    gc6133c_i2c_write(gc6133c, 0xfc, 0x12);
    gc6133c_i2c_write(gc6133c, 0xfe, 0x02);
    gc6133c_i2c_write(gc6133c, 0x01, 0x01);
}
static void gc6133c_hw_off_reset(struct gc6133c *gc6133c)
{
	qvga_dev_info(gc6133c->dev, "%s enter\n", __func__);
    gc6133c_i2c_write(gc6133c, 0xf1, 0x00);
    gc6133c_i2c_write(gc6133c, 0xfc, 0x01);
    gc6133c_i2c_write(gc6133c, 0xfe, 0x02);
    gc6133c_i2c_write(gc6133c, 0x01, 0x00);
}
static void gc6133c_hw_on(struct gc6133c *gc6133c)
{
	int ret;
	gc6133c_iovdd_control(gc6133c, true);
	udelay(10);
	gc6133c_avdd_control(gc6133c, true);
	udelay(10);
	ret = pinctrl_select_state(gc6133c->gc6133c_pinctrl, gc6133c_mclk_on);
	if (ret < 0) {
		qvga_dev_err(g_gc6133c->dev, "%s: pinctrl select failed for mclk on\n", __func__);
	}
	udelay(10);
	GC6133C_GetSensorID(gc6133c);
	gc6133c_hw_on_reset(gc6133c);
	GC6133C_Init(gc6133c);
	gc6133c->hwen_flag = 1;
}
static void gc6133c_hw_off(struct gc6133c *gc6133c)
{
	int ret;
	gc6133c_hw_off_reset(gc6133c);
	gc6133c->hwen_flag = 0;
	gc6133c_avdd_control(gc6133c, false);
	gc6133c_iovdd_control(gc6133c, false);
	ret = pinctrl_select_state(gc6133c->gc6133c_pinctrl, gc6133c_mclk_off);
	if (ret < 0) {
		qvga_dev_err(g_gc6133c->dev, "%s: pinctrl select failed for mclk off\n", __func__);
	}
}
static ssize_t gc6133c_get_reg(struct device *dev,
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
static ssize_t gc6133c_set_reg(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	unsigned int databuf[2] = { 0 };
	unsigned char reg_data = 0x00;
	int length;
	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		gc6133c_i2c_write(g_gc6133c, databuf[0], databuf[1]);
	}
	else if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 1) {
		length = gc6133c_i2c_read(g_gc6133c, databuf[0], &reg_data);
		read_reg_id = databuf[0];
		read_reg_value = reg_data;
		read_reg_flag = 1;
	}
	return len;
}
static ssize_t gc6133c_get_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	if (driver_flag) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n","gc6133_hlt_i");
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n","none");
	}
	return len;
}
static ssize_t gc6133c_get_light(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
/* code at 2022/09/1 start */
	unsigned char reg_data = 0xff;
/* code at 2022/09/1 end */
	int length;
	//GC6133C_Init(g_gc6133c);
	//mdelay(500);
    //gc6133c_hw_on(g_gc6133c);
	//gc6133c_i2c_write(g_gc6133c, 0xfe, 0x00);
	length = gc6133c_i2c_read(g_gc6133c, 0x93, &reg_data);
/* code at 2022/08/31 end */
	if (reg_data > 0 && reg_data != 0xff)
		reg_data -= 1;
/* code at 2022/08/31 end */
	len += snprintf(buf + len, PAGE_SIZE - len, "light:%d  \n",reg_data);
	pr_info("%s start !!!!",__func__);
	return len;
}
static ssize_t gc6133c_set_light(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	ssize_t ret;
	unsigned int state;
	ret = kstrtouint(buf, 10, &state);
    pr_err(" %s start !!! \n", __func__);
	if (ret) { 
		qvga_dev_err(g_gc6133c->dev, "%s:  fail to change str to int\n",
			   __func__);
		return ret;
	}
	if (state == 0)
		{
            gc6133c_hw_off(g_gc6133c); /*OFF*/
            pr_info(" %s failed, light_hw_off\n", __func__);
        }
	else
		{
            gc6133c_hw_on(g_gc6133c);
            pr_info(" %s sucess ,light_hw_on\n", __func__);
         } /*ON*/
	return len;
}
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO,
		gc6133c_get_reg, gc6133c_set_reg);
static DEVICE_ATTR(cam_name, S_IWUSR | S_IRUGO,
		gc6133c_get_name, NULL);
static DEVICE_ATTR(light, S_IWUSR | S_IRUGO,
		gc6133c_get_light, gc6133c_set_light);
static struct attribute *gc6133c_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_cam_name.attr,
	&dev_attr_light.attr,
	NULL
};
static const struct attribute_group gc6133c_attribute_group = {
	.attrs = gc6133c_attributes
};
/*static void gc6133c_parse_gpio_dt(struct gc6133c *gc6133c,
					struct device_node *np)
{
	qvga_dev_info(gc6133c->dev, "%s enter, dev_i2c%d@0x%02X\n", __func__,
			gc6133c->i2c_seq, gc6133c->i2c_addr);
	gc6133c->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (gc6133c->reset_gpio < 0) {
		qvga_dev_err(gc6133c->dev,
			   "%s: no reset gpio provided, hardware reset unavailable\n",
			__func__);
		gc6133c->reset_gpio = -1;
	} else {
		qvga_dev_info(gc6133c->dev, "%s: reset gpio provided ok\n",
			 __func__);
	}
}
static void gc6133c_parse_dt(struct gc6133c *gc6133c, struct device_node *np)
{
	qvga_dev_info(gc6133c->dev, "%s enter, dev_i2c%d@0x%02X\n", __func__,
		    gc6133c->i2c_seq, gc6133c->i2c_addr);
	gc6133c_parse_gpio_dt(gc6133c, np);
}*/
/****************************************************************************
* gc6133c i2c driver
*****************************************************************************/
static int gc6133c_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct pinctrl_state *set_state;
	struct gc6133c *gc6133c = NULL;
	struct class *qvga_class;
	struct device *dev;
	int ret = -1;
	pr_info("%s enter , %d@0x%02x\n", __func__,client->adapter->nr, client->addr);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		qvga_dev_err(&client->dev, "%s: check_functionality failed\n",__func__);
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}
	gc6133c = gc6133c_malloc_init(client);
	g_gc6133c = gc6133c;
	gc6133c->i2c_seq = gc6133c->i2c_client->adapter->nr;
	gc6133c->i2c_addr = gc6133c->i2c_client->addr;
	if (gc6133c == NULL) {
		dev_err(&client->dev, "%s: failed to parse device tree node\n",__func__);
		ret = -ENOMEM;
		goto exit_devm_kzalloc_failed;
	}
	gc6133c->dev = &client->dev;
	i2c_set_clientdata(client, gc6133c);
//	gc6133c_parse_dt(gc6133c, np);
/*	if (gpio_is_valid(gc6133c->reset_gpio)) {
		ret = devm_gpio_request_one(&client->dev,
					    gc6133c->reset_gpio,
					    GPIOF_OUT_INIT_HIGH, "gc6133c_rst");
		if (ret) {
			qvga_dev_err(&client->dev,
				   "%s: rst request failed\n", __func__);
			goto exit_gpio_request_failed;
		}
	}*/
	gc6133c->gc6133c_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(gc6133c->gc6133c_pinctrl)) {
		qvga_dev_err(&client->dev, "%s: pinctrl not defined\n", __func__);
	} else {
		set_state = pinctrl_lookup_state(gc6133c->gc6133c_pinctrl, GC6133C_MCLK_ON);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: pinctrl lookup failed for mclk on\n", __func__);
		} else {
			gc6133c_mclk_on = set_state;
		}
		set_state = pinctrl_lookup_state(gc6133c->gc6133c_pinctrl, GC6133C_MCLK_OFF);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: pinctrl lookup failed for mclk off\n", __func__);
		} else {
			gc6133c_mclk_off = set_state;
		}
		// ret = pinctrl_select_state(gc6133c->gc6133c_pinctrl, gc6133c_mclk_off);
		// if (ret < 0) {
		// 	qvga_dev_err(&client->dev, "%s: pinctrl select failed for mclk off\n", __func__);
	}
	//power on  camera
	gc6133c_iovdd_control(gc6133c, true);
	udelay(10);
	gc6133c_avdd_control(gc6133c, true);
	udelay(10);
	ret = pinctrl_select_state(gc6133c->gc6133c_pinctrl, gc6133c_mclk_on);
	udelay(10);
	if (ret < 0) {
		qvga_dev_err(&client->dev, "%s: select failed for mclk on\n", __func__);
	}
	gc6133c_hw_on_reset(gc6133c);
	gc6133c->hwen_flag = 1;
	ret = GC6133C_GetSensorID(gc6133c);
	if (ret < 0) {
		qvga_dev_err(&client->dev,"%s: read_sensorid failed \n", __func__,ret);
		goto exit_i2c_check_id_failed;
	}
	pr_err("%s: sensorid=0x%d\n", __func__,ret);

    //power off camera
	gc6133c_hw_off_reset(gc6133c);
	ret = pinctrl_select_state(gc6133c->gc6133c_pinctrl, gc6133c_mclk_off);
	if (ret < 0) {
		qvga_dev_err(&client->dev, "%s:  select failed for mclk off\n", __func__);
	}
	gc6133c_avdd_control(gc6133c, false);
	gc6133c_iovdd_control(gc6133c, false);
	gc6133c->hwen_flag = 0;

	//creat device
	qvga_class = class_create(THIS_MODULE, "qvga_cam");
	dev = device_create(qvga_class, NULL, client->dev.devt, NULL, "qvga_depth");
	ret = sysfs_create_group(&dev->kobj, &gc6133c_attribute_group);
	if (ret < 0) {
		qvga_dev_err(&client->dev,"%s failed to create sysfs nodes\n", __func__);
	}
	return 0;
	exit_i2c_check_id_failed:
	gc6133c_avdd_control(gc6133c, false);
	gc6133c_iovdd_control(gc6133c, false);
	devm_kfree(&client->dev, gc6133c);
	gc6133c = NULL;
	exit_devm_kzalloc_failed:
	exit_check_functionality_failed:
	return ret;
}
static int gc6133c_i2c_remove(struct i2c_client *client)
{
	struct gc6133c *gc6133c = i2c_get_clientdata(client);
	devm_kfree(&client->dev, gc6133c);
	gc6133c = NULL;
	return 0;
}
static const struct of_device_id gc6133c_of_match[] = {
	{.compatible = "gc,gc6133c_yuv"},
	{},
};
static struct i2c_driver gc6133c_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name =  "gc6133c_yuv",
		   .of_match_table = gc6133c_of_match,
		   },
	.probe = gc6133c_i2c_probe,
	.remove = gc6133c_i2c_remove,
};
static int __init gc6133c_yuv_init(void)
{
	int ret;
	pr_info("%s: driver version: %s\n", __func__,GC6133C_DRIVER_VERSION);
	ret = i2c_add_driver(&gc6133c_i2c_driver);
	if (ret) {
		pr_err("[%s] Unable to register driver (%d)\n",__func__, ret);
		return ret;
	}
	return 0;
}
static void __exit gc6133c_yuv_exit(void)
{
	pr_info("%s enter\n", __func__);
	i2c_del_driver(&gc6133c_i2c_driver);
}
module_init(gc6133c_yuv_init);
module_exit(gc6133c_yuv_exit);
MODULE_AUTHOR("wuzhenyue@huaqin.com>");
MODULE_DESCRIPTION("gc6133c yuv driver");
MODULE_LICENSE("GPL V2");
/* code at 2022/08/1 end */