/* drivers/i2c/chips/mag3110.c - MAG3110 compass driver
 *
 * Copyright (C) 2009 AMIT Technology Inc.
 * Author: Kyle Chen <sw-support@amit-inc.com>
 *
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>


#include <cust_mag.h>
#include <linux/hwmsen_helper.h>
#include <linux/ioctl.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


/*-------------------------MT6516&MT6573 define-------------------------------*/

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#define MAG3110_I2C_ADDRESS 			0x1c //new Addr=0x0E(Low), old Addr=0x0F(High)
/*----------------------------------------------------------------------------*/
#define MAG3110_ID		0xC4
#define MAG3110_XYZ_DATA_LEN	6
#define MAG3110_STATUS_ZYXDR	0x08
#define MAG3110_AC_MASK         (0x01)
#define MAG3110_AC_OFFSET       0
#define MAG3110_DR_MODE_MASK    (0x7 << 5)
#define MAG3110_DR_MODE_OFFSET  5

/* register enum for mag3110 registers */
enum {
	MAG3110_DR_STATUS = 0x00,
	MAG3110_OUT_X_MSB,
	MAG3110_OUT_X_LSB,
	MAG3110_OUT_Y_MSB,
	MAG3110_OUT_Y_LSB,
	MAG3110_OUT_Z_MSB,
	MAG3110_OUT_Z_LSB,
	MAG3110_WHO_AM_I,

	MAG3110_OFF_X_MSB,
	MAG3110_OFF_X_LSB,
	MAG3110_OFF_Y_MSB,
	MAG3110_OFF_Y_LSB,
	MAG3110_OFF_Z_MSB,
	MAG3110_OFF_Z_LSB,

	MAG3110_DIE_TEMP,

	MAG3110_CTRL_REG1 = 0x10,
	MAG3110_CTRL_REG2,
};


/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_MAG3110 304
#define DEBUG 1
#define MAG3110_DEV_NAME         "mag3110"
#define DRIVER_VERSION          "1.0.6.11"
/*-----------------------------------------------------------------------------*/

#define MAG3110_BUFSIZE				256
#define MAG3110_NORMAL_MODE			0
#define MAG3110_FORCE_MODE			1
#define MAG3110_IRQ				IRQ_EINT9

// conversion of magnetic data to nT units
#define CONVERT_M                       25
#define ORIENTATION_ACCURACY_RATE                   10

