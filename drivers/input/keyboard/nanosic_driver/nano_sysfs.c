/** ***************************************************************************
 * @file nano_sysfs.c
 *
 * @brief Create device sysfs node  
 *        Create /sys/class/nanodev/nanodev0/_debuglevel        
 *        Create /sys/class/nanodev/nanodev0/_schedule
 *        Create /sys/class/nanodev/nanodev0/_versioncode
 *
 * <em>Copyright (C) 2010, Nanosic, Inc.  All rights reserved.</em>
 * Author : Bin.yuan bin.yuan@nanosic.com 
 * */

/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h> 
#include <linux/device.h>
#include <linux/kdev_t.h>
#include "nano_macro.h"

int debuglevel = 7;

/** ************************************************************************//**
 * @func debuglevel_show
 *
 * @brief null
 ** */
static ssize_t 
debuglevel_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf,"debuglevel=%d\n",debuglevel);
}

/** ************************************************************************//**
 * @func debuglevel_show
 *
 * @brief null
 ** */
static ssize_t 
debuglevel_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int i;
    
	ret = kstrtouint(buf, 10, &i);
    if(ret)
        return 0;

    debuglevel = i;

    return count;
}

/** ************************************************************************//**
 * @func schedule_show
 *
 * @brief null
 ** */
static ssize_t 
schedule_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct nano_i2c_client* i2c_client = gI2c_client;
    
    if(!IS_ERR_OR_NULL(i2c_client) && !IS_ERR_OR_NULL(i2c_client->worker))
        return sprintf(buf,"schedule=%d i2c-read=%d i2c-read-error=%d\n",
                atomic_read(&i2c_client->worker->schedule_count),atomic_read(&i2c_client->i2c_read_count),
                atomic_read(&i2c_client->i2c_error_count));
    else
        return sprintf(buf,"schedule error\n");
}

/** ************************************************************************//**
 * @func schedule_store
 *
 * @brief 通过echo 5 >  /sys/class/nanodev/nanodev0/_schedule 方式来模拟i2c中断的次数,并执行5次i2c_read
 ** */
static ssize_t 
schedule_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	unsigned int i;
	unsigned int schedule_count;
    	struct nano_i2c_client* i2c_client = gI2c_client;
    
	ret = kstrtouint(buf, 10, &i);
    if(ret)
        return 0;

    if(IS_ERR_OR_NULL(i2c_client))
        return count;

    schedule_count = i> 10000 ? 10000 : i;

    while(schedule_count)
    {
        Nanosic_workQueue_schedule(i2c_client->worker);
        schedule_count--;
    }

//    Nanosic_GPIO_set(i?true:false);

    return count;
}

/** ************************************************************************//**
 * @func version_code_show
 *
 * @brief null
 ** */
static ssize_t 
version_SDK_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf,"SDK %s\n",DRV_VERSION);
}


/** ************************************************************************//**
 * @func version_code_store
 *
 * @brief 查看驱动版本号
 ** */
static ssize_t 
version_SDK_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return count;
}

/** ************************************************************************//**
 * @func version_803x_show
 *
 * @brief 查看803x版本
 ** */
static ssize_t 
version_803x_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf,"version803x=%s\n",strlen(gVers803x)>0?gVers803x:"null");
}

/** ************************************************************************//**
 * @func version_803x_store
 *
 * @brief 发送读803x版本命令
 ** */
static ssize_t 
version_803x_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct nano_i2c_client* i2c_client = gI2c_client;
    
    if(i2c_client)
    {
        int i=2;
        char read_803_vers_cmd[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4F,0x30,0x80,FIELD_803X,0x01,0x00,0x00};
        memset(gVers803x,0,sizeof(gVers803x));
        for(;i<8;i++)
        {
            read_803_vers_cmd[8] +=read_803_vers_cmd[i];/*cal sum*/
        }
        rawdata_show("request 803 version",read_803_vers_cmd,sizeof(read_803_vers_cmd));
        Nanosic_i2c_write(i2c_client,read_803_vers_cmd,sizeof(read_803_vers_cmd));
    }
    
    return count;
}

/** ************************************************************************//**
 * @func version_176x_show
 *
 * @brief 查看keypad版本命令
 ** */