/*----------------------------------------------------------------------------*/
#define MAG3110_AXIS_X            0
#define MAG3110_AXIS_Y            1
#define MAG3110_AXIS_Z            2
#define MAG3110_AXES_NUM          3
/*----------------------------------------------------------------------------*/
#define MSE_TAG                  "MSENSOR"
#define MSE_FUN(f)               printk(MSE_TAG" %s\r\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)    printk(MSE_TAG" %s %d : \r\n"fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)    printk(MSE_TAG fmt, ##args)
#define MSE_VER(fmt, args...)   ((void)0)
/*----------------------------------------------------------------------------*/
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);
/*----------------------------------------------------------------------------*/
static struct i2c_client *mag3110_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mag3110_i2c_id[] = {{MAG3110_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_mag3110={ I2C_BOARD_INFO("mag3110", (0X1C>>1))};
/*the adapter id will be available in customization*/
//static unsigned short mag3110_force[] = {0x00, MAG3110_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const mag3110_forces[] = { mag3110_force, NULL };
//static struct i2c_client_address_data mag3110_addr_data = { .forces = mag3110_forces,};
/*----------------------------------------------------------------------------*/
static int mag3110_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int mag3110_i2c_remove(struct i2c_client *client);
//static int mag3110_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);

static struct platform_driver ami_sensor_driver;

/*----------------------------------------------------------------------------*/
typedef enum {
    AMI_TRC_DEBUG  = 0x01,
} AMI_TRC;
/*----------------------------------------------------------------------------*/
struct _ami302_data {
    rwlock_t lock;
    int mode;
    int rate;
    volatile int updated;
} mag3110_data;
/*----------------------------------------------------------------------------*/
struct _mag3110_mid_data {
    rwlock_t datalock;
    rwlock_t ctrllock;
    int controldata[10];
    unsigned int debug;
    int yaw;
    int roll;
    int pitch;
    int nmx;
    int nmy;
    int nmz;
    int nax;
    int nay;
    int naz;
    int mag_status;
} mag3110_mid_data;
/*----------------------------------------------------------------------------*/
struct mag3110_i2c_data {
    struct i2c_client *client;
    struct mag_hw *hw;
    struct hwmsen_convert   cvt;
    atomic_t layout;   
    atomic_t trace;
#if defined(CONFIG_HAS_EARLYSUSPEND)    
    struct early_suspend    early_drv;
#endif 
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver mag3110_i2c_driver = {
    .driver = {
//      .owner = THIS_MODULE, 
        .name  = MAG3110_DEV_NAME,
    },
	.probe      = mag3110_i2c_probe,
	.remove     = mag3110_i2c_remove,
//	.detect     = mag3110_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend    = mag3110_suspend,
	.resume     = mag3110_resume,
#endif 
	.id_table = mag3110_i2c_id,
//	.address_data = &mag3110_addr_data,
};
/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/
static int hwmsen_read_byte_sr(struct i2c_client *client, u8 addr, u8 *data)
{
   u8 buf;
    int ret = 0;
	
    client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
    buf = addr;
	ret = i2c_master_send(client, (const char*)&buf, 1<<8 | 1);
    //ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        MSE_ERR("send command error!!\n");
        return -EFAULT;
    }

    *data = buf;
	client->addr = client->addr& I2C_MASK_FLAG;
    return 0;
}
static int hwmsen_read_block_sr(struct i2c_client *client, u8 addr, u8 *data)
{
   u8 buf[10];
    int ret = 0;
	memset(buf, 0, sizeof(u8)*10);
    client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
    buf[0] = addr;
	ret = i2c_master_send(client, (const char*)&buf, 6<<8 | 1);
    //ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        MSE_ERR("send command error!!\n");
        return -EFAULT;
    }

    *data = buf;
	client->addr = client->addr& I2C_MASK_FLAG;
    return 0;
}

static void mag3110_power(struct mag_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)
	{        
		MSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)
		{
			MSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "MAG3110")) 
			{
				MSE_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "MAG3110")) 
			{
				MSE_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/

static int mag3110_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}


static int mag3110_gpio_config(void)
{
	return 0;
}

static int MAG3110_Chipset_Init(int mode)
{
	
	/* enable automatic resets */
	u8 databuf[10];    
	int ret = 0;
	u8 val;

	memset(databuf, 0, sizeof(u8)*10);	  
	databuf[0] = MAG3110_CTRL_REG2;    
	databuf[1] = 0x80;
	ret = i2c_master_send(mag3110_i2c_client, databuf, 2);
    
	hwmsen_read_byte_sr(mag3110_i2c_client,MAG3110_WHO_AM_I,&val);
	val = 0;
	/* set default data rate to 10HZ */
	hwmsen_read_byte_sr(mag3110_i2c_client,MAG3110_CTRL_REG1,&val);
	val |= 0x01;
	memset(databuf, 0, sizeof(u8)*10);	  
	databuf[0] = MAG3110_CTRL_REG1;    
	databuf[1] = val;
	ret = i2c_master_send(mag3110_i2c_client, databuf, 2);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int MAG3110_SetMode(int newmode)
{
	
}
/*----------------------------------------------------------------------------*/
static int MAG3110_ReadChipInfo(char *buf, int bufsize)
{
	if((!buf)||(bufsize<=30))
	{
		return -1;
	}
	if(!mag3110_i2c_client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "MAG3110 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int MAG3110_ReadSensorData(char *revbuf, int bufsize)
{
    	u8 addr = MAG3110_OUT_X_MSB;
		u8 buf[6] = {0};
		u8 tmp_data[6];
		s16 mag[MAG3110_AXES_NUM];
		s16 output[3];
        struct mag3110_i2c_data *data = i2c_get_clientdata(mag3110_i2c_client);        
		buf[0] = MAG3110_OUT_X_MSB;
	    mag3110_i2c_client->addr = mag3110_i2c_client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
        i2c_master_send(mag3110_i2c_client, (const char*)&buf, 6<<8 | 1);
	    mag3110_i2c_client->addr = mag3110_i2c_client->addr& I2C_MASK_FLAG;
		
		output[MAG3110_AXIS_X] = (s16)((buf[MAG3110_AXIS_X*2] << 8) |
		         (buf[MAG3110_AXIS_X*2+1]));
		output[MAG3110_AXIS_Y] = (s16)((buf[MAG3110_AXIS_Y*2] << 8) |
		         (buf[MAG3110_AXIS_Y*2+1]));
		output[MAG3110_AXIS_Z] = (s16)((buf[MAG3110_AXIS_Z*2] << 8) |
		         (buf[MAG3110_AXIS_Z*2+1]));    
		mag[data->cvt.map[MAG3110_AXIS_X]] = data->cvt.sign[MAG3110_AXIS_X]*output[MAG3110_AXIS_X];
		mag[data->cvt.map[MAG3110_AXIS_Y]] = data->cvt.sign[MAG3110_AXIS_Y]*output[MAG3110_AXIS_Y];
		mag[data->cvt.map[MAG3110_AXIS_Z]] = data->cvt.sign[MAG3110_AXIS_Z]*output[MAG3110_AXIS_Z];
		sprintf(revbuf, "%04x %04x %04x", mag[MAG3110_AXIS_X], mag[MAG3110_AXIS_Y], mag[MAG3110_AXIS_Z]);

		
	return 0;
}


/*----------------------------------------------------------------------------*/
static int MAG3110_ReadPostureData(char *buf, int bufsize)
{
	if((!buf)||(bufsize<=80))
	{
		return -1;
	}
	
	read_lock(&mag3110_mid_data.datalock);
	sprintf(buf, "%d %d %d %d", mag3110_mid_data.yaw, mag3110_mid_data.pitch,
		mag3110_mid_data.roll, mag3110_mid_data.mag_status);
	read_unlock(&mag3110_mid_data.datalock);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int MAG3110_ReadCaliData(char *buf, int bufsize)
{
	if((!buf)||(bufsize<=80))
	{
		return -1;
	}
	
	read_lock(&mag3110_mid_data.datalock);
	sprintf(buf, "%d %d %d %d %d %d %d", mag3110_mid_data.nmx, mag3110_mid_data.nmy, 
		mag3110_mid_data.nmz,mag3110_mid_data.nax,mag3110_mid_data.nay,mag3110_mid_data.naz,mag3110_mid_data.mag_status);
	read_unlock(&mag3110_mid_data.datalock);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int MAG3110_ReadMiddleControl(char *buf, int bufsize)
{
	if ((!buf)||(bufsize<=80))
	{
		return -1;
	}
	
	read_lock(&mag3110_mid_data.ctrllock);
	sprintf(buf, "%d %d %d %d %d %d %d %d %d %d",mag3110_mid_data.controldata[0],	mag3110_mid_data.controldata[1], 
		mag3110_mid_data.controldata[2],mag3110_mid_data.controldata[3],mag3110_mid_data.controldata[4],
		mag3110_mid_data.controldata[5], mag3110_mid_data.controldata[6], mag3110_mid_data.controldata[7],
		mag3110_mid_data.controldata[8], mag3110_mid_data.controldata[9]);
	read_unlock(&mag3110_mid_data.ctrllock);
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MAG3110_BUFSIZE];
	MAG3110_ReadChipInfo(strbuf, MAG3110_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MAG3110_BUFSIZE];
	MAG3110_ReadSensorData(strbuf, MAG3110_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MAG3110_BUFSIZE];
	MAG3110_ReadPostureData(strbuf, MAG3110_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_calidata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MAG3110_BUFSIZE];
	MAG3110_ReadCaliData(strbuf, MAG3110_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_midcontrol_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MAG3110_BUFSIZE];
	MAG3110_ReadMiddleControl(strbuf, MAG3110_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_midcontrol_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int p[10];
	if(10 == sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",&p[0], &p[1], &p[2], &p[3], &p[4], 
		&p[5], &p[6], &p[7], &p[8], &p[9]))
	{
		write_lock(&mag3110_mid_data.ctrllock);
		memcpy(&mag3110_mid_data.controldata[0], &p, sizeof(int)*10);    
		write_unlock(&mag3110_mid_data.ctrllock);        
	}
	else
	{
		MSE_ERR("invalid format\n");     
	}
	return sizeof(int)*10;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_middebug_value(struct device_driver *ddri, char *buf)
{
	ssize_t len;
	read_lock(&mag3110_mid_data.ctrllock);
	len = sprintf(buf, "0x%08X\n", mag3110_mid_data.debug);
	read_unlock(&mag3110_mid_data.ctrllock);

	return len;            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_middebug_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int debug;
	if(1 == sscanf(buf, "0x%x", &debug))
	{
		write_lock(&mag3110_mid_data.ctrllock);
		mag3110_mid_data.debug = debug;
		write_unlock(&mag3110_mid_data.ctrllock);        
	}
	else
	{
		MSE_ERR("invalid format\n");     
	}
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_mode_value(struct device_driver *ddri, char *buf)
{
	int mode=0;
	read_lock(&mag3110_data.lock);
	mode = mag3110_data.mode;
	read_unlock(&mag3110_data.lock);        
	return sprintf(buf, "%d\n", mode);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int mode = 0;
	sscanf(buf, "%d", &mode);    
	MAG3110_SetMode(mode);
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mag3110_i2c_client;  
	struct mag3110_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = mag3110_i2c_client;  
	struct mag3110_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			MSE_ERR("HWMSEN_GET_CONVERT function error!\r\n");
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			MSE_ERR("invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		}
		else
		{
			MSE_ERR("invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	}
	else
	{
		MSE_ERR("invalid format = '%s'\n", buf);
	}
	
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mag3110_i2c_client;  
	struct mag3110_i2c_data *data = i2c_get_clientdata(client);
	ssize_t len = 0;

	if(data->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n", 
			data->hw->i2c_num, data->hw->direction, data->hw->power_id, data->hw->power_vol);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	
	len += snprintf(buf+len, PAGE_SIZE-len, "OPEN: %d\n", atomic_read(&dev_open_count));
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mag3110_i2c_data *obj = i2c_get_clientdata(mag3110_i2c_client);
	if(NULL == obj)
	{
		MSE_ERR("mag3110_i2c_data is null!!\n");
		return 0;
	}	
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct mag3110_i2c_data *obj = i2c_get_clientdata(mag3110_i2c_client);
	int trace;
	if(NULL == obj)
	{
		MSE_ERR("mag3110_i2c_data is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}
	else 
	{
		MSE_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;    
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,    S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata,  S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DRIVER_ATTR(calidata,    S_IRUGO, show_calidata_value, NULL);
static DRIVER_ATTR(midcontrol,  S_IRUGO | S_IWUSR, show_midcontrol_value, store_midcontrol_value );
static DRIVER_ATTR(middebug,    S_IRUGO | S_IWUSR, show_middebug_value, store_middebug_value );
static DRIVER_ATTR(mode,        S_IRUGO | S_IWUSR, show_mode_value, store_mode_value );
static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR, show_layout_value, store_layout_value );
static DRIVER_ATTR(status,      S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(trace,       S_IRUGO | S_IWUSR, show_trace_value, store_trace_value );
/*----------------------------------------------------------------------------*/
static struct driver_attribute *mag3110_attr_list[] = {
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_calidata,
	&driver_attr_midcontrol,
	&driver_attr_middebug,
	&driver_attr_mode,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
};
/*----------------------------------------------------------------------------*/
static int mag3110_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(mag3110_attr_list)/sizeof(mag3110_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, mag3110_attr_list[idx])))
		{            
			MSE_ERR("driver_create_file (%s) = %d\n", mag3110_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int mag3110_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(mag3110_attr_list)/sizeof(mag3110_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, mag3110_attr_list[idx]);
	}
	

	return err;
}


static int mag3110_open(struct inode *inode, struct file *file)
{    
	struct mag3110_i2c_data *obj = i2c_get_clientdata(mag3110_i2c_client);    
	int ret = -1;
	atomic_inc(&dev_open_count);
	
	if(atomic_read(&obj->trace) & AMI_TRC_DEBUG)
	{
		MSE_LOG("Open device node:mag3110\n");
	}
	ret = nonseekable_open(inode, file);
	
	return ret;
}
/*----------------------------------------------------------------------------*/
static int mag3110_release(struct inode *inode, struct file *file)
{
	struct mag3110_i2c_data *obj = i2c_get_clientdata(mag3110_i2c_client);
	atomic_dec(&dev_open_count);
	if(atomic_read(&obj->trace) & AMI_TRC_DEBUG)
	{
		MSE_LOG("Release device node:mag3110\n");
	}	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mag3110_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
    void __user *argp = (void __user *)arg;	
	int delay;
	int valuebuf[4];
	int calidata[7];
	int controlbuf[10];
	char strbuf[MAG3110_BUFSIZE];
	void __user *data;
	int retval=0;
	int mode=0;
	hwm_sensor_data* osensor_data;
	uint32_t enable;
	char buff[512];	
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
//	MSE_FUN(f);

	switch (cmd)
	{
		case ECOMPASS_IOC_GET_DELAY:
			delay = mag3110_mid_data.controldata[0];
			if(copy_to_user(argp, &delay, sizeof(delay)))
			{
				printk(KERN_ERR "copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case MSENSOR_IOCTL_INIT:
			read_lock(&mag3110_data.lock);
			mode = mag3110_data.mode;
			read_unlock(&mag3110_data.lock);
			MAG3110_Chipset_Init(mode);         
			break;

		case MSENSOR_IOCTL_SET_POSTURE:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			   
			if(copy_from_user(&valuebuf, data, sizeof(valuebuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}
			
			write_lock(&mag3110_mid_data.datalock);
			mag3110_mid_data.yaw   = valuebuf[0];
			mag3110_mid_data.pitch = valuebuf[1];
			mag3110_mid_data.roll  = valuebuf[2];
			mag3110_mid_data.mag_status = valuebuf[3];
			write_unlock(&mag3110_mid_data.datalock);    
			break; 

		case ECOMPASS_IOC_GET_OFLAG:
			sensor_status = atomic_read(&o_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case ECOMPASS_IOC_GET_MFLAG:
			sensor_status = atomic_read(&m_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			break;
			
		case ECOMPASS_IOC_GET_OPEN_STATUS:
			status = mag3110_GetOpenStatus();			
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_LOG("copy_to_user failed.");
				return -EFAULT;
			}
			break;        

		case MSENSOR_IOCTL_SET_CALIDATA:
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&calidata, data, sizeof(calidata)))
			{
				retval = -EFAULT;
				goto err_out;
			}
			
			write_lock(&mag3110_mid_data.datalock);            
			mag3110_mid_data.nmx = calidata[0];
			mag3110_mid_data.nmy = calidata[1];
			mag3110_mid_data.nmz = calidata[2];
			mag3110_mid_data.nax = calidata[3];
			mag3110_mid_data.nay = calidata[4];
			mag3110_mid_data.naz = calidata[5];
			mag3110_mid_data.mag_status = calidata[6];
			mag3110_mid_data.yaw = calidata[3];
			mag3110_mid_data.pitch = calidata[4];
			mag3110_mid_data.roll = calidata[5];
			write_unlock(&mag3110_mid_data.datalock);    
			break;                                

		case MSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			MAG3110_ReadChipInfo(strbuf, MAG3110_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_SENSOR_ENABLE:
			
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&enable, data, sizeof(enable)))
			{
				MSE_ERR("copy_from_user failed.");
				return -EFAULT;
			}
			else
			{
				read_lock(&mag3110_mid_data.ctrllock);
				if(enable == 1)
				{
					mag3110_mid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					mag3110_mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}		
				}
				wake_up(&open_wq);				
				read_unlock(&mag3110_mid_data.ctrllock);
				
			}
			
			break;
			
		case MSENSOR_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;    
			}
			MAG3110_ReadSensorData(strbuf, MAG3110_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
			
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			osensor_data = (hwm_sensor_data *)buff;

			read_lock(&mag3110_mid_data.datalock);
			osensor_data->values[0] = mag3110_mid_data.yaw;
			osensor_data->values[1] = mag3110_mid_data.pitch;
			osensor_data->values[2] = mag3110_mid_data.roll;
			//status = mag3110_mid_data.mag_status;
			read_unlock(&mag3110_mid_data.datalock); 
						
			osensor_data->value_divide = ORIENTATION_ACCURACY_RATE;	

			switch (mag3110_mid_data.mag_status)
		    {
		            case 1: case 2:
		                osensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
		                break;
		            case 3:
		                osensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		                break;
		            case 4:
		                osensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
		                break;
		            default:        
		                osensor_data->status = SENSOR_STATUS_UNRELIABLE;
		                break;    
		    }
     
			
            sprintf(buff, "%x %x %x %x %x", osensor_data->values[0], osensor_data->values[1],
				osensor_data->values[2],osensor_data->status,osensor_data->value_divide);
			if(copy_to_user(data, buff, strlen(buff)+1))
			{
				return -EFAULT;
			} 
			
			break;                

		case MSENSOR_IOCTL_READ_POSTUREDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			MAG3110_ReadPostureData(strbuf, MAG3110_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;            

		case MSENSOR_IOCTL_READ_CALIDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;    
			}
			MAG3110_ReadCaliData(strbuf, MAG3110_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_READ_CONTROL:
			read_lock(&mag3110_mid_data.ctrllock);
			memcpy(controlbuf, &mag3110_mid_data.controldata[0], sizeof(controlbuf));
			read_unlock(&mag3110_mid_data.ctrllock);            
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;
			}
			if(copy_to_user(data, controlbuf, sizeof(controlbuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}                                
			break;

		case MSENSOR_IOCTL_SET_CONTROL:
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;
			}
			if(copy_from_user(controlbuf, data, sizeof(controlbuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}    
			write_lock(&mag3110_mid_data.ctrllock);
			memcpy(&mag3110_mid_data.controldata[0], controlbuf, sizeof(controlbuf));
			write_unlock(&mag3110_mid_data.ctrllock);        
			break;

		case MSENSOR_IOCTL_SET_MODE:
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;
			}
			if(copy_from_user(&mode, data, sizeof(mode)))
			{
				retval = -EFAULT;
				goto err_out;
			}
			
			MAG3110_SetMode(mode);                
			break;
		    
		default:
			MSE_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			retval = -ENOIOCTLCMD;
			break;
		}

	err_out:
	return retval;    
}
/*----------------------------------------------------------------------------*/
static struct file_operations mag3110_fops = {
//	.owner = THIS_MODULE,
	.open = mag3110_open,
	.release = mag3110_release,
	.unlocked_ioctl = mag3110_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mag3110_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &mag3110_fops,
};
/*----------------------------------------------------------------------------*/
int mag3110_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status;
	hwm_sensor_data* msensor_data;
	
//	MSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					sample_delay = 20;
				}
				sample_delay = value;	
				
				mag3110_mid_data.controldata[0] = sample_delay;  // Loop Delay
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				read_lock(&mag3110_mid_data.ctrllock);
				if(value == 1)
				{
					mag3110_mid_data.controldata[7] |= SENSOR_MAGNETIC;
					atomic_set(&m_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					mag3110_mid_data.controldata[7] &= ~SENSOR_MAGNETIC;
					atomic_set(&m_flag, 0);
					if(atomic_read(&o_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}	
				}
				wake_up(&open_wq);
				read_unlock(&mag3110_mid_data.ctrllock);
				// TODO: turn device into standby or normal mode
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				msensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&mag3110_mid_data.datalock);
				msensor_data->values[0] = mag3110_mid_data.nmx;
				msensor_data->values[1] = mag3110_mid_data.nmy;
				msensor_data->values[2] = mag3110_mid_data.nmz;
				status = mag3110_mid_data.mag_status;
				read_unlock(&mag3110_mid_data.datalock); 
				
				msensor_data->values[0] = msensor_data->values[0] * CONVERT_M;
				msensor_data->values[1] = msensor_data->values[1] * CONVERT_M;
				msensor_data->values[2] = msensor_data->values[2] * CONVERT_M;
				msensor_data->value_divide = 100;

				switch (status)
		        {
		            case 1: case 2:
		                msensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
		                break;
		            case 3:
		                msensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		                break;
		            case 4:
		                msensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
		                break;
		            default:        
		                msensor_data->status = SENSOR_STATUS_UNRELIABLE;
		                break;    
		        }
				
			}
			break;
		default:
			MSE_ERR("msensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
int mag3110_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status=0;
	hwm_sensor_data* osensor_data=NULL;
	
	//MSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					sample_delay = 20;
				}
				sample_delay = value;	
				
				mag3110_mid_data.controldata[0] = sample_delay;  // Loop Delay
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				read_lock(&mag3110_mid_data.ctrllock);
				if(value == 1)
				{
					mag3110_mid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					mag3110_mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}
				}
				wake_up(&open_wq);
				read_unlock(&mag3110_mid_data.ctrllock);
				// Do nothing
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				osensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&mag3110_mid_data.datalock);
				osensor_data->values[0] = mag3110_mid_data.yaw;
				osensor_data->values[1] = mag3110_mid_data.pitch;
				osensor_data->values[2] = mag3110_mid_data.roll;
				status = mag3110_mid_data.mag_status;
				read_unlock(&mag3110_mid_data.datalock); 
				
				
				osensor_data->value_divide = ORIENTATION_ACCURACY_RATE;				
			}

			switch (status)
	        {
	            case 1: case 2:
	                osensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
	                break;
	            case 3:
	                osensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
	                break;
	            case 4:
	                osensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
	                break;
	            default:        
	                osensor_data->status = SENSOR_STATUS_UNRELIABLE;
	                break;    
	        }
			break;
		default:
			MSE_ERR("gsensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mag3110_suspend(struct i2c_client *client, pm_message_t msg) 
{
	int err;
	struct mag3110_i2c_data *obj = i2c_get_clientdata(client)
	MSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		
		mag3110_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mag3110_resume(struct i2c_client *client)
{
	int err;
	struct mag3110_i2c_data *obj = i2c_get_clientdata(client)
	MSE_FUN();

	mag3110_power(obj->hw, 1);

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void mag3110_early_suspend(struct early_suspend *h) 
{
	struct mag3110_i2c_data *obj = container_of(h, struct mag3110_i2c_data, early_drv);   
	int err;
	MSE_FUN();    

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}

}
/*----------------------------------------------------------------------------*/
static void mag3110_late_resume(struct early_suspend *h)
{
	struct mag3110_i2c_data *obj = container_of(h, struct mag3110_i2c_data, early_drv);         
	int err;
	MSE_FUN();

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}

	mag3110_power(obj->hw, 1);

}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
#if 0
static int mag3110_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, MAG3110_DEV_NAME);
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int mag3110_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mag3110_i2c_data *data;
	int err = 0;
	struct hwmsen_object sobj_m, sobj_o;

	if (!(data = kmalloc(sizeof(struct mag3110_i2c_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct mag3110_i2c_data));

	data->hw = get_cust_mag_hw();
	if((err = hwmsen_get_convert(data->hw->direction, &data->cvt)))
	{
		MSE_ERR("invalid direction: %d\n", data->hw->direction);
		goto exit;
	}
	
	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);
	init_waitqueue_head(&data_ready_wq);
	init_waitqueue_head(&open_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);
	
	mag3110_i2c_client = new_client;	

	if((err = MAG3110_Chipset_Init(MAG3110_FORCE_MODE)))
	{
		goto exit_init_failed;
	}
	if((err = misc_register(&mag3110_device)))
	{
		MSE_ERR("mag3110_device register failed\n");
		goto exit_misc_device_register_failed;	}    

	sobj_m.self = data;
    sobj_m.polling = 1;
    sobj_m.sensor_operate = mag3110_operate;
	if((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
	
	sobj_o.self = data;
    sobj_o.polling = 1;
    sobj_o.sensor_operate = mag3110_orientation_operate;
	if((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
	
#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	data->early_drv.suspend  = mag3110_early_suspend,
	data->early_drv.resume   = mag3110_late_resume,    
	register_early_suspend(&data->early_drv);
#endif

	MSE_LOG("%s: OK\n", __func__);
	return 0;
	
    exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(new_client);
	//exit_misc_device_register_failed:
	exit_kfree:
	kfree(data);
	exit:
	MSE_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int mag3110_i2c_remove(struct i2c_client *client)
{
	int err;	
	mag3110_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));	
	misc_deregister(&mag3110_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mag_probe(struct platform_device *pdev) 
{
	struct mag_hw *hw = get_cust_mag_hw();

	mag3110_power(hw, 1);    
	rwlock_init(&mag3110_mid_data.ctrllock);
	rwlock_init(&mag3110_mid_data.datalock);
	rwlock_init(&mag3110_data.lock);
	mag3110_mid_data.controldata[0] =    20;  // Loop Delay
	mag3110_mid_data.controldata[1] =     0;  // Run   
	mag3110_mid_data.controldata[2] =     0;  // Disable Start-AccCali
	mag3110_mid_data.controldata[3] =     1;  // Enable Start-Cali
	mag3110_mid_data.controldata[4] =   350;  // MW-Timout
	mag3110_mid_data.controldata[5] =    10;  // MW-IIRStrength_M
	mag3110_mid_data.controldata[6] =    10;  // MW-IIRStrength_G   
	mag3110_mid_data.controldata[7] =     0;  // Active Sensors
	mag3110_mid_data.controldata[8] =     0;  // Wait for define
	mag3110_mid_data.controldata[9] =     0;  // Wait for define   
	atomic_set(&dev_open_count, 0);
	//mag3110_force[0] = hw->i2c_num;

	if(i2c_add_driver(&mag3110_i2c_driver))
	{
		MSE_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mag_remove(struct platform_device *pdev)
{
	struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();    
	mag3110_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&mag3110_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
#if 0
static struct platform_driver mag_sensor_driver = {
	.probe      = mag_probe,
	.remove     = mag_remove,    
	.driver     = {
		.name  = "msensor",
//		.owner = THIS_MODULE,
	}
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id mag_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver mag_sensor_driver =
{
	.probe      = mag_probe,
	.remove     = mag_remove,    
	.driver     = 
	{
		.name = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = mag_of_match,
		#endif
	}
};

/*----------------------------------------------------------------------------*/
static int __init mag3110_init(void)
{
	MSE_FUN();
	struct mag_hw *hw = get_cust_mag_hw();
	MSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_mag3110, 1);
	if(platform_driver_register(&mag_sensor_driver))
	{
		MSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit mag3110_exit(void)
{
	MSE_FUN();
	platform_driver_unregister(&mag_sensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(mag3110_init);
module_exit(mag3110_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Kyle K.Y. Chen");
MODULE_DESCRIPTION("MAG3110 MI-Sensor driver without DRDY");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