static ssize_t 
version_176x_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    bool keypad_conneted = (gHallStatus>>0)&0x1;
    bool keypad_power    = (gHallStatus>>1)&0x1;
    bool keypad_POGOPIN  = (gHallStatus>>6)&0x1;

    if(keypad_conneted & keypad_power){
        sprintf(buf,"Connected=[%d] Power=[%d] POGOPIN=[%s] versionKeyPad=[%s]\n",keypad_conneted,keypad_power,keypad_POGOPIN ?"ERROR":"OK",strlen(gVers176x)>0?gVers176x:"null");
    }else{
        sprintf(buf,"Connected=[%d] Power=[%d]\n",keypad_conneted,keypad_power);
    }

    return strlen(buf);
}

/** ************************************************************************//**
 * @func version_176x_store
 *
 * @brief 发送读keypad版本命令
 ** */
static ssize_t 
version_176x_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct nano_i2c_client* i2c_client = gI2c_client;

    if(i2c_client)
    {
        int i=0;

        char read_keypad_status[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4E,0x31,FIELD_HOST,FIELD_176X,0xA1,0x01,0x01,0x00};
        char read_keypad_vers_cmd[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4F,0x30,FIELD_HOST,FIELD_176X,0x01,0x00};
        gHallStatus = 0;

        for(i=2;i<9;i++)
            read_keypad_status[9] +=read_keypad_status[i];/*cal sum*/
        rawdata_show("request keypad hall status",read_keypad_status,sizeof(read_keypad_status));
        Nanosic_i2c_write(i2c_client,read_keypad_status,sizeof(read_keypad_status));

        msleep(10);

        memset(gVers176x,0,sizeof(gVers176x));
        for(i=2;i<8;i++)
            read_keypad_vers_cmd[8] +=read_keypad_vers_cmd[i];/*cal sum*/
        rawdata_show("request keypad version",read_keypad_vers_cmd,sizeof(read_keypad_vers_cmd));
        Nanosic_i2c_write(i2c_client,read_keypad_vers_cmd,sizeof(read_keypad_vers_cmd));
    }

    return count;
}

/** ************************************************************************//**
 * @func sleep_803x_show
 *
 * @brief 查看803x版本
 ** */
static ssize_t 
sleep_803x_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf,"sleep 803x\n");
}

/** ************************************************************************//**
 * @func sleep_803x_store
 *
 * @brief 发送读803x版本命令
 ** */
static ssize_t 
sleep_803x_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{   
	int ret;
	unsigned int i;
    
	ret = kstrtouint(buf, 16, &i);
    if(ret)
        return 0;
    
    Nanosic_GPIO_sleep(i>0?true:false);
    
    return count;
}

/** ************************************************************************//**
 * @func gpio_set_show
 *
 * @brief gpio set help
 ** */
static ssize_t
gpio_set_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf,"usage echo pin level > _gpioset\n");
}

/** ************************************************************************//**
 * @func gpio_set_store
 *
 * @brief 设置gpio pin电压
 ** */
static ssize_t
gpio_set_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int gpio_pin;
    int gpio_value;
    char delim[] = " ";
    char* str = kstrdup(buf,GFP_KERNEL);
    char* token = NULL;

    if(!str)
        return count;

    dbgprint(DEBUG_LEVEL,"gpio set %s\n",str);

    token=strsep(&str, delim);
    if(token != NULL)
        kstrtouint(token,10,&gpio_pin);

    token=strsep(&str, delim);
    if(token != NULL)
        kstrtouint(token,10,&gpio_value);

    dbgprint(DEBUG_LEVEL,"gpio set %d %d\n",gpio_pin,gpio_value);

    if(str)
        kfree(str);

    Nanosic_GPIO_set(gpio_pin,gpio_value > 0 ? true : false);

    return count;
}

/** ************************************************************************//**
 * @func debuglevel_show
 *
 * @brief null
 ** */
static ssize_t
dispatch_keycode_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf,"dispatch_keycode_show\n");
}

/** ************************************************************************//**
 * @func debuglevel_show
 *
 * @brief write keycode to input system for test
 ** */
static ssize_t 
dispatch_keycode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int i;
    unsigned char down[12] = {0x57,0x00,0x39,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    unsigned char up[12]   = {0x57,0x00,0x39,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    
	ret = kstrtouint(buf, 16, &i);
    if(ret)
        return 0;

    down[6] = i;

    Nanosic_i2c_parse(down,sizeof(down));
    Nanosic_i2c_parse(up,sizeof(up));

    return count;
}

/*设置调试级别*/
static DEVICE_ATTR(_debuglevel, 0600, debuglevel_show, debuglevel_store);

/*统计workqueue运行次数*/
static DEVICE_ATTR(_schedule, 0600, schedule_show, schedule_store);

/*查看sdk版本号*/
static DEVICE_ATTR(_versionSDK, 0600, version_SDK_show, version_SDK_store);

/*键盘测试*/
static DEVICE_ATTR(_keycode, 0600, dispatch_keycode_show, dispatch_keycode_store);

/*查看803版本号*/
static DEVICE_ATTR(_version803x, 0600, version_803x_show, version_803x_store);

/*查看keypad版本号*/
static DEVICE_ATTR(_version176x, 0600, version_176x_show, version_176x_store);

/*控制803睡眠*/
static DEVICE_ATTR(_sleep803x, 0600, sleep_803x_show, sleep_803x_store);

/*gpio set method*/
static DEVICE_ATTR(_gpioset, 0600, gpio_set_show, gpio_set_store);

static struct device_attribute *sysfs_device_attr_debuglevel = {
	&dev_attr__debuglevel,
};

static struct device_attribute *sysfs_device_attr_schedule = {
	&dev_attr__schedule,
};

static struct device_attribute *sysfs_device_attr_version_SDK = {
	&dev_attr__versionSDK,
};

static struct device_attribute *sysfs_device_attr_dispatch = {
	&dev_attr__keycode,
};

static struct device_attribute *sysfs_device_attr_version_803x = {
	&dev_attr__version803x,
};

static struct device_attribute *sysfs_device_attr_version_176x = {
	&dev_attr__version176x,
};

static struct device_attribute *sysfs_device_attr_sleep_803x = {
	&dev_attr__sleep803x,
};

static struct device_attribute *sysfs_device_attr_gpio_set = {
	&dev_attr__gpioset,
};

/** ************************************************************************//**
 * @func Nanosic_Sysfs_create
 *
 * @brief create sysfs node for nanosic i2c-hid driver
 */
void
Nanosic_sysfs_create(struct device* dev)
{
    /* Create /sys/class/nanodev/nanodev0/_debuglevel    */
    device_create_file(dev,sysfs_device_attr_debuglevel);
    /* Create /sys/class/nanodev/nanodev0/_schedule      */
    device_create_file(dev,sysfs_device_attr_schedule);
    /* Create /sys/class/nanodev/nanodev0/_versioncode   */
    device_create_file(dev,sysfs_device_attr_version_SDK);
    /* Create /sys/class/nanodev/nanodev0/_keycode*/
    device_create_file(dev,sysfs_device_attr_dispatch);
    /* Create /sys/class/nanodev/nanodev0/_version803x*/
    device_create_file(dev,sysfs_device_attr_version_803x);
    /* Create /sys/class/nanodev/nanodev0/_version176x*/
    device_create_file(dev,sysfs_device_attr_version_176x);
    /* Create /sys/class/nanodev/nanodev0/_sleep803x*/
    device_create_file(dev,sysfs_device_attr_sleep_803x);
    /* Create /sys/class/nanodev/nanodev0/_gpioset*/
    device_create_file(dev,sysfs_device_attr_gpio_set);
}

/** ************************************************************************//**
 * @func Nanosic_Sysfs_release
 *
 * @brief 
 */
void
Nanosic_sysfs_release(struct device* dev)
{
    device_remove_file(dev,sysfs_device_attr_debuglevel);
    device_remove_file(dev,sysfs_device_attr_schedule);
    device_remove_file(dev,sysfs_device_attr_version_SDK);
    device_remove_file(dev,sysfs_device_attr_dispatch);
    device_remove_file(dev,sysfs_device_attr_version_803x);
    device_remove_file(dev,sysfs_device_attr_version_176x);
    device_remove_file(dev,sysfs_device_attr_sleep_803x);
    device_remove_file(dev,sysfs_device_attr_gpio_set);
}

